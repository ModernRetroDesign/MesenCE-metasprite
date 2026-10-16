#include "pch.h"
#include "Shared/Audio/MusicReplacement.h"
#include "Utilities/Audio/stb_vorbis.h"
#include "Utilities/VirtualFile.h"
#include <set>
#include <unordered_map>
#include <cmath>

namespace {
class MusicPacketReader {
	const uint8_t* _data; size_t _length, _offset = 0;
public:
	MusicPacketReader(const uint8_t* data, size_t length) : _data(data), _length(length) { if(!data || length > 1048576) { throw std::runtime_error("Invalid music packet"); } }
	uint32_t Read() { if(_offset > _length || _length - _offset < 4) { throw std::runtime_error("Truncated music packet"); } uint32_t v = 0; for(int i = 0; i < 4; i++) { v |= (uint32_t)_data[_offset++] << (i * 8); } return v; }
	bool Bool() { auto v = Read(); if(v > 1) { throw std::runtime_error("Invalid music flag"); } return v != 0; }
	uint32_t Bounded(uint32_t max) { auto v = Read(); if(v > max) { throw std::runtime_error("Invalid music field"); } return v; }
	std::string String() { uint32_t size = Bounded(32768); if(size > _length - _offset) { throw std::runtime_error("Truncated music path"); } std::string s((const char*)_data + _offset, size); _offset += size; if(s.find('\0') != std::string::npos) { throw std::runtime_error("Invalid music path"); } return s; }
	MetaMemoryCondition Condition(uint32_t maxAddress) { auto addr = Bounded(maxAddress), value = Bounded(255), mask = Bounded(255), op = Bounded(5); return {addr, (uint8_t)value, (uint8_t)mask, (uint8_t)op}; }
	void End() { if(_offset != _length) { throw std::runtime_error("Trailing music data"); } }
};

// OGG data stays compressed in memory; decode only a small block at a time.
// PCM WAV is supported for authoring/tests. No filesystem work occurs on a write.
class MusicTrack {
	std::vector<uint8_t> _data;
	stb_vorbis* _ogg = nullptr;
	std::vector<int16_t> _cache;
	uint32_t _cacheIndex = 0, _cacheFrames = 0, _position = 0, _wavOffset = 0, _channels = 2;
	uint32_t _length = 0, _rate = 0, _loopStart = 0, _loopEnd = 0;
	bool _loop = false, _ended = false;
	double _phase = 0;
	int16_t _a[2] = {}, _b[2] = {};
	uint32_t U32(size_t p) const { if(p + 4 > _data.size()) { throw std::runtime_error("Truncated WAV"); } return (uint32_t)_data[p] | ((uint32_t)_data[p+1] << 8) | ((uint32_t)_data[p+2] << 16) | ((uint32_t)_data[p+3] << 24); }
	uint32_t U16(size_t p) const { if(p + 2 > _data.size()) { throw std::runtime_error("Truncated WAV"); } return _data[p] | (_data[p+1] << 8); }
	void Seek(uint32_t position) { _position = position; _cacheIndex = _cacheFrames = 0; if(_ogg && !stb_vorbis_seek(_ogg, position)) { _ended = true; } }
	bool Next(int16_t* out) {
		if(_ended) { out[0] = out[1] = 0; return false; }
		if(_position >= _loopEnd) { if(_loop) { Seek(_loopStart); } else { _ended = true; out[0] = out[1] = 0; return false; } }
		if(_ogg) {
			if(_cacheIndex >= _cacheFrames) {
				_cacheFrames = (uint32_t)stb_vorbis_get_samples_short_interleaved(_ogg, 2, _cache.data(), (int)std::min(4096u, _loopEnd - _position) * 2); _cacheIndex = 0;
				if(!_cacheFrames) { _ended = true; out[0] = out[1] = 0; return false; }
			}
			out[0] = _cache[_cacheIndex * 2]; out[1] = _cache[_cacheIndex * 2 + 1]; _cacheIndex++;
		} else {
			size_t offset = _wavOffset + (size_t)_position * _channels * 2;
			out[0] = (int16_t)U16(offset); out[1] = _channels == 1 ? out[0] : (int16_t)U16(offset + 2);
		}
		_position++; return true;
	}
public:
	uint32_t Id, Volume, FadeIn;
	std::string Path;
	MusicTrack(uint32_t id, const std::string& path, bool loop, uint32_t start, uint32_t end, uint32_t volume, uint32_t fade, size_t& total) : Id(id), Volume(volume), FadeIn(fade), Path(path) {
		VirtualFile file(path); int64_t size = file.GetSize();
		if(size <= 0 || size > 67108864 || total + size > 134217728 || !file.ReadFile(_data)) { throw std::runtime_error("Music files exceed budget or cannot be read"); } total += _data.size();
		if(_data.size() >= 12 && U32(0) == 0x46464952 && U32(8) == 0x45564157) {
			bool format = false; uint32_t dataSize = 0;
			for(size_t p = 12; p + 8 <= _data.size();) {
				uint32_t tag = U32(p), bytes = U32(p + 4); size_t begin = p + 8;
				if(bytes > _data.size() - begin) { throw std::runtime_error("Truncated WAV chunk"); }
				if(tag == 0x20746D66) { if(bytes < 16 || U16(begin) != 1 || U16(begin + 14) != 16) { throw std::runtime_error("Use PCM 16-bit WAV"); } _channels = U16(begin + 2); _rate = U32(begin + 4); if((_channels != 1 && _channels != 2) || U16(begin + 12) != _channels * 2) { throw std::runtime_error("Invalid WAV channels"); } format = true; }
				if(tag == 0x61746164) { _wavOffset = (uint32_t)begin; dataSize = bytes; }
				p = begin + bytes + (bytes & 1);
			}
			if(!format || !_wavOffset || dataSize % (_channels * 2)) { throw std::runtime_error("Invalid WAV data"); } _length = dataSize / (_channels * 2);
		} else {
			int error = 0; _ogg = stb_vorbis_open_memory(_data.data(), (int)_data.size(), &error, nullptr);
			if(!_ogg) { throw std::runtime_error("Use OGG Vorbis or PCM 16-bit WAV"); }
			auto info = stb_vorbis_get_info(_ogg); _rate = info.sample_rate;
			if(info.channels < 1 || info.channels > 2) { stb_vorbis_close(_ogg); _ogg = nullptr; throw std::runtime_error("Use mono/stereo audio"); }
			_length = stb_vorbis_stream_length_in_samples(_ogg);
		}
		_loop = loop; _loopStart = start; _loopEnd = end ? end : _length;
		if(_rate < 8000 || _rate > 192000 || !_length || _loopEnd > _length || start >= _loopEnd) { if(_ogg) { stb_vorbis_close(_ogg); _ogg = nullptr; } throw std::runtime_error("Invalid audio rate or loop points"); }
		_cache.resize(8192); Restart();
	}
	~MusicTrack() { if(_ogg) { stb_vorbis_close(_ogg); } }
	void Restart() { _ended = false; _phase = 0; Seek(0); Next(_a); Next(_b); }
	bool Sample(uint32_t outputRate, double& left, double& right) {
		if(_ended && !_a[0] && !_a[1] && !_b[0] && !_b[1]) { left = right = 0; return false; }
		left = _a[0] + (_b[0] - _a[0]) * _phase; right = _a[1] + (_b[1] - _a[1]) * _phase;
		_phase += (double)_rate / outputRate;
		while(_phase >= 1) { _phase -= 1; _a[0] = _b[0]; _a[1] = _b[1]; Next(_b); }
		return true;
	}
};
}

struct MusicReplacement::Impl {
	std::mutex Mutex;
	bool Enabled, Mute, Paused = false;
	uint32_t NativeVolume, MusicVolume, FadeOut;
	std::unordered_map<uint32_t, std::unique_ptr<MusicTrack>> Tracks;
	std::unordered_map<uint32_t, std::unique_ptr<MusicTrack>> Effects;
	std::set<uint32_t> PlayingEffects;
	std::vector<MusicWriteRule> Rules;
	std::set<uint32_t> Addresses;
	MusicTrack* Active = nullptr;
	double Gain = 1, FadeRemaining = 0, FadeStep = 0;
	bool Stopping = false;
	bool Command(uint32_t action, uint32_t id, bool restart) {
		if(!Enabled || action > 5) { return false; }
		if(action == 4) {
			auto found = Effects.find(id); if(found == Effects.end()) { return false; }
			if(PlayingEffects.count(id) && !restart) { return true; }
			found->second->Restart(); PlayingEffects.insert(id); return true;
		} else if(action == 5) { PlayingEffects.clear(); return true; }
		if(action == 0) {
			auto found = Tracks.find(id); if(found == Tracks.end()) { return false; }
			if(Active == found->second.get() && !restart) { return true; }
			Active = found->second.get(); Active->Restart(); Paused = Stopping = false; Gain = Active->FadeIn ? 0 : 1; FadeRemaining = Active->FadeIn / 1000.0;
		} else if(action == 1) { if(FadeOut && Active && !Paused) { Stopping = true; FadeRemaining = FadeOut / 1000.0; FadeStep = Gain / FadeRemaining; } else { Active = nullptr; } }
		else if(action == 2) { Paused = true; } else { Paused = false; }
		return true;
	}
};

MusicReplacement::MusicReplacement(const uint8_t* packet, size_t length, uint32_t maxAddress, uint32_t maxContextAddress) : _impl(new Impl()) {
	MusicPacketReader p(packet, length); uint32_t version = p.Read();
	if(version != 0x3152554D && version != 0x3252554D) { throw std::runtime_error("Invalid music packet version"); }
	maxAddress = std::min(maxAddress, version == 0x3252554D ? 0x0FFFFFFFu : 0xFFFFu);
	auto& s = *_impl; s.Enabled = p.Bool(); s.Mute = p.Bool(); s.NativeVolume = p.Bounded(100); s.MusicVolume = p.Bounded(100); s.FadeOut = p.Bounded(10000);
	uint32_t count = p.Bounded(256); size_t total = 0;
	for(uint32_t i = 0; i < count; i++) {
		uint32_t id = p.Bounded(65535); auto path = p.String(); bool loop = p.Bool(); uint32_t start = p.Read(), end = p.Read(), volume = p.Bounded(100), fade = p.Bounded(10000);
		if(!id || s.Tracks.count(id)) { throw std::runtime_error("Duplicate music track"); }
		s.Tracks.emplace(id, std::make_unique<MusicTrack>(id, path, loop, start, end, volume, fade, total));
	}
	count = p.Bounded(512); std::set<uint32_t> ids;
	for(uint32_t i = 0; i < count; i++) {
		MusicWriteRule rule; rule.Id = p.Read(); rule.Action = p.Bounded(5); rule.TrackId = p.Read(); rule.Restart = p.Bool(); rule.Trigger = p.Condition(maxAddress);
		if(!rule.Id || !ids.insert(rule.Id).second || ((rule.Action == 0 || rule.Action == 4) && !s.Tracks.count(rule.TrackId))) { throw std::runtime_error("Invalid music rule"); }
		uint32_t n = p.Bounded(64); for(uint32_t j = 0; j < n; j++) { rule.Conditions.push_back(p.Condition(std::min(maxAddress, maxContextAddress))); }
		s.Addresses.insert(rule.Trigger.Address); s.Rules.push_back(std::move(rule));
	}
	p.End();
	// Independent one-shot decoders: effects never seek or pause the music decoder.
	// Allocate while applying the pack, never on a CPU write or mixer callback.
	for(const auto& rule : s.Rules) {
		if(rule.Action != 4 || s.Effects.count(rule.TrackId)) { continue; }
		if(s.Effects.size() >= 32) { throw std::runtime_error("Maximum 32 distinct sound effects"); }
		auto& track = *s.Tracks.at(rule.TrackId);
		s.Effects.emplace(rule.TrackId, std::make_unique<MusicTrack>(track.Id, track.Path, false, 0, 0, track.Volume, 0, total));
	}
}
MusicReplacement::~MusicReplacement() = default;
bool MusicReplacement::HasTriggers() const { return _impl->Enabled && !_impl->Rules.empty(); }
std::vector<uint32_t> MusicReplacement::TriggerAddresses() const { return {_impl->Addresses.begin(), _impl->Addresses.end()}; }
void MusicReplacement::ProcessWrite(uint32_t address, uint8_t value, const std::function<bool(uint32_t, uint8_t&)>& read) {
	auto& s = *_impl; if(!s.Enabled || !s.Addresses.count(address)) { return; }
	std::lock_guard<std::mutex> lock(s.Mutex); MetaMemorySnapshot snapshot;
	bool musicMatched = false, effectMatched = false;
	for(const auto& rule : s.Rules) {
		bool effect = rule.Action >= 4;
		if(effect ? effectMatched : musicMatched) { continue; }
		if(rule.Trigger.Address != address || !MetaMemoryCondition::Matches({rule.Trigger}, {{address, value}})) { continue; }
		bool available = true;
		for(const auto& c : rule.Conditions) { if(!snapshot.count(c.Address)) { uint8_t b; if(!read(c.Address, b)) { available = false; break; } snapshot.emplace(c.Address, b); } }
		if(available && MetaMemoryCondition::Matches(rule.Conditions, snapshot)) { s.Command(rule.Action, rule.TrackId, rule.Restart); if(effect) { effectMatched = true; } else { musicMatched = true; } }
	}
}
bool MusicReplacement::Command(uint32_t action, uint32_t id, bool restart) { std::lock_guard<std::mutex> lock(_impl->Mutex); return _impl->Command(action, id, restart); }
void MusicReplacement::ResetPlayback() { std::lock_guard<std::mutex> lock(_impl->Mutex); _impl->Active = nullptr; _impl->PlayingEffects.clear(); _impl->Paused = false; _impl->Stopping = false; }
int MusicReplacement::ActiveTrack() { std::lock_guard<std::mutex> lock(_impl->Mutex); return _impl->Active ? (int)_impl->Active->Id : 0; }
void MusicReplacement::Mix(int16_t* samples, uint32_t frames, uint32_t rate) {
	auto& s = *_impl; if(!s.Enabled || !rate) { return; }
	std::lock_guard<std::mutex> lock(s.Mutex);
	for(uint32_t i = 0; i < frames; i++) {
		double left = 0, right = 0;
		if(s.Active && !s.Paused) {
			if(!s.Active->Sample(rate, left, right)) { s.Active = nullptr; }
			else {
				if(s.FadeRemaining > 0) { double step = std::min(s.FadeRemaining, 1.0 / rate); s.FadeRemaining -= step; if(s.Stopping) { s.Gain = std::max(0.0, s.Gain - s.FadeStep * step); } else { s.Gain = std::min(1.0, s.Gain + step * 1000 / std::max(1u, s.Active->FadeIn)); } }
				double volume = s.MusicVolume * s.Active->Volume * s.Gain / 10000.0; left *= volume; right *= volume;
				if(s.Stopping && s.FadeRemaining <= 0) { s.Active = nullptr; }
			}
		}
		for(auto it = s.PlayingEffects.begin(); it != s.PlayingEffects.end();) {
			auto& effect = *s.Effects.at(*it); double effectLeft, effectRight;
			if(!effect.Sample(rate, effectLeft, effectRight)) { it = s.PlayingEffects.erase(it); }
			else { left += effectLeft * effect.Volume / 100.0; right += effectRight * effect.Volume / 100.0; ++it; }
		}
		double native = s.Mute ? 0 : s.NativeVolume / 100.0;
		samples[i * 2] = (int16_t)std::clamp((int)std::round(samples[i * 2] * native + left), (int)INT16_MIN, (int)INT16_MAX);
		samples[i * 2 + 1] = (int16_t)std::clamp((int)std::round(samples[i * 2 + 1] * native + right), (int)INT16_MIN, (int)INT16_MAX);
	}
}

#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <memory>
#include <unordered_map>
#include <array>

// Stable content identity for optional save-state clock matching. Computed once
// from validated runtime packets, never from live CPU memory.
inline std::array<uint64_t,2> MetaPngPacketIdentity(const uint8_t* data, size_t length)
{
	std::array<uint64_t,2> hash = {14695981039346656037ULL,7809847782465536322ULL};
	for(size_t i=0;i<length;i++) {
		hash[0]=(hash[0]^data[i])*1099511628211ULL;
		hash[1]=(hash[1]^data[i])*14029467366897019727ULL;
	}
	return hash;
}

enum class MetaPngPlayback : uint8_t { Loop, PingPong, PlayOnce };

// Compiled once when artwork is loaded. Selection reads immutable timing data;
// no decoded artwork allocation or pixel copying is performed on a frame tick.
class MetaPngTimeline
{
	MetaPngPlayback _playback = MetaPngPlayback::Loop;
	std::vector<uint64_t> _ends;
	std::vector<uint16_t> _indices;
public:
	MetaPngTimeline() = default;
	MetaPngTimeline(MetaPngPlayback playback, const std::vector<uint32_t>& durations) : _playback(playback)
	{
		if((uint8_t)playback > 2 || durations.empty() || durations.size() > 256) { throw std::runtime_error("Invalid PNG animation timeline"); }
		for(auto duration : durations) { if(duration < 1 || duration > 3600000) { throw std::runtime_error("Invalid PNG animation duration"); } }
		size_t count = durations.size();
		size_t visits = count + (playback == MetaPngPlayback::PingPong && count > 2 ? count - 2 : 0);
		_ends.reserve(visits); _indices.reserve(visits); uint64_t end = 0;
		for(size_t visit = 0; visit < visits; visit++) {
			size_t index = visit < count ? visit : count * 2 - 2 - visit;
			end += durations[index]; _ends.push_back(end); _indices.push_back((uint16_t)index);
		}
	}
	uint64_t Duration() const { return _ends.empty() ? 0 : _ends.back(); }
	int SelectFrame(uint64_t elapsedEmulatedFrames) const
	{
		if(_ends.empty()) { return -1; }
		if(_playback == MetaPngPlayback::PlayOnce && elapsedEmulatedFrames >= Duration()) { return _indices.back(); }
		auto found = std::upper_bound(_ends.begin(), _ends.end(), elapsedEmulatedFrames % Duration());
		return _indices[(size_t)(found - _ends.begin())];
	}
};

// A load-scoped pool; frames retain ownership after parsing finishes. Hash matches
// are always checked against the complete buffer before sharing storage.
class MetaPngPixelPool
{
	std::unordered_map<uint64_t, std::vector<std::shared_ptr<const std::vector<uint32_t>>>> _buffers;
public:
	std::shared_ptr<const std::vector<uint32_t>> Intern(std::vector<uint32_t> pixels)
	{
		uint64_t hash = 14695981039346656037ULL;
		for(auto pixel : pixels) { hash ^= pixel; hash *= 1099511628211ULL; }
		auto& bucket = _buffers[hash];
		for(const auto& existing : bucket) { if(*existing == pixels) { return existing; } }
		auto stored = std::make_shared<const std::vector<uint32_t>>(std::move(pixels));
		bucket.push_back(stored); return stored;
	}
};

// Resource IDs refer only to earlier inline buffers in the same packet.
class MetaPngPacketResources
{
	std::vector<std::shared_ptr<const std::vector<uint32_t>>> _pixels;
public:
	std::shared_ptr<const std::vector<uint32_t>> Resolve(uint32_t id,size_t pixelCount) const
	{
		if(id<1 || id>_pixels.size() || _pixels[id-1]->size()!=pixelCount) { throw std::runtime_error("Invalid PNG pixel-resource reference"); }
		return _pixels[id-1];
	}
	void Add(const std::shared_ptr<const std::vector<uint32_t>>& pixels) { _pixels.push_back(pixels); }
};

struct MetaPngArtworkFrame
{
	int OriginX = 0, OriginY = 0, Width = 0, Height = 0;
	std::shared_ptr<const std::vector<uint32_t>> Pixels;
};

// Updated only on the emulation thread. Repeated presentation does not tick;
// frame-counter wrap advances normally, while rewind/load to an earlier frame
// starts a fresh activation rather than producing a huge unsigned elapsed time.
class MetaPngActivationClock
{
	uint32_t _last = 0;
	uint64_t _elapsed = 0;
	bool _active = false;
public:
	std::array<uint64_t,2> Save() const { return {uint64_t(_last) | (uint64_t(_active) << 32), _elapsed}; }
	bool Restore(uint64_t lastAndActive, uint64_t elapsed)
	{
		if((lastAndActive >> 32) > 1 || elapsed > UINT64_MAX-UINT32_MAX) { return false; }
		_last=(uint32_t)lastAndActive; _active=(lastAndActive >> 32)!=0; _elapsed=elapsed; return true;
	}
	uint64_t Observe(uint32_t frame, bool active)
	{
		uint32_t delta = frame - _last;
		if(!active || !_active || delta > INT32_MAX) { _elapsed = 0; }
		else { _elapsed += delta; }
		_active = active; _last = frame; return _elapsed;
	}
};

struct MetaPngArtworkAnimation;
struct MetaPngAnimationSnapshot
{
	std::unordered_map<const MetaPngArtworkAnimation*, uint64_t> Elapsed;
	// Keep keys alive until the captured frame is released, even after pack edits.
	std::shared_ptr<const void> Groups, Backgrounds;
};

struct MetaPngArtworkAnimation
{
	MetaPngTimeline Timeline;
	bool RestartOnActivation = false;
	std::array<uint64_t,2> SourceIdentity = {};
	std::vector<MetaPngArtworkFrame> Frames;
	const MetaPngArtworkFrame* Select(uint64_t emulatedFrame, const MetaPngAnimationSnapshot* timing = nullptr) const
	{
		if(RestartOnActivation) {
			emulatedFrame = 0;
			if(timing) { auto found = timing->Elapsed.find(this); if(found != timing->Elapsed.end()) { emulatedFrame = found->second; } }
		}
		int index = Timeline.SelectFrame(emulatedFrame);
		return index >= 0 && (size_t)index < Frames.size() ? &Frames[index] : nullptr;
	}
};

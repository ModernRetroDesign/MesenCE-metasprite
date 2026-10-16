#pragma once
#include "pch.h"
#include "Core/Shared/Interfaces/IAudioDevice.h"
#include "Utilities/safe_ptr.h"
#include "Utilities/Audio/HermiteResampler.h"
#include <functional>
#include <array>

class Emulator;
class Equalizer;
class SoundResampler;
class WaveRecorder;
class IAudioProvider;
class CrossFeedFilter;
class ReverbFilter;
class MusicReplacement;

class SoundMixer
{
private:
	IAudioDevice* _audioDevice;
	vector<IAudioProvider*> _audioProviders;
	Emulator* _emu;
	unique_ptr<Equalizer> _equalizer;
	unique_ptr<SoundResampler> _resampler;
	safe_ptr<WaveRecorder> _waveRecorder;
	int16_t* _sampleBuffer = nullptr;

	HermiteResampler _pitchAdjust;
	int16_t* _pitchAdjustBuffer = nullptr;

	int16_t _leftSample = 0;
	int16_t _rightSample = 0;

	unique_ptr<CrossFeedFilter> _crossFeedFilter;
	unique_ptr<ReverbFilter> _reverbFilter;
	shared_ptr<MusicReplacement> _replacementMusic;
	atomic<bool> _hasMusicTriggers{false};
	// Low-16-bit prefilter; the engine checks full addresses. Updated with the emulation lock held, or while the CPU is stopped.
	std::array<uint64_t, 1024> _musicTriggerMask{};

	void ProcessEqualizer(int16_t* samples, uint32_t sampleCount, uint32_t targetRate);

public:
	SoundMixer(Emulator* emu);
	~SoundMixer();

	void PlayAudioBuffer(int16_t* samples, uint32_t sampleCount, uint32_t sourceRate);
	void StopAudio(bool clearBuffer = false);

	void RegisterAudioDevice(IAudioDevice* audioDevice);

	void RegisterAudioProvider(IAudioProvider* provider);
	void UnregisterAudioProvider(IAudioProvider* provider);

	AudioStatistics GetStatistics();
	double GetRateAdjustment();

	void StartRecording(string filepath);
	void StopRecording();
	bool IsRecording();
	void GetLastSamples(int16_t& left, int16_t& right);
	void SetReplacementMusic(shared_ptr<MusicReplacement> music);
	void ClearReplacementMusic();
	void ResetReplacementMusic();
	bool HasMusicTriggers() const { return _hasMusicTriggers.load(std::memory_order_relaxed); }
	bool HasMusicTrigger(uint32_t address) const { return HasMusicTriggers() && (_musicTriggerMask[(address & 0xFFFF) >> 6] & (1ULL << (address & 63))); }
	void ProcessMusicWrite(uint32_t address, uint8_t value, const std::function<bool(uint32_t, uint8_t&)>& read);
	bool MusicCommand(uint32_t action, uint32_t track);
	int MusicActiveTrack();
};

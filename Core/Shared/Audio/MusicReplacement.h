#pragma once
#include "Shared/Video/MetaMemoryCondition.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

// Event rules run after an actual CPU write. Merely changing a context byte
// cannot select a track. The first matching rule per music/effect category in pack order wins.
struct MusicWriteRule
{
	uint32_t Id = 0, TrackId = 0, Action = 0; // Music play/stop/pause/resume, effect play/stop all
	bool Restart = false;
	MetaMemoryCondition Trigger;
	std::vector<MetaMemoryCondition> Conditions;
};

class MusicReplacement
{
	struct Impl;
	std::unique_ptr<Impl> _impl;
public:
	MusicReplacement(const uint8_t* packet, size_t length, uint32_t maxAddress = 0xFFFF, uint32_t maxContextAddress = 0xFFFFFFFF);
	~MusicReplacement();
	bool HasTriggers() const;
	std::vector<uint32_t> TriggerAddresses() const;
	void ProcessWrite(uint32_t address, uint8_t value, const std::function<bool(uint32_t, uint8_t&)>& read);
	bool Command(uint32_t action, uint32_t trackId = 0, bool restart = true);
	void ResetPlayback();
	void Mix(int16_t* samples, uint32_t frames, uint32_t rate);
	int ActiveTrack();
};

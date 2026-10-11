#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

// CPU address-space bytes captured with the video frame, never live decoder reads.
using MetaMemorySnapshot = std::unordered_map<uint32_t, uint8_t>;
struct MetaMemoryCondition
{
	uint32_t Address = 0;
	uint8_t Value = 0, Mask = 0xFF;
	static bool Matches(const std::vector<MetaMemoryCondition>& conditions, const MetaMemorySnapshot& snapshot)
	{
		for(const auto& condition : conditions) {
			auto value = snapshot.find(condition.Address);
			if(value == snapshot.end() || (value->second & condition.Mask) != (condition.Value & condition.Mask)) { return false; }
		}
		return true;
	}
};

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
	uint8_t Comparison = 0; // ==, !=, >, <, >=, <=; legacy conditions default to ==.
	static bool Matches(const std::vector<MetaMemoryCondition>& conditions, const MetaMemorySnapshot& snapshot)
	{
		for(const auto& condition : conditions) {
			auto value = snapshot.find(condition.Address);
			if(value == snapshot.end()) { return false; }
			uint8_t left = value->second & condition.Mask, right = condition.Value & condition.Mask;
			bool matches = false;
			switch(condition.Comparison) {
				case 0: matches = left == right; break; case 1: matches = left != right; break;
				case 2: matches = left > right; break; case 3: matches = left < right; break;
				case 4: matches = left >= right; break; case 5: matches = left <= right; break;
			}
			if(!matches) { return false; }
		}
		return true;
	}
};

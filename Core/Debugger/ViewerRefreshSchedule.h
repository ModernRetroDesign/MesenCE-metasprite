#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

// Viewer timings change under DebugBreakHelper. PPU ticks only read this
// schedule, and almost every tick can reject without traversing viewer entries.
class ViewerRefreshSchedule
{
	struct Entry { uint32_t Id; uint16_t Scanline, Cycle; };
	std::vector<Entry> _entries;
	uint64_t _scanlines[1024] = {}, _cycles[1024] = {};
	bool _hasViewers = false;
	void Rebuild()
	{
		std::fill_n(_scanlines, 1024, uint64_t(0));
		std::fill_n(_cycles, 1024, uint64_t(0));
		_hasViewers = !_entries.empty();
		for(const auto& entry : _entries) {
			_scanlines[entry.Scanline >> 6] |= uint64_t(1) << (entry.Scanline & 63);
			_cycles[entry.Cycle >> 6] |= uint64_t(1) << (entry.Cycle & 63);
		}
	}
public:
	void Set(uint32_t id, uint16_t scanline, uint16_t cycle)
	{
		auto entry = std::find_if(_entries.begin(), _entries.end(), [id](const Entry& e) { return e.Id == id; });
		if(entry == _entries.end()) { _entries.push_back({id, scanline, cycle}); }
		else { *entry = {id, scanline, cycle}; }
		Rebuild();
	}
	void Remove(uint32_t id)
	{
		_entries.erase(std::remove_if(_entries.begin(), _entries.end(), [id](const Entry& e) { return e.Id == id; }), _entries.end());
		Rebuild();
	}
	bool HasViewers() const { return _hasViewers; }
	template<typename Callback> void Visit(uint16_t scanline, uint16_t cycle, Callback&& notify) const
	{
		if(!(_scanlines[scanline >> 6] & (uint64_t(1) << (scanline & 63)))
			|| !(_cycles[cycle >> 6] & (uint64_t(1) << (cycle & 63)))) { return; }
		const Entry* entries = _entries.data();
		for(size_t i = 0, count = _entries.size(); i < count; i++) {
			if(entries[i].Scanline == scanline && entries[i].Cycle == cycle) { notify(entries[i].Id); }
		}
	}
};

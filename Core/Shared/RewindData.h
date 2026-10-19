#pragma once
#include "pch.h"
#include <deque>
#include "Shared/BaseControlDevice.h"

class Emulator;

class RewindData
{
private:
	vector<uint8_t> _saveStateData;
	vector<uint8_t> _uncompressedData;
	// Compressed display history, separate from CPU/PPU save-state bytes.
	vector<vector<uint8_t>> _frameBlendData;
	vector<vector<uint8_t>> _ppuCaptureData;

	template<typename T>
	void ProcessXorState(T& data, deque<RewindData>& prevStates, int32_t position);

public:
	constexpr static size_t FrameBlendChunkPixels=256*1024;
	std::deque<ControlDeviceState> InputLogs[BaseControlDevice::PortCount];
	int32_t FrameCount = 0;
	bool EndOfSegment = false;
	bool IsFullState = false;

	void GetStateData(stringstream& stateData, deque<RewindData>& prevStates, int32_t position);
	uint32_t GetStateSize() {
		uint64_t size=_saveStateData.size();
		for(const auto& chunk:_frameBlendData) { size+=chunk.size(); }
		for(const auto& chunk:_ppuCaptureData) { size+=chunk.size(); }
		return (uint32_t)std::min<uint64_t>(UINT32_MAX,size);
	}

	void LoadState(Emulator* emu, deque<RewindData>& prevStates, int32_t position = -1, bool sendNotification = true);
	void SaveState(Emulator* emu, deque<RewindData>& prevStates, int32_t position = -1);
};

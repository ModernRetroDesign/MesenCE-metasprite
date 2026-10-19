#include "pch.h"
#include "Shared/RewindData.h"
#include "Shared/Emulator.h"
#include "Shared/SaveStateManager.h"
#include "Utilities/CompressionHelper.h"
#include "Shared/Video/VideoDecoder.h"
#include "SNES/SnesConsole.h"
#include "SNES/SnesPpu.h"
#include "Shared/Video/MetaSnesFieldCheckpoint.h"

void RewindData::GetStateData(stringstream& stateData, deque<RewindData>& prevStates, int32_t position)
{
	vector<uint8_t> data;
	CompressionHelper::Decompress(_saveStateData, data);

	if(!IsFullState) {
		position = (position > 0 ? position : (int32_t)prevStates.size()) - 1;
		ProcessXorState(data, prevStates, position);
	}

	stateData.write((char*)data.data(), data.size());
}

template<typename T>
void RewindData::ProcessXorState(T& data, deque<RewindData>& prevStates, int32_t position)
{
	//Find last full state and XOR with it
	while(position >= 0 && position < prevStates.size()) {
		RewindData& prevState = prevStates[position];
		if(prevState.IsFullState) {
			//XOR with previous state to restore state data to its initial state
			if(!prevState._uncompressedData.empty()) {
				for(size_t i = 0, len = std::min(prevState._uncompressedData.size(), data.size()); i < len; i++) {
					data[i] ^= prevState._uncompressedData[i];
				}
			} else {
				vector<uint8_t> prevStateData;
				CompressionHelper::Decompress(prevState._saveStateData, prevStateData);
				for(size_t i = 0, len = std::min(prevStateData.size(), data.size()); i < len; i++) {
					data[i] ^= prevStateData[i];
				}
			}
			break;
		}
		position--;
	}
}

void RewindData::LoadState(Emulator* emu, deque<RewindData>& prevStates, int32_t position, bool sendNotification)
{
	if(_saveStateData.size() == 0) {
		return;
	}

	vector<uint8_t> data;
	CompressionHelper::Decompress(_saveStateData, data);

	if(!IsFullState) {
		position = (position > 0 ? position : (int32_t)prevStates.size()) - 1;
		ProcessXorState(data, prevStates, position);
	}

	stringstream stream;
	stream.write((char*)data.data(), data.size());
	stream.seekg(0, ios::beg);

	emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
	if(emu->Deserialize(stream, SaveStateManager::FileFormatVersion, true, std::nullopt, sendNotification)==DeserializeResult::Success) {
		vector<uint32_t> history;
		for(auto& chunk:_frameBlendData) {
			vector<uint8_t> decoded;
			if(!CompressionHelper::Decompress(chunk,decoded) || decoded.size()%sizeof(uint32_t)) { history.clear(); break; }
			size_t offset=history.size(); history.resize(offset+decoded.size()/sizeof(uint32_t));
			memcpy(history.data()+offset,decoded.data(),decoded.size());
		}
		emu->GetVideoDecoder()->RestoreRewindBlendHistory(std::move(history));
		if(!_ppuCaptureData.empty() && emu->GetConsoleType()==ConsoleType::Snes) {
			vector<uint8_t> capture;
			for(auto& chunk:_ppuCaptureData) {
				vector<uint8_t> decoded;
				if(!CompressionHelper::Decompress(chunk,decoded) || decoded.size()>MetaSnesFieldCheckpoint::MaxBytes-capture.size()) { capture.clear(); break; }
				capture.insert(capture.end(),decoded.begin(),decoded.end());
			}
			if(!capture.empty()) { static_cast<SnesConsole*>(emu->GetConsole().get())->GetPpu()->RestoreReplacementRewindState(capture); }
		}
	}
}

void RewindData::SaveState(Emulator* emu, deque<RewindData>& prevStates, int32_t position)
{
	std::stringstream state;
	emu->Serialize(state, true, 0);
	_ppuCaptureData.clear();
	if(emu->GetConsoleType()==ConsoleType::Snes) {
		auto capture=static_cast<SnesConsole*>(emu->GetConsole().get())->GetPpu()->CaptureReplacementRewindState();
		constexpr size_t ChunkBytes=FrameBlendChunkPixels*sizeof(uint32_t);
		for(size_t offset=0;offset<capture.size();offset+=ChunkBytes) {
			size_t count=std::min(ChunkBytes,capture.size()-offset); vector<uint8_t> chunk;
			CompressionHelper::Compress(string((const char*)(capture.data()+offset),count),1,chunk);
			_ppuCaptureData.push_back(std::move(chunk));
		}
	}
	_frameBlendData.clear();
	auto history=emu->GetVideoDecoder()->CaptureRewindBlendHistory();
	// CompressionHelper bounds each decoded chunk to 10 MiB. Smaller chunks
	// retain that bound even when full-canvas PNG scaling enlarges the frame.
	for(size_t offset=0;offset<history.size();offset+=FrameBlendChunkPixels) {
		size_t count=std::min(FrameBlendChunkPixels,history.size()-offset);
		vector<uint8_t> chunk;
		CompressionHelper::Compress(string((const char*)(history.data()+offset),count*sizeof(uint32_t)),1,chunk);
		_frameBlendData.push_back(std::move(chunk));
	}

	string data = state.str();

	position = position > 0 ? position : (int32_t)prevStates.size();

	if(position > 0 && (position % 30) != 0) {
		position--;
		ProcessXorState(data, prevStates, position);
	} else {
		IsFullState = true;
		while(position > 0) {
			position--;
			RewindData& prevState = prevStates[position];
			if(prevState.IsFullState) {
				//Get rid of previous full state's uncompressed data once the next full state is added
				prevState._uncompressedData = {};
				break;
			}
		}

		//Keep uncompressed data for the next 30 states - this avoids having to decompress the state 30 times
		_uncompressedData = vector<uint8_t>(data.begin(), data.end());
	}

	CompressionHelper::Compress(data, 1, _saveStateData);
	FrameCount = 0;
}

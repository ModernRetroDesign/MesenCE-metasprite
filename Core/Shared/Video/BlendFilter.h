#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

class BlendFilter
{
private:
	uint32_t* _prevFrame = nullptr;
	uint32_t _size = 0;
	bool _enabled = false;
	uint32_t _frameNumber = 0;
	bool _historyValid = false;

	static uint32_t BlendPixels(uint32_t a, uint32_t b)
	{
		return ((((a) ^ (b)) & 0xFFFEFEFEL) >> 1) + ((a) & (b));
	}

public:
	// Own the unblended previous frame, never a pointer into a decoder buffer.
	// Checkpoints must restore this input rather than the displayed average.
	struct History
	{
		uint32_t FrameNumber = 0;
		std::vector<uint32_t> Pixels;
	};

	BlendFilter() {}
	~BlendFilter()
	{
		delete[] _prevFrame;
	}

	bool IsEnabled()
	{
		return _enabled;
	}

	void SetEnabled(bool enabled)
	{
		_enabled = enabled;
	}
	void ResetHistory() { _historyValid = false; }
	History CaptureHistory() const
	{
		History history;
		if(_enabled && _historyValid && _size) {
			history.FrameNumber = _frameNumber;
			history.Pixels.assign(_prevFrame, _prevFrame + _size);
		}
		return history;
	}
	bool RestoreHistory(const History& history)
	{
		ResetHistory();
		if(!_enabled || history.Pixels.empty() || history.Pixels.size() > std::numeric_limits<uint32_t>::max()) { return false; }
		uint32_t size = (uint32_t)history.Pixels.size();
		if(_size != size) {
			delete[] _prevFrame;
			_prevFrame = new uint32_t[size];
			_size = size;
		}
		std::copy(history.Pixels.begin(), history.Pixels.end(), _prevFrame);
		_frameNumber = history.FrameNumber;
		_historyValid = true;
		return true;
	}

	void ApplyFilter(uint32_t* out, uint32_t size, uint32_t frameNumber)
	{
		if(!_enabled) {
			_frameNumber = 0;
			ResetHistory();
			return;
		}

		bool needFill = !_historyValid || _size != size || frameNumber != _frameNumber + 1;
		_frameNumber = frameNumber;
		_historyValid = true;

		if(_size != size) {
			delete[] _prevFrame;
			_prevFrame = new uint32_t[size];
			_size = size;
		}

		if(needFill) {
			//Copy frame buffer without blending, to avoid blending with an old frame
			std::copy(out, out + _size, _prevFrame);
		} else if(_enabled) {
			for(uint32_t i = 0; i < size; i++) {
				uint32_t old = _prevFrame[i];

				//Update copy of the previous frame
				_prevFrame[i] = out[i];

				//Blend new frame
				out[i] = BlendPixels(out[i], old);
			}
		}
	}
};

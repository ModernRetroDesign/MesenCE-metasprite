#pragma once
#include "MetaBackgroundReplacement.h"
#include <stdexcept>
#include <set>

class MetaBackgroundPacket
{
	const uint8_t* _data;
	size_t _length, _offset = 0;
	uint32_t Read()
	{
		if(_length - _offset < 4) { throw std::runtime_error("Truncated background packet"); }
		uint32_t value = uint32_t(_data[_offset]) | (uint32_t(_data[_offset + 1]) << 8)
			| (uint32_t(_data[_offset + 2]) << 16) | (uint32_t(_data[_offset + 3]) << 24);
		_offset += 4;
		return value;
	}
public:
	MetaBackgroundPacket(const uint8_t* data, size_t length) : _data(data), _length(length) {}
	std::vector<MetaBackgroundReplacement> Parse()
	{
		if(!_data || _length > 536870912 || Read() != 0x3147424D) { throw std::runtime_error("Invalid background packet"); }
		uint32_t count = Read();
		if(count > 128) { throw std::runtime_error("Too many background layers"); }
		std::set<int> ids;
		std::vector<MetaBackgroundReplacement> result;
		for(uint32_t i = 0; i < count; i++) {
			MetaBackgroundReplacement layer;
			uint32_t id = Read(), nativeLayer = Read();
			if(id == 0 || id > INT32_MAX || nativeLayer > 7 || !ids.insert(id).second) { throw std::runtime_error("Invalid background identity"); }
			layer.Id = id; layer.Layer = nativeLayer;
			layer.X = static_cast<int32_t>(Read()); layer.Y = static_cast<int32_t>(Read());
			uint32_t flags = Read(), scale = Read(), width = Read(), height = Read();
			if(flags > 7 || scale < 1 || scale > 10 || width < 1 || height < 1 || width > 8192 || height > 8192
				|| width % scale || height % scale) { throw std::runtime_error("Invalid background artwork"); }
			layer.FollowScroll = flags & 1; layer.WrapX = flags & 2; layer.WrapY = flags & 4;
			layer.Scale = scale; layer.Width = width; layer.Height = height;
			size_t pixels = static_cast<size_t>(width) * height;
			if(pixels > (_length - _offset) / 4) { throw std::runtime_error("Invalid background pixels"); }
			layer.Artwork.reserve(pixels);
			for(size_t p = 0; p < pixels; p++) { layer.Artwork.push_back(Read()); }
			result.push_back(std::move(layer));
		}
		if(_offset != _length) { throw std::runtime_error("Trailing background data"); }
		return result;
	}
};

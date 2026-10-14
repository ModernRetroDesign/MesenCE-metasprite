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
		if(!_data || _length > 536870912) { throw std::runtime_error("Invalid background packet"); }
		uint32_t magic = Read();
		if(magic < 0x3147424D || magic > 0x3847424D || (magic & 0xFFFFFF) != 0x47424D) { throw std::runtime_error("Invalid background packet"); }
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
			if(magic >= 0x3447424D) {
				layer.ScrollSpeedX = static_cast<int32_t>(Read()); layer.ScrollSpeedY = static_cast<int32_t>(Read());
				if(layer.ScrollSpeedX < -16000 || layer.ScrollSpeedX > 16000 || layer.ScrollSpeedY < -16000 || layer.ScrollSpeedY > 16000) { throw std::runtime_error("Invalid scroll multiplier"); }
			}
			size_t pixels = static_cast<size_t>(width) * height;
			if(magic >= 0x3547424D) {
				uint32_t ignoreRaster = Read(), reference = Read();
				if(ignoreRaster > 1 || reference > 239) { throw std::runtime_error("Invalid NES raster scroll settings"); }
				layer.IgnoreRasterScroll = ignoreRaster != 0; layer.ScrollReferenceScanline = reference;
			}
			if(magic >= 0x3647424D) { uint32_t hide = Read(); if(hide > 1) { throw std::runtime_error("Invalid hide-layer flag"); } layer.HideOriginalLayer = hide != 0; }
			if(magic >= 0x3747424D) {
				uint32_t camera = Read(); if(camera > 1) { throw std::runtime_error("Invalid CPU camera flag"); } layer.UseCpuCamera = camera != 0;
				for(auto* axis : {&layer.CameraX, &layer.CameraY}) {
					axis->Format = (int32_t)Read(); axis->Address = Read(); axis->PageAddress = Read(); axis->PageSize = (int32_t)Read();
					if(magic >= 0x3847424D) { uint32_t signedPage = Read(); if(signedPage > 1) { throw std::runtime_error("Invalid signed-page flag"); } axis->SignedPage = signedPage != 0; }
					if(!axis->IsValid()) { throw std::runtime_error("Invalid CPU camera axis"); }
				}
			}
			if(pixels > (_length - _offset) / 4) { throw std::runtime_error("Invalid background pixels"); }
			layer.Artwork.reserve(pixels);
			for(size_t p = 0; p < pixels; p++) { layer.Artwork.push_back(Read()); }
			if(magic != 0x3147424D) {
				uint32_t conditions = Read();
				if(conditions > 64) { throw std::runtime_error("Too many background conditions"); }
				for(uint32_t c = 0; c < conditions; c++) {
					uint32_t address = Read(), value = Read(), mask = Read();
					uint32_t comparison = magic >= 0x3347424D ? Read() : 0;
					if(comparison > 5) { throw std::runtime_error("Invalid background comparison operator"); }
					if(value > 255 || mask > 255) { throw std::runtime_error("Invalid background condition byte"); }
					layer.MemoryConditions.push_back({address, static_cast<uint8_t>(value), static_cast<uint8_t>(mask), static_cast<uint8_t>(comparison)});
				}
			}
			result.push_back(std::move(layer));
		}
		if(_offset != _length) { throw std::runtime_error("Trailing background data"); }
		return result;
	}
};

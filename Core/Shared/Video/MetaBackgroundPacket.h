#pragma once
#include "MetaBackgroundReplacement.h"
#include <stdexcept>
#include <set>

class MetaBackgroundPacket
{
	const uint8_t* _data;
	size_t _length, _offset = 0;
	MetaPngPixelPool _localPool;
	MetaPngPixelPool* _pixelPool;
	uint32_t Read()
	{
		if(_length - _offset < 4) { throw std::runtime_error("Truncated background packet"); }
		uint32_t value = uint32_t(_data[_offset]) | (uint32_t(_data[_offset + 1]) << 8)
			| (uint32_t(_data[_offset + 2]) << 16) | (uint32_t(_data[_offset + 3]) << 24);
		_offset += 4;
		return value;
	}
public:
	MetaBackgroundPacket(const uint8_t* data, size_t length, MetaPngPixelPool* pool = nullptr) : _data(data), _length(length), _pixelPool(pool ? pool : &_localPool) {}
	std::vector<MetaBackgroundReplacement> Parse()
	{
		if(!_data || _length > 536870912) { throw std::runtime_error("Invalid background packet"); }
		uint32_t magic = Read();
		if(((magic < 0x3147424D || magic > 0x3947424D) && (magic < 0x4147424D || magic > 0x4847424D)) || (magic & 0xFFFFFF) != 0x47424D) { throw std::runtime_error("Invalid background packet"); }
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
			if(magic >= 0x3947424D) { uint32_t extend = Read(); if(extend > 1) { throw std::runtime_error("Invalid margin-extension flag"); } layer.ExtendIntoMargins = extend != 0; }
			if(magic >= 0x4147424D) {
				uint32_t use = Read(); if(use > 1) { throw std::runtime_error("Invalid positioning flag"); } layer.UseWidescreenPosition = use != 0;
				layer.WidescreenPosition = (int32_t)Read(); layer.WidescreenCameraMin = (int32_t)Read(); layer.WidescreenCameraMax = (int32_t)Read(); layer.WidescreenSlideSpeed = (int32_t)Read();
				if(magic >= 0x4247424D) { layer.WidescreenSlideSpeed /= 1000.0; layer.WidescreenReleaseDelay = Read(); if(layer.WidescreenReleaseDelay > 600) { throw std::runtime_error("Invalid boundary release delay"); } }
				uint32_t flagsMode = Read(); if(flagsMode > 1) { throw std::runtime_error("Invalid boundary source"); } layer.WidescreenUseRamFlags = flagsMode != 0;
				if(layer.WidescreenPosition < 0 || layer.WidescreenPosition > 3 || layer.WidescreenSlideSpeed < 0 || layer.WidescreenSlideSpeed > 64
					|| layer.WidescreenCameraMin < -16777216 || layer.WidescreenCameraMin > 16777216 || layer.WidescreenCameraMax < -16777216 || layer.WidescreenCameraMax > 16777216) { throw std::runtime_error("Invalid positioning settings"); }
				for(auto* conditions : {&layer.WidescreenLeftFlags, &layer.WidescreenRightFlags}) {
					uint32_t n = Read(); if(n > 8) { throw std::runtime_error("Too many PNG boundary conditions"); }
					for(uint32_t c = 0; c < n; c++) { uint32_t address = Read(), value = Read(), mask = Read(), op = Read(); if(value > 255 || mask > 255 || op > 5) { throw std::runtime_error("Invalid PNG boundary condition"); } conditions->push_back({address, (uint8_t)value, (uint8_t)mask, (uint8_t)op}); }
				}
				if(layer.UseWidescreenPosition && layer.WidescreenUseRamFlags && layer.WidescreenLeftFlags.empty() && layer.WidescreenRightFlags.empty()) { throw std::runtime_error("Missing PNG boundary conditions"); }
				if(magic >= 0x4347424D) {
					auto& speed = layer.WidescreenRamSpeed;
					uint32_t enabled = Read(); if(enabled > 1) { throw std::runtime_error("Invalid RAM speed flag"); } speed.Enabled = enabled != 0;
					speed.Format = (int32_t)Read(); speed.Address = Read(); speed.Multiplier = Read() / 1000000.0; speed.Maximum = Read() / 1000.0;
					if(!speed.IsValid()) { throw std::runtime_error("Invalid RAM speed settings"); }
				}
			}
			if(magic >= 0x4447424D) { uint32_t position = Read(); if(position > (magic >= 0x4747424D ? 6u : magic >= 0x4647424D ? 3u : magic >= 0x4547424D ? 2u : 1u)) { throw std::runtime_error("Invalid PNG composition position"); } layer.CompositionPosition = position; }
			if(magic >= 0x4547424D) { uint32_t priority = Read(); if(priority > 3) { throw std::runtime_error("Invalid GBA PNG priority"); } layer.GbaPriority = priority; }
			if(magic >= 0x4647424D) { uint32_t priority = Read(); if(priority < 1 || priority > 15) { throw std::runtime_error("Invalid SNES PNG priority"); } layer.SnesPriority = priority; }
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
			if(magic == 0x4847424D) {
				uint32_t animationCount=Read(); if(animationCount>256) { throw std::runtime_error("Too many background animation frames"); }
				if(animationCount) {
					uint32_t mode=Read(), restart=Read(); if(mode>2 || restart>1) { throw std::runtime_error("Unsupported background animation settings"); }
					layer.Animation.RestartOnActivation=restart!=0;
					std::vector<uint32_t> durations;
					for(uint32_t f=0;f<animationCount;f++) {
						uint32_t duration=Read(); if(duration<1 || duration>3600000) { throw std::runtime_error("Invalid background frame duration"); }
						durations.push_back(duration); MetaPngArtworkFrame image;
						image.OriginX=(int32_t)Read(); image.OriginY=(int32_t)Read();
						uint32_t w=Read(), h=Read();
						if(image.OriginX < -4096 || image.OriginX > 4096 || image.OriginY < -4096 || image.OriginY > 4096
							|| w<1 || w>8192 || h<1 || h>8192 || w%scale || h%scale || (size_t)w*h > (_length-_offset)/4) { throw std::runtime_error("Invalid background animation canvas"); }
						image.Width=w; image.Height=h; std::vector<uint32_t> animationPixels; animationPixels.reserve((size_t)w*h);
						for(size_t p=0;p<(size_t)w*h;p++) { animationPixels.push_back(Read()); }
						image.Pixels=_pixelPool->Intern(std::move(animationPixels));
						layer.Animation.Frames.push_back(std::move(image));
					}
					layer.Animation.Timeline=MetaPngTimeline((MetaPngPlayback)mode,durations);
				}
			}
			result.push_back(std::move(layer));
		}
		if(_offset != _length) { throw std::runtime_error("Trailing background data"); }
		auto identity=MetaPngPacketIdentity(_data,_length);
		for(auto& layer : result) { layer.Animation.SourceIdentity=identity; }
		return result;
	}
};

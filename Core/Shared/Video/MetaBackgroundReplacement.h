#pragma once
#include "MetaSpriteFrame.h"
#include "MetaPngAnimation.h"
#include <array>
#include <vector>
#include <functional>
#include <cstddef>
#include <cmath>

struct MetaSlideRamSpeed
{
	bool Enabled = false;
	// Signed/unsigned 8-bit, signed/unsigned 16-bit LE, signed/unsigned 16-bit BE.
	int Format = 0;
	uint32_t Address = 0;
	double Multiplier = 0.0625, Maximum = 4;
	bool IsValid() const { return Format >= 0 && Format <= 5 && (Format < 2 || Address != UINT32_MAX)
		&& std::isfinite(Multiplier) && Multiplier >= 0 && Multiplier <= 64 && std::isfinite(Maximum) && Maximum >= 0 && Maximum <= 64; }
	bool Decode(const MetaMemorySnapshot& memory, double& speed, bool signedVelocity = false) const {
		speed = 0; if(!IsValid()) { return false; }
		auto first = memory.find(Address); if(first == memory.end()) { return false; }
		int value = first->second;
		if(Format >= 2) {
			auto second = memory.find(Address + 1); if(second == memory.end()) { return false; }
			value = Format >= 4 ? value * 256 + second->second : value + second->second * 256;
			if((Format == 2 || Format == 4) && value >= 32768) { value -= 65536; }
		} else if(Format == 0 && value >= 128) { value -= 256; }
		speed = std::min(Maximum, std::abs(value) * Multiplier);
		if(signedVelocity && value < 0) { speed = -speed; }
		return true;
	}
};

struct MetaCameraAxis
{
	// 0 fixed, 1 byte, 2 little endian, 3 big endian, 4 pixel offset + page.
	int Format = 0;
	uint32_t Address = 0, PageAddress = 0;
	int PageSize = 256;
	bool SignedPage = false;
	bool IsValid() const { return Format >= 0 && Format <= 4 && PageSize >= 1 && PageSize <= 65536 && !((Format == 2 || Format == 3) && Address == UINT32_MAX); }
	void AddAddresses(std::vector<uint32_t>& addresses) const {
		if(!Format) { return; } addresses.push_back(Address);
		if(Format == 2 || Format == 3) { addresses.push_back(Address + 1); }
		if(Format == 4) { addresses.push_back(PageAddress); }
	}
	bool Decode(const std::unordered_map<uint32_t, uint8_t>& memory, int& value) const {
		value = 0; if(!Format) { return true; }
		auto low = memory.find(Address); if(low == memory.end()) { return false; }
		value = low->second; if(Format == 1) { return true; }
		auto high = memory.find(Format == 4 ? PageAddress : Address + 1); if(high == memory.end()) { return false; }
		int page = high->second;
		if(Format == 4 && SignedPage && page >= 128) { page -= 256; }
		value = Format == 3 ? low->second * 256 + high->second : page * (Format == 4 ? PageSize : 256) + low->second;
		return true;
	}
};

struct MetaBackgroundReplacement
{
	int Id = 0, Layer = 0;
	int CompositionPosition = 0; // 0 native layer, 1 final display overlay, 2 GBA priority
	int GbaPriority = 0;
	int SnesPriority = 1;
	int X = 0, Y = 0;
	int Width = 0, Height = 0, Scale = 1;
	bool FollowScroll = true, WrapX = false, WrapY = false;
	int ScrollSpeedX = 1000, ScrollSpeedY = 1000;
	bool IgnoreRasterScroll = false;
	bool HideOriginalLayer = false;
	bool ExtendIntoMargins = false;
	bool UseWidescreenPosition = false, WidescreenUseRamFlags = false;
	int WidescreenPosition = 3, WidescreenCameraMin = 0, WidescreenCameraMax = 1024;
	double WidescreenSlideSpeed = 0;
	uint32_t WidescreenReleaseDelay = 0;
	MetaSlideRamSpeed WidescreenRamSpeed;
	std::vector<MetaMemoryCondition> WidescreenLeftFlags, WidescreenRightFlags;
	bool UseCpuCamera = false;
	MetaCameraAxis CameraX, CameraY;
	int ScrollReferenceScanline = 32;
	std::vector<uint32_t> Artwork;
	std::vector<MetaMemoryCondition> MemoryConditions;
	MetaPngArtworkAnimation Animation;
	static int64_t WrapCoordinate(int64_t coordinate, int dimension)
	{
		// Power-of-two canvases are common. Unsigned masking also handles
		// negative placement without a signed remainder or correction branch.
		if((dimension & (dimension - 1)) == 0) { return static_cast<uint64_t>(coordinate) & static_cast<uint64_t>(dimension - 1); }
		coordinate %= dimension;
		return coordinate < 0 ? coordinate + dimension : coordinate;
	}

	bool IsValid() const
	{
		return Id > 0 && Layer >= 0 && Layer <= 7 && Scale >= 1 && Scale <= 10
			&& CompositionPosition >= 0 && CompositionPosition <= 6 && GbaPriority >= 0 && GbaPriority <= 3 && SnesPriority >= 1 && SnesPriority <= 15
			&& Width > 0 && Height > 0 && Width <= 8192 && Height <= 8192
			&& Width % Scale == 0 && Height % Scale == 0
			&& ScrollSpeedX >= -16000 && ScrollSpeedX <= 16000 && ScrollSpeedY >= -16000 && ScrollSpeedY <= 16000
			&& ScrollReferenceScanline >= 0 && ScrollReferenceScanline <= 239
			&& CameraX.IsValid() && CameraY.IsValid()
			&& (!UseWidescreenPosition || (WidescreenPosition >= 0 && WidescreenPosition <= 3 && WidescreenSlideSpeed >= 0 && WidescreenSlideSpeed <= 64 && WidescreenReleaseDelay <= 600
				&& WidescreenCameraMin >= -16777216 && WidescreenCameraMin <= 16777216 && WidescreenCameraMax >= -16777216 && WidescreenCameraMax <= 16777216
				&& WidescreenRamSpeed.IsValid() && WidescreenLeftFlags.size() <= 8 && WidescreenRightFlags.size() <= 8
				&& (!WidescreenUseRamFlags || !WidescreenLeftFlags.empty() || !WidescreenRightFlags.empty())))
			&& Artwork.size() == static_cast<size_t>(Width) * Height;
	}

	bool Sample(const MetaBackgroundSample& source, int screenX, int screenY,
		int subX, int subY, int outputScale, uint32_t& color) const
	{
		if(!IsValid()) { color = 0; return false; }
		return SampleValidated(source, screenX, screenY, subX, subY, outputScale, color);
	}
	// The caller must validate the immutable layer before entering its pixel loop.
	bool SampleValidated(const MetaBackgroundSample& source, int screenX, int screenY,
		int subX, int subY, int outputScale, uint32_t& color, const MetaPngArtworkFrame* image = nullptr) const
	{
		color = 0;
		if(!source.Valid || source.Layer != Layer || outputScale < 1
			|| source.PixelScaleX < 1 || source.PixelScaleY < 1
			|| source.PixelOffsetX >= source.PixelScaleX || source.PixelOffsetY >= source.PixelScaleY
			|| subX < 0 || subY < 0 || subX >= outputScale || subY >= outputScale) { return false; }
		auto floorDivide = [](int value, int divisor) { return value / divisor - (value % divisor < 0 ? 1 : 0); };
		int64_t x = static_cast<int64_t>(FollowScroll ? source.X : floorDivide(screenX, source.PixelScaleX)) - X;
		int64_t y = static_cast<int64_t>(FollowScroll ? source.Y : floorDivide(screenY, source.PixelScaleY)) - Y;
		if(image) { x += image->OriginX; y += image->OriginY; }
		auto scaledOffset = [](int offset, int rate) {
			int64_t value = (int64_t)offset * (rate - 1000);
			return value / 1000 - (value % 1000 < 0 ? 1 : 0);
		};
		if(FollowScroll) {
			if(ScrollSpeedX != 1000 && source.ScrollWrapX > 0) { x += source.ScrollX + WrapCoordinate((int64_t)source.X - source.ScrollX, source.ScrollWrapX) - source.X; }
			if(ScrollSpeedY != 1000 && source.ScrollWrapY > 0) { y += source.ScrollY + WrapCoordinate((int64_t)source.Y - source.ScrollY, source.ScrollWrapY) - source.Y; }
			x += scaledOffset(source.ScrollX, ScrollSpeedX); y += scaledOffset(source.ScrollY, ScrollSpeedY);
		}
		int imageWidth = image ? image->Width : Width, imageHeight = image ? image->Height : Height;
		int nativeWidth = imageWidth / Scale, nativeHeight = imageHeight / Scale;
		if(WrapX) { x = WrapCoordinate(x, nativeWidth); }
		if(WrapY) { y = WrapCoordinate(y, nativeHeight); }
		if(x < 0 || y < 0 || x >= nativeWidth || y >= nativeHeight) { return false; }
		x = x * Scale + (static_cast<int64_t>(source.PixelOffsetX) * outputScale + subX) * Scale / (source.PixelScaleX * outputScale);
		y = y * Scale + (static_cast<int64_t>(source.PixelOffsetY) * outputScale + subY) * Scale / (source.PixelScaleY * outputScale);
		const auto& pixels = image ? *image->Pixels : Artwork;
		color = pixels[static_cast<size_t>(y) * imageWidth + static_cast<size_t>(x)];
		return true;
	}
};

// References immutable artwork; filtering a frame must not copy PNG buffers.
struct MetaBackgroundView
{
	std::vector<const MetaBackgroundReplacement*> Layers;
	std::unordered_map<const MetaBackgroundReplacement*, MetaBackgroundSample> RasterReferences;
	std::unordered_map<const MetaBackgroundReplacement*, MetaBackgroundSample> CameraReferences;
	std::unordered_map<const MetaBackgroundReplacement*, std::array<const MetaPngArtworkFrame*,2>> AnimationFrames;
	bool WovenAnimation = false;
	bool RasterPrepared = false;
	bool SnesIndependentPriority = false;
	bool HasSnesIndependentPriority() const {
		if(RasterPrepared) { return SnesIndependentPriority; }
		for(const auto* layer : Layers) { if(layer->CompositionPosition == 3) { return true; } }
		return false;
	}
	uint8_t HiddenNativeLayers = 0;
	bool HidesNativeLayer(int layer) const {
		if(layer < 0 || layer > 7) { return false; }
		if(RasterPrepared) { return HiddenNativeLayers & (1 << layer); }
		for(const auto* background : Layers) { if(background->Layer == layer && background->HideOriginalLayer && background->IsValid()) { return true; } }
		return false;
	}
	void PrepareRasterReferences(const MetaSpriteFrame& frame)
	{
		RasterReferences.clear();
		CameraReferences.clear();
		AnimationFrames.clear(); WovenAnimation = frame.SnesFieldMemoryCaptured;
		RasterPrepared = true;
		HiddenNativeLayers = 0;
		SnesIndependentPriority = false;
		if(frame.Pixels.size() != (uint64_t)frame.Width * frame.Height) { return; }
		for(const auto* layer : Layers) {
			if(!layer->Animation.Frames.empty()) {
				std::array<const MetaPngArtworkFrame*,2> selected;
				for(int field=0;field<2;field++) { selected[field]=layer->Animation.Select(WovenAnimation ? frame.SnesFieldFrameNumber[field] : frame.FrameNumber, WovenAnimation ? frame.SnesFieldAnimationTiming[field].get() : frame.AnimationTiming.get()); }
				AnimationFrames.emplace(layer,selected);
			}
			SnesIndependentPriority |= layer->CompositionPosition == 3;
			if(layer->UseCpuCamera && layer->FollowScroll) {
				MetaBackgroundSample camera;
				camera.Valid = layer->CameraX.Decode(frame.MemorySnapshot, camera.ScrollX) && layer->CameraY.Decode(frame.MemorySnapshot, camera.ScrollY);
				CameraReferences.emplace(layer, camera);
				if(!camera.Valid) { continue; }
			}
			if(layer->HideOriginalLayer && layer->IsValid()) { HiddenNativeLayers |= (uint8_t)(1 << layer->Layer); }
			if(!layer->IgnoreRasterScroll || !layer->FollowScroll || layer->ScrollReferenceScanline < 0 || (uint32_t)layer->ScrollReferenceScanline >= frame.Height) { continue; }
			for(uint32_t x = 0; x < frame.Width; x++) {
				const auto& sample = frame.Pixels[(size_t)layer->ScrollReferenceScanline * frame.Width + x].BackgroundSample;
				if(sample.Valid && sample.NesRasterScroll && sample.Layer == layer->Layer) { RasterReferences.emplace(layer, sample); break; }
			}
		}
	}
	bool Sample(const MetaBackgroundReplacement& layer, const MetaBackgroundSample& source, int x, int y, int sx, int sy, int scale, uint32_t& color, bool independent = false) const
	{
		if(layer.CompositionPosition != 0 && !independent) { color = 0; return false; }
		const MetaPngArtworkFrame* image=nullptr;
		if(!layer.Animation.Frames.empty()) {
			auto selected=AnimationFrames.find(&layer);
			image=selected==AnimationFrames.end() ? (RasterPrepared ? nullptr : layer.Animation.Select(0)) : selected->second[WovenAnimation ? (y & 1) : 0];
		}
		if(layer.UseCpuCamera && layer.FollowScroll) {
			auto reference = CameraReferences.find(&layer);
			if(reference == CameraReferences.end() || !reference->second.Valid) { color = 0; return false; }
			MetaBackgroundSample camera = source;
			camera.ScrollX = reference->second.ScrollX; camera.ScrollY = reference->second.ScrollY;
			auto floorDivide = [](int value, int divisor) { return value / divisor - (value % divisor < 0 ? 1 : 0); };
			camera.X = floorDivide(x, std::max(1, (int)source.PixelScaleX)) + camera.ScrollX;
			camera.Y = floorDivide(y, std::max(1, (int)source.PixelScaleY)) + camera.ScrollY;
			camera.ScrollWrapX = camera.ScrollWrapY = 0;
			return layer.SampleValidated(camera, x, y, sx, sy, scale, color,image);
		}
		if(!layer.IgnoreRasterScroll || !source.NesRasterScroll) { return layer.SampleValidated(source, x, y, sx, sy, scale, color,image); }
		auto reference = RasterReferences.find(&layer);
		if(reference == RasterReferences.end()) { return layer.SampleValidated(source, x, y, sx, sy, scale, color,image); }
		MetaBackgroundSample uniform = source;
		uniform.ScrollX = reference->second.ScrollX; uniform.ScrollY = reference->second.ScrollY;
		uniform.ScrollWrapX = reference->second.ScrollWrapX; uniform.ScrollWrapY = reference->second.ScrollWrapY;
		uniform.X = x + uniform.ScrollX; uniform.Y = y + uniform.ScrollY;
		if(uniform.ScrollWrapX > 0) { uniform.X = (int)MetaBackgroundReplacement::WrapCoordinate(uniform.X, uniform.ScrollWrapX); }
		if(uniform.ScrollWrapY > 0) { uniform.Y = (int)MetaBackgroundReplacement::WrapCoordinate(uniform.Y, uniform.ScrollWrapY); }
		return layer.SampleValidated(uniform, x, y, sx, sy, scale, color,image);
	}
	struct Iterator {
		// The view stays immutable during composition. Pointer iteration avoids
		// checked STL iterator construction for each output pixel in Debug.
		const MetaBackgroundReplacement* const* Position;
		const MetaBackgroundReplacement& operator*() const { return **Position; }
		Iterator& operator++() { ++Position; return *this; }
		bool operator!=(const Iterator& other) const { return Position != other.Position; }
	};
	MetaBackgroundView() = default;
	MetaBackgroundView(const std::vector<MetaBackgroundReplacement>& layers) { for(const auto& layer : layers) { Layers.push_back(&layer); } }
	MetaBackgroundView(std::initializer_list<MetaBackgroundReplacement> layers) { for(const auto& layer : layers) { Layers.push_back(&layer); } }
	Iterator begin() const { return {Layers.data()}; }
	Iterator end() const { return {Layers.empty() ? Layers.data() : Layers.data() + Layers.size()}; }
};

// For direct-color frames whose background sample identifies the winning layer.
// Systems with main/subscreen or blend effects need their own composition adapters.
class MetaBackgroundCompositor
{
public:
	static bool Apply(const MetaSpriteFrame& frame, const MetaBackgroundView& layers,
		int outputScale, std::vector<uint32_t>& output, int screenLeft = 0, int screenTop = 0, const std::function<uint32_t(uint16_t)>& convertColor = {})
	{
		return ApplyImpl(frame, layers, outputScale, output, true, screenLeft, screenTop, convertColor);
	}

	static bool BuildFallback(const MetaSpriteFrame& frame, const MetaBackgroundView& layers,
		int outputScale, const std::function<uint32_t(uint16_t)>& convertColor, std::vector<uint32_t>& output, int screenLeft = 0, int screenTop = 0)
	{
		if(!convertColor || outputScale < 1 || outputScale > 10 || frame.Width == 0 || frame.Height == 0
			|| frame.GbaColorEffects || frame.SnesColorEffects || frame.Pixels.size() != static_cast<uint64_t>(frame.Width) * frame.Height) { return false; }
		std::vector<uint32_t> fallback(static_cast<size_t>(frame.Width) * frame.Height * outputScale * outputScale);
		size_t width = static_cast<size_t>(frame.Width) * outputScale;
		for(uint32_t y = 0; y < frame.Height; y++) {
			for(uint32_t x = 0; x < frame.Width; x++) {
				const auto& pixel = frame.Pixels[static_cast<size_t>(y) * frame.Width + x];
				uint32_t color = convertColor(pixel.NativeBackdropCaptured && pixel.BackgroundSample.Valid && layers.HidesNativeLayer(pixel.BackgroundSample.Layer) ? pixel.NativeBackdropColor : pixel.BackgroundColor);
				for(int sy = 0; sy < outputScale; sy++) {
					for(int sx = 0; sx < outputScale; sx++) { fallback[(static_cast<size_t>(y) * outputScale + sy) * width + x * outputScale + sx] = color; }
				}
			}
		}
		if(!ApplyImpl(frame, layers, outputScale, fallback, false, screenLeft, screenTop, convertColor)) { return false; }
		output.swap(fallback);
		return true;
	}

private:
	static bool ApplyImpl(const MetaSpriteFrame& frame, const MetaBackgroundView& layers,
		int outputScale, std::vector<uint32_t>& output, bool preserveSprites, int screenLeft, int screenTop, const std::function<uint32_t(uint16_t)>& convertColor)
	{
		if(outputScale < 1 || outputScale > 10 || frame.GbaColorEffects || frame.SnesColorEffects
			|| frame.Width == 0 || frame.Height == 0
			|| frame.Pixels.size() != static_cast<uint64_t>(frame.Width) * frame.Height
			|| output.size() != static_cast<uint64_t>(frame.Width) * frame.Height * outputScale * outputScale) { return false; }
		size_t outputWidth = static_cast<size_t>(frame.Width) * outputScale;
		MetaBackgroundView prepared = layers; if(!prepared.RasterPrepared) { prepared.PrepareRasterReferences(frame); }
		const MetaSpritePixel* framePixels = frame.Pixels.data();
		uint32_t* outputPixels = output.data();
		for(uint32_t y = 0; y < frame.Height; y++) {
			for(uint32_t x = 0; x < frame.Width; x++) {
				const auto& pixel = framePixels[static_cast<size_t>(y) * frame.Width + x];
				const auto* pair = frame.BackgroundUnderlays.size() == frame.Pixels.size() ? &frame.BackgroundUnderlays[(size_t)y * frame.Width + x] : nullptr;
				if(convertColor && pair && pair->ForegroundSample.Valid) {
					for(int sy = 0; sy < outputScale; sy++) { for(int sx = 0; sx < outputScale; sx++) {
						size_t offset = ((size_t)y * outputScale + sy) * outputWidth + x * outputScale + sx;
						uint32_t result = convertColor(pixel.NativeBackdropCaptured ? pixel.NativeBackdropColor : pixel.BackgroundColor);
						auto overlay = [&](const MetaBackgroundSample& sample) {
							for(const auto& layer : layers) {
								uint32_t artwork;
								if(!layer.IsValid() || !prepared.Sample(layer, sample, x + screenLeft, y + screenTop, sx, sy, outputScale, artwork)) { continue; }
								uint32_t alpha = artwork >> 24, blended = 0xFF000000;
								for(int shift : {0, 8, 16}) { blended |= ((((artwork >> shift) & 255) * alpha + ((result >> shift) & 255) * (255 - alpha) + 127) / 255) << shift; }
								result = blended;
							}
						};
						if(pair->Sample.Valid) { if(!prepared.HidesNativeLayer(pair->Sample.Layer)) { result = convertColor(pair->Color); } overlay(pair->Sample); }
						bool nativeObject = preserveSprites && pixel.VisibleSprite >= 0;
						if(nativeObject && pixel.NativeVisibleSpriteClass == 2) { result = convertColor(pixel.SpriteColor); }
						if(pair->ForegroundOpaque && !prepared.HidesNativeLayer(pair->ForegroundSample.Layer)) { result = convertColor(pair->ForegroundColor); }
						overlay(pair->ForegroundSample);
						if(nativeObject && pixel.NativeVisibleSpriteClass != 2) { result = convertColor(pixel.SpriteColor); }
						outputPixels[offset] = result;
					} }
					continue;
				}
				if((preserveSprites && pixel.VisibleSprite >= 0 && !(pixel.BackgroundOverlayOccludesClasses & pixel.NativeVisibleSpriteClass)) || !pixel.BackgroundSample.Valid) { continue; }
				for(int sy = 0; sy < outputScale; sy++) {
					for(int sx = 0; sx < outputScale; sx++) {
						size_t offset = (static_cast<size_t>(y) * outputScale + sy) * outputWidth + x * outputScale + sx;
						if(convertColor && pixel.NativeBackdropCaptured && prepared.HidesNativeLayer(pixel.BackgroundSample.Layer)) {
							outputPixels[offset] = convertColor(pixel.NativeBackdropColor);
							if(frame.BackgroundUnderlays.size() == frame.Pixels.size()) {
								const auto& lower = frame.BackgroundUnderlays[(size_t)y * frame.Width + x];
								if(lower.Sample.Valid) {
									if(!prepared.HidesNativeLayer(lower.Sample.Layer)) { outputPixels[offset] = convertColor(lower.Color); }
									for(const auto& layer : layers) {
										uint32_t artwork;
										if(!layer.IsValid() || !prepared.Sample(layer, lower.Sample, x + screenLeft, y + screenTop, sx, sy, outputScale, artwork)) { continue; }
										uint32_t alpha = artwork >> 24, result = 0xFF000000;
										for(int shift : {0, 8, 16}) { result |= ((((artwork >> shift) & 255) * alpha + ((outputPixels[offset] >> shift) & 255) * (255 - alpha) + 127) / 255) << shift; }
										outputPixels[offset] = result;
									}
								}
							}
						}
						for(const auto& layer : layers) {
							uint32_t color;
							if(!layer.IsValid() || !prepared.Sample(layer, pixel.BackgroundSample, x + screenLeft, y + screenTop, sx, sy, outputScale, color)) { continue; }
							uint32_t alpha = color >> 24, result = 0xFF000000;
							if(alpha == 255) { outputPixels[offset] = color; continue; }
							if(alpha == 0) { outputPixels[offset] |= 0xFF000000; continue; }
							for(int shift : {0, 8, 16}) {
								result |= ((((color >> shift) & 255) * alpha + ((outputPixels[offset] >> shift) & 255) * (255 - alpha) + 127) / 255) << shift;
							}
							outputPixels[offset] = result;
							// Later authored PNGs compose over earlier PNGs, as in GBA/SNES.
						}
					}
				}
			}
		}
		return true;
	}
};

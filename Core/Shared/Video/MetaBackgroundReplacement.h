#pragma once
#include "MetaSpriteFrame.h"
#include <vector>
#include <functional>
#include <cstddef>

struct MetaBackgroundReplacement
{
	int Id = 0, Layer = 0;
	int X = 0, Y = 0;
	int Width = 0, Height = 0, Scale = 1;
	bool FollowScroll = true, WrapX = false, WrapY = false;
	std::vector<uint32_t> Artwork;
	std::vector<MetaMemoryCondition> MemoryConditions;
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
			&& Width > 0 && Height > 0 && Width <= 8192 && Height <= 8192
			&& Width % Scale == 0 && Height % Scale == 0
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
		int subX, int subY, int outputScale, uint32_t& color) const
	{
		color = 0;
		if(!source.Valid || source.Layer != Layer || outputScale < 1
			|| source.PixelScaleX < 1 || source.PixelScaleY < 1
			|| source.PixelOffsetX >= source.PixelScaleX || source.PixelOffsetY >= source.PixelScaleY
			|| subX < 0 || subY < 0 || subX >= outputScale || subY >= outputScale) { return false; }
		int64_t x = static_cast<int64_t>(FollowScroll ? source.X : screenX / source.PixelScaleX) - X;
		int64_t y = static_cast<int64_t>(FollowScroll ? source.Y : screenY / source.PixelScaleY) - Y;
		int nativeWidth = Width / Scale, nativeHeight = Height / Scale;
		if(WrapX) { x = WrapCoordinate(x, nativeWidth); }
		if(WrapY) { y = WrapCoordinate(y, nativeHeight); }
		if(x < 0 || y < 0 || x >= nativeWidth || y >= nativeHeight) { return false; }
		x = x * Scale + (static_cast<int64_t>(source.PixelOffsetX) * outputScale + subX) * Scale / (source.PixelScaleX * outputScale);
		y = y * Scale + (static_cast<int64_t>(source.PixelOffsetY) * outputScale + subY) * Scale / (source.PixelScaleY * outputScale);
		color = Artwork[static_cast<size_t>(y) * Width + static_cast<size_t>(x)];
		return true;
	}
};

// References immutable artwork; filtering a frame must not copy PNG buffers.
struct MetaBackgroundView
{
	std::vector<const MetaBackgroundReplacement*> Layers;
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
		int outputScale, std::vector<uint32_t>& output, int screenLeft = 0, int screenTop = 0)
	{
		return ApplyImpl(frame, layers, outputScale, output, true, screenLeft, screenTop);
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
				uint32_t color = convertColor(frame.Pixels[static_cast<size_t>(y) * frame.Width + x].BackgroundColor);
				for(int sy = 0; sy < outputScale; sy++) {
					for(int sx = 0; sx < outputScale; sx++) { fallback[(static_cast<size_t>(y) * outputScale + sy) * width + x * outputScale + sx] = color; }
				}
			}
		}
		if(!ApplyImpl(frame, layers, outputScale, fallback, false, screenLeft, screenTop)) { return false; }
		output.swap(fallback);
		return true;
	}

private:
	static bool ApplyImpl(const MetaSpriteFrame& frame, const MetaBackgroundView& layers,
		int outputScale, std::vector<uint32_t>& output, bool preserveSprites, int screenLeft, int screenTop)
	{
		if(outputScale < 1 || outputScale > 10 || frame.GbaColorEffects || frame.SnesColorEffects
			|| frame.Width == 0 || frame.Height == 0
			|| frame.Pixels.size() != static_cast<uint64_t>(frame.Width) * frame.Height
			|| output.size() != static_cast<uint64_t>(frame.Width) * frame.Height * outputScale * outputScale) { return false; }
		size_t outputWidth = static_cast<size_t>(frame.Width) * outputScale;
		const MetaSpritePixel* framePixels = frame.Pixels.data();
		uint32_t* outputPixels = output.data();
		for(uint32_t y = 0; y < frame.Height; y++) {
			for(uint32_t x = 0; x < frame.Width; x++) {
				const auto& pixel = framePixels[static_cast<size_t>(y) * frame.Width + x];
				if((preserveSprites && pixel.VisibleSprite >= 0) || !pixel.BackgroundSample.Valid) { continue; }
				for(int sy = 0; sy < outputScale; sy++) {
					for(int sx = 0; sx < outputScale; sx++) {
						size_t offset = (static_cast<size_t>(y) * outputScale + sy) * outputWidth + x * outputScale + sx;
						for(const auto& layer : layers) {
							uint32_t color;
							if(!layer.Sample(pixel.BackgroundSample, x + screenLeft, y + screenTop, sx, sy, outputScale, color)) { continue; }
							uint32_t alpha = color >> 24, result = 0xFF000000;
							if(alpha == 255) { outputPixels[offset] = color; break; }
							if(alpha == 0) { outputPixels[offset] |= 0xFF000000; break; }
							for(int shift : {0, 8, 16}) {
								result |= ((((color >> shift) & 255) * alpha + ((outputPixels[offset] >> shift) & 255) * (255 - alpha) + 127) / 255) << shift;
							}
							outputPixels[offset] = result;
							break; // Authored order determines overlapping replacement priority.
						}
					}
				}
			}
		}
		return true;
	}
};

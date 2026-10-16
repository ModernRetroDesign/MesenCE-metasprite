#pragma once
#include "MetaBackgroundReplacement.h"
#include <cmath>

struct DecorativeWidescreenOptions
{
	int HomeLeft = -1; // Custom margin width in output pixels; -1 centres
	uint32_t FadeWidth = 0; // Output pixels from the gameplay edge to side brightness
	uint32_t CameraBackgroundId = 1;
	int CameraMin = 0, CameraMax = 1024;
	double SlideSpeed = 0; // Output pixels/emulated frame; zero is instant
	uint32_t ReleaseDelay = 0; // Consecutive inactive frames before returning home
	MetaSlideRamSpeed RamSpeed;
	double SpeedScale = 1; // Native RAM speed to output pixels
	bool UseRamBoundaryFlags = false;
	std::vector<MetaMemoryCondition> LeftFlags, RightFlags;
	uint32_t PositionKey = 0;
};

// Scenery only: never invent native tiles, objects, or game logic outside the
// captured viewport. Retain reusable storage; PNG pixels stay in the pack.
class DecorativeWidescreen
{
	std::vector<uint32_t> _output;
	uint32_t _lastFrame = 0, _lastExtra = 0, _lastPosition = 0;
	double _left = 0;
	uint32_t _heldTarget = 0, _releaseFrames = 0;
	bool _positionValid = false;
	uint32_t _lastPositionKey = 0;
	static bool FindReference(const MetaSpriteFrame& frame, const MetaBackgroundReplacement& layer, int y,
		int cropLeft, int count, MetaBackgroundSample& reference, int& referenceX) {
		for(int x = cropLeft; x < cropLeft + count; x++) {
			size_t offset = (size_t)y * frame.Width + x;
			const auto& pixel = frame.Pixels[offset];
			auto consider = [&](const MetaBackgroundSample& sample) { if(!reference.Valid && sample.Valid && sample.Layer == layer.Layer) { reference = sample; referenceX = x; } };
			consider(pixel.BackgroundSample);
			for(const auto& sample : pixel.GbaBackgroundSamples) { consider(sample); }
			if(frame.SnesBackgrounds.size() == frame.Pixels.size()) { for(const auto& bg : frame.SnesBackgrounds[offset].Layers) { consider(bg.Sample); } }
			if(frame.BackgroundUnderlays.size() == frame.Pixels.size()) { consider(frame.BackgroundUnderlays[offset].Sample); consider(frame.BackgroundUnderlays[offset].ForegroundSample); }
			if(reference.Valid) { return true; }
		}
		return false;
	}
public:
	static const MetaBackgroundReplacement* SelectPosition(const MetaSpriteFrame* frame, const std::vector<MetaBackgroundReplacement>* definitions) {
		if(!frame || !definitions) { return nullptr; }
		for(const auto& layer : *definitions) {
			if(!layer.UseWidescreenPosition || !layer.ExtendIntoMargins || !layer.IsValid() || !MetaMemoryCondition::Matches(layer.MemoryConditions, frame->MemorySnapshot)) { continue; }
			int x, y;
			if(layer.UseCpuCamera && layer.FollowScroll && (!layer.CameraX.Decode(frame->MemorySnapshot, x) || !layer.CameraY.Decode(frame->MemorySnapshot, y))) { continue; }
			return &layer;
		}
		return nullptr;
	}
	void Reset() { _positionValid = false; }
	uint32_t ResolveLeft(uint32_t extra, uint32_t position, const MetaSpriteFrame* frame,
		const std::vector<MetaBackgroundReplacement>* definitions, const DecorativeWidescreenOptions& options,
		uint32_t coverageMin = 0, uint32_t coverageMax = UINT32_MAX)
	{
		uint32_t home = options.HomeLeft < 0 ? extra / 2 : std::min(extra, (uint32_t)options.HomeLeft);
		uint32_t target = position == 1 ? 0 : position == 2 ? extra : home;
		bool validNeutral = false;
		if(position == 3 && frame && definitions && (options.UseRamBoundaryFlags || options.CameraMin < options.CameraMax)) {
			for(const auto& layer : *definitions) {
				if(layer.Id != (int)options.CameraBackgroundId || !layer.ExtendIntoMargins
					|| !layer.IsValid() || !MetaMemoryCondition::Matches(layer.MemoryConditions, frame->MemorySnapshot)) { continue; }
				int cameraX, cameraY;
				bool cameraValid = layer.CameraX.Decode(frame->MemorySnapshot, cameraX) && layer.CameraY.Decode(frame->MemorySnapshot, cameraY);
				if(options.UseRamBoundaryFlags) {
					if(layer.UseCpuCamera && layer.FollowScroll && !cameraValid) { break; }
					auto available = [&](const auto& conditions) { for(const auto& condition : conditions) { if(!frame->MemorySnapshot.count(condition.Address)) { return false; } } return true; };
					if(!available(options.LeftFlags) || !available(options.RightFlags)) { break; }
					bool leftFlag = !options.LeftFlags.empty() && MetaMemoryCondition::Matches(options.LeftFlags, frame->MemorySnapshot);
					bool rightFlag = !options.RightFlags.empty() && MetaMemoryCondition::Matches(options.RightFlags, frame->MemorySnapshot);
					if(leftFlag != rightFlag) { target = leftFlag ? 0 : extra; }
					validNeutral = !leftFlag && !rightFlag;
				} else if(layer.UseCpuCamera && layer.FollowScroll && cameraValid) {
					target = cameraX <= options.CameraMin ? 0 : cameraX >= options.CameraMax ? extra : home;
					validNeutral = target == home;
				}
				break;
			}
		}
		uint32_t number = frame ? frame->FrameNumber : 0;
		bool reset = !_positionValid || _lastExtra != extra || _lastPosition != position || _lastPositionKey != options.PositionKey || number < _lastFrame || number - _lastFrame > 120;
		if(reset) { _left = home; _heldTarget = home; _releaseFrames = 0; }
		uint32_t frames = reset ? 1 : number > _lastFrame ? number - _lastFrame : 0;
		if(position == 3 && target == home && validNeutral && options.ReleaseDelay && _heldTarget != home) {
			_releaseFrames = (uint32_t)std::min<uint64_t>(600, (uint64_t)_releaseFrames + frames);
			if(_releaseFrames < std::min(600u, options.ReleaseDelay)) { target = _heldTarget; }
			else { _heldTarget = home; }
		} else { _heldTarget = target; _releaseFrames = 0; }
		double speed = std::isfinite(options.SlideSpeed) ? std::max(0.0, options.SlideSpeed) : 0;
		if(options.RamSpeed.Enabled) {
			// Missing speed freezes motion, never invokes the fixed-speed zero shortcut.
			speed = 0; if(frame) { options.RamSpeed.Decode(frame->MemorySnapshot, speed, true); }
			speed *= options.SpeedScale;
		}
		if(position != 3 || (!options.RamSpeed.Enabled && !speed)) { _left = target; }
		else if(reset || number > _lastFrame) {
			double step = std::min((double)extra, std::abs(speed) * frames);
			double low = std::min(home, target), high = std::max(home, target);
			bool directional = options.RamSpeed.Enabled && options.RamSpeed.Format % 2 == 0;
			if(directional && target != home && _left >= low && _left <= high) {
				// A signed speed is a velocity at an edge: turning around must
				// reverse the slide even while the camera/flag remains at that edge.
				_left = std::clamp(_left + (speed < 0 ? -step : step), low, high);
			} else {
				// Unsigned sources have no direction. Neutral/invalid boundaries
				// return toward home, including when crossing between edge ranges.
				double destination = directional && target != home && (_left < low || _left > high) ? home : target;
				_left = destination < _left ? _left - std::min(step, _left - destination) : _left + std::min(step, destination - _left);
			}
		}
		// PNG coverage takes precedence over speed/delay, including before an
		// edge trigger. Clamp the stored position too, preventing integration drift.
		_left = std::clamp(_left, (double)std::min(extra, coverageMin), (double)std::min(extra, coverageMax));
		_lastFrame = number; _lastExtra = extra; _lastPosition = position; _positionValid = true;
		_lastPositionKey = options.PositionKey;
		return (uint32_t)std::round(_left);
	}
	static uint32_t CanvasWidth(uint32_t width, double nativeAspect, double targetAspect)
	{
		if(!width || !std::isfinite(nativeAspect) || nativeAspect <= 0 || !std::isfinite(targetAspect) || targetAspect < 1 || targetAspect > 5) { return width; }
		return (uint32_t)std::min(32768.0, std::max((double)width, std::ceil(width * targetAspect / nativeAspect)));
	}
	uint32_t* Apply(uint32_t* original, uint32_t width, uint32_t height, uint32_t canvasWidth,
		uint32_t brightness, uint32_t position, const MetaSpriteFrame* frame,
		const std::vector<MetaBackgroundReplacement>* definitions, int cropLeft, int cropTop, int scale,
		const DecorativeWidescreenOptions& options = {})
	{
		if(!original || !width || !height || canvasWidth <= width || canvasWidth > 32768 || (uint64_t)canvasWidth * height > 67108864) { Reset(); return original; }
		uint32_t extra = canvasWidth - width, coverageMin = 0, coverageMax = extra;
		bool compatible = frame && definitions && scale >= 1 && width % scale == 0 && height % scale == 0
			&& frame->Pixels.size() == (uint64_t)frame->Width * frame->Height && cropLeft >= 0 && cropTop >= 0
			&& cropLeft + width / scale <= frame->Width && cropTop + height / scale <= frame->Height;
		if(position == 3 && compatible) {
			for(const auto& layer : *definitions) {
				if(layer.Id != (int)options.CameraBackgroundId || layer.WrapX || !layer.ExtendIntoMargins || !layer.IsValid()
					|| !layer.UseCpuCamera || !layer.FollowScroll || !MetaMemoryCondition::Matches(layer.MemoryConditions, frame->MemorySnapshot)) { continue; }
				int cameraX, cameraY;
				if(!layer.CameraX.Decode(frame->MemorySnapshot, cameraX) || !layer.CameraY.Decode(frame->MemorySnapshot, cameraY)) { break; }
				int64_t product = (int64_t)cameraX * layer.ScrollSpeedX;
				int64_t scroll = product / 1000 - (product % 1000 < 0 ? 1 : 0);
				int64_t low = 0, high = extra;
				for(int y = cropTop; y < cropTop + (int)(height / scale); y++) {
					MetaBackgroundSample reference; int referenceX = 0;
					if(!FindReference(*frame, layer, y, cropLeft, width / scale, reference, referenceX)) { continue; }
					int pixelScale = std::max(1, (int)reference.PixelScaleX);
					int64_t begin = ((int64_t)layer.X - scroll) * pixelScale;
					int64_t end = begin + (int64_t)(layer.Width / layer.Scale) * pixelScale;
					low = std::max(low, (int64_t)canvasWidth - (end - cropLeft) * scale);
					high = std::min(high, ((int64_t)cropLeft - begin) * scale);
				}
				// Too-small/misplaced artwork cannot cover the full output. Do not
				// stretch, wrap, or shift unrelated layers to fabricate coverage.
				if(low <= high && low <= extra && high >= 0) { coverageMin = (uint32_t)low; coverageMax = (uint32_t)high; }
				break;
			}
		}
		uint32_t left = ResolveLeft(extra, position, frame, definitions, options, coverageMin, coverageMax);
		_output.assign((size_t)canvasWidth * height, 0xFF000000);
		for(uint32_t y = 0; y < height; y++) { std::copy_n(original + (size_t)y * width, width, _output.data() + (size_t)y * canvasWidth + left); }
		if(!frame || !definitions || scale < 1 || width % scale || height % scale
			|| frame->Pixels.size() != (uint64_t)frame->Width * frame->Height
			|| cropLeft < 0 || cropTop < 0 || cropLeft + width / scale > frame->Width || cropTop + height / scale > frame->Height) { return _output.data(); }
		MetaBackgroundView layers;
		for(const auto& layer : *definitions) {
			if(layer.ExtendIntoMargins && layer.IsValid() && MetaMemoryCondition::Matches(layer.MemoryConditions, frame->MemorySnapshot)) { layers.Layers.push_back(&layer); }
		}
		layers.PrepareRasterReferences(*frame);
		brightness = std::min(100u, brightness);
		// Resolve a native reference once per row/layer, then extrapolate only its
		// PNG coordinates. CPU camera sampling ignores raster wrap as usual.
		for(uint32_t y = 0; y < height; y++) {
			int py = y / scale + cropTop;
			for(const auto& layer : layers) {
				MetaBackgroundSample reference; int referenceX = 0;
				if(!FindReference(*frame, layer, py, cropLeft, width / scale, reference, referenceX)) { continue; }
				for(uint32_t x = 0; x < canvasWidth; x++) {
					if(x >= left && x < left + width) { continue; }
					int relative = (int)x - (int)left;
					int px = (int)std::floor((double)relative / scale) + cropLeft;
					int sx = relative % scale; if(sx < 0) { sx += scale; }
					MetaBackgroundSample source = reference;
					int texel = px - referenceX + reference.PixelOffsetX;
					int delta = (int)std::floor((double)texel / std::max(1, (int)reference.PixelScaleX));
					source.X += delta;
					source.PixelOffsetX = (uint8_t)(texel - delta * std::max(1, (int)reference.PixelScaleX));
					if(source.ScrollWrapX > 0) { source.X = (int)MetaBackgroundReplacement::WrapCoordinate(source.X, source.ScrollWrapX); }
					uint32_t color;
					if(!layers.Sample(layer, source, px, py, sx, y % scale, scale, color, true) || !(color >> 24)) { continue; }
					uint32_t& dest = _output[(size_t)y * canvasWidth + x];
					uint32_t alpha = color >> 24, result = 0xFF000000;
					for(int shift : {0, 8, 16}) {
						uint32_t value = (((color >> shift) & 255) * alpha + ((dest >> shift) & 255) * (255 - alpha) + 127) / 255;
						result |= value << shift;
					}
					dest = result;
				}
			}
			for(uint32_t x = 0; x < canvasWidth; x++) {
				if(x >= left && x < left + width) { continue; }
				uint32_t& color = _output[(size_t)y * canvasWidth + x];
				uint32_t dim = 0xFF000000;
				uint32_t distance = x < left ? left - 1 - x : x - (left + width);
				uint32_t fade = options.FadeWidth ? std::min(distance, options.FadeWidth) : 1;
				uint32_t denominator = options.FadeWidth ? options.FadeWidth : 1;
				uint64_t level = options.FadeWidth ? 100ULL * denominator - (100ULL - brightness) * fade : brightness;
				for(int shift : {0, 8, 16}) {
					uint64_t value = ((color >> shift) & 255) * level;
					dim |= (uint32_t)((value + denominator * 50ULL) / (denominator * 100ULL)) << shift;
				}
				color = dim;
			}
		}
		return _output.data();
	}
};

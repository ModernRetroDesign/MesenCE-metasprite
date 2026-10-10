#pragma once
#include "MetaSpriteFrame.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <functional>

struct MetaSpriteGraphic
{
	int X = 0, Y = 0, Width = 0, Height = 0;
	int HardwareSprite = -1;
	std::vector<uint32_t> Pixels;
};

struct MetaSpriteReplacement
{
	int Id = 0;
	int OriginX = 0, OriginY = 0;
	int ArtworkOriginX = 0, ArtworkOriginY = 0;
	int ArtworkScale = 1, ArtworkWidth = 0, ArtworkHeight = 0;
	std::vector<MetaSpriteGraphic> Members;
	std::vector<uint32_t> Artwork;
};

// Shared recognition/composition accepts captured native state. It never consults live VRAM.
class MetaSpriteCompositor
{
	std::mutex _lock;
	std::shared_ptr<const std::vector<MetaSpriteReplacement>> _groups;
	std::vector<uint32_t> _output;
	uint32_t _outputScale = 1;

	static uint32_t Rgb555(uint16_t value)
	{
		auto channel = [](int v) { return (v << 3) | (v >> 2); };
		return 0xFF000000 | (channel(value & 31) << 16) | (channel((value >> 5) & 31) << 8) | channel((value >> 10) & 31);
	}

public:
	static uint32_t ApplySnesColorEffect(uint32_t main, uint32_t secondary, const MetaSpritePixel& pixel)
	{
		if(pixel.SnesClip) { main = 0xFF000000; }
		uint32_t other = pixel.SnesUseSubscreen ? secondary : Rgb555(pixel.SnesFixedColor);
		uint32_t result = 0xFF000000;
		for(int shift : {0, 8, 16}) {
			int channel = (main >> shift) & 255;
			if(pixel.SnesMathEnabled) {
				int sub = (other >> shift) & 255;
				channel = pixel.SnesSubtract ? std::max(0, channel - sub) : channel + sub;
				if(pixel.SnesHalf) { channel >>= 1; }
				channel = std::min(255, channel);
			}
			channel = channel * std::min<int>(15, pixel.Brightness) / 15;
			result |= (uint32_t)channel << shift;
		}
		return result;
	}
	static uint32_t CompositeAlpha(uint32_t foreground, uint32_t background, uint32_t alpha)
	{
		uint32_t result = 0xFF000000;
		for(int shift : {0, 8, 16}) {
			result |= ((((foreground >> shift) & 255) * alpha + ((background >> shift) & 255) * (255 - alpha) + 127) / 255) << shift;
		}
		return result;
	}
	// Apply captured GBA effects to an opaque artwork sample. PNG alpha is composed
	// separately; hardware semi-transparency takes precedence over window effects.
	static uint32_t ApplyGbaColorEffect(uint32_t main, uint32_t secondary, const MetaSpritePixel& pixel)
	{
		bool blend = pixel.SecondaryBlendTarget && (pixel.SemiTransparent || (pixel.EffectsEnabled && pixel.Effect == 1));
		if(!blend && (!pixel.EffectsEnabled || pixel.Effect < 2 || pixel.Effect > 3)) { return main; }
		uint32_t mainCoefficient = std::min<uint32_t>(16, pixel.MainCoefficient);
		uint32_t secondaryCoefficient = std::min<uint32_t>(16, pixel.SecondaryCoefficient);
		if(!blend) {
			secondaryCoefficient = std::min<uint32_t>(16, pixel.Brightness);
			mainCoefficient = 16 - secondaryCoefficient;
			secondary = pixel.Effect == 2 ? 0xFFFFFFFF : 0xFF000000;
		}
		uint32_t result = main & 0xFF000000;
		for(int shift : {0, 8, 16}) {
			uint32_t value = (((main >> shift) & 255) * mainCoefficient + ((secondary >> shift) & 255) * secondaryCoefficient) / 16;
			result |= std::min<uint32_t>(255, value) << shift;
		}
		return result;
	}

	static std::vector<MetaSpriteGraphic> BuildDecodedCandidates(const MetaSpriteFrame& frame)
	{
		std::vector<MetaSpriteGraphic> result;
		std::vector<std::vector<bool>> observed;
		std::vector<bool> invalid;
		for(const auto& row : frame.DecodedRows) {
			if(row.Sprite < 0 || row.Width < 1 || row.Width > 128 || row.Height < 1 || row.Height > 128
				|| row.Row < 0 || row.Row >= row.Height || row.OffsetX < 0 || row.OffsetX >= row.Width
				|| row.Pixels.empty() || row.Pixels.size() > (size_t)(row.Width - row.OffsetX)) { continue; }
			auto found = std::find_if(result.begin(), result.end(), [&](const auto& item) { return item.HardwareSprite == row.Sprite; });
			if(found == result.end()) {
				result.push_back({row.X, row.Y, row.Width, row.Height, row.Sprite, std::vector<uint32_t>(row.Width * row.Height)});
				observed.push_back(std::vector<bool>(row.Width * row.Height)); invalid.push_back(false);
				found = result.end() - 1;
			}
			size_t index = found - result.begin();
			if(found->X != row.X || found->Y != row.Y || found->Width != row.Width || found->Height != row.Height) { invalid[index] = true; continue; }
			size_t offset = row.Row * row.Width + row.OffsetX;
			for(size_t x = 0; x < row.Pixels.size(); x++) {
				if(observed[index][offset + x] && found->Pixels[offset + x] != row.Pixels[x]) { invalid[index] = true; }
				found->Pixels[offset + x] = row.Pixels[x];
				observed[index][offset + x] = true;
			}
		}
		std::vector<MetaSpriteGraphic> complete;
		for(size_t i = 0; i < result.size(); i++) {
			if(!invalid[i] && std::all_of(observed[i].begin(), observed[i].end(), [](bool value) { return value; })) { complete.push_back(std::move(result[i])); }
		}
		return complete;
	}

	uint32_t GetOutputScale() const { return _outputScale; }
	static std::vector<MetaSpriteGraphic> BuildNesCandidates(const MetaSpriteFrame& frame, const std::function<uint32_t(uint16_t)>& nativeColor)
	{
		if(!nativeColor) { return {}; }
		MetaSpriteFrame decoded(0, 0);
		decoded.DecodedRows.reserve(frame.NesRows.size());
		for(const auto& row : frame.NesRows) {
			int y = (int)row.Scanline - row.Y;
			if(row.Sprite >= 64 || (row.Height != 8 && row.Height != 16) || y < 0 || y >= row.Height) { continue; }
			MetaSpriteDecodedRow graphic {row.Sprite, row.X, row.Y, y, 8, row.Height, {}};
			for(int x = 0; x < 8; x++) {
				int color = ((row.LowByte >> (7 - x)) & 1) | (((row.HighByte >> (7 - x)) & 1) << 1);
				graphic.Pixels.push_back(color ? nativeColor(row.Palette[color]) : 0);
			}
			decoded.DecodedRows.push_back(std::move(graphic));
		}
		return BuildDecodedCandidates(decoded);
	}
	void SetGroups(std::vector<MetaSpriteReplacement> groups)
	{
		auto snapshot = std::make_shared<const std::vector<MetaSpriteReplacement>>(std::move(groups));
		std::lock_guard<std::mutex> lock(_lock);
		_groups = std::move(snapshot);
	}

	static std::vector<MetaSpriteGraphic> BuildGameboyCandidates(const MetaSpriteFrame& frame)
	{
		std::vector<MetaSpriteGraphic> candidates(40);
		uint32_t observedRows[40] = {};
		bool invalid[40] = {};
		for(const auto& row : frame.Rows) {
			if(row.Sprite >= 40 || (row.Height != 8 && row.Height != 16)) { continue; }
			int y = (int)row.Scanline - row.Y;
			if(y < 0 || y >= row.Height) { continue; }
			auto& sprite = candidates[row.Sprite];
			if(sprite.Pixels.empty()) {
				sprite.X = row.X; sprite.Y = row.Y; sprite.Width = 8; sprite.Height = row.Height;
				sprite.HardwareSprite = row.Sprite;
				sprite.Pixels.resize(8 * row.Height);
			} else if(sprite.X != row.X || sprite.Y != row.Y || sprite.Height != row.Height) {
				invalid[row.Sprite] = true;
			}
			observedRows[row.Sprite] |= 1u << y;
			for(int x = 0; x < 8; x++) {
				int shift = (row.Attributes & 0x20) ? x : 7 - x;
				int color = ((row.LowByte >> shift) & 1) | (((row.HighByte >> shift) & 1) << 1);
				sprite.Pixels[y * 8 + x] = color ? Rgb555(row.Palette[color]) : 0;
			}
		}
		candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](const MetaSpriteGraphic& s) {
			return s.HardwareSprite < 0 || invalid[s.HardwareSprite] || observedRows[s.HardwareSprite] != ((1u << s.Height) - 1);
		}), candidates.end());
		return candidates;
	}

	// Enabled artwork determines the HD output scale, preserving individual artwork pixels.
	// A pixel is replaced only when a matched member won hardware composition at that position.
	uint32_t* Apply(uint32_t* original, uint32_t width, uint32_t height, const MetaSpriteFrame& frame,
		int cropLeft = 0, int cropTop = 0, std::function<uint32_t(uint16_t)> backgroundColor = {})
	{
		_outputScale = 1;
		std::shared_ptr<const std::vector<MetaSpriteReplacement>> groups;
		{ std::lock_guard<std::mutex> lock(_lock); groups = _groups; }
		if(!groups || groups->empty() || !original || cropLeft < 0 || cropTop < 0 || width + cropLeft > frame.Width || height + cropTop > frame.Height) { return original; }
		auto candidates = !frame.NesRows.empty() ? BuildNesCandidates(frame, backgroundColor)
			: frame.DecodedRows.empty() ? BuildGameboyCandidates(frame) : BuildDecodedCandidates(frame);
		for(const auto& group : *groups) { _outputScale = std::max(_outputScale, (uint32_t)std::clamp(group.ArtworkScale, 1, 10)); }
		uint32_t outputWidth = width * _outputScale;
		_output.resize((size_t)outputWidth * height * _outputScale);
		std::vector<uint32_t> effectMain, effectSecondary;
		std::vector<uint8_t> effectChanged;
		bool stagedEffects = frame.GbaColorEffects || frame.SnesColorEffects;
		if(stagedEffects) {
			effectMain.resize(_output.size()); effectSecondary.resize(_output.size()); effectChanged.resize(_output.size());
		}
		for(uint32_t y = 0; y < height * _outputScale; y++) {
			for(uint32_t x = 0; x < outputWidth; x++) {
				size_t offset = (size_t)y * outputWidth + x;
				_output[offset] = original[(y / _outputScale) * width + x / _outputScale];
				if(stagedEffects) {
					const auto& pixel = frame.Pixels[(y / _outputScale + cropTop) * frame.Width + x / _outputScale + cropLeft];
					effectMain[offset] = Rgb555(pixel.SpriteColor); effectSecondary[offset] = Rgb555(pixel.SecondaryColor);
				}
			}
		}
		std::vector<bool> consumed(candidates.size());
		for(const auto& group : *groups) {
			if(group.Members.empty() || group.ArtworkScale < 1 || group.ArtworkWidth < 1 || group.ArtworkHeight < 1
				|| group.Artwork.size() != (size_t)group.ArtworkWidth * group.ArtworkHeight) { continue; }
			const auto& anchor = group.Members[0];
			for(size_t i = 0; i < candidates.size(); i++) {
				if(consumed[i] || candidates[i].Width != anchor.Width || candidates[i].Height != anchor.Height || candidates[i].Pixels != anchor.Pixels) { continue; }
				int left = candidates[i].X - anchor.X, top = candidates[i].Y - anchor.Y;
				std::vector<size_t> matched { i };
				for(size_t m = 1; m < group.Members.size(); m++) {
					const auto& member = group.Members[m];
					for(size_t j = 0; j < candidates.size(); j++) {
						const auto& candidate = candidates[j];
						if(!consumed[j] && std::find(matched.begin(), matched.end(), j) == matched.end() && candidate.X == left + member.X && candidate.Y == top + member.Y
							&& candidate.Width == member.Width && candidate.Height == member.Height && candidate.Pixels == member.Pixels) { matched.push_back(j); break; }
					}
					if(matched.size() != m + 1) { break; }
				}
				if(matched.size() != group.Members.size()) { continue; }
				for(auto index : matched) { consumed[index] = true; }
				int artworkLeft = left + group.OriginX - group.ArtworkOriginX;
				int artworkTop = top + group.OriginY - group.ArtworkOriginY;
				for(uint32_t y = 0; y < height; y++) {
					for(uint32_t x = 0; x < width; x++) {
						const auto& pixel = frame.Pixels[(y + cropTop) * frame.Width + x + cropLeft];
						bool memberVisible = std::any_of(matched.begin(), matched.end(), [&](size_t index) { return candidates[index].HardwareSprite == pixel.VisibleSprite; });
						bool memberSecondary = stagedEffects && std::any_of(matched.begin(), matched.end(), [&](size_t index) { return candidates[index].HardwareSprite == pixel.SecondarySprite; });
						if(!memberVisible && !memberSecondary) { continue; }
						uint32_t background = backgroundColor ? backgroundColor(pixel.BackgroundColor) : Rgb555(pixel.BackgroundColor);
						for(uint32_t sy = 0; sy < _outputScale; sy++) {
							for(uint32_t sx = 0; sx < _outputScale; sx++) {
								uint32_t color = 0;
								int ax = ((int)x + cropLeft - artworkLeft) * group.ArtworkScale + (int)(sx * group.ArtworkScale / _outputScale);
								int ay = ((int)y + cropTop - artworkTop) * group.ArtworkScale + (int)(sy * group.ArtworkScale / _outputScale);
								if(ax >= 0 && ay >= 0 && ax < group.ArtworkWidth && ay < group.ArtworkHeight) { color = group.Artwork[ay * group.ArtworkWidth + ax]; }
								size_t outputOffset = (size_t)(y * _outputScale + sy) * outputWidth + x * _outputScale + sx;
								if(stagedEffects && ((color >> 24) == 255 || (frame.GbaColorEffects && pixel.GbaFallbackCaptured) || (frame.SnesColorEffects && pixel.SnesFallbackCaptured))) {
									if(memberVisible) { effectMain[outputOffset] = color; }
									if(memberSecondary) { effectSecondary[outputOffset] = color; }
									effectChanged[outputOffset] |= (memberVisible ? 1 : 0) | (memberSecondary ? 2 : 0);
									continue;
								}
								// Older/synthetic captures without fallback state cannot remove a layer.
								if(stagedEffects) { continue; }
								uint32_t alpha = color >> 24, result = 0xFF000000;
								for(int shift : { 0, 8, 16 }) {
									result |= ((((color >> shift) & 255) * alpha + ((background >> shift) & 255) * (255 - alpha) + 127) / 255) << shift;
								}
								_output[(size_t)(y * _outputScale + sy) * outputWidth + x * _outputScale + sx] = result;
							}
						}
					}
				}
			}
		}
		if(stagedEffects) {
			for(uint32_t y = 0; y < height * _outputScale; y++) {
				for(uint32_t x = 0; x < outputWidth; x++) {
					size_t offset = (size_t)y * outputWidth + x;
					if(!effectChanged[offset]) { continue; }
					const auto& pixel = frame.Pixels[(y / _outputScale + cropTop) * frame.Width + x / _outputScale + cropLeft];
					if(frame.SnesColorEffects) {
						auto compose = [&](bool removeMain, bool removeSub) {
							auto state = pixel;
							if(removeMain) { state.SnesMathEnabled = (pixel.SnesFallbackMainFlags & 0x80) && pixel.SnesWindowMathAllowed; }
							if(removeSub || removeMain) {
								state.SnesUseSubscreen = pixel.SnesAddSubscreen && (removeSub ? pixel.SnesFallbackSubPriority > 0 : pixel.SnesUseSubscreen);
								state.SnesHalf = pixel.SnesHalfRequested && (!pixel.SnesAddSubscreen || state.SnesUseSubscreen);
							}
							return ApplySnesColorEffect(removeMain ? Rgb555(pixel.SnesFallbackMainColor) : effectMain[offset],
								removeSub ? Rgb555(pixel.SnesFallbackSubColor) : effectSecondary[offset], state);
						};
						uint32_t mainAlpha = (effectChanged[offset] & 1) ? effectMain[offset] >> 24 : 255;
						uint32_t subAlpha = (effectChanged[offset] & 2) ? effectSecondary[offset] >> 24 : 255;
						uint32_t result = compose(false, false);
						if(subAlpha < 255) { result = CompositeAlpha(result, compose(false, true), subAlpha); }
						if(mainAlpha < 255) {
							uint32_t fallback = compose(true, false);
							if(subAlpha < 255) { fallback = CompositeAlpha(fallback, compose(true, true), subAlpha); }
							result = CompositeAlpha(result, fallback, mainAlpha);
						}
						_output[offset] = result;
						continue;
					}
					uint32_t result = ApplyGbaColorEffect(effectMain[offset] | 0xFF000000, effectSecondary[offset], pixel);
					if((effectChanged[offset] & 2) && (effectSecondary[offset] >> 24) < 255) {
						auto removedSecondary = pixel;
						removedSecondary.SecondaryBlendTarget = pixel.GbaFallbackBlendTarget;
						uint32_t fallback = ApplyGbaColorEffect(effectMain[offset] | 0xFF000000, Rgb555(pixel.GbaFallbackMainColor), removedSecondary);
						result = CompositeAlpha(result, fallback, effectSecondary[offset] >> 24);
					}
					if((effectChanged[offset] & 1) && (effectMain[offset] >> 24) < 255) {
						result = CompositeAlpha(result, Rgb555(pixel.GbaFallbackColor), effectMain[offset] >> 24);
					}
					_output[offset] = result;
				}
			}
		}
		return _output.data();
	}
};

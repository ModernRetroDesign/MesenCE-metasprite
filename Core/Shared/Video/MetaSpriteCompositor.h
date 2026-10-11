#pragma once
#include "MetaSpriteFrame.h"
#include "MetaBackgroundReplacement.h"
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
	std::vector<MetaMemoryCondition> MemoryConditions;
};

// Shared recognition/composition accepts captured native state. It never consults live VRAM.
class MetaSpriteCompositor
{
	std::mutex _lock;
	std::shared_ptr<const std::vector<MetaSpriteReplacement>> _groups;
	std::shared_ptr<const std::vector<MetaBackgroundReplacement>> _backgroundLayers;
	std::shared_ptr<const std::vector<uint32_t>> _memoryAddresses;
	static std::shared_ptr<const std::vector<uint32_t>> CompileMemoryAddresses(const std::vector<MetaSpriteReplacement>& groups)
	{
		std::vector<uint32_t> addresses;
		for(const auto& group : groups) { for(const auto& condition : group.MemoryConditions) { addresses.push_back(condition.Address); } }
		std::sort(addresses.begin(), addresses.end());
		addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
		return std::make_shared<const std::vector<uint32_t>>(std::move(addresses));
	}
	std::vector<uint32_t> _output;
	// The decoder owns one compositor; retain staging capacity between frames.
	std::vector<uint32_t> _effectMain, _effectSecondary, _gbaObjectArtwork;
	std::vector<uint8_t> _effectChanged;
	std::vector<uint16_t> _gbaObjectState;
	std::vector<const MetaGbaSpriteCoverage*> _gbaCoverage;
	std::vector<const MetaSpritePixel::SnesCoverageSample*> _snesMainCoverage, _snesSubCoverage;
	uint32_t _outputScale = 1;

	static uint32_t Rgb555(uint16_t value)
	{
		auto channel = [](int v) { return (v << 3) | (v >> 2); };
		return 0xFF000000 | (channel(value & 31) << 16) | (channel((value >> 5) & 31) << 8) | channel((value >> 10) & 31);
	}

public:
	struct SnesColorState
	{
		bool SnesClip, SnesUseSubscreen, SnesMathEnabled, SnesSubtract, SnesHalf;
		uint16_t SnesFixedColor;
		uint8_t Brightness;
		explicit SnesColorState(const MetaSpritePixel& pixel)
			: SnesClip(pixel.SnesClip), SnesUseSubscreen(pixel.SnesUseSubscreen),
			SnesMathEnabled(pixel.SnesMathEnabled), SnesSubtract(pixel.SnesSubtract),
			SnesHalf(pixel.SnesHalf), SnesFixedColor(pixel.SnesFixedColor), Brightness(pixel.Brightness) {}
	};
	static uint32_t ApplySnesColorEffect(uint32_t main, uint32_t secondary, const MetaSpritePixel& pixel)
	{
		return ApplySnesColorEffect(main, secondary, SnesColorState(pixel));
	}
	static uint32_t ApplySnesColorEffect(uint32_t main, uint32_t secondary, const SnesColorState& pixel)
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
	struct GbaColorState
	{
		bool SecondaryBlendTarget, SemiTransparent, EffectsEnabled;
		uint8_t Effect, MainCoefficient, SecondaryCoefficient, Brightness;
		explicit GbaColorState(const MetaSpritePixel& pixel)
			: SecondaryBlendTarget(pixel.SecondaryBlendTarget), SemiTransparent(pixel.SemiTransparent),
			EffectsEnabled(pixel.EffectsEnabled), Effect(pixel.Effect), MainCoefficient(pixel.MainCoefficient),
			SecondaryCoefficient(pixel.SecondaryCoefficient), Brightness(pixel.Brightness) {}
	};
	static uint32_t ApplyGbaColorEffect(uint32_t main, uint32_t secondary, const MetaSpritePixel& pixel)
	{
		return ApplyGbaColorEffect(main, secondary, GbaColorState(pixel));
	}
	static uint32_t ApplyGbaColorEffect(uint32_t main, uint32_t secondary, const GbaColorState& pixel)
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

	// Reselect the native GBA layer pair for each PNG-alpha outcome before effects.
	// Object overrides replace source coverage; background PNGs overlay their
	// native layer, matching the background editor's existing transparency behavior.
	static uint32_t ComposeGbaLayers(const MetaSpritePixel& pixel, const std::vector<MetaBackgroundReplacement>& backgrounds,
		int x, int y, int sx, int sy, int scale, bool overrideObject = false, uint32_t objectArtwork = 0,
		uint8_t objectPriority = 0, bool objectSemiTransparent = false, bool preserveNativeObject = false)
	{
		if(overrideObject && preserveNativeObject && (objectArtwork >> 24) < 255) {
			uint32_t foreground = ComposeGbaLayers(pixel, backgrounds, x, y, sx, sy, scale, true, objectArtwork | 0xFF000000, objectPriority, objectSemiTransparent);
			uint32_t fallback = ComposeGbaLayers(pixel, backgrounds, x, y, sx, sy, scale);
			return CompositeAlpha(foreground, fallback, objectArtwork >> 24);
		}
		struct Layer { int Index, Priority; uint32_t Color; bool SemiTransparent; };
		Layer layers[6]; int count = 0;
		if(overrideObject || pixel.GbaObjectSprite >= 0) {
			layers[count++] = {4, overrideObject ? objectPriority : pixel.GbaObjectPriority,
				overrideObject ? objectArtwork : Rgb555(pixel.GbaObjectColor), overrideObject ? objectSemiTransparent : pixel.GbaObjectSemiTransparent};
		}
		for(int i = 0; i < 4; i++) {
			if(!pixel.GbaBackgroundSamples[i].Valid) { continue; }
			uint32_t color = pixel.GbaBackgroundLayers[i].Opaque ? Rgb555(pixel.GbaBackgroundLayers[i].Color) : 0;
			for(const auto& background : backgrounds) {
				uint32_t artwork;
				if(!background.Sample(pixel.GbaBackgroundSamples[i], x, y, sx, sy, scale, artwork)) { continue; }
				uint32_t a = artwork >> 24, b = color >> 24;
				uint32_t alphaNumerator = a * 255 + b * (255 - a);
				if(alphaNumerator == 0) { color = 0; continue; }
				uint32_t merged = ((alphaNumerator + 127) / 255) << 24;
				for(int shift : {0, 8, 16}) {
					uint32_t channel = (((artwork >> shift) & 255) * a * 255 + ((color >> shift) & 255) * b * (255 - a) + alphaNumerator / 2) / alphaNumerator;
					merged |= channel << shift;
				}
				color = merged;
			}
			layers[count++] = {i, pixel.GbaBackgroundLayers[i].Priority, color, false};
		}
		layers[count++] = {5, 255, Rgb555(pixel.GbaBackdropColor), false};
		// Stable ties retain OBJ ahead of BG0..BG3, then BG order.
		std::stable_sort(layers, layers + count, [](const Layer& a, const Layer& b) { return a.Priority < b.Priority; });
		double channels[3] = {};
		auto compose = [&](const Layer& main, const Layer& secondary, double weight) {
			GbaColorState state(pixel);
			state.EffectsEnabled = pixel.GbaWindowEffectsEnabled && (pixel.GbaBlendMainMask & (1 << main.Index));
			state.SecondaryBlendTarget = pixel.GbaBlendSubMask & (1 << secondary.Index);
			state.SemiTransparent = main.SemiTransparent;
			uint32_t color = ApplyGbaColorEffect(main.Color | 0xFF000000, secondary.Color | 0xFF000000, state);
			for(int c = 0; c < 3; c++) { channels[c] += ((color >> (c * 8)) & 255) * weight; }
		};
		auto visit = [&](auto&& self, int index, const Layer* main, double weight) -> void {
			if(index == count) {
				if(main) { compose(*main, layers[count - 1], weight); }
				return;
			}
			const auto& layer = layers[index];
			uint32_t alpha = layer.Color >> 24;
			if(alpha) {
				double present = weight * alpha / 255.0;
				if(main) { compose(*main, layer, present); }
				else { self(self, index + 1, &layer, present); }
			}
			if(alpha < 255) { self(self, index + 1, main, weight * (255 - alpha) / 255.0); }
		};
		visit(visit, 0, nullptr, 1.0);
		uint32_t result = 0xFF000000;
		for(int c = 0; c < 3; c++) { result |= (uint32_t)std::clamp((int)(channels[c] + 0.5), 0, 255) << (c * 8); }
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
		auto addresses = CompileMemoryAddresses(groups);
		auto snapshot = std::make_shared<const std::vector<MetaSpriteReplacement>>(std::move(groups));
		std::lock_guard<std::mutex> lock(_lock);
		_groups = std::move(snapshot);
		_memoryAddresses = std::move(addresses);
	}
	void SetBackgroundLayers(std::vector<MetaBackgroundReplacement> layers)
	{
		auto snapshot = std::make_shared<const std::vector<MetaBackgroundReplacement>>(std::move(layers));
		std::lock_guard<std::mutex> lock(_lock);
		_backgroundLayers = std::move(snapshot);
	}
	void SetReplacements(std::vector<MetaSpriteReplacement> groups, std::vector<MetaBackgroundReplacement> layers)
	{
		auto addresses = CompileMemoryAddresses(groups);
		auto sprites = std::make_shared<const std::vector<MetaSpriteReplacement>>(std::move(groups));
		auto backgrounds = std::make_shared<const std::vector<MetaBackgroundReplacement>>(std::move(layers));
		std::lock_guard<std::mutex> lock(_lock);
		_groups = std::move(sprites);
		_backgroundLayers = std::move(backgrounds);
		_memoryAddresses = std::move(addresses);
	}
	// Called on the emulation thread before publishing the captured frame.
	// Failed reads stay absent so conditions cannot match stale memory.
	void CaptureMemorySnapshot(const MetaSpriteFrame& frame, const std::function<bool(uint32_t, uint8_t&)>& readByte)
	{
		std::shared_ptr<const std::vector<uint32_t>> addresses;
		{ std::lock_guard<std::mutex> lock(_lock); addresses = _memoryAddresses; }
		frame.MemorySnapshot.clear();
		if(!addresses || !readByte) { return; }
		frame.MemorySnapshot.reserve(addresses->size());
		for(uint32_t address : *addresses) {
			uint8_t value;
			if(readByte(address, value)) { frame.MemorySnapshot.emplace(address, value); }
		}
	}
	bool HasReplacements()
	{
		std::lock_guard<std::mutex> lock(_lock);
		return (_groups && !_groups->empty()) || (_backgroundLayers && !_backgroundLayers->empty());
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
		int cropLeft = 0, int cropTop = 0, std::function<uint32_t(uint16_t)> backgroundColor = {},
		const std::vector<uint32_t>* backgroundFallback = nullptr)
	{
		_outputScale = 1;
		std::shared_ptr<const std::vector<MetaSpriteReplacement>> groups;
		std::shared_ptr<const std::vector<MetaBackgroundReplacement>> backgrounds;
		{ std::lock_guard<std::mutex> lock(_lock); groups = _groups; backgrounds = _backgroundLayers; }
		bool applyBackgrounds = backgrounds && !backgrounds->empty() && backgroundColor && !frame.GbaColorEffects && !frame.SnesColorEffects
			&& frame.Pixels.size() == (size_t)frame.Width * frame.Height
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) { return pixel.BackgroundSample.Valid; });
		bool applyGbaBackgrounds = backgrounds && !backgrounds->empty() && frame.GbaColorEffects
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) {
				return pixel.GbaFallbackCaptured && std::any_of(std::begin(pixel.GbaBackgroundSamples), std::end(pixel.GbaBackgroundSamples), [](const auto& sample) { return sample.Valid; });
			});
		if(!original || cropLeft < 0 || cropTop < 0 || width + cropLeft > frame.Width || height + cropTop > frame.Height
			|| ((!groups || groups->empty()) && !applyBackgrounds && !applyGbaBackgrounds)) { return original; }
		if(!groups) { groups = std::make_shared<const std::vector<MetaSpriteReplacement>>(); }
		auto candidates = !frame.NesRows.empty() ? BuildNesCandidates(frame, backgroundColor)
			: frame.DecodedRows.empty() ? BuildGameboyCandidates(frame) : BuildDecodedCandidates(frame);
		for(const auto& group : *groups) {
			if(MetaMemoryCondition::Matches(group.MemoryConditions, frame.MemorySnapshot)) {
				_outputScale = std::max(_outputScale, (uint32_t)std::clamp(group.ArtworkScale, 1, 10));
			}
		}
		if(applyBackgrounds || applyGbaBackgrounds) { for(const auto& layer : *backgrounds) { if(layer.IsValid()) { _outputScale = std::max(_outputScale, (uint32_t)layer.Scale); } } }
		uint32_t outputWidth = width * _outputScale;
		_output.resize((size_t)outputWidth * height * _outputScale);
		auto& effectMain = _effectMain;
		auto& effectSecondary = _effectSecondary;
		auto& effectChanged = _effectChanged;
		auto& gbaCoverage = _gbaCoverage;
		auto& gbaObjectArtwork = _gbaObjectArtwork;
		auto& gbaObjectState = _gbaObjectState;
		auto& snesMainCoverage = _snesMainCoverage;
		auto& snesSubCoverage = _snesSubCoverage;
		bool stagedEffects = frame.GbaColorEffects || frame.SnesColorEffects;
		if(stagedEffects) {
			effectMain.resize(_output.size()); effectSecondary.resize(_output.size()); effectChanged.assign(_output.size(), 0);
			if(frame.GbaColorEffects) { gbaCoverage.assign(_output.size(), nullptr); }
			if(applyGbaBackgrounds) { gbaObjectArtwork.resize(_output.size()); gbaObjectState.assign(_output.size(), 0); }
			if(frame.SnesColorEffects) { snesMainCoverage.assign(_output.size(), nullptr); snesSubCoverage.assign(_output.size(), nullptr); }
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
		std::vector<uint32_t> generatedFallback;
		if(applyBackgrounds) {
			const MetaSpriteFrame* backgroundFrame = &frame;
			std::unique_ptr<MetaSpriteFrame> croppedBackground;
			if(cropLeft || cropTop || width != frame.Width || height != frame.Height) {
				croppedBackground = std::make_unique<MetaSpriteFrame>(width, height);
				for(uint32_t y = 0; y < height; y++) {
					std::copy_n(frame.Pixels.begin() + (y + cropTop) * frame.Width + cropLeft, width, croppedBackground->Pixels.begin() + y * width);
				}
				backgroundFrame = croppedBackground.get();
			}
			MetaBackgroundCompositor::Apply(*backgroundFrame, *backgrounds, _outputScale, _output, cropLeft, cropTop);
			if(MetaBackgroundCompositor::BuildFallback(*backgroundFrame, *backgrounds, _outputScale, backgroundColor, generatedFallback, cropLeft, cropTop)) { backgroundFallback = &generatedFallback; }
		}
		std::vector<bool> consumed(candidates.size());
		for(const auto& group : *groups) {
			if(!MetaMemoryCondition::Matches(group.MemoryConditions, frame.MemorySnapshot)) { continue; }
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
				uint64_t memberMaskLow = 0, memberMaskHigh = 0;
				for(auto index : matched) {
					int id = candidates[index].HardwareSprite;
					if(id >= 0 && id < 64) { memberMaskLow |= uint64_t(1) << id; }
					else if(id >= 64 && id < 128) { memberMaskHigh |= uint64_t(1) << (id - 64); }
				}
				auto matchesHardware = [&](int id) {
					if(id >= 0 && id < 64) { return (memberMaskLow & (uint64_t(1) << id)) != 0; }
					if(id >= 64 && id < 128) { return (memberMaskHigh & (uint64_t(1) << (id - 64))) != 0; }
					// Preserve synthetic/future adapters with IDs beyond current hardware.
					return id >= 128 && std::any_of(matched.begin(), matched.end(), [&](size_t index) { return candidates[index].HardwareSprite == id; });
				};
				int artworkLeft = left + group.OriginX - group.ArtworkOriginX;
				int artworkTop = top + group.OriginY - group.ArtworkOriginY;
				// Include both artwork and fetched members: a smaller/shifted PNG
				// must still remove original pixels outside its own canvas.
				int64_t boundLeft = artworkLeft, boundTop = artworkTop;
				int64_t boundRight = boundLeft + (group.ArtworkWidth + group.ArtworkScale - 1) / group.ArtworkScale;
				int64_t boundBottom = boundTop + (group.ArtworkHeight + group.ArtworkScale - 1) / group.ArtworkScale;
				for(auto index : matched) {
					const auto& candidate = candidates[index];
					boundLeft = std::min(boundLeft, (int64_t)candidate.X); boundTop = std::min(boundTop, (int64_t)candidate.Y);
					boundRight = std::max(boundRight, (int64_t)candidate.X + candidate.Width);
					boundBottom = std::max(boundBottom, (int64_t)candidate.Y + candidate.Height);
				}
				uint32_t xBegin = (uint32_t)std::clamp<int64_t>(boundLeft - cropLeft, 0, width);
				uint32_t xEnd = (uint32_t)std::clamp<int64_t>(boundRight - cropLeft, 0, width);
				uint32_t yBegin = (uint32_t)std::clamp<int64_t>(boundTop - cropTop, 0, height);
				uint32_t yEnd = (uint32_t)std::clamp<int64_t>(boundBottom - cropTop, 0, height);
				for(uint32_t y = yBegin; y < yEnd; y++) {
					for(uint32_t x = xBegin; x < xEnd; x++) {
						const auto& pixel = frame.Pixels[(y + cropTop) * frame.Width + x + cropLeft];
						bool memberVisible = matchesHardware(pixel.VisibleSprite);
						bool originalMemberVisible = memberVisible;
						const MetaGbaSpriteCoverage* insertedGbaSprite = nullptr;
						if(frame.GbaColorEffects && !memberVisible) {
							for(const auto& coverage : pixel.GbaCoverage) {
								if(matchesHardware(coverage.Sprite)) {
									if(!insertedGbaSprite || coverage.Priority < insertedGbaSprite->Priority || (coverage.Priority == insertedGbaSprite->Priority && coverage.Sprite < insertedGbaSprite->Sprite)) { insertedGbaSprite = &coverage; }
									memberVisible = true;
								}
							}
						}
						if(!stagedEffects) {
							memberVisible |= (pixel.ReplacementEligibleSprites & memberMaskLow) != 0 || (pixel.ReplacementEligibleSpritesHigh & memberMaskHigh) != 0;
						}
						bool memberSecondary = stagedEffects && matchesHardware(pixel.SecondarySprite);
						const MetaSpritePixel::SnesCoverageSample* insertedSnesMain = nullptr;
						const MetaSpritePixel::SnesCoverageSample* insertedSnesSub = nullptr;
						if(frame.SnesColorEffects) {
							for(const auto& coverage : pixel.SnesCoverage) {
								if(!matchesHardware(coverage.Sprite)) { continue; }
								if(!originalMemberVisible && coverage.Main && (!insertedSnesMain || coverage.Order > insertedSnesMain->Order)) { insertedSnesMain = &coverage; }
								if(!memberSecondary && coverage.Sub && (!insertedSnesSub || coverage.Order > insertedSnesSub->Order)) { insertedSnesSub = &coverage; }
							}
							memberVisible |= insertedSnesMain != nullptr;
							memberSecondary |= insertedSnesSub != nullptr;
						}
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
									if(applyGbaBackgrounds && (insertedGbaSprite || matchesHardware(pixel.GbaObjectSprite))) {
										gbaObjectArtwork[outputOffset] = color;
										uint16_t priority = insertedGbaSprite ? insertedGbaSprite->Priority : pixel.GbaObjectPriority;
										bool semi = insertedGbaSprite ? insertedGbaSprite->SemiTransparent : pixel.GbaObjectSemiTransparent;
										gbaObjectState[outputOffset] = 0x100 | priority | (semi ? 0x200 : 0) | (!matchesHardware(pixel.GbaObjectSprite) ? 0x400 : 0);
									}
									if(insertedGbaSprite) {
										gbaCoverage[outputOffset] = insertedGbaSprite;
										effectSecondary[outputOffset] = Rgb555(insertedGbaSprite->SecondaryColor);
									}
									if(memberSecondary) { effectSecondary[outputOffset] = color; }
									if(insertedSnesMain) { snesMainCoverage[outputOffset] = insertedSnesMain; }
									if(insertedSnesSub) { snesSubCoverage[outputOffset] = insertedSnesSub; }
									effectChanged[outputOffset] |= (memberVisible ? 1 : 0) | (memberSecondary ? 2 : 0);
									continue;
								}
								// Older/synthetic captures without fallback state cannot remove a layer.
								if(stagedEffects) { continue; }
								uint32_t alpha = color >> 24, result = 0xFF000000;
								uint32_t fallback = backgroundFallback && backgroundFallback->size() == _output.size() ? (*backgroundFallback)[outputOffset] : background;
								if(!originalMemberVisible) { fallback = _output[outputOffset]; }
								for(int shift : { 0, 8, 16 }) {
									result |= ((((color >> shift) & 255) * alpha + ((fallback >> shift) & 255) * (255 - alpha) + 127) / 255) << shift;
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
					if(!effectChanged[offset] && !applyGbaBackgrounds) { continue; }
					const auto& pixel = frame.Pixels[(y / _outputScale + cropTop) * frame.Width + x / _outputScale + cropLeft];
					if(applyGbaBackgrounds) {
						bool changed = gbaObjectState[offset] & 0x100;
						for(const auto& layer : *backgrounds) {
							uint32_t color;
							if(layer.Layer < 4 && layer.Layer >= 0 && layer.Sample(pixel.GbaBackgroundSamples[layer.Layer], x / _outputScale + cropLeft, y / _outputScale + cropTop, x % _outputScale, y % _outputScale, _outputScale, color) && (color >> 24)) { changed = true; break; }
						}
						if(changed) {
							uint16_t state = gbaObjectState[offset];
							_output[offset] = ComposeGbaLayers(pixel, *backgrounds, x / _outputScale + cropLeft, y / _outputScale + cropTop, x % _outputScale, y % _outputScale, _outputScale,
								state & 0x100, gbaObjectArtwork[offset], state & 0xFF, state & 0x200, state & 0x400);
						}
						continue;
					}
					if(frame.SnesColorEffects) {
						auto compose = [&](bool removeMain, bool removeSub) {
							// Color math needs only scalar state. Copying the pixel also
							// copies its coverage vectors for every alpha outcome.
							SnesColorState state(pixel);
							if(!removeMain && snesMainCoverage[offset]) { state.SnesMathEnabled = snesMainCoverage[offset]->Math && pixel.SnesWindowMathAllowed; }
							if(removeMain && !snesMainCoverage[offset]) { state.SnesMathEnabled = (pixel.SnesFallbackMainFlags & 0x80) && pixel.SnesWindowMathAllowed; }
							if(removeSub || removeMain || snesMainCoverage[offset] || snesSubCoverage[offset]) {
								bool useSub = removeSub ? (snesSubCoverage[offset] ? pixel.SnesUseSubscreen : pixel.SnesFallbackSubPriority > 0) : snesSubCoverage[offset] || pixel.SnesUseSubscreen;
								state.SnesUseSubscreen = pixel.SnesAddSubscreen && useSub;
								state.SnesHalf = pixel.SnesHalfRequested && (!pixel.SnesAddSubscreen || state.SnesUseSubscreen);
							}
							return ApplySnesColorEffect(removeMain ? Rgb555(snesMainCoverage[offset] ? pixel.SpriteColor : pixel.SnesFallbackMainColor) : effectMain[offset],
								removeSub ? Rgb555(snesSubCoverage[offset] ? pixel.SecondaryColor : pixel.SnesFallbackSubColor) : effectSecondary[offset], state);
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
					if(gbaCoverage[offset]) {
						GbaColorState state(pixel);
						state.EffectsEnabled = gbaCoverage[offset]->EffectsEnabled;
						state.SemiTransparent = gbaCoverage[offset]->SemiTransparent;
						state.SecondaryBlendTarget = gbaCoverage[offset]->SecondaryBlendTarget;
						uint32_t result = ApplyGbaColorEffect(effectMain[offset] | 0xFF000000, effectSecondary[offset], state);
						_output[offset] = CompositeAlpha(result, _output[offset], effectMain[offset] >> 24);
						continue;
					}
					uint32_t result = ApplyGbaColorEffect(effectMain[offset] | 0xFF000000, effectSecondary[offset], pixel);
					if((effectChanged[offset] & 2) && (effectSecondary[offset] >> 24) < 255) {
						GbaColorState removedSecondary(pixel);
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

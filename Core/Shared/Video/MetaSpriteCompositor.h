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

struct MetaAlternateArtwork
{
	int ArtworkOriginX = 0, ArtworkOriginY = 0;
	int ArtworkScale = 1, ArtworkWidth = 0, ArtworkHeight = 0;
	std::vector<uint32_t> Artwork;
	std::vector<MetaMemoryCondition> MemoryConditions;
};

struct MetaArtworkSelection
{
	int ArtworkOriginX, ArtworkOriginY, ArtworkScale, ArtworkWidth, ArtworkHeight;
	const std::vector<uint32_t>& Artwork;
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
	std::vector<MetaAlternateArtwork> AlternateArtwork;
	MetaArtworkSelection SelectArtwork(const MetaMemorySnapshot& snapshot) const
	{
		for(const auto& alternate : AlternateArtwork) {
			if(!alternate.MemoryConditions.empty() && MetaMemoryCondition::Matches(alternate.MemoryConditions, snapshot)) {
				return {alternate.ArtworkOriginX, alternate.ArtworkOriginY, alternate.ArtworkScale, alternate.ArtworkWidth, alternate.ArtworkHeight, alternate.Artwork};
			}
		}
		return {ArtworkOriginX, ArtworkOriginY, ArtworkScale, ArtworkWidth, ArtworkHeight, Artwork};
	}
};

// Shared recognition/composition accepts captured native state. It never consults live VRAM.
class MetaSpriteCompositor
{
	struct SnesPreparedScreen {
		struct Layer { const MetaSnesBackgroundLayer* Source; uint32_t Color; };
		Layer Layers[4] = {};
		uint32_t Backdrop = 0;
		uint8_t Count = 0, HighestMainPriority = 0, HighestSubPriority = 0;
		bool BackdropMath = false;
	};
	std::vector<SnesPreparedScreen> _snesBackgroundRow;
	std::mutex _lock;
	std::shared_ptr<const std::vector<MetaSpriteReplacement>> _groups;
	std::shared_ptr<const std::vector<MetaBackgroundReplacement>> _backgroundLayers;
	std::shared_ptr<const std::vector<uint32_t>> _memoryAddresses;
	MetaBackgroundView _activeBackgrounds;
	static std::shared_ptr<const std::vector<uint32_t>> CompileMemoryAddresses(const std::vector<MetaSpriteReplacement>& groups, const std::vector<MetaBackgroundReplacement>& backgrounds)
	{
		std::vector<uint32_t> addresses;
		for(const auto& group : groups) { for(const auto& condition : group.MemoryConditions) { addresses.push_back(condition.Address); } }
		for(const auto& group : groups) { for(const auto& alternate : group.AlternateArtwork) {
			for(const auto& condition : alternate.MemoryConditions) { addresses.push_back(condition.Address); }
		} }
		for(const auto& layer : backgrounds) { for(const auto& condition : layer.MemoryConditions) { addresses.push_back(condition.Address); } }
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
		if(alpha == 255) { return foreground | 0xFF000000; }
		if(alpha == 0) { return background | 0xFF000000; }
		uint32_t result = 0xFF000000;
		for(int shift : {0, 8, 16}) {
			result |= ((((foreground >> shift) & 255) * alpha + ((background >> shift) & 255) * (255 - alpha) + 127) / 255) << shift;
		}
		return result;
	}
	struct SnesResolvedScreen { uint32_t Color; uint8_t Priority; bool Math; };
	static SnesPreparedScreen PrepareSnesBackgroundScreen(const MetaSnesBackgroundPixel& pixel)
	{
		SnesPreparedScreen screen;
		screen.Backdrop = Rgb555(pixel.BackdropColor); screen.BackdropMath = pixel.BackdropMath;
		if(!pixel.Captured) { return screen; }
		for(const auto& layer : pixel.Layers) {
			if(layer.Sample.Valid && (layer.Main || layer.Sub)) {
				screen.Layers[screen.Count++] = {&layer, layer.Opaque ? Rgb555(layer.Color) : 0};
				if(layer.Main) { screen.HighestMainPriority = std::max(screen.HighestMainPriority, layer.Priority); }
				if(layer.Sub) { screen.HighestSubPriority = std::max(screen.HighestSubPriority, layer.Priority); }
			}
		}
		// At most four native BG layers: order directly without general sort setup.
		for(int i = 1; i < screen.Count; i++) {
			auto layer = screen.Layers[i];
			int position = i;
			while(position > 0 && screen.Layers[position - 1].Source->Priority > layer.Source->Priority) {
				screen.Layers[position] = screen.Layers[position - 1];
				position--;
			}
			screen.Layers[position] = layer;
		}
		return screen;
	}
	static SnesResolvedScreen ComposeSnesBackgroundScreen(const SnesPreparedScreen& screen, const MetaBackgroundView& backgrounds,
		bool mainScreen, int x, int y, int sx, int sy, int scale, bool objectVisible, uint32_t objectColor, uint8_t objectPriority, bool objectMath)
	{
		// Retain enabled layers even at transparent native texels. PNG alpha overlays
		// its own native layer, then layer priority determines OBJ/BG occlusion.
		// Each alpha outcome supplies an opaque OBJ color here. Lower BG layers
		// cannot change that outcome, so do not sample their PNGs or sort them.
		uint8_t highestPriority = mainScreen ? screen.HighestMainPriority : screen.HighestSubPriority;
		if(objectVisible && objectPriority >= highestPriority) { return {objectColor, objectPriority, objectMath}; }
		SnesResolvedScreen result {screen.Backdrop, 0, screen.BackdropMath};
		if(highestPriority == 0) { return result; }
		bool objectDrawn = false;
		for(int i = 0; i < screen.Count; i++) {
			const auto& layer = *screen.Layers[i].Source;
			if(!(mainScreen ? layer.Main : layer.Sub)) { continue; }
			if(objectVisible && !objectDrawn && objectPriority < layer.Priority) {
				result = {objectColor, objectPriority, objectMath}; objectDrawn = true;
			}
			uint32_t color = layer.Opaque ? screen.Layers[i].Color : result.Color;
			bool opaque = layer.Opaque;
			for(const auto& background : backgrounds) {
				uint32_t artwork;
				if(background.SampleValidated(layer.Sample, x, y, sx, sy, scale, artwork) && (artwork >> 24)) {
					color = CompositeAlpha(artwork, color, artwork >> 24); opaque = true;
				}
			}
			if(opaque) { result = {color, layer.Priority, layer.Math}; }
		}
		if(objectVisible && !objectDrawn) { result = {objectColor, objectPriority, objectMath}; }
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
	static uint32_t ComposeGbaLayers(const MetaSpritePixel& pixel, const MetaBackgroundView& backgrounds,
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
		auto snapshot = std::make_shared<const std::vector<MetaSpriteReplacement>>(std::move(groups));
		std::lock_guard<std::mutex> lock(_lock);
		static const std::vector<MetaBackgroundReplacement> emptyBackgrounds;
		auto addresses = CompileMemoryAddresses(*snapshot, _backgroundLayers ? *_backgroundLayers : emptyBackgrounds);
		_groups = std::move(snapshot);
		_memoryAddresses = std::move(addresses);
	}
	void SetBackgroundLayers(std::vector<MetaBackgroundReplacement> layers)
	{
		auto snapshot = std::make_shared<const std::vector<MetaBackgroundReplacement>>(std::move(layers));
		std::lock_guard<std::mutex> lock(_lock);
		_backgroundLayers = std::move(snapshot);
		static const std::vector<MetaSpriteReplacement> emptyGroups;
		_memoryAddresses = CompileMemoryAddresses(_groups ? *_groups : emptyGroups, *_backgroundLayers);
	}
	void SetReplacements(std::vector<MetaSpriteReplacement> groups, std::vector<MetaBackgroundReplacement> layers)
	{
		auto addresses = CompileMemoryAddresses(groups, layers);
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
	bool HasBackgroundLayers()
	{
		std::lock_guard<std::mutex> lock(_lock);
		return _backgroundLayers && !_backgroundLayers->empty();
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
		_activeBackgrounds.Layers.clear();
		if(backgrounds) { for(const auto& layer : *backgrounds) {
			if(layer.IsValid() && MetaMemoryCondition::Matches(layer.MemoryConditions, frame.MemorySnapshot)) { _activeBackgrounds.Layers.push_back(&layer); }
		} }
		bool applyBackgrounds = !_activeBackgrounds.Layers.empty() && backgroundColor && !frame.GbaColorEffects && !frame.SnesColorEffects
			&& frame.Pixels.size() == (size_t)frame.Width * frame.Height
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) { return pixel.BackgroundSample.Valid; });
		bool applyGbaBackgrounds = !_activeBackgrounds.Layers.empty() && frame.GbaColorEffects
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) {
				return pixel.GbaFallbackCaptured && std::any_of(std::begin(pixel.GbaBackgroundSamples), std::end(pixel.GbaBackgroundSamples), [](const auto& sample) { return sample.Valid; });
			});
		bool applySnesBackgrounds = !_activeBackgrounds.Layers.empty() && frame.SnesColorEffects
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) {
				return pixel.SnesFallbackCaptured && (pixel.BackgroundSample.Valid || pixel.SnesSubBackgroundSample.Valid);
			});
		if(!_activeBackgrounds.Layers.empty() && frame.SnesColorEffects && frame.SnesBackgrounds.size() == frame.Pixels.size()) {
			applySnesBackgrounds |= std::any_of(frame.SnesBackgrounds.begin(), frame.SnesBackgrounds.end(), [](const auto& pixel) {
				return pixel.Captured && std::any_of(std::begin(pixel.Layers), std::end(pixel.Layers), [](const auto& layer) { return layer.Sample.Valid && (layer.Main || layer.Sub); });
			});
		}
		if(!original || cropLeft < 0 || cropTop < 0 || width + cropLeft > frame.Width || height + cropTop > frame.Height
			|| frame.Pixels.size() != static_cast<uint64_t>(frame.Width) * frame.Height
			|| ((!groups || groups->empty()) && !applyBackgrounds && !applyGbaBackgrounds && !applySnesBackgrounds)) { return original; }
		if(!groups) { groups = std::make_shared<const std::vector<MetaSpriteReplacement>>(); }
		auto candidates = !frame.NesRows.empty() ? BuildNesCandidates(frame, backgroundColor)
			: frame.DecodedRows.empty() ? BuildGameboyCandidates(frame) : BuildDecodedCandidates(frame);
		for(const auto& group : *groups) {
			if(MetaMemoryCondition::Matches(group.MemoryConditions, frame.MemorySnapshot)) {
				auto artwork = group.SelectArtwork(frame.MemorySnapshot);
				_outputScale = std::max(_outputScale, (uint32_t)std::clamp(artwork.ArtworkScale, 1, 10));
			}
		}
		if(applyBackgrounds || applyGbaBackgrounds || applySnesBackgrounds) { for(const auto& layer : _activeBackgrounds) { if(layer.IsValid()) { _outputScale = std::max(_outputScale, (uint32_t)layer.Scale); } } }
		uint32_t outputWidth = width * _outputScale;
		_output.resize((size_t)outputWidth * height * _outputScale);
		bool stagedEffects = frame.GbaColorEffects || frame.SnesColorEffects;
		if(stagedEffects) {
			_effectMain.resize(_output.size()); _effectSecondary.resize(_output.size()); _effectChanged.assign(_output.size(), 0);
			if(frame.GbaColorEffects) { _gbaCoverage.assign(_output.size(), nullptr); }
			if(applyGbaBackgrounds) { _gbaObjectArtwork.resize(_output.size()); _gbaObjectState.assign(_output.size(), 0); }
			if(frame.SnesColorEffects) { _snesMainCoverage.assign(_output.size(), nullptr); _snesSubCoverage.assign(_output.size(), nullptr); }
		}
		// All staging buffers are fully sized above and remain fixed during the
		// pixel loops. Cache their data once rather than checking vector access
		// repeatedly for every alpha outcome in Debug builds.
		auto* outputPixels = _output.data();
		const auto* framePixels = frame.Pixels.data();
		auto* effectMain = _effectMain.data(); auto* effectSecondary = _effectSecondary.data();
		auto* effectChanged = _effectChanged.data(); auto* gbaCoverage = _gbaCoverage.data();
		auto* gbaObjectArtwork = _gbaObjectArtwork.data(); auto* gbaObjectState = _gbaObjectState.data();
		auto* snesMainCoverage = _snesMainCoverage.data(); auto* snesSubCoverage = _snesSubCoverage.data();
		for(uint32_t y = 0; y < height * _outputScale; y++) {
			for(uint32_t x = 0; x < outputWidth; x++) {
				size_t offset = (size_t)y * outputWidth + x;
				outputPixels[offset] = original[(y / _outputScale) * width + x / _outputScale];
				if(stagedEffects) {
					const auto& pixel = framePixels[(y / _outputScale + cropTop) * frame.Width + x / _outputScale + cropLeft];
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
			MetaBackgroundCompositor::Apply(*backgroundFrame, _activeBackgrounds, _outputScale, _output, cropLeft, cropTop);
			if(MetaBackgroundCompositor::BuildFallback(*backgroundFrame, _activeBackgrounds, _outputScale, backgroundColor, generatedFallback, cropLeft, cropTop)) { backgroundFallback = &generatedFallback; }
		}
		std::vector<uint8_t> consumedStorage(candidates.size(), 0);
		uint8_t* consumed = consumedStorage.data();
		std::vector<size_t> matched;
		for(const auto& group : *groups) {
			if(!MetaMemoryCondition::Matches(group.MemoryConditions, frame.MemorySnapshot)) { continue; }
			auto artwork = group.SelectArtwork(frame.MemorySnapshot);
			if(group.Members.empty() || artwork.ArtworkScale < 1 || artwork.ArtworkWidth < 1 || artwork.ArtworkHeight < 1
				|| artwork.Artwork.size() != (size_t)artwork.ArtworkWidth * artwork.ArtworkHeight) { continue; }
			const auto& anchor = group.Members[0];
			for(size_t i = 0; i < candidates.size(); i++) {
				if(consumed[i] || candidates[i].Width != anchor.Width || candidates[i].Height != anchor.Height || candidates[i].Pixels != anchor.Pixels) { continue; }
				int left = candidates[i].X - anchor.X, top = candidates[i].Y - anchor.Y;
				matched.clear();
				matched.push_back(i);
				for(size_t m = 1; m < group.Members.size(); m++) {
					const auto& member = group.Members[m];
					for(size_t j = 0; j < candidates.size(); j++) {
						const auto& candidate = candidates[j];
						if(candidate.X == left + member.X && candidate.Y == top + member.Y && candidate.Width == member.Width && candidate.Height == member.Height
							&& !consumed[j] && candidate.Pixels == member.Pixels && std::find(matched.begin(), matched.end(), j) == matched.end()) { matched.push_back(j); break; }
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
				int artworkLeft = left + group.OriginX - artwork.ArtworkOriginX;
				int artworkTop = top + group.OriginY - artwork.ArtworkOriginY;
				// Include both artwork and fetched members: a smaller/shifted PNG
				// must still remove original pixels outside its own canvas.
				int64_t boundLeft = artworkLeft, boundTop = artworkTop;
				int64_t boundRight = boundLeft + (artwork.ArtworkWidth + artwork.ArtworkScale - 1) / artwork.ArtworkScale;
				int64_t boundBottom = boundTop + (artwork.ArtworkHeight + artwork.ArtworkScale - 1) / artwork.ArtworkScale;
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
						const auto& pixel = framePixels[(y + cropTop) * frame.Width + x + cropLeft];
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
								int ax = ((int)x + cropLeft - artworkLeft) * artwork.ArtworkScale + (int)(sx * artwork.ArtworkScale / _outputScale);
								int ay = ((int)y + cropTop - artworkTop) * artwork.ArtworkScale + (int)(sy * artwork.ArtworkScale / _outputScale);
								if(ax >= 0 && ay >= 0 && ax < artwork.ArtworkWidth && ay < artwork.ArtworkHeight) { color = artwork.Artwork[ay * artwork.ArtworkWidth + ax]; }
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
								if(!originalMemberVisible) { fallback = outputPixels[outputOffset]; }
								for(int shift : { 0, 8, 16 }) {
									result |= ((((color >> shift) & 255) * alpha + ((fallback >> shift) & 255) * (255 - alpha) + 127) / 255) << shift;
								}
								outputPixels[(size_t)(y * _outputScale + sy) * outputWidth + x * _outputScale + sx] = result;
							}
						}
					}
				}
			}
		}
		if(stagedEffects) {
			bool prepareSnesLayers = applySnesBackgrounds && frame.SnesBackgrounds.size() == frame.Pixels.size();
			if(prepareSnesLayers) { _snesBackgroundRow.resize(width); }
			auto* snesBackgroundRow = _snesBackgroundRow.data();
			const auto* capturedSnesBackgrounds = frame.SnesBackgrounds.data();
			for(uint32_t y = 0; y < height * _outputScale; y++) {
				if(prepareSnesLayers && y % _outputScale == 0) {
					for(uint32_t nx = 0; nx < width; nx++) {
						const auto& rowBackgrounds = capturedSnesBackgrounds[(y / _outputScale + cropTop) * frame.Width + nx + cropLeft];
						snesBackgroundRow[nx] = PrepareSnesBackgroundScreen(rowBackgrounds);
					}
				}
				for(uint32_t x = 0; x < outputWidth; x++) {
					size_t offset = (size_t)y * outputWidth + x;
					if(!effectChanged[offset] && !applyGbaBackgrounds && !applySnesBackgrounds) { continue; }
					const auto& pixel = framePixels[(y / _outputScale + cropTop) * frame.Width + x / _outputScale + cropLeft];
					if(applyGbaBackgrounds) {
						bool changed = gbaObjectState[offset] & 0x100;
						for(const auto& layer : _activeBackgrounds) {
							uint32_t color;
							if(layer.Layer < 4 && layer.Layer >= 0 && layer.Sample(pixel.GbaBackgroundSamples[layer.Layer], x / _outputScale + cropLeft, y / _outputScale + cropTop, x % _outputScale, y % _outputScale, _outputScale, color) && (color >> 24)) { changed = true; break; }
						}
						if(changed) {
							uint16_t state = gbaObjectState[offset];
							outputPixels[offset] = ComposeGbaLayers(pixel, _activeBackgrounds, x / _outputScale + cropLeft, y / _outputScale + cropTop, x % _outputScale, y % _outputScale, _outputScale,
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
							uint32_t main = removeMain ? Rgb555(snesMainCoverage[offset] ? pixel.SpriteColor : pixel.SnesFallbackMainColor) : effectMain[offset];
							uint32_t secondary = removeSub ? Rgb555(snesSubCoverage[offset] ? pixel.SecondaryColor : pixel.SnesFallbackSubColor) : effectSecondary[offset];
							if(applySnesBackgrounds && pixel.SnesFallbackCaptured) {
								bool mainBackground = removeMain ? !snesMainCoverage[offset] || pixel.VisibleSprite < 0 : !(effectChanged[offset] & 1) && pixel.VisibleSprite < 0;
								bool subBackground = removeSub ? !snesSubCoverage[offset] || pixel.SecondarySprite < 0 : !(effectChanged[offset] & 2) && pixel.SecondarySprite < 0;
								const MetaSnesBackgroundPixel* backgrounds = prepareSnesLayers
									? &capturedSnesBackgrounds[(y / _outputScale + cropTop) * frame.Width + x / _outputScale + cropLeft] : nullptr;
								if(backgrounds && backgrounds->Captured) {
									uint8_t mainPriority = !removeMain && snesMainCoverage[offset] ? snesMainCoverage[offset]->Priority : backgrounds->MainPriority;
									uint8_t subPriority = !removeSub && snesSubCoverage[offset] ? snesSubCoverage[offset]->Priority : backgrounds->SubPriority;
									int px = x / _outputScale + cropLeft, py = y / _outputScale + cropTop;
									auto resolvedMain = ComposeSnesBackgroundScreen(snesBackgroundRow[x / _outputScale], _activeBackgrounds, true, px, py, x % _outputScale, y % _outputScale,
										_outputScale, !mainBackground, main, mainPriority, state.SnesMathEnabled);
									auto resolvedSub = ComposeSnesBackgroundScreen(snesBackgroundRow[x / _outputScale], _activeBackgrounds, false, px, py, x % _outputScale, y % _outputScale,
										_outputScale, !subBackground, secondary, subPriority, false);
									main = resolvedMain.Color; secondary = resolvedSub.Color;
									state.SnesMathEnabled = resolvedMain.Math && pixel.SnesWindowMathAllowed;
									state.SnesUseSubscreen = pixel.SnesAddSubscreen && resolvedSub.Priority > 0;
									state.SnesHalf = pixel.SnesHalfRequested && (!pixel.SnesAddSubscreen || state.SnesUseSubscreen);
								} else for(const auto& layer : _activeBackgrounds) {
									uint32_t artwork;
									if(mainBackground && layer.SampleValidated(pixel.BackgroundSample, x / _outputScale + cropLeft, y / _outputScale + cropTop,
										x % _outputScale, y % _outputScale, _outputScale, artwork)) { main = CompositeAlpha(artwork, main, artwork >> 24); }
									if(subBackground && layer.SampleValidated(pixel.SnesSubBackgroundSample, x / _outputScale + cropLeft, y / _outputScale + cropTop,
										x % _outputScale, y % _outputScale, _outputScale, artwork)) { secondary = CompositeAlpha(artwork, secondary, artwork >> 24); }
								}
							}
							return ApplySnesColorEffect(main, secondary, state);
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
						outputPixels[offset] = result;
						continue;
					}
					if(gbaCoverage[offset]) {
						GbaColorState state(pixel);
						state.EffectsEnabled = gbaCoverage[offset]->EffectsEnabled;
						state.SemiTransparent = gbaCoverage[offset]->SemiTransparent;
						state.SecondaryBlendTarget = gbaCoverage[offset]->SecondaryBlendTarget;
						uint32_t result = ApplyGbaColorEffect(effectMain[offset] | 0xFF000000, effectSecondary[offset], state);
						outputPixels[offset] = CompositeAlpha(result, outputPixels[offset], effectMain[offset] >> 24);
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
					outputPixels[offset] = result;
				}
			}
		}
		return _output.data();
	}
};

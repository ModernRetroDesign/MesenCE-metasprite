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
	uint8_t ExpansionClass = 0;
	int ExpansionOrder = -1;
	uint8_t GbaPriority = 0xFF;
	bool GbaSemiTransparent = false;
	uint8_t SnesPriorityCode = 0xFF, SnesPalette = 0;
	uint16_t SnesFetchOrder = 0;
	uint8_t SnesField = 0;
	uint8_t SnesRowStep = 1, SnesRowPhase = 0;
	int GetExpansionOrder() const { return ExpansionOrder >= 0 ? ExpansionOrder : HardwareSprite; }
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
		for(const auto& layer : backgrounds) {
			for(const auto& condition : layer.MemoryConditions) { addresses.push_back(condition.Address); }
			if(layer.UseCpuCamera && layer.FollowScroll) { layer.CameraX.AddAddresses(addresses); layer.CameraY.AddAddresses(addresses); }
		}
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
		if(backgrounds.HasSnesIndependentPriority()) {
			struct Entry { const MetaSnesBackgroundLayer* Source; const MetaBackgroundReplacement* Artwork; uint32_t Color; uint8_t Priority; };
			// Main/subscreen resolution is synchronous and nonrecursive. Reuse
			// per-thread storage rather than allocate it for every output texel.
			thread_local std::vector<Entry> entries; entries.clear(); entries.reserve(screen.Count + backgrounds.Layers.size());
			for(int i = 0; i < screen.Count; i++) {
				const auto& prepared = screen.Layers[i]; const auto& layer = *prepared.Source;
				if(!(mainScreen ? layer.Main : layer.Sub)) { continue; }
				entries.push_back({&layer, nullptr, prepared.Color, layer.Priority});
			}
			// Add authored layers after native layers, so PNG wins a BG tie.
			// Stable sorting retains saved draw order among equal-priority PNGs.
			for(const auto& background : backgrounds) {
				if(background.CompositionPosition != 3) { continue; }
				for(int i = 0; i < screen.Count; i++) {
					const auto& layer = *screen.Layers[i].Source;
					if(layer.Sample.Layer != background.Layer || !(mainScreen ? layer.Main : layer.Sub)) { continue; }
					entries.push_back({&layer, &background, 0, (uint8_t)background.SnesPriority}); break;
				}
			}
			auto* data = entries.data();
			for(size_t i = 1; i < entries.size(); i++) {
				auto entry = data[i]; size_t position = i;
				while(position && data[position - 1].Priority > entry.Priority) { data[position] = data[position - 1]; position--; }
				data[position] = entry;
			}
			SnesResolvedScreen result {screen.Backdrop, 0, screen.BackdropMath}; bool objectDrawn = false;
			for(size_t index = 0; index < entries.size(); index++) {
				const auto& entry = data[index];
				if(objectVisible && !objectDrawn && objectPriority < entry.Priority) { result = {objectColor, objectPriority, objectMath}; objectDrawn = true; }
				const auto& layer = *entry.Source;
				if(entry.Artwork) {
					uint32_t artwork;
					if(backgrounds.Sample(*entry.Artwork, layer.Sample, x, y, sx, sy, scale, artwork, true) && (artwork >> 24)) {
						result = {CompositeAlpha(artwork, result.Color, artwork >> 24), entry.Priority, layer.Math};
					}
				} else {
					bool opaque = layer.Opaque && !backgrounds.HidesNativeLayer(layer.Sample.Layer);
					uint32_t color = opaque ? entry.Color : result.Color;
					for(const auto& background : backgrounds) {
						uint32_t artwork;
						if(backgrounds.Sample(background, layer.Sample, x, y, sx, sy, scale, artwork) && (artwork >> 24)) { color = CompositeAlpha(artwork, color, artwork >> 24); opaque = true; }
					}
					if(opaque) { result = {color, entry.Priority, layer.Math}; }
				}
			}
			if(objectVisible && !objectDrawn) { result = {objectColor, objectPriority, objectMath}; }
			return result;
		}
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
			bool opaque = layer.Opaque && !backgrounds.HidesNativeLayer(layer.Sample.Layer);
			uint32_t color = opaque ? screen.Layers[i].Color : result.Color;
			for(const auto& background : backgrounds) {
				uint32_t artwork;
				if(backgrounds.Sample(background, layer.Sample, x, y, sx, sy, scale, artwork) && (artwork >> 24)) {
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
	struct GbaArtworkLayer { uint32_t Color; uint8_t Priority; bool SemiTransparent; int Order; bool Native = false; };
	static uint32_t ComposeGbaLayers(const MetaSpritePixel& pixel, const MetaBackgroundView& backgrounds,
		int x, int y, int sx, int sy, int scale, bool overrideObject = false, uint32_t objectArtwork = 0,
		uint8_t objectPriority = 0, bool objectSemiTransparent = false, bool preserveNativeObject = false,
		const std::vector<GbaArtworkLayer>* objects = nullptr)
	{
		if(overrideObject && preserveNativeObject && (objectArtwork >> 24) < 255) {
			uint32_t foreground = ComposeGbaLayers(pixel, backgrounds, x, y, sx, sy, scale, true, objectArtwork | 0xFF000000, objectPriority, objectSemiTransparent);
			uint32_t fallback = ComposeGbaLayers(pixel, backgrounds, x, y, sx, sy, scale);
			return CompositeAlpha(foreground, fallback, objectArtwork >> 24);
		}
		struct Layer { int Index, Priority; uint32_t Color; bool SemiTransparent; bool Artwork = false; };
		std::vector<Layer> storage;
		storage.reserve((objects ? objects->size() + 5 : 6) + backgrounds.Layers.size());
		if(objects) {
			for(const auto& object : *objects) { storage.push_back({4, object.Priority, object.Color, object.SemiTransparent, !object.Native}); }
		} else if(overrideObject || pixel.GbaObjectSprite >= 0) {
			storage.push_back({4, overrideObject ? objectPriority : pixel.GbaObjectPriority,
				overrideObject ? objectArtwork : Rgb555(pixel.GbaObjectColor), overrideObject ? objectSemiTransparent : pixel.GbaObjectSemiTransparent, overrideObject});
		}
		for(int i = 0; i < 4; i++) {
			if(!pixel.GbaBackgroundSamples[i].Valid) { continue; }
			// Insert independent PNGs as separate layers. Reversing saved order
			// puts higher authored slots first at a hardware-priority tie.
			const auto* backgroundData = backgrounds.Layers.data();
			for(size_t entry = backgrounds.Layers.size(); entry > 0; --entry) {
				const auto& background = *backgroundData[entry - 1]; uint32_t artwork;
				if(background.CompositionPosition != 2 || !backgrounds.Sample(background, pixel.GbaBackgroundSamples[i], x, y, sx, sy, scale, artwork, true)) { continue; }
				storage.push_back({i, background.GbaPriority, artwork, false, true});
			}
			uint32_t color = pixel.GbaBackgroundLayers[i].Opaque && !backgrounds.HidesNativeLayer(i) ? Rgb555(pixel.GbaBackgroundLayers[i].Color) : 0;
			bool artworkUsed = false;
			for(const auto& background : backgrounds) {
				uint32_t artwork;
				if(!backgrounds.Sample(background, pixel.GbaBackgroundSamples[i], x, y, sx, sy, scale, artwork)) { continue; }
				uint32_t a = artwork >> 24, b = color >> 24;
				artworkUsed |= a != 0;
				uint32_t alphaNumerator = a * 255 + b * (255 - a);
				if(alphaNumerator == 0) { color = 0; continue; }
				uint32_t merged = ((alphaNumerator + 127) / 255) << 24;
				for(int shift : {0, 8, 16}) {
					uint32_t channel = (((artwork >> shift) & 255) * a * 255 + ((color >> shift) & 255) * b * (255 - a) + alphaNumerator / 2) / alphaNumerator;
					merged |= channel << shift;
				}
				color = merged;
			}
			storage.push_back({i, pixel.GbaBackgroundLayers[i].Priority, color, false, artworkUsed});
		}
		storage.push_back({5, 255, Rgb555(pixel.GbaBackdropColor), false});
		auto* layers = storage.data(); int count = (int)storage.size();
		// Stable ties retain OBJ ahead of BG0..BG3, then BG order.
		std::stable_sort(layers, layers + count, [](const Layer& a, const Layer& b) { return a.Priority < b.Priority; });
		double channels[3] = {};
		auto compose = [&](const Layer& main, const Layer& secondary, double weight) {
			GbaColorState state(pixel);
			state.EffectsEnabled = pixel.GbaWindowEffectsEnabled && (pixel.GbaBlendMainMask & (1 << main.Index));
			state.SecondaryBlendTarget = pixel.GbaBlendSubMask & (1 << secondary.Index);
			state.SemiTransparent = main.SemiTransparent;
			uint32_t color = ApplyGbaColorEffect(main.Color | 0xFF000000, secondary.Color | 0xFF000000, state);
			// Native-only outcomes retain the hardware's five-bit effect rounding.
			if(!main.Artwork && !secondary.Artwork) {
				uint32_t nativeMain = 0xFF000000, nativeSub = 0xFF000000;
				for(int shift : {0, 8, 16}) {
					nativeMain |= (((main.Color >> shift) & 255) >> 3) << shift;
					nativeSub |= (((secondary.Color >> shift) & 255) >> 3) << shift;
				}
				// Effect white is 31 per channel in native precision.
				bool blend = state.SecondaryBlendTarget && (state.SemiTransparent || (state.EffectsEnabled && state.Effect == 1));
				uint32_t a = blend ? std::min<uint32_t>(16, state.MainCoefficient) : 16 - std::min<uint32_t>(16, state.Brightness);
				uint32_t b = blend ? std::min<uint32_t>(16, state.SecondaryCoefficient) : std::min<uint32_t>(16, state.Brightness);
				color = 0xFF000000;
				for(int shift : {0, 8, 16}) {
					uint32_t value = (nativeMain >> shift) & 31;
					if(blend || (state.EffectsEnabled && (state.Effect == 2 || state.Effect == 3))) {
						uint32_t subValue = blend ? (nativeSub >> shift) & 31 : state.Effect == 2 ? 31 : 0;
						value = std::min<uint32_t>(31, (value * a + subValue * b) / 16);
					}
					color |= ((value << 3) | (value >> 2)) << shift;
				}
			}
			for(int c = 0; c < 3; c++) { channels[c] += ((color >> (c * 8)) & 255) * weight; }
		};
		auto visit = [&](auto&& self, int index, const Layer* main, double weight) -> void {
			if(index == count) {
				if(main) { compose(*main, layers[count - 1], weight); }
				return;
			}
			const auto& layer = layers[index];
			// GBA blends the winning OBJ with a background, never another OBJ.
			if(main && main->Index == 4 && layer.Index == 4) { self(self, index + 1, main, weight); return; }
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
				|| (row.SnesRowStep != 1 && row.SnesRowStep != 2) || row.SnesRowPhase >= row.SnesRowStep
				|| (row.SnesRowStep == 2 && (!frame.SnesColorEffects || row.SnesPriorityCode > 3 || row.Height < 2))
				|| row.Row % row.SnesRowStep != row.SnesRowPhase
				|| row.Row < 0 || row.Row >= row.Height || row.OffsetX < 0 || row.OffsetX >= row.Width
				|| row.Pixels.empty() || row.Pixels.size() > (size_t)(row.Width - row.OffsetX)) { continue; }
			auto found = std::find_if(result.begin(), result.end(), [&](const auto& item) { return item.HardwareSprite == row.Sprite; });
			if(found == result.end()) {
				result.push_back({row.X, row.Y, row.Width, row.Height, row.Sprite, std::vector<uint32_t>(row.Width * row.Height)});
				observed.push_back(std::vector<bool>(row.Width * row.Height)); invalid.push_back(false);
				found = result.end() - 1;
				found->ExpansionClass = row.ExpansionClass;
				found->GbaPriority = row.GbaPriority;
				found->GbaSemiTransparent = row.GbaSemiTransparent;
				found->SnesPriorityCode = row.SnesPriorityCode;
				found->SnesPalette = row.SnesPalette;
				found->SnesField = row.SnesField;
				found->SnesRowStep = row.SnesRowStep; found->SnesRowPhase = row.SnesRowPhase;
			}
			found->SnesFetchOrder = std::max(found->SnesFetchOrder, row.SnesFetchOrder);
			size_t index = found - result.begin();
			if(found->X != row.X || found->Y != row.Y || found->Width != row.Width || found->Height != row.Height
				|| found->SnesRowStep != row.SnesRowStep || found->SnesRowPhase != row.SnesRowPhase) { invalid[index] = true; continue; }
			size_t offset = row.Row * row.Width + row.OffsetX;
			for(size_t x = 0; x < row.Pixels.size(); x++) {
				if(observed[index][offset + x] && found->Pixels[offset + x] != row.Pixels[x]) { invalid[index] = true; }
				found->Pixels[offset + x] = row.Pixels[x];
				observed[index][offset + x] = true;
			}
		}
		std::vector<MetaSpriteGraphic> complete;
		for(size_t i = 0; i < result.size(); i++) {
			bool completeRows = !invalid[i];
			for(int y = result[i].SnesRowPhase; completeRows && y < result[i].Height; y += result[i].SnesRowStep) {
				for(int x = 0; x < result[i].Width; x++) { if(!observed[i][y * result[i].Width + x]) { completeRows = false; break; } }
			}
			if(completeRows) { complete.push_back(std::move(result[i])); }
		}
		return complete;
	}

	// Compare only explicitly captured source rows. PNG alpha is unrelated to
	// this content recognition; saved members always contain the full artwork.
	static bool MatchesGraphic(const MetaSpriteGraphic& candidate, const MetaSpriteGraphic& member)
	{
		if(candidate.Width != member.Width || candidate.Height != member.Height || candidate.Width < 1 || candidate.Height < 1
			|| candidate.Pixels.size() != (size_t)candidate.Width * candidate.Height || member.Pixels.size() != candidate.Pixels.size()
			|| (candidate.SnesRowStep != 1 && candidate.SnesRowStep != 2) || candidate.SnesRowPhase >= candidate.SnesRowStep
			|| (candidate.SnesRowStep == 2 && candidate.Height < 2)) { return false; }
		if(candidate.SnesRowStep == 1) { return candidate.Pixels == member.Pixels; }
		for(int y = candidate.SnesRowPhase; y < candidate.Height; y += candidate.SnesRowStep) {
			for(int x = 0; x < candidate.Width; x++) { if(candidate.Pixels[y * candidate.Width + x] != member.Pixels[y * candidate.Width + x]) { return false; } }
		}
		return true;
	}

	uint32_t GetOutputScale() const { return _outputScale; }
	std::shared_ptr<const std::vector<MetaBackgroundReplacement>> GetBackgroundLayers()
	{
		std::lock_guard<std::mutex> lock(_lock);
		return _backgroundLayers;
	}
	static std::vector<MetaSpriteGraphic> BuildNesCandidates(const MetaSpriteFrame& frame, const std::function<uint32_t(uint16_t)>& nativeColor)
	{
		if(!nativeColor) { return {}; }
		MetaSpriteFrame decoded(0, 0);
		decoded.DecodedRows.reserve(frame.NesRows.size());
		for(const auto& row : frame.NesRows) {
			int y = (int)row.Scanline - row.Y;
			if(row.Sprite >= 64 || (row.Height != 8 && row.Height != 16) || y < 0 || y >= row.Height) { continue; }
			MetaSpriteDecodedRow graphic {row.Sprite, row.X, row.Y, y, 8, row.Height, {}};
			graphic.ExpansionClass = (row.Attributes & 0x20) ? 2 : 1;
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
				sprite.ExpansionClass = (row.Attributes & 0x80) ? 2 : 1;
				sprite.ExpansionOrder = row.ExpansionOrder;
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
			int cameraX, cameraY;
			bool cameraValid = !layer.UseCpuCamera || !layer.FollowScroll || (layer.CameraX.Decode(frame.MemorySnapshot, cameraX) && layer.CameraY.Decode(frame.MemorySnapshot, cameraY));
			if(layer.IsValid() && cameraValid && MetaMemoryCondition::Matches(layer.MemoryConditions, frame.MemorySnapshot)) { _activeBackgrounds.Layers.push_back(&layer); }
		} }
		bool applyForegroundBackgrounds = std::any_of(_activeBackgrounds.Layers.begin(), _activeBackgrounds.Layers.end(), [](const auto* layer) { return layer->CompositionPosition == 1; });
		bool wsNeedsStages = frame.WsCapture && ((groups && !groups->empty())
			|| std::any_of(_activeBackgrounds.Layers.begin(), _activeBackgrounds.Layers.end(), [](const auto* layer) { return layer->CompositionPosition == 0 || layer->CompositionPosition >= 4; }));
		// Assets can activate after this frame's capture began. Wait for complete
		// BG stages rather than partially hiding/drawing a pack during that frame.
		if(wsNeedsStages && frame.BackgroundUnderlays.size() != frame.Pixels.size()) { return original; }
		bool applyOrderedSpriteScene = backgroundColor && !frame.GbaColorEffects && !frame.SnesColorEffects
			&& (!frame.WsCapture || frame.BackgroundUnderlays.size() == frame.Pixels.size())
			&& (std::any_of(_activeBackgrounds.Layers.begin(), _activeBackgrounds.Layers.end(), [](const auto* layer) { return layer->CompositionPosition >= 4; })
				|| (!frame.NesRows.empty() && std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) { return pixel.NativeBackdropCaptured; })
					&& ((groups && !groups->empty()) || std::any_of(_activeBackgrounds.Layers.begin(), _activeBackgrounds.Layers.end(), [](const auto* layer) { return layer->CompositionPosition == 0; }))));
		bool gameboyScene = backgroundColor && (frame.GameboyCapture || !frame.Rows.empty()) && frame.NesRows.empty() && frame.DecodedRows.empty()
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) { return pixel.NativeBackdropCaptured; })
			&& ((groups && !groups->empty()) || std::any_of(_activeBackgrounds.Layers.begin(), _activeBackgrounds.Layers.end(), [](const auto* layer) { return layer->CompositionPosition == 0 || layer->CompositionPosition >= 4; }));
		// Select an OBJ before deciding whether BG hides that OBJ.
		applyOrderedSpriteScene |= gameboyScene;
		bool pceScene = backgroundColor && frame.PceCapture
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) { return pixel.NativeBackdropCaptured; })
			&& ((groups && !groups->empty()) || std::any_of(_activeBackgrounds.Layers.begin(), _activeBackgrounds.Layers.end(), [](const auto* layer) { return layer->CompositionPosition == 0 || layer->CompositionPosition >= 4; }));
		applyOrderedSpriteScene |= pceScene;
		bool smsScene = backgroundColor && frame.SmsCapture
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) { return pixel.NativeBackdropCaptured; })
			&& ((groups && !groups->empty()) || std::any_of(_activeBackgrounds.Layers.begin(), _activeBackgrounds.Layers.end(), [](const auto* layer) { return layer->CompositionPosition == 0 || layer->CompositionPosition >= 4; }));
		applyOrderedSpriteScene |= smsScene;
		bool wsScene = backgroundColor && frame.WsCapture && frame.BackgroundUnderlays.size() == frame.Pixels.size()
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) { return pixel.NativeBackdropCaptured; })
			&& ((groups && !groups->empty()) || std::any_of(_activeBackgrounds.Layers.begin(), _activeBackgrounds.Layers.end(), [](const auto* layer) { return layer->CompositionPosition == 0 || layer->CompositionPosition >= 4; }));
		applyOrderedSpriteScene |= wsScene;
		bool decodedScene = pceScene || smsScene || wsScene;
		bool applyBackgrounds = !_activeBackgrounds.Layers.empty() && backgroundColor && !frame.GbaColorEffects && !frame.SnesColorEffects
			&& frame.Pixels.size() == (size_t)frame.Width * frame.Height
			&& std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) { return pixel.BackgroundSample.Valid; });
		_activeBackgrounds.PrepareRasterReferences(frame);
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
		if(!original || frame.SpriteCoordinateScale < 1 || frame.SpriteCoordinateScale > 2 || cropLeft < 0 || cropTop < 0 || width + cropLeft > frame.Width || height + cropTop > frame.Height
			|| frame.Pixels.size() != static_cast<uint64_t>(frame.Width) * frame.Height
			|| ((!groups || groups->empty()) && !applyBackgrounds && !applyGbaBackgrounds && !applySnesBackgrounds && !applyForegroundBackgrounds && !applyOrderedSpriteScene)) { return original; }
		if(!groups) { groups = std::make_shared<const std::vector<MetaSpriteReplacement>>(); }
		auto candidates = !frame.NesRows.empty() ? BuildNesCandidates(frame, backgroundColor)
			: frame.DecodedRows.empty() ? BuildGameboyCandidates(frame) : BuildDecodedCandidates(frame);
		for(const auto& group : *groups) {
			auto includeScale = [&](const MetaMemorySnapshot& snapshot) {
				if(MetaMemoryCondition::Matches(group.MemoryConditions, snapshot)) {
					auto artwork = group.SelectArtwork(snapshot);
					_outputScale = std::max(_outputScale, (uint32_t)((std::clamp(artwork.ArtworkScale, 1, 10) + frame.SpriteCoordinateScale - 1) / frame.SpriteCoordinateScale));
				}
			};
			includeScale(frame.MemorySnapshot);
			if(frame.SnesFieldMemoryCaptured) { includeScale(frame.SnesFieldMemory[0]); includeScale(frame.SnesFieldMemory[1]); }
		}
		if(applyBackgrounds || applyGbaBackgrounds || applySnesBackgrounds || applyForegroundBackgrounds || applyOrderedSpriteScene) { for(const auto& layer : _activeBackgrounds) { if(layer.IsValid()) { _outputScale = std::max(_outputScale, (uint32_t)layer.Scale); } } }
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
		if(applyBackgrounds && !applyOrderedSpriteScene) {
			const MetaSpriteFrame* backgroundFrame = &frame;
			std::unique_ptr<MetaSpriteFrame> croppedBackground;
			if(cropLeft || cropTop || width != frame.Width || height != frame.Height) {
				croppedBackground = std::make_unique<MetaSpriteFrame>(width, height);
				for(uint32_t y = 0; y < height; y++) {
					std::copy_n(frame.Pixels.begin() + (y + cropTop) * frame.Width + cropLeft, width, croppedBackground->Pixels.begin() + y * width);
					if(frame.BackgroundUnderlays.size() == frame.Pixels.size()) {
						if(croppedBackground->BackgroundUnderlays.empty()) { croppedBackground->BackgroundUnderlays.resize((size_t)width * height); }
						std::copy_n(frame.BackgroundUnderlays.begin() + (y + cropTop) * frame.Width + cropLeft, width, croppedBackground->BackgroundUnderlays.begin() + y * width);
					}
				}
				backgroundFrame = croppedBackground.get();
			}
			MetaBackgroundCompositor::Apply(*backgroundFrame, _activeBackgrounds, _outputScale, _output, cropLeft, cropTop, backgroundColor);
			if(MetaBackgroundCompositor::BuildFallback(*backgroundFrame, _activeBackgrounds, _outputScale, backgroundColor, generatedFallback, cropLeft, cropTop)) { backgroundFallback = &generatedFallback; }
		}
		std::vector<uint8_t> consumedStorage(candidates.size(), 0);
		uint8_t* consumed = consumedStorage.data();
		std::vector<size_t> matched;
		struct ExpandedArtwork {
			MetaArtworkSelection Image;
			int Left, Top, Order;
			uint8_t Class;
			std::vector<size_t> Members;
			bool Native = false;
		};
		std::vector<ExpandedArtwork> expansions;
		struct GbaCanvas { MetaArtworkSelection Image; int Left, Top, Order; std::vector<size_t> Members; };
		std::vector<GbaCanvas> gbaCanvases;
		struct SnesCanvas { MetaArtworkSelection Image; int Left, Top; std::vector<size_t> Members; };
		std::vector<SnesCanvas> snesCanvases;
		bool hasCanvasDestinations = std::any_of(frame.Pixels.begin(), frame.Pixels.end(), [](const auto& pixel) { return pixel.ExpandedSpriteClasses != 0 || pixel.ExpandedSpriteClassesWithoutBackground != 0; });
		for(const auto& group : *groups) {
			if(group.Members.empty()) { continue; }
			if(!MetaMemoryCondition::Matches(group.MemoryConditions,frame.MemorySnapshot)
				&& (!frame.SnesFieldMemoryCaptured || (!MetaMemoryCondition::Matches(group.MemoryConditions,frame.SnesFieldMemory[0])
					&& !MetaMemoryCondition::Matches(group.MemoryConditions,frame.SnesFieldMemory[1])))) { continue; }
			const auto& anchor = group.Members[0];
			for(size_t i = 0; i < candidates.size(); i++) {
				if(consumed[i] || !MatchesGraphic(candidates[i], anchor)) { continue; }
				uint8_t field=candidates[i].SnesField;
				const auto& snapshot=frame.SnesFieldMemoryCaptured && field>=1 && field<=2 ? frame.SnesFieldMemory[field-1] : frame.MemorySnapshot;
				if(!MetaMemoryCondition::Matches(group.MemoryConditions,snapshot)) { continue; }
				auto artwork=group.SelectArtwork(snapshot);
				if(artwork.ArtworkScale<1 || artwork.ArtworkWidth<1 || artwork.ArtworkHeight<1
					|| artwork.Artwork.size()!=(size_t)artwork.ArtworkWidth*artwork.ArtworkHeight) { continue; }
				int left = candidates[i].X - anchor.X, top = candidates[i].Y - anchor.Y;
				matched.clear();
				matched.push_back(i);
				for(size_t m = 1; m < group.Members.size(); m++) {
					const auto& member = group.Members[m];
					for(size_t j = 0; j < candidates.size(); j++) {
						const auto& candidate = candidates[j];
						if(candidate.X == left + member.X && candidate.Y == top + member.Y && candidate.Width == member.Width && candidate.Height == member.Height
							&& candidate.SnesField == candidates[i].SnesField && candidate.SnesRowStep == candidates[i].SnesRowStep
							&& candidate.SnesRowPhase == candidates[i].SnesRowPhase && !consumed[j] && MatchesGraphic(candidate, member)
							&& std::find(matched.begin(), matched.end(), j) == matched.end()) { matched.push_back(j); break; }
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
				// The frontmost hardware member supplies the group's canvas
				// priority. Staged effects retain their system-specific adapters.
				auto front = *std::min_element(matched.begin(), matched.end(), [&](size_t a, size_t b) { return candidates[a].GetExpansionOrder() < candidates[b].GetExpansionOrder(); });
				if(frame.SnesColorEffects && candidates[front].SnesPriorityCode <= 3) {
					snesCanvases.push_back({artwork, artworkLeft, artworkTop, matched});
					continue;
				}
				if(frame.GbaColorEffects && candidates[front].GbaPriority <= 3) {
					front = *std::min_element(matched.begin(), matched.end(), [&](size_t a, size_t b) {
						return candidates[a].GbaPriority != candidates[b].GbaPriority ? candidates[a].GbaPriority < candidates[b].GbaPriority : candidates[a].HardwareSprite < candidates[b].HardwareSprite;
					});
					gbaCanvases.push_back({artwork, artworkLeft, artworkTop, candidates[front].HardwareSprite, matched});
					continue;
				}
				bool fullCanvas = !stagedEffects && hasCanvasDestinations && candidates[front].ExpansionClass;
				if(fullCanvas) {
					expansions.push_back({artwork, artworkLeft, artworkTop, candidates[front].GetExpansionOrder(), candidates[front].ExpansionClass, matched});
				}
				if(applyOrderedSpriteScene) { continue; }
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
				uint32_t xBegin = (uint32_t)std::clamp<int64_t>(boundLeft * frame.SpriteCoordinateScale - cropLeft, 0, width);
				uint32_t xEnd = (uint32_t)std::clamp<int64_t>(boundRight * frame.SpriteCoordinateScale - cropLeft, 0, width);
				uint32_t yBegin = (uint32_t)std::clamp<int64_t>(boundTop * frame.SpriteCoordinateScale - cropTop, 0, height);
				uint32_t yEnd = (uint32_t)std::clamp<int64_t>(boundBottom * frame.SpriteCoordinateScale - cropTop, 0, height);
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
								auto sampleCoordinate = [&](int64_t position, int origin, uint32_t subPixel) {
									int64_t numerator = ((position - (int64_t)origin * frame.SpriteCoordinateScale) * _outputScale + subPixel) * artwork.ArtworkScale;
									int64_t denominator = _outputScale * frame.SpriteCoordinateScale;
									return numerator >= 0 ? numerator / denominator : -((-numerator + denominator - 1) / denominator);
								};
								int64_t ax = sampleCoordinate((int64_t)x + cropLeft, artworkLeft, sx);
								int64_t ay = sampleCoordinate((int64_t)y + cropTop, artworkTop, sy);
								if(ax >= 0 && ay >= 0 && ax < artwork.ArtworkWidth && ay < artwork.ArtworkHeight) { color = artwork.Artwork[ay * artwork.ArtworkWidth + ax]; }
								// Remove captured source pixels first. All replacement artwork
								// is then composed together in hardware order below.
								if(fullCanvas) { color = 0; }
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
							if(layer.HideOriginalLayer && layer.Layer >= 0 && layer.Layer < 4 && pixel.GbaBackgroundSamples[layer.Layer].Valid) { changed = true; break; }
							if(layer.Layer < 4 && layer.Layer >= 0 && _activeBackgrounds.Sample(layer, pixel.GbaBackgroundSamples[layer.Layer], x / _outputScale + cropLeft, y / _outputScale + cropTop, x % _outputScale, y % _outputScale, _outputScale, color, layer.CompositionPosition == 2) && (color >> 24)) { changed = true; break; }
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
									bool mainObject = !mainBackground, subObject = !subBackground;
									bool mainMath = state.SnesMathEnabled;
									// Native BG occlusion does not discard the fetched OBJ. Reselect
									// it when hiding/reprioritizing artwork, retaining OBJ windows.
									if(pixel.SnesObjectSprite >= 0 && pixel.SnesObjectPriorityCode <= 3) {
										if(mainBackground && pixel.VisibleSprite < 0 && pixel.SnesObjectsMain) {
											mainObject = true; main = Rgb555(pixel.SnesObjectColor); mainPriority = pixel.SnesObjectPriorities[pixel.SnesObjectPriorityCode];
											mainMath = pixel.SnesObjectMath && pixel.SnesObjectPalette > 3;
										}
										if(subBackground && pixel.SecondarySprite < 0 && pixel.SnesObjectsSub) {
											subObject = true; secondary = Rgb555(pixel.SnesObjectColor); subPriority = pixel.SnesObjectPriorities[pixel.SnesObjectPriorityCode];
										}
									}
									auto resolvedMain = ComposeSnesBackgroundScreen(snesBackgroundRow[x / _outputScale], _activeBackgrounds, true, px, py, x % _outputScale, y % _outputScale,
										_outputScale, mainObject, main, mainPriority, mainMath);
									auto resolvedSub = ComposeSnesBackgroundScreen(snesBackgroundRow[x / _outputScale], _activeBackgrounds, false, px, py, x % _outputScale, y % _outputScale,
										_outputScale, subObject, secondary, subPriority, false);
									main = resolvedMain.Color; secondary = resolvedSub.Color;
									state.SnesMathEnabled = resolvedMain.Math && pixel.SnesWindowMathAllowed;
									state.SnesUseSubscreen = pixel.SnesAddSubscreen && resolvedSub.Priority > 0;
									state.SnesHalf = pixel.SnesHalfRequested && (!pixel.SnesAddSubscreen || state.SnesUseSubscreen);
								} else for(const auto& layer : _activeBackgrounds) {
									uint32_t artwork;
										if(mainBackground && _activeBackgrounds.Sample(layer, pixel.BackgroundSample, x / _outputScale + cropLeft, y / _outputScale + cropTop,
										x % _outputScale, y % _outputScale, _outputScale, artwork)) { main = CompositeAlpha(artwork, main, artwork >> 24); }
										if(subBackground && _activeBackgrounds.Sample(layer, pixel.SnesSubBackgroundSample, x / _outputScale + cropLeft, y / _outputScale + cropTop,
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
		if(!gbaCanvases.empty()) {
			std::vector<size_t> canvasOrder;
			for(size_t index = 0; index < gbaCanvases.size(); index++) { canvasOrder.push_back(index); }
			std::stable_sort(canvasOrder.begin(), canvasOrder.end(), [&](size_t a, size_t b) { return gbaCanvases[a].Order < gbaCanvases[b].Order; });
			std::vector<GbaArtworkLayer> objects;
			for(uint32_t y = 0; y < height * _outputScale; y++) { for(uint32_t x = 0; x < outputWidth; x++) {
				int px = x / _outputScale + cropLeft, py = y / _outputScale + cropTop;
				const auto& pixel = framePixels[py * frame.Width + px];
				bool replacedNative = pixel.GbaObjectSprite >= 0 && std::any_of(candidates.begin(), candidates.end(), [&](const auto& candidate) {
					return candidate.HardwareSprite == pixel.GbaObjectSprite && candidate.GbaPriority <= 3 && consumed[&candidate - candidates.data()];
				});
				bool changed = replacedNative;
				objects.clear();
				if(pixel.GbaObjectsEnabled) {
					for(size_t index : canvasOrder) {
						const auto& canvas = gbaCanvases[index]; const auto& image = canvas.Image;
						int64_t ax = ((int64_t)(px - canvas.Left) * _outputScale + x % _outputScale) * image.ArtworkScale / _outputScale;
						int64_t ay = ((int64_t)(py - canvas.Top) * _outputScale + y % _outputScale) * image.ArtworkScale / _outputScale;
						if(px < canvas.Left || py < canvas.Top || ax < 0 || ay < 0 || ax >= image.ArtworkWidth || ay >= image.ArtworkHeight) { continue; }
						// Preserve member priority without consulting source alpha. Expanded margins
						// inherit the nearest member rectangle; overlaps/ties use native OBJ order.
						auto distance = [&](size_t member) {
							const auto& candidate = candidates[member];
							int64_t dx = std::max({(int64_t)candidate.X - px, (int64_t)px - candidate.X - candidate.Width + 1, int64_t(0)});
							int64_t dy = std::max({(int64_t)candidate.Y - py, (int64_t)py - candidate.Y - candidate.Height + 1, int64_t(0)});
							return dx * dx + dy * dy;
						};
						size_t member = *std::min_element(canvas.Members.begin(), canvas.Members.end(), [&](size_t a, size_t b) {
							int64_t da = distance(a), db = distance(b);
							if(da != db) { return da < db; }
							return candidates[a].GbaPriority != candidates[b].GbaPriority ? candidates[a].GbaPriority < candidates[b].GbaPriority : candidates[a].HardwareSprite < candidates[b].HardwareSprite;
						});
						const auto& hardware = candidates[member];
						objects.push_back({image.Artwork[ay * image.ArtworkWidth + ax], hardware.GbaPriority, hardware.GbaSemiTransparent, hardware.HardwareSprite}); changed = true;
					}
				}
				if(!changed) { continue; }
				if(pixel.GbaNativeObjects.begin() != pixel.GbaNativeObjects.end()) {
					for(const auto& native : pixel.GbaNativeObjects) {
						bool replaced = std::any_of(candidates.begin(), candidates.end(), [&](const auto& candidate) {
							return candidate.HardwareSprite == native.Sprite && candidate.GbaPriority <= 3 && consumed[&candidate - candidates.data()];
						});
						if(!replaced) { objects.push_back({Rgb555(native.Color), native.Priority, native.SemiTransparent, native.Sprite, true}); }
					}
				} else if(pixel.GbaObjectSprite >= 0 && !replacedNative) { objects.push_back({Rgb555(pixel.GbaObjectColor), pixel.GbaObjectPriority, pixel.GbaObjectSemiTransparent, pixel.GbaObjectSprite, true}); }
				std::stable_sort(objects.begin(), objects.end(), [](const auto& a, const auto& b) { return a.Order < b.Order; });
				outputPixels[(size_t)y * outputWidth + x] = ComposeGbaLayers(pixel, _activeBackgrounds, px, py, x % _outputScale, y % _outputScale, _outputScale, false, 0, 0, false, false, &objects);
			} }
		}
		if(!snesCanvases.empty()) {
			struct ObjectSample { uint32_t Color; uint16_t Order; uint8_t Priority; bool Math; };
			struct Outcome { SnesResolvedScreen Screen; double Weight; };
			std::vector<ObjectSample> objects;
			std::vector<Outcome> mainOutcomes, subOutcomes;
			for(uint32_t y = 0; y < height * _outputScale; y++) { for(uint32_t x = 0; x < outputWidth; x++) {
				int px = x / _outputScale + cropLeft, py = y / _outputScale + cropTop;
				const auto& pixel = framePixels[py * frame.Width + px];
				if(!pixel.SnesFallbackCaptured) { continue; }
				bool replacedNative = pixel.SnesObjectSprite >= 0 && std::any_of(candidates.begin(), candidates.end(), [&](const auto& candidate) {
					return candidate.HardwareSprite == pixel.SnesObjectSprite && candidate.SnesPriorityCode <= 3
						&& (candidate.SnesRowStep == 1 || pixel.SnesObjectSourceY >= 0) && consumed[&candidate - candidates.data()];
				});
				objects.clear(); bool changed = replacedNative;
				auto objectOrder = [&](int id) { return (uint16_t)(128 - ((id - pixel.SnesFirstObject + 128) & 127)); };
				for(const auto& canvas : snesCanvases) {
					uint8_t field = candidates[canvas.Members.front()].SnesField;
					if(field && field != pixel.SnesField) { continue; }
					bool sourceRows = candidates[canvas.Members.front()].SnesRowStep == 2;
					if(sourceRows && pixel.SnesObjectSourceY < 0) { continue; }
					const auto& image = canvas.Image;
					int64_t relativeX = (int64_t)px * _outputScale + x % _outputScale - (int64_t)canvas.Left * frame.SpriteCoordinateScale * _outputScale;
					int64_t relativeY = ((int64_t)py + pixel.SnesOutputSampleOffsetY) * _outputScale + y % _outputScale - ((int64_t)canvas.Top * frame.SpriteCoordinateScale + frame.SnesOutputOffsetY) * _outputScale;
					if(sourceRows) { relativeY = ((int64_t)pixel.SnesObjectSourceY - canvas.Top) * frame.SpriteCoordinateScale * _outputScale + (int64_t)(y % _outputScale) * 2; }
					if(relativeX < 0 || relativeY < 0) { continue; }
					int64_t ax = relativeX * image.ArtworkScale / (_outputScale * frame.SpriteCoordinateScale);
					int64_t ay = relativeY * image.ArtworkScale / (_outputScale * frame.SpriteCoordinateScale);
					if(ax >= image.ArtworkWidth || ay >= image.ArtworkHeight) { continue; }
					// Choose attributes by logical member rectangles, independently of
					// artwork/source alpha and physical hires/bob sampling phases.
					auto logical = [&](int64_t value) { return value / frame.SpriteCoordinateScale - (value % frame.SpriteCoordinateScale < 0 ? 1 : 0); };
					int64_t logicalX = logical(px), logicalY = logical((int64_t)py + pixel.SnesOutputSampleOffsetY - frame.SnesOutputOffsetY);
					if(sourceRows) { logicalY = pixel.SnesObjectSourceY; }
					auto distance = [&](size_t index) {
						const auto& member = candidates[index];
						int64_t dx = std::max({(int64_t)member.X - logicalX, logicalX - member.X - member.Width + 1, int64_t(0)});
						int64_t dy = std::max({(int64_t)member.Y - logicalY, logicalY - member.Y - member.Height + 1, int64_t(0)});
						return dx * dx + dy * dy;
					};
					size_t front = *std::min_element(canvas.Members.begin(), canvas.Members.end(), [&](size_t a, size_t b) {
						int64_t da = distance(a), db = distance(b);
						return da != db ? da < db : objectOrder(candidates[a].HardwareSprite) > objectOrder(candidates[b].HardwareSprite);
					});
					const auto& member = candidates[front];
					if(member.SnesPriorityCode > 3) { continue; }
					objects.push_back({image.Artwork[ay * image.ArtworkWidth + ax], objectOrder(member.HardwareSprite), pixel.SnesObjectPriorities[member.SnesPriorityCode], pixel.SnesObjectMath && member.SnesPalette > 3});
					changed = true;
				}
				if(!changed) { continue; }
				bool sampledWinner = false;
				if(pixel.SnesNativeObjects.begin() != pixel.SnesNativeObjects.end()) {
					for(const auto& native : pixel.SnesNativeObjects) {
						if(native.Sprite < 0 || native.PriorityCode > 3 || native.Palette > 7) { continue; }
						sampledWinner |= native.Sprite == pixel.SnesObjectSprite;
						bool replaced = std::any_of(candidates.begin(), candidates.end(), [&](const auto& candidate) {
							return candidate.HardwareSprite == native.Sprite && candidate.SnesPriorityCode <= 3
								&& (candidate.SnesRowStep == 1 || pixel.SnesObjectSourceY >= 0) && consumed[&candidate - candidates.data()];
						});
						if(!replaced) { objects.push_back({Rgb555(native.Color), objectOrder(native.Sprite), pixel.SnesObjectPriorities[native.PriorityCode], pixel.SnesObjectMath && native.Palette > 3}); }
					}
				}
				if(!sampledWinner && pixel.SnesObjectSprite >= 0 && !replacedNative && pixel.SnesObjectPriorityCode <= 3) {
					objects.push_back({Rgb555(pixel.SnesObjectColor), objectOrder(pixel.SnesObjectSprite), pixel.SnesObjectPriorities[pixel.SnesObjectPriorityCode], pixel.SnesObjectMath && pixel.SnesObjectPalette > 3});
				}
				std::stable_sort(objects.begin(), objects.end(), [](const auto& a, const auto& b) { return a.Order > b.Order; });
				const auto* capturedScreens = frame.SnesBackgrounds.size() == frame.Pixels.size() ? &frame.SnesBackgrounds[py * frame.Width + px] : nullptr;
				SnesPreparedScreen prepared;
				if(capturedScreens && capturedScreens->Captured) { prepared = PrepareSnesBackgroundScreen(*capturedScreens); }
				auto resolve = [&](bool mainScreen, const ObjectSample* object) {
					if(capturedScreens && capturedScreens->Captured) {
						return ComposeSnesBackgroundScreen(prepared, _activeBackgrounds, mainScreen, px, py, x % _outputScale, y % _outputScale, _outputScale,
							object != nullptr, object ? object->Color | 0xFF000000 : 0, object ? object->Priority : 0, object && object->Math);
					}
					SnesResolvedScreen fallback {Rgb555(mainScreen ? pixel.SnesFallbackMainColor : pixel.SnesFallbackSubColor),
						(uint8_t)(mainScreen ? pixel.SnesFallbackMainFlags & 15 : pixel.SnesFallbackSubPriority), mainScreen && (pixel.SnesFallbackMainFlags & 0x80)};
					return object && object->Priority > fallback.Priority ? SnesResolvedScreen {object->Color | 0xFF000000, object->Priority, object->Math} : fallback;
				};
				auto outcomes = [&](bool mainScreen, std::vector<Outcome>& result) {
					result.clear(); double remainder = 1.0;
					if(mainScreen ? pixel.SnesObjectsMain : pixel.SnesObjectsSub) {
						for(const auto& object : objects) {
							uint32_t alpha = object.Color >> 24;
							if(alpha) { result.push_back({resolve(mainScreen, &object), remainder * alpha / 255.0}); }
							remainder *= (255 - alpha) / 255.0;
							if(remainder == 0) { break; }
						}
					}
					if(remainder > 0) { result.push_back({resolve(mainScreen, nullptr), remainder}); }
				};
				outcomes(true, mainOutcomes); outcomes(false, subOutcomes);
				double channels[3] = {};
				for(const auto& main : mainOutcomes) { for(const auto& sub : subOutcomes) {
					SnesColorState state(pixel);
					state.SnesMathEnabled = main.Screen.Math && pixel.SnesWindowMathAllowed;
					state.SnesUseSubscreen = pixel.SnesAddSubscreen && sub.Screen.Priority > 0;
					state.SnesHalf = pixel.SnesHalfRequested && (!pixel.SnesAddSubscreen || state.SnesUseSubscreen);
					uint32_t color = ApplySnesColorEffect(main.Screen.Color, sub.Screen.Color, state);
					for(int c = 0; c < 3; c++) { channels[c] += ((color >> (c * 8)) & 255) * main.Weight * sub.Weight; }
				} }
				uint32_t result = 0xFF000000;
				for(int c = 0; c < 3; c++) { result |= (uint32_t)std::clamp((int)(channels[c] + 0.5), 0, 255) << (c * 8); }
				outputPixels[(size_t)y * outputWidth + x] = result;
			} }
		}
		// Source alpha must not select the winner between replacement canvases.
		// Only unreplaced hardware sprites retain their captured occlusion.
		std::vector<int> replacedOrders;
		if(!applyOrderedSpriteScene && (!expansions.empty() || (applyBackgrounds && _activeBackgrounds.HiddenNativeLayers))) {
			for(size_t index = 0; index < candidates.size(); index++) {
				const auto& candidate = candidates[index];
				if(consumed[index] || !candidate.ExpansionClass) { continue; }
				expansions.push_back({{0, 0, 1, candidate.Width, candidate.Height, candidate.Pixels}, candidate.X, candidate.Y,
					candidate.GetExpansionOrder(), candidate.ExpansionClass, {index}, true});
			}
		}
		for(const auto& expansion : expansions) {
			if(expansion.Native) { continue; }
			for(size_t member : expansion.Members) { replacedOrders.push_back(candidates[member].GetExpansionOrder()); }
		}
		std::sort(replacedOrders.begin(), replacedOrders.end());
		// Draw complete replacement canvases back-to-front using PNG alpha.
		std::vector<size_t> expansionOrder;
		for(size_t index = 0; index < expansions.size(); index++) { expansionOrder.push_back(index); }
		std::stable_sort(expansionOrder.begin(), expansionOrder.end(), [&](size_t a, size_t b) { return expansions[a].Order > expansions[b].Order; });
		for(size_t index : expansionOrder) {
			if(applyOrderedSpriteScene) { continue; }
			const auto& expansion = expansions[index];
			const auto& image = expansion.Image;
			int64_t physicalLeft = (int64_t)expansion.Left * frame.SpriteCoordinateScale;
			int64_t physicalTop = (int64_t)expansion.Top * frame.SpriteCoordinateScale;
			int64_t physicalRight = physicalLeft + ((int64_t)image.ArtworkWidth * frame.SpriteCoordinateScale + image.ArtworkScale - 1) / image.ArtworkScale;
			int64_t physicalBottom = physicalTop + ((int64_t)image.ArtworkHeight * frame.SpriteCoordinateScale + image.ArtworkScale - 1) / image.ArtworkScale;
			int xBegin = (int)std::clamp<int64_t>(physicalLeft - cropLeft, 0, width);
			int xEnd = (int)std::clamp<int64_t>(physicalRight - cropLeft, 0, width);
			int yBegin = (int)std::clamp<int64_t>(physicalTop - cropTop, 0, height);
			int yEnd = (int)std::clamp<int64_t>(physicalBottom - cropTop, 0, height);
			for(int y = yBegin; y < yEnd; y++) { for(int x = xBegin; x < xEnd; x++) {
				int px = x + cropLeft, py = y + cropTop;
				const auto& pixel = framePixels[py * frame.Width + px];
				bool hiddenBackground = pixel.BackgroundSample.Valid && _activeBackgrounds.HidesNativeLayer(pixel.BackgroundSample.Layer);
				if(expansion.Native && !hiddenBackground && !std::binary_search(replacedOrders.begin(), replacedOrders.end(), pixel.ExpandedSpriteOccluder)) { continue; }
				uint8_t eligibleClasses = hiddenBackground ? pixel.ExpandedSpriteClassesWithoutBackground : pixel.ExpandedSpriteClasses;
				if(hiddenBackground && frame.BackgroundUnderlays.size() == frame.Pixels.size()) {
					const auto& lower = frame.BackgroundUnderlays[py * frame.Width + px];
					if(lower.SpriteEligibilityCaptured && lower.Sample.Valid && !_activeBackgrounds.HidesNativeLayer(lower.Sample.Layer)) {
						eligibleClasses &= lower.SpriteEligibleClasses;
					}
				}
				if(!(eligibleClasses & expansion.Class)
					|| (pixel.ExpandedSpriteOccluder >= 0 && pixel.ExpandedSpriteOccluder < expansion.Order
						&& !std::binary_search(replacedOrders.begin(), replacedOrders.end(), pixel.ExpandedSpriteOccluder))) { continue; }
				for(uint32_t sy = 0; sy < _outputScale; sy++) { for(uint32_t sx = 0; sx < _outputScale; sx++) {
					int64_t ax = (((int64_t)px - physicalLeft) * _outputScale + sx) * image.ArtworkScale / (_outputScale * frame.SpriteCoordinateScale);
					int64_t ay = (((int64_t)py - physicalTop) * _outputScale + sy) * image.ArtworkScale / (_outputScale * frame.SpriteCoordinateScale);
					if(ax < 0 || ay < 0 || ax >= image.ArtworkWidth || ay >= image.ArtworkHeight) { continue; }
					uint32_t color = image.Artwork[ay * image.ArtworkWidth + ax];
					if(hiddenBackground && applyBackgrounds && frame.BackgroundUnderlays.size() == frame.Pixels.size()) {
						const auto& lower = frame.BackgroundUnderlays[py * frame.Width + px];
						if(lower.SpriteEligibilityCaptured && (lower.OverlayOccludesClasses & expansion.Class)) {
							for(const auto& layer : _activeBackgrounds) {
								uint32_t artwork;
								if(!_activeBackgrounds.Sample(layer, lower.Sample, px, py, sx, sy, _outputScale, artwork)) { continue; }
								uint32_t alpha = color >> 24;
								color = (CompositeAlpha(artwork, color | 0xFF000000, artwork >> 24) & 0xFFFFFF) | (alpha << 24);
							}
						}
					}
					if((pixel.BackgroundOverlayOccludesClasses & expansion.Class) && applyBackgrounds) {
						// The destination already contains the background PNG over its native
						// layer. Resolve that same PNG over the opaque sprite outcome before
						// applying sprite alpha, keeping partial backgrounds above the sprite.
						for(const auto& layer : _activeBackgrounds) {
							uint32_t artwork;
							if(!_activeBackgrounds.Sample(layer, pixel.BackgroundSample, px, py, sx, sy, _outputScale, artwork)) { continue; }
							uint32_t alpha = color >> 24;
							color = (CompositeAlpha(artwork, color | 0xFF000000, artwork >> 24) & 0xFFFFFF) | (alpha << 24);
							// Compose every matching PNG in the same order as the background pass.
						}
					}
					size_t offset = (size_t)(y * _outputScale + sy) * outputWidth + x * _outputScale + sx;
					outputPixels[offset] = CompositeAlpha(color, outputPixels[offset], color >> 24);
				} }
			} }
		}
		if(applyOrderedSpriteScene) {
			// Resolve hardware-order sprite alpha outcomes before inserting BG bands.
			// An opaque behind-BG sprite still blocks a later foreground sprite.
			std::vector<std::vector<const MetaSpriteRow*>> rows(frame.Height);
			for(const auto& row : gameboyScene ? frame.Rows : frame.NesRows) { if(row.Scanline < frame.Height && row.Sprite < (gameboyScene ? 40 : 64)) { rows[row.Scanline].push_back(&row); } }
			std::vector<std::vector<const MetaSpriteDecodedRow*>> decodedRows(decodedScene ? frame.Height : 0);
			if(decodedScene) { for(const auto& row : frame.DecodedRows) {
				int64_t y = (int64_t)row.Y + row.Row;
				if(row.Sprite >= 0 && row.Sprite < (wsScene ? 128 : 64) && row.Width > 0 && row.Width <= 128 && row.Height > 0 && row.Height <= 128 && row.Row >= 0 && row.Row < row.Height && y >= 0 && y < frame.Height && row.OffsetX >= 0 && row.OffsetX < row.Width && row.Pixels.size() <= (size_t)(row.Width - row.OffsetX)) { decodedRows[(size_t)y].push_back(&row); }
			} }
			bool fullFrame = std::any_of(_activeBackgrounds.Layers.begin(), _activeBackgrounds.Layers.end(), [](const auto* layer) { return layer->CompositionPosition == 0 || layer->CompositionPosition >= 4; });
			struct Draw { int Order; const MetaSpriteRow* Native; const ExpandedArtwork* Replacement; uint8_t Class = 0; const MetaSpriteDecodedRow* Decoded = nullptr; };
			std::vector<Draw> draws; draws.reserve(64 + expansions.size());
			for(uint32_t y = 0; y < height; y++) {
				int py = (int)y + cropTop; draws.clear();
				int firstX = fullFrame ? 0 : (int)width, lastX = fullFrame ? (int)width : 0;
				auto include = [&](int64_t left, int64_t right) { firstX = std::min(firstX, (int)std::clamp<int64_t>(left - cropLeft, 0, width)); lastX = std::max(lastX, (int)std::clamp<int64_t>(right - cropLeft, 0, width)); };
				for(const auto* row : rows[py]) {
					include(row->X, row->X + 8); // Source rectangles still need clearing for a shifted/smaller PNG.
					int order = gameboyScene && row->ExpansionOrder >= 0 ? row->ExpansionOrder : row->Sprite;
					if(!std::binary_search(replacedOrders.begin(), replacedOrders.end(), order)) { draws.push_back({order, row, nullptr}); }
				}
				if(decodedScene) { for(const auto* row : decodedRows[py]) {
					include((int64_t)row->X + row->OffsetX, (int64_t)row->X + row->OffsetX + row->Pixels.size());
					if(!std::binary_search(replacedOrders.begin(), replacedOrders.end(), row->Sprite)) { draws.push_back({row->Sprite, nullptr, nullptr, row->ExpansionClass, row}); }
				} }
				for(const auto& image : expansions) {
					if(!image.Native && py >= image.Top && (int64_t)(py - image.Top) * image.Image.ArtworkScale < image.Image.ArtworkHeight) {
						draws.push_back({image.Order, nullptr, &image}); include(image.Left, (int64_t)image.Left + (image.Image.ArtworkWidth + image.Image.ArtworkScale - 1) / image.Image.ArtworkScale);
					}
				}
				std::stable_sort(draws.begin(), draws.end(), [](const auto& a, const auto& b) { return a.Order < b.Order; });
				const auto* drawData = draws.data();
				for(int x = firstX; x < lastX; x++) {
					int px = (int)x + cropLeft; const auto& pixel = framePixels[(size_t)py * frame.Width + px];
					if(!pixel.NativeBackdropCaptured) { continue; }
					bool memberOrder = false;
					for(auto& draw : draws) {
						if(!draw.Replacement) { continue; }
						const auto& image = *draw.Replacement;
						draw.Class = image.Class;
						if(image.Members.size() < 2) { continue; }
						// Member rectangles determine attributes, never original texel alpha.
						// Margins inherit the nearest rectangle; ties use captured OBJ order.
						auto distance = [&](size_t member) {
							const auto& candidate = candidates[member];
							int64_t dx = std::max({(int64_t)candidate.X - px, (int64_t)px - candidate.X - candidate.Width + 1, int64_t(0)});
							int64_t dy = std::max({(int64_t)candidate.Y - py, (int64_t)py - candidate.Y - candidate.Height + 1, int64_t(0)});
							return dx * dx + dy * dy;
						};
						size_t member = *std::min_element(image.Members.begin(), image.Members.end(), [&](size_t a, size_t b) {
							int64_t da = distance(a), db = distance(b);
							return da != db ? da < db : candidates[a].GetExpansionOrder() < candidates[b].GetExpansionOrder();
						});
						draw.Order = candidates[member].GetExpansionOrder(); draw.Class = candidates[member].ExpansionClass;
						memberOrder = true;
					}
					if(memberOrder) { std::stable_sort(draws.begin(), draws.end(), [](const auto& a, const auto& b) { return a.Order < b.Order; }); }
					for(uint32_t sy = 0; sy < _outputScale; sy++) { for(uint32_t sx = 0; sx < _outputScale; sx++) {
						auto overlay = [&](int position, uint32_t color) {
							for(const auto& layer : _activeBackgrounds) {
								uint32_t artwork;
								if(layer.CompositionPosition != position) { continue; }
								bool sampled = _activeBackgrounds.Sample(layer, pixel.BackgroundSample, px, py, sx, sy, _outputScale, artwork, true);
								if(!sampled && (gameboyScene || wsScene) && frame.BackgroundUnderlays.size() == frame.Pixels.size()) { sampled = _activeBackgrounds.Sample(layer, frame.BackgroundUnderlays[(size_t)py * frame.Width + px].Sample, px, py, sx, sy, _outputScale, artwork, true); }
								if(sampled) { color = CompositeAlpha(artwork, color, artwork >> 24); }
							} return color;
						};
						auto aboveBehind = [&](uint32_t color) {
							color = overlay(5, color);
							if(pixel.BackgroundColorIndex && !_activeBackgrounds.HidesNativeLayer(0)) { color = backgroundColor(pixel.BackgroundColor); }
							return overlay(6, overlay(0, color));
						};
						uint32_t empty = aboveBehind(overlay(4, backgroundColor(pixel.NativeBackdropColor)));
						auto priorityBackgroundOutcome = [&](uint32_t spriteColor, uint8_t spriteClass) {
							bool hidden = pixel.BackgroundSample.Valid && _activeBackgrounds.HidesNativeLayer(pixel.BackgroundSample.Layer);
							const auto* lower = hidden && frame.BackgroundUnderlays.size() == frame.Pixels.size() ? &frame.BackgroundUnderlays[(size_t)py * frame.Width + px] : nullptr;
							bool lowerVisible = lower && lower->Sample.Valid && !_activeBackgrounds.HidesNativeLayer(lower->Sample.Layer);
							uint8_t eligible = hidden ? (lowerVisible && lower->SpriteEligibilityCaptured ? lower->SpriteEligibleClasses : 3) : pixel.ExpandedSpriteClasses;
							bool spriteVisible = spriteClass && (eligible & spriteClass);
							uint32_t nativeColor = backgroundColor(hidden ? lowerVisible ? lower->Color : pixel.NativeBackdropColor : pixel.BackgroundColor);
							bool opaque = hidden ? lowerVisible && lower->ColorIndex != 0 : pixel.BackgroundSample.Valid && pixel.BackgroundColorIndex != 0;
							uint8_t mask = hidden ? lowerVisible ? lower->OverlayOccludesClasses : 0 : pixel.BackgroundOverlayOccludesClasses;
							bool behind = spriteVisible && spriteClass == 2 && mask != 0;
							uint32_t result = overlay(4, opaque ? backgroundColor(pixel.NativeBackdropColor) : nativeColor);
							if(behind) { result = spriteColor; }
							result = overlay(5, result);
							if(opaque) { result = nativeColor; }
							auto addPng = [&](const MetaBackgroundSample& sample, uint8_t occludes, bool front) {
								if(front && !(occludes & spriteClass)) { return; }
								for(const auto& layer : _activeBackgrounds) { uint32_t artwork; if(_activeBackgrounds.Sample(layer, sample, px, py, sx, sy, _outputScale, artwork)) {
									// A priority BG PNG can hide an otherwise front OBJ. Resolve
									// its alpha outcome under band 6, preserving uncovered OBJ.
									uint32_t color = front ? overlay(6, artwork | 0xFF000000) : artwork;
									result = CompositeAlpha(color, result, artwork >> 24);
								} }
							};
							if(lower && lower->Sample.Valid) { addPng(lower->Sample, lower->OverlayOccludesClasses, false); }
							addPng(pixel.BackgroundSample, pixel.BackgroundOverlayOccludesClasses, false);
							result = overlay(6, result);
							if(spriteVisible && !behind) {
								result = spriteColor;
								if(lower && lower->Sample.Valid) { addPng(lower->Sample, lower->OverlayOccludesClasses, true); }
								addPng(pixel.BackgroundSample, pixel.BackgroundOverlayOccludesClasses, true);
							}
							return result;
						};
						if(gameboyScene || smsScene) { empty = priorityBackgroundOutcome(0, 0); }
						auto wsOutcome = [&](uint32_t spriteColor, uint8_t spriteClass) {
							const auto& pair = frame.BackgroundUnderlays[(size_t)py * frame.Width + px];
							uint32_t result = backgroundColor(pixel.NativeBackdropColor);
							auto nativePng = [&](const MetaBackgroundSample& sample) {
								for(const auto& layer : _activeBackgrounds) { uint32_t artwork; if(_activeBackgrounds.Sample(layer, sample, px, py, sx, sy, _outputScale, artwork)) { result = CompositeAlpha(artwork, result, artwork >> 24); } }
							};
							if(pair.Sample.Valid && !_activeBackgrounds.HidesNativeLayer(pair.Sample.Layer)) { result = backgroundColor(pair.Color); }
							nativePng(pair.Sample);
							result = overlay(4, result);
							if(spriteClass & 10) { result = spriteColor; }
							result = overlay(5, result);
							if(pair.ForegroundOpaque && !_activeBackgrounds.HidesNativeLayer(pair.ForegroundSample.Layer)) { result = backgroundColor(pair.ForegroundColor); }
							nativePng(pair.ForegroundSample);
							result = overlay(6, result);
							if(spriteClass & 5) { result = spriteColor; }
							return result;
						};
						if(wsScene) { empty = wsOutcome(0, 0); }
						double channels[3] = {}, remainder = 1.0;
						for(size_t d = 0; d < draws.size() && remainder > 0; d++) {
							const auto& draw = drawData[d]; uint32_t color = 0; uint8_t spriteClass;
							if(draw.Native) {
								const auto& row = *draw.Native; int offset = px - row.X;
								if(offset < 0 || offset >= 8) { continue; }
								int bit = gameboyScene && (row.Attributes & 0x20) ? offset : 7-offset;
								int index = ((row.LowByte >> bit) & 1) | (((row.HighByte >> bit) & 1) << 1);
								if(!index) { continue; }
								uint16_t nativeColor = gameboyScene && (row.OutputColorMask & (1 << offset)) ? row.OutputColors[offset]
									: !gameboyScene && pixel.ExpandedSpriteOccluder == row.Sprite ? pixel.SpriteColor : row.Palette[index];
								color = backgroundColor(nativeColor) | 0xFF000000;
								spriteClass = row.Attributes & (gameboyScene ? 0x80 : 0x20) ? 2 : 1;
							} else if(draw.Decoded) {
								const auto& row = *draw.Decoded;
								int64_t offset = (int64_t)px - row.X - row.OffsetX;
								if(offset < 0 || offset >= (int64_t)row.Pixels.size()) { continue; }
								color = row.Pixels[(size_t)offset];
								if(!(color >> 24)) { continue; }
								if(pixel.VisibleSprite == row.Sprite) { color = backgroundColor(pixel.SpriteColor) | 0xFF000000; }
								spriteClass = row.ExpansionClass;
							} else {
								const auto& image = *draw.Replacement;
								int64_t ax = ((int64_t)(px - image.Left) * _outputScale + sx) * image.Image.ArtworkScale / _outputScale;
								int64_t ay = ((int64_t)(py - image.Top) * _outputScale + sy) * image.Image.ArtworkScale / _outputScale;
								if(px < image.Left || ax < 0 || ay < 0 || ax >= image.Image.ArtworkWidth || ay >= image.Image.ArtworkHeight) { continue; }
								color = image.Image.Artwork[(size_t)ay * image.Image.ArtworkWidth + ax]; spriteClass = draw.Class;
							}
							if(!(pixel.ExpandedSpriteClassesWithoutBackground & spriteClass) || !(color >> 24)) { continue; }
							double weight = remainder * (color >> 24) / 255.0;
							uint32_t outcome = wsScene ? wsOutcome(color | 0xFF000000, spriteClass) : gameboyScene || smsScene ? priorityBackgroundOutcome(color | 0xFF000000, spriteClass) : spriteClass == 2 ? aboveBehind(color | 0xFF000000) : color | 0xFF000000;
							for(int c = 0; c < 3; c++) { channels[c] += ((outcome >> (c*8)) & 255) * weight; }
							remainder *= (255 - (color >> 24)) / 255.0;
						}
						uint32_t result = 0xFF000000;
						for(int c = 0; c < 3; c++) { result |= (uint32_t)std::clamp((int)(channels[c] + ((empty >> (c*8)) & 255) * remainder + 0.5), 0, 255) << (c*8); }
						outputPixels[((size_t)y * _outputScale + sy) * outputWidth + x * _outputScale + sx] = result;
					} }
				}
			}
		}
		if(applyForegroundBackgrounds) {
			// A display overlay is intentionally outside native priority/color math.
			// Samples retain the selected layer's coordinates and visibility mask.
			for(const auto& layer : _activeBackgrounds) {
				if(layer.CompositionPosition != 1) { continue; }
				for(uint32_t y = 0; y < height; y++) { for(uint32_t x = 0; x < width; x++) {
					int px = (int)x + cropLeft, py = (int)y + cropTop;
					size_t index = (size_t)py * frame.Width + px;
					const auto& pixel = framePixels[index];
					const MetaBackgroundSample* source = nullptr;
					auto consider = [&](const MetaBackgroundSample& sample) { if(!source && sample.Valid && sample.Layer == layer.Layer) { source = &sample; } };
					consider(pixel.BackgroundSample);
					for(const auto& sample : pixel.GbaBackgroundSamples) { consider(sample); }
					if(frame.SnesBackgrounds.size() == frame.Pixels.size()) { for(const auto& bg : frame.SnesBackgrounds[index].Layers) { consider(bg.Sample); } }
					consider(pixel.SnesSubBackgroundSample);
					if(frame.BackgroundUnderlays.size() == frame.Pixels.size()) { consider(frame.BackgroundUnderlays[index].Sample); consider(frame.BackgroundUnderlays[index].ForegroundSample); }
					if(!source) { continue; }
					for(uint32_t sy = 0; sy < _outputScale; sy++) { for(uint32_t sx = 0; sx < _outputScale; sx++) {
						uint32_t artwork;
						if(!_activeBackgrounds.Sample(layer, *source, px, py, sx, sy, _outputScale, artwork, true)) { continue; }
						size_t offset = ((size_t)y * _outputScale + sy) * outputWidth + x * _outputScale + sx;
						outputPixels[offset] = CompositeAlpha(artwork, outputPixels[offset], artwork >> 24);
					} }
				} }
			}
		}
		return _output.data();
	}
};

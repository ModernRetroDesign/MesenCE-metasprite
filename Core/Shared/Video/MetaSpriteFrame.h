#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <utility>
#include <memory>
#include <array>
#include "MetaMemoryCondition.h"
#include "MetaPngAnimation.h"

// Most pixels have no overlapping OBJ coverage. Avoid constructing two checked
// STL vector iterator proxies for every pixel in Debug builds. Storage is only
// allocated when coverage is actually recorded; iteration uses bounded pointers.
template<typename T> class MetaSpriteCoverageList
{
	T* _items = nullptr;
	size_t _size = 0, _capacity = 0;
public:
	MetaSpriteCoverageList() = default;
	~MetaSpriteCoverageList() { delete[] _items; }
	MetaSpriteCoverageList(const MetaSpriteCoverageList& other)
	{
		if(other._size) {
			_items = new T[other._size];
			_size = _capacity = other._size;
			std::copy_n(other._items, _size, _items);
		}
	}
	MetaSpriteCoverageList(MetaSpriteCoverageList&& other) noexcept
		: _items(std::exchange(other._items, nullptr)), _size(std::exchange(other._size, 0)), _capacity(std::exchange(other._capacity, 0)) {}
	MetaSpriteCoverageList& operator=(MetaSpriteCoverageList other) noexcept
	{
		swap(other);
		return *this;
	}
	void swap(MetaSpriteCoverageList& other) noexcept
	{
		std::swap(_items, other._items); std::swap(_size, other._size); std::swap(_capacity, other._capacity);
	}
	void clear() { _size = 0; }
	void push_back(T value)
	{
		if(_size == _capacity) {
			size_t capacity = _capacity ? _capacity * 2 : 4;
			T* items = new T[capacity];
			if(_size) { std::copy_n(_items, _size, items); }
			delete[] _items; _items = items; _capacity = capacity;
		}
		_items[_size++] = value;
	}
	T& operator[](size_t index) { return _items[index]; }
	const T& operator[](size_t index) const { return _items[index]; }
	T* begin() { return _items; }
	T* end() { return _size ? _items + _size : _items; }
	const T* begin() const { return _items; }
	const T* end() const { return _size ? _items + _size : _items; }
};

// Captured at pixel composition time, before source identity is lost in the framebuffer.
// Hardware IDs are transient references within a frame, never pack matching keys.
struct MetaBackgroundSample
{
	int32_t X = 0, Y = 0;
	uint8_t Layer = 0;
	bool Valid = false;
	// Number of captured output pixels per native background texel and the
	// current pixel's phase within it. Other adapters use the default 1:1 grid.
	uint8_t PixelScaleX = 1, PixelScaleY = 1, PixelOffsetX = 0, PixelOffsetY = 0;
	int32_t ScrollX = 0, ScrollY = 0;
	int32_t ScrollWrapX = 0, ScrollWrapY = 0;
	bool NesRasterScroll = false;
};

struct MetaGbaSpriteCoverage
{
	int16_t Sprite = -1;
	uint16_t SecondaryColor = 0;
	bool EffectsEnabled = false, SemiTransparent = false, SecondaryBlendTarget = false;
	uint8_t Priority = 0;
};

struct MetaGbaNativeObject
{
	int16_t Sprite = -1;
	uint16_t Color = 0;
	uint8_t Priority = 0;
	bool SemiTransparent = false;
};

struct MetaSnesNativeObject
{
	int16_t Sprite = -1;
	uint16_t Color = 0;
	uint8_t PriorityCode = 0xFF, Palette = 0;
};

struct MetaSnesBackgroundLayer
{
	MetaBackgroundSample Sample;
	MetaBackgroundSample SubSample;
	uint16_t Color = 0;
	uint16_t SubColor = 0;
	uint8_t Priority = 0;
	bool Opaque = false, Main = false, Sub = false, Math = false;
	bool SeparateSub = false, SubOpaque = false;
};

struct MetaSnesBackgroundPixel
{
	MetaSnesBackgroundLayer Layers[4];
	uint16_t BackdropColor = 0;
	uint8_t MainPriority = 0, SubPriority = 0;
	bool Captured = false, BackdropMath = false;
};

struct MetaSpritePixelState
{
	struct GbaBackgroundLayer {
		uint16_t Color = 0;
		uint8_t Priority = 0;
		bool Opaque = false;
	};
	MetaBackgroundSample BackgroundSample;
	MetaBackgroundSample SnesSubBackgroundSample;
	MetaBackgroundSample GbaBackgroundSamples[4];
	GbaBackgroundLayer GbaBackgroundLayers[4];
	uint8_t GbaBlendMainMask = 0, GbaBlendSubMask = 0;
	bool GbaWindowEffectsEnabled = false;
	uint16_t GbaBackdropColor = 0, GbaObjectColor = 0;
	int16_t GbaObjectSprite = -1;
	uint8_t GbaObjectPriority = 0xFF;
	bool GbaObjectsEnabled = false;
	bool GbaObjectSemiTransparent = false;
	int16_t VisibleSprite = -1;
	// Destination eligibility for artwork outside fetched sprite rectangles.
	// Class bits are supplied by each adapter, not inferred from source alpha.
	uint8_t ExpandedSpriteClasses = 0;
	// Same window/enable/clipping rules before native background occlusion.
	uint8_t ExpandedSpriteClassesWithoutBackground = 0;
	uint8_t NativeVisibleSpriteClass = 0;
	// Sprite classes below artwork on the captured background layer. Unlike
	// eligibility, this also applies when the original background texel is clear.
	uint8_t BackgroundOverlayOccludesClasses = 0;
	// Lower values win. DMG includes hardware X before OAM index; NES/SMS
	// and GBC use OAM order. This is frame-local rendering metadata.
	int16_t ExpandedSpriteOccluder = -1;
	// Adapter-approved sprite coverage, independent of source texel alpha.
	// Bits are transient hardware IDs (0..127), never persistent matching keys.
	uint64_t ReplacementEligibleSprites = 0;
	uint64_t ReplacementEligibleSpritesHigh = 0;
	void AllowReplacementSprite(int id)
	{
		if(id >= 0 && id < 64) { ReplacementEligibleSprites |= uint64_t(1) << id; }
		else if(id >= 64 && id < 128) { ReplacementEligibleSpritesHigh |= uint64_t(1) << (id - 64); }
	}
	bool AllowsReplacementSprite(int id) const
	{
		if(id >= 0 && id < 64) { return (ReplacementEligibleSprites & (uint64_t(1) << id)) != 0; }
		if(id >= 64 && id < 128) { return (ReplacementEligibleSpritesHigh & (uint64_t(1) << (id - 64))) != 0; }
		return false;
	}
	uint16_t BackgroundColor = 0;
	uint16_t NativeBackdropColor = 0;
	bool NativeBackdropCaptured = false;
	uint16_t SpriteColor = 0;
	uint8_t BackgroundColorIndex = 0;
	uint8_t BackgroundAttributes = 0;
	uint8_t SpriteColorIndex = 0;
	uint8_t SpriteAttributes = 0;
	bool Window = false;
	int16_t SecondarySprite = -1;
	uint16_t SecondaryColor = 0;
	uint8_t MainLayer = 0, SecondaryLayer = 0;
	uint8_t Effect = 0, MainCoefficient = 16, SecondaryCoefficient = 0, Brightness = 0;
	bool EffectsEnabled = false, SemiTransparent = false;
	bool SecondaryBlendTarget = false;
	uint16_t GbaFallbackColor = 0;
	uint16_t GbaFallbackMainColor = 0;
	bool GbaFallbackBlendTarget = false;
	bool GbaFallbackCaptured = false;
	bool SnesMathEnabled = false, SnesSubtract = false, SnesHalf = false;
	bool SnesHiresBackgrounds = false;
	bool SnesClip = false, SnesUseSubscreen = false;
	uint16_t SnesFixedColor = 0;
	uint16_t SnesFallbackMainColor = 0, SnesFallbackSubColor = 0;
	uint8_t SnesFallbackMainFlags = 0, SnesFallbackSubPriority = 0;
	// BG0..BG3 identities before OBJ composition; 0xFF denotes backdrop.
	uint8_t SnesFallbackMainLayer = 0xFF, SnesFallbackSubLayer = 0xFF;
	bool SnesFallbackCaptured = false;
	bool SnesWindowMathAllowed = true, SnesAddSubscreen = false, SnesHalfRequested = false;
	bool SnesObjectsMain = false, SnesObjectsSub = false, SnesObjectMath = false;
	uint8_t SnesObjectPriorities[4] = {};
	int16_t SnesObjectSprite = -1;
	uint16_t SnesObjectColor = 0, SnesObjectOrder = 0;
	uint8_t SnesObjectPriorityCode = 0xFF, SnesObjectPalette = 0;
	uint8_t SnesFirstObject = 0;
	uint8_t SnesField = 0; // 0: ordinary frame; 1/2: even/odd woven field.
	uint8_t SnesOutputSampleOffsetY = 0;
	// Absolute full-source screen row for OBJ-interlace; copied with the
	// originating field sample through hires/bob/weave. -1 means unavailable.
	int16_t SnesObjectSourceY = -1;
};

struct MetaSpritePixel : MetaSpritePixelState
{
	MetaSpriteCoverageList<MetaGbaSpriteCoverage> GbaCoverage;
	MetaSpriteCoverageList<MetaGbaNativeObject> GbaNativeObjects;
	struct SnesCoverageSample {
		int16_t Sprite = -1;
		uint16_t Order = 0;
		uint8_t Priority = 0, Palette = 0;
		bool Main = false, Sub = false, Math = false;
		uint8_t ColorIndex = 0;
	};
	MetaSpriteCoverageList<SnesCoverageSample> SnesCoverage;
	MetaSpriteCoverageList<MetaSnesNativeObject> SnesNativeObjects;
};

struct MetaSnesSubOutputPhase : MetaSpritePixelState
{
	// Color math reads main-screen flags and subscreen priority at this
	// logical X, independently of where the primary output pixel is located.
	int16_t MathControlX = -1;
	bool Captured = false;
};

struct MetaSpriteRow
{
	uint16_t Sprite = 0;
	int16_t X = 0;
	int16_t Y = 0;
	uint16_t Scanline = 0;
	uint16_t PatternAddress = 0;
	uint8_t LowByte = 0;
	uint8_t HighByte = 0;
	uint8_t Attributes = 0;
	uint8_t Height = 8;
	uint16_t Palette[4] = {};
	// GB native reveal uses output-time palette colors, independently of the
	// fetch-time graphic used for persistent content recognition.
	uint16_t OutputColors[8] = {};
	uint8_t OutputColorMask = 0;
	// Game Boy's evaluated scanline slot, captured while the sprite is fetched.
	uint8_t ScanlineSpriteIndex = 0xFF;
	int ExpansionOrder = -1;
};

// Decoded fetched row for systems with more than two bitplanes.
struct MetaSpriteDecodedRow
{
	int Sprite = -1, X = 0, Y = 0, Row = 0, Width = 0, Height = 0;
	std::vector<uint32_t> Pixels;
	int OffsetX = 0; // A fetched slice within a wider hardware sprite.
	uint8_t ExpansionClass = 0;
	uint8_t GbaPriority = 0xFF;
	bool GbaSemiTransparent = false;
	uint8_t SnesPriorityCode = 0xFF, SnesPalette = 0;
	uint16_t SnesFetchOrder = 0;
	uint8_t SnesField = 0;
	// Explicit source-row sampling for OBJ-interlace. Unfetched rows are
	// unknown, not transparent; ordinary captures must remain complete.
	uint8_t SnesRowStep = 1, SnesRowPhase = 0;
};

struct MetaBackgroundUnderlay
{
	MetaBackgroundSample Sample;
	uint16_t Color = 0;
	uint8_t ColorIndex = 0;
	MetaBackgroundSample ForegroundSample;
	uint16_t ForegroundColor = 0;
	bool ForegroundOpaque = false;
	bool SpriteEligibilityCaptured = false;
	uint8_t SpriteEligibleClasses = 0;
	uint8_t OverlayOccludesClasses = 0;
};

struct MetaSpriteFrame
{
	uint32_t Width;
	uint32_t Height;
	uint32_t FrameNumber = 0;
	bool GbaColorEffects = false;
	bool GameboyCapture = false;
	bool PceCapture = false;
	bool SmsCapture = false;
	bool WsCapture = false;
	bool SnesColorEffects = false;
	// Captured output pixels may be denser than the decoded hardware-sprite
	// coordinates used for recognition and persistent group origins.
	uint8_t SpriteCoordinateScale = 1;
	uint8_t SnesOutputOffsetY = 0;
	std::vector<MetaSpritePixel> Pixels;
	// Keep SNES layer capture separate so other systems pay no per-pixel cost.
	std::vector<MetaSnesBackgroundPixel> SnesBackgrounds;
	// Allocated by adapters that retain a native background below the winner.
	std::vector<MetaBackgroundUnderlay> BackgroundUnderlays;
	// Hires even-column output is the current subscreen pixel, with math
	// controlled by the previous main pixel. Keep that phase independent of
	// the current main pixel's state. Allocated only on double-width scanlines.
	std::vector<MetaSnesSubOutputPhase> SnesSubOutput;
	std::vector<MetaSpriteRow> Rows;
	std::vector<MetaSpriteDecodedRow> DecodedRows;
	// NES palettes contain PPU color indices/emphasis, rather than RGB555 values.
	std::vector<MetaSpriteRow> NesRows;
	// Filled on the emulation thread at publication, after prior decode completes.
	mutable MetaMemorySnapshot MemorySnapshot;
	mutable std::shared_ptr<const MetaPngAnimationSnapshot> AnimationTiming;
	std::shared_ptr<const MetaPngAnimationSnapshot> SnesFieldAnimationTiming[2];
	MetaMemorySnapshot SnesFieldMemory[2];
	bool SnesFieldMemoryCaptured = false;
	uint32_t SnesFieldFrameNumber[2] = {};

	MetaSpriteFrame(uint32_t width, uint32_t height) : Width(width), Height(height), Pixels(width * height) {}
	void Reset()
	{
		FrameNumber = 0; GbaColorEffects = SnesColorEffects = GameboyCapture = PceCapture = SmsCapture = WsCapture = false; SpriteCoordinateScale = 1; SnesOutputOffsetY = 0;
		const MetaSpritePixelState blank;
		MetaSpritePixel* pixels = Pixels.data();
		for(size_t i = 0, count = Pixels.size(); i < count; i++) {
			static_cast<MetaSpritePixelState&>(pixels[i]) = blank;
			pixels[i].GbaCoverage.clear(); pixels[i].GbaNativeObjects.clear(); pixels[i].SnesCoverage.clear(); pixels[i].SnesNativeObjects.clear();
		}
		SnesBackgrounds.clear(); BackgroundUnderlays.clear(); SnesSubOutput.clear(); Rows.clear(); DecodedRows.clear(); NesRows.clear(); MemorySnapshot.clear();
		SnesFieldMemory[0].clear(); SnesFieldMemory[1].clear(); SnesFieldMemoryCaptured = false;
		SnesFieldFrameNumber[0] = SnesFieldFrameNumber[1] = 0;
		AnimationTiming.reset(); SnesFieldAnimationTiming[0].reset(); SnesFieldAnimationTiming[1].reset();
	}
};

// Owned by each PPU and accessed only on the emulation thread. A published
// frame remains immutable until all decoder/renderer references are released.
class MetaSpriteFramePool
{
	std::array<std::shared_ptr<MetaSpriteFrame>, 3> _frames;
public:
	std::shared_ptr<MetaSpriteFrame> Acquire(uint32_t width, uint32_t height)
	{
		for(auto& frame : _frames) {
			if(frame && frame.use_count() == 1 && frame->Width == width && frame->Height == height) {
				frame->Reset(); return frame;
			}
		}
		auto frame = std::make_shared<MetaSpriteFrame>(width, height);
		for(auto& slot : _frames) {
			if(!slot || slot.use_count() == 1) { slot = frame; break; }
		}
		return frame;
	}
};

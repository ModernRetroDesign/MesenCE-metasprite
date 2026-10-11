#pragma once
#include <cstdint>
#include <vector>
#include "MetaMemoryCondition.h"

// Captured at pixel composition time, before source identity is lost in the framebuffer.
// Hardware IDs are transient references within a frame, never pack matching keys.
struct MetaBackgroundSample
{
	int32_t X = 0, Y = 0;
	uint8_t Layer = 0;
	bool Valid = false;
};

struct MetaGbaSpriteCoverage
{
	int16_t Sprite = -1;
	uint16_t SecondaryColor = 0;
	bool EffectsEnabled = false, SemiTransparent = false, SecondaryBlendTarget = false;
	uint8_t Priority = 0;
};

struct MetaSpritePixel
{
	struct GbaBackgroundLayer {
		uint16_t Color = 0;
		uint8_t Priority = 0;
		bool Opaque = false;
	};
	MetaBackgroundSample BackgroundSample;
	MetaBackgroundSample GbaBackgroundSamples[4];
	GbaBackgroundLayer GbaBackgroundLayers[4];
	uint8_t GbaBlendMainMask = 0, GbaBlendSubMask = 0;
	bool GbaWindowEffectsEnabled = false;
	uint16_t GbaBackdropColor = 0, GbaObjectColor = 0;
	int16_t GbaObjectSprite = -1;
	uint8_t GbaObjectPriority = 0xFF;
	bool GbaObjectSemiTransparent = false;
	int16_t VisibleSprite = -1;
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
	std::vector<MetaGbaSpriteCoverage> GbaCoverage;
	struct SnesCoverageSample {
		int16_t Sprite = -1;
		uint16_t Order = 0;
		uint8_t Priority = 0, Palette = 0;
		bool Main = false, Sub = false, Math = false;
	};
	std::vector<SnesCoverageSample> SnesCoverage;
	bool SnesMathEnabled = false, SnesSubtract = false, SnesHalf = false;
	bool SnesClip = false, SnesUseSubscreen = false;
	uint16_t SnesFixedColor = 0;
	uint16_t SnesFallbackMainColor = 0, SnesFallbackSubColor = 0;
	uint8_t SnesFallbackMainFlags = 0, SnesFallbackSubPriority = 0;
	bool SnesFallbackCaptured = false;
	bool SnesWindowMathAllowed = true, SnesAddSubscreen = false, SnesHalfRequested = false;
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
	// Game Boy's evaluated scanline slot, captured while the sprite is fetched.
	uint8_t ScanlineSpriteIndex = 0xFF;
};

// Decoded fetched row for systems with more than two bitplanes.
struct MetaSpriteDecodedRow
{
	int Sprite = -1, X = 0, Y = 0, Row = 0, Width = 0, Height = 0;
	std::vector<uint32_t> Pixels;
	int OffsetX = 0; // A fetched slice within a wider hardware sprite.
};

struct MetaSpriteFrame
{
	uint32_t Width;
	uint32_t Height;
	uint32_t FrameNumber = 0;
	bool GbaColorEffects = false;
	bool SnesColorEffects = false;
	std::vector<MetaSpritePixel> Pixels;
	std::vector<MetaSpriteRow> Rows;
	std::vector<MetaSpriteDecodedRow> DecodedRows;
	// NES palettes contain PPU color indices/emphasis, rather than RGB555 values.
	std::vector<MetaSpriteRow> NesRows;
	// Filled on the emulation thread at publication, after prior decode completes.
	mutable MetaMemorySnapshot MemorySnapshot;

	MetaSpriteFrame(uint32_t width, uint32_t height) : Width(width), Height(height), Pixels(width * height) {}
};

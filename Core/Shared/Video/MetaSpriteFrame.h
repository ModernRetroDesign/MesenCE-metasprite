#pragma once
#include <cstdint>
#include <vector>

// Captured at pixel composition time, before source identity is lost in the framebuffer.
// Hardware IDs are transient references within a frame, never pack matching keys.
struct MetaSpritePixel
{
	int16_t VisibleSprite = -1;
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

	MetaSpriteFrame(uint32_t width, uint32_t height) : Width(width), Height(height), Pixels(width * height) {}
};

#pragma once

#include "pch.h"
#include "Shared/Video/BaseVideoFilter.h"
#include "Shared/SettingTypes.h"

class SnesDefaultVideoFilter : public BaseVideoFilter
{
private:
	uint32_t _calculatedPalette[0x8000] = {};
	VideoConfig _videoConfig = {};

	SnesHighResBlendMode _highResBlendMode = SnesHighResBlendMode::None;
	SnesColorCorrectionMode _colorCorrection = SnesColorCorrectionMode::None;
	bool _forceFixedRes = false;

	void InitLookupTable();

	__forceinline static uint32_t BlendPixels(uint32_t a, uint32_t b);
	__forceinline uint32_t GetPixel(uint16_t* ppuFrame, uint32_t offset);

	void ApplyBlend(FrameInfo frameInfo, uint32_t* out);

protected:
	void OnBeforeApplyFilter() override;
	bool DeferFrameBlend() const override { return _frame.MetaSprites && SupportsMetaSprites(); }
	bool AllowRewindFrameBlend() const override { return DeferFrameBlend(); }
	FrameInfo GetFrameInfo() override;

public:
	SnesDefaultVideoFilter(Emulator* emu);
	~SnesDefaultVideoFilter();

	void ApplyFilter(uint16_t* ppuOutputBuffer) override;
	OverscanDimensions GetOverscan() override;
	uint32_t GetNativeColor(uint16_t color) const { return _calculatedPalette[color&0x7FFF]; }
	bool SupportsMetaSprites() const {
		return !_forceFixedRes && _colorCorrection == SnesColorCorrectionMode::None
			&& _videoConfig.Hue == 0 && _videoConfig.Saturation == 0 && _videoConfig.Brightness == 0 && _videoConfig.Contrast == 0;
	}
};

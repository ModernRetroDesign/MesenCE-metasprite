#pragma once
#include "pch.h"
#include "Shared/Video/BaseVideoFilter.h"
#include "Shared/Video/GenericNtscFilter.h"
#include "Shared/SettingTypes.h"

class Emulator;

class GbDefaultVideoFilter : public BaseVideoFilter
{
private:
	uint32_t _calculatedPalette[0x8000] = {};
	VideoConfig _videoConfig = {};

	bool _gbcAdjustColors = false;

	bool _applyNtscFilter = false;
	GenericNtscFilter _ntscFilter;

	void InitLookupTable();

	__forceinline uint32_t GetPixel(uint16_t* ppuFrame, uint32_t offset);

protected:
	void OnBeforeApplyFilter() override;
	FrameInfo GetFrameInfo() override;

public:
	uint32_t GetNativeColor(uint16_t color) const { return _calculatedPalette[color & 0x7FFF]; }
	GbDefaultVideoFilter(Emulator* emu, bool applyNtscFilter);

	void ApplyFilter(uint16_t* ppuOutputBuffer) override;
};

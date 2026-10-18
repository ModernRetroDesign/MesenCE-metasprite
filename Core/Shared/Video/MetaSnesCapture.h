#pragma once
#include "MetaSpriteFrame.h"

namespace MetaSnesCapture
{
	inline MetaSnesSubOutputPhase CaptureSubOutputPhase(const MetaSpritePixelState& current,
		const MetaSpritePixelState& control,int x,uint16_t subColor,uint16_t previousMainColor,bool captured)
	{
		MetaSnesSubOutputPhase phase;
		static_cast<MetaSpritePixelState&>(phase)=current;
		phase.MathControlX=(int16_t)(x>0 ? x-1 : 0); phase.SecondarySampleX=(int16_t)(x>0 ? x-1 : -1); phase.Captured=captured;
		phase.VisibleSprite=current.SecondarySprite; phase.SecondarySprite=x>0 ? control.VisibleSprite : -1;
		phase.SpriteColor=subColor; phase.SecondaryColor=x>0 ? previousMainColor : 0;
		phase.SnesFallbackMainColor=current.SnesFallbackSubColor;
		phase.SnesFallbackSubColor=x>0 ? control.SnesFallbackMainColor : 0;
		phase.SnesFallbackMainFlags=current.SnesFallbackSubPriority | (control.SnesFallbackMainFlags & 0x80);
		phase.SnesFallbackSubPriority=x>0 ? control.SnesFallbackMainFlags & 15 : 0;
		phase.SnesFallbackMainLayer=current.SnesFallbackSubLayer; phase.SnesFallbackSubLayer=x>0 ? control.SnesFallbackMainLayer : 0xFF;
		phase.BackgroundSample=current.SnesSubBackgroundSample;
		phase.SnesSubBackgroundSample=x>0 ? control.BackgroundSample : MetaBackgroundSample{};
		return phase;
	}
	// Normal-width fields duplicate the completed main screen horizontally.
	// Their existing low-resolution metadata already contains both math operands;
	// unlike hires sub/main phases, no operand must be synthesized or discarded.
	inline bool CanExpandMainWithMath(const MetaSpriteFrame& source)
	{
		if(!source.SnesColorEffects || source.SpriteCoordinateScale!=1 || source.Pixels.size()!=(size_t)source.Width*source.Height
			|| (!source.SnesSubOutput.empty() && source.SnesSubOutput.size()!=source.Pixels.size())
			|| (!source.SnesBackgrounds.empty() && source.SnesBackgrounds.size()!=source.Pixels.size())) { return false; }
		for(const auto& phase : source.SnesSubOutput) { if(phase.Captured) { return false; } }
		for(const auto& pixel : source.Pixels) { if(pixel.SnesHiresBackgrounds) { return false; } }
		for(const auto& backgrounds : source.SnesBackgrounds) { for(const auto& layer : backgrounds.Layers) { if(layer.SeparateSub) { return false; } } }
		return true;
	}
	inline bool ExpandMainWithMath(const MetaSpriteFrame& source,MetaSpriteFrame& output)
	{
		if(!CanExpandMainWithMath(source) || output.Width!=source.Width*2 || output.Height!=source.Height*2 || output.Pixels.size()!=source.Pixels.size()*4) { return false; }
		output.Reset(); output.SnesColorEffects=true; output.SpriteCoordinateScale=2;
		output.FrameNumber=source.FrameNumber; output.DecodedRows=source.DecodedRows;
		output.MemorySnapshot=source.MemorySnapshot; output.AnimationTiming=source.AnimationTiming;
		if(!source.SnesBackgrounds.empty()) { output.SnesBackgrounds.resize(output.Pixels.size()); }
		for(uint32_t y=0;y<source.Height;y++) { for(uint32_t x=0;x<source.Width;x++) {
			size_t index=(size_t)y*source.Width+x;
			for(uint8_t dy=0;dy<2;dy++) { for(uint8_t dx=0;dx<2;dx++) {
				size_t destination=(size_t)(y*2+dy)*output.Width+x*2+dx;
				auto sample=[&](MetaBackgroundSample& value) { value.PixelScaleX=value.PixelScaleY=2; value.PixelOffsetX=dx; value.PixelOffsetY=dy; };
				auto& pixel=output.Pixels[destination]; pixel=source.Pixels[index];
				sample(pixel.BackgroundSample); sample(pixel.SnesSubBackgroundSample);
				if(!source.SnesBackgrounds.empty()) {
					auto& background=output.SnesBackgrounds[destination]; background=source.SnesBackgrounds[index];
					for(auto& layer : background.Layers) { sample(layer.Sample); sample(layer.SubSample); }
				}
			} }
		} }
		return true;
	}
	// Hires sub/main phases currently require math-free captures. Normal-width
	// fields can instead preserve both operands through ExpandMainWithMath.
	inline bool CanExpandWithoutMath(const MetaSpriteFrame& source)
	{
		if(!source.SnesColorEffects || source.SpriteCoordinateScale != 1
			|| source.Pixels.size() != (size_t)source.Width * source.Height
			|| (!source.SnesSubOutput.empty() && source.SnesSubOutput.size() != source.Pixels.size())) { return false; }
		for(const auto& pixel : source.Pixels) {
			if(pixel.SnesMathEnabled || (pixel.SnesWindowMathAllowed && (pixel.SnesFallbackMainFlags & 0x80))) { return false; }
			for(const auto& coverage : pixel.SnesCoverage) {
				if(pixel.SnesWindowMathAllowed && coverage.Math && (coverage.Main || coverage.Sub)) { return false; }
			}
			if(pixel.SnesWindowMathAllowed && pixel.SnesObjectMath && (pixel.SnesObjectsMain || pixel.SnesObjectsSub)) {
				for(const auto& object : pixel.SnesNativeObjects) { if(object.Palette > 3) { return false; } }
			}
		}
		for(const auto& phase : source.SnesSubOutput) { if(phase.Captured && phase.SnesMathEnabled) { return false; } }
		if(!source.SnesBackgrounds.empty() && source.SnesBackgrounds.size() != source.Pixels.size()) { return false; }
		for(size_t i = 0; i < source.SnesBackgrounds.size(); i++) {
			if(!source.Pixels[i].SnesWindowMathAllowed) { continue; }
			const auto& backgrounds = source.SnesBackgrounds[i];
			if(backgrounds.BackdropMath) { return false; }
			for(const auto& layer : backgrounds.Layers) { if(layer.Math && (layer.Main || layer.Sub)) { return false; } }
		}
		return true;
	}

	inline bool CanExpandHiresMath(const MetaSpriteFrame& source)
	{
		if(!source.SnesColorEffects || source.SpriteCoordinateScale!=1 || !source.Width || !source.Height
			|| source.Pixels.size()!=(size_t)source.Width*source.Height || source.SnesSubOutput.size()!=source.Pixels.size()
			|| (!source.SnesBackgrounds.empty() && source.SnesBackgrounds.size()!=source.Pixels.size())) { return false; }
		for(const auto& row : source.DecodedRows) { if(row.SnesField || row.SnesRowStep!=1) { return false; } }
		for(size_t i=0;i<source.SnesSubOutput.size();i++) {
			const auto& phase=source.SnesSubOutput[i]; int x=(int)(i%source.Width);
			if(phase.Captured && (phase.MathControlX!=(x ? x-1 : 0) || phase.SecondarySampleX!=(x ? x-1 : -1))) { return false; }
		}
		return true;
	}
	inline bool ExpandWithoutMath(const MetaSpriteFrame& source, MetaSpriteFrame& output,bool retainHiresMath=false)
	{
		if(output.Width != source.Width * 2 || output.Height != source.Height * 2
			|| output.Pixels.size() != source.Pixels.size() * 4 || !(retainHiresMath ? CanExpandHiresMath(source) : CanExpandWithoutMath(source))) { return false; }
		output.Reset(); output.SnesColorEffects = true; output.SpriteCoordinateScale = 2;
		output.FrameNumber = source.FrameNumber; output.DecodedRows = source.DecodedRows;
		output.MemorySnapshot = source.MemorySnapshot; output.AnimationTiming = source.AnimationTiming;
		if(!source.SnesBackgrounds.empty()) { output.SnesBackgrounds.resize(output.Pixels.size()); }
		for(uint32_t y = 0; y < source.Height; y++) {
			for(uint32_t x = 0; x < source.Width; x++) {
				size_t index = (size_t)y * source.Width + x;
				const auto& pixel = source.Pixels[index];
				const auto* subPhase = source.SnesSubOutput.empty() || !source.SnesSubOutput[index].Captured ? nullptr : &source.SnesSubOutput[index];
				for(uint8_t dy = 0; dy < 2; dy++) {
					for(uint8_t phase = 0; phase < 2; phase++) {
						auto& target = output.Pixels[(size_t)(y * 2 + dy) * output.Width + x * 2 + phase];
						bool sub = phase == 0 && subPhase;
						static_cast<MetaSpritePixelState&>(target) = sub ? static_cast<const MetaSpritePixelState&>(*subPhase) : pixel;
						// Color math is excluded above. The secondary plane cannot
						// affect this phase, including replacement alpha fallback.
						target.SecondarySprite = -1; target.SecondaryColor = 0;
						target.SnesObjectsMain = sub ? pixel.SnesObjectsSub : pixel.SnesObjectsMain;
						target.SnesObjectsSub = false; target.SnesObjectMath = false;
						target.SnesNativeObjects = pixel.SnesNativeObjects;
						target.SnesObjectSourceY = pixel.SnesObjectSourceY;
						target.SnesSubBackgroundSample = {};
						target.BackgroundSample.PixelScaleX = pixel.SnesHiresBackgrounds ? 1 : 2;
						target.BackgroundSample.PixelScaleY = 2;
						target.BackgroundSample.PixelOffsetX = pixel.SnesHiresBackgrounds ? 0 : phase;
						target.BackgroundSample.PixelOffsetY = dy;
						for(auto coverage : pixel.SnesCoverage) {
							coverage.Main = sub ? coverage.Sub : coverage.Main;
							coverage.Sub = false; coverage.Math = false;
							if(coverage.Main) { target.SnesCoverage.push_back(coverage); }
						}
						if(!output.SnesBackgrounds.empty()) {
							const auto& backgrounds = source.SnesBackgrounds[index];
							auto& expanded = output.SnesBackgrounds[(size_t)(y * 2 + dy) * output.Width + x * 2 + phase];
							expanded.Captured = backgrounds.Captured; expanded.BackdropColor = backgrounds.BackdropColor;
							expanded.MainPriority = sub ? backgrounds.SubPriority : backgrounds.MainPriority;
							for(int i = 0; i < 4; i++) {
								const auto& layer = backgrounds.Layers[i];
								auto& mapped = expanded.Layers[i]; mapped = layer;
								if(sub && layer.SeparateSub) { mapped.Sample = layer.SubSample; mapped.Color = layer.SubColor; mapped.Opaque = layer.SubOpaque; }
								mapped.Main = sub ? layer.Sub : layer.Main; mapped.Sub = false; mapped.Math = false;
								mapped.Sample.PixelScaleX = layer.SeparateSub ? 1 : 2; mapped.Sample.PixelScaleY = 2;
								mapped.Sample.PixelOffsetX = layer.SeparateSub ? 0 : phase; mapped.Sample.PixelOffsetY = dy;
								mapped.SeparateSub = false;
							}
						}
					}
				}
			}
		}
		return true;
	}

	// Prepare one woven field. The other field intentionally has no replacement
	// eligibility: runtime integration must combine it with its own capture and
	// recognition data, rather than repeat the current field's sprite pose.
	// Legacy helper names default to strict math-free mapping. Detailed math
	// callers retain the logical sources alongside the projected output.
	inline bool RequiresHiresMathSource(const MetaSpriteFrame& source) { return !CanExpandWithoutMath(source) && CanExpandHiresMath(source); }
	inline bool CanUseHiresMathSource(const MetaSpriteFrame& source)
	{
		if(CanExpandHiresMath(source)) { return true; }
		if(!CanExpandMainWithMath(source)) { return false; }
		for(const auto& row : source.DecodedRows) { if(row.SnesField || row.SnesRowStep!=1) { return false; } } return true;
	}
	inline bool ExpandSupportedMath(const MetaSpriteFrame& source,MetaSpriteFrame& output,bool mainMath,bool hiresMath)
	{
		if(hiresMath && RequiresHiresMathSource(source)) { return ExpandWithoutMath(source,output,true); }
		return mainMath && CanExpandMainWithMath(source) ? ExpandMainWithMath(source,output) : ExpandWithoutMath(source,output);
	}
	inline bool MapFieldWithoutMath(const MetaSpriteFrame& source, MetaSpriteFrame& output, bool oddField, bool allowMainMath=false,bool allowHiresMath=false)
	{
		if(!ExpandSupportedMath(source,output,allowMainMath,allowHiresMath)) { return false; }
		for(uint32_t y = 0; y < source.Height; y++) {
			uint32_t inactiveRow = y * 2 + (oddField ? 0 : 1);
			for(uint32_t x = 0; x < output.Width; x++) {
				size_t index = (size_t)inactiveRow * output.Width + x;
				output.Pixels[index] = {};
				if(!output.SnesBackgrounds.empty()) { output.SnesBackgrounds[index] = {}; }
			}
		}
		return true;
	}

	inline bool MapBobWithoutMath(const MetaSpriteFrame& source, MetaSpriteFrame& output, bool oddField, uint32_t firstPictureRow, bool allowMainMath=false,bool allowHiresMath=false)
	{
		if(firstPictureRow >= source.Height || !ExpandSupportedMath(source,output,allowMainMath,allowHiresMath)) { return false; }
		if(!oddField) { return true; }
		output.SnesOutputOffsetY = 1;
		for(uint32_t y = output.Height - 1; y > 0; y--) { for(uint32_t x = 0; x < output.Width; x++) {
			size_t index = (size_t)y * output.Width + x;
			output.Pixels[index] = output.Pixels[index - output.Width];
			if(!output.SnesBackgrounds.empty()) { output.SnesBackgrounds[index] = output.SnesBackgrounds[index - output.Width]; }
		} }
		for(uint32_t x = 0; x < output.Width; x++) {
			output.Pixels[x] = {};
			if(!output.SnesBackgrounds.empty()) { output.SnesBackgrounds[x] = {}; }
			size_t first = (size_t)firstPictureRow * 2 * output.Width + x;
			output.Pixels[first] = output.Pixels[first + output.Width];
			output.Pixels[first].SnesOutputSampleOffsetY = 1;
			if(!output.SnesBackgrounds.empty()) { output.SnesBackgrounds[first] = output.SnesBackgrounds[first + output.Width]; }
		}
		return true;
	}

	inline bool WeaveFieldsWithoutMath(const MetaSpriteFrame& even, const MetaSpriteFrame& odd, MetaSpriteFrame& output, bool allowMainMath=false,bool allowHiresMath=false)
	{
		auto canExpand=[&](const MetaSpriteFrame& field) { return (allowHiresMath && CanExpandHiresMath(field)) || (allowMainMath && CanExpandMainWithMath(field)) || CanExpandWithoutMath(field); };
		if(even.Width != odd.Width || even.Height != odd.Height || !canExpand(even) || !canExpand(odd)) { return false; }
		if(allowHiresMath && (RequiresHiresMathSource(even) || RequiresHiresMathSource(odd)) && (!CanUseHiresMathSource(even) || !CanUseHiresMathSource(odd))) { return false; }
		// Each input must contain untagged SNES OAM IDs. Reject an already
		// woven capture before changing output, rather than aliasing its fields.
		for(const auto* field : {&even, &odd}) {
			for(const auto& row : field->DecodedRows) { if(row.Sprite < 0 || row.Sprite >= 128 || row.SnesField) { return false; } }
			for(const auto& pixel : field->Pixels) {
				if(pixel.SnesField || pixel.VisibleSprite >= 128 || pixel.SecondarySprite >= 128 || pixel.SnesObjectSprite >= 128) { return false; }
				for(const auto& coverage : pixel.SnesCoverage) { if(coverage.Sprite >= 128) { return false; } }
				for(const auto& object : pixel.SnesNativeObjects) { if(object.Sprite >= 128) { return false; } }
			}
		}
		MetaSpriteFrame oddOutput(output.Width, output.Height);
		auto expand=[&](const MetaSpriteFrame& field,MetaSpriteFrame& target) { return ExpandSupportedMath(field,target,allowMainMath,allowHiresMath); };
		if(!expand(even, output) || !expand(odd, oddOutput)) { return false; }
		if(!oddOutput.SnesBackgrounds.empty() && output.SnesBackgrounds.empty()) { output.SnesBackgrounds.resize(output.Pixels.size()); }
		for(uint32_t y = 0; y < output.Height; y++) { for(uint32_t x = 0; x < output.Width; x++) {
			size_t index = (size_t)y * output.Width + x;
			if(y & 1) {
				output.Pixels[index] = oddOutput.Pixels[index];
				if(!output.SnesBackgrounds.empty()) { output.SnesBackgrounds[index] = oddOutput.SnesBackgrounds.empty() ? MetaSnesBackgroundPixel{} : oddOutput.SnesBackgrounds[index]; }
			}
			auto& pixel = output.Pixels[index]; pixel.SnesField = (y & 1) ? 2 : 1;
			if(y & 1) {
				auto tag = [](int16_t& id) { if(id >= 0) { id += 128; } };
				tag(pixel.VisibleSprite); tag(pixel.SecondarySprite); tag(pixel.SnesObjectSprite);
				for(auto& coverage : pixel.SnesCoverage) { tag(coverage.Sprite); }
				for(auto& object : pixel.SnesNativeObjects) { tag(object.Sprite); }
			}
		} }
		for(auto& row : output.DecodedRows) { row.SnesField = 1; }
		for(auto row : odd.DecodedRows) { row.Sprite += 128; row.SnesField = 2; output.DecodedRows.push_back(std::move(row)); }
		const auto& newest = even.FrameNumber > odd.FrameNumber ? even : odd;
		output.FrameNumber = newest.FrameNumber; output.MemorySnapshot = newest.MemorySnapshot;
		output.AnimationTiming = newest.AnimationTiming;
		output.SnesFieldAnimationTiming[0] = even.AnimationTiming; output.SnesFieldAnimationTiming[1] = odd.AnimationTiming;
		output.SnesFieldMemory[0] = even.MemorySnapshot; output.SnesFieldMemory[1] = odd.MemorySnapshot;
		output.SnesFieldMemoryCaptured = true;
		output.SnesFieldFrameNumber[0] = even.FrameNumber; output.SnesFieldFrameNumber[1] = odd.FrameNumber;
		return true;
	}
}

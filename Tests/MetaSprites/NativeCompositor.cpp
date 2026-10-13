#include "../../Core/Shared/Video/MetaSpriteCompositor.h"
#include "../../Core/Shared/Video/MetaSpritePacket.h"
#include "../../Core/Shared/Video/MetaBackgroundReplacement.h"
#include "../../Core/Shared/Video/MetaBackgroundPacket.h"
#include "../../Core/Debugger/ViewerRefreshSchedule.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <iterator>
#include <chrono>

#ifdef NDEBUG
#error Native compositor verification requires assertions in every configuration.
#endif

static void BenchmarkComposition(bool largeGroups = false, int effects = 0)
{
	MetaSpriteFrame frame(256, 239);
	frame.GbaColorEffects = effects == 1;
	frame.SnesColorEffects = effects >= 2;
	std::vector<uint32_t> original(256 * 239, 0xFF0000FF);
	std::vector<MetaSpriteReplacement> groups;
	for(int id = 0; id < (largeGroups ? 128 : 32); id++) {
		int relativeX = largeGroups ? id % 4 * 16 : 0, relativeY = largeGroups ? id % 16 / 4 * 16 : 0;
		int left = largeGroups ? id / 16 % 4 * 64 + relativeX : (id % 8) * 24 + 8;
		int top = largeGroups ? id / 64 * 64 + relativeY : (id / 8) * 24 + 8;
		uint32_t sourceColor = 0xFF000000 | ((id + 1) << 16);
		MetaSpriteGraphic member {relativeX, relativeY, 16, 16, -1, {}};
		for(int y = 0; y < 16; y++) {
			std::vector<uint32_t> row(16, sourceColor); row[15] = 0;
			member.Pixels.insert(member.Pixels.end(), row.begin(), row.end());
			frame.DecodedRows.push_back({id, left, top, y, 16, 16, row});
			for(int x = 0; x < 16; x++) {
				auto& pixel = frame.Pixels[(top + y) * 256 + left + x];
				pixel.BackgroundColor = 0x7C00; pixel.AllowReplacementSprite(id);
				if(effects) {
					pixel.SpriteColor = 31; pixel.SecondaryColor = 0x7C00;
					pixel.Brightness = 15; pixel.Effect = 1;
					pixel.MainCoefficient = pixel.SecondaryCoefficient = 8;
					pixel.EffectsEnabled = true; pixel.SecondaryBlendTarget = true;
					pixel.GbaFallbackCaptured = effects == 1; pixel.GbaFallbackColor = 0x7C00;
					pixel.SnesFallbackCaptured = effects >= 2;
					pixel.SnesFixedColor = 0x7C00;
					pixel.SnesMathEnabled = true;
					if(x == 15 && effects == 1) { pixel.GbaCoverage.push_back({static_cast<int16_t>(id), 0x7C00, true, false, true}); }
					if(x == 15 && effects >= 2) { pixel.SnesCoverage.push_back({static_cast<int16_t>(id), 1, 3, 4, true, false, true}); }
				}
				if(x < 15) { pixel.VisibleSprite = (int16_t)id; original[(top + y) * 256 + left + x] = sourceColor; }
			}
		}
		if(!largeGroups || id % 16 == 0) {
			MetaSpriteReplacement group; group.Id = id + 1;
			group.ArtworkScale = 2; group.ArtworkWidth = group.ArtworkHeight = largeGroups ? 128 : 32;
			group.Artwork.assign(group.ArtworkWidth * group.ArtworkHeight, effects ? 0x8000FF00 : 0xFF00FF00); group.Artwork[0] = 0;
			groups.push_back(std::move(group));
		}
		groups.back().Members.push_back(std::move(member));
	}
	MetaSpriteCompositor compositor; compositor.SetGroups(std::move(groups));
	if(effects >= 3) {
		if(effects == 4) { frame.SnesBackgrounds.resize(frame.Pixels.size()); }
		for(int y = 0; y < 239; y++) { for(int x = 0; x < 256; x++) {
			auto& pixel = frame.Pixels[y * 256 + x];
			pixel.BackgroundSample = {x, y, 0, true}; pixel.SnesFallbackCaptured = true;
			pixel.SnesFallbackMainColor = 0x7C00; pixel.Brightness = 15;
			if(effects == 4) {
				auto& captured = frame.SnesBackgrounds[y * 256 + x];
				captured.Captured = true; captured.BackdropColor = 0x7C00;
				captured.MainPriority = pixel.VisibleSprite >= 0 ? 3 : 0;
				captured.Layers[0].Sample = {x, y, 0, true}; captured.Layers[0].Priority = 1;
				captured.Layers[0].Opaque = captured.Layers[0].Main = true; captured.Layers[0].Color = 0x7C00;
			}
		} }
		MetaBackgroundReplacement background;
		background.Id = 1; background.Scale = 2; background.Width = background.Height = 2;
		background.WrapX = background.WrapY = true; background.Artwork.assign(4, 0x80FFFFFF);
		compositor.SetBackgroundLayers({background});
	}
	for(int warmup = 0; warmup < 8; warmup++) { compositor.Apply(original.data(), 256, 239, frame); }
	constexpr int iterations = 120;
	std::vector<double> timings;
	timings.reserve(iterations);
	double total = 0;
	int overBudget = 0;
	uint64_t checksum = 0;
	for(int i = 0; i < iterations; i++) {
		auto start = std::chrono::steady_clock::now();
		const auto* output = compositor.Apply(original.data(), 256, 239, frame);
		checksum = 0;
		for(int j = 0; j < 512 * 478; j++) { checksum += output[j]; }
		double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		timings.push_back(elapsed); total += elapsed;
		if(elapsed > 16.67) { overBudget++; }
	}
	double milliseconds = total / iterations;
	std::sort(timings.begin(), timings.end());
	std::cout << "BENCH: " << (effects == 1 ? "GBA alpha/effects " : effects == 2 ? "SNES alpha/effects " : effects == 3 ? "SNES background/alpha " : effects == 4 ? "SNES captured layers/alpha " : "") << (largeGroups ? "8 metasprites of 16 members" : "32 matched 16x16 sprites") << ", 256x239 frame, 2x artwork: " << milliseconds << " ms/frame, checksum=" << checksum << std::endl;
	std::cout << "TIMING: composition plus checksum readback, " << iterations << " frames; p50=" << timings[59]
		<< " ms, p95=" << timings[113] << " ms, p99=" << timings[118] << " ms, max=" << timings.back()
		<< " ms, frames over 16.67 ms=" << overBudget << "/" << iterations
		<< "; excludes emulation, capture, filtering, and presentation." << std::endl;
}

int main(int argc, char** argv)
{
	assert(MetaSpriteCompositor::CompositeAlpha(0x12010203, 0x34040506, 255) == 0xFF010203);
	assert(MetaSpriteCompositor::CompositeAlpha(0x12010203, 0x34040506, 0) == 0xFF040506);
	ViewerRefreshSchedule refreshSchedule;
	assert(!refreshSchedule.HasViewers());
	refreshSchedule.Set(123, 224, 0); refreshSchedule.Set(456, 240, 16); refreshSchedule.Set(789, 224, 0);
	std::vector<uint32_t> refreshed;
	auto visitRefresh = [&](uint16_t scanline, uint16_t cycle) {
		refreshed.clear(); refreshSchedule.Visit(scanline, cycle, [&](uint32_t id) { refreshed.push_back(id); });
		std::sort(refreshed.begin(), refreshed.end());
	};
	visitRefresh(224, 0); assert((refreshed == std::vector<uint32_t>{123, 789}));
	visitRefresh(224, 16); assert(refreshed.empty()); // Both masks match, but no viewer uses this pair.
	visitRefresh(240, 16); assert((refreshed == std::vector<uint32_t>{456}));
	refreshSchedule.Set(123, UINT16_MAX, UINT16_MAX);
	visitRefresh(224, 0); assert((refreshed == std::vector<uint32_t>{789}));
	visitRefresh(UINT16_MAX, UINT16_MAX); assert((refreshed == std::vector<uint32_t>{123}));
	refreshSchedule.Remove(789); refreshSchedule.Remove(456); refreshSchedule.Remove(123); refreshSchedule.Remove(999);
	assert(!refreshSchedule.HasViewers());
	visitRefresh(224, 0); assert(refreshed.empty());
	refreshSchedule.Set(1, 0, 0); visitRefresh(0, 0); assert((refreshed == std::vector<uint32_t>{1}));
	refreshSchedule.Set(2, 64, 63); refreshSchedule.Set(3, 63, 64);
	visitRefresh(64, 63); assert((refreshed == std::vector<uint32_t>{2}));
	visitRefresh(63, 64); assert((refreshed == std::vector<uint32_t>{3}));
	visitRefresh(64, 64); assert(refreshed.empty());
	refreshSchedule.Remove(2); visitRefresh(64, 63); assert(refreshed.empty());
	// Coverage storage must retain order through growth, deep copies, moves and reuse.
	MetaSpriteCoverageList<MetaGbaSpriteCoverage> coverageList;
	for(int i = 0; i < 128; i++) { coverageList.push_back({static_cast<int16_t>(i), static_cast<uint16_t>(i * 3)}); }
	auto copiedCoverage = coverageList;
	copiedCoverage[0].Sprite = 127;
	if(coverageList[0].Sprite != 0) { return 1; }
	auto movedCoverage = std::move(copiedCoverage);
	if(copiedCoverage.begin() != copiedCoverage.end() || movedCoverage[127].SecondaryColor != 381) { return 1; }
	coverageList = movedCoverage;
	movedCoverage.clear(); movedCoverage.push_back({7, 9});
	if(coverageList[0].Sprite != 127 || movedCoverage[0].Sprite != 7) { return 1; }
	int coverageCount = 0;
	for(const auto& coverage : coverageList) {
		if(coverage.Sprite != (coverageCount == 0 ? 127 : coverageCount)) { return 1; }
		coverageCount++;
	}
	if(coverageCount != 128) { return 1; }
	for(int i = 0; i < 3; i++) { movedCoverage.push_back({8, 10}); }
	movedCoverage.push_back(movedCoverage[0]);
	if(movedCoverage[4].Sprite != 7 || movedCoverage[4].SecondaryColor != 9) { return 1; }
	MetaSpriteCoverageList<MetaGbaSpriteCoverage> swappedCoverage;
	swappedCoverage.swap(movedCoverage);
	if(movedCoverage.begin() != movedCoverage.end() || swappedCoverage[4].SecondaryColor != 9) { return 1; }
	MetaSpriteFramePool framePool;
	auto firstFrame = framePool.Acquire(3, 2);
	auto retainedFrame = firstFrame;
	auto secondFrame = framePool.Acquire(3, 2);
	auto thirdFrame = framePool.Acquire(3, 2);
	firstFrame->FrameNumber = 99; firstFrame->GbaColorEffects = firstFrame->SnesColorEffects = true;
	firstFrame->Pixels[0].VisibleSprite = 7; firstFrame->Pixels[0].BackgroundSample = {4, 5, 2, true};
	firstFrame->Pixels[0].GbaCoverage.push_back({7, 9});
	firstFrame->Pixels[0].SnesCoverage.push_back({7, 1, 3, 4, true, false, true});
	firstFrame->SnesBackgrounds.resize(6); firstFrame->Rows.resize(1); firstFrame->DecodedRows.resize(1); firstFrame->NesRows.resize(1);
	firstFrame->MemorySnapshot[0x123] = 9;
	auto overflowFrame = framePool.Acquire(3, 2);
	if(overflowFrame == firstFrame || overflowFrame == secondFrame || overflowFrame == thirdFrame) { return 1; }
	firstFrame.reset(); overflowFrame.reset();
	overflowFrame = framePool.Acquire(3, 2);
	if(overflowFrame == retainedFrame || retainedFrame->Pixels[0].VisibleSprite != 7) { return 1; }
	MetaSpriteFrame* reusableAddress = retainedFrame.get();
	retainedFrame.reset();
	auto reusedFrame = framePool.Acquire(3, 2);
	if(reusedFrame.get() != reusableAddress || reusedFrame->FrameNumber != 0 || reusedFrame->GbaColorEffects || reusedFrame->SnesColorEffects
		|| reusedFrame->Pixels[0].VisibleSprite != -1 || reusedFrame->Pixels[0].BackgroundSample.Valid
		|| reusedFrame->Pixels[0].GbaCoverage.begin() != reusedFrame->Pixels[0].GbaCoverage.end()
		|| reusedFrame->Pixels[0].SnesCoverage.begin() != reusedFrame->Pixels[0].SnesCoverage.end()
		|| !reusedFrame->SnesBackgrounds.empty() || !reusedFrame->Rows.empty() || !reusedFrame->DecodedRows.empty()
		|| !reusedFrame->NesRows.empty() || !reusedFrame->MemorySnapshot.empty()) { return 1; }
	if(reusedFrame->Pixels[0].GbaObjectPriority != 0xFF || reusedFrame->Pixels[0].SnesFallbackMainLayer != 0xFF
		|| reusedFrame->Pixels[0].MainCoefficient != 16 || !reusedFrame->Pixels[0].SnesWindowMathAllowed) { return 1; }
	auto otherSize = framePool.Acquire(4, 3);
	if(otherSize->Width != 4 || otherSize->Height != 3 || otherSize->Pixels.size() != 12 || reusedFrame->Width != 3) { return 1; }
	if(argc > 1 && std::string(argv[1]) == "--benchmark-capture-allocation") {
		auto start = std::chrono::steady_clock::now();
		uint64_t checksum = 0;
		for(int i = 0; i < 120; i++) {
			MetaSpriteFrame allocatedFrame(256, 239);
			checksum += allocatedFrame.Pixels.size();
		}
		std::cout << "Capture frame allocation/destruction: " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 120
			<< " ms/frame; checksum=" << checksum << std::endl;
		MetaSpriteFramePool allocationPool;
		auto pooledFrame = allocationPool.Acquire(256, 239);
		pooledFrame = allocationPool.Acquire(256, 239);
		start = std::chrono::steady_clock::now();
		for(int i = 0; i < 120; i++) { pooledFrame = allocationPool.Acquire(256, 239); }
		std::cout << "Capture frame reuse/reset: " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 120 << " ms/frame" << std::endl;
		return 0;
	}
	if(argc > 1 && std::string(argv[1]) == "--benchmark") { BenchmarkComposition(); return 0; }
	if(argc > 1 && std::string(argv[1]) == "--benchmark-metagroups") { BenchmarkComposition(true); return 0; }
	if(argc > 1 && std::string(argv[1]) == "--benchmark-snes-background") { BenchmarkComposition(true, 3); return 0; }
	if(argc > 1 && std::string(argv[1]) == "--benchmark-snes-background-layers") { BenchmarkComposition(true, 4); return 0; }
	if(argc > 1 && std::string(argv[1]) == "--benchmark-gba-alpha") { BenchmarkComposition(true, 1); return 0; }
	if(argc > 1 && std::string(argv[1]) == "--benchmark-snes-alpha") { BenchmarkComposition(true, 2); return 0; }
	MetaSpriteFrame crossWord(3, 1);
	MetaBackgroundView emptyBackgroundView;
	assert(!(emptyBackgroundView.begin() != emptyBackgroundView.end()));
	std::vector<MetaBackgroundReplacement> viewLayers(2);
	viewLayers[0].Id = 9; viewLayers[1].Id = 3;
	MetaBackgroundView orderedBackgroundView(viewLayers);
	int viewIndex = 0;
	for(const auto& layer : orderedBackgroundView) { assert(&layer == &viewLayers[viewIndex++]); }
	assert(viewIndex == 2);
	for(int dimension : {1, 2, 3, 4, 5, 8, 17}) {
		MetaBackgroundReplacement wrapping;
		wrapping.Id = 1; wrapping.X = 3; wrapping.Y = -5; wrapping.Scale = 2;
		wrapping.Width = wrapping.Height = dimension * 2; wrapping.WrapX = wrapping.WrapY = true;
		for(int i = 0; i < wrapping.Width * wrapping.Height; i++) { wrapping.Artwork.push_back(0xFF000000u | i); }
		for(int x : {INT32_MIN, -18, -9, -1, 0, 1, 9, 18, INT32_MAX}) {
			for(int y : {INT32_MIN, -18, -9, -1, 0, 1, 9, 18, INT32_MAX}) {
				int64_t expectedX = (static_cast<int64_t>(x) - wrapping.X) % dimension;
				int64_t expectedY = (static_cast<int64_t>(y) - wrapping.Y) % dimension;
				if(expectedX < 0) { expectedX += dimension; } if(expectedY < 0) { expectedY += dimension; }
				for(int sy = 0; sy < 2; sy++) { for(int sx = 0; sx < 2; sx++) {
					uint32_t color;
					assert(wrapping.Sample({x, y, 0, true}, 0, 0, sx, sy, 2, color));
					assert(color == wrapping.Artwork[(expectedY * 2 + sy) * wrapping.Width + expectedX * 2 + sx]);
				} }
			}
		}
	}
	crossWord.DecodedRows = {{63, 0, 0, 0, 1, 1, {0xFFFF0000}}, {64, 1, 0, 0, 1, 1, {0}}};
	crossWord.Pixels[0].VisibleSprite = 63; crossWord.Pixels[0].BackgroundColor = 0x7C00;
	crossWord.Pixels[1].AllowReplacementSprite(64); crossWord.Pixels[2].AllowReplacementSprite(32);
	MetaSpriteReplacement crossWordGroup;
	crossWordGroup.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}, {1, 0, 1, 1, -1, {0}}};
	crossWordGroup.ArtworkWidth = 3; crossWordGroup.ArtworkHeight = 1; crossWordGroup.Artwork.assign(3, 0xFF00FF00);
	MetaSpriteCompositor crossWordCompositor; crossWordCompositor.SetGroups({crossWordGroup});
	uint32_t crossWordOriginal[] = {0xFFFF0000, 0xFF0000FF, 0xFF0000FF};
	const auto* crossWordOutput = crossWordCompositor.Apply(crossWordOriginal, 3, 1, crossWord);
	assert(crossWordOutput[0] == 0xFF00FF00 && crossWordOutput[1] == 0xFF00FF00 && crossWordOutput[2] == 0xFF0000FF);
	crossWordGroup.MemoryConditions = {{0x1234, 0xA0, 0xF0}, {0x5678, 3, 0xFF}};
	crossWordGroup.ArtworkScale = 2;
	crossWordGroup.ArtworkWidth = 6; crossWordGroup.ArtworkHeight = 2;
	crossWordGroup.Artwork.assign(12, 0xFF00FF00);
	crossWordCompositor.SetGroups({crossWordGroup});
	assert(crossWordCompositor.Apply(crossWordOriginal, 3, 1, crossWord)[0] == crossWordOriginal[0]);
	assert(crossWordCompositor.GetOutputScale() == 1);
	crossWord.MemorySnapshot = {{0x1234u, uint8_t(0xA7)}, {0x5678u, uint8_t(3)}};
	assert(crossWordCompositor.Apply(crossWordOriginal, 3, 1, crossWord)[0] == 0xFF00FF00);
	assert(crossWordCompositor.GetOutputScale() == 2);
	crossWord.MemorySnapshot[0x5678] = 4;
	assert(crossWordCompositor.Apply(crossWordOriginal, 3, 1, crossWord)[0] == crossWordOriginal[0]);
	crossWord.MemorySnapshot.erase(0x5678);
	assert(crossWordCompositor.Apply(crossWordOriginal, 3, 1, crossWord)[0] == crossWordOriginal[0]);
	assert(crossWordCompositor.GetOutputScale() == 1);
	MetaSpriteFrame boundedFrame(4, 1);
	crossWordGroup.MemoryConditions.push_back({0x1234, 0xA0, 0xF0});
	crossWordCompositor.SetGroups({crossWordGroup});
	int memoryReads = 0;
	crossWordCompositor.CaptureMemorySnapshot(crossWord, [&](uint32_t address, uint8_t& value) {
		memoryReads++; value = address == 0x1234 ? 0xA7 : 3; return true;
	});
	assert(memoryReads == 2 && crossWord.MemorySnapshot.size() == 2);
	assert(crossWordCompositor.Apply(crossWordOriginal, 3, 1, crossWord)[0] == 0xFF00FF00);
	crossWordCompositor.CaptureMemorySnapshot(crossWord, [](uint32_t, uint8_t&) { return false; });
	assert(crossWord.MemorySnapshot.empty());
	assert(crossWordCompositor.Apply(crossWordOriginal, 3, 1, crossWord)[0] == crossWordOriginal[0]);
	crossWordCompositor.SetGroups({});
	crossWordCompositor.CaptureMemorySnapshot(crossWord, [&](uint32_t, uint8_t&) { assert(false); return false; });
	boundedFrame.DecodedRows = {{0, 1, 0, 0, 2, 1, {0xFFFF0000, 0xFFFF0000}}};
	for(int x : {1, 2}) { boundedFrame.Pixels[x].VisibleSprite = 0; boundedFrame.Pixels[x].BackgroundColor = 0x7C00; }
	MetaSpriteReplacement boundedGroup;
	boundedGroup.ArtworkWidth = boundedGroup.ArtworkHeight = 1; boundedGroup.Artwork = {0xFF00FF00};
	boundedGroup.Members = {{0, 0, 2, 1, -1, {0xFFFF0000, 0xFFFF0000}}};
	MetaSpriteCompositor boundedCompositor; boundedCompositor.SetGroups({boundedGroup});
	MetaSpriteFrame malformedFrame(4, 1);
	malformedFrame.Pixels.resize(3);
	uint32_t malformedOriginal[] = {0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF};
	assert(boundedCompositor.Apply(malformedOriginal, 4, 1, malformedFrame) == malformedOriginal);
	uint32_t boundedOriginal[] = {0xFF0000FF, 0xFFFF0000, 0xFFFF0000, 0xFF0000FF};
	const auto* boundedOutput = boundedCompositor.Apply(boundedOriginal, 4, 1, boundedFrame);
	assert(boundedOutput[0] == boundedOriginal[0] && boundedOutput[1] == 0xFF00FF00 && boundedOutput[2] == 0xFF0000FF && boundedOutput[3] == boundedOriginal[3]);
	boundedGroup.ArtworkOriginX = -1; boundedCompositor.SetGroups({boundedGroup});
	uint32_t boundedCrop[] = {0xFFFF0000, 0xFFFF0000};
	boundedOutput = boundedCompositor.Apply(boundedCrop, 2, 1, boundedFrame, 1);
	assert(boundedOutput[0] == 0xFF0000FF && boundedOutput[1] == 0xFF00FF00);
	MetaSpriteReplacement alternateGroup = boundedGroup;
	alternateGroup.ArtworkOriginX = 0;
	MetaAlternateArtwork alternate;
	alternate.ArtworkOriginX = -1; alternate.ArtworkScale = 2;
	alternate.ArtworkWidth = alternate.ArtworkHeight = 2;
	alternate.Artwork.assign(4, 0xFFFFFFFF);
	alternate.MemoryConditions = {{0x2345, 1, 0xFF}};
	alternateGroup.AlternateArtwork = {alternate};
	alternate.Artwork.assign(4, 0xFFFF0000);
	alternateGroup.AlternateArtwork.push_back(alternate);
	MetaSpriteCompositor alternateCompositor; alternateCompositor.SetGroups({alternateGroup});
	auto alternateOutput = alternateCompositor.Apply(boundedOriginal, 4, 1, boundedFrame);
	assert(alternateCompositor.GetOutputScale() == 1 && alternateOutput[1] == 0xFF00FF00);
	int alternateReads = 0;
	alternateCompositor.CaptureMemorySnapshot(boundedFrame, [&](uint32_t address, uint8_t& value) {
		assert(address == 0x2345); alternateReads++; value = 1; return true;
	});
	assert(alternateReads == 1);
	alternateOutput = alternateCompositor.Apply(boundedOriginal, 4, 1, boundedFrame);
	assert(alternateCompositor.GetOutputScale() == 2 && alternateOutput[2] == 0xFF0000FF && alternateOutput[4] == 0xFFFFFFFF);
	alternateGroup.AlternateArtwork[0].Artwork[0] = 0;
	alternateGroup.AlternateArtwork[0].Artwork[1] = 0x80FFFFFF;
	alternateCompositor.SetGroups({alternateGroup});
	alternateOutput = alternateCompositor.Apply(boundedOriginal, 4, 1, boundedFrame);
	assert(alternateOutput[4] == 0xFF0000FF && alternateOutput[5] == 0xFF8080FF);
	boundedFrame.MemorySnapshot[0x2345] = 2;
	alternateOutput = alternateCompositor.Apply(boundedOriginal, 4, 1, boundedFrame);
	assert(alternateCompositor.GetOutputScale() == 1 && alternateOutput[1] == 0xFF00FF00);
	MetaSpritePixel gbaComposition;
	gbaComposition.GbaObjectSprite = 0; gbaComposition.GbaObjectPriority = 1; gbaComposition.GbaObjectColor = 31;
	gbaComposition.GbaBackdropColor = 0x7C00;
	gbaComposition.GbaBackgroundSamples[0] = {0, 0, 0, true};
	gbaComposition.GbaBackgroundLayers[0] = {0x7C00, 0, false};
	gbaComposition.GbaWindowEffectsEnabled = true;
	gbaComposition.GbaBlendMainMask = 1; gbaComposition.GbaBlendSubMask = 16 | 32;
	gbaComposition.Effect = 1; gbaComposition.MainCoefficient = gbaComposition.SecondaryCoefficient = 8;
	MetaBackgroundReplacement gbaArtwork;
	gbaArtwork.Id = 1; gbaArtwork.Layer = 0; gbaArtwork.Width = gbaArtwork.Height = 1; gbaArtwork.Artwork = {0xFF00FF00};
	assert(MetaSpriteCompositor::ComposeGbaLayers(gbaComposition, {gbaArtwork}, 0, 0, 0, 0, 1) == 0xFF7F7F00);
	gbaArtwork.Artwork[0] = 0;
	assert(MetaSpriteCompositor::ComposeGbaLayers(gbaComposition, {gbaArtwork}, 0, 0, 0, 0, 1) == 0xFFFF0000);
	gbaArtwork.Artwork[0] = 0x8000FF00;
	assert(MetaSpriteCompositor::ComposeGbaLayers(gbaComposition, {gbaArtwork}, 0, 0, 0, 0, 1) == 0xFFBF4000);
	gbaArtwork.Artwork[0] = 0xFF00FF00;
	assert(MetaSpriteCompositor::ComposeGbaLayers(gbaComposition, {gbaArtwork}, 0, 0, 0, 0, 1, true, 0) == 0xFF007F7F);
	gbaComposition.GbaObjectPriority = 0;
	assert(MetaSpriteCompositor::ComposeGbaLayers(gbaComposition, {gbaArtwork}, 0, 0, 0, 0, 1) == 0xFFFF0000);
	gbaComposition.GbaBlendSubMask |= 1; gbaComposition.GbaWindowEffectsEnabled = false;
	assert(MetaSpriteCompositor::ComposeGbaLayers(gbaComposition, {gbaArtwork}, 0, 0, 0, 0, 1, true, 0xFFFFFFFF, 0, true) == 0xFF7FFF7F);
	gbaComposition.GbaObjectPriority = 1; gbaComposition.GbaWindowEffectsEnabled = true;
	gbaComposition.Effect = 3; gbaComposition.Brightness = 8;
	assert(MetaSpriteCompositor::ComposeGbaLayers(gbaComposition, {gbaArtwork}, 0, 0, 0, 0, 1) == 0xFF007F00);
	gbaComposition.GbaBackgroundSamples[0].Valid = false;
	assert(MetaSpriteCompositor::ComposeGbaLayers(gbaComposition, {gbaArtwork}, 0, 0, 0, 0, 1) == 0xFFFF0000);

	if(argc > 1) {
		std::ifstream input(std::string(argv[1]) + ".background", std::ios::binary);
		std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		assert(!bytes.empty());
		auto transferred = MetaBackgroundPacket(bytes.data(), bytes.size()).Parse();
		assert(transferred.size() == 1 && transferred[0].Id == 8 && transferred[0].Layer == 1 && transferred[0].X == -3
			&& transferred[0].Y == 5 && transferred[0].WrapX && transferred[0].FollowScroll && transferred[0].Scale == 2
			&& transferred[0].Artwork[1] == 0x8000FF00);
		for(size_t length = 0; length < bytes.size(); length++) {
			bool rejected = false;
			try { MetaBackgroundPacket(bytes.data(), length).Parse(); } catch(const std::runtime_error&) { rejected = true; }
			assert(rejected);
		}
		bytes.push_back(0);
		bool rejected = false;
		try { MetaBackgroundPacket(bytes.data(), bytes.size()).Parse(); } catch(const std::runtime_error&) { rejected = true; }
		assert(rejected);
	}
	MetaBackgroundReplacement background;
	if(argc > 1) {
		std::ifstream input(std::string(argv[1]) + ".background.conditions", std::ios::binary);
		assert(input.good());
		std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		auto layers = MetaBackgroundPacket(bytes.data(), bytes.size()).Parse();
		assert(layers.size() == 1 && layers[0].MemoryConditions.size() == 1);
		assert(layers[0].MemoryConditions[0].Address == 0xC123 && layers[0].MemoryConditions[0].Value == 0xA7 && layers[0].MemoryConditions[0].Mask == 0xF0);
		MetaSpriteFrame conditionalFrame(2, 2);
		for(auto& pixel : conditionalFrame.Pixels) { pixel.BackgroundSample = {-3, 5, 1, true}; }
		MetaSpriteCompositor conditionalCompositor; conditionalCompositor.SetReplacements({}, layers);
		uint32_t native[] = {0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF};
		assert(conditionalCompositor.Apply(native, 2, 2, conditionalFrame, 0, 0, [](uint16_t) { return 0xFF0000FF; }) == native);
		conditionalCompositor.CaptureMemorySnapshot(conditionalFrame, [](uint32_t address, uint8_t& value) {
			assert(address == 0xC123); value = 0xAF; return true;
		});
		assert(conditionalCompositor.Apply(native, 2, 2, conditionalFrame, 0, 0, [](uint16_t) { return 0xFF0000FF; })[1] == 0xFF00807F);
		conditionalFrame.MemorySnapshot[0xC123] = 0xB0;
		assert(conditionalCompositor.Apply(native, 2, 2, conditionalFrame, 0, 0, [](uint16_t) { return 0xFF0000FF; }) == native);
		for(size_t length = 0; length < bytes.size(); length++) {
			bool invalid = false;
			try { MetaBackgroundPacket(bytes.data(), length).Parse(); } catch(const std::runtime_error&) { invalid = true; }
			assert(invalid);
		}
		bytes[bytes.size() - 3] = 1;
		bool invalid = false;
		try { MetaBackgroundPacket(bytes.data(), bytes.size()).Parse(); } catch(const std::runtime_error&) { invalid = true; }
		assert(invalid);
	}
	background.Id = 1; background.Layer = 1; background.Width = 4; background.Height = 4; background.Scale = 2;
	background.X = 3; background.Y = 5; background.WrapX = background.WrapY = true;
	for(uint32_t i = 0; i < 16; i++) { background.Artwork.push_back(0x80000000 | i); }
	MetaBackgroundSample source; source.Valid = true; source.Layer = 1; source.X = 2; source.Y = 4;
	uint32_t sampled = 0;
	assert(background.Sample(source, 0, 0, 1, 1, 2, sampled) && sampled == 0x8000000F);
	source.X = 3; source.Y = 5;
	for(int y = 0; y < 3; y++) {
		for(int x = 0; x < 3; x++) {
			assert(background.Sample(source, 0, 0, x, y, 3, sampled) && sampled == (0x80000000u | ((y * 2 / 3) * 4 + x * 2 / 3)));
		}
	}
	background.FollowScroll = false;
	assert(background.Sample(source, 4, 6, 0, 0, 1, sampled) && sampled == 0x8000000A);
	source.Layer = 0; assert(!background.Sample(source, 4, 6, 0, 0, 1, sampled) && sampled == 0);
	source.Layer = 1; source.Valid = false; assert(!background.Sample(source, 4, 6, 0, 0, 1, sampled));
	source.Valid = true; background.WrapX = false; assert(!background.Sample(source, 2, 6, 0, 0, 1, sampled));
	background.Artwork.pop_back(); assert(!background.IsValid() && !background.Sample(source, 4, 6, 0, 0, 1, sampled));
	MetaSpriteFrame backgroundFrame(2, 2);
	for(auto& pixel : backgroundFrame.Pixels) { pixel.BackgroundSample.Valid = true; }
	backgroundFrame.Pixels[1].VisibleSprite = 9;
	backgroundFrame.Pixels[2].BackgroundSample.Valid = false;
	MetaBackgroundReplacement compositionLayer;
	compositionLayer.Id = 1; compositionLayer.Width = compositionLayer.Height = 2;
	compositionLayer.Scale = 2; compositionLayer.WrapX = compositionLayer.WrapY = true;
	compositionLayer.Artwork = {0, 0x8000FF00, 0xFFFF0000, 0xFFFFFFFF};
	std::vector<uint32_t> backgroundOutput(16, 0xFF0000FF);
	assert(MetaBackgroundCompositor::Apply(backgroundFrame, {compositionLayer}, 2, backgroundOutput));
	assert(backgroundOutput[0] == 0xFF0000FF && backgroundOutput[1] == 0xFF00807F);
	assert(backgroundOutput[4] == 0xFFFF0000 && backgroundOutput[5] == 0xFFFFFFFF);
	assert(backgroundOutput[2] == 0xFF0000FF && backgroundOutput[6] == 0xFF0000FF); // Winning OBJ.
	assert(backgroundOutput[8] == 0xFF0000FF && backgroundOutput[12] == 0xFF0000FF); // Invalid capture.
	auto unchangedBackground = backgroundOutput;
	MetaSpriteCompositor gatedBackground;
	compositionLayer.MemoryConditions = {{0x1234, 1, 0xFF}};
	gatedBackground.SetReplacements({}, {compositionLayer});
	uint32_t nativeBackground[] = {0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF};
	assert(gatedBackground.Apply(nativeBackground, 2, 2, backgroundFrame, 0, 0, [](uint16_t) { return 0xFF0000FF; }) == nativeBackground);
	int backgroundReads = 0;
	gatedBackground.CaptureMemorySnapshot(backgroundFrame, [&](uint32_t address, uint8_t& value) {
		assert(address == 0x1234); backgroundReads++; value = 1; return true;
	});
	assert(backgroundReads == 1);
	assert(gatedBackground.Apply(nativeBackground, 2, 2, backgroundFrame, 0, 0, [](uint16_t) { return 0xFF0000FF; })[1] == 0xFF00807F);
	assert(gatedBackground.GetOutputScale() == 2);
	MetaSpriteFrame conditionalGba(2, 2); conditionalGba.GbaColorEffects = true;
	conditionalGba.MemorySnapshot = backgroundFrame.MemorySnapshot;
	for(auto& pixel : conditionalGba.Pixels) {
		pixel.GbaFallbackCaptured = true; pixel.GbaBackdropColor = 0x7C00;
		pixel.GbaBackgroundSamples[0] = {0, 0, 0, true};
		pixel.GbaBackgroundLayers[0].Priority = 3;
	}
	assert(gatedBackground.Apply(nativeBackground, 2, 2, conditionalGba)[1] == 0xFF00807F);
	conditionalGba.MemorySnapshot[0x1234] = 2;
	assert(gatedBackground.Apply(nativeBackground, 2, 2, conditionalGba) == nativeBackground && gatedBackground.GetOutputScale() == 1);
	backgroundFrame.MemorySnapshot[0x1234] = 2;
	assert(gatedBackground.Apply(nativeBackground, 2, 2, backgroundFrame, 0, 0, [](uint16_t) { return 0xFF0000FF; }) == nativeBackground);
	backgroundFrame.GbaColorEffects = true;
	assert(!MetaBackgroundCompositor::Apply(backgroundFrame, {compositionLayer}, 2, backgroundOutput) && backgroundOutput == unchangedBackground);
	MetaSpritePixel snesEffect;
	snesEffect.Brightness = 15; snesEffect.SnesMathEnabled = true; snesEffect.SnesUseSubscreen = true;
	assert(MetaSpriteCompositor::ApplySnesColorEffect(0xFFFF0000, 0xFF0000FF, snesEffect) == 0xFFFF00FF);
	snesEffect.SnesHalf = true;
	assert(MetaSpriteCompositor::ApplySnesColorEffect(0xFFFFFFFF, 0xFFFFFFFF, snesEffect) == 0xFFFFFFFF);
	snesEffect.SnesSubtract = true;
	assert(MetaSpriteCompositor::ApplySnesColorEffect(0xFF80FF00, 0xFFFF8000, snesEffect) == 0xFF003F00);
	snesEffect.SnesMathEnabled = false; snesEffect.Brightness = 0;
	assert(MetaSpriteCompositor::ApplySnesColorEffect(0xFFFFFFFF, 0, snesEffect) == 0xFF000000);
	snesEffect.Brightness = 15; snesEffect.SnesClip = true;
	assert(MetaSpriteCompositor::ApplySnesColorEffect(0xFFFFFFFF, 0, snesEffect) == 0xFF000000);
	MetaSpriteFrame nes(256, 240);
	auto nesPalette = [](uint16_t index) { return index == 0x121 ? 0xFF112233u : 0xFF445566u; };
	for(int y = 0; y < 8; y++) {
		MetaSpriteRow row;
		row.Sprite = 37; row.X = 18; row.Y = 9; row.Scanline = static_cast<uint16_t>(9 + y); row.Height = 8;
		row.LowByte = 0xF0; row.HighByte = 0x0C; row.Palette[1] = 0x121; row.Palette[2] = 0x022;
		nes.NesRows.push_back(row);
	}
	auto nesCandidates = MetaSpriteCompositor::BuildNesCandidates(nes, nesPalette);
	assert(nesCandidates.size() == 1 && nesCandidates[0].HardwareSprite == 37 && nesCandidates[0].X == 18 && nesCandidates[0].Y == 9);
	assert(nesCandidates[0].Pixels[0] == 0xFF112233 && nesCandidates[0].Pixels[4] == 0xFF445566 && nesCandidates[0].Pixels[7] == 0);
	assert(MetaSpriteCompositor::BuildNesCandidates(nes, {}).empty());
	auto lastNesRow = nes.NesRows.back(); nes.NesRows.pop_back();
	assert(MetaSpriteCompositor::BuildNesCandidates(nes, nesPalette).empty());
	nes.NesRows.push_back(lastNesRow); lastNesRow.LowByte = 0;
	nes.NesRows.push_back(lastNesRow);
	assert(MetaSpriteCompositor::BuildNesCandidates(nes, nesPalette).empty());
	MetaSpritePixel effect;
	effect.Effect = 1; effect.EffectsEnabled = true; effect.SecondaryBlendTarget = true;
	effect.MainCoefficient = 8; effect.SecondaryCoefficient = 8;
	assert(MetaSpriteCompositor::ApplyGbaColorEffect(0xFFFF0000, 0xFF0000FF, effect) == 0xFF7F007F);
	effect.EffectsEnabled = false;
	assert(MetaSpriteCompositor::ApplyGbaColorEffect(0xFFFF0000, 0xFF0000FF, effect) == 0xFFFF0000);
	effect.SemiTransparent = true; // Hardware object blending overrides the window effect mask.
	assert(MetaSpriteCompositor::ApplyGbaColorEffect(0xFFFF0000, 0xFF0000FF, effect) == 0xFF7F007F);
	effect.MainCoefficient = 31; effect.SecondaryCoefficient = 31;
	assert(MetaSpriteCompositor::ApplyGbaColorEffect(0xFFAAAAAA, 0xFFAAAAAA, effect) == 0xFFFFFFFF);
	effect.SemiTransparent = false; effect.EffectsEnabled = true; effect.Effect = 2;
	effect.Brightness = 8;
	assert(MetaSpriteCompositor::ApplyGbaColorEffect(0xFF000000, 0, effect) == 0xFF7F7F7F);
	effect.Effect = 3;
	assert(MetaSpriteCompositor::ApplyGbaColorEffect(0xFFFFFFFF, 0, effect) == 0xFF7F7F7F);
	effect.Brightness = 31;
	assert(MetaSpriteCompositor::ApplyGbaColorEffect(0xFFFFFFFF, 0, effect) == 0xFF000000);
	effect.Effect = 2;
	assert(MetaSpriteCompositor::ApplyGbaColorEffect(0xFF123456, 0, effect) == 0xFFFFFFFF);
	effect.Brightness = 0;
	assert(MetaSpriteCompositor::ApplyGbaColorEffect(0xFF123456, 0, effect) == 0xFF123456);
	MetaSpriteFrame slices(32, 16);
	MetaSpriteFrame gbaLayers(1, 1);
	gbaLayers.GbaColorEffects = true;
	gbaLayers.DecodedRows = {{1, 0, 0, 0, 1, 1, {0xFFFF0000}}, {2, 0, 0, 0, 1, 1, {0xFF0000FF}}};
	auto& layeredPixel = gbaLayers.Pixels[0];
	layeredPixel.VisibleSprite = 1; layeredPixel.SecondarySprite = 2;
	layeredPixel.SpriteColor = 31; layeredPixel.SecondaryColor = 0x7C00;
	layeredPixel.Effect = 1; layeredPixel.EffectsEnabled = true; layeredPixel.SecondaryBlendTarget = true;
	layeredPixel.MainCoefficient = 8; layeredPixel.SecondaryCoefficient = 8;
	MetaSpriteReplacement primaryGroup, secondaryGroup;
	primaryGroup.ArtworkWidth = primaryGroup.ArtworkHeight = 1;
	primaryGroup.Artwork = {0xFF00FF00}; primaryGroup.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
	secondaryGroup = primaryGroup;
	secondaryGroup.Artwork = {0xFFFFFFFF}; secondaryGroup.Members[0].Pixels = {0xFF0000FF};
	MetaSpriteCompositor layeredCompositor;
	uint32_t layeredOriginal = 0xFF7F007F;
	layeredCompositor.SetGroups({primaryGroup, secondaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF7FFF7F);
	layeredCompositor.SetGroups({secondaryGroup, primaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF7FFF7F);
	layeredCompositor.SetGroups({secondaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFFFF7F7F);
	layeredPixel.GbaFallbackCaptured = true;
	layeredPixel.GbaFallbackColor = 0x7C00;
	layeredPixel.GbaFallbackMainColor = 0;
	layeredPixel.GbaFallbackBlendTarget = false;
	primaryGroup.Artwork[0] = 0;
	layeredCompositor.SetGroups({primaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF0000FF);
	primaryGroup.Artwork[0] = 0x8000FF00;
	layeredCompositor.SetGroups({primaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF0040BF);
	secondaryGroup.Artwork[0] = 0;
	layeredCompositor.SetGroups({secondaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFFFF0000);
	secondaryGroup.Artwork[0] = 0x80FFFFFF;
	layeredCompositor.SetGroups({secondaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFFFF4040);
	gbaLayers.GbaColorEffects = false; gbaLayers.SnesColorEffects = true;
	// A source-transparent object must use its own effect targets, with the
	// original native main layer as its secondary and PNG-alpha fallback.
	MetaSpriteFrame gbaHole(1, 1);
	gbaHole.GbaColorEffects = true;
	gbaHole.DecodedRows = {{96, 0, 0, 0, 1, 1, {0}}};
	auto& holePixel = gbaHole.Pixels[0];
	holePixel.GbaFallbackCaptured = true;
	holePixel.Effect = 1; holePixel.MainCoefficient = holePixel.SecondaryCoefficient = 8;
	holePixel.GbaCoverage.push_back({96, 0x7C00, true, false, true});
	MetaSpriteReplacement holeGroup = primaryGroup;
	holeGroup.Members[0].Pixels = {0}; holeGroup.Artwork = {0xFF00FF00};
	MetaSpriteCompositor holeCompositor; holeCompositor.SetGroups({holeGroup});
	uint32_t holeOriginal = 0xFF0000FF;
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, gbaHole)[0] == 0xFF007F7F);
	holePixel.GbaCoverage[0].EffectsEnabled = false;
	holePixel.GbaCoverage[0].SemiTransparent = true;
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, gbaHole)[0] == 0xFF007F7F);
	holeGroup.Artwork[0] = 0; holeCompositor.SetGroups({holeGroup});
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, gbaHole)[0] == holeOriginal);
	holeGroup.Artwork[0] = 0x8000FF00; holeCompositor.SetGroups({holeGroup});
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, gbaHole)[0] == 0xFF0040BF);
	// Reusing staging buffers must not carry replacement flags or coverage
	// pointers into a same-sized frame where the source group no longer matches.
	gbaHole.DecodedRows[0].Pixels[0] = 0xFFFF0000;
	holePixel.GbaCoverage.clear();
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, gbaHole)[0] == holeOriginal);
	gbaHole.DecodedRows[0].Pixels[0] = 0;
	MetaSpriteFrame snesHole(1, 1);
	snesHole.SnesColorEffects = true;
	snesHole.DecodedRows = {{96, 0, 0, 0, 1, 1, {0}}};
	auto& snesHolePixel = snesHole.Pixels[0];
	snesHolePixel.Brightness = 15; snesHolePixel.SpriteColor = 0x7C00;
	snesHolePixel.SnesFixedColor = 0x7C00; snesHolePixel.SnesHalfRequested = true;
	snesHolePixel.SnesFallbackCaptured = true;
	snesHolePixel.SnesCoverage.push_back({96, 1, 3, 4, true, false, true});
	holeGroup.Artwork[0] = 0xFF00FF00; holeCompositor.SetGroups({holeGroup});
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, snesHole)[0] == 0xFF007F7F);
	holeGroup.Artwork[0] = 0; holeCompositor.SetGroups({holeGroup});
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, snesHole)[0] == holeOriginal);
	holeGroup.Artwork[0] = 0x8000FF00; holeCompositor.SetGroups({holeGroup});
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, snesHole)[0] == 0xFF0040BF);
	snesHolePixel.SnesCoverage[0].Main = false; snesHolePixel.SnesCoverage[0].Sub = true;
	snesHolePixel.SpriteColor = 31; snesHolePixel.SecondaryColor = 0x7C00;
	snesHolePixel.SnesMathEnabled = true; snesHolePixel.SnesAddSubscreen = true;
	holeGroup.Artwork[0] = 0xFF00FF00; holeCompositor.SetGroups({holeGroup});
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, snesHole)[0] == 0xFF7F7F00);
	snesHole.DecodedRows[0].Pixels[0] = 0xFFFF0000;
	snesHolePixel.SnesCoverage.clear();
	assert(holeCompositor.Apply(&holeOriginal, 1, 1, snesHole)[0] == holeOriginal);
	layeredPixel.Brightness = 15; layeredPixel.SnesMathEnabled = true; layeredPixel.SnesHalf = true; layeredPixel.SnesUseSubscreen = true;
	primaryGroup.Artwork[0] = 0xFF00FF00; secondaryGroup.Artwork[0] = 0xFFFFFFFF;
	layeredCompositor.SetGroups({primaryGroup, secondaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF7FFF7F);
	layeredCompositor.SetGroups({secondaryGroup, primaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF7FFF7F);
	layeredPixel.SnesSubtract = true;
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF000000);
	MetaSpriteFrame snesBackground(1, 1);
	snesBackground.SnesColorEffects = true;
	auto& sb = snesBackground.Pixels[0];
	sb.SnesFallbackCaptured = true; sb.Brightness = 15;
	sb.SpriteColor = sb.SnesFallbackMainColor = 0x7C00;
	sb.BackgroundSample = {0, 0, 0, true};
	MetaBackgroundReplacement snesBg;
	snesBg.Id = 1; snesBg.Width = snesBg.Height = 1; snesBg.Artwork = {0xFF00FF00};
	MetaSpriteCompositor snesBgCompositor;
	snesBgCompositor.SetBackgroundLayers({snesBg});
	uint32_t snesBgOriginal = 0xFF0000FF;
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBackground)[0] == 0xFF00FF00);
	snesBg.Artwork[0] = 0x8000FF00; snesBgCompositor.SetBackgroundLayers({snesBg});
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBackground)[0] == 0xFF00807F);
	sb.VisibleSprite = 0; sb.SpriteColor = 31;
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBackground)[0] == 0xFFFF0000);
	snesBg.Layer = 1; snesBg.Artwork[0] = 0xFFFFFFFF;
	sb.SnesSubBackgroundSample = {0, 0, 1, true}; sb.SecondaryColor = 0x7C00;
	sb.SnesMathEnabled = sb.SnesUseSubscreen = sb.SnesAddSubscreen = sb.SnesHalf = true;
	snesBgCompositor.SetBackgroundLayers({snesBg});
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBackground)[0] == 0xFFFF7F7F);
	snesBg.MemoryConditions = {{0x1234, 7, 0xFF}}; snesBgCompositor.SetBackgroundLayers({snesBg});
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBackground) == &snesBgOriginal);
	snesBackground.MemorySnapshot[0x1234] = 7;
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBackground)[0] == 0xFFFF7F7F);
	MetaSpriteFrame snesBgHole(1, 1);
	MetaSpriteFrame snesLayerHole(1, 1);
	snesLayerHole.SnesColorEffects = true; snesLayerHole.SnesBackgrounds.resize(1);
	auto& slp = snesLayerHole.Pixels[0]; slp.Brightness = 15; slp.SnesFallbackCaptured = true; slp.SpriteColor = 0x7C00;
	auto& slb = snesLayerHole.SnesBackgrounds[0]; slb.Captured = true; slb.BackdropColor = 0x7C00;
	slb.Layers[0].Sample = {0, 0, 0, true}; slb.Layers[0].Priority = 3; slb.Layers[0].Main = true;
	snesBg.Layer = 0; snesBg.MemoryConditions.clear(); snesBg.Artwork[0] = 0xFF00FF00;
	snesBgCompositor.SetBackgroundLayers({snesBg});
	// A completely transparent native BG still supplies coordinates for its PNG.
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesLayerHole)[0] == 0xFF00FF00);
	snesBg.Artwork[0] = 0x8000FF00; snesBgCompositor.SetBackgroundLayers({snesBg});
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesLayerHole)[0] == 0xFF00807F);
	// A lower opaque native layer is the alpha fallback, not the backdrop.
	slb.Layers[1].Sample = {0, 0, 1, true}; slb.Layers[1].Priority = 1;
	slb.Layers[1].Opaque = slb.Layers[1].Main = true; slb.Layers[1].Color = 31;
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesLayerHole)[0] == 0xFF7F8000);
	// Native OBJ priority occludes a lower BG PNG; a higher BG PNG occludes OBJ.
	slp.VisibleSprite = 0; slp.SpriteColor = 31; slb.MainPriority = 4;
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesLayerHole)[0] == 0xFFFF0000);
	slb.Layers[0].Priority = 5;
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesLayerHole)[0] == 0xFF7F8000);
	slb.Layers[0].Main = false;
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesLayerHole)[0] == 0xFFFF0000);
	// The same transparent layer can participate exclusively on the subscreen.
	slb.Layers[0].Sub = true; slp.SnesMathEnabled = slp.SnesAddSubscreen = slp.SnesHalfRequested = true;
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesLayerHole)[0] == 0xFF7F403F);
	snesBgHole.SnesColorEffects = true;
	snesBgHole.DecodedRows = {{96, 0, 0, 0, 1, 1, {0}}};
	auto& sbh = snesBgHole.Pixels[0];
	sbh.Brightness = 15; sbh.SnesFallbackCaptured = true;
	sbh.SpriteColor = sbh.SnesFallbackMainColor = 0x7C00;
	sbh.BackgroundSample = {0, 0, 0, true};
	sbh.SnesCoverage.push_back({96, 0, 1, 0, true, false, false});
	snesBg.Layer = 0; snesBg.Artwork[0] = 0xFF00FF00; snesBg.MemoryConditions.clear();
	holeGroup.Artwork = {0};
	snesBgCompositor.SetReplacements({holeGroup}, {snesBg});
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBgHole)[0] == 0xFF00FF00);
	holeGroup.Artwork[0] = 0x80FFFFFF;
	snesBgCompositor.SetReplacements({holeGroup}, {snesBg});
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBgHole)[0] == 0xFF80FF80);
	sbh.BackgroundSample.Valid = false; sbh.SnesSubBackgroundSample = {0, 0, 0, true};
	sbh.VisibleSprite = 0; sbh.SpriteColor = 31; sbh.SecondaryColor = sbh.SnesFallbackSubColor = 0x7C00;
	sbh.SnesCoverage[0].Main = false; sbh.SnesCoverage[0].Sub = true;
	sbh.SnesFallbackSubPriority = 1;
	sbh.SnesMathEnabled = sbh.SnesUseSubscreen = sbh.SnesAddSubscreen = sbh.SnesHalf = sbh.SnesHalfRequested = true;
	holeGroup.Artwork[0] = 0; snesBgCompositor.SetReplacements({holeGroup}, {snesBg});
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBgHole)[0] == 0xFF7F7F00);
	holeGroup.Artwork[0] = 0x80FFFFFF; snesBgCompositor.SetReplacements({holeGroup}, {snesBg});
	assert(snesBgCompositor.Apply(&snesBgOriginal, 1, 1, snesBgHole)[0] == 0xFFBF7F40);
	for(int y = 0; y < 16; y++) {
		slices.DecodedRows.push_back({5, 0, 0, y, 32, 16, std::vector<uint32_t>(16, 0xFFFF0000), 0});
	}
	assert(MetaSpriteCompositor::BuildDecodedCandidates(slices).empty());
	for(int y = 15; y >= 0; y--) {
		slices.DecodedRows.push_back({5, 0, 0, y, 32, 16, std::vector<uint32_t>(16, 0xFF0000FF), 16});
	}
	auto wide = MetaSpriteCompositor::BuildDecodedCandidates(slices);
	assert(wide.size() == 1 && wide[0].Width == 32 && wide[0].Height == 16);
	assert(wide[0].Pixels[15] == 0xFFFF0000 && wide[0].Pixels[16] == 0xFF0000FF);
	slices.DecodedRows.push_back({5, 0, 0, 0, 32, 16, {0xFFFFFFFF}, 15});
	assert(MetaSpriteCompositor::BuildDecodedCandidates(slices).empty());
	MetaSpriteFrame decoded(256, 240);
	decoded.DecodedRows.push_back({12, 30, 40, 1, 2, 2, {0xFF112233, 0}});
	assert(MetaSpriteCompositor::BuildDecodedCandidates(decoded).empty());
	decoded.DecodedRows.push_back({12, 30, 40, 0, 2, 2, {0, 0xFF445566}});
	auto reconstructed = MetaSpriteCompositor::BuildDecodedCandidates(decoded);
	assert(reconstructed.size() == 1 && reconstructed[0].X == 30 && reconstructed[0].HardwareSprite == 12);
	assert(reconstructed[0].Pixels == std::vector<uint32_t>({0, 0xFF445566, 0xFF112233, 0}));
	MetaSpriteReplacement decodedGroup;
	decodedGroup.Id = 99; decodedGroup.ArtworkScale = 1;
	decodedGroup.ArtworkWidth = 2; decodedGroup.ArtworkHeight = 2;
	decodedGroup.Artwork.assign(4, 0xFF00FF00);
	decodedGroup.Members = reconstructed;
	decodedGroup.Members[0].X = 0; decodedGroup.Members[0].Y = 0;
	decoded.Pixels[40 * 256 + 31].VisibleSprite = 12;
	decoded.Pixels[41 * 256 + 30].VisibleSprite = -1; // Background priority wins.
	std::vector<uint32_t> decodedOriginal(256 * 240, 0xFF102030);
	MetaSpriteCompositor decodedCompositor;
	decodedCompositor.SetGroups({decodedGroup});
	auto decodedOutput = decodedCompositor.Apply(decodedOriginal.data(), 256, 240, decoded);
	assert(decodedOutput[40 * 256 + 31] == 0xFF00FF00);
	assert(decodedOutput[41 * 256 + 30] == 0xFF102030);
	decoded.GbaColorEffects = true;
	auto& visibleEffect = decoded.Pixels[40 * 256 + 31];
	visibleEffect.Effect = 3; visibleEffect.EffectsEnabled = true; visibleEffect.Brightness = 8;
	decodedOutput = decodedCompositor.Apply(decodedOriginal.data(), 256, 240, decoded);
	assert(decodedOutput[40 * 256 + 31] == 0xFF007F00);
	assert(decodedOutput[41 * 256 + 30] == 0xFF102030);
	visibleEffect.Effect = 1; visibleEffect.SecondaryBlendTarget = true;
	visibleEffect.SecondaryColor = 0x7C00; visibleEffect.MainCoefficient = 8; visibleEffect.SecondaryCoefficient = 8;
	decodedOutput = decodedCompositor.Apply(decodedOriginal.data(), 256, 240, decoded);
	assert(decodedOutput[40 * 256 + 31] == 0xFF007F7F);
	decoded.GbaColorEffects = false;
	decodedOutput = decodedCompositor.Apply(decodedOriginal.data(), 256, 240, decoded);
	assert(decodedOutput[40 * 256 + 31] == 0xFF00FF00);
	decoded.DecodedRows.push_back({12, 31, 40, 0, 2, 2, {0, 0xFF445566}});
	assert(MetaSpriteCompositor::BuildDecodedCandidates(decoded).empty());
	decoded.DecodedRows.back().X = 30;
	decoded.DecodedRows.back().Pixels[0] = 0xFFFFFFFF;
	assert(MetaSpriteCompositor::BuildDecodedCandidates(decoded).empty());
	MetaSpriteFrame frame(8, 8);
	MetaSpriteFrame highIdFrame(2, 1);
	highIdFrame.DecodedRows.push_back({96, 0, 0, 0, 2, 1, {0xFFFF0000, 0}});
	highIdFrame.Pixels[1].AllowReplacementSprite(96);
	assert(highIdFrame.Pixels[1].AllowsReplacementSprite(96) && !highIdFrame.Pixels[1].AllowsReplacementSprite(32));
	highIdFrame.Pixels[1].AllowReplacementSprite(127);
	assert(highIdFrame.Pixels[1].AllowsReplacementSprite(127) && !highIdFrame.Pixels[1].AllowsReplacementSprite(128) && !highIdFrame.Pixels[1].AllowsReplacementSprite(-1));
	MetaSpriteReplacement highIdGroup;
	highIdGroup.Id = 99; highIdGroup.ArtworkWidth = 2; highIdGroup.ArtworkHeight = 1;
	highIdGroup.Artwork = {0xFF00FF00, 0xFF00FF00};
	highIdGroup.Members.push_back({0, 0, 2, 1, -1, {0xFFFF0000, 0}});
	MetaSpriteCompositor highIdCompositor; highIdCompositor.SetGroups({highIdGroup});
	uint32_t highIdOriginal[] = {0xFF0000FF, 0xFF0000FF};
	auto highIdOutput = highIdCompositor.Apply(highIdOriginal, 2, 1, highIdFrame);
	assert(highIdOutput[0] == highIdOriginal[0] && highIdOutput[1] == 0xFF00FF00);
	for(int y = 0; y < 8; y++) {
		MetaSpriteRow row;
		row.Sprite = 3; row.Scanline = static_cast<uint16_t>(y); row.LowByte = 255; row.Palette[1] = 31;
		frame.Rows.push_back(row);
	}
	for(auto& pixel : frame.Pixels) { pixel.VisibleSprite = 3; pixel.BackgroundColor = 0x7C00; }
	frame.Pixels[1].VisibleSprite = -1;
	frame.Pixels[2].VisibleSprite = 12;
	MetaSpriteReplacement group;
	group.Id = 1; group.ArtworkWidth = 8; group.ArtworkHeight = 8;
	group.Members.push_back({0, 0, 8, 8, -1, std::vector<uint32_t>(64, 0xFFFF0000)});
	group.Artwork.assign(64, 0xFF00FF00);
	group.Artwork[3] = 0;
	group.Artwork[4] = 0x8000FF00;
	MetaSpriteCompositor compositor;
	compositor.SetGroups({group});
	std::vector<uint32_t> original(64, 0xFFFF0000);
	uint32_t* result = compositor.Apply(original.data(), 8, 8, frame);
	assert(result[0] == 0xFF00FF00);
	assert(result[1] == original[1] && result[2] == original[2]);
	assert(result[3] == 0xFF0000FF);
	assert(result[4] == 0xFF00807F);
	assert(original[0] == 0xFFFF0000);
	frame.Rows[0].LowByte &= ~(1 << 6);
	group.Members[0].Pixels[1] = 0;
	frame.Pixels[1].ReplacementEligibleSprites = uint64_t(1) << 3;
	compositor.SetGroups({group});
	result = compositor.Apply(original.data(), 8, 8, frame);
	assert(result[1] == 0xFF00FF00 && result[2] == original[2]);
	group.Artwork[1] = 0;
	compositor.SetGroups({group});
	assert(compositor.Apply(original.data(), 8, 8, frame)[1] == original[1]);
	frame.Pixels[1].ReplacementEligibleSprites = 0;
	frame.Rows[0].LowByte = 255; group.Members[0].Pixels[1] = 0xFFFF0000; group.Artwork[1] = 0xFF00FF00;
	MetaSpriteReplacement hdGroup = group;
	hdGroup.ArtworkScale = 2;
	hdGroup.ArtworkWidth = 16;
	hdGroup.ArtworkHeight = 16;
	hdGroup.Artwork.assign(256, 0xFF00FF00);
	hdGroup.Artwork[1] = 0xFFFFFF00;
	hdGroup.Artwork[16] = 0;
	hdGroup.Artwork[17] = 0x8000FF00;
	compositor.SetGroups({hdGroup});
	result = compositor.Apply(original.data(), 8, 8, frame);
	assert(compositor.GetOutputScale() == 2);
	assert(result[0] == 0xFF00FF00 && result[1] == 0xFFFFFF00);
	assert(result[16] == 0xFF0000FF && result[17] == 0xFF00807F);
	assert(result[2] == original[1] && result[3] == original[1]);
	assert(result[4] == original[2] && result[5] == original[2]);
	for(auto& pixel : frame.Pixels) { pixel.BackgroundSample.Valid = true; }
	MetaBackgroundReplacement backdrop;
	backdrop.Id = 4; backdrop.Width = backdrop.Height = 1; backdrop.WrapX = backdrop.WrapY = true;
	backdrop.Artwork = {0xFFFFFFFF};
	MetaSpriteCompositor combinedCompositor;
	combinedCompositor.SetReplacements({hdGroup}, {backdrop});
	assert(combinedCompositor.HasReplacements());
	auto combined = combinedCompositor.Apply(original.data(), 8, 8, frame, 0, 0, [](uint16_t) { return 0xFF0000FFu; });
	assert(combinedCompositor.GetOutputScale() == 2 && combined[16] == 0xFFFFFFFF && combined[17] == 0xFF7FFF7F);
	combinedCompositor.SetReplacements({}, {});
	assert(!combinedCompositor.HasReplacements() && combinedCompositor.Apply(original.data(), 8, 8, frame) == original.data());
	MetaSpriteFrame cropFrame(3, 2);
	for(auto& pixel : cropFrame.Pixels) { pixel.BackgroundSample.Valid = true; }
	MetaBackgroundReplacement cropLayer;
	cropLayer.Id = 9; cropLayer.Width = 6; cropLayer.Height = 4; cropLayer.Scale = 2; cropLayer.FollowScroll = false;
	for(int index = 0; index < 24; index++) { cropLayer.Artwork.push_back(0xFF000000u | index); }
	combinedCompositor.SetReplacements({}, {cropLayer});
	std::vector<uint32_t> cropOriginal(2, 0xFF0000FF);
	combined = combinedCompositor.Apply(cropOriginal.data(), 2, 1, cropFrame, 1, 1, [](uint16_t) { return 0xFF0000FFu; });
	assert(combinedCompositor.GetOutputScale() == 2);
	assert(combined[0] == cropLayer.Artwork[14] && combined[3] == cropLayer.Artwork[17] && combined[4] == cropLayer.Artwork[20]);
	for(int y = 0; y < 2; y++) { for(int x = 0; x < 3; x++) {
		cropFrame.Pixels[y * 3 + x].BackgroundSample.X = x + 10;
		cropFrame.Pixels[y * 3 + x].BackgroundSample.Y = y + 20;
	} }
	cropLayer.FollowScroll = true; cropLayer.WrapX = cropLayer.WrapY = true;
	combinedCompositor.SetReplacements({}, {cropLayer});
	combined = combinedCompositor.Apply(cropOriginal.data(), 2, 1, cropFrame, 1, 1, [](uint16_t) { return 0xFF0000FFu; });
	assert(combined[0] == cropLayer.Artwork[16] && combined[2] == cropLayer.Artwork[12] && combined[4] == cropLayer.Artwork[22]);
	std::vector<uint32_t> replacementFallback;
	assert(MetaBackgroundCompositor::BuildFallback(frame, {backdrop}, 2, [](uint16_t) { return 0xFF0000FFu; }, replacementFallback));
	result = compositor.Apply(original.data(), 8, 8, frame, 0, 0, {}, &replacementFallback);
	assert(result[16] == 0xFFFFFFFF && result[17] == 0xFF7FFF7F);
	assert(result[0] == 0xFF00FF00 && result[2] == original[1]);
	replacementFallback.pop_back();
	result = compositor.Apply(original.data(), 8, 8, frame, 0, 0, {}, &replacementFallback);
	assert(result[16] == 0xFF0000FF); // Invalid dimensions retain the native fallback.
	// Cropping moves the native visibility mask and the HD samples together.
	std::vector<uint32_t> cropped(7 * 8, 0xFFFF0000);
	result = compositor.Apply(cropped.data(), 7, 8, frame, 1, 0);
	assert(result[0] == cropped[0] && result[1] == cropped[0]);
	assert(result[4] == 0xFF00FF00);
	compositor.SetGroups({group});
	auto candidates = MetaSpriteCompositor::BuildGameboyCandidates(frame);
	assert(candidates.size() == 1 && candidates[0].HardwareSprite == 3);
	frame.Rows.pop_back();
	assert(MetaSpriteCompositor::BuildGameboyCandidates(frame).empty());
	result = compositor.Apply(original.data(), 8, 8, frame);
	assert(result[0] == original[0]);
	compositor.SetGroups({});
	assert(compositor.Apply(original.data(), 8, 8, frame) == original.data());
	assert(compositor.GetOutputScale() == 1);
	if(argc > 1) {
		std::ifstream file(argv[1], std::ios::binary);
		assert(file.good());
		std::vector<uint8_t> packet((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		auto transferred = MetaSpritePacket(packet.data(), packet.size()).Parse();
		assert(transferred.size() == 1 && transferred[0].Id == 22 && transferred[0].Artwork[0] == 0xFF00FF00);
		MetaSpriteRow lastRow = frame.Rows.front();
		lastRow.Scanline = 7;
		frame.Rows.push_back(lastRow);
		compositor.SetGroups(transferred);
		result = compositor.Apply(original.data(), 8, 8, frame);
		assert(result[0] == 0xFF00FF00 && result[1] == original[1] && result[2] == original[2]);
		for(size_t length = 0; length < packet.size(); length++) {
			bool rejected = false;
			try { MetaSpritePacket(packet.data(), length).Parse(); } catch(const std::runtime_error&) { rejected = true; }
			assert(rejected);
		}
		packet.push_back(0);
		bool rejected = false;
		try { MetaSpritePacket(packet.data(), packet.size()).Parse(); } catch(const std::runtime_error&) { rejected = true; }
		assert(rejected);
		std::cout << "PASS: managed-to-native enabled-group transfer and rejection of every truncated packet and trailing data.\n";
		std::ifstream conditionFile(std::string(argv[1]) + ".conditions", std::ios::binary);
		assert(conditionFile.good());
		std::vector<uint8_t> conditionPacket((std::istreambuf_iterator<char>(conditionFile)), std::istreambuf_iterator<char>());
		auto conditioned = MetaSpritePacket(conditionPacket.data(), conditionPacket.size()).Parse();
		assert(conditioned.size() == 1 && conditioned[0].MemoryConditions.size() == 2);
		assert(conditioned[0].MemoryConditions[0].Address == 0x1234 && conditioned[0].MemoryConditions[0].Value == 0xAF && conditioned[0].MemoryConditions[0].Mask == 0xF0);
		compositor.SetGroups(conditioned);
		frame.MemorySnapshot = {{0x1234u, uint8_t(0xA7)}, {0x5678u, uint8_t(3)}};
		assert(compositor.Apply(original.data(), 8, 8, frame)[0] == 0xFF00FF00);
		frame.MemorySnapshot[0x5678] = 4;
		assert(compositor.Apply(original.data(), 8, 8, frame)[0] == original[0]);
		for(size_t length = 0; length < conditionPacket.size(); length++) {
			bool invalid = false;
			try { MetaSpritePacket(conditionPacket.data(), length).Parse(); } catch(const std::runtime_error&) { invalid = true; }
			assert(invalid);
		}
		// A four-byte mask field may not overflow the native byte representation.
		conditionPacket[conditionPacket.size() - 3] = 1;
		bool invalid = false;
		try { MetaSpritePacket(conditionPacket.data(), conditionPacket.size()).Parse(); } catch(const std::runtime_error&) { invalid = true; }
		assert(invalid);
		std::cout << "PASS: MSP1 compatibility, MSP2 condition transfer and truncated/invalid condition rejection.\n";
		std::ifstream alternateFile(std::string(argv[1]) + ".alternates", std::ios::binary);
		assert(alternateFile.good());
		std::vector<uint8_t> alternatePacket((std::istreambuf_iterator<char>(alternateFile)), std::istreambuf_iterator<char>());
		auto alternates = MetaSpritePacket(alternatePacket.data(), alternatePacket.size()).Parse();
		assert(alternates.size() == 1 && alternates[0].AlternateArtwork.size() == 1);
		const auto& image = alternates[0].AlternateArtwork[0];
		assert(image.ArtworkScale == 2 && image.ArtworkWidth == 16 && image.ArtworkHeight == 16 && image.ArtworkOriginX == 1 && image.ArtworkOriginY == 2);
		compositor.SetGroups(alternates);
		compositor.CaptureMemorySnapshot(frame, [](uint32_t address, uint8_t& value) { assert(address == 0x2345); value = 7; return true; });
		assert(compositor.Apply(original.data(), 8, 8, frame)[0] == 0xFFFFFFFF && compositor.GetOutputScale() == 2);
		frame.MemorySnapshot[0x2345] = 8;
		assert(compositor.Apply(original.data(), 8, 8, frame)[0] == 0xFF00FF00 && compositor.GetOutputScale() == 1);
		for(size_t length = 0; length < alternatePacket.size(); length++) {
			bool alternateInvalid = false;
			try { MetaSpritePacket(alternatePacket.data(), length).Parse(); } catch(const std::runtime_error&) { alternateInvalid = true; }
			assert(alternateInvalid);
		}
		alternatePacket.push_back(0);
		invalid = false;
		try { MetaSpritePacket(alternatePacket.data(), alternatePacket.size()).Parse(); } catch(const std::runtime_error&) { invalid = true; }
		assert(invalid);
		std::cout << "PASS: MSP3 alternate transfer, selection/fallback, origins/scale, and truncation rejection.\n";
	}
	std::cout << "PASS: native graphic reconstruction, content matching, visibility/priority masks, transparent suppression, alpha blend, incomplete capture fallback, disable fallback.\n";
}

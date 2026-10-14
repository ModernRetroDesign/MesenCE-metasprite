#include "../../Core/Shared/Video/MetaSpriteCompositor.h"
#include "../../Core/Shared/Video/MetaSnesCapture.h"
#include "../../Core/Shared/Video/MetaSpritePacket.h"
#include "../../Core/Shared/Video/MetaBackgroundReplacement.h"
#include "../../Core/Shared/Video/MetaBackgroundPacket.h"
#include "../../Core/Shared/Video/BlendFilter.h"
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
	for(uint8_t op = 0; op < 6; op++) { for(uint8_t mask : std::initializer_list<uint8_t>{0, 15, 240, 255}) {
		for(uint8_t a : std::initializer_list<uint8_t>{0, 1, 127, 128, 255}) { for(uint8_t b : std::initializer_list<uint8_t>{0, 1, 127, 128, 255}) {
			uint8_t left = a & mask, right = b & mask;
			bool expected = op == 0 ? left == right : op == 1 ? left != right : op == 2 ? left > right : op == 3 ? left < right : op == 4 ? left >= right : left <= right;
			assert(MetaMemoryCondition::Matches({{7, b, mask, op}}, {{7, a}}) == expected);
			assert(!MetaMemoryCondition::Matches({{7, b, mask, op}}, {}));
		} }
	} }
	if(argc > 1 && argv[1][0] != '-') {
		std::ifstream spriteFile(std::string(argv[1]) + ".comparisons", std::ios::binary);
		std::vector<uint8_t> spritePacket((std::istreambuf_iterator<char>(spriteFile)), {});
		std::ifstream bgFile(std::string(argv[1]) + ".background.comparisons", std::ios::binary);
		std::vector<uint8_t> bgPacket((std::istreambuf_iterator<char>(bgFile)), {});
		auto groups = MetaSpritePacket(spritePacket.data(), spritePacket.size()).Parse();
		auto backgrounds = MetaBackgroundPacket(bgPacket.data(), bgPacket.size()).Parse();
		assert(groups.size() == 1 && backgrounds.size() == 1);
		for(uint8_t op = 0; op < 6; op++) { assert(groups[0].MemoryConditions[op].Comparison == op && backgrounds[0].MemoryConditions[op].Comparison == op); }
		MetaMemorySnapshot snapshot = {{0u, uint8_t(42)}, {1u, uint8_t(43)}, {2u, uint8_t(43)}, {3u, uint8_t(41)}, {4u, uint8_t(42)}, {5u, uint8_t(42)}, {6u, uint8_t(1)}};
		assert(MetaMemoryCondition::Matches(groups[0].MemoryConditions, snapshot));
		assert(MetaMemoryCondition::Matches(backgrounds[0].MemoryConditions, snapshot));
		assert(groups[0].SelectArtwork(snapshot).Artwork[0] == 0xFFFFFFFF);
		snapshot[6] = 0; assert(groups[0].SelectArtwork(snapshot).Artwork[0] == 0xFF00FF00);
		for(size_t size = 0; size < spritePacket.size(); size++) { bool rejected = false; try { MetaSpritePacket(spritePacket.data(), size).Parse(); } catch(const std::runtime_error&) { rejected = true; } assert(rejected); }
		for(size_t size = 0; size < bgPacket.size(); size++) { bool rejected = false; try { MetaBackgroundPacket(bgPacket.data(), size).Parse(); } catch(const std::runtime_error&) { rejected = true; } assert(rejected); }
		spritePacket[spritePacket.size() - 4] = 6; bgPacket[bgPacket.size() - 4] = 6;
		bool rejectedSprite = false, rejectedBackground = false;
		try { MetaSpritePacket(spritePacket.data(), spritePacket.size()).Parse(); } catch(const std::runtime_error&) { rejectedSprite = true; }
		try { MetaBackgroundPacket(bgPacket.data(), bgPacket.size()).Parse(); } catch(const std::runtime_error&) { rejectedBackground = true; }
		assert(rejectedSprite && rejectedBackground);
		std::ifstream scrollFile(std::string(argv[1]) + ".background.scroll", std::ios::binary);
		std::vector<uint8_t> scrollPacket((std::istreambuf_iterator<char>(scrollFile)), {});
		auto scrolling = MetaBackgroundPacket(scrollPacket.data(), scrollPacket.size()).Parse();
		assert(scrolling[0].ScrollSpeedX == 500 && scrolling[0].ScrollSpeedY == -1000 && scrolling[0].MemoryConditions[5].Comparison == 5);
		for(size_t size = 0; size < scrollPacket.size(); size++) { bool rejected = false; try { MetaBackgroundPacket(scrollPacket.data(), size).Parse(); } catch(const std::runtime_error&) { rejected = true; } assert(rejected); }
		MetaBackgroundReplacement image; image.Id = 1; image.Width = image.Height = 256; image.WrapX = image.WrapY = true;
		image.Artwork.resize(256 * 256); for(uint32_t p = 0; p < image.Artwork.size(); p++) { image.Artwork[p] = 0xFF000000 | p; }
		MetaBackgroundSample sample {2, 255, 0, true}; sample.ScrollX = 253; sample.ScrollY = 250; sample.ScrollWrapX = sample.ScrollWrapY = 256;
		image.ScrollSpeedX = 500; image.ScrollSpeedY = -1000; uint32_t color;
		assert(image.Sample(sample, 5, 5, 0, 0, 1, color) && color == (0xFF000000 | (11 * 256 + 131)));
		image.ScrollSpeedX = image.ScrollSpeedY = 0;
		assert(image.Sample(sample, 5, 5, 0, 0, 1, color) && color == (0xFF000000 | (5 * 256 + 5)));
		image.ScrollSpeedX = 2000; image.ScrollSpeedY = 1000;
		assert(image.Sample(sample, 5, 5, 0, 0, 1, color) && color == (0xFF000000 | (255 * 256 + 255)));
		image.FollowScroll = false; image.ScrollSpeedX = 2000;
		assert(image.Sample(sample, 5, 5, 0, 0, 1, color) && color == (0xFF000000 | (5 * 256 + 5)));
		std::ifstream rasterFile(std::string(argv[1]) + ".background.raster", std::ios::binary);
		std::vector<uint8_t> rasterPacket((std::istreambuf_iterator<char>(rasterFile)), {});
		auto rasterLayers = MetaBackgroundPacket(rasterPacket.data(), rasterPacket.size()).Parse();
		assert(rasterLayers[0].IgnoreRasterScroll && rasterLayers[0].ScrollReferenceScanline == 47 && rasterLayers[0].ScrollSpeedX == 500);
		for(size_t size = 0; size < rasterPacket.size(); size++) { bool rejected = false; try { MetaBackgroundPacket(rasterPacket.data(), size).Parse(); } catch(const std::runtime_error&) { rejected = true; } assert(rejected); }
		rasterPacket[52] = 240; bool rejectedRaster = false;
		try { MetaBackgroundPacket(rasterPacket.data(), rasterPacket.size()).Parse(); } catch(const std::runtime_error&) { rejectedRaster = true; } assert(rejectedRaster);
		std::ifstream hideFile(std::string(argv[1]) + ".background.hide", std::ios::binary);
		std::vector<uint8_t> hidePacket((std::istreambuf_iterator<char>(hideFile)), {});
		auto hidden = MetaBackgroundPacket(hidePacket.data(), hidePacket.size()).Parse();
		assert(hidden[0].HideOriginalLayer && hidden[0].IgnoreRasterScroll && hidden[0].ScrollSpeedX == 500);
		for(size_t size = 0; size < hidePacket.size(); size++) { bool rejected = false; try { MetaBackgroundPacket(hidePacket.data(), size).Parse(); } catch(const std::runtime_error&) { rejected = true; } assert(rejected); }
		hidePacket[56] = 2; bool rejectedHide = false; try { MetaBackgroundPacket(hidePacket.data(), hidePacket.size()).Parse(); } catch(const std::runtime_error&) { rejectedHide = true; } assert(rejectedHide);
		std::cout << "PASS: six comparison operators, masked/missing bytes, MSP4/MBG3 transfer, alternate selection, truncation and invalid operator rejection." << std::endl;
	}
	for(int path = 0; path < 3; path++) {
		MetaSpriteFrame frame(2, 2); frame.GbaColorEffects = path == 0; frame.SnesColorEffects = path != 0;
		if(path == 1) { frame.SnesBackgrounds.resize(4); }
		frame.MemorySnapshot = {{0x60u, uint8_t(3)}, {0x70u, uint8_t(2)}};
		for(int y = 0; y < 2; y++) { for(int x = 0; x < 2; x++) {
			auto& pixel = frame.Pixels[y * 2 + x]; pixel.Brightness = 15; pixel.GbaFallbackCaptured = pixel.SnesFallbackCaptured = true;
			pixel.BackgroundSample = {x + 80, y + 90, 0, true}; pixel.GbaBackgroundSamples[0] = pixel.BackgroundSample;
			pixel.GbaBackgroundLayers[0] = {31, 0, true}; pixel.SnesFallbackMainLayer = 0; pixel.SnesFallbackMainFlags = 2;
			pixel.SnesFallbackMainColor = 31;
			if(path == 1) {
				auto& screen = frame.SnesBackgrounds[y * 2 + x]; screen.Captured = true; screen.MainPriority = 2;
				screen.Layers[0].Sample = pixel.BackgroundSample; screen.Layers[0].Color = 31; screen.Layers[0].Priority = 2;
				screen.Layers[0].Main = screen.Layers[0].Opaque = true;
			}
		} }
		MetaBackgroundReplacement camera; camera.Id = 1; camera.Width = camera.Height = 8; camera.WrapX = camera.WrapY = true;
		camera.UseCpuCamera = true; camera.CameraX = {1, 0x60, 0, 256}; camera.CameraY = {1, 0x70, 0, 256};
		for(int y = 0; y < 8; y++) { for(int x = 0; x < 8; x++) { camera.Artwork.push_back(0xFF000000u | (x << 16) | (y << 8)); } }
		MetaSpriteCompositor compositor; compositor.SetReplacements({}, {camera}); uint32_t original[4] = {};
		const auto* output = compositor.Apply(original, 2, 2, frame);
		for(int y = 0; y < 2; y++) { for(int x = 0; x < 2; x++) { assert(output[y * 2 + x] == (0xFF000000u | ((x + 3) << 16) | ((y + 2) << 8))); } }
	}
	std::cout << "PASS: GBA, SNES layer composition and SNES fallback use CPU camera coordinates instead of hardware scroll." << std::endl;
	for(bool snes : {false, true}) {
		MetaSpriteFrame frame(4, 1); frame.GbaColorEffects = !snes; frame.SnesColorEffects = snes;
		if(snes) { frame.SnesBackgrounds.resize(4); }
		for(int x = 0; x < 4; x++) {
			auto& pixel = frame.Pixels[x]; pixel.GbaFallbackCaptured = true; pixel.SnesFallbackCaptured = true;
			pixel.Brightness = 15;
			pixel.GbaBackgroundSamples[0] = {x, 0, 0, true}; pixel.GbaBackgroundSamples[1] = {x, 0, 1, true};
			pixel.GbaBackgroundLayers[0] = {31, 0, true}; pixel.GbaBackgroundLayers[1] = {0x7C00, 1, true};
			if(snes) {
				auto& screen = frame.SnesBackgrounds[x]; screen.Captured = true; screen.MainPriority = 2;
				screen.Layers[0].Sample = {x, 0, 0, true}; screen.Layers[0].Color = 31; screen.Layers[0].Priority = 2; screen.Layers[0].Main = screen.Layers[0].Opaque = true;
				screen.Layers[1].Sample = {x, 0, 1, true}; screen.Layers[1].Color = 0x7C00; screen.Layers[1].Priority = 1; screen.Layers[1].Main = screen.Layers[1].Opaque = true;
			}
		}
		frame.MemorySnapshot = {{0u, uint8_t(1)}};
		MetaBackgroundReplacement layer; layer.Id = 1; layer.Width = 4; layer.Height = 1; layer.HideOriginalLayer = true;
		layer.Artwork = {0, 0x8000FF00, 0xFF00FF00, 0}; layer.MemoryConditions = {{0, 1, 255}};
		MetaSpriteCompositor compositor; compositor.SetReplacements({}, {layer});
		std::vector<uint32_t> original(4, 0xFFFF0000);
		const auto* output = compositor.Apply(original.data(), 4, 1, frame);
		assert(output[0] == 0xFF0000FF && output[1] == 0xFF00807F && output[2] == 0xFF00FF00 && output[3] == 0xFF0000FF);
		frame.MemorySnapshot[0] = 0; output = compositor.Apply(original.data(), 4, 1, frame);
		assert(output[0] == original[0] && output[1] == original[1]);
	}
	{
		MetaSpriteFrame frame(4, 1); frame.MemorySnapshot = {{0u, uint8_t(1)}};
		frame.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFFFFFF00}}}; frame.DecodedRows[0].ExpansionClass = 2;
		for(int x = 0; x < 4; x++) {
			auto& pixel = frame.Pixels[x]; pixel.BackgroundColor = 31; pixel.NativeBackdropColor = 0x7C00; pixel.NativeBackdropCaptured = true;
			pixel.BackgroundSample = {x, 0, 0, true}; pixel.ExpandedSpriteClasses = 1; pixel.ExpandedSpriteClassesWithoutBackground = 3; pixel.BackgroundOverlayOccludesClasses = 2;
		}
		frame.Pixels[0].ExpandedSpriteOccluder = 0;
		MetaBackgroundReplacement layer; layer.Id = 1; layer.Width = 4; layer.Height = 1; layer.HideOriginalLayer = true; layer.MemoryConditions = {{0, 1, 255}};
		layer.Artwork = {0, 0x8000FF00, 0xFF00FF00, 0};
		MetaSpriteCompositor compositor; compositor.SetReplacements({}, {layer});
		std::vector<uint32_t> original(4, 0xFFFF0000);
		const auto* output = compositor.Apply(original.data(), 4, 1, frame, 0, 0, [](uint16_t color) { return color == 31 ? 0xFFFF0000u : 0xFF0000FFu; });
		assert(output[0] == 0xFFFFFF00 && output[1] == 0xFF00807F && output[2] == 0xFF00FF00 && output[3] == 0xFF0000FF);
		frame.MemorySnapshot[0] = 0; output = compositor.Apply(original.data(), 4, 1, frame, 0, 0, [](uint16_t) { return 0xFFFF0000u; });
		assert(output[0] == original[0]);
	}
	std::cout << "PASS: hide original background reveals lower BG/native OBJ/backdrop, preserves PNG alpha and restores native artwork on condition mismatch." << std::endl;
	{
		std::ifstream file(std::string(argv[1]) + ".background.camera", std::ios::binary);
		std::vector<uint8_t> packet((std::istreambuf_iterator<char>(file)), {});
		auto parsed = MetaBackgroundPacket(packet.data(), packet.size()).Parse();
		assert(parsed[0].UseCpuCamera && parsed[0].CameraX.Format == 4 && parsed[0].CameraX.PageAddress == 0x70 && parsed[0].CameraX.PageSize == 240);
		for(size_t size = 0; size < packet.size(); size++) { bool rejected = false; try { MetaBackgroundPacket(packet.data(), size).Parse(); } catch(const std::runtime_error&) { rejected = true; } assert(rejected); }
		packet[64] = 5; bool rejected = false; try { MetaBackgroundPacket(packet.data(), packet.size()).Parse(); } catch(const std::runtime_error&) { rejected = true; } assert(rejected);
		MetaSpriteFrame frame(1, 1); frame.MemorySnapshot = {{0x60u, uint8_t(0)}, {0x70u, uint8_t(3)}};
		frame.Pixels[0].BackgroundSample = {0, 0, 0, true}; frame.Pixels[0].BackgroundSample.NesRasterScroll = true; frame.Pixels[0].BackgroundSample.ScrollWrapX = 512;
		MetaBackgroundReplacement camera; camera.Id = 1; camera.Width = 2048; camera.Height = 1; camera.UseCpuCamera = true; camera.IgnoreRasterScroll = true;
		camera.CameraX = {4, 0x60, 0x70, 256};
		for(int x = 0; x < 2048; x++) { camera.Artwork.push_back(0xFF000000u | x); }
		std::vector<MetaBackgroundReplacement> cameraLayers{camera}; MetaBackgroundView view(cameraLayers); view.PrepareRasterReferences(frame); uint32_t color;
		const auto& layer = *view.Layers[0];
		assert(view.Sample(layer, frame.Pixels[0].BackgroundSample, 0, 0, 0, 0, 1, color) && color == (0xFF000000u | 768));
		frame.MemorySnapshot[0x70] = 4; view.PrepareRasterReferences(frame);
		assert(view.Sample(layer, frame.Pixels[0].BackgroundSample, 0, 0, 0, 0, 1, color) && color == (0xFF000000u | 1024));
		frame.MemorySnapshot.erase(0x70); view.PrepareRasterReferences(frame);
		assert(!view.Sample(layer, frame.Pixels[0].BackgroundSample, 0, 0, 0, 0, 1, color));
		MetaSpriteCompositor compositor; compositor.SetReplacements({}, {camera});
		compositor.CaptureMemorySnapshot(frame, [](uint32_t, uint8_t& value) { value = 0; return true; });
		assert(frame.MemorySnapshot.size() == 2 && frame.MemorySnapshot.count(0x60) && frame.MemorySnapshot.count(0x70));
		std::cout << "PASS: MBG7 CPU camera transport, strict truncation/format validation, NES wrap/raster independence and configured-byte capture." << std::endl;
		std::ifstream signedFile(std::string(argv[1]) + ".background.camera.signed", std::ios::binary);
		std::vector<uint8_t> signedPacket((std::istreambuf_iterator<char>(signedFile)), {});
		auto signedLayers = MetaBackgroundPacket(signedPacket.data(), signedPacket.size()).Parse();
		assert(signedLayers[0].CameraX.SignedPage && !signedLayers[0].CameraY.SignedPage);
		for(size_t size = 0; size < signedPacket.size(); size++) { bool invalid = false; try { MetaBackgroundPacket(signedPacket.data(), size).Parse(); } catch(const std::runtime_error&) { invalid = true; } assert(invalid); }
		signedPacket[80] = 2; bool invalid = false; try { MetaBackgroundPacket(signedPacket.data(), signedPacket.size()).Parse(); } catch(const std::runtime_error&) { invalid = true; } assert(invalid);
		cameraLayers[0].CameraX.SignedPage = true;
		for(auto pair : {std::pair<int, int>{246, -10}, {255, -1}, {0, 0}}) {
			frame.MemorySnapshot[0x60] = (uint8_t)pair.first; frame.MemorySnapshot[0x70] = pair.second < 0 ? uint8_t(255) : uint8_t(0);
			view.PrepareRasterReferences(frame);
			assert(view.Sample(layer, frame.Pixels[0].BackgroundSample, 20, 0, 0, 0, 1, color) && color == (0xFF000000u | (uint32_t)(20 + pair.second)));
		}
		std::cout << "PASS: MBG8 signed pages retain FF:F6=-10, cross page zero and reject invalid/truncated flags." << std::endl;
	}
	{
		MetaSpriteFrame frame(2, 1); frame.BackgroundUnderlays.resize(2); frame.MemorySnapshot = {{0u, uint8_t(1)}};
		for(int x = 0; x < 2; x++) {
			frame.Pixels[x].BackgroundSample = {x, 0, 1, true}; frame.Pixels[x].NativeBackdropCaptured = true;
			frame.Pixels[x].BackgroundColor = 31; frame.BackgroundUnderlays[x].Sample = {x, 0, 0, true}; frame.BackgroundUnderlays[x].Color = 0x7C00;
		}
		MetaBackgroundReplacement upper; upper.Id = 1; upper.Layer = 1; upper.Width = 2; upper.Height = 1; upper.HideOriginalLayer = true;
		upper.Artwork = {0, 0x8000FF00}; upper.MemoryConditions = {{0, 1, 255}};
		MetaSpriteCompositor compositor; compositor.SetReplacements({}, {upper}); std::vector<uint32_t> original(2, 0xFFFF0000);
		auto convert = [](uint16_t color) { return color == 0x7C00 ? 0xFF0000FFu : color == 31 ? 0xFFFF0000u : 0xFF000000u; };
		const auto* output = compositor.Apply(original.data(), 2, 1, frame, 0, 0, convert);
		assert(output[0] == 0xFF0000FF && output[1] == 0xFF00807F);
		output = compositor.Apply(original.data(), 1, 1, frame, 1, 0, convert); assert(output[0] == 0xFF00807F);
		MetaBackgroundReplacement lower; lower.Id = 2; lower.Width = lower.Height = 1; lower.WrapX = true; lower.Artwork = {0xFFFFFFFF}; lower.MemoryConditions = upper.MemoryConditions;
		compositor.SetReplacements({}, {upper, lower}); output = compositor.Apply(original.data(), 2, 1, frame, 0, 0, convert);
		assert(output[0] == 0xFFFFFFFF && output[1] == 0xFF7FFF7F);
		lower.HideOriginalLayer = true; lower.Artwork[0] = 0; compositor.SetReplacements({}, {upper, lower}); output = compositor.Apply(original.data(), 2, 1, frame, 0, 0, convert);
		assert(output[0] == 0xFF000000 && output[1] == 0xFF008000);
		frame.MemorySnapshot[0] = 0; output = compositor.Apply(original.data(), 2, 1, frame, 0, 0, convert); assert(output[0] == original[0]);
		std::cout << "PASS: hidden foreground reveals retained lower BG/PNG, handles both hidden layers, cropped output and condition restoration." << std::endl;
		frame.MemorySnapshot[0] = 1; upper.Artwork.assign(2, 0);
		frame.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFFFF0000}}};
		frame.DecodedRows[0].ExpansionClass = 1;
		frame.Pixels[0].ExpandedSpriteClassesWithoutBackground = 3;
		frame.BackgroundUnderlays[0].SpriteEligibilityCaptured = true;
		frame.BackgroundUnderlays[0].SpriteEligibleClasses = 0;
		MetaSpriteReplacement sprite; sprite.ArtworkWidth = sprite.ArtworkHeight = 1; sprite.Artwork = {0xFF00FF00};
		sprite.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		compositor.SetReplacements({sprite}, {upper}); output = compositor.Apply(original.data(), 2, 1, frame, 0, 0, convert);
		assert(output[0] == 0xFF0000FF);
		frame.BackgroundUnderlays[0].SpriteEligibleClasses = 3;
		output = compositor.Apply(original.data(), 2, 1, frame, 0, 0, convert); assert(output[0] == 0xFF00FF00);
		std::cout << "PASS: hiding the GB window retains revealed BG priority for replacement sprites." << std::endl;
		frame.DecodedRows[0].ExpansionClass = 2;
		frame.BackgroundUnderlays[0].OverlayOccludesClasses = 2;
		lower.HideOriginalLayer = false; lower.Artwork[0] = 0x80FFFFFF;
		compositor.SetReplacements({sprite}, {upper, lower});
		output = compositor.Apply(original.data(), 2, 1, frame, 0, 0, convert); assert(output[0] == 0xFF80FF80);
		lower.Artwork[0] = 0; compositor.SetReplacements({sprite}, {upper, lower});
		output = compositor.Apply(original.data(), 2, 1, frame, 0, 0, convert); assert(output[0] == 0xFF00FF00);
		std::cout << "PASS: revealed GB background PNG alpha composites above behind-BG replacement sprites." << std::endl;
	}
	{
		MetaSpriteFrame frame(2, 1);
		frame.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFFFF0000}}, {1, 0, 0, 0, 1, 1, {0xFF0000FF}}};
		for(auto& row : frame.DecodedRows) { row.ExpansionClass = 1; }
		for(auto& pixel : frame.Pixels) { pixel.ExpandedSpriteClasses = 3; }
		frame.Pixels[0].VisibleSprite = 0; frame.Pixels[0].ExpandedSpriteOccluder = 0;
		MetaSpriteReplacement group; group.ArtworkWidth = 2; group.ArtworkHeight = 1;
		group.Artwork = {0, 0xFF00FF00}; group.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		MetaSpriteCompositor compositor; compositor.SetGroups({group});
		uint32_t original[] = {0xFFFF0000, 0xFF000000};
		auto convert = [](uint16_t) { return 0xFF000000u; };
		const auto* output = compositor.Apply(original, 2, 1, frame, 0, 0, convert);
		assert(output[0] == 0xFF0000FF && output[1] == 0xFF00FF00);
		group.Artwork[0] = 0x8000FF00; compositor.SetGroups({group});
		assert(compositor.Apply(original, 2, 1, frame, 0, 0, convert)[0] == 0xFF00807F);
		frame.DecodedRows[1].Pixels[0] = 0;
		assert(compositor.Apply(original, 2, 1, frame, 0, 0, convert)[0] == 0xFF008000);
		std::cout << "PASS: transparent and partial-alpha replacement reveals unreplaced lower OBJ using its native alpha and hardware order." << std::endl;
	}
	{
		MetaSpriteFrame frame(3, 1); frame.BackgroundUnderlays.resize(3);
		for(int x = 0; x < 3; x++) {
			auto& pair = frame.BackgroundUnderlays[x];
			pair.Sample = {x, 0, 0, true}; pair.Color = 1;
			pair.ForegroundSample = {x, 0, 1, true}; pair.ForegroundOpaque = false;
			frame.Pixels[x].BackgroundSample = pair.ForegroundSample;
			frame.Pixels[x].NativeBackdropCaptured = true;
		}
		frame.Pixels[1].VisibleSprite = frame.Pixels[2].VisibleSprite = 0;
		frame.Pixels[1].SpriteColor = frame.Pixels[2].SpriteColor = 2;
		frame.Pixels[1].NativeVisibleSpriteClass = 2; frame.Pixels[2].NativeVisibleSpriteClass = 1;
		MetaBackgroundReplacement upper; upper.Id = 1; upper.Layer = 1; upper.Width = 3; upper.Height = 1;
		upper.Artwork.assign(3, 0x8000FF00);
		MetaBackgroundCompositor backgrounds;
		auto convert = [](uint16_t color) { return color == 1 ? 0xFF0000FFu : color == 2 ? 0xFFFF0000u : 0xFF000000u; };
		std::vector<uint32_t> output(3, 0xFF0000FF);
		assert(backgrounds.Apply(frame, {upper}, 1, output, 0, 0, convert));
		assert(output[0] == 0xFF00807F && output[1] == 0xFF7F8000 && output[2] == 0xFFFF0000);
		upper.Artwork.assign(3, 0); output.assign(3, 0);
		assert(backgrounds.Apply(frame, {upper}, 1, output, 0, 0, convert));
		assert(output[0] == 0xFF0000FF && output[1] == 0xFFFF0000 && output[2] == 0xFFFF0000);
		std::cout << "PASS: transparent WS foreground texels accept PNG alpha and retain front/behind native OBJ priority." << std::endl;
	}
	{
		MetaSpriteFrame frame(4, 3);
		for(int y = 0; y < 3; y++) { for(int x = 0; x < 4; x++) {
			auto& sample = frame.Pixels[y * 4 + x].BackgroundSample;
			sample = {x + y * 4, y + 1, 0, true}; sample.ScrollX = y * 4; sample.ScrollY = 1;
			sample.ScrollWrapX = 16; sample.ScrollWrapY = 8; sample.NesRasterScroll = true;
		} }
		MetaBackgroundReplacement layer; layer.Id = 1; layer.Width = 16; layer.Height = 8; layer.ScrollReferenceScanline = 1;
		layer.Artwork.resize(128); for(uint32_t p = 0; p < 128; p++) { layer.Artwork[p] = 0xFF000000 | p; }
		std::vector<uint32_t> original(12, 0xFF000000);
		MetaSpriteCompositor compositor;
		for(bool ignore : {false, true}) {
			layer.IgnoreRasterScroll = ignore; compositor.SetReplacements({}, {layer});
			const auto* result = compositor.Apply(original.data(), 4, 3, frame, 0, 0, [](uint16_t) { return 0xFF000000; });
			for(int y = 0; y < 3; y++) { for(int x = 0; x < 4; x++) { assert(result[y * 4 + x] == (0xFF000000u | (uint32_t)((y + 1) * 16 + x + (ignore ? 4 : y * 4)))); } }
		}
		layer.ScrollSpeedX = 500; compositor.SetReplacements({}, {layer});
		const auto* result = compositor.Apply(original.data(), 2, 2, frame, 1, 1, [](uint16_t) { return 0xFF000000; });
		assert(result[0] == (0xFF000000 | (2 * 16 + 3)));
		layer.ScrollSpeedX = 1000; layer.ScrollReferenceScanline = 239;
		compositor.SetReplacements({}, {layer}); result = compositor.Apply(original.data(), 4, 3, frame, 0, 0, [](uint16_t) { return 0xFF000000; });
		assert(result[8] == (0xFF000000 | (3 * 16 + 8)));
		layer.ScrollReferenceScanline = 1; frame.Pixels[0].BackgroundSample.Valid = false;
		compositor.SetReplacements({}, {layer}); result = compositor.Apply(original.data(), 4, 3, frame, 0, 0, [](uint16_t) { return 0xFF000000; });
		assert(result[0] == original[0]);
		std::cout << "PASS: NES uniform reference scroll, native split tracking, multipliers, cropping, missing reference fallback and clipping." << std::endl;
	}
	{
		BlendFilter blend; blend.SetEnabled(true);
		uint32_t pixels[] = {0xFFFF0000, 0xFF0000FF};
		blend.ApplyFilter(pixels, 2, 10);
		assert(pixels[0] == 0xFFFF0000);
		pixels[0] = 0xFF00FF00; pixels[1] = 0xFFFFFF00;
		blend.ApplyFilter(pixels, 2, 11);
		assert(pixels[0] == 0xFF7F7F00 && pixels[1] == 0xFF7F7F7F);
		blend.ResetHistory(); pixels[0] = 0xFF0000FF;
		blend.ApplyFilter(pixels, 2, 12); assert(pixels[0] == 0xFF0000FF);
		blend.SetEnabled(false); blend.ApplyFilter(pixels, 2, 13);
		blend.SetEnabled(true); pixels[0] = 0xFFFF0000;
		blend.ApplyFilter(pixels, 2, 14); assert(pixels[0] == 0xFFFF0000);
		pixels[0] = 0xFF00FF00; blend.ApplyFilter(pixels, 2, 17);
		assert(pixels[0] == 0xFF00FF00);
		std::cout << "PASS: final-frame blend averages consecutive PNG colors and clears history on explicit reset, disable and discontinuity." << std::endl;
	}
	{
		MetaSpriteFrame logicalField(2, 2), mappedField(4, 4);
		logicalField.SnesColorEffects = true;
		logicalField.SnesBackgrounds.resize(4);
		logicalField.FrameNumber = 17; logicalField.MemorySnapshot = {{0x10u, uint8_t(7)}};
		logicalField.DecodedRows = {{3, 0, 0, 0, 1, 1, {0xFFFF0000}}};
		for(size_t index = 0; index < 4; index++) {
			auto& pixel = logicalField.Pixels[index];
			pixel.SnesFallbackCaptured = true; pixel.VisibleSprite = 3;
			pixel.SpriteColor = (uint16_t)(index + 1); pixel.SnesObjectsMain = true;
			logicalField.SnesBackgrounds[index].Captured = true;
			auto& fieldLayer = logicalField.SnesBackgrounds[index].Layers[0];
			fieldLayer.Main = fieldLayer.Opaque = true;
			fieldLayer.Sample.Valid = true; fieldLayer.Sample.X = (int)index * 3; fieldLayer.Sample.Y = (int)index * 5;
			fieldLayer.Color = (uint16_t)(index + 9);
		}
		for(bool odd : {false, true}) {
			assert(MetaSnesCapture::MapFieldWithoutMath(logicalField, mappedField, odd));
			assert(mappedField.FrameNumber == 17 && mappedField.MemorySnapshot.size() == 1 && mappedField.DecodedRows.size() == 1);
			for(int y = 0; y < 4; y++) { for(int x = 0; x < 4; x++) {
				bool active = (y & 1) == (odd ? 1 : 0);
				const auto& pixel = mappedField.Pixels[y * 4 + x];
				assert(pixel.SnesFallbackCaptured == active && pixel.SnesObjectsMain == active);
				assert(mappedField.SnesBackgrounds[y * 4 + x].Captured == active);
				assert(pixel.VisibleSprite == (active ? 3 : -1));
				if(active) { assert(pixel.SpriteColor == logicalField.Pixels[y / 2 * 2 + x / 2].SpriteColor); }
			} }
		}
		assert(MetaSnesCapture::MapBobWithoutMath(logicalField, mappedField, true, 0));
		assert(mappedField.SnesOutputOffsetY == 1);
		assert(mappedField.Pixels[0].SpriteColor == logicalField.Pixels[0].SpriteColor);
		assert(mappedField.Pixels[8].SpriteColor == logicalField.Pixels[0].SpriteColor);
		assert(mappedField.Pixels[12].SpriteColor == logicalField.Pixels[2].SpriteColor);
		for(int y = 0; y < 4; y++) { for(int x = 0; x < 4; x++) {
			int sourceRow = y == 0 ? 0 : (y - 1) / 2;
			const auto& fieldLayer = mappedField.SnesBackgrounds[y * 4 + x].Layers[0];
			const auto& originalLayer = logicalField.SnesBackgrounds[sourceRow * 2 + x / 2].Layers[0];
			assert(fieldLayer.Main && fieldLayer.Opaque && fieldLayer.Sample.Valid);
			assert(fieldLayer.Sample.X == originalLayer.Sample.X && fieldLayer.Sample.Y == originalLayer.Sample.Y && fieldLayer.Color == originalLayer.Color);
			assert(fieldLayer.Sample.PixelScaleX == 2 && fieldLayer.Sample.PixelScaleY == 2);
			assert(fieldLayer.Sample.PixelOffsetX == x % 2 && fieldLayer.Sample.PixelOffsetY == (y == 0 ? 0 : (y - 1) % 2));
		} }
		assert(MetaSnesCapture::MapBobWithoutMath(logicalField, mappedField, false, 0));
		assert(mappedField.SnesOutputOffsetY == 0 && mappedField.Pixels[8].SpriteColor == logicalField.Pixels[2].SpriteColor);
		logicalField.Pixels[0].SnesMathEnabled = true;
		assert(!MetaSnesCapture::MapFieldWithoutMath(logicalField, mappedField, false));
		std::cout << "PASS: SNES even/odd field mapping keeps only the current field's capture and preserves configured memory samples." << std::endl;
	}
	{
		MetaSpriteFrame logicalBackground(2, 2), bobBackground(4, 4);
		logicalBackground.SnesColorEffects = true; logicalBackground.SnesBackgrounds.resize(4);
		logicalBackground.MemorySnapshot = {{0x10u, uint8_t(7)}};
		for(int y = 0; y < 2; y++) { for(int x = 0; x < 2; x++) {
			auto& pixel = logicalBackground.Pixels[y * 2 + x];
			pixel.SnesFallbackCaptured = true; pixel.Brightness = 15;
			pixel.SnesFallbackMainColor = 31; pixel.SnesFallbackMainFlags = 1; pixel.SnesFallbackMainLayer = 0;
			auto& captured = logicalBackground.SnesBackgrounds[y * 2 + x];
			captured.Captured = true; captured.BackdropColor = 0x7C00; captured.MainPriority = 1;
			auto& fieldLayer = captured.Layers[0];
			fieldLayer.Main = fieldLayer.Opaque = true; fieldLayer.Priority = 1; fieldLayer.Color = 31;
			fieldLayer.Sample = {x, y, 0, true};
		} }
		MetaBackgroundReplacement background;
		background.Id = 1; background.Width = background.Height = 4; background.Scale = 2;
		background.MemoryConditions = {{0x10u, 7, 0xFF}};
		for(uint32_t i = 0; i < 16; i++) { background.Artwork.push_back(0xFF000000u | ((i + 1) * 0x00110307u)); }
		background.Artwork[0] = 0; background.Artwork[1] = 0x8000FF00;
		uint32_t originalBackground[16]; std::fill(std::begin(originalBackground), std::end(originalBackground), 0xFFFF0000);
		MetaSpriteCompositor backgroundCompositor; backgroundCompositor.SetBackgroundLayers({background});
		std::vector<uint32_t> previousBackground;
		BlendFilter backgroundBlend; backgroundBlend.SetEnabled(true);
		for(bool odd : {false, true}) {
			assert(MetaSnesCapture::MapBobWithoutMath(logicalBackground, bobBackground, odd, 0));
			const auto* outputBackground = backgroundCompositor.Apply(originalBackground, 4, 4, bobBackground);
			assert(backgroundCompositor.GetOutputScale() == 2);
			for(int y = 0; y < 4; y++) { for(int x = 0; x < 4; x++) {
				int artworkY = odd && y > 0 ? y - 1 : y;
				for(int sy = 0; sy < 2; sy++) { for(int sx = 0; sx < 2; sx++) {
					uint32_t artworkPixel = background.Artwork[artworkY * 4 + x];
					uint32_t expectedBackground = !(artworkPixel >> 24) ? 0xFFFF0000 : (artworkPixel >> 24) == 128 ? 0xFF7F8000 : artworkPixel;
					assert(outputBackground[(y * 2 + sy) * 8 + x * 2 + sx] == expectedBackground);
				} }
			} }
			std::vector<uint32_t> currentBackground(outputBackground, outputBackground + 64);
			std::vector<uint32_t> blendedBackground = currentBackground;
			backgroundBlend.ApplyFilter(blendedBackground.data(), 64, odd ? 31 : 30);
			for(size_t i = 0; i < 64; i++) {
				uint32_t expectedBlend = odd ? (((currentBackground[i] ^ previousBackground[i]) & 0xFFFEFEFEu) >> 1) + (currentBackground[i] & previousBackground[i]) : currentBackground[i];
				assert(blendedBackground[i] == expectedBlend);
			}
			previousBackground = std::move(currentBackground);
		}
		bobBackground.MemorySnapshot[0x10] = 8;
		logicalBackground.DecodedRows = {{0, 0, 1, 0, 1, 1, {0xFFFF0000}}};
		logicalBackground.DecodedRows[0].SnesPriorityCode = 0;
		logicalBackground.Pixels[2].SnesObjectSprite = 0;
		for(auto& pixel : logicalBackground.Pixels) { pixel.SnesObjectsMain = true; pixel.SnesObjectPriorities[0] = 3; }
		MetaSpriteReplacement foreground;
		foreground.ArtworkWidth = foreground.ArtworkHeight = 2; foreground.ArtworkScale = 2;
		foreground.Artwork.assign(4, 0xFF00FFFF);
		foreground.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		backgroundCompositor.SetGroups({foreground});
		for(bool odd : {false, true}) {
			assert(MetaSnesCapture::MapBobWithoutMath(logicalBackground, bobBackground, odd, 0));
			const auto* withForeground = backgroundCompositor.Apply(originalBackground, 4, 4, bobBackground);
			int firstRow = odd ? 3 : 2;
			for(int y = firstRow * 2; y < 8; y++) { for(int x = 0; x < 4; x++) { assert(withForeground[y * 8 + x] == 0xFF00FFFF); } }
			assert(withForeground[0] == 0xFFFF0000 && withForeground[2] == 0xFF7F8000);
		}
		backgroundCompositor.SetGroups({});
		logicalBackground.DecodedRows[0].Y = 0;
		logicalBackground.Pixels[2].SnesObjectSprite = -1; logicalBackground.Pixels[0].SnesObjectSprite = 0;
		backgroundCompositor.SetGroups({foreground});
		assert(MetaSnesCapture::MapBobWithoutMath(logicalBackground, bobBackground, true, 0));
		const auto* firstPictureArtwork = backgroundCompositor.Apply(originalBackground, 4, 4, bobBackground);
		for(int y = 0; y < 6; y++) { for(int x = 0; x < 4; x++) { assert(firstPictureArtwork[y * 8 + x] == 0xFF00FFFF); } }
		backgroundCompositor.SetGroups({});
		bobBackground.MemorySnapshot[0x10] = 8;
		const auto* suppressedBackground = backgroundCompositor.Apply(originalBackground, 4, 4, bobBackground);
		for(int i = 0; i < 16; i++) { assert(suppressedBackground[i] == originalBackground[i]); }
		std::cout << "PASS: Bob background PNGs preserve both field mappings, 2x subpixel artwork and configured CPU gating." << std::endl;
	}
	{
		MetaSpriteFrame canvas(4, 1); canvas.SnesColorEffects = true;
		canvas.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFFFF0000}}};
		canvas.DecodedRows[0].SnesPriorityCode = 0; canvas.DecodedRows[0].SnesPalette = 5; canvas.DecodedRows[0].SnesFetchOrder = 1;
		for(auto& pixel : canvas.Pixels) {
			pixel.SnesFallbackCaptured = true; pixel.Brightness = 15;
			pixel.SnesObjectsMain = true; pixel.SnesObjectPriorities[0] = 3;
			pixel.SnesFallbackMainColor = pixel.SnesFallbackSubColor = 0x7C00; pixel.SnesFallbackMainFlags = 2;
		}
		canvas.Pixels[0].SnesObjectSprite = 0; canvas.Pixels[0].SnesObjectPriorityCode = 0; canvas.Pixels[0].SnesObjectOrder = 1;
		canvas.Pixels[3].SnesObjectsMain = false;
		MetaSpriteReplacement group; group.ArtworkWidth = 4; group.ArtworkHeight = 1;
		group.Artwork.assign(4, 0xFF00FF00); group.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		uint32_t original[] = {0xFFFF0000, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF};
		MetaSpriteCompositor compositor; compositor.SetGroups({group});
		auto output = compositor.Apply(original, 4, 1, canvas);
		assert(output[0] == 0xFF00FF00 && output[1] == 0xFF00FF00 && output[2] == 0xFF00FF00 && output[3] == original[3]);
		for(auto& pixel : canvas.Pixels) { pixel.SnesObjectPriorities[0] = 1; }
		assert(compositor.Apply(original, 4, 1, canvas)[1] == 0xFF0000FF);
		for(auto& pixel : canvas.Pixels) { pixel.SnesObjectPriorities[0] = 3; pixel.SnesObjectMath = true; pixel.SnesFixedColor = 31; }
		assert(compositor.Apply(original, 4, 1, canvas)[1] == 0xFFFFFF00);
		for(auto& pixel : canvas.Pixels) { pixel.SnesObjectMath = false; }
		group.Artwork.assign(4, 0x8000FF00); compositor.SetGroups({group});
		assert(compositor.Apply(original, 4, 1, canvas)[1] == 0xFF00807F);
		group.Artwork.assign(4, 0); compositor.SetGroups({group});
		assert(compositor.Apply(original, 4, 1, canvas)[0] == 0xFF0000FF);
		std::cout << "PASS: SNES full canvas uses destination windows, BG priority, color math, PNG alpha and source removal." << std::endl;
	}
	{
		MetaSpriteFrame even(2, 1), odd(2, 1), woven(4, 2);
		even.FrameNumber = 20; odd.FrameNumber = 21;
		even.MemorySnapshot = {{0x10u, uint8_t(3)}}; odd.MemorySnapshot = {{0x10u, uint8_t(7)}};
		even.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFFFF0000}}};
		odd.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFF0000FF}}};
		for(auto* field : {&even, &odd}) {
			field->SnesColorEffects = true;
			field->DecodedRows[0].SnesPriorityCode = 0;
			for(auto& pixel : field->Pixels) {
				pixel.SnesFallbackCaptured = true; pixel.Brightness = 15;
				pixel.SnesObjectsMain = true; pixel.SnesObjectPriorities[0] = 3;
				pixel.SnesFallbackMainColor = 0x7C00;
			}
			field->Pixels[0].VisibleSprite = field->Pixels[0].SnesObjectSprite = 0;
		}
		assert(MetaSnesCapture::WeaveFieldsWithoutMath(even, odd, woven));
		assert(woven.FrameNumber == 21 && woven.MemorySnapshot == odd.MemorySnapshot);
		assert(woven.DecodedRows.size() == 2 && woven.DecodedRows[1].Sprite == 128);
		assert(woven.Pixels[0].SnesField == 1 && woven.Pixels[4].SnesField == 2);
		assert(woven.Pixels[4].SnesObjectSprite == 128 && woven.Pixels[4].VisibleSprite == 128);
		MetaSpriteReplacement first, second;
		first.ArtworkWidth = second.ArtworkWidth = 2; first.ArtworkHeight = second.ArtworkHeight = 1;
		first.Artwork.assign(2, 0xFF00FF00); second.Artwork.assign(2, 0xFFFFFF00);
		first.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		second.Members = {{0, 0, 1, 1, -1, {0xFF0000FF}}};
		uint32_t original[8] = {};
		MetaSpriteCompositor compositor; compositor.SetGroups({first, second});
		const auto* output = compositor.Apply(original, 4, 2, woven);
		for(int x = 0; x < 4; x++) { assert(output[x] == 0xFF00FF00 && output[4 + x] == 0xFFFFFF00); }
		// A moving odd-field pose must not shift the retained even-field PNG.
		odd.DecodedRows[0].X = 1;
		odd.Pixels[0].VisibleSprite = odd.Pixels[0].SnesObjectSprite = -1;
		odd.Pixels[1].VisibleSprite = odd.Pixels[1].SnesObjectSprite = 0;
		odd.FrameNumber = 22; even.FrameNumber = 23;
		std::fill(std::begin(original), std::end(original), 0xFF0000FF);
		assert(MetaSnesCapture::WeaveFieldsWithoutMath(even, odd, woven));
		assert(woven.FrameNumber == 23 && woven.MemorySnapshot == even.MemorySnapshot);
		output = compositor.Apply(original, 4, 2, woven);
		for(int x = 0; x < 4; x++) { assert(output[x] == 0xFF00FF00 && output[4 + x] == (x < 2 ? 0xFF0000FF : 0xFFFFFF00)); }
		odd.DecodedRows[0].X = 0;
		odd.Pixels[0].VisibleSprite = odd.Pixels[0].SnesObjectSprite = 0;
		odd.Pixels[1].VisibleSprite = odd.Pixels[1].SnesObjectSprite = -1;
		assert(MetaSnesCapture::WeaveFieldsWithoutMath(even, odd, woven));
		// A group assembled from one member in each field must never match.
		first.Members.push_back(second.Members[0]); compositor.SetGroups({first});
		assert(compositor.Apply(original, 4, 2, woven)[0] != 0xFF00FF00);
		odd.DecodedRows[0].Sprite = 128;
		assert(!MetaSnesCapture::WeaveFieldsWithoutMath(even, odd, woven));
		assert(woven.DecodedRows[1].Sprite == 128 && woven.FrameNumber == 23);
		odd.DecodedRows[0].Sprite = 0;
		odd.Pixels[0].SnesField = 2;
		assert(!MetaSnesCapture::WeaveFieldsWithoutMath(even, odd, woven));
		odd.Pixels[0].SnesField = 0;
		odd.Pixels[0].SnesMathEnabled = true;
		assert(!MetaSnesCapture::WeaveFieldsWithoutMath(even, odd, woven));
		std::cout << "PASS: woven SNES fields retain independent poses, hardware IDs and configured memory samples; recognition never mixes fields." << std::endl;
	}
	{
		MetaSpriteFrame canvas(4, 1); canvas.GbaColorEffects = true;
		canvas.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFFFF0000}}}; canvas.DecodedRows[0].GbaPriority = 0;
		for(auto& pixel : canvas.Pixels) {
			pixel.GbaObjectsEnabled = true; pixel.GbaBackdropColor = 0x7C00;
			pixel.GbaBackgroundSamples[0] = {0, 0, 0, true}; pixel.GbaBackgroundLayers[0] = {0x7C00, 1, true};
			pixel.GbaWindowEffectsEnabled = true; pixel.GbaBlendMainMask = 16;
		}
		canvas.Pixels[0].GbaObjectSprite = 0; canvas.Pixels[0].GbaObjectPriority = 0;
		canvas.Pixels[3].GbaObjectsEnabled = false;
		MetaSpriteReplacement group; group.ArtworkWidth = 4; group.ArtworkHeight = 1;
		group.Artwork.assign(4, 0xFF00FF00); group.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		uint32_t original[] = {0xFFFF0000, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF};
		MetaSpriteCompositor compositor; compositor.SetGroups({group});
		auto output = compositor.Apply(original, 4, 1, canvas);
		assert(output[0] == 0xFF00FF00 && output[1] == 0xFF00FF00 && output[2] == 0xFF00FF00 && output[3] == original[3]);
		canvas.DecodedRows[0].GbaPriority = 2;
		assert(compositor.Apply(original, 4, 1, canvas)[1] == 0xFF0000FF);
		canvas.DecodedRows[0].GbaPriority = 0;
		for(auto& pixel : canvas.Pixels) { pixel.Effect = 3; pixel.Brightness = 8; }
		assert(compositor.Apply(original, 4, 1, canvas)[1] == 0xFF007F00);
		for(auto& pixel : canvas.Pixels) { pixel.Effect = 0; }
		group.Artwork.assign(4, 0x8000FF00); compositor.SetGroups({group});
		assert(compositor.Apply(original, 4, 1, canvas)[1] == 0xFF00807F);
		group.Artwork.assign(4, 0); compositor.SetGroups({group});
		assert(compositor.Apply(original, 4, 1, canvas)[0] == 0xFF0000FF);
		for(auto& pixel : canvas.Pixels) { pixel.Effect = 3; pixel.GbaBlendMainMask = 49; }
		assert(compositor.Apply(original, 4, 1, canvas)[0] == 0xFF00007B);
		std::cout << "PASS: GBA full canvas uses destination OBJ windows, BG priority, brightness, PNG alpha and source removal." << std::endl;
	}
	{
		MetaSpriteFrame metadata(1, 1);
		// The background must participate in fallback selection after removing a matched OBJ.
		MetaSpriteFrame layered(4, 1); layered.GbaColorEffects = true;
		layered.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFFFF0000}}}; layered.DecodedRows[0].GbaPriority = 0;
		for(auto& pixel : layered.Pixels) {
			pixel.GbaObjectsEnabled = true; pixel.GbaBackdropColor = 0x7C00;
			pixel.GbaBackgroundSamples[0] = {0, 0, 0, true};
			pixel.GbaBackgroundLayers[0] = {0x7C1F, 1, true};
			pixel.GbaNativeObjects.push_back({2, 0x03FF, 3, false});
		}
		layered.Pixels[0].GbaObjectSprite = 0; layered.Pixels[0].GbaObjectPriority = 0;
		layered.Pixels[0].GbaNativeObjects.push_back({0, 31, 0, false});
		MetaSpriteReplacement layerArtwork; layerArtwork.ArtworkWidth = 4; layerArtwork.ArtworkHeight = 1;
		layerArtwork.Artwork = {0, 0x8000FF00, 0xFF00FF00, 0};
		layerArtwork.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		MetaSpriteCompositor layerCompositor; layerCompositor.SetGroups({layerArtwork});
		uint32_t layeredNative[] = {0xFFFF0000, 0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF};
		for(uint8_t priority = 0; priority < 4; priority++) {
			for(auto& pixel : layered.Pixels) { pixel.GbaNativeObjects[0].Priority = priority; }
			auto result = layerCompositor.Apply(layeredNative, 4, 1, layered);
			uint32_t fallback = priority <= 1 ? 0xFFFFFF00 : 0xFFFF00FF;
			assert(result[0] == fallback && result[3] == fallback && result[2] == 0xFF00FF00);
			assert(result[1] == (priority <= 1 ? 0xFF7FFF00 : 0xFF7F807F));
		}
		std::cout << "PASS: GBA PNG alpha reselects background versus hidden native OBJ across all four priorities, including ties." << std::endl;
		MetaSpriteFrame mixed(6, 1); mixed.GbaColorEffects = true;
		mixed.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFFFF0000}}, {1, 2, 0, 0, 1, 1, {0xFFFF0000}}};
		mixed.DecodedRows[0].GbaPriority = 0; mixed.DecodedRows[1].GbaPriority = 3;
		for(auto& pixel : mixed.Pixels) {
			pixel.GbaObjectsEnabled = true; pixel.GbaBackdropColor = 0x7C00;
			pixel.GbaBackgroundSamples[0] = {0, 0, 0, true}; pixel.GbaBackgroundLayers[0] = {0x7C1F, 1, true};
		}
		mixed.Pixels[0].GbaObjectSprite = 0; mixed.Pixels[0].GbaObjectPriority = 0;
		MetaSpriteReplacement mixedGroup; mixedGroup.ArtworkWidth = 6; mixedGroup.ArtworkHeight = 1;
		mixedGroup.Artwork = {0, 0x8000FF00, 0xFF00FF00, 0xFF00FF00, 0xFF00FF00, 0xFF00FF00};
		mixedGroup.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}, {2, 0, 1, 1, -1, {0xFFFF0000}}};
		uint32_t mixedNative[] = {0xFFFF0000, 0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF};
		layerCompositor.SetGroups({mixedGroup});
		auto mixedOutput = layerCompositor.Apply(mixedNative, 6, 1, mixed);
		assert(mixedOutput[0] == 0xFFFF00FF && mixedOutput[1] == 0xFF7F807F);
		for(int x = 2; x < 6; x++) { assert(mixedOutput[x] == 0xFFFF00FF); }
		std::cout << "PASS: mixed-priority GBA groups preserve member rectangles and nearest-member margin priority without source-alpha masking." << std::endl;
		metadata.DecodedRows = {{0, 0, 0, 0, 1, 1, {0xFFFF0000}}};
		metadata.Pixels[0].VisibleSprite = 0;
		MetaSpriteReplacement artwork;
		artwork.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		artwork.ArtworkWidth = artwork.ArtworkHeight = 1; artwork.Artwork = {0xFF00FF00};
		MetaSpriteCompositor compositor; compositor.SetGroups({artwork});
		uint32_t original = 0xFFFF0000;
		for(uint8_t priority = 0; priority < 4; priority++) { for(bool semi : {false, true}) {
			metadata.DecodedRows[0].GbaPriority = priority;
			metadata.DecodedRows[0].GbaSemiTransparent = semi;
			metadata.DecodedRows[0].SnesPriorityCode = priority;
			metadata.DecodedRows[0].SnesPalette = 5; metadata.DecodedRows[0].SnesFetchOrder = 42;
			auto candidates = MetaSpriteCompositor::BuildDecodedCandidates(metadata);
			assert(candidates.size() == 1 && candidates[0].GbaPriority == priority && candidates[0].GbaSemiTransparent == semi);
			assert(candidates[0].SnesPriorityCode == priority && candidates[0].SnesPalette == 5 && candidates[0].SnesFetchOrder == 42);
			assert(compositor.Apply(&original, 1, 1, metadata)[0] == 0xFF00FF00);
		} }
		std::cout << "PASS: GBA decoded priority/blend metadata survives reconstruction without changing content recognition." << std::endl;
	}
	{
		// Front source has a hole, but replacement PNG alpha controls overlap.
		MetaSpriteFrame overlap(4, 1);
		overlap.DecodedRows = {{0, 0, 0, 0, 2, 1, {0xFFFF0000, 0}}, {1, 0, 0, 0, 2, 1, {0xFF0000FF, 0xFF0000FF}}};
		for(auto& row : overlap.DecodedRows) { row.ExpansionClass = 1; }
		for(auto& pixel : overlap.Pixels) { pixel.ExpandedSpriteClasses = 3; }
		overlap.Pixels[0].VisibleSprite = 0; overlap.Pixels[0].AllowReplacementSprite(0); overlap.Pixels[0].ExpandedSpriteOccluder = 0;
		overlap.Pixels[1].VisibleSprite = 1; overlap.Pixels[1].AllowReplacementSprite(0); overlap.Pixels[1].AllowReplacementSprite(1); overlap.Pixels[1].ExpandedSpriteOccluder = 1;
		MetaSpriteReplacement front, back;
		front.ArtworkWidth = back.ArtworkWidth = 4; front.ArtworkHeight = back.ArtworkHeight = 1;
		back.Artwork.assign(4, 0xFFFFFF00);
		front.Members = {{0, 0, 2, 1, -1, {0xFFFF0000, 0}}};
		back.Members = {{0, 0, 2, 1, -1, {0xFF0000FF, 0xFF0000FF}}};
		uint32_t original[] = {0xFFFF0000, 0xFF0000FF, 0xFF000000, 0xFF000000};
		for(int system = 0; system < 3; system++) {
			overlap.GbaColorEffects = system == 1; overlap.SnesColorEffects = system == 2;
			for(auto& row : overlap.DecodedRows) { row.GbaPriority = 0; }
			for(auto& pixel : overlap.Pixels) { pixel.GbaObjectsEnabled = true; pixel.GbaObjectSprite = pixel.VisibleSprite; pixel.GbaObjectPriority = 0; pixel.GbaBackdropColor = 0x7C00; }
			// Tile fetch counts can vary by scanline and must not reorder OAM.
			for(auto& row : overlap.DecodedRows) { row.SnesPriorityCode = 0; row.SnesFetchOrder = (uint16_t)(row.Sprite ? 99 : 1); }
			for(auto& pixel : overlap.Pixels) {
				pixel.SnesFallbackCaptured = true; pixel.Brightness = 15; pixel.SnesObjectsMain = true; pixel.SnesObjectPriorities[0] = 3;
				pixel.SnesObjectSprite = pixel.VisibleSprite; pixel.SnesFallbackMainColor = pixel.SnesFallbackSubColor = 0x7C00;
			}
			overlap.Pixels[1].SnesFirstObject = 1;
		for(uint32_t alpha : {255u, 128u, 0u}) {
			front.Artwork.assign(4, (alpha << 24) | 0x0000FF00);
			uint32_t expected = 0xFF00FF00 | ((255 - alpha) << 16);
			for(bool reverse : {false, true}) {
				MetaSpriteCompositor compositor; compositor.SetGroups(reverse ? std::vector<MetaSpriteReplacement>{back, front} : std::vector<MetaSpriteReplacement>{front, back});
				const auto* output = compositor.Apply(original, 4, 1, overlap);
				for(int x = 0; x < 4; x++) { assert(output[x] == (system == 2 && x == 1 ? 0xFFFFFF00 : expected)); }
			}
		}
		}
		std::cout << "PASS: shared and GBA crossing replacement canvases use PNG alpha and hardware order inside source bounds and margins, independent of pack order." << std::endl;
	}
	{
		// Oversized PNG margins use destination rules, not original coverage.
		MetaSpriteFrame frame(4, 1);
		frame.DecodedRows = {{5, 1, 0, 0, 1, 1, {0xFFFF0000}}};
		frame.DecodedRows[0].ExpansionClass = 2;
		for(int x = 0; x < 4; x++) {
			frame.Pixels[x].ExpandedSpriteClasses = 3;
			frame.Pixels[x].BackgroundColor = 0x7C00;
			frame.Pixels[x].BackgroundSample = {x, 0, 0, true};
			frame.Pixels[x].BackgroundOverlayOccludesClasses = 2;
		}
		frame.Pixels[1].VisibleSprite = 5;
		MetaSpriteReplacement group;
		group.ArtworkWidth = 4; group.ArtworkHeight = 1; group.ArtworkOriginX = 1;
		group.Artwork = {0xFF00FF00, 0xFF00FF00, 0xFF00FF00, 0x8000FF00};
		group.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		MetaBackgroundReplacement bg; bg.Id = 1; bg.Width = 4; bg.Height = 1;
		bg.Artwork = {0, 0x80FFFFFF, 0xFFFFFFFF, 0x80FFFFFF};
		MetaSpriteCompositor compositor; compositor.SetReplacements({group}, {bg});
		std::vector<uint32_t> original = {0xFF0000FF, 0xFFFF0000, 0xFF0000FF, 0xFF0000FF};
		const auto* output = compositor.Apply(original.data(), 4, 1, frame, 0, 0, [](uint16_t) { return 0xFF0000FF; });
		assert(output[0] == 0xFF00FF00 && output[1] == 0xFF80FF80 && output[2] == 0xFFFFFFFF && output[3] == 0xFF80C0BF);
		frame.DecodedRows[0].ExpansionClass = 1;
		output = compositor.Apply(original.data(), 4, 1, frame, 0, 0, [](uint16_t) { return 0xFF0000FF; });
		assert(output[0] == 0xFF00FF00 && output[1] == 0xFF00FF00 && output[2] == 0xFF00FF00 && output[3] == 0xFF40C07F);
		frame.DecodedRows[0].ExpansionClass = 2;
		for(auto& pixel : frame.Pixels) { pixel.BackgroundOverlayOccludesClasses = 0; }
		output = compositor.Apply(original.data(), 4, 1, frame, 0, 0, [](uint16_t) { return 0xFF0000FF; });
		assert(output[1] == 0xFF00FF00 && output[2] == 0xFF00FF00 && output[3] == 0xFF40C07F);
		std::cout << "PASS: background PNG alpha stays above behind-background full canvases, including partial sprite alpha." << std::endl;
	}
	{
		// Oversized PNG margins use destination rules, not original coverage.
		MetaSpriteFrame frame(6, 4);
		frame.DecodedRows = {{5, 2, 1, 0, 1, 1, {0xFFFF0000}}};
		frame.DecodedRows[0].ExpansionClass = 1;
		std::vector<uint32_t> original(24, 0xFF0000FF);
		original[8] = 0xFFFF0000; frame.Pixels[8].VisibleSprite = 5;
		for(auto& pixel : frame.Pixels) { pixel.ExpandedSpriteClasses = 3; pixel.BackgroundColor = 0x7C00; }
		MetaSpriteReplacement group;
		group.ArtworkWidth = 4; group.ArtworkHeight = 3;
		group.ArtworkOriginX = group.ArtworkOriginY = 1;
		group.Artwork.assign(12, 0xFF00FF00);
		group.Artwork[0] = 0; group.Artwork[1] = 0x80FFFFFF;
		group.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		MetaSpriteCompositor compositor; compositor.SetGroups({group});
		const auto* output = compositor.Apply(original.data(), 6, 4, frame);
		for(int y = 0; y < 4; y++) { for(int x = 0; x < 6; x++) {
			uint32_t expected = x >= 1 && x < 5 && y < 3 ? 0xFF00FF00 : original[y * 6 + x];
			if(y == 0 && x == 1) { expected = original[1]; }
			if(y == 0 && x == 2) { expected = 0xFF8080FF; }
			assert(output[y * 6 + x] == expected);
		} }
		frame.Pixels[3].ExpandedSpriteClasses = 0; // Masked destination.
		frame.Pixels[4].ExpandedSpriteOccluder = 4; // Earlier hardware sprite.
		frame.Pixels[13].ExpandedSpriteOccluder = 6; // Later sprite loses.
		output = compositor.Apply(original.data(), 6, 4, frame);
		assert(output[3] == original[3] && output[4] == original[4] && output[13] == 0xFF00FF00);
		frame.DecodedRows[0].ExpansionClass = 2;
		frame.Pixels[13].ExpandedSpriteClasses = 1; // Opaque BG blocks behind-BG OBJ.
		assert(compositor.Apply(original.data(), 6, 4, frame)[13] == original[13]);
		MetaAlternateArtwork alternate;
		alternate.ArtworkWidth = 6; alternate.ArtworkHeight = 4;
		alternate.ArtworkOriginX = 3; alternate.ArtworkOriginY = 2;
		alternate.Artwork.assign(24, 0xFFFFFF00); alternate.MemoryConditions = {{0x20, 1, 0xFF}};
		group.AlternateArtwork = {alternate}; compositor.SetGroups({group});
		frame.MemorySnapshot = {{0x20u, uint8_t(1)}};
		output = compositor.Apply(original.data(), 6, 4, frame);
		assert(output[0] == 0xFFFFFF00 && output[6] == 0xFFFFFF00 && output[5] == original[5]);
		// Runtime class metadata never becomes a graphic matching requirement.
		assert(output[8] == 0xFFFFFF00);
		group.AlternateArtwork.clear(); frame.DecodedRows[0].ExpansionClass = 1;
		for(auto& pixel : frame.Pixels) { pixel.ExpandedSpriteClasses = 3; pixel.ExpandedSpriteOccluder = -1; }
		for(int scale : {2, 3}) { for(int edge : {0, 1}) {
			group.ArtworkScale = scale; group.ArtworkWidth = 4 * scale + edge; group.ArtworkHeight = 3 * scale + edge;
			group.Artwork.assign(group.ArtworkWidth * group.ArtworkHeight, 0xFF00FF00);
			if(edge) { group.Artwork.back() = 0xFFFF00FF; }
			compositor.SetGroups({group}); output = compositor.Apply(original.data(), 6, 4, frame);
			assert(compositor.GetOutputScale() == (uint32_t)scale);
			for(int y = 0; y < 4 * scale; y++) { for(int x = 0; x < 6 * scale; x++) {
				bool inside = x >= scale && x < 5 * scale + edge && y < 3 * scale + edge;
				uint32_t color = edge && x == 5 * scale && y == 3 * scale ? 0xFFFF00FF : 0xFF00FF00;
				assert(output[y * 6 * scale + x] == (inside ? color : original[y / scale * 6 + x / scale]));
			} }
		} }
		std::cout << "PASS: full PNG canvas, 1x/2x/3x scales, transparent/partial-alpha margins, destination masks/occlusion and oversized alternate origins." << std::endl;
	}
	{
		MetaSpriteFrame frame(24, 12);
		for(int id = 0; id < 2; id++) { for(int y = 0; y < 8; y++) {
			MetaSpriteRow row;
			row.Sprite = (uint16_t)id; row.X = id ? 4 : 8; row.Y = 0; row.Scanline = (uint16_t)y;
			row.LowByte = 255; row.Palette[1] = id ? 0x7FFF : 31;
			row.ExpansionOrder = (row.X + 8) * 64 + id;
			frame.Rows.push_back(row);
		} }
		for(auto& pixel : frame.Pixels) { pixel.ExpandedSpriteClasses = 3; }
		std::vector<uint32_t> original(24 * 12, 0xFF0000FF);
		MetaSpriteReplacement first, second;
		first.Members = {{0, 0, 8, 8, -1, std::vector<uint32_t>(64, 0xFFFF0000)}};
		second.Members = {{0, 0, 8, 8, -1, std::vector<uint32_t>(64, 0xFFFFFFFF)}};
		first.ArtworkWidth = second.ArtworkWidth = 24; first.ArtworkHeight = second.ArtworkHeight = 8;
		first.Artwork.assign(192, 0xFFFFFF00); second.Artwork.assign(192, 0xFF00FFFF);
		MetaSpriteCompositor compositor; compositor.SetGroups({first, second});
		assert(compositor.Apply(original.data(), 24, 12, frame)[18] == 0xFF00FFFF); // DMG: lower X wins despite higher OAM ID.
		for(auto& row : frame.Rows) { row.ExpansionOrder = row.Sprite; }
		assert(compositor.Apply(original.data(), 24, 12, frame)[18] == 0xFFFFFF00); // GBC: lower OAM ID wins.
		for(auto& row : frame.Rows) { row.X = 8; row.ExpansionOrder = (row.X + 8) * 64 + row.Sprite; }
		assert(compositor.Apply(original.data(), 24, 12, frame)[18] == 0xFFFFFF00); // DMG equal-X tie uses OAM order.
		for(auto& row : frame.Rows) { row.X = row.Sprite ? 4 : 8; }
		frame.Pixels[18].ExpandedSpriteOccluder = 0;
		for(auto& row : frame.Rows) { row.ExpansionOrder = (row.X + 8) * 64 + row.Sprite; }
		assert(compositor.Apply(original.data(), 24, 12, frame)[18] == original[18]);
		frame.Pixels[18].ExpandedSpriteOccluder = -1; frame.Pixels[18].ExpandedSpriteClasses = 1;
		for(auto& row : frame.Rows) { row.ExpansionOrder = row.Sprite; if(row.Sprite == 0) { row.Attributes = 0x80; } }
		assert(compositor.Apply(original.data(), 24, 12, frame)[18] == 0xFF00FFFF); // Behind-BG class cannot draw over opaque BG.
		std::cout << "PASS: Game Boy expanded canvas uses DMG X/OAM ties, GBC OAM order and destination background/OBJ priority." << std::endl;
	}
	{
		MetaBackgroundReplacement layer; layer.Id = 1; layer.Layer = 0;
		layer.Scale = 2; layer.Width = layer.Height = 8;
		for(uint32_t i = 0; i < 64; i++) { layer.Artwork.push_back(0xFF000000 | i); }
		MetaBackgroundSample sample {1, 2, 0, true};
		sample.PixelScaleX = sample.PixelScaleY = 2; sample.PixelOffsetX = sample.PixelOffsetY = 1;
		uint32_t color = 0;
		assert(layer.Sample(sample, 5, 5, 0, 0, 1, color) && color == layer.Artwork[5 * 8 + 3]);
		assert(layer.Sample(sample, 5, 5, 1, 1, 2, color) && color == layer.Artwork[5 * 8 + 3]);
		layer.FollowScroll = false;
		assert(layer.Sample(sample, 5, 5, 0, 0, 1, color) && color == layer.Artwork[5 * 8 + 5]);
		sample.PixelOffsetX = 2; assert(!layer.Sample(sample, 5, 5, 0, 0, 1, color));
	}
	{
		MetaSpriteFrame logical(2, 1), physical(4, 2);
		logical.SnesColorEffects = true;
		logical.DecodedRows.push_back({0, 0, 0, 0, 1, 1, {0xFFFF0000}});
		logical.SnesSubOutput.resize(2);
		for(auto& pixel : logical.Pixels) {
			pixel.SpriteColor = pixel.SnesFallbackMainColor = 0x7C00;
			pixel.Brightness = 15; pixel.SnesFallbackCaptured = true;
		}
		logical.Pixels[0].VisibleSprite = 0; logical.Pixels[0].SpriteColor = 31;
		logical.Pixels[0].SnesObjectsMain = true; logical.Pixels[1].SnesObjectsSub = true;
		logical.Pixels[0].SnesObjectPriorities[2] = 9;
		logical.Pixels[0].SnesCoverage.push_back({0, 1, 3, 0, false, true, false});
		for(auto& phase : logical.SnesSubOutput) {
			phase.Captured = true; phase.Brightness = 15;
			phase.SpriteColor = phase.SnesFallbackMainColor = 0x7C00; phase.SnesFallbackCaptured = true;
		}
		assert(MetaSnesCapture::ExpandWithoutMath(logical, physical));
		assert(physical.SpriteCoordinateScale == 2 && physical.DecodedRows[0].Width == 1);
		assert(physical.Pixels[0].SnesCoverage[0].Main && !physical.Pixels[0].SnesCoverage[0].Sub);
		assert(physical.Pixels[1].VisibleSprite == 0 && physical.Pixels[4].SnesCoverage[0].Main);
		assert(!physical.Pixels[0].SnesObjectsMain && physical.Pixels[1].SnesObjectsMain);
		assert(physical.Pixels[2].SnesObjectsMain && !physical.Pixels[3].SnesObjectsMain);
		assert(!physical.Pixels[1].SnesObjectsSub && physical.Pixels[1].SnesObjectPriorities[2] == 9);
		logical.SnesSubOutput[0].Captured = false;
		assert(MetaSnesCapture::ExpandWithoutMath(logical, physical));
		assert(physical.Pixels[0].VisibleSprite == 0 && physical.Pixels[1].VisibleSprite == 0);
		logical.SnesSubOutput[0].Captured = true;
		logical.SnesBackgrounds.resize(2);
		logical.SnesBackgrounds[0].Captured = true;
		auto& bg = logical.SnesBackgrounds[0].Layers[0];
		bg.Sample = {1, 2, 0, true}; bg.SubSample = {0, 2, 0, true};
		bg.Main = bg.Sub = bg.SeparateSub = true; bg.Color = 31; bg.SubColor = 0x7C00;
		bg.Opaque = bg.SubOpaque = true;
		assert(MetaSnesCapture::ExpandWithoutMath(logical, physical));
		const auto& evenBg = physical.SnesBackgrounds[0].Layers[0];
		const auto& oddBg = physical.SnesBackgrounds[1].Layers[0];
		assert(evenBg.Sample.X == 0 && oddBg.Sample.X == 1 && evenBg.Color == 0x7C00 && oddBg.Color == 31);
		assert(evenBg.Main && oddBg.Main && !evenBg.Sub && !oddBg.Sub && evenBg.Sample.PixelScaleX == 1);
		assert(physical.SnesBackgrounds[4].Layers[0].Sample.PixelOffsetY == 1);
		bg.Math = true; assert(!MetaSnesCapture::ExpandWithoutMath(logical, physical));
		logical.SnesBackgrounds.clear();
		logical.SnesSubOutput[0].SnesMathEnabled = true;
		assert(!MetaSnesCapture::ExpandWithoutMath(logical, physical));
		logical.SnesSubOutput[0].SnesMathEnabled = false;
		logical.Pixels[0].SnesFallbackMainFlags = 0x80;
		assert(!MetaSnesCapture::ExpandWithoutMath(logical, physical));
		logical.Pixels[0].SnesFallbackMainFlags = 0;
		logical.Pixels[0].SnesCoverage.clear(); // Isolate main-only OBJ for artwork mapping.
		assert(MetaSnesCapture::ExpandWithoutMath(logical, physical));
		std::vector<uint32_t> native {0xFF0000FF, 0xFFFF0000, 0xFF0000FF, 0xFF0000FF,
			0xFF0000FF, 0xFFFF0000, 0xFF0000FF, 0xFF0000FF};
		MetaSpriteReplacement group;
		group.Members = {{0, 0, 1, 1, -1, {0xFFFF0000}}};
		group.ArtworkScale = 1; group.ArtworkWidth = group.ArtworkHeight = 1; group.Artwork = {0xFF00FF00};
		group.ArtworkOriginX = -1;
		MetaSpriteCompositor compositor; compositor.SetGroups({group});
		auto output = compositor.Apply(native.data(), 4, 2, physical);
		assert(compositor.GetOutputScale() == 1 && output[1] == 0xFF0000FF && output[5] == 0xFF0000FF);
		group.ArtworkOriginX = 0; group.ArtworkScale = 3;
		group.ArtworkWidth = group.ArtworkHeight = 3; group.Artwork.clear();
		for(int i = 0; i < 9; i++) { group.Artwork.push_back(0xFF000000 | ((i + 1) * 20 << 8)); }
		compositor.SetGroups({group}); output = compositor.Apply(native.data(), 4, 2, physical);
		assert(compositor.GetOutputScale() == 2);
		for(int y = 0; y < 4; y++) { for(int x = 0; x < 8; x++) {
			assert(output[y * 8 + x] == (x == 2 || x == 3 ? group.Artwork[y * 3 / 4 * 3 + x * 3 / 4] : 0xFF0000FF));
		} }
		compositor.SetGroups({}); assert(compositor.Apply(native.data(), 4, 2, physical) == native.data());
		assert(compositor.GetOutputScale() == 1);
		std::cout << "PASS: SNES hires phase coverage, logical group coordinates, shifted 1x artwork, odd artwork scale, and unsupported-math fallback." << std::endl;
	}
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
	firstFrame->Pixels[0].NativeBackdropCaptured = true; firstFrame->Pixels[0].NativeBackdropColor = 0x123;
	firstFrame->Pixels[0].ExpandedSpriteClassesWithoutBackground = 3;
	firstFrame->BackgroundUnderlays.resize(6); firstFrame->BackgroundUnderlays[0].Color = 0x456;
	auto secondFrame = framePool.Acquire(3, 2);
	auto thirdFrame = framePool.Acquire(3, 2);
	firstFrame->FrameNumber = 99; firstFrame->GbaColorEffects = firstFrame->SnesColorEffects = true; firstFrame->SpriteCoordinateScale = 2;
	firstFrame->Pixels[0].VisibleSprite = 7; firstFrame->Pixels[0].BackgroundSample = {4, 5, 2, true};
	firstFrame->Pixels[0].GbaCoverage.push_back({7, 9});
	firstFrame->Pixels[0].GbaNativeObjects.push_back({11, 0x03FF, 2, true});
	firstFrame->Pixels[0].SnesCoverage.push_back({7, 1, 3, 4, true, false, true});
	firstFrame->SnesBackgrounds.resize(6); firstFrame->Rows.resize(1); firstFrame->DecodedRows.resize(1); firstFrame->NesRows.resize(1);
	firstFrame->SnesSubOutput.resize(6);
	firstFrame->SnesSubOutput[0].Captured = true; firstFrame->SnesSubOutput[0].MathControlX = 2;
	firstFrame->MemorySnapshot[0x123] = 9;
	auto overflowFrame = framePool.Acquire(3, 2);
	if(overflowFrame == firstFrame || overflowFrame == secondFrame || overflowFrame == thirdFrame) { return 1; }
	firstFrame.reset(); overflowFrame.reset();
	overflowFrame = framePool.Acquire(3, 2);
	if(overflowFrame == retainedFrame || retainedFrame->Pixels[0].VisibleSprite != 7
		|| !retainedFrame->Pixels[0].NativeBackdropCaptured || retainedFrame->Pixels[0].NativeBackdropColor != 0x123
		|| retainedFrame->Pixels[0].ExpandedSpriteClassesWithoutBackground != 3
		|| retainedFrame->BackgroundUnderlays.size() != 6 || retainedFrame->BackgroundUnderlays[0].Color != 0x456
		|| retainedFrame->Pixels[0].GbaNativeObjects.begin() == retainedFrame->Pixels[0].GbaNativeObjects.end()
		|| retainedFrame->Pixels[0].GbaNativeObjects[0].Color != 0x03FF || !retainedFrame->Pixels[0].GbaNativeObjects[0].SemiTransparent
		|| !retainedFrame->SnesSubOutput[0].Captured || retainedFrame->SnesSubOutput[0].MathControlX != 2) { return 1; }
	MetaSpriteFrame* reusableAddress = retainedFrame.get();
	retainedFrame.reset();
	auto reusedFrame = framePool.Acquire(3, 2);
	if(reusedFrame.get() != reusableAddress || reusedFrame->FrameNumber != 0 || reusedFrame->SpriteCoordinateScale != 1 || reusedFrame->GbaColorEffects || reusedFrame->SnesColorEffects
		|| reusedFrame->Pixels[0].VisibleSprite != -1 || reusedFrame->Pixels[0].BackgroundSample.Valid
		|| reusedFrame->Pixels[0].NativeBackdropCaptured || reusedFrame->Pixels[0].NativeBackdropColor != 0
		|| reusedFrame->Pixels[0].ExpandedSpriteClassesWithoutBackground != 0
		|| !reusedFrame->BackgroundUnderlays.empty()
		|| reusedFrame->Pixels[0].GbaCoverage.begin() != reusedFrame->Pixels[0].GbaCoverage.end()
		|| reusedFrame->Pixels[0].GbaNativeObjects.begin() != reusedFrame->Pixels[0].GbaNativeObjects.end()
		|| reusedFrame->Pixels[0].SnesCoverage.begin() != reusedFrame->Pixels[0].SnesCoverage.end()
		|| !reusedFrame->SnesBackgrounds.empty() || !reusedFrame->SnesSubOutput.empty() || !reusedFrame->Rows.empty() || !reusedFrame->DecodedRows.empty()
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
		std::ifstream partialFile(std::string(argv[1]) + ".partial", std::ios::binary);
		assert(partialFile.good());
		std::vector<uint8_t> partialPacket((std::istreambuf_iterator<char>(partialFile)), std::istreambuf_iterator<char>());
		auto partial = MetaSpritePacket(partialPacket.data(), partialPacket.size()).Parse();
		assert(partial.size() == 1 && partial[0].ArtworkScale == 2 && partial[0].ArtworkWidth == 17 && partial[0].ArtworkHeight == 19);
		assert(partial[0].Artwork.size() == 17 * 19 && partial[0].Artwork.back() == 0xFF00FF00);
		assert(partial[0].AlternateArtwork.size() == 1 && partial[0].AlternateArtwork[0].ArtworkScale == 2);
		assert(partial[0].AlternateArtwork[0].ArtworkWidth == 19 && partial[0].AlternateArtwork[0].ArtworkHeight == 21);
		assert(partial[0].AlternateArtwork[0].Artwork.back() == 0xFFFFFFFF);
		for(size_t length = 0; length < partialPacket.size(); length++) {
			bool partialRejected = false;
			try { MetaSpritePacket(partialPacket.data(), length).Parse(); } catch(const std::runtime_error&) { partialRejected = true; }
			assert(partialRejected);
		}
		std::cout << "PASS: partial native PNG canvases transfer completely for default/alternate images; truncated packets remain rejected.\n";
	}
	std::cout << "PASS: native graphic reconstruction, content matching, visibility/priority masks, transparent suppression, alpha blend, incomplete capture fallback, disable fallback.\n";
}

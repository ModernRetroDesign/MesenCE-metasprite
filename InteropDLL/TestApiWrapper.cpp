#include "Common.h"
#include "Core/Shared/RecordedRomTest.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/RewindManager.h"
#include "Core/Shared/Video/MetaSpritePacket.h"
#include "Core/Shared/Video/MetaBackgroundPacket.h"
#include "Core/Shared/Video/MetaTextPacket.h"
#include "Core/Shared/Audio/MusicReplacement.h"
#include "Core/Shared/Audio/SoundMixer.h"
#include "Core/NES/NesMemoryManager.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/MessageManager.h"
#include "Utilities/FolderUtilities.h"
#include "Utilities/StringUtilities.h"
#include "Core/Gameboy/Gameboy.h"
#include "Core/Gameboy/GbMemoryManager.h"
#include "Core/Gameboy/GbPpu.h"
#include "Core/SMS/SmsConsole.h"
#include "Core/SMS/SmsMemoryManager.h"
#include "Core/SMS/SmsVdp.h"
#include "Core/PCE/PceConsole.h"
#include "Core/PCE/PceMemoryManager.h"
#include "Core/PCE/PceVdc.h"
#include "Core/GBA/GbaConsole.h"
#include "Core/GBA/GbaMemoryManager.h"
#include "Core/GBA/GbaPpu.h"
#include "Core/WS/WsConsole.h"
#include "Core/WS/WsMemoryManager.h"
#include "Core/WS/WsPpu.h"
#include "Core/NES/NesConsole.h"
#include "Core/NES/NesCpu.h"
#include "Core/NES/BaseNesPpu.h"
#include "Core/SNES/SnesConsole.h"
#include "Core/SNES/SnesMemoryManager.h"
#include "Core/SNES/Spc.h"
#include "Core/SNES/SnesPpu.h"
#include "Core/Shared/Video/VideoDecoder.h"
#include "Core/Shared/Video/VideoRenderer.h"
#include "Core/Shared/Video/SoftwareRenderer.h"
#include "Core/Debugger/Debugger.h"
#include "Core/Debugger/PpuTools.h"
#include "Core/Debugger/MemoryAccessCounter.h"
#include "Core/Debugger/DebugBreakHelper.h"
#include "Core/Shared/NotificationManager.h"
#include "Core/Shared/SaveStateManager.h"
#include "Core/Shared/RewindData.h"
#include <sstream>
#include "Utilities/Serializer.h"
#include "Utilities/CompressionHelper.h"
#include <mutex>
#include <set>
#include <iostream>

class MetaSpriteViewerRefreshListener : public INotificationListener
{
public:
	std::atomic<uint32_t> First {0}, Second {0}, Unexpected {0};
	void ProcessNotification(ConsoleNotificationType type, void* parameter) override
	{
		if(type != ConsoleNotificationType::ViewerRefresh) { return; }
		uintptr_t id = reinterpret_cast<uintptr_t>(parameter);
		if(id == 123) { First.fetch_add(1, std::memory_order_relaxed); }
		else if(id == 456) { Second.fetch_add(1, std::memory_order_relaxed); }
		else { Unexpected.fetch_add(1, std::memory_order_relaxed); }
	}
};

static bool VerifyMetaViewerCounters(Emulator* emu)
{
	auto* debugger = emu->InternalGetDebugger();
	DebugBreakHelper paused(debugger);
	auto* counter = debugger->GetMemoryAccessCounter();
	AddressCounters before[6] = {}, after[6] = {};
	constexpr uint32_t start = 0x1FFF0;
	counter->GetAccessCounts(start, 6, MemoryType::SnesWorkRam, before);
	AddressInfo address {(int32_t)start, MemoryType::SnesWorkRam};
	if(counter->ProcessMemoryRead<2>(address, 100) != ReadResult::Normal) { return false; }
	address.Address = start + 1; counter->ProcessMemoryWrite<2>(address, 200);
	address.Address = start + 2; counter->ProcessMemoryExec<4>(address, 300);
	address.Address = start; counter->ProcessMemoryExec<3>(address, 400);
	counter->GetAccessCounts(start, 6, MemoryType::SnesWorkRam, after);
	for(int i = 0; i < 6; i++) {
		if(after[i].ReadCounter != before[i].ReadCounter + (i < 2 ? 1 : 0)
			|| after[i].ReadStamp != (i < 2 ? 100 : before[i].ReadStamp)
			|| after[i].WriteCounter != before[i].WriteCounter + (i >= 1 && i <= 2 ? 1 : 0)
			|| after[i].WriteStamp != (i >= 1 && i <= 2 ? 200 : before[i].WriteStamp)
			|| after[i].ExecCounter != before[i].ExecCounter + (i < 3 ? 1 : 0) + (i >= 2 ? 1 : 0)
			|| after[i].ExecStamp != (i < 3 ? 400 : 300)) { return false; }
	}
	address.Address = -1;
	if(counter->ProcessMemoryRead(address, 500) != ReadResult::Normal) { return false; }
	counter->ProcessMemoryWrite(address, 500); counter->ProcessMemoryExec(address, 500);
	counter->ResetCounts();
	counter->GetAccessCounts(start, 6, MemoryType::SnesWorkRam, after);
	for(const auto& value : after) {
		if(value.ReadCounter || value.WriteCounter || value.ExecCounter || value.ReadStamp || value.WriteStamp || value.ExecStamp) { return false; }
	}
	address = {(int32_t)start, MemoryType::SnesWorkRam};
	counter->ProcessMemoryRead(address, 12345);
	counter->GetAccessCounts(start, 1, MemoryType::SnesWorkRam, after);
	return after[0].ReadCounter == 1 && after[0].ReadStamp == 12345;
}

class MetaSpriteTestRenderer final : public IRenderingDevice
{
public:
	std::mutex Mutex;
	uint32_t Width = 0, Height = 0, FrameNumber = 0;
	vector<uint32_t> Pixels;
	shared_ptr<const MetaSpriteFrame> Capture;
	bool RecordNextCapture = false;
	vector<uint32_t> FirstCapturePixels;
	uint8_t FirstCaptureOffsetY = 0;
	bool MeasureTiming = false;
	vector<double> FrameIntervals;
	uint32_t TimingFrame = 0, SkippedTimingFrames = 0;
	bool RecordReplayFrames=false, CheckReplayFrames=false;
	bool CompressReplayFrames=false;
	static vector<uint32_t> EncodeReplayRuns(const vector<uint32_t>& pixels) {
		vector<uint32_t> runs;
		for(size_t i=0;i<pixels.size();) {
			size_t end=i+1; while(end<pixels.size() && pixels[end]==pixels[i]) { end++; }
			runs.push_back((uint32_t)(end-i)); runs.push_back(pixels[i]); i=end;
		}
		return runs;
	}
	std::unordered_map<uint32_t,vector<uint32_t>> ReplayFrames;
	uint32_t ReplayMatches=0, ReplayErrors=0, ReplayPrevious=UINT32_MAX;
	std::chrono::steady_clock::time_point TimingPrevious;
	void UpdateFrame(RenderedFrame& frame) override
	{
		std::lock_guard<std::mutex> lock(Mutex);
		Width = frame.Width; Height = frame.Height; FrameNumber = frame.FrameNumber;
		Pixels.assign((uint32_t*)frame.FrameBuffer, (uint32_t*)frame.FrameBuffer + Width * Height);
		Capture = frame.MetaSprites;
		if(RecordReplayFrames && ReplayFrames.size()<180) { ReplayFrames[FrameNumber]=CompressReplayFrames ? EncodeReplayRuns(Pixels) : Pixels; }
		if(CheckReplayFrames && !Capture) {
			auto expected=ReplayFrames.find(FrameNumber);
			const auto actual=CompressReplayFrames ? EncodeReplayRuns(Pixels) : Pixels;
			if(expected==ReplayFrames.end() || expected->second!=actual || FrameNumber>=ReplayPrevious) {
				ReplayErrors++; std::cerr << "Rewind output mismatch at frame " << FrameNumber << " previous=" << ReplayPrevious << " recorded=" << (expected!=ReplayFrames.end()) << std::endl;
				if(expected!=ReplayFrames.end() && !CompressReplayFrames) {
					std::cerr<<"Rewind dimensions "<<Width<<"x"<<Height<<" expected pixels="<<expected->second.size()<<std::endl;
					for(size_t i=0;i<std::min(Pixels.size(),expected->second.size());i++) { if(Pixels[i]!=expected->second[i]) { std::cerr<<"Rewind first mismatch x="<<i%Width<<" y="<<i/Width<<" got="<<std::hex<<Pixels[i]<<" expected="<<expected->second[i]<<std::dec<<std::endl; break; } }
				}
				if(expected!=ReplayFrames.end() && CompressReplayFrames) {
					size_t a=0,b=0,index=0; uint32_t leftA=actual.empty()?0:actual[0],leftB=expected->second.empty()?0:expected->second[0];
					while(a+1<actual.size() && b+1<expected->second.size()) {
						if(actual[a+1]!=expected->second[b+1]) { std::cerr<<"Rewind first mismatch x="<<index%Width<<" y="<<index/Width<<" got="<<std::hex<<actual[a+1]<<" expected="<<expected->second[b+1]<<std::dec<<std::endl; break; }
						uint32_t count=std::min(leftA,leftB); index+=count; leftA-=count; leftB-=count;
						if(!leftA) { a+=2; if(a<actual.size()) { leftA=actual[a]; } }
						if(!leftB) { b+=2; if(b<expected->second.size()) { leftB=expected->second[b]; } }
					}
				}
			} else { ReplayMatches++; }
			ReplayPrevious=FrameNumber;
		}
		if(RecordNextCapture && Capture) {
			FirstCapturePixels = Pixels; FirstCaptureOffsetY = Capture->SnesOutputOffsetY;
			RecordNextCapture = false;
		}
		if(MeasureTiming) {
			auto now = std::chrono::steady_clock::now();
			if(TimingFrame && frame.FrameNumber > TimingFrame) {
				FrameIntervals.push_back(std::chrono::duration<double, std::milli>(now - TimingPrevious).count());
				SkippedTimingFrames += frame.FrameNumber - TimingFrame - 1;
			}
			TimingFrame = frame.FrameNumber; TimingPrevious = now;
		}
	}
	void ClearFrame() override {}
	void Render(RenderSurfaceInfo&, RenderSurfaceInfo&) override {}
	void Reset() override {}
	void SetFullscreenMode(FullscreenSettings) override {}
};

static bool VerifyExpandedCanvas(Emulator* emu, MetaSpriteTestRenderer& renderer, MetaSpriteReplacement group,
	int spriteX, int spriteY, int hardwareSprite, uint8_t expansionClass, int cropLeft = 0, int cropTop = 0, bool opaqueBackgroundPng = false)
{
	vector<uint32_t> baseline;
	shared_ptr<const MetaSpriteFrame> capture;
	uint32_t width, height, previousFrame;
	{
		std::lock_guard<std::mutex> lock(renderer.Mutex);
		baseline = renderer.Pixels; capture = renderer.Capture;
		width = renderer.Width; height = renderer.Height; previousFrame = renderer.FrameNumber;
	}
	if(!capture || group.ArtworkScale != 2 || width * height != baseline.size()) { return false; }
	const int padding = 10, paddingPixels = padding * 2;
	MetaSpriteReplacement enlarged = group;
	// One extra PNG pixel exercises a partial native texel at scale two.
	enlarged.ArtworkWidth += paddingPixels * 2 + 1; enlarged.ArtworkHeight += paddingPixels * 2 + 1;
	enlarged.ArtworkOriginX += padding; enlarged.ArtworkOriginY += padding;
	enlarged.Artwork.resize(enlarged.ArtworkWidth * enlarged.ArtworkHeight);
	for(int y = 0; y < enlarged.ArtworkHeight; y++) { for(int x = 0; x < enlarged.ArtworkWidth; x++) {
		enlarged.Artwork[y * enlarged.ArtworkWidth + x] = (x + y) % 3 == 0 ? 0 : (x + y) % 5 == 0 ? 0x80FFFFFF : 0xFFFFFF00;
	} }
	for(int y = 0; y < group.ArtworkHeight; y++) {
		std::copy_n(group.Artwork.data() + y * group.ArtworkWidth, group.ArtworkWidth,
			enlarged.Artwork.data() + (y + paddingPixels) * enlarged.ArtworkWidth + paddingPixels);
	}
	vector<uint32_t> expected = baseline;
	int expandedPixels = 0, blockedPixels = 0;
	vector<MetaSpriteGraphic> instances;
	if(!capture->DecodedRows.empty() || !capture->Rows.empty()) {
		const auto candidates = capture->Rows.empty() ? MetaSpriteCompositor::BuildDecodedCandidates(*capture) : MetaSpriteCompositor::BuildGameboyCandidates(*capture);
		for(const auto& candidate : candidates) {
			if(group.Members.size() == 1 && candidate.Width == group.Members[0].Width && candidate.Height == group.Members[0].Height && candidate.Pixels == group.Members[0].Pixels) { instances.push_back(candidate); }
		}
	}
	if(instances.empty()) { instances.push_back({spriteX + cropLeft, spriteY + cropTop, 0, 0, hardwareSprite, {}}); }
	std::sort(instances.begin(), instances.end(), [](const MetaSpriteGraphic& a, const MetaSpriteGraphic& b) { return a.GetExpansionOrder() > b.GetExpansionOrder(); });
	// Obtain the destination with matched source sprites removed, including
	// configured background replacements. Then independently blend full PNGs.
	MetaSpriteReplacement cleared = group;
	std::fill(cleared.Artwork.begin(), cleared.Artwork.end(), 0);
	emu->GetVideoDecoder()->SetMetaSpriteGroups({cleared});
	bool destinationCaptured = false;
	for(int attempt = 0; attempt < 500; attempt++) {
		{ std::lock_guard<std::mutex> lock(renderer.Mutex);
			if(renderer.FrameNumber > previousFrame + 2 && renderer.Width == width && renderer.Height == height) {
				expected = renderer.Pixels; previousFrame = renderer.FrameNumber; destinationCaptured = true; break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	if(!destinationCaptured) { emu->GetVideoDecoder()->SetMetaSpriteGroups({group}); return false; }
	for(const auto& instance : instances) {
	spriteX = instance.X - cropLeft; spriteY = instance.Y - cropTop; hardwareSprite = instance.GetExpansionOrder();
	for(int y = 0; y < (int)height; y++) { for(int x = 0; x < (int)width; x++) {
		int ax = x - spriteX * 2 + paddingPixels, ay = y - spriteY * 2 + paddingPixels;
		if(ax < 0 || ay < 0 || ax >= enlarged.ArtworkWidth || ay >= enlarged.ArtworkHeight) { continue; }
		const auto& pixel = capture->Pixels[(y / 2 + cropTop) * capture->Width + x / 2 + cropLeft];
		bool replacedOccluder = std::any_of(instances.begin(), instances.end(), [&](const auto& other) { return other.GetExpansionOrder() == pixel.ExpandedSpriteOccluder; });
		if(!(pixel.ExpandedSpriteClasses & expansionClass) || (pixel.ExpandedSpriteOccluder >= 0 && pixel.ExpandedSpriteOccluder < hardwareSprite && !replacedOccluder)) { blockedPixels++; continue; }
		if(opaqueBackgroundPng && (pixel.BackgroundOverlayOccludesClasses & expansionClass) && pixel.BackgroundSample.Valid) { blockedPixels++; continue; }
		uint32_t color = enlarged.Artwork[ay * enlarged.ArtworkWidth + ax], alpha = color >> 24;
		uint32_t result = 0xFF000000, bg = expected[y * width + x];
		for(int shift : {0, 8, 16}) { result |= ((((color >> shift) & 255) * alpha + ((bg >> shift) & 255) * (255 - alpha) + 127) / 255) << shift; }
		expected[y * width + x] = result;
		if(alpha) { expandedPixels++; }
	} }
	}
	if(!expandedPixels && !blockedPixels) { return false; }
	emu->GetVideoDecoder()->SetMetaSpriteGroups({enlarged});
	bool passed = false;
	for(int attempt = 0; attempt < 500; attempt++) {
		{ std::lock_guard<std::mutex> lock(renderer.Mutex);
			if(renderer.FrameNumber > previousFrame + 2 && renderer.Width == width && renderer.Height == height && renderer.Pixels == expected) { passed = true; break; }
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
	bool restored = false;
	for(int attempt = 0; attempt < 500; attempt++) {
		{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.Width == width && renderer.Height == height && renderer.Pixels == baseline) { restored = true; break; } }
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	passed &= restored;
	if(passed) { std::cout << "PASS: live full PNG canvas, origin alignment, margin alpha and screen/priority clipping; expanded=" << expandedPixels << ", blocked=" << blockedPixels << std::endl; }
	else { std::cerr << "Expanded PNG canvas mismatch" << std::endl; }
	if(passed) {
		VideoConfig savedVideo = emu->GetSettings()->GetVideoConfig(), wideVideo = savedVideo;
		FrameInfo logical = emu->GetVideoDecoder()->GetBaseFrameInfo(true);
		double ratio = (double)width / std::max(1u, logical.Width);
		int left = (int)std::round(5 * ratio), right = (int)std::round(9 * ratio);
		uint32_t wideWidth = (width + left + right + 1) / 2 * 2;
		wideVideo.DecorativeWidescreen = true; wideVideo.DecorativeCustomWidths = true;
		wideVideo.DecorativeLeftWidth = 5; wideVideo.DecorativeRightWidth = 9; wideVideo.DecorativeViewportPosition = 0;
		emu->GetSettings()->SetVideoConfig(wideVideo);
		bool widened = false;
		for(int attempt = 0; attempt < 500; attempt++) {
			{ std::lock_guard<std::mutex> lock(renderer.Mutex);
				if(renderer.Width == wideWidth && renderer.Height == height) {
					widened = true;
					for(uint32_t y = 0; y < height && widened; y++) { for(uint32_t x = 0; x < width; x++) {
						if(renderer.Pixels[y * wideWidth + x + left] != baseline[y * width + x]) { widened = false; break; }
					} }
					if(widened) { break; }
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		emu->GetSettings()->SetVideoConfig(savedVideo);
		bool nativeRestored = false;
		for(int attempt = 0; attempt < 500; attempt++) {
			{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.Width == width && renderer.Height == height && renderer.Pixels == baseline) { nativeRestored = true; break; } }
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		passed &= widened && nativeRestored;
		if(passed) { std::cout << "PASS: live shared widescreen asymmetric canvas preserves every native viewport pixel and restores dimensions." << std::endl; }
	}
	return passed;
}

static bool VerifyDedicatedWidescreen(Emulator* emu, MetaSpriteTestRenderer& renderer, uint32_t width, uint32_t height, int layer = 0, bool negativeCamera = false)
{
	auto wait = [&](const std::function<bool()>& predicate) {
		for(int attempt = 0; attempt < 500; attempt++) {
			{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(predicate()) { return true; } }
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return false;
	};
	VideoConfig saved = emu->GetSettings()->GetVideoConfig();
	auto savedLayers = emu->GetVideoDecoder()->HasMetaBackgroundLayers();
	if(savedLayers) { return false; } // Dedicated fixtures enter with native output restored.
	vector<uint32_t> native, baseline;
	uint32_t before;
	{ std::lock_guard<std::mutex> lock(renderer.Mutex); native = renderer.Pixels; before = renderer.FrameNumber; }
	MetaBackgroundReplacement bg; bg.Id = 1; bg.Layer = layer; bg.Width = bg.Height = 1;
	bg.WrapX = bg.WrapY = true; bg.UseCpuCamera = true; bg.ExtendIntoMargins = true;
	bg.Artwork = {0xFFFFFFFF}; bg.MemoryConditions = {{0, 0, 0}};
	emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
	bool passed = wait([&]() {
		if(renderer.Width != width || renderer.Height != height || renderer.FrameNumber <= before + 2 || !renderer.Capture
			|| renderer.Capture->MemorySnapshot.size() != 1) { return false; }
		baseline = renderer.Pixels; return true;
	});
	for(int variant = 0; variant < 5 && passed; variant++) {
		VideoConfig cfg = saved; cfg.DecorativeWidescreen = true; cfg.AspectRatio = VideoAspectRatio::NoStretching;
		cfg.DecorativeCustomWidths = variant == 0; cfg.DecorativeLeftWidth = 5; cfg.DecorativeRightWidth = 9;
		cfg.DecorativeAspectRatio = 16.0 / 9; cfg.DecorativeSideBrightness = variant == 0 ? 37 : 100;
		cfg.DecorativeFadeWidth = variant == 0 ? 2 : 0; cfg.DecorativeViewportPosition = variant >= 2 ? 3 : 0;
		bg.UseWidescreenPosition = variant == 2 || variant == 3; bg.WidescreenPosition = 3;
		bg.WidescreenCameraMin = variant == 3 ? -100 : 0; bg.WidescreenCameraMax = variant == 3 ? 0 : 100;
		emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
		emu->GetSettings()->SetVideoConfig(cfg);
		uint32_t wideWidth = variant == 0 ? width + 14 : (uint32_t)std::ceil(height * 16.0 / 9);
		uint32_t left = variant == 0 ? 5 : variant == 2 ? 0 : variant == 3 ? wideWidth - width : (wideWidth - width) / 2;
		passed = wait([&]() {
			if(renderer.Width != wideWidth || renderer.Height != height) { return false; }
			for(uint32_t y = 0; y < height; y++) { for(uint32_t x = 0; x < width; x++) {
				if(renderer.Pixels[y * wideWidth + left + x] != baseline[y * width + x]) { return false; }
			} }
			for(uint32_t x = 0; x < wideWidth; x++) {
				if(x >= left && x < left + width) { continue; }
				uint32_t fade = variant == 0 ? std::min(x < left ? left - 1 - x : x - (left + width), 2u) : 1;
				uint32_t divisor = variant == 0 ? 2 : 1;
				uint32_t level = variant == 0 ? 100 * divisor - (100 - cfg.DecorativeSideBrightness) * fade : cfg.DecorativeSideBrightness;
				uint32_t shade = (255 * level + 50 * divisor) / (100 * divisor);
				if(renderer.Pixels[(height / 2) * wideWidth + x] != (0xFF000000 | shade * 0x010101)) { return false; }
			}
			return true;
		});
		if(!passed) { std::cerr << "Dedicated GBA/SNES widescreen variant failed: " << variant << std::endl; }
	}
	bg.UseWidescreenPosition = false; // Remaining presentation uses pack-independent placement.
	// Patterned canvases expose accidental native-map wrapping and distinguish
	// every 256-pixel page. Their CPU camera starts at 1014 or signed FF:F6=-10.
	uint32_t cameraAddress = dynamic_cast<GbaConsole*>(emu->GetConsole().get()) ? 0x02000068 : 0x68;
	int cameraX = negativeCamera ? -10 : 1014;
	for(int pngWidth : {256, 512, 1024}) {
		if(!passed) { break; }
		emu->GetSettings()->SetVideoConfig(saved);
		bg.Width = pngWidth; bg.Height = 256; bg.Artwork.resize((size_t)pngWidth * 256);
		bg.CameraX = {4, cameraAddress, cameraAddress + 1, 256, true};
		auto texture = [](int x, int y) { return 0xFF000000u | ((uint32_t)(x & 255) << 16) | ((uint32_t)(x >> 8) * 64 << 8) | (y & 255); };
		for(int y = 0; y < 256; y++) { for(int x = 0; x < pngWidth; x++) { bg.Artwork[(size_t)y * pngWidth + x] = texture(x, y); } }
		{ std::lock_guard<std::mutex> lock(renderer.Mutex); before = renderer.FrameNumber; }
		emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
		passed = wait([&]() {
			if(renderer.Width != width || renderer.Height != height || renderer.FrameNumber <= before + 2 || !renderer.Capture
				|| renderer.Capture->MemorySnapshot.size() != 3) { return false; }
			int decoded; if(!bg.CameraX.Decode(renderer.Capture->MemorySnapshot, decoded) || decoded != cameraX) { return false; }
			baseline = renderer.Pixels; return true;
		});
		VideoConfig cfg = saved; cfg.DecorativeWidescreen = true; cfg.DecorativeCustomWidths = true;
		cfg.DecorativeLeftWidth = 96; cfg.DecorativeRightWidth = 160; cfg.DecorativeViewportPosition = 0;
		cfg.DecorativeSideBrightness = 100; cfg.DecorativeFadeWidth = 0;
		emu->GetSettings()->SetVideoConfig(cfg);
		uint32_t wideWidth = width + 256;
		passed &= wait([&]() {
			if(renderer.Width != wideWidth || renderer.Height != height) { return false; }
			for(uint32_t y = 0; y < height; y++) { for(uint32_t x = 0; x < width; x++) {
				if(renderer.Pixels[y * wideWidth + x + 96] != baseline[y * width + x]) { return false; }
			} }
			for(uint32_t x = 0; x < wideWidth; x++) {
				if(x >= 96 && x < 96 + width) { continue; }
				int sourceX = (((int)x - 96 + cameraX) % pngWidth + pngWidth) % pngWidth;
				if(renderer.Pixels[(height / 2) * wideWidth + x] != texture(sourceX, height / 2)) { return false; }
			}
			return true;
		});
		if(!passed) { std::cerr << "Patterned widescreen PNG mismatch: width=" << pngWidth << ", camera=" << cameraX << std::endl; }
	}
	for(int flagVariant = 0; flagVariant < 5 && passed; flagVariant++) {
		VideoConfig cfg = saved; cfg.DecorativeWidescreen = true; cfg.DecorativeCustomWidths = true;
		cfg.DecorativeLeftWidth = 96; cfg.DecorativeRightWidth = 160; cfg.DecorativeViewportPosition = 3;
		cfg.DecorativeSideBrightness = 100; cfg.DecorativeFadeWidth = 0;
		bg.UseWidescreenPosition = true; bg.WidescreenPosition = 3; bg.WidescreenUseRamFlags = true;
		bg.WidescreenLeftFlags = {{flagVariant == 4 ? UINT32_MAX : cameraAddress + 2, (uint8_t)(flagVariant == 1 || flagVariant == 2 ? 2 : 1), 15}, {cameraAddress + 1, 0, 255, 1}};
		bg.WidescreenRightFlags = {{cameraAddress + 2, (uint8_t)(flagVariant == 0 ? 2 : flagVariant == 2 ? 3 : 1), 15}};
		emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
		emu->GetSettings()->SetVideoConfig(cfg);
		uint32_t wideWidth = width + 256, left = flagVariant == 0 ? 0 : flagVariant == 1 ? 256 : 96;
		passed = wait([&]() {
			if(renderer.Width != wideWidth || renderer.Height != height || !renderer.Capture || renderer.Capture->MemorySnapshot.size() != 4
				|| renderer.Capture->MemorySnapshot.count(UINT32_MAX) || !renderer.Capture->MemorySnapshot.count(cameraAddress + 2)) { return false; }
			for(uint32_t y = 0; y < height; y++) { for(uint32_t x = 0; x < width; x++) {
				if(renderer.Pixels[y * wideWidth + left + x] != baseline[y * width + x]) { return false; }
			} }
			return true;
		});
		if(!passed) { std::cerr << "RAM-boundary live variant failed: " << flagVariant << std::endl; }
	}
	for(int scene = 0; scene < 5 && passed; scene++) {
		bg.UseWidescreenPosition = true; bg.WidescreenPosition = 3; bg.WidescreenSlideSpeed = 0;
		bg.WidescreenCameraMin = scene == 0 ? cameraX : cameraX - 100; bg.WidescreenCameraMax = scene == 0 ? cameraX + 100 : cameraX;
		bg.WidescreenUseRamFlags = scene >= 2;
		bg.WidescreenLeftFlags = {{cameraAddress + 2, 1, 255}};
		bg.WidescreenRightFlags = {{cameraAddress + 2, (uint8_t)(scene == 3 ? 1 : 2), 255}};
		vector<MetaBackgroundReplacement> layers {bg};
		if(scene == 4) {
			layers[0].MemoryConditions = {{UINT32_MAX, 0, 0}};
			layers.push_back(bg); layers[1].Id = 2; layers[1].WidescreenPosition = 2;
		}
		emu->GetVideoDecoder()->SetMetaBackgroundLayers(std::move(layers));
		VideoConfig cfg = saved; cfg.DecorativeWidescreen = true; cfg.DecorativeCustomWidths = true;
		cfg.DecorativeLeftWidth = 96; cfg.DecorativeRightWidth = 160;
		cfg.DecorativeViewportPosition = 0;
		cfg.DecorativeSideBrightness = 100; cfg.DecorativeFadeWidth = 0;
		emu->GetSettings()->SetVideoConfig(cfg);
		uint32_t wideWidth = width + 256, left = scene == 0 || scene == 2 ? 0 : scene == 3 ? 96 : 256;
		passed = wait([&]() {
			if(renderer.Width != wideWidth || renderer.Height != height || !renderer.Capture || renderer.Capture->MemorySnapshot.size() != (scene == 2 || scene == 3 ? 4u : 3u)) { return false; }
			for(uint32_t y = 0; y < height; y++) { for(uint32_t x = 0; x < width; x++) {
				if(renderer.Pixels[y * wideWidth + x + left] != baseline[y * width + x]) { return false; }
			} }
			return true;
		});
		if(!passed) { std::cerr << "Per-PNG positioning scene failed: " << scene << std::endl; }
	}
	emu->GetSettings()->SetVideoConfig(saved); emu->GetVideoDecoder()->SetMetaBackgroundLayers({});
	passed &= wait([&]() { return renderer.Width == width && renderer.Height == height && renderer.Pixels == native; });
	if(passed) { std::cout << "PASS: dedicated GBA/SNES live PNG margins, native viewport preservation, custom widths, fade, brightness, CPU boundaries, 256/512/1024-pixel patterned canvases and restoration; camera=" << cameraX << std::endl; }
	return passed;
}

static bool MeasureLiveFrameTiming(Emulator* emu, MetaSpriteTestRenderer& renderer, const char* stage)
{
	if(emu->GetSettings()->GetEmulationSpeed() != 0) { return false; }
	{
		std::lock_guard<std::mutex> lock(renderer.Mutex);
		renderer.FrameIntervals.clear(); renderer.TimingFrame = 0; renderer.SkippedTimingFrames = 0;
		renderer.MeasureTiming = true;
	}
	vector<double> samples;
	uint32_t skipped = 0;
	for(int attempt = 0; attempt < 30000; attempt++) {
		{
			std::lock_guard<std::mutex> lock(renderer.Mutex);
			if(renderer.FrameIntervals.size() >= 120) {
				renderer.MeasureTiming = false; samples = renderer.FrameIntervals; skipped = renderer.SkippedTimingFrames; break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if(samples.empty()) {
		std::lock_guard<std::mutex> lock(renderer.Mutex); renderer.MeasureTiming = false; return false;
	}
	double total = 0; int overBudget = 0;
	for(double interval : samples) { total += interval; if(interval > 16.67) { overBudget++; } }
	std::sort(samples.begin(), samples.end());
	auto percentile = [&](int percent) { return samples[(samples.size() * percent + 99) / 100 - 1]; };
	std::cout << "LIVE TIMING: " << stage << "; " << samples.size() << " delivered-frame intervals, mean=" << total / samples.size()
		<< " ms, p50=" << percentile(50) << " ms, p95=" << percentile(95) << " ms, p99=" << percentile(99)
		<< " ms, max=" << samples.back() << " ms, over 16.67 ms=" << overBudget << ", skipped frames=" << skipped
		<< "; maximum-speed emulation/capture/decode/headless readback, excludes desktop presentation." << std::endl;
	return true;
}

static bool VerifyLiveMemoryConditions(Emulator* emu, MetaSpriteTestRenderer& renderer, MetaSpriteReplacement group, bool artworkVisible = true)
{
	uint8_t value;
	uint32_t hdWidth, hdHeight;
	{
		std::lock_guard<std::mutex> lock(renderer.Mutex);
		if(!renderer.Capture || renderer.Capture->MemorySnapshot.size() != 1 || !renderer.Capture->MemorySnapshot.count(0)) { return false; }
		value = renderer.Capture->MemorySnapshot.at(0);
		hdWidth = renderer.Width; hdHeight = renderer.Height;
	}
	auto waitFor = [&](bool enabled, bool missing, uint64_t newerThan = 0) {
		for(int attempt = 0; attempt < 500; attempt++) {
			{
				std::lock_guard<std::mutex> lock(renderer.Mutex);
				if(renderer.Capture && renderer.FrameNumber > newerThan && renderer.Width == (enabled ? hdWidth : hdWidth / 2)
					&& renderer.Height == (enabled ? hdHeight : hdHeight / 2)
					&& (missing ? renderer.Capture->MemorySnapshot.empty() : renderer.Capture->MemorySnapshot.count(0) == 1)) { return true; }
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return false;
	};
	group.MemoryConditions = {{0, value, 0xFF}};
	for(uint8_t comparison = 0; comparison < 6; comparison++) {
		for(uint8_t threshold : std::initializer_list<uint8_t>{0, value, 255}) {
			bool expected = comparison == 0 ? value == threshold : comparison == 1 ? value != threshold
				: comparison == 2 ? value > threshold : comparison == 3 ? value < threshold : comparison == 4 ? value >= threshold : value <= threshold;
			group.MemoryConditions = {{0, threshold, 0xFF, comparison}};
			uint64_t previous;
			{ std::lock_guard<std::mutex> lock(renderer.Mutex); previous = renderer.FrameNumber + 1; }
			emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
			if(!waitFor(expected, false, previous)) { return false; }
		}
	}
	group.MemoryConditions = {{0, value, 0xFF}};
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
	if(!waitFor(true, false)) { return false; }
	group.MemoryConditions[0].Value = value ^ 0x80;
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
	if(!waitFor(false, false)) { return false; }
	group.MemoryConditions = {{UINT32_MAX, 0, 0}};
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
	if(!waitFor(false, true)) { return false; }
	group.MemoryConditions = {{0, value, 0xFF}};
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
	if(!waitFor(true, false)) { return false; }
	auto checksum = [&]() {
		uint64_t hash = 1469598103934665603ULL;
		for(uint32_t pixel : renderer.Pixels) { hash = (hash ^ pixel) * 1099511628211ULL; }
		return hash;
	};
	uint64_t defaultChecksum, previousFrame;
	{
		std::lock_guard<std::mutex> lock(renderer.Mutex);
		defaultChecksum = checksum();
		previousFrame = renderer.FrameNumber;
	}
	auto waitForArtwork = [&](uint64_t previous, bool equal, uint64_t& observed) {
		for(int attempt = 0; attempt < 500; attempt++) {
			{
				std::lock_guard<std::mutex> lock(renderer.Mutex);
				if(renderer.Capture && renderer.FrameNumber > previousFrame + 1 && renderer.Width == hdWidth && renderer.Height == hdHeight
					&& renderer.Capture->MemorySnapshot.size() == 1 && renderer.Capture->MemorySnapshot.count(0)) {
					observed = checksum();
					if((observed == previous) == equal) { previousFrame = renderer.FrameNumber; return true; }
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return false;
	};
	MetaAlternateArtwork white;
	white.ArtworkOriginX = group.ArtworkOriginX; white.ArtworkOriginY = group.ArtworkOriginY;
	white.ArtworkScale = group.ArtworkScale;
	white.ArtworkWidth = group.ArtworkWidth; white.ArtworkHeight = group.ArtworkHeight;
	white.Artwork.assign(group.Artwork.size(), 0xFFFFFFFF);
	white.MemoryConditions = {{0, value, 0xFF}};
	MetaAlternateArtwork red = white;
	red.Artwork.assign(group.Artwork.size(), 0xFFFF0000);
	group.AlternateArtwork = {white, red};
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
	uint64_t whiteChecksum, redChecksum, restoredChecksum;
	if(!waitForArtwork(defaultChecksum, !artworkVisible, whiteChecksum)) { return false; }
	group.AlternateArtwork = {red, white};
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
	if(!waitForArtwork(whiteChecksum, !artworkVisible, redChecksum) || (artworkVisible && redChecksum == defaultChecksum)) { return false; }
	for(auto& alternate : group.AlternateArtwork) { alternate.MemoryConditions[0].Value = value ^ 0x80; }
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
	if(!waitForArtwork(defaultChecksum, true, restoredChecksum)) { return false; }
	MetaBackgroundReplacement background;
	{
		std::lock_guard<std::mutex> lock(renderer.Mutex);
		// SNES background capture/composition is still under development.
		if(renderer.Capture->SnesColorEffects) { return true; }
		bool found = false;
		for(const auto& pixel : renderer.Capture->Pixels) {
			if(pixel.BackgroundSample.Valid) { background.Layer = pixel.BackgroundSample.Layer; found = true; break; }
			for(const auto& sample : pixel.GbaBackgroundSamples) {
				if(sample.Valid) { background.Layer = sample.Layer; found = true; break; }
			}
			if(found) { break; }
		}
		if(!found) { return true; }
	}
	background.Id = 1; background.Width = background.Height = 2; background.Scale = 2;
	background.WrapX = background.WrapY = true; background.Artwork.assign(4, 0xFFA531D7);
	background.MemoryConditions = {{0, value, 0xFF}, {0, static_cast<uint8_t>(value & 0xF0), 0xF0}};
	group.AlternateArtwork.clear();
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
	uint64_t backgroundChecksum;
	if(!waitForArtwork(defaultChecksum, false, backgroundChecksum)) { std::cerr << "Background condition activation failed" << std::endl; return false; }
	// Every condition must match, even when conditions share an address.
	background.MemoryConditions[0].Value = value ^ 0x80;
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
	if(!waitForArtwork(defaultChecksum, true, restoredChecksum)) { std::cerr << "Background condition mismatch restoration failed" << std::endl; return false; }
	background.MemoryConditions = {{UINT32_MAX, 0, 0}};
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
	if(!waitForArtwork(defaultChecksum, true, restoredChecksum)) { std::cerr << "Background missing address restoration failed" << std::endl; return false; }
	background.MemoryConditions = {{0, value, 0xFF}};
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
	if(!waitForArtwork(backgroundChecksum, true, restoredChecksum)) { std::cerr << "Background condition reactivation failed" << std::endl; return false; }
	emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
	return waitForArtwork(defaultChecksum, true, restoredChecksum);
}

static bool VerifyChangingSnesMemory(Emulator* emu, MetaSpriteTestRenderer& renderer,
	MetaSpriteReplacement group, int spriteX, int spriteY, uint32_t nativeWidth, uint32_t nativeHeight)
{
	constexpr uint32_t stateAddress = 0x7E0000, maskedAddress = 0x7E0002;
	group.MemoryConditions = {{stateAddress, 0, 0}, {maskedAddress, 0xA0, 0xF0}};
	group.AlternateArtwork.clear();
	for(uint8_t state = 0; state < 2; state++) {
		MetaAlternateArtwork alternate;
		alternate.ArtworkScale = group.ArtworkScale;
		alternate.ArtworkWidth = group.ArtworkWidth; alternate.ArtworkHeight = group.ArtworkHeight;
		alternate.ArtworkOriginX = group.ArtworkOriginX; alternate.ArtworkOriginY = group.ArtworkOriginY;
		alternate.Artwork.assign(group.Artwork.size(), state ? 0xFFFFFFFF : 0xFF0000FF);
		alternate.MemoryConditions = {{stateAddress, state, 0xFF}, {maskedAddress, state, 1}};
		group.AlternateArtwork.push_back(std::move(alternate));
	}
	MetaBackgroundReplacement background;
	background.Id = 1; background.Layer = 0; background.Scale = 2;
	background.Width = background.Height = 2; background.WrapX = background.WrapY = true;
	background.Artwork.assign(4, 0xFF00FF00);
	background.MemoryConditions = {{stateAddress, 1, 0xFF}, {maskedAddress, 0xA1, 0xFF}};
	for(bool gateGroup : {false, true}) {
		uint32_t firstFrame;
		{ std::lock_guard<std::mutex> lock(renderer.Mutex); firstFrame = renderer.FrameNumber; }
		if(gateGroup) { group.MemoryConditions[0] = {stateAddress, 1, 0xFF}; }
		emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
		uint32_t previousFrame = firstFrame, transitions = 0, seen[2] = {};
		int previousState = -1;
		for(int attempt = 0; attempt < 5000 && (transitions < 4 || seen[0] < 6 || seen[1] < 6); attempt++) {
			{
				std::lock_guard<std::mutex> lock(renderer.Mutex);
				if(renderer.FrameNumber > firstFrame + 2 && renderer.FrameNumber != previousFrame && renderer.Capture) {
					previousFrame = renderer.FrameNumber;
					const auto& memory = renderer.Capture->MemorySnapshot;
					if(memory.size() != 2 || !memory.count(stateAddress) || !memory.count(maskedAddress)) { return false; }
					uint8_t state = memory.at(stateAddress);
					if(state > 1 || memory.at(maskedAddress) != (0xA0 | state)) { return false; }
					bool hd = !gateGroup || state == 1;
					uint32_t scale = hd ? 2 : 1;
					uint32_t expected = state ? 0xFF7FFF7F : gateGroup ? 0xFFFF0000 : 0xFF7F007F;
					if(renderer.Width != nativeWidth * scale || renderer.Height != nativeHeight * scale
						|| renderer.Pixels[((spriteY + 1) * scale) * renderer.Width + (spriteX + 1) * scale] != expected) {
						std::cerr << "Changing SNES memory/frame mismatch, state=" << (int)state << " gate=" << gateGroup << std::endl;
						return false;
					}
					seen[state]++;
					if(previousState >= 0 && previousState != state) { transitions++; }
					previousState = state;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if(transitions < 4 || seen[0] < 6 || seen[1] < 6) { return false; }
	}
	std::cout << "PASS: ROM-driven WRAM transitions select alternate PNGs and gate backgrounds/groups; snapshots contain only two configured addresses." << std::endl;
	return true;
}

class MetaSpriteTestMessages final : public IMessageManager
{
public:
	void DisplayMessage(string title, string message) override { std::cerr << title << ": " << message << std::endl; }
};

extern unique_ptr<Emulator> _emu;
shared_ptr<RecordedRomTest> _recordedRomTest;

extern "C"
{
	DllExport bool __stdcall CheckWidescreenPresentationInterop()
	{
		if(!_emu) { return false; }
		const auto& cfg = _emu->GetSettings()->GetVideoConfig();
		return cfg.DecorativeWidescreen && cfg.DecorativeViewportPosition == 3 && cfg.DecorativeCustomWidths
			&& cfg.DecorativeLeftWidth == 23 && cfg.DecorativeRightWidth == 71 && cfg.DecorativeSideBrightness == 37 && cfg.DecorativeFadeWidth == 12;
	}
	static uint32_t RunLiveAnimationProbe(Emulator* emu,MetaSpriteTestRenderer& renderer,MetaSpriteGraphic source,
		int x,int y,uint32_t width,uint32_t height,uint32_t nativeColor)
	{
		emu->GetSettings()->ClearFlag(EmulationFlags::MaximumSpeed);
		uint32_t address=0;
		std::function<void(uint32_t,uint8_t)> write;
		if(auto c=dynamic_cast<NesConsole*>(emu->GetConsole().get())) { address=0x700; write=[c](uint32_t a,uint8_t v) { c->GetMemoryManager()->DebugWrite((uint16_t)a,v); }; }
		else if(auto c=dynamic_cast<Gameboy*>(emu->GetConsole().get())) { address=0xC100; write=[c](uint32_t a,uint8_t v) { c->GetMemoryManager()->DebugWrite((uint16_t)a,v); }; }
		else if(auto c=dynamic_cast<GbaConsole*>(emu->GetConsole().get())) { address=0x203FFF0; write=[c](uint32_t a,uint8_t v) { c->GetMemoryManager()->DebugWrite(a,v); }; }
		else if(auto c=dynamic_cast<PceConsole*>(emu->GetConsole().get())) { address=0x20F0; write=[c](uint32_t a,uint8_t v) { c->GetMemoryManager()->DebugWrite((uint16_t)a,v); }; }
		else if(auto c=dynamic_cast<SmsConsole*>(emu->GetConsole().get())) { address=0xC1F0; write=[c](uint32_t a,uint8_t v) { c->GetMemoryManager()->DebugWrite((uint16_t)a,v); }; }
		else if(auto c=dynamic_cast<SnesConsole*>(emu->GetConsole().get())) { address=0x7E1FF0; write=[c](uint32_t a,uint8_t v) { c->GetMemoryManager()->Write(a,v,MemoryOperationType::Write); }; }
		else if(auto c=dynamic_cast<WsConsole*>(emu->GetConsole().get())) { address=0x3F00; write=[c](uint32_t a,uint8_t v) { c->GetMemoryManager()->DebugWrite(a,v); }; }
		if(!write) { return 1; }
		MetaPngPixelPool pixels;
		uint64_t sequenceIdentity=0;
		auto sequence=[&](int w,int h,MetaPngPlayback playback,const std::vector<uint32_t>& colors,const std::vector<uint32_t>& durations) {
			MetaPngArtworkAnimation animation; animation.RestartOnActivation=true; animation.Timeline=MetaPngTimeline(playback,durations);
			// Production packet parsers supply content identities. This direct native
			// fixture also needs distinct nonzero identities for save-state matching.
			animation.SourceIdentity={0x414E494D54455354ULL,++sequenceIdentity};
			for(auto color : colors) { MetaPngArtworkFrame image; image.Width=w; image.Height=h; image.Pixels=pixels.Intern(std::vector<uint32_t>((size_t)w*h,color)); animation.Frames.push_back(std::move(image)); }
			return animation;
		};
		MetaSpriteReplacement group; group.Id=1; group.ArtworkWidth=source.Width; group.ArtworkHeight=source.Height;
		group.Artwork.assign((size_t)source.Width*source.Height,0xFFFF00FF); group.MemoryConditions={{address,1,0xFF}};
		source.X=source.Y=0; source.HardwareSprite=-1; group.Members={source};
		group.Animation=sequence(source.Width,source.Height,MetaPngPlayback::Loop,{0xFF00FF00,0xFFFF00FF,0xFF00FF00},{2,3,1});
		MetaAlternateArtwork alternate; alternate.ArtworkWidth=source.Width; alternate.ArtworkHeight=source.Height; alternate.Artwork=group.Artwork;
		alternate.MemoryConditions={{address+1,1,0xFF}};
		alternate.Animation=sequence(source.Width,source.Height,MetaPngPlayback::PingPong,{0xFF0000FF,0xFFFFFFFF,0xFFFFFF00},{1,2,3}); group.AlternateArtwork={alternate};
		MetaBackgroundReplacement background; background.Id=1; background.CompositionPosition=1; background.Width=background.Height=1;
		background.WrapX=background.WrapY=true; background.Artwork={0}; background.MemoryConditions={{address,1,0xFF},{address+1,2,0xFF}};
		constexpr uint32_t backgroundDuration=60;
		background.Animation=sequence(1,1,MetaPngPlayback::PlayOnce,{0x800000FF,0},{backgroundDuration,7});
		auto gates=[&](uint8_t enabled,uint8_t selection) { auto lock=emu->AcquireLock(); write(address,enabled); write(address+1,selection); };
		{ auto lock=emu->AcquireLock(); write(address,1); write(address+1,0); emu->GetVideoDecoder()->SetMetaReplacements({group},{background}); }
		uint32_t flags=1; uint64_t firstDefault=UINT64_MAX, secondDefault=UINT64_MAX;
		// Expected colors/timeline visits are calculated independently of Select().
		auto observe=[&](const char* stage,uint8_t enabled,uint8_t selection,uint64_t& start,bool withBackground,bool requireHold=false,bool requireSpeculative=false,uint64_t expectedBackgroundOrigin=UINT64_MAX,bool replayBackground=false) {
			uint32_t seen=0, bgSeen=0, count=0, previous=UINT32_MAX; uint64_t backgroundStart=expectedBackgroundOrigin;
			uint32_t backgroundMask=expectedBackgroundOrigin!=UINT64_MAX && !replayBackground ? (requireHold ? 6u : 2u) : (requireHold ? 7u : 3u);
			bool sawSpeculative=false;
			for(int attempt=0;attempt<3000;attempt++) {
				sawSpeculative|=emu->IsRunAheadFrame();
				{
					std::lock_guard<std::mutex> lock(renderer.Mutex); const auto& capture=renderer.Capture;
					if(capture && capture->FrameNumber!=previous && renderer.Width==width && renderer.Height==height) {
						auto a=capture->MemorySnapshot.find(address), b=capture->MemorySnapshot.find(address+1);
						if(a!=capture->MemorySnapshot.end() && b!=capture->MemorySnapshot.end() && a->second==enabled && b->second==selection && capture->AnimationTiming) {
							if(capture->MemorySnapshot.size()!=2) { std::cerr << "Animation sampled unconfigured RAM: " << stage << std::endl; return false; }
							if(capture->FrameNumber!=renderer.FrameNumber) { std::cerr << "Animation capture/presentation frame mismatch: " << stage << std::endl; return false; }
							previous=capture->FrameNumber; uint64_t tick=0; uint32_t expected=nativeColor;
							if(enabled) {
								auto owners=std::static_pointer_cast<const std::vector<MetaSpriteReplacement>>(capture->AnimationTiming->Groups);
								if(!owners || owners->size()!=1) { return false; }
								const auto* animation=selection==1 ? &owners->front().AlternateArtwork.front().Animation : &owners->front().Animation;
								auto elapsed=capture->AnimationTiming->Elapsed.find(animation); if(elapsed==capture->AnimationTiming->Elapsed.end()) { return false; }
								tick=elapsed->second; if(tick>capture->FrameNumber) { return false; }
								uint64_t origin=capture->FrameNumber-tick;
								if(start==UINT64_MAX) { start=origin; } else if(start!=origin) { std::cerr << "Animation clock restarted unexpectedly: " << stage << std::endl; return false; }
								if(selection==1) {
									uint64_t visit=tick%8; uint32_t index=visit<1 ? 0 : visit<3 ? 1 : visit<6 ? 2 : 1;
									expected=index==0 ? 0xFF0000FF : index==1 ? 0xFFFFFFFF : 0xFFFFFF00; seen|=1u<<index; if(visit>=6) { seen|=8; }
								} else {
									uint64_t visit=tick%6; uint32_t index=visit<2 ? 0 : visit<5 ? 1 : 2;
									expected=index==1 ? 0xFFFF00FF : 0xFF00FF00; seen|=1u<<index;
								}
								if(withBackground) {
									auto layers=std::static_pointer_cast<const std::vector<MetaBackgroundReplacement>>(capture->AnimationTiming->Backgrounds);
									if(!layers || layers->size()!=1) { return false; }
									auto bgElapsed=capture->AnimationTiming->Elapsed.find(&layers->front().Animation); if(bgElapsed==capture->AnimationTiming->Elapsed.end()) { return false; }
									uint64_t bgTick=bgElapsed->second;
									if(bgTick>capture->FrameNumber) { return false; }
									uint64_t bgOrigin=capture->FrameNumber-bgTick;
									if(backgroundStart==UINT64_MAX) { backgroundStart=bgOrigin; }
									else if(backgroundStart!=bgOrigin) { std::cerr << "Background clock restarted unexpectedly: " << stage << std::endl; return false; }
									if(bgTick<backgroundDuration) { expected=0xFF000080 | (((expected>>16)&255)*127/255<<16) | (((expected>>8)&255)*127/255<<8) | ((expected&255)*127/255); bgSeen|=1; }
									else { bgSeen|=2; if(bgTick>=backgroundDuration+7) { bgSeen|=4; } }
								}
							}
							if(!enabled) { for(const auto& elapsed : capture->AnimationTiming->Elapsed) { if(elapsed.second!=0) { return false; } } }
							uint32_t actual=renderer.Pixels[(size_t)y*width+x];
							if(actual!=expected) { std::cerr << "Animation pixel mismatch: " << stage << " frame=" << capture->FrameNumber << " tick=" << tick << " actual=" << std::hex << actual << " expected=" << expected << std::dec << std::endl; return false; }
							count++;
							if((!requireSpeculative || sawSpeculative) && (!enabled ? count>=3 : ((seen&(selection==1 ? 15u : 7u))==(selection==1 ? 15u : 7u) && (!withBackground || (bgSeen&backgroundMask)==backgroundMask)))) { return true; }
						}
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
			std::cerr << "Animation observation timed out: " << stage << " seen=" << seen << " bg=" << bgSeen << std::endl; return false;
		};
		if(!observe("default loop",1,0,firstDefault,false)) { return flags; } flags|=2;
		uint64_t alternateStart=UINT64_MAX; gates(1,1);
		if(!observe("alternate ping-pong",1,1,alternateStart,false)) { return flags; } flags|=4;
		gates(1,0); if(!observe("default re-entry",1,0,secondDefault,false) || secondDefault<=firstDefault) { return flags; } flags|=8;
		gates(1,2); if(!observe("background play-once/alpha",1,2,secondDefault,true,true)) { return flags; } flags|=16;
		gates(1,0); if(!observe("background condition off",1,0,secondDefault,false)) { return flags; } flags|=32;
		uint64_t unused=UINT64_MAX; gates(0,0); if(!observe("native restoration",0,0,unused,false)) { return flags; } flags|=64;
		uint64_t reactivated=UINT64_MAX; gates(1,0); if(!observe("group reactivation",1,0,reactivated,false) || reactivated<=secondDefault) { return flags; } flags|=128;
		emu->Pause();
		uint32_t pausedFrame=0, pausedPixel=0;
		{ auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode(); std::lock_guard<std::mutex> renderLock(renderer.Mutex); pausedFrame=renderer.FrameNumber; pausedPixel=renderer.Pixels[(size_t)y*width+x]; }
		std::this_thread::sleep_for(std::chrono::milliseconds(60)); bool frozen;
		{ std::lock_guard<std::mutex> lock(renderer.Mutex); frozen=renderer.FrameNumber==pausedFrame && renderer.Pixels[(size_t)y*width+x]==pausedPixel; }
		emu->Resume(); if(!frozen) { return flags; } flags|=256;
		{ auto lock=emu->AcquireLock(); emu->GetSettings()->GetEmulationConfig().EmulationSpeed=200; }
		if(!observe("double-speed emulated timing",1,0,reactivated,false)) { return flags; } flags|=512;
		for(uint32_t ahead : {1u,2u}) {
			{ auto lock=emu->AcquireLock(); auto& config=emu->GetSettings()->GetEmulationConfig(); config.EmulationSpeed=100; config.RunAheadFrames=ahead; }
			if(!observe("run-ahead default animation",1,0,reactivated,false,false,true)) { return flags; }
			flags|=ahead==1 ? 2048 : 4096;
		}
		gates(1,2);
		if(!observe("run-ahead background animation",1,2,reactivated,true,true,true)) { return flags; } flags|=8192;
		std::stringstream runAheadSaved; uint32_t savedPresentation=0; uint64_t savedBackgroundOrigin=UINT64_MAX;
		{
			auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
			emu->Serialize(runAheadSaved,false,0);
			std::lock_guard<std::mutex> renderLock(renderer.Mutex); savedPresentation=renderer.FrameNumber;
			const auto& capture=renderer.Capture;
			if(!capture || !capture->AnimationTiming) { return flags; }
			auto owners=std::static_pointer_cast<const std::vector<MetaBackgroundReplacement>>(capture->AnimationTiming->Backgrounds);
			if(!owners || owners->size()!=1) { return flags; }
			auto elapsed=capture->AnimationTiming->Elapsed.find(&owners->front().Animation);
			if(elapsed==capture->AnimationTiming->Elapsed.end() || elapsed->second>capture->FrameNumber) { return flags; }
			savedBackgroundOrigin=capture->FrameNumber-elapsed->second;
		}
		if(!observe("run-ahead after save",1,2,reactivated,true,true,true,savedBackgroundOrigin)) { return flags; }
		{
			auto lock=emu->AcquireLock(); runAheadSaved.seekg(0);
			if(emu->Deserialize(runAheadSaved,SaveStateManager::FileFormatVersion,false,std::nullopt,false)!=DeserializeResult::Success) { return flags; }
		}
		bool sawRestoredFrame=false;
		for(int attempt=0;attempt<1000;attempt++) {
			{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.FrameNumber<=savedPresentation+2) { sawRestoredFrame=true; break; } }
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if(!sawRestoredFrame || !observe("run-ahead restored sprite/background phases",1,2,reactivated,true,true,true,savedBackgroundOrigin)) { return flags; } flags|=16384;
		gates(1,0); if(!observe("background reset before early save",1,0,reactivated,false)) { return flags; }
		gates(1,2); std::stringstream earlySaved; bool savedEarly=false;
		for(int attempt=0;attempt<1000 && !savedEarly;attempt++) {
			{
				auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
				{
					std::lock_guard<std::mutex> renderLock(renderer.Mutex); const auto& capture=renderer.Capture;
					if(capture && capture->AnimationTiming && capture->MemorySnapshot.count(address+1) && capture->MemorySnapshot.at(address+1)==2) {
						auto owners=std::static_pointer_cast<const std::vector<MetaBackgroundReplacement>>(capture->AnimationTiming->Backgrounds);
						if(owners && owners->size()==1) {
							auto elapsed=capture->AnimationTiming->Elapsed.find(&owners->front().Animation);
							if(elapsed!=capture->AnimationTiming->Elapsed.end() && elapsed->second<backgroundDuration && elapsed->second<=capture->FrameNumber) {
								savedPresentation=capture->FrameNumber; savedBackgroundOrigin=capture->FrameNumber-elapsed->second; savedEarly=true;
							}
						}
					}
				}
				if(savedEarly) { emu->Serialize(earlySaved,false,0); }
			}
			if(!savedEarly) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
		}
		if(!savedEarly || !observe("early background save then transition",1,2,reactivated,true,true,true,savedBackgroundOrigin,true)) { return flags; }
		{ auto lock=emu->AcquireLock(); earlySaved.seekg(0); if(emu->Deserialize(earlySaved,SaveStateManager::FileFormatVersion,false,std::nullopt,false)!=DeserializeResult::Success) { return flags; } }
		if(!observe("early background restored transition",1,2,reactivated,true,true,true,savedBackgroundOrigin,true)) { return flags; } flags|=32768;
		{ auto lock=emu->AcquireLock(); emu->GetSettings()->GetEmulationConfig().RunAheadFrames=0; }
		{
			auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
			write(address,1); write(address+1,2); emu->GetVideoDecoder()->SetMetaReplacements({group},{background});
			emu->GetSettings()->GetPreferences().RewindBufferSize=64; emu->GetRewindManager()->InitHistory();
			std::lock_guard<std::mutex> renderLock(renderer.Mutex); renderer.ReplayFrames.clear(); renderer.RecordReplayFrames=true;
		}
		bool recorded=false;
		// Rewind keeps a 30-frame preparation block. Record enough complete
		// checkpoints to compare 110 reverse frames, including the first PNG phase.
		for(int attempt=0;attempt<6000;attempt++) {
			{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.ReplayFrames.size()>=150) { recorded=true; break; } }
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		if(!recorded) { std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr << "Rewind recording incomplete: " << renderer.ReplayFrames.size() << " frames" << std::endl; return flags; }
		{
			auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
			{ std::lock_guard<std::mutex> renderLock(renderer.Mutex); renderer.RecordReplayFrames=false; renderer.CheckReplayFrames=true; }
			// Do not allow an unrecorded forward frame between the oracle and rewind.
			emu->GetRewindManager()->StartRewinding();
		}
		bool replayed=false;
		for(int attempt=0;attempt<6000;attempt++) {
			{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.ReplayErrors) { break; } if(renderer.ReplayMatches>=110) { replayed=true; break; } }
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		{
			auto lock=emu->AcquireLock();
			{ std::lock_guard<std::mutex> renderLock(renderer.Mutex); renderer.CheckReplayFrames=false; renderer.ReplayFrames.clear(); }
			emu->GetRewindManager()->StopRewinding(true,false);
		}
		if(!replayed) { std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr << "Rewind replay incomplete: matches=" << renderer.ReplayMatches << " errors=" << renderer.ReplayErrors << std::endl; return flags; } flags|=65536;
		emu->GetVideoDecoder()->SetMetaReplacements({},{});
		for(int attempt=0;attempt<1000;attempt++) {
			{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.Capture && renderer.Capture->MemorySnapshot.empty() && !renderer.Capture->AnimationTiming && renderer.Pixels[(size_t)y*width+x]==nativeColor) { flags|=1024; break; } }
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		return flags;
	}

	static uint32_t RunForegroundOverlayGameplayTestImpl(const char* filename,bool animationOnly)
	{
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& nesConfig = emu->GetSettings()->GetNesConfig(); nesConfig.RamPowerOnState = RamState::AllZeros; nesConfig.IsFullColorPalette = true; nesConfig.NtscOverscan = {};
		for(int i = 0; i < 512; i++) { nesConfig.UserPalette[i] = (i & 63) == 1 ? 0xFFFF0000 : (i & 63) == 2 ? 0xFF00FF00 : 0xFF0000FF; }
		auto& gbConfig = emu->GetSettings()->GetGameboyConfig(); gbConfig.BlendFrames = false; gbConfig.GbcAdjustColors = false; gbConfig.RamPowerOnState = RamState::AllZeros;
		gbConfig.Model = string(filename).find(".gbc") != string::npos ? GameboyModel::GameboyColor : GameboyModel::Gameboy;
		auto& gbaConfig = emu->GetSettings()->GetGbaConfig(); gbaConfig.SkipBootScreen = true; gbaConfig.DisableFrameSkipping = true;
		gbaConfig.BlendFrames = false; gbaConfig.GbaAdjustColors = false; gbaConfig.RamPowerOnState = RamState::AllZeros;
		emu->GetSettings()->GetPcEngineConfig().DisableFrameSkipping = true;
		emu->GetSettings()->GetSmsConfig().RamPowerOnState = RamState::AllZeros;
		auto& snesConfig = emu->GetSettings()->GetSnesConfig(); snesConfig.DisableFrameSkipping = true; snesConfig.RamPowerOnState = RamState::AllZeros; snesConfig.ColorCorrection = SnesColorCorrectionMode::None;
		auto& wsConfig = emu->GetSettings()->GetWsConfig(); wsConfig.UseBootRom = false; wsConfig.LcdShowIcons = false; wsConfig.BlendFrames = false; wsConfig.LcdAdjustColors = false;
		MetaSpriteTestRenderer renderer; uint32_t flags = 0;
		auto wait = [&](const std::function<bool()>& check) { for(int i = 0; i < 500; i++) { { std::lock_guard<std::mutex> lock(renderer.Mutex); if(check()) { return true; } } std::this_thread::sleep_for(std::chrono::milliseconds(10)); } return false; };
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			if(auto c = dynamic_cast<NesConsole*>(emu->GetConsole().get())) { c->GetPpu()->SetMetaSpriteCapture(true); }
			if(auto c = dynamic_cast<Gameboy*>(emu->GetConsole().get())) { c->GetPpu()->SetMetaSpriteCapture(true); }
			if(auto c = dynamic_cast<GbaConsole*>(emu->GetConsole().get())) { c->GetPpu()->SetMetaSpriteCapture(true); }
			if(auto c = dynamic_cast<PceConsole*>(emu->GetConsole().get())) { c->GetVdc()->SetMetaSpriteCapture(true); }
			if(auto c = dynamic_cast<SmsConsole*>(emu->GetConsole().get())) { c->GetVdp()->SetMetaSpriteCapture(true); }
			if(auto c = dynamic_cast<SnesConsole*>(emu->GetConsole().get())) { c->GetPpu()->SetMetaSpriteCapture(true); }
			if(auto c = dynamic_cast<WsConsole*>(emu->GetConsole().get())) { c->GetPpu()->SetMetaSpriteCapture(true); }
			// GBA/SNES retain background stages only while a background pack needs them.
			MetaBackgroundReplacement captureRequest; captureRequest.Id = 1; captureRequest.CompositionPosition = 1;
			captureRequest.Width = captureRequest.Height = 1; captureRequest.WrapX = captureRequest.WrapY = true; captureRequest.Artwork = {0};
			emu->GetVideoDecoder()->SetMetaBackgroundLayers({captureRequest});
			int cropLeft = dynamic_cast<PceConsole*>(emu->GetConsole().get()) ? PceConstants::GetLeftOverscan(4) : 0;
			MetaSpriteGraphic source; int x = 0, y = 0; uint32_t nativeColor = 0, width = 0, height = 0;
			if(wait([&]() {
				if(!renderer.Capture || renderer.FrameNumber < 12) { return false; }
				const auto& frame = *renderer.Capture;
				auto candidates = !frame.NesRows.empty() ? MetaSpriteCompositor::BuildNesCandidates(frame, [&](uint16_t c) { return nesConfig.UserPalette[c & 511]; }) : frame.DecodedRows.empty() ? MetaSpriteCompositor::BuildGameboyCandidates(frame) : MetaSpriteCompositor::BuildDecodedCandidates(frame);
				for(const auto& candidate : candidates) {
					if(candidate.HardwareSprite != 0) { continue; }
					int px = candidate.X + candidate.Width / 2, py = candidate.Y + candidate.Height / 2;
					if(px < cropLeft || px >= (int)renderer.Width + cropLeft || py < 0 || py >= (int)renderer.Height || px >= (int)frame.Width || py >= (int)frame.Height) { continue; }
					const auto& pixel = frame.Pixels[(size_t)py * frame.Width + px];
					bool sample = pixel.BackgroundSample.Valid && pixel.BackgroundSample.Layer == 0;
					sample |= pixel.GbaBackgroundSamples[0].Valid;
					if(frame.SnesBackgrounds.size() == frame.Pixels.size()) { sample |= frame.SnesBackgrounds[(size_t)py * frame.Width + px].Layers[0].Sample.Valid; }
					if(frame.BackgroundUnderlays.size() == frame.Pixels.size()) { const auto& lower = frame.BackgroundUnderlays[(size_t)py * frame.Width + px]; sample |= lower.Sample.Valid && lower.Sample.Layer == 0; }
					if(!sample || pixel.VisibleSprite != candidate.HardwareSprite) { continue; }
					source = candidate; x = px - cropLeft; y = py; width = renderer.Width; height = renderer.Height; nativeColor = renderer.Pixels[(size_t)y * width + x]; return true;
				} return false;
			})) {
                if(animationOnly) {
                    flags=RunLiveAnimationProbe(emu.get(),renderer,source,x,y,width,height,nativeColor);
                    emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer); emu->Release(); return flags;
                }
				flags |= 1;
				MetaBackgroundReplacement bg; bg.Id = 1; bg.CompositionPosition = 1; bg.Width = bg.Height = 1; bg.WrapX = bg.WrapY = true; bg.Artwork = {0xFF00FFFF}; bg.MemoryConditions = {{0, 0, 0}};
				uint32_t afterFrame = 0;
				auto mark = [&]() { std::lock_guard<std::mutex> lock(renderer.Mutex); afterFrame = renderer.FrameNumber + 2; };
				auto shown = [&](uint32_t color) { return renderer.FrameNumber > afterFrame && renderer.Width == width && renderer.Height == height && renderer.Pixels[(size_t)y * width + x] == color; };
				mark(); emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
				if(wait([&]() { return shown(0xFF00FFFF) && renderer.Capture && renderer.Capture->MemorySnapshot.size() == 1 && renderer.Capture->MemorySnapshot.count(0); })) { flags |= 2; }
				mark(); emu->GetVideoDecoder()->SetMetaBackgroundLayers({}); if(wait([&]() { return shown(nativeColor); })) { flags |= 4; }
				MetaSpriteReplacement group; group.Id = 1; group.ArtworkWidth = source.Width; group.ArtworkHeight = source.Height; group.Artwork.assign((size_t)source.Width * source.Height, 0xFFFF00FF);
				source.X = source.Y = 0; source.HardwareSprite = -1; group.Members = {source};
				mark(); emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
				if(wait([&]() { return shown(0xFFFF00FF); })) {
					flags |= 8; bg.Artwork = {0x800000FF}; mark(); emu->GetVideoDecoder()->SetMetaReplacements({group}, {bg});
					if(wait([&]() { return shown(0xFF7F00FF); })) { flags |= 16; }
					bg.Artwork = {0}; mark(); emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg}); if(wait([&]() { return shown(0xFFFF00FF); })) { flags |= 32; }
					bg.Artwork = {0xFF00FFFF}; bg.MemoryConditions[0].Comparison = 1; mark(); emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg}); if(wait([&]() { return shown(0xFFFF00FF); })) { flags |= 64; }
				}
				mark(); emu->GetVideoDecoder()->SetMetaReplacements({}, {}); if(wait([&]() { return shown(nativeColor) && renderer.Capture && renderer.Capture->MemorySnapshot.empty(); })) { flags |= 128; }
				if(auto nes = dynamic_cast<NesConsole*>(emu->GetConsole().get())) {
					auto configure = [&](bool behind, bool opaqueBg) {
						auto lock = emu->AcquireLock(); auto mm = nes->GetMemoryManager();
						auto write = [&](uint16_t address, uint8_t value) { mm->Write(address, value, MemoryOperationType::Write); nes->GetCpu()->Exec(); };
						write(0x2001, 0); write(0x2003, 2); write(0x2004, behind ? 0x20 : 0);
						// A later foreground OBJ with a distinct blue palette shares the position.
						write(0x2003, 4); for(uint8_t value : {uint8_t(7), uint8_t(1), uint8_t(1), uint8_t(8)}) { write(0x2004, value); }
						write(0x2003, 0); write(0x2006, 0x3F); write(0x2006, 0x15); write(0x2007, 3);
						for(uint8_t address : {uint8_t(1), uint8_t(5), uint8_t(9), uint8_t(13)}) { write(0x2006, 0x3F); write(0x2006, address); write(0x2007, 2); }
						if(opaqueBg) { write(0x2006, 0x20); write(0x2006, 0); for(int tile = 0; tile < 4096; tile++) { write(0x2007, 1); } }
						write(0x2006, 0x20); write(0x2006, 0); write(0x2005, 253); write(0x2005, 237); write(0x2000, 3); write(0x2001, 0x1E);
					};
					bool bandsPassed = true; bg.MemoryConditions[0].Comparison = 0; bg.HideOriginalLayer = false;
					for(bool behind : {false, true}) {
						if(!bandsPassed) { break; }
						configure(behind, false);
						for(int replacement = 0; replacement < 2; replacement++) { for(int position : {4, 5, 6, 1}) {
							bg.CompositionPosition = position; bg.Artwork = {0xFF00FFFF}; mark(); emu->GetVideoDecoder()->SetMetaReplacements(replacement ? vector<MetaSpriteReplacement>{group} : vector<MetaSpriteReplacement>{}, {bg});
							if(!bandsPassed) { break; }
							bool passed = wait([&]() { return shown(position == 1 || (behind && position >= 5) ? 0xFF00FFFFu : replacement ? 0xFFFF00FFu : nativeColor); });
							if(!passed) { std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr << "NES band mismatch: behind=" << behind << " replacement=" << replacement << " position=" << position << " actual=" << std::hex << renderer.Pixels[(size_t)y*renderer.Width+x] << std::dec << std::endl; }
							bandsPassed &= passed;
						} }
					}
					configure(true, true);
					for(int position : {4, 5, 6, 1}) {
						if(!bandsPassed) { break; }
						bg.CompositionPosition = position; mark(); emu->GetVideoDecoder()->SetMetaReplacements({group}, {bg});
						bandsPassed &= wait([&]() { return shown(position >= 6 || position == 1 ? 0xFF00FFFFu : 0xFF00FF00u); });
					}
					bg.CompositionPosition = 4; bg.Artwork = {0}; group.Artwork.assign(group.Artwork.size(), 0x80FF00FF);
					mark(); emu->GetVideoDecoder()->SetMetaReplacements({group}, {bg}); bandsPassed = bandsPassed && wait([&]() { return shown(0xFF00807F); });
					group.Artwork.assign(group.Artwork.size(), 0); mark(); emu->GetVideoDecoder()->SetMetaReplacements({group}, {bg}); bandsPassed = bandsPassed && wait([&]() { return shown(0xFF0000FF); });
					emu->GetVideoDecoder()->SetMetaReplacements({}, {});
					if(!bandsPassed) { flags |= 1024; } else { std::cout << "PASS: live NES insertion bands with front/behind native and replacement OBJ, opaque tiles, lower native reveal and OAM alpha outcomes.\n"; }
					// Move the later front OBJ beside the behind-BG member and recognize
					// the group from actual fetched graphics, including both priorities.
					{ auto lock = emu->AcquireLock(); auto mm = nes->GetMemoryManager(); auto write = [&](uint16_t address, uint8_t value) { mm->Write(address, value, MemoryOperationType::Write); nes->GetCpu()->Exec(); }; write(0x2001, 0); write(0x2003, 7); write(0x2004, 16); write(0x2003, 0); write(0x2001, 0x1E); }
					MetaSpriteReplacement mixed; mixed.Id = 2; mixed.ArtworkWidth = 24; mixed.ArtworkHeight = 8; mixed.Artwork.assign(192, 0xFFFF00FF);
					mark(); bool mixedPassed = wait([&]() {
						if(renderer.FrameNumber <= afterFrame || !renderer.Capture) { return false; }
						auto captured = MetaSpriteCompositor::BuildNesCandidates(*renderer.Capture, [&](uint16_t c) { return nesConfig.UserPalette[c & 511]; });
						mixed.Members.clear();
						for(const auto& member : captured) { if(member.HardwareSprite == 0 || member.HardwareSprite == 1) { mixed.Members.push_back(member); } }
						if(mixed.Members.size() != 2 || mixed.Members[1].X - mixed.Members[0].X != 8) { return false; }
						int left = mixed.Members[0].X, top = mixed.Members[0].Y;
						for(auto& member : mixed.Members) { member.X -= left; member.Y -= top; member.HardwareSprite = -1; }
						return true;
					});
					if(!mixedPassed) { std::cerr << "NES mixed member capture failed." << std::endl; }
					if(mixedPassed) {
						mark(); emu->GetVideoDecoder()->SetMetaReplacements({mixed}, {});
						mixedPassed = wait([&]() { return shown(0xFF00FF00) && renderer.Pixels[(size_t)y*width+x+8] == 0xFFFF00FF && renderer.Pixels[(size_t)y*width+x+16] == 0xFFFF00FF; });
						if(!mixedPassed) { std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr << "NES mixed opaque mismatch: " << std::hex << renderer.Pixels[(size_t)y*width+x] << "," << renderer.Pixels[(size_t)y*width+x+8] << "," << renderer.Pixels[(size_t)y*width+x+16] << std::dec << std::endl; }
						mixed.Artwork.assign(192, 0x80FF00FF); mark(); emu->GetVideoDecoder()->SetMetaReplacements({mixed}, {});
						mixedPassed &= wait([&]() { return shown(0xFF00FF00) && renderer.Pixels[(size_t)y*width+x+8] == 0xFF807F80; });
						// Overlap the members, shift the artwork left, and retain a right margin.
						{ auto lock = emu->AcquireLock(); auto mm = nes->GetMemoryManager(); auto write = [&](uint16_t address, uint8_t value) { mm->Write(address, value, MemoryOperationType::Write); nes->GetCpu()->Exec(); }; write(0x2001, 0); write(0x2003, 7); write(0x2004, 12); write(0x2003, 0); write(0x2001, 0x1E); }
						mixed.Members[1].X = 4; mixed.ArtworkOriginX = 2; mixed.ArtworkWidth = 28; mixed.Artwork.assign(224, 0xFFFF00FF);
						mark(); emu->GetVideoDecoder()->SetMetaReplacements({mixed}, {});
						bool overlapPassed = wait([&]() { return shown(0xFF00FF00) && renderer.Pixels[(size_t)y*width+x-6] == 0xFF00FF00 && renderer.Pixels[(size_t)y*width+x+4] == 0xFFFF00FF && renderer.Pixels[(size_t)y*width+x+20] == 0xFFFF00FF; });
						mixed.Artwork.assign(224, 0x80FF00FF); mark(); emu->GetVideoDecoder()->SetMetaReplacements({mixed}, {});
						overlapPassed &= wait([&]() { return shown(0xFF00FF00) && renderer.Pixels[(size_t)y*width+x+4] == 0xFF807F80; });
						if(!overlapPassed) { std::cerr << "NES overlapping member/origin check failed." << std::endl; }
						mixedPassed &= overlapPassed;
					}
					if(!mixedPassed) { flags |= 1024; } else { std::cout << "PASS: live NES mixed-priority group capture, overlap OAM ties, shifted origin, oversized margins and partial PNG alpha.\n"; }
					emu->GetVideoDecoder()->SetMetaReplacements({}, {});
				}
				if(auto gb = dynamic_cast<Gameboy*>(emu->GetConsole().get()); gb && string(filename).find(".gbc") == string::npos) {
					// Step the real DMG PPU while the emulation thread is paused. Sweep
					// an OBP1 write across the fetched row, without editing capture data.
					bool rasterPassed = false;
					{ auto lock = emu->AcquireLock(); auto ppu = gb->GetPpu(); auto mm = gb->GetMemoryManager();
						auto stepUntil = [&](const std::function<bool()>& done) { for(int cycle = 0; cycle < 100000; cycle++) { if(done()) { return true; } ppu->Exec<true>(); } return false; };
						bool ready = stepUntil([&]() { return ppu->GetScanline() == 144; });
						if(ready) { mm->Write<MemoryOperationType::Write>(0xFE04, 24); mm->Write<MemoryOperationType::Write>(0xFE05, 16); mm->Write<MemoryOperationType::Write>(0xFE06, 1); mm->Write<MemoryOperationType::Write>(0xFE07, 0x10); }
						for(int cut = 100; ready && !rasterPassed && cut <= 140; cut += 2) {
							ready = stepUntil([&]() { return ppu->GetScanline() == 0; });
							mm->Write<MemoryOperationType::Write>(0xFF49, 0xE4);
							ready &= stepUntil([&]() { return ppu->GetScanline() == 8 && ppu->GetCycle() >= cut; });
							mm->Write<MemoryOperationType::Write>(0xFF49, 0xEC);
							uint32_t target = ppu->GetFrameCount() + 1;
							ready &= stepUntil([&]() { return ppu->GetFrameCount() >= target; });
							if(!ready) { break; }
							ready &= wait([&]() {
								if(renderer.FrameNumber < target || !renderer.Capture) { return false; }
								for(const auto& row : renderer.Capture->Rows) {
									if(row.Sprite != 1 || row.Scanline != 8 || row.OutputColorMask != 0xFF) { continue; }
									bool before = false, after = false;
									for(int pixel = 0; pixel < 8; pixel++) { before |= row.OutputColors[pixel] == row.Palette[1]; after |= row.OutputColors[pixel] != row.Palette[1]; }
									if(before && after) {
										auto candidates = MetaSpriteCompositor::BuildGameboyCandidates(*renderer.Capture);
										auto sourceMember = std::find_if(candidates.begin(), candidates.end(), [](const auto& member) { return member.HardwareSprite == 0; });
										if(sourceMember == candidates.end()) { continue; }
										MetaSpriteReplacement transparent; transparent.ArtworkWidth = transparent.ArtworkHeight = 8; transparent.Artwork.assign(64, 0);
										auto member = *sourceMember; member.X = member.Y = 0; member.HardwareSprite = -1; transparent.Members = {member};
										MetaSpriteCompositor probe; probe.SetGroups({transparent});
										auto convert = [](uint16_t color) { auto channel = [](int c) { return (c << 3) | (c >> 2); }; return 0xFF000000u | (channel(color & 31) << 16) | (channel((color >> 5) & 31) << 8) | channel((color >> 10) & 31); };
										auto revealed = probe.Apply(renderer.Pixels.data(), renderer.Width, renderer.Height, *renderer.Capture, 0, 0, convert);
										rasterPassed = true;
										for(int pixel = 0; pixel < 8; pixel++) { rasterPassed &= revealed[(size_t)row.Scanline * renderer.Width + row.X + pixel] == convert(row.OutputColors[pixel]); }
										return true;
									}
								} return true;
							});
						}
					}
					if(!rasterPassed) { flags |= 4096; } else { std::cout << "PASS: live DMG midscanline OBP1 write changes hidden lower-OBJ output samples while fetched matching colors remain intact.\n"; }
				}
				if(auto gb = dynamic_cast<Gameboy*>(emu->GetConsole().get()); gb && string(filename).find(".gbc") != string::npos) {
					{ auto lock = emu->AcquireLock(); auto mm = gb->GetMemoryManager(); auto write = [&](uint16_t address, uint8_t value) { mm->Write<MemoryOperationType::Write>(address, value); };
						write(0xFF40, 0); write(0xFF4F, 0); for(int tile = 0; tile < 1024; tile++) { write((uint16_t)(0x9800 + tile), 1); }
						write(0xFE03, 0x80); write(0xFE04, 24); write(0xFE05, 16); write(0xFE06, 1); write(0xFE07, 1);
						write(0xFF68, 0x82); write(0xFF69, 0xE0); write(0xFF69, 3);
						write(0xFF6A, 0x8A); write(0xFF6B, 0); write(0xFF6B, 0x7C); write(0xFF40, 0x93);
					}
					bool orderedPassed = true;
					for(uint32_t alpha : {255u, 0u, 128u}) {
						group.Artwork.assign(group.Artwork.size(), (alpha << 24) | 0xFF00FF);
						mark(); emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
						uint32_t expected = alpha == 255 ? 0xFF00FF00 : alpha == 0 ? 0xFF0000FF : 0xFF00807F;
						orderedPassed &= wait([&]() { return shown(expected); });
					}
					orderedPassed &= wait([&]() {
						if(!renderer.Capture) { return false; }
						bool winning = false, lower = false;
						for(const auto& row : renderer.Capture->Rows) {
							int offset = x - row.X;
							if(row.Scanline != y || offset < 0 || offset >= 8 || !(row.OutputColorMask & (1 << offset))) { continue; }
							winning |= row.Sprite == 0 && row.OutputColors[offset] == 31;
							lower |= row.Sprite == 1 && row.OutputColors[offset] == 0x7C00;
						} return winning && lower;
					});
					bg.MemoryConditions[0].Comparison = 0; bg.HideOriginalLayer = false; bg.Artwork = {0xFF00FFFF};
					for(int position : {4,5,6,1}) {
						bg.CompositionPosition = position; group.Artwork.assign(group.Artwork.size(),0xFFFF00FF);
						mark(); emu->GetVideoDecoder()->SetMetaReplacements({group},{bg});
						orderedPassed &= wait([&]() { return shown(position >= 6 || position == 1 ? 0xFF00FFFFu : 0xFF00FF00u); });
						group.Artwork.assign(group.Artwork.size(),0); mark(); emu->GetVideoDecoder()->SetMetaReplacements({group},{bg});
						orderedPassed &= wait([&]() { return shown(position == 1 ? 0xFF00FFFFu : 0xFF0000FFu); });
					}
					bg.CompositionPosition = 6; group.Artwork.assign(group.Artwork.size(),0x80FF00FF);
					mark(); emu->GetVideoDecoder()->SetMetaReplacements({group},{bg}); orderedPassed &= wait([&]() { return shown(0xFF0080FF); });
					if(!orderedPassed) { flags |= 2048; } else { std::cout << "PASS: live CGB ordered OBJ alpha and all background insertion bands with opaque/transparent/partial replacements.\n"; }
					emu->GetVideoDecoder()->SetMetaReplacements({}, {});
				}
				if(dynamic_cast<PceConsole*>(emu->GetConsole().get()) || dynamic_cast<SmsConsole*>(emu->GetConsole().get())) {
					const char* system = dynamic_cast<PceConsole*>(emu->GetConsole().get()) ? "PCE" : "SMS";
					int priorityPixel = -1; uint32_t priorityColor = 0;
					bool smsMode4 = false;
					if(auto sms = dynamic_cast<SmsConsole*>(emu->GetConsole().get())) {
						{ auto lock = emu->AcquireLock(); smsMode4 = sms->GetVdp()->GetState().UseMode4; }
						std::lock_guard<std::mutex> lock(renderer.Mutex);
						if(renderer.Capture && renderer.Capture->Width == width && renderer.Capture->Height == height) {
							for(size_t index = 0; index < renderer.Capture->Pixels.size(); index++) { const auto& pixel = renderer.Capture->Pixels[index];
								if(pixel.NativeBackdropCaptured && pixel.ExpandedSpriteOccluder >= 0 && pixel.BackgroundColorIndex && (pixel.BackgroundOverlayOccludesClasses & 1) && !(pixel.ExpandedSpriteClasses & 1)) { priorityPixel = (int)index; priorityColor = renderer.Pixels[index]; break; }
							}
						}
					}
					bool pceBandsPassed = !smsMode4 || priorityPixel >= 0;
					if(!pceBandsPassed) { std::cerr << "SMS Mode 4 fixture did not capture its priority-hidden OBJ." << std::endl; }
					bg.MemoryConditions[0].Comparison = 0; bg.Artwork = {0xFF00FFFF}; group.Artwork.assign(group.Artwork.size(),0xFFFF00FF);
					for(int replacement = 0; replacement < 2; replacement++) { for(int position : {4,5,6,1}) {
						bg.CompositionPosition = position; mark(); emu->GetVideoDecoder()->SetMetaReplacements(replacement ? vector<MetaSpriteReplacement>{group} : vector<MetaSpriteReplacement>{}, {bg});
						bool passed = wait([&]() { return shown(position == 1 ? 0xFF00FFFFu : replacement ? 0xFFFF00FFu : nativeColor)
							&& (priorityPixel < 0 || renderer.Pixels[priorityPixel] == (position == 6 || position == 1 ? 0xFF00FFFFu : priorityColor)); });
						if(!passed) { std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr << system << " band mismatch: replacement=" << replacement << " position=" << position << " actual=" << std::hex << renderer.Pixels[(size_t)y*width+x] << " native=" << nativeColor << std::dec << std::endl; }
						pceBandsPassed &= passed;
					} }
					bg.CompositionPosition = 6; group.Artwork.assign(group.Artwork.size(),0); mark(); emu->GetVideoDecoder()->SetMetaReplacements({group},{bg}); bool holesPassed = wait([&]() { return shown(0xFF00FFFF); }); pceBandsPassed &= holesPassed;
					group.Artwork.assign(group.Artwork.size(),0x80FF00FF); mark(); emu->GetVideoDecoder()->SetMetaReplacements({group},{bg}); bool partialPassed = wait([&]() { return shown(0xFF807FFF); }); pceBandsPassed &= partialPassed;
					if(!holesPassed || !partialPassed) { std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr << system << " band alpha mismatch: holes=" << holesPassed << " partial=" << partialPassed << " actual=" << std::hex << renderer.Pixels[(size_t)y*width+x] << std::dec << std::endl; }
					if(!pceBandsPassed) { flags |= 8192; } else { std::cout << "PASS: live " << system << " insertion bands with native/replacement foreground OBJ and transparent/partial PNG alpha.\n"; }
					emu->GetVideoDecoder()->SetMetaReplacements({},{});
				}
				if(auto ws = dynamic_cast<WsConsole*>(emu->GetConsole().get())) {
					bool wsBandsPassed = true; bg.MemoryConditions[0].Comparison = 0; bg.Artwork = {0xFF00FFFF};
					for(bool foreground : {false,true}) {
						{ auto lock = emu->AcquireLock(); auto mm = ws->GetMemoryManager();
							for(int tile = 0; tile < 1024; tile++) { mm->DebugWrite(0x0800 + tile*2, 0); mm->DebugWrite(0x0801 + tile*2, 2); }
							mm->DebugWrite(0xFE22, 0xF0); mm->DebugWrite(0xFE23, 0);
							mm->WritePort<uint8_t>(7, 0x10); mm->WritePort<uint8_t>(0, foreground ? 7 : 5);
						}
						for(bool high : {false,true}) {
							{ auto lock = emu->AcquireLock(); ws->GetMemoryManager()->DebugWrite(0x1001, high ? 0x20 : 0); }
							group.Artwork.assign(group.Artwork.size(),0xFFFF00FF);
							for(int replacement = 0; replacement < 2; replacement++) { for(int position : {4,5,6,1}) {
								bg.CompositionPosition = position; mark(); emu->GetVideoDecoder()->SetMetaReplacements(replacement ? vector<MetaSpriteReplacement>{group} : vector<MetaSpriteReplacement>{}, {bg});
								uint32_t expected = position == 1 || (!high && position >= 5 && (!foreground || position >= 6)) ? 0xFF00FFFF : !high && foreground ? 0xFF00FF00 : replacement ? 0xFFFF00FF : nativeColor;
								bool passed = wait([&]() { return shown(expected); });
								if(!passed) { std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr << "WS band mismatch: fg=" << foreground << " high=" << high << " replacement=" << replacement << " position=" << position << " actual=" << std::hex << renderer.Pixels[(size_t)y*width+x] << " expected=" << expected << std::dec << std::endl; }
								wsBandsPassed &= passed;
							} }
							bg.CompositionPosition = 6; group.Artwork.assign(group.Artwork.size(),0x80FF00FF); mark(); emu->GetVideoDecoder()->SetMetaReplacements({group},{bg});
							wsBandsPassed &= wait([&]() { return shown(high ? 0xFF807FFFu : 0xFF00FFFFu); });
						}
						if(!wsBandsPassed) { break; }
					}
					if(wsBandsPassed) {
						{ auto lock = emu->AcquireLock(); auto mm = ws->GetMemoryManager();
							mm->DebugWrite(0x1001, 0); mm->DebugWrite(0x1004, 0); mm->DebugWrite(0x1005, 0x22); mm->DebugWrite(0x1006, 8); mm->DebugWrite(0x1007, 8);
							mm->DebugWrite(0xFF22, 15); mm->DebugWrite(0xFF23, 0); mm->WritePort<uint8_t>(6, 2);
						}
						for(uint32_t alpha : {255u,0u,128u}) {
							group.Artwork.assign(group.Artwork.size(),(alpha << 24) | 0xFF00FF); mark(); emu->GetVideoDecoder()->SetMetaReplacements({group},{});
							wsBandsPassed &= wait([&]() { return shown(alpha == 255 ? 0xFF00FF00u : alpha == 0 ? 0xFF0000FFu : 0xFF00807Fu); });
						}
					}
					if(!wsBandsPassed) { flags |= 16384; } else { std::cout << "PASS: live WonderSwan BG0/BG1 insertion bands, low/high OBJ, ordered lower native reveal without BG packs and PNG alpha.\n"; }
					emu->GetVideoDecoder()->SetMetaReplacements({},{});
				}
				if(auto gba = dynamic_cast<GbaConsole*>(emu->GetConsole().get())) {
					// Keep the fixture's native BG0 at priority 3, move OBJ0 to 2.
					{ auto lock = emu->AcquireLock(); gba->GetMemoryManager()->DebugWrite(0x07000005, 8); }
					bg.CompositionPosition = 2; bg.Artwork = {0xFF0000FF}; bg.MemoryConditions[0].Comparison = 0;
					bool priorityPassed = true;
					for(int replacement = 0; replacement < 2; replacement++) {
						for(int priority = 0; priority < 4; priority++) {
							bg.GbaPriority = priority; mark(); emu->GetVideoDecoder()->SetMetaReplacements(replacement ? vector<MetaSpriteReplacement>{group} : vector<MetaSpriteReplacement>{}, {bg});
							priorityPassed &= wait([&]() { return shown(priority < 2 ? 0xFF0000FFu : replacement ? 0xFFFF00FFu : nativeColor); });
						}
					}
					bg.GbaPriority = 0; bg.Artwork = {0x800000FF}; mark(); emu->GetVideoDecoder()->SetMetaReplacements({group}, {bg});
					priorityPassed &= wait([&]() { return shown(0xFF7F00FF); });
					emu->GetVideoDecoder()->SetMetaReplacements({}, {});
					if(!priorityPassed) { flags |= 256; } else { std::cout << "PASS: live GBA independent priorities 0..3 above/below native and replacement OBJ2, priority ties and PNG alpha.\n"; }
				}
				if(auto snes = dynamic_cast<SnesConsole*>(emu->GetConsole().get())) {
					{ auto lock = emu->AcquireLock(); auto mm = snes->GetMemoryManager(); mm->Write(0x00212C, 0x11, MemoryOperationType::Write); mm->Write(0x00212D, 0, MemoryOperationType::Write); mm->Write(0x002131, 0, MemoryOperationType::Write); }
					bg.CompositionPosition = 3; bg.Artwork = {0xFF0000FF}; bg.MemoryConditions[0].Comparison = 0; bg.HideOriginalLayer = true;
					bool priorityPassed = true;
					for(int replacement = 0; replacement < 2; replacement++) {
						for(int priority = 1; priority <= 15; priority++) {
							bg.SnesPriority = priority; mark(); emu->GetVideoDecoder()->SetMetaReplacements(replacement ? vector<MetaSpriteReplacement>{group} : vector<MetaSpriteReplacement>{}, {bg});
							priorityPassed &= wait([&]() { return shown(priority > 2 ? 0xFF0000FFu : replacement ? 0xFFFF00FFu : nativeColor); });
						}
					}
					bg.SnesPriority = 15; bg.Artwork = {0x800000FF}; mark(); emu->GetVideoDecoder()->SetMetaReplacements({group}, {bg});
					priorityPassed &= wait([&]() { return shown(0xFF7F00FF); });
					emu->GetVideoDecoder()->SetMetaReplacements({}, {});
					if(!priorityPassed) { flags |= 512; } else { std::cout << "PASS: live SNES independent priorities 1..15 above/below native and replacement OBJ2, priority ties and PNG alpha.\n"; }
					{ auto lock = emu->AcquireLock(); auto mm = snes->GetMemoryManager(); auto write = [&](uint32_t address, uint8_t value) { mm->Write(address,value,MemoryOperationType::Write); };
						write(0x2100,0x80); write(0x2102,2); write(0x2103,0);
						for(uint8_t value : {uint8_t(8),uint8_t(8),uint8_t(0),uint8_t(2)}) { write(0x2104,value); }
						write(0x2121,145); write(0x2122,0xFF); write(0x2122,3);
						write(0x212C,0x10); write(0x212D,0); write(0x2130,0); write(0x2131,0); write(0x2100,15);
					}
					bool revealPassed = true;
					for(uint32_t alpha : {255u,0u,128u}) {
						group.Artwork.assign(group.Artwork.size(),(alpha << 24) | 0x00FF00); mark(); emu->GetVideoDecoder()->SetMetaReplacements({group},{});
						revealPassed &= wait([&]() { return shown(alpha == 255 ? 0xFF00FF00u : alpha == 0 ? 0xFFFFFF00u : 0xFF7FFF00u); });
					}
					{ auto lock = emu->AcquireLock(); auto mm = snes->GetMemoryManager(); mm->Write(0x2125,2,MemoryOperationType::Write); mm->Write(0x2126,0,MemoryOperationType::Write); mm->Write(0x2127,31,MemoryOperationType::Write); mm->Write(0x212E,0x10,MemoryOperationType::Write); }
					mark(); revealPassed &= wait([&]() { return shown(0xFF0000FF); });
					{ auto lock = emu->AcquireLock(); auto mm = snes->GetMemoryManager(); auto write = [&](uint32_t address, uint8_t value) { mm->Write(address,value,MemoryOperationType::Write); };
						write(0x2100,0x80); write(0x212E,0); write(0x2102,3); write(0x2103,0); write(0x2104,0); write(0x2104,10);
						write(0x2121,209); write(0x2122,0xFF); write(0x2122,3); write(0x2132,0x9F); write(0x2131,0x10); write(0x2100,15);
					}
					for(uint32_t alpha : {0u,128u}) {
						group.Artwork.assign(group.Artwork.size(),(alpha << 24) | 0x00FF00); mark(); emu->GetVideoDecoder()->SetMetaReplacements({group},{});
						revealPassed &= wait([&]() { return shown(alpha == 0 ? 0xFFFFFFFFu : 0xFF7FFF7Fu); });
					}
					if(!revealPassed) { flags |= 32768; } else { std::cout << "PASS: live SNES lower native OBJ reveal through PNG holes/partial alpha, OBJ window clipping and palette-dependent fixed-color math.\n"; }
					emu->GetVideoDecoder()->SetMetaReplacements({},{});
					{ auto lock = emu->AcquireLock(); auto mm = snes->GetMemoryManager(); auto write = [&](uint32_t address, uint8_t value) { mm->Write(address,value,MemoryOperationType::Write); };
						write(0x2100,0x80); write(0x2102,1); write(0x2103,0); write(0x2104,0); write(0x2104,0x30);
						write(0x2102,2); for(uint8_t value : {uint8_t(16),uint8_t(8),uint8_t(0),uint8_t(2)}) { write(0x2104,value); }
						write(0x2121,5); write(0x2122,31); write(0x2122,0x7C); write(0x212C,0x11); write(0x2131,0); write(0x2100,15);
					}
					MetaSpriteReplacement mixed; mixed.Id = 2; mixed.ArtworkWidth = 32; mixed.ArtworkHeight = 8; mixed.ArtworkOriginX = 2; mixed.Artwork.assign(256,0xFF00FF00);
					mark(); bool mixedPassed = wait([&]() {
						if(renderer.FrameNumber <= afterFrame || !renderer.Capture) { return false; }
						auto candidates = MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture); mixed.Members.clear();
						for(int id : {0,1}) { auto found = std::find_if(candidates.begin(),candidates.end(),[&](const auto& member) { return member.HardwareSprite == id; }); if(found != candidates.end()) { mixed.Members.push_back(*found); } }
						if(mixed.Members.size() != 2 || mixed.Members[0].SnesPriorityCode != 3 || mixed.Members[1].SnesPriorityCode != 0 || mixed.Members[1].SnesPalette != 1 || mixed.Members[1].X - mixed.Members[0].X != 8) { return false; }
						int left = mixed.Members[0].X, top = mixed.Members[0].Y;
						for(auto& member : mixed.Members) { member.X -= left; member.Y -= top; member.HardwareSprite = -1; } return true;
					});
					auto checkMixed = [&](const char* name, uint32_t first, uint32_t right, uint32_t margin, uint32_t left, uint32_t scale = 1) {
						bool passed = wait([&]() { if(renderer.FrameNumber <= afterFrame || renderer.Width != width*scale || renderer.Height != height*scale) { return false; }
							auto sample = [&](int px) { return renderer.Pixels[(size_t)(y*scale)*renderer.Width+px*scale]; };
							return sample(x) == first && sample(x+8) == right && sample(x+24) == margin && sample(x-6) == left;
						});
						if(!passed) { std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr << "SNES mixed member mismatch: " << name << " dimensions=" << renderer.Width << "x" << renderer.Height << std::endl; } return passed;
					};
					if(mixedPassed) {
						mark(); emu->GetVideoDecoder()->SetMetaReplacements({mixed},{}); mixedPassed &= checkMixed("priority/origin",0xFF00FF00,0xFFFF00FF,0xFFFF00FF,0xFF00FF00);
						mixed.Artwork.assign(256,0x8000FF00); mark(); emu->GetVideoDecoder()->SetMetaReplacements({mixed},{}); mixedPassed &= checkMixed("partial",0xFF7F807F,0xFFFF00FF,0xFFFF00FF,0xFF7F807F);
						mixed.ArtworkScale = 2; mixed.ArtworkWidth = 64; mixed.ArtworkHeight = 16; mixed.ArtworkOriginX = 4; mixed.Artwork.assign(1024,0x8000FF00);
						mark(); emu->GetVideoDecoder()->SetMetaReplacements({mixed},{}); mixedPassed &= checkMixed("2x",0xFF7F807F,0xFFFF00FF,0xFFFF00FF,0xFF7F807F,2);
						mixed.ArtworkScale = 1; mixed.ArtworkWidth = 32; mixed.ArtworkHeight = 8; mixed.ArtworkOriginX = 2; mixed.Artwork.assign(256,0xFF00FF00);
						{ auto lock = emu->AcquireLock(); auto mm = snes->GetMemoryManager(); mm->Write(0x2100,0x80,MemoryOperationType::Write); mm->Write(0x2102,3,MemoryOperationType::Write); mm->Write(0x2103,0,MemoryOperationType::Write); mm->Write(0x2104,0,MemoryOperationType::Write); mm->Write(0x2104,0x3A,MemoryOperationType::Write); mm->Write(0x2131,0x10,MemoryOperationType::Write); mm->Write(0x2100,15,MemoryOperationType::Write); }
						mark(); emu->GetVideoDecoder()->SetMetaReplacements({mixed},{}); mixedPassed &= checkMixed("palette math",0xFF00FF00,0xFF00FFFF,0xFF00FFFF,0xFF00FF00);
						mixed.Members[1].X = 4;
						for(int firstObject : {0,1}) {
							{ auto lock = emu->AcquireLock(); auto mm = snes->GetMemoryManager(); auto write = [&](uint32_t address, uint8_t value) { mm->Write(address,value,MemoryOperationType::Write); };
								write(0x2100,0x80); write(0x2102,2); write(0x2103,0); for(uint8_t value : {uint8_t(12),uint8_t(8),uint8_t(0),uint8_t(2)}) { write(0x2104,value); }
								write(0x2131,0); write(0x2102,firstObject ? 2 : 0); write(0x2103,firstObject ? 0x80 : 0); write(0x2100,15);
							}
							mark(); emu->GetVideoDecoder()->SetMetaReplacements({mixed},{}); mixedPassed &= checkMixed(firstObject ? "rotated overlap" : "overlap",firstObject ? 0xFFFF00FFu : 0xFF00FF00u,0xFFFF00FF,0xFFFF00FF,0xFF00FF00);
						}
					}
					if(!mixedPassed) { flags |= 65536; } else { std::cout << "PASS: live SNES mixed-member priorities, native BG insertion, shifted origin, 2x partial PNG, palette math and rotated overlap.\n"; }
					emu->GetVideoDecoder()->SetMetaReplacements({},{});
				}
			}
			if(!flags) { std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr << "Foreground capture missing: " << renderer.Width << "x" << renderer.Height << " frame=" << renderer.FrameNumber << " capture=" << (bool)renderer.Capture; if(renderer.Capture) { std::cerr << " rows=" << renderer.Capture->DecodedRows.size() << " dimensions=" << renderer.Capture->Width << "x" << renderer.Capture->Height; if(renderer.Capture->Width > 12 && renderer.Capture->Height > 12) { const auto& p = renderer.Capture->Pixels[12*renderer.Capture->Width+12]; std::cerr << " bg=" << p.BackgroundSample.Valid << " gba=" << p.GbaBackgroundSamples[0].Valid; } } std::cerr << std::endl; }
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunForegroundOverlayGameplayTest(const char* filename) { return RunForegroundOverlayGameplayTestImpl(filename,false); }
	DllExport uint32_t __stdcall RunAnimationGameplayTest(const char* filename) { return RunForegroundOverlayGameplayTestImpl(filename,true); }
	DllExport uint32_t __stdcall RunSmsLegacyTextBackgroundTest(const char* filename)
	{
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->GetSmsConfig().RamPowerOnState = RamState::AllZeros;
		MetaSpriteTestRenderer renderer; uint32_t flags = 0;
		auto wait = [&](std::function<bool()> check) { for(int i = 0; i < 500; i++) { { std::lock_guard<std::mutex> lock(renderer.Mutex); if(check()) { return true; } } std::this_thread::sleep_for(std::chrono::milliseconds(10)); } return false; };
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			auto sms = dynamic_cast<SmsConsole*>(emu->GetConsole().get()); sms->GetVdp()->SetMetaSpriteCapture(true);
			if(wait([&]() {
				if(!renderer.Capture || renderer.Width != 256 || renderer.Height != 240 || !renderer.Capture->DecodedRows.empty()) { return false; }
				for(int y = 24; y < 216; y++) { for(int x = 0; x < 256; x++) {
					auto& sample = renderer.Capture->Pixels[y*256+x].BackgroundSample;
					bool interior = x >= 8 && x < 248;
					if(sample.Valid != interior) { return false; }
					if(interior && (sample.X != x-8 || sample.Y != y-24 || sample.Layer != 0 || renderer.Pixels[y*256+x] != 0xFFFF0000)) { return false; }
				} } return true;
			})) {
				flags |= 1; MetaBackgroundReplacement bg; bg.Id = 1; bg.Layer = 0; bg.Width = bg.Height = 1; bg.WrapX = bg.WrapY = true; bg.Artwork = {0xFFFFFFFF}; bg.MemoryConditions = {{0xC000,0,0xFF}};
				emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
				if(wait([&]() {
					if(renderer.Width != 256 || !renderer.Capture || renderer.Capture->MemorySnapshot.size() != 1 || renderer.Capture->MemorySnapshot.count(0xC000) != 1) { return false; }
					for(int y = 24; y < 216; y++) { for(int x = 0; x < 256; x++) { if(renderer.Pixels[y*256+x] != (x >= 8 && x < 248 ? 0xFFFFFFFFu : 0xFF0000FFu)) { return false; } } } return true;
				})) { flags |= 2; }
				bg.HideOriginalLayer = true; bg.Artwork = {0}; emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
				if(wait([&]() { return renderer.Pixels[32*256+8] == 0xFF0000FF; })) { flags |= 4; }
				bg.MemoryConditions[0].Value = 1; bg.Artwork = {0xFFFFFFFF}; emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
				if(wait([&]() { return renderer.Pixels[32*256+8] == 0xFFFF0000; })) { flags |= 8; }
				emu->GetVideoDecoder()->SetMetaBackgroundLayers({});
				if(wait([&]() { return renderer.Pixels[32*256+8] == 0xFFFF0000 && renderer.Capture && renderer.Capture->MemorySnapshot.empty(); })) { flags |= 16; }
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunWsMusicReplacementChecks(const uint8_t* packet, uint32_t length, const char* filename)
	{
		unique_ptr<Emulator> emu(new Emulator()); uint32_t stage = 1;
		try {
			emu->Initialize(false); emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
			bool color = string(filename).find(".wsc") != string::npos;
			emu->GetSettings()->GetWsConfig().Model = color ? WsModel::Color : WsModel::Monochrome;
			emu->GetSettings()->GetWsConfig().UseBootRom = false;
			if(!emu->LoadRom((VirtualFile)filename, VirtualFile(), false)) { emu->Release(); return stage; }
			{
				auto lock = emu->AcquireLock(); auto ws = dynamic_cast<WsConsole*>(emu->GetConsole().get());
				if(!ws) { throw std::runtime_error("Expected WS"); }
				auto check = [](bool v) { if(!v) { throw std::runtime_error("WS audio assertion"); } }; check(ws->IsColorModel() == color);
				auto mm = ws->GetMemoryManager(); auto music = std::make_shared<MusicReplacement>(packet, length, 0x10FFFF, 0xFFFFF);
				emu->GetSoundMixer()->SetReplacementMusic(music);
				stage = 2; mm->DebugWrite(0x760, 6); mm->DebugWrite(0x653, 0x55); mm->DebugWrite(0x652, 8); check(!music->ActiveTrack());
				mm->Write<uint8_t>(0, 0x652, 8, MemoryOperationType::DmaWrite); check(!music->ActiveTrack());
				mm->Write<uint8_t>(0x60, 0x52, 8); check(music->ActiveTrack() == 1); // Segment alias resolves to linear 00652.
				stage = 3; music->ResetPlayback(); mm->DebugWrite(0x653, 0); mm->Write<uint16_t>(0xFFFF, 0x662, 0x5508); check(music->ActiveTrack() == 1); // 20-bit wrap; complete aligned word context.
				stage = 4; music->ResetPlayback(); mm->Write<uint16_t>(0, 0x651, 0x08AB, MemoryOperationType::DmaWrite); check(!music->ActiveTrack()); // Split writes must retain operation type.
				mm->Write<uint16_t>(0, 0x651, 0x08AB); check(music->ActiveTrack() == 1);
				stage = 5; mm->Write<uint8_t>(0, 0x760, 4); check(music->ActiveTrack() == 1); mm->Write<uint8_t>(0x1000, 0x652, 8); check(music->ActiveTrack() == 2); // Full 20-bit trigger identity.
				stage = 6; music->ResetPlayback(); mm->Write<uint8_t>(0, 0x80, 0x90); check(!music->ActiveTrack()); mm->WritePort<uint8_t>(0x280, 0x90); check(!music->ActiveTrack());
				mm->WritePort<uint8_t>(0x80, 0x90); check(music->ActiveTrack() == 2);
				stage = 7; music->ResetPlayback(); mm->WritePort<uint16_t>(0x80, 0x3412); check(!music->ActiveTrack()); vector<int16_t> samples(16, 0); music->Mix(samples.data(), 8, 8000); check(samples[0] == 4000); // High byte of word port write.
				music->ResetPlayback(); music->Mix(samples.data(), 8, 8000); check(samples[0] == 0);
			}
			emu->Stop(false); emu->Release(); return 0;
		} catch(...) { emu->Stop(false); emu->Release(); return stage; }
	}
	DllExport uint32_t __stdcall RunSnesMusicReplacementChecks(const uint8_t* packet, uint32_t length, const char* filename)
	{
		unique_ptr<Emulator> emu(new Emulator()); uint32_t stage = 1;
		try {
			emu->Initialize(false); emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
			if(!emu->LoadRom((VirtualFile)filename, VirtualFile(), false)) { emu->Release(); return stage; }
			{
				auto lock = emu->AcquireLock(); auto snes = dynamic_cast<SnesConsole*>(emu->GetConsole().get());
				if(!snes) { throw std::runtime_error("Expected SNES"); }
				auto check = [](bool v) { if(!v) { throw std::runtime_error("SNES audio assertion"); } };
				auto mm = snes->GetMemoryManager(); auto spc = snes->GetSpc(); auto music = std::make_shared<MusicReplacement>(packet, length, 0x100FFFF);
				emu->GetSoundMixer()->SetReplacementMusic(music);
				stage = 2; mm->GetMemoryMappings()->DebugWrite(0x7E0760, 6); mm->GetMemoryMappings()->DebugWrite(0x7E0652, 8); check(!music->ActiveTrack());
				mm->WriteDma(0x7E0652, 8, true); check(!music->ActiveTrack());
				mm->Write(0x7E0652, 8, MemoryOperationType::Write); check(music->ActiveTrack() == 1);
				stage = 3; mm->Write(0x000760, 4, MemoryOperationType::Write); check(music->ActiveTrack() == 1); // WRAM mirror.
				music->ResetPlayback(); mm->Write(0x002140, 8, MemoryOperationType::Write); check(music->ActiveTrack() == 1);
				stage = 4; music->ResetPlayback(); spc->DebugWrite(0x652, 8); spc->DspWriteRam(0x652, 8); check(!music->ActiveTrack());
				// Execute original SPC700 code through the real instruction interpreter.
				spc->Reset(); spc->GetState().PC = 0x1000;
				vector<uint8_t> program {0xE8,6,0xC5,0x60,7,0xE8,8,0xC5,0x52,6,0xE8,1,0xC5,0x53,6,0x2F,0xFE};
				for(size_t i = 0; i < program.size(); i++) { spc->DebugWrite((uint16_t)(0x1000 + i), program[i]); }
				for(int i = 0; i < 1000; i++) { mm->IncrementMasterClockValue(10); } spc->Run(); check(music->ActiveTrack() == 2);
				stage = 5; music->Command(2); vector<int16_t> samples(16, 0); music->Mix(samples.data(), 8, 8000); check(samples[0] == 4000); // SPC trigger, main CPU RAM context, independent effect.
				music->Command(5); music->Command(3); music->Mix(samples.data(), 8, 8000); check(samples[0] == -4000);
				stage = 6; uint8_t value; check(!mm->ReadMusicMemory(0x002140, value)); check(mm->ReadMusicMemory(0x1000760, value) && value == 6);
				music->ResetPlayback(); music->Mix(samples.data(), 8, 8000); check(samples[0] == 0);
			}
			emu->Stop(false); emu->Release(); return 0;
		} catch(...) { emu->Stop(false); emu->Release(); return stage; }
	}
	DllExport uint32_t __stdcall RunSmsMusicReplacementChecks(const uint8_t* packet, uint32_t length, const char* filename)
	{
		unique_ptr<Emulator> emu(new Emulator()); uint32_t stage = 1;
		try {
			emu->Initialize(false); emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
			if(!emu->LoadRom((VirtualFile)filename, VirtualFile(), false)) { emu->Release(); return stage; }
			{
				auto lock = emu->AcquireLock(); auto sms = dynamic_cast<SmsConsole*>(emu->GetConsole().get());
				if(!sms) { throw std::runtime_error("Expected SMS"); }
				auto check = [](bool v) { if(!v) { throw std::runtime_error("SMS audio assertion"); } };
				auto mm = sms->GetMemoryManager(); auto music = std::make_shared<MusicReplacement>(packet, length, 0x100FF, 0xFFFF);
				emu->GetSoundMixer()->SetReplacementMusic(music);
				stage = 2; mm->DebugWrite(0xC760, 6); mm->DebugWrite(0xC652, 8); check(!music->ActiveTrack());
				mm->Write(0xC652, 8); check(music->ActiveTrack() == 1);
				stage = 3; mm->Write(0xC760, 4); check(music->ActiveTrack() == 1); mm->Write(0xC652, 8); check(music->ActiveTrack() == 2);
				stage = 4; music->ResetPlayback(); mm->Write(0x007F, 0x90); check(!music->ActiveTrack()); // Memory and ports cannot collide.
				mm->WritePort(0x7E, 0x90); check(!music->ActiveTrack()); // Aliased hardware port is a distinct rule address.
				mm->WritePort(0x7F, 0x90); check(music->ActiveTrack() == 1);
				stage = 5; music->ResetPlayback(); mm->WritePort(0x7F, 0x91); check(!music->ActiveTrack());
				vector<int16_t> samples(16, 0); music->Mix(samples.data(), 8, 8000); check(samples[0] == 4000); // Effect triggered by port write.
				stage = 6; mm->Write(0xE760, 6); mm->Write(0xC652, 8); check(music->ActiveTrack() == 1); // RAM mirror visible in context.
				music->ResetPlayback(); music->Mix(samples.data(), 8, 8000); check(samples[0] == 0);
				stage = 7; bool rejected = false; try { MusicReplacement narrow(packet, length); } catch(...) { rejected = true; } check(rejected);
			}
			emu->Stop(false); emu->Release(); return 0;
		} catch(...) { emu->Stop(false); emu->Release(); return stage; }
	}
	DllExport uint32_t __stdcall RunPceMusicReplacementChecks(const uint8_t* packet, uint32_t length, const char* filename)
	{
		unique_ptr<Emulator> emu(new Emulator()); uint32_t stage = 1;
		try {
			emu->Initialize(false); emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
			if(!emu->LoadRom((VirtualFile)filename, VirtualFile(), false)) { emu->Release(); return stage; }
			{
				auto lock = emu->AcquireLock(); auto pce = dynamic_cast<PceConsole*>(emu->GetConsole().get());
				if(!pce) { throw std::runtime_error("Expected PCE"); }
				auto check = [](bool v) { if(!v) { throw std::runtime_error("PCE audio assertion"); } };
				auto mm = pce->GetMemoryManager(); auto music = std::make_shared<MusicReplacement>(packet, length);
				emu->GetSoundMixer()->SetReplacementMusic(music);
				mm->SetMprValue(1, 0xFF); mm->SetMprValue(2, 0xF8); mm->SetMprValue(4, 0xF8);
				stage = 2; mm->DebugWrite(0x2760, 6); mm->DebugWrite(0x2652, 8); check(!music->ActiveTrack());
				mm->Write(0x2652, 8, MemoryOperationType::DmaWrite); check(!music->ActiveTrack());
				mm->Write(0x2652, 8, MemoryOperationType::Write); check(music->ActiveTrack() == 1);
				stage = 3; music->ResetPlayback(); mm->SetMprValue(2, 0); // Current bank now ROM, whose byte does not match post-write RAM context.
				mm->Write(0x2652, 8, MemoryOperationType::Write); check(!music->ActiveTrack());
				mm->SetMprValue(2, 0xF8); mm->Write(0x2652, 8, MemoryOperationType::Write); check(music->ActiveTrack() == 1);
				stage = 4; mm->Write(0x4760, 4, MemoryOperationType::Write); check(music->ActiveTrack() == 1);
				mm->Write(0x4652, 8, MemoryOperationType::Write); check(music->ActiveTrack() == 2); // Separate logical trigger, same mapped RAM bank.
				stage = 5; music->ResetPlayback(); mm->Write(0x2653, 1, MemoryOperationType::Write);
				vector<int16_t> samples(16, 0); music->Mix(samples.data(), 8, 8000); check(samples[0] == 4000 && !music->ActiveTrack()); // Missing I/O context must fail even !=.
				music->ResetPlayback(); music->Mix(samples.data(), 8, 8000); check(samples[0] == 0);
			}
			emu->Stop(false); emu->Release(); return 0;
		} catch(...) { emu->Stop(false); emu->Release(); return stage; }
	}
	DllExport uint32_t __stdcall RunGbaMusicReplacementChecks(const uint8_t* packet, uint32_t length, const char* filename)
	{
		unique_ptr<Emulator> emu(new Emulator()); uint32_t stage = 1;
		try {
			emu->Initialize(false); emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
			if(!emu->LoadRom((VirtualFile)filename, VirtualFile(), false)) { emu->Release(); return stage; }
			{
				auto lock = emu->AcquireLock(); auto gba = dynamic_cast<GbaConsole*>(emu->GetConsole().get());
				if(!gba) { throw std::runtime_error("Expected GBA"); }
				auto check = [](bool v) { if(!v) { throw std::runtime_error("GBA audio assertion"); } };
				auto mm = gba->GetMemoryManager(); auto music = std::make_shared<MusicReplacement>(packet, length, 0x0FFFFFFF);
				emu->GetSoundMixer()->SetReplacementMusic(music);
				stage = 2; mm->DebugWrite(0x02000760, 6); mm->DebugWrite(0x02000653, 0x55); mm->DebugWrite(0x02000652, 8); check(!music->ActiveTrack());
				mm->Write(GbaAccessMode::Byte | GbaAccessMode::Dma, 0x02000652, 8); check(!music->ActiveTrack());
				mm->Write(GbaAccessMode::Byte, 0x02000652, 8); check(music->ActiveTrack() == 1);
				stage = 3; music->ResetPlayback(); mm->DebugWrite(0x02000653, 0);
				mm->Write(GbaAccessMode::HalfWord, 0x02000653, 0x5508); check(music->ActiveTrack() == 1); // Aligned write; both bytes committed before conditions.
				stage = 4; music->ResetPlayback(); mm->Write(GbaAccessMode::Byte, 0x03000760, 4); check(!music->ActiveTrack());
				mm->Write(GbaAccessMode::Byte, 0x03000652, 8); check(music->ActiveTrack() == 2); // Full address identity despite prefilter collision.
				stage = 5; music->ResetPlayback(); mm->Write(GbaAccessMode::Word, 0x02000657, 0x1234ABCD); check(!music->ActiveTrack());
				vector<int16_t> samples(16, 0); music->Mix(samples.data(), 8, 8000); check(samples[0] == 4000); // High-byte effect trigger plus sibling context.
				stage = 6; bool rejected = false; try { MusicReplacement narrow(packet, length); } catch(...) { rejected = true; } check(rejected);
				for(uint32_t cut = 0; cut < length; cut++) { rejected = false; try { MusicReplacement invalid(packet, cut, 0x0FFFFFFF); } catch(...) { rejected = true; } check(rejected); }
			}
			emu->Stop(false); emu->Release(); return 0;
		} catch(...) { emu->Stop(false); emu->Release(); return stage; }
	}
	DllExport uint32_t __stdcall RunGbMusicReplacementChecks(const uint8_t* packet, uint32_t length, const char* filename)
	{
		unique_ptr<Emulator> emu(new Emulator());
		try {
			emu->Initialize(false); emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
			emu->GetSettings()->GetGameboyConfig().Model = string(filename).find(".gbc") != string::npos ? GameboyModel::GameboyColor : GameboyModel::Gameboy;
			if(!emu->LoadRom((VirtualFile)filename, VirtualFile(), false)) { emu->Release(); return 1; }
			uint32_t result = 0;
			{
				auto lock = emu->AcquireLock(); auto gb = dynamic_cast<Gameboy*>(emu->GetConsole().get());
				if(!gb) { throw std::runtime_error("Expected standalone Game Boy"); }
				auto mm = gb->GetMemoryManager(); auto music = std::make_shared<MusicReplacement>(packet, length);
				emu->GetSoundMixer()->SetReplacementMusic(music);
				mm->DebugWrite(0xC760, 6); mm->DebugWrite(0xC652, 8);
				if(music->ActiveTrack()) { result = 2; }
				mm->Write<MemoryOperationType::DmaWrite>(0xC652, 8);
				if(music->ActiveTrack()) { result = 3; }
				mm->ProcessCpuWrite(0xC652, 8);
				if(music->ActiveTrack() != 1) { result = 4; }
				vector<int16_t> samples(16, 0); music->Mix(samples.data(), 8, 8000);
				if(samples[0] != 4000) { result = 5; }
				mm->ProcessCpuWrite(0xC760, 4);
				if(music->ActiveTrack() != 1) { result = 6; }
				mm->ProcessCpuWrite(0xC652, 8);
				if(music->ActiveTrack() != 2) { result = 7; }
				music->ResetPlayback(); mm->ProcessCpuWrite(0xC653, 1);
				if(music->ActiveTrack()) { result = 8; }
				samples.assign(16, 0); music->Mix(samples.data(), 8, 8000);
				if(samples[0] != 4000) { result = 9; } // Effect-only event.
			}
			emu->Stop(false); emu->Release(); return result;
		} catch(...) { emu->Stop(false); emu->Release(); return 10; }
	}
	DllExport uint32_t __stdcall RunMusicReplacementChecks(const uint8_t* packet, uint32_t length, const char* filename)
	{
		uint32_t stage = 1;
		try {
			auto check = [](bool value) { if(!value) { throw std::runtime_error("Music assertion"); } };
			MusicReplacement music(packet, length); check(music.HasTriggers());
			vector<int16_t> samples(16, 3000); music.Mix(samples.data(), 8, 8000); check(std::all_of(samples.begin(), samples.end(), [](int16_t v) { return v == 0; }));
			stage = 2; MetaMemorySnapshot memory {{0x652, 8}, {0x760, 6}}; std::set<uint32_t> reads;
			auto read = [&](uint32_t address, uint8_t& value) { reads.insert(address); auto found = memory.find(address); if(found == memory.end()) { return false; } value = found->second; return true; };
			music.ProcessWrite(0x760, 6, read); check(music.ActiveTrack() == 0);
			music.ProcessWrite(0x652, 8, read); check(music.ActiveTrack() == 1 && reads.size() == 2);
			stage = 3; std::fill(samples.begin(), samples.end(), 0); music.Mix(samples.data(), 8, 8000);
			for(int i = 0; i < 8; i++) { check(samples[i * 2] == 4000 + i * 10 && samples[i * 2 + 1] == samples[i * 2]); }
			music.ProcessWrite(0x652, 8, read); music.Mix(samples.data(), 8, 8000); check(samples[0] == 4080); // Continue, no restart.
			music.Command(0, 1, true); music.Mix(samples.data(), 8, 16000); check(samples[0] == 4000 && samples[2] == 4005 && samples[4] == 4010);
			stage = 4; memory[0x760] = 4; check(music.ActiveTrack() == 1); music.ProcessWrite(0x652, 8, read); check(music.ActiveTrack() == 2);
			samples.resize(80); music.Mix(samples.data(), 40, 8000); check(samples[0] == -4000 && samples[64] == -4160 && samples[78] == -4230);
			stage = 5; music.ProcessWrite(0x653, 1, read); samples.assign(16, 1234); music.Mix(samples.data(), 8, 8000); check(samples[0] == 0 && music.ActiveTrack() == 2);
			music.ProcessWrite(0x654, 1, read); music.Mix(samples.data(), 8, 8000); check(samples[0] == -4240);
			music.ProcessWrite(0x655, 1, read); check(music.ActiveTrack() == 0);
			memory.erase(0x760); music.ProcessWrite(0x652, 8, read); check(music.ActiveTrack() == 0);
			vector<uint8_t> volumes(packet, packet + length); volumes[8] = 0; volumes[12] = volumes[16] = 50;
			MusicReplacement mixed(volumes.data(), volumes.size()); samples.assign(16, 3000); mixed.Mix(samples.data(), 8, 8000); check(samples[0] == 1500);
			mixed.Command(0, 1); samples.assign(16, 3000); mixed.Mix(samples.data(), 8, 8000); check(samples[0] == 3500);
			volumes[4] = 0; MusicReplacement disabled(volumes.data(), volumes.size()); samples.assign(16, 3000); disabled.Mix(samples.data(), 8, 8000); check(samples[0] == 3000 && !disabled.HasTriggers());
			stage = 6;
			for(uint32_t cut = 0; cut < length; cut++) { bool rejected = false; try { MusicReplacement invalid(packet, cut); } catch(...) { rejected = true; } check(rejected); }
			vector<uint8_t> bad(packet, packet + length); bad.push_back(0); bool rejected = false; try { MusicReplacement invalid(bad.data(), bad.size()); } catch(...) { rejected = true; } check(rejected);
			stage = 7;
			vector<uint8_t> fade(packet, packet + length); fade[20] = 1;
			uint32_t pathLength = (uint32_t)fade[32] | ((uint32_t)fade[33] << 8) | ((uint32_t)fade[34] << 16) | ((uint32_t)fade[35] << 24);
			fade[36 + pathLength + 16] = 1; MusicReplacement fading(fade.data(), fade.size()); fading.Command(0, 1);
			samples.assign(32, 0); fading.Mix(samples.data(), 16, 8000); check(samples[0] > 0 && samples[0] < 4000 && samples[30] == 4150);
			fading.Command(1); fading.Mix(samples.data(), 16, 8000); check(fading.ActiveTrack() == 0 && samples[30] == 0);
			stage = 9;
			vector<uint8_t> effects(packet, packet + length);
			auto word = [&](size_t at) { uint32_t v = 0; for(int j = 0; j < 4; j++) { v |= (uint32_t)effects.at(at + j) << (8 * j); } return v; };
			auto put = [&](size_t at, uint32_t v) { for(int j = 0; j < 4; j++) { effects.at(at + j) = (uint8_t)(v >> (8 * j)); } };
			size_t offset = 28;
			for(uint32_t i = 0; i < word(24); i++) { offset += 28 + word(offset + 4); }
			size_t ruleCountOffset = offset; uint32_t originalRules = word(offset); offset += 4;
			vector<vector<uint8_t>> effectRules;
			for(uint32_t i = 0; i < originalRules; i++) {
				size_t bytes = 36 + word(offset + 32) * 16;
				if(i < 2) { effectRules.emplace_back(effects.begin() + offset, effects.begin() + offset + bytes); }
				offset += bytes;
			}
			put(ruleCountOffset, originalRules + 2);
			for(uint32_t i = 0; i < 2; i++) {
				size_t start = effects.size(); effects.insert(effects.end(), effectRules[i].begin(), effectRules[i].end());
				put(start, 1000 + i); put(start + 4, 4); put(start + 12, 1);
			}
			MusicReplacement fx(effects.data(), effects.size()); memory[0x760] = 6;
			fx.ProcessWrite(0x652, 8, read); samples.assign(16, 0); fx.Mix(samples.data(), 8, 8000);
			check(fx.ActiveTrack() == 1 && samples[0] == 8000); // Same event starts both independently.
			fx.Command(2); fx.Mix(samples.data(), 8, 8000); check(samples[0] == 4080); // Music pause leaves effects running.
			check(fx.Command(4, 1, false)); fx.Mix(samples.data(), 8, 8000); check(samples[0] == 4160); // Ignore repeated effect.
			check(fx.Command(4, 1, true)); fx.Mix(samples.data(), 8, 8000); check(samples[0] == 4000);
			check(fx.Command(4, 2, true)); fx.Mix(samples.data(), 8, 8000); check(samples[0] == 80); // Two independent effects overlap.
			fx.Command(5); fx.Mix(samples.data(), 8, 8000); check(samples[0] == 0 && fx.ActiveTrack() == 1);
			fx.Command(4, 1); samples.assign(160, 0); fx.Mix(samples.data(), 80, 8000); check(samples[0] == 4000 && samples[126] == 4630 && samples[128] == 0); // Effect never loops.
			fx.Command(4, 1); fx.ResetPlayback(); fx.Mix(samples.data(), 80, 8000); check(samples[0] == 0 && fx.ActiveTrack() == 0);
			std::cout << "PASS: independent music/effect rules, overlapping effects, per-effect retrigger, one-shot completion, stop and reset." << std::endl;
			stage = 8;
			std::cout << "MUSIC: pure playback/packet checks passed; loading live NES fixture." << std::endl;
			FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
			unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false); emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
			// Only exercise committed memory writes here; no running CPU thread is
			// needed. This avoids racing immediate Pause/Lock against thread startup.
			if(!emu->LoadRom((VirtualFile)filename, VirtualFile(), false)) { emu->Release(); return stage; }
			std::cout << "MUSIC: live fixture loaded; pausing for post-write checks." << std::endl;
			emu->Pause();
			{
				auto emuLock = emu->AcquireLock(); auto nes = dynamic_cast<NesConsole*>(emu->GetConsole().get()); check(nes != nullptr);
				std::cout << "MUSIC: live fixture locked." << std::endl;
				auto live = std::make_shared<MusicReplacement>(packet, length); emu->GetSoundMixer()->SetReplacementMusic(live);
				nes->GetMemoryManager()->Write(0x760, 6, MemoryOperationType::Write); nes->GetMemoryManager()->Write(0x652, 8, MemoryOperationType::Write);
				check(live->ActiveTrack() == 1); // Includes trigger-address context, proving post-commit capture.
				live->ResetPlayback(); nes->GetMemoryManager()->Write(0x760, 4, MemoryOperationType::Write); check(live->ActiveTrack() == 0);
				nes->GetMemoryManager()->Write(0x652, 8, MemoryOperationType::Write); check(live->ActiveTrack() == 2);
				emu->GetSoundMixer()->ResetReplacementMusic(); check(live->ActiveTrack() == 0);
				emu->GetSoundMixer()->ClearReplacementMusic(); check(!emu->GetSoundMixer()->HasMusicTriggers());
			}
			emu->Stop(false); emu->Release();
			std::cout << "MUSIC: live fixture shutdown completed." << std::endl;
			std::cout << "PASS: music native mute, write-event/context precedence, post-commit NES writes, continue/restart, resampling, loops, pause/resume/stop, fades, missing bytes and packet rejection." << std::endl;
			return 0;
		} catch(...) { return stage; }
	}
	DllExport void __stdcall ConfigureMetaSoftwareRendererTest()
	{
		// Use the regular software renderer without opening an OS window or audio
		// device. The managed reproduction can consume its normal frame callback.
		_emu->SetAudioVideoInitCallback(nullptr, []() -> IRenderingDevice* { return new SoftwareRenderer(_emu.get()); });
	}

	DllExport bool __stdcall RunMetaViewerRefreshTest(const char* filename)
	{
		MetaSpriteTestMessages messages; MessageManager::RegisterMessageManager(&messages);
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->GetGameboyConfig().Model = string(filename).find(".gbc") != string::npos ? GameboyModel::GameboyColor : GameboyModel::Gameboy;
		emu->GetSettings()->GetGbaConfig().SkipBootScreen = true;
		emu->GetSettings()->GetGbaConfig().DisableFrameSkipping = true;
		emu->GetSettings()->GetPcEngineConfig().DisableFrameSkipping = true;
		emu->GetSettings()->GetSnesConfig().DisableFrameSkipping = true;
		emu->GetSettings()->GetWsConfig().UseBootRom = false;
		MetaSpriteTestRenderer renderer;
		bool passed = false;
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			auto waitFrames = [&](uint32_t count) {
				uint32_t start;
				{ std::lock_guard<std::mutex> lock(renderer.Mutex); start = renderer.FrameNumber; }
				for(int attempt = 0; attempt < 3000; attempt++) {
					{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.FrameNumber >= start + count) { return true; } }
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				return false;
			};
			if(waitFrames(8) && MeasureLiveFrameTiming(emu.get(), renderer, "native without debugger, representative viewer ROM")) {
				emu->InitDebugger();
				bool debuggerOnlyPassed = waitFrames(8) && MeasureLiveFrameTiming(emu.get(), renderer, "native with debugger, no viewer schedules, representative ROM");
				auto* tools = emu->InternalGetDebugger()->GetPpuTools(emu->InternalGetDebugger()->GetMainCpuType());
				auto listener = std::make_shared<MetaSpriteViewerRefreshListener>();
				emu->GetNotificationManager()->RegisterNotificationListener(listener);
				tools->SetViewerUpdateTiming(123, 8, 12); tools->SetViewerUpdateTiming(456, 8, 16);
				uint32_t first = listener->First.load(), second = listener->Second.load();
				passed = MeasureLiveFrameTiming(emu.get(), renderer, "native with debugger and two viewer schedules, representative ROM") && debuggerOnlyPassed;
				uint32_t deliveredFirst = listener->First.load() - first, deliveredSecond = listener->Second.load() - second;
				passed &= deliveredFirst >= 118 && deliveredFirst <= 125 && deliveredSecond >= 118 && deliveredSecond <= 125 && listener->Unexpected.load() == 0;
				tools->RemoveViewer(123); tools->RemoveViewer(456);
				first = listener->First.load(); second = listener->Second.load();
				passed &= waitFrames(8) && first == listener->First.load() && second == listener->Second.load();
				std::cout << "VIEWER ROM: " << filename << "; notifications=" << deliveredFirst << "/" << deliveredSecond << "; delivery/removal=" << passed << std::endl;
				emu->StopDebugger();
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return passed;
	}
	bool __stdcall SetMetaReplacements(const char*, const uint8_t*, uint32_t, const uint8_t*, uint32_t);
	bool __stdcall SetMetaBackgroundReplacements(const char*, const uint8_t*, uint32_t);
	DllExport uint32_t __stdcall GetMetaReplacementDefinitionFlagsTest()
	{
		return (_emu->GetVideoDecoder()->HasMetaReplacements() ? 1u : 0u) | (_emu->GetVideoDecoder()->HasMetaBackgroundLayers() ? 2u : 0u);
	}
	DllExport uint32_t __stdcall RunMetaPackActivationTest(const char* filename, const uint8_t* sprites, uint32_t spriteLength, const uint8_t* backgrounds, uint32_t backgroundLength)
	{
		auto previous = std::move(_emu);
		_emu.reset(new Emulator()); _emu->Initialize(false);
		_emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		uint32_t flags = 0;
		if(_emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			string hash = _emu->GetHash(HashType::Sha1);
			const uint8_t emptySprites[] = {'M', 'S', 'P', '1', 0, 0, 0, 0};
			const uint8_t emptyBackgrounds[] = {'M', 'B', 'G', '1', 0, 0, 0, 0};
			if(SetMetaReplacements(hash.c_str(), sprites, spriteLength, emptyBackgrounds, sizeof(emptyBackgrounds)) && _emu->GetVideoDecoder()->HasMetaReplacements()) {
				flags |= 1;
				bool retained = true;
				for(uint32_t length = 0; length < backgroundLength; length++) {
					retained &= !SetMetaReplacements(hash.c_str(), emptySprites, sizeof(emptySprites), backgrounds, length) && _emu->GetVideoDecoder()->HasMetaReplacements();
				}
				vector<uint8_t> invalidBackground(backgrounds, backgrounds + backgroundLength);
				invalidBackground.push_back(0);
				retained &= !SetMetaReplacements(hash.c_str(), emptySprites, sizeof(emptySprites), invalidBackground.data(), (uint32_t)invalidBackground.size()) && _emu->GetVideoDecoder()->HasMetaReplacements();
				invalidBackground.pop_back();
				if(invalidBackground.size() >= 16) {
					invalidBackground[12] = dynamic_cast<SnesConsole*>(_emu->GetConsole().get()) ? 4 : 3;
					retained &= !SetMetaReplacements(hash.c_str(), emptySprites, sizeof(emptySprites), invalidBackground.data(), (uint32_t)invalidBackground.size()) && _emu->GetVideoDecoder()->HasMetaReplacements();
				} else { retained = false; }
				if(retained) { flags |= 2; }
				if(!SetMetaReplacements("wrong-rom", emptySprites, sizeof(emptySprites), emptyBackgrounds, sizeof(emptyBackgrounds)) && _emu->GetVideoDecoder()->HasMetaReplacements()) { flags |= 4; }
				if(SetMetaReplacements(hash.c_str(), sprites, spriteLength, backgrounds, backgroundLength)) { flags |= 8; }
				if(SetMetaReplacements(hash.c_str(), emptySprites, sizeof(emptySprites), emptyBackgrounds, sizeof(emptyBackgrounds)) && !_emu->GetVideoDecoder()->HasMetaReplacements()) { flags |= 16; }
				if(SetMetaBackgroundReplacements(hash.c_str(), backgrounds, backgroundLength) && _emu->GetVideoDecoder()->HasMetaReplacements()
					&& SetMetaBackgroundReplacements(hash.c_str(), emptyBackgrounds, sizeof(emptyBackgrounds)) && !_emu->GetVideoDecoder()->HasMetaReplacements()) { flags |= 32; }
			}
			_emu->Stop(false);
		}
		_emu->Release(); _emu = std::move(previous);
		return flags;
	}
    static uint32_t RunSnesHiresObjectMathTest(const char* filename)
    {
        FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename)); unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
        emu->GetSettings()->SetFlag(EmulationFlags::TestMode); emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
        auto& cfg=emu->GetSettings()->GetSnesConfig(); cfg.DisableFrameSkipping=true; cfg.RamPowerOnState=RamState::AllZeros; cfg.ColorCorrection=SnesColorCorrectionMode::None;
        string name(filename); bool both=name.find("both-screens")!=string::npos,window=name.find("window")!=string::npos,operand=name.find("operand")!=string::npos;
        bool current=name.find("current-field")!=string::npos,blend=name.find("bob-blend-field")!=string::npos,bob=blend || name.find("bob-field")!=string::npos;
        if(current) { cfg.DeinterlaceMode=SnesDeinterlaceMode::CurrentField; } else if(bob) { cfg.DeinterlaceMode=SnesDeinterlaceMode::Bob; }
        MetaSpriteTestRenderer renderer; uint32_t flags=0;
        if(emu->LoadRom((VirtualFile)filename,VirtualFile())) {
            emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer); auto snes=dynamic_cast<SnesConsole*>(emu->GetConsole().get()); snes->GetPpu()->SetMetaSpriteCapture(true);
            auto wait=[&](const std::function<bool()>& check,int attempts=600) { for(int n=0;n<attempts;n++) { { std::lock_guard<std::mutex> lock(renderer.Mutex); if(check()) { return true; } } std::this_thread::sleep_for(std::chrono::milliseconds(10)); } return false; };
            auto framePhase=[&]() { return current ? (renderer.Capture->Pixels[14*512].SnesFallbackCaptured ? 0 : 1) : bob ? int(renderer.Capture->SnesOutputOffsetY) : 0; };
            vector<uint32_t> native[2],sourcePixels(64); bool rowsReady[2]={}; MetaSpriteGraphic source;
            if(wait([&]() {
                if(renderer.FrameNumber<10 || renderer.Width!=512 || renderer.Height!=478 || !renderer.Capture || !renderer.Capture->SnesHiresSource) { return false; }
                for(const auto& candidate:MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
                    if(candidate.HardwareSprite<0 || candidate.HardwareSprite%128 || candidate.X!=8 || candidate.Y!=30 || candidate.Width!=8 || candidate.Height!=8 || candidate.SnesPalette!=4 || candidate.SnesRowStep!=2) { continue; }
                    source=candidate; int phase=candidate.SnesRowPhase; rowsReady[phase]=true;
                    for(int y=phase;y<8;y+=2) { for(int x=0;x<8;x++) { sourcePixels[y*8+x]=candidate.Pixels[y*8+x]; } }
                }
                int phase=framePhase(); native[phase]=renderer.Pixels; if(!current && !bob) { native[1]=native[0]; }
                return rowsReady[0] && rowsReady[1] && !native[0].empty() && !native[1].empty();
            })) {
                flags=1; source.X=source.Y=0; source.HardwareSprite=-1; source.SnesField=0; source.Pixels=sourcePixels;
                MetaSpriteReplacement group; group.Id=1; group.Members={source}; group.ArtworkScale=2; group.ArtworkWidth=group.ArtworkHeight=21; group.ArtworkOriginX=group.ArtworkOriginY=1; group.MemoryConditions={{0,0,0}};
                for(int y=0;y<21;y++) { for(int x=0;x<21;x++) { uint32_t rgb=y%3==0 ? 0xFF0000u : y%3==1 ? 0x00FF00u : 0xFFFFFFu,alpha=x%7==0 ? 0 : x%7==1 ? 128 : 255; group.Artwork.push_back(rgb|(alpha<<24)); } }
                struct Outcome { uint32_t Color; bool Math,Artwork; int Priority; double Weight; };
                auto samples=[&](int px,int sourceY,int sy,bool main) {
                    vector<Outcome> result; int ax=px-14,ay=(sourceY-29)*2+sy; bool enabled=main ? !(window && px/2>=12 && px/2<=15) : both;
                    uint32_t color=enabled && ax>=0 && ax<21 && ay>=0 && ay<21 ? group.Artwork[ay*21+ax] : 0,alpha=color>>24;
                    if(alpha) { result.push_back({color|0xFF000000,true,true,1,alpha/255.0}); }
                    if(alpha<255) { result.push_back({0xFF0000FF,false,false,0,(255-alpha)/255.0}); } return result;
                };
                auto math=[](uint32_t primary,uint32_t other,bool enabled,bool half,bool artwork) {
                    uint32_t result=0xFF000000; for(int shift:{0,8,16}) { int a=(primary>>shift)&255,b=(other>>shift)&255;
                        if(!artwork) { a>>=3; b>>=3; } if(enabled) { a+=b; if(half) { a/=2; } a=std::min(artwork ? 255 : 31,a); }
                        if(!artwork) { a=(a<<3)|(a>>2); } result|=uint32_t(a)<<shift;
                    } return result;
                };
                vector<uint32_t> expected[2];
                for(int phase=0;phase<2;phase++) {
                    expected[phase].resize(1024*956);
                    for(int y=0;y<956;y++) { for(int x=0;x<1024;x++) {
                        int px=x/2,py=y/2; uint32_t color=native[phase][py*512+px];
                        bool active=py>=14 && py<462 && (!current || (py&1)==phase);
                        int sourceY=!current && !bob ? py : current ? py : phase ? (py==14 ? 15 : ((py-1)/2)*2+1) : (py/2)*2;
                        if(active && px>=12 && px<=38 && sourceY>=27 && sourceY<=41) {
                            int cell=px/2,control=cell ? cell-1 : 0; auto a=samples(px,sourceY,y&1,(px&1)!=0);
                            auto b=samples(px&1 ? cell*2 : control*2+1,sourceY,y&1,(px&1)==0);
                            auto priority=px&1 ? vector<Outcome>{{0,false,false,0,1}} : samples(control*2,sourceY,y&1,false);
                            double channels[3]={}; for(const auto& first:a) { for(const auto& second:b) { for(const auto& p:priority) {
                                bool enabled=px&1 ? first.Math : second.Math,useSub=operand && ((px&1) ? second.Priority : p.Priority)>0,half=!operand || useSub;
                                uint32_t other=useSub ? second.Color : 0xFF0000FFu;
                                uint32_t resolved=math(first.Color,other,enabled,half,first.Artwork || (useSub && second.Artwork)); double weight=first.Weight*second.Weight*p.Weight;
                                for(int ch=0;ch<3;ch++) { channels[ch]+=((resolved>>(ch*8))&255)*weight; }
                            } } }
                            color=0xFF000000; for(int ch=0;ch<3;ch++) { color|=uint32_t(channels[ch]+0.5)<<(ch*8); }
                        }
                        expected[phase][y*1024+x]=color;
                    } }
                }
                if(blend) { auto lock=emu->AcquireLock(); cfg.DeinterlaceMode=SnesDeinterlaceMode::BobBlend; }
                vector<uint32_t> blended; if(blend) { blended.resize(expected[0].size()); for(size_t i=0;i<blended.size();i++) { auto a=expected[0][i],b=expected[1][i]; blended[i]=(((a^b)&0xFFFEFEFEu)>>1)+(a&b); } }
                emu->GetVideoDecoder()->SetMetaSpriteGroups({group}); uint8_t seen=0;
                if(wait([&]() {
                    if(renderer.Width!=1024 || renderer.Height!=956 || !renderer.Capture || !renderer.Capture->SnesHiresSource) { return false; }
                    int phase=framePhase(); if(renderer.Pixels!=(blend ? blended : expected[phase])) { return false; } seen|=uint8_t(1<<phase); return !(current || bob) || seen==3;
                })) { flags|=2; }
                else { std::lock_guard<std::mutex> lock(renderer.Mutex); int phase=renderer.Capture ? framePhase() : 0; const auto& reference=blend ? blended : expected[phase];
                    std::cerr<<"OBJ hires math dimensions "<<renderer.Width<<"x"<<renderer.Height<<std::endl;
                    for(size_t i=0;i<reference.size() && i<renderer.Pixels.size();i++) { if(reference[i]!=renderer.Pixels[i]) { std::cerr<<"OBJ hires math mismatch "<<name<<" x="<<i%1024<<" y="<<i/1024<<" got="<<std::hex<<renderer.Pixels[i]<<" expected="<<reference[i]<<std::dec<<std::endl; break; } }
                }
                bool woven=!current && !bob;
                bool checkpointOnly=name.find("checkpoint-only")!=string::npos;
                bool animatedRewind=blend || woven;
                if(flags==3) {
                    if(animatedRewind) {
                        MetaPngPixelPool pool;
                        auto sequence=[&](int w,int h,int origin,uint64_t identity,const vector<uint32_t>& colors) {
                            MetaPngArtworkAnimation animation; animation.RestartOnActivation=true;
                            animation.SourceIdentity={0x424F42424C454E44ULL,identity};
                            animation.Timeline=MetaPngTimeline(MetaPngPlayback::Loop,{3,5,4});
                            for(uint32_t color:colors) {
                                MetaPngArtworkFrame image; image.Width=w; image.Height=h; image.OriginX=image.OriginY=origin;
                                vector<uint32_t> pixels((size_t)w*h,color);
                                for(int y=0;y<h;y++) { pixels[y*w]=0; }
                                image.Pixels=pool.Intern(std::move(pixels)); animation.Frames.push_back(std::move(image));
                            }
                            return animation;
                        };
                        auto animated=group; animated.MemoryConditions={{0x7E1FF0,0,0xFF}};
                        MetaAlternateArtwork alternate; alternate.ArtworkScale=2; alternate.ArtworkWidth=alternate.ArtworkHeight=21;
                        alternate.ArtworkOriginX=alternate.ArtworkOriginY=1; alternate.Artwork=group.Artwork;
                        alternate.MemoryConditions={{0x7E1FF1,0,0xFF}};
                        alternate.Animation=sequence(21,21,1,1,{0x80FF0000,0x8000FF00,0x80FFFFFF}); animated.AlternateArtwork={alternate};
                        MetaBackgroundReplacement background; background.Id=1; background.Layer=0; background.X=40; background.Y=20;
                        background.Width=16; background.Height=12; background.FollowScroll=false;
                        background.Artwork.assign(16*12,0x8000FF00); background.MemoryConditions={{0x7E1FF2,0,0xFF}};
                        background.Animation=sequence(16,12,0,2,{0x8000FF00,0x80FFFF00,0x8000FFFF});
                        { auto lock=emu->AcquireLock(); snes->GetMemoryManager()->Write(0x212C,0x11,MemoryOperationType::Write);
                            emu->GetVideoDecoder()->SetMetaReplacements({animated},{background}); }
                        int probeX=(name.find("mode5")!=string::npos || name.find("mode6")!=string::npos) ? 82 : 166;
                        if(!wait([&]() {
                            if(renderer.Width!=1024 || renderer.Height!=956 || !renderer.Capture || renderer.Capture->MemorySnapshot.size()!=3) { return false; }
                            auto timing=woven ? renderer.Capture->SnesFieldAnimationTiming[0] : renderer.Capture->AnimationTiming;
                            auto other=renderer.Capture->SnesFieldAnimationTiming[1];
                            return timing && timing->Elapsed.size()==2 && (!woven || (other && other->Elapsed.size()==2))
                                && ((renderer.Pixels[80*1024+probeX]>>8)&255)>0;
                        },18000)) {
                            std::lock_guard<std::mutex> renderLock(renderer.Mutex);
                            std::cerr<<"Interlaced animated setup: "<<renderer.Width<<"x"<<renderer.Height<<" memory="<<(renderer.Capture?renderer.Capture->MemorySnapshot.size():0)
                                <<" clocks="<<(renderer.Capture && renderer.Capture->AnimationTiming?renderer.Capture->AnimationTiming->Elapsed.size():0)
                                <<" background="<<std::hex<<(renderer.Pixels.size()>80*1024+probeX?renderer.Pixels[80*1024+probeX]:0)<<std::dec<<std::endl;
                            flags&=~2u;
                        }
                        {
                            auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
                            auto snapshot=emu->GetVideoDecoder()->CaptureRewindBlendHistory();
                            std::deque<RewindData> history; RewindData checkpoint; checkpoint.SaveState(emu.get(),history);
                            std::stringstream raw; checkpoint.GetStateData(raw,history,-1);
                            vector<uint8_t> stateBytes; CompressionHelper::Compress(raw.str(),1,stateBytes);
                            size_t expectedSize=stateBytes.size(),chunks=0;
                            for(size_t offset=0;offset<snapshot.size();offset+=RewindData::FrameBlendChunkPixels) {
                                size_t count=std::min(RewindData::FrameBlendChunkPixels,snapshot.size()-offset);
                                vector<uint8_t> chunk; CompressionHelper::Compress(string((const char*)(snapshot.data()+offset),count*sizeof(uint32_t)),1,chunk);
                                expectedSize+=chunk.size(); chunks++;
                            }
                            auto capture=snes->GetPpu()->CaptureReplacementRewindState();
                            constexpr size_t ChunkBytes=RewindData::FrameBlendChunkPixels*sizeof(uint32_t);
                            for(size_t offset=0;offset<capture.size();offset+=ChunkBytes) {
                                size_t count=std::min(ChunkBytes,capture.size()-offset); vector<uint8_t> chunk;
                                CompressionHelper::Compress(string((const char*)(capture.data()+offset),count),1,chunk);
                                expectedSize+=chunk.size(); chunks++;
                            }
                            if(chunks<2 || checkpoint.GetStateSize()!=expectedSize) { flags&=~2u; }
                            else { std::cout<<"PASS: interlaced checkpoint memory budget includes "<<chunks<<" compressed capture/display chunks ("<<expectedSize-stateBytes.size()<<" bytes)."<<std::endl; }
                        }
                    }
                    if(woven) {
                        auto lock=emu->AcquireLock(); auto* ppu=snes->GetPpu();
                        // Stop inside a visible field, including partially fetched rows.
                        int busReads=0;
                        while((ppu->GetScanline()!=80 || ppu->GetCycle()<64) && busReads++<500000) { snes->GetMemoryManager()->Read(0x7E1FF0,MemoryOperationType::Read); }
                        emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
                        std::deque<RewindData> history; RewindData checkpoint; checkpoint.SaveState(emu.get(),history);
                        auto original=ppu->CaptureReplacementRewindState();
                        bool valid=busReads<500000 && !original.empty();
                        auto malformed=original; if(!malformed.empty()) { malformed.pop_back(); }
                        valid=valid && !ppu->RestoreReplacementRewindState(malformed);
                        malformed=original; malformed.push_back(0);
                        valid=valid && !ppu->RestoreReplacementRewindState(malformed) && ppu->CaptureReplacementRewindState()==original;
                        snes->RunFrame(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
                        vector<uint32_t> expectedPartial; uint32_t expectedFrame=0;
                        { std::lock_guard<std::mutex> renderLock(renderer.Mutex); expectedPartial=renderer.Pixels; expectedFrame=renderer.FrameNumber; }
                        // Leave interlace before loading: its latched flags are not in
                        // the ordinary serializer, so the capture sidecar must restore them.
                        snes->GetMemoryManager()->Write(0x2133,0,MemoryOperationType::Write); snes->RunFrame();
                        valid=valid && ppu->CaptureReplacementRewindState().empty();
                        checkpoint.LoadState(emu.get(),history);
                        valid=valid && ppu->CaptureReplacementRewindState()==original;
                        snes->RunFrame(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
                        { std::lock_guard<std::mutex> renderLock(renderer.Mutex); valid=valid && renderer.FrameNumber==expectedFrame && renderer.Pixels==expectedPartial; }
                        checkpoint.LoadState(emu.get(),history);
                        if(valid) { std::cout<<"PASS: partial visible-field checkpoint restores identical capture bytes and whole-frame output across an interlace mode change; malformed snapshots are transactional."<<std::endl; }
                        else { std::cerr<<"FAIL: partial visible-field checkpoint or interlace transition restoration."<<std::endl; flags&=~2u; }
                    }
                    if(!checkpointOnly) {
                    {
                        auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
                        emu->GetSettings()->GetPreferences().RewindBufferSize=64; emu->GetRewindManager()->InitHistory();
                        std::lock_guard<std::mutex> renderLock(renderer.Mutex); renderer.ReplayFrames.clear(); renderer.CompressReplayFrames=true; renderer.RecordReplayFrames=true;
                    }
                    bool recorded=wait([&]() { return renderer.ReplayFrames.size()>=100; },60000);
                    if(animatedRewind && recorded) {
                        std::lock_guard<std::mutex> renderLock(renderer.Mutex);
                        const auto& first=renderer.ReplayFrames.begin()->second;
                        recorded=std::any_of(renderer.ReplayFrames.begin(),renderer.ReplayFrames.end(),[&](const auto& frame) { return frame.second!=first; });
                    }
                    { std::lock_guard<std::mutex> renderLock(renderer.Mutex); std::cout<<"OBJ rewind recorded="<<renderer.ReplayFrames.size()<<std::endl; }
                    if(recorded) {
                        auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
                        { std::lock_guard<std::mutex> renderLock(renderer.Mutex); renderer.RecordReplayFrames=false; renderer.CheckReplayFrames=true; }
                        emu->GetRewindManager()->StartRewinding();
                    }
                    bool replayed=recorded && wait([&]() { return renderer.ReplayErrors || renderer.ReplayMatches>=40; },60000);
                    {
                        auto lock=emu->AcquireLock();
                        { std::lock_guard<std::mutex> renderLock(renderer.Mutex);
                            std::cout<<"OBJ rewind matches="<<renderer.ReplayMatches<<" errors="<<renderer.ReplayErrors<<std::endl;
                            replayed=replayed && !renderer.ReplayErrors && renderer.ReplayMatches>=40;
                            renderer.RecordReplayFrames=false; renderer.CheckReplayFrames=false; renderer.ReplayFrames.clear();
                        }
                        if(recorded) { emu->GetRewindManager()->StopRewinding(true,false); }
                    }
                    if(!replayed) { std::cerr<<"OBJ hires single-field rewind failed: "<<name<<std::endl; flags&=~2u; }
                    else { std::cout<<"PASS: hires OBJ-interlace PNG playback, 40 exact reverse whole-frame matches across rewind checkpoints."<<std::endl; }
                    }
                    if(woven) {
                        auto lock=emu->AcquireLock(); auto* ppu=snes->GetPpu(); auto saved=ppu->CaptureReplacementRewindState();
                        emu->GetVideoDecoder()->SetMetaReplacements({group},{});
                        if(!saved.empty() && !ppu->RestoreReplacementRewindState(saved)) { std::cout<<"PASS: live PPU rewind rejects a checkpoint from an earlier pack revision."<<std::endl; }
                        else { std::cerr<<"FAIL: live PPU accepted stale replacement definitions."<<std::endl; flags&=~2u; }
                    }
                    if(animatedRewind) { auto lock=emu->AcquireLock(); snes->GetMemoryManager()->Write(0x212C,0x10,MemoryOperationType::Write); }
                }
                vector<uint32_t> nativeBlend; if(blend) { nativeBlend.resize(native[0].size()); for(size_t i=0;i<nativeBlend.size();i++) { auto a=native[0][i],b=native[1][i]; nativeBlend[i]=(((a^b)&0xFFFEFEFEu)>>1)+(a&b); } }
                emu->GetVideoDecoder()->SetMetaReplacements({},{}); seen=0;
                if(wait([&]() { if(renderer.Width!=512 || !renderer.Capture) { return false; } int phase=framePhase(); if(renderer.Pixels!=(blend ? nativeBlend : native[phase])) { return false; } seen|=uint8_t(1<<phase); return !(current || bob) || seen==3; })) { flags|=4; }
                if(flags==7) { std::cout<<"PASS: actual hires OBJ-interlace whole-frame 2x PNG rows, alpha, origins, priority/windows, color-math operands and field/native restoration."<<std::endl; }
            }
            emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
        }
        emu->Release(); return flags;
    }
	DllExport uint32_t __stdcall RunMetaSpriteSnesHiresMathTest(const char* filename)
	{
		if(string(filename).find("obj-interlace")!=string::npos) { return RunSnesHiresObjectMathTest(filename); }
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false); emu->GetSettings()->SetFlag(EmulationFlags::TestMode); emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg=emu->GetSettings()->GetSnesConfig(); cfg.DisableFrameSkipping=true; cfg.RamPowerOnState=RamState::AllZeros; cfg.ColorCorrection=SnesColorCorrectionMode::None;
		MetaSpriteTestRenderer renderer; uint32_t flags=0;
		bool both=string(filename).find("both-screens")!=string::npos,window=string(filename).find("window")!=string::npos,operand=string(filename).find("operand")!=string::npos;
		bool currentField=string(filename).find("current-field")!=string::npos,bobBlend=string(filename).find("bob-blend-field")!=string::npos;
		bool bobField=bobBlend || string(filename).find("bob-field")!=string::npos;
		if(currentField) { cfg.DeinterlaceMode=SnesDeinterlaceMode::CurrentField; }
		if(bobField) { cfg.DeinterlaceMode=SnesDeinterlaceMode::Bob; } // Capture the unblended even native reference first.
		if(emu->LoadRom((VirtualFile)filename,VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer); auto snes=dynamic_cast<SnesConsole*>(emu->GetConsole().get()); snes->GetPpu()->SetMetaSpriteCapture(true);
			auto wait=[&](const std::function<bool()>& check) { for(int attempt=0;attempt<500;attempt++) { { std::lock_guard<std::mutex> lock(renderer.Mutex); if(check()) { return true; } } std::this_thread::sleep_for(std::chrono::milliseconds(10)); } return false; };
			MetaSpriteGraphic source; vector<uint32_t> native; int nativeField=0;
			if(wait([&]() {
				if(renderer.Width!=512 || renderer.Height!=478 || !renderer.Capture || !renderer.Capture->SnesHiresSource) { return false; }
				if((currentField || bobField) && renderer.FrameNumber<10) { return false; }
				if(bobField && renderer.Capture->SnesOutputOffsetY!=0) { return false; }
				for(const auto& graphic : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
					if(graphic.HardwareSprite==0 && graphic.X==8 && graphic.Y==15 && graphic.Width==8 && graphic.Height==8 && graphic.SnesPalette==4) { source=graphic; native=renderer.Pixels; nativeField=currentField && !renderer.Capture->Pixels[30*512].SnesFallbackCaptured ? 1 : 0; return true; }
				} return false;
			})) {
				flags=1; MetaSpriteReplacement group; group.Id=1; source.X=source.Y=0; source.HardwareSprite=-1; source.SnesField=0; group.Members={source}; group.ArtworkScale=2;
				if(currentField) {
					for(int y=0;y<478;y+=2) { for(int x=0;x<512;x++) { uint32_t color=native[(y+nativeField)*512+x]; native[y*512+x]=native[(y+1)*512+x]=color; } }
				}
				auto phase=[&]() { return currentField ? (renderer.Capture->Pixels[30*512].SnesFallbackCaptured ? 0 : 1) : bobField ? renderer.Capture->SnesOutputOffsetY : 0; };
				auto present=[&](const vector<uint32_t>& full,int field) {
					if(!currentField && !bobField) { return full; } vector<uint32_t> result(full.size(),0xFF000000);
					for(int y=14;y<462;y++) { for(int x=0;x<512;x++) {
						if(currentField) { if((y&1)==field) { result[y*512+x]=full[y*512+x]; } }
						else {
							int oddY=y==14 ? 14 : y-1; uint32_t a=full[y*512+x],b=full[oddY*512+x];
							result[y*512+x]=bobBlend ? (((a^b)&0xFFFEFEFEu)>>1)+(a&b) : field ? b : a;
						}
					} } return result;
				};
				if(bobBlend) { auto lock=emu->AcquireLock(); cfg.DeinterlaceMode=SnesDeinterlaceMode::BobBlend; }
				if(string(filename).find("mixed-raster")!=string::npos) {
					group.ArtworkWidth=16; group.ArtworkHeight=400; group.MemoryConditions={{0,0,0}};
					for(int y=0;y<400;y++) { for(int x=0;x<16;x++) { group.Artwork.push_back((x&1) ? 0xFFFFFFFF : 0xFF0000FF); } }
					emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(wait([&]() {
						if(!renderer.Capture || !renderer.Capture->SnesHiresSource || renderer.Width!=512 || renderer.Height!=478) { return false; }
						const auto& raw=*renderer.Capture->SnesHiresSource; bool sawHires=false,sawOrdinary=false;
						for(int y=0;y<478;y++) { for(int x=0;x<512;x++) {
							uint32_t expected=native[y*512+x];
							if(y>=30 && y<430 && x>=16 && x<=32) {
								bool hires=raw.SnesSubOutput[(y/2)*raw.Width+x/2].Captured; sawHires|=hires; sawOrdinary|=!hires;
								expected=hires ? (x==16 ? 0xFF0000FFu : 0xFF7F7FFFu) : (x==32 || !(x&1) ? 0xFF0000FFu : 0xFFFFFFFFu);
							}
							if(renderer.Pixels[y*512+x]!=expected) { return false; }
						} } return sawHires && sawOrdinary;
					})) { flags|=2; }
					emu->GetVideoDecoder()->SetMetaReplacements({},{}); if(wait([&]() { return renderer.Pixels==native; })) { flags|=4; }
					emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer); emu->Release(); return flags;
				}
				group.ArtworkWidth=group.ArtworkHeight=21; group.ArtworkOriginX=group.ArtworkOriginY=1; group.MemoryConditions={{0,0,0}};
				for(int y=0;y<21;y++) { for(int x=0;x<21;x++) { uint32_t color=x%3==0 ? 0xFF0000u : x%3==1 ? 0x00FF00u : 0xFFFFFFu; uint32_t alpha=x%7==0 ? 0 : x%7==1 ? 128 : 255; group.Artwork.push_back(color|(alpha<<24)); } }
				struct Sample { uint32_t Color; bool Math,Artwork; int Priority; double Weight; };
				auto samples=[&](int x,int y,bool main) {
					std::vector<Sample> values; int ax=x-14,ay=y-28; bool enabled=main ? !(window && x/2>=12 && x/2<=15) : both;
					uint32_t color=enabled && ax>=0 && ax<21 && ay>=0 && ay<21 ? group.Artwork[ay*21+ax] : 0;
					uint32_t alpha=color>>24; if(alpha) { values.push_back({color|0xFF000000,true,true,1,alpha/255.0}); }
					if(alpha<255) { values.push_back({0xFF0000FF,false,false,0,(255-alpha)/255.0}); } return values;
				};
				auto colorMath=[&](uint32_t primary,uint32_t other,bool math,bool half,bool artwork) {
					uint32_t color=0xFF000000; for(int shift : {0,8,16}) { int a=(primary>>shift)&255,b=(other>>shift)&255; if(!artwork) { a>>=3; b>>=3; }
						if(math) { a+=b; if(half) { a/=2; } a=std::min(artwork ? 255 : 31,a); } if(!artwork) { a=(a<<3)|(a>>2); } color|=(uint32_t)a<<shift;
					} return color;
				};
				vector<uint32_t> expected=native;
				for(int y=28;y<49;y++) { for(int x=14;x<36;x++) {
					int cell=x/2,control=cell ? cell-1 : 0; double channels[3]={};
					auto primary=samples(x,y,(x&1)!=0);
					auto secondary=samples((x&1) ? cell*2 : control*2+1,y,(x&1)==0);
					auto priority=(x&1) ? std::vector<Sample>{{0,false,false,0,1}} : samples(control*2,y,false);
					for(const auto& a : primary) { for(const auto& b : secondary) { for(const auto& p : priority) {
						bool enabled=(x&1) ? a.Math : b.Math,useSub=operand && ((x&1) ? b.Priority : p.Priority)>0,half=!operand || useSub;
						uint32_t other=useSub ? (!(x&1) && !cell ? 0xFF000000u : b.Color) : 0xFF0000FFu;
						bool artwork=a.Artwork || (useSub && !(!(x&1) && !cell) && b.Artwork);
						uint32_t color=colorMath(a.Color,other,enabled,half,artwork); double weight=a.Weight*b.Weight*p.Weight;
						for(int c=0;c<3;c++) { channels[c]+=((color>>(c*8))&255)*weight; }
					} } }
					uint32_t color=0xFF000000; for(int c=0;c<3;c++) { color|=(uint32_t)(channels[c]+0.5)<<(c*8); } expected[y*512+x]=color;
				} }
				emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
				uint8_t seenFields=0;
				if(wait([&]() {
					if(renderer.Width!=512 || renderer.Height!=478 || !renderer.Capture || !renderer.Capture->SnesHiresSource) { return false; }
					int field=phase(); if(renderer.Pixels!=present(expected,field)) { return false; } seenFields|=(uint8_t)(1<<field); return !(currentField || bobField) || seenFields==3;
				})) { flags|=2; }
				else { std::lock_guard<std::mutex> lock(renderer.Mutex); for(size_t i=0;i<expected.size() && i<renderer.Pixels.size();i++) { if(expected[i]!=renderer.Pixels[i]) { std::cerr << "Hires math mismatch " << filename << " x=" << i%512 << " y=" << i/512 << " actual=" << std::hex << renderer.Pixels[i] << " expected=" << expected[i] << std::dec << std::endl; break; } } }
				emu->GetVideoDecoder()->SetMetaReplacements({},{}); if(wait([&]() { return renderer.Capture && renderer.Pixels==present(native,phase()); })) { flags|=4; }
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpriteSnesHiresCaptureTest(const char* filename)
	{
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		auto& cfg = emu->GetSettings()->GetSnesConfig();
		cfg.DisableFrameSkipping = true; cfg.RamPowerOnState = RamState::AllZeros; cfg.ColorCorrection = SnesColorCorrectionMode::None;
		MetaSpriteTestRenderer renderer; uint32_t flags = 0;
		bool both = string(filename).find("both-screens") != string::npos;
		bool window = string(filename).find("window") != string::npos;
		bool backgroundFixture = string(filename).find("hires-background") != string::npos;
		bool trueHires = string(filename).find("mode5") != string::npos || string(filename).find("mode6") != string::npos;
		auto eligiblePhase = [&](int x) { return (x & 1) ? !window || x / 2 < 4 : both; };
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			if(auto snes = dynamic_cast<SnesConsole*>(emu->GetConsole().get())) {
				snes->GetPpu()->SetMetaSpriteCapture(true);
				auto waitFor = [&](std::function<bool()> condition) {
					for(int attempt = 0; attempt < 500; attempt++) {
						{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
					return false;
				};
				MetaSpriteGraphic source; vector<uint32_t> native;
				if(waitFor([&]() {
					if(renderer.Width != 512 || renderer.Height != 478 || !renderer.Capture
						|| renderer.Capture->Width != 512 || renderer.Capture->Height != 478 || renderer.Capture->SpriteCoordinateScale != 2) { return false; }
					for(const auto& candidate : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
						if(candidate.HardwareSprite != 0 || candidate.X != 8 || candidate.Y != 15 || candidate.Width != 8 || candidate.Height != 8) { continue; }
						for(int y = 0; y < 8; y++) { for(int x = 0; x < 8; x++) {
							if(candidate.Pixels[y * 8 + x] != (x == 7 ? 0 : 0xFFFF0000u)) { return false; }
						} }
						for(int y = 0; y < 16; y++) { for(int x = 0; x < 16; x++) {
							size_t pos = (30 + y) * 512 + 16 + x;
							bool eligible = eligiblePhase(x);
							if(renderer.Pixels[pos] != (eligible && x / 2 < 7 ? 0xFFFF0000u : 0xFF0000FFu)) { return false; }
							const auto& pixel = renderer.Capture->Pixels[pos];
							bool covered = pixel.VisibleSprite == 0 || std::any_of(pixel.SnesCoverage.begin(), pixel.SnesCoverage.end(), [](const auto& c) { return c.Sprite == 0 && c.Main; });
							if(covered != eligible || pixel.SnesMathEnabled) { return false; }
						} }
						source = candidate; native = renderer.Pixels; return true;
					}
					return false;
				})) {
					flags = 1;
					MetaSpriteReplacement group; group.Id = 1; group.ArtworkScale = 2; group.ArtworkWidth = group.ArtworkHeight = 16;
					group.Artwork.assign(256, 0xFF00FF00); group.Artwork[0] = group.Artwork[14] = 0;
					group.Artwork[1] = group.Artwork[15] = 0x8000FF00;
					source.X = source.Y = 0; group.Members = {source}; group.MemoryConditions = {{0, 0, 0}};
					emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(waitFor([&]() {
						if(renderer.Width != 512 || renderer.Height != 478 || !renderer.Capture
							|| renderer.Capture->MemorySnapshot.size() != 1 || !renderer.Capture->MemorySnapshot.count(0)) { return false; }
						for(int y = 0; y < 478; y++) { for(int x = 0; x < 512; x++) {
							uint32_t expected = native[y * 512 + x];
							int ax = x - 16, ay = y - 30;
							if(ax >= 0 && ax < 16 && ay >= 0 && ay < 16 && eligiblePhase(ax)) {
								expected = ay == 0 && (ax == 0 || ax == 14) ? 0xFF0000FF
									: ay == 0 && (ax == 1 || ax == 15) ? 0xFF00807F : 0xFF00FF00;
								}
							if(renderer.Pixels[y * 512 + x] != expected) { return false; }
						} }
						return true;
					})) { flags |= 2; }
					if(flags & 2) {
						vector<uint32_t> originalArtworkFrame;
						{ std::lock_guard<std::mutex> lock(renderer.Mutex); originalArtworkFrame = renderer.Pixels; }
						MetaSpriteReplacement cleared = group; std::fill(cleared.Artwork.begin(), cleared.Artwork.end(), 0);
						emu->GetVideoDecoder()->SetMetaSpriteGroups({cleared});
						vector<uint32_t> expandedExpected; shared_ptr<const MetaSpriteFrame> destinations;
						bool clearedCaptured = waitFor([&]() {
							if(renderer.Width != 512 || renderer.Height != 478 || !renderer.Capture
								|| renderer.Pixels[34 * 512 + 17] != 0xFF0000FF) { return false; }
							expandedExpected = renderer.Pixels; destinations = renderer.Capture; return true;
						});
						if(clearedCaptured) {
							MetaSpriteReplacement enlarged = group;
							enlarged.ArtworkWidth = enlarged.ArtworkHeight = 21; enlarged.ArtworkOriginX = enlarged.ArtworkOriginY = 1;
							enlarged.Artwork.assign(441, 0xFF00FF00); enlarged.Artwork[0] = 0; enlarged.Artwork[1] = 0x8000FF00;
							int marginPixels = 0;
							for(int y = 0; y < 21; y++) { for(int x = 0; x < 21; x++) {
								int dx = x + 14, dy = y + 28; size_t offset = (size_t)dy * 512 + dx;
								const auto& pixel = destinations->Pixels[offset];
								if(!pixel.SnesFallbackCaptured || !pixel.SnesObjectsMain
									|| pixel.SnesObjectPriorities[source.SnesPriorityCode] <= (pixel.SnesFallbackMainFlags & 15)) { continue; }
								uint32_t color = enlarged.Artwork[y * 21 + x];
								expandedExpected[offset] = MetaSpriteCompositor::CompositeAlpha(color, expandedExpected[offset], color >> 24);
								if((color >> 24) && (dx < 16 || dx >= 32 || dy < 30 || dy >= 46)) { marginPixels++; }
							} }
							emu->GetVideoDecoder()->SetMetaSpriteGroups({enlarged});
							if(!marginPixels || !waitFor([&]() { return renderer.Width == 512 && renderer.Height == 478 && renderer.Pixels == expandedExpected; })) { flags &= ~2u; }
							else { std::cout << "PASS: SNES hires full PNG canvas, origin, phase/window gating and partial native edges." << std::endl; }
						} else { flags &= ~2u; }
						emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
						if(!waitFor([&]() { return renderer.Width == 512 && renderer.Height == 478 && renderer.Pixels == originalArtworkFrame; })) { flags &= ~2u; }
					}
					if(backgroundFixture && (flags & 2)) {
						vector<uint32_t> spriteOnly;
						{ std::lock_guard<std::mutex> lock(renderer.Mutex); spriteOnly = renderer.Pixels; }
						MetaBackgroundReplacement background;
						background.Id = 1; background.Layer = 0; background.Width = 8; background.Height = 4; background.Scale = 1;
						background.X = 3; background.Y = -2; background.WrapX = background.WrapY = true; background.MemoryConditions = {{0, 0, 0}};
						for(int y = 0; y < 4; y++) { for(int x = 0; x < 8; x++) { background.Artwork.push_back(0xFF000055 | ((x + 1) * 24 << 16) | ((y + 1) * 48 << 8)); } }
						background.Artwork[0] = 0; background.Artwork[1] = 0x80FFFFFF;
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
						bool validBackground = waitFor([&]() {
							if(renderer.Width != 512 || renderer.Height != 478 || !renderer.Capture
								|| renderer.Capture->SnesBackgrounds.size() != 512 * 478 || renderer.Capture->MemorySnapshot.size() != 1) { return false; }
							for(int y = 0; y < 478; y++) { for(int x = 0; x < 512; x++) {
								uint32_t expected = spriteOnly[y * 512 + x];
								if(y >= 14 && y < 462 && !(x & 1)) {
									int sampleX = trueHires ? (x + 506) & 511 : (x / 2 + 253) & 255;
									int sampleY = (y / 2 - 6 + 250) & 255;
									const auto& captured = renderer.Capture->SnesBackgrounds[y * 512 + x].Layers[0];
									if(!captured.Main || !captured.Opaque || captured.Sample.X != sampleX || captured.Sample.Y != sampleY) { return false; }
									uint32_t artwork = background.Artwork[((sampleY + 2) & 3) * 8 + ((sampleX - 3) & 7)];
									expected = MetaSpriteCompositor::CompositeAlpha(artwork, 0xFF0000FF, artwork >> 24);
								}
								if(renderer.Pixels[y * 512 + x] != expected) { return false; }
							} }
							return true;
						});
						background.Scale = 2; background.Width = 16; background.Height = 8; background.Artwork.clear();
						for(int y = 0; y < 8; y++) { for(int x = 0; x < 16; x++) { background.Artwork.push_back(0xFF000055 | ((x + 1) * 14 << 16) | ((y + 1) * 28 << 8)); } }
						background.Artwork[0] = 0; background.Artwork[1] = 0x80FFFFFF;
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
						bool validScaledBackground = waitFor([&]() {
							if(renderer.Width != 1024 || renderer.Height != 956) { return false; }
							for(int y = 0; y < 956; y++) { for(int x = 0; x < 1024; x++) {
								int px = x / 2, py = y / 2;
								uint32_t expected = spriteOnly[py * 512 + px];
								if(py >= 14 && py < 462 && !(px & 1)) {
									int sampleX = trueHires ? (px + 506) & 511 : (px / 2 + 253) & 255;
									int sampleY = (py / 2 - 6 + 250) & 255;
									int ax = ((sampleX - 3) & 7) * 2 + (trueHires ? x & 1 : (x & 3) / 2);
									int ay = ((sampleY + 2) & 3) * 2 + (y & 3) / 2;
									uint32_t artwork = background.Artwork[ay * 16 + ax];
									expected = MetaSpriteCompositor::CompositeAlpha(artwork, 0xFF0000FF, artwork >> 24);
								}
								if(renderer.Pixels[y * 1024 + x] != expected) { return false; }
							} }
							return true;
						});
						background.MemoryConditions = {{0, 255, 255}};
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
						if(!validBackground || !validScaledBackground || !waitFor([&]() { return renderer.Pixels == spriteOnly; })) { flags &= ~2u; }
					}
					group.MemoryConditions = {{0, 255, 255}}; emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
					bool conditionRestored = waitFor([&]() { return renderer.Pixels == native; });
					emu->GetVideoDecoder()->SetMetaReplacements({}, {});
					if(conditionRestored && waitFor([&]() { return renderer.Width == 512 && renderer.Height == 478 && renderer.Pixels == native; })) { flags |= 4; }
				}
				if(flags != 7) {
					std::lock_guard<std::mutex> lock(renderer.Mutex);
					std::cerr << "SNES hires phase verification failed: " << filename << "; output=" << renderer.Width << "x" << renderer.Height
						<< "; capture=" << (renderer.Capture != nullptr) << "; flags=" << flags << std::endl;
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}

	static uint32_t RunMetaSpriteSnesCaptureTestImpl(const char* filename, const std::vector<MetaSpriteReplacement>* authored, const std::vector<MetaBackgroundReplacement>* animatedBackground = nullptr)
	{
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode); emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg = emu->GetSettings()->GetSnesConfig(); cfg.DisableFrameSkipping = true; cfg.RamPowerOnState = RamState::AllZeros;
		cfg.ColorCorrection = SnesColorCorrectionMode::None;
		MetaSpriteGraphic source; uint32_t nativeWidth = 0, nativeHeight = 0, backdrop = 0; int spriteX = 0, spriteY = 0;
		MetaSpriteTestRenderer renderer; uint32_t flags = 0;
		bool currentField = string(filename).find("current-field") != string::npos;
		if(currentField) { cfg.DeinterlaceMode = SnesDeinterlaceMode::CurrentField; }
		bool bobBlendField = string(filename).find("bob-blend-field") != string::npos;
		bool firstPictureArtwork = string(filename).find("first-picture") != string::npos;
		bool bobField = bobBlendField || string(filename).find("bob-field") != string::npos;
		if(bobField) { cfg.DeinterlaceMode = bobBlendField ? SnesDeinterlaceMode::BobBlend : SnesDeinterlaceMode::Bob; }
		bool holes = string(filename).find("holes") != string::npos;
		bool window = string(filename).find("window") != string::npos;
		bool middleWindow = string(filename).find("middle") != string::npos;
		auto masked = [&](int x) { return window && (middleWindow ? x >= 2 && x < 6 : x >= 4); };
		bool math = string(filename).find("math") != string::npos && string(filename).find("exempt") == string::npos;
		uint32_t nativeSpriteColor = math ? 0xFF7B007B : 0xFFFF0000;
		bool sub = string(filename).find("subscreen") != string::npos;
		bool backgroundSub = string(filename).find("background-sub") != string::npos;
		bool backgroundScroll = string(filename).find("scroll") != string::npos;
		bool mode7 = string(filename).find("mode7") != string::npos;
		bool mode7Turn = mode7 && string(filename).find("turn") != string::npos;
		bool mode7Scale = mode7 && string(filename).find("scale") != string::npos;
		uint8_t mode7Bg2Priority = string(filename).find("ext-high") != string::npos ? 5 : 1;
		auto sampleX = [&](int x, int y) { return mode7 ? (mode7Turn ? y : mode7Scale ? x * 2 : x) & 1023 : (x + (backgroundScroll ? 253 : 0)) & 255; };
		auto sampleY = [&](int x, int y) { return mode7 ? (mode7Turn ? -x : y) & 1023 : (y + (backgroundScroll ? 250 : 0)) & 255; };
		bool backgroundHoles = string(filename).find("bgholes") != string::npos;
		bool backgroundMask = string(filename).find("bgmask") != string::npos;
		int backgroundLayer = 0;
		for(int layer = 1; layer < 4; layer++) { if(string(filename).find("bg" + std::to_string(layer)) != string::npos) { backgroundLayer = layer; } }
		bool overlap = string(filename).find("overlap") != string::npos;
		bool rotate = string(filename).find("rotate") != string::npos;
		bool transparent = string(filename).find("transparent") != string::npos;
		if(transparent) { nativeSpriteColor = 0xFF0000FF; }
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			// Loading creates the rewind manager, which clears MaximumSpeed.
			if(string(filename).find("frame-timing") != string::npos) { emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed); }
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			if(auto snes = dynamic_cast<SnesConsole*>(emu->GetConsole().get())) {
				snes->GetPpu()->SetMetaSpriteCapture(true);
				if(string(filename).find("obj-interlace") != string::npos) {
					bool horizontalFlip=string(filename).find("hflip")!=string::npos;
					bool verticalFlip=string(filename).find("vflip")!=string::npos;
					bool multiMember=string(filename).find("obj-interlace-group")!=string::npos;
					bool rectangular=string(filename).find("obj-interlace-rect")!=string::npos;
					bool changingPoses=string(filename).find("obj-interlace-poses")!=string::npos;
					int sourceWidth=rectangular ? 16 : 8, sourceHeight=rectangular ? 32 : 8;
					auto sourcePixel = [&](int x,int y) {
						int sx=horizontalFlip ? sourceWidth-1-x : x;
						int sy=verticalFlip ? (y<sourceWidth ? sourceWidth-1-y : sourceWidth*3-1-y) : y;
						return sx>=7 || sy>=8 ? 0u : (sy & 1) ? 0xFF0000FFu : 0xFFFF0000u;
					};
					auto waitForField = [&](std::function<bool()> condition) {
						for(int attempt=0;attempt<500;attempt++) {
							{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						} return false;
					};
					uint8_t capturedPhases=0;
					bool captured = waitForField([&]() {
						if(!renderer.Capture || renderer.Width!=512 || renderer.Height!=478) { return false; }
						uint8_t phases=0;
						for(const auto& graphic : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
							if((graphic.HardwareSprite & 127)!=0 || graphic.X!=8 || graphic.Y!=30 || graphic.Width!=sourceWidth || graphic.Height!=sourceHeight || graphic.SnesRowStep!=2) { continue; }
							uint32_t first=graphic.Pixels[graphic.SnesRowPhase*sourceWidth];
							bool secondPose=changingPoses && (first==0xFF00FF00 || first==0xFFFFFFFF);
							for(int y=graphic.SnesRowPhase;y<sourceHeight;y+=2) { for(int x=0;x<sourceWidth;x++) {
								uint32_t expected=sourcePixel(x,y);
								if(secondPose && expected) { expected=expected==0xFFFF0000 ? 0xFF00FF00 : 0xFFFFFFFF; }
								if(graphic.Pixels[y*sourceWidth+x]!=expected) { return false; }
							} }
							phases |= (uint8_t)(1 << graphic.SnesRowPhase);
						}
						capturedPhases |= phases; return capturedPhases==3;
					});
					if(captured) {
						flags=1; MetaSpriteReplacement replacement; replacement.Id=1;
						if(animatedBackground) {
							emu->GetVideoDecoder()->SetMetaReplacements({},*animatedBackground);
							const auto& layer=animatedBackground->front();
							if(waitForField([&]() {
								if(!renderer.Capture || renderer.Width!=512 || renderer.Height!=478 || renderer.Capture->SnesBackgrounds.empty()) { return false; }
                                const auto& capture=*renderer.Capture;
								if(string(filename).find("bg-fields")!=string::npos) {
									if(!capture.SnesFieldMemoryCaptured || !capture.SnesFieldMemory[0].count(0) || !capture.SnesFieldMemory[0].count(1)
										|| !capture.SnesFieldMemory[1].count(0) || !capture.SnesFieldMemory[1].count(1) || capture.SnesFieldMemory[0].at(0)==capture.SnesFieldMemory[1].at(0)) { return false; }
								}
								if(!capture.SnesBackgrounds[30*512+16].Layers[0].Sample.Valid) { return false; }
								for(int y=0;y<478;y++) { for(int x=0;x<512;x++) {
									uint32_t expected=y<14 || y>=462 ? 0xFF000000u : 0xFF0000FFu;
									if(x>=16 && x<30 && y>=30 && y<38) { expected=(y & 1) ? 0xFF0000FFu : 0xFFFF0000u; }
									auto image=layer.Animation.Select(capture.SnesFieldMemoryCaptured ? capture.SnesFieldFrameNumber[y & 1] : capture.FrameNumber);
									if(!image) { return false; }
                                    const auto& memory=capture.SnesFieldMemoryCaptured ? capture.SnesFieldMemory[y & 1] : capture.MemorySnapshot;
									bool active=MetaMemoryCondition::Matches(layer.MemoryConditions,memory); int cameraX=0,cameraY=0;
									if(layer.UseCpuCamera && layer.FollowScroll) { active &= layer.CameraX.Decode(memory,cameraX) && layer.CameraY.Decode(memory,cameraY); }
									int ax=x/2+cameraX-layer.X+image->OriginX, ay=y/2+cameraY-layer.Y+image->OriginY;
									if(active && y>=14 && y<462 && ax>=0 && ay>=0 && ax<image->Width && ay<image->Height) { expected=(*image->Pixels)[ay*image->Width+ax]; }
									if(renderer.Pixels[y*512+x]!=expected) { return false; }
								} } return true;
							})) { flags|=2; }
							emu->GetVideoDecoder()->SetMetaReplacements({},{});
							if(waitForField([&]() { return renderer.Pixels[30*512+18]==0xFFFF0000 && renderer.Pixels[60*512+40]==0xFF0000FF; })) { flags|=4; }
							std::cout << "SNES background PNG animation whole-frame probe flags: " << flags << std::endl;
							emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer); emu->Release(); return flags;
						}
						MetaSpriteGraphic member {0,0,sourceWidth,sourceHeight,-1,std::vector<uint32_t>(sourceWidth*sourceHeight)};
						for(int y=0;y<sourceHeight;y++) { for(int x=0;x<sourceWidth;x++) { member.Pixels[y*sourceWidth+x]=sourcePixel(x,y); } }
						replacement.Members={member};
						if(multiMember) { member.X=12; member.Y=8; replacement.Members.push_back(member); }
						replacement.ArtworkWidth=replacement.ArtworkHeight=12;
						replacement.ArtworkOriginX=replacement.ArtworkOriginY=1;
						replacement.Artwork.assign(144,0xFF00FF00); replacement.Artwork[0]=0; replacement.Artwork[1]=0x8000FF00;
						if(authored) { replacement=authored->front(); }
						if(changingPoses) {
							auto second=replacement; second.Id=2;
							for(auto& color : second.Members[0].Pixels) { if(color) { color=color==0xFFFF0000 ? 0xFF00FF00 : 0xFFFFFFFF; } }
							second.Artwork.assign(144,0xFFFFFF00); second.Artwork[0]=0; second.Artwork[1]=0x80FFFF00;
							if(string(filename).find("poses-conditions")!=string::npos) { replacement.MemoryConditions={{0,0,0xFF}}; second.MemoryConditions={{0,1,0xFF}}; }
							emu->GetVideoDecoder()->SetMetaSpriteGroups({replacement,second});
						} else { emu->GetVideoDecoder()->SetMetaSpriteGroups({replacement}); }
						uint8_t renderedPhases=0;
						if(waitForField([&]() {
							if(!renderer.Capture || renderer.Width!=512 || renderer.Height!=478) { return false; }
							int phase=bobField ? renderer.Capture->SnesOutputOffsetY : 0;
							if(currentField) { phase=renderer.Capture->Pixels[30*512].SnesFallbackCaptured ? 0 : 1; }
							int poseByField[2]={-1,-1};
							if(changingPoses) {
								for(const auto& graphic : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
									if((graphic.HardwareSprite & 127)!=0 || graphic.SnesField<1 || graphic.SnesField>2) { continue; }
									uint32_t first=graphic.Pixels[graphic.SnesRowPhase*graphic.Width];
									poseByField[graphic.SnesField-1]=(first==0xFF00FF00 || first==0xFFFFFFFF) ? 1 : 0;
								}
								if(poseByField[0]<0 || poseByField[1]<0 || poseByField[0]==poseByField[1]) { return false; }
							}
							auto expectedPixel = [&](int x,int y,int fieldPhase) {
								if(y<14 || y>=462 || (currentField && (y & 1)!=fieldPhase)) { return 0xFF000000u; }
								if(window && x>=24 && x<32) { return 0xFF0000FFu; }
								int sourceY=bobField ? (fieldPhase ? (y==14 ? 15 : ((y-1)/2)*2+1) : (y/2)*2) : y;
								if(authored) {
									const auto& capture=*renderer.Capture; int field=y & 1;
									const auto& snapshot=capture.SnesFieldMemoryCaptured ? capture.SnesFieldMemory[field] : capture.MemorySnapshot;
									auto image=replacement.SelectArtwork(snapshot,capture.SnesFieldMemoryCaptured ? capture.SnesFieldFrameNumber[field] : capture.FrameNumber);
									int ax=x/2-(8+replacement.OriginX-image.ArtworkOriginX);
									int ay=sourceY-(30+replacement.OriginY-image.ArtworkOriginY);
									if(ax>=0 && ay>=0 && ax<image.ArtworkWidth && ay<image.ArtworkHeight) {
										uint32_t color=image.Artwork[ay*image.ArtworkWidth+ax];
										return (color>>24)==255 ? color : 0xFF0000FFu;
									} return 0xFF0000FFu;
								}
								if(x>=14 && x<38 && sourceY>=29 && sourceY<41) {
									if(changingPoses && poseByField[y & 1]==1) { return sourceY==29 && x<16 ? 0xFF0000FFu : sourceY==29 && x<18 ? 0xFF80807Fu : 0xFFFFFF00u; }
									return sourceY==29 && x<16 ? 0xFF0000FFu : sourceY==29 && x<18 ? (math ? 0xFF0040BFu : 0xFF00807Fu) : (math ? 0xFF007F7Fu : 0xFF00FF00u);
								} return 0xFF0000FFu;
							};
							for(int y=0;y<478;y++) { for(int x=0;x<512;x++) {
								uint32_t expected=expectedPixel(x,y,phase);
								if(bobBlendField) { uint32_t previous=expectedPixel(x,y,1-phase); expected=(((expected ^ previous) & 0xFFFEFEFEu)>>1)+(expected & previous); }
								if(renderer.Pixels[y*512+x]!=expected) { return false; }
							} } renderedPhases |= (uint8_t)(1 << phase); return (!currentField && !bobField) || renderedPhases==3;
						})) { flags|=2; }
						emu->GetVideoDecoder()->SetMetaReplacements({},{});
						if(waitForField([&]() {
							uint32_t native=renderer.Pixels[(verticalFlip ? (rectangular ? 39 : 31) : 30)*512+18];
							return (native==(math ? (bobBlendField ? 0xFF3D00BDu : 0xFF7B007Bu) : (bobBlendField ? 0xFF7F007Fu : 0xFFFF0000u)) || (changingPoses && native==0xFF00FF00)) && renderer.Pixels[40*512+36]==0xFF0000FF;
						})) { flags|=4; }
					}
					std::cout << "SNES OBJ-interlace field capture/full-canvas replacement probe flags: " << flags << std::endl;
					emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer); emu->Release(); return flags;
				}
				for(int attempt = 0; attempt < 500; attempt++) {
					{
						std::lock_guard<std::mutex> lock(renderer.Mutex);
						if(renderer.Capture) {
							for(const auto& graphic : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
								if(graphic.HardwareSprite != 0 || graphic.X != 8 || graphic.Y != 15 || graphic.Width != 8 || graphic.Height != 8) { continue; }
								auto captureIndex = [&](size_t x, size_t y) {
									size_t scale = renderer.Capture->SpriteCoordinateScale;
									size_t index = y * scale * renderer.Capture->Width + x * scale;
									if(bobField) { index += renderer.Capture->SnesOutputOffsetY * renderer.Capture->Width; }
									return currentField && !renderer.Capture->Pixels[index].SnesFallbackCaptured ? index + renderer.Capture->Width : index;
								};
								bool validGraphic = true;
								for(size_t i = 0; i < graphic.Pixels.size(); i++) {
									validGraphic &= graphic.Pixels[i] == (transparent || (holes && i % 8 == 7) ? 0 : 0xFFFF0000u);
									if(transparent || (holes && i % 8 == 7)) {
										const auto& hole = renderer.Capture->Pixels[captureIndex(8 + i % 8, 15 + i / 8)];
										bool eligible = std::any_of(hole.SnesCoverage.begin(), hole.SnesCoverage.end(), [&](const auto& c) { return c.Sprite == 0 && (sub ? c.Sub : c.Main); });
										validGraphic &= hole.VisibleSprite == (overlap ? 1 : -1) && eligible == (!masked(7) && !rotate);
										if(math) { validGraphic &= std::any_of(hole.SnesCoverage.begin(), hole.SnesCoverage.end(), [](const auto& c) { return c.Sprite == 0 && c.Main && c.Math; }); }
									}
								}
								if(!validGraphic) { continue; }
								const auto& pixel = renderer.Capture->Pixels[captureIndex(8, 15)];
								if(pixel.SnesFirstObject != (rotate ? 1 : 0)) { continue; }
								if(pixel.VisibleSprite == (sub || transparent ? -1 : 0) && pixel.SecondarySprite == (sub ? 0 : -1) && pixel.SpriteColor == (transparent ? 0x7C00 : 31) && pixel.Brightness == 15
									&& pixel.SnesFallbackCaptured && pixel.SnesFallbackMainColor == (sub ? 31 : 0x7C00) && pixel.SnesFallbackSubColor == 0x7C00
									&& pixel.SnesFallbackMainLayer == (sub ? backgroundLayer : 0xFF) && pixel.SnesFallbackSubLayer == (backgroundSub ? backgroundLayer : 0xFF)
									&& pixel.BackgroundSample.Valid == sub && pixel.SnesSubBackgroundSample.Valid == backgroundSub
									&& (!sub || (pixel.BackgroundSample.X == sampleX(8, 9) && pixel.BackgroundSample.Y == sampleY(8, 9)))
									&& (!backgroundSub || (pixel.SnesSubBackgroundSample.X == sampleX(8, 9) && pixel.SnesSubBackgroundSample.Y == sampleY(8, 9)))) {
									bool validCoordinates = true;
									if(sub || backgroundSub) {
										for(int y = 7; y < 231; y++) {
											for(int x = 0; x < 256; x++) {
												const auto& captured = renderer.Capture->Pixels[y * 256 + x];
												const auto& sample = sub ? captured.BackgroundSample : captured.SnesSubBackgroundSample;
												if(mode7 && backgroundLayer == 1) {
													validCoordinates &= (sub ? captured.SnesFallbackMainFlags & 0x0F : captured.SnesFallbackSubPriority) == mode7Bg2Priority;
												}
												if(backgroundMask && x >= 40 && x <= 95) { validCoordinates &= !sample.Valid; continue; }
												if(backgroundHoles && (x & 7) == 7 && ((y - 6) & 7) == 0) { validCoordinates &= !sample.Valid; continue; }
												validCoordinates &= sample.Valid && sample.Layer == backgroundLayer
													&& sample.X == sampleX(x, y - 6)
													&& sample.Y == sampleY(x, y - 6);
											}
										}
									}
									if(!validCoordinates) { continue; }
									auto red = std::find(renderer.Pixels.begin(), renderer.Pixels.end(), nativeSpriteColor);
									if(transparent && renderer.Width == 256 && renderer.Height == 239) { red = renderer.Pixels.begin() + 15 * 256 + 8; }
									if(red != renderer.Pixels.end()) {
										source = graphic; flags = 1; nativeWidth = renderer.Width; nativeHeight = renderer.Height; backdrop = renderer.Pixels[0];
										spriteX = (int)(red - renderer.Pixels.begin()) % renderer.Width; spriteY = (int)(red - renderer.Pixels.begin()) / renderer.Width;
									}
								}
							}
						}
					}
					if(flags) { break; } std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
				if(!flags) {
					std::lock_guard<std::mutex> lock(renderer.Mutex);
					std::cerr << "SNES capture failed: " << filename;
					if(renderer.Capture) {
						const auto& center = renderer.Capture->Pixels[15 * renderer.Capture->SpriteCoordinateScale * renderer.Capture->Width + 8 * renderer.Capture->SpriteCoordinateScale];
						std::cerr << " center visible=" << center.VisibleSprite << " secondary=" << center.SecondarySprite
							<< " color=" << center.SpriteColor << " fallback=" << center.SnesFallbackMainColor << "," << center.SnesFallbackSubColor
							<< " layers=" << (int)center.SnesFallbackMainLayer << "," << (int)center.SnesFallbackSubLayer
							<< " samples=" << center.BackgroundSample.Valid << "," << center.SnesSubBackgroundSample.Valid;
						for(const auto& graphic : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) { std::cerr << " sprite=" << graphic.HardwareSprite << " pos=" << graphic.X << "," << graphic.Y << " first=" << std::hex << graphic.Pixels[0] << std::dec; }
						const auto& hole = renderer.Capture->Pixels[15 * 256 + 15];
						std::cerr << " hole winner=" << hole.VisibleSprite << " coverage=";
						for(const auto& c : hole.SnesCoverage) { std::cerr << c.Sprite << ":" << c.Main << ":" << c.Order << ","; }
					}
					std::cerr << std::endl;
				}
				if(flags) {
					MetaSpriteReplacement group; group.Id = 1; group.ArtworkScale = 2; group.ArtworkWidth = group.ArtworkHeight = 16;
					group.Artwork.assign(256, 0xFF00FF00); group.Artwork[0] = 0; group.Artwork[1] = 0x8000FF00;
					if(holes) { group.Artwork[14] = 0; group.Artwork[15] = 0x8000FF00; }
					source.X = source.Y = 0; group.Members = {source};
					auto waitFor = [&](std::function<bool()> condition) {
						for(int attempt = 0; attempt < 500; attempt++) {
							{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						} return false;
					};
					group.MemoryConditions = {{0, 0, 0}}; emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(string(filename).find("interlace") != string::npos) {
						MetaSpriteReplacement expanded = group;
						expanded.ArtworkWidth = expanded.ArtworkHeight = 21;
						expanded.ArtworkOriginX = expanded.ArtworkOriginY = 1;
						if(firstPictureArtwork) { expanded.ArtworkOriginY = 8; }
						expanded.Artwork.assign(441, 0xFF00FF00); expanded.Artwork[0] = 0; expanded.Artwork[1] = 0x8000FF00;
						emu->GetVideoDecoder()->SetMetaSpriteGroups({expanded});
						uint8_t bobParities = 0;
						bool rendered = waitFor([&]() {
							if(renderer.Width != 512 || renderer.Height != 478 || !renderer.Capture
								|| renderer.Capture->MemorySnapshot.size() != 1) { return false; }
							for(int y = 0; y < 478; y++) { for(int x = 0; x < 512; x++) {
								int top = firstPictureArtwork ? 14 : 28;
								int ax = x - 14, ay = y + renderer.Capture->Pixels[y * 512 + x].SnesOutputSampleOffsetY - top - renderer.Capture->SnesOutputOffsetY;
								uint32_t expected = y < 14 || y >= 462 ? 0xFF000000 : 0xFF0000FF;
								if(ax >= 0 && ay >= 0 && ax < 21 && ay < 21) {
									expected = expanded.Artwork[ay * 21 + ax];
									if(!(expected >> 24)) { expected = 0xFF0000FF; }
									else if((expected >> 24) == 128) { expected = 0xFF00807F; }
								}
								if(currentField && ((y & 1) != (renderer.Capture->Pixels[30 * 512 + 16].SnesFallbackCaptured ? 0 : 1))) { expected = 0xFF000000; }
								if(bobBlendField) {
									int otherOffset = 1 - renderer.Capture->SnesOutputOffsetY;
									int otherY = y + (firstPictureArtwork && y == 14 ? otherOffset : 0) - top - otherOffset;
									uint32_t previous = y < 14 || y >= 462 ? 0xFF000000 : 0xFF0000FF;
									if(ax >= 0 && otherY >= 0 && ax < 21 && otherY < 21) {
										uint32_t artworkPixel = expanded.Artwork[otherY * 21 + ax];
										previous = !(artworkPixel >> 24) ? 0xFF0000FF : (artworkPixel >> 24) == 128 ? 0xFF00807F : artworkPixel;
									}
									expected = (((expected ^ previous) & 0xFFFEFEFEu) >> 1) + (expected & previous);
								}
								if(renderer.Pixels[y * 512 + x] != expected) { return false; }
							} }
							bobParities |= static_cast<uint8_t>(1u << renderer.Capture->SnesOutputOffsetY);
							return !bobField || bobParities == 3;
						});
						if(rendered) { flags |= 2; }
						else {
							flags |= 128;
							std::lock_guard<std::mutex> lock(renderer.Mutex);
							std::cerr << "Woven output dimensions=" << renderer.Width << "," << renderer.Height
								<< " margin=" << std::hex << renderer.Pixels[28 * renderer.Width + 16]
								<< " body=" << renderer.Pixels[34 * renderer.Width + 20] << std::dec << std::endl;
						}
						uint8_t sampled = 0;
						if(bobBlendField && !firstPictureArtwork && rendered) {
							bool stateLoaded = false;
							{
								auto stateLock = emu->AcquireLock();
								emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
								std::stringstream stateStream;
								emu->GetSaveStateManager()->SaveState(stateStream);
								stateStream.seekg(0);
								stateLoaded = emu->GetSaveStateManager()->LoadState(stateStream);
								emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
								std::lock_guard<std::mutex> lock(renderer.Mutex);
								renderer.FirstCapturePixels.clear(); renderer.RecordNextCapture = true;
							}
							if(!stateLoaded || !waitFor([&]() {
								if(renderer.FirstCapturePixels.size() != 512 * 478) { return false; }
								return renderer.FirstCapturePixels[28 * 512 + 16] == (renderer.FirstCaptureOffsetY ? 0xFF0000FFu : 0xFF00FF00u);
							})) { flags |= 128; }
							else { std::cout << "PASS: BobBlend first composed frame after state restore has no stale temporal edge pixels." << std::endl; }
						}
						{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.Capture && renderer.Capture->MemorySnapshot.count(0)) { sampled = renderer.Capture->MemorySnapshot.at(0); } }
						expanded.MemoryConditions = {{0, static_cast<uint8_t>(sampled ^ 0x80), 0xFF}};
						emu->GetVideoDecoder()->SetMetaSpriteGroups({expanded});
						if(!waitFor([&]() { return renderer.Width == nativeWidth && renderer.Height == nativeHeight && renderer.Pixels[spriteY * nativeWidth + spriteX] == nativeSpriteColor; })) { flags |= 128; }
						emu->GetVideoDecoder()->SetMetaReplacements({}, {});
						if(waitFor([&]() { return renderer.Capture && renderer.Capture->MemorySnapshot.empty() && renderer.Width == nativeWidth && renderer.Pixels[spriteY * nativeWidth + spriteX] == nativeSpriteColor; })) { flags |= 4; }
						emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
						emu->Release(); return flags;
					}
					if(waitFor([&]() {
						if(renderer.Width != nativeWidth * 2 || renderer.Height != nativeHeight * 2) { return false; }
						for(int y = 0; y < 16; y++) { for(int x = 0; x < 16; x++) {
							uint32_t expected = y == 0 && (x == 0 || (holes && x == 14)) ? 0xFF0000FF : y == 0 && (x == 1 || (holes && x == 15)) ? 0xFF00807F : 0xFF00FF00;
							if(math) { expected = y == 0 && (x == 0 || (holes && x == 14)) ? 0xFF0000FF : y == 0 && (x == 1 || (holes && x == 15)) ? 0xFF0040BF : 0xFF007F7F; }
							if(sub) { expected = y == 0 && (x == 0 || (holes && x == 14)) ? 0xFFFF00FF : y == 0 && (x == 1 || (holes && x == 15)) ? 0xFFBF407F : 0xFF7F7F00; }
							// Before a background PNG is loaded, native BG holes expose
							// the blue backdrop, whose color math is disabled in this ROM.
							if(backgroundHoles && ((spriteX + x / 2) & 7) == 7 && ((spriteY + y / 2 - 6) & 7) == 0) { expected = 0xFF0000FF; }
							if(masked(x / 2)) { expected = 0xFF0000FF; }
							if(rotate && x >= 14) { expected = 0xFF0000FF; }
							if(renderer.Pixels[(spriteY * 2 + y) * renderer.Width + spriteX * 2 + x] != expected) { return false; }
						} } return renderer.Pixels[0] == backdrop;
					})) { flags |= 2; }
					if(!math && !sub && !backgroundSub && !window && !overlap && !rotate && (flags & 2)) {
						MetaSpriteReplacement cleared = group; std::fill(cleared.Artwork.begin(), cleared.Artwork.end(), 0);
						emu->GetVideoDecoder()->SetMetaSpriteGroups({cleared});
						vector<uint32_t> canvasExpected;
						if(!waitFor([&]() {
							if(renderer.Width != nativeWidth * 2 || renderer.Height != nativeHeight * 2
								|| renderer.Pixels[(spriteY * 2 + 4) * renderer.Width + spriteX * 2 + 4] != 0xFF0000FF) { return false; }
							canvasExpected = renderer.Pixels; return true;
						})) { flags |= 128; }
						else {
							MetaSpriteReplacement enlarged = group;
							enlarged.ArtworkWidth = enlarged.ArtworkHeight = 21; enlarged.ArtworkOriginX = enlarged.ArtworkOriginY = 1;
							enlarged.Artwork.assign(441, 0xFF00FF00); enlarged.Artwork[0] = 0; enlarged.Artwork[1] = 0x8000FF00;
							for(int y = 0; y < 21; y++) { for(int x = 0; x < 21; x++) {
								int dx = (spriteX - 1) * 2 + x, dy = (spriteY - 1) * 2 + y;
								if(dx < 0 || dy < 0 || dx >= (int)nativeWidth * 2 || dy >= (int)nativeHeight * 2) { continue; }
								size_t offset = (size_t)dy * nativeWidth * 2 + dx; uint32_t color = enlarged.Artwork[y * 21 + x], alpha = color >> 24;
								uint32_t result = 0xFF000000;
								for(int shift : {0, 8, 16}) { result |= ((((color >> shift) & 255) * alpha + ((canvasExpected[offset] >> shift) & 255) * (255 - alpha) + 127) / 255) << shift; }
								canvasExpected[offset] = result;
							} }
							emu->GetVideoDecoder()->SetMetaSpriteGroups({enlarged});
							if(!waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Pixels == canvasExpected; })) { flags |= 128; }
							else { std::cout << "PASS: SNES live full PNG canvas, shifted origin, alpha and partial native edges." << std::endl; }
						}
						emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					}
					if(string(filename).find("memory") != string::npos) {
						if(!VerifyChangingSnesMemory(emu.get(), renderer, group, spriteX, spriteY, nativeWidth, nativeHeight)) { flags |= 128; }
					} else if(!VerifyLiveMemoryConditions(emu.get(), renderer, group)) { flags |= 128; }
					emu->GetVideoDecoder()->SetMetaReplacements({}, {});
					if(waitFor([&]() { return renderer.Width == nativeWidth && renderer.Height == nativeHeight && renderer.Pixels[spriteY * nativeWidth + spriteX] == nativeSpriteColor; })) { flags |= 4; }
					if(string(filename).find("panorama") != string::npos && !VerifyDedicatedWidescreen(emu.get(), renderer, nativeWidth, nativeHeight, 0, string(filename).find("negative") != string::npos)) { flags |= 128; }
					if(string(filename).find("frame-timing") != string::npos) {
						snes->GetPpu()->SetMetaSpriteCapture(false);
						if(!waitFor([&]() { return !renderer.Capture && renderer.Width == nativeWidth && renderer.Height == nativeHeight; })
							|| !MeasureLiveFrameTiming(emu.get(), renderer, "SNES native without sprite capture")) { flags |= 128; }
						snes->GetPpu()->SetMetaSpriteCapture(true);
						if(!waitFor([&]() { return renderer.Capture != nullptr; })) { flags |= 128; }
						if(!MeasureLiveFrameTiming(emu.get(), renderer, "SNES native with sprite capture")) { flags |= 128; }
						emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
						if(!waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Height == nativeHeight * 2; })
							|| !MeasureLiveFrameTiming(emu.get(), renderer, "SNES metasprite PNG")) { flags |= 128; }
						emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					}
					if(sub) {
						MetaBackgroundReplacement background;
						background.Layer = backgroundLayer;
						background.Id = 1; background.Scale = 2; background.Width = background.Height = 2;
						background.WrapX = background.WrapY = true; background.Artwork.assign(4, 0xFF00FF00);
						background.MemoryConditions = {{0, 0, 0}};
						emu->GetVideoDecoder()->SetMetaBackgroundLayers({background});
						if(!waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Height == nativeHeight * 2
							&& renderer.Capture && renderer.Capture->MemorySnapshot.size() == 1
							&& renderer.Pixels[(spriteY * 2) * renderer.Width + spriteX * 2] == 0xFF7F7F00; })) { flags |= 128; }
						if(backgroundHoles && !waitFor([&]() {
							if(renderer.Width != nativeWidth * 2 || !renderer.Capture || renderer.Capture->SnesBackgrounds.size() != 256 * 239) { return false; }
							for(int y = 14; y < 231; y += 8) { for(int x = 7; x < 256; x += 8) {
								const auto& captured = renderer.Capture->SnesBackgrounds[y * 256 + x].Layers[backgroundLayer];
								bool masked = backgroundMask && x >= 40 && x <= 95;
								if(!captured.Sample.Valid || captured.Opaque || captured.Main == masked
									|| captured.Sample.X != x || captured.Sample.Y != y - 6
									|| renderer.Pixels[(y * 2) * renderer.Width + x * 2] != (masked ? 0xFF0000FF : 0xFF00FFFF)) { return false; }
							} }
							return true;
						})) { flags |= 128; }
						if(backgroundMask && !waitFor([&]() {
							if(renderer.Width != nativeWidth * 2 || renderer.Height != nativeHeight * 2) { return false; }
							for(int y = 14; y < 462; y++) {
								for(int x = 80; x < 192; x++) {
									if(renderer.Pixels[y * renderer.Width + x] != 0xFF0000FF) { return false; }
								}
							}
							return renderer.Pixels[64 * renderer.Width + 200] == 0xFF00FFFF;
						})) { flags |= 128; }
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
						if(string(filename).find("bglow") != string::npos) {
							background.Layer = 3;
							emu->GetVideoDecoder()->SetMetaReplacements({}, {background});
							if(!waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Height == nativeHeight * 2
								&& renderer.Pixels[(spriteY * 2) * renderer.Width + spriteX * 2] == nativeSpriteColor; })) { flags |= 128; }
							background.Layer = backgroundLayer;
							emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
						}
						if(!waitFor([&]() {
							if(renderer.Width != nativeWidth * 2 || renderer.Height != nativeHeight * 2) { return false; }
							for(int y = 0; y < 16; y++) {
								for(int x = 0; x < 16; x++) {
									uint32_t expected = y == 0 && (x == 0 || (holes && x == 14)) ? 0xFF00FFFF
										: y == 0 && (x == 1 || (holes && x == 15)) ? 0xFF00FF7F : 0xFF00FF00;
									if(renderer.Pixels[(spriteY * 2 + y) * renderer.Width + spriteX * 2 + x] != expected) { return false; }
								}
							}
							return true;
						})) { flags |= 128; }
						background.Artwork.assign(4, 0x80FFFFFF);
						if(string(filename).find("frame-timing") != string::npos
							&& !MeasureLiveFrameTiming(emu.get(), renderer, "SNES metasprite PNG and background PNG")) { flags |= 128; }
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
						if(!waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Height == nativeHeight * 2
							&& renderer.Pixels[(spriteY * 2 + 2) * renderer.Width + spriteX * 2 + 2] == 0xFF7FBF40; })) { flags |= 128; }
						background.Width = background.Height = 16; background.X = 3; background.Y = -2;
						background.Artwork.resize(256);
						auto texture = [](int x, int y) { return 0xFF000000u | ((uint32_t)x * 11 << 16) | ((uint32_t)y * 13 << 8) | ((x ^ y) * 7); };
						for(int y = 0; y < 16; y++) { for(int x = 0; x < 16; x++) { background.Artwork[y * 16 + x] = texture(x, y); } }
						int tx = (sampleX(spriteX + 1, spriteY + 1 - 6) - background.X + 8) % 8 * 2;
						int ty = (sampleY(spriteX + 1, spriteY + 1 - 6) - background.Y + 8) % 8 * 2;
						uint32_t sampled = texture(tx, ty);
						uint32_t expectedPattern = 0xFF000000 | (((sampled >> 16) & 255) / 2 << 16)
							| ((((sampled >> 8) & 255) + 255) / 2 << 8) | ((sampled & 255) / 2);
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
						if(!waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Height == nativeHeight * 2
							&& renderer.Pixels[(spriteY * 2 + 2) * renderer.Width + spriteX * 2 + 2] == expectedPattern; })) { flags |= 128; }
						emu->GetVideoDecoder()->SetMetaSpriteGroups({});
						background.MemoryConditions = {{UINT32_MAX, 0, 0}};
						emu->GetVideoDecoder()->SetMetaBackgroundLayers({background});
						if(!waitFor([&]() { return renderer.Width == nativeWidth && renderer.Height == nativeHeight
							&& renderer.Capture && renderer.Capture->MemorySnapshot.empty()
							&& renderer.Pixels[spriteY * nativeWidth + spriteX] == nativeSpriteColor; })) { flags |= 128; }
						emu->GetVideoDecoder()->SetMetaBackgroundLayers({});
						if(string(filename).find("viewer-core") != string::npos) {
							snes->GetPpu()->SetMetaSpriteCapture(false);
							emu->InitDebugger();
							bool counterChecks = VerifyMetaViewerCounters(emu.get());
							if(!counterChecks) { flags |= 128; }
							std::cout << "MEMORY COUNTERS: mixed access widths, timestamps, negative addresses and reset=" << counterChecks << std::endl;
							if(!waitFor([&]() { return !renderer.Capture && renderer.Width == nativeWidth && renderer.Height == nativeHeight; })
								|| !MeasureLiveFrameTiming(emu.get(), renderer, "SNES native with debugger initialized (viewer core only)")) { flags |= 128; }
							auto* viewerTools = emu->InternalGetDebugger()->GetPpuTools(CpuType::Snes);
							auto refreshListener = std::make_shared<MetaSpriteViewerRefreshListener>();
							emu->GetNotificationManager()->RegisterNotificationListener(refreshListener);
							viewerTools->SetViewerUpdateTiming(123, 224, 0);
							viewerTools->SetViewerUpdateTiming(456, 224, 16);
							uint32_t firstBefore = refreshListener->First.load(), secondBefore = refreshListener->Second.load();
							if(!MeasureLiveFrameTiming(emu.get(), renderer, "SNES native with two scheduled viewer refreshes (no desktop drawing)")) { flags |= 128; }
							uint32_t firstDelivered = refreshListener->First.load() - firstBefore, secondDelivered = refreshListener->Second.load() - secondBefore;
							if(firstDelivered < 118 || firstDelivered > 123 || secondDelivered < 118 || secondDelivered > 123 || refreshListener->Unexpected.load()) { flags |= 128; }
							viewerTools->RemoveViewer(123); viewerTools->RemoveViewer(456);
							firstBefore = refreshListener->First.load(); secondBefore = refreshListener->Second.load();
							snes->GetPpu()->SetMetaSpriteCapture(true);
							if(!waitFor([&]() { return renderer.Capture != nullptr; })
								|| !MeasureLiveFrameTiming(emu.get(), renderer, "SNES capture with debugger initialized (viewer core only)")) { flags |= 128; }
							bool removalStopped = refreshListener->First.load() == firstBefore && refreshListener->Second.load() == secondBefore;
							if(!removalStopped) { flags |= 128; }
							std::cout << "VIEWER REFRESH: " << firstDelivered << "/" << secondDelivered << " notifications during 120 frame intervals; removal stops delivery=" << removalStopped << std::endl;
							emu->StopDebugger();
						}
					}
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpriteSnesCaptureTest(const char* filename)
	{
		return RunMetaSpriteSnesCaptureTestImpl(filename,nullptr);
	}
	DllExport uint32_t __stdcall RunMetaSpriteSnesAuthoredPackTest(const char* filename, const uint8_t* packet, uint32_t length)
	{
		try {
			auto groups=MetaSpritePacket(packet,length).Parse();
			if(groups.size()!=1 || groups.front().ArtworkScale!=1 || groups.front().Members.size()!=2) { return 0; }
			return RunMetaSpriteSnesCaptureTestImpl(filename,&groups);
		} catch(...) { return 0; }
	}
	static uint32_t RunMetaAnimationStateImpl(Emulator* emu, const uint8_t* sprites, uint32_t spriteLength, const uint8_t* backgrounds, uint32_t backgroundLength)
	{
		uint32_t stage=1;
		try {
			auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode();
			std::stringstream original; emu->Serialize(original,false,0);
			auto groups=MetaSpritePacket(sprites,spriteLength).Parse();
			auto layers=MetaBackgroundPacket(backgrounds,backgroundLength).Parse();
			emu->GetVideoDecoder()->SetMetaReplacements(groups,layers);
			MetaSpriteFrame captured(0,0);
			auto phase=[&](uint32_t frame) {
				captured.FrameNumber=frame;
				emu->GetVideoDecoder()->CaptureMetaSpriteMemory(captured,[](uint32_t,uint8_t& value) { value=0; return true; });
				uint64_t largest=0;
				if(captured.AnimationTiming) { for(const auto& entry : captured.AnimationTiming->Elapsed) { largest=std::max(largest,entry.second); } }
				return largest;
			};
			auto check=[&](bool value) { if(!value) { throw std::runtime_error("Animation emulator state mismatch"); } };
			stage=2; check(phase(100)==0 && phase(107)==7);
			for(bool settings : {false,true}) {
				std::stringstream saved; emu->Serialize(saved,settings,0);
				check(phase(140)==40);
				saved.seekg(0); stage=3;
				check(emu->Deserialize(saved,SaveStateManager::FileFormatVersion,settings,std::nullopt,false)==DeserializeResult::Success);
				check(phase(107)==7); // Same-frame state load retains phase.
				emu->GetVideoDecoder()->ResetReplacementAnimation(); check(phase(1)==0);
				saved.clear(); saved.seekg(0); stage=4;
				check(emu->Deserialize(saved,SaveStateManager::FileFormatVersion,settings,std::nullopt,false)==DeserializeResult::Success);
				check(phase(107)==7 && phase(108)==8); // Forward load, then one normal tick.
				// Return to the same saved phase before the next settings variant.
				saved.clear(); saved.seekg(0); check(emu->Deserialize(saved,SaveStateManager::FileFormatVersion,settings,std::nullopt,false)==DeserializeResult::Success);
				check(phase(107)==7);
			}
            stage=7;
            std::deque<RewindData> history;
            RewindData full; full.SaveState(emu,history); history.push_back(full);
            check(phase(109)==9);
            RewindData delta; delta.SaveState(emu,history); history.push_back(delta);
            check(phase(150)==50);
            history[1].LoadState(emu,history,1,false); check(phase(109)==9);
            history[0].LoadState(emu,history,0,false); check(phase(107)==7);
			stage=5;
			Serializer legacy(SaveStateManager::FileFormatVersion,true); auto console=emu->GetConsole(); legacy.Stream(console,"");
			std::stringstream oldState; legacy.SaveTo(oldState,0); oldState.seekg(0);
			check(emu->Deserialize(oldState,SaveStateManager::FileFormatVersion,false,std::nullopt,false)==DeserializeResult::Success);
			check(phase(107)==0);
			stage=6; emu->GetVideoDecoder()->SetMetaReplacements(groups,layers);
			original.seekg(0); check(emu->Deserialize(original,SaveStateManager::FileFormatVersion,false,std::nullopt,false)==DeserializeResult::Success);
			std::cout << "PASS: actual emulator animation state serialization with/without settings, same-frame/forward loads, continued ticks, full/XOR rewind checkpoints and legacy state fallback." << std::endl;
			return 0;
		} catch(...) { return stage; }
	}
	DllExport uint32_t __stdcall RunMetaAnimationStateTest(const uint8_t* sprites, uint32_t spriteLength, const uint8_t* backgrounds, uint32_t backgroundLength)
	{
		return RunMetaAnimationStateImpl(_emu.get(),sprites,spriteLength,backgrounds,backgroundLength);
	}
	DllExport uint32_t __stdcall RunMetaAnimationStateRomTest(const char* filename, const uint8_t* sprites, uint32_t spriteLength, const uint8_t* backgrounds, uint32_t backgroundLength)
	{
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		MetaSpriteTestRenderer renderer;
		emu->GetSettings()->GetGameboyConfig().Model=string(filename).find(".gbc")!=string::npos ? GameboyModel::GameboyColor : GameboyModel::Gameboy;
		emu->GetSettings()->GetGbaConfig().SkipBootScreen=true;
		emu->GetSettings()->GetWsConfig().UseBootRom=false;
		uint32_t result=100;
		if(emu->LoadRom((VirtualFile)filename,VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			// Wait for startup before acquiring the emulator lock. Otherwise a very
			// fast state-only test can race Run() setting its initial stop flag.
			bool started=false;
			for(int i=0;i<500;i++) {
				{ std::lock_guard<std::mutex> lock(renderer.Mutex); started=renderer.FrameNumber>=2; }
				if(started) { break; }
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			result=started ? RunMetaAnimationStateImpl(emu.get(),sprites,spriteLength,backgrounds,backgroundLength) : 101;
			std::cout << "Animation state: stopping " << filename << std::endl;
			emu->Stop(false,true,false);
			emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); std::cout << "Animation state: released " << filename << std::endl; return result;
	}
	DllExport uint32_t __stdcall RunMetaBackgroundSnesAnimationTest(const char* filename, const uint8_t* packet, uint32_t length)
	{
		try {
			auto layers=MetaBackgroundPacket(packet,length).Parse();
			if(layers.size()!=1 || layers[0].Animation.Frames.size()!=2 || layers[0].CompositionPosition!=1 || (layers[0].FollowScroll && !layers[0].UseCpuCamera) || layers[0].Scale!=1) { return 0; }
			return RunMetaSpriteSnesCaptureTestImpl(filename,nullptr,&layers);
		} catch(...) { return 0; }
	}
	DllExport uint32_t __stdcall RunMetaTextSystemTest(const char* filename,const uint8_t* packet,uint32_t length)
	{
		unique_ptr<Emulator> emu(new Emulator()); uint32_t stage=1; MetaSpriteTestRenderer renderer;
		try {
			auto profiles=MetaTextPacket(packet,length).Parse(); if(profiles.size()!=1 || profiles[0].Glyphs.size()!=1) { return stage; }
			auto p=profiles[0]; p.Mappings.clear(); p.Patterns.clear(); p.PatternStarts.clear(); p.Mappings[MetaTextReplacement::Key(UINT32_MAX,-1)]=0;
			FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename)); emu->Initialize(false); emu->GetSettings()->SetFlag(EmulationFlags::TestMode); emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
			auto& gb=emu->GetSettings()->GetGameboyConfig(); gb.Model=string(filename).find(".gbc")!=string::npos ? GameboyModel::GameboyColor : GameboyModel::Gameboy; gb.BlendFrames=false; gb.GbcAdjustColors=false; gb.RamPowerOnState=RamState::AllZeros;
			auto& gba=emu->GetSettings()->GetGbaConfig(); gba.SkipBootScreen=true; gba.DisableFrameSkipping=true; gba.BlendFrames=false; gba.GbaAdjustColors=false; gba.RamPowerOnState=RamState::AllZeros;
			emu->GetSettings()->GetPcEngineConfig().DisableFrameSkipping=true; emu->GetSettings()->GetSmsConfig().RamPowerOnState=RamState::AllZeros;
			auto& snes=emu->GetSettings()->GetSnesConfig(); snes.DisableFrameSkipping=true; snes.ColorCorrection=SnesColorCorrectionMode::None; snes.RamPowerOnState=RamState::AllZeros; snes.Overscan={};
			auto& ws=emu->GetSettings()->GetWsConfig(); ws.UseBootRom=false; ws.LcdShowIcons=false; ws.BlendFrames=false; ws.LcdAdjustColors=false;
			if(!emu->LoadRom((VirtualFile)filename,VirtualFile())) { throw std::runtime_error("load"); }
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			auto console=emu->GetConsole();
			if(auto c=dynamic_cast<Gameboy*>(console.get())) { c->GetPpu()->SetMetaSpriteCapture(true); }
			if(auto c=dynamic_cast<GbaConsole*>(console.get())) { c->GetPpu()->SetMetaSpriteCapture(true); }
			if(auto c=dynamic_cast<PceConsole*>(console.get())) { c->GetVdc()->SetMetaSpriteCapture(true); }
			if(auto c=dynamic_cast<SmsConsole*>(console.get())) { c->GetVdp()->SetMetaSpriteCapture(true); }
			if(auto c=dynamic_cast<SnesConsole*>(console.get())) { c->GetPpu()->SetMetaSpriteCapture(true); }
			if(auto c=dynamic_cast<WsConsole*>(console.get())) { c->GetPpu()->SetMetaSpriteCapture(true); }
			auto write=[&](uint8_t value) { uint32_t addr=p.Conditions[0].Address;
				if(auto c=dynamic_cast<Gameboy*>(console.get())) { c->GetMemoryManager()->DebugWrite(addr,value); }
				if(auto c=dynamic_cast<GbaConsole*>(console.get())) { c->GetMemoryManager()->DebugWrite(addr,value); }
				if(auto c=dynamic_cast<PceConsole*>(console.get())) { c->GetMemoryManager()->DebugWrite(addr,value); }
				if(auto c=dynamic_cast<SmsConsole*>(console.get())) { c->GetMemoryManager()->DebugWrite(addr,value); }
				if(auto c=dynamic_cast<SnesConsole*>(console.get())) { c->GetMemoryManager()->GetMemoryMappings()->DebugWrite(addr,value); }
				if(auto c=dynamic_cast<WsConsole*>(console.get())) { c->GetMemoryManager()->DebugWrite(addr,value); }
			};
			{ auto lock=emu->AcquireLock(); write(0); emu->GetVideoDecoder()->SetAllMetaReplacements({},{},{p}); }
			auto wait=[&](const std::function<bool()>& check) { for(int n=0;n<500;n++) { { std::lock_guard<std::mutex> lock(renderer.Mutex); if(check()) { return true; } } std::this_thread::sleep_for(std::chrono::milliseconds(10)); } return false; };
			std::vector<uint32_t> baseline,stablePixels; uint32_t width=0,height=0,stableFrame=UINT32_MAX,inheritedInk=0xFFFFFFFF; int stableFrames=0,minimumSourceY=p.Y; stage=2;
			if(!wait([&]() {
				if(renderer.FrameNumber<12 || !renderer.Capture || renderer.Capture->TextLayers[p.Layer].empty()) { return false; }
				const auto& f=*renderer.Capture; const auto& plane=f.TextLayers[p.Layer];
				auto smsConsole=dynamic_cast<SmsConsole*>(console.get());
				bool legacyText=smsConsole && !smsConsole->GetVdp()->GetState().UseMode4 && smsConsole->GetVdp()->GetState().M1_Use224LineMode;
				if(f.Rows.empty() && f.DecodedRows.empty() && !legacyText) { return false; } // Text mode has no sprites.
				if(renderer.FrameNumber==stableFrame) { return false; } stableFrame=renderer.FrameNumber;
				if(renderer.Pixels!=stablePixels) { stablePixels=renderer.Pixels; stableFrames=0; return false; }
				if(++stableFrames<3) { return false; }
				int left=dynamic_cast<PceConsole*>(console.get()) ? PceConstants::GetLeftOverscan(4) : 0;
				for(uint32_t y=8;y+uint32_t(p.CellHeight)<f.Height;y++) { for(uint32_t x=uint32_t(left+8);x+uint32_t(p.CellWidth)<f.Width;x++) {
					const auto& t=plane[y*f.Width+x]; if(!t.Tile.Valid || t.Tile.TileWidth!=p.SourceTileWidth || t.Tile.LocalX || t.Tile.LocalY || t.Y<minimumSourceY) { continue; }
					std::vector<uint32_t> ids; bool valid=true;
					for(int dy=0;dy<p.CellHeight;dy+=8) { for(int dx=0;dx<p.CellWidth;dx+=p.SourceTileWidth) {
						const auto& part=plane[(y+dy)*f.Width+x+dx]; if(!part.Tile.Valid || part.X!=t.X+dx || part.Y!=t.Y+dy) { valid=false; }
						ids.push_back(p.MatchKind ? part.Tile.GraphicsAddress : part.Tile.TileIndex);
					} } if(!valid) { continue; }
					if(auto sms=dynamic_cast<SmsConsole*>(console.get()); sms && !sms->GetVdp()->GetState().UseMode4) {
						const auto& state=sms->GetVdp()->GetState(); const auto* vram=static_cast<const uint8_t*>(emu->GetMemory(MemoryType::SmsVideoRam).Memory);
						uint32_t row=t.Y/8,index=vram[state.NametableAddress+row*(legacyText ? 40 : 32)+t.X/(legacyText ? 6 : 8)],address;
						if(legacyText) { address=(state.BgPatternTableAddress&0x3800)+index*8; }
						else if(state.M3_Use240LineMode) { address=(state.BgPatternTableAddress&0x3800)+index*8+(row&3)*2; index=UINT32_MAX; }
						else if(state.M2_AllowHeightChange) { index+=(row&0x18)<<5; address=(state.BgPatternTableAddress&0x2000)+(index&(((state.BgPatternTableAddress>>3)|0xFF)&0x3FF))*8; }
						else { address=(state.BgPatternTableAddress&0x3800)+index*8; }
						if(t.Tile.TileIndex!=index || t.Tile.GraphicsAddress!=address) { throw std::runtime_error("legacy fetched identity differs from tilemap viewer"); }
						uint32_t colorAddress=state.M3_Use240LineMode ? address+((t.Y&7)>=4 ? 1 : 0)
							: state.M2_AllowHeightChange ? (state.ColorTableAddress&0x2000)|((index&(((state.ColorTableAddress>>3)|7)&0x3FF))<<3)+(t.Y&7)
							: (state.ColorTableAddress&0x3FC0)|((index>>3)&0x1F);
						uint8_t ink=legacyText ? state.TextColorIndex : vram[colorAddress]>>4; uint16_t rgb=sms->GetVdp()->GetSmsSgPalette()[ink ? ink : state.BackgroundColorIndex];
						if(t.Tile.InkColors[0]!=rgb) { throw std::runtime_error("legacy fetched foreground ink"); }
						auto expand=[](int c) { return (c<<3)|(c>>2); };
						inheritedInk=0xFF000000u|(expand(rgb&31)<<16)|(expand((rgb>>5)&31)<<8)|expand((rgb>>10)&31);
					}
					p.X=t.X; p.Y=t.Y; p.Width=p.CellWidth; p.Height=p.CellHeight; p.UseRegion=true; p.CellOriginX=t.X%p.CellWidth; p.CellOriginY=t.Y%p.CellHeight;
					p.Mappings.clear(); if(ids.size()==1) { p.Mappings[MetaTextReplacement::Key(ids[0],-1)]=0; }
					else { p.Patterns={ {ids,-1,0} }; p.PatternStarts[MetaTextReplacement::Key(ids[0],-1)]={0}; }
					baseline=renderer.Pixels; width=renderer.Width; height=renderer.Height; return true;
				} } return false;
			})) { throw std::runtime_error("capture"); }
			{ auto lock=emu->AcquireLock(); write(0); emu->GetVideoDecoder()->SetAllMetaReplacements({},{},{p}); }
			auto blend=[](uint32_t front,uint32_t back) { uint32_t a=front>>24,c=0xFF000000; if(!a) { return back; } if(a==255) { return front; } for(int ch=0;ch<3;ch++) { c|=((((front>>(8*ch))&255)*a+((back>>(8*ch))&255)*(255-a)+127)/255)<<(8*ch); } return c; };
			int scale=p.Scale;
			int displayLeft=p.DisplayX-(dynamic_cast<PceConsole*>(console.get()) ? PceConstants::GetLeftOverscan(4) : 0);
			std::vector<uint32_t> expected(size_t(width)*height*scale*scale);
			for(uint32_t y=0;y<height*scale;y++) { for(uint32_t x=0;x<width*scale;x++) { expected[size_t(y)*width*scale+x]=baseline[size_t(y/scale)*width+x/scale]; } }
			if(p.FillEnabled) { for(int y=p.DisplayY*scale;y<(p.DisplayY+p.DisplayHeight)*scale;y++) { for(int x=std::max(0,displayLeft*scale);x<(displayLeft+p.DisplayWidth)*scale;x++) { auto& c=expected[size_t(y)*width*scale+x]; c=blend(p.FillColor,c); } } }
			const auto& g=p.Glyphs[0]; for(int y=0;y<g.Height;y++) { for(int x=0;x<g.Width;x++) {
				int dx=displayLeft*scale+g.OffsetX+x,dy=p.DisplayY*scale+g.OffsetY+y;
				if(dx<std::max(0,displayLeft*scale) || dx>=(displayLeft+p.DisplayWidth)*scale || dy<p.DisplayY*scale || dy>=(p.DisplayY+p.DisplayHeight)*scale) { continue; }
				size_t id=size_t(y)*g.Width+x; auto& color=expected[size_t(dy)*width*scale+dx]; if(!g.Decorations.empty()) { color=blend(g.Decorations[id],color); }
				uint32_t tint=p.PaletteColor ? inheritedInk : p.Color;
				uint32_t a=((g.Pixels[id]>>24)*(tint>>24)+127)/255; color=blend((tint&0xFFFFFF)|(a<<24),color);
			} }
			stage=3; if(!wait([&]() { return renderer.Width==width*scale && renderer.Height==height*scale && renderer.Pixels==expected; })) {
				std::lock_guard<std::mutex> lock(renderer.Mutex); std::cerr<<"TEXT dims "<<renderer.Width<<"x"<<renderer.Height<<" expected "<<width*scale<<"x"<<height*scale<<" region "<<p.X<<","<<p.Y<<std::endl;
				if(renderer.Pixels.size()==expected.size()) { for(size_t i=0;i<expected.size();i++) { if(renderer.Pixels[i]!=expected[i]) { std::cerr<<"TEXT first difference "<<i<<" got "<<std::hex<<renderer.Pixels[i]<<" expected "<<expected[i]<<std::dec<<std::endl; break; } } }
				throw std::runtime_error("render");
			}
			{ auto lock=emu->AcquireLock(); auto decoder=emu->GetVideoDecoder(); decoder->WaitForAsyncFrameDecode(); decoder->RequestTextPreview(); FrameInfo size;
				uint32_t count=decoder->ReadTextPreview(nullptr,0,size); std::vector<uint32_t> snapshot(count); decoder->ReadTextPreview(snapshot.data(),count,size);
				uint32_t left=dynamic_cast<PceConsole*>(console.get()) ? PceConstants::GetLeftOverscan(4) : 0;
				if(size.Width<width+left || size.Height!=height) { throw std::runtime_error("snapshot dimensions"); }
				for(uint32_t y=0;y<height;y++) { for(uint32_t x=0;x<width;x++) { if(snapshot[size_t(y)*size.Width+x+left]!=(expected[size_t(y*scale)*width*scale+x*scale]|0xFF000000)) { throw std::runtime_error("snapshot crop"); } } }
			}
			std::stringstream activeState,inactiveState;
			{ auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode(); emu->GetSaveStateManager()->SaveState(activeState); }
			{ auto lock=emu->AcquireLock(); write(1); } stage=4; if(!wait([&]() { return renderer.Width==width && renderer.Pixels==baseline; })) { throw std::runtime_error("condition off"); }
			{ auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode(); emu->GetSaveStateManager()->SaveState(inactiveState); }
			{ auto lock=emu->AcquireLock(); write(0); } stage=5; if(!wait([&]() { return renderer.Width==width*scale && renderer.Pixels==expected; })) { throw std::runtime_error("condition on"); }
			// Restore both RAM-trigger states without reapplying the pack or rewriting RAM.
			// Inspect the first captured gameplay frame, so a later good frame cannot hide stale text.
			auto restore=[&](std::stringstream& saved,const std::vector<uint32_t>& pixels) {
				{ auto lock=emu->AcquireLock(); auto decoder=emu->GetVideoDecoder(); decoder->WaitForAsyncFrameDecode();
					saved.clear(); saved.seekg(0); if(!emu->GetSaveStateManager()->LoadState(saved)) { throw std::runtime_error("load text state"); }
					decoder->WaitForAsyncFrameDecode(); std::lock_guard<std::mutex> renderLock(renderer.Mutex);
					renderer.FirstCapturePixels.clear(); renderer.RecordNextCapture=true;
				}
				return wait([&]() { return !renderer.FirstCapturePixels.empty() && renderer.FirstCapturePixels==pixels; });
			};
			stage=7; if(!restore(inactiveState,baseline)) { throw std::runtime_error("first inactive restored frame"); }
			stage=8; if(!restore(activeState,expected)) { throw std::runtime_error("first active restored frame"); }
			// Change actual source identities while the RAM condition remains enabled.
			// These generated fixtures use regular tilemaps; preserve palette/flip/priority bits.
			{ auto lock=emu->AcquireLock();
				MemoryType type=MemoryType::WsWorkRam; uint32_t start=0,count=0x800,step=2;
				if(dynamic_cast<Gameboy*>(console.get())) { type=MemoryType::GbVideoRam; start=0x1800; count=0x800; step=1; }
				if(auto c=dynamic_cast<GbaConsole*>(console.get())) { type=MemoryType::GbaVideoRam; start=c->GetPpu()->GetState().BgLayers[p.Layer].TilemapAddr; }
				if(auto c=dynamic_cast<PceConsole*>(console.get())) { type=MemoryType::PceVideoRam; const auto& state=c->GetVdc()->GetState(); count=uint32_t(state.HvLatch.ColumnCount)*state.HvLatch.RowCount*2; }
				if(auto c=dynamic_cast<SmsConsole*>(console.get())) { type=MemoryType::SmsVideoRam; const auto& state=c->GetVdp()->GetState();
					start=state.UseMode4 ? state.EffectiveNametableAddress : state.NametableAddress; count=state.UseMode4 ? 32*28*2 : state.M1_Use224LineMode ? 40*24 : 32*24; step=state.UseMode4 ? 2 : 1; }
				if(auto c=dynamic_cast<SnesConsole*>(console.get())) { type=MemoryType::SnesVideoRam; start=c->GetPpu()->GetState().Layers[p.Layer].TilemapAddress*2; }
				if(auto c=dynamic_cast<WsConsole*>(console.get())) { start=c->GetPpu()->GetState().BgLayers[p.Layer].MapAddress&0x7FFF; }
				auto memory=emu->GetMemory(type); if(!memory.Memory || !count || start+count>memory.Size) { throw std::runtime_error("tilemap mutation bounds"); }
				auto bytes=static_cast<uint8_t*>(memory.Memory); for(uint32_t i=start;i<start+count;i+=step) {
					// WonderSwan maps and CPU RAM share storage in these fixtures.
					if(type==MemoryType::WsWorkRam && i==p.Conditions[0].Address) { continue; }
					bytes[i]^=0x3F;
				}
			}
			stage=9; std::vector<uint32_t> changedPixels; stableFrame=UINT32_MAX; stableFrames=0;
			if(!wait([&]() {
				if(renderer.Width!=width || renderer.FrameNumber==stableFrame || !renderer.Capture
					|| !renderer.Capture->MemorySnapshot.count(p.Conditions[0].Address) || renderer.Capture->MemorySnapshot.at(p.Conditions[0].Address)!=0) { return false; }
				stableFrame=renderer.FrameNumber;
				if(renderer.Pixels!=changedPixels) { changedPixels=renderer.Pixels; stableFrames=0; return false; }
				return ++stableFrames>=3;
			})) { throw std::runtime_error("changed tilemap must remove overlay while RAM remains enabled"); }
			std::stringstream changedState;
			{ auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode(); emu->GetSaveStateManager()->SaveState(changedState); }
			stage=10; if(!restore(activeState,expected)) { throw std::runtime_error("original tilemap restored frame"); }
			stage=11; if(!restore(changedState,changedPixels)) { throw std::runtime_error("changed tilemap restored frame"); }
			stage=12; if(!restore(activeState,expected)) { throw std::runtime_error("original tilemap restored again"); }
			{ auto lock=emu->AcquireLock(); emu->GetVideoDecoder()->SetAllMetaReplacements({},{},{}); } stage=6;
			if(!wait([&]() { return renderer.Width==width && renderer.Pixels==baseline && renderer.Capture && renderer.Capture->TextLayerMask==0; })) { throw std::runtime_error("clear"); }
			emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer); emu->Stop(false); emu->Release(); return 0;
		} catch(const std::exception& e) { std::cerr<<"TEXT SYSTEM stage "<<stage<<": "<<e.what()<<std::endl; emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer); emu->Stop(false); emu->Release(); return stage; }
	}
	DllExport uint32_t __stdcall RunMetaTextNesTest(const char* filename,const uint8_t* packet,uint32_t length)
	{
		try {
			int scrollX=std::string(filename).find("-scroll")!=std::string::npos ? 5 : 0,scrollY=scrollX ? 3 : 0;
			auto profiles=MetaTextPacket(packet,length).Parse();
			if(profiles.size()!=1 || profiles[0].Glyphs.size()!=3 || profiles[0].Mappings.size()+profiles[0].Patterns.size()!=3 || profiles[0].Conditions.size()!=2) { return 0; }
			FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
			unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
			emu->GetSettings()->SetFlag(EmulationFlags::TestMode); emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
			auto& cfg=emu->GetSettings()->GetNesConfig(); cfg.IsFullColorPalette=true; cfg.RamPowerOnState=RamState::AllZeros;
			for(int i=0;i<512;i++) { cfg.UserPalette[i]=(i&63)==1 ? 0xFFFF0000 : (i&63)==2 ? 0xFF0000FF : (i&63)==3 ? 0xFFFF00FF : (i&63)==4 ? 0xFFFFFF00 : 0xFF000000; }
			MetaSpriteTestRenderer renderer; uint32_t flags=0;
			if(!emu->LoadRom((VirtualFile)filename,VirtualFile())) { return 0; }
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			auto nes=dynamic_cast<NesConsole*>(emu->GetConsole().get());
			nes->GetPpu()->SetMetaSpriteCapture(true);
			auto wait=[&](const std::function<bool()>& check) {
				for(int attempt=0;attempt<1000;attempt++) { { std::lock_guard<std::mutex> lock(renderer.Mutex); if(check()) { return true; } } std::this_thread::sleep_for(std::chrono::milliseconds(10)); } return false;
			};
			std::vector<uint32_t> baseline;
			if(wait([&]() { if(renderer.Width!=256 || renderer.Height!=240 || !renderer.Capture || renderer.FrameNumber<6 || renderer.Pixels[(32-scrollY)*256+32-scrollX]!=0xFFFF0000 || renderer.Pixels[32*256+40]!=0xFFFF00FF) { return false; } baseline=renderer.Pixels; return true; })) { flags|=1; }
			const auto& p=profiles[0]; int scale=p.Scale;
			auto blend=[](uint32_t ink,uint32_t under,int alpha) { uint32_t color=0xFF000000; for(int c=0;c<3;c++) { color|=((((ink>>(c*8))&255)*alpha+((under>>(c*8))&255)*(255-alpha)+127)/255)<<(c*8); } return color; };
			std::vector<uint32_t> runExpected;
			if(flags==1 && p.Unclipped) {
				int width=256*scale,height=240*scale;
				runExpected.resize(size_t(width)*height); for(int y=0;y<height;y++) { for(int x=0;x<width;x++) { runExpected[y*width+x]=baseline[(y/scale)*256+x/scale]; } }
				int left=std::max(0,p.DisplayX*scale),top=std::max(0,p.DisplayY*scale),right=std::min(width,(p.DisplayX+p.DisplayWidth)*scale),bottom=std::min(height,(p.DisplayY+p.DisplayHeight)*scale);
				if(p.FillEnabled) { for(int y=top;y<bottom;y++) { for(int x=left;x<right;x++) { runExpected[y*width+x]=blend(p.FillColor,runExpected[y*width+x],p.FillColor>>24); } } }
				auto drawGlyph=[&](uint32_t id,int64_t cursor,int64_t line,uint32_t ink) {
                    const auto& glyph=p.Glyphs[id]; double originX=double(cursor)*scale/64+glyph.OffsetX,originY=double(line)*scale/64+glyph.OffsetY;
                    for(int y=top;y<bottom;y++) { int sy=int(std::floor(y+0.5-originY)); if(sy<0 || sy>=glyph.Height) { continue; }
                        for(int x=left;x<right;x++) { int sx=int(std::floor(x+0.5-originX)); if(sx<0 || sx>=glyph.Width) { continue; }
                            size_t index=size_t(sy)*glyph.Width+sx; auto& color=runExpected[y*width+x];
                            if(!glyph.Decorations.empty()) { color=blend(glyph.Decorations[index],color,glyph.Decorations[index]>>24); }
                            color=blend(ink,color,((glyph.Pixels[index]>>24)*(ink>>24)+127)/255);
                        }
                    }
                };
                if(p.WrapWords) {
                    // Independent oracle for this generated source: "AV V" then "VAV ".
                    // The display fits AV/VA, but neither a trailing space nor VAV.
                    auto kern=[&](uint32_t a,uint32_t b) { auto it=p.Kerning.find((uint64_t(a)<<32)|b); return it==p.Kerning.end() ? 0 : it->second; };
                    int64_t x=int64_t(p.DisplayX)*64,y=int64_t(p.DisplayY)*64;
                    int64_t av=p.Glyphs[0].Advance64+p.LetterSpacing64+kern(0,1)+p.Glyphs[1].Advance64;
                    int64_t va=p.Glyphs[1].Advance64+p.LetterSpacing64+kern(1,0)+p.Glyphs[0].Advance64;
                    if(!p.WrapText || !p.Glyphs[2].WordSeparator || av>p.DisplayWidth*64 || va>p.DisplayWidth*64 || av+p.Glyphs[2].Advance64<=p.DisplayWidth*64) { throw std::runtime_error("Word-wrap fixture dimensions"); }
                    drawGlyph(0,x,y,p.Color); drawGlyph(1,x+p.Glyphs[0].Advance64+p.LetterSpacing64+kern(0,1),y,p.Color);
                    drawGlyph(1,x,y+p.LineHeight64,p.Color);
                    drawGlyph(1,x,y+2*p.LineHeight64,p.Color); drawGlyph(0,x+p.Glyphs[1].Advance64+p.LetterSpacing64+kern(1,0),y+2*p.LineHeight64,p.Color);
                    drawGlyph(1,x,y+3*p.LineHeight64,p.Color);
                } else for(int row=0;row<2;row+=p.CellHeight/8) {
					int64_t cursor=int64_t(p.DisplayX)*64; uint32_t previous=UINT32_MAX;
					for(int col=0;col<4;col+=p.CellWidth/8) {
						bool b=(col+row)%2!=0; uint32_t id=b ? 1 : col>=2 ? 2 : 0;
						if(previous!=UINT32_MAX) { auto pair=p.Kerning.find((uint64_t(previous)<<32)|id); if(pair!=p.Kerning.end()) { cursor+=pair->second; } }
						drawGlyph(id,cursor,int64_t(p.DisplayY)*64+(row/(p.CellHeight/8))*p.LineHeight64,p.PaletteColor ? col>=2 ? 0xFFFFFF00 : 0xFFFF0000 : p.Color);
                        const auto& glyph=p.Glyphs[id]; cursor+=glyph.Advance64+p.LetterSpacing64; previous=id;
					}
				}
			}
			auto expected=[&]() {
				if(renderer.Width!=uint32_t(256*scale) || renderer.Height!=uint32_t(240*scale) || !renderer.Capture || renderer.Capture->TextPixels.size()!=256*240
					|| renderer.Capture->MemorySnapshot.size()!=2 || renderer.Capture->MemorySnapshot.at(0x652)!=8 || renderer.Capture->MemorySnapshot.at(0x760)!=4) { return false; }
				if(p.Unclipped) { return renderer.Pixels==runExpected; }
				for(int y=0;y<240*scale;y++) { for(int x=0;x<256*scale;x++) {
					int px=x/scale,py=y/scale,wx=(px+scrollX)%512,wy=(py+scrollY)%480; uint32_t color=baseline[py*256+px];
					if(wx>=32 && wx<64 && wy>=32 && wy<48) {
						int col=(wx-32)/8,row=(wy-32)/8; bool b=(col+row)%2!=0; int glyph=b ? 1 : col>=2 ? 2 : 0;
						uint32_t raw=p.Glyphs[glyph].Pixels[((wy%8)*scale+y%scale)*8*scale+(wx%8)*scale+x%scale];
						uint32_t ink=p.PaletteColor ? col>=2 ? 0xFFFFFF00 : 0xFFFF0000 : p.Color;
						int alpha=int(((raw>>24)*(ink>>24)+127)/255);
						if(p.FillEnabled) { color=blend(p.FillColor,color,p.FillColor>>24); }
						color=blend(ink,color,alpha);
						const auto& tile=renderer.Capture->TextPixels[py*256+px];
						if(!tile.Valid || tile.TileIndex!=uint32_t(b ? 2 : 1) || tile.GraphicsAddress!=uint32_t(b ? 32 : 16) || tile.LocalX!=(wx%8) || tile.LocalY!=(wy%8) || tile.Palette!=(col>=2 ? 1 : 0)) { return false; }
					}
					if(renderer.Pixels[y*256*scale+x]!=color) { return false; }
				} } return true;
			};
			if(flags==1) {
				std::stringstream activeState,inactiveState;
				{ auto emuLock=emu->AcquireLock(); emu->GetVideoDecoder()->SetAllMetaReplacements({},{},profiles); }
				if(wait(expected)) { flags|=2; }
				{ auto emuLock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode(); emu->GetSaveStateManager()->SaveState(activeState); }
				{ auto emuLock=emu->AcquireLock(); nes->GetMemoryManager()->DebugWrite(0x760,6); }
				if(wait([&]() { return renderer.Width==256 && renderer.Pixels==baseline && renderer.Capture && renderer.Capture->MemorySnapshot.count(0x760) && renderer.Capture->MemorySnapshot.at(0x760)==6; })) { flags|=4; }
				{ auto emuLock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode(); emu->GetSaveStateManager()->SaveState(inactiveState); }
				{ auto emuLock=emu->AcquireLock(); nes->GetMemoryManager()->DebugWrite(0x760,4); }
				if(wait(expected)) { flags|=8; }
				if(flags==15) {
					std::vector<uint32_t> activePixels;
					{ std::lock_guard<std::mutex> lock(renderer.Mutex); activePixels=renderer.Pixels; }
					auto restore=[&](std::stringstream& saved,const std::vector<uint32_t>& pixels) {
						{ auto emuLock=emu->AcquireLock(); auto decoder=emu->GetVideoDecoder(); decoder->WaitForAsyncFrameDecode();
							saved.clear(); saved.seekg(0); if(!emu->GetSaveStateManager()->LoadState(saved)) { return false; }
							decoder->WaitForAsyncFrameDecode(); std::lock_guard<std::mutex> lock(renderer.Mutex);
							renderer.FirstCapturePixels.clear(); renderer.RecordNextCapture=true;
						}
						return wait([&]() { return !renderer.FirstCapturePixels.empty() && renderer.FirstCapturePixels==pixels; });
					};
					if(!restore(inactiveState,baseline) || !restore(activeState,activePixels)) {
						std::cerr<<"NES text first frame after state restore mismatch"<<std::endl; flags|=32;
					}
					if(flags==15) {
						{ auto emuLock=emu->AcquireLock(); auto memory=emu->GetMemory(MemoryType::NesNametableRam);
							if(!memory.Memory || memory.Size<0x800) { throw std::runtime_error("NES nametable bounds"); }
							auto bytes=static_cast<uint8_t*>(memory.Memory);
							for(uint32_t i=0;i<memory.Size;i++) { if((i&0x3FF)<0x3C0) { bytes[i]^=0x3F; } }
						}
						std::vector<uint32_t> changedPixels; uint32_t lastFrame=UINT32_MAX; int stable=0;
						if(!wait([&]() {
							if(renderer.Width!=256 || renderer.FrameNumber==lastFrame || !renderer.Capture
								|| !renderer.Capture->MemorySnapshot.count(0x760) || renderer.Capture->MemorySnapshot.at(0x760)!=4) { return false; }
							lastFrame=renderer.FrameNumber;
							if(renderer.Pixels!=changedPixels) { changedPixels=renderer.Pixels; stable=0; return false; }
							return ++stable>=3;
						})) { throw std::runtime_error("NES changed tilemap still matches text"); }
						std::stringstream changedState;
						{ auto emuLock=emu->AcquireLock(); emu->GetVideoDecoder()->WaitForAsyncFrameDecode(); emu->GetSaveStateManager()->SaveState(changedState); }
						if(!restore(activeState,activePixels) || !restore(changedState,changedPixels) || !restore(activeState,activePixels)) {
							std::cerr<<"NES text first frame after tilemap-state restore mismatch"<<std::endl; flags|=32;
						}
					}
				}
				{ auto emuLock=emu->AcquireLock(); emu->GetVideoDecoder()->SetAllMetaReplacements({},{},{}); }
				if(wait([&]() { return renderer.Width==256 && renderer.Pixels==baseline && renderer.Capture && renderer.Capture->MemorySnapshot.empty() && renderer.Capture->TextPixels.empty(); })) { flags|=16; }
				if(flags==31) {
					// Authoring snapshot must remain in gameplay coordinates even when presentation rotates/widens.
					{ auto emuLock=emu->AcquireLock(); auto& video=emu->GetSettings()->GetVideoConfig(); video.DecorativeWidescreen=true; video.ScreenRotation=90; }
					if(!wait([&]() { return renderer.Width==240 && renderer.Height>256; })) { return 0; }
					auto emuLock=emu->AcquireLock(); auto decoder=emu->GetVideoDecoder(); decoder->WaitForAsyncFrameDecode(); decoder->RequestTextPreview();
					FrameInfo size; uint32_t count=decoder->ReadTextPreview(nullptr,0,size); std::vector<uint32_t> preview(count);
					decoder->ReadTextPreview(preview.data(),count,size);
					if(size.Width!=256 || size.Height!=240 || preview!=baseline) { return 0; }
				}
			}
			emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer); emu->Stop(false); emu->Release();
			return flags;
		} catch(...) { return 0; }
	}
	DllExport uint32_t __stdcall RunMetaSpriteNesCaptureTest(const char* filename)
	{
		bool rasterFixture = string(filename).find("sprite0-raster") != string::npos;
		bool backgroundFixture = string(filename).find("background") != string::npos;
		bool scrollFixture = string(filename).find("scroll") != string::npos;
		bool maskFixture = string(filename).find("mask") != string::npos;
		bool cropFixture = string(filename).find("crop") != string::npos;
		bool holeFixture = string(filename).find("holes") != string::npos;
		bool transparentFixture = string(filename).find("transparent") != string::npos;
		int cropLeft = cropFixture ? 2 : 0, cropTop = cropFixture ? 3 : 0;
		int nativeWidth = cropFixture ? 250 : 256, nativeHeight = cropFixture ? 232 : 240;
		int spriteLeft = (8 - cropLeft) * 2, spriteTop = (8 - cropTop) * 2;
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg = emu->GetSettings()->GetNesConfig(); cfg.IsFullColorPalette = true;
		// These generated ROMs expect empty nametables. The native default is
		// random RAM, which can occasionally select the nonempty sprite tile.
		cfg.RamPowerOnState = RamState::AllZeros;
		if(cropFixture) { cfg.NtscOverscan = {2, 4, 3, 5}; }
		MetaSpriteGraphic source;
		for(int i = 0; i < 512; i++) { cfg.UserPalette[i] = (i & 63) == 1 ? 0xFFFF0000 : 0xFF0000FF; }
		MetaSpriteTestRenderer renderer; uint32_t flags = 0;
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			if(auto nes = dynamic_cast<NesConsole*>(emu->GetConsole().get())) {
				nes->GetPpu()->SetMetaSpriteCapture(true);
				for(int attempt = 0; attempt < 500; attempt++) {
					{
						std::lock_guard<std::mutex> lock(renderer.Mutex);
						if(renderer.Capture) {
							for(const auto& graphic : MetaSpriteCompositor::BuildNesCandidates(*renderer.Capture, [&](uint16_t index) { return cfg.UserPalette[index & 511]; })) {
								if(graphic.HardwareSprite != 0 || graphic.X != 8 || graphic.Y != 8 || graphic.Height != 8
									) { continue; }
								bool validGraphic = true;
								for(size_t i = 0; i < graphic.Pixels.size(); i++) { validGraphic &= graphic.Pixels[i] == (transparentFixture || (holeFixture && i % 8 == 7) ? 0 : 0xFFFF0000u); }
								if(!validGraphic) { continue; }
								const auto& pixel = renderer.Capture->Pixels[8 * 256 + 8];
								bool validBackground = true;
								if(string(filename).find("background") != string::npos) {
									for(int y = 0; y < 240; y++) { for(int x = 0; x < 256; x++) {
										const auto& sample = renderer.Capture->Pixels[y * 256 + x].BackgroundSample;
										if(maskFixture && x < 8) { validBackground &= !sample.Valid; continue; }
										validBackground &= sample.Valid && sample.Layer == 0 && sample.X == ((x + (scrollFixture ? 509 : 0)) % 512) && sample.Y == ((y + (scrollFixture ? 477 : 0)) % 480);
									} }
								}
								bool validSource = transparentFixture ? pixel.VisibleSprite == -1 && (pixel.ReplacementEligibleSprites & 1)
									: pixel.VisibleSprite == 0 && pixel.SpriteColor == 1;
								if(validBackground && validSource && pixel.BackgroundColor == 0) { flags = 1; source = graphic; }
							}
						}
					}
					if(flags) { break; }
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
				if(!flags) { std::cerr << "NES capture failed" << std::endl; }
				if(flags) {
					MetaSpriteReplacement group;
					group.Id = 1; group.ArtworkScale = 2; group.ArtworkWidth = group.ArtworkHeight = 16;
					group.Artwork.assign(256, 0xFF00FF00); group.Artwork[0] = 0;
					source.X = source.Y = 0; group.Members = {source};
					auto waitFor = [&](std::function<bool()> condition, uint32_t frameBudget = 0) {
						uint32_t firstFrame; { std::lock_guard<std::mutex> lock(renderer.Mutex); firstFrame = renderer.FrameNumber; }
						// Smooth movement requires emulated frames, independent of host
						// Debug throughput. Keep a frame limit plus a bounded watchdog.
						for(int attempt = 0; attempt < (frameBudget ? 6000 : 500); attempt++) {
							{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } if(frameBudget && renderer.FrameNumber - firstFrame > frameBudget) { return false; } }
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						}
						return false;
					};
					MetaBackgroundReplacement background;
					background.Id = 1; background.Width = background.Height = 2; background.Scale = 2;
					background.WrapX = background.WrapY = true; background.Artwork.assign(4, 0xFFFFFFFF);
					auto texture = [](int x, int y) { return 0xFF000000u | ((uint32_t)x * 7 << 16) | ((uint32_t)y * 5 << 8) | ((x ^ y) * 3); };
					if(scrollFixture) {
						background.Width = background.Height = 32; background.Artwork.resize(1024);
						for(int y = 0; y < 32; y++) { for(int x = 0; x < 32; x++) { background.Artwork[y * 32 + x] = texture(x, y); } }
					}
					auto expectedBackground = [&](int x, int y) {
						if(maskFixture && x < 16) { return 0xFF0000FFu; }
						if(!backgroundFixture) { return 0xFF0000FFu; }
						if(!scrollFixture) { return 0xFFFFFFFFu; }
						return texture((((x / 2 + 509) % 512) % 16) * 2 + x % 2, (((y / 2 + 477) % 480) % 16) * 2 + y % 2);
					};
					if(backgroundFixture) { group.Artwork[1] = 0x8000FF00; }
					group.MemoryConditions = {{0, 0, 0}}; emu->GetVideoDecoder()->SetMetaReplacements({group}, backgroundFixture ? vector<MetaBackgroundReplacement>{background} : vector<MetaBackgroundReplacement>{});
					if(waitFor([&]() {
						if(renderer.Width != nativeWidth * 2 || renderer.Height != nativeHeight * 2) { return false; }
						for(int y = 0; y < 16; y++) {
							for(int x = 0; x < 16; x++) {
								uint32_t expected = x == 0 && y == 0 ? expectedBackground(x + 16, y + 16) : 0xFF00FF00;
								if(backgroundFixture && x == 1 && y == 0) {
									expected = 0xFF000000; uint32_t bg = expectedBackground(x + 16, y + 16);
									for(int shift : {0, 8, 16}) { expected |= (((shift == 8 ? 255 : 0) * 128 + ((bg >> shift) & 255) * 127 + 127) / 255) << shift; }
								}
								if(renderer.Pixels[(y + spriteTop) * nativeWidth * 2 + x + spriteLeft] != expected) { return false; }
							}
						}
						if(backgroundFixture) {
							for(int y = 0; y < nativeHeight * 2; y++) { for(int x = 0; x < nativeWidth * 2; x++) {
								if(x >= spriteLeft && x < spriteLeft + 16 && y >= spriteTop && y < spriteTop + 16) { continue; }
								if(renderer.Pixels[y * nativeWidth * 2 + x] != expectedBackground(x + cropLeft * 2, y + cropTop * 2)) { return false; }
							} }
						}
						return renderer.Pixels[0] == expectedBackground(cropLeft * 2, cropTop * 2);
					})) { flags |= 2; }
					if(rasterFixture && (flags & 2)) {
						MetaBackgroundReplacement raster;
						raster.Id = 1; raster.Width = raster.Height = 64; raster.Scale = 2; raster.WrapX = raster.WrapY = true;
						raster.MemoryConditions = {{0, 0, 0}}; raster.Artwork.resize(4096);
						for(int y = 0; y < 64; y++) { for(int x = 0; x < 64; x++) { raster.Artwork[y * 64 + x] = 0xFF000000 | (x << 16) | (y << 8); } }
						for(int mode = 0; mode < 6; mode++) {
							raster.IgnoreRasterScroll = mode != 0; raster.ScrollReferenceScanline = mode == 2 ? 0 : 32;
							raster.ScrollSpeedX = mode == 3 ? 500 : 1000;
							raster.UseCpuCamera = mode >= 4; raster.CameraX = mode == 5 ? MetaCameraAxis{4, 0x62, 0x63, 256, true} : MetaCameraAxis{4, 0x60, 0x61, 256};
							emu->GetVideoDecoder()->SetMetaReplacements({group}, {raster});
							if(!waitFor([&]() {
								if(renderer.Width != 512 || renderer.Height != 480 || !renderer.Capture) { return false; }
								const auto& top = renderer.Capture->Pixels[0].BackgroundSample;
								const auto& lower = renderer.Capture->Pixels[32 * 256].BackgroundSample;
								if(!top.Valid || !lower.Valid || !top.NesRasterScroll || top.ScrollX != 0 || lower.ScrollX != 24) { return false; }
								for(int y = 0; y < 480; y++) { for(int x = 0; x < 512; x++) {
									if(x >= 16 && x < 32 && y >= 16 && y < 32) { continue; }
									const auto& sample = renderer.Capture->Pixels[(y / 2) * 256 + x / 2].BackgroundSample;
									if(!sample.Valid) { return false; }
									int scrollX = mode == 0 ? sample.ScrollX : mode == 2 ? top.ScrollX : lower.ScrollX;
									int scrollY = mode == 0 ? sample.ScrollY : mode == 2 ? top.ScrollY : lower.ScrollY;
									if(mode == 3) { scrollX /= 2; }
									if(mode == 4) { scrollX = 781; scrollY = 0; }
									if(mode == 5) { scrollX = -10; scrollY = 0; }
									int ax = ((x / 2 + scrollX) % 32 + 32) % 32 * 2 + x % 2, ay = ((y / 2 + scrollY) % 32 + 32) % 32 * 2 + y % 2;
									if(renderer.Pixels[y * 512 + x] != (0xFF000000u | (uint32_t)(ax << 16) | (uint32_t)(ay << 8))) { return false; }
								} }
								return renderer.Capture->MemorySnapshot.size() == (mode >= 4 ? 3 : 1) && renderer.Capture->MemorySnapshot.count(0)
									&& (mode < 4 || (renderer.Capture->MemorySnapshot.count(raster.CameraX.Address) && renderer.Capture->MemorySnapshot.count(raster.CameraX.PageAddress)));
							})) { flags |= 128; }
						}
						if(!(flags & 128)) { std::cout << "PASS: NES NROM sprite-0-hit split remains native; PNG uses gameplay/HUD reference, half-rate tracking or CPU pixel/page camera beyond nametable wrapping." << std::endl; }
						// Verify the actual decoder/renderer canvas, including native-pixel
						// preservation, CPU-camera scenery and switching back to native size.
						vector<uint32_t> nativeViewport;
						{ std::lock_guard<std::mutex> lock(renderer.Mutex); nativeViewport = renderer.Pixels; }
						VideoConfig originalVideo = emu->GetSettings()->GetVideoConfig();
						for(int variant = 0; variant < 17; variant++) {
							VideoConfig wideVideo = originalVideo;
							wideVideo.DecorativeWidescreen = true;
							wideVideo.AspectRatio = VideoAspectRatio::NoStretching;
							wideVideo.DecorativeAspectRatio = variant == 1 ? 21.0 / 9 : variant == 2 ? 2.0 : 16.0 / 9;
							wideVideo.DecorativeSideBrightness = variant == 1 ? 37 : variant == 4 ? 0 : 100;
							wideVideo.DecorativeViewportPosition = variant == 2 ? 1 : variant == 3 ? 2 : 0;
							wideVideo.DecorativeCustomWidths = variant == 6;
							wideVideo.DecorativeLeftWidth = 5; wideVideo.DecorativeRightWidth = 9;
							wideVideo.DecorativeFadeWidth = variant == 6 ? 4 : 0;
							if(variant >= 7) {
								raster.WidescreenPosition = 3; raster.WidescreenSlideSpeed = 0;
								raster.WidescreenCameraMin = variant == 7 ? -10 : -100;
								raster.WidescreenCameraMax = variant == 7 ? 300 : -10;
							}
							raster.ExtendIntoMargins = variant != 5;
							raster.UseWidescreenPosition = variant >= 7;
							if(variant >= 9) {
								// Per-PNG RAM boundaries must override a centred video profile,
								// including capture of bytes not used by activation/camera.
								wideVideo.DecorativeViewportPosition = 0;
								raster.WidescreenPosition = 3; raster.WidescreenUseRamFlags = true;
								raster.WidescreenSlideSpeed = variant == 10 ? 3 : 0;
								raster.WidescreenLeftFlags = {{0x60, (uint8_t)(variant == 9 || variant == 11 ? 13 : 12), 255}};
								raster.WidescreenRightFlags = {{0x60, (uint8_t)(variant == 10 || variant == 11 ? 13 : 12), 255}};
							}
							if(variant >= 13) {
								raster.Id = variant; // Start each source at home, including the zero-speed case.
								raster.WidescreenRamSpeed.Enabled = true;
								raster.WidescreenRamSpeed.Address = variant == 13 ? 0x65 : variant == 14 ? 0x64 : 0x66;
								raster.WidescreenRamSpeed.Multiplier = 0.0625;
								raster.WidescreenRamSpeed.Maximum = 2;
								raster.WidescreenUseRamFlags = false; // CPU limits still choose the target.
								raster.WidescreenCameraMin = variant == 13 ? -10 : -100;
								raster.WidescreenCameraMax = variant == 13 ? 100 : -10;
							}
							if(variant == 16) {
								// Finite 1024-native-pixel canvas, camera effectively at 766
								// before its configured trigger. Preserve the baseline pattern.
								raster.WrapX = raster.WrapY = false; raster.X = -776;
								raster.Width = 2048; raster.Height = 480; raster.Artwork.resize(2048 * 480);
								for(int y = 0; y < 480; y++) { for(int x = 0; x < 2048; x++) {
									raster.Artwork[y * 2048 + x] = 0xFF000000u | (((x - 1552) % 64 + 64) % 64 << 16) | (y % 64 << 8);
								} }
								raster.WidescreenCameraMin = -1000; raster.WidescreenCameraMax = 0;
							}
							uint32_t speedBefore;
							{ std::lock_guard<std::mutex> lock(renderer.Mutex); speedBefore = renderer.FrameNumber; }
							emu->GetVideoDecoder()->SetMetaReplacements({group}, {raster});
							emu->GetSettings()->SetVideoConfig(wideVideo);
							int wideWidth = variant == 6 ? 540 : variant == 1 ? 1120 : variant == 2 ? 960 : 854;
							int left = variant == 16 ? 338 : variant == 6 ? 10 : variant == 2 || variant == 7 || variant == 9 || variant == 13 ? 0 : variant == 3 || variant == 8 || variant == 10 || variant == 14 ? wideWidth - 512 : (wideWidth - 512) / 2;
							if(!waitFor([&]() {
								if(renderer.Width != wideWidth || renderer.Height != 480 || nativeViewport.size() != 512 * 480) { return false; }
								if(variant >= 13 && (!renderer.Capture || renderer.FrameNumber <= speedBefore + 5
									|| !renderer.Capture->MemorySnapshot.count(raster.WidescreenRamSpeed.Address)
									|| renderer.Capture->MemorySnapshot.at(raster.WidescreenRamSpeed.Address) != (variant == 13 ? 0xE8 : variant == 14 ? 0x18 : 0))) { return false; }
								for(int y = 0; y < 480; y++) { for(int x = 0; x < wideWidth; x++) {
									uint32_t expected;
									if(x >= left && x < left + 512) { expected = nativeViewport[y * 512 + x - left]; }
									else if(variant == 5) { expected = 0xFF000000; }
									else {
										int relative = x - left;
										int ax = ((relative - 20) % 64 + 64) % 64, ay = y % 64;
										int fade = variant == 6 ? std::min(x < left ? left - 1 - x : x - (left + 512), 8) : 1;
										int denominator = variant == 6 ? 8 : 1;
										int level = variant == 6 ? 100 * denominator - (100 - wideVideo.DecorativeSideBrightness) * fade : wideVideo.DecorativeSideBrightness;
										expected = 0xFF000000 | ((ax * level + 50 * denominator) / (100 * denominator) << 16)
											| ((ay * level + 50 * denominator) / (100 * denominator) << 8);
									}
									if(renderer.Pixels[y * wideWidth + x] != expected) { return false; }
								} }
								return true;
							}, variant == 10 || variant == 13 || variant == 14 ? 128 : 0)) {
								flags |= 128; std::lock_guard<std::mutex> lock(renderer.Mutex); int observedLeft = -1;
								if(renderer.Height > 18) { for(uint32_t x = 0; x < renderer.Width; x++) { if(renderer.Pixels[18 * renderer.Width + x] == 0xFF00FF00) { observedLeft = (int)x - 16; break; } } }
								std::cerr << "Decorative widescreen mismatch, NROM variant " << variant << "; left=" << observedLeft << "/" << left << "; frames=" << renderer.FrameNumber - speedBefore << std::endl;
							}
						}
						// Config changes can happen while emulation is running.
						emu->GetSettings()->SetVideoConfig(originalVideo);
						if(!waitFor([&]() { return renderer.Width == 512 && renderer.Height == 480 && renderer.Pixels == nativeViewport; })) { flags |= 128; }
						if(!(flags & 128)) { std::cout << "PASS: live NROM decorative canvas, CPU limits, RAM boundaries, signed player $18/$E8 speed, finite 1024px PNG coverage before trigger at effective camera 766, and restoration." << std::endl; }
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
						if(!waitFor([&]() { return renderer.Width == 512 && renderer.Pixels[0] == 0xFF0000FF && renderer.Pixels[17 * 512 + 17] == 0xFF00FF00; })) { flags |= 128; }
					}
					if(!VerifyExpandedCanvas(emu.get(), renderer, group, 8 - cropLeft, 8 - cropTop, 0, source.ExpansionClass, cropLeft, cropTop)) { flags |= 128; }
					if(!VerifyLiveMemoryConditions(emu.get(), renderer, group)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaReplacements({}, {});
					if(waitFor([&]() { return renderer.Width == nativeWidth && renderer.Height == nativeHeight && renderer.Pixels[(8 - cropTop) * nativeWidth + 8 - cropLeft] == (transparentFixture ? 0xFF0000FF : 0xFFFF0000); })) { flags |= 4; }
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpriteWsCaptureTest(const char* filename)
	{
		bool monochrome = string(filename).find("mono") != string::npos;
		bool underlayFixture = string(filename).find("underlay") != string::npos;
		bool clearForegroundFixture = string(filename).find("underlay-clear") != string::npos;
		uint32_t nativeSprite = monochrome ? 0xFF000000 : 0xFFFF0000;
		uint32_t nativeBackground = underlayFixture ? 0xFFFFFF00 : monochrome ? 0xFFFFFFFF : 0xFF0000FF;
		bool holes = string(filename).find("holes") != string::npos;
		bool occluded = string(filename).find("occluded") != string::npos;
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg = emu->GetSettings()->GetWsConfig();
		cfg.UseBootRom = false; cfg.LcdShowIcons = false; cfg.BlendFrames = false; cfg.LcdAdjustColors = false;
		MetaSpriteTestRenderer renderer; uint32_t flags = 0;
		MetaSpriteGraphic source;
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			if(auto ws = dynamic_cast<WsConsole*>(emu->GetConsole().get())) {
				ws->GetPpu()->SetMetaSpriteCapture(true);
				for(int attempt = 0; attempt < 500; attempt++) {
					{
						std::lock_guard<std::mutex> lock(renderer.Mutex);
						if(renderer.Capture && renderer.Width == 224 && renderer.Height == 144) {
							for(const auto& graphic : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
								if(graphic.HardwareSprite != 0 || graphic.X != 8 || graphic.Y != 8 || graphic.Width != 8 || graphic.Height != 8) { continue; }
								bool validGraphic = true;
								for(size_t i = 0; i < graphic.Pixels.size(); i++) { validGraphic &= graphic.Pixels[i] == (holes && i % 8 == 7 ? 0 : nativeSprite); }
								if(!validGraphic) { continue; }
								const auto& pixel = renderer.Capture->Pixels[8 * 224 + 8];
								if(string(filename).find("background") != string::npos) {
									bool scrolled = string(filename).find("scroll") != string::npos;
									bool windowFixture = string(filename).find("window") != string::npos;
									bool outside = string(filename).find("outside") != string::npos;
									bool valid = true;
									for(int y = 0; y < 144; y++) {
										for(int x = 0; x < 224; x++) {
											const auto& sample = renderer.Capture->Pixels[y * 224 + x].BackgroundSample;
											bool inside = x >= 40 && x <= 95 && y >= 24 && y <= 63;
											bool window = windowFixture && inside != outside && !clearForegroundFixture;
											valid &= sample.Valid && sample.Layer == (window ? 1 : 0) && sample.X == ((x + (window ? 7 : scrolled ? 253 : 0)) & 255)
												&& sample.Y == ((y + (window ? 9 : scrolled ? 250 : 0)) & 255);
										}
									}
									if(!valid) { continue; }
								}
								if((occluded ? pixel.VisibleSprite == -1 : pixel.VisibleSprite == 0 && pixel.SpriteColor == (monochrome ? 0 : 0xF00))
									&& pixel.BackgroundColor == (underlayFixture ? 0xFF0 : monochrome ? 0xFFF : 15) && renderer.Pixels[8 * 224 + 8] == (occluded ? nativeBackground : nativeSprite)
									&& renderer.Pixels[0] == nativeBackground) { flags = 1; source = graphic; }
							}
						}
					}
					if(flags) { break; }
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
				if(!flags) { std::cerr << "WonderSwan capture failed" << std::endl; }
				if(flags) {
					MetaSpriteReplacement group;
					group.Id = 1; group.ArtworkScale = 2; group.ArtworkWidth = group.ArtworkHeight = 16;
					group.Artwork.assign(256, 0xFF00FF00); group.Artwork[0] = 0;
					bool backgroundFixture = string(filename).find("background") != string::npos;
					bool patterned = backgroundFixture && string(filename).find("scroll") != string::npos;
					bool customScroll = backgroundFixture && string(filename).find("scroll-speed") != string::npos;
					bool windowFixture = backgroundFixture && string(filename).find("window") != string::npos;
					bool outsideWindow = windowFixture && string(filename).find("outside") != string::npos;
					auto texture = [](int x, int y) { return 0xFF000000u | ((uint32_t)x * 13 << 16) | ((uint32_t)y * 11 << 8) | ((x ^ y) * 7); };
					auto expectedBackground = [&](int x, int y) {
						if(!backgroundFixture) { return nativeBackground; }
						bool inside = x / 2 >= 40 && x / 2 <= 95 && y / 2 >= 24 && y / 2 <= 63;
						if(windowFixture && inside != outsideWindow) { return 0xFFFF00FFu; }
						if(!patterned) { return 0xFFFFFFFFu; }
						int tx = (((x / 2 + (customScroll ? 126 : 253)) & 255) % 8) * 2 + x % 2;
						int ty = (((y / 2 + (customScroll ? -250 : 250)) & 255) % 8) * 2 + y % 2;
						return texture(tx, ty);
					};
					std::vector<MetaBackgroundReplacement> authoredBackgrounds;
					if(backgroundFixture) {
						MetaBackgroundReplacement bg;
						bg.Id = 1; bg.Width = bg.Height = 2; bg.Scale = 2; bg.WrapX = bg.WrapY = true; bg.Artwork.assign(4, 0xFFFFFFFF);
						bg.MemoryConditions = {{0, 0, 0}, {1, 0, 0}};
						if(customScroll) { bg.ScrollSpeedX = 500; bg.ScrollSpeedY = -1000; }
						if(patterned) {
							bg.Width = bg.Height = 16; bg.Artwork.resize(256);
							for(int y = 0; y < 16; y++) { for(int x = 0; x < 16; x++) { bg.Artwork[y * 16 + x] = texture(x, y); } }
						}
						if(windowFixture) {
							MetaBackgroundReplacement window = bg; window.Id = 2; window.Layer = 1; window.Artwork.assign(4, 0xFFFF00FF);
							authoredBackgrounds = {bg, window};
						} else { authoredBackgrounds = {bg}; }
						emu->GetVideoDecoder()->SetMetaBackgroundLayers(authoredBackgrounds);
						group.Artwork[1] = 0x8000FF00;
					}
					source.X = source.Y = 0; group.Members = {source};
					auto waitFor = [&](std::function<bool()> condition) {
						for(int attempt = 0; attempt < 500; attempt++) {
							{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						}
						return false;
					};
					group.MemoryConditions = {{0, 0, 0}}; emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(waitFor([&]() {
						if(renderer.Width != 448 || renderer.Height != 288) { return false; }
						for(int y = 0; y < 16; y++) {
							for(int x = 0; x < 16; x++) {
								uint32_t expected = x == 0 && y == 0 ? expectedBackground(x + 16, y + 16) : 0xFF00FF00;
								if(x == 1 && y == 0 && backgroundFixture) {
									expected = 0xFF000000;
									uint32_t bg = expectedBackground(x + 16, y + 16);
									for(int shift : {0, 8, 16}) { expected |= (((shift == 8 ? 255 : 0) * 128 + ((bg >> shift) & 255) * 127 + 127) / 255) << shift; }
								}
								if(occluded) { expected = expectedBackground(x + 16, y + 16); }
								if(renderer.Pixels[(y + 16) * 448 + x + 16] != expected) { return false; }
							}
						}
						if(patterned || windowFixture) {
							for(int y = 0; y < 288; y++) {
								for(int x = 0; x < 448; x++) {
									if(x >= 16 && x < 32 && y >= 16 && y < 32) { continue; }
									if(renderer.Pixels[y * 448 + x] != expectedBackground(x, y)) { return false; }
								}
							}
						}
						return renderer.Pixels[0] == expectedBackground(0, 0);
					})) { flags |= 2; }
					if(!VerifyExpandedCanvas(emu.get(), renderer, group, 8, 8, 0, source.ExpansionClass)) { flags |= 128; }
					if(underlayFixture) {
						MetaBackgroundReplacement upper = authoredBackgrounds[1]; upper.HideOriginalLayer = true; upper.Artwork.assign(4, 0);
						auto check = [&](uint32_t expected) { return waitFor([&]() {
							if(renderer.Width != 448 || !renderer.Capture || renderer.Capture->BackgroundUnderlays.size() != 224 * 144) { return false; }
							const auto& lower = renderer.Capture->BackgroundUnderlays[30 * 224 + 50];
								return lower.Sample.Valid && lower.Sample.Layer == 0 && lower.Color == 0xFF0
									&& lower.ForegroundSample.Valid && lower.ForegroundOpaque == !clearForegroundFixture
								&& renderer.Capture->Pixels[30 * 224 + 50].NativeBackdropColor == 15 && renderer.Pixels[60 * 448 + 100] == expected;
						}); };
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {authoredBackgrounds[0], upper});
						if(!check(0xFFFFFFFF)) { flags |= 128; }
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {upper});
						if(!check(0xFFFFFF00)) { flags |= 128; }
						upper.Artwork.assign(4, 0x8000FF00); emu->GetVideoDecoder()->SetMetaReplacements({group}, {upper});
						if(!check(0xFF7FFF00)) { flags |= 128; }
						MetaBackgroundReplacement lower = authoredBackgrounds[0]; lower.HideOriginalLayer = true; lower.Artwork.assign(4, 0);
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {lower, upper});
						if(!check(0xFF00807F)) { flags |= 128; }
						if(!(flags & 128)) { std::cout << "PASS: WonderSwan hidden BG1 reveals distinct native BG0 or its PNG; both hidden layers reveal backdrop with partial-alpha artwork." << std::endl; }
						emu->GetVideoDecoder()->SetMetaReplacements({group}, authoredBackgrounds);
						if(!waitFor([&]() { return renderer.Width == 448 && renderer.Pixels[60 * 448 + 100] == 0xFFFF00FF; })) { flags |= 128; }
					}
					if(backgroundFixture) {
						uint8_t first = 0, second = 0;
						if(!waitFor([&]() {
							if(!renderer.Capture || renderer.Capture->MemorySnapshot.size() != 2) { return false; }
							const auto& bytes = renderer.Capture->MemorySnapshot;
							if(!bytes.count(0) || !bytes.count(1)) { return false; }
							first = bytes.at(0); second = bytes.at(1); return true;
						})) { flags |= 128; }
						for(int failingCondition : {0, 1, -1}) {
							for(auto& bg : authoredBackgrounds) {
								bg.MemoryConditions = {{0, (uint8_t)(first ^ (failingCondition == 0 ? 1 : 0)), 1},
									{1, (uint8_t)(second ^ (failingCondition == 1 ? 0x80 : 0)), 0x80}};
							}
							emu->GetVideoDecoder()->SetMetaReplacements({group}, authoredBackgrounds);
							if(!waitFor([&]() {
								if(renderer.Width != 448 || renderer.Height != 288 || !renderer.Capture || renderer.Capture->MemorySnapshot.size() != 2) { return false; }
								for(auto point : {std::pair<int, int>{0, 0}, {80, 48}}) {
									uint32_t expected = failingCondition < 0 ? expectedBackground(point.first, point.second) : nativeBackground;
									if(underlayFixture && !clearForegroundFixture && failingCondition >= 0 && point.first == 80) { expected = 0xFF0000FF; }
									if(renderer.Pixels[point.second * 448 + point.first] != expected) { return false; }
								}
								return true;
							})) { flags |= 128; }
						}
						if(!(flags & 128)) { std::cout << "PASS: WonderSwan background AND/masked CPU conditions gate both layers; capture contains only addresses 0 and 1." << std::endl; }
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
						if(!waitFor([&]() { return renderer.Capture && renderer.Width == 448 && renderer.Capture->MemorySnapshot.size() == 1 && renderer.Capture->MemorySnapshot.count(0); })) { flags |= 128; }
					}
					if(!VerifyLiveMemoryConditions(emu.get(), renderer, group, !occluded)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					emu->GetVideoDecoder()->SetMetaBackgroundLayers({});
					if(waitFor([&]() { return renderer.Width == 224 && renderer.Height == 144 && renderer.Pixels[8 * 224 + 8] == (occluded ? nativeBackground : nativeSprite) && renderer.Pixels[0] == nativeBackground; })) { flags |= 4; }
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpriteGbaCaptureTest(const char* filename)
	{
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator());
		emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg = emu->GetSettings()->GetGbaConfig();
		cfg.SkipBootScreen = true; cfg.DisableFrameSkipping = true;
		cfg.BlendFrames = false; cfg.GbaAdjustColors = false; cfg.RamPowerOnState = RamState::AllZeros;
		MetaSpriteTestRenderer renderer;
		uint32_t flags = 0;
		bool mirrored = string(filename).find("mirror") != string::npos;
		bool bpp8 = string(filename).find("bpp8") != string::npos;
		bool asymmetricObject = string(filename).find("object-affine-enlarged-asymmetric") != string::npos;
		bool transposedObject = string(filename).find("object-affine-transposed") != string::npos;
		bool quarterTurnObject = string(filename).find("quarter-turn") != string::npos;
		bool twoDimensionalObject = string(filename).find("quarter-turn-2d") != string::npos;
		bool mosaicObject = string(filename).find("object-affine") != string::npos && string(filename).find("mosaic") != string::npos;
		bool ordinaryMosaic = string(filename).find("object-ordinary-") != string::npos;
		bool flippedOrdinary = ordinaryMosaic && string(filename).find("flipped") != string::npos;
		bool verticalAffineMosaic = mosaicObject && string(filename).find("vmosaic") != string::npos;
		bool horizontalAffineMosaic = mosaicObject && string(filename).find("-vmosaic") == string::npos;
		bool lowerReveal = string(filename).find("lower-reveal") != string::npos;
		bool lowerSemi = lowerReveal && string(filename).find("semi") != string::npos;
		bool lowerMosaic = string(filename).find("lower-reveal-mosaic") != string::npos;
		bool clippedLowerMosaic = string(filename).find("mosaic-clipped") != string::npos;
		bool lowerVerticalMosaic = string(filename).find("lower-reveal-vmosaic") != string::npos;
		bool backgroundBetween = string(filename).find("bg-between") != string::npos;
		bool backgroundTie = backgroundBetween && string(filename).find("priority1") != string::npos;
		bool backgroundBetweenPng = string(filename).find("bg-between-png") != string::npos;
		int objectSize = string(filename).find("double-size") != string::npos ? 16 : 8;
		bool dark = string(filename).find("dark") != string::npos;
		bool holes = string(filename).find("holes") != string::npos;
		MetaSpriteGraphic source;
		bool window = string(filename).find("window") != string::npos;
		bool middleWindow = string(filename).find("middle") != string::npos;
		bool overlap = string(filename).find("overlap") != string::npos;
		bool transparentSource = string(filename).find("transparent") != string::npos;
		bool backgroundCapture = string(filename).find("background") != string::npos;
		bool wideBackground = string(filename).find("wide") != string::npos;
		int bitmapMode = string(filename).find("bitmap3") != string::npos ? 3 : string(filename).find("bitmap5") != string::npos ? 5 : string(filename).find("bitmap4") != string::npos ? 4 : 0;
		bool bitmapBackground = bitmapMode != 0;
		bool opaqueBitmap = bitmapBackground && (bitmapMode != 4 || string(filename).find("page1") != string::npos);
		auto inBitmap = [&](int x, int y) { return bitmapMode != 5 || (x < 160 && y < 128); };
		int backgroundLayer = bitmapBackground || string(filename).find("affine") != string::npos ? 2 : 0;
		bool scaledBackground = string(filename).find("scaled") != string::npos;
		bool rotatedBackground = string(filename).find("rotated") != string::npos;
		auto backgroundX = [&](int x, int y) { return bitmapBackground ? x : rotatedBackground ? 253 + y : scaledBackground ? 253 + 2 * x + y : 253 + x; };
		auto backgroundY = [&](int x, int y) { return bitmapBackground ? y : rotatedBackground ? 250 - x : 250 + y; };
		auto masked = [&](int x) { return window && (middleWindow ? x >= 2 && x < 6 : x >= 4); };
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			if(auto gba = dynamic_cast<GbaConsole*>(emu->GetConsole().get())) {
				gba->GetPpu()->SetMetaSpriteCapture(true);
				for(int attempt = 0; attempt < 500; attempt++) {
					{
						std::lock_guard<std::mutex> lock(renderer.Mutex);
						if(renderer.Capture && renderer.Width == 240 && renderer.Height == 160) {
							for(const auto& graphic : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
								if(graphic.HardwareSprite != 0 || graphic.X != 8 || graphic.Y != 8 || graphic.Width != objectSize || graphic.Height != objectSize) { continue; }
								bool expectedGraphic = graphic.GbaPriority == 0 && !graphic.GbaSemiTransparent;
								for(int y = 0; y < objectSize; y++) {
									for(int x = 0; x < objectSize; x++) {
										// Enlarged inverse sampling uses floor(2 + axis/2); source columns 5..7 are blue.
										int blueBoundary = objectSize == 16 ? (quarterTurnObject ? 12 : asymmetricObject ? 10 : 8) : asymmetricObject ? 6 : 4;
										uint32_t expected = (mirrored ? x < 4 : bpp8 && (transposedObject || quarterTurnObject ? y : x) >= blueBoundary) ? 0xFF0000FF : 0xFFFF0000;
										// Quarter-turn source Y = floor(6 - x/2); rows 0..3 are also blue.
										if(twoDimensionalObject && x >= (objectSize == 16 ? 9 : 5)) { expected = 0xFF0000FF; }
										uint32_t displayed = dark ? (expected == 0xFF0000FF ? 0xFF00007B : 0xFF7B0000) : expected;
										if(mosaicObject) { int sampledX = horizontalAffineMosaic ? (x & ~1) : x; int sampledY = verticalAffineMosaic ? (y & ~3) : y; displayed = sampledY >= 6 || sampledX >= 5 ? 0xFF0000FF : 0xFFFF0000; }
										if(ordinaryMosaic) {
											int sourceX = flippedOrdinary ? 7 - x : x, sourceY = flippedOrdinary ? 7 - y : y;
											expected = sourceX >= 5 || sourceY >= 5 ? 0xFF0000FF : 0xFFFF0000;
											int displayedX = flippedOrdinary ? 7 - (x & ~1) : x, displayedY = flippedOrdinary ? 7 - (y & ~3) : (y & ~3);
											displayed = displayedX >= 5 || displayedY >= 5 ? 0xFF0000FF : 0xFFFF0000;
										}
										if(renderer.Capture->Pixels[(y + 8) * 240 + x + 8].GbaObjectsEnabled == masked(x)) { expectedGraphic = false; }
										if(masked(x)) { displayed = 0xFF0000FF; }
										if(transparentSource || (holes && x == 7)) {
											expected = 0; displayed = lowerReveal && !masked(x) ? (dark ? 0xFF7B7B00 : 0xFFFFFF00) : dark ? 0xFF00007B : 0xFF0000FF;
											if(lowerSemi && !masked(x)) { displayed = 0xFF7B7B7B; }
											if(lowerMosaic && !masked(x)) { displayed = 0xFFFF0000; } // Previous winning upper OBJ is not mosaic, so the native hole starts a new sample.
											if(clippedLowerMosaic) { displayed = 0xFFFFFF00; }
											if(backgroundBetween && !backgroundTie) { displayed = 0xFFFF00FF; }
											const auto& hole = renderer.Capture->Pixels[(y + 8) * 240 + x + 8];
											bool eligible = std::any_of(hole.GbaCoverage.begin(), hole.GbaCoverage.end(), [](const auto& coverage) { return coverage.Sprite == 0; });
											if(hole.VisibleSprite != (overlap && !masked(x) && (!backgroundBetween || backgroundTie) ? 1 : -1) || eligible == masked(x)) { expectedGraphic = false; }
										}
										if(graphic.Pixels[y * objectSize + x] != expected || renderer.Pixels[(y + 8) * 240 + x + 8] != displayed) {
											expectedGraphic = false;
										}
									}
									}
								if(!expectedGraphic) { continue; }
								if(backgroundCapture) {
									bool valid = true;
									int size = wideBackground ? 512 : 256;
									for(int y = 0; y < 160; y++) { for(int x = 0; x < 240; x++) {
										const auto& samples = renderer.Capture->Pixels[y * 240 + x].GbaBackgroundSamples;
										const auto& captured = renderer.Capture->Pixels[y * 240 + x];
										const auto& layer0 = captured.GbaBackgroundLayers[backgroundLayer];
										valid &= layer0.Priority == 3 && layer0.Opaque == (opaqueBitmap && inBitmap(x, y)) && layer0.Color == 0x7C00;
										valid &= captured.GbaBlendMainMask == (1 << backgroundLayer) && captured.GbaBlendSubMask == 32 && captured.GbaWindowEffectsEnabled && captured.GbaBackdropColor == 0x7C00;
										bool objectHere = x >= 8 && x < 16 && y >= 8 && y < 16;
										valid &= captured.GbaObjectSprite == (objectHere ? 0 : -1) && captured.GbaObjectPriority == (objectHere ? 0 : 0xFF);
										valid &= captured.GbaObjectColor == (objectHere ? 31 : 0x7C00) && !captured.GbaObjectSemiTransparent;
										const auto& sample = samples[backgroundLayer];
										int expectedX = backgroundLayer == 2 ? backgroundX(x, y) & (size - 1) : (x + size - 3) % size;
										int expectedY = backgroundLayer == 2 ? backgroundY(x, y) & (size - 1) : (y + size - 6) % size;
										valid &= sample.Valid == inBitmap(x, y);
										if(sample.Valid) { valid &= sample.Layer == backgroundLayer && sample.X == expectedX && sample.Y == expectedY; }
										for(int layer = 0; layer < 4; layer++) { if(layer != backgroundLayer) { valid &= !samples[layer].Valid; } }
									} }
									if(!valid) { continue; }
								}
								const auto& pixel = renderer.Capture->Pixels[8 * 240 + 8];
									if(pixel.VisibleSprite == (transparentSource ? -1 : 0) && pixel.SpriteColor == (transparentSource || mirrored || flippedOrdinary ? 0x7C00 : 31) && pixel.SecondarySprite == -1
									&& pixel.GbaFallbackCaptured && pixel.GbaFallbackColor == (backgroundBetween ? 0x7C1F : dark ? 0x3C00 : 0x7C00) && pixel.GbaFallbackMainColor == (backgroundBetween ? 0x7C1F : 0x7C00)) { flags |= 1; source = graphic; }
							}
						}
					}
					if(flags) { break; }
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
				if(!flags) {
					std::lock_guard<std::mutex> lock(renderer.Mutex);
					std::cerr << "GBA capture: size=" << renderer.Width << "x" << renderer.Height << ", capture=" << (bool)renderer.Capture;
					if(renderer.Capture) { std::cerr << ", fragments=" << renderer.Capture->DecodedRows.size() << ", candidates=" << MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture).size(); }
					std::cerr << std::endl;
				}
				if(flags) {
					MetaSpriteReplacement group;
					group.Id = 1; group.ArtworkScale = 2; group.ArtworkWidth = group.ArtworkHeight = 16;
					group.Artwork.assign(256, 0xFF00FF00); group.Artwork[0] = 0; group.Artwork[1] = 0x8000FF00;
					if(holes) { group.Artwork[14] = 0; group.Artwork[15] = 0x8000FF00; }
					if(lowerVerticalMosaic) { for(int y = 1; y < 16; y++) { group.Artwork[y * 16 + 14] = 0; group.Artwork[y * 16 + 15] = 0x8000FF00; } }
					if(clippedLowerMosaic) { group.Artwork[2] = 0; group.Artwork[3] = 0x8000FF00; }
					source.X = source.Y = 0; group.Members = {source};
					if(backgroundCapture) {
						MetaBackgroundReplacement background;
						background.Id = 1; background.Layer = backgroundLayer; background.Width = background.Height = 16; background.Scale = 2;
						background.WrapX = background.WrapY = true; background.Artwork.assign(256, 0xFFFFFFFF);
						if(backgroundLayer == 2 && !bitmapBackground) {
							for(int y = 0; y < 16; y++) { for(int x = 0; x < 16; x++) {
								background.Artwork[y * 16 + x] = (x / 2) % 2 ? 0xFFFFFFFF : 0xFF00FFFF;
							} }
						}
						group.MemoryConditions = {{0, 0, 0}}; emu->GetVideoDecoder()->SetMetaReplacements({group}, {background});
					} else {
						group.MemoryConditions = {{0, 0, 0}};
						if(backgroundBetweenPng) {
							MetaBackgroundReplacement bg; bg.Id = 1; bg.Layer = 0; bg.Width = bg.Height = 1;
							bg.WrapX = bg.WrapY = true; bg.Artwork = {0xFFFFFFFF}; bg.MemoryConditions = {{0, 0, 0}};
							emu->GetVideoDecoder()->SetMetaReplacements({group}, {bg});
						} else { emu->GetVideoDecoder()->SetMetaSpriteGroups({group}); }
					}
					auto waitFor = [&](std::function<bool()> condition) {
						for(int attempt = 0; attempt < 500; attempt++) {
							{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						}
						return false;
					};
					if(waitFor([&]() {
						if(renderer.Width != 480 || renderer.Height != 320) { return false; }
						for(int y = 0; y < 16; y++) {
							for(int x = 0; x < 16; x++) {
								bool transparent = (y == 0 && x == 0) || (holes && x == 14 && (y == 0 || lowerVerticalMosaic));
								bool partial = (y == 0 && x == 1) || (holes && x == 15 && (y == 0 || lowerVerticalMosaic));
								transparent |= clippedLowerMosaic && y == 0 && x == 2;
								partial |= clippedLowerMosaic && y == 0 && x == 3;
								uint32_t expected = transparent ? 0xFF0000FF : partial ? 0xFF00807F : 0xFF00FF00;
								if(lowerReveal) { expected = transparent ? 0xFFFFFF00 : partial ? 0xFF7FFF00 : 0xFF00FF00; }
								if(dark) { expected = transparent ? 0xFF00007B : partial ? 0xFF00403D : 0xFF007F00; }
								if(lowerReveal && dark) { expected = transparent ? 0xFF7B7B00 : partial ? 0xFF3D7D00 : 0xFF007F00; }
								if(lowerSemi) { expected = transparent ? 0xFF7B7B7B : partial ? 0xFF3DBD3D : 0xFF00FF00; }
								if(backgroundBetween && !backgroundTie) { expected = transparent ? (backgroundBetweenPng ? 0xFFFFFFFF : 0xFFFF00FF) : partial ? (backgroundBetweenPng ? 0xFF7FFF7F : 0xFF7F807F) : 0xFF00FF00; }
								if(clippedLowerMosaic && y == 0) {
									if(x == 0) { expected = 0xFF0000FF; } else if(x == 1) { expected = 0xFF00807F; }
									else if(x == 14) { expected = 0xFFFF0000; } else if(x == 15) { expected = 0xFF7F8000; }
								}
								if(masked(x / 2)) { expected = 0xFF0000FF; }
								if(backgroundCapture) { expected = transparent ? 0xFF7F7FFF : partial ? 0xFF3FBF7F : 0xFF00FF00; }
								if(renderer.Pixels[(y + 16) * 480 + x + 16] != expected) { return false; }
							}
						}
						if(backgroundCapture) {
							for(int y = 0; y < 320; y++) { for(int x = 0; x < 480; x++) {
								if(x >= 16 && x < 32 && y >= 16 && y < 32) { continue; }
								uint32_t expectedBackground = backgroundLayer == 2 && !bitmapBackground && (backgroundX(x / 2, y / 2) % 2 == 0) ? 0xFF007FFF : 0xFF7F7FFF;
								if(!inBitmap(x / 2, y / 2)) { expectedBackground = 0xFF0000FF; }
								if(renderer.Pixels[y * 480 + x] != expectedBackground) { return false; }
							} }
						}
						return renderer.Pixels[0] == (backgroundBetweenPng ? 0xFFFFFFFF : backgroundBetween ? 0xFFFF00FF : backgroundCapture ? 0xFF7F7FFF : dark ? 0xFF00007B : 0xFF0000FF);
					})) { flags |= 2; }
					if(!backgroundCapture && !window && !overlap && (flags & 2)) {
						MetaSpriteReplacement enlarged = group;
						enlarged.ArtworkWidth = enlarged.ArtworkHeight = 21;
						enlarged.ArtworkOriginX = enlarged.ArtworkOriginY = 1;
						enlarged.Artwork.assign(441, 0xFF00FF00); enlarged.Artwork[0] = 0; enlarged.Artwork[1] = 0x8000FF00;
						emu->GetVideoDecoder()->SetMetaSpriteGroups({enlarged});
						if(!waitFor([&]() {
							if(renderer.Width != 480 || renderer.Height != 320) { return false; }
							for(int y = 0; y < 320; y++) { for(int x = 0; x < 480; x++) {
								uint32_t expected = x >= 14 && x < 35 && y >= 14 && y < 35 ? 0xFF00FF00 : 0xFF0000FF;
								if(y == 14 && x == 14) { expected = 0xFF0000FF; }
								if(y == 14 && x == 15) { expected = 0xFF00807F; }
								if(dark) { expected = expected == 0xFF00FF00 ? 0xFF007F00 : expected == 0xFF00807F ? 0xFF00403D : 0xFF00007B; }
								if(renderer.Pixels[y * 480 + x] != expected) { return false; }
							} }
							return true;
						})) { flags |= 128; }
						else { std::cout << "PASS: GBA live full canvas, shifted origin, PNG alpha and partial native edge." << std::endl; }
						emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					}
					if(string(filename).find("mixed-group") != string::npos && (flags & 2)) {
						MetaSpriteReplacement mixed = group;
						bool capturedSecond = false;
						{
							std::lock_guard<std::mutex> lock(renderer.Mutex);
							if(renderer.Capture) { for(auto member : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
								if(member.HardwareSprite == 1 && member.X == 15 && member.Y == 8 && member.GbaPriority == 3 && member.Width == 8 && member.Height == 8) {
									member.X = 7; member.Y = 0; mixed.Members.push_back(member); capturedSecond = true; break;
								}
							} }
						}
						mixed.ArtworkWidth = 40; mixed.ArtworkHeight = 20;
						mixed.ArtworkOriginX = mixed.ArtworkOriginY = 2;
						mixed.Artwork.assign(800, 0xFF00FF00); mixed.Artwork[0] = 0; mixed.Artwork[1] = 0x8000FF00;
						emu->GetVideoDecoder()->SetMetaSpriteGroups({mixed});
						if(!capturedSecond || !waitFor([&]() {
							if(renderer.Width != 480 || renderer.Height != 320) { return false; }
							for(int y = 0; y < 320; y++) { for(int x = 0; x < 480; x++) {
								uint32_t expected = 0xFFFF00FF;
								// Source rectangles overlap at native x=15; priority 0 wins there.
								// Beyond it the priority-3 member and its full-canvas margin stay behind BG0.
								if(y >= 12 && y < 32 && x >= 12 && x < 32) { expected = 0xFF00FF00; }
								if(y == 12 && x == 12) { expected = 0xFFFF00FF; }
								if(y == 12 && x == 13) { expected = 0xFF7F807F; }
								if(renderer.Pixels[y * 480 + x] != expected) { return false; }
							} }
							return true;
						})) { flags |= 128; }
						else { std::cout << "PASS: GBA live mixed-priority members, overlap ordering, PNG alpha and oversized margins against BG0." << std::endl; }
						emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					}
					if(bitmapBackground && string(filename).find("page1-live") != string::npos) {
						for(bool opaque : {false, true}) {
							if(!waitFor([&]() {
								return renderer.Width == 480 && renderer.Capture
									&& renderer.Capture->Pixels[8 * 240 + 8].GbaBackgroundLayers[2].Opaque == opaque
									&& renderer.Pixels[0] == 0xFF7F7FFF;
							})) { flags |= 128; }
						}
					}
					if(backgroundBetweenPng) {
						uint8_t sampled = 0;
						if(!waitFor([&]() {
							if(!renderer.Capture || renderer.Capture->MemorySnapshot.size() != 1) { return false; }
							auto found = renderer.Capture->MemorySnapshot.find(0);
							if(found == renderer.Capture->MemorySnapshot.end()) { return false; }
							sampled = found->second; return true;
						})) { flags |= 128; }
						MetaBackgroundReplacement bg; bg.Id = 1; bg.Layer = 0; bg.Width = bg.Height = 1;
						bg.WrapX = bg.WrapY = true; bg.Artwork = {0xFFFFFFFF}; bg.MemoryConditions = {{0, (uint8_t)(sampled ^ 0x80), 0xFF}};
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {bg});
						if(!waitFor([&]() { return renderer.Width == 480 && renderer.Pixels[0] == 0xFFFF00FF && renderer.Pixels[16 * 480 + 16] == 0xFFFF00FF; })) { flags |= 128; }
						bg.MemoryConditions = {{0, sampled, 0xFF}}; emu->GetVideoDecoder()->SetMetaReplacements({group}, {bg});
						if(!waitFor([&]() { return renderer.Width == 480 && renderer.Pixels[0] == 0xFFFFFFFF && renderer.Pixels[16 * 480 + 16] == 0xFFFFFFFF; })) { flags |= 128; }
					}
					if(backgroundCapture || backgroundBetweenPng) { if(!VerifyLiveMemoryConditions(emu.get(), renderer, group)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaReplacements({}, {}); }
					else { if(!VerifyLiveMemoryConditions(emu.get(), renderer, group)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaSpriteGroups({}); }
					if(waitFor([&]() { return renderer.Width == 240 && renderer.Height == 160 && renderer.Pixels[8 * 240 + 8] == (dark ? 0xFF7B0000 : transparentSource || mirrored || flippedOrdinary ? 0xFF0000FF : 0xFFFF0000) && (!backgroundBetween || renderer.Pixels[0] == 0xFFFF00FF); })) { flags |= 4; }
					if(string(filename).find("panorama") != string::npos && !VerifyDedicatedWidescreen(emu.get(), renderer, 240, 160, backgroundLayer, string(filename).find("negative") != string::npos)) { flags |= 128; }
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpritePceGameplayTest(const char* filename)
	{
		bool behindOpaque = string(filename).find("opaque-bg-behind") != string::npos;
		bool opaqueNative = string(filename).find("opaque-bg") != string::npos;
		bool hideFixture = string(filename).find("opaque-bg-behind-hide") != string::npos;
		bool behindPng = string(filename).find("png-background-behind") != string::npos;
		bool backgroundFixture = string(filename).find("background") != string::npos;
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator());
		emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg = emu->GetSettings()->GetPcEngineConfig();
		cfg.RamPowerOnState = RamState::AllZeros; cfg.DisableFrameSkipping = true;
		for(int color = 0; color < 512; color++) {
			auto expand = [](int value) { return (value << 5) | (value << 2) | (value >> 1); };
			cfg.Palette[color] = 0xFF000000u | (expand((color >> 3) & 7) << 16) | (expand((color >> 6) & 7) << 8) | expand(color & 7);
		}
		MetaSpriteTestRenderer renderer;
		uint8_t divider = string(filename).find("divider3") != string::npos ? 3 : string(filename).find("divider2") != string::npos ? 2 : 4;
		bool mirrored = string(filename).find("mirror") != string::npos;
		uint32_t flags = 0, width = PceConstants::GetRowWidth(divider), height = 242, background = 0;
		int spriteWidth = string(filename).find("wide") != string::npos ? 32 : 16;
		int x = 0, y = 0;
		MetaSpriteGraphic source;
		auto waitFor = [&](std::function<bool()> condition) {
			for(int attempt = 0; attempt < 500; attempt++) {
				{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			return false;
		};
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			if(auto pce = dynamic_cast<PceConsole*>(emu->GetConsole().get())) {
				pce->GetVdc()->SetMetaSpriteCapture(true);
				if(waitFor([&]() {
					if(!renderer.Capture || renderer.Width != width || renderer.Height != height) { return false; }
					for(auto candidate : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
						if(candidate.Width != spriteWidth || candidate.Height != 16) { continue; }
						bool correct = true;
						for(size_t pixel = 0; pixel < candidate.Pixels.size(); pixel++) {
							uint32_t expected = string(filename).find("holes") != string::npos && pixel % 16 == 15 ? 0
								: mirrored && pixel % spriteWidth < 8 ? 0xFF0000FF : 0xFFFF0000;
							if(candidate.Pixels[pixel] != expected) { correct = false; break; }
						}
						if(!correct) { continue; }
						if(string(filename).find("holes") != string::npos) {
							for(int py = 0; py < 16; py++) { for(int px = 15; px < spriteWidth; px += 16) {
								const auto& hole = renderer.Capture->Pixels[(candidate.Y + py) * renderer.Capture->Width + candidate.X + px];
								correct &= hole.VisibleSprite == -1 && hole.AllowsReplacementSprite(candidate.HardwareSprite);
							} }
							if(!correct) { continue; }
						}
						x = candidate.X - PceConstants::GetLeftOverscan(divider); y = candidate.Y;
						if(opaqueNative) {
							const auto& captured = renderer.Capture->Pixels[candidate.Y * renderer.Capture->Width + candidate.X];
							if(!captured.NativeBackdropCaptured || captured.NativeBackdropColor != 0 || captured.BackgroundColor == captured.NativeBackdropColor) { continue; }
							if(captured.ExpandedSpriteClassesWithoutBackground != 3 || captured.ExpandedSpriteClasses != 1) { continue; }
						}
						if(x < 0 || y < 0 || x + spriteWidth > (int)width || y + 16 > (int)height || renderer.Pixels[y * width + x] != (behindOpaque ? 0xFF0000FF : candidate.Pixels[0])) { continue; }
						if(string(filename).find("background") != string::npos) {
							const auto& sample = renderer.Capture->Pixels[candidate.Y * renderer.Capture->Width + candidate.X].BackgroundSample;
							if(!sample.Valid || sample.Layer != 0 || sample.X != 8 || sample.Y != 8) { continue; }
							bool valid = true; size_t samples = 0;
							for(uint32_t py = 0; py < renderer.Capture->Height; py++) {
								for(uint32_t px = 0; px < renderer.Capture->Width; px++) {
									const auto& bg = renderer.Capture->Pixels[py * renderer.Capture->Width + px].BackgroundSample;
									if(!bg.Valid) { continue; }
									samples++;
									valid &= bg.Layer == 0 && bg.X == (((int)px - candidate.X + 8 + 256) % 256)
										&& bg.Y == (((int)py - candidate.Y + 8 + 256) % 256);
								}
							}
							if(!valid || samples < 256 * 224) { continue; }
						}
						source = std::move(candidate); background = renderer.Pixels[0]; return true;
					}
					return false;
				})) {
					flags |= 1; source.X = 0; source.Y = 0; source.HardwareSprite = -1;
					MetaSpriteReplacement group;
					group.Id = 1; group.ArtworkScale = 2; group.ArtworkWidth = spriteWidth * 2; group.ArtworkHeight = 32;
					group.Members.push_back(source); group.Artwork.assign(group.ArtworkWidth * group.ArtworkHeight, 0xFF00FF00);
					if(backgroundFixture) {
						MetaBackgroundReplacement bg; bg.Id = 1; bg.Width = bg.Height = 2; bg.Scale = 2;
						bg.WrapX = bg.WrapY = true; bg.Artwork.assign(4, 0xFFFFFFFF);
						group.Artwork[0] = 0; group.Artwork[1] = 0x8000FF00;
						emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
					}
					group.MemoryConditions = {{0, 0, 0}}; emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(waitFor([&]() { return renderer.Width == width * 2 && renderer.Height == height * 2; })) { flags |= 2; }
					if(waitFor([&]() {
						if(renderer.Width != width * 2) { return false; }
						for(int row = 0; row < 32; row++) {
							for(int column = 0; column < spriteWidth * 2; column++) {
								uint32_t expected = backgroundFixture && row == 0 && column == 0 ? 0xFFFFFFFF
									: backgroundFixture && row == 0 && column == 1 ? 0xFF7FFF7F : 0xFF00FF00;
								if(behindOpaque) { expected = 0xFF0000FF; }
								if(behindPng) { expected = 0xFFFFFFFF; }
								if(renderer.Pixels[(y * 2 + row) * renderer.Width + x * 2 + column] != expected) { return false; }
							}
						}
						return true;
					})) { flags |= 4; }
					if(waitFor([&]() { return renderer.Width == width * 2 && renderer.Pixels[0] == background
						&& (!backgroundFixture || renderer.Pixels[y * 2 * renderer.Width + (x + spriteWidth + 1) * 2] == 0xFFFFFFFF); })) { flags |= 8; }
					if(hideFixture) {
						MetaBackgroundReplacement hidden; hidden.Id = 1; hidden.Width = hidden.Height = 2; hidden.Scale = 2;
						hidden.WrapX = hidden.WrapY = true; hidden.HideOriginalLayer = true; hidden.Artwork.assign(4, 0); hidden.MemoryConditions = {{0, 0, 0}};
						auto check = [&](uint32_t inside, uint32_t outside, bool scaled) {
							return waitFor([&]() {
								uint32_t scale = scaled ? 2 : 1;
								return renderer.Width == width * scale && renderer.Height == height * scale
									&& renderer.Pixels[(y + 2) * scale * renderer.Width + (x + 2) * scale] == inside
									&& renderer.Pixels[(y + 2) * scale * renderer.Width + (x + spriteWidth + 3) * scale] == outside;
								});
						};
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {hidden});
						if(!check(0xFF00FF00, 0xFF000000, true)) { flags |= 128; }
						emu->GetVideoDecoder()->SetMetaReplacements({}, {hidden});
						if(!check(0xFFFF0000, 0xFF000000, true)) { flags |= 128; }
						hidden.Artwork.assign(4, 0x8000FF00); emu->GetVideoDecoder()->SetMetaReplacements({}, {hidden});
						if(!check(0xFF7F8000, 0xFF008000, true)) { flags |= 128; }
						uint8_t capturedValue = 0;
						{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.Capture && renderer.Capture->MemorySnapshot.count(0)) { capturedValue = renderer.Capture->MemorySnapshot.at(0); } else { flags |= 128; } }
						hidden.MemoryConditions = {{0, (uint8_t)(capturedValue ^ 1), 255}}; emu->GetVideoDecoder()->SetMetaReplacements({}, {hidden});
						if(!check(0xFF0000FF, 0xFF0000FF, false)) { flags |= 128; }
						hidden.MemoryConditions = {{0, capturedValue, 255}}; hidden.HideOriginalLayer = false; emu->GetVideoDecoder()->SetMetaReplacements({}, {hidden});
						if(!check(0xFF00807F, 0xFF00807F, true)) { flags |= 128; }
						if(!(flags & 128)) { std::cout << "PASS: PCE hide-layer toggle reveals native/replacement sprites and backdrop through PNG alpha; mismatch restores tiles and toggle-off restores overlay behavior." << std::endl; }
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
						if(!check(0xFF0000FF, 0xFF0000FF, true)) { flags |= 128; }
					}
					if(!VerifyExpandedCanvas(emu.get(), renderer, group, x, y, 0, source.ExpansionClass, PceConstants::GetLeftOverscan(divider), 0, behindPng)) { flags |= 128; }
					if(!VerifyLiveMemoryConditions(emu.get(), renderer, group, !behindOpaque)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					emu->GetVideoDecoder()->SetMetaBackgroundLayers({});
					if(waitFor([&]() { return renderer.Width == width && renderer.Height == height && renderer.Pixels[y * width + x] == (behindOpaque ? 0xFF0000FF : source.Pixels[0]); })) { flags |= 16; }
				} else {
					std::lock_guard<std::mutex> lock(renderer.Mutex);
					std::cerr << "PCE capture: size=" << renderer.Width << "x" << renderer.Height << ", capture=" << (bool)renderer.Capture;
					if(renderer.Capture) {
						std::cerr << ", fragments=" << renderer.Capture->DecodedRows.size() << ", candidates=" << MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture).size();
						for(auto& row : renderer.Capture->DecodedRows) { std::cerr << " [x=" << row.X << " y=" << row.Y << " row=" << row.Row << "]"; }
					}
					std::cerr << std::endl;
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpriteGameplayTest(const char* filename)
	{
		MetaSpriteTestMessages messages;
		MessageManager::RegisterMessageManager(&messages);
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator());
		emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg = emu->GetSettings()->GetGameboyConfig();
		bool gbcFixture = string(filename).find(".gbc") != string::npos;
		bool scrolledGbFixture = string(filename).find("scroll.gb") != string::npos;
		bool windowGbFixture = string(filename).find("window") != string::npos && string(filename).find(".gb") != string::npos;
		bool distinctGbUnderlay = windowGbFixture && string(filename).find("underlay") != string::npos;
		bool windowObjectFixture = windowGbFixture && string(filename).find("window-object") != string::npos;
		bool behindWindowObject = windowObjectFixture && string(filename).find("behind") != string::npos;
		bool nativeOverlapFixture = gbcFixture && string(filename).find("native-overlap") != string::npos;
		cfg.Model = gbcFixture ? GameboyModel::GameboyColor : GameboyModel::Gameboy;
		cfg.RamPowerOnState = RamState::AllZeros;
		cfg.BlendFrames = false;
		cfg.GbcAdjustColors = false;
		emu->GetSettings()->GetSmsConfig().RamPowerOnState = RamState::AllZeros;
		bool legacySmsFixture = string(filename).find("legacy-") != string::npos;
		bool largeLegacySprite = legacySmsFixture && string(filename).find("large") != string::npos;
		bool zoomFixture = string(filename).find("zoom") != string::npos;
		bool sms1Fixture = string(filename).find("sms1") != string::npos;
		bool leftFixture = string(filename).find("left") != string::npos;
		bool maskFixture = string(filename).find("mask") != string::npos;
		bool scrolledSmsFixture = string(filename).find("scroll.sms") != string::npos;
		bool lockedSmsFixture = string(filename).find("locks-scroll.sms") != string::npos;
		emu->GetSettings()->GetSmsConfig().Revision = sms1Fixture ? SmsRevision::Sms1 : SmsRevision::Sms2;
		uint32_t flags = 0;
		MetaSpriteTestRenderer renderer;
		auto waitFor = [&](std::function<bool()> condition) {
			for(int attempt = 0; attempt < 500; attempt++) {
				{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			return false;
		};
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			auto gameboy = dynamic_cast<Gameboy*>(emu->GetConsole().get());
			auto sms = dynamic_cast<SmsConsole*>(emu->GetConsole().get());
			if(gameboy || sms) {
				if(gameboy) { gameboy->GetPpu()->SetMetaSpriteCapture(true); }
				if(sms) { sms->GetVdp()->SetMetaSpriteCapture(true); }
				uint32_t nativeWidth = sms ? 256 : 160, nativeHeight = sms ? 240 : 144;
				int spriteY = sms ? 32 : windowObjectFixture ? 30 : 8;
				int spriteX = sms && leftFixture ? -4 : sms && maskFixture ? 4 : windowObjectFixture ? 50 : 8;
				int visibleX = maskFixture ? 8 : std::max(0, spriteX);
				int spriteWidth = legacySmsFixture ? (largeLegacySprite ? 16 : 8) * (zoomFixture ? 2 : 1) : sms && zoomFixture && !sms1Fixture ? 16 : 8;
				int spriteHeight = legacySmsFixture ? spriteWidth : sms && zoomFixture ? 16 : 8;
				MetaSpriteGraphic source;
				uint32_t originalBackground = 0;
				uint32_t retainedBackgroundArgb = 0;
				uint16_t retainedBackgroundColor = 0;
				uint32_t occludedPixel = 0;
				uint32_t maskedPixel = 0;
				bool captured = waitFor([&]() {
					if(!renderer.Capture || renderer.Width != nativeWidth || renderer.Height != nativeHeight) { return false; }
					auto candidates = sms ? MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture) : MetaSpriteCompositor::BuildGameboyCandidates(*renderer.Capture);
					for(auto& candidate : candidates) {
						if(candidate.X == spriteX && candidate.Y == spriteY && candidate.Width == spriteWidth && candidate.Height == spriteHeight) {
							// The ROM's source pattern is solid red, including its offscreen portion.
							if(sms) {
								bool holes = string(filename).find("holes") != string::npos, valid = true;
								for(size_t i = 0; i < candidate.Pixels.size(); i++) {
									bool hole = holes && i % spriteWidth >= spriteWidth - spriteWidth / 8;
									valid &= candidate.Pixels[i] == (hole ? 0 : 0xFFFF0000u);
									if(hole) {
										const auto& pixel = renderer.Capture->Pixels[(candidate.Y + i / spriteWidth) * nativeWidth + candidate.X + i % spriteWidth];
										valid &= pixel.VisibleSprite == -1 && candidate.HardwareSprite >= 0 && (pixel.ReplacementEligibleSprites & (uint64_t(1) << candidate.HardwareSprite));
									}
								}
								if(!valid) { continue; }
							}
							if(sms) {
								bool validBackground = true;
								for(int y = 24; y < 216; y++) {
									bool horizontalLock = lockedSmsFixture && y - 24 < 16;
									for(int x = maskFixture ? 8 : scrolledSmsFixture && !horizontalLock ? 5 : 0; x < 256; x++) {
										const auto& sample = renderer.Capture->Pixels[y * 256 + x].BackgroundSample;
										bool verticalLock = lockedSmsFixture && x >= (horizontalLock ? 192 : 197);
										int expectedY = y - 24 + (scrolledSmsFixture && !verticalLock ? 218 : 0);
										if(expectedY >= 224) { expectedY -= 224; }
										validBackground &= sample.Valid && sample.Layer == 0 && sample.X == ((x - (scrolledSmsFixture && !horizontalLock ? 5 : 0)) & 255) && sample.Y == expectedY;
									}
								}
								if(!validBackground) { continue; }
							}
							if(gbcFixture) {
								bool holeFixture = string(filename).find("holes") != string::npos, validGraphic = true;
								for(size_t i = 0; i < candidate.Pixels.size(); i++) { validGraphic &= candidate.Pixels[i] == (holeFixture && i % 8 == 7 ? 0 : 0xFFFF0000u); }
								if(!validGraphic || renderer.Pixels[0] != (distinctGbUnderlay ? 0xFFFFFF00 : 0xFF0000FF)) { continue; }
							}
							if(gameboy) {
								if(string(filename).find("holes") != string::npos) {
									bool validHoles = true;
									for(int y = 0; y < 8; y++) {
										const auto& hole = renderer.Capture->Pixels[(y + 8) * nativeWidth + 15];
										validHoles &= candidate.Pixels[y * 8 + 7] == 0 && hole.VisibleSprite == -1 && (hole.ReplacementEligibleSprites & 1);
									}
									if(!validHoles) { continue; }
								}
								bool validBackground = true;
								for(uint32_t y = 0; y < nativeHeight; y++) {
									for(uint32_t x = 0; x < nativeWidth; x++) {
										const auto& sample = renderer.Capture->Pixels[y * nativeWidth + x].BackgroundSample;
										bool window = windowGbFixture && x >= 40 && y >= 24;
										int expectedX = window ? (int)x - 40 : (int)((x + (scrolledGbFixture ? 253 : 0)) & 255);
										int expectedY = window ? (int)y - 24 : (int)((y + (scrolledGbFixture ? 250 : 0)) & 255);
										validBackground &= sample.Valid && sample.Layer == (window ? 1 : 0) && sample.X == expectedX && sample.Y == expectedY;
									}
								}
								if(!validBackground) { continue; }
							}
							source = candidate; originalBackground = renderer.Pixels[0];
							retainedBackgroundArgb = renderer.Pixels[0]; retainedBackgroundColor = renderer.Capture->Pixels[0].BackgroundColor;
							if(sms) { occludedPixel = renderer.Pixels[spriteY * nativeWidth + 24]; }
							if(maskFixture) { maskedPixel = renderer.Pixels[spriteY * nativeWidth + 4]; }
							return true;
						}
					}
					return false;
				});
				if(captured) {
					flags |= 1;
					source.X = 0; source.Y = 0; source.HardwareSprite = -1;
					MetaSpriteReplacement group;
					group.Id = 1; group.ArtworkScale = 2; group.ArtworkWidth = spriteWidth * 2; group.ArtworkHeight = spriteHeight * 2;
					group.Members.push_back(source);
					group.Artwork.assign(group.ArtworkWidth * group.ArtworkHeight, 0xFF00FF00);
					bool smsBackgroundFixture = sms && !zoomFixture && !leftFixture && !maskFixture;
					if(smsBackgroundFixture) {
						MetaBackgroundReplacement bg;
						bg.Id = 1; bg.Width = bg.Height = 2; bg.Scale = 2; bg.WrapX = bg.WrapY = true; bg.Artwork.assign(4, 0xFFFFFFFF);
						emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg});
						occludedPixel = 0xFFFFFFFF;
					}
					if(windowGbFixture) {
						MetaBackgroundReplacement bg;
						bg.Id = 1; bg.Width = bg.Height = 2; bg.Scale = 2; bg.WrapX = bg.WrapY = true;
						bg.Artwork.assign(4, 0xFFFFFFFF);
						MetaBackgroundReplacement window = bg; window.Id = 2; window.Layer = 1;
						emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg, window});
						originalBackground = 0xFFFFFFFF;
					}
					uint32_t partialBackground = 0xFF000000;
					uint32_t alphaBackground = smsBackgroundFixture ? 0xFFFFFFFF : originalBackground;
					if(nativeOverlapFixture) { alphaBackground = 0xFFFFFF00; }
					if(gameboy || smsBackgroundFixture) {
						group.Artwork[0] = 0;
						group.Artwork[1] = 0x8000FF00;
						for(int shift : {0, 8, 16}) {
							uint32_t foreground = shift == 8 ? 255 : 0;
							partialBackground |= ((foreground * 128 + ((alphaBackground >> shift) & 255) * 127 + 127) / 255) << shift;
						}
					}
					group.MemoryConditions = {{0, 0, 0}}; emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Height == nativeHeight * 2; })) { flags |= 2; }
					if(waitFor([&]() {
						if(renderer.Width != nativeWidth * 2 || renderer.Pixels[spriteY * 2 * renderer.Width + visibleX * 2] != (gameboy || smsBackgroundFixture ? alphaBackground : 0xFF00FF00)) { return false; }
						if(gameboy || smsBackgroundFixture) {
							for(int y = 0; y < spriteHeight * 2; y++) {
								for(int x = 0; x < spriteWidth * 2; x++) {
									uint32_t expected = y == 0 && x == 0 ? alphaBackground : y == 0 && x == 1 ? partialBackground : 0xFF00FF00;
									if(behindWindowObject) { expected = alphaBackground; }
									if(renderer.Pixels[(spriteY * 2 + y) * renderer.Width + spriteX * 2 + x] != expected) { return false; }
								}
							}
						}
						return true;
					})) { flags |= 4; }
					{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.Width == nativeWidth * 2 && renderer.Pixels[0] == originalBackground) { flags |= 8; } }
					int priorityX = scrolledSmsFixture && !lockedSmsFixture ? 29 : 24, priorityY = spriteY + (scrolledSmsFixture ? 6 : 0);
					if(sms && !legacySmsFixture && waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Pixels[priorityY * 2 * renderer.Width + priorityX * 2] == occludedPixel
						&& occludedPixel != 0xFF00FF00 && renderer.Capture && renderer.Capture->Pixels[priorityY * nativeWidth + priorityX].VisibleSprite == -1; })) { flags |= 32; }
					if(maskFixture && waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Pixels[spriteY * 2 * renderer.Width + 8] == maskedPixel
						&& maskedPixel != 0xFF00FF00 && renderer.Capture && renderer.Capture->Pixels[spriteY * nativeWidth + 4].VisibleSprite == -1; })) { flags |= 64; }
					if(!VerifyExpandedCanvas(emu.get(), renderer, group, spriteX, spriteY, 0, source.ExpansionClass, 0, 0, smsBackgroundFixture || behindWindowObject)) { flags |= 128; }
					if(windowGbFixture) {
						MetaBackgroundReplacement window; window.Id = 2; window.Layer = 1;
						window.Width = window.Height = 2; window.Scale = 2; window.WrapX = window.WrapY = true;
						window.HideOriginalLayer = true; window.Artwork.assign(4, 0);
						emu->GetVideoDecoder()->SetMetaReplacements({group}, {window});
						if(!waitFor([&]() {
							if(renderer.Width != 320 || !renderer.Capture || renderer.Capture->BackgroundUnderlays.size() != 160 * 144) { return false; }
							const auto& lower = renderer.Capture->BackgroundUnderlays[48 * 160 + 80];
							return lower.Sample.Valid && lower.Sample.Layer == 0 && lower.Sample.X == ((80 + 253) & 255)
								&& lower.Sample.Y == ((48 + 250) & 255) && lower.Color == retainedBackgroundColor
								&& lower.SpriteEligibilityCaptured && renderer.Pixels[96 * 320 + 160] == retainedBackgroundArgb
								&& (!windowObjectFixture || renderer.Pixels[62 * 320 + 102] == 0xFF00FF00);
						})) { flags |= 128; }
						window.Artwork.assign(4, 0x8000FF00); emu->GetVideoDecoder()->SetMetaReplacements({group}, {window});
						uint32_t blended = 0xFF000000;
						for(int shift : {0, 8, 16}) { blended |= (((shift == 8 ? 255 : 0) * 128 + ((retainedBackgroundArgb >> shift) & 255) * 127 + 127) / 255) << shift; }
						if(!waitFor([&]() { return renderer.Width == 320 && renderer.Pixels[96 * 320 + 160] == blended
							&& (!windowObjectFixture || renderer.Pixels[62 * 320 + 102] == 0xFF00FF00); })) { flags |= 128; }
						if(windowObjectFixture) {
							emu->GetVideoDecoder()->SetMetaReplacements({}, {window});
							if(!waitFor([&]() { return renderer.Width == 320 && renderer.Pixels[62 * 320 + 102] == (behindWindowObject ? 0xFF7F8000 : 0xFFFF0000)
								&& renderer.Pixels[96 * 320 + 160] == blended; })) { flags |= 128; }
							window.Artwork.assign(4, 0); emu->GetVideoDecoder()->SetMetaReplacements({}, {window});
							if(!waitFor([&]() { return renderer.Width == 320 && renderer.Pixels[62 * 320 + 102] == 0xFFFF0000
								&& renderer.Pixels[96 * 320 + 160] == retainedBackgroundArgb; })) { flags |= 128; }
							emu->GetVideoDecoder()->SetMetaReplacements({group}, {});
							if(!waitFor([&]() { return renderer.Width == 320 && renderer.Capture
								&& renderer.Capture->MemorySnapshot.size() == 1 && renderer.Capture->MemorySnapshot.count(0)
								&& renderer.Pixels[62 * 320 + 102] == 0xFF00FF00; })) { flags |= 128; }
						}
						if(!(flags & 128)) { std::cout << "PASS: GB/GBC hidden window retains scrolled native BG and blends PNG alpha above it." << std::endl; }
						emu->GetVideoDecoder()->SetMetaBackgroundLayers({});
					}
					if(!VerifyLiveMemoryConditions(emu.get(), renderer, group)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					emu->GetVideoDecoder()->SetMetaBackgroundLayers({});
					if(waitFor([&]() { return renderer.Width == nativeWidth && renderer.Height == nativeHeight && renderer.Pixels[spriteY * nativeWidth + visibleX] != 0xFF00FF00; })) { flags |= 16; }
				}
				else {
					std::lock_guard<std::mutex> lock(renderer.Mutex);
					std::cerr << "Capture diagnostic: emulated frames=" << emu->GetFrameCount() << ", collected=" << renderer.FrameNumber
						<< ", size=" << renderer.Width << "x" << renderer.Height << ", capture=" << (bool)renderer.Capture;
					if(renderer.Capture) {
						std::cerr << ", rows=" << renderer.Capture->Rows.size() << ", decoded rows=" << renderer.Capture->DecodedRows.size();
						for(const auto& row : renderer.Capture->DecodedRows) { std::cerr << " [id=" << row.Sprite << " x=" << row.X << " y=" << row.Y << " row=" << row.Row << "]"; }
						for(const auto& candidate : sms ? MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture) : MetaSpriteCompositor::BuildGameboyCandidates(*renderer.Capture)) {
							std::cerr << ", sprite " << candidate.HardwareSprite << " at " << candidate.X << "," << candidate.Y;
						}
					}
					std::cerr << std::endl;
					if(sms) { auto& state = sms->GetVdp()->GetState(); std::cerr << "SMS mode4=" << state.UseMode4 << ", display=" << state.RenderingEnabled << ", large=" << state.UseLargeSprites << ", zoom=" << state.EnableDoubleSpriteSize << ", SAT=" << state.SpriteTableAddress << std::endl; }
				}
			}
			emu->Stop(false);
			emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		else { std::cerr << "Gameplay ROM load failed: " << MessageManager::GetLog() << std::endl; }
		emu->Release();
		MessageManager::UnregisterMessageManager(&messages);
		return flags;
	}
	DllExport RomTestResult __stdcall RunRecordedTest(const char* filename, bool inBackground)
	{
		if(inBackground) {
			unique_ptr<Emulator> emu(new Emulator());
			emu->Initialize(false);
			emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
			shared_ptr<RecordedRomTest> romTest(new RecordedRomTest(emu.get(), true));
			RomTestResult result = romTest->Run(filename);
			emu->Release();
			return result;
		} else {
			shared_ptr<RecordedRomTest> romTest(new RecordedRomTest(_emu.get(), false));
			return romTest->Run(filename);
		}
	}

	DllExport uint64_t __stdcall RunTest(char* filename, uint32_t address, MemoryType memType)
	{
		unique_ptr<Emulator> emu(new Emulator());
		emu->Initialize();
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->GetGameboyConfig().Model = GameboyModel::Gameboy;
		emu->GetSettings()->GetGameboyConfig().RamPowerOnState = RamState::AllZeros;
		emu->LoadRom((VirtualFile)filename, VirtualFile());
		emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);

		while(emu->GetFrameCount() < 500) {
			std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(10));
		}

		ConsoleMemoryInfo memInfo = emu->GetMemory(memType);
		uint8_t* memBuffer = (uint8_t*)memInfo.Memory;
		uint64_t result = memBuffer[address];
		for(int i = 1; i < 8; i++) {
			if(address + i < memInfo.Size) {
				result |= ((uint64_t)memBuffer[address + i] << (8 * i));
			} else {
				break;
			}
		}

		emu->Stop(false);
		emu->Release();

		return result;
	}

	DllExport void __stdcall RomTestRecord(char* filename, bool reset)
	{
		_recordedRomTest.reset(new RecordedRomTest(_emu.get(), false));
		_recordedRomTest->Record(filename, reset);
	}

	DllExport void __stdcall RomTestStop()
	{
		if(_recordedRomTest) {
			_recordedRomTest->Stop();
			_recordedRomTest.reset();
		}
	}

	DllExport bool __stdcall RomTestRecording()
	{
		return _recordedRomTest != nullptr;
	}

	DllExport bool __stdcall RunCiTests(string testFolder, bool enableDebugger)
	{
		std::replace(testFolder.begin(), testFolder.end(), '\\', '/');
		if(!StringUtilities::EndsWith(testFolder, "/")) {
			testFolder += "/";
		}

		//GBA tests can't run without the GBA BIOS, skip all of them
		vector<string> foldersToSkip = { "GBA/" };

		unordered_set<string> testsToSkip = {
			//These GB tests fail because the test runner can't use the official GB boot rom
			"GB/GBEmulatorShootout/hacktix/bully_gbc.mtp",
			"GB/GBEmulatorShootout/hacktix/bully.mtp",
			"GB/GBEmulatorShootout/mooneye/acceptance/boot_div-dmgABCmgb.mtp",
			"GB/GBEmulatorShootout/mooneye/acceptance/boot_hwio-dmgABCmgb.mtp",
			"GB/GBEmulatorShootout/mooneye/acceptance/serial/boot_sclk_align-dmgABCmgb.mtp",
			"GB/GBEmulatorShootout/mooneye/misc/boot_div-cgbABCDE.mtp",

			//SGB tests can't run properly without the SGB ROM
			"SNES/sgb_packet_test.mtp",
			"SNES/sgb-ext-test.mtp",
			"SNES/sgb-mlt-test.mtp"
		};

		vector<string> tests = FolderUtilities::GetFilesInFolder(testFolder, { ".mtp" }, true, 5);

		vector<string> testsToRun;

		for(string& test : tests) {
			string testPath = test.substr(testFolder.size());
			std::replace(testPath.begin(), testPath.end(), '\\', '/');

			bool include = true;
			for(string& folderToSkip : foldersToSkip) {
				if(StringUtilities::StartsWith(testPath, folderToSkip.c_str())) {
					include = false;
					break;
				}
			}

			if(testsToSkip.find(testPath) != testsToSkip.end()) {
				include = false;
			}

			if(include) {
				testsToRun.push_back(test);
			}
		}

		FolderUtilities::SetHomeFolder(FolderUtilities::CombinePath(testFolder, "MesenHomeFolder"));

		std::cout << "== CI test mode ==\n";
		int threadCount = std::thread::hardware_concurrency();
		int skipCount = (int)(tests.size() - testsToRun.size());
		std::cout << "Running on " << threadCount << " threads\n";
		std::cout << "Test count: " << tests.size() << "\n";
		std::cout << "Tests to skip: " << skipCount << "\n\n";

		std::atomic<int> testNumber = 0;
		std::atomic<int> failCount = 0;
		std::atomic<int> passCount = 0;
		std::atomic<int> progress = 0;

		vector<thread> threads(threadCount);
		for(int i = 0; i < threadCount; i++) {
			threads[i] = std::thread([&]() {
				while(true) {
					int nextTest = testNumber++;
					if(nextTest >= testsToRun.size()) {
						break;
					}

					RomTestResult result = RunRecordedTest(testsToRun[nextTest].c_str(), true);
					if(result.State == RomTestState::Failed) {
						failCount++;
						string testPath = testsToRun[nextTest].substr(testFolder.size());
						std::cout << ("\n== FAILED: " + testPath + "\n");
					} else {
						passCount++;
					}

					int newProgress = (failCount + passCount) * 100 / (uint32_t)testsToRun.size();
					if(newProgress > progress) {
						std::cout << ("\rRunning... (" + std::to_string(newProgress) + "%)");
						progress = newProgress;
					}
				}
			});
		}

		for(int i = 0; i < threadCount; i++) {
			threads[i].join();
		}

		std::cout << "\n\n===================\n";
		std::cout << "Passed: " << passCount << "\n";
		std::cout << "Failed: " << failCount << "\n";
		std::cout << "Skipped: " << skipCount << "\n";
		std::cout << "===================\n";

		return failCount == 0;
	}
}


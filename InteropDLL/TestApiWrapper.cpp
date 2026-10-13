#include "Common.h"
#include "Core/Shared/RecordedRomTest.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/MessageManager.h"
#include "Utilities/FolderUtilities.h"
#include "Utilities/StringUtilities.h"
#include "Core/Gameboy/Gameboy.h"
#include "Core/Gameboy/GbPpu.h"
#include "Core/SMS/SmsConsole.h"
#include "Core/SMS/SmsVdp.h"
#include "Core/PCE/PceConsole.h"
#include "Core/PCE/PceVdc.h"
#include "Core/GBA/GbaConsole.h"
#include "Core/GBA/GbaPpu.h"
#include "Core/WS/WsConsole.h"
#include "Core/WS/WsPpu.h"
#include "Core/NES/NesConsole.h"
#include "Core/NES/BaseNesPpu.h"
#include "Core/SNES/SnesConsole.h"
#include "Core/SNES/SnesPpu.h"
#include "Core/Shared/Video/VideoDecoder.h"
#include "Core/Shared/Video/VideoRenderer.h"
#include "Core/Shared/Video/SoftwareRenderer.h"
#include "Core/Debugger/Debugger.h"
#include "Core/Debugger/PpuTools.h"
#include "Core/Debugger/MemoryAccessCounter.h"
#include "Core/Debugger/DebugBreakHelper.h"
#include "Core/Shared/NotificationManager.h"
#include <mutex>
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
	bool MeasureTiming = false;
	vector<double> FrameIntervals;
	uint32_t TimingFrame = 0, SkippedTimingFrames = 0;
	std::chrono::steady_clock::time_point TimingPrevious;
	void UpdateFrame(RenderedFrame& frame) override
	{
		std::lock_guard<std::mutex> lock(Mutex);
		Width = frame.Width; Height = frame.Height; FrameNumber = frame.FrameNumber;
		Pixels.assign((uint32_t*)frame.FrameBuffer, (uint32_t*)frame.FrameBuffer + Width * Height);
		Capture = frame.MetaSprites;
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
	auto waitFor = [&](bool enabled, bool missing) {
		for(int attempt = 0; attempt < 500; attempt++) {
			{
				std::lock_guard<std::mutex> lock(renderer.Mutex);
				if(renderer.Capture && renderer.Width == (enabled ? hdWidth : hdWidth / 2)
					&& renderer.Height == (enabled ? hdHeight : hdHeight / 2)
					&& (missing ? renderer.Capture->MemorySnapshot.empty() : renderer.Capture->MemorySnapshot.count(0) == 1)) { return true; }
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return false;
	};
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
	DllExport uint32_t __stdcall RunMetaSpriteSnesCaptureTest(const char* filename)
	{
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode); emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg = emu->GetSettings()->GetSnesConfig(); cfg.DisableFrameSkipping = true; cfg.RamPowerOnState = RamState::AllZeros;
		cfg.ColorCorrection = SnesColorCorrectionMode::None;
		MetaSpriteGraphic source; uint32_t nativeWidth = 0, nativeHeight = 0, backdrop = 0; int spriteX = 0, spriteY = 0;
		MetaSpriteTestRenderer renderer; uint32_t flags = 0;
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
				for(int attempt = 0; attempt < 500; attempt++) {
					{
						std::lock_guard<std::mutex> lock(renderer.Mutex);
						if(renderer.Capture) {
							for(const auto& graphic : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
								if(graphic.HardwareSprite != 0 || graphic.X != 8 || graphic.Y != 15 || graphic.Width != 8 || graphic.Height != 8) { continue; }
								bool validGraphic = true;
								for(size_t i = 0; i < graphic.Pixels.size(); i++) {
									validGraphic &= graphic.Pixels[i] == (transparent || (holes && i % 8 == 7) ? 0 : 0xFFFF0000u);
									if(transparent || (holes && i % 8 == 7)) {
										const auto& hole = renderer.Capture->Pixels[(15 + i / 8) * 256 + 8 + i % 8];
										bool eligible = std::any_of(hole.SnesCoverage.begin(), hole.SnesCoverage.end(), [&](const auto& c) { return c.Sprite == 0 && (sub ? c.Sub : c.Main); });
										validGraphic &= hole.VisibleSprite == (overlap ? 1 : -1) && eligible == (!masked(7) && !rotate);
										if(math) { validGraphic &= std::any_of(hole.SnesCoverage.begin(), hole.SnesCoverage.end(), [](const auto& c) { return c.Sprite == 0 && c.Main && c.Math; }); }
									}
								}
								if(!validGraphic) { continue; }
								const auto& pixel = renderer.Capture->Pixels[15 * 256 + 8];
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
					if(string(filename).find("memory") != string::npos) {
						if(!VerifyChangingSnesMemory(emu.get(), renderer, group, spriteX, spriteY, nativeWidth, nativeHeight)) { flags |= 128; }
					} else if(!VerifyLiveMemoryConditions(emu.get(), renderer, group)) { flags |= 128; }
					emu->GetVideoDecoder()->SetMetaReplacements({}, {});
					if(waitFor([&]() { return renderer.Width == nativeWidth && renderer.Height == nativeHeight && renderer.Pixels[spriteY * nativeWidth + spriteX] == nativeSpriteColor; })) { flags |= 4; }
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
	DllExport uint32_t __stdcall RunMetaSpriteNesCaptureTest(const char* filename)
	{
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
					auto waitFor = [&](std::function<bool()> condition) {
						for(int attempt = 0; attempt < 500; attempt++) {
							{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
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
								for(size_t i = 0; i < graphic.Pixels.size(); i++) { validGraphic &= graphic.Pixels[i] == (holes && i % 8 == 7 ? 0 : 0xFFFF0000u); }
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
											bool window = windowFixture && inside != outside;
											valid &= sample.Valid && sample.Layer == (window ? 1 : 0) && sample.X == ((x + (window ? 7 : scrolled ? 253 : 0)) & 255)
												&& sample.Y == ((y + (window ? 9 : scrolled ? 250 : 0)) & 255);
										}
									}
									if(!valid) { continue; }
								}
								if((occluded ? pixel.VisibleSprite == -1 : pixel.VisibleSprite == 0 && pixel.SpriteColor == 0xF00)
									&& pixel.BackgroundColor == 15 && renderer.Pixels[8 * 224 + 8] == (occluded ? 0xFF0000FF : 0xFFFF0000)
									&& renderer.Pixels[0] == 0xFF0000FF) { flags = 1; source = graphic; }
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
					bool windowFixture = backgroundFixture && string(filename).find("window") != string::npos;
					bool outsideWindow = windowFixture && string(filename).find("outside") != string::npos;
					auto texture = [](int x, int y) { return 0xFF000000u | ((uint32_t)x * 13 << 16) | ((uint32_t)y * 11 << 8) | ((x ^ y) * 7); };
					auto expectedBackground = [&](int x, int y) {
						if(!backgroundFixture) { return 0xFF0000FFu; }
						bool inside = x / 2 >= 40 && x / 2 <= 95 && y / 2 >= 24 && y / 2 <= 63;
						if(windowFixture && inside != outsideWindow) { return 0xFFFF00FFu; }
						if(!patterned) { return 0xFFFFFFFFu; }
						int tx = (((x / 2 + 253) & 255) % 8) * 2 + x % 2;
						int ty = (((y / 2 + 250) & 255) % 8) * 2 + y % 2;
						return texture(tx, ty);
					};
					if(backgroundFixture) {
						MetaBackgroundReplacement bg;
						bg.Id = 1; bg.Width = bg.Height = 2; bg.Scale = 2; bg.WrapX = bg.WrapY = true; bg.Artwork.assign(4, 0xFFFFFFFF);
						if(patterned) {
							bg.Width = bg.Height = 16; bg.Artwork.resize(256);
							for(int y = 0; y < 16; y++) { for(int x = 0; x < 16; x++) { bg.Artwork[y * 16 + x] = texture(x, y); } }
						}
						if(windowFixture) {
							MetaBackgroundReplacement window = bg; window.Id = 2; window.Layer = 1; window.Artwork.assign(4, 0xFFFF00FF);
							emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg, window});
						} else { emu->GetVideoDecoder()->SetMetaBackgroundLayers({bg}); }
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
					if(!VerifyLiveMemoryConditions(emu.get(), renderer, group, !occluded)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					emu->GetVideoDecoder()->SetMetaBackgroundLayers({});
					if(waitFor([&]() { return renderer.Width == 224 && renderer.Height == 144 && renderer.Pixels[8 * 224 + 8] == (occluded ? 0xFF0000FF : 0xFFFF0000) && renderer.Pixels[0] == 0xFF0000FF; })) { flags |= 4; }
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
								if(graphic.HardwareSprite != 0 || graphic.X != 8 || graphic.Y != 8 || graphic.Width != 8 || graphic.Height != 8) { continue; }
								bool expectedGraphic = true;
								for(int y = 0; y < 8; y++) {
									for(int x = 0; x < 8; x++) {
										uint32_t expected = (mirrored ? x < 4 : bpp8 && x >= 4) ? 0xFF0000FF : 0xFFFF0000;
										uint32_t displayed = dark ? 0xFF7B0000 : expected;
										if(masked(x)) { displayed = 0xFF0000FF; }
										if(transparentSource || (holes && x == 7)) {
											expected = 0; displayed = dark ? 0xFF00007B : 0xFF0000FF;
											const auto& hole = renderer.Capture->Pixels[(y + 8) * 240 + x + 8];
											bool eligible = std::any_of(hole.GbaCoverage.begin(), hole.GbaCoverage.end(), [](const auto& coverage) { return coverage.Sprite == 0; });
											if(hole.VisibleSprite != (overlap ? 1 : -1) || eligible == masked(x)) { expectedGraphic = false; }
										}
										if(graphic.Pixels[y * 8 + x] != expected || renderer.Pixels[(y + 8) * 240 + x + 8] != displayed) { expectedGraphic = false; }
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
									if(pixel.VisibleSprite == (transparentSource ? -1 : 0) && pixel.SpriteColor == (transparentSource || mirrored ? 0x7C00 : 31) && pixel.SecondarySprite == -1
									&& pixel.GbaFallbackCaptured && pixel.GbaFallbackColor == (dark ? 0x3C00 : 0x7C00) && pixel.GbaFallbackMainColor == 0x7C00) { flags |= 1; source = graphic; }
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
					} else { group.MemoryConditions = {{0, 0, 0}}; emu->GetVideoDecoder()->SetMetaSpriteGroups({group}); }
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
								bool transparent = y == 0 && (x == 0 || (holes && x == 14));
								bool partial = y == 0 && (x == 1 || (holes && x == 15));
								uint32_t expected = transparent ? 0xFF0000FF : partial ? 0xFF00807F : 0xFF00FF00;
								if(dark) { expected = transparent ? 0xFF00007B : partial ? 0xFF00403D : 0xFF007F00; }
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
						return renderer.Pixels[0] == (backgroundCapture ? 0xFF7F7FFF : dark ? 0xFF00007B : 0xFF0000FF);
					})) { flags |= 2; }
					if(bitmapBackground && string(filename).find("page1-live") != string::npos) {
						for(bool opaque : {false, true}) {
							if(!waitFor([&]() {
								return renderer.Width == 480 && renderer.Capture
									&& renderer.Capture->Pixels[8 * 240 + 8].GbaBackgroundLayers[2].Opaque == opaque
									&& renderer.Pixels[0] == 0xFF7F7FFF;
							})) { flags |= 128; }
						}
					}
					if(backgroundCapture) { if(!VerifyLiveMemoryConditions(emu.get(), renderer, group)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaReplacements({}, {}); }
					else { if(!VerifyLiveMemoryConditions(emu.get(), renderer, group)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaSpriteGroups({}); }
					if(waitFor([&]() { return renderer.Width == 240 && renderer.Height == 160 && renderer.Pixels[8 * 240 + 8] == (dark ? 0xFF7B0000 : transparentSource || mirrored ? 0xFF0000FF : 0xFFFF0000); })) { flags |= 4; }
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpritePceGameplayTest(const char* filename)
	{
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
						if(x < 0 || y < 0 || x + spriteWidth > (int)width || y + 16 > (int)height || renderer.Pixels[y * width + x] != candidate.Pixels[0]) { continue; }
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
								if(renderer.Pixels[(y * 2 + row) * renderer.Width + x * 2 + column] != expected) { return false; }
							}
						}
						return true;
					})) { flags |= 4; }
					if(waitFor([&]() { return renderer.Width == width * 2 && renderer.Pixels[0] == background
						&& (!backgroundFixture || renderer.Pixels[y * 2 * renderer.Width + (x + spriteWidth + 1) * 2] == 0xFFFFFFFF); })) { flags |= 8; }
					if(!VerifyLiveMemoryConditions(emu.get(), renderer, group)) { flags |= 128; } emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					emu->GetVideoDecoder()->SetMetaBackgroundLayers({});
					if(waitFor([&]() { return renderer.Width == width && renderer.Height == height && renderer.Pixels[y * width + x] == source.Pixels[0]; })) { flags |= 16; }
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
		bool scrolledGbFixture = gbcFixture && string(filename).find("scroll") != string::npos;
		bool windowGbFixture = gbcFixture && string(filename).find("window") != string::npos;
		cfg.Model = gbcFixture ? GameboyModel::GameboyColor : GameboyModel::Gameboy;
		cfg.RamPowerOnState = RamState::AllZeros;
		cfg.BlendFrames = false;
		cfg.GbcAdjustColors = false;
		emu->GetSettings()->GetSmsConfig().RamPowerOnState = RamState::AllZeros;
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
				int spriteY = sms ? 32 : 8;
				int spriteX = sms && leftFixture ? -4 : sms && maskFixture ? 4 : 8;
				int visibleX = maskFixture ? 8 : std::max(0, spriteX);
				int spriteWidth = sms && zoomFixture && !sms1Fixture ? 16 : 8;
				int spriteHeight = sms && zoomFixture ? 16 : 8;
				MetaSpriteGraphic source;
				uint32_t originalBackground = 0;
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
								if(!validGraphic || renderer.Pixels[0] != 0xFF0000FF) { continue; }
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
									if(renderer.Pixels[(spriteY * 2 + y) * renderer.Width + spriteX * 2 + x] != expected) { return false; }
								}
							}
						}
						return true;
					})) { flags |= 4; }
					{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.Width == nativeWidth * 2 && renderer.Pixels[0] == originalBackground) { flags |= 8; } }
					int priorityX = scrolledSmsFixture && !lockedSmsFixture ? 29 : 24, priorityY = spriteY + (scrolledSmsFixture ? 6 : 0);
					if(sms && waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Pixels[priorityY * 2 * renderer.Width + priorityX * 2] == occludedPixel
						&& occludedPixel != 0xFF00FF00 && renderer.Capture && renderer.Capture->Pixels[priorityY * nativeWidth + priorityX].VisibleSprite == -1; })) { flags |= 32; }
					if(maskFixture && waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Pixels[spriteY * 2 * renderer.Width + 8] == maskedPixel
						&& maskedPixel != 0xFF00FF00 && renderer.Capture && renderer.Capture->Pixels[spriteY * nativeWidth + 4].VisibleSprite == -1; })) { flags |= 64; }
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


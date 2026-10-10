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
#include <mutex>
#include <iostream>

class MetaSpriteTestRenderer final : public IRenderingDevice
{
public:
	std::mutex Mutex;
	uint32_t Width = 0, Height = 0, FrameNumber = 0;
	vector<uint32_t> Pixels;
	shared_ptr<const MetaSpriteFrame> Capture;
	void UpdateFrame(RenderedFrame& frame) override
	{
		std::lock_guard<std::mutex> lock(Mutex);
		Width = frame.Width; Height = frame.Height; FrameNumber = frame.FrameNumber;
		Pixels.assign((uint32_t*)frame.FrameBuffer, (uint32_t*)frame.FrameBuffer + Width * Height);
		Capture = frame.MetaSprites;
	}
	void ClearFrame() override {}
	void Render(RenderSurfaceInfo&, RenderSurfaceInfo&) override {}
	void Reset() override {}
	void SetFullscreenMode(FullscreenSettings) override {}
};

class MetaSpriteTestMessages final : public IMessageManager
{
public:
	void DisplayMessage(string title, string message) override { std::cerr << title << ": " << message << std::endl; }
};

extern unique_ptr<Emulator> _emu;
shared_ptr<RecordedRomTest> _recordedRomTest;

extern "C"
{
	DllExport uint32_t __stdcall RunMetaSpriteSnesCaptureTest(const char* filename)
	{
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode); emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg = emu->GetSettings()->GetSnesConfig(); cfg.DisableFrameSkipping = true; cfg.RamPowerOnState = RamState::AllZeros;
		cfg.ColorCorrection = SnesColorCorrectionMode::None;
		MetaSpriteGraphic source; uint32_t nativeWidth = 0, nativeHeight = 0, backdrop = 0; int spriteX = 0, spriteY = 0;
		MetaSpriteTestRenderer renderer; uint32_t flags = 0;
		if(emu->LoadRom((VirtualFile)filename, VirtualFile())) {
			emu->GetVideoRenderer()->RegisterRenderingDevice(&renderer);
			if(auto snes = dynamic_cast<SnesConsole*>(emu->GetConsole().get())) {
				snes->GetPpu()->SetMetaSpriteCapture(true);
				for(int attempt = 0; attempt < 500; attempt++) {
					{
						std::lock_guard<std::mutex> lock(renderer.Mutex);
						if(renderer.Capture) {
							for(const auto& graphic : MetaSpriteCompositor::BuildDecodedCandidates(*renderer.Capture)) {
								if(graphic.HardwareSprite != 0 || graphic.X != 8 || graphic.Y != 15 || graphic.Width != 8 || graphic.Height != 8
									|| !std::all_of(graphic.Pixels.begin(), graphic.Pixels.end(), [](uint32_t pixel) { return pixel == 0xFFFF0000; })) { continue; }
								const auto& pixel = renderer.Capture->Pixels[15 * 256 + 8];
								if(pixel.VisibleSprite == 0 && pixel.SpriteColor == 31 && pixel.Brightness == 15
									&& pixel.SnesFallbackCaptured && pixel.SnesFallbackMainColor == 0x7C00 && pixel.SnesFallbackSubColor == 0x7C00) {
									auto red = std::find(renderer.Pixels.begin(), renderer.Pixels.end(), 0xFFFF0000);
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
				if(!flags) { std::cerr << "SNES capture failed" << std::endl; }
				if(flags) {
					MetaSpriteReplacement group; group.Id = 1; group.ArtworkScale = 2; group.ArtworkWidth = group.ArtworkHeight = 16;
					group.Artwork.assign(256, 0xFF00FF00); group.Artwork[0] = 0; group.Artwork[1] = 0x8000FF00;
					source.X = source.Y = 0; group.Members = {source};
					auto waitFor = [&](std::function<bool()> condition) {
						for(int attempt = 0; attempt < 500; attempt++) {
							{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						} return false;
					};
					emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(waitFor([&]() {
						if(renderer.Width != nativeWidth * 2 || renderer.Height != nativeHeight * 2) { return false; }
						for(int y = 0; y < 16; y++) { for(int x = 0; x < 16; x++) {
							uint32_t expected = y == 0 && x == 0 ? 0xFF0000FF : y == 0 && x == 1 ? 0xFF00807F : 0xFF00FF00;
							if(renderer.Pixels[(spriteY * 2 + y) * renderer.Width + spriteX * 2 + x] != expected) { return false; }
						} } return renderer.Pixels[0] == backdrop;
					})) { flags |= 2; }
					emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					if(waitFor([&]() { return renderer.Width == nativeWidth && renderer.Height == nativeHeight && renderer.Pixels[spriteY * nativeWidth + spriteX] == 0xFFFF0000; })) { flags |= 4; }
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpriteNesCaptureTest(const char* filename)
	{
		FolderUtilities::SetHomeFolder(FolderUtilities::GetFolderName(filename));
		unique_ptr<Emulator> emu(new Emulator()); emu->Initialize(false);
		emu->GetSettings()->SetFlag(EmulationFlags::TestMode);
		emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
		auto& cfg = emu->GetSettings()->GetNesConfig(); cfg.IsFullColorPalette = true;
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
									|| !std::all_of(graphic.Pixels.begin(), graphic.Pixels.end(), [](uint32_t pixel) { return pixel == 0xFFFF0000; })) { continue; }
								const auto& pixel = renderer.Capture->Pixels[8 * 256 + 8];
								if(pixel.VisibleSprite == 0 && pixel.SpriteColor == 1 && pixel.BackgroundColor == 0) { flags = 1; source = graphic; }
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
					emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(waitFor([&]() {
						if(renderer.Width != 512 || renderer.Height != 480) { return false; }
						for(int y = 0; y < 16; y++) {
							for(int x = 0; x < 16; x++) {
								if(renderer.Pixels[(y + 16) * 512 + x + 16] != (x == 0 && y == 0 ? 0xFF0000FF : 0xFF00FF00)) { return false; }
							}
						}
						return renderer.Pixels[0] == 0xFF0000FF;
					})) { flags |= 2; }
					emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					if(waitFor([&]() { return renderer.Width == 256 && renderer.Height == 240 && renderer.Pixels[8 * 256 + 8] == 0xFFFF0000; })) { flags |= 4; }
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpriteWsCaptureTest(const char* filename)
	{
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
								if(graphic.HardwareSprite != 0 || graphic.X != 8 || graphic.Y != 8 || graphic.Width != 8 || graphic.Height != 8
									|| !std::all_of(graphic.Pixels.begin(), graphic.Pixels.end(), [](uint32_t pixel) { return pixel == 0xFFFF0000; })) { continue; }
								const auto& pixel = renderer.Capture->Pixels[8 * 224 + 8];
								if(pixel.VisibleSprite == 0 && pixel.SpriteColor == 0xF00 && pixel.BackgroundColor == 15
									&& renderer.Pixels[8 * 224 + 8] == 0xFFFF0000 && renderer.Pixels[0] == 0xFF0000FF) { flags = 1; source = graphic; }
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
					source.X = source.Y = 0; group.Members = {source};
					auto waitFor = [&](std::function<bool()> condition) {
						for(int attempt = 0; attempt < 500; attempt++) {
							{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(condition()) { return true; } }
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						}
						return false;
					};
					emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(waitFor([&]() {
						if(renderer.Width != 448 || renderer.Height != 288) { return false; }
						for(int y = 0; y < 16; y++) {
							for(int x = 0; x < 16; x++) {
								if(renderer.Pixels[(y + 16) * 448 + x + 16] != (x == 0 && y == 0 ? 0xFF0000FF : 0xFF00FF00)) { return false; }
							}
						}
						return renderer.Pixels[0] == 0xFF0000FF;
					})) { flags |= 2; }
					emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					if(waitFor([&]() { return renderer.Width == 224 && renderer.Height == 144 && renderer.Pixels[8 * 224 + 8] == 0xFFFF0000; })) { flags |= 4; }
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
		MetaSpriteGraphic source;
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
										if(graphic.Pixels[y * 8 + x] != expected || renderer.Pixels[(y + 8) * 240 + x + 8] != displayed) { expectedGraphic = false; }
									}
									}
								if(!expectedGraphic) { continue; }
								const auto& pixel = renderer.Capture->Pixels[8 * 240 + 8];
								if(pixel.VisibleSprite == 0 && pixel.SpriteColor == (mirrored ? 0x7C00 : 31) && pixel.SecondarySprite == -1
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
					source.X = source.Y = 0; group.Members = {source};
					emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
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
								uint32_t expected = y == 0 && x == 0 ? 0xFF0000FF : y == 0 && x == 1 ? 0xFF00807F : 0xFF00FF00;
								if(dark) { expected = y == 0 && x == 0 ? 0xFF00007B : y == 0 && x == 1 ? 0xFF00403D : 0xFF007F00; }
								if(renderer.Pixels[(y + 16) * 480 + x + 16] != expected) { return false; }
							}
						}
						return renderer.Pixels[0] == (dark ? 0xFF00007B : 0xFF0000FF);
					})) { flags |= 2; }
					emu->GetVideoDecoder()->SetMetaSpriteGroups({});
					if(waitFor([&]() { return renderer.Width == 240 && renderer.Height == 160 && renderer.Pixels[8 * 240 + 8] == (dark ? 0xFF7B0000 : mirrored ? 0xFF0000FF : 0xFFFF0000); })) { flags |= 4; }
				}
			}
			emu->Stop(false); emu->GetVideoRenderer()->UnregisterRenderingDevice(&renderer);
		}
		emu->Release(); return flags;
	}
	DllExport uint32_t __stdcall RunMetaSpritePceGameplayTest(const char* filename)
	{
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
							uint32_t expected = mirrored && pixel % spriteWidth < 8 ? 0xFF0000FF : 0xFFFF0000;
							if(candidate.Pixels[pixel] != expected) { correct = false; break; }
						}
						if(!correct) { continue; }
						x = candidate.X - PceConstants::GetLeftOverscan(divider); y = candidate.Y;
						if(x < 0 || y < 0 || x + spriteWidth > (int)width || y + 16 > (int)height || renderer.Pixels[y * width + x] != candidate.Pixels[0]) { continue; }
						source = std::move(candidate); background = renderer.Pixels[0]; return true;
					}
					return false;
				})) {
					flags |= 1; source.X = 0; source.Y = 0; source.HardwareSprite = -1;
					MetaSpriteReplacement group;
					group.Id = 1; group.ArtworkScale = 2; group.ArtworkWidth = spriteWidth * 2; group.ArtworkHeight = 32;
					group.Members.push_back(source); group.Artwork.assign(group.ArtworkWidth * group.ArtworkHeight, 0xFF00FF00);
					emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(waitFor([&]() { return renderer.Width == width * 2 && renderer.Height == height * 2; })) { flags |= 2; }
					if(waitFor([&]() {
						if(renderer.Width != width * 2) { return false; }
						for(int row = 0; row < 32; row++) {
							for(int column = 0; column < spriteWidth * 2; column++) {
								if(renderer.Pixels[(y * 2 + row) * renderer.Width + x * 2 + column] != 0xFF00FF00) { return false; }
							}
						}
						return true;
					})) { flags |= 4; }
					if(waitFor([&]() { return renderer.Width == width * 2 && renderer.Pixels[0] == background; })) { flags |= 8; }
					emu->GetVideoDecoder()->SetMetaSpriteGroups({});
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
		cfg.Model = gbcFixture ? GameboyModel::GameboyColor : GameboyModel::Gameboy;
		cfg.RamPowerOnState = RamState::AllZeros;
		cfg.BlendFrames = false;
		cfg.GbcAdjustColors = false;
		emu->GetSettings()->GetSmsConfig().RamPowerOnState = RamState::AllZeros;
		bool zoomFixture = string(filename).find("zoom") != string::npos;
		bool sms1Fixture = string(filename).find("sms1") != string::npos;
		bool leftFixture = string(filename).find("left") != string::npos;
		bool maskFixture = string(filename).find("mask") != string::npos;
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
							if(sms && !std::all_of(candidate.Pixels.begin(), candidate.Pixels.end(), [](uint32_t pixel) { return pixel == 0xFFFF0000; })) { continue; }
							if(gbcFixture && (!std::all_of(candidate.Pixels.begin(), candidate.Pixels.end(), [](uint32_t pixel) { return pixel == 0xFFFF0000; }) || renderer.Pixels[0] != 0xFF0000FF)) { continue; }
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
					uint32_t partialBackground = 0xFF000000;
					if(gameboy) {
						group.Artwork[0] = 0;
						group.Artwork[1] = 0x8000FF00;
						for(int shift : {0, 8, 16}) {
							uint32_t foreground = shift == 8 ? 255 : 0;
							partialBackground |= ((foreground * 128 + ((originalBackground >> shift) & 255) * 127 + 127) / 255) << shift;
						}
					}
					emu->GetVideoDecoder()->SetMetaSpriteGroups({group});
					if(waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Height == nativeHeight * 2; })) { flags |= 2; }
					if(waitFor([&]() {
						if(renderer.Width != nativeWidth * 2 || renderer.Pixels[spriteY * 2 * renderer.Width + visibleX * 2] != (gameboy ? originalBackground : 0xFF00FF00)) { return false; }
						if(gameboy) {
							for(int y = 0; y < spriteHeight * 2; y++) {
								for(int x = 0; x < spriteWidth * 2; x++) {
									uint32_t expected = y == 0 && x == 0 ? originalBackground : y == 0 && x == 1 ? partialBackground : 0xFF00FF00;
									if(renderer.Pixels[(spriteY * 2 + y) * renderer.Width + spriteX * 2 + x] != expected) { return false; }
								}
							}
						}
						return true;
					})) { flags |= 4; }
					{ std::lock_guard<std::mutex> lock(renderer.Mutex); if(renderer.Width == nativeWidth * 2 && renderer.Pixels[0] == originalBackground) { flags |= 8; } }
					if(sms && waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Pixels[spriteY * 2 * renderer.Width + 48] == occludedPixel
						&& occludedPixel != 0xFF00FF00 && renderer.Capture && renderer.Capture->Pixels[spriteY * nativeWidth + 24].VisibleSprite == -1; })) { flags |= 32; }
					if(maskFixture && waitFor([&]() { return renderer.Width == nativeWidth * 2 && renderer.Pixels[spriteY * 2 * renderer.Width + 8] == maskedPixel
						&& maskedPixel != 0xFF00FF00 && renderer.Capture && renderer.Capture->Pixels[spriteY * nativeWidth + 4].VisibleSprite == -1; })) { flags |= 64; }
					emu->GetVideoDecoder()->SetMetaSpriteGroups({});
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

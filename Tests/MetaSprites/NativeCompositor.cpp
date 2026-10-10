#include "../../Core/Shared/Video/MetaSpriteCompositor.h"
#include "../../Core/Shared/Video/MetaSpritePacket.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <iterator>

int main(int argc, char** argv)
{
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
	layeredPixel.Brightness = 15; layeredPixel.SnesMathEnabled = true; layeredPixel.SnesHalf = true; layeredPixel.SnesUseSubscreen = true;
	primaryGroup.Artwork[0] = 0xFF00FF00; secondaryGroup.Artwork[0] = 0xFFFFFFFF;
	layeredCompositor.SetGroups({primaryGroup, secondaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF7FFF7F);
	layeredCompositor.SetGroups({secondaryGroup, primaryGroup});
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF7FFF7F);
	layeredPixel.SnesSubtract = true;
	assert(layeredCompositor.Apply(&layeredOriginal, 1, 1, gbaLayers)[0] == 0xFF000000);
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
	}
	std::cout << "PASS: native graphic reconstruction, content matching, visibility/priority masks, transparent suppression, alpha blend, incomplete capture fallback, disable fallback.\n";
}

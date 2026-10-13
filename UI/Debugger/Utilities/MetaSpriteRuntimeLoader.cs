using Mesen.Config;
using Mesen.Interop;
using SkiaSharp;
using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace Mesen.Debugger.Utilities;

internal static class MetaSpriteRuntimeLoader
{
	public static string GetPackPath(string romHash, CpuType cpuType) =>
		Path.Combine(ConfigManager.HdPackFolder, "MetaSprites", romHash, cpuType.ToString(), "groups.json");

	public static bool Apply(MetaSpritePack pack, string path)
	{
		ValidateBackgroundSupport(pack);
		byte[] packet = MetaSpriteRuntimePacket.Create(pack, group => LoadArtwork(group, Path.GetDirectoryName(path)!),
			alternate => LoadPng(Path.Combine(Path.GetDirectoryName(path)!, alternate.Path), alternate.Scale));
		byte[] backgrounds = MetaBackgroundRuntimePacket.Create(pack, layer => {
			if(layer.ArtworkPath.Length == 0) { throw new InvalidDataException($"Background {layer.Id} has no PNG."); }
			return LoadPng(Path.Combine(Path.GetDirectoryName(path)!, layer.ArtworkPath), layer.ArtworkScale);
		});
		return DebugApi.SetMetaReplacements(pack.RomHash, packet, (uint)packet.Length, backgrounds, (uint)backgrounds.Length);
	}

	public static void ValidateBackgroundSupport(MetaSpritePack pack)
	{
		// Use the same activation policy as the packet builder: drafts without checks are inactive.
		var activeLayers = pack.BackgroundLayers.Where(layer => layer.Enabled && layer.MemoryConditions.Count > 0).ToArray();
		if(activeLayers.Length > 0 && ((pack.System != CpuType.Gameboy.ToString() && pack.System != CpuType.Sms.ToString() && pack.System != CpuType.Ws.ToString() && pack.System != CpuType.Nes.ToString() && pack.System != CpuType.Pce.ToString() && pack.System != CpuType.Gba.ToString() && pack.System != CpuType.Snes.ToString())
			|| activeLayers.Any(layer => pack.System == CpuType.Sms.ToString() || pack.System == CpuType.Nes.ToString() || pack.System == CpuType.Pce.ToString() ? layer.NativeLayer != 0 : layer.NativeLayer > (pack.System == CpuType.Gba.ToString() || pack.System == CpuType.Snes.ToString() ? 3 : 1)))) {
			throw new InvalidDataException("Background gameplay replacement currently supports NES/PCE layer 0, GB/GBC layers 0/1, GBA/SNES layers 0..3, SMS Mode 4 layer 0, and WonderSwan layers 0/1.");
		}
	}

	// A delayed game-load callback must never apply assets belonging to an older ROM.
	public static bool LoadForGame(string expectedRomHash, CpuType cpuType = CpuType.Gameboy)
	{
		if(EmuApi.GetRomHash(HashType.Sha1) != expectedRomHash) { return false; }
		string path = GetPackPath(expectedRomHash, cpuType);
		if(!File.Exists(path)) { return false; }
		try {
			MetaSpritePack pack = MetaSpritePack.Load(path, expectedRomHash, cpuType.ToString());
			if(!Apply(pack, path)) {
				EmuApi.DisplayMessage("Meta groups", "Could not apply saved replacements for this console layout.");
				return false;
			}
			return true;
		} catch(Exception ex) {
			EmuApi.DisplayMessage("Meta groups", "Could not load saved replacements: " + ex.Message);
			return false;
		}
	}

	private static (int Width, int Height, uint[] Pixels) LoadArtwork(MetaSpriteGroup group, string directory)
	{
		if(group.ReplacementPath.Length == 0) { throw new InvalidDataException($"Meta {group.Id:D3} has no replacement PNG."); }
		return LoadPng(Path.Combine(directory, group.ReplacementPath), group.ReplacementScale);
	}

	public static void ValidateSpriteArtworkCanvases(MetaSpritePack pack, string packPath)
	{
		string directory = Path.GetDirectoryName(packPath)!;
		foreach(var group in pack.Groups) {
			if(group.ReplacementPath.Length > 0) { ValidatePngCanvas(Path.Combine(directory, group.ReplacementPath), group.ReplacementScale); }
			foreach(var alternate in group.AlternateArtwork) {
				alternate.Validate(); ValidatePngCanvas(Path.Combine(directory, alternate.Path), alternate.Scale);
			}
		}
	}
	public static void ValidateBackgroundArtworkCanvases(MetaSpritePack pack, string packPath)
	{
		foreach(var layer in pack.BackgroundLayers) {
			if(layer.ArtworkPath.Length == 0) { continue; }
			string path = Path.Combine(Path.GetDirectoryName(packPath)!, layer.ArtworkPath);
			using SKCodec codec = SKCodec.Create(path) ?? throw new InvalidDataException($"Could not read PNG '{layer.ArtworkPath}'.");
			MetaSpritePlacement.ValidateCanvas(codec.Info.Width, codec.Info.Height, layer.ArtworkScale);
			if(codec.Info.Width != layer.ArtworkWidth || codec.Info.Height != layer.ArtworkHeight) {
				throw new InvalidDataException($"Background {layer.Id:D3} PNG dimensions changed. Import the edited PNG again before saving.");
			}
		}
	}
	private static void ValidatePngCanvas(string path, int scale)
	{
		using SKCodec codec = SKCodec.Create(path) ?? throw new InvalidDataException("Could not read replacement PNG dimensions: " + Path.GetFileName(path));
		MetaSpritePlacement.ValidateCanvas(codec.Info.Width, codec.Info.Height, scale);
	}

	private static (int Width, int Height, uint[] Pixels) LoadPng(string path, int scale)
	{
		using SKCodec codec = SKCodec.Create(path) ?? throw new InvalidDataException("Could not decode replacement PNG.");
		MetaSpritePlacement.ValidateCanvas(codec.Info.Width, codec.Info.Height, scale);
		// Decode straight alpha directly: a premultiplied UI bitmap round trip
		// can change partial-alpha RGB values before gameplay composition.
		var info = new SKImageInfo(codec.Info.Width, codec.Info.Height, SKColorType.Bgra8888, SKAlphaType.Unpremul);
		using SKBitmap converted = new(info);
		if(codec.GetPixels(info, converted.GetPixels()) != SKCodecResult.Success) { throw new InvalidDataException("Could not decode complete replacement PNG pixels."); }
		uint[] pixels = new uint[info.Width * info.Height];
		int[] row = new int[info.Width];
		for(int y = 0; y < info.Height; y++) {
			Marshal.Copy(converted.GetPixels() + y * converted.RowBytes, row, 0, row.Length);
			for(int x = 0; x < row.Length; x++) { pixels[y * row.Length + x] = unchecked((uint)row[x]); }
		}
		return (info.Width, info.Height, pixels);
	}
}

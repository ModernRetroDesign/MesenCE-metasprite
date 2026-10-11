using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Mesen.Config;
using Mesen.Interop;
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
		if(pack.Groups.Any(g => g.Enabled && g.AlternateArtwork.Count > 0) || pack.BackgroundLayers.Any(b => b.Enabled && b.MemoryConditions.Count > 0)) {
			throw new InvalidDataException("Alternate metasprite artwork and memory-conditioned backgrounds are not implemented yet.");
		}
		if(pack.BackgroundLayers.Any(layer => layer.Enabled) && ((pack.System != CpuType.Gameboy.ToString() && pack.System != CpuType.Sms.ToString() && pack.System != CpuType.Ws.ToString() && pack.System != CpuType.Nes.ToString() && pack.System != CpuType.Pce.ToString() && pack.System != CpuType.Gba.ToString())
			|| pack.BackgroundLayers.Any(layer => layer.Enabled && (pack.System == CpuType.Sms.ToString() || pack.System == CpuType.Nes.ToString() || pack.System == CpuType.Pce.ToString() ? layer.NativeLayer != 0 : layer.NativeLayer > (pack.System == CpuType.Gba.ToString() ? 3 : 1))))) {
			throw new InvalidDataException("Background gameplay replacement currently supports NES/PCE layer 0, GB/GBC layers 0/1, GBA layers 0..3, SMS Mode 4 layer 0, and WonderSwan layers 0/1.");
		}
		byte[] packet = MetaSpriteRuntimePacket.Create(pack, group => LoadArtwork(group, Path.GetDirectoryName(path)!));
		byte[] backgrounds = MetaBackgroundRuntimePacket.Create(pack, layer => {
			if(layer.ArtworkPath.Length == 0) { throw new InvalidDataException($"Background {layer.Id} has no PNG."); }
			return LoadPng(Path.Combine(Path.GetDirectoryName(path)!, layer.ArtworkPath), layer.ArtworkScale);
		});
		return DebugApi.SetMetaReplacements(pack.RomHash, packet, (uint)packet.Length, backgrounds, (uint)backgrounds.Length);
	}

	// A delayed game-load callback must never apply assets belonging to an older ROM.
	public static void LoadForGame(string expectedRomHash, CpuType cpuType = CpuType.Gameboy)
	{
		if(EmuApi.GetRomHash(HashType.Sha1) != expectedRomHash) { return; }
		string path = GetPackPath(expectedRomHash, cpuType);
		if(!File.Exists(path)) { return; }
		try {
			MetaSpritePack pack = MetaSpritePack.Load(path, expectedRomHash, cpuType.ToString());
			if(!Apply(pack, path)) { EmuApi.DisplayMessage("Meta groups", "Could not apply saved replacements for this console layout."); }
		} catch(Exception ex) {
			EmuApi.DisplayMessage("Meta groups", "Could not load saved replacements: " + ex.Message);
		}
	}

	private static (int Width, int Height, uint[] Pixels) LoadArtwork(MetaSpriteGroup group, string directory)
	{
		if(group.ReplacementPath.Length == 0) { throw new InvalidDataException($"Meta {group.Id:D3} has no replacement PNG."); }
		return LoadPng(Path.Combine(directory, group.ReplacementPath), group.ReplacementScale);
	}

	private static (int Width, int Height, uint[] Pixels) LoadPng(string path, int scale)
	{
		using Bitmap source = new(path);
		MetaSpritePlacement.ValidateCanvas(source.PixelSize.Width, source.PixelSize.Height, scale);
		using WriteableBitmap converted = new(source.PixelSize, new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);
		uint[] pixels = new uint[source.PixelSize.Width * source.PixelSize.Height];
		using(var fb = converted.Lock()) {
			source.CopyPixels(fb);
			int[] row = new int[source.PixelSize.Width];
			for(int y = 0; y < source.PixelSize.Height; y++) {
				Marshal.Copy(fb.Address + y * fb.RowBytes, row, 0, row.Length);
				for(int x = 0; x < row.Length; x++) { pixels[y * row.Length + x] = unchecked((uint)row[x]); }
			}
		}
		return (source.PixelSize.Width, source.PixelSize.Height, pixels);
	}
}

using Mesen.Config;
using Mesen.Interop;
using SkiaSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace Mesen.Debugger.Utilities;

internal static class MetaSpriteRuntimeLoader
{
	public static string GetPackPath(string romHash, CpuType cpuType) =>
		Path.Combine(ConfigManager.HdPackFolder, "MetaSprites", cpuType.ToString(), romHash, "groups.json");

	public static bool Apply(MetaSpritePack pack, string path)
	{
		ValidateBackgroundSupport(pack);
		string directory=Path.GetDirectoryName(path)!;
		var decoded=new MetaPngDecodeCache(64L*1024*1024,128,(file,reserve) => DecodePng(file,1,reserve));
		(int Width,int Height,uint[] Pixels) LoadCached(string relative,int scale) {
			if(string.IsNullOrWhiteSpace(relative)) { throw new InvalidDataException("Replacement artwork requires a PNG."); }
			MetaSpritePlacement.ValidateSpriteCanvas(1,1,scale); // Reject invalid scale before decoding any pixels.
			string fullPath=Path.GetFullPath(Path.Combine(directory,relative));
			var image=decoded.Get(fullPath);
			MetaSpritePlacement.ValidateSpriteCanvas(image.Width,image.Height,scale); return image;
		}
		using var packet = MetaSpriteRuntimePacket.CreateFile(pack, group => LoadCached(group.ReplacementPath,group.ReplacementScale),
			alternate => LoadCached(alternate.Path,alternate.Scale), (frame,scale) => LoadCached(frame.Path,scale), sharedFrames:true);
		using var backgrounds = MetaBackgroundRuntimePacket.CreateFile(pack, layer => {
			if(layer.ArtworkPath.Length == 0) { throw new InvalidDataException($"Background {layer.Id} has no PNG."); }
			var artwork = LoadCached(layer.ArtworkPath, layer.ArtworkScale);
			MetaSpritePlacement.ValidateCanvas(artwork.Width, artwork.Height, layer.ArtworkScale);
			return artwork;
		}, (frame,scale) => LoadCached(frame.Path,scale), sharedFrames:true);
		var glyphBudget=new MetaTextRasterizer.Budget();
		using var texts=MetaTextRuntimePacket.CreateFile(pack,p => glyphBudget.Rasterize(p,directory));
		return DebugApi.SetMetaReplacementFilesWithText(pack.RomHash,packet.Path,backgrounds.Path,texts.Path);
	}

	public static void ValidateBackgroundSupport(MetaSpritePack pack)
	{
		// Use the same activation policy as the packet builder: drafts without checks are inactive.
		var activeLayers = pack.BackgroundLayers.Where(layer => layer.Enabled && layer.MemoryConditions.Count > 0).ToArray();
		if(pack.System != CpuType.Gba.ToString() && activeLayers.Any(layer => layer.CompositionPosition == 2)) { throw new InvalidDataException("Independent GBA priority requires a GBA pack."); }
		if(pack.System != CpuType.Snes.ToString() && activeLayers.Any(layer => layer.CompositionPosition == 3)) { throw new InvalidDataException("Independent SNES priority requires a SNES pack."); }
		if(pack.System != CpuType.Nes.ToString() && pack.System != CpuType.Gameboy.ToString() && pack.System != CpuType.Pce.ToString() && pack.System != CpuType.Sms.ToString() && pack.System != CpuType.Ws.ToString() && activeLayers.Any(layer => layer.CompositionPosition >= 4)) { throw new InvalidDataException("Sprite insertion bands require a NES, Game Boy, PCE, SMS or WonderSwan pack."); }
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
			ValidateAnimationCanvases(group.Animation,directory,group.ReplacementScale,false);
			if(group.ReplacementPath.Length > 0) { ValidatePngCanvas(Path.Combine(directory, group.ReplacementPath), group.ReplacementScale); }
			foreach(var alternate in group.AlternateArtwork) {
				ValidateAnimationCanvases(alternate.Animation,directory,alternate.Scale,false);
				alternate.Validate(); ValidatePngCanvas(Path.Combine(directory, alternate.Path), alternate.Scale);
			}
		}
	}
	public static void ValidateBackgroundArtworkCanvases(MetaSpritePack pack, string packPath)
	{
		foreach(var layer in pack.BackgroundLayers) {
			ValidateAnimationCanvases(layer.Animation,Path.GetDirectoryName(packPath)!,layer.ArtworkScale,true);
			if(layer.ArtworkPath.Length == 0) { continue; }
			string path = Path.Combine(Path.GetDirectoryName(packPath)!, layer.ArtworkPath);
			using SKCodec codec = SKCodec.Create(path) ?? throw new InvalidDataException($"Could not read PNG '{layer.ArtworkPath}'.");
			MetaSpritePlacement.ValidateCanvas(codec.Info.Width, codec.Info.Height, layer.ArtworkScale);
			if(codec.Info.Width != layer.ArtworkWidth || codec.Info.Height != layer.ArtworkHeight) {
				throw new InvalidDataException($"Background {layer.Id:D3} PNG dimensions changed. Import the edited PNG again before saving.");
			}
		}
	}
	private static void ValidateAnimationCanvases(MetaPngAnimation animation,string directory,int scale,bool background)
	{
		animation.Validate(); if(!animation.Enabled) { return; }
		foreach(var frame in animation.Frames) {
			using var codec=SKCodec.Create(Path.Combine(directory,frame.Path)) ?? throw new InvalidDataException($"Could not read animation PNG '{frame.Path}'.");
			if(background) { MetaSpritePlacement.ValidateCanvas(codec.Info.Width,codec.Info.Height,scale); }
			else { MetaSpritePlacement.ValidateSpriteCanvas(codec.Info.Width,codec.Info.Height,scale); }
		}
	}
	private static void ValidatePngCanvas(string path, int scale)
	{
		using SKCodec codec = SKCodec.Create(path) ?? throw new InvalidDataException("Could not read replacement PNG dimensions: " + Path.GetFileName(path));
		MetaSpritePlacement.ValidateSpriteCanvas(codec.Info.Width, codec.Info.Height, scale);
	}

	private static (int Width, int Height, uint[] Pixels) LoadPng(string path, int scale) => DecodePng(path,scale,null);
	private static (int Width, int Height, uint[] Pixels) DecodePng(string path,int scale,Action<long>? reserve)
	{
		using SKCodec codec = SKCodec.Create(path) ?? throw new InvalidDataException("Could not decode replacement PNG.");
		MetaSpritePlacement.ValidateSpriteCanvas(codec.Info.Width, codec.Info.Height, scale);
		reserve?.Invoke((long)codec.Info.Width*codec.Info.Height*4);
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

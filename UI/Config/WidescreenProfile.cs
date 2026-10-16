using System;
using System.IO;
using System.Linq;
using System.Text.Json;
using Mesen.Interop;
using Mesen.Utilities;

namespace Mesen.Config;

public sealed class WidescreenProfile
{
	public bool DecorativeWidescreen { get; set; } = false;
	public double DecorativeAspectRatio { get; set; } = 16.0 / 9.0;
	public int DecorativeRatioPreset { get; set; } = 0;
	public uint DecorativeSideBrightness { get; set; } = 50;
	public uint DecorativeViewportPosition { get; set; } = 0;
	public bool DecorativeCustomWidths { get; set; } = false;
	public uint DecorativeLeftWidth { get; set; } = 64;
	public uint DecorativeRightWidth { get; set; } = 64;
	public uint DecorativeFadeWidth { get; set; } = 0;
	public static WidescreenProfile From(VideoConfig video) => new() {
		DecorativeWidescreen = video.DecorativeWidescreen,
		DecorativeAspectRatio = video.DecorativeAspectRatio,
		DecorativeRatioPreset = video.DecorativeRatioPreset,
		DecorativeSideBrightness = video.DecorativeSideBrightness,
		DecorativeViewportPosition = video.DecorativeViewportPosition,
		DecorativeCustomWidths = video.DecorativeCustomWidths,
		DecorativeLeftWidth = video.DecorativeLeftWidth,
		DecorativeRightWidth = video.DecorativeRightWidth,
		DecorativeFadeWidth = video.DecorativeFadeWidth,
	};
	public void CopyTo(VideoConfig video) {
		video.DecorativeWidescreen = DecorativeWidescreen;
		video.DecorativeAspectRatio = DecorativeAspectRatio;
		video.DecorativeRatioPreset = DecorativeRatioPreset;
		video.DecorativeSideBrightness = DecorativeSideBrightness;
		video.DecorativeViewportPosition = DecorativeViewportPosition;
		video.DecorativeCustomWidths = DecorativeCustomWidths;
		video.DecorativeLeftWidth = DecorativeLeftWidth;
		video.DecorativeRightWidth = DecorativeRightWidth;
		video.DecorativeFadeWidth = DecorativeFadeWidth;
	}
	public void ApplyTo(ref InteropVideoConfig video) {
		video.DecorativeWidescreen = DecorativeWidescreen;
		video.DecorativeAspectRatio = DecorativeRatioPreset == 0 ? 16.0 / 9.0 : DecorativeRatioPreset == 1 ? 21.0 / 9.0 : DecorativeAspectRatio;
		video.DecorativeSideBrightness = DecorativeSideBrightness;
		video.DecorativeViewportPosition = DecorativeViewportPosition;
		video.DecorativeCustomWidths = DecorativeCustomWidths;
		video.DecorativeLeftWidth = DecorativeLeftWidth;
		video.DecorativeRightWidth = DecorativeRightWidth;
		video.DecorativeFadeWidth = DecorativeFadeWidth;
	}

	public void Validate() {
		if(!double.IsFinite(DecorativeAspectRatio) || DecorativeAspectRatio < 1 || DecorativeAspectRatio > 5
			|| DecorativeRatioPreset < 0 || DecorativeRatioPreset > 2 || DecorativeSideBrightness > 100
			|| DecorativeViewportPosition > 3 || DecorativeLeftWidth > 1024 || DecorativeRightWidth > 1024 || DecorativeFadeWidth > 1024) {
			throw new InvalidDataException("Invalid widescreen profile settings.");
		}
	}
}

public static class WidescreenProfileStore
{
	public static WidescreenProfile? Active { get; private set; }
	public static string? ActivePath { get; private set; }
	public static string GetPath(string root, string hash, CpuType cpu) {
		if(hash.Length != 40 || !hash.All(Uri.IsHexDigit)) { throw new InvalidDataException("Invalid ROM hash."); }
		return Path.Combine(root, "Widescreen", cpu.ToString(), hash.ToUpperInvariant() + ".json");
	}
	public static WidescreenProfile? Read(string path) {
		if(!File.Exists(path)) { return null; }
		var profile = JsonSerializer.Deserialize(File.ReadAllText(path), MesenSerializerContext.Default.WidescreenProfile)
			?? throw new InvalidDataException("Missing widescreen profile.");
		profile.Validate(); return profile;
	}
	public static void Write(string path, WidescreenProfile profile) {
		profile.Validate(); Directory.CreateDirectory(Path.GetDirectoryName(path)!);
		string temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
		try { File.WriteAllText(temporary, JsonSerializer.Serialize(profile, MesenSerializerContext.Default.WidescreenProfile)); File.Move(temporary, path, true); }
		finally { if(File.Exists(temporary)) { File.Delete(temporary); } }
	}
	public static void LoadForGame(string hash, CpuType cpu) {
		Active = null; ActivePath = GetPath(ConfigManager.GameConfigFolder, hash, cpu);
		try { Active = Read(ActivePath); }
		catch(Exception ex) { EmuApi.DisplayMessage("Widescreen", "Could not load game profile: " + ex.Message); }
		ConfigManager.Config.Video.ApplyConfig();
	}
	public static void Save(string expectedPath, WidescreenProfile profile) {
		if(ActivePath != expectedPath) { throw new InvalidOperationException("The loaded game changed. Reopen Video settings."); }
		Write(expectedPath, profile); Active = profile;
	}
	public static void Remove(string expectedPath) {
		if(ActivePath != expectedPath) { throw new InvalidOperationException("The loaded game changed. Reopen Video settings."); }
		if(File.Exists(expectedPath)) { File.Delete(expectedPath); } Active = null;
	}
	public static void Clear() { Active = null; ActivePath = null; }
}


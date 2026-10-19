using Mesen.Config;
using Mesen.Interop;
using System;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace Mesen.Debugger.Utilities;

internal static class MusicReplacementRuntime
{
	public static MetaNamedConditionLibrary? GetLibrary(string hash,System.Collections.Generic.IEnumerable<string> expressions)
	{
		if(!expressions.Any(MetaNamedConditionBindings.UsesNames)) { return null; }
		var cpu=EmuApi.GetRomInfo().ConsoleType.GetMainCpuType();
		string path=Path.Combine(Path.GetDirectoryName(MetaSpriteRuntimeLoader.GetPackPath(hash,cpu))!,MetaNamedConditionLibrary.FileName);
		return MetaNamedConditionLibrary.Load(path,hash,cpu.ToString());
	}
	public static MusicReplacementPack LoadPack(string path,string hash,string system)
	{
		var pack=File.Exists(path) ? JsonSerializer.Deserialize(File.ReadAllText(path),MusicReplacementJson.Default.MusicReplacementPack) ?? throw new InvalidDataException("Empty music pack.") : new MusicReplacementPack {RomHash=hash,System=system};
		if(pack.RomHash!=hash || pack.System!=system) { throw new InvalidDataException("Music pack belongs to another game/system."); }
		// Validate structure before enumerating user-owned rule entries.
		if(pack.Rules==null || pack.Rules.Any(r=>r==null || r.Trigger==null || r.Conditions==null)) { throw new InvalidDataException("Invalid music rules."); }
		pack.Validate(GetLibrary(hash,pack.NamedExpressions())); return pack;
	}
	public static string GetPath(string hash) => Path.Combine(ConfigManager.HdPackFolder, "Audio", EmuApi.GetRomInfo().ConsoleType.ToString(), hash, "music.json");
	public static bool Apply(MusicReplacementPack pack, string path) {
		if(pack.System != EmuApi.GetRomInfo().ConsoleType.ToString()) { throw new InvalidDataException("Audio pack belongs to another system."); }
		byte[] packet = pack.CreatePacket(Path.GetDirectoryName(path)!,GetLibrary(pack.RomHash,pack.NamedExpressions()));
		return DebugApi.SetMusicReplacements(pack.RomHash, packet, (uint)packet.Length);
	}
	public static void LoadForGame(string hash) {
		string path = GetPath(hash); if(!File.Exists(path) || EmuApi.GetRomHash(HashType.Sha1) != hash) { return; }
		try { if(!Apply(LoadPack(path, hash, EmuApi.GetRomInfo().ConsoleType.ToString()), path)) { throw new InvalidDataException("Audio file or loop points could not be loaded. Use OGG Vorbis or PCM 16-bit mono/stereo WAV, 8–192 kHz."); } }
		catch(Exception ex) { EmuApi.DisplayMessage("Music replacements", ex.Message); }
	}
}

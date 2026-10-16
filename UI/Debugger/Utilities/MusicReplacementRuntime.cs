using Mesen.Config;
using Mesen.Interop;
using System;
using System.IO;

namespace Mesen.Debugger.Utilities;

internal static class MusicReplacementRuntime
{
	public static string GetPath(string hash) => Path.Combine(ConfigManager.HdPackFolder, "Audio", EmuApi.GetRomInfo().ConsoleType.ToString(), hash, "music.json");
	public static bool Apply(MusicReplacementPack pack, string path) {
		if(pack.System != EmuApi.GetRomInfo().ConsoleType.ToString()) { throw new InvalidDataException("Audio pack belongs to another system."); }
		byte[] packet = pack.CreatePacket(Path.GetDirectoryName(path)!);
		return DebugApi.SetMusicReplacements(pack.RomHash, packet, (uint)packet.Length);
	}
	public static void LoadForGame(string hash) {
		string path = GetPath(hash); if(!File.Exists(path) || EmuApi.GetRomHash(HashType.Sha1) != hash) { return; }
		try { if(!Apply(MusicReplacementPack.Load(path, hash, EmuApi.GetRomInfo().ConsoleType.ToString()), path)) { throw new InvalidDataException("Audio file or loop points could not be loaded. Use OGG Vorbis or PCM 16-bit mono/stereo WAV, 8–192 kHz."); } }
		catch(Exception ex) { EmuApi.DisplayMessage("Music replacements", ex.Message); }
	}
}

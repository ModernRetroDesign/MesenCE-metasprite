using System;
using System.IO;
using System.Linq;
using Mesen.Debugger.Utilities;

internal static class MetaTextTests
{
	public static void Run(string? packetPath=null)
	{
		static void Check(bool value,string message) { if(!value) { throw new Exception(message); } }
		static void Reject(Action action,string message) { try { action(); } catch(InvalidDataException) { return; } throw new Exception(message); }
		var mappings=MetaTextMappingText.Parse("0010 * = A\r\n0010 02 = U+1F642\n0011 = U+0020\n0012 = =");
		Check(mappings.Count==4 && mappings[1].Character=="\U0001F642" && mappings[2].Character==" " && mappings[3].Character=="=","Mapping parser lost Unicode, spaces or equals glyphs.");
		Check(MetaTextMappingText.Format(MetaTextMappingText.Parse(MetaTextMappingText.Format(mappings)))==MetaTextMappingText.Format(mappings),"Unicode mapping round trip changed identity.");
		var profile=new MetaTextProfile {Id=1,Name="Dialog",Enabled=true,FontPath="fonts/dialog.ttf",Mappings=mappings,FillEnabled=true,FillColor="10203080",
			MemoryConditions=MetaMemoryConditionText.Parse("0652 == 08\n0760 != 06")};
		profile.Validate();
		var grouped=MetaTextMappingText.Parse("0010,0020 * = A\n0010,0021 * = B\n0010,0020 02 = C");
		var groupedProfile=new MetaTextProfile {Id=2,CellHeight=16,CellOriginY=8,Mappings=grouped}; groupedProfile.Validate();
		Check(groupedProfile.Find(0x10,0)==null && groupedProfile.Find(new uint[] {0x10,0x21},0)?.Character=="B" && groupedProfile.Find(new uint[] {0x10,0x20},2)?.Character=="C","Managed lookup must match complete source patterns and prefer exact palettes.");
		Check(MetaTextMappingText.Format(MetaTextMappingText.Parse(MetaTextMappingText.Format(grouped)))==MetaTextMappingText.Format(grouped),"Multi-tile mappings did not round trip.");
		groupedProfile.Mappings.Add(new() {Tile=3,Character="D"}); Reject(groupedProfile.Validate,"Incomplete multi-tile character accepted."); groupedProfile.Mappings.RemoveAt(3);
		groupedProfile.CellWidth=9; Reject(groupedProfile.Validate,"Non-tile source cell size accepted."); groupedProfile.CellWidth=8;
		groupedProfile.CellOriginY=16; Reject(groupedProfile.Validate,"Source cell origin outside one cell accepted."); groupedProfile.CellOriginY=8;
		Reject(() => MetaTextMappingText.Parse("10,20 = A\n10,20 * = B"),"Duplicate full pattern accepted.");
		Console.WriteLine("PASS: multi-tile source patterns, shared top tiles, grid origins, completeness validation and mapping round trip.");
		Check(profile.Find(0x10,2)?.Character=="\U0001F642" && profile.Find(0x10,3)?.Character=="A" && profile.Find(0x13,2)==null,"Palette-specific mapping must override wildcard without matching an unrelated tile.");
		Check(MetaMemoryCondition.Matches(profile.MemoryConditions,a => a==0x652 ? (byte)8 : (byte)4)
			&& !MetaMemoryCondition.Matches(profile.MemoryConditions,a => a==0x652 ? (byte)8 : null),"Text condition snapshots must require every configured byte.");
		foreach(string bad in new[] {"10 = AB","10 = U+D800","10 = U+110000","10 = U+000A","10 100 = A","10 = A\n10 * = B","GG = A","10 = "}) {
			Reject(() => MetaTextMappingText.Parse(bad),"Malformed/ambiguous mapping accepted: "+bad);
		}
		Check(!MetaTextProfile.IsCharacter("") && !MetaTextProfile.IsCharacter("\uD800"),"Empty or unpaired UTF-16 accepted as a glyph.");
		foreach(string path in new[] {"../font.ttf","fonts/../font.ttf","C:/font.ttf","/font.ttf","fonts//font.ttf","fonts/./font.ttf","font.png","font.ttf:extra"}) {
			profile.FontPath=path; Reject(profile.Validate,"Unsafe font path accepted: "+path);
		}
		profile.FontPath="fonts/dialog.ttf";
		string directory=Path.Combine(Path.GetTempPath(),"mesen-text-"+Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(directory);
		try {
			string path=Path.Combine(directory,"pack.json");
			var pack=new MetaSpritePack {RomHash="text-fixture",System="Nes",NextBackgroundId=7,TextProfiles=new() {profile}};
			pack.Save(path); var loaded=MetaSpritePack.Load(path,"text-fixture","Nes");
			Check(loaded.TextProfiles.Single().Find(0x10,2)?.Character=="\U0001F642" && loaded.TextProfiles[0].MemoryConditions.Count==2 && loaded.NextBackgroundId==7 && loaded.TextProfiles[0].FillEnabled && loaded.TextProfiles[0].FillColor=="10203080","Pack persistence lost text definitions/fill or unrelated state.");
			File.WriteAllText(path,"{\"Version\":1,\"RomHash\":\"text-fixture\",\"System\":\"Nes\"}");
			Check(MetaSpritePack.Load(path,"text-fixture","Nes").TextProfiles.Count==0,"Legacy pack without text profiles should load unchanged.");
			pack.TextProfiles.Add(profile); pack.Save(path);
			Reject(() => MetaSpritePack.Load(path,"text-fixture","Nes"),"Duplicate text IDs accepted.");
		} finally { foreach(string file in Directory.GetFiles(directory)) { File.Delete(file); } Directory.Delete(directory); }
		Console.WriteLine("PASS: text Unicode/palette mappings, malformed input rejection, safe font paths, fail-closed RAM conditions, pack round trip and legacy compatibility.");
		var runtime=new MetaSpritePack {System="Nes",TextProfiles=new() {new() {Id=1,Enabled=true,FontPath="test.ttf",RasterScale=2,UsePaletteColor=false,Color="00FF00FF",FillEnabled=true,FillColor="10203080",
			Mappings=new() {new() {Tile=0x10,Character="A"},new() {Tile=0x10,Palette=2,Character="B"}},MemoryConditions=new() {new() {Address=0x652,Value=8}}}}};
		var packet=MetaTextRuntimePacket.Create(runtime,p => new[] {new MetaTextGlyph("A",Enumerable.Repeat(0x80FFFFFFu,256).ToArray()),new MetaTextGlyph("B",Enumerable.Repeat(0xFFFFFFFFu,256).ToArray())});
		Check(BitConverter.ToUInt32(packet,0)==MetaTextRuntimePacket.Magic && BitConverter.ToUInt32(packet,4)==1,"Text transport header mismatch.");
		if(packetPath!=null) { File.WriteAllBytes(packetPath+".text",packet); }
		groupedProfile.Enabled=true; groupedProfile.FontPath="test.ttf"; groupedProfile.Proportional=true;
		var multiPacket=MetaTextRuntimePacket.Create(new MetaSpritePack {System="Nes",TextProfiles=new() {groupedProfile}},p => {
			var atlas=new MetaTextGlyphSet {Unclipped=true,LineHeight64=1024};
			foreach(string c in new[] {"A","B","C"}) { atlas.Add(new(c,Enumerable.Repeat(0xFFFFFFFFu,128).ToArray(),8,16)); } return atlas;
		});
		if(packetPath!=null) { File.WriteAllBytes(packetPath+".text-multi",multiPacket); }
		foreach(string system in new[] {"Gameboy","Gba","Pce","Sms","Snes","Ws"}) { runtime.System=system; Check(MetaTextRuntimePacket.Create(runtime,p => new[] {new MetaTextGlyph("A",new uint[256]),new MetaTextGlyph("B",new uint[256])}).Length>8,"Supported text adapter rejected: "+system); }
		runtime.System="Unknown"; Reject(() => MetaTextRuntimePacket.Create(runtime,_ => throw new Exception("Unsupported text system reached rasterization.")),"Unsupported renderer accepted active text.");
		runtime.TextProfiles[0].Enabled=false;
		Check(MetaTextRuntimePacket.Create(runtime,_ => throw new Exception("Disabled text decoded glyphs.")).Length==8,"Disabled text must clear definitions without loading fonts.");
		Console.WriteLine("PASS: text runtime packet header/resources, unsupported-system rejection and disabled-profile clearing without font loading.");
	}
}

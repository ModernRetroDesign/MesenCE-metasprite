using System;
using System.IO;
using System.Linq;
using Mesen.Debugger.Utilities;

internal static class NamedConditionTests
{
	private static void CheckAudioBindings()
	{
		foreach(string system in new[]{"Nes","Gameboy","Gba","PcEngine","Sms","Snes","Ws"}) {
			string hash=new('A',40);
			var library=new MetaNamedConditionLibrary {RomHash=hash,System=system=="PcEngine" ? "Pce" : system,Conditions=new() {
				new() {Name="WriteEvent",Address=0x652,Value=8}, new() {Name="Location",Address=0x760,Value=4}
			}};
			var pack=new MusicReplacementPack {RomHash=hash,System=system,Enabled=true,Rules=new() {
				new() {Id=1,Action=1,Trigger="[WriteEvent]",Conditions="[Location] != $03 / $0F"}
			}};
			byte[] named=pack.CreatePacket(".",library);
			string json=pack.ToJson(library);
			var reloaded=System.Text.Json.JsonSerializer.Deserialize(json,MusicReplacementJson.Default.MusicReplacementPack)!;
			Require(reloaded.Rules[0].Trigger=="[WriteEvent]" && reloaded.Rules[0].Conditions.Contains("[Location]"),system+" audio sources were discarded");
			pack.Rules[0].Trigger="0652 == 08"; pack.Rules[0].Conditions="0760 != 03 / 0F";
			Require(named.SequenceEqual(pack.CreatePacket(".")),system+" named audio packet differed from equivalent numeric event/gate");
			pack.Rules[0].Conditions="[Location]";
			Invalid(()=>pack.CreatePacket("."),system+" audio names silently accepted a missing library");
			var wrong=new MetaNamedConditionLibrary {RomHash=new('B',40),System=library.System,Conditions=library.Conditions};
			Invalid(()=>pack.CreatePacket(".",wrong),system+" audio accepted a different ROM library");
			pack.Rules[0].Conditions="[Missing]";
			Invalid(()=>pack.CreatePacket(".",library),system+" unresolved audio gate was accepted");
			if(system=="Snes") {
				pack.Rules[0].Conditions="[Location]"; pack.Rules[0].ContextSource=1;
				Invalid(()=>pack.CreatePacket(".",library),"main CPU alias became an SPC context address");
			}
			if(system is "Sms" or "Snes" or "Ws") {
				pack.Rules[0].Conditions=""; pack.Rules[0].ContextSource=0; pack.Rules[0].Trigger="[WriteEvent]"; pack.Rules[0].TriggerSource=1;
				Invalid(()=>pack.CreatePacket(".",library),system+" main CPU alias became an I/O or SPC write event");
			}
		}
		Console.WriteLine("PASS: named music/effect event and context bindings across seven systems, persisted expressions, byte-identical packets, missing/wrong libraries and CPU/SPC/I/O domain guards.");
	}
	private static void Require(bool condition,string reason) { if(!condition) { throw new Exception("Named conditions: "+reason); } }
	private static void Invalid(Action action,string reason)
	{
		bool rejected=false; try { action(); } catch(InvalidDataException) { rejected=true; }
		Require(rejected,reason);
	}
	public static void Run()
	{
		CheckAudioBindings();
		var library=new MetaNamedConditionLibrary {RomHash="fixture-rom",System="Nes",Conditions=new() {
			new() {Name="DialogBoxOpen",Address=0xDE,Value=2,Mask=255,Comparison=MetaMemoryComparison.Equal},
			new() {Name="Town1",Address=0x760,Value=1}, new() {Name="Town3",Address=0x760,Value=3},
			new() {Name="LocationName",Address=0x760},
			new() {Name="Masked",Address=0x652,Value=0xA0,Mask=0xF0,Comparison=MetaMemoryComparison.NotEqual}
		} };
		MetaMemoryCondition Parse(string text)=>MetaMemoryConditionText.Parse(text,library).Single();
		var dialog=Parse("[DialogBoxOpen]");
		Require(dialog.Address==0xDE && dialog.Value==2 && dialog.Mask==255 && dialog.Comparison==MetaMemoryComparison.Equal,"complete preset did not resolve");
		Require(Parse("[town1]").Address==Parse("[Town3]").Address && Parse("[Town1]").Value==1 && Parse("[Town3]").Value==3,"shared addresses or case-insensitive lookup lost distinct values");
		var alias=Parse("[LocationName] == $04 / $FF");
		Require(alias.Address==0x760 && alias.Value==4 && alias.Mask==255,"address-only alias did not use local fields");
		var overridden=Parse("[Masked] >= 0x04 / $0F");
		Require(overridden.Value==4 && overridden.Mask==15 && overridden.Comparison==MetaMemoryComparison.GreaterOrEqual,"explicit overrides were not applied");
		Require(Parse("[Masked]").Value==0xA0 && Parse("[Masked]").Mask==0xF0 && Parse("[Masked]").Comparison==MetaMemoryComparison.NotEqual,"local overrides changed the shared preset");
		Require(Parse("[Town1] !=").Value==1 && Parse("[Town1] !=").Comparison==MetaMemoryComparison.NotEqual,"operator-only override did not inherit the value");
		Require(Parse("[Town3] / $0F").Value==3 && Parse("[Town3] / $0F").Mask==15,"mask-only override did not inherit defaults");
		Require(Parse("[Masked] $04").Value==4 && Parse("[Masked] $04").Comparison==MetaMemoryComparison.NotEqual,"value-only override did not inherit the operator");
		var mixed=MetaMemoryConditionText.Parse("[DialogBoxOpen]\n$652 != $08 / $0F\n[Town3]",library);
		Require(mixed.Count==3 && mixed.Select(c=>c.Address).SequenceEqual(new uint[]{0xDE,0x652,0x760}),"mixed numeric and named conditions changed address selection");
		foreach(var comparison in Enum.GetValues<MetaMemoryComparison>()) {
			var condition=Parse("[LocationName] "+MetaMemoryConditionText.Operator(comparison)+" $20 / $FF");
			Require(condition.Comparison==comparison,"a comparison operator was lost");
			bool expected=comparison switch {MetaMemoryComparison.Equal=>false,MetaMemoryComparison.NotEqual=>true,MetaMemoryComparison.Greater=>true,MetaMemoryComparison.Less=>false,MetaMemoryComparison.GreaterOrEqual=>true,_=>false};
			Require(MetaMemoryCondition.Matches(new(){condition},_=>0x21)==expected,"resolved condition did not use existing comparison semantics");
		}
		foreach(string invalid in new[]{"[Unknown]","[LocationName]","[LocationName] ==","[Town1] == $100","[Town1] / $100","[Town1] == GG","[Town1] trailing","[[Town1]]","[ Town1]","[Town1] == $01 / $FF extra",new string(' ',4097)+"[Town1]"}) {
			Invalid(()=>Parse(invalid),"invalid reference or override was accepted: "+invalid[..Math.Min(invalid.Length,80)]);
		}
		Invalid(()=>MetaMemoryConditionText.Parse("[DialogBoxOpen]"),"an alias resolved without a library");
		Invalid(()=>MetaMemoryConditionText.Parse(string.Join('\n',Enumerable.Repeat("[Town1]",65)),library),"more than 64 named conditions were accepted");
		library.Conditions.Add(new(){Name="town1",Address=0xDE,Value=7}); Invalid(library.Validate,"duplicate names differing by case were accepted"); library.Conditions.RemoveAt(library.Conditions.Count-1);
		foreach(var (system,maximum) in new[]{("Nes",0xFFFFu),("Gameboy",0xFFFFu),("Pce",0xFFFFu),("Sms",0xFFFFu),("Snes",0xFFFFFFu),("Ws",0xFFFFFu),("Gba",uint.MaxValue)}) {
			var scoped=new MetaNamedConditionLibrary {RomHash="fixture-rom",System=system,Conditions=new(){new(){Name="Limit",Address=maximum,Value=1}}};
			Require(scoped.ResolveExpression("[Limit]").Address==maximum,"a valid CPU address was rejected");
			if(maximum<uint.MaxValue) { scoped.Conditions[0].Address=maximum+1; Invalid(scoped.Validate,"out-of-domain CPU address was accepted"); }
		}
		library.Conditions[0].MemoryDomain="Ppu"; Invalid(library.Validate,"a physical/PPU domain was silently treated as CPU memory"); library.Conditions[0].MemoryDomain="Cpu";
		string directory=Path.GetFullPath(Path.Combine("Tests","MetaSprites","obj","named-conditions-"+Guid.NewGuid().ToString("N")));
		string path=Path.Combine(directory,MetaNamedConditionLibrary.FileName);
		try {
			library.Save(path); string snapshot=File.ReadAllText(path);
			var loaded=MetaNamedConditionLibrary.Load(path,"fixture-rom","Nes");
			Require(loaded.Conditions.Count==5 && loaded.Conditions.Single(c=>c.Name=="LocationName").Value==null && loaded.ResolveExpression("[Town3]").Value==3,"preset persistence lost aliases or values");
			Invalid(()=>MetaNamedConditionLibrary.Load(path,"different-rom","Nes"),"cross-ROM library was accepted");
			Invalid(()=>MetaNamedConditionLibrary.Load(path,"fixture-rom","Snes"),"cross-system library was accepted");
			loaded.Conditions.Single(c=>c.Name=="Town3").Value=4; loaded.Save(path,snapshot); string changed=File.ReadAllText(path);
			Invalid(()=>library.Save(path,snapshot),"a stale editor overwrote newer definitions"); Require(File.ReadAllText(path)==changed,"stale save modified the library");
			Require(MetaNamedConditionLibrary.Load(path,"fixture-rom","Nes").ResolveExpression("[Town3]").Value==4,"reloading used a stale saved value");
			using(var held=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read)) {
				bool failed=false; try { loaded.Save(path,changed); } catch(Exception ex) when(ex is IOException or UnauthorizedAccessException) { failed=true; }
				Require(failed && File.ReadAllText(path)==changed,"failed atomic save damaged the library");
			}
			Require(!Directory.GetFiles(directory,"*.tmp.*").Any(),"failed save left a temporary library behind");
			loaded.Conditions.RemoveAll(c=>c.Name=="Town3"); Invalid(()=>loaded.ResolveExpression("[Town3]"),"a deleted reference fell back to cached values");
			var group=new MetaSpriteGroup {Id=1,Enabled=true,Width=8,Height=8,MemoryConditionExpression="[Town3]",MemoryConditions=new(){new(){Address=0x1111,Value=99}},
				Members=new(){new(){Width=8,Height=8,Pixels=new uint[64]}},AlternateArtwork=new(){new(){Path="alternate.png",MemoryConditionExpression="[LocationName] == $04 / $FF"}}};
			var background=new MetaBackgroundLayer {Id=1,Enabled=true,ArtworkWidth=8,ArtworkHeight=8,ArtworkPath="background.png",MemoryConditionExpression="[DialogBoxOpen]",
				UseWidescreenPosition=true,WidescreenPosition=new() {UseRamFlags=true,LeftFlags="[Town3]",RightFlags="[LocationName] == $09 / $0F"}};
			var profile=new MetaTextProfile {Id=1,Enabled=true,FontPath="font.ttf",RasterScale=1,Proportional=false,MemoryConditionExpression="[Masked] >= $04 / $0F",Mappings=new(){new(){Tile=1,Character="A"}}};
			var pack=new MetaSpritePack {RomHash="fixture-rom",System="Nes",Groups=new(){group},BackgroundLayers=new(){background},TextProfiles=new(){profile}};
			string packPath=Path.Combine(directory,"groups.json"); pack.Save(packPath);
			var restored=MetaSpritePack.Load(packPath,"fixture-rom","Nes");
			var edge=restored.BackgroundLayers[0].WidescreenPosition;
			Require(edge.LeftFlags=="[Town3]" && edge.ParseFlags(edge.LeftFlags).Single().Address==0x760 && edge.ParseFlags(edge.RightFlags).Single().Value==9,"boundary references did not persist and resolve");
			Require(restored.Groups[0].MemoryConditions.Single().Value==4 && restored.Groups[0].AlternateArtwork[0].MemoryConditions.Single().Value==4
				&& restored.BackgroundLayers[0].MemoryConditions.Single().Value==2 && restored.TextProfiles[0].MemoryConditions.Single().Mask==15,"saved references did not compile across all graphics/text owners");
			Require(MetaNamedConditionBindings.Text(restored.Groups[0])=="[Town3]" && MetaNamedConditionBindings.Text(restored.Groups[0].AlternateArtwork[0])=="[LocationName] == $04 / $FF","pack reload replaced reference expressions with numeric cache text");
			string spriteSnapshot=restored.GetSpriteDefinitionsSnapshot();
			var editedLibrary=MetaNamedConditionLibrary.Load(path,"fixture-rom","Nes"); string librarySnapshot=File.ReadAllText(path);
			editedLibrary.Conditions.Single(c=>c.Name=="Town3").Value=5; editedLibrary.Save(path,librarySnapshot);
			var updated=MetaSpritePack.Load(packPath,"fixture-rom","Nes");
			var refreshedEdge=updated.BackgroundLayers[0].WidescreenPosition;
			Require(refreshedEdge.ParseFlags(refreshedEdge.LeftFlags).Single().Value==5,"boundary cache ignored a library update");
			Require(updated.Groups[0].MemoryConditions.Single().Value==5 && updated.GetSpriteDefinitionsSnapshot()==spriteSnapshot,"library updates were stale or falsely marked sprite definitions as concurrently edited");
			var numeric=MetaSpritePack.Load(packPath,"fixture-rom","Nes"); foreach(var owner in numeric.ConditionOwners()) { owner.MemoryConditionExpression=null; }
			(int,int,uint[]) Artwork()=> (8,8,Enumerable.Repeat(0xFFFFFFFFu,64).ToArray());
			Require(MetaSpriteRuntimePacket.Create(updated,_=>Artwork(),_=>Artwork()).SequenceEqual(MetaSpriteRuntimePacket.Create(numeric,_=>Artwork(),_=>Artwork())),"named sprite/alternate references changed the numeric packet schema");
			Require(MetaBackgroundRuntimePacket.Create(updated,_=>Artwork()).SequenceEqual(MetaBackgroundRuntimePacket.Create(numeric,_=>Artwork())),"named background references changed the numeric packet schema");
			Require(MetaTextRuntimePacket.Create(updated,_=>new[]{new MetaTextGlyph("A",new uint[64])}).SequenceEqual(MetaTextRuntimePacket.Create(numeric,_=>new[]{new MetaTextGlyph("A",new uint[64])})),"named text references changed the numeric packet schema");
			var updatedEdge=updated.BackgroundLayers[0].WidescreenPosition;
			updatedEdge.LeftFlags="[MissingBoundary]";
			Invalid(()=>MetaBackgroundRuntimePacket.Create(updated,_=>Artwork()),"edited boundary used stale compiled values");
			Invalid(()=>updated.ResolveNamedConditions(directory),"missing boundary name was accepted");
			updatedEdge.LeftFlags=string.Join("\n",Enumerable.Repeat("[Town3]",9));
			Invalid(()=>updated.ResolveNamedConditions(directory),"more than eight named boundary conditions were accepted");
			updatedEdge.LeftFlags="[Town3]"; updated.ResolveNamedConditions(directory);
			// Management can inspect source references without requiring their library.
			var raw=MetaSpritePack.Load(packPath,"fixture-rom","Nes",resolveNamedConditions:false);
			Require(raw.BackgroundLayers[0].WidescreenPosition.RightFlags.Contains("[LocationName]"),"raw reference management discarded boundary sources");
			updated.Groups[0].MemoryConditionExpression="[Deleted]";
			int imageLoads=0;
			Invalid(()=>MetaSpriteRuntimePacket.Create(updated,_=>{imageLoads++; return Artwork();}),"an unresolved reference used cached sprite conditions");
			Invalid(()=>MetaBackgroundRuntimePacket.Create(updated,_=>Artwork()),"an unresolved pack reference reached background packet output");
			Invalid(()=>MetaTextRuntimePacket.Create(updated,_=>Array.Empty<MetaTextGlyph>()),"an unresolved pack reference reached text packet output");
			Require(imageLoads==0,"unresolved references loaded PNG assets before validation");
			int cached=updated.Groups[0].MemoryConditions.Single().Value;
			Invalid(()=>updated.ResolveNamedConditions(directory),"a deleted name resolved to cached values");
			Require(!updated.Groups[0].NamedConditionsResolved && updated.Groups[0].MemoryConditions.Single().Value==cached,"failed resolution did not invalidate its compiled cache safely");
			string savedPack=File.ReadAllText(packPath); Invalid(()=>updated.Save(packPath),"invalid expressions were saved over working definitions"); Require(File.ReadAllText(packPath)==savedPack,"failed reference save changed the pack");
			File.Delete(path); Invalid(()=>MetaSpritePack.Load(packPath,"fixture-rom","Nes"),"a missing library fell back to cached pack conditions");
		} finally { if(Directory.Exists(directory)) { Directory.Delete(directory,true); } }
		Console.WriteLine("PASS: named CPU presets/overrides, domain identity, library/pack persistence, live re-resolution, stable edit snapshots, unchanged numeric sprite/background/text/boundary packets, boundary limits, fail-closed caches and safe atomic saves.");
	}
}

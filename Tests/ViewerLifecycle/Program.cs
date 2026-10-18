using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.Loader;
using System.Reflection;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Threading;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Mesen.Debugger.ViewModels;
using Mesen.Debugger.Utilities;
using Mesen.Interop;
using Mesen.Utilities;
using Mesen.Config;
using Mesen.Debugger.Controls;
using Mesen.Controls;

internal static class Program
{
	private static string _textCorePath="";
	[DllImport("MesenCore.dll")] private static extern uint RunMetaTextSystemTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename,byte[] packet,uint length);
	private static void CheckTextSystems()
	{
		string directory=Path.Combine(AppContext.BaseDirectory,"artifacts","text-systems-"+Guid.NewGuid().ToString("N")); Directory.CreateDirectory(directory);
		File.Copy(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Fonts),"arial.ttf"),Path.Combine(directory,"font.ttf"));
		foreach(var fixture in new[] {("gb","Gameboy",0), ("gbc","Gameboy",0), ("window-scroll.gbc","Gameboy",1), ("sms","Sms",0), ("background.pce","Pce",0), ("background-scroll.gba","Gba",0), ("background-sub-bg0-mode1-scroll.sfc","Snes",0), ("background-sub-bg1-scroll.sfc","Snes",1), ("background-sub-bg2-scroll.sfc","Snes",2), ("background-sub-bg3-scroll.sfc","Snes",3), ("background-scroll.wsc","Ws",0), ("background-window.wsc","Ws",1)}) {
			foreach(int cellHeight in new[] {8,16}) { foreach(bool address in new[] {false,true}) {
				uint ram=fixture.Item2 switch {"Gameboy" or "Sms" => 0xC652,"Gba" => 0x02000652,"Snes" => 0x7E0652,"Pce" => 0x2652,_ => 0x652};
				var p=new MetaTextProfile {Id=1,Enabled=true,FontPath="font.ttf",FontSize=12,RasterScale=2,Proportional=true,NativeLayer=fixture.Item3,CellHeight=cellHeight,
					UsePaletteColor=false,Color="30E070C0",OutlineEnabled=true,OutlineColor="FF0000FF",OutlineWidth=.5,ShadowEnabled=true,ShadowColor="0000FF80",ShadowX=1,ShadowY=1,
					UseDisplayRegion=true,DisplayX=fixture.Item2=="Pce" ? 40 : 8,DisplayY=8,DisplayWidth=40,DisplayHeight=32,WrapText=false,FillEnabled=address,FillColor="10182080",MatchKind=address ? MetaTextMatchKind.GraphicsAddress : MetaTextMatchKind.TileIndex,
					Mappings=new() {new() {Tile=0,TilePattern=cellHeight==16 ? new uint[] {0,0} : Array.Empty<uint>(),Character="A"}},MemoryConditions=new() {new() {Address=ram,Value=0}}};
				var budget=new MetaTextRasterizer.Budget(); byte[] packet=MetaTextRuntimePacket.Create(new MetaSpritePack {System=fixture.Item2,TextProfiles=new() {p}},profile => budget.Rasterize(profile,directory));
				string path=Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture."+fixture.Item1); uint result=RunMetaTextSystemTest(path,packet,(uint)packet.Length);
				if(result!=0) { throw new Exception($"Text adapter {fixture.Item1}, layer={fixture.Item3}, height={cellHeight}, address={address}, failed at {result}."); }
				Console.WriteLine($"PASS: {fixture.Item1} layer {fixture.Item3}, source 8x{cellHeight}, {(address ? "graphics addresses/fill" : "indices")}, actual TrueType glyph/effects/display region, full-frame pixel comparison and RAM disable/reactivation/clear.");
			} }
		}
		foreach(var system in new[] {("gb",CpuType.Gameboy),("background-scroll.gba",CpuType.Gba),("background.pce",CpuType.Pce),("sms",CpuType.Sms),("background-sub-bg0-mode1-scroll.sfc",CpuType.Snes),("background-window.wsc",CpuType.Ws)}) {
			// The desktop native singleton is initialized/released once per process.
			var start=new ProcessStartInfo(Environment.ProcessPath!) {UseShellExecute=false,CreateNoWindow=true};
			if(Path.GetFileNameWithoutExtension(Environment.ProcessPath)=="dotnet") { start.ArgumentList.Add(Assembly.GetExecutingAssembly().Location); }
			foreach(string arg in new[] {"--text-apply-system",system.Item2.ToString(),Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture."+system.Item1),Path.Combine(directory,"font.ttf"),"--native-core",_textCorePath}) { start.ArgumentList.Add(arg); }
			using var child=Process.Start(start)!; if(!child.WaitForExit(60000) || child.ExitCode!=0) { throw new Exception("Text editor activation failed for "+system.Item2); }
		}
	}
	[DllImport("MesenCore.dll")]
	private static extern void ConfigureMetaSoftwareRendererTest();
	[DllImport("MesenCore.dll")] private static extern uint RunMetaSpriteSnesAuthoredPackTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename, byte[] packet, uint length);
	[DllImport("MesenCore.dll")] private static extern uint RunMetaTextNesTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename,byte[] packet,uint length);
	[DllImport("MesenCore.dll")] private static extern uint GetMetaReplacementDefinitionFlagsTest();
	[DllImport("MesenCore.dll")] private static extern uint RunMetaAnimationStateTest(byte[] sprites,uint spriteLength,byte[] backgrounds,uint backgroundLength);
	[DllImport("MesenCore.dll")] private static extern uint RunMetaBackgroundSnesAnimationTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename, byte[] packet, uint length);
	[DllImport("MesenCore.dll")]
	[return: MarshalAs(UnmanagedType.I1)] private static extern bool CheckWidescreenPresentationInterop();
	[DllImport("MesenCore.dll")] private static extern uint RunWsMusicReplacementChecks(byte[] packet, uint length, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")] private static extern uint RunSnesMusicReplacementChecks(byte[] packet, uint length, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")] private static extern uint RunSmsMusicReplacementChecks(byte[] packet, uint length, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")] private static extern uint RunPceMusicReplacementChecks(byte[] packet, uint length, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")] private static extern uint RunGbaMusicReplacementChecks(byte[] packet, uint length, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")] private static extern uint RunGbMusicReplacementChecks(byte[] packet, uint length, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")] private static extern uint RunMusicReplacementChecks(byte[] packet, uint length, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[STAThread]
	private static void Main(string[] args)
	{
		int coreIndex = Array.IndexOf(args, "--native-core");
		if(coreIndex >= 0) {
			_textCorePath=Path.GetFullPath(args[coreIndex+1]); IntPtr core = NativeLibrary.Load(_textCorePath);
			NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? core : IntPtr.Zero);
			NativeLibrary.SetDllImportResolver(typeof(EmuApi).Assembly, (name, _, _) => name == "MesenCore.dll" ? core : IntPtr.Zero);
		}
		AssemblyLoadContext.Default.Resolving += (context, name) => {
			string path = Path.Combine(AppContext.BaseDirectory, name.Name + ".dll");
			return File.Exists(path) ? context.LoadFromAssemblyPath(path) : null;
		};
		int pointerIndex = Array.IndexOf(args, "--pointer-suite");
		if(pointerIndex >= 0 || Array.IndexOf(args, "--widescreen-profile") >= 0 || Array.IndexOf(args, "--music-replacement") >= 0 || Array.IndexOf(args,"--text-replacement")>=0 || Array.IndexOf(args,"--text-gameplay")>=0 || Array.IndexOf(args,"--text-systems")>=0 || Array.IndexOf(args,"--text-apply-system")>=0) {
			string uiHome = Path.Combine(AppContext.BaseDirectory, "artifacts", "pointer-ui-" + Guid.NewGuid().ToString("N"));
			Directory.CreateDirectory(uiHome);
			typeof(ConfigManager).GetField("_homeFolder", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, uiHome);
			typeof(ConfigManager).GetField("_config", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, Configuration.CreateConfig());
			AppBuilder.Configure<Mesen.App>().UsePlatformDetect().SetupWithoutStarting();
		} else { AppBuilder.Configure<Application>().UsePlatformDetect().SetupWithoutStarting(); }
		if(Array.IndexOf(args, "--widescreen-profile") >= 0) { CheckWidescreenProfile(); return; }
		if(Array.IndexOf(args, "--music-replacement") >= 0) { CheckMusicReplacement(); return; }
		if(Array.IndexOf(args,"--text-replacement")>=0) { CheckTextReplacement(); return; }
		if(Array.IndexOf(args,"--text-gameplay")>=0) { CheckTextGameplay(); return; }
		if(Array.IndexOf(args,"--text-systems")>=0) { CheckTextSystems(); return; }
		int textApply=Array.IndexOf(args,"--text-apply-system"); if(textApply>=0) { CheckTextRuntimeApply(args[textApply+2],args[textApply+3],Enum.Parse<CpuType>(args[textApply+1])); return; }
		if(Array.IndexOf(args, "--widescreen-settings") >= 0) {
			var video = new VideoConfig {
				DecorativeWidescreen = true, DecorativeRatioPreset = 2, DecorativeAspectRatio = 2.25,
				DecorativeSideBrightness = 37, DecorativeViewportPosition = 3, DecorativeCustomWidths = true,
				DecorativeLeftWidth = 23, DecorativeRightWidth = 71, DecorativeFadeWidth = 12
			};
			var saved = video.Clone();
			if(!video.IsIdentical(saved) || !saved.DecorativeWidescreen || !saved.CustomDecorativeRatio
				|| saved.DecorativeSideBrightness != 37 || saved.DecorativeViewportPosition != 3
				|| !saved.DecorativeCustomWidths || saved.DecorativeLeftWidth != 23 || saved.DecorativeRightWidth != 71
				|| saved.DecorativeFadeWidth != 12) {
				throw new Exception("Widescreen settings did not survive configuration serialization.");
			}
			Console.WriteLine("PASS: widescreen ratio, arbitrary brightness, asymmetric widths, fade and gameplay selector persist through configuration serialization.");
			return;
		}
		if(pointerIndex >= 0) { CheckSelection(pointerIndex + 1 < args.Length ? args[pointerIndex + 1] : "Nes", false, false, true); return; }
		int coldIndex = Array.IndexOf(args, "--cold-start");
		int stateIndex = Array.IndexOf(args, "--state");
		int settingsIndex = Array.IndexOf(args, "--settings");
		if(coldIndex >= 0) { CheckColdStartup(args[coldIndex + 1], args[coldIndex + 2], coldIndex + 3 < args.Length && !args[coldIndex + 3].StartsWith("--") ? args[coldIndex + 3] : "Tests/MetaSprites/obj/gameplay-fixture.sfc", stateIndex >= 0 ? args[stateIndex + 1] : null, Array.IndexOf(args, "--scroll-turn") >= 0, settingsIndex >= 0 ? args[settingsIndex + 1] : null, Array.IndexOf(args, "--software-renderer") >= 0); return; }
		if(Array.IndexOf(args, "--preview-benchmark") >= 0) { BenchmarkPreviews(); return; }
		int selectionIndex = Array.IndexOf(args, "--selection-suite");
		if(selectionIndex >= 0) { CheckSelection(selectionIndex + 1 < args.Length ? args[selectionIndex + 1] : "Snes", Array.IndexOf(args, "--startup-timing") >= 0, Array.IndexOf(args, "--recognition-benchmark") >= 0); return; }
		var model = new SpritePreviewModel();
		var info = new DebugSpriteInfo { Width = 8, Height = 8, SpriteIndex = 0 };
		var layout = new DebugSpritePreviewInfo();
		uint[] pixels = new uint[128 * 128];
		model.Init(ref info, pixels, layout);
		DynamicBitmap original = model.SpritePreview ?? throw new Exception("Preview missing.");
		int invalidations = 0;
		original.Invalidated += (_, _) => invalidations++;
		model.Init(ref info, pixels, layout);
		if(!ReferenceEquals(original, model.SpritePreview)) { throw new Exception("Unchanged dimensions replaced the bitmap."); }
		if(invalidations != 0) { throw new Exception("Unchanged pixels invalidated the preview."); }
		pixels[0] = 0xFF123456;
		pixels[63] = 0xFFABCDEF;
		model.Init(ref info, pixels, layout);
		CheckPreviewPixels(original, pixels, 64);
		if(invalidations != 1) { throw new Exception("Changed pixels did not invalidate exactly once."); }
		model.Init(ref info, pixels, layout);
		if(invalidations != 1) { throw new Exception("The updated pixel cache did not retain its contents."); }
		pixels[0] = 0;
		pixels[63] = 0;
		model.Init(ref info, pixels, layout);
		CheckPreviewPixels(original, pixels, 64);
		if(invalidations != 2) { throw new Exception("Returning to transparent pixels did not update the preview."); }
		info.Width = 16;
		model.Init(ref info, pixels, layout);
		if(ReferenceEquals(original, model.SpritePreview)) { throw new Exception("Dimension change did not replace the bitmap."); }
		CheckReleased(original);
		DynamicBitmap replacement = model.SpritePreview ?? throw new Exception("Replacement preview missing.");
		CheckPreviewPixels(replacement, pixels, 128);
		using(var bitmapLock = replacement.Lock()) { }
		model.ReleasePreview();
		if(model.SpritePreview != null) { throw new Exception("Release did not clear the binding."); }
		CheckReleased(replacement);
		model.ReleasePreview();
		Console.WriteLine("PASS: real Avalonia bitmap retention, dimension replacement, disposal and repeated release; no window created.");
		Console.WriteLine("PASS: unchanged previews stay valid; first/last pixel changes, transparency restoration and resize update the actual bitmap.");
		CheckGroupPng();
		CheckCreationSave();
		CheckGroupLabelBinding();
	}

	private static void CheckTextGameplay()
	{
		string directory=Path.Combine(AppContext.BaseDirectory,"artifacts","text-gameplay-"+Guid.NewGuid().ToString("N")); Directory.CreateDirectory(directory);
		string romPath=Path.Combine(directory,"dialog-mapper-0.nes");
		byte[] rom=new byte[16+16384+8192]; new byte[] {0x4E,0x45,0x53,0x1A,1,1}.CopyTo(rom,0);
		var code=new System.Collections.Generic.List<byte>(new byte[] {0x78,0xD8,0xA2,0xFF,0x9A});
		void Store(ushort address,byte value) => code.AddRange(new byte[] {0xA9,value,0x8D,(byte)address,(byte)(address>>8)});
		void Ppu(ushort address,byte value) { Store(0x2006,(byte)(address>>8)); Store(0x2006,(byte)address); Store(0x2007,value); }
		Store(0x2000,0); Store(0x2001,0); code.AddRange(new byte[] {0x2C,0x02,0x20,0x10,0xFB,0x2C,0x02,0x20,0x10,0xFB});
		code.AddRange(new byte[] {0xA2,0,0xA9,0xFF,0x9D,0,2,0xE8,0xD0,0xFA});
		for(int i=0;i<2;i++) { Store((ushort)(0x200+i*4),31); Store((ushort)(0x201+i*4),3); Store((ushort)(0x202+i*4),(byte)(i==0 ? 0 : 0x20)); Store((ushort)(0x203+i*4),(byte)(i==0 ? 40 : 32)); }
		Store(0x2003,0); Store(0x4014,2);
		byte[] palette={0,1,2,3,0,4,2,3}; for(int i=0;i<palette.Length;i++) { Ppu((ushort)(0x3F00+i),palette[i]); } Ppu(0x3F11,3);
		for(int row=0;row<2;row++) { for(int col=0;col<4;col++) { Ppu((ushort)(0x2000+(row+4)*32+col+4),(byte)((row+col)%2==0 ? 1 : 2)); } }
		Ppu(0x23C9,4); Store(0x652,8); Store(0x760,4);
		Store(0x2006,0x20); Store(0x2006,0); int scrollWrites=code.Count; Store(0x2005,0); Store(0x2005,0); Store(0x2000,0); Store(0x2001,0x1E);
		ushort loop=(ushort)(0x8000+code.Count); code.AddRange(new byte[] {0x4C,(byte)loop,(byte)(loop>>8)}); code.CopyTo(rom,16);
		for(int vector=0x3FFA;vector<0x4000;vector+=2) { rom[16+vector]=0; rom[16+vector+1]=0x80; }
		for(int row=0;row<8;row++) { rom[16+16384+16+row]=255; rom[16+16384+32+8+row]=255; rom[16+16384+48+row]=255; }
		File.WriteAllBytes(romPath,rom);
		string scrollRom=Path.Combine(directory,"dialog-mapper-0-scroll.nes"); byte[] scrolled=(byte[])rom.Clone(); scrolled[16+scrollWrites+1]=5; scrolled[16+scrollWrites+6]=3; File.WriteAllBytes(scrollRom,scrolled);
		string font=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Fonts),"consola.ttf"); File.Copy(font,Path.Combine(directory,"dialog.ttf"));
		foreach(bool scrolling in new[] {false,true}) { foreach(bool graphics in new[] {false,true}) { foreach(bool inherit in new[] {true,false}) { foreach(bool fill in new[] {false,true}) {
			var profile=new MetaTextProfile {Id=1,Enabled=true,FontPath="dialog.ttf",RasterScale=2,FontSize=8,UsePaletteColor=inherit,Color="20E080C0",FillEnabled=fill,FillColor="102040B0",
				MatchKind=graphics ? MetaTextMatchKind.GraphicsAddress : MetaTextMatchKind.TileIndex,UseRegion=true,X=32,Y=32,Width=32,Height=16,
				Mappings=new() {new() {Tile=graphics ? 16u : 1u,Character="A"},new() {Tile=graphics ? 32u : 2u,Character="B"},new() {Tile=graphics ? 16u : 1u,Palette=1,Character="C"}},
				MemoryConditions=MetaMemoryConditionText.Parse("0652 == 08\n0760 != 06")};
			var pack=new MetaSpritePack {System="Nes",TextProfiles=new() {profile}}; var budget=new MetaTextRasterizer.Budget();
			byte[] packet=MetaTextRuntimePacket.Create(pack,p => budget.Rasterize(p,directory));
			uint result=RunMetaTextNesTest(scrolling ? scrollRom : romPath,packet,(uint)packet.Length);
			if(result!=31) { throw new Exception($"NES live TrueType overlay failed: scrolling={scrolling}, graphics={graphics}, inherited={inherit}, fill={fill}, flags={result:X}/1F."); }
			Console.WriteLine($"PASS: NES mapper 0 cached font overlay, {(scrolling ? "fine X/Y scroll" : "static")}, {(graphics ? "graphics addresses" : "tile indices")}, {(inherit ? "native palette" : "custom partial-alpha color")}, fill={fill}, original-pixel retention, two-byte RAM gating/reactivation and disable restoration.");
		} } } }
		string proportionalFont=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Fonts),"arial.ttf"); File.Copy(proportionalFont,Path.Combine(directory,"proportional.ttf"));
		foreach(bool fill in new[] {false,true}) {
			var profile=new MetaTextProfile {Id=1,Enabled=true,FontPath="proportional.ttf",FontSize=20,RasterScale=4,Proportional=true,UseDisplayRegion=true,DisplayX=20,DisplayY=76,DisplayWidth=100,DisplayHeight=64,WrapText=false,
				OutlineEnabled=true,OutlineWidth=0.75,OutlineColor="FF0000FF",ShadowEnabled=true,ShadowX=2,ShadowY=2,ShadowColor="0000FF80",UsePaletteColor=false,Color="30E070C0",FillEnabled=fill,FillColor="101820C0",
				UseRegion=true,X=32,Y=32,Width=32,Height=16,MemoryConditions=MetaMemoryConditionText.Parse("0652 == 08\n0760 != 06"),
				Mappings=new() {new() {Tile=1,Character="A"},new() {Tile=2,Character="V"},new() {Tile=1,Palette=1,Character="T"}}};
			var pack=new MetaSpritePack {System="Nes",TextProfiles=new() {profile}}; var budget=new MetaTextRasterizer.Budget();
			byte[] packet=MetaTextRuntimePacket.Create(pack,p => budget.Rasterize(p,directory)); uint result=RunMetaTextNesTest(scrollRom,packet,(uint)packet.Length);
			if(result!=31) { throw new Exception($"Live proportional font/kerning/effects failed: fill={fill}, flags={result:X}/1F."); }
			Console.WriteLine($"PASS: NES mapper 0 large proportional TrueType glyphs, font kerning, moved display region, independent red outline/blue shadow, fill={fill}, fine source scrolling and RAM reactivation/restoration.");
		}
		CheckTextRuntimeApply(romPath,font);
		foreach(var size in new[] {(W:8,H:16),(W:16,H:8),(W:16,H:16)}) { foreach(bool fill in new[] {false,true}) {
			uint[] Pattern(uint first) => Enumerable.Range(0,size.W/8*size.H/8).Select(i => ((i%(size.W/8)+i/(size.W/8))%2==0) ? first : 3-first).ToArray();
			var p=new MetaTextProfile {Id=1,Enabled=true,FontPath="proportional.ttf",FontSize=20,RasterScale=4,Proportional=true,CellWidth=size.W,CellHeight=size.H,
				UseDisplayRegion=true,DisplayX=20,DisplayY=76,DisplayWidth=100,DisplayHeight=64,WrapText=false,UseRegion=true,X=32,Y=32,Width=32,Height=16,
				OutlineEnabled=true,OutlineWidth=.75,OutlineColor="FF0000FF",ShadowEnabled=true,ShadowX=2,ShadowY=2,ShadowColor="0000FF80",UsePaletteColor=false,Color="30E070C0",FillEnabled=fill,FillColor="101820C0",
				MemoryConditions=MetaMemoryConditionText.Parse("0652 == 08\n0760 != 06"),Mappings=new() {new() {Tile=1,TilePattern=Pattern(1),Character="A"},new() {Tile=2,TilePattern=Pattern(2),Character="V"},new() {Tile=1,TilePattern=Pattern(1),Palette=1,Character="T"}}};
			var budget=new MetaTextRasterizer.Budget(); byte[] bytes=MetaTextRuntimePacket.Create(new MetaSpritePack {System="Nes",TextProfiles=new() {p}},profile => budget.Rasterize(profile,directory));
			uint flags=RunMetaTextNesTest(scrollRom,bytes,(uint)bytes.Length); if(flags!=31) { throw new Exception($"Live multi-tile source cells failed: {size.W}x{size.H}, fill={fill}, flags={flags:X}."); }
			Console.WriteLine($"PASS: NES mapper 0 {size.W}x{size.H} source cells, complete index-pattern matching, exact/wildcard palettes, one emitted glyph/cell, fine scrolling, fill={fill}, RAM restoration and paused presentation snapshots.");
		} }
		Console.WriteLine("PASS: requested gameplay snapshots match native pixels while paused, including widened and rotated presentation in all 24 NES text fixtures.");
	}
	private static void CheckTextRuntimeApply(string romPath,string font,CpuType cpu=CpuType.Nes)
	{
		string home=ConfigManager.HomeFolder;
		EmuApi.InitDll(); EmuApi.InitializeEmu(home,IntPtr.Zero,IntPtr.Zero,true,true,true,true); ConfigManager.Config.ApplyConfig();
		try {
			if(!EmuApi.LoadRom(romPath)) { throw new Exception("Text authoring fixture failed to load."); } EmuApi.Pause(); DebugApi.InitializeDebugger();
			using var viewer=new TilemapViewerViewModel(cpu,new PictureViewer(),new ScrollPictureViewer(),new Window());
			var loader=typeof(MetaSpritePack).Assembly.GetType("Mesen.Debugger.Utilities.MetaSpriteRuntimeLoader")!;
			string romHash=EmuApi.GetRomHash(HashType.Sha1),packPath=(string)loader.GetMethod("GetPackPath",BindingFlags.Static|BindingFlags.Public|BindingFlags.NonPublic)!.Invoke(null,new object[] {romHash,cpu})!;
			bool ApplyText(MetaSpritePack pack) { try { return (bool)loader.GetMethod("Apply",BindingFlags.Static|BindingFlags.Public|BindingFlags.NonPublic)!.Invoke(null,new object[] {pack,packPath})!; } catch(TargetInvocationException ex) when(ex.InnerException!=null) { throw ex.InnerException; } }
			using var editor=cpu==CpuType.Nes ? viewer.CreateTextReplacementEditor() : new TextReplacementViewModel(packPath,romHash,cpu.ToString(),viewer.BackgroundEditor.HardwareLayerNames,0,1,16,0,() => EmuApi.GetRomHash(HashType.Sha1)==romHash,ApplyText);
			if(!editor.ImportFont(font)) { throw new Exception(editor.Status); }
			editor.SelectedProfile!.Enabled=true; editor.MappingText="0001 = A\n0002 = B"; editor.ConditionsText="0652 == 08";
			var window=new Mesen.Debugger.Windows.TextReplacementWindow(editor);
			window.FindControl<Button>("textApply")!.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent)); Dispatcher.UIThread.RunJobs();
			if(!editor.Status.StartsWith("Text profiles saved and applied") || GetMetaReplacementDefinitionFlagsTest()!=1) { throw new Exception("Actual text Save and apply failed: "+editor.Status); }
			string directory=editor.DirectoryPath,hash=EmuApi.GetRomHash(HashType.Sha1);
			string sprites=Path.Combine(directory,"empty-sprites.bin"),backgrounds=Path.Combine(directory,"empty-backgrounds.bin"),texts=Path.Combine(directory,"invalid-text.bin");
			File.WriteAllBytes(sprites,new byte[] {0x4D,0x53,0x50,0x31,0,0,0,0}); File.WriteAllBytes(backgrounds,new byte[] {0x4D,0x42,0x47,0x31,0,0,0,0}); File.WriteAllBytes(texts,new byte[] {0});
			if(DebugApi.SetMetaReplacementFilesWithText(hash,sprites,backgrounds,texts) || GetMetaReplacementDefinitionFlagsTest()!=1) { throw new Exception("Malformed text file replaced installed definitions."); }
			if(DebugApi.SetMetaReplacementFilesWithText("wrong-rom",sprites,backgrounds,texts) || GetMetaReplacementDefinitionFlagsTest()!=1) { throw new Exception("Wrong-ROM text activation changed installed definitions."); }
			editor.MappingText="0001 = U+10FFFF";
			if(editor.SaveAndApply() || GetMetaReplacementDefinitionFlagsTest()!=1 || !editor.Status.Contains("no glyph")) { throw new Exception("Missing glyph was accepted or cleared installed artwork: "+editor.Status); }
			editor.MappingText="0001 = A\n0002 = B";
			if(!editor.SaveAndApply()) { throw new Exception("Text apply retry failed: "+editor.Status); }
			editor.SelectedProfile.Enabled=false;
			if(!editor.SaveAndApply() || GetMetaReplacementDefinitionFlagsTest()!=0) { throw new Exception("Disabling all text failed to clear native definitions."); }
			window.Close();
			Console.WriteLine($"PASS: {cpu} text editor Save and apply, three-file native activation, malformed/wrong-ROM retention, missing-glyph rollback/retry and disabled-pack clearing.");
		} finally { DebugApi.ReleaseDebugger(); EmuApi.Stop(); EmuApi.Release(); }
	}

	private static void CheckTextReplacement()
	{
		string directory=Path.Combine(AppContext.BaseDirectory,"artifacts","text-authoring-"+Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(directory);
		string path=Path.Combine(directory,"pack.json"); bool sameGame=true;
		new MetaSpritePack {RomHash="text-fixture",System="Nes",NextBackgroundId=7}.Save(path);
		using var editor=new TextReplacementViewModel(path,"text-fixture","Nes",new[] {"Background"},0,0x2A,0x1020,3,() => sameGame);
		var window=new Mesen.Debugger.Windows.TextReplacementWindow(editor);
		window.Measure(new Size(820,800)); window.Arrange(new Rect(0,0,820,800)); Dispatcher.UIThread.RunJobs();
		using(var source=new WriteableBitmap(new PixelSize(512,256),new Vector(96,96),PixelFormat.Bgra8888,AlphaFormat.Opaque)) {
			using var png=new MemoryStream(); source.Save(png,PngBitmapEncoderOptions.Default); png.Position=0;
			editor.SetSourcePreview(new Bitmap(png),new Rect(13,19,18,10)); editor.UseViewerSelection();
			if(editor.SourceRegion!=new Rect(8,16,24,16) || !editor.SelectedProfile!.UseRegion) { throw new Exception("Source selection did not expand to tile boundaries."); }
			var sourcePicker=window.FindControl<Mesen.Debugger.Controls.TextRegionSelector>("textSourcePicker")!;
			var displayPicker=window.FindControl<Mesen.Debugger.Controls.TextRegionSelector>("textDisplayPicker")!;
			Dispatcher.UIThread.RunJobs();
			if(sourcePicker.CanvasWidth!=512 || sourcePicker.Region!=editor.SourceRegion) { throw new Exception("Source picker snapshot/region binding failed."); }
			editor.SelectedProfile.X=40; Dispatcher.UIThread.RunJobs();
			if(sourcePicker.Region.X!=40) { throw new Exception("Numeric source coordinates did not update the visual rectangle."); }
			displayPicker.SetCurrentValue(Mesen.Debugger.Controls.TextRegionSelector.RegionProperty,new Rect(17,29,140,46)); Dispatcher.UIThread.RunJobs();
			if(editor.SelectedProfile.DisplayX!=17 || editor.SelectedProfile.DisplayWidth!=140 || !editor.SelectedProfile.UseDisplayRegion) { throw new Exception("Visual display rectangle did not update profile coordinates."); }
			editor.SelectedProfile.DisplayY=33; Dispatcher.UIThread.RunJobs(); if(displayPicker.Region.Y!=33) { throw new Exception("Numeric display coordinates did not update the visual rectangle."); }
			editor.MatchSourceRegion(); Dispatcher.UIThread.RunJobs(); if(displayPicker.Region!=sourcePicker.Region) { throw new Exception("Match source region did not copy position and dimensions."); }
			// Pointer coordinates include zoom and letterboxing; test both drag directions.
			var picker=new Mesen.Debugger.Controls.TextRegionSelector {CanvasWidth=256,CanvasHeight=240,Snap=8};
			var host=new Window {Content=picker}; host.Measure(new Size(600,480)); host.Arrange(new Rect(0,0,600,480));
			var pointer=new Avalonia.Input.Pointer(81,Avalonia.Input.PointerType.Mouse,true);
			var pressed=new Avalonia.Input.PointerPointProperties(Avalonia.Input.RawInputModifiers.LeftMouseButton,Avalonia.Input.PointerUpdateKind.LeftButtonPressed);
			var moved=new Avalonia.Input.PointerPointProperties(Avalonia.Input.RawInputModifiers.LeftMouseButton,Avalonia.Input.PointerUpdateKind.Other);
			var released=new Avalonia.Input.PointerPointProperties(Avalonia.Input.RawInputModifiers.None,Avalonia.Input.PointerUpdateKind.LeftButtonReleased);
			Point Root(Point p) { double zoom=Math.Min(picker.Bounds.Width/256,picker.Bounds.Height/240); return picker.TranslatePoint(new Point((picker.Bounds.Width-256*zoom)/2+p.X*zoom,(picker.Bounds.Height-240*zoom)/2+p.Y*zoom),host)!.Value; }
			void Drag(Point a,Point b) {
				picker.RaiseEvent(new Avalonia.Input.PointerPressedEventArgs(picker,pointer,host,Root(a),0,pressed,Avalonia.Input.KeyModifiers.None,1));
				picker.RaiseEvent(new Avalonia.Input.PointerEventArgs(Avalonia.Input.InputElement.PointerMovedEvent,picker,pointer,host,Root(b),1,moved,Avalonia.Input.KeyModifiers.None));
				picker.RaiseEvent(new Avalonia.Input.PointerReleasedEventArgs(picker,pointer,host,Root(b),2,released,Avalonia.Input.KeyModifiers.None,Avalonia.Input.MouseButton.Left));
			}
			foreach(bool reverse in new[] {false,true}) {
				picker.DrawMode=true; Drag(reverse ? new Point(31,29) : new Point(13,19),reverse ? new Point(13,19) : new Point(31,29));
				if(picker.Region!=new Rect(8,16,24,16) || picker.DrawMode) { throw new Exception("Visual source drag did not snap correctly through preview zoom."); }
			}
			picker.Snap=1; picker.DrawMode=true; Drag(new Point(20,30),new Point(100,80));
			Drag(new Point(50,50),new Point(55,53)); if(picker.Region!=new Rect(25,33,80,50)) { throw new Exception("Display rectangle interior drag did not move at pixel precision: "+picker.Region); }
			Drag(new Point(105,83),new Point(115,89)); if(picker.Region!=new Rect(25,33,90,56)) { throw new Exception("Display corner handle did not resize."); }
			picker.RaiseEvent(new Avalonia.Input.KeyEventArgs {RoutedEvent=Avalonia.Input.InputElement.KeyDownEvent,Key=Avalonia.Input.Key.Right});
			if(picker.Region.X!=26) { throw new Exception("Display arrow key did not nudge one native pixel."); }
			host.Close();
			editor.SelectedProfile.UseRegion=false; editor.SelectedProfile.UseDisplayRegion=false;
			Console.WriteLine("PASS: visual text region selectors, bidirectional numeric bindings, tile snapping, reversed zoomed drags, pixel movement, resize handles and keyboard nudge.");
		}
		var mappings=window.FindControl<TextBox>("textMappings") ?? throw new Exception("Missing text mapping editor.");
		using(var cellEditor=new TextReplacementViewModel(path,"text-fixture","Nes",new[] {"Background"},0,-1,-1,-1,() => true)) {
			cellEditor.SetSourceTiles(new System.Collections.Generic.Dictionary<(int,int),TextReplacementViewModel.SourceTile> {
				[(8,8)]=new(0x10,0x100,0),[(8,16)]=new(0x20,0x200,0),[(16,8)]=new(0x10,0x100,0),[(16,16)]=new(0x21,0x210,0)},0,0);
			cellEditor.SourceRegion=new Rect(8,8,8,16); cellEditor.UseSelectionCellSize(); cellEditor.CaptureCharacter="A"; cellEditor.AddSelectedSourceCell();
			if(cellEditor.SelectedProfile!.CellHeight!=16 || cellEditor.SelectedProfile.CellOriginY!=8 || !MetaTextMappingText.Parse(cellEditor.MappingText).Single().GetTiles().SequenceEqual(new uint[] {0x10,0x20})) { throw new Exception("8x16 snapshot capture lost tile order or grid origin."); }
			cellEditor.SourceRegion=new Rect(16,8,8,16); cellEditor.CaptureCharacter="B"; cellEditor.AddSelectedSourceCell();
			if(MetaTextMappingText.Parse(cellEditor.MappingText).Count!=2) { throw new Exception("Different letters sharing a top tile were rejected."); }
			cellEditor.SourceRegion=new Rect(24,8,8,16); string before=cellEditor.MappingText; cellEditor.AddSelectedSourceCell();
			if(cellEditor.MappingText!=before || !cellEditor.Status.Contains("unavailable")) { throw new Exception("Incomplete source-cell capture was accepted."); }
			cellEditor.AddProfile(); cellEditor.SelectedProfile!.CellHeight=16; cellEditor.SelectedProfile.MatchKind=MetaTextMatchKind.GraphicsAddress;
			cellEditor.SourceRegion=new Rect(8,8,8,16); cellEditor.AddSelectedSourceCell();
			if(!MetaTextMappingText.Parse(cellEditor.MappingText).Single().GetTiles().SequenceEqual(new uint[] {0x100,0x200})) { throw new Exception("Multi-tile graphics-address capture used indices."); }
			Console.WriteLine("PASS: complete selected source-cell capture, 8x16 sizing/origin, shared top-tile disambiguation, incomplete capture rejection and graphics-address patterns.");
		}
		mappings.Text="002A * = A"; Dispatcher.UIThread.RunJobs();
		if(editor.MappingText!="002A * = A") { throw new Exception("Text mapping control is not bound."); }
		editor.CaptureCharacter="Ω"; editor.AddCapturedTile();
		var first=editor.SelectedProfile!;
		first.Proportional=false; // This fixture also verifies the legacy fixed-cell editor preview.
		if(MetaTextMappingText.Parse(editor.MappingText).Count!=2) { throw new Exception("Captured tile was not appended with its palette."); }
		editor.ConditionsText="0652 == 08\n0760 != 06";
		editor.AddProfile(); var second=editor.SelectedProfile!; editor.MappingText="bad draft";
		editor.SelectedProfile=first;
		if(!editor.MappingText.Contains("U+03A9") || !editor.ConditionsText.Contains("0760")) { throw new Exception("Changing text profiles lost drafts."); }
		editor.SelectedProfile=second;
		if(editor.MappingText!="bad draft") { throw new Exception("Invalid mapping draft was silently discarded."); }
		editor.RemoveProfile();
		string font=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Fonts),"consola.ttf");
		if(!File.Exists(font)) { throw new Exception("Font fixture missing: "+font); }
		if(!editor.ImportFont(font)) { throw new Exception(editor.Status); }
		var repeated=new MetaTextProfile {Id=1,FontPath=first.FontPath,Mappings=Enumerable.Range(0,4096).Select(i => new MetaTextMapping {Tile=(uint)i,Character="A"}).ToList()};
		if(new MetaTextRasterizer.Budget().Rasterize(repeated,directory).Count!=1) { throw new Exception("Repeated character mappings rasterized duplicate glyphs."); }
		var oversized=new MetaTextProfile {Id=1,FontPath=first.FontPath,CellWidth=128,CellHeight=128,RasterScale=8,Mappings=Enumerable.Range(0,17).Select(i => new MetaTextMapping {Tile=(uint)i,TilePattern=Enumerable.Repeat((uint)i,256).ToArray(),Character=((char)('A'+i)).ToString()}).ToList()};
		bool budgetRejected=false;
		try { new MetaTextRasterizer.Budget().Rasterize(oversized,directory); } catch(InvalidDataException ex) { budgetRejected=ex.Message.Contains("64 MiB"); }
		if(!budgetRejected) { throw new Exception("Oversized glyph atlas bypassed the activation budget."); }
		int files=Directory.GetFiles(directory,"*.ttf").Length;
		if(!editor.ImportFont(Path.Combine(directory,first.FontPath)) || Directory.GetFiles(directory,"*.ttf").Length!=files) { throw new Exception("In-pack font was copied or renamed."); }
		first.Enabled=true; editor.PreviewText="AΩ"; first.UsePaletteColor=false; first.Color="FF0000FF";
		editor.RefreshPreview();
		var old=editor.Preview ?? throw new Exception(editor.Status);
		if(old.PixelSize.Width!=64 || old.PixelSize.Height!=32) { throw new Exception("Font preview lost native cell dimensions/raster scale."); }
		byte[] ReadPng(Bitmap bitmap) { using var stream=new MemoryStream(); bitmap.Save(stream,PngBitmapEncoderOptions.Default); return stream.ToArray(); }
		byte[] red=ReadPng(old); first.Color="00FF00FF"; editor.RefreshPreview();
		if(editor.Preview==null || red.SequenceEqual(ReadPng(editor.Preview))) { throw new Exception("Custom text color did not change the rendered glyphs."); }
		if(ReferenceEquals(old,editor.Preview)) { throw new Exception("Refreshing text retained the obsolete preview."); }
		var fillToggle=window.FindControl<CheckBox>("textFillEnabled")!; var fillColor=window.FindControl<TextBox>("textFillColor")!;
		fillToggle.IsChecked=true; fillColor.Text="11223380"; Dispatcher.UIThread.RunJobs();
		if(!first.FillEnabled || first.FillColor!="11223380" || !fillColor.IsEnabled) { throw new Exception("Font overlay fill controls did not bind or enable the color field."); }
		byte[] beforeFill=ReadPng(editor.Preview); editor.RefreshPreview();
		if(editor.Preview==null || beforeFill.SequenceEqual(ReadPng(editor.Preview))) { throw new Exception("Font preview ignored optional background fill."); }
		var latest=MetaSpritePack.Load(path,"text-fixture","Nes"); latest.NextBackgroundId=19; latest.Save(path);
		window.FindControl<Button>("textSave")!.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent)); Dispatcher.UIThread.RunJobs();
		latest=MetaSpritePack.Load(path,"text-fixture","Nes");
		if(latest.TextProfiles.Count!=1 || latest.TextProfiles[0].Find(0x2A,3)?.Character!="Ω" || latest.TextProfiles[0].MemoryConditions.Count!=2 || latest.NextBackgroundId!=19 || !latest.TextProfiles[0].FillEnabled || latest.TextProfiles[0].FillColor!="11223380") { throw new Exception("Text save lost mappings, RAM conditions, overlay fill or unrelated pack changes: "+editor.Status); }
		string saved=File.ReadAllText(path); editor.MappingText="invalid";
		if(editor.Save() || File.ReadAllText(path)!=saved) { throw new Exception("Invalid mappings overwrote the saved pack."); }
		editor.MappingText=MetaTextMappingText.Format(first.Mappings); Directory.CreateDirectory(path+".tmp");
		try { if(editor.Save() || File.ReadAllText(path)!=saved) { throw new Exception("Failed text save modified pack or reported success."); } } finally { Directory.Delete(path+".tmp"); }
		if(!editor.Save()) { throw new Exception("Text save retry failed: "+editor.Status); }
		saved=File.ReadAllText(path); sameGame=false;
		if(editor.Save() || File.ReadAllText(path)!=saved || editor.ImportFont(font)) { throw new Exception("Stale game context accepted font import/save."); }
		sameGame=true;
		using(var competing=new TextReplacementViewModel(path,"text-fixture","Nes",new[] {"Background"},0,-1,-1,-1,() => true)) {
			competing.SelectedProfile!.Name="Updated elsewhere";
			if(!competing.Save()) { throw new Exception(competing.Status); }
			if(editor.Save() || !editor.Status.Contains("another editor")) { throw new Exception("Stale editor overwrote newer text definitions."); }
		}
		using(var reopened=new TextReplacementViewModel(path,"text-fixture","Nes",new[] {"Background"},0,-1,-1,-1,() => true)) {
			reopened.SelectedProfile!.Name="Unsaved";
		}
		if(MetaSpritePack.Load(path,"text-fixture","Nes").TextProfiles[0].Name!="Updated elsewhere") { throw new Exception("Closing an unsaved editor changed the pack."); }
		window.Close();
		using(var layoutEditor=new TextReplacementViewModel(path,"text-fixture","Nes",new[] {"Background"},0,-1,-1,-1,() => true)) {
			if(!layoutEditor.ImportFont(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Fonts),"arial.ttf"))) { throw new Exception(layoutEditor.Status); }
			var p=layoutEditor.SelectedProfile!; p.Proportional=true; p.FontSize=24; p.OutlineEnabled=true; p.OutlineWidth=1; p.OutlineColor="FF0000FF"; p.ShadowEnabled=true; p.ShadowX=2; p.ShadowY=2; p.ShadowColor="0000FFFF";
			p.UseDisplayRegion=true; p.DisplayX=20; p.DisplayY=48; p.DisplayWidth=80; p.DisplayHeight=64;
			layoutEditor.MappingText="0001 = A\n0002 = V\n0003 = T"; layoutEditor.PreviewText="AVATAR"; layoutEditor.RefreshPreview();
			if(layoutEditor.Preview==null) { throw new Exception("Text-run preview failed: "+layoutEditor.Status); }
			var atlas=(MetaTextGlyphSet)new MetaTextRasterizer.Budget().Rasterize(p,directory);
			if(!atlas.Unclipped || atlas[0].Height<=8*p.RasterScale || !atlas.Kerning.TryGetValue(1,out int kern) || kern>=0) { throw new Exception("Proportional font lost full-size bounds or actual A/V font kerning."); }
			bool hasRedOutline=atlas.SelectMany(g => g.Decorations ?? Array.Empty<uint>()).Any(c => ((c>>16)&255)>100 && (c&255)<30 && (c>>24)>0);
			bool hasBlueShadow=atlas.SelectMany(g => g.Decorations ?? Array.Empty<uint>()).Any(c => (c&255)>100 && ((c>>16)&255)<30 && (c>>24)>0);
			if(!hasRedOutline || !hasBlueShadow) { throw new Exception("Cached outline/shadow lost their independent colors."); }
			if(!layoutEditor.Save()) { throw new Exception(layoutEditor.Status); }
			var savedLayout=MetaSpritePack.Load(path,"text-fixture","Nes").TextProfiles.Single();
			if(!savedLayout.Proportional || !savedLayout.OutlineEnabled || !savedLayout.ShadowEnabled || savedLayout.DisplayWidth!=80 || savedLayout.OutlineColor!="FF0000FF" || savedLayout.ShadowColor!="0000FFFF") { throw new Exception("Text layout/effects failed persistence."); }
			string previewPath=Path.Combine(directory,"proportional-effects-preview.png"); layoutEditor.Preview.Save(previewPath,PngBitmapEncoderOptions.Default); Console.WriteLine("TEXT PREVIEW: "+previewPath);
		}
		Console.WriteLine("PASS: text editor XAML/bindings, Unicode tile capture, independent invalid drafts, real font import/reuse and color rasterization, preview replacement, merged persistence, failed-save retry, stale game/editor guards and cancel isolation.");
	}

	private static void CheckPointerInteraction(CpuType cpu)
	{
		var window = new Mesen.Debugger.Windows.SpriteViewerWindow(cpu);
		using var model = (SpriteViewerViewModel)window.DataContext!;
		try {
			var picture = window.FindControl<ScrollPictureViewer>("picViewer")!.InnerViewer;
			using var bitmap = new WriteableBitmap(new PixelSize(256, 256), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);
			picture.Source = bitmap; picture.Zoom = 1;
			picture.LeftClipSize = 0; picture.RightClipSize = 0; picture.TopClipSize = 0; picture.BottomClipSize = 0;
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900));
			model.SpritePreviews = new() {
				new() { SpriteIndex = 0, PreviewX = 10, PreviewY = 10, Width = 8, Height = 8, Visibility = SpriteVisibility.Visible },
				new() { SpriteIndex = 1, PreviewX = 252, PreviewY = 252, Width = 8, Height = 8, WrapWidth = 256, WrapHeight = 256, Visibility = SpriteVisibility.Visible }
			};
			var pointer = new Avalonia.Input.Pointer(17, Avalonia.Input.PointerType.Mouse, true);
			var pressed = new Avalonia.Input.PointerPointProperties(Avalonia.Input.RawInputModifiers.LeftMouseButton, Avalonia.Input.PointerUpdateKind.LeftButtonPressed);
			var moved = new Avalonia.Input.PointerPointProperties(Avalonia.Input.RawInputModifiers.LeftMouseButton, Avalonia.Input.PointerUpdateKind.Other);
			var released = new Avalonia.Input.PointerPointProperties(Avalonia.Input.RawInputModifiers.None, Avalonia.Input.PointerUpdateKind.LeftButtonReleased);
			Point RootPoint(Point point) => picture.TranslatePoint(new Point(point.X * picture.MinWidth / 256, point.Y * picture.MinHeight / 256), window) ?? throw new Exception("PictureViewer has no window transform.");
			void Press(Point point, Avalonia.Input.KeyModifiers modifiers = Avalonia.Input.KeyModifiers.None) => picture.RaiseEvent(new Avalonia.Input.PointerPressedEventArgs(picture, pointer, window, RootPoint(point), 0, pressed, modifiers, 1));
			void Move(Point point) => picture.RaiseEvent(new Avalonia.Input.PointerEventArgs(Avalonia.Input.InputElement.PointerMovedEvent, picture, pointer, window, RootPoint(point), 1, moved, Avalonia.Input.KeyModifiers.None));
			void Release(Point point) => picture.RaiseEvent(new Avalonia.Input.PointerReleasedEventArgs(picture, pointer, window, RootPoint(point), 2, released, Avalonia.Input.KeyModifiers.None, Avalonia.Input.MouseButton.Left));
			foreach(int zoom in new[] { 1, 2, 4 }) {
				picture.Zoom = zoom;
				foreach(bool reverse in new[] { false, true }) {
					model.ClearMetaSelection();
					Point start = reverse ? new Point(19, 19) : new Point(8, 8), end = reverse ? new Point(8, 8) : new Point(19, 19);
					Press(start); Move(end); Release(end);
					if(model.MetaSelectionDragging || pointer.Captured != null || model.SelectionRect != new Rect(10, 10, 8, 8)) { throw new Exception($"Actual routed rectangle drag did not select the sprite and release capture: dragging={model.MetaSelectionDragging}, captured={pointer.Captured}, rect={model.SelectionRect}, zoom={zoom}, reverse={reverse}."); }
				}
			}
			Press(new Point(11, 11), Avalonia.Input.KeyModifiers.Control); Release(new Point(11, 11));
			if(model.SelectionRect != default) { throw new Exception("Actual routed Ctrl-click did not toggle the selected sprite off."); }
			Press(new Point(1, 1), Avalonia.Input.KeyModifiers.Control); Release(new Point(1, 1));
			if(model.SelectionRect != new Rect(-4, -4, 8, 8)) { throw new Exception("Actual routed Ctrl-click lost wrapped sprite coordinates."); }
			Rect previous = model.SelectionRect;
			Press(new Point(8, 8)); Move(new Point(19, 19)); pointer.Capture(null);
			if(model.MetaSelectionDragging || model.SelectionRect != previous) { throw new Exception("Capture loss failed to cancel rectangle selection."); }
			Console.WriteLine($"PASS: actual {cpu} SpriteViewerWindow/PictureViewer routed pointer drag, Ctrl toggling, wrapped selection and capture-loss cancellation; no visible window.");
			model.Config.ShowSettingsPanel = true;
			model.RefreshData(); model.LoadMetaGroups(); Dispatcher.UIThread.RunJobs();
			model.SelectMetaSprites(new Rect(-4096, -4096, 8192, 8192));
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900)); Dispatcher.UIThread.RunJobs();
			var createButton = Avalonia.VisualTree.VisualExtensions.GetVisualDescendants(window).OfType<Button>().Single(button => button.Content as string == "Create meta group");
			createButton.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
			var group = model.SelectedMetaGroup ?? throw new Exception("Actual window could not create a group: " + model.MetaStatus);
			if(!group.Enabled || !model.PreviewUsesReplacement) { throw new Exception("Actual window creation did not assign/enable replacement PNG."); }
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900)); Dispatcher.UIThread.RunJobs();
			var canvas = window.FindControl<Canvas>("metaCanvas")!;
			void ClickOrigin(Point point) => canvas.RaiseEvent(new Avalonia.Input.PointerPressedEventArgs(canvas, pointer, window,
				canvas.TranslatePoint(point, window) ?? throw new Exception("Origin canvas has no window transform."), 3, pressed, Avalonia.Input.KeyModifiers.None, 1));
			ClickOrigin(new Point(2.2, 3.2));
			if(group.ReplacementOriginX != 2 || group.ReplacementOriginY != 3 || model.MetaPreviewOriginX != 2 || model.MetaPreviewOriginY != 3
				|| Canvas.GetLeft(canvas.Children[1]) != 2 || Canvas.GetTop(canvas.Children[2]) != 3) { throw new Exception("Actual origin canvas click did not update the artwork crosshair."); }
			model.LoadMetaGroups(); model.SelectedMetaGroup = model.MetaGroups[0];
			if(model.SelectedMetaGroup.ReplacementOriginX != 2 || model.SelectedMetaGroup.ReplacementOriginY != 3) { throw new Exception("Canvas-origin coordinates did not persist through reload."); }
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900)); Dispatcher.UIThread.RunJobs();
			window.FindControl<NumericUpDown>("metaReplacementOriginX")!.Value = 4;
			window.FindControl<NumericUpDown>("metaReplacementOriginY")!.Value = 5;
			if(model.SelectedMetaGroup.ReplacementOriginX != 4 || model.SelectedMetaGroup.ReplacementOriginY != 5
				|| model.MetaPreviewOriginX != 4 || model.MetaPreviewOriginY != 5) { throw new Exception("Numeric artwork controls did not update group/crosshair coordinates."); }
			var saveButton = Avalonia.VisualTree.VisualExtensions.GetVisualDescendants(window).OfType<Button>().Single(button => button.Content as string == "Save group edits");
			saveButton.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
			model.LoadMetaGroups(); model.SelectedMetaGroup = model.MetaGroups[0];
			if(model.SelectedMetaGroup.ReplacementOriginX != 4 || model.SelectedMetaGroup.ReplacementOriginY != 5) { throw new Exception("Numeric artwork controls did not persist through Save/reload."); }
			string packPath = Path.Combine(ConfigManager.HdPackFolder, "MetaSprites", cpu.ToString(), EmuApi.GetRomHash(HashType.Sha1), "groups.json");
			var scaleControl = window.FindControl<NumericUpDown>("metaReplacementScale")!;
			scaleControl.Value = 3; ClickScaleSave();
			void ClickScaleSave() => saveButton.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
			if(!model.MetaStatus.StartsWith("Replacements applied.") || MetaSpritePack.Load(packPath, EmuApi.GetRomHash(HashType.Sha1), cpu.ToString()).Groups[0].ReplacementScale != 3) { throw new Exception("Partial native canvas scale did not save/apply for the default PNG."); }
			scaleControl.Value = 1; ClickScaleSave();
			if(!model.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Correcting the artwork scale did not restore saving."); }
			string sourcePng = Path.Combine(Path.GetDirectoryName(packPath)!, model.SelectedMetaGroup.ReplacementPath);
			model.AddMetaAlternatePng(sourcePng, false);
			var firstAlternate = model.SelectedMetaAlternate ?? throw new Exception("Adding a draft alternate did not select its preview.");
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900)); Dispatcher.UIThread.RunJobs();
			var alternateOriginCanvas = window.FindControl<Canvas>("metaAlternateCanvas")!;
			alternateOriginCanvas.RaiseEvent(new Avalonia.Input.PointerPressedEventArgs(alternateOriginCanvas, pointer, window,
				alternateOriginCanvas.TranslatePoint(new Point(1.2, 2.2), window) ?? throw new Exception("Alternate origin canvas has no transform."), 3, pressed, Avalonia.Input.KeyModifiers.None, 1));
			if(firstAlternate.OriginX != 1 || firstAlternate.OriginY != 2) { throw new Exception("Always-enabled alternate origin click failed."); }
			string retainedPng = Path.Combine(Path.GetDirectoryName(packPath)!, firstAlternate.Path);
			string baseArtworkPath = model.SelectedMetaGroup.ReplacementPath;
			model.ImportMetaReplacementPng(sourcePng);
			if(firstAlternate.Path == Path.GetFileName(retainedPng) || model.SelectedMetaGroup.ReplacementPath != baseArtworkPath
				|| model.SelectedMetaAlternate != firstAlternate || firstAlternate.MemoryConditions.Count != 0 || !File.Exists(retainedPng)) {
				throw new Exception("Loading artwork while viewing an alternate changed the default or lost alternate settings.");
			}
			string replacementPath = firstAlternate.Path;
			string beforeFailedArtworkImport = File.ReadAllText(packPath);
			int artworkFileCount = Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length;
			Directory.CreateDirectory(packPath + ".tmp");
			try { model.ImportMetaReplacementPng(sourcePng); }
			finally { Directory.Delete(packPath + ".tmp"); }
			if(firstAlternate.Path != replacementPath || File.ReadAllText(packPath) != beforeFailedArtworkImport
				|| Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length != artworkFileCount) {
				throw new Exception("Failed alternate artwork import changed its saved path or left an unsaved PNG.");
			}
			if(firstAlternate.MemoryConditions.Count != 0 || !model.ViewingMetaAlternate || !model.HasMetaAlternates) { throw new Exception("New alternate draft was not saved inactive in the shared preview."); }
			var previousButton = window.FindControl<Button>("metaArtworkPrevious")!;
			var nextButton = window.FindControl<Button>("metaArtworkNext")!;
			var removeButton = window.FindControl<Button>("metaArtworkRemove")!;
			var defaultPanel = window.FindControl<StackPanel>("metaDefaultArtworkPanel")!;
			var alternatePanel = window.FindControl<StackPanel>("metaAlternateArtworkPanel")!;
			void Click(Button button) => button.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
			Click(previousButton);
			if(model.ViewingMetaAlternate || removeButton.IsEnabled || !model.MetaArtworkLabel.StartsWith("Default PNG") || !defaultPanel.IsVisible || alternatePanel.IsVisible) { throw new Exception("Previous did not show the default PNG with remove disabled."); }
			Click(nextButton);
			if(model.SelectedMetaAlternate != firstAlternate || !removeButton.IsEnabled || defaultPanel.IsVisible || !alternatePanel.IsVisible) { throw new Exception("Next did not show the alternate with remove enabled."); }
			model.AddMetaAlternatePng(sourcePng, false);
			var secondAlternate = model.SelectedMetaAlternate!;
			Click(nextButton);
			if(model.ViewingMetaAlternate) { throw new Exception("Next did not wrap from the last alternate to the default PNG."); }
			Click(previousButton);
			if(model.SelectedMetaAlternate != secondAlternate) { throw new Exception("Previous did not wrap from default to the last alternate."); }
			window.FindControl<NumericUpDown>("metaAlternateScale")!.Value = 3;
			Click(nextButton);
			if(model.ViewingMetaAlternate || secondAlternate.Scale != 3) { throw new Exception("Browsing failed to preserve partial native canvas scale edits."); }
			model.SelectedMetaAlternate = null; Click(saveButton);
			if(!model.MetaStatus.StartsWith("Replacements applied.") || MetaSpritePack.Load(packPath, EmuApi.GetRomHash(HashType.Sha1), cpu.ToString()).Groups[0].AlternateArtwork[1].Scale != 3) { throw new Exception("Saving another PNG failed to preserve an unselected alternate's partial canvas scale."); }
			secondAlternate.Scale = 1; model.SelectedMetaAlternate = secondAlternate;
			model.MetaAlternateConditionsText = "invalid condition";
			var conditionsGroup = model.SelectedMetaGroup;
			model.MetaMemoryConditionsText = "invalid group condition";
			model.SelectedMetaGroup = null; model.SelectedMetaGroup = conditionsGroup;
			model.SelectedMetaAlternate = secondAlternate;
			if(model.MetaMemoryConditionsText != "invalid group condition" || model.MetaAlternateConditionsText != "invalid condition") {
				throw new Exception("Switching groups discarded invalid CPU condition drafts.");
			}
			model.MetaMemoryConditionsText = "";
			Click(nextButton);
			if(model.SelectedMetaAlternate != secondAlternate || model.MetaAlternateConditionsText != "invalid condition") { throw new Exception("Browsing discarded invalid alternate condition edits."); }
			model.MetaAlternateConditionsText = "";
			string beforeFailedRemoval = File.ReadAllText(packPath);
			Directory.CreateDirectory(packPath + ".tmp");
			try { Click(removeButton); }
			finally { Directory.Delete(packPath + ".tmp"); }
			if(model.SelectedMetaAlternate != secondAlternate || model.MetaAlternates.Count != 2 || !removeButton.IsEnabled
				|| File.ReadAllText(packPath) != beforeFailedRemoval) { throw new Exception("Failed alternate removal lost the selected PNG or changed saved definitions."); }
			Click(removeButton);
			if(model.SelectedMetaAlternate != firstAlternate || model.MetaAlternates.Count != 1) { throw new Exception("Remove did not select the remaining alternate."); }
			model.MetaAlternateConditionsText = "$0 = $00"; Click(saveButton);
			if(MetaSpritePack.Load(packPath, EmuApi.GetRomHash(HashType.Sha1), cpu.ToString()).Groups[0].AlternateArtwork[0].MemoryConditions.Count != 1) { throw new Exception("Draft alternate activation conditions did not persist."); }
			Click(removeButton);
			if(model.ViewingMetaAlternate || removeButton.IsEnabled || model.HasMetaAlternates || !File.Exists(retainedPng)
				|| MetaSpritePack.Load(packPath, EmuApi.GetRomHash(HashType.Sha1), cpu.ToString()).Groups[0].AlternateArtwork.Count != 0) {
				throw new Exception("Removing the last alternate failed to return to default, persist removal or retain source artwork.");
			}
			Console.WriteLine($"PASS: actual {cpu} previous/next/remove buttons browse and wrap default/alternates, save draft conditions and retain removed PNG files.");
			string oversizedInput = Path.Combine(ConfigManager.HomeFolder, "oversized-partial-input.png");
			using(var oversizedBitmap = new WriteableBitmap(new PixelSize(17, 19), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul)) {
				using(var pixels = oversizedBitmap.Lock()) {
					var pngRow = Enumerable.Repeat(unchecked((int)0xFF00FF00), 17).ToArray();
					for(int y = 0; y < 19; y++) { if(y == 18) { pngRow[16] = unchecked((int)0xFFFF00FF); } Marshal.Copy(pngRow, 0, pixels.Address + y * pixels.RowBytes, 17); }
				}
				oversizedBitmap.Save(oversizedInput, PngBitmapEncoderOptions.Default);
			}
			model.SelectedMetaGroup!.ReplacementScale = 2;
			model.ImportMetaReplacementPng(oversizedInput);
			if(!model.MetaStatus.StartsWith("Replacements applied.") || model.SelectedMetaGroup.ReplacementScale != 2
				|| model.MetaPreview?.PixelSize != new PixelSize(17, 19) || model.MetaPreviewWidth != 8.5 || model.MetaPreviewHeight != 9.5) {
				throw new Exception("Oversized default PNG lost its full canvas or selected scale during import.");
			}
			model.AddMetaAlternatePng(oversizedInput, false);
			if(model.SelectedMetaAlternate?.Scale != 2 || model.MetaAlternatePreview?.PixelSize != new PixelSize(17, 19)) {
				throw new Exception("Oversized alternate import reset the selected scale or changed its PNG canvas.");
			}
			model.LoadMetaGroups(); model.SelectedMetaGroup = model.MetaGroups[0];
			if(model.SelectedMetaGroup.ReplacementWidth != 17 || model.SelectedMetaGroup.ReplacementHeight != 19
				|| model.SelectedMetaGroup.ReplacementScale != 2 || model.SelectedMetaGroup.AlternateArtwork[0].Scale != 2) {
				throw new Exception("Oversized PNG canvas/scale did not persist across pack reload.");
			}
			Console.WriteLine($"PASS: actual {cpu} imports/persists a 17x19 default and alternate PNG at scale 2, with full fractional preview bounds.");
			string screenshotPath = Path.Combine(ConfigManager.HomeFolder, "sprite-viewer-layout.png");
			var content = (DockPanel)window.Content!;
			// The offscreen content render does not include the Window background.
			content.Background = window.Background;
			content.Measure(new Size(1000, 900)); content.Arrange(new Rect(0, 0, 1000, 900));
			Dispatcher.UIThread.RunJobs();
			using(var rendered = new RenderTargetBitmap(new PixelSize(1000, 900), new Vector(96, 96))) {
				rendered.Render(content); rendered.Save(screenshotPath, PngBitmapEncoderOptions.Default);
			}
			Console.WriteLine("LAYOUT: " + screenshotPath);
			Console.WriteLine($"PASS: actual {cpu} window creates enabled PNG artwork; routed preview-origin click updates crosshair bindings and persists through reload.");
		} finally { window.Close(); }
	}

	private static void CheckMusicReplacement()
	{
		void Check(bool condition, string message) { if(!condition) { throw new Exception(message); } }
		string home = ConfigManager.HomeFolder;
		string MakeWave(string name, int direction) {
			string path = Path.Combine(home, name); using var data = new BinaryWriter(File.Create(path));
			data.Write(System.Text.Encoding.ASCII.GetBytes("RIFF")); data.Write(36 + 128); data.Write(System.Text.Encoding.ASCII.GetBytes("WAVEfmt ")); data.Write(16); data.Write((ushort)1); data.Write((ushort)1); data.Write(8000); data.Write(16000); data.Write((ushort)2); data.Write((ushort)16); data.Write(System.Text.Encoding.ASCII.GetBytes("data")); data.Write(128);
			for(int i = 0; i < 64; i++) { data.Write((short)(direction * (4000 + i * 10))); } return path;
		}
		string first = MakeWave("music-positive.wav", 1), second = MakeWave("music-negative.wav", -1);
		var pack = new MusicReplacementPack { RomHash = new string('A', 40), Enabled = true, MuteNative = true };
		pack.Tracks.Add(new() { Id = 1, Name = "Track 1", Path = Path.GetFileName(first), LoopStart = 16, LoopEnd = 32 });
		pack.Tracks.Add(new() { Id = 2, Name = "Track 2", Path = Path.GetFileName(second), LoopStart = 16, LoopEnd = 32 });
		pack.Rules.Add(new() { Id = 1, Name = "Context six", TrackId = 1, Trigger = "0652 == 08", Conditions = "0760 == 06\n0652 == 08" });
		pack.Rules.Add(new() { Id = 2, Name = "Context four", TrackId = 2, Trigger = "0652 == 08", Conditions = "0760 == 04\n0652 == 08" });
		pack.Rules.Add(new() { Id = 3, Name = "Pause", Action = 2, TrackId = 0, Trigger = "0653 == 01" });
		pack.Rules.Add(new() { Id = 4, Name = "Resume", Action = 3, TrackId = 0, Trigger = "0654 == 01" });
		pack.Rules.Add(new() { Id = 5, Name = "Stop", Action = 1, TrackId = 0, Trigger = "0655 == 01" });
		byte[] packet = pack.CreatePacket(home);
		string saved = Path.Combine(home, "music.json"); File.WriteAllText(saved, pack.ToJson());
		var loaded = MusicReplacementPack.Load(saved, pack.RomHash); Check(loaded.Rules.Count == 5 && loaded.Tracks[0].LoopStart == 16, "Music JSON round trip failed.");
		bool rejected = false; try { MusicReplacementPack.Load(saved, new string('B', 40)); } catch(InvalidDataException) { rejected = true; } Check(rejected, "ROM isolation failed.");
		rejected = false; try { MusicReplacementPack.AssetPath(home, "../outside.wav"); } catch(InvalidDataException) { rejected = true; } Check(rejected, "Escaping music path was accepted.");
		byte[] rom = new byte[16 + 16384 + 8192]; new byte[] {0x4E, 0x45, 0x53, 0x1A, 1, 1}.CopyTo(rom, 0);
		new byte[] {0x78, 0xD8, 0x4C, 0x02, 0x80}.CopyTo(rom, 16); for(int vector = 0x3FFA; vector < 0x4000; vector += 2) { rom[16 + vector + 1] = 0x80; }
		string romPath = Path.Combine(home, "music-write-nrom-mapper0.nes"); File.WriteAllBytes(romPath, rom);
		uint native = RunMusicReplacementChecks(packet, (uint)packet.Length, romPath); Check(native == 0, $"Native music checks failed at stage {native}.");
		// Original generated ROM-only fixture, with a simple idle loop and no copyrighted assets.
		byte[] gbRom = new byte[32768]; gbRom[0x100] = 0x18; gbRom[0x101] = 0xFE;
		System.Text.Encoding.ASCII.GetBytes("AUDIO-ROMONLY").CopyTo(gbRom, 0x134);
		gbRom[0x143] = 0x80; gbRom[0x147] = 0; gbRom[0x148] = 0; gbRom[0x149] = 0;
		byte checksum = 0; for(int i = 0x134; i <= 0x14C; i++) { checksum = unchecked((byte)(checksum - gbRom[i] - 1)); } gbRom[0x14D] = checksum;
		string gbPath = Path.Combine(home, "audio-rom-only.gb"), gbcPath = Path.Combine(home, "audio-rom-only.gbc"); File.WriteAllBytes(gbPath, gbRom); File.WriteAllBytes(gbcPath, gbRom);
		var gbPack = MusicReplacementPack.Load(saved, pack.RomHash); gbPack.System = "Gameboy";
		gbPack.Rules.Clear();
		gbPack.Rules.Add(new() { Id = 1, TrackId = 1, Trigger = "C652 == 08", Conditions = "C760 == 06\nC652 == 08" });
		gbPack.Rules.Add(new() { Id = 2, TrackId = 2, Trigger = "C652 == 08", Conditions = "C760 == 04\nC652 == 08" });
		gbPack.Rules.Add(new() { Id = 3, Action = 4, TrackId = 1, Restart = true, Trigger = "C653 == 01" });
		byte[] gbPacket = gbPack.CreatePacket(home);
		foreach(string fixture in new[] { gbPath, gbcPath }) { uint result = RunGbMusicReplacementChecks(gbPacket, (uint)gbPacket.Length, fixture); Check(result == 0, $"GB/GBC audio write checks failed at stage {result}."); }
		Console.WriteLine("PASS: GB/GBC ROM-only post-commit write triggers, RAM context, effects and debugger/DMA exclusion.");
		byte[] gbaRom = new byte[32768]; BitConverter.GetBytes(0xEA00002Eu).CopyTo(gbaRom, 0); BitConverter.GetBytes(0xEAFFFFFEu).CopyTo(gbaRom, 0xC0);
		System.Text.Encoding.ASCII.GetBytes("AUDIOFIXTURE").CopyTo(gbaRom, 0xA0); gbaRom[0xB2] = 0x96;
		string gbaPath = Path.Combine(home, "audio-arm-rom-only.gba"); File.WriteAllBytes(gbaPath, gbaRom);
		var gbaPack = MusicReplacementPack.Load(saved, pack.RomHash); gbaPack.System = "Gba"; gbaPack.Rules.Clear();
		gbaPack.Rules.Add(new() { Id = 1, TrackId = 1, Trigger = "02000652 == 08", Conditions = "02000760 == 06\n02000652 == 08\n02000653 == 55" });
		gbaPack.Rules.Add(new() { Id = 2, TrackId = 2, Trigger = "03000652 == 08", Conditions = "03000760 == 04" });
		gbaPack.Rules.Add(new() { Id = 3, Action = 4, TrackId = 1, Restart = true, Trigger = "02000655 == AB", Conditions = "02000654 == CD\n02000656 == 34\n02000657 == 12" });
		byte[] gbaPacket = gbaPack.CreatePacket(home); Check(BitConverter.ToUInt32(gbaPacket) == 0x3252554D, "GBA packet version incorrect.");
		uint gbaResult = RunGbaMusicReplacementChecks(gbaPacket, (uint)gbaPacket.Length, gbaPath); Check(gbaResult == 0, $"GBA native audio checks failed at stage {gbaResult}.");
		Console.WriteLine("PASS: GBA full addresses, byte/halfword/word triggers, committed sibling conditions, effects, DMA/debug exclusion and packet rejection.");
		byte[] pceRom = new byte[8192]; new byte[] { 0x78, 0xD8, 0xA9, 0xFF, 0x53, 0x01, 0xA9, 0xF8, 0x53, 0x02, 0x80, 0xFE }.CopyTo(pceRom, 0);
		pceRom[0x1FFE] = 0; pceRom[0x1FFF] = 0xE0;
		string pcePath = Path.Combine(home, "audio-hucard-8k.pce"); File.WriteAllBytes(pcePath, pceRom);
		var pcePack = MusicReplacementPack.Load(saved, pack.RomHash); pcePack.System = "PcEngine"; pcePack.Rules.Clear();
		pcePack.Rules.Add(new() { Id = 1, TrackId = 1, Trigger = "2652 == 08", Conditions = "2760 == 06\n2652 == 08" });
		pcePack.Rules.Add(new() { Id = 2, TrackId = 2, Trigger = "4652 == 08", Conditions = "4760 == 04\n4652 == 08" });
		pcePack.Rules.Add(new() { Id = 3, Action = 4, TrackId = 2, Trigger = "2653 == 01", Conditions = "0000 != 00" });
		pcePack.Rules.Add(new() { Id = 4, Action = 4, TrackId = 1, Trigger = "2653 == 01", Conditions = "2760 == 04" });
		byte[] pcePacket = pcePack.CreatePacket(home); uint pceResult = RunPceMusicReplacementChecks(pcePacket, (uint)pcePacket.Length, pcePath); Check(pceResult == 0, $"PCE native audio checks failed at stage {pceResult}.");
		Console.WriteLine("PASS: PCE HuCard logical write triggers, post-write context, MPR remapping, effects, debugger/non-CPU exclusion and unavailable I/O checks.");
		byte[] smsRom = new byte[32768]; new byte[] { 0xF3, 0x31, 0xF0, 0xDF, 0x18, 0xFE }.CopyTo(smsRom, 0);
		System.Text.Encoding.ASCII.GetBytes("TMR SEGA").CopyTo(smsRom, 0x7FF0); smsRom[0x7FFF] = 0x4C;
		string smsPath = Path.Combine(home, "audio-z80-sega-mapper.sms"), ggPath = Path.Combine(home, "audio-z80-sega-mapper.gg"); File.WriteAllBytes(smsPath, smsRom); smsRom[0x7FFF] = 0x6C; File.WriteAllBytes(ggPath, smsRom);
		var smsPack = MusicReplacementPack.Load(saved, pack.RomHash); smsPack.System = "Sms"; smsPack.Rules.Clear();
		smsPack.Rules.Add(new() { Id = 1, TrackId = 1, Trigger = "C652 == 08", Conditions = "C760 == 06\nC652 == 08" });
		smsPack.Rules.Add(new() { Id = 2, TrackId = 2, Trigger = "C652 == 08", Conditions = "C760 == 04\nC652 == 08" });
		smsPack.Rules.Add(new() { Id = 3, TrackId = 1, TriggerSource = 1, Trigger = "7F == 90", Conditions = "C760 == 04" });
		smsPack.Rules.Add(new() { Id = 4, Action = 4, TrackId = 1, TriggerSource = 1, Trigger = "7F == 91", Conditions = "C760 == 04" });
		byte[] smsPacket = smsPack.CreatePacket(home);
		foreach(string fixture in new[] { smsPath, ggPath }) { uint result = RunSmsMusicReplacementChecks(smsPacket, (uint)smsPacket.Length, fixture); Check(result == 0, $"SMS/GG native audio checks failed at stage {result}."); }
		string smsJson = Path.Combine(home, "sms-music.json"); File.WriteAllText(smsJson, smsPack.ToJson()); Check(MusicReplacementPack.Load(smsJson, pack.RomHash, "Sms").Rules[2].TriggerSource == 1, "SMS source lost on JSON reload.");
		smsPack.Rules[2].Trigger = "100 == 90"; bool portRejected = false; try { smsPack.Validate(); } catch(InvalidDataException) { portRejected = true; } Check(portRejected, "Out-of-range SMS port accepted.");
		Console.WriteLine("PASS: SMS/GG Sega mapper memory/port triggers, source isolation, RAM mirrors, effects, debugger exclusion and source persistence/validation.");
		byte[] snesRom = new byte[32768]; new byte[] { 0x78, 0xD8, 0x80, 0xFE }.CopyTo(snesRom, 0); System.Text.Encoding.ASCII.GetBytes("AUDIO LOROM TEST     ").CopyTo(snesRom, 0x7FC0);
		snesRom[0x7FD5] = 0x20; snesRom[0x7FD7] = 5; snesRom[0x7FD9] = 1; snesRom[0x7FDC] = 255; snesRom[0x7FDD] = 255; snesRom[0x7FFC] = 0; snesRom[0x7FFD] = 0x80;
		string snesPath = Path.Combine(home, "audio-65816-lorom.sfc"); File.WriteAllBytes(snesPath, snesRom);
		var snesPack = MusicReplacementPack.Load(saved, pack.RomHash); snesPack.System = "Snes"; snesPack.Rules.Clear();
		snesPack.Rules.Add(new() { Id = 1, TrackId = 1, Trigger = "7E0652 == 08", Conditions = "7E0760 == 06\n7E0652 == 08" });
		snesPack.Rules.Add(new() { Id = 2, TrackId = 1, Trigger = "002140 == 08", Conditions = "7E0760 == 04" });
		snesPack.Rules.Add(new() { Id = 3, TrackId = 2, TriggerSource = 1, ContextSource = 1, Trigger = "0652 == 08", Conditions = "0760 == 06\n0652 == 08" });
		snesPack.Rules.Add(new() { Id = 4, Action = 4, TrackId = 1, TriggerSource = 1, Trigger = "0653 == 01", Conditions = "7E0760 == 04" });
		byte[] snesPacket = snesPack.CreatePacket(home); uint snesResult = RunSnesMusicReplacementChecks(snesPacket, (uint)snesPacket.Length, snesPath); Check(snesResult == 0, $"SNES audio checks failed at stage {snesResult}.");
		Console.WriteLine("PASS: SNES LoROM main CPU/APU command writes, interpreted SPC writes, separate main/SPC context, effects and debugger/DMA/DSP exclusion.");
		byte[] wsRom = new byte[65536]; new byte[] { 0xFA, 0x31, 0xC0, 0x8E, 0xD8, 0xEB, 0xFE }.CopyTo(wsRom, 0); new byte[] { 0xEA, 0, 0, 0, 0xF0 }.CopyTo(wsRom, 0xFFF0);
		string wsPath = Path.Combine(home, "audio-v30-rom-64k.ws"), wscPath = Path.Combine(home, "audio-v30-rom-64k.wsc"); File.WriteAllBytes(wsPath, wsRom); wsRom[0xFFF7] = 1; File.WriteAllBytes(wscPath, wsRom);
		var wsPack = MusicReplacementPack.Load(saved, pack.RomHash); wsPack.System = "Ws"; wsPack.Rules.Clear();
		wsPack.Rules.Add(new() { Id = 1, TrackId = 1, Trigger = "00652 == 08", Conditions = "00760 == 06\n00652 == 08\n00653 == 55" });
		wsPack.Rules.Add(new() { Id = 2, TrackId = 2, Trigger = "10652 == 08", Conditions = "00760 == 04" });
		wsPack.Rules.Add(new() { Id = 3, TrackId = 2, TriggerSource = 1, Trigger = "0080 == 90", Conditions = "00760 == 04" });
		wsPack.Rules.Add(new() { Id = 4, Action = 4, TrackId = 1, TriggerSource = 1, Trigger = "0081 == 34", Conditions = "00760 == 04" });
		byte[] wsPacket = wsPack.CreatePacket(home);
		foreach(string fixture in new[] { wsPath, wscPath }) { uint result = RunWsMusicReplacementChecks(wsPacket, (uint)wsPacket.Length, fixture); Check(result == 0, $"WS/WSC audio checks failed at stage {result}."); }
		wsPack.Rules[0].Trigger = "100000 == 08"; bool wsRangeRejected = false; try { wsPack.Validate(); } catch(InvalidDataException) { wsRangeRejected = true; } Check(wsRangeRejected, "Out-of-range WS memory accepted."); wsPack.Rules[0].Trigger = "00652 == 08";
		wsPack.Rules[2].Trigger = "10000 == 90"; wsRangeRejected = false; try { wsPack.Validate(); } catch(InvalidDataException) { wsRangeRejected = true; } Check(wsRangeRejected, "Out-of-range WS port accepted.");
		Console.WriteLine("PASS: WS/WSC 20-bit memory, segment/wrap addressing, byte/word/split writes, operation-type preservation, I/O sources and effect playback.");





		ConfigManager.Config.Preferences.GameSelectionScreenMode = GameSelectionMode.Disabled;
		ConfigManager.Config.Gba.SkipBootScreen = true; // Original fixture needs no proprietary BIOS.
		EmuApi.InitDll(); EmuApi.InitializeEmu(home, IntPtr.Zero, IntPtr.Zero, true, true, true, true); ConfigManager.Config.ApplyConfig();
		try {
			Check(EmuApi.LoadRom(romPath), "Music editor ROM failed to load."); EmuApi.Pause();
			var model = new MusicReplacementViewModel(); model.Import(first); model.Import(second);
			Check(model.Pack.Tracks.Count == 2, model.Status); model.Pack.Enabled = model.Pack.MuteNative = true;
			foreach(var track in model.Pack.Tracks) { track.LoopStart = 16; track.LoopEnd = 32; }
			foreach(var rule in pack.Rules) { model.Pack.Rules.Add(rule); }
			model.AddEffectRule(); Check(model.SelectedRule?.Action == 4 && model.SelectedRule.Restart && model.SelectedRule.TrackId == 2, "Effect rule defaults failed.");
			model.SaveApply(); Check(model.Status.Contains("saved and applied"), model.Status);
			model.Playback(0); Check(DebugApi.GetReplacementMusicTrack() == 2, model.Status);
			model.Playback(4); Check(model.Status.StartsWith("Playback command sent"), model.Status);
			model.Playback(5); Check(DebugApi.GetReplacementMusicTrack() == 2, "Effect stop changed music.");
			model.RemoveTrack(); Check(model.Status.Contains("Remove rules"), "Referenced effect track was removed.");
			string before = File.ReadAllText(model.PackPath);
			Check(!DebugApi.SetMusicReplacements(new string('B', 40), packet, (uint)packet.Length) && DebugApi.GetReplacementMusicTrack() == 2, "Stale ROM modified playback.");
			string badFile = Path.Combine(home, "broken.ogg"); File.WriteAllText(badFile, "invalid audio"); model.Import(badFile); model.SaveApply();
			Check(model.Status.Contains("Apply failed") && File.ReadAllText(model.PackPath) == before && DebugApi.GetReplacementMusicTrack() == 2, "Failed decode modified saved pack or live music.");
			model.RemoveTrack(); Check(model.Pack.Tracks.Count == 2, model.Status);
			File.WriteAllText(model.PackPath, before + " "); model.SaveApply(); Check(model.Status.Contains("changed on disk") && File.ReadAllText(model.PackPath) == before + " ", "Stale editor overwrote saved rules."); model.Reload();
			var window = new Mesen.Debugger.Windows.MusicReplacementWindow { Width = 860, Height = 820, ShowInTaskbar = false, Opacity = 0 }; window.Show(); Dispatcher.UIThread.RunJobs();
			window.Measure(new Size(860, 820)); window.Arrange(new Rect(0, 0, 860, 820)); Dispatcher.UIThread.RunJobs();
			((Panel)window.Content!).Background = Avalonia.Media.Brushes.White;
			var scroll = window.FindControl<ScrollViewer>("musicScroll") ?? throw new Exception("Music editor did not construct.");
			Check(scroll.Extent.Width <= scroll.Viewport.Width + 1, "Music editor overflows horizontally.");
			using(var image = new RenderTargetBitmap(new PixelSize(860, 820), new Vector(96, 96))) { image.Render((Control)window.Content!); image.Save(Path.Combine(home, "music-editor.png"), PngBitmapEncoderOptions.Default); }
			scroll.Offset = new Vector(0, 500); Dispatcher.UIThread.RunJobs();
			using(var image = new RenderTargetBitmap(new PixelSize(860, 820), new Vector(96, 96))) { image.Render((Control)window.Content!); image.Save(Path.Combine(home, "music-rules.png"), PngBitmapEncoderOptions.Default); }
			window.Width = 650; window.Measure(new Size(650, 820)); window.Arrange(new Rect(0, 0, 650, 820)); Dispatcher.UIThread.RunJobs();
			Check(scroll.Extent.Width <= scroll.Viewport.Width + 1, "Music editor overflows at its minimum width.");
			window.FindControl<TabControl>("audioTabs")!.SelectedIndex = 1; Dispatcher.UIThread.RunJobs();
			var effectsScroll = window.FindControl<ScrollViewer>("effectsScroll")!;
			Check(effectsScroll.Extent.Width <= effectsScroll.Viewport.Width + 1, "Sound effect editor overflows at minimum width.");
			using(var image = new RenderTargetBitmap(new PixelSize(650, 820), new Vector(96, 96))) { image.Render((Control)window.Content!); image.Save(Path.Combine(home, "sound-effects-editor.png"), PngBitmapEncoderOptions.Default); }
			window.Close(); model.Playback(1);
			EmuApi.Stop(); Check(DebugApi.GetReplacementMusicTrack() == 0, "Stopping a ROM retained replacement music.");
			bool preloaded = false; uint beforeFrame = 0, afterFrame = 1; ulong beforeClock = 0, afterClock = 1;
			using var listener = new NotificationListener();
			listener.OnNotification += e => {
				if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
				var beforeTiming = EmuApi.GetTimingInfo(CpuType.Nes); beforeFrame = beforeTiming.FrameCount; beforeClock = beforeTiming.MasterClock;
				typeof(MusicReplacementPack).Assembly.GetType("Mesen.Debugger.Utilities.MusicReplacementRuntime")!.GetMethod("LoadForGame")!.Invoke(null, new object[] { model.Pack.RomHash });
				preloaded = DebugApi.ReplacementMusicCommand(model.Pack.RomHash, 0, 1);
				var afterTiming = EmuApi.GetTimingInfo(CpuType.Nes); afterFrame = afterTiming.FrameCount; afterClock = afterTiming.MasterClock;
			};
			Check(EmuApi.LoadRom(romPath) && preloaded && beforeFrame == afterFrame && beforeClock == afterClock, "Saved music failed to load during GameLoaded before CPU execution."); EmuApi.Pause();
			Check(EmuApi.LoadRom(gbPath), "GB audio editor fixture failed to load."); EmuApi.Pause();
			var gbModel = new MusicReplacementViewModel(); Check(gbModel.Pack.System == "Gameboy" && gbModel.PackPath.Contains("Gameboy"), "GB pack path/system incorrect.");
			gbModel.Import(first); gbModel.Pack.Enabled = true; gbModel.AddEffectRule(); gbModel.SaveApply();
			Check(gbModel.Status.Contains("saved and applied"), gbModel.Status); gbModel.Playback(4); Check(gbModel.Status.StartsWith("Playback command sent"), gbModel.Status);
			bool wrongSystem = false; try { MusicReplacementPack.Load(gbModel.PackPath, gbModel.Pack.RomHash, "Nes"); } catch(InvalidDataException) { wrongSystem = true; }
			Check(wrongSystem, "Cross-system audio pack accepted.");
			gbModel.Reload(); Check(gbModel.Pack.Rules.Single().Action == 4, "GB effect pack reload failed.");
			bool gbPreloaded = false;
			using(var gbListener = new NotificationListener()) {
				gbListener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
					var timing = EmuApi.GetTimingInfo(CpuType.Gameboy);
					typeof(MusicReplacementPack).Assembly.GetType("Mesen.Debugger.Utilities.MusicReplacementRuntime")!.GetMethod("LoadForGame")!.Invoke(null, new object[] { gbModel.Pack.RomHash });
					var after = EmuApi.GetTimingInfo(CpuType.Gameboy);
					gbPreloaded = DebugApi.ReplacementMusicCommand(gbModel.Pack.RomHash, 4, 1) && timing.MasterClock == after.MasterClock && timing.FrameCount == after.FrameCount;
				};
				EmuApi.Stop(); Check(EmuApi.LoadRom(gbPath) && gbPreloaded, "GB audio pack failed to load before CPU execution."); EmuApi.Pause();
			}
			Console.WriteLine("PASS: Game Boy editor import/apply/effect preview/reload and system isolation.");
			Check(EmuApi.LoadRom(gbaPath), "GBA editor fixture failed to load."); EmuApi.Pause();
			var gbaModel = new MusicReplacementViewModel(); Check(gbaModel.Pack.System == "Gba" && gbaModel.PackPath.Contains("Gba"), "GBA audio system/path incorrect.");
			gbaModel.Import(first); gbaModel.Pack.Enabled = true; gbaModel.AddEffectRule(); Check(gbaModel.SelectedRule!.Trigger == "02000652 == 08", "GBA default trigger incorrect.");
			gbaModel.SaveApply(); Check(gbaModel.Status.Contains("saved and applied"), gbaModel.Status); gbaModel.Playback(4); Check(gbaModel.Status.StartsWith("Playback command sent"), gbaModel.Status);
			gbaModel.Reload(); Check(gbaModel.Pack.Rules.Single().Trigger == "02000652 == 08", "GBA address lost on reload.");
			gbaModel.SelectedRule!.Trigger = "10000000 == 08"; bool gbaAddressRejected = false; try { gbaModel.Pack.Validate(); } catch(InvalidDataException) { gbaAddressRejected = true; } Check(gbaAddressRejected, "GBA out-of-range address accepted."); gbaModel.Reload();
			bool gbaPreloaded = false;
			using(var gbaListener = new NotificationListener()) {
				gbaListener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
					var beforeTiming = EmuApi.GetTimingInfo(CpuType.Gba);
					typeof(MusicReplacementPack).Assembly.GetType("Mesen.Debugger.Utilities.MusicReplacementRuntime")!.GetMethod("LoadForGame")!.Invoke(null, new object[] { gbaModel.Pack.RomHash });
					var afterTiming = EmuApi.GetTimingInfo(CpuType.Gba);
					gbaPreloaded = DebugApi.ReplacementMusicCommand(gbaModel.Pack.RomHash, 4, 1) && beforeTiming.MasterClock == afterTiming.MasterClock;
				};
				EmuApi.Stop(); Check(EmuApi.LoadRom(gbaPath) && gbaPreloaded, "GBA audio pack failed to preload."); EmuApi.Pause();
				// Wait for startup before stopping: Run clears its stop flag when it starts.
				var startup = Stopwatch.StartNew();
				while(EmuApi.GetTimingInfo(CpuType.Gba).FrameCount == 0 && startup.ElapsedMilliseconds < 5000) { System.Threading.Thread.Sleep(1); }
				Check(EmuApi.GetTimingInfo(CpuType.Gba).FrameCount > 0, "GBA running fixture never completed its first frame.");
			}
			Console.WriteLine("PASS: GBA editor apply/preview/reload, address validation and pre-execution loading.");
			Check(EmuApi.LoadRom(pcePath), "PCE editor fixture failed to load."); EmuApi.Pause();
			var pceModel = new MusicReplacementViewModel(); Check(pceModel.Pack.System == "PcEngine" && pceModel.PackPath.Contains("PcEngine"), "PCE audio system/path incorrect.");
			pceModel.Import(first); pceModel.Pack.Enabled = true; pceModel.AddEffectRule(); Check(pceModel.SelectedRule!.Trigger == "2652 == 08", "PCE default trigger incorrect.");
			pceModel.SaveApply(); Check(pceModel.Status.Contains("saved and applied"), pceModel.Status); pceModel.Playback(4); Check(pceModel.Status.StartsWith("Playback command sent"), pceModel.Status);
			pceModel.Reload(); Check(pceModel.Pack.Rules.Single().Action == 4, "PCE effect pack reload failed.");
			bool pcePreloaded = false;
			using(var pceListener = new NotificationListener()) {
				pceListener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
					var beforeTiming = EmuApi.GetTimingInfo(CpuType.Pce);
					typeof(MusicReplacementPack).Assembly.GetType("Mesen.Debugger.Utilities.MusicReplacementRuntime")!.GetMethod("LoadForGame")!.Invoke(null, new object[] { pceModel.Pack.RomHash });
					var afterTiming = EmuApi.GetTimingInfo(CpuType.Pce);
					pcePreloaded = DebugApi.ReplacementMusicCommand(pceModel.Pack.RomHash, 4, 1) && beforeTiming.MasterClock == afterTiming.MasterClock;
				};
				EmuApi.Stop(); Check(EmuApi.LoadRom(pcePath) && pcePreloaded, "PCE audio pack failed to preload."); EmuApi.Pause();
				var startup = Stopwatch.StartNew(); while(EmuApi.GetTimingInfo(CpuType.Pce).FrameCount == 0 && startup.ElapsedMilliseconds < 5000) { System.Threading.Thread.Sleep(1); }
				Check(EmuApi.GetTimingInfo(CpuType.Pce).FrameCount > 0, "PCE fixture never completed its first frame.");
			}
			Console.WriteLine("PASS: PCE editor apply/preview/reload and pre-execution loading.");
			Check(EmuApi.LoadRom(smsPath), "SMS editor fixture failed to load."); EmuApi.Pause();
			var smsModel = new MusicReplacementViewModel(); Check(smsModel.Pack.System == "Sms" && smsModel.PackPath.Contains("Sms") && smsModel.SupportsPortTriggers, "SMS audio editor system/path/source incorrect.");
			smsModel.Import(first); smsModel.Pack.Enabled = true; smsModel.AddEffectRule(); smsModel.SelectedRule!.TriggerSource = 1; smsModel.SelectedRule.Trigger = "7F == 91";
			smsModel.SaveApply(); Check(smsModel.Status.Contains("saved and applied"), smsModel.Status); smsModel.Playback(4); Check(smsModel.Status.StartsWith("Playback command sent"), smsModel.Status);
			smsModel.Reload(); Check(smsModel.Pack.Rules.Single().TriggerSource == 1, "SMS source lost on editor reload.");
			var smsWindow = new Mesen.Debugger.Windows.MusicReplacementWindow { Width = 650, Height = 820, ShowInTaskbar = false, Opacity = 0 }; smsWindow.Show(); Dispatcher.UIThread.RunJobs();
			try {
				smsWindow.Measure(new Size(650, 820)); smsWindow.Arrange(new Rect(0, 0, 650, 820)); Dispatcher.UIThread.RunJobs();
				var smsWindowModel = (MusicReplacementViewModel)smsWindow.DataContext!;
				var sourceSelector = smsWindow.FindControl<ComboBox>("musicTriggerSource")!;
				Check(sourceSelector.IsVisible && sourceSelector.SelectedIndex == 1, "SMS trigger source did not load in actual UI.");
				sourceSelector.SelectedIndex = 0; Dispatcher.UIThread.RunJobs(); Check(smsWindowModel.SelectedRule!.TriggerSource == 0, "SMS source selector did not update rule.");
				sourceSelector.SelectedIndex = 1; Dispatcher.UIThread.RunJobs(); Check(smsWindowModel.SelectedRule!.TriggerSource == 1, "SMS port source selection failed.");
				var smsScroll = smsWindow.FindControl<ScrollViewer>("musicScroll")!; Check(smsScroll.Extent.Width <= smsScroll.Viewport.Width + 1, "SMS editor overflows at minimum width.");
			} finally { smsWindow.Close(); }
			bool smsPreloaded = false;
			using(var smsListener = new NotificationListener()) {
				smsListener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
					var beforeTiming = EmuApi.GetTimingInfo(CpuType.Sms);
					typeof(MusicReplacementPack).Assembly.GetType("Mesen.Debugger.Utilities.MusicReplacementRuntime")!.GetMethod("LoadForGame")!.Invoke(null, new object[] { smsModel.Pack.RomHash });
					var afterTiming = EmuApi.GetTimingInfo(CpuType.Sms);
					smsPreloaded = DebugApi.ReplacementMusicCommand(smsModel.Pack.RomHash, 4, 1) && beforeTiming.MasterClock == afterTiming.MasterClock;
				};
				EmuApi.Stop(); Check(EmuApi.LoadRom(smsPath) && smsPreloaded, "SMS audio pack failed to preload."); EmuApi.Pause();
				var startup = Stopwatch.StartNew(); while(EmuApi.GetTimingInfo(CpuType.Sms).FrameCount == 0 && startup.ElapsedMilliseconds < 5000) { System.Threading.Thread.Sleep(1); }
				Check(EmuApi.GetTimingInfo(CpuType.Sms).FrameCount > 0, "SMS fixture never completed its first frame.");
			}
			Console.WriteLine("PASS: SMS editor port rule apply/preview/reload and pre-execution loading.");
			Check(EmuApi.LoadRom(snesPath), "SNES editor fixture failed to load."); EmuApi.Pause();
			var snesModel = new MusicReplacementViewModel(); Check(snesModel.Pack.System == "Snes" && snesModel.SupportsSpcTriggers, "SNES audio editor system/source incorrect.");
			snesModel.Import(first); snesModel.Pack.Enabled = true; snesModel.AddEffectRule(); snesModel.SelectedRule!.TriggerSource = 1; snesModel.SelectedRule.ContextSource = 1; snesModel.SelectedRule.Trigger = "0652 == 08"; snesModel.SelectedRule.Conditions = "0760 == 06";
			snesModel.SaveApply(); Check(snesModel.Status.Contains("saved and applied"), snesModel.Status); snesModel.Playback(4); Check(snesModel.Status.StartsWith("Playback command sent"), snesModel.Status);
			snesModel.Reload(); Check(snesModel.Pack.Rules.Single().TriggerSource == 1 && snesModel.SelectedRule!.ContextSource == 1, "SNES CPU source settings lost.");
			snesModel.SelectedRule!.Trigger = "10000 == 08"; bool spcRangeRejected = false; try { snesModel.Pack.Validate(); } catch(InvalidDataException) { spcRangeRejected = true; } Check(spcRangeRejected, "Wide SPC address accepted."); snesModel.Reload();
			var snesWindow = new Mesen.Debugger.Windows.MusicReplacementWindow { Width = 650, Height = 820, ShowInTaskbar = false, Opacity = 0 }; snesWindow.Show(); Dispatcher.UIThread.RunJobs();
			try {
				snesWindow.Measure(new Size(650, 820)); snesWindow.Arrange(new Rect(0, 0, 650, 820)); Dispatcher.UIThread.RunJobs();
				var windowModel = (MusicReplacementViewModel)snesWindow.DataContext!;
				var triggerSelector = snesWindow.FindControl<ComboBox>("musicTriggerSource")!; var contextSelector = snesWindow.FindControl<ComboBox>("musicContextSource")!;
				Check(triggerSelector.IsVisible && contextSelector.IsVisible && triggerSelector.SelectedIndex == 1 && contextSelector.SelectedIndex == 1, "SNES source selectors did not restore.");
				contextSelector.SelectedIndex = 0; Dispatcher.UIThread.RunJobs(); Check(windowModel.SelectedRule!.ContextSource == 0 && windowModel.SelectedRule.TriggerSource == 1, "SNES RAM source changed trigger CPU.");
				triggerSelector.SelectedIndex = 0; Dispatcher.UIThread.RunJobs(); Check(windowModel.SelectedRule!.TriggerSource == 0, "SNES trigger selector failed.");
				var snesScroll = snesWindow.FindControl<ScrollViewer>("musicScroll")!; Check(snesScroll.Extent.Width <= snesScroll.Viewport.Width + 1, "SNES editor overflows at minimum width.");
			} finally { snesWindow.Close(); }
			bool snesPreloaded = false;
			using(var snesListener = new NotificationListener()) {
				snesListener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
					var beforeTiming = EmuApi.GetTimingInfo(CpuType.Snes);
					typeof(MusicReplacementPack).Assembly.GetType("Mesen.Debugger.Utilities.MusicReplacementRuntime")!.GetMethod("LoadForGame")!.Invoke(null, new object[] { snesModel.Pack.RomHash });
					var afterTiming = EmuApi.GetTimingInfo(CpuType.Snes);
					snesPreloaded = DebugApi.ReplacementMusicCommand(snesModel.Pack.RomHash, 4, 1) && beforeTiming.MasterClock == afterTiming.MasterClock;
				};
				EmuApi.Stop(); Check(EmuApi.LoadRom(snesPath) && snesPreloaded, "SNES audio pack failed to preload."); EmuApi.Pause();
				var startup = Stopwatch.StartNew(); while(EmuApi.GetTimingInfo(CpuType.Snes).FrameCount == 0 && startup.ElapsedMilliseconds < 5000) { System.Threading.Thread.Sleep(1); }
				Check(EmuApi.GetTimingInfo(CpuType.Snes).FrameCount > 0, "SNES fixture never completed its first frame.");
			}
			Console.WriteLine("PASS: SNES editor CPU/context sources, apply/preview/reload, validation and pre-execution loading.");
			Check(EmuApi.LoadRom(wsPath), "WS editor fixture failed to load."); EmuApi.Pause();
			var wsModel = new MusicReplacementViewModel(); Check(wsModel.Pack.System == "Ws" && wsModel.PackPath.Contains("Ws") && wsModel.SupportsPortTriggers, "WS audio editor system/path incorrect.");
			wsModel.Import(first); wsModel.Pack.Enabled = true; wsModel.AddEffectRule(); Check(wsModel.SelectedRule!.Trigger == "00652 == 08", "WS default address incorrect.");
			wsModel.SelectedRule.TriggerSource = 1; wsModel.SelectedRule.Trigger = "0081 == 34"; wsModel.SaveApply(); Check(wsModel.Status.Contains("saved and applied"), wsModel.Status);
			wsModel.Playback(4); Check(wsModel.Status.StartsWith("Playback command sent"), wsModel.Status); wsModel.Reload(); Check(wsModel.SelectedRule!.TriggerSource == 1, "WS port source lost on reload.");
			var wsWindow = new Mesen.Debugger.Windows.MusicReplacementWindow { Width = 650, Height = 820, ShowInTaskbar = false, Opacity = 0 }; wsWindow.Show(); Dispatcher.UIThread.RunJobs();
			try {
				wsWindow.Measure(new Size(650, 820)); wsWindow.Arrange(new Rect(0, 0, 650, 820)); Dispatcher.UIThread.RunJobs();
				var windowModel = (MusicReplacementViewModel)wsWindow.DataContext!; var selector = wsWindow.FindControl<ComboBox>("musicTriggerSource")!;
				Check(selector.IsVisible && selector.SelectedIndex == 1, "WS source selector did not restore."); selector.SelectedIndex = 0; Dispatcher.UIThread.RunJobs(); Check(windowModel.SelectedRule!.TriggerSource == 0, "WS selector failed.");
				var scrollView = wsWindow.FindControl<ScrollViewer>("musicScroll")!; Check(scrollView.Extent.Width <= scrollView.Viewport.Width + 1, "WS editor overflows at minimum width.");
			} finally { wsWindow.Close(); }
			bool wsPreloaded = false;
			using(var wsListener = new NotificationListener()) {
				wsListener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
					var beforeTiming = EmuApi.GetTimingInfo(CpuType.Ws);
					typeof(MusicReplacementPack).Assembly.GetType("Mesen.Debugger.Utilities.MusicReplacementRuntime")!.GetMethod("LoadForGame")!.Invoke(null, new object[] { wsModel.Pack.RomHash });
					var afterTiming = EmuApi.GetTimingInfo(CpuType.Ws);
					wsPreloaded = DebugApi.ReplacementMusicCommand(wsModel.Pack.RomHash, 4, 1) && beforeTiming.MasterClock == afterTiming.MasterClock;
				};
				EmuApi.Stop(); Check(EmuApi.LoadRom(wsPath) && wsPreloaded, "WS audio pack failed to preload."); EmuApi.Pause();
				var startup = Stopwatch.StartNew(); while(EmuApi.GetTimingInfo(CpuType.Ws).FrameCount == 0 && startup.ElapsedMilliseconds < 5000) { System.Threading.Thread.Sleep(1); }
				Check(EmuApi.GetTimingInfo(CpuType.Ws).FrameCount > 0, "WS fixture never completed its first frame.");
			}
			Console.WriteLine("PASS: WS editor source controls, apply/preview/reload, address validation and pre-execution loading.");





			Console.WriteLine("LAYOUT: " + Path.Combine(home, "music-editor.png"));
			Console.WriteLine("PASS: music editor import/save/apply/reload, loop metadata, previews, ROM/path isolation, failed-decode rollback, stale-save protection, UI layout and pre-emulation pack loading.");
		} finally { Console.WriteLine("AUDIO: stopping final fixture."); EmuApi.Stop(); Console.WriteLine("AUDIO: releasing emulator."); EmuApi.Release(); Console.WriteLine("AUDIO: shutdown complete."); }
	}
	private static void CheckWidescreenProfile()
	{
		string home = ConfigManager.HomeFolder;
		EmuApi.InitDll(); EmuApi.InitializeEmu(home, IntPtr.Zero, IntPtr.Zero, true, true, true, true);
		ConfigManager.Config.ApplyConfig();
		try {
			if(!EmuApi.LoadRom(Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture.gb"))) { throw new Exception("Profile fixture failed to load."); }
			EmuApi.Pause();
			string hash = EmuApi.GetRomHash(HashType.Sha1);
			WidescreenProfileStore.LoadForGame(hash, CpuType.Gameboy);
			var original = ConfigManager.Config.Video.Clone();
			using var model = new Mesen.ViewModels.VideoConfigViewModel();
			model.WidescreenSettings.DecorativeWidescreen = true;
			model.WidescreenSettings.DecorativeSideBrightness = 37;
			model.WidescreenSettings.DecorativeCustomWidths = true;
			model.WidescreenSettings.DecorativeLeftWidth = 23; model.WidescreenSettings.DecorativeRightWidth = 71;
			model.WidescreenSettings.DecorativeFadeWidth = 12; model.WidescreenSettings.DecorativeViewportPosition = 3;
			model.SaveGameWidescreenCommand.Execute(null);
			string path = WidescreenProfileStore.ActivePath!;
			var profile = WidescreenProfileStore.Read(path) ?? throw new Exception(model.WidescreenProfileStatus);
			if(profile.DecorativeSideBrightness != 37 || profile.DecorativeViewportPosition != 3 || profile.DecorativeFadeWidth != 12
				|| !original.IsIdentical(ConfigManager.Config.Video)) { throw new Exception("Profile save lost fields or modified global defaults."); }
			var interop = new InteropVideoConfig { Brightness = 0.25, Contrast = -0.5 };
			profile.ApplyTo(ref interop);
			if(!CheckWidescreenPresentationInterop()) { throw new Exception("Native video config did not receive widescreen presentation settings correctly."); }
			if(interop.DecorativeLeftWidth != 23 || interop.DecorativeRightWidth != 71 || interop.DecorativeSideBrightness != 37
				|| interop.Brightness != 0.25 || interop.Contrast != -0.5) { throw new Exception("Profile override altered unrelated video settings."); }
			string current = File.ReadAllText(path);
			File.WriteAllText(path, current[..current.LastIndexOf('}')] + ",\"DecorativeCameraMin\":-50,\"DecorativeSlideSpeed\":0.1,\"DecorativeLeftFlags\":\"invalid legacy flags\"}");
			var legacy = WidescreenProfileStore.Read(path) ?? throw new Exception("Legacy profile did not load.");
			if(legacy.DecorativeViewportPosition != 3 || legacy.DecorativeSideBrightness != 37) { throw new Exception("Discarding obsolete boundary fields lost presentation settings."); }
			WidescreenProfileStore.Write(path, legacy);
			if(File.ReadAllText(path).Contains("DecorativeCameraMin") || File.ReadAllText(path).Contains("DecorativeSlideSpeed")) { throw new Exception("Obsolete boundary fields survived profile saving."); }
			string prior = File.ReadAllText(path); profile.DecorativeSideBrightness = 101;
			bool invalid = false; try { WidescreenProfileStore.Write(path, profile); } catch(InvalidDataException) { invalid = true; }
			if(!invalid || File.ReadAllText(path) != prior) { throw new Exception("Invalid profile overwrote saved settings."); }
			var view = new Mesen.Views.VideoConfigView { DataContext = model, Background = Avalonia.Media.Brushes.White };
			var host = new Window { Content = view, Width = 500, Height = 700, ShowInTaskbar = false, Opacity = 0 };
			host.Show(); Dispatcher.UIThread.RunJobs();
			var gameplayPosition = view.FindControl<ComboBox>("wideGameplayPosition") ?? throw new Exception("Gameplay position selector was removed.");
			if(gameplayPosition.ItemCount != 4 || gameplayPosition.SelectedIndex != 3) { throw new Exception("Gameplay position choices or saved selection were lost."); }
			var brightness = view.FindControl<NumericUpDown>("wideBrightness") ?? throw new Exception("Brightness numeric input is missing.");
			if(brightness.Value != 37 || brightness.Minimum != 0 || brightness.Maximum != 100 || brightness.Increment != 1) { throw new Exception("Brightness spinbox configuration/binding failed."); }
			brightness.Value = 38;
			if(model.WidescreenSettings.DecorativeSideBrightness != 38 || WidescreenProfileStore.Read(path)!.DecorativeSideBrightness != 37) { throw new Exception("Brightness edit failed or saved without the explicit save action."); }
			brightness.Value = 37;
			view.Measure(new Size(500, 700)); view.Arrange(new Rect(0, 0, 500, 700)); Dispatcher.UIThread.RunJobs();
			string imagePath = Path.Combine(home, "widescreen-settings-layout.png");
			using(var bitmap = new RenderTargetBitmap(new PixelSize(500, 700), new Vector(96, 96))) { bitmap.Render(view); bitmap.Save(imagePath, PngBitmapEncoderOptions.Default); }
			var scroll = view.FindControl<ScrollViewer>("wideSettingsScroll") ?? throw new Exception("Settings scroll host missing.");
			if(scroll.Extent.Width > scroll.Viewport.Width + 1) { throw new Exception("Widescreen settings overflow horizontally."); }
			scroll.Offset = new Vector(0, 350); Dispatcher.UIThread.RunJobs();
			using(var bitmap = new RenderTargetBitmap(new PixelSize(500, 700), new Vector(96, 96))) { bitmap.Render(view); bitmap.Save(Path.Combine(home, "widescreen-presentation-layout.png"), PngBitmapEncoderOptions.Default); }
			host.Close();
			Console.WriteLine("LAYOUT: " + imagePath);
			WidescreenProfileStore.LoadForGame(new string('A', 40), CpuType.Gameboy);
			model.SaveGameWidescreenCommand.Execute(null);
			if(WidescreenProfileStore.Active != null || !model.WidescreenProfileStatus.Contains("game changed")) { throw new Exception("Stale profile editor saved to another game."); }
			if(WidescreenProfileStore.GetPath(home, hash, CpuType.Gba) == WidescreenProfileStore.GetPath(home, hash, CpuType.Gameboy)) { throw new Exception("Profile system isolation failed."); }
			WidescreenProfileStore.LoadForGame(hash, CpuType.Gameboy);
			using var reopened = new Mesen.ViewModels.VideoConfigViewModel();
			if(reopened.WidescreenSettings.DecorativeSideBrightness != 37 || reopened.WidescreenSettings.DecorativeViewportPosition != 3) { throw new Exception("Saved profile did not restore into controls."); }
			reopened.RemoveGameWidescreenCommand.Execute(null);
			if(File.Exists(path) || WidescreenProfileStore.Active != null || !original.IsIdentical(reopened.WidescreenSettings)) { throw new Exception("Profile removal did not restore global defaults."); }
			Console.WriteLine("PASS: actual profile editor save/load/remove, global isolation, unrelated video settings, invalid-save retention, stale-game rejection, system/hash isolation and UI construction/render.");
		} finally { WidescreenProfileStore.Clear(); EmuApi.Stop(); EmuApi.Release(); }
	}
	private static void CheckGroupLabelBinding()
	{
		var group = new MetaSpriteGroup { Id = 17 };
		var label = new TextBlock { DataContext = group };
		using var binding = label.Bind(TextBlock.TextProperty, new Avalonia.Data.Binding(nameof(MetaSpriteGroup.DisplayName)));
		if(label.Text != "Meta 017") { throw new Exception("Numbered group label binding is missing."); }
		group.Name = "Standing"; group.PoseName = "Idle";
		Dispatcher.UIThread.RunJobs();
		if(label.Text != "Meta 017 — Standing (Idle)") { throw new Exception("Group label did not update after name/pose edits."); }
		group.Name = ""; group.PoseName = "";
		Dispatcher.UIThread.RunJobs();
		if(label.Text != "Meta 017") { throw new Exception("Clearing optional group labels lost the persistent number."); }
		Console.WriteLine("PASS: actual Avalonia group label binding updates immediately for group/pose renames and clearing optional names.");
	}

	private static void CheckColdStartup(string sourceHome, string mode, string romPath, string? statePath, bool scrollTurn, string? settingsPath, bool softwareRenderer)
	{
		if(mode != "preloaded" && mode != "queued" && mode != "disabled") { throw new ArgumentException("Use preloaded, queued or disabled activation mode."); }
		if(statePath != null && !File.Exists(statePath)) { throw new FileNotFoundException("Gameplay save state missing.", statePath); }
		CpuType cpu = Path.GetExtension(romPath).Equals(".nes", StringComparison.OrdinalIgnoreCase) ? CpuType.Nes : CpuType.Snes;
		if(scrollTurn && (cpu != CpuType.Nes || statePath == null)) { throw new ArgumentException("Scroll-turn reproduction requires a NES gameplay save state."); }
		string home = Path.Combine(AppContext.BaseDirectory, "artifacts", "cold-" + mode + "-" + Guid.NewGuid().ToString("N"));
		string sourceAssets = Path.Combine(Path.GetFullPath(sourceHome), "HdPacks");
		foreach(string source in Directory.GetFiles(sourceAssets, "*", SearchOption.AllDirectories)) {
			string destination = Path.Combine(home, "HdPacks", Path.GetRelativePath(sourceAssets, source));
			Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
			File.Copy(source, destination);
		}
		typeof(ConfigManager).GetField("_homeFolder", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, home);
		typeof(ConfigManager).GetField("_config", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, settingsPath != null ? Configuration.Deserialize(Path.GetFullPath(settingsPath)) : Configuration.CreateConfig());
		// Preserve gameplay/video settings, but keep all test output in its own home.
		var preferences = ConfigManager.Config.Preferences;
		preferences.OverrideAviFolder = preferences.OverrideMovieFolder = preferences.OverrideSaveDataFolder = false;
		preferences.OverrideSaveStateFolder = preferences.OverrideScreenshotFolder = preferences.OverrideWaveFolder = false;
		Type loader = typeof(SpriteViewerViewModel).Assembly.GetType("Mesen.Debugger.Utilities.MetaSpriteRuntimeLoader")!;
		MethodInfo load = loader.GetMethod("LoadForGame", BindingFlags.Public | BindingFlags.Static)!;
		EmuApi.InitDll();
		EmuApi.InitializeEmu(home, IntPtr.Zero, IntPtr.Zero, true, true, true, true);
		ConfigManager.Config.ApplyConfig();
		SoftwareRendererView? softwareView = null;
		if(softwareRenderer) {
			ConfigureMetaSoftwareRendererTest();
			softwareView = new SoftwareRendererView { DataContext = new SoftwareRendererViewModel() };
			EmuApi.SetRendererSize(512, 480);
		}
		try {
			bool applied = false;
			using var listener = new NotificationListener();
			listener.OnNotification += e => {
				if(e.NotificationType == ConsoleNotificationType.RefreshSoftwareRenderer && softwareView != null) {
					softwareView.UpdateSoftwareRenderer(Marshal.PtrToStructure<SoftwareRendererFrame>(e.Parameter));
					return;
				}
				if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
				string hash = EmuApi.GetRomHash(HashType.Sha1);
				if(mode == "disabled") {
					// Change only the isolated copy. Keep recognition metadata for the
					// same viewer workload, but omit native replacement capture/rendering.
					string path = Path.Combine(home, "HdPacks", "MetaSprites", cpu.ToString(), hash, "groups.json");
					var disabledPack = MetaSpritePack.Load(path, hash, cpu.ToString());
					foreach(var group in disabledPack.Groups) { group.Enabled = false; }
					foreach(var layer in disabledPack.BackgroundLayers) { layer.Enabled = false; }
					disabledPack.Save(path);
				}
				void Apply() {
					var decoding = Stopwatch.StartNew();
					applied = (bool)load.Invoke(null, new object[] { hash, cpu })!;
					Console.WriteLine($"COLD ACTIVATION: {mode}, {decoding.Elapsed.TotalMilliseconds:F3} ms, applied={applied}, frame={EmuApi.GetTimingInfo(cpu).FrameCount}.");
				}
				if(mode == "queued") { Dispatcher.UIThread.Post(Apply); } else { Apply(); }
			};
			if(!EmuApi.LoadRom(Path.GetFullPath(romPath))) { throw new Exception("Cold-start ROM failed to load."); }
			DebugApi.InitializeDebugger();
			try {
				if(statePath != null) {
					EmuApi.Pause();
					int stateLoads = 0;
					listener.OnNotification += e => { if(e.NotificationType == ConsoleNotificationType.StateLoaded) { stateLoads++; } };
					EmuApi.LoadStateFile(Path.GetFullPath(statePath));
					if(stateLoads != 1) { throw new Exception("Gameplay save state was not successfully loaded."); }
					Console.WriteLine("GAMEPLAY STATE: " + Path.GetFileName(statePath));
					using var preview = EmuApi.GetSaveStatePreview(statePath);
					preview?.Save(Path.Combine(home, "starting-scene.png"), PngBitmapEncoderOptions.Default);
				}
				using var viewer = new SpriteViewerViewModel(cpu, new PictureViewer(), new ScrollPictureViewer(), new Grid(), new Grid(), new Window());
				string hash = EmuApi.GetRomHash(HashType.Sha1);
				var pack = MetaSpritePack.Load(Path.Combine(home, "HdPacks", "MetaSprites", cpu.ToString(), hash, "groups.json"), hash, cpu.ToString());
				// Populate recognition metadata without activating again: only the
				// selected GameLoaded path may decode/apply assets in this comparison.
				foreach(var group in pack.Groups) { viewer.MetaGroups.Add(group); }
				const int viewerId = 19072;
				int cameraX = -1, previousCameraX = -1, stableCameraFrames = 0;
				bool cameraMoved = false;
				bool cameraMovedAfterReversal = false;
				int movementPhase = 0;
				int boundaryCameraX = -1;
				uint movementFrames = 0, boundaryFrame = 0, reversalFrame = 0;
				listener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.ViewerRefresh || e.Parameter.ToInt32() != viewerId) { return; }
					viewer.RefreshData();
					if(!scrollTurn) { return; }
					NesPpuState ppu = DebugApi.GetPpuState<NesPpuState>(cpu);
					int x = ((ppu.VideoRamAddr & 0x1F) << 3) | ((ppu.VideoRamAddr & 0x400) != 0 ? 256 : 0);
					x -= (int)(ppu.Cycle & ~7u) + 16;
					cameraX = (x + ppu.ScrollX + 512) % 512;
					movementFrames++;
					if(previousCameraX >= 0 && cameraX != previousCameraX) {
						cameraMoved = true; stableCameraFrames = 0;
						if(movementPhase == 2) { cameraMovedAfterReversal = true; }
					}
					else { stableCameraFrames++; }
					previousCameraX = cameraX;
					if(movementPhase == 0 && cameraMoved && stableCameraFrames >= 30) {
						movementPhase = 1; boundaryFrame = movementFrames; boundaryCameraX = cameraX;
						Console.WriteLine($"SCROLL TURN: camera stationary for 30 frames at scroll X={cameraX}, movement frame={movementFrames}; continue right for 30 frames.");
					} else if(movementPhase == 1 && cameraX != boundaryCameraX) {
						movementPhase = 0;
						Console.WriteLine($"SCROLL TURN: camera resumed before reversal at movement frame={movementFrames}; keep walking right.");
					} else if(movementPhase == 1 && movementFrames - boundaryFrame >= 30) {
						DebugApi.SetInputOverrides(0, new DebugControllerState { Left = true });
						movementPhase = 2; reversalFrame = movementFrames;
						Console.WriteLine($"SCROLL TURN: reverse left at movement frame={movementFrames}, scroll X={cameraX}.");
					} else if(movementPhase == 2 && ((cameraMovedAfterReversal && stableCameraFrames >= 30) || movementFrames - reversalFrame >= 360)) {
						DebugApi.SetInputOverrides(0, default);
						movementPhase = 3;
						Console.WriteLine($"SCROLL TURN: neutral input at movement frame={movementFrames}, scroll X={cameraX}.");
					}
				};
				DebugApi.SetViewerUpdateTiming(viewerId, scrollTurn ? 120 : 8, 12, cpu);
				if(scrollTurn) { DebugApi.SetInputOverrides(0, new DebugControllerState { Right = true }); }
				if(statePath != null) { EmuApi.Resume(); }
				var timer = Stopwatch.StartNew();
				double previousSeconds = 0;
				uint previousFrame = EmuApi.GetTimingInfo(cpu).FrameCount;
				long previousAllocated = GC.GetTotalAllocatedBytes(true);
				int previousGen0 = GC.CollectionCount(0), previousGen1 = GC.CollectionCount(1), previousGen2 = GC.CollectionCount(2);
				string report = "seconds,frames_per_second,allocated_bytes,gen0,gen1,gen2,camera_x,movement_phase\n";
				bool observedMatches = false;
				while(timer.Elapsed.TotalSeconds < 60) {
					Dispatcher.UIThread.RunJobs();
					observedMatches |= viewer.MetaMatchStatus.StartsWith("Meta ", StringComparison.Ordinal);
					double seconds = timer.Elapsed.TotalSeconds;
					if(seconds - previousSeconds >= 5) {
						uint frame = EmuApi.GetTimingInfo(cpu).FrameCount;
						double fps = (frame - previousFrame) / (seconds - previousSeconds);
						long allocated = GC.GetTotalAllocatedBytes(true);
						int gen0 = GC.CollectionCount(0), gen1 = GC.CollectionCount(1), gen2 = GC.CollectionCount(2);
						Console.WriteLine($"COLD TIMING: {mode}, {seconds:F1}s, {fps:F2} emulated FPS, {allocated - previousAllocated} bytes, GC {gen0 - previousGen0}/{gen1 - previousGen1}/{gen2 - previousGen2}, camera X={cameraX}, phase={movementPhase}; excludes desktop presentation.");
						report += FormattableString.Invariant($"{seconds:F3},{fps:F3},{allocated - previousAllocated},{gen0 - previousGen0},{gen1 - previousGen1},{gen2 - previousGen2},{cameraX},{movementPhase}\n");
						previousAllocated = allocated; previousGen0 = gen0; previousGen1 = gen1; previousGen2 = gen2;
						previousSeconds = seconds; previousFrame = frame;
						if(scrollTurn) {
							EmuApi.TakeScreenshot();
							Console.WriteLine("SCENE MATCHES: " + viewer.MetaMatchStatus.Replace(Environment.NewLine, "; "));
						}
						if(softwareView?.DataContext is SoftwareRendererViewModel softwareModel && softwareModel.FrameSurface is DynamicBitmap surface) {
							using var snapshot = new WriteableBitmap(surface.PixelSize, surface.Dpi, PixelFormat.Bgra8888, AlphaFormat.Premul);
							using(var framebuffer = snapshot.Lock()) {
								surface.CopyPixels(new PixelRect(0, 0, surface.PixelSize.Width, surface.PixelSize.Height), framebuffer.Address, framebuffer.RowBytes * framebuffer.Size.Height, framebuffer.RowBytes);
							}
							snapshot.Save(Path.Combine(home, $"composited-{(int)seconds:D2}.png"), PngBitmapEncoderOptions.Default);
						}
					}
					System.Threading.Thread.Sleep(1);
				}
				DebugApi.RemoveViewerId(viewerId, cpu);
				if(scrollTurn) {
					DebugApi.SetInputOverrides(0, default);
					Console.WriteLine($"SCROLL TURN RESULT: camera moved={cameraMoved}, reversal reached={movementPhase >= 2}, completed={movementPhase == 3}.");
				}
				if(!applied) { throw new Exception("Cold-start saved pack activation did not succeed."); }
				if(settingsPath != null && pack.Groups.Count > 0 && !observedMatches) { throw new Exception("Configured run never recognized a supplied group; timings do not verify actual replacement artwork."); }
				if(softwareRenderer && (softwareView?.DataContext is not SoftwareRendererViewModel renderedModel || renderedModel.FrameSurface == null)) { throw new Exception("Software renderer never delivered a frame."); }
				File.WriteAllText(Path.Combine(home, "startup-timing.csv"), report);
				Console.WriteLine("Cold timing saved: " + Path.Combine(home, "startup-timing.csv"));
			} finally { DebugApi.ReleaseDebugger(); }
		} finally {
			EmuApi.Stop(); EmuApi.Release();
			if(softwareView?.DataContext is SoftwareRendererViewModel softwareModel) {
				softwareModel.FrameSurface?.Dispose(); softwareModel.EmuHudSurface?.Dispose(); softwareModel.ScriptHudSurface?.Dispose();
			}
		}
	}

	private static void CheckSelection(string system, bool startupTiming, bool recognitionBenchmark, bool pointerTesting = false)
	{
		var (cpu, extension) = system switch {
			"Snes" => (CpuType.Snes, "sfc"), "Nes" => (CpuType.Nes, "nes"),
			"SnesObj" => (CpuType.Snes, "obj-interlace-group-holes.sfc"),
			"Gameboy" => (CpuType.Gameboy, "gb"), "Gbc" => (CpuType.Gameboy, "gbc"),
			"Gba" => (CpuType.Gba, "gba"), "Pce" => (CpuType.Pce, "pce"),
			"Sms" => (CpuType.Sms, "sms"), "SmsLegacy" => (CpuType.Sms, "legacy-mode0.sms"), "Ws" => (CpuType.Ws, "wsc"),
			_ => throw new ArgumentException("Unknown selection test system: " + system)
		};
		string romPath = Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture." + extension);
		string home = Path.Combine(AppContext.BaseDirectory, "artifacts", "selection-" + Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(home);
		typeof(ConfigManager).GetField("_homeFolder", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, home);
		typeof(ConfigManager).GetField("_config", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, Configuration.CreateConfig());
		EmuApi.InitDll();
		EmuApi.InitializeEmu(home, IntPtr.Zero, IntPtr.Zero, true, true, true, true);
		ConfigManager.Config.ApplyConfig();
		try {
			if(!EmuApi.LoadRom(romPath)) { throw new Exception("Selection fixture ROM failed to load: " + system); }
			System.Threading.Thread.Sleep(150);
			EmuApi.Pause();
			DebugApi.InitializeDebugger();
			try {
				if(pointerTesting) { CheckPointerInteraction(cpu); return; }
				using var viewer = new SpriteViewerViewModel(cpu, new PictureViewer(), new ScrollPictureViewer(), new Grid(), new Grid(), new Window());
				var main = new SpritePreviewModel { SpriteIndex = 0, PreviewX = 10, PreviewY = 10, Width = 8, Height = 8, Visibility = SpriteVisibility.Visible };
				var wrapped = new SpritePreviewModel { SpriteIndex = 1, PreviewX = 252, PreviewY = 252, Width = 8, Height = 8, WrapWidth = 256, WrapHeight = 256, Visibility = SpriteVisibility.Visible };
				var disabled = new SpritePreviewModel { SpriteIndex = 2, PreviewX = 0, PreviewY = 0, Width = 8, Height = 8, Visibility = SpriteVisibility.Disabled };
				viewer.SpritePreviews = new() { main, wrapped, disabled };
				viewer.SelectMetaSprites(new Rect(0, 0, 4, 4));
				if(viewer.SelectionRect != new Rect(-4, -4, 8, 8)) { throw new Exception("Wrapped rectangle selection did not retain the selected copy's origin."); }
				viewer.ToggleMetaSprite(main);
				if(viewer.SelectionRect != new Rect(-4, -4, 22, 22)) { throw new Exception("Ctrl-click addition did not preserve the wrapped member offset."); }
				viewer.ToggleMetaSprite(wrapped);
				if(viewer.SelectionRect != new Rect(10, 10, 8, 8)) { throw new Exception("Ctrl-click removal did not update group bounds."); }
				viewer.SelectMetaSprites(new Rect(18, 10, 1, 8));
				if(viewer.SelectionRect != default) { throw new Exception("Edge contact selected a sprite without positive overlap."); }
				viewer.ToggleMetaSprite(wrapped, new Rect(-4, 252, 8, 8));
				if(viewer.SelectionRect != new Rect(-4, 252, 8, 8)) { throw new Exception("Ctrl-click did not retain the clicked wrap copy."); }
				viewer.ClearMetaSelection();
				if(viewer.SelectionRect != default) { throw new Exception("Clearing group selection retained its bounds."); }
				Console.WriteLine("PASS: actual viewer model rectangle selection, disabled-sprite exclusion, wrapped-copy offsets, Ctrl-click toggling, edge contact and clear.");
				viewer.LoadMetaGroups();
				viewer.RefreshData();
				Dispatcher.UIThread.RunJobs();
				if(viewer.SpritePreviews.Count == 0 || viewer.SpritePreviews[0].Width <= 0 || viewer.SpritePreviews[0].Height <= 0) { throw new Exception("Actual sprite snapshot was not refreshed: " + system); }
				viewer.ToggleMetaSprite(viewer.SpritePreviews[0]);
				if(system == "SnesObj") {
					var second=viewer.SpritePreviews.Single(s => s.SpriteIndex==1);
					if(viewer.ViewerBitmap.PixelSize.Height!=512 || second.PreviewY-viewer.SpritePreviews[0].PreviewY!=8) { throw new Exception("OBJ-interlace preview did not retain full-source vertical coordinates."); }
					viewer.ToggleMetaSprite(second);
				}
				viewer.CreateMetaGroup();
				if(viewer.MetaGroups.Count != 1 || !viewer.MetaGroups[0].Enabled || !viewer.MetaStatus.StartsWith("Replacements applied.")) {
					throw new Exception("Actual Create flow failed: " + viewer.MetaStatus);
				}
				string hash = EmuApi.GetRomHash(HashType.Sha1);
				string packPath = Path.Combine(home, "HdPacks", "MetaSprites", cpu.ToString(), hash, "groups.json");
				var saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(saved.Groups.Count != 1 || !saved.Groups[0].Enabled || !File.Exists(Path.Combine(Path.GetDirectoryName(packPath)!, saved.Groups[0].ReplacementPath))) {
					throw new Exception("Actual Create flow did not persist its enabled PNG group in the default folder.");
				}
				if(system == "SnesObj") {
					var group=saved.Groups[0];
					if(group.Width!=20 || group.Height!=16 || group.Members.Count!=2 || group.Members[1].X!=12 || group.Members[1].Y!=8) { throw new Exception("Authored OBJ-interlace member arrangement used field-height coordinates."); }
					(int Width,int Height,uint[] Pixels) Decode(string path) {
						using var bitmap=new Bitmap(path); int count=bitmap.PixelSize.Width*bitmap.PixelSize.Height;
						IntPtr pixels=Marshal.AllocHGlobal(count*4);
						try {
							bitmap.CopyPixels(new PixelRect(bitmap.PixelSize),pixels,count*4,bitmap.PixelSize.Width*4);
							int[] raw=new int[count]; Marshal.Copy(pixels,raw,0,count);
							return (bitmap.PixelSize.Width,bitmap.PixelSize.Height,raw.Select(p => unchecked((uint)p)).ToArray());
						} finally { Marshal.FreeHGlobal(pixels); }
					}
					var exported=Decode(Path.Combine(Path.GetDirectoryName(packPath)!,group.ReplacementPath));
					if(exported.Width!=20 || exported.Height!=16 || (exported.Pixels[8]>>24)!=0 || (exported.Pixels[7]>>24)!=0 || (exported.Pixels[8*20+12]>>24)!=255) { throw new Exception("Default OBJ-interlace PNG lost source layout, transparent gap or native hole."); }
					string enlarged=Path.Combine(home,"obj-canvas.png");
					using(var canvas=new WriteableBitmap(new PixelSize(22,18),new Vector(96,96),PixelFormat.Bgra8888,AlphaFormat.Unpremul)) {
						using(var pixels=canvas.Lock()) { Marshal.Copy(Enumerable.Repeat(unchecked((int)0xFF00FF00),22*18).ToArray(),0,pixels.Address,22*18); }
						canvas.Save(enlarged,PngBitmapEncoderOptions.Default);
					}
					viewer.ImportMetaReplacementPng(enlarged);
					if(!viewer.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Authored interlace artwork import failed: "+viewer.MetaStatus); }
					saved=MetaSpritePack.Load(packPath,hash,cpu.ToString());
					byte[] packet=MetaSpriteRuntimePacket.Create(saved,g => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,g.ReplacementPath)));
					uint result=RunMetaSpriteSnesAuthoredPackTest(romPath,packet,(uint)packet.Length);
					if(result!=7) { throw new Exception("Viewer-authored OBJ-interlace pack failed live whole-frame replacement: "+result); }
					viewer.LoadMetaGroups();
					if(viewer.MetaGroups.Count!=1 || viewer.MetaGroups[0].Members[1].Y!=8) { throw new Exception("OBJ-interlace group reload lost source offsets."); }
					Console.WriteLine("PASS: actual SNES OBJ-interlace two-member selection, full-source PNG export, oversized PNG import, persisted pack decode, live whole-frame rendering and reload.");
					string whitePath=Path.Combine(Path.GetDirectoryName(packPath)!,"animation-white.png");
					using(var canvas=new WriteableBitmap(new PixelSize(22,18),new Vector(96,96),PixelFormat.Bgra8888,AlphaFormat.Unpremul)) {
						using(var pixels=canvas.Lock()) { Marshal.Copy(Enumerable.Repeat(unchecked((int)0xFFFFFFFF),22*18).ToArray(),0,pixels.Address,22*18); }
						canvas.Save(whitePath,PngBitmapEncoderOptions.Default);
					}
					string portraitPath=Path.Combine(home,"portrait-preview.png");
					using(var portrait=new WriteableBitmap(new PixelSize(32,1024),new Vector(96,96),PixelFormat.Bgra8888,AlphaFormat.Unpremul)) { portrait.Save(portraitPath,PngBitmapEncoderOptions.Default); }
					using(var portraitEditor=new PngAnimationViewModel("Portrait test",Path.GetDirectoryName(packPath)!,new(),2,2,3,true,_ => (true,"Saved"))) {
						portraitEditor.AddFrame(portraitPath);
						if(portraitEditor.PreviewHeight!=512 || portraitEditor.PreviewWidth!=16) { throw new Exception("Portrait animation thumbnail was not bounded on both axes."); }
						portraitEditor.SetPreviewOrigin(4,16);
						if(portraitEditor.SelectedFrame?.OriginX!=4 || portraitEditor.SelectedFrame.OriginY!=16 || portraitEditor.PreviewOriginX!=4 || portraitEditor.PreviewOriginY!=16) { throw new Exception("Portrait thumbnail origin did not round-trip through native coordinates."); }
					}
					Console.WriteLine("PASS: portrait animation thumbnails fit both axes and preserve scaled click/crosshair origin coordinates.");
					using(var aligned=new PngAnimationViewModel("Aligned playback test",Path.GetDirectoryName(packPath)!,new(),2,2,3,true,_ => (true,"Saved"))) {
						aligned.AddFrames(new[] {whitePath,portraitPath});
						if(aligned.Frames.Any(f => f.Duration!=8)) { throw new Exception("New animation frames did not default to eight emulated frames."); }
						if(aligned.Frames.Count!=2 || aligned.Frames[0].Path!=Path.GetFileName(whitePath) || !aligned.Frames[1].Path.StartsWith("portrait-preview")) { throw new Exception("Batch PNG import lost picker order or in-pack filename reuse."); }
						aligned.OverrideOrigin=true; aligned.FrameOriginX=10; aligned.FrameOriginY=-3;
						aligned.StartPreview();
						if(!aligned.IsPlaying) { throw new Exception("Aligned preview did not start: "+aligned.Status); }
						int width=aligned.PreviewWidth,height=aligned.PreviewHeight;
						double anchorX=aligned.PreviewOriginX,anchorY=aligned.PreviewOriginY,firstLeft=aligned.PreviewImageLeft;
						for(int tick=0;tick<8;tick++) { aligned.AdvancePreview(); }
						if(aligned.PreviewWidth!=width || aligned.PreviewHeight!=height || width>512 || height>512 || Math.Abs(aligned.PreviewOriginX-anchorX)>0.00001 || Math.Abs(aligned.PreviewOriginY-anchorY)>0.00001 || aligned.PreviewImageLeft==firstLeft) { throw new Exception("Different-size animation frames shifted the shared preview origin or canvas."); }
						aligned.SetPreviewOrigin(anchorX,anchorY);
						if(aligned.IsPlaying || aligned.SelectedFrame?.OriginX!=10 || aligned.SelectedFrame.OriginY!=-3) { throw new Exception("Clicking an aligned playback origin did not preserve native coordinates."); }
						aligned.AddFrames(Enumerable.Repeat(whitePath,255));
						if(aligned.Frames.Count!=2 || !aligned.Status.Contains("No frames were added")) { throw new Exception("Over-capacity batch import partially changed the sequence."); }
						aligned.AddFrames(new[] {whitePath,Path.Combine(home,"missing-animation-frame.png")});
						if(aligned.Frames.Count!=3 || !aligned.Status.StartsWith("Added 1 of 2 frames.")) { throw new Exception("Partial batch failure did not retain/report successfully imported frames."); }
					}
					Console.WriteLine("PASS: different-size animation frames share a bounded origin-aligned playback canvas and accurate origin clicks.");
					Console.WriteLine("PASS: batch PNG import preserves order and in-pack names, rejects excess capacity before changes and reports partial failures.");
					using(var batch=new PngAnimationViewModel("Batch refresh test",Path.GetDirectoryName(packPath)!,new(),1,0,0,false,_ => (true,"Saved"))) {
						int previews=0;
						batch.PropertyChanged+=(_,e) => { if(e.PropertyName==nameof(batch.Preview) && batch.Preview!=null) { previews++; } };
						batch.AddFrames(Enumerable.Repeat(whitePath,64));
						if(batch.Frames.Count!=64 || previews!=1 || batch.SelectedFrame!=batch.Frames[63] || !batch.MemoryInfo.Contains("0.0 MiB")) { throw new Exception("Batch import refreshed intermediate frames or failed final selection/measurement."); }
					}
					Console.WriteLine("PASS: 64-frame batch imports refresh and measure the preview once, selecting the final frame.");
					viewer.SelectedMetaGroup=viewer.MetaGroups[0];
					viewer.SelectedMetaGroup.Animation=new() { Enabled=true,Frames=new() {
						new() {Path=viewer.SelectedMetaGroup.ReplacementPath},
						new() {Path="animation-white.png",OriginX=viewer.SelectedMetaGroup.ReplacementOriginX+1,OriginY=viewer.SelectedMetaGroup.ReplacementOriginY+1}
					} };
					var textPack=MetaSpritePack.Load(packPath,hash,cpu.ToString());
					textPack.TextProfiles.Add(new() {Id=1,Name="Dialog fixture",Mappings=new() {new() {Tile=0x2A,Character="A"}}}); textPack.Save(packPath);
					viewer.SaveMetaGroups();
					if(!viewer.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Actual animation PNG loading failed: "+viewer.MetaStatus); }
					saved=MetaSpritePack.Load(packPath,hash,cpu.ToString());
					if(saved.TextProfiles.Single().Name!="Dialog fixture") { throw new Exception("Sprite editor save erased independently saved text profiles."); }
					packet=MetaSpriteRuntimePacket.Create(saved,g => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,g.ReplacementPath)),
						loadFrame:(frame,_) => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,frame.Path)));
					result=RunMetaSpriteSnesAuthoredPackTest(romPath,packet,(uint)packet.Length);
					if(result!=7) { throw new Exception("Saved animated PNG pack failed field-clock live rendering: "+result); }
					Console.WriteLine("PASS: actual PNG animation files load/save, MSP5 activation and live woven full-canvas playback with distinct field clocks and frame origins.");
					var sharedSpritePack=MetaSpritePack.Load(packPath,hash,cpu.ToString());
					sharedSpritePack.Groups[0].Animation.Frames.Add(new() {Path=sharedSpritePack.Groups[0].ReplacementPath,
						OriginX=sharedSpritePack.Groups[0].ReplacementOriginX+2,OriginY=sharedSpritePack.Groups[0].ReplacementOriginY+2});
					byte[] sharedSpritePacket=MetaSpriteRuntimePacket.Create(sharedSpritePack,g => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,g.ReplacementPath)),
						loadFrame:(frame,_) => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,frame.Path)),sharedFrames:true);
					if(BitConverter.ToUInt32(sharedSpritePacket)!=MetaSpriteRuntimePacket.SharedAnimationMagic || RunMetaSpriteSnesAuthoredPackTest(romPath,sharedSpritePacket,(uint)sharedSpritePacket.Length)!=7) {
						throw new Exception("MSP6 shared real PNG frames failed live whole-frame field playback.");
					}
					Console.WriteLine("PASS: actual repeated PNG frames use MSP6 references with independent origins and live whole-frame SNES field playback.");

					saved.BackgroundLayers.Add(new() { Id=1,Enabled=true,NativeLayer=0,CompositionPosition=1,FollowScroll=false,HideOriginalLayer=true,
						ArtworkPath=saved.Groups[0].ReplacementPath,ArtworkWidth=22,ArtworkHeight=18,X=8,Y=15,
						MemoryConditions=new() {new() {Address=0,Mask=0}}, Animation=new() {Enabled=true,Frames=new() {
							new() {Path=saved.Groups[0].ReplacementPath},new() {Path="animation-white.png",OriginX=1,OriginY=1}
						}}
					});
					saved.Save(packPath); viewer.LoadMetaGroups(); viewer.SaveMetaGroups();
					if(!viewer.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Background animation PNG loading failed: "+viewer.MetaStatus); }
					byte[] backgroundPacket=MetaBackgroundRuntimePacket.Create(saved,layer => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,layer.ArtworkPath)),
						(frame,_) => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,frame.Path)));
					string backgroundRom=Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture.obj-interlace-bg-holes.sfc");
					result=RunMetaBackgroundSnesAnimationTest(backgroundRom,backgroundPacket,(uint)backgroundPacket.Length);
					if(result!=7) { throw new Exception("Saved background animation failed live whole-frame playback: "+result); }
					Console.WriteLine("PASS: actual background PNG animation files, saved settings, MBGH loader activation and live whole-frame SNES field playback.");
					string spritePacketPath=Path.Combine(home,"file-\u03A9-sprites.bin"), bgPacketPath=Path.Combine(home,"file-\u03A9-backgrounds.bin");
					string emptySpritePath=Path.Combine(home,"file-empty-sprites.bin"), emptyBgPath=Path.Combine(home,"file-empty-backgrounds.bin"), badBgPath=Path.Combine(home,"file-bad-background.bin");
					File.WriteAllBytes(spritePacketPath,packet); File.WriteAllBytes(bgPacketPath,backgroundPacket);
					File.WriteAllBytes(emptySpritePath,MetaSpriteRuntimePacket.Create(new(),_ => throw new Exception("Empty pack decoded artwork.")));
					File.WriteAllBytes(emptyBgPath,MetaBackgroundRuntimePacket.Create(new(),_ => throw new Exception("Empty pack decoded artwork.")));
					File.WriteAllBytes(badBgPath,backgroundPacket.Take(backgroundPacket.Length-1).ToArray());
					byte[] emptyBgData=File.ReadAllBytes(emptyBgPath);
					if(!DebugApi.SetMetaReplacements(hash,packet,(uint)packet.Length,emptyBgData,(uint)emptyBgData.Length) || GetMetaReplacementDefinitionFlagsTest()!=1) { throw new Exception("Legacy byte-array API did not retain its successful activation path."); }
					if(!DebugApi.SetMetaReplacementFiles(hash,spritePacketPath,emptyBgPath) || GetMetaReplacementDefinitionFlagsTest()!=1) { throw new Exception("File API failed valid Unicode-path sprite activation."); }
					if(DebugApi.SetMetaReplacementFiles(hash,emptySpritePath,badBgPath) || GetMetaReplacementDefinitionFlagsTest()!=1) { throw new Exception("Bad file background cleared the installed sprite pack."); }
					if(DebugApi.SetMetaReplacementFiles("wrong-rom",emptySpritePath,emptyBgPath) || GetMetaReplacementDefinitionFlagsTest()!=1
						|| DebugApi.SetMetaReplacementFiles(hash,spritePacketPath+".missing",emptyBgPath) || GetMetaReplacementDefinitionFlagsTest()!=1) { throw new Exception("Wrong ROM/missing packet file changed installed definitions."); }
					if(!DebugApi.SetMetaReplacementFiles(hash,spritePacketPath,bgPacketPath) || GetMetaReplacementDefinitionFlagsTest()!=3) { throw new Exception("File API failed combined activation."); }
					if(DebugApi.SetMetaReplacementFiles(hash,emptySpritePath,badBgPath) || GetMetaReplacementDefinitionFlagsTest()!=3) { throw new Exception("Failed file transaction replaced one set before validating both."); }
					if(!DebugApi.SetMetaReplacementFiles(hash,emptySpritePath,emptyBgPath) || GetMetaReplacementDefinitionFlagsTest()!=0) { throw new Exception("Empty file packets did not clear both sets."); }
					viewer.LoadMetaGroups(); viewer.SaveMetaGroups();
					if(!viewer.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Direct-file runtime loading did not restore saved PNG definitions: "+viewer.MetaStatus); }
					Console.WriteLine("PASS: file API Unicode paths, legacy byte activation, combined activation/clear, missing/wrong-ROM rejection and retention after a malformed second packet.");

					var sharedBackgroundPack=MetaSpritePack.Load(packPath,hash,cpu.ToString());
					sharedBackgroundPack.BackgroundLayers[0].Animation.Frames[1].Path=sharedBackgroundPack.BackgroundLayers[0].Animation.Frames[0].Path;
					byte[] sharedBackgroundPacket=MetaBackgroundRuntimePacket.Create(sharedBackgroundPack,layer => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,layer.ArtworkPath)),
						(frame,_) => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,frame.Path)),sharedFrames:true);
					if(BitConverter.ToUInt32(sharedBackgroundPacket)!=MetaBackgroundRuntimePacket.SharedAnimationMagic || RunMetaBackgroundSnesAnimationTest(backgroundRom,sharedBackgroundPacket,(uint)sharedBackgroundPacket.Length)!=7) {
						throw new Exception("MBGI shared real background PNG frames failed live field playback.");
					}
					Console.WriteLine("PASS: actual repeated background PNG frames use MBGI references with independent origins and live whole-frame field playback.");
					foreach(byte match in new byte[] {0,1}) {
						var fieldPack=MetaSpritePack.Load(packPath,hash,cpu.ToString()); var fieldLayer=fieldPack.BackgroundLayers[0];
						fieldLayer.MemoryConditions=new() {new() {Address=0,Value=match,Mask=255}};
						fieldLayer.FollowScroll=true; fieldLayer.UseCpuCamera=true;
						fieldLayer.CameraX=new() {Format=1,Address=0}; fieldLayer.CameraY=new() {Format=1,Address=1};
						byte[] fieldPacket=MetaBackgroundRuntimePacket.Create(fieldPack,layer => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,layer.ArtworkPath)),
							(frame,_) => Decode(Path.Combine(Path.GetDirectoryName(packPath)!,frame.Path)),sharedFrames:true);
						string fieldRom=Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture.obj-interlace-bg-fields-holes.sfc");
						if(RunMetaBackgroundSnesAnimationTest(fieldRom,fieldPacket,(uint)fieldPacket.Length)!=7) { throw new Exception("Woven background field conditions/cameras failed for RAM match "+match); }
					}
					Console.WriteLine("PASS: real SNES woven fields retain opposite background RAM conditions and independent X/Y CPU camera snapshots in whole-frame PNG playback.");


					viewer.SelectedMetaGroup=viewer.MetaGroups[0];
					using(var editor=viewer.CreateMetaAnimationEditor() ?? throw new Exception("Missing default PNG animation editor.")) {
						var original=viewer.SelectedMetaGroup.Animation; editor.FrameDuration=2;
						if(original.Frames[0].Duration!=1) { throw new Exception("Animation draft changed its owner before save."); }
						var window=new Mesen.Debugger.Windows.PngAnimationWindow(editor);
						var restartControl=window.FindControl<CheckBox>("animationRestart") ?? throw new Exception("Missing animation restart control.");
						restartControl.IsChecked=true; Dispatcher.UIThread.RunJobs();
						if(!editor.RestartOnActivation || original.RestartOnActivation) { throw new Exception("Restart control failed binding or changed the saved owner before save."); }
						var first=editor.SelectedFrame;
						window.FindControl<Button>("animationPrevious")!.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
						window.FindControl<Button>("animationNext")!.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
						if(!ReferenceEquals(first,editor.SelectedFrame)) { throw new Exception("Animation window navigation was not routed."); }
						window.FindControl<Button>("animationPlay")!.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
						editor.AdvancePreview(); if(!ReferenceEquals(first,editor.SelectedFrame)) { throw new Exception("Preview ignored frame duration."); }
						editor.AdvancePreview(); if(ReferenceEquals(first,editor.SelectedFrame)) { throw new Exception("Preview did not advance at the duration boundary."); }
						window.FindControl<Button>("animationStop")!.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
						if(editor.IsPlaying) { throw new Exception("Animation stop control failed."); }
						editor.RemoveFrame(); if(!File.Exists(whitePath)) { throw new Exception("Removing an animation frame deleted its shared PNG."); }
						int files=Directory.GetFiles(Path.GetDirectoryName(packPath)!,"*.png").Length; editor.AddFrame(whitePath);
						if(Directory.GetFiles(Path.GetDirectoryName(packPath)!,"*.png").Length!=files) { throw new Exception("Animation importer copied a PNG already in the pack."); }
						editor.AddFrame(enlarged); if(editor.SelectedFrame?.Path!="obj-canvas.png") { throw new Exception("Animation import did not preserve the external filename."); }
						editor.Move(-1); editor.Browse(1); editor.SetPreviewOrigin(2,3); editor.FrameDuration=3; editor.PlaybackMode=1;
						if(!editor.OverrideOrigin || editor.SelectedFrame?.OriginX!=2 || editor.SelectedFrame.OriginY!=3) { throw new Exception("Animation preview origin mapping failed."); }
						string beforeAnimationSave=File.ReadAllText(packPath); Directory.CreateDirectory(packPath+".tmp");
						try { if(editor.Save()) { throw new Exception("Failed animation save reported success."); } } finally { Directory.Delete(packPath+".tmp"); }
						if(!ReferenceEquals(original,viewer.SelectedMetaGroup.Animation) || File.ReadAllText(packPath)!=beforeAnimationSave) { throw new Exception("Failed animation save did not roll back owner/settings."); }
						if(!editor.Save()) { throw new Exception("Animation editor retry failed: "+editor.Status); }
						var authored=MetaSpritePack.Load(packPath,hash,cpu.ToString()).Groups[0].Animation;
						if(authored.Frames.Count!=3 || !authored.RestartOnActivation || authored.Playback!=MetaPngPlayback.PingPong || authored.Frames[2].OriginX!=2) { throw new Exception("Animation controls did not persist frames/order/origins: "+authored.Playback+" / "+string.Join(",",authored.Frames.Select(f => f.Path+":"+f.OriginX))); }
						window.Close();
					}
					viewer.MetaAlternateConditionsText="$0 == $00 / $00"; viewer.AddMetaAlternatePng(enlarged);
					using(var alternateEditor=viewer.CreateMetaAnimationEditor() ?? throw new Exception("Missing alternate animation editor.")) {
						alternateEditor.AddFrame(whitePath); alternateEditor.FrameDuration=4; alternateEditor.RestartOnActivation=true;
						if(!alternateEditor.Save()) { throw new Exception("Alternate animation editor save failed: "+alternateEditor.Status); }
						if(viewer.SelectedMetaAlternate?.Animation.RestartOnActivation!=true || viewer.SelectedMetaAlternate?.Animation.Frames.Single().Duration!=4 || viewer.SelectedMetaGroup!.Animation.Frames.Count!=3) { throw new Exception("Alternate animation edited the default sequence."); }
					}
					using(var tilemap=new TilemapViewerViewModel(cpu,new PictureViewer(),new ScrollPictureViewer(),new Window())) {
						tilemap.OnGameLoaded(); Dispatcher.UIThread.RunJobs(); tilemap.BackgroundEditor.SelectedMetaBackground=tilemap.BackgroundEditor.MetaBackgrounds[0];
						using var backgroundEditor=tilemap.BackgroundEditor.CreateAnimationEditor() ?? throw new Exception("Missing background animation editor.");
						backgroundEditor.RestartOnActivation=true; backgroundEditor.FrameDuration=5; if(!backgroundEditor.Save()) { throw new Exception("Background animation editor save failed: "+backgroundEditor.Status); }
						var authored=MetaSpritePack.Load(packPath,hash,cpu.ToString());
						if(!authored.BackgroundLayers[0].Animation.RestartOnActivation || authored.BackgroundLayers[0].Animation.Frames[0].Duration!=5 || authored.Groups[0].Animation.Frames.Count!=3 || authored.TextProfiles.Single().Name!="Dialog fixture") { throw new Exception("Background animation save lost updated sprite sequences or text profiles."); }
					}
					var statePack=MetaSpritePack.Load(packPath,hash,cpu.ToString());
					string stateFolder=Path.GetDirectoryName(packPath)!;
					byte[] stateSprites=MetaSpriteRuntimePacket.Create(statePack,g => Decode(Path.Combine(stateFolder,g.ReplacementPath)),
						a => Decode(Path.Combine(stateFolder,a.Path)),(f,_) => Decode(Path.Combine(stateFolder,f.Path)));
					byte[] stateBackgrounds=MetaBackgroundRuntimePacket.Create(statePack,b => Decode(Path.Combine(stateFolder,b.ArtworkPath)),
						(f,_) => Decode(Path.Combine(stateFolder,f.Path)));
					uint stateResult=RunMetaAnimationStateTest(stateSprites,(uint)stateSprites.Length,stateBackgrounds,(uint)stateBackgrounds.Length);
					if(stateResult!=0) { throw new Exception("Emulator animation state test failed at stage "+stateResult); }
					Console.WriteLine("PASS: PNG animation editor XAML controls, draft isolation, timing preview, frame import/reuse/order/removal, origins, rollback/retry and default/alternate/background persistence.");
					string thumbnailPath=Path.Combine(home,"wide-preview.png");
					using(var wide=new WriteableBitmap(new PixelSize(1024,32),new Vector(96,96),PixelFormat.Bgra8888,AlphaFormat.Unpremul)) { wide.Save(thumbnailPath,PngBitmapEncoderOptions.Default); }
					using(var scaled=new PngAnimationViewModel("Scaled test",Path.GetDirectoryName(packPath)!,new(),2,2,3,false,_ => (true,"Saved"))) {
						scaled.AddFrame(thumbnailPath); if(scaled.PreviewWidth!=512) { throw new Exception("Animation thumbnail was not bounded."); }
						scaled.SetPreviewOrigin(16,4);
						if(scaled.SelectedFrame?.OriginX!=16 || scaled.SelectedFrame.OriginY!=4) { throw new Exception("Scaled thumbnail click did not map back to native origin units."); }
						scaled.OverrideOrigin=false;
						if(scaled.FrameOriginX!=2 || scaled.FrameOriginY!=3 || scaled.SelectedFrame.OriginX!=null || scaled.SelectedFrame.OriginY!=null) { throw new Exception("Clearing frame origin override did not restore inherited origin."); }
					}
					viewer.SelectedMetaGroup=viewer.MetaGroups[0];
					using(var stale=viewer.CreateMetaAnimationEditor() ?? throw new Exception("Missing stale-animation test editor.")) {
						string latestAnimation=File.ReadAllText(packPath); viewer.LoadMetaGroups();
						if(stale.Save() || File.ReadAllText(packPath)!=latestAnimation || !stale.Status.Contains("changed")) { throw new Exception("Animation dialog saved against a replaced pack context."); }
					}
					Console.WriteLine("PASS: bounded animation preview, scaled origin clicks/inheritance and stale-context save rejection.");
					string repeatPath=Path.Combine(Path.GetDirectoryName(packPath)!,"repeated-large.png");
					using(var repeated=new WriteableBitmap(new PixelSize(1024,1024),new Vector(96,96),PixelFormat.Bgra8888,AlphaFormat.Unpremul)) { repeated.Save(repeatPath,PngBitmapEncoderOptions.Default); }
					var repeatSequence=new MetaPngAnimation {Enabled=true};
					for(int i=0;i<129;i++) { repeatSequence.Frames.Add(new() {Path="repeated-large.png"}); }
					bool repeatSaved=false;
					using(var repeatEditor=new PngAnimationViewModel("Repeated image test",Path.GetDirectoryName(packPath)!,repeatSequence,1,0,0,false,_ => {repeatSaved=true; return (true,"Saved");})) {
						if(!repeatEditor.Save() || !repeatSaved || !repeatEditor.MemoryInfo.Contains("4.0 MiB")) { throw new Exception("Editor rejected reusable frame content or miscounted unique paths: "+repeatEditor.Status+" / "+repeatEditor.MemoryInfo); }
					}
					Console.WriteLine("PASS: repeated paths count once in editor estimates; an unshared sum over 512 MiB does not incorrectly reject a reusable sequence.");

					return;
				}
				viewer.SelectedMetaGroup!.Name = "Standing";
				viewer.SetSelectedMetaOrigin(2, 3);
				if(!viewer.PreviewUsesReplacement || viewer.MetaPreviewOriginX != 2 || viewer.MetaPreviewOriginY != 3) {
					throw new Exception("Artwork origin editing did not update the preview crosshair.");
				}
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(saved.Groups[0].Name != "Standing" || saved.Groups[0].ReplacementOriginX != 2 || saved.Groups[0].ReplacementOriginY != 3) {
					throw new Exception("Named group/artwork origin changes did not persist.");
				}
				viewer.CreateMetaGroup(true);
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(saved.Groups.Count != 2 || saved.NextGroupId != 3 || saved.Groups[0].PoseFamilyId != 1 || saved.Groups[1].PoseFamilyId != 1
					|| saved.Groups[1].Id != 2 || !saved.Groups[1].Enabled || saved.Groups[1].Name != "Standing"
					|| saved.Groups[1].OriginX != saved.Groups[0].OriginX || saved.Groups[1].OriginY != saved.Groups[0].OriginY
					|| !File.Exists(Path.Combine(Path.GetDirectoryName(packPath)!, saved.Groups[1].ReplacementPath))) {
					throw new Exception("Actual Create-pose flow did not preserve family/origin or automatically save enabled artwork.");
				}
				viewer.LoadMetaGroups();
				if(viewer.MetaGroups.Count != 2 || viewer.MetaGroups[0].Id != 1 || viewer.MetaGroups[1].Id != 2) { throw new Exception("Actual viewer reload lost the created groups."); }
				Console.WriteLine($"PASS: actual {system} snapshot -> selection -> Create -> default PNG/JSON save -> native activation -> viewer reload; no visible window.");
				Console.WriteLine($"PASS: {system} named group, artwork origin/crosshair and enabled pose family survive actual authoring and reload.");
				viewer.SelectedMetaGroup = viewer.MetaGroups[0];
				viewer.MetaGroups[1].ReplacementOriginX = -15; viewer.MetaGroups[1].ReplacementOriginY = -20;
				string beforeFailedAlignment = File.ReadAllText(packPath);
				Directory.CreateDirectory(packPath + ".tmp");
				try { viewer.AlignMetaPoseArtworkOrigins(); }
				finally { Directory.Delete(packPath + ".tmp"); }
				if(viewer.MetaGroups[1].ReplacementOriginX != -15 || viewer.MetaGroups[1].ReplacementOriginY != -20
					|| File.ReadAllText(packPath) != beforeFailedAlignment) { throw new Exception("Failed PNG pose alignment did not restore prior anchors/definitions."); }
				viewer.AlignMetaPoseArtworkOrigins();
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(saved.Groups[1].ReplacementOriginX != 2 || saved.Groups[1].ReplacementOriginY != 3
					|| saved.Groups[1].OriginX != viewer.MetaGroups[1].OriginX || saved.Groups[1].OriginY != viewer.MetaGroups[1].OriginY) {
					throw new Exception("PNG pose alignment failed to persist aligned artwork anchors while preserving native anchors.");
				}
				string alternateSource = Path.Combine(Path.GetDirectoryName(packPath)!, viewer.SelectedMetaGroup.ReplacementPath);
				var importGroup = viewer.SelectedMetaGroup;
				var previousPreview = viewer.MetaPreview;
				string beforeFailedImport = File.ReadAllText(packPath);
				string previousArtwork = importGroup.ReplacementPath;
				int replacementFiles = Directory.GetFiles(Path.GetDirectoryName(packPath)!, "meta-*-replacement-*.png").Length;
				viewer.MetaMemoryConditionsText = "invalid condition";
				viewer.ImportMetaReplacementPng(alternateSource);
				if(importGroup.ReplacementPath != previousArtwork || !ReferenceEquals(previousPreview, viewer.MetaPreview)
					|| File.ReadAllText(packPath) != beforeFailedImport || !File.Exists(alternateSource)
					|| Directory.GetFiles(Path.GetDirectoryName(packPath)!, "meta-*-replacement-*.png").Length != replacementFiles) {
					throw new Exception("Replacement save failure changed existing artwork/preview or left an orphan PNG.");
				}
				viewer.MetaMemoryConditionsText = "";
				viewer.ImportMetaReplacementPng(alternateSource);
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(importGroup.ReplacementPath == previousArtwork || saved.Groups[0].ReplacementPath != importGroup.ReplacementPath
					|| !viewer.PreviewUsesReplacement || ReferenceEquals(previousPreview, viewer.MetaPreview)
					|| !File.Exists(alternateSource) || saved.Groups[0].ReplacementOriginX != 2 || saved.Groups[0].ReplacementOriginY != 3) {
					throw new Exception("Replacement import did not save/update preview while preserving artwork origin and source PNG.");
				}
				Console.WriteLine($"PASS: {system} replacement PNG import persists artwork/origin; failed saves preserve prior preview and clean up unsaved copies.");
				viewer.MetaAlternateConditionsText = "$0 <= $03 / $0F\n$1 >= $07";
				viewer.AddMetaAlternatePng(alternateSource);
				if(viewer.MetaAlternates.Count != 1 || viewer.SelectedMetaAlternate == null || !viewer.MetaStatus.StartsWith("Replacements applied.")) {
					throw new Exception("Actual conditional alternate PNG import failed: " + viewer.MetaStatus);
				}
				viewer.SetSelectedMetaAlternateOrigin(4, 5);
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				var authoredAlternate = saved.Groups[0].AlternateArtwork;
				if(authoredAlternate.Count != 1 || authoredAlternate[0].OriginX != 4 || authoredAlternate[0].OriginY != 5
					|| authoredAlternate[0].MemoryConditions.Count != 2 || authoredAlternate[0].MemoryConditions[0].Mask != 15
					|| authoredAlternate[0].MemoryConditions[0].Comparison != MetaMemoryComparison.LessOrEqual || authoredAlternate[0].MemoryConditions[1].Comparison != MetaMemoryComparison.GreaterOrEqual
					|| authoredAlternate[0].MemoryConditions[1].Address != 1) { throw new Exception("Actual alternate conditions/origin did not persist."); }
				string beforeFailedAlternate = File.ReadAllText(packPath);
				int alternateFiles = Directory.GetFiles(Path.GetDirectoryName(packPath)!, "meta-*-alternate-*.png").Length;
				viewer.MetaMemoryConditionsText = "invalid condition";
				viewer.AddMetaAlternatePng(alternateSource);
				if(viewer.MetaAlternates.Count != 1 || File.ReadAllText(packPath) != beforeFailedAlternate
					|| Directory.GetFiles(Path.GetDirectoryName(packPath)!, "meta-*-alternate-*.png").Length != alternateFiles) {
					throw new Exception("Alternate save failure left an orphan PNG or changed prior definitions.");
				}
				viewer.MetaMemoryConditionsText = "";
				Console.WriteLine($"PASS: {system} alternate PNG authoring with two masked CPU-byte conditions and independent origin; failed saves preserve existing assets/definitions.");
				using(var tilemap = new TilemapViewerViewModel(cpu, new PictureViewer(), new ScrollPictureViewer(), new Window())) {
					tilemap.OnGameLoaded();
					Dispatcher.UIThread.RunJobs();
					string exported = Path.Combine(home, "tilemap.png");
					using(var snapshot = tilemap.CaptureBackgroundPngSnapshot()) { snapshot.Save(exported, PngBitmapEncoderOptions.Default); }
					byte[] transparentExport = File.ReadAllBytes(exported);
					foreach(var backdrop in new[] { TilemapBackground.Black, TilemapBackground.White, TilemapBackground.Magenta }) {
						tilemap.Config.Background = backdrop;
						using var snapshot = tilemap.CaptureBackgroundPngSnapshot();
						snapshot.Save(exported, PngBitmapEncoderOptions.Default);
						if(!transparentExport.SequenceEqual(File.ReadAllBytes(exported))) { throw new Exception("Tilemap export depends on the viewer backdrop."); }
					}
					tilemap.SelectionRect = new Rect(0, 0, 8, 8);
					tilemap.ExportBackgroundPng(new Window()).GetAwaiter().GetResult();
					tilemap.ExportBackgroundPng(new Window()).GetAwaiter().GetResult();
					string exportStem = $"tilemap-{cpu}-layer-{tilemap.SelectedTab.Layer:D2}";
					foreach(string suffix in new[] { "", "-002" }) {
						string exportedTilemapPath = Path.Combine(Path.GetDirectoryName(packPath)!, exportStem + suffix + ".png");
						if(!File.Exists(exportedTilemapPath) || !transparentExport.SequenceEqual(File.ReadAllBytes(exportedTilemapPath))) { throw new Exception("Default tilemap export naming, transparency or full canvas failed: " + tilemap.BackgroundEditor.MetaStatus); }
					}
					tilemap.BackgroundEditor.ImportMetaBackgroundPng(exported, 0);
					var background = tilemap.BackgroundEditor.SelectedMetaBackground ?? throw new Exception("Actual tilemap PNG import failed: " + tilemap.BackgroundEditor.MetaStatus);
					if(background.ArtworkPath != "tilemap.png") { throw new Exception("Background import did not preserve the source filename."); }
					if(!background.UseWidescreenPosition || background.WidescreenPosition.Position != 0) { throw new Exception("New PNG background did not receive its own centred gameplay positioning defaults."); }
					string beforeUngatedBackground = File.ReadAllText(packPath);
					background.Enabled = true;
					tilemap.BackgroundEditor.SaveMetaGroups();
					if(File.ReadAllText(packPath) != beforeUngatedBackground || !tilemap.BackgroundEditor.MetaStatus.Contains("requires at least one CPU memory condition")) {
						throw new Exception("Background editor allowed gameplay activation without a configured CPU check.");
					}
					background.X = -7; background.Y = 11; background.FollowScroll = true; background.WrapX = true; background.Enabled = true;
					background.ScrollSpeedX = 0.5; background.ScrollSpeedY = -1;
					background.IgnoreRasterScroll = cpu == CpuType.Nes; background.ScrollReferenceScanline = 47;
					background.HideOriginalLayer = true;
					background.ExtendIntoMargins = true;
					background.UseWidescreenPosition = true;
					background.WidescreenPosition = new() { Position = 3, CameraMin = -10, CameraMax = 2048, SlideSpeed = 0.1, ReleaseDelay = 6, UseRamFlags = true, LeftFlags = "$68 == $01", RightFlags = "$68 == $02" };
					background.WidescreenPosition.RamSpeed = new() { Enabled = true, Format = 0, Address = 0x64, Multiplier = 0.0625, Maximum = 2.5 };
					background.UseCpuCamera = true;
					background.CameraX.Format = 4; background.CameraX.AddressHex = "0060"; background.CameraX.PageAddressHex = "0061"; background.CameraX.PageSize = 256;
					background.CameraY.Format = 0;
					background.CameraX.SignedPage = cpu == CpuType.Nes;
					{
						tilemap.BackgroundEditor.RefreshCameraReadout();
						if(!tilemap.BackgroundEditor.CameraReadout.Contains("Camera X:")) { throw new Exception("CPU camera live readout did not decode selected addresses."); }
					}
					background.MemoryConditions = new() { new() { Address = 0, Value = 0, Mask = 15, Comparison = MetaMemoryComparison.GreaterOrEqual } };
					tilemap.BackgroundEditor.MemoryConditionsText = "invalid background condition";
					tilemap.BackgroundEditor.SelectedMetaBackground = null;
					tilemap.BackgroundEditor.SelectedMetaBackground = background;
					if(tilemap.BackgroundEditor.MemoryConditionsText != "invalid background condition") { throw new Exception("Switching backgrounds discarded invalid condition text."); }
					tilemap.BackgroundEditor.SaveMetaGroups();
					if(File.ReadAllText(packPath) != beforeUngatedBackground) { throw new Exception("Invalid background condition text overwrote the pack."); }
					tilemap.BackgroundEditor.MemoryConditionsText = MetaMemoryConditionText.Format(background.MemoryConditions);
					background.DrawOrder = 7;
					background.CompositionPosition = 1;
					tilemap.BackgroundEditor.SaveMetaGroups();
					if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Actual background activation failed: " + tilemap.BackgroundEditor.MetaStatus); }
					if(MetaSpritePack.Load(packPath, EmuApi.GetRomHash(HashType.Sha1), cpu.ToString()).BackgroundLayers.Single(layer => layer.Id == background.Id).DrawOrder != 7) { throw new Exception("Background editor did not persist the PNG draw order."); }
					if(MetaSpritePack.Load(packPath, hash, cpu.ToString()).BackgroundLayers.Single(layer => layer.Id == background.Id).CompositionPosition != 1) { throw new Exception("Background editor did not persist foreground composition."); }
					background.CompositionPosition = 0; tilemap.BackgroundEditor.SaveMetaGroups();
					if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Restoring native PNG position failed."); }
					if(cpu == CpuType.Gba) {
						background.CompositionPosition = 2; background.GbaPriority = 3;
						tilemap.BackgroundEditor.SaveMetaGroups();
						if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Independent GBA PNG activation failed: " + tilemap.BackgroundEditor.MetaStatus); }
						var priorityLayer = MetaSpritePack.Load(packPath, hash, cpu.ToString()).BackgroundLayers.Single(layer => layer.Id == background.Id);
						if(priorityLayer.CompositionPosition != 2 || priorityLayer.GbaPriority != 3) { throw new Exception("Independent GBA priority did not persist."); }
						background.CompositionPosition = 0; tilemap.BackgroundEditor.SaveMetaGroups();
					}
					if(cpu == CpuType.Snes) {
						background.CompositionPosition = 3; background.SnesPriority = 12;
						tilemap.BackgroundEditor.SaveMetaGroups();
						if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Independent SNES PNG activation failed: " + tilemap.BackgroundEditor.MetaStatus); }
						var priorityLayer = MetaSpritePack.Load(packPath, hash, cpu.ToString()).BackgroundLayers.Single(layer => layer.Id == background.Id);
						if(priorityLayer.CompositionPosition != 3 || priorityLayer.SnesPriority != 12) { throw new Exception("Independent SNES priority did not persist."); }
						background.CompositionPosition = 0; tilemap.BackgroundEditor.SaveMetaGroups();
					}
					if(cpu == CpuType.Nes || cpu == CpuType.Gameboy || cpu == CpuType.Pce || cpu == CpuType.Sms || cpu == CpuType.Ws) {
						foreach(int position in new[] {4, 5, 6}) {
							background.CompositionPosition = position; tilemap.BackgroundEditor.SaveMetaGroups();
							if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.") || MetaSpritePack.Load(packPath, hash, cpu.ToString()).BackgroundLayers.Single(layer => layer.Id == background.Id).CompositionPosition != position) { throw new Exception("Sprite insertion band failed to save/apply/reload: " + position); }
						}
						background.CompositionPosition = 0; tilemap.BackgroundEditor.SaveMetaGroups();
						var gbaOnlyPack = new MetaSpritePack { System = "Gba", BackgroundLayers = new() { new() { Id = 1, Enabled = true, CompositionPosition = 2, ArtworkWidth = 1, ArtworkHeight = 1, MemoryConditions = new() {new() {Address = 0, Value = 0, Mask = 0}} } } };
						byte[] unsupported = MetaBackgroundRuntimePacket.Create(gbaOnlyPack, _ => (1, 1, new[] {0xFFFFFFFFu}));
						byte[] emptySprites = { (byte)'M', (byte)'S', (byte)'P', (byte)'1', 0, 0, 0, 0 };
						if(DebugApi.SetMetaBackgroundReplacements(hash, unsupported, (uint)unsupported.Length) || DebugApi.SetMetaReplacements(hash, emptySprites, (uint)emptySprites.Length, unsupported, (uint)unsupported.Length)) { throw new Exception("NES accepted a GBA-only priority pack."); }
						gbaOnlyPack.System = "Snes"; gbaOnlyPack.BackgroundLayers[0].CompositionPosition = 3;
						unsupported = MetaBackgroundRuntimePacket.Create(gbaOnlyPack, _ => (1, 1, new[] {0xFFFFFFFFu}));
						if(DebugApi.SetMetaBackgroundReplacements(hash, unsupported, (uint)unsupported.Length) || DebugApi.SetMetaReplacements(hash, emptySprites, (uint)emptySprites.Length, unsupported, (uint)unsupported.Length)) { throw new Exception("NES accepted a SNES-only priority pack."); }
					}
					if(cpu == CpuType.Gba) {
						var nesOnlyPack = new MetaSpritePack { System = "Nes", BackgroundLayers = new() { new() { Id = 1, Enabled = true, CompositionPosition = 4, ArtworkWidth = 1, ArtworkHeight = 1, MemoryConditions = new() {new() {Address = 0, Value = 0, Mask = 0}} } } };
						byte[] unsupported = MetaBackgroundRuntimePacket.Create(nesOnlyPack, _ => (1, 1, new[] {0xFFFFFFFFu}));
						byte[] emptySprites = { (byte)'M', (byte)'S', (byte)'P', (byte)'1', 0, 0, 0, 0 };
						if(DebugApi.SetMetaBackgroundReplacements(hash, unsupported, (uint)unsupported.Length) || DebugApi.SetMetaReplacements(hash, emptySprites, (uint)emptySprites.Length, unsupported, (uint)unsupported.Length)) { throw new Exception("GBA accepted NES-only insertion bands."); }
					}
					if(cpu == CpuType.Nes) {
						tilemap.BackgroundEditor.SaveCpuCamera();
						if(!tilemap.BackgroundEditor.HasSavedCamera) { throw new Exception("CPU camera profile was not saved."); }
						string beforeFailedCameraSave = File.ReadAllText(packPath);
						background.CameraX.AddressHex = "0062";
						Directory.CreateDirectory(packPath + ".tmp");
						try { tilemap.BackgroundEditor.SaveCpuCamera(); } finally { Directory.Delete(packPath + ".tmp"); }
						if(File.ReadAllText(packPath) != beforeFailedCameraSave || !tilemap.BackgroundEditor.MetaStatus.StartsWith("Could not save CPU camera:")) { throw new Exception("Failed CPU camera save changed the pack."); }
						tilemap.BackgroundEditor.LinkCpuCamera();
						var linked = MetaSpritePack.Load(packPath, hash, cpu.ToString());
						if(!background.UseSavedCamera || !linked.BackgroundLayers[0].UseSavedCamera || linked.SavedCamera?.X.Address != 0x60) { throw new Exception("Linking did not use the last successfully saved camera."); }
						tilemap.BackgroundEditor.UnlinkCpuCamera();
						if(background.UseSavedCamera || background.CameraX.Address != 0x60 || background.CameraX.PageAddress != 0x61 || !background.CameraX.SignedPage) { throw new Exception("Unlinking lost the saved camera settings."); }
					}
					string backgroundAssetPath = Path.Combine(Path.GetDirectoryName(packPath)!, background.ArtworkPath);
					byte[] originalBackgroundAsset = File.ReadAllBytes(backgroundAssetPath);
					string beforeChangedCanvas = File.ReadAllText(packPath);
					try {
						using var changedCanvas = new WriteableBitmap(new PixelSize(1, 1), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);
						changedCanvas.Save(backgroundAssetPath, PngBitmapEncoderOptions.Default);
						tilemap.BackgroundEditor.SaveMetaGroups();
						if(File.ReadAllText(packPath) != beforeChangedCanvas || !tilemap.BackgroundEditor.MetaStatus.Contains("dimensions changed")) { throw new Exception("Edited background PNG dimensions were committed before validation."); }
						tilemap.BackgroundEditor.ReplaceBackgroundPngFile(exported);
						if(background.ArtworkPath == Path.GetFileName(backgroundAssetPath) || background.X != -7 || background.Y != 11
							|| background.MemoryConditions.Count != 1 || !background.Enabled || !File.Exists(backgroundAssetPath)
							|| !tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Replacing the resized background failed to preserve placement/conditions or activate."); }
					} finally { File.WriteAllBytes(backgroundAssetPath, originalBackgroundAsset); }
					if(background.ArtworkPath != "tilemap-002.png") { throw new Exception("Background filename collision did not use a short numbered suffix."); }
					int beforeReuseCount = Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length;
					string reusedPath = background.ArtworkPath;
					tilemap.BackgroundEditor.ReplaceBackgroundPngFile(Path.Combine(Path.GetDirectoryName(packPath)!, reusedPath));
					if(background.ArtworkPath != reusedPath || Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length != beforeReuseCount
						|| !tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Background artwork already in the pack folder was copied or renamed."); }
					string beforeFailedBackgroundReplacement = File.ReadAllText(packPath);
					string retainedBackgroundPath = background.ArtworkPath;
					var retainedBackgroundPreview = tilemap.BackgroundEditor.MetaBackgroundPreview;
					int beforeReplacementPngCount = Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length;
					Directory.CreateDirectory(packPath + ".tmp");
					try { tilemap.BackgroundEditor.ReplaceBackgroundPngFile(exported); }
					finally { Directory.Delete(packPath + ".tmp"); }
					if(background.ArtworkPath != retainedBackgroundPath || File.ReadAllText(packPath) != beforeFailedBackgroundReplacement
						|| tilemap.BackgroundEditor.MetaBackgroundPreview != retainedBackgroundPreview
						|| Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length != beforeReplacementPngCount) {
						throw new Exception("Failed background replacement changed saved artwork/preview or left an unsaved PNG.");
					}
					viewer.LoadMetaGroups(); viewer.SaveMetaGroups();
					saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
					if(saved.Groups.Count != 2 || saved.BackgroundLayers.Count != 1 || saved.BackgroundLayers[0].X != -7 || saved.BackgroundLayers[0].Y != 11
						|| !saved.BackgroundLayers[0].Enabled || !saved.BackgroundLayers[0].FollowScroll || !saved.BackgroundLayers[0].WrapX
						|| saved.BackgroundLayers[0].ScrollSpeedX != 0.5 || saved.BackgroundLayers[0].ScrollSpeedY != -1
						|| saved.BackgroundLayers[0].IgnoreRasterScroll != (cpu == CpuType.Nes) || saved.BackgroundLayers[0].ScrollReferenceScanline != 47
						|| !saved.BackgroundLayers[0].HideOriginalLayer || !saved.BackgroundLayers[0].ExtendIntoMargins
						|| !saved.BackgroundLayers[0].UseWidescreenPosition || saved.BackgroundLayers[0].WidescreenPosition.CameraMin != -10 || saved.BackgroundLayers[0].WidescreenPosition.LeftFlags != "$68 == $01"
						|| saved.BackgroundLayers[0].WidescreenPosition.SlideSpeed != 0.1 || saved.BackgroundLayers[0].WidescreenPosition.ReleaseDelay != 6
						|| !saved.BackgroundLayers[0].WidescreenPosition.RamSpeed.Enabled || saved.BackgroundLayers[0].WidescreenPosition.RamSpeed.Address != 0x64
						|| saved.BackgroundLayers[0].WidescreenPosition.RamSpeed.Multiplier != 0.0625 || saved.BackgroundLayers[0].WidescreenPosition.RamSpeed.Maximum != 2.5
						|| !saved.BackgroundLayers[0].UseCpuCamera || saved.BackgroundLayers[0].CameraX.Format != 4
						|| saved.BackgroundLayers[0].CameraX.Address != 0x60 || saved.BackgroundLayers[0].CameraX.PageAddress != 0x61 || saved.BackgroundLayers[0].CameraX.PageSize != 256
						|| saved.BackgroundLayers[0].MemoryConditions.Count != 1 || saved.BackgroundLayers[0].MemoryConditions[0].Mask != 15) {
						throw new Exception("Tilemap layer placement/conditions or sprite groups were lost during shared-pack saving.");
					}
					tilemap.BackgroundEditor.Load();
					if(tilemap.BackgroundEditor.MetaBackgrounds.Count != 1) { throw new Exception("Tilemap editor reload lost the imported background."); }
					string assetDirectory = Path.GetDirectoryName(packPath)!;
					int assetCount = Directory.GetFiles(assetDirectory, "background-*.png").Length;
					string priorJson = File.ReadAllText(packPath);
					tilemap.BackgroundEditor.ImportMetaBackgroundPng(exported, -1);
					var afterFailure = MetaSpritePack.Load(packPath, hash, cpu.ToString());
					if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Could not import background:") || afterFailure.NextBackgroundId != 2
						|| tilemap.BackgroundEditor.MetaBackgrounds.Count != 1 || File.ReadAllText(packPath) != priorJson
						|| Directory.GetFiles(assetDirectory, "background-*.png").Length != assetCount || !File.Exists(exported)) {
						throw new Exception("Failed background import left an orphan PNG or changed existing definitions/source artwork.");
					}
					Console.WriteLine($"PASS: {system} actual tilemap snapshot PNG export/import, placement/scroll settings, CPU-byte condition, native activation and shared-pack preservation.");
					Console.WriteLine($"PASS: {system} background import save failure rolls back new PNG and ID while retaining prior JSON/source artwork.");
					using(var otherBackgroundEditor = new MetaBackgroundEditorViewModel(cpu)) {
						otherBackgroundEditor.Load();
						otherBackgroundEditor.SelectedMetaBackground = otherBackgroundEditor.MetaBackgrounds[0];
						otherBackgroundEditor.SelectedMetaBackground.X = -17;
						otherBackgroundEditor.SaveMetaGroups();
						if(!otherBackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Second background editor save failed."); }
						string latestBackgrounds = File.ReadAllText(packPath);
						tilemap.BackgroundEditor.SelectedMetaBackground = tilemap.BackgroundEditor.MetaBackgrounds[0];
						tilemap.BackgroundEditor.SaveMetaGroups();
						if(!tilemap.BackgroundEditor.MetaStatus.Contains("saved by another editor")) { throw new Exception("Stale background save was accepted."); }
						tilemap.BackgroundEditor.RemoveMetaBackground();
						tilemap.BackgroundEditor.ImportMetaBackgroundPng(exported, 0);
						if(File.ReadAllText(packPath) != latestBackgrounds || tilemap.BackgroundEditor.MetaBackgrounds.Count != 1
							|| Directory.GetFiles(assetDirectory, "background-*.png").Length != assetCount) {
							throw new Exception("Stale background save/remove/import overwrote newer layers or left unsaved artwork.");
						}
						tilemap.BackgroundEditor.Load();
						tilemap.BackgroundEditor.SelectedMetaBackground = tilemap.BackgroundEditor.MetaBackgrounds[0];
						if(tilemap.BackgroundEditor.SelectedMetaBackground.X != -17) { throw new Exception("Background reload lost newer placement."); }
						tilemap.BackgroundEditor.SelectedMetaBackground.X = -7;
						tilemap.BackgroundEditor.SaveMetaGroups();
						if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Background reload did not restore editing."); }
						Console.WriteLine($"PASS: {system} stale background save/remove/import preserve newer layers and assets; reload restores editing.");
					}
				}
				using(var otherViewer = new SpriteViewerViewModel(cpu, new PictureViewer(), new ScrollPictureViewer(), new Grid(), new Grid(), new Window())) {
					otherViewer.LoadMetaGroups();
					otherViewer.SelectedMetaGroup = otherViewer.MetaGroups[0];
					otherViewer.SelectedMetaGroup.Name = "Updated in second editor";
					otherViewer.SaveMetaGroups();
					if(!otherViewer.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Second editor could not save group changes: " + otherViewer.MetaStatus); }
					string latestDefinitions = File.ReadAllText(packPath);
					viewer.SaveMetaGroups();
					if(File.ReadAllText(packPath) != latestDefinitions || !viewer.MetaStatus.Contains("saved by another editor")) {
						throw new Exception("Stale Sprite Viewer overwrote newer group definitions.");
					}
					viewer.LoadMetaGroups(); viewer.SelectedMetaGroup = viewer.MetaGroups[0];
					if(viewer.SelectedMetaGroup.Name != "Updated in second editor") { throw new Exception("Reload did not retrieve the second editor's changes."); }
					viewer.SelectedMetaGroup.Name = "Standing"; viewer.SaveMetaGroups();
					if(!viewer.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Reload did not restore group editing."); }
					Console.WriteLine($"PASS: {system} stale Sprite Viewer saves preserve newer definitions; reload restores editing.");
				}
				if(recognitionBenchmark) {
					MethodInfo refresh = typeof(SpriteViewerViewModel).GetMethod("InternalRefreshTab", BindingFlags.NonPublic | BindingFlags.Instance)!;
					for(int i = 0; i < 30; i++) { refresh.Invoke(viewer, null); }
					string expectedMatches = viewer.MetaMatchStatus;
					long allocated = GC.GetAllocatedBytesForCurrentThread();
					var timer = Stopwatch.StartNew();
					const int iterations = 240;
					for(int i = 0; i < iterations; i++) {
						refresh.Invoke(viewer, null);
						if(viewer.MetaMatchStatus != expectedMatches) { throw new Exception("Repeated snapshot refresh changed recognition results."); }
					}
					double elapsedMs = timer.Elapsed.TotalMilliseconds;
					long bytes = GC.GetAllocatedBytesForCurrentThread() - allocated;
					Console.WriteLine($"RECOGNITION BENCHMARK: {system}, {elapsedMs / iterations:F4} ms/refresh, {bytes / iterations} bytes/refresh; actual paused viewer snapshot, excludes desktop presentation.");
				}
				Type loader = typeof(SpriteViewerViewModel).Assembly.GetType("Mesen.Debugger.Utilities.MetaSpriteRuntimeLoader")!;
				MethodInfo preload = loader.GetMethod("LoadForGame", BindingFlags.Public | BindingFlags.Static)!;
				int loads = 0;
				bool applied = false;
				uint before = uint.MaxValue, after = uint.MaxValue;
				ulong beforeClock = ulong.MaxValue, afterClock = ulong.MaxValue;
				double preloadMs = 0;
				using var listener = new NotificationListener();
				listener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
					loads++;
					TimingInfo beforeTiming = EmuApi.GetTimingInfo(cpu);
					before = beforeTiming.FrameCount; beforeClock = beforeTiming.MasterClock;
					var timer = Stopwatch.StartNew();
					applied = (bool)preload.Invoke(null, new object[] { hash, cpu })!;
					preloadMs = timer.Elapsed.TotalMilliseconds;
					TimingInfo afterTiming = EmuApi.GetTimingInfo(cpu);
					after = afterTiming.FrameCount; afterClock = afterTiming.MasterClock;
				};
				if(!EmuApi.LoadRom(romPath) || loads != 1 || !applied || before != after || beforeClock != afterClock) {
					throw new Exception($"Pre-emulation PNG activation failed: loads={loads}, applied={applied}, frames={before}/{after}, clocks={beforeClock}/{afterClock}.");
				}
				Console.WriteLine($"PASS: {system} saved PNG decode/native activation completed during GameLoaded with frame/clock unchanged ({before}/{beforeClock}); {preloadMs:F3} ms.");
				Console.WriteLine($"CLEANUP: {system} activation check returned; finishing viewer scope.");
				if(startupTiming) {
					const int viewerId = 19071;
					listener.OnNotification += e => {
						if(e.NotificationType == ConsoleNotificationType.ViewerRefresh && e.Parameter.ToInt32() == viewerId) { viewer.RefreshData(); }
					};
					DebugApi.SetViewerUpdateTiming(viewerId, 8, 12, cpu);
					EmuApi.Resume();
					var timer = Stopwatch.StartNew();
					uint previousFrame = EmuApi.GetTimingInfo(cpu).FrameCount;
					double previousSeconds = 0;
					string report = "seconds,frames_per_second\n";
					while(timer.Elapsed.TotalSeconds < 60) {
						Dispatcher.UIThread.RunJobs();
						double seconds = timer.Elapsed.TotalSeconds;
						if(seconds - previousSeconds >= 5) {
							uint frame = EmuApi.GetTimingInfo(cpu).FrameCount;
							double fps = (frame - previousFrame) / (seconds - previousSeconds);
							Console.WriteLine($"STARTUP TIMING: {system}, {seconds:F1}s, {fps:F2} emulated FPS; actual viewer model refresh, excludes desktop presentation.");
							report += FormattableString.Invariant($"{seconds:F3},{fps:F3}\n");
							previousFrame = frame; previousSeconds = seconds;
						}
						System.Threading.Thread.Sleep(1);
					}
					DebugApi.RemoveViewerId(viewerId, cpu);
					EmuApi.Pause();
					File.WriteAllText(Path.Combine(home, "startup-timing.csv"), report);
					Console.WriteLine("Startup timing saved: " + Path.Combine(home, "startup-timing.csv"));
				}
			} finally {
				Console.WriteLine($"CLEANUP: {system} releasing debugger.");
				DebugApi.ReleaseDebugger();
				Console.WriteLine($"CLEANUP: {system} debugger released.");
			}
		} finally {
			Console.WriteLine($"CLEANUP: {system} stopping emulator.");
			EmuApi.Stop();
			Console.WriteLine($"CLEANUP: {system} releasing emulator.");
			EmuApi.Release();
			Console.WriteLine($"PASS: {system} viewer verification shutdown completed.");
		}
	}

	private static void CheckCreationSave()
	{
		MethodInfo save = typeof(SpriteViewerViewModel).GetMethod("SaveNewMetaGroup", BindingFlags.NonPublic | BindingFlags.Static)
			?? throw new Exception("Create-group save helper missing.");
		string directory = Path.Combine(AppContext.BaseDirectory, "artifacts", "creation-" + Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(directory);
		string packPath = Path.Combine(directory, "groups.json");
		var pack = new MetaSpritePack { RomHash = "creation-fixture", System = "Snes" };
		MetaSpriteGroup NewGroup() => new() {
			Id = pack.NextGroupId, Width = 2, Height = 1, OriginX = 1, OriginY = 1,
			Members = new() { new() { Width = 2, Height = 1, Pixels = new uint[] { 0, 0xFF123456 } } }
		};
		var group = NewGroup();
		save.Invoke(null, new object[] { pack, group, packPath, (Action)(() => pack.Save(packPath)) });
		var restored = MetaSpritePack.Load(packPath, pack.RomHash, pack.System);
		if(restored.Groups.Count != 1 || restored.NextGroupId != 2 || !restored.Groups[0].Enabled
			|| restored.Groups[0].ReplacementPath != "meta-001.png" || !File.Exists(Path.Combine(directory, group.ReplacementPath))) {
			throw new Exception("Creation did not automatically save and enable the group PNG.");
		}
		string existing = Path.Combine(directory, "meta-002.png");
		File.WriteAllText(existing, "existing user artwork");
		group = NewGroup();
		string originalPack = File.ReadAllText(packPath);
		bool rejected = false;
		try { save.Invoke(null, new object[] { pack, group, packPath, (Action)(() => throw new IOException("Simulated definition save failure.")) }); }
		catch(TargetInvocationException ex) when(ex.InnerException is IOException) { rejected = true; }
		if(!rejected || pack.Groups.Count != 1 || pack.NextGroupId != 2 || File.ReadAllText(packPath) != originalPack
			|| File.ReadAllText(existing) != "existing user artwork" || File.Exists(Path.Combine(directory, group.ReplacementPath))) {
			throw new Exception("Failed creation did not roll back its PNG and group while retaining existing files.");
		}
		group = NewGroup();
		save.Invoke(null, new object[] { pack, group, packPath, (Action)(() => pack.Save(packPath)) });
		if(!group.Enabled || group.ReplacementPath == "meta-002.png" || !File.Exists(Path.Combine(directory, group.ReplacementPath))
			|| File.ReadAllText(existing) != "existing user artwork" || pack.NextGroupId != 3) {
			throw new Exception("Creation retry overwrote artwork or failed to reuse the rolled-back ID.");
		}
		Console.WriteLine("PASS: actual Create save helper exports/assigns/enables PNG, preserves existing artwork, rolls back failed saves and retries with the same group ID.");
	}

	private static void CheckPreviewPixels(DynamicBitmap bitmap, uint[] expected, int count)
	{
		using var frame = bitmap.Lock(true);
		int[] actual = new int[count];
		Marshal.Copy(frame.FrameBuffer.Address, actual, 0, count);
		for(int i = 0; i < count; i++) {
			if(unchecked((uint)actual[i]) != expected[i]) { throw new Exception($"Preview bitmap pixel {i} did not update."); }
		}
	}

	private static void BenchmarkPreviews()
	{
		const int count = 128, iterations = 240;
		foreach(int size in new[] { 8, 64 }) {
			var models = new SpritePreviewModel[count];
			var sprites = new DebugSpriteInfo[count];
			var layout = new DebugSpritePreviewInfo();
			var pixels = new uint[count * 128 * 128];
			for(int i = 0; i < count; i++) {
				models[i] = new();
				sprites[i] = new() { SpriteIndex = (short)i, Width = (ushort)size, Height = (ushort)size };
				Array.Fill(pixels, 0xFF204080u, i * 128 * 128, size * size);
				models[i].Init(ref sprites[i], pixels, layout);
			}
			foreach(bool changing in new[] { false, true }) {
				for(int iteration = 0; iteration < 30; iteration++) {
					for(int i = 0; i < count; i++) { models[i].Init(ref sprites[i], pixels, layout); }
				}
				var timer = Stopwatch.StartNew();
				for(int iteration = 0; iteration < iterations; iteration++) {
					for(int i = 0; i < count; i++) {
						if(changing) { pixels[i * 128 * 128 + size * size - 1] ^= 0x000000FF; }
						models[i].Init(ref sprites[i], pixels, layout);
					}
				}
				timer.Stop();
				Console.WriteLine($"Preview model: {count} sprites {size}x{size}, {(changing ? "changing" : "unchanged")}: {timer.Elapsed.TotalMilliseconds / iterations:F4} ms/refresh.");
			}
			foreach(var model in models) { model.ReleasePreview(); }
		}
	}

	private static void CheckGroupPng()
	{
		var group = new MetaSpriteGroup { Width = 4, Height = 3 };
		group.Members.Add(new() { X = 0, Y = 1, Width = 2, Height = 1, Pixels = new uint[] { 0, 0xFFFF0000 } });
		group.Members.Add(new() { X = 3, Y = 2, Width = 1, Height = 1, Pixels = new uint[] { 0xFF0000FF } });
		group.Members.Add(new() { X = 2, Y = 0, Width = 1, Height = 1, Pixels = new uint[] { 0x80FFFF00 } });
		MethodInfo builder = typeof(SpriteViewerViewModel).GetMethod("BuildMetaBitmap", BindingFlags.NonPublic | BindingFlags.Static)
			?? throw new Exception("Viewer group bitmap builder missing.");
		using var bitmap = (WriteableBitmap)(builder.Invoke(null, new object[] { group }) ?? throw new Exception("Group bitmap missing."));
		string directory = Path.Combine(AppContext.BaseDirectory, "artifacts");
		Directory.CreateDirectory(directory);
		string path = Path.Combine(directory, "transparent-group.png");
		bitmap.Save(path, PngBitmapEncoderOptions.Default);
		using var loaded = new Bitmap(path);
		if(loaded.PixelSize != new PixelSize(4, 3)) { throw new Exception("PNG canvas changed."); }
		using var decoded = new WriteableBitmap(loaded.PixelSize, new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);
		using var frame = decoded.Lock();
		loaded.CopyPixels(frame);
		int[] row = new int[4];
		Type runtimeLoader = typeof(SpriteViewerViewModel).Assembly.GetType("Mesen.Debugger.Utilities.MetaSpriteRuntimeLoader")
			?? throw new Exception("Gameplay PNG loader missing.");
		MethodInfo loadPng = runtimeLoader.GetMethod("LoadPng", BindingFlags.NonPublic | BindingFlags.Static)
			?? throw new Exception("Gameplay PNG decoder missing.");
		MethodInfo validateBackgrounds = runtimeLoader.GetMethod("ValidateBackgroundSupport", BindingFlags.Public | BindingFlags.Static)!;
		foreach(CpuType cpu in new[] { CpuType.Nes, CpuType.Gameboy, CpuType.Gba, CpuType.Pce, CpuType.Sms, CpuType.Snes, CpuType.Ws }) {
			var draft = new MetaBackgroundLayer { Enabled = true, NativeLayer = 4 };
			var draftPack = new MetaSpritePack { System = cpu.ToString(), BackgroundLayers = new() { draft } };
			validateBackgrounds.Invoke(null, new object[] { draftPack });
			draft.MemoryConditions.Add(new() { Address = 0, Value = 0 });
			bool rejected = false;
			try { validateBackgrounds.Invoke(null, new object[] { draftPack }); }
			catch(TargetInvocationException ex) when(ex.InnerException is InvalidDataException) { rejected = true; }
			if(!rejected) { throw new Exception("Active unsupported background layer accepted for " + cpu); }
			draft.Enabled = false; validateBackgrounds.Invoke(null, new object[] { draftPack });
			draft.Enabled = true; draft.NativeLayer = 0; validateBackgrounds.Invoke(null, new object[] { draftPack });
		}
		Console.WriteLine("PASS: background support validation ignores inactive drafts but rejects active unsupported layers across all target systems.");
		var gameplay = ((int Width, int Height, uint[] Pixels))(loadPng.Invoke(null, new object[] { path, 1 })
			?? throw new Exception("Gameplay PNG pixels missing."));
		if(gameplay.Width != 4 || gameplay.Height != 3 || gameplay.Pixels.Length != 12) { throw new Exception("Gameplay PNG dimensions changed."); }
		for(int y = 0; y < 3; y++) {
			Marshal.Copy(frame.Address + y * frame.RowBytes, row, 0, 4);
			for(int x = 0; x < 4; x++) {
				uint expected = y == 0 && x == 2 ? 0x80FFFF00u : y == 1 && x == 1 ? 0xFFFF0000u : y == 2 && x == 3 ? 0xFF0000FFu : 0;
				uint display = unchecked((uint)row[x]);
				if((display >> 24) != (expected >> 24)) { throw new Exception($"PNG alpha mismatch at {x},{y}."); }
				for(int shift = 0; shift <= 16; shift += 8) {
					int difference = Math.Abs((int)((display >> shift) & 255) - (int)((expected >> shift) & 255));
					if(difference > ((expected >> 24) == 128 ? 1 : 0)) { throw new Exception($"PNG display color mismatch at {x},{y}."); }
				}
				if(gameplay.Pixels[y * 4 + x] != expected) { throw new Exception($"Gameplay PNG alpha/color mismatch at {x},{y}."); }
			}
		}
		Console.WriteLine("PASS: actual viewer export and shared gameplay PNG loader preserve transparent canvas, partial alpha and relative member placement: " + path);
		string corrupt = Path.Combine(directory, "corrupt.png");
		File.WriteAllBytes(corrupt, new byte[] { 137, 80, 78, 71 });
		var partialCanvas = ((int Width, int Height, uint[] Pixels))loadPng.Invoke(null, new object[] { path, 2 })!;
		if(partialCanvas.Width != 4 || partialCanvas.Height != 3 || !partialCanvas.Pixels.SequenceEqual(gameplay.Pixels)) { throw new Exception("Partial native canvas dropped PNG pixels or changed scale."); }
		foreach(var invalid in new[] { (Path: corrupt, Scale: 1), (Path: path, Scale: 0), (Path: path, Scale: 11) }) {
			bool rejected = false;
			try { loadPng.Invoke(null, new object[] { invalid.Path, invalid.Scale }); }
			catch(TargetInvocationException ex) when(ex.InnerException is InvalidDataException) { rejected = true; }
			if(!rejected) { throw new Exception("Invalid PNG or canvas scale was accepted."); }
		}
		Console.WriteLine("PASS: gameplay PNG loader preserves partial native canvases and rejects corrupt data/out-of-range scale.");
		uint[] rampPixels = new uint[256];
		for(uint alpha = 1; alpha < 256; alpha++) { rampPixels[alpha] = (alpha << 24) | 0x00E741A5; }
		var ramp = new MetaSpriteGroup { Width = 256, Height = 1 };
		ramp.Members.Add(new() { Width = 256, Height = 1, Pixels = rampPixels });
		using var rampBitmap = (WriteableBitmap)builder.Invoke(null, new object[] { ramp })!;
		string rampPath = Path.Combine(directory, "alpha-ramp.png");
		rampBitmap.Save(rampPath, PngBitmapEncoderOptions.Default);
		var rampImage = ((int Width, int Height, uint[] Pixels))loadPng.Invoke(null, new object[] { rampPath, 1 })!;
		for(int alpha = 0; alpha < 256; alpha++) {
			if(rampImage.Pixels[alpha] != rampPixels[alpha]) { throw new Exception("Gameplay PNG color/alpha changed at alpha " + alpha); }
		}
		Console.WriteLine("PASS: exact gameplay RGB/alpha round trip across all 256 alpha values.");
	}

	private static void CheckReleased(DynamicBitmap bitmap)
	{
		try { using var bitmapLock = bitmap.Lock(); }
		catch(ObjectDisposedException) { return; }
		throw new Exception("Obsolete bitmap remained usable after release.");
	}
}

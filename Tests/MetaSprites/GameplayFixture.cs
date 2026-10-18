using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

internal static class GameplayFixture
{
	[DllImport("MesenCore.dll")]
	private static extern uint RunAnimationGameplayTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	public static void RunAnimationGameplaySuite(string dllPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(),(name,_,_) => name=="MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		foreach(var fixture in new[] {("background-scroll.nes","NES mapper 0"),("gb","DMG"),("gbc","CGB"),("background-scroll.gba","GBA BG/OBJ"),("background.pce","PCE single VDC"),("sms","SMS mode 4"),("legacy-mode0.sms","SMS mode 0"),("background-sub-bg0-mode1-scroll.sfc","SNES mode 1 BG0/OBJ"),("background-scroll.wsc","WS BG/OBJ")}) {
			string path=Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture."+fixture.Item1);
			if(!File.Exists(path)) { throw new Exception("Missing generated animation fixture: "+path); }
			uint flags=RunAnimationGameplayTest(path);
			if(flags!=131071) { throw new Exception($"Live animation failed for {fixture.Item2}: {flags:X}/1FFFF."); }
			Console.WriteLine($"PASS: {fixture.Item2} live animation/RAM gates/pause/2x/run-ahead/save-load transitions, 110 exact whole-frame reverse rewind matches and native restoration.");
		}
	}
	[DllImport("MesenCore.dll")]
	private static extern uint RunMetaAnimationStateRomTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename,byte[] sprites,uint spriteLength,byte[] backgrounds,uint backgroundLength);
	public static void RunAnimationStateSuite(string dllPath,string packetPath,bool shared=false)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(),(name,_,_) => name=="MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		string sharedSuffix=shared ? ".shared" : "";
		byte[] sprites=File.ReadAllBytes(packetPath+".activated"+sharedSuffix), backgrounds=File.ReadAllBytes(packetPath+".background.activated"+sharedSuffix);
		foreach(string extension in new[] {"nes","gb","gbc","gba","pce","sms","sfc","wsc"}) {
			string filename=Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture."+extension);
			if(!File.Exists(filename)) { throw new Exception("Missing generated animation state fixture: "+filename); }
			uint result=RunMetaAnimationStateRomTest(filename,sprites,(uint)sprites.Length,backgrounds,(uint)backgrounds.Length);
			if(result!=0) { throw new Exception($"Animation state round-trip failed for {extension} at stage {result}."); }
			Console.WriteLine($"PASS: {extension} {(shared ? "shared-format" : "legacy-format")} animation clocks through actual emulator save/load and full/XOR rewind serialization.");
		}
	}
	[DllImport("MesenCore.dll")]
	private static extern uint RunForegroundOverlayGameplayTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	public static void RunForegroundSuite(string dllPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		foreach(string suffix in new[] { "background-scroll.nes", "gb", "gbc", "background-scroll.gba", "background.pce", "sms", "legacy-mode0.sms", "background-sub-bg0-mode1-scroll.sfc", "background-scroll.wsc" }) {
			string filename = Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture." + suffix);
			if(!File.Exists(filename)) { throw new Exception("Missing generated fixture: " + filename); }
			uint flags = RunForegroundOverlayGameplayTest(filename);
			if(flags != 255) { throw new Exception($"Live foreground overlay failed for {suffix}: flags {flags:X2}/FF."); }
			Console.WriteLine($"PASS: live foreground overlay, native/replacement OBJ, partial/transparent alpha, configured byte, condition/disable restoration: {suffix}.");
		}
	}
	[DllImport("MesenCore.dll")]
	[return: MarshalAs(UnmanagedType.I1)]
	private static extern bool RunMetaViewerRefreshTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	public static void RunViewerRefreshSuite(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		foreach(string extension in new[] { ".gb", ".gbc", ".gba", ".pce", ".sms", ".nes", ".sfc", ".wsc" }) {
			string filename = Path.ChangeExtension(romPath, extension);
			if(!File.Exists(filename)) { throw new Exception("Generate representative ROMs with --gameplay before running --viewer-refresh."); }
			if(!RunMetaViewerRefreshTest(filename)) { throw new Exception("Viewer refresh integration failed: " + filename); }
			Console.WriteLine("PASS: live debugger/viewer schedule delivery and removal: " + extension);
		}
	}
	[DllImport("MesenCore.dll")]
	private static extern uint RunMetaPackActivationTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename, byte[] sprites, uint spriteLength, byte[] backgrounds, uint backgroundLength);
	public static void VerifyPackActivation(string romPath, string packetPath)
	{
		byte[] sprites = File.ReadAllBytes(packetPath), backgrounds = File.ReadAllBytes(packetPath + ".background");
		uint flags = RunMetaPackActivationTest(romPath, sprites, (uint)sprites.Length, backgrounds, (uint)backgrounds.Length);
		if(flags != 63) { throw new Exception($"Combined pack activation failed: flags {flags}."); }
		Console.WriteLine("PASS: combined interop activation, retention after every truncated background packet and wrong ROM, and clearing both sets.");
	}
	[DllImport("MesenCore.dll")]
	private static extern uint RunSmsLegacyTextBackgroundTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")]
	private static extern uint RunMetaSpriteGameplayTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")]
	private static extern uint RunMetaSpritePceGameplayTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")]
	private static extern uint RunMetaSpriteGbaCaptureTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")]
	private static extern uint RunMetaSpriteWsCaptureTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")]
	private static extern uint RunMetaSpriteNesCaptureTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll")]
	private static extern uint RunMetaSpriteSnesCaptureTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll", CallingConvention = CallingConvention.StdCall)]
	private static extern uint RunMetaSpriteSnesHiresCaptureTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);
	[DllImport("MesenCore.dll", CallingConvention = CallingConvention.StdCall)]
	private static extern uint RunMetaSpriteSnesHiresMathTest([MarshalAs(UnmanagedType.LPUTF8Str)] string filename);

	public static void Run(string dllPath, string romPath, bool gameboyOnly = false)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		foreach(int variant in new[] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }) {
		bool color = variant > 0 && variant != 6 && variant != 8, banked = variant == 2;
		bool window = variant == 4 || variant >= 7 && variant <= 10, scrolled = variant >= 3 && variant <= 5 || variant >= 7 && variant <= 10, holes = variant == 5 || variant == 6;
		string gbPath = variant == 6 ? Path.ChangeExtension(romPath, "holes.gb") : variant == 5 ? Path.ChangeExtension(romPath, "holes-scroll.gbc") : window ? Path.ChangeExtension(romPath, "window-scroll.gbc") : scrolled ? Path.ChangeExtension(romPath, "scroll.gbc") : banked ? Path.ChangeExtension(romPath, "banked.gbc") : color ? Path.ChangeExtension(romPath, ".gbc") : romPath;
		byte[] rom = new byte[32768];
		if(variant == 7) { gbPath = Path.ChangeExtension(romPath, "window-underlay-scroll.gbc"); }
		if(variant == 8) { gbPath = Path.ChangeExtension(romPath, "window-scroll.gb"); }
		if(variant == 9) { gbPath = Path.ChangeExtension(romPath, "window-object-underlay-scroll.gbc"); }
		if(variant == 10) { gbPath = Path.ChangeExtension(romPath, "window-object-behind-underlay-scroll.gbc"); }
		if(variant == 11) { gbPath = Path.ChangeExtension(romPath, "native-overlap.gbc"); }
		rom[0x143] = color ? (byte)0xC0 : (byte)0;
		rom[0x100] = 0xC3; rom[0x101] = 0x50; rom[0x102] = 0x01;
		// Original test program: turn off LCD, clear OAM, draw one opaque 8x8 object, then loop.
		List<byte> code = new() { 0xF3, 0x31, 0xFE, 0xFF, 0xAF, 0xE0, 0x40, 0x21, 0x00, 0xFE, 0x06, 0xA0, 0x22, 0x05, 0x20, 0xFC };
		// Clear the background tile map and patterns for a deterministic backdrop.
		code.AddRange(new byte[] { 0x21, 0x00, 0x80, 0x01, 0x00, 0x20, 0xAF, 0x22, 0x0B, 0x78, 0xB1, 0x20, 0xF9 });
		if(banked) {
			// Bank zero's tile stays empty; only bank one contains the matching graphic.
			code.AddRange(new byte[] { 0x3E, 1, 0xE0, 0x4F, 0x21, 0x00, 0x80, 0x01, 0x00, 0x20, 0xAF, 0x22, 0x0B, 0x78, 0xB1, 0x20, 0xF9 });
		}
		for(int y = 0; y < 8; y++) {
			code.AddRange(new byte[] { 0x3E, holes ? (byte)0xFE : (byte)0xFF, 0xEA, (byte)(0x10 + y * 2), 0x80 });
		}
		if(banked) { code.AddRange(new byte[] { 0xAF, 0xE0, 0x4F, 0x3E, 0x0B, 0xEA, 0x03, 0xFE }); }
		// OAM: Y=24 (screen 8), X=16 (screen 8), pattern 1, palette 0.
		code.AddRange(new byte[] { 0x3E, 24, 0xEA, 0x00, 0xFE, 0x3E, 16, 0xEA, 0x01, 0xFE, 0x3E, 1, 0xEA, 0x02, 0xFE });
		if(variant == 9 || variant == 10) { code.AddRange(new byte[] { 0x3E, 46, 0xEA, 0, 0xFE, 0x3E, 58, 0xEA, 1, 0xFE }); }
		if(variant == 10) { code.AddRange(new byte[] { 0x3E, 0x80, 0xEA, 3, 0xFE }); }
		if(color) {
			// GBC palette RAM: blue backdrop (RGB555 0x7C00), red OBJ color 1 (0x001F).
			code.AddRange(new byte[] { 0x3E, 0x80, 0xE0, 0x68, 0xAF, 0xE0, 0x69, 0x3E, 0x7C, 0xE0, 0x69,
				0x3E, banked ? (byte)0x9A : (byte)0x82, 0xE0, 0x6A, 0x3E, 0x1F, 0xE0, 0x6B, 0xAF, 0xE0, 0x6B });
		}
		if(variant == 11) {
			// OAM 1 shares OAM 0's position/pattern; OBJ palette 1 makes it yellow.
			code.AddRange(new byte[] { 0x3E, 24, 0xEA, 4, 0xFE, 0x3E, 16, 0xEA, 5, 0xFE,
				0x3E, 1, 0xEA, 6, 0xFE, 0xEA, 7, 0xFE, 0x3E, 0x8A, 0xE0, 0x6A,
				0x3E, 0xFF, 0xE0, 0x6B, 0x3E, 3, 0xE0, 0x6B });
		}
		if(variant == 7 || variant == 9 || variant == 10) {
			// BG palette 1 is yellow; the separate window map keeps palette 0 blue.
			code.AddRange(new byte[] { 0x3E, 0x88, 0xE0, 0x68, 0x3E, 0xFF, 0xE0, 0x69, 0x3E, 0x03, 0xE0, 0x69,
				0x3E, 1, 0xE0, 0x4F, 0x21, 0, 0x98, 0x01, 0, 4,
				0x3E, 1, 0x22, 0x0B, 0x78, 0xB1, 0x20, 0xF8, 0xAF, 0xE0, 0x4F });
		}
		if(scrolled) { code.AddRange(new byte[] { 0x3E, 253, 0xE0, 0x43, 0x3E, 250, 0xE0, 0x42 }); }
		if(window) { code.AddRange(new byte[] { 0x3E, 47, 0xE0, 0x4B, 0x3E, 24, 0xE0, 0x4A }); }
		code.AddRange(new byte[] { 0x3E, 0xE4, 0xE0, 0x47, 0xE0, 0x48, 0x3E, window ? (byte)0xF3 : (byte)0x93, 0xE0, 0x40, 0x18, 0xFE });
		code.CopyTo(rom, 0x150);
		byte checksum = 0;
		for(int i = 0x134; i <= 0x14C; i++) { checksum = unchecked((byte)(checksum - rom[i] - 1)); }
		rom[0x14D] = checksum;
		File.WriteAllBytes(gbPath, rom);
		uint result = RunMetaSpriteGameplayTest(gbPath);
		if(result != 31) { throw new Exception($"Game Boy gameplay integration failed: flags 0x{result:X}, expected 0x1F."); }
		Console.WriteLine($"PASS: {Path.GetFileName(gbPath)} (ROM-only mapper, {(color ? "GBC" : "DMG")}), live capture, HD replacement, transparent/partial-alpha backdrop, and disabling during emulation.");
		}
		if(gameboyOnly) { return; }
		RunSms(Path.ChangeExtension(romPath, ".sms"));
		RunSms(Path.ChangeExtension(romPath, "holes.sms"));
		RunSms(Path.ChangeExtension(romPath, "holes-zoom.sms"), true);
		RunSms(Path.ChangeExtension(romPath, "holes-sms1-zoom.sms"), true);
		RunSms(Path.ChangeExtension(romPath, "scroll.sms"), scroll: true);
		RunSms(Path.ChangeExtension(romPath, "locks-scroll.sms"), scroll: true, scrollLocks: true);
		RunSms(Path.ChangeExtension(romPath, "zoom.sms"), true);
		RunSms(Path.ChangeExtension(romPath, "sms1-zoom.sms"), true);
		RunSms(Path.ChangeExtension(romPath, "left.sms"), false, true);
		RunSms(Path.ChangeExtension(romPath, "mask.sms"), false, false, true);
		RunPce(Path.ChangeExtension(romPath, ".pce"));
		RunPce(Path.ChangeExtension(romPath, "wide.pce"), true);
		RunPce(Path.ChangeExtension(romPath, "divider3.pce"), false, 1);
		RunPce(Path.ChangeExtension(romPath, "divider2.pce"), false, 2);
		RunPce(Path.ChangeExtension(romPath, "mirror.pce"), false, 0, true);
		RunPce(Path.ChangeExtension(romPath, "background.pce"), background: true);
		RunPce(Path.ChangeExtension(romPath, "opaque-bg-front.pce"));
		RunPce(Path.ChangeExtension(romPath, "opaque-bg-behind.pce"));
		RunPce(Path.ChangeExtension(romPath, "png-background-behind.pce"), background: true);
		RunPce(Path.ChangeExtension(romPath, "holes.pce"));
		RunPce(Path.ChangeExtension(romPath, "holes-wide.pce"), true);
		RunPce(Path.ChangeExtension(romPath, "holes-divider3.pce"), clockMode: 1);
		RunPce(Path.ChangeExtension(romPath, "holes-divider2.pce"), clockMode: 2);
		RunGba(Path.ChangeExtension(romPath, ".gba"));
		RunGba(Path.ChangeExtension(romPath, "mirror.gba"), true);
		RunGba(Path.ChangeExtension(romPath, "bpp8.gba"), false, true);
		RunGba(Path.ChangeExtension(romPath, "dark.gba"), dark: true);
		RunGba(Path.ChangeExtension(romPath, "holes.gba"), holes: true);
		RunGba(Path.ChangeExtension(romPath, "holes-dark.gba"), dark: true, holes: true);
		RunGba(Path.ChangeExtension(romPath, "holes-window.gba"), holes: true, window: 1);
		RunGba(Path.ChangeExtension(romPath, "holes-window-middle.gba"), holes: true, window: 2);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "transparent.gba"), transparentSource: true);
		RunGba(Path.ChangeExtension(romPath, "background-scroll.gba"), background: 1);
		RunGba(Path.ChangeExtension(romPath, "background-wide.gba"), background: 2);
		RunGba(Path.ChangeExtension(romPath, "background-affine.gba"), background: 3);
		RunGba(Path.ChangeExtension(romPath, "background-affine-scaled.gba"), background: 3);
		RunGba(Path.ChangeExtension(romPath, "background-affine-rotated.gba"), background: 3);
		RunGba(Path.ChangeExtension(romPath, "object-affine-identity.gba"));
		RunGba(Path.ChangeExtension(romPath, "object-affine-identity-holes-window.gba"), holes: true, window: 1);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged.gba"));
		RunGba(Path.ChangeExtension(romPath, "background-bitmap4.gba"), background: 4);
		RunGba(Path.ChangeExtension(romPath, "background-bitmap4-page1.gba"), background: 4);
		RunGba(Path.ChangeExtension(romPath, "background-bitmap4-page1-live.gba"), background: 4);
		RunGba(Path.ChangeExtension(romPath, "background-bitmap3.gba"), background: 4);
		RunGba(Path.ChangeExtension(romPath, "background-bitmap5.gba"), background: 4);
		RunWs(Path.ChangeExtension(romPath, ".wsc"));
		RunWs(Path.ChangeExtension(romPath, "background.wsc"), background: true);
		RunWs(Path.ChangeExtension(romPath, "background-scroll.wsc"), background: true, scroll: true);
		RunWs(Path.ChangeExtension(romPath, "background-window.wsc"), background: true, window: true);
		RunWs(Path.ChangeExtension(romPath, "background-outside-window.wsc"), background: true, window: true, outsideWindow: true);
		RunWs(Path.ChangeExtension(romPath, "background-outside-window-occluded.wsc"), background: true, window: true, outsideWindow: true, occluded: true);
		RunWs(Path.ChangeExtension(romPath, "planar.wsc"), 0xC0);
		RunWs(Path.ChangeExtension(romPath, "holes-planar.wsc"), 0xC0);
		RunWs(Path.ChangeExtension(romPath, "packed.wsc"), 0xE0);
		RunWs(Path.ChangeExtension(romPath, "obj-window.wsc"), spriteWindow: true);
		RunWs(Path.ChangeExtension(romPath, "obj-outside-window.wsc"), spriteWindow: true, spriteOutside: true);
		RunWs(Path.ChangeExtension(romPath, "mono.ws"), 0);
		RunWs(Path.ChangeExtension(romPath, "mono-background-scroll.ws"), 0, background: true, scroll: true);
		RunWs(Path.ChangeExtension(romPath, "mono-background-window.ws"), 0, background: true, window: true);
		RunWs(Path.ChangeExtension(romPath, "mono-background-outside-window-occluded.ws"), 0, background: true, window: true, outsideWindow: true, occluded: true);
		RunWs(Path.ChangeExtension(romPath, "mono-obj-window.ws"), 0, spriteWindow: true);
		RunWs(Path.ChangeExtension(romPath, "mono-obj-outside-window.ws"), 0, spriteWindow: true, spriteOutside: true);
		RunNesFixtures(romPath);
		RunSnes(Path.ChangeExtension(romPath, ".sfc"));
		RunSnes(Path.ChangeExtension(romPath, "holes.sfc"), true);
		RunSnes(Path.ChangeExtension(romPath, "holes-window.sfc"), true, 1);
		RunSnes(Path.ChangeExtension(romPath, "holes-window-middle.sfc"), true, 2);
		RunSnes(Path.ChangeExtension(romPath, "holes-math.sfc"), true, math: 1);
		RunSnes(Path.ChangeExtension(romPath, "holes-math-exempt.sfc"), true, math: 2);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-overlap.sfc"), true, overlap: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-overlap-rotate.sfc"), true, overlap: true, rotate: true);
		RunSnes(Path.ChangeExtension(romPath, "transparent.sfc"), transparent: true);
		RunSnes(Path.ChangeExtension(romPath, "background-sub.sfc"), backgroundSub: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen-bg0-mode7.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "background-sub-bg0-mode7.sfc"), backgroundSub: true);
		foreach(string priority in new[] { "low", "high" }) {
			RunSnes(Path.ChangeExtension(romPath, $"holes-subscreen-bg1-mode7-ext-{priority}.sfc"), true, sub: true, backgroundLayer: 1);
			RunSnes(Path.ChangeExtension(romPath, $"background-sub-bg1-mode7-ext-{priority}.sfc"), backgroundSub: true, backgroundLayer: 1);
		}
		foreach(string transform in new[] { "scale", "turn" }) {
			RunSnes(Path.ChangeExtension(romPath, $"holes-subscreen-bg0-mode7-{transform}.sfc"), true, sub: true);
			RunSnes(Path.ChangeExtension(romPath, $"background-sub-bg0-mode7-{transform}.sfc"), backgroundSub: true);
		}
		RunSnes(Path.ChangeExtension(romPath, "background-sub-scroll.sfc"), backgroundSub: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen-scroll.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen-bgmask.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "background-sub-bgmask.sfc"), backgroundSub: true);
		for(int layer = 1; layer < 4; layer++) {
			RunSnes(Path.ChangeExtension(romPath, $"holes-subscreen-bg{layer}-scroll.sfc"), true, sub: true, backgroundLayer: layer);
			RunSnes(Path.ChangeExtension(romPath, $"background-sub-bg{layer}-scroll.sfc"), backgroundSub: true, backgroundLayer: layer);
		}
		RunSnesAdditionalBackgrounds(romPath);
	}

	public static void RunNesSuite(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunNesFixtures(romPath);
	}

	private static void RunNesFixtures(string romPath)
	{
		RunNes(Path.ChangeExtension(romPath, ".nes"));
		RunNes(Path.ChangeExtension(romPath, "background.nes"), true);
		RunNes(Path.ChangeExtension(romPath, "background-scroll.nes"), true, true);
		RunNes(Path.ChangeExtension(romPath, "background-scroll-mask.nes"), true, true, true);
		RunNes(Path.ChangeExtension(romPath, "background-scroll-crop.nes"), true, true);
		RunNes(Path.ChangeExtension(romPath, "background-scroll-holes.nes"), true, true);
		RunNes(Path.ChangeExtension(romPath, "background-scroll-transparent.nes"), true, true);
	}

	public static void RunSnesCanvas(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunSnes(Path.ChangeExtension(romPath, ".sfc"));
		RunSnes(Path.ChangeExtension(romPath, "holes.sfc"), true);
		RunSnes(Path.ChangeExtension(romPath, "holes-window.sfc"), true, 1);
		RunSnes(Path.ChangeExtension(romPath, "holes-window-middle.sfc"), true, 2);
		RunSnes(Path.ChangeExtension(romPath, "holes-math.sfc"), true, math: 1);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-overlap-rotate.sfc"), true, overlap: true, rotate: true);
		RunSnes(Path.ChangeExtension(romPath, "transparent.sfc"), transparent: true);
	}
	public static void RunSnesOrdering(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen-bg0-mode7.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "background-sub-bg0-mode7.sfc"), backgroundSub: true);
		foreach(string priority in new[] { "low", "high" }) {
			RunSnes(Path.ChangeExtension(romPath, $"holes-subscreen-bg1-mode7-ext-{priority}.sfc"), true, sub: true, backgroundLayer: 1);
			RunSnes(Path.ChangeExtension(romPath, $"background-sub-bg1-mode7-ext-{priority}.sfc"), backgroundSub: true, backgroundLayer: 1);
		}
		foreach(string transform in new[] { "scale", "turn" }) {
			RunSnes(Path.ChangeExtension(romPath, $"holes-subscreen-bg0-mode7-{transform}.sfc"), true, sub: true);
			RunSnes(Path.ChangeExtension(romPath, $"background-sub-bg0-mode7-{transform}.sfc"), backgroundSub: true);
		}
		RunSnes(Path.ChangeExtension(romPath, "holes-overlap.sfc"), true, overlap: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-overlap-rotate.sfc"), true, overlap: true, rotate: true);
		RunSnes(Path.ChangeExtension(romPath, "transparent.sfc"), transparent: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "background-sub.sfc"), backgroundSub: true);
		RunSnes(Path.ChangeExtension(romPath, "background-sub-scroll.sfc"), backgroundSub: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen-scroll.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen-bgmask.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "background-sub-bgmask.sfc"), backgroundSub: true);
		for(int layer = 1; layer < 4; layer++) {
			RunSnes(Path.ChangeExtension(romPath, $"holes-subscreen-bg{layer}-scroll.sfc"), true, sub: true, backgroundLayer: layer);
			RunSnes(Path.ChangeExtension(romPath, $"background-sub-bg{layer}-scroll.sfc"), backgroundSub: true, backgroundLayer: layer);
		}
		RunSnesAdditionalBackgrounds(romPath);
	}

	public static void RunSnesHiRes(string dllPath, string romPath, bool includeInterlace = true, bool includeObjInterlace = true, bool onlyObjInterlace = false)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		// Hires uses per-phase assertions; ordinary woven OBJ uses a physical
		// whole-frame oracle. OBJ-interlace probes retain distinct source-row colors.
		List<Exception> failures = new();
		foreach(string fixture in new[] {
			"mode5-hires", "mode6-hires", "pseudo-hires", "mode5-hires-both-screens", "mode5-hires-both-screens-window", "pseudo-hires-window",
			"mode5-hires-background-scroll", "mode6-hires-background-scroll", "pseudo-hires-background-scroll", "interlace-holes", "interlace-current-field-holes",
			"mode5-hires-math-both-screens", "mode6-hires-math-both-screens-window", "pseudo-hires-math",
			"mode5-hires-math-operand-both-screens", "mode6-hires-math-operand-both-screens-window", "pseudo-hires-math-operand-both-screens",
			"pseudo-hires-math-operand-both-screens-mixed-raster",
			"mode5-hires-math-interlace-both-screens", "mode6-hires-math-operand-interlace-both-screens-window", "pseudo-hires-math-operand-interlace-both-screens",
			"mode5-hires-math-interlace-current-field-both-screens", "mode6-hires-math-operand-interlace-bob-field-both-screens", "pseudo-hires-math-operand-interlace-bob-blend-field-both-screens",
			"interlace-bob-field-holes", "interlace-bob-blend-field-holes", "interlace-bob-field-first-picture-holes", "interlace-bob-blend-field-first-picture-holes",
			"obj-interlace-holes", "obj-interlace-current-field-holes", "obj-interlace-bob-field-holes", "obj-interlace-bob-blend-field-holes",
			"obj-interlace-math-holes", "obj-interlace-current-field-math-holes", "obj-interlace-bob-field-math-holes", "obj-interlace-bob-blend-field-math-holes",
			"obj-interlace-group-holes", "obj-interlace-hflip-holes", "obj-interlace-vflip-holes",
			"obj-interlace-rect-holes", "obj-interlace-rect-vflip-holes", "obj-interlace-window-holes", "obj-interlace-poses-holes", "obj-interlace-poses-conditions-holes", "obj-interlace-bg-holes", "obj-interlace-bg-fields-holes"
		}) {
			if(onlyObjInterlace && !fixture.StartsWith("obj-interlace")) { continue; }
			if(!includeInterlace && fixture.Contains("interlace")) { continue; }
			if(!includeObjInterlace && fixture.Contains("obj-interlace")) { continue; }
			try { RunSnes(Path.ChangeExtension(romPath, fixture + ".sfc"), holes: true, window: fixture.Contains("window") ? 1 : 0, math: fixture.Contains("math") ? 1 : 0, backgroundSub: fixture.Contains("hires-background")); }
			catch(Exception ex) { failures.Add(ex); Console.WriteLine($"FAIL: {ex.Message}"); }
		}
		if(failures.Count != 0) { throw new AggregateException("SNES hires/interlace capture probes failed.", failures); }
	}

	public static void RunSnesFrameTiming(string dllPath, string romPath, bool viewerCore = false)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunSnes(Path.ChangeExtension(romPath, viewerCore ? "holes-subscreen-frame-timing-viewer-core.sfc" : "holes-subscreen-frame-timing.sfc"), true, sub: true);
	}

	private static void RunSnesAdditionalBackgrounds(string romPath)
	{
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen-memory.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen-bgholes.sfc"), true, sub: true);
		RunSnes(Path.ChangeExtension(romPath, "holes-subscreen-bgholes-bgmask.sfc"), true, sub: true);
		for(int layer = 0; layer < 3; layer++) {
			RunSnes(Path.ChangeExtension(romPath, $"holes-subscreen-bg{layer}-bglow-scroll.sfc"), true, sub: true, backgroundLayer: layer);
		}
		for(int layer = 0; layer < 3; layer++) {
			RunSnes(Path.ChangeExtension(romPath, $"holes-subscreen-bg{layer}-mode1-scroll.sfc"), true, sub: true, backgroundLayer: layer);
			RunSnes(Path.ChangeExtension(romPath, $"background-sub-bg{layer}-mode1-scroll.sfc"), backgroundSub: true, backgroundLayer: layer);
		}
		for(int mode = 2; mode <= 4; mode++) {
			for(int layer = 0; layer < 2; layer++) {
				RunSnes(Path.ChangeExtension(romPath, $"holes-subscreen-bg{layer}-mode{mode}-scroll.sfc"), true, sub: true, backgroundLayer: layer);
				RunSnes(Path.ChangeExtension(romPath, $"background-sub-bg{layer}-mode{mode}-scroll.sfc"), backgroundSub: true, backgroundLayer: layer);
			}
		}
	}

	public static void RunWidescreenSuite(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunGba(Path.ChangeExtension(romPath, "panorama-background.gba"), background: 1);
		RunGba(Path.ChangeExtension(romPath, "panorama-background-affine.gba"), background: 3);
		RunGba(Path.ChangeExtension(romPath, "panorama-background-bitmap3.gba"), background: 4);
		RunSnes(Path.ChangeExtension(romPath, "panorama-background-sub.sfc"), backgroundSub: true);
		RunSnes(Path.ChangeExtension(romPath, "panorama-background-sub-mode1.sfc"), backgroundSub: true);
		RunSnes(Path.ChangeExtension(romPath, "panorama-background-sub-mode7.sfc"), backgroundSub: true);
		RunGba(Path.ChangeExtension(romPath, "panorama-background-negative.gba"), background: 1);
		RunSnes(Path.ChangeExtension(romPath, "panorama-background-sub-negative.sfc"), backgroundSub: true);
	}
	private static void RunSnes(string romPath, bool holes = false, int window = 0, int math = 0, bool sub = false, bool overlap = false, bool rotate = false, bool transparent = false, bool backgroundSub = false, int backgroundLayer = 0)
	{
		byte[] rom = new byte[32768];
		List<byte> code = new() {0x78, 0x18, 0xFB, 0xC2, 0x10, 0xA2, 0xFF, 0x1F, 0x9A, 0xE2, 0x20};
		void Store(ushort address, byte value) => code.AddRange(new byte[] {0xA9, value, 0x8D, (byte)address, (byte)(address >> 8)});
		bool mode7 = romPath.Contains("mode7");
		if(romPath.Contains("panorama")) { Store(0x0068, 0xF6); Store(0x0069, romPath.Contains("negative") ? (byte)0xFF : (byte)3); Store(0x006A, 1); }
		Store(0x2100, 0x80); Store(0x2101, romPath.Contains("obj-interlace-rect") ? (byte)0xC0 : mode7 ? (byte)2 : (byte)0); Store(0x2102, 0); Store(0x2103, 0);
		int backgroundMode = 0;
		for(int mode = 1; mode <= 6; mode++) { if(romPath.Contains($"mode{mode}")) { backgroundMode = mode; } }
		if(mode7) { backgroundMode = 7; }
		Store(0x2105, (byte)backgroundMode);
		for(int sprite = 0; sprite < 128; sprite++) {
			Store(0x2104, 0); Store(0x2104, 240); Store(0x2104, 0); Store(0x2104, 0);
		}
		for(int index = 0; index < 32; index++) { Store(0x2104, 0); }
		Store(0x2102, 0); Store(0x2103, 0);
		Store(0x2104, 8); Store(0x2104, 8); Store(0x2104, 0);
		Store(0x2104, (byte)((math == 1 ? 8 : 0) | (romPath.Contains("obj-interlace-bg") ? 0x30 : 0) | (romPath.Contains("obj-interlace") && romPath.Contains("hflip") ? 0x40 : 0) | (romPath.Contains("obj-interlace") && romPath.Contains("vflip") ? 0x80 : 0)));
		if(romPath.Contains("obj-interlace-group")) { Store(0x2104, 20); Store(0x2104, 12); Store(0x2104, 0); Store(0x2104, 0); }
		if(overlap) { Store(0x2104, 15); Store(0x2104, 8); Store(0x2104, 1); Store(0x2104, 0); }
		Store(0x2115, 0x80); Store(0x2116, 0); Store(0x2117, mode7 ? (byte)0x40 : (byte)0);
		for(int word = 0; word < 16; word++) {
			byte bits = !transparent && word < 8 ? (holes ? (byte)254 : (byte)255) : (byte)0;
			bool alternateObjRow = romPath.Contains("obj-interlace") && word < 8 && (word & 1) != 0;
			Store(0x2118, alternateObjRow ? (byte)0 : bits); Store(0x2119, alternateObjRow ? bits : (byte)0);
		}
		if(romPath.Contains("obj-interlace")) { Store(0x2121, math==1 ? (byte)194 : (byte)130); Store(0x2122, 0); Store(0x2122, 0x7C); }
		if(overlap) {
			for(int word = 0; word < 16; word++) { Store(0x2118, 0); Store(0x2119, word < 8 ? (byte)255 : (byte)0); }
			Store(0x2121, 130); Store(0x2122, 0); Store(0x2122, 0x7C);
		}
		Store(0x2121, 0); Store(0x2122, 0); Store(0x2122, 0x7C);
		Store(0x2121, math == 1 ? (byte)193 : (byte)129); Store(0x2122, 31); Store(0x2122, 0);
		if(romPath.Contains("obj-interlace-bg")) {
			Store(0x2121,1); Store(0x2122,0); Store(0x2122,0x7C); Store(0x2122,0); Store(0x2122,0x7C);
		}
		if(mode7) {
			Store(0x2116, 0); Store(0x2117, 0);
			for(int word = 0; word < 64; word++) { Store(0x2118, 0); Store(0x2119, romPath.Contains("ext-high") ? (byte)0x81 : (byte)1); }
			for(ushort address = 0x211B; address <= 0x2120; address++) {
				byte high = romPath.Contains("turn")
					? address == 0x211C ? (byte)1 : address == 0x211D ? (byte)255 : (byte)0
					: address == 0x211B ? romPath.Contains("scale") ? (byte)2 : (byte)1 : address == 0x211E ? (byte)1 : (byte)0;
				Store(address, 0); Store(address, high);
			}
			Store(0x210D, 0); Store(0x210D, 0); Store(0x210E, 0); Store(0x210E, 0);
			Store(0x211A, 0);
			Store(0x2121, 1); Store(0x2122, sub ? (byte)31 : (byte)0); Store(0x2122, sub ? (byte)0 : (byte)0x7C);
		} else if(sub || backgroundSub) {
			Store(0x2116, 0); Store(0x2117, 0x10);
			int bpp = backgroundMode switch {
				1 => backgroundLayer < 2 ? 4 : 2,
				2 => 4,
				3 => backgroundLayer == 0 ? 8 : 4,
				4 => backgroundLayer == 0 ? 8 : 2,
				5 => backgroundLayer == 0 ? 4 : 2,
				6 => 4,
				_ => 2
			};
			// Distinct colors exercise every plane pair, including the high
			// planes of 8-bit backgrounds (color 85 instead of color 5).
			for(int word = 0; word < bpp * (backgroundMode == 5 || backgroundMode == 6 ? 8 : 4); word++) { Store(0x2118, romPath.Contains("bgholes") && word % 8 == 0 ? (byte)254 : (byte)255); Store(0x2119, 0); }
			Store((ushort)(0x2107 + backgroundLayer), 0x20);
			Store((ushort)(0x210B + backgroundLayer / 2), (byte)(backgroundLayer % 2 == 0 ? 1 : 0x10));
			Store(0x2121, (byte)(backgroundMode == 0 ? backgroundLayer * 32 + 1 : bpp == 8 ? 85 : bpp == 4 ? 5 : 1)); Store(0x2122, sub ? (byte)31 : (byte)0); Store(0x2122, sub ? (byte)0 : (byte)0x7C);
			if(romPath.Contains("scroll")) {
				Store((ushort)(0x210D + backgroundLayer * 2), 253); Store((ushort)(0x210D + backgroundLayer * 2), 0);
				Store((ushort)(0x210E + backgroundLayer * 2), 250); Store((ushort)(0x210E + backgroundLayer * 2), 0);
			}
		}
		if(window != 0) {
			Store(0x2125, 2); // Object window 1 enabled, normal inside mask.
			Store(0x2126, window == 1 ? (byte)12 : (byte)10);
			Store(0x2127, window == 1 ? (byte)15 : (byte)13);
			Store(0x212E, 0x10); // Apply object window to the main screen.
		}
		if(romPath.Contains("bgmask")) {
			Store(0x2123, 2); Store(0x2126, 40); Store(0x2127, 95);
			Store(sub ? (ushort)0x212E : (ushort)0x212F, 1);
		}
		Store(0x212C, sub ? (byte)(1 << backgroundLayer) : (byte)0x10); Store(0x212D, sub || romPath.Contains("both-screens") ? (byte)0x10 : backgroundSub ? (byte)(1 << backgroundLayer) : (byte)0);
		if(romPath.Contains("bglow")) {
			Store(0x210A, 0x30); Store(0x210C, backgroundLayer == 2 ? (byte)0x11 : (byte)0x10);
			Store(0x2121, 97); Store(0x2122, 0); Store(0x2122, 0x7C);
			Store(0x212C, (byte)((1 << backgroundLayer) | 8));
		}
		Store(0x2130, sub ? (byte)2 : (byte)0); Store(0x2131, sub ? (byte)(0x40 | (1 << backgroundLayer)) : math != 0 ? (byte)0x50 : (byte)0);
		if(romPath.Contains("hires-math-operand")) { Store(0x2130,2); }
		Store(0x2132, 0x9F); // Blue fixed color for object half-add math.
		byte screenMode = romPath.Contains("pseudo-hires") ? (byte)8 : mode7 && backgroundLayer == 1 ? (byte)0x40 : (byte)0;
		if(romPath.Contains("interlace")) { screenMode |= 1; }
		if(romPath.Contains("obj-interlace")) { screenMode |= 2; }
		Store(0x2133, screenMode);
		if(romPath.Contains("obj-interlace-bg")) { Store(0x212C,0x11); }
		if(rotate) { Store(0x2102, 2); Store(0x2103, 0x80); } // Start evaluation at sprite 1.
		Store(0x2100, 15);
        if(romPath.Contains("obj-interlace-bg-fields")) {
			// Opposite WRAM camera/condition values in adjacent real PPU fields.
			Store(0x0000,0); Store(0x0001,1);
			ushort poll=(ushort)(0x8000+code.Count);
			code.AddRange(new byte[] {0xAD,0x12,0x42,0x10,0xFB,0xAD,0,0,0x49,1,0x8D,0,0,0x49,1,0x8D,1,0});
			code.AddRange(new byte[] {0xAD,0x12,0x42,0x30,0xFB,0x4C,(byte)poll,(byte)(poll>>8)});
		} else if(romPath.Contains("mixed-raster")) {
			// Toggle pseudo-hires off after 80 real horizontal blank periods.
			ushort poll=(ushort)(0x8000+code.Count);
			code.AddRange(new byte[] {0xAD,0x12,0x42,0x10,0xFB}); Store(0x2133,8);
			code.AddRange(new byte[] {0xAD,0x12,0x42,0x30,0xFB,0xA2,80,0});
			int horizontal=code.Count;
			code.AddRange(new byte[] {0x2C,0x12,0x42,0x50,0xFB,0x2C,0x12,0x42,0x70,0xFB,0xCA,0xD0,0});
			code[^1]=unchecked((byte)(horizontal-code.Count)); Store(0x2133,0);
			code.AddRange(new byte[] {0x4C,(byte)poll,(byte)(poll>>8)});
		} else if(romPath.Contains("obj-interlace-poses")) {
			// Switch two source colors every vblank. Adjacent woven fields must
			// retain different poses, even though their OAM identity is unchanged.
			Store(0x0000, 0);
			ushort poll = (ushort)(0x8000 + code.Count);
			code.AddRange(new byte[] {0xAD,0x12,0x42,0x10,0xFB,0xAD,0,0,0x49,1,0x8D,0,0});
			int poseBranch=code.Count; code.AddRange(new byte[] {0xF0,0});
			Store(0x2121,129); Store(0x2122,0xE0); Store(0x2122,3);
			Store(0x2121,130); Store(0x2122,0xFF); Store(0x2122,0x7F);
			int doneBranch=code.Count; code.AddRange(new byte[] {0x80,0});
			code[poseBranch+1]=(byte)(code.Count-poseBranch-2);
			Store(0x2121,129); Store(0x2122,31); Store(0x2122,0);
			Store(0x2121,130); Store(0x2122,0); Store(0x2122,0x7C);
			code[doneBranch+1]=(byte)(code.Count-doneBranch-2);
			code.AddRange(new byte[] {0xAD,0x12,0x42,0x30,0xFB,0x4C,(byte)poll,(byte)(poll>>8)});
		} else if(romPath.Contains("memory")) {
			// Toggle two related WRAM bytes every sixteen vblanks. Writes happen
			// in vblank, keeping each displayed frame's state internally consistent.
			Store(0x0000, 0); Store(0x0001, 0); Store(0x0002, 0xA0);
			ushort poll = (ushort)(0x8000 + code.Count);
			code.AddRange(new byte[] {0xAD, 0x12, 0x42, 0x10, 0xFB, 0xEE, 0x01, 0x00,
				0xAD, 0x01, 0x00, 0x29, 0x0F, 0xD0, 0x0F,
				0xAD, 0x00, 0x00, 0x49, 0x01, 0x8D, 0x00, 0x00,
				0x09, 0xA0, 0x8D, 0x02, 0x00, 0xEA, 0xEA,
				0xAD, 0x12, 0x42, 0x30, 0xFB, 0x4C, (byte)poll, (byte)(poll >> 8)});
		}
		ushort loop = (ushort)(0x8000 + code.Count); code.AddRange(new byte[] {0x4C, (byte)loop, (byte)(loop >> 8)});
		code.CopyTo(rom, 0);
		System.Text.Encoding.ASCII.GetBytes("METASPRITE TEST      ").CopyTo(rom, 0x7FC0);
		rom[0x7FD5] = 0x20; rom[0x7FD7] = 5; rom[0x7FD9] = 1;
		rom[0x7FDC] = 255; rom[0x7FDD] = 255; rom[0x7FFC] = 0; rom[0x7FFD] = 0x80;
		File.WriteAllBytes(romPath, rom);
		uint result = romPath.Contains("hires-math") ? RunMetaSpriteSnesHiresMathTest(romPath) : romPath.Contains("hires") ? RunMetaSpriteSnesHiresCaptureTest(romPath) : RunMetaSpriteSnesCaptureTest(romPath);
		if(result != 7) { throw new Exception($"SNES live capture/replacement failed for {Path.GetFileName(romPath)} (LoROM, BG mode {backgroundMode}): flags {result}."); }
		Console.WriteLine($"PASS: {Path.GetFileName(romPath)}, SNES LoROM / BG mode {backgroundMode}, live capture, HD replacement, transparent/partial-alpha backdrop, border placement, and disable restoration.");
	}

	private static void RunNes(string romPath, bool background = false, bool scroll = false, bool mask = false)
	{
		byte[] rom = new byte[16 + 16384 + 8192];
		new byte[] {0x4E, 0x45, 0x53, 0x1A, 1, 1}.CopyTo(rom, 0);
		List<byte> code = new() {0x78, 0xD8, 0xA2, 0xFF, 0x9A};
		if(romPath.Contains("sprite0-raster")) { code.AddRange(new byte[] {0xA9, 13, 0x85, 0x60, 0xA9, 3, 0x85, 0x61, 0xA9, 0xF6, 0x85, 0x62, 0xA9, 0xFF, 0x85, 0x63,
			0xA9, 0x18, 0x85, 0x64, 0xA9, 0xE8, 0x85, 0x65, 0xA9, 0, 0x85, 0x66}); }
		void Store(ushort address, byte value) => code.AddRange(new byte[] {0xA9, value, 0x8D, (byte)address, (byte)(address >> 8)});
		Store(0x2000, 0); Store(0x2001, 0);
		code.AddRange(new byte[] {0x2C, 0x02, 0x20, 0x10, 0xFB, 0x2C, 0x02, 0x20, 0x10, 0xFB});
		// Hide all sprites, then give primary OAM sprite zero a known 8x8 graphic.
		code.AddRange(new byte[] {0xA2, 0, 0xA9, 0xFF, 0x9D, 0, 2, 0xE8, 0xD0, 0xFA});
		Store(0x0200, 7); Store(0x0201, 1); Store(0x0202, 0); Store(0x0203, 8);
		Store(0x2003, 0); Store(0x4014, 2);
		Store(0x2006, 0x3F); Store(0x2006, 0); Store(0x2007, 0);
		Store(0x2006, 0x3F); Store(0x2006, 0x11); Store(0x2007, 1);
		if(romPath.Contains("sprite0-raster")) { Store(0x2006, 0x3F); Store(0x2006, 1); Store(0x2007, 0); }
		Store(0x2006, 0x20); Store(0x2006, 0); Store(0x2005, scroll ? (byte)253 : (byte)0); Store(0x2005, scroll ? (byte)237 : (byte)0);
		Store(0x2000, scroll ? (byte)3 : (byte)0); Store(0x2001, mask ? (byte)0x1C : background ? (byte)0x1E : (byte)0x14);
		ushort loop = (ushort)(0x8000 + code.Count);
		if(romPath.Contains("sprite0-raster")) {
			// NROM: sprite 0 hit signals a horizontal split; PNG tracking must not alter the PPU event.
			code.AddRange(new byte[] {0x2C, 0x02, 0x20, 0x10, 0xFB});
			Store(0x2005, 0); Store(0x2005, 0);
			code.AddRange(new byte[] {0x2C, 0x02, 0x20, 0x70, 0xFB, 0x2C, 0x02, 0x20, 0x50, 0xFB});
			Store(0x2005, 24); Store(0x2005, 0);
		}
		code.AddRange(new byte[] {0x4C, (byte)loop, (byte)(loop >> 8)});
		code.CopyTo(rom, 16);
		for(int vector = 0x3FFA; vector < 0x4000; vector += 2) { rom[16 + vector] = 0; rom[16 + vector + 1] = 0x80; }
		for(int row = 0; row < 8; row++) { rom[16 + 16384 + 16 + row] = romPath.Contains("transparent") ? (byte)0 : romPath.Contains("holes") ? (byte)0xFE : (byte)0xFF; }
		if(romPath.Contains("sprite0-raster")) { for(int row = 0; row < 8; row++) { rom[16 + 16384 + row] = 255; } }
		File.WriteAllBytes(romPath, rom);
		uint result = RunMetaSpriteNesCaptureTest(romPath);
		if(result != 7) { throw new Exception($"NES live capture/replacement failed: flags {result}."); }
		Console.WriteLine($"PASS: {Path.GetFileName(romPath)}, live capture, HD replacement, transparent/partial-alpha backdrop, and disable restoration.");
	}

	public static void RunWsSuite(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunWs(Path.ChangeExtension(romPath, "mono.ws"), 0);
		RunWs(Path.ChangeExtension(romPath, "mono-background-scroll.ws"), 0, background: true, scroll: true);
		RunWs(Path.ChangeExtension(romPath, "mono-background-scroll-speed.ws"), 0, background: true, scroll: true);
		RunWs(Path.ChangeExtension(romPath, "background-scroll-speed.wsc"), background: true, scroll: true);
		RunWs(Path.ChangeExtension(romPath, "background-window-underlay.wsc"), background: true, window: true);
		RunWs(Path.ChangeExtension(romPath, "background-window-underlay-clear.wsc"), background: true, window: true);
		RunWs(Path.ChangeExtension(romPath, "mono-background-window.ws"), 0, background: true, window: true);
		RunWs(Path.ChangeExtension(romPath, "mono-background-outside-window-occluded.ws"), 0, background: true, window: true, outsideWindow: true, occluded: true);
		RunWs(Path.ChangeExtension(romPath, "mono-obj-window.ws"), 0, spriteWindow: true);
		RunWs(Path.ChangeExtension(romPath, "mono-obj-outside-window.ws"), 0, spriteWindow: true, spriteOutside: true);
		RunWs(Path.ChangeExtension(romPath, ".wsc"));
		RunWs(Path.ChangeExtension(romPath, "background.wsc"), background: true);
		RunWs(Path.ChangeExtension(romPath, "background-scroll.wsc"), background: true, scroll: true);
		RunWs(Path.ChangeExtension(romPath, "background-window.wsc"), background: true, window: true);
		RunWs(Path.ChangeExtension(romPath, "background-outside-window.wsc"), background: true, window: true, outsideWindow: true);
		RunWs(Path.ChangeExtension(romPath, "background-outside-window-occluded.wsc"), background: true, window: true, outsideWindow: true, occluded: true);
		RunWs(Path.ChangeExtension(romPath, "planar.wsc"), 0xC0);
		RunWs(Path.ChangeExtension(romPath, "holes-planar.wsc"), 0xC0);
		RunWs(Path.ChangeExtension(romPath, "packed.wsc"), 0xE0);
		RunWs(Path.ChangeExtension(romPath, "obj-window.wsc"), spriteWindow: true);
		RunWs(Path.ChangeExtension(romPath, "obj-outside-window.wsc"), spriteWindow: true, spriteOutside: true);
	}
	public static void RunNesRasterSuite(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunNes(Path.ChangeExtension(romPath, "sprite0-raster.nes"), background: true);
	}
	private static void RunWs(string romPath, byte videoMode = 0x80, bool background = false, bool scroll = false, bool window = false, bool outsideWindow = false, bool occluded = false, bool spriteWindow = false, bool spriteOutside = false)
	{
		byte[] rom = new byte[65536];
		List<byte> code = new() {0xFA, 0x31, 0xC0, 0x8E, 0xD8}; // CLI; AX=0; DS=0.
		void Port(byte port, byte value) => code.AddRange(new byte[] {0xB0, value, 0xE6, port});
		void Word(ushort address, ushort value) => code.AddRange(new byte[] {0xC7, 0x06, (byte)address, (byte)(address >> 8), (byte)value, (byte)(value >> 8)});
		Port(0, 0); Port(0x60, videoMode); Port(0x14, 1); Port(0x16, 158);
		Word(0xFE00, 0x000F); Word(0xFF02, 0x0F00);
		if(videoMode == 0) { Port(0x1C, 0xF0); Port(0x20, 0); Port(0x30, 0x10); } // Shade 0 white, shade 1 black; BG0 white and OBJ palette 8 black.
		if(background) { Word(0xFE02, 0x000F); }
		if(romPath.Contains("underlay")) {
			Word(0xFE02, 0x0FF0); Word(0xFE22, 0x000F); Port(7, 0x10); // BG0 yellow; separate BG1 map/palette blue; backdrop blue.
			bool clearForeground = romPath.Contains("underlay-clear");
			for(ushort tile = 0; tile < 1024; tile++) { Word((ushort)(0x0800 + tile * 2), clearForeground ? (ushort)0x0801 : (ushort)0x0200); }
			if(clearForeground) { for(ushort row = 0; row < 8; row++) { Word((ushort)(0x2010 + row * 2), 0); } }
		}
		for(ushort row = 0; row < 8; row++) {
			if(videoMode == 0 || videoMode == 0x80) { Word((ushort)(0x2000 + row * 2), 0x00FF); }
			else {
				Word((ushort)(0x4000 + row * 4), videoMode == 0xE0 ? (ushort)0x1111 : romPath.Contains("holes") ? (ushort)0x00FE : (ushort)0x00FF);
				Word((ushort)(0x4002 + row * 4), videoMode == 0xE0 ? (ushort)0x1111 : (ushort)0);
			}
		}
		Word(0x1000, outsideWindow && !occluded ? (ushort)0x2000 : (ushort)0); Word(0x1002, 0x0808);
		if(spriteOutside) { Word(0x1000, 0x1000); }
		if(spriteWindow) { Port(0x0C, spriteOutside ? (byte)20 : (byte)4); Port(0x0D, 4); Port(0x0E, spriteOutside ? (byte)30 : (byte)19); Port(0x0F, 19); }
		if(scroll) { Port(0x10, 253); Port(0x11, 250); }
		if(window) { Port(0x08, 40); Port(0x09, 24); Port(0x0A, 95); Port(0x0B, 63); Port(0x12, 7); Port(0x13, 9); }
		Port(4, 8); Port(5, 0); Port(6, 1); Port(1, 0); Port(0, outsideWindow ? (byte)0x37 : window ? (byte)0x27 : background ? (byte)5 : (byte)4);
		if(spriteWindow) { Port(0, 0x0C); }
		code.AddRange(new byte[] {0xEB, 0xFE});
		code.CopyTo(rom, 0);
		new byte[] {0xEA, 0, 0, 0, 0xF0}.CopyTo(rom, 0xFFF0);
		rom[0xFFF7] = videoMode == 0 ? (byte)0 : (byte)1;
		File.WriteAllBytes(romPath, rom);
		uint result = RunMetaSpriteWsCaptureTest(romPath);
		if(result != 7) { throw new Exception($"WonderSwan live capture/replacement failed: flags {result}."); }
		Console.WriteLine($"PASS: {Path.GetFileName(romPath)} (WonderSwan mode 0x{videoMode:X}), live capture, HD replacement, transparent backdrop, and disable restoration.");
	}

	public static void RunGbaAffine(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(typeof(GameplayFixture).Assembly, (name, assembly, searchPath) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunGba(Path.ChangeExtension(romPath, "object-ordinary-vmosaic-palette3.gba"));
		RunGba(Path.ChangeExtension(romPath, "object-ordinary-hvmosaic-flipped-palette3.gba"));
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-asymmetric-bpp8.gba"), bpp8: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-transposed-bpp8.gba"), bpp8: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-asymmetric-quarter-turn-bpp8.gba"), bpp8: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-asymmetric-quarter-turn-2d-bpp8.gba"), bpp8: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-asymmetric-quarter-turn-2d-bpp8-dark.gba"), bpp8: true, dark: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-double-size.gba"));
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-asymmetric-double-size-bpp8.gba"), bpp8: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-asymmetric-quarter-turn-2d-double-size-bpp8.gba"), bpp8: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-asymmetric-quarter-turn-2d-bpp8-mosaic.gba"), bpp8: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-asymmetric-quarter-turn-2d-bpp8-vmosaic.gba"), bpp8: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged-asymmetric-quarter-turn-2d-bpp8-hvmosaic.gba"), bpp8: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-window.gba"), holes: true, overlap: true, window: 1);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-dark.gba"), holes: true, overlap: true, dark: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-semi.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-priority1.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-priority3-semi.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-mosaic.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-vmosaic.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-mosaic-clipped.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-priority3-bg-between.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-priority1-bg-between.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-lower-reveal-priority3-bg-between-png.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap-priority3-bg-between-mixed-group.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "object-affine-identity.gba"));
		RunGba(Path.ChangeExtension(romPath, "object-affine-identity-holes-window.gba"), holes: true, window: 1);
		RunGba(Path.ChangeExtension(romPath, "object-affine-enlarged.gba"));
		RunGba(Path.ChangeExtension(romPath, "background-affine.gba"), background: 3);
		RunGba(Path.ChangeExtension(romPath, "background-affine-scaled.gba"), background: 3);
		RunGba(Path.ChangeExtension(romPath, "background-affine-rotated.gba"), background: 3);
		RunGba(Path.ChangeExtension(romPath, "background-bitmap4.gba"), background: 4);
		RunGba(Path.ChangeExtension(romPath, "background-bitmap4-page1.gba"), background: 4);
		RunGba(Path.ChangeExtension(romPath, "background-bitmap4-page1-live.gba"), background: 4);
		RunGba(Path.ChangeExtension(romPath, "background-bitmap3.gba"), background: 4);
		RunGba(Path.ChangeExtension(romPath, "background-bitmap5.gba"), background: 4);
	}
	public static void RunGbaCaptureMetadata(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunGba(Path.ChangeExtension(romPath, ".gba"));
		RunGba(Path.ChangeExtension(romPath, "holes-window.gba"), holes: true, window: 1);
		RunGba(Path.ChangeExtension(romPath, "holes-window-middle.gba"), holes: true, window: 2);
		RunGba(Path.ChangeExtension(romPath, "holes-overlap.gba"), holes: true, overlap: true);
		RunGba(Path.ChangeExtension(romPath, "transparent.gba"), transparentSource: true);
		RunGba(Path.ChangeExtension(romPath, "dark.gba"), dark: true);
	}

	private static void RunGba(string romPath, bool mirror = false, bool bpp8 = false, bool dark = false, bool holes = false, int window = 0, bool overlap = false, bool transparentSource = false, int background = 0)
	{
		byte[] rom = new byte[32768];
		BitConverter.GetBytes(0xEA00002Eu).CopyTo(rom, 0); // Branch past cartridge header.
		List<uint> instructions = new();
		List<(int Instruction, uint Value)> literals = new();
		void Load(int register, uint value) { literals.Add((instructions.Count, value)); instructions.Add(0xE59F0000u | ((uint)register << 12)); }
		void Store(uint address, uint value, bool word = false) {
			Load(0, address); Load(1, value); instructions.Add(word ? 0xE5801000u : 0xE1C010B0u);
		}
		Store(0x04000000, 0x0080); // Forced blank during initialization.
		if(romPath.Contains("panorama")) { Store(0x02000068, romPath.Contains("negative") ? 0xFFF6u : 0x03F6u); Store(0x0200006A, 1); }
		for(uint sprite = 0; sprite < 128; sprite++) { Store(0x07000000 + sprite * 8, 0x0200); }
		Store(0x05000202, 0x001F);
		Store(0x05000000, 0x7C00); // Known backdrop for object-removal fallback.
		Store(0x05000204, 0x7C00);
		if(romPath.Contains("lower-reveal")) { Store(0x05000204, 0x03FF); } // Lower OBJ yellow, backdrop blue: distinguish reveal from fallback.
		uint objectTiles = background == 4 ? 0x06014000u : 0x06010000u;
		for(uint row = 0; row < 8; row++) {
			if(bpp8) {
				bool blueRow = romPath.Contains("quarter-turn-2d") && row <= 3;
				Store(0x06010000 + row * 8, blueRow ? 0x02020202u : 0x01010101u, true);
				Store(0x06010004 + row * 8, blueRow ? 0x02020202u : romPath.Contains("asymmetric") ? 0x02020201u : 0x02020202u, true);
			} else { Store(objectTiles + row * 4, transparentSource ? 0u : holes ? 0x01111111u : mirror ? 0x22221111u : 0x11111111u, true); }
		}
		Store(0x07000000, bpp8 ? 0x2008u : 8u); Store(0x07000002, mirror ? 0x1008u : 8u); Store(0x07000004, background == 4 ? 512u : 0u);
		if(romPath.Contains("object-affine-")) {
			bool transposed = romPath.Contains("transposed");
			bool quarterTurn = romPath.Contains("quarter-turn");
			uint diagonal = quarterTurn && romPath.Contains("double-size") ? 0x40u : romPath.Contains("enlarged") ? 0x80u : 0x100u;
			Store(0x07000000, bpp8 ? 0x2108u : 0x0108u); // Affine OBJ, parameter set 0, normal bounding box.
			if(romPath.Contains("double-size")) { Store(0x07000000, bpp8 ? 0x2308u : 0x0308u); } // 8x8 source in a 16x16 affine bounding box.
			Store(0x07000006, transposed || quarterTurn ? 0u : diagonal); Store(0x0700000E, quarterTurn ? diagonal : transposed ? 0x100u : 0u);
			Store(0x07000016, quarterTurn ? 0x10000u - diagonal : transposed ? 0x100u : 0u); Store(0x0700001E, transposed || quarterTurn ? 0u : diagonal); // Signed 8.8 inverse matrix.
			if(romPath.Contains("mosaic")) {
				Store(0x07000000, bpp8 ? 0x3108u : 0x1108u);
				Store(0x0400004C, romPath.Contains("hvmosaic") ? 0x3100u : romPath.Contains("vmosaic") ? 0x3000u : 0x0100u); // Horizontal pairs and/or vertical groups of four.
			}
		}
		if(romPath.Contains("object-ordinary-")) {
			for(uint row = 0; row < 8; row++) {
				uint packed = 0; for(int column = 0; column < 8; column++) { packed |= (column >= 5 || row >= 5 ? 2u : 1u) << (column * 4); }
				Store(0x06010000 + row * 4, packed, true);
			}
			Store(0x05000202, 0x03E0); // Palette zero differs: recognition must use OBJ palette 3.
			Store(0x05000262, 0x001F); Store(0x05000264, 0x7C00);
			Store(0x07000000, 0x1008); Store(0x07000002, romPath.Contains("flipped") ? 0x3008u : 8u); Store(0x07000004, 0x3000);
			Store(0x0400004C, romPath.Contains("hvmosaic") ? 0x3100u : 0x3000u);
		}
		if(overlap) {
			for(uint row = 0; row < 8; row++) { Store(0x06010020 + row * 4, romPath.Contains("lower-reveal-mosaic") ? 0x12121212u : romPath.Contains("vmosaic") && row % 2 != 0 ? 0x11111111u : 0x22222222u, true); }
			Store(0x07000008, 8); Store(0x0700000A, romPath.Contains("lower-reveal") ? 8u : 15u); Store(0x0700000C, 1);
			if(romPath.Contains("lower-reveal") && romPath.Contains("semi")) {
				Store(0x07000008, 0x0408); // Lower OBJ forces alpha blend with backdrop; upper OBJ stays ordinary.
				Store(0x04000050, 0x2000); Store(0x04000052, 0x0808);
			}
			if(romPath.Contains("priority1")) { Store(0x0700000C, 0x0401); }
			if(romPath.Contains("priority3")) { Store(0x0700000C, 0x0C01); }
			if(romPath.Contains("lower-reveal-mosaic")) { Store(0x07000008, 0x1008); Store(0x0400004C, 0x0100); }
			if(romPath.Contains("mosaic-clipped")) { Store(0x0700000A, 9); }
			if(romPath.Contains("lower-reveal-vmosaic")) { Store(0x07000008, 0x1008); Store(0x0400004C, 0x1000); }
		}
		if(window != 0) {
			Store(0x04000040, window == 1 ? 0x0C10u : 0x0A0Eu); // Mask right or middle columns.
			Store(0x04000044, 0x0810);
			Store(0x04000048, 0x20); // Effects permitted; objects hidden inside WIN0.
			Store(0x0400004A, 0x30); // Objects and effects permitted outside WIN0.
		}
		if(dark) { Store(0x04000050, 0x00F0); Store(0x04000054, 8); } // Darken objects and backdrop by half.
		if(background == 4) {
			int bitmapMode = romPath.Contains("bitmap3") ? 3 : romPath.Contains("bitmap5") ? 5 : 4;
			if(bitmapMode != 4 || romPath.Contains("page1")) {
				Store(0x05000002, 0x7C00);
				// Mode 4 fills only page 1; direct-color modes fill page 0 with blue.
				Load(0, bitmapMode == 4 ? 0x0600A000u : 0x06000000u);
				Load(1, bitmapMode == 4 ? 0x01010101u : 0x7C007C00u);
				Load(2, bitmapMode == 5 ? 160u * 128 / 2 : bitmapMode == 3 ? 240u * 160 / 2 : 240u * 160 / 4);
				instructions.Add(0xE4801004); // STR r1,[r0],#4
				instructions.Add(0xE2522001); // SUBS r2,r2,#1
				instructions.Add(0x1AFFFFFC); // BNE to STR
			}
			Store(0x0400000C, 3);
			Store(0x04000020, 0x100); Store(0x04000022, 0);
			Store(0x04000024, 0); Store(0x04000026, 0x100);
			Store(0x04000028, 0, true); Store(0x0400002C, 0, true);
			Store(0x04000050, 0x2044); Store(0x04000052, 0x0808);
		} else if(background == 3) {
			bool scaled = romPath.Contains("scaled"), rotated = romPath.Contains("rotated");
			Store(0x0400000C, 0x6103); // BG2: 256x256 affine map, wrap, priority 3.
			Store(0x04000020, rotated ? 0u : scaled ? 0x200u : 0x100u); Store(0x04000022, rotated || scaled ? 0x100u : 0u);
			Store(0x04000024, rotated ? 0xFF00u : 0u); Store(0x04000026, rotated ? 0u : 0x100u);
			Store(0x04000028, 253u << 8, true); Store(0x0400002C, 250u << 8, true);
			Store(0x04000050, 0x2044); Store(0x04000052, 0x0808);
		} else if(background != 0) {
			Store(0x04000008, background == 2 ? 0xC103u : 0x0103u);
			Store(0x04000010, background == 2 ? 509u : 253u); Store(0x04000012, background == 2 ? 506u : 250u);
			Store(0x04000050, 0x2041); Store(0x04000052, 0x0808); // BG0 main / backdrop secondary alpha targets.
		}
		uint bitmapDisplay = romPath.Contains("bitmap3") ? 0x1443u : romPath.Contains("bitmap5") ? 0x1445u : romPath.Contains("page1") ? 0x1454u : 0x1444u;
		Store(0x04000000, background == 4 ? bitmapDisplay : background == 3 ? 0x1442u : window != 0 ? 0x3040u : background != 0 ? 0x1140u : 0x1040u);
		if(romPath.Contains("bg-between")) {
			Store(0x05000002, 0x7C1F); // Magenta BG0 at priority 1, between OBJ priorities 0 and 3.
			for(uint row = 0; row < 8; row++) { Store(0x06000000 + row * 4, 0x11111111u, true); }
			Store(0x04000008, 0x0101); // Screen block 1, tile 0 throughout the zero-initialized map.
			Store(0x04000000, 0x1140);
		}
		if(romPath.Contains("page1-live")) {
			Load(0, 0x04000004); Load(3, 16); Load(4, bitmapDisplay); Load(5, 0x04000000);
			int loop = instructions.Count;
			instructions.Add(0xE1D010B0); instructions.Add(0xE3110001); instructions.Add(0x0AFFFFFC); // Wait for vblank.
			instructions.Add(0xE1D010B0); instructions.Add(0xE3110001); instructions.Add(0x1AFFFFFC); // Wait for its end.
			instructions.Add(0xE2533001);
			instructions.Add(0x1A000000u | ((uint)(loop - instructions.Count - 2) & 0xFFFFFF));
			Load(3, 16); instructions.Add(0xE2244010); instructions.Add(0xE1C540B0); // Toggle DISPCNT page every 16 frames.
			instructions.Add(0xEA000000u | ((uint)(loop - instructions.Count - 2) & 0xFFFFFF));
		} else { instructions.Add(0xEAFFFFFE); } // Stable display loop.
		foreach(var literal in literals) {
			int offset = instructions.Count * 4 - (literal.Instruction * 4 + 8);
			if(offset < 0 || offset > 4095) { throw new Exception("ARM literal out of range."); }
			instructions[literal.Instruction] |= (uint)offset;
			instructions.Add(literal.Value);
		}
		for(int index = 0; index < instructions.Count; index++) { BitConverter.GetBytes(instructions[index]).CopyTo(rom, 0xC0 + index * 4); }
		File.WriteAllBytes(romPath, rom);
		uint result = RunMetaSpriteGbaCaptureTest(romPath);
		if(result != 7) { throw new Exception($"GBA live capture/replacement failed: flags 0x{result:X}, expected 7."); }
		Console.WriteLine($"PASS: {Path.GetFileName(romPath)}, GBA live capture, HD replacement, transparent/partial-alpha backdrop, and disable restoration.");
	}

	public static void RunPcePrioritySuite(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		RunPce(Path.ChangeExtension(romPath, "opaque-bg-front.pce"));
		RunPce(Path.ChangeExtension(romPath, "opaque-bg-behind.pce"));
		RunPce(Path.ChangeExtension(romPath, "opaque-bg-behind-hide.pce"));
		RunPce(Path.ChangeExtension(romPath, "png-background-behind.pce"), background: true);
		RunPce(Path.ChangeExtension(romPath, "wide.pce"), wide: true);
		RunPce(Path.ChangeExtension(romPath, "holes-wide.pce"), wide: true);
		RunPce(Path.ChangeExtension(romPath, "holes-divider3.pce"), clockMode: 1);
		RunPce(Path.ChangeExtension(romPath, "holes-divider2.pce"), clockMode: 2);
		RunPce(Path.ChangeExtension(romPath, "mirror.pce"), mirror: true);
	}
	private static void RunPce(string romPath, bool wide = false, byte clockMode = 0, bool mirror = false, bool background = false)
	{
		byte[] rom = new byte[8192];
		List<byte> code = new() { 0x78, 0xD8, 0xA9, 0xFF, 0x53, 0x01, 0xA9, 0xF8, 0x53, 0x02, 0xA2, 0xFF, 0x9A };
		void Register(byte index, ushort value) => code.AddRange(new byte[] { 0x03, index, 0x13, (byte)value, 0x23, (byte)(value >> 8) });
		void Word(ushort value) => code.AddRange(new byte[] { 0x13, (byte)value, 0x23, (byte)(value >> 8) });
		void Store(ushort address, byte value) => code.AddRange(new byte[] { 0xA9, value, 0x8D, (byte)address, (byte)(address >> 8) });
		Store(0x0400, clockMode);
		Register(5, 0); Register(9, 0);
		Register(0x0A, clockMode == 2 ? (ushort)0x0702 : (ushort)0x0202);
		Register(0x0B, clockMode == 1 ? (ushort)0x062A : clockMode == 2 ? (ushort)0x093F : (ushort)0x031F);
		Register(0x0C, 0x0F02); Register(0x0D, 223); Register(0x0E, 3);
		Register(0, 0x1000); code.AddRange(new byte[] { 0x03, 2 });
		for(int word = 0; word < (wide ? 128 : 64); word++) {
			Word(mirror ? word < 16 ? (ushort)0xFF00 : word < 32 ? (ushort)0x00FF : (ushort)0
				: word % 64 < 16 ? romPath.Contains("holes") ? (ushort)0xFFFE : (ushort)0xFFFF : (ushort)0);
		}
		Register(0, 0x2000); code.AddRange(new byte[] { 0x03, 2 });
		Word(72); Word(40); Word(0x80); Word(romPath.Contains("bg-behind") || romPath.Contains("background-behind") ? (ushort)0 : mirror ? (ushort)0x880 : wide ? (ushort)0x180 : (ushort)0x80);
		for(int word = 4; word < 256; word++) { Word(0); }
		Store(0x0402, 1); Store(0x0403, 1); Store(0x0404, 0x38); Store(0x0405, 0);
		if(mirror) { Store(0x0404, 7); Store(0x0405, 0); }
		if(romPath.Contains("opaque-bg")) {
			Register(0, 0); code.AddRange(new byte[] {0x03, 2});
			for(int tile = 0; tile < 1024; tile++) { Word(1); }
			Register(0, 16); code.AddRange(new byte[] {0x03, 2});
			for(int row = 0; row < 16; row++) { Word(row < 8 ? (ushort)0x00FF : (ushort)0); }
			Store(0x0402, 1); Store(0x0403, 0); Store(0x0404, 7); Store(0x0405, 0);
			background = true;
		}
		Register(0x0F, 0x10); Register(0x13, 0x2000); Register(5, background ? (ushort)0xC0 : (ushort)0x40);
		code.AddRange(new byte[] { 0x80, 0xFE });
		code.CopyTo(rom, 0); rom[0x1FFE] = 0; rom[0x1FFF] = 0xE0;
		File.WriteAllBytes(romPath, rom);
		uint result = RunMetaSpritePceGameplayTest(romPath);
		if(result != 31) { throw new Exception($"PCE gameplay integration failed: flags 0x{result:X}, expected 0x1F."); }
		Console.WriteLine($"PASS: {Path.GetFileName(romPath)} (HuCard, VDC1, clock divider {(clockMode == 0 ? 4 : clockMode == 1 ? 3 : 2)}, {(wide ? 32 : 16)}-pixel OBJ), live fetched capture, crop alignment, HD replacement, background preservation, and disable restoration.");
	}

	public static void RunSmsLegacySuite(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		foreach(int mode in new[] {0,1,2,3}) { RunSmsLegacy(Path.ChangeExtension(romPath, $"legacy-mode{mode}.sms"), mode); }
		RunSmsLegacy(Path.ChangeExtension(romPath, "legacy-large.sms"), 0, large: true);
		RunSmsLegacy(Path.ChangeExtension(romPath, "legacy-large-zoom.sms"), 0, large: true, zoom: true);
		RunSmsLegacy(Path.ChangeExtension(romPath, "legacy-holes.sms"), 0, holes: true);
		RunSmsLegacy(Path.ChangeExtension(romPath, "legacy-left.sms"), 0, left: true);
		RunSms(Path.ChangeExtension(romPath, "mode4-regression.sms"));
		RunSms(Path.ChangeExtension(romPath, "mode4-regression-zoom.sms"), zoom: true);
		RunSms(Path.ChangeExtension(romPath, "mode4-regression-left.sms"), leftClip: true);
		RunSms(Path.ChangeExtension(romPath, "mode4-regression-mask.sms"), columnMask: true);
		RunSms(Path.ChangeExtension(romPath, "mode4-regression-locks-scroll.sms"), scroll: true, scrollLocks: true);
	}
	private static void RunSmsLegacy(string path, int mode, bool large = false, bool zoom = false, bool holes = false, bool left = false)
	{
		byte[] rom = new byte[32768]; List<byte> code = new() {0xF3,0x31,0xF0,0xDF};
		void Out(byte port, byte value) => code.AddRange(new byte[] {0x3E,value,0xD3,port});
		void Reg(byte index, byte value) { Out(0xBF,value); Out(0xBF,(byte)(0x80|index)); }
		void Address(ushort value) { Out(0xBF,(byte)value); Out(0xBF,(byte)(value>>8)); }
		Reg(0, mode == 2 ? (byte)2 : (byte)0); Reg(1,0); Reg(2,6); Reg(3,0x80); Reg(4,0); Reg(5,0x7E); Reg(6,1); Reg(7,0x95);
		Address(0x4000); for(int i=0;i<8;i++) { Out(0xBE, mode == 3 ? (byte)0x55 : mode == 1 ? (byte)0xFC : (byte)0); }
		Address(0x6000); for(int i=0;i<8;i++) { Out(0xBE,0x55); }
		Address(0x4800); for(int i=0;i<32;i++) { Out(0xBE,holes ? (byte)0xFE : (byte)0xFF); }
		Address(0x7F00); foreach(byte v in new byte[] {7,left ? (byte)28 : (byte)8,0,left ? (byte)0x89 : (byte)9,0xD0,0,0,0}) { Out(0xBE,v); }
		code.AddRange(new byte[] {0x3E,0,0x32,0,0xC0}); // Explicit author-defined RAM gate.
		Reg(1,(byte)(0x40 | (mode == 1 ? 0x10 : mode == 3 ? 0x08 : 0) | (large ? 2 : 0) | (zoom ? 1 : 0))); code.AddRange(new byte[] {0x18,0xFE}); code.CopyTo(rom,0);
		File.WriteAllBytes(path,rom); uint result = mode == 1 ? RunSmsLegacyTextBackgroundTest(path) : RunMetaSpriteGameplayTest(path);
		if(result != 31) { throw new Exception($"SMS legacy graphics mode {mode}, large={large}, zoom={zoom}: flags 0x{result:X}, expected 1F."); }
		Console.WriteLine($"PASS: {Path.GetFileName(path)} legacy sprite/background capture, PNG composition and native restoration.");
	}
	private static void RunSms(string romPath, bool zoom = false, bool leftClip = false, bool columnMask = false, bool scroll = false, bool scrollLocks = false)
	{
		byte[] rom = new byte[32768];
		List<byte> code = new() { 0xF3, 0x31, 0xF0, 0xDF }; // DI; LD SP,$DFF0
		void Out(byte port, byte value) => code.AddRange(new byte[] { 0x3E, value, 0xD3, port });
		void Register(byte index, byte value) { Out(0xBF, value); Out(0xBF, (byte)(0x80 | index)); }
		void Address(ushort value) { Out(0xBF, (byte)value); Out(0xBF, (byte)(value >> 8)); }
		Register(1, 0); Register(0, columnMask ? (byte)0x24 : leftClip ? (byte)0x0C : (byte)4); Register(2, 0x0E); Register(5, 0x7E); Register(6, 0);
		Address(0x4000);
		// Clear all VRAM: LD BC,$4000; XOR A; OUT ($BE),A; DEC BC; LD A,B; OR C; JR NZ.
		code.AddRange(new byte[] { 0x01, 0x00, 0x40, 0xAF, 0xD3, 0xBE, 0x0B, 0x78, 0xB1, 0x20, 0xF8 });
		Address(0x4020); // Pattern 1, color index 1 in every pixel.
		for(int y = 0; y < 8; y++) { Out(0xBE, romPath.Contains("holes") ? (byte)0xFE : (byte)0xFF); Out(0xBE, 0); Out(0xBE, 0); Out(0xBE, 0); }
		Address(0x7F00); Out(0xBE, 7); Out(0xBE, 7); Out(0xBE, 0xD0);
		Address(0x7F80); Out(0xBE, leftClip || columnMask ? (byte)4 : (byte)8); Out(0xBE, 1); Out(0xBE, leftClip ? (byte)32 : (byte)24); Out(0xBE, 1);
		// Cover the second sprite with a nontransparent high-priority background tile.
		Address(0x7846); Out(0xBE, 1); Out(0xBE, 0x10);
		Address(0xC010); Out(0xBE, 0); Out(0xBE, 3); // Sprite color 1 is red.
		if(scroll) { Register(8, 5); Register(9, 218); }
		if(scrollLocks) { Register(0, 0xC4); }
		Register(1, zoom ? (byte)0x41 : (byte)0x40);
		code.AddRange(new byte[] { 0x18, 0xFE });
		code.CopyTo(rom, 0);
		File.WriteAllBytes(romPath, rom);
		uint result = RunMetaSpriteGameplayTest(romPath);
		uint expected = columnMask ? 127u : 63u;
		if(result != expected) { throw new Exception($"SMS gameplay integration failed: flags 0x{result:X}, expected 0x{expected:X}."); }
		Console.WriteLine($"PASS: {Path.GetFileName(romPath)}, live decoded capture, HD replacement pixels, vertical border placement, background priority, and disable restoration.");
	}
}

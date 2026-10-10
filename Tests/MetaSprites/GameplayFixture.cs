using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

internal static class GameplayFixture
{
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

	public static void Run(string dllPath, string romPath)
	{
		NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (name, _, _) => name == "MesenCore.dll" ? NativeLibrary.Load(dllPath) : IntPtr.Zero);
		foreach(int variant in new[] { 0, 1, 2 }) {
		bool color = variant > 0, banked = variant == 2;
		string gbPath = banked ? Path.ChangeExtension(romPath, "banked.gbc") : color ? Path.ChangeExtension(romPath, ".gbc") : romPath;
		byte[] rom = new byte[32768];
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
			code.AddRange(new byte[] { 0x3E, 0xFF, 0xEA, (byte)(0x10 + y * 2), 0x80 });
		}
		if(banked) { code.AddRange(new byte[] { 0xAF, 0xE0, 0x4F, 0x3E, 0x0B, 0xEA, 0x03, 0xFE }); }
		// OAM: Y=24 (screen 8), X=16 (screen 8), pattern 1, palette 0.
		code.AddRange(new byte[] { 0x3E, 24, 0xEA, 0x00, 0xFE, 0x3E, 16, 0xEA, 0x01, 0xFE, 0x3E, 1, 0xEA, 0x02, 0xFE });
		if(color) {
			// GBC palette RAM: blue backdrop (RGB555 0x7C00), red OBJ color 1 (0x001F).
			code.AddRange(new byte[] { 0x3E, 0x80, 0xE0, 0x68, 0xAF, 0xE0, 0x69, 0x3E, 0x7C, 0xE0, 0x69,
				0x3E, banked ? (byte)0x9A : (byte)0x82, 0xE0, 0x6A, 0x3E, 0x1F, 0xE0, 0x6B, 0xAF, 0xE0, 0x6B });
		}
		code.AddRange(new byte[] { 0x3E, 0xE4, 0xE0, 0x47, 0xE0, 0x48, 0x3E, 0x93, 0xE0, 0x40, 0x18, 0xFE });
		code.CopyTo(rom, 0x150);
		byte checksum = 0;
		for(int i = 0x134; i <= 0x14C; i++) { checksum = unchecked((byte)(checksum - rom[i] - 1)); }
		rom[0x14D] = checksum;
		File.WriteAllBytes(gbPath, rom);
		uint result = RunMetaSpriteGameplayTest(gbPath);
		if(result != 31) { throw new Exception($"Game Boy gameplay integration failed: flags 0x{result:X}, expected 0x1F."); }
		Console.WriteLine($"PASS: generated {(banked ? "GBC VRAM bank 1 / OBJ palette 3" : color ? "GBC" : "Game Boy")} ROM, live capture, all HD replacement pixels, transparent/partial-alpha backdrop, and disabling during emulation.");
		}
		RunSms(Path.ChangeExtension(romPath, ".sms"));
		RunSms(Path.ChangeExtension(romPath, "zoom.sms"), true);
		RunSms(Path.ChangeExtension(romPath, "sms1-zoom.sms"), true);
		RunSms(Path.ChangeExtension(romPath, "left.sms"), false, true);
		RunSms(Path.ChangeExtension(romPath, "mask.sms"), false, false, true);
		RunPce(Path.ChangeExtension(romPath, ".pce"));
		RunPce(Path.ChangeExtension(romPath, "wide.pce"), true);
		RunPce(Path.ChangeExtension(romPath, "divider3.pce"), false, 1);
		RunPce(Path.ChangeExtension(romPath, "divider2.pce"), false, 2);
		RunPce(Path.ChangeExtension(romPath, "mirror.pce"), false, 0, true);
		RunGba(Path.ChangeExtension(romPath, ".gba"));
		RunGba(Path.ChangeExtension(romPath, "mirror.gba"), true);
		RunGba(Path.ChangeExtension(romPath, "bpp8.gba"), false, true);
		RunGba(Path.ChangeExtension(romPath, "dark.gba"), dark: true);
		RunWs(Path.ChangeExtension(romPath, ".wsc"));
		RunWs(Path.ChangeExtension(romPath, "planar.wsc"), 0xC0);
		RunWs(Path.ChangeExtension(romPath, "packed.wsc"), 0xE0);
		RunNes(Path.ChangeExtension(romPath, ".nes"));
		RunSnes(Path.ChangeExtension(romPath, ".sfc"));
	}

	private static void RunSnes(string romPath)
	{
		byte[] rom = new byte[32768];
		List<byte> code = new() {0x78, 0x18, 0xFB, 0xC2, 0x10, 0xA2, 0xFF, 0x1F, 0x9A, 0xE2, 0x20};
		void Store(ushort address, byte value) => code.AddRange(new byte[] {0xA9, value, 0x8D, (byte)address, (byte)(address >> 8)});
		Store(0x2100, 0x80); Store(0x2101, 0); Store(0x2102, 0); Store(0x2103, 0);
		for(int sprite = 0; sprite < 128; sprite++) {
			Store(0x2104, 0); Store(0x2104, 240); Store(0x2104, 0); Store(0x2104, 0);
		}
		for(int index = 0; index < 32; index++) { Store(0x2104, 0); }
		Store(0x2102, 0); Store(0x2103, 0);
		Store(0x2104, 8); Store(0x2104, 8); Store(0x2104, 0); Store(0x2104, 0);
		Store(0x2115, 0x80); Store(0x2116, 0); Store(0x2117, 0);
		for(int word = 0; word < 16; word++) { Store(0x2118, word < 8 ? (byte)255 : (byte)0); Store(0x2119, 0); }
		Store(0x2121, 0); Store(0x2122, 0); Store(0x2122, 0x7C);
		Store(0x2121, 129); Store(0x2122, 31); Store(0x2122, 0);
		Store(0x212C, 0x10); Store(0x212D, 0); Store(0x2131, 0); Store(0x2133, 0); Store(0x2100, 15);
		ushort loop = (ushort)(0x8000 + code.Count); code.AddRange(new byte[] {0x4C, (byte)loop, (byte)(loop >> 8)});
		code.CopyTo(rom, 0);
		System.Text.Encoding.ASCII.GetBytes("METASPRITE TEST      ").CopyTo(rom, 0x7FC0);
		rom[0x7FD5] = 0x20; rom[0x7FD7] = 5; rom[0x7FD9] = 1;
		rom[0x7FDC] = 255; rom[0x7FDD] = 255; rom[0x7FFC] = 0; rom[0x7FFD] = 0x80;
		File.WriteAllBytes(romPath, rom);
		uint result = RunMetaSpriteSnesCaptureTest(romPath);
		if(result != 7) { throw new Exception($"SNES live capture/replacement failed: flags {result}."); }
		Console.WriteLine("PASS: generated SNES ROM, live capture, HD replacement, transparent/partial-alpha backdrop, border placement, and disable restoration.");
	}

	private static void RunNes(string romPath)
	{
		byte[] rom = new byte[16 + 16384 + 8192];
		new byte[] {0x4E, 0x45, 0x53, 0x1A, 1, 1}.CopyTo(rom, 0);
		List<byte> code = new() {0x78, 0xD8, 0xA2, 0xFF, 0x9A};
		void Store(ushort address, byte value) => code.AddRange(new byte[] {0xA9, value, 0x8D, (byte)address, (byte)(address >> 8)});
		Store(0x2000, 0); Store(0x2001, 0);
		code.AddRange(new byte[] {0x2C, 0x02, 0x20, 0x10, 0xFB, 0x2C, 0x02, 0x20, 0x10, 0xFB});
		// Hide all sprites, then give primary OAM sprite zero a known 8x8 graphic.
		code.AddRange(new byte[] {0xA2, 0, 0xA9, 0xFF, 0x9D, 0, 2, 0xE8, 0xD0, 0xFA});
		Store(0x0200, 7); Store(0x0201, 1); Store(0x0202, 0); Store(0x0203, 8);
		Store(0x2003, 0); Store(0x4014, 2);
		Store(0x2006, 0x3F); Store(0x2006, 0); Store(0x2007, 0);
		Store(0x2006, 0x3F); Store(0x2006, 0x11); Store(0x2007, 1);
		Store(0x2000, 0); Store(0x2001, 0x14);
		ushort loop = (ushort)(0x8000 + code.Count);
		code.AddRange(new byte[] {0x4C, (byte)loop, (byte)(loop >> 8)});
		code.CopyTo(rom, 16);
		for(int vector = 0x3FFA; vector < 0x4000; vector += 2) { rom[16 + vector] = 0; rom[16 + vector + 1] = 0x80; }
		for(int row = 0; row < 8; row++) { rom[16 + 16384 + 16 + row] = 0xFF; }
		File.WriteAllBytes(romPath, rom);
		uint result = RunMetaSpriteNesCaptureTest(romPath);
		if(result != 7) { throw new Exception($"NES live capture/replacement failed: flags {result}."); }
		Console.WriteLine("PASS: generated NES ROM, live capture, HD replacement, transparent backdrop, and disable restoration.");
	}

	private static void RunWs(string romPath, byte videoMode = 0x80)
	{
		byte[] rom = new byte[65536];
		List<byte> code = new() {0xFA, 0x31, 0xC0, 0x8E, 0xD8}; // CLI; AX=0; DS=0.
		void Port(byte port, byte value) => code.AddRange(new byte[] {0xB0, value, 0xE6, port});
		void Word(ushort address, ushort value) => code.AddRange(new byte[] {0xC7, 0x06, (byte)address, (byte)(address >> 8), (byte)value, (byte)(value >> 8)});
		Port(0, 0); Port(0x60, videoMode); Port(0x14, 1); Port(0x16, 158);
		Word(0xFE00, 0x000F); Word(0xFF02, 0x0F00);
		for(ushort row = 0; row < 8; row++) {
			if(videoMode == 0x80) { Word((ushort)(0x2000 + row * 2), 0x00FF); }
			else {
				Word((ushort)(0x4000 + row * 4), videoMode == 0xE0 ? (ushort)0x1111 : (ushort)0x00FF);
				Word((ushort)(0x4002 + row * 4), videoMode == 0xE0 ? (ushort)0x1111 : (ushort)0);
			}
		}
		Word(0x1000, 0); Word(0x1002, 0x0808);
		Port(4, 8); Port(5, 0); Port(6, 1); Port(1, 0); Port(0, 4);
		code.AddRange(new byte[] {0xEB, 0xFE});
		code.CopyTo(rom, 0);
		new byte[] {0xEA, 0, 0, 0, 0xF0}.CopyTo(rom, 0xFFF0);
		rom[0xFFF7] = 1;
		File.WriteAllBytes(romPath, rom);
		uint result = RunMetaSpriteWsCaptureTest(romPath);
		if(result != 7) { throw new Exception($"WonderSwan live capture/replacement failed: flags {result}."); }
		Console.WriteLine($"PASS: generated WonderSwan ROM (mode 0x{videoMode:X}), live capture, HD replacement, transparent backdrop, and disable restoration.");
	}

	private static void RunGba(string romPath, bool mirror = false, bool bpp8 = false, bool dark = false)
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
		for(uint sprite = 0; sprite < 128; sprite++) { Store(0x07000000 + sprite * 8, 0x0200); }
		Store(0x05000202, 0x001F);
		Store(0x05000000, 0x7C00); // Known backdrop for object-removal fallback.
		Store(0x05000204, 0x7C00);
		for(uint row = 0; row < 8; row++) {
			if(bpp8) {
				Store(0x06010000 + row * 8, 0x01010101, true);
				Store(0x06010004 + row * 8, 0x02020202, true);
			} else { Store(0x06010000 + row * 4, mirror ? 0x22221111u : 0x11111111u, true); }
		}
		Store(0x07000000, bpp8 ? 0x2008u : 8u); Store(0x07000002, mirror ? 0x1008u : 8u); Store(0x07000004, 0);
		if(dark) { Store(0x04000050, 0x00F0); Store(0x04000054, 8); } // Darken objects and backdrop by half.
		Store(0x04000000, 0x1040);
		instructions.Add(0xEAFFFFFE); // Stable display loop.
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
		Console.WriteLine($"PASS: generated GBA ROM ({(dark ? "brightness effects" : mirror ? "horizontal flip" : bpp8 ? "8-bit palette" : "basic")}), live capture, HD replacement, transparent/partial-alpha backdrop, and disable restoration.");
	}

	private static void RunPce(string romPath, bool wide = false, byte clockMode = 0, bool mirror = false)
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
				: word % 64 < 16 ? (ushort)0xFFFF : (ushort)0);
		}
		Register(0, 0x2000); code.AddRange(new byte[] { 0x03, 2 });
		Word(72); Word(40); Word(0x80); Word(mirror ? (ushort)0x880 : wide ? (ushort)0x180 : (ushort)0x80);
		for(int word = 4; word < 256; word++) { Word(0); }
		Store(0x0402, 1); Store(0x0403, 1); Store(0x0404, 0x38); Store(0x0405, 0);
		if(mirror) { Store(0x0404, 7); Store(0x0405, 0); }
		Register(0x0F, 0x10); Register(0x13, 0x2000); Register(5, 0x40);
		code.AddRange(new byte[] { 0x80, 0xFE });
		code.CopyTo(rom, 0); rom[0x1FFE] = 0; rom[0x1FFF] = 0xE0;
		File.WriteAllBytes(romPath, rom);
		uint result = RunMetaSpritePceGameplayTest(romPath);
		if(result != 31) { throw new Exception($"PCE gameplay integration failed: flags 0x{result:X}, expected 0x1F."); }
		Console.WriteLine($"PASS: {Path.GetFileName(romPath)}, live fetched capture, crop alignment, HD replacement, background preservation, and disable restoration.");
	}

	private static void RunSms(string romPath, bool zoom = false, bool leftClip = false, bool columnMask = false)
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
		for(int y = 0; y < 8; y++) { Out(0xBE, 0xFF); Out(0xBE, 0); Out(0xBE, 0); Out(0xBE, 0); }
		Address(0x7F00); Out(0xBE, 7); Out(0xBE, 7); Out(0xBE, 0xD0);
		Address(0x7F80); Out(0xBE, leftClip || columnMask ? (byte)4 : (byte)8); Out(0xBE, 1); Out(0xBE, leftClip ? (byte)32 : (byte)24); Out(0xBE, 1);
		// Cover the second sprite with a nontransparent high-priority background tile.
		Address(0x7846); Out(0xBE, 1); Out(0xBE, 0x10);
		Address(0xC010); Out(0xBE, 0); Out(0xBE, 3); // Sprite color 1 is red.
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

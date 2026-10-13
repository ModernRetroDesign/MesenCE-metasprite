using System;
using System.IO;
using System.Linq;
using Mesen.Debugger.Utilities;

static void Check(bool condition, string message)
{
	if(!condition) { throw new Exception(message); }
}

static MetaSpriteMember Sprite(int x, int y, uint color) => new() { X = x, Y = y, Width = 1, Height = 1, Pixels = new[] { color } };

var memoryConditions = new System.Collections.Generic.List<MetaMemoryCondition> {
	new() { Address = 0xC000, Value = 3, Mask = 15 }, new() { Address = 0xC001, Value = 7 }
};
Check(MetaMemoryCondition.Matches(memoryConditions, address => address == 0xC000 ? (byte)0xA3 : (byte)7), "All masked CPU-byte conditions must match.");
Check(!MetaMemoryCondition.Matches(memoryConditions, address => address == 0xC000 ? (byte)0xA3 : (byte)8), "A single mismatched CPU byte must suppress artwork.");
Check(!MetaMemoryCondition.Matches(memoryConditions, _ => null), "Missing memory snapshots must suppress conditioned artwork.");
Check(MetaMemoryCondition.Matches(new(), _ => null), "Old packs without conditions remain unconditional.");
var conditionPack = new MetaSpritePack {
	Groups = new() { new() { MemoryConditions = memoryConditions, AlternateArtwork = new() {
		new() { Path = "powered.png", Scale = 2, OriginX = 3, OriginY = 9, MemoryConditions = memoryConditions }
	} } },
	BackgroundLayers = new() { new() { MemoryConditions = memoryConditions } }
};
string conditionJson = System.Text.Json.JsonSerializer.Serialize(conditionPack, MetaSpriteSerializerContext.Default.MetaSpritePack);
var restoredConditions = System.Text.Json.JsonSerializer.Deserialize(conditionJson, MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(restoredConditions.Groups[0].AlternateArtwork[0].Path == "powered.png" && restoredConditions.Groups[0].AlternateArtwork[0].OriginY == 9
	&& restoredConditions.Groups[0].MemoryConditions[0].Mask == 15 && restoredConditions.BackgroundLayers[0].MemoryConditions[1].Address == 0xC001,
	"CPU conditions and alternate origins must survive pack serialization.");

MetaSpriteGroup group = new() { Id = 7, Name = "Standing", Width = 2, Height = 1, OriginX = 1, OriginY = 1 };
group.Enabled = true;
MetaSpriteGroup pose = new() { Id = 8, Width = 6, Height = 5 };
pose.LinkAsPoseOf(group);
pose.PoseName = "Walking";
Check(group.PoseFamilyId == 7 && pose.PoseFamilyId == 7, "Animation poses must share a persistent family.");
Check(pose.OriginX == 3 && pose.OriginY == 5, "Different pose bounds must preserve the bottom-center anchor offset.");
pose.ReplacementScale = 4;
pose.UseExportedArtwork("pose.png");
Check(pose.Enabled && pose.ReplacementPath == "pose.png" && pose.ReplacementWidth == 6 && pose.ReplacementHeight == 5
	&& pose.ReplacementScale == 1 && pose.ReplacementOriginX == pose.OriginX && pose.ReplacementOriginY == pose.OriginY,
	"Newly exported poses must enable gameplay and use their native canvas and consistent anchor.");
var anchor = new MetaSpriteGroup { Id = 20, PoseFamilyId = 20, Width = 9, Height = 12, OriginX = 2, OriginY = 10 };
var sibling = new MetaSpriteGroup { Id = 21, PoseFamilyId = 20, Width = 14, Height = 8, ReplacementOriginX = 9, ReplacementOriginY = 11 };
var unrelated = new MetaSpriteGroup { Id = 22, PoseFamilyId = 22, OriginX = 3, OriginY = 4 };
var alignmentPack = new MetaSpritePack { Groups = new() { anchor, sibling, unrelated } };
Check(MetaBackgroundLayer.ResolveGameboyRole(0, true, false) == 1 && MetaBackgroundLayer.ResolveGameboyRole(1, true, false) == 0,
	"Game Boy map tabs must resolve current hardware roles rather than assuming map index equals layer.");
Check(MetaBackgroundLayer.ResolveGameboyRole(0, false, false) == 0 && MetaBackgroundLayer.ResolveGameboyRole(1, false, false) == null,
	"Shared Game Boy maps prefer background, and unused maps must not silently target a role.");
Check(alignmentPack.AlignPoseOrigins(anchor) == 1 && sibling.OriginX == 5 && sibling.OriginY == 6,
	"Editing a family anchor must preserve its bottom-center offset across different pose bounds.");
Check(unrelated.OriginX == 3 && unrelated.OriginY == 4 && sibling.ReplacementOriginX == 9 && sibling.ReplacementOriginY == 11,
	"Pose alignment must preserve other families and independent artwork anchors.");
Check(alignmentPack.AlignPoseOrigins(new MetaSpriteGroup { PoseFamilyId = 20 }) == 0,
	"An external group must not change a pack's pose origins.");
anchor.ReplacementPath = "anchor.png"; anchor.ReplacementWidth = 24; anchor.ReplacementHeight = 32;
anchor.ReplacementScale = 2; anchor.ReplacementOriginX = 4; anchor.ReplacementOriginY = 13;
sibling.ReplacementPath = "sibling.png"; sibling.ReplacementWidth = 80; sibling.ReplacementHeight = 40; sibling.ReplacementScale = 4;
Check(alignmentPack.AlignPoseArtworkOrigins(anchor) == 1 && sibling.ReplacementOriginX == 8 && sibling.ReplacementOriginY == 7,
	"PNG pose anchors must preserve native bottom-center offsets across independent canvas sizes and scales.");
Check(anchor.ReplacementOriginX == 4 && anchor.ReplacementOriginY == 13 && sibling.OriginX == 5 && sibling.OriginY == 6
	&& unrelated.OriginX == 3 && unrelated.OriginY == 4, "PNG alignment must preserve the source anchor, native anchors and other families.");
Check(alignmentPack.AlignPoseArtworkOrigins(new MetaSpriteGroup { PoseFamilyId = 20 }) == 0,
	"An external group must not align PNG anchors.");
group.Members.Add(Sprite(0, 0, 0xFF102030));
group.Members.Add(Sprite(1, 0, 0xFF405060));
var candidates = new[] { Sprite(101, 50, 0xFF405060), Sprite(100, 50, 0xFF102030), Sprite(200, 60, 0xFF102030), Sprite(201, 60, 0xFF405060) };
var matches = MetaSpriteMatcher.FindMatches(group, candidates);
group.ReplacementOriginX = 4;
group.ReplacementOriginY = 6;
Check(MetaSpritePlacement.GetTopLeft(group, 101, 51, 3) == (291, 135), "Larger replacement artwork must align its own origin at output scale.");
MetaSpritePlacement.ValidateCanvas(48, 64, 2);
MetaSpritePlacement.ValidateSpriteCanvas(49, 65, 2);
MetaSpritePlacement.ValidateSpriteCanvas(1, 1, 10);
bool badScale = false;
try { MetaSpritePlacement.ValidateCanvas(49, 64, 2); } catch(InvalidDataException) { badScale = true; }
Check(badScale, "Background wrapping canvases must retain whole native dimensions.");
Check(matches.Count == 2, "Must recognize two instances with reordered hardware indices.");
Check(matches[0].X == 101 && matches[0].Y == 51 && matches[0].Members.SequenceEqual(new[] { 1, 0 }), "Origin or member mapping incorrect.");
Check(MetaSpriteMatcher.FindMatches(group, new[] { candidates[0] }).Count == 0, "Partial groups must not match.");
Check(MetaSpriteMatcher.FindMatches(group, new[] { candidates[1], Sprite(102, 50, 0xFF405060) }).Count == 0, "Incorrect arrangement must not match.");
Check(MetaSpriteMatcher.FindMatches(group, new[] { candidates[1], Sprite(101, 50, 0xFF405061) }).Count == 0, "Changed graphic must not match.");
var wrappedCandidates = new[] { Sprite(100, 50, 0xFF102030), Sprite(101, 50, 0xFF405060) };
wrappedCandidates[0].RuntimeSpriteIndex = 3;
wrappedCandidates[1].RuntimeSpriteIndex = 3;
Check(MetaSpriteMatcher.FindMatches(group, wrappedCandidates).Count == 0, "Wrapped copies of one hardware sprite must not satisfy two members.");
wrappedCandidates[1].RuntimeSpriteIndex = 12;
Check(MetaSpriteMatcher.FindMatches(group, wrappedCandidates).Count == 1, "Distinct hardware sprites must match regardless of their indices.");

string directory = Path.Combine(Path.GetTempPath(), "mesen-meta-tests-" + Guid.NewGuid().ToString("N"));
string path = Path.Combine(directory, "groups.json");
try {
	MetaSpritePack pack = new() { RomHash = "test-rom", System = "Gameboy", NextGroupId = 1 };
	pack.Groups.Add(group);
	var background = new MetaBackgroundLayer { Id = 1, NativeLayer = 1, ArtworkPath = "background.png", ArtworkWidth = 64, ArtworkHeight = 32, ArtworkScale = 2, X = 3, Y = 5, WrapX = true };
	pack.BackgroundLayers.Add(background);
	pack.Save(path);
	Check(!File.ReadAllText(path).Contains("RuntimeSpriteIndex"), "Transient hardware indices must never enter saved recognition definitions.");
	var loaded = MetaSpritePack.Load(path, "test-rom", "Gameboy");
	Check(loaded.BackgroundLayers.Count == 1 && loaded.BackgroundLayers[0].ArtworkPath == "background.png" && loaded.BackgroundLayers[0].NativeLayer == 1,
		"Background layer definitions must persist per game with artwork and hardware layer selection.");
	Check(loaded.NextBackgroundId == 2, "Older background packs must allocate above existing IDs.");
	loaded.NextBackgroundId = 7;
	loaded.BackgroundLayers.Clear();
	loaded.Save(path);
	Check(MetaSpritePack.Load(path, "test-rom", "Gameboy").NextBackgroundId == 7,
		"Removing backgrounds must not reuse IDs or overwrite retained artwork on the next import.");
	Check(background.TryGetArtworkPixel(0, 0, 0, 5, out int bx, out int by) && bx == 58 && by == 0,
		"Negative horizontal scroll coordinates must wrap within the scaled PNG canvas.");
	Check(!background.TryGetArtworkPixel(0, 0, 0, 4, out _, out _), "Unwrapped vertical coordinates must clip outside the artwork.");
	background.FollowScroll = false;
	Check(background.TryGetArtworkPixel(3, 5, int.MaxValue, int.MinValue, out bx, out by) && bx == 0 && by == 0,
		"Screen-locked layers must ignore hardware scrolling.");
	background.FollowScroll = true;
	background.WrapY = true;
	Check(background.TryGetArtworkPixel(int.MaxValue, int.MinValue, int.MaxValue, int.MinValue, out bx, out by) && bx >= 0 && bx < 64 && by >= 0 && by < 32,
		"Wrapping must remain valid for extreme signed scroll values without overflow.");
	for(int sy = 0; sy < 3; sy++) {
		for(int sx = 0; sx < 3; sx++) {
			Check(background.TryGetArtworkPixel(3, 5, 0, 0, sx, sy, 3, out bx, out by) && bx == sx * 2 / 3 && by == sy * 2 / 3,
				"Artwork detail must use nearest samples when output scale differs from PNG scale.");
		}
	}
	Check(background.TryGetArtworkPixel(2, 4, 0, 0, 1, 1, 2, out bx, out by) && bx == 63 && by == 31,
		"HD samples at a negative wrapped seam must reach the last PNG pixel.");
	bool invalidSample = false;
	try { background.TryGetArtworkPixel(0, 0, 0, 0, 2, 0, 2, out _, out _); } catch(ArgumentOutOfRangeException) { invalidSample = true; }
	Check(invalidSample, "Subpixels outside the output pixel must be rejected.");
	Check(loaded.NextGroupId == 8, "IDs must stay above saved IDs.");
	Check(loaded.Groups[0].Enabled, "Enabled state must persist for automatic game-load application.");
	Check(loaded.Groups[0].PoseFamilyId == 7, "Pose families must persist with reflection-free serialization.");
	pose.Members.Add(Sprite(0, 0, 0xFF112233));
	pack.Groups.Add(pose);
	pack.Save(path);
	var posePack = MetaSpritePack.Load(path, "test-rom", "Gameboy");
	Check(posePack.Groups[1].PoseName == "Walking" && posePack.Groups[1].PoseFamilyId == 7 && posePack.Groups[1].OriginY == 5,
		"Named animation poses and their independent anchors must survive save/load.");
	pack.Groups.Remove(pose);
	Check(loaded.Groups[0].OriginY == 1 && loaded.Groups[0].Members[1].Pixels[0] == 0xFF405060, "Snapshot must round-trip with reflection disabled.");
	bool rejected = false;
	try { MetaSpritePack.Load(path, "other-rom", "Gameboy"); } catch(InvalidDataException) { rejected = true; }
	Check(rejected, "Must reject a different ROM.");
	group.ReplacementPath = "meta-007-replacement.png";
	group.ReplacementWidth = 0;
	group.ReplacementOriginX = 0;
	group.ReplacementOriginY = 0;
	pack.Save(path);
	var migrated = MetaSpritePack.Load(path, "test-rom", "Gameboy");
	Check(migrated.Groups[0].ReplacementOriginX == group.OriginX && migrated.Groups[0].ReplacementOriginY == group.OriginY,
		"Earlier replacement definitions must retain their original alignment.");
	group.ReplacementWidth = 8;
	group.ReplacementHeight = 8;
	group.ReplacementOriginX = 4;
	group.ReplacementOriginY = 6;
	pack.Save(path);
	loaded = MetaSpritePack.Load(path, "test-rom", "Gameboy");
	Check(loaded.Groups[0].ReplacementOriginX == 4 && loaded.Groups[0].ReplacementOriginY == 6, "Independent replacement origins must persist.");
	group.Members[0].Pixels = Array.Empty<uint>();
	pack.Save(path);
	rejected = false;
	try { MetaSpritePack.Load(path, "test-rom", "Gameboy"); } catch(InvalidDataException) { rejected = true; }
	Check(rejected, "Must reject corrupt pixel buffers before rendering.");
} finally {
	if(File.Exists(path)) { File.Delete(path); }
	if(Directory.Exists(directory)) { Directory.Delete(directory); }
}
Console.WriteLine("PASS: instance matching, independent artwork origins and scale, origin migration, reordered indices, incomplete/incorrect groups, persistence, ID allocation, ROM isolation, corrupt data rejection.");

MetaSpritePack runtimePack = new();
var editedConditions = MetaMemoryConditionText.Parse("$C000 = $01 / $0F\r\n0xC001 = 0xA0\nFFFFFFFF = FF / 00");
var observableAlternate = new MetaAlternateArtwork();
var alternateNotifications = new System.Collections.Generic.List<string?>();
observableAlternate.PropertyChanged += (_, e) => alternateNotifications.Add(e.PropertyName);
observableAlternate.Scale = 2; observableAlternate.OriginX = 3; observableAlternate.OriginY = -2;
observableAlternate.Scale = 2; observableAlternate.OriginX = 3; observableAlternate.OriginY = -2;
Check(alternateNotifications.SequenceEqual(new[] { "Scale", "OriginX", "OriginY" }), "Alternate preview bindings must observe changes without redundant notifications.");
var observableBackground = new MetaBackgroundLayer();
var backgroundNotifications = new System.Collections.Generic.List<string?>();
observableBackground.PropertyChanged += (_, e) => backgroundNotifications.Add(e.PropertyName);
observableBackground.ArtworkScale = 2; observableBackground.X = 3; observableBackground.Y = -2;
observableBackground.ArtworkScale = 2; observableBackground.X = 3; observableBackground.Y = -2;
Check(backgroundNotifications.SequenceEqual(new[] { "ArtworkScale", "X", "Y" }), "Background placement edits must notify the preview only when values change.");
Check(editedConditions.Count == 3 && editedConditions[0].Address == 0xC000 && editedConditions[0].Mask == 15
	&& editedConditions[1].Value == 160 && editedConditions[1].Mask == 255 && editedConditions[2].Address == uint.MaxValue,
	"Condition editor must parse hex prefixes, optional masks, and 32-bit addresses.");
var editedRoundtrip = MetaMemoryConditionText.Parse(MetaMemoryConditionText.Format(editedConditions));
Check(editedRoundtrip.Count == 3 && editedRoundtrip[2].Mask == 0 && MetaMemoryConditionText.Parse(" \r\n").Count == 0, "Condition editor must preserve conditions and allow clearing.");
foreach(string invalidCondition in new[] { "C000", "C000 = 100", "C000 = 01 / 100", "100000000 = 01", "C000 = GG", "C000 = 01 / FF extra",
	string.Join('\n', Enumerable.Repeat("C000 = 01", 65)) }) {
	bool invalidEditorInput = false;
	try { MetaMemoryConditionText.Parse(invalidCondition); } catch(InvalidDataException) { invalidEditorInput = true; }
	Check(invalidEditorInput, "Condition editor must reject invalid input before saving.");
}
var runtimeGroup = new MetaSpriteGroup { Id = 22, Width = 8, Height = 8, Enabled = true };
runtimeGroup.Members.Add(new() { Width = 8, Height = 8, Pixels = Enumerable.Repeat(0xFFFF0000u, 64).ToArray() });
runtimePack.Groups.Add(runtimeGroup);
runtimePack.Groups.Add(new() { Id = 23, Enabled = false });
byte[] runtimePacket = MetaSpriteRuntimePacket.Create(runtimePack, _ => (8, 8, Enumerable.Repeat(0xFF00FF00u, 64).ToArray()));
using(var reader = new BinaryReader(new MemoryStream(runtimePacket))) {
	Check(reader.ReadUInt32() == MetaSpriteRuntimePacket.Magic && reader.ReadInt32() == 1 && reader.ReadInt32() == 22,
		"Runtime packet must contain only enabled groups and use the agreed little-endian header.");
}
if(args.Length > 0) { File.WriteAllBytes(args[0], runtimePacket); }
runtimeGroup.MemoryConditions = new() {
	new() { Address = 0x1234, Value = 0xAF, Mask = 0xF0 },
	new() { Address = 0x5678, Value = 3 }
};
byte[] conditionalPacket = MetaSpriteRuntimePacket.Create(runtimePack, _ => (8, 8, Enumerable.Repeat(0xFF00FF00u, 64).ToArray()));
using(var reader = new BinaryReader(new MemoryStream(conditionalPacket))) {
	Check(reader.ReadUInt32() == MetaSpriteRuntimePacket.ConditionalMagic, "Conditioned groups require MSP2.");
	reader.BaseStream.Position = conditionalPacket.Length - 28;
	Check(reader.ReadInt32() == 2 && reader.ReadUInt32() == 0x1234 && reader.ReadInt32() == 0xAF && reader.ReadInt32() == 0xF0
		&& reader.ReadUInt32() == 0x5678 && reader.ReadInt32() == 3 && reader.ReadInt32() == 255, "Condition bytes must survive packet transfer.");
}
Check(MetaMemoryCondition.Matches(runtimeGroup.MemoryConditions, address => address == 0x1234 ? (byte)0xA7 : (byte)3), "Masked expected bits must match native semantics.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".conditions", conditionalPacket); }
runtimeGroup.MemoryConditions.Clear();
runtimeGroup.AlternateArtwork.Add(new() { Path = "draft.png" });
runtimeGroup.AlternateArtwork[0].Validate();
byte[] draftPacket = MetaSpriteRuntimePacket.Create(runtimePack,
	_ => (8, 8, Enumerable.Repeat(0xFF00FF00u, 64).ToArray()), _ => throw new Exception("Inactive alternate must not decode its PNG."));
Check(BitConverter.ToUInt32(draftPacket, 0) == MetaSpriteRuntimePacket.Magic, "Conditionless alternate drafts must not enter gameplay packets.");
runtimeGroup.AlternateArtwork.Clear();
runtimeGroup.AlternateArtwork.Add(new() { Path = "alternate.png", Scale = 2, OriginX = 1, OriginY = 2,
	MemoryConditions = new() { new() { Address = 0x2345, Value = 7 } } });
byte[] alternatePacket = MetaSpriteRuntimePacket.Create(runtimePack,
	_ => (8, 8, Enumerable.Repeat(0xFF00FF00u, 64).ToArray()),
	_ => (16, 16, Enumerable.Repeat(0xFFFFFFFFu, 256).ToArray()));
Check(BitConverter.ToUInt32(alternatePacket, 0) == MetaSpriteRuntimePacket.AlternateMagic, "Alternate PNGs require MSP3.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".alternates", alternatePacket); }
int previousArtworkScale = runtimeGroup.ReplacementScale;
runtimeGroup.ReplacementScale = 2;
byte[] partialCanvasPacket = MetaSpriteRuntimePacket.Create(runtimePack,
	_ => (17, 19, Enumerable.Repeat(0xFF00FF00u, 17 * 19).ToArray()),
	_ => (19, 21, Enumerable.Repeat(0xFFFFFFFFu, 19 * 21).ToArray()));
Check(BitConverter.ToUInt32(partialCanvasPacket, 0) == MetaSpriteRuntimePacket.AlternateMagic, "Partial native canvases must retain default/alternate packet compatibility.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".partial", partialCanvasPacket); }
runtimeGroup.ReplacementScale = previousArtworkScale;
runtimeGroup.AlternateArtwork.Clear();
var bgTransfer = new MetaSpritePack();
bgTransfer.BackgroundLayers.Add(new() { Id = 8, NativeLayer = 1, X = -3, Y = 5, Enabled = true, ArtworkWidth = 2, ArtworkHeight = 2, ArtworkScale = 2, WrapX = true });
Check(MetaBackgroundRuntimePacket.Create(bgTransfer, _ => throw new Exception("Conditionless backgrounds must not load PNG.")).Length == 8,
	"Enabled backgrounds without configured CPU checks must serialize a clear packet.");
bgTransfer.BackgroundLayers[0].MemoryConditions = new() { new() { Address = 0xC123, Value = 0xA7, Mask = 0xF0 } };
byte[] conditionedBackground = MetaBackgroundRuntimePacket.Create(bgTransfer, _ => (2, 2, new uint[] {0, 0x8000FF00, 0xFFFF0000, 0xFFFFFFFF}));
// Exercise the native legacy decoder separately from user-pack activation policy.
byte[] bgPacket = conditionedBackground.AsSpan(0, conditionedBackground.Length - 16).ToArray();
BitConverter.GetBytes(MetaBackgroundRuntimePacket.Magic).CopyTo(bgPacket, 0);
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background", bgPacket); }
using(var reader = new BinaryReader(new MemoryStream(conditionedBackground))) {
	Check(reader.ReadUInt32() == MetaBackgroundRuntimePacket.ConditionalMagic, "Conditioned backgrounds require MBG2.");
	reader.BaseStream.Position = conditionedBackground.Length - 16;
	Check(reader.ReadInt32() == 1 && reader.ReadUInt32() == 0xC123 && reader.ReadInt32() == 0xA7 && reader.ReadInt32() == 0xF0, "Background condition bytes must survive transfer.");
}
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.conditions", conditionedBackground); }
bgTransfer.BackgroundLayers[0].Enabled = false;
Check(MetaBackgroundRuntimePacket.Create(bgTransfer, _ => throw new Exception("Disabled background must not load PNG.")).Length == 8, "Disabled backgrounds must serialize a clear packet.");
Console.WriteLine("PASS: enabled-group packet serialization.");
var priorityPack = new MetaSpritePack { Groups = new() {
	new MetaSpriteGroup { Id = 90, Enabled = true },
	new MetaSpriteGroup { Id = 12, Enabled = false },
	new MetaSpriteGroup { Id = 3, Enabled = true }
} };
var loadedOrder = new System.Collections.Generic.List<int>();
MetaSpriteRuntimePacket.Create(priorityPack, g => { loadedOrder.Add(g.Id); return (1, 1, new[] { 0xFFFFFFFFu }); });
Check(loadedOrder.SequenceEqual(new[] { 90, 3 }), "Gameplay priority must follow pack order rather than numerical group IDs, excluding disabled groups.");
priorityPack.Groups.Reverse();
loadedOrder.Clear();
MetaSpriteRuntimePacket.Create(priorityPack, g => { loadedOrder.Add(g.Id); return (1, 1, new[] { 0xFFFFFFFFu }); });
Check(loadedOrder.SequenceEqual(new[] { 3, 90 }), "Reordering definitions must update gameplay priority.");
runtimeGroup.Enabled = false;
byte[] disabledPacket = MetaSpriteRuntimePacket.Create(runtimePack, _ => throw new Exception("Disabled groups must not load PNG assets."));
Check(disabledPacket.Length == 8 && BitConverter.ToInt32(disabledPacket, 4) == 0, "An all-disabled pack must clear native definitions without reading artwork.");
Console.WriteLine("PASS: all-disabled pack clears runtime definitions without loading PNG assets.");
if(args.Length >= 3 && args[1] == "--snes-hires") {
	GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
}
if(args.Length >= 3 && args[1] == "--snes-hires-canvas") {
	GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), includeInterlace: false);
}
if(args.Length >= 3 && args[1] == "--snes-hires-weave") {
	GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), includeObjInterlace: false);
}
if(args.Length >= 3 && args[1] == "--snes-ordering") {
	GameplayFixture.RunSnesOrdering(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
	GameplayFixture.VerifyPackActivation(Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.holes-subscreen.sfc")), Path.GetFullPath(args[0]));
}
if(args.Length >= 3 && args[1] == "--snes-canvas") {
	GameplayFixture.RunSnesCanvas(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
}
if(args.Length >= 3 && (args[1] == "--snes-frame-timing" || args[1] == "--snes-viewer-core-timing")) {
	GameplayFixture.RunSnesFrameTiming(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), args[1] == "--snes-viewer-core-timing");
}
if(args.Length >= 3 && args[1] == "--gameplay-nes") {
	GameplayFixture.RunNesSuite(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
}
if(args.Length >= 3 && args[1] == "--gameplay-ws") {
	GameplayFixture.RunWsSuite(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
}
if(args.Length >= 3 && args[1] == "--pce-priority") {
	GameplayFixture.RunPcePrioritySuite(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.pce")));
}
if(args.Length >= 3 && args[1] == "--gba-affine") {
	GameplayFixture.RunGbaAffine(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gba")));
}
if(args.Length >= 3 && args[1] == "--gba-capture-metadata") {
	GameplayFixture.RunGbaCaptureMetadata(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gba")));
}
if(args.Length >= 3 && args[1] == "--gameplay") {
	GameplayFixture.Run(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
	GameplayFixture.VerifyPackActivation(Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), Path.GetFullPath(args[0]));
}
if(args.Length >= 3 && args[1] == "--viewer-refresh") {
	GameplayFixture.RunViewerRefreshSuite(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
}
if(args.Length >= 2 && args[1] == "--matcher-benchmark") { MatcherBenchmark.Run(); }

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Mesen.Debugger.Utilities;

static void Check(bool condition, string message)
{
	if(!condition) { throw new Exception(message); }
}

static MetaSpriteMember Sprite(int x, int y, uint color) => new() { X = x, Y = y, Width = 1, Height = 1, Pixels = new[] { color } };

MetaTextTests.Run(args.Length>0 ? args[0] : null);

// Exercise the same forward-only stream used by both native packet formats.
{
	string? spill=null;
	using(var packet=new MetaReplacementPacketStream(64,8)) {
		packet.Write(new byte[] {1,2,3}); Check(packet.SpillPath==null,"Small packet builders must remain in memory.");
		packet.Write(new byte[] {4,5,6,7,8}); Check(packet.SpillPath==null && packet.Length==8,"Exact spill threshold must remain in memory.");
		packet.WriteByte(9); spill=packet.SpillPath;
		Check(spill!=null && packet.Length==9 && packet.Position==9,"The next byte must spill without changing stream position.");
		Check(packet.ToArray().SequenceEqual(Enumerable.Range(1,9).Select(v => (byte)v)),"Spill copy must preserve existing bytes and append order.");
		using(var writer=new BinaryWriter(packet,System.Text.Encoding.UTF8,true)) { MetaReplacementPacketStream.WritePixels(writer,new[] {0x80ABCDEFu,0x01020304u}); }
		byte[] bytes=packet.ToArray();
		Check(bytes.Skip(9).SequenceEqual(new byte[] {0xEF,0xCD,0xAB,0x80,4,3,2,1}),"Bulk RGBA transport must preserve little-endian bytes and straight alpha.");
		packet.Write(new byte[64-bytes.Length]); Check(packet.Length==64,"Exact packet limit must be accepted.");
		bool rejected=false; try { packet.WriteByte(1); } catch(InvalidDataException) { rejected=true; }
		Check(rejected && packet.Length==64 && packet.ToArray().Length==64,"Over-limit writes must fail before publishing or growing storage.");
	}
	Check(!File.Exists(spill),"Spilled packet temporary file must be removed on disposal.");
	string? failedSpill=null;
	try {
		using var packet=new MetaReplacementPacketStream(16,0); packet.WriteByte(1); failedSpill=packet.SpillPath;
		packet.Write(new byte[16]); throw new Exception("Over-limit packet was accepted.");
	} catch(InvalidDataException) { }
	Check(failedSpill!=null && !File.Exists(failedSpill),"Failed packet construction must also clean its temporary file.");
	using(var bounded=new MetaReplacementPacketStream(3,8)) {
		bounded.Write(new byte[] {1,2,3}); bool rejected=false;
		try { bounded.Write(new byte[] {4,5}); } catch(InvalidDataException) { rejected=true; }
		Check(rejected && bounded.SpillPath==null && bounded.ToArray().SequenceEqual(new byte[] {1,2,3}),"Limit checks must also protect in-memory builders.");
	}
	using var golden=new MemoryStream(); using var reference=new BinaryWriter(golden,System.Text.Encoding.UTF8,true);
	using var actual=new MetaReplacementPacketStream(1024,7); using var actualWriter=new BinaryWriter(actual,System.Text.Encoding.UTF8,true);
	foreach(var writer in new[] {reference,actualWriter}) { writer.Write(0x3550534Du); writer.Write(-4096); writer.Write("packet"); MetaReplacementPacketStream.WritePixels(writer,new[] {0u,0x80FFFFFFu}); }
	Check(actual.ToArray().SequenceEqual(golden.ToArray()),"Spilled output must match existing BinaryWriter packet encoding exactly.");
	using(var capacity=new MetaReplacementPacketStream()) {
		capacity.Write(new byte[3*1024*1024]); capacity.WriteByte(1);
		Check(capacity.MemoryCapacity<=4*1024*1024,"Non-power-of-two writes must not cause the RAM builder to double past its threshold.");
	}
	Console.WriteLine("PASS: packet spill thresholds, append/readback, exact/over limits, cleanup after success/failure and bulk little-endian pixel compatibility.");
}

// Force hash collisions and cache eviction; only exact packet bytes may share.
{
	using var stream=new MetaReplacementPacketStream(1024,1); using var writer=new BinaryWriter(stream,System.Text.Encoding.UTF8,true);
	var resources=new MetaPngPacketResources(stream,2,_ => "collision");
	resources.Write(writer,new[] {1u,2u}); resources.Write(writer,new[] {1u,3u}); resources.Write(writer,new[] {1u,2u});
	resources.Write(writer,new[] {9u}); resources.Write(writer,new[] {1u,3u});
	byte[] packet=stream.ToArray();
	Check(packet.Length==48 && BitConverter.ToUInt32(packet,0)==0 && BitConverter.ToUInt32(packet,12)==0
		&& BitConverter.ToUInt32(packet,24)==1 && BitConverter.ToUInt32(packet,28)==0 && BitConverter.ToUInt32(packet,36)==0,
		"Resource collisions must compare complete bytes and evicted resources must be safely re-inlined.");
	Check(resources.Count==2,"Resource metadata must remain bounded independently of decoded pixels.");
	writer.Write(0x12345678u); Check(stream.ToArray().TakeLast(4).SequenceEqual(new byte[] {0x78,0x56,0x34,0x12}),"Resource comparisons must restore the append position.");
	using var largeStream=new MetaReplacementPacketStream(1024*1024,16); using var largeWriter=new BinaryWriter(largeStream,System.Text.Encoding.UTF8,true);
	var largeResources=new MetaPngPacketResources(largeStream,2,_ => "collision"); uint[] image=new uint[32769]; image[^1]=0x80ABCDEF;
	largeResources.Write(largeWriter,image); long firstEnd=largeStream.Length;
	largeResources.Write(largeWriter,(uint[])image.Clone()); Check(largeStream.Length==firstEnd+4,"Multi-block spilled comparison must recognize identical pixels.");
	var changed=(uint[])image.Clone(); changed[^1]^=1; largeResources.Write(largeWriter,changed);
	Check(largeStream.Length==firstEnd*2+4,"A mismatch after multiple compare blocks must remain an inline image.");
	Console.WriteLine("PASS: bounded pixel-resource metadata, exact collision-safe sharing, spill-backed multi-block comparisons and safe re-inline after eviction.");
}

// A detached file is sealed/readable until native parsing finishes, then removed.
{
	string? ownedPath=null;
	using(var builder=new MetaReplacementPacketStream(1024,8,fileLease:true)) {
		builder.Write(new byte[] {1,2,3,4,5,6,7,8,9});
		using(var owned=builder.DetachFile()) {
			ownedPath=owned.Path;
			Check(File.ReadAllBytes(owned.Path).SequenceEqual(Enumerable.Range(1,9).Select(v => (byte)v)),"Detached packet must preserve bytes and permit independent readers.");
			bool denied=false; try { using var writer=File.OpenWrite(owned.Path); } catch(IOException) { denied=true; }
			if(OperatingSystem.IsWindows()) { Check(denied,"Sealed handoff must deny another writer."); }
			Check(!builder.CanWrite,"Detached builders must not retain write ownership.");
		}
	}
	Check(ownedPath!=null && !File.Exists(ownedPath),"File handoff must remove its owned temporary file after parsing.");
	string? failedPath=null;
	try {
		using var builder=new MetaReplacementPacketStream(8,0,fileLease:true); builder.WriteByte(1); failedPath=builder.SpillPath;
		builder.Write(new byte[8]);
	} catch(InvalidDataException) { }
	Check(failedPath!=null && !File.Exists(failedPath),"Failed file-mode construction must clean its unleased file.");
	Console.WriteLine("PASS: sealed direct-file packet ownership, independent read access, write exclusion and cleanup after success/failure.");
}

// Decode cache bounds are independent of PNG contents and packet construction.
{
	var loads=new Dictionary<string,int>(); MetaPngDecodeCache? cache=null;
	cache=new MetaPngDecodeCache(32,2,(path,reserve) => {
		string name=Path.GetFileName(path); int count=name=="large.png" ? 12 : 4;
		reserve(count*4L);
		Check(cache!.RetainedBytes<=32-count*4L || (count*4L>32 && cache.Count==0),"Eviction must happen before allocating the next frame.");
		loads[name]=loads.GetValueOrDefault(name)+1;
		if(name=="failed.png") { throw new InvalidDataException("Synthetic decode failure"); }
		return (count,1,Enumerable.Repeat(0x80ABCDEFu,count).ToArray());
	});
	string root=Path.Combine(Path.GetTempPath(),"decode-cache-tests");
	var first=cache.Get(Path.Combine(root,"a.png")); cache.Get(Path.Combine(root,"b.png"));
	Check(ReferenceEquals(first.Pixels,cache.Get(Path.Combine(root,".","a.png")).Pixels),"Canonical path hits must reuse exact straight-alpha pixels.");
	cache.Get(Path.Combine(root,"c.png"));
	Check(loads["a.png"]==1 && cache.Count==2 && cache.RetainedBytes==32,"LRU retention must honor the byte budget.");
	cache.Get(Path.Combine(root,"a.png")); cache.Get(Path.Combine(root,"b.png"));
	Check(loads["b.png"]==2 && loads["a.png"]==1,"A cache hit must protect the recently used frame from eviction.");
	cache.Get(Path.Combine(root,"large.png")); cache.Get(Path.Combine(root,"large.png"));
	Check(cache.Count==0 && cache.RetainedBytes==0 && loads["large.png"]==2,"Single oversized frames must evict retained buffers and bypass caching.");
	try { cache.Get(Path.Combine(root,"failed.png")); } catch(InvalidDataException) { }
	Check(cache.Count==0 && cache.RetainedBytes==0,"A failed decode must not publish a partial entry.");
	cache.Get(Path.Combine(root,"a.png"));
	Check(cache.Count==1 && first.Pixels[0]==0x80ABCDEF,"Eviction must preserve external frame ownership and partial-alpha values.");
	var tiny=new MetaPngDecodeCache(1024,2,(_,reserve) => { reserve(4); return (1,1,new[] {1u}); });
	for(int n=0;n<1000;n++) { tiny.Get(Path.Combine(root,n+".png")); Check(tiny.Count<=2 && tiny.RetainedBytes<=8,"Entry count must also bound tiny images."); }
	foreach(bool reserveWrong in new[] {false,true}) {
		var invalid=new MetaPngDecodeCache(16,2,(_,reserve) => { if(reserveWrong) { reserve(8); } return (1,1,new[] {1u}); });
		bool rejected=false; try { invalid.Get(Path.Combine(root,"invalid.png")); } catch(InvalidDataException) { rejected=true; }
		Check(rejected && invalid.Count==0,"Missing or inconsistent decoder reservations must be rejected.");
	}
	Console.WriteLine("PASS: bounded decode cache LRU, canonical reuse, preallocation eviction, oversized bypass, tiny-entry limits, straight alpha and failed-decode isolation.");
}

var animation = new MetaPngAnimation { Enabled = true, RestartOnActivation = true, Frames = new() {
	new() { Path = "a.png", Duration = 2 }, new() { Path = "b.png", Duration = 3, OriginX = -2, OriginY = 7 }, new() { Path = "c.png", Duration = 1 }
} };
var timeline = new MetaPngTimeline(animation);
Check(Enumerable.Range(0,12).Select(i => timeline.SelectFrame((ulong)i)).SequenceEqual(new[] {0,0,1,1,1,2,0,0,1,1,1,2}), "Animation durations and loop boundaries must use emulated frames.");
Check(timeline.SelectFrame(ulong.MaxValue) == timeline.SelectFrame(ulong.MaxValue % 6), "Long-running animation must not overflow.");
animation.Playback = MetaPngPlayback.PingPong; timeline = new MetaPngTimeline(animation);
Check(timeline.Duration == 9 && Enumerable.Range(0,10).Select(i => timeline.SelectFrame((ulong)i)).SequenceEqual(new[] {0,0,1,1,1,2,1,1,1,0}), "Ping-pong must reverse without repeating endpoint visits.");
animation.Playback = MetaPngPlayback.PlayOnce; timeline = new MetaPngTimeline(animation);
Check(timeline.SelectFrame(5) == 2 && timeline.SelectFrame(6) == 2 && timeline.SelectFrame(ulong.MaxValue) == 2, "Play-once must hold its final frame.");
animation.Frames[0].Duration = 10;
Check(timeline.SelectFrame(2) == 1, "Compiled timing must not change when editor drafts change.");
animation.Enabled = false; Check(new MetaPngTimeline(animation).SelectFrame(0) == -1, "Disabled animation must select static artwork.");
foreach(string invalid in new[] { "../outside.png", "C:\\outside.png", "/outside.png", "a/../../outside.png", "file.jpg" }) {
	var rejectedAnimation = new MetaPngAnimation { Frames = new() { new() { Path = invalid } } };
	bool rejected = false; try { rejectedAnimation.Validate(); } catch(InvalidDataException) { rejected = true; }
	Check(rejected, "Invalid animation asset paths must be rejected: " + invalid);
}
animation.Enabled = true; animation.Frames[0].Duration = 0;
bool rejectedDuration = false; try { animation.Validate(); } catch(InvalidDataException) { rejectedDuration = true; }
Check(rejectedDuration, "Zero-duration animation frames must be rejected."); animation.Frames[0].Duration = 2;
var oneFrame = new MetaPngAnimation { Enabled=true, Playback=MetaPngPlayback.PingPong, Frames=new() { new() { Path="one.png", Duration=7 } } };
Check(new MetaPngTimeline(oneFrame).SelectFrame(ulong.MaxValue)==0,"A single ping-pong frame must remain stable.");
oneFrame.Frames.Add(new() { Path="two.png", Duration=2 }); var twoFrames=new MetaPngTimeline(oneFrame);
Check(twoFrames.Duration==9 && twoFrames.SelectFrame(7)==1 && twoFrames.SelectFrame(9)==0,"Two-frame ping-pong must not invent reverse endpoint visits.");
Console.WriteLine("PASS: PNG animation duration boundaries, loop/ping-pong/play-once timing, immutable compilation, large ticks and invalid input rejection.");
if(args.Length > 0) {
	Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[0]))!);
	using var timingFile = new BinaryWriter(File.Create(args[0]+".animation"));
	timingFile.Write(0x31544150u); // PAT1 managed/native timing verification.
	timingFile.Write(12);
	foreach(MetaPngPlayback playback in Enum.GetValues<MetaPngPlayback>()) {
		foreach(int count in new[] {1,2,3,256}) {
			var sequence = new MetaPngAnimation { Enabled=true, Playback=playback };
			for(int i=0;i<count;i++) { sequence.Frames.Add(new() { Path=$"frame-{i}.png", Duration=count==256 ? 3600000 : i+1 }); }
			var compiled = new MetaPngTimeline(sequence);
			var ticks = new SortedSet<ulong> {0,1,2,3,4,5,6,compiled.Duration-1,compiled.Duration,compiled.Duration+1,ulong.MaxValue};
			ulong boundary=0;
			foreach(var frame in sequence.Frames) { boundary+=(ulong)frame.Duration; ticks.Add(boundary-1); ticks.Add(boundary); }
			timingFile.Write((int)playback); timingFile.Write(count);
			foreach(var frame in sequence.Frames) { timingFile.Write(frame.Duration); }
			timingFile.Write(compiled.Duration); timingFile.Write(ticks.Count);
			foreach(ulong tick in ticks) { timingFile.Write(tick); timingFile.Write(compiled.SelectFrame(tick)); }
		}
	}
}

var animationPack = new MetaSpritePack { Groups=new() { new() {
	Id=1, Enabled=true, Width=1, Height=1, ReplacementPath="static.png", Members=new() { Sprite(0,0,0xFFFF0000) },
	Animation=new() { Enabled=true, Frames=new() {new() {Path="a.png",Duration=2},new() {Path="b.png",Duration=1,OriginX=1}} },
	AlternateArtwork=new() { new() { Path="alt.png",MemoryConditions=new() {new() {Address=7,Value=1}},
		Animation=new() { Enabled=true,Frames=new() {new() {Path="blue.png"},new() {Path="white.png"}} } } }
} } };
byte[] animatedPacket=MetaSpriteRuntimePacket.Create(animationPack,_ => (2,1,new[] {0xFFFF0000u,0xFFFF0000u}),
	_ => (2,1,new[] {0xFF0000FFu,0xFF0000FFu}), (frame,_) => (2,1,frame.Path switch {
		"a.png" => new[] {0xFF00FF00u,0xFF00FF00u}, "b.png" => new[] {0u,0x80FFFF00u},
		"blue.png" => new[] {0xFF0000FFu,0xFF0000FFu}, _ => new[] {0xFFFFFFFFu,0xFFFFFFFFu}
	}));
Check(BitConverter.ToUInt32(animatedPacket)==MetaSpriteRuntimePacket.AnimationMagic,"Animated artwork requires MSP5 transport.");
if(args.Length>0) { File.WriteAllBytes(args[0]+".animated",animatedPacket); }
animationPack.Groups[0].Animation.RestartOnActivation=true;
animationPack.Groups[0].AlternateArtwork[0].Animation.RestartOnActivation=true;
byte[] activationPacket=MetaSpriteRuntimePacket.Create(animationPack,_ => (2,1,new[] {0xFFFF0000u,0xFFFF0000u}),
	_ => (2,1,new[] {0xFF0000FFu,0xFF0000FFu}), (frame,_) => (2,1,frame.Path switch {
		"a.png" => new[] {0xFF00FF00u,0xFF00FF00u}, "b.png" => new[] {0u,0x80FFFF00u},
		"blue.png" => new[] {0xFF0000FFu,0xFF0000FFu}, _ => new[] {0xFFFFFFFFu,0xFFFFFFFFu}
	}));
if(args.Length>0) { File.WriteAllBytes(args[0]+".activated",activationPacket); }
if(args.Length>0) { File.WriteAllBytes(args[0]+".activated.shared",MetaSpriteRuntimePacket.Create(animationPack,_ => (2,1,new[] {0xFFFF0000u,0xFFFF0000u}),
	_ => (2,1,new[] {0xFF0000FFu,0xFF0000FFu}), (frame,_) => (2,1,frame.Path switch {
		"a.png" => new[] {0xFF00FF00u,0xFF00FF00u}, "b.png" => new[] {0u,0x80FFFF00u},
		"blue.png" => new[] {0xFF0000FFu,0xFF0000FFu}, _ => new[] {0xFFFFFFFFu,0xFFFFFFFFu}
	}),sharedFrames:true)); }
Console.WriteLine("PASS: MSP5 default/conditional alternate animation frame transport with inherited scale, optional origins and PNG alpha.");
var animatedBackgroundPack = new MetaSpritePack { System="Gameboy", BackgroundLayers=new() { new() {
	Id=1,Enabled=true,ArtworkPath="background.png",ArtworkWidth=1,ArtworkHeight=1,HideOriginalLayer=true,
	MemoryConditions=new() {new() {Address=7,Value=1}},Animation=new() {Enabled=true,Frames=new() {
		new() {Path="bg-a.png",Duration=2},new() {Path="bg-b.png",OriginX=1}
	}}
} } };
byte[] animatedBackgroundPacket=MetaBackgroundRuntimePacket.Create(animatedBackgroundPack,_ => (1,1,new[] {0xFF000000u}),
	(frame,_) => frame.Path=="bg-a.png" ? (2,1,new[] {0xFF00FF00u,0xFF00FF00u}) : (3,1,new[] {0u,0x80FFFF00u,0xFFFFFFFFu}));
Check(BitConverter.ToUInt32(animatedBackgroundPacket)==MetaBackgroundRuntimePacket.AnimationMagic,"Animated backgrounds require MBGH transport.");
if(args.Length>0) { File.WriteAllBytes(args[0]+".background.animated",animatedBackgroundPacket); }
animatedBackgroundPack.BackgroundLayers[0].Animation.RestartOnActivation=true;
byte[] activatedBackgroundPacket=MetaBackgroundRuntimePacket.Create(animatedBackgroundPack,_ => (1,1,new[] {0xFF000000u}),
	(frame,_) => frame.Path=="bg-a.png" ? (2,1,new[] {0xFF00FF00u,0xFF00FF00u}) : (3,1,new[] {0u,0x80FFFF00u,0xFFFFFFFFu}));
if(args.Length>0) { File.WriteAllBytes(args[0]+".background.activated",activatedBackgroundPacket); }
if(args.Length>0) { File.WriteAllBytes(args[0]+".background.activated.shared",MetaBackgroundRuntimePacket.Create(animatedBackgroundPack,_ => (1,1,new[] {0xFF000000u}),
	(frame,_) => frame.Path=="bg-a.png" ? (2,1,new[] {0xFF00FF00u,0xFF00FF00u}) : (3,1,new[] {0u,0x80FFFF00u,0xFFFFFFFFu}),sharedFrames:true)); }
Console.WriteLine("PASS: MBGH background sequence transport preserves RAM conditions, hide-native settings, changing dimensions and origins.");

// Cross the production spill threshold through the actual MSP5/MBGH writers.
{
	uint[] pixels=new uint[1024*256]; pixels[0]=0x80ABCDEF; pixels[^1]=0x10203040;
	var spillAnimation=new MetaPngAnimation {Enabled=true};
	for(int i=0;i<5;i++) { spillAnimation.Frames.Add(new() {Path="shared.png",Duration=i+1}); }
	var spillPack=new MetaSpritePack {System="Gameboy",Groups=new() {new() {Id=1,Enabled=true,Width=1,Height=1,ReplacementPath="base.png",Members=new() {Sprite(0,0,0xFFFF0000)},Animation=spillAnimation}},
		BackgroundLayers=new() {new() {Id=1,Enabled=true,ArtworkPath="base.png",ArtworkWidth=1,ArtworkHeight=1,MemoryConditions=new() {new() {Address=7,Mask=0}},Animation=spillAnimation}}};
	byte[] spriteSpill=MetaSpriteRuntimePacket.Create(spillPack,_ => (1,1,new[] {0xFFFFFFFFu}),null,(_,_) => (1024,256,pixels));
	byte[] bgSpill=MetaBackgroundRuntimePacket.Create(spillPack,_ => (1,1,new[] {0xFFFFFFFFu}),(_,_) => (1024,256,pixels));
	Check(spriteSpill.Length>4*1024*1024 && bgSpill.Length>4*1024*1024,"Production animation fixtures must exercise actual packet spilling.");
	if(args.Length>0) { File.WriteAllBytes(args[0]+".spilled",spriteSpill); File.WriteAllBytes(args[0]+".background.spilled",bgSpill); }
	Console.WriteLine("PASS: actual MSP5/MBGH multi-megabyte animation packet construction crosses the production spill threshold.");
	for(int i=0;i<spillAnimation.Frames.Count;i++) { spillAnimation.Frames[i].OriginX=i; spillAnimation.Frames[i].OriginY=-i; }
	byte[] sharedSprite=MetaSpriteRuntimePacket.Create(spillPack,_ => (1,1,new[] {0xFFFFFFFFu}),null,(_,_) => (1024,256,(uint[])pixels.Clone()),sharedFrames:true);
	byte[] sharedBg=MetaBackgroundRuntimePacket.Create(spillPack,_ => (1,1,new[] {0xFFFFFFFFu}),(_,_) => (1024,256,(uint[])pixels.Clone()),sharedFrames:true);
	Check(BitConverter.ToUInt32(sharedSprite)==MetaSpriteRuntimePacket.SharedAnimationMagic && BitConverter.ToUInt32(sharedBg)==MetaBackgroundRuntimePacket.SharedAnimationMagic,"Shared animation transport must select MSP6/MBGI.");
	Check(sharedSprite.Length<spriteSpill.Length/4 && sharedBg.Length<bgSpill.Length/4,"Repeated independent decoded buffers must be transmitted once.");
	spillPack.Groups[0].AlternateArtwork.Add(new() {Path="alternate.png",MemoryConditions=new() {new() {Address=7,Mask=0}},Animation=spillAnimation});
	spillPack.Groups.Add(new() {Id=2,Enabled=true,Width=1,Height=1,ReplacementPath="base.png",Members=new() {Sprite(0,0,0xFFFF0000)},Animation=spillAnimation});
	spillPack.BackgroundLayers.Add(new() {Id=2,Enabled=true,ArtworkPath="base.png",ArtworkWidth=1,ArtworkHeight=1,MemoryConditions=new() {new() {Address=7,Mask=0}},Animation=spillAnimation});
	byte[] multiple=MetaSpriteRuntimePacket.Create(spillPack,_ => (1,1,new[] {0xFFFFFFFFu}),_ => (1,1,new[] {0xFFFFFFFFu}),(_,_) => (1024,256,(uint[])pixels.Clone()),sharedFrames:true);
	byte[] multipleBg=MetaBackgroundRuntimePacket.Create(spillPack,_ => (1,1,new[] {0xFFFFFFFFu}),(_,_) => (1024,256,(uint[])pixels.Clone()),sharedFrames:true);
	Check(multiple.Length<sharedSprite.Length+1024 && multipleBg.Length<sharedBg.Length+1024,"Resource references must cross alternate/default/group/layer sequence boundaries.");
	if(args.Length>0) {
		File.WriteAllBytes(args[0]+".shared",sharedSprite); File.WriteAllBytes(args[0]+".background.shared",sharedBg);
		File.WriteAllBytes(args[0]+".shared-multiple",multiple); File.WriteAllBytes(args[0]+".background.shared-multiple",multipleBg);
	}
	string spriteFilePath,bgFilePath;
	using(var spriteFile=MetaSpriteRuntimePacket.CreateFile(spillPack,_ => (1,1,new[] {0xFFFFFFFFu}),_ => (1,1,new[] {0xFFFFFFFFu}),(_,_) => (1024,256,(uint[])pixels.Clone()),sharedFrames:true))
	using(var bgFile=MetaBackgroundRuntimePacket.CreateFile(spillPack,_ => (1,1,new[] {0xFFFFFFFFu}),(_,_) => (1024,256,(uint[])pixels.Clone()),sharedFrames:true)) {
		spriteFilePath=spriteFile.Path; bgFilePath=bgFile.Path;
		Check(File.ReadAllBytes(spriteFile.Path).SequenceEqual(multiple) && File.ReadAllBytes(bgFile.Path).SequenceEqual(multipleBg),"Direct-file MSP6/MBGI output must exactly match byte-array encoding.");
	}
	Check(!File.Exists(spriteFilePath) && !File.Exists(bgFilePath),"Both packet leases must clean up after application.");
	Console.WriteLine($"PASS: content-shared MSP6/MBGI transport reduces repeated-frame packets from {spriteSpill.Length}/{bgSpill.Length} to {sharedSprite.Length}/{sharedBg.Length} bytes with independent origins and shared resources across sequences.");

}

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
	var background = new MetaBackgroundLayer { Id = 1, NativeLayer = 1, DrawOrder = 7, ArtworkPath = "background.png", ArtworkWidth = 64, ArtworkHeight = 32, ArtworkScale = 2, X = 3, Y = 5, WrapX = true };
	pack.BackgroundLayers.Add(background);
	pack.Save(path);
	pack.Groups[0].Animation = animation;
	pack.BackgroundLayers[0].Animation = animation;
	pack.Groups[0].AlternateArtwork.Add(new() { Path="alternate.png", Animation=animation });
	pack.Save(path);
	var animatedPack=MetaSpritePack.Load(path,"test-rom","Gameboy");
	Check(animatedPack.Groups[0].Animation.Frames[1].OriginX == -2 && animatedPack.Groups[0].Animation.Frames[0].OriginX == null
		&& animatedPack.Groups[0].Animation.RestartOnActivation && animatedPack.BackgroundLayers[0].Animation.Playback == MetaPngPlayback.PlayOnce
		&& animatedPack.Groups[0].AlternateArtwork[^1].Animation.Frames.Count == 3, "Animation sequences must survive JSON for all artwork owners.");
	bool rejectedPlayback=false;
	try { MetaSpriteRuntimePacket.Create(animatedPack,_ => (1,1,new[] {0xFFFFFFFFu})); } catch(InvalidDataException) { rejectedPlayback=true; }
	Check(rejectedPlayback,"Enabled animation requires a PNG frame loader.");
	pack.Groups[0].Animation=new(); pack.BackgroundLayers[0].Animation=new(); pack.Groups[0].AlternateArtwork.RemoveAt(pack.Groups[0].AlternateArtwork.Count-1);
	pack.Save(path);
	Check(!File.ReadAllText(path).Contains("RuntimeSpriteIndex"), "Transient hardware indices must never enter saved recognition definitions.");
	var loaded = MetaSpritePack.Load(path, "test-rom", "Gameboy");
	Check(loaded.BackgroundLayers.Count == 1 && loaded.BackgroundLayers[0].ArtworkPath == "background.png" && loaded.BackgroundLayers[0].NativeLayer == 1,
		"Background layer definitions must persist per game with artwork and hardware layer selection.");
	Check(loaded.NextBackgroundId == 2, "Older background packs must allocate above existing IDs.");
	Check(loaded.BackgroundLayers[0].DrawOrder == 7, "PNG draw order must survive a saved pack reload.");
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

foreach(var comparison in Enum.GetValues<MetaMemoryComparison>()) {
	foreach(int mask in new[] {0, 15, 240, 255}) { foreach(int leftByte in new[] {0, 1, 127, 128, 255}) { foreach(int rightByte in new[] {0, 1, 127, 128, 255}) {
		int left = leftByte & mask, right = rightByte & mask;
		bool expected = comparison switch {
			MetaMemoryComparison.Equal => left == right, MetaMemoryComparison.NotEqual => left != right,
			MetaMemoryComparison.Greater => left > right, MetaMemoryComparison.Less => left < right,
			MetaMemoryComparison.GreaterOrEqual => left >= right, _ => left <= right
		};
		var parsed = MetaMemoryConditionText.Parse($"C000 {MetaMemoryConditionText.Operator(comparison)} {rightByte:X2} / {mask:X2}");
		Check(MetaMemoryCondition.Matches(parsed, _ => (byte)leftByte) == expected, "Unsigned masked comparison boundary mismatch.");
		Check(!MetaMemoryCondition.Matches(parsed, _ => null), "Missing bytes must fail every comparison.");
		Check(MetaMemoryConditionText.Parse(MetaMemoryConditionText.Format(parsed))[0].Comparison == comparison, "Comparison editor round trip failed.");
	} } }
}
Check(MetaMemoryConditionText.Format(editedConditions).Contains(" == "), "Legacy equality must display as ==.");
Console.WriteLine("PASS: all six unsigned comparison operators, masks, boundary bytes, missing addresses, text round trips and legacy equality.");
Check(editedRoundtrip.Count == 3 && editedRoundtrip[2].Mask == 0 && MetaMemoryConditionText.Parse(" \r\n").Count == 0, "Condition editor must preserve conditions and allow clearing.");
foreach(string invalidCondition in new[] { "C000", "C000 = 100", "C000 = 01 / 100", "100000000 = 01", "C000 = GG", "C000 = 01 / FF extra",
	string.Join('\n', Enumerable.Repeat("C000 = 01", 65)) }) {
	bool invalidEditorInput = false;
	try { MetaMemoryConditionText.Parse(invalidCondition); } catch(InvalidDataException) { invalidEditorInput = true; }
	Check(invalidEditorInput, "Condition editor must reject invalid input before saving.");
}
NamedConditionTests.Run();
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
var comparisonGroup = new MetaSpriteGroup {Id = 77, Enabled = true, Width = 8, Height = 8, Members = runtimeGroup.Members};
var orderPack = new MetaSpritePack { BackgroundLayers = new() {
	new() { Id = 1, Enabled = true, DrawOrder = 9, ArtworkWidth = 1, ArtworkHeight = 1, MemoryConditions = new() {new() {Address = 0, Value = 1}} },
	new() { Id = 2, Enabled = true, DrawOrder = 0, ArtworkWidth = 1, ArtworkHeight = 1, MemoryConditions = new() {new() {Address = 0, Value = 1}} },
	new() { Id = 3, Enabled = true, DrawOrder = 0, ArtworkWidth = 1, ArtworkHeight = 1, MemoryConditions = new() {new() {Address = 0, Value = 1}} }
}};
var emittedOrder = new List<int>();
byte[] orderPacket = MetaBackgroundRuntimePacket.Create(orderPack, layer => { emittedOrder.Add(layer.Id); return (1, 1, new[] {layer.Id == 1 ? 0x800000FFu : layer.Id == 2 ? 0xFFFF0000u : 0u}); });
Check(emittedOrder.SequenceEqual(new[] {2, 3, 1}), "Draw slots must sort stably without changing native layer assignment.");
Check(orderPack.BackgroundLayers.Select(layer => layer.Id).SequenceEqual(new[] {1, 2, 3}), "Runtime ordering must not reorder saved editor entries.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.order", orderPacket); }
orderPack.BackgroundLayers[0].CompositionPosition = 1;
orderPack.BackgroundLayers[0].DrawOrder = 0;
var compositionReloaded = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(orderPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(compositionReloaded.BackgroundLayers[0].CompositionPosition == 1, "Foreground position must survive JSON reload.");
emittedOrder.Clear();
byte[] foregroundPacket = MetaBackgroundRuntimePacket.Create(orderPack, layer => { emittedOrder.Add(layer.Id); return (1, 1, new[] {layer.Id == 1 ? 0x800000FFu : layer.Id == 2 ? 0xFFFF0000u : 0u}); });
Check(emittedOrder.SequenceEqual(new[] {2, 3, 1}), "Foreground position must sort above native PNGs independently of draw slot and saved entry order.");
Check(BitConverter.ToUInt32(foregroundPacket) == MetaBackgroundRuntimePacket.CompositionMagic, "Foreground position must use explicit MBGD transport.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.foreground", foregroundPacket); }
orderPack.BackgroundLayers[0].CompositionPosition = 2;
bool rejectedComposition = false;
try { MetaBackgroundRuntimePacket.Create(orderPack, _ => (1, 1, new uint[1])); } catch(InvalidDataException) { rejectedComposition = true; }
Check(rejectedComposition, "Invalid foreground position must be rejected.");
orderPack.BackgroundLayers[0].CompositionPosition = 0;
foreach(int invalidOrder in new[] {-1, 10}) {
	orderPack.BackgroundLayers[0].DrawOrder = invalidOrder;
	bool rejectedOrder = false;
	try { MetaBackgroundRuntimePacket.Create(orderPack, _ => (1, 1, new uint[1])); } catch(InvalidDataException) { rejectedOrder = true; }
	Check(rejectedOrder, "Out-of-range PNG draw slots must be rejected.");
}
Check(new MetaBackgroundLayer().DrawOrder == 0, "Legacy PNGs must retain saved ordering by default.");
var gbaPriorityPack = new MetaSpritePack { System = "Gba", BackgroundLayers = new() {
	new() { Id = 1, Enabled = true, CompositionPosition = 2, GbaPriority = 3, ArtworkWidth = 1, ArtworkHeight = 1, MemoryConditions = new() {new() {Address = 0, Value = 1}} }
}};
var priorityReloaded = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(gbaPriorityPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(priorityReloaded.BackgroundLayers[0].CompositionPosition == 2 && priorityReloaded.BackgroundLayers[0].GbaPriority == 3, "Independent GBA priority must persist separately from the source layer.");
byte[] priorityPacket = MetaBackgroundRuntimePacket.Create(gbaPriorityPack, _ => (1, 1, new[] {0xFF0000FFu}));
Check(BitConverter.ToUInt32(priorityPacket) == MetaBackgroundRuntimePacket.GbaPriorityMagic, "Independent GBA priority requires MBGE transport.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.gba-priority", priorityPacket); }
gbaPriorityPack.BackgroundLayers[0].GbaPriority = 4;
bool rejectedPriority = false; try { MetaBackgroundRuntimePacket.Create(gbaPriorityPack, _ => (1, 1, new uint[1])); } catch(InvalidDataException) { rejectedPriority = true; }
Check(rejectedPriority, "GBA priorities outside 0..3 must be rejected.");
var nesBandPack = new MetaSpritePack { System = "Nes", BackgroundLayers = new() };
foreach(int position in new[] {1, 6, 5, 4}) { nesBandPack.BackgroundLayers.Add(new() {Id = position, Enabled = true, CompositionPosition = position, ArtworkWidth = 1, ArtworkHeight = 1, MemoryConditions = new() {new() {Address = 0, Value = 1}}}); }
var nesBandOrder = new List<int>();
byte[] nesBandPacket = MetaBackgroundRuntimePacket.Create(nesBandPack, layer => { nesBandOrder.Add(layer.CompositionPosition); return (1, 1, new[] {0xFFFFFFFFu}); });
Check(nesBandOrder.SequenceEqual(new[] {4, 5, 6, 1}) && BitConverter.ToUInt32(nesBandPacket) == MetaBackgroundRuntimePacket.NesBandsMagic, "NES bands must serialize in scenery composition order, retaining the final foreground overlay.");
var nesBandReloaded = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(nesBandPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(nesBandReloaded.BackgroundLayers[2].CompositionPosition == 5, "NES insertion positions must persist.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.nes-bands", nesBandPacket); }
nesBandPack.System = "Gameboy";
byte[] gbBandPacket = MetaBackgroundRuntimePacket.Create(nesBandPack, _ => (1, 1, new[] {0xFFFFFFFFu}));
Check(BitConverter.ToUInt32(gbBandPacket) == MetaBackgroundRuntimePacket.NesBandsMagic, "GB insertion bands must retain the shared MBGG layout.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.gb-bands", gbBandPacket); }
nesBandPack.System = "Pce";
byte[] pceBandPacket = MetaBackgroundRuntimePacket.Create(nesBandPack, _ => (1, 1, new[] {0xFFFFFFFFu}));
Check(BitConverter.ToUInt32(pceBandPacket) == MetaBackgroundRuntimePacket.NesBandsMagic, "PCE insertion bands must retain the shared MBGG layout.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.pce-bands", pceBandPacket); }
nesBandPack.System = "Sms";
byte[] smsBandPacket = MetaBackgroundRuntimePacket.Create(nesBandPack, _ => (1, 1, new[] {0xFFFFFFFFu}));
Check(BitConverter.ToUInt32(smsBandPacket) == MetaBackgroundRuntimePacket.NesBandsMagic, "SMS insertion bands must retain the shared MBGG layout.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.sms-bands", smsBandPacket); }
nesBandPack.System = "Ws";
byte[] wsBandPacket = MetaBackgroundRuntimePacket.Create(nesBandPack, _ => (1, 1, new[] {0xFFFFFFFFu}));
Check(BitConverter.ToUInt32(wsBandPacket) == MetaBackgroundRuntimePacket.NesBandsMagic, "WS insertion bands must retain the shared MBGG layout.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.ws-bands", wsBandPacket); }
nesBandPack.System = "Gba";
bool rejectedNesBands = false; try { MetaBackgroundRuntimePacket.Create(nesBandPack, _ => (1, 1, new uint[1])); } catch(InvalidDataException) { rejectedNesBands = true; }
Check(rejectedNesBands, "Sprite insertion bands must not activate in an unsupported system pack.");
var snesPriorityPack = new MetaSpritePack { System = "Snes", BackgroundLayers = new() {
	new() {Id = 1, Enabled = true, CompositionPosition = 3, SnesPriority = 12, ArtworkWidth = 1, ArtworkHeight = 1, MemoryConditions = new() {new() {Address = 0, Value = 1}}}
}};
var snesPriorityReloaded = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(snesPriorityPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(snesPriorityReloaded.BackgroundLayers[0].SnesPriority == 12 && snesPriorityReloaded.BackgroundLayers[0].CompositionPosition == 3, "SNES independent priority must persist.");
byte[] snesPriorityPacket = MetaBackgroundRuntimePacket.Create(snesPriorityPack, _ => (1, 1, new[] {0xFF0000FFu}));
Check(BitConverter.ToUInt32(snesPriorityPacket) == MetaBackgroundRuntimePacket.SnesPriorityMagic, "Independent SNES priority requires MBGF transport.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.snes-priority", snesPriorityPacket); }
foreach(int value in new[] {0, 16}) {
	snesPriorityPack.BackgroundLayers[0].SnesPriority = value;
	bool rejectedSnesPriority = false; try { MetaBackgroundRuntimePacket.Create(snesPriorityPack, _ => (1, 1, new uint[1])); } catch(InvalidDataException) { rejectedSnesPriority = true; }
	Check(rejectedSnesPriority, "SNES hardware priorities must be 1..15.");
}
for(int op = 0; op < 6; op++) { comparisonGroup.MemoryConditions.Add(new() {Address = (uint)op, Value = 42, Comparison = (MetaMemoryComparison)op}); }
comparisonGroup.AlternateArtwork.Add(new() {Path = "comparison.png", MemoryConditions = new() {new() {Address = 6, Value = 0, Comparison = MetaMemoryComparison.NotEqual}}});
var comparisonPack = new MetaSpritePack {Groups = new() {comparisonGroup}, BackgroundLayers = new() {
	new() {Id = 1, Enabled = true, ArtworkWidth = 2, ArtworkHeight = 2, MemoryConditions = comparisonGroup.MemoryConditions}
}};
byte[] comparisonPacket = MetaSpriteRuntimePacket.Create(comparisonPack, _ => (8, 8, Enumerable.Repeat(0xFF00FF00u, 64).ToArray()),
	_ => (8, 8, Enumerable.Repeat(0xFFFFFFFFu, 64).ToArray()));
byte[] comparisonBackgroundPacket = MetaBackgroundRuntimePacket.Create(comparisonPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(comparisonPacket) == MetaSpriteRuntimePacket.ComparisonMagic && BitConverter.ToUInt32(comparisonBackgroundPacket) == MetaBackgroundRuntimePacket.ComparisonMagic, "Comparison packet versions must be explicit.");
var comparisonRestored = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(comparisonPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(comparisonRestored.Groups[0].MemoryConditions[5].Comparison == MetaMemoryComparison.LessOrEqual && comparisonRestored.BackgroundLayers[0].MemoryConditions[1].Comparison == MetaMemoryComparison.NotEqual, "JSON must preserve operators.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".comparisons", comparisonPacket); File.WriteAllBytes(args[0] + ".background.comparisons", comparisonBackgroundPacket); }
comparisonPack.BackgroundLayers[0].ScrollSpeedX = 0.5; comparisonPack.BackgroundLayers[0].ScrollSpeedY = -1;
byte[] scrollPacket = MetaBackgroundRuntimePacket.Create(comparisonPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(scrollPacket) == MetaBackgroundRuntimePacket.ScrollMagic, "Custom scroll rates require MBG4.");
comparisonRestored = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(comparisonPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(comparisonRestored.BackgroundLayers[0].ScrollSpeedX == 0.5 && comparisonRestored.BackgroundLayers[0].ScrollSpeedY == -1, "JSON must preserve scroll multipliers.");
var scrollLayer = new MetaBackgroundLayer {Id = 1, ArtworkWidth = 256, ArtworkHeight = 256, ScrollSpeedX = 0.5, ScrollSpeedY = -1, WrapX = true, WrapY = true};
Check(scrollLayer.TryGetArtworkPixel(5, 5, 253, 250, out int scrollX, out int scrollY) && scrollX == 131 && scrollY == 11, "Custom scroll rates must preserve independent axes and negative wrapping.");
scrollLayer.ScrollSpeedX = scrollLayer.ScrollSpeedY = 0;
Check(scrollLayer.TryGetArtworkPixel(5, 5, 253, 250, out scrollX, out scrollY) && scrollX == 5 && scrollY == 5, "Zero rates must freeze both axes.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.scroll", scrollPacket); }
comparisonPack.BackgroundLayers[0].IgnoreRasterScroll = true; comparisonPack.BackgroundLayers[0].ScrollReferenceScanline = 47;
byte[] rasterPacket = MetaBackgroundRuntimePacket.Create(comparisonPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(rasterPacket) == MetaBackgroundRuntimePacket.RasterMagic, "Uniform NES scroll requires MBG5.");
comparisonRestored = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(comparisonPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(comparisonRestored.BackgroundLayers[0].IgnoreRasterScroll && comparisonRestored.BackgroundLayers[0].ScrollReferenceScanline == 47, "JSON must preserve NES raster scroll settings.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.raster", rasterPacket); }
comparisonPack.BackgroundLayers[0].HideOriginalLayer = true;
byte[] hidePacket = MetaBackgroundRuntimePacket.Create(comparisonPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(hidePacket) == MetaBackgroundRuntimePacket.HideLayerMagic, "Hide-original-layer requires MBG6.");
comparisonRestored = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(comparisonPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(comparisonRestored.BackgroundLayers[0].HideOriginalLayer && comparisonRestored.BackgroundLayers[0].ScrollReferenceScanline == 47, "JSON must preserve hide-layer settings alongside raster tracking.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.hide", hidePacket); }
var cameraAxis = new MetaCameraAxis { Address = 0x60, PageAddress = 0x70, PageSize = 240 };
var cameraBytes = new System.Collections.Generic.Dictionary<uint, byte> { [0x60] = 0x34, [0x61] = 0x12, [0x70] = 3 };
foreach(var (format, expected) in new[] { (0, 0), (1, 52), (2, 4660), (3, 13330), (4, 772) }) {
	cameraAxis.Format = format; Check(cameraAxis.Decode(address => cameraBytes[address]) == expected, "CPU camera format decoding.");
	Check(cameraAxis.Addresses().Count() == (format == 0 ? 0 : format == 1 ? 1 : 2), "Capture only configured camera bytes.");
}
comparisonPack.BackgroundLayers[0].UseCpuCamera = true;
comparisonPack.BackgroundLayers[0].CameraX = cameraAxis;
byte[] cameraPacket = MetaBackgroundRuntimePacket.Create(comparisonPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(cameraPacket) == MetaBackgroundRuntimePacket.CameraMagic, "CPU camera requires MBG7.");
comparisonRestored = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(comparisonPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(comparisonRestored.BackgroundLayers[0].UseCpuCamera && comparisonRestored.BackgroundLayers[0].CameraX.PageAddress == 0x70 && comparisonRestored.BackgroundLayers[0].CameraX.PageSize == 240, "CPU camera must persist.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.camera", cameraPacket); }
Console.WriteLine("PASS: CPU camera byte/endian/page decoding, configured byte selection and JSON/MBG7 persistence.");
var linkedCameraPack = new MetaSpritePack { SavedCamera = new() { X = cameraAxis.Copy(), Y = new() }, BackgroundLayers = new() {
	new() { Id = 101, Enabled = true, UseCpuCamera = true, UseSavedCamera = true, ArtworkWidth = 2, ArtworkHeight = 2, MemoryConditions = new() {new() {Address = 0, Mask = 0}} },
	new() { Id = 102, Enabled = true, UseCpuCamera = true, UseSavedCamera = true, ArtworkWidth = 2, ArtworkHeight = 2, MemoryConditions = new() {new() {Address = 0, Mask = 0}} }
} };
string linkedBefore = linkedCameraPack.GetBackgroundDefinitionsSnapshot();
byte[] linkedCameraPacket = MetaBackgroundRuntimePacket.Create(linkedCameraPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(linkedCameraPacket, 68) == 0x60 && BitConverter.ToUInt32(linkedCameraPacket, 192) == 0x60, "Both linked backgrounds must resolve the saved camera.");
linkedCameraPack.SavedCamera.X.Address = 0x62;
linkedCameraPacket = MetaBackgroundRuntimePacket.Create(linkedCameraPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(linkedCameraPacket, 68) == 0x62 && BitConverter.ToUInt32(linkedCameraPacket, 192) == 0x62, "Updating the saved camera must update all linked backgrounds.");
Check(linkedCameraPack.GetBackgroundDefinitionsSnapshot() != linkedBefore, "Saved camera updates must participate in background conflict detection.");
var restoredLinks = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(linkedCameraPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(restoredLinks.SavedCamera!.X.Address == 0x62 && restoredLinks.BackgroundLayers.All(layer => layer.UseSavedCamera), "Camera profile and links must survive JSON reload.");
restoredLinks.SavedCamera = null; bool missingProfileRejected = false;
try { MetaBackgroundRuntimePacket.Create(restoredLinks, _ => (2, 2, new uint[4])); } catch(InvalidDataException) { missingProfileRejected = true; }
Check(missingProfileRejected, "A dangling camera link must not silently use unrelated local addresses.");
Console.WriteLine("PASS: saved camera links resolve/update all backgrounds, persist and reject missing profiles.");
cameraBytes[0x60] = 0xF6; cameraBytes[0x70] = 0xFF; cameraAxis.PageSize = 256;
Check(cameraAxis.Decode(address => cameraBytes[address]) == 65526, "Unchecked FF:F6 must retain unsigned interpretation.");
cameraAxis.SignedPage = true;
Check(cameraAxis.Decode(address => cameraBytes[address]) == -10, "Signed FF:F6 must decode as -10 pixels.");
cameraBytes[0x60] = 255; Check(cameraAxis.Decode(address => cameraBytes[address]) == -1, "Signed FF:FF must decode as -1.");
cameraBytes[0x60] = 0; cameraBytes[0x70] = 0; Check(cameraAxis.Decode(address => cameraBytes[address]) == 0, "Signed camera must cross page zero smoothly.");
linkedCameraPack.SavedCamera!.X = cameraAxis.Copy();
byte[] signedCameraPacket = MetaBackgroundRuntimePacket.Create(comparisonPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(signedCameraPacket) == MetaBackgroundRuntimePacket.SignedCameraMagic, "Signed pages require MBG8.");
var signedRestored = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(linkedCameraPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(signedRestored.SavedCamera!.X.SignedPage && !signedRestored.SavedCamera.Y.SignedPage, "Saved camera signed axes must persist independently.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.camera.signed", signedCameraPacket); }
Console.WriteLine("PASS: signed FF:F6/FF:FF/00:00 camera decoding and saved-profile/MBG8 persistence.");
comparisonPack.BackgroundLayers[0].ExtendIntoMargins = true;
byte[] widescreenPacket = MetaBackgroundRuntimePacket.Create(comparisonPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(widescreenPacket) == MetaBackgroundRuntimePacket.WidescreenMagic, "Decorative extension requires MBG9.");
var widescreenRestored = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(comparisonPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(widescreenRestored.BackgroundLayers[0].ExtendIntoMargins, "Margin extension must survive pack reload.");
comparisonPack.BackgroundLayers[0].UseWidescreenPosition = true;
comparisonPack.BackgroundLayers[0].WidescreenPosition = new() { Position = 3, CameraMin = -10, CameraMax = 2048, SlideSpeed = 0.1, ReleaseDelay = 6, UseRamFlags = true, LeftFlags = "$68 == $01", RightFlags = "$68 == $02" };
byte[] positionPacket = MetaBackgroundRuntimePacket.Create(comparisonPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(positionPacket) == MetaBackgroundRuntimePacket.SmoothPositionMagic, "Smooth PNG positioning requires MBGB.");
var positionReloaded = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(comparisonPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
Check(positionReloaded.BackgroundLayers[0].WidescreenPosition.CameraMin == -10 && positionReloaded.BackgroundLayers[0].WidescreenPosition.LeftFlags == "$68 == $01", "Per-PNG scene positioning must persist.");
Check(positionReloaded.BackgroundLayers[0].WidescreenPosition.SlideSpeed == 0.1 && positionReloaded.BackgroundLayers[0].WidescreenPosition.ReleaseDelay == 6, "Fractional speed and release delay must persist.");
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.position", positionPacket); }
var ramSpeed = comparisonPack.BackgroundLayers[0].WidescreenPosition.RamSpeed;
ramSpeed.Enabled = true; ramSpeed.Format = 0; ramSpeed.AddressHex = "$72"; ramSpeed.Multiplier = 0.0625; ramSpeed.Maximum = 2.5;
byte[] ramSpeedPacket = MetaBackgroundRuntimePacket.Create(comparisonPack, _ => (2, 2, Enumerable.Repeat(0xFFFFFFFFu, 4).ToArray()));
Check(BitConverter.ToUInt32(ramSpeedPacket) == MetaBackgroundRuntimePacket.RamSpeedMagic, "Player speed requires MBGC.");
var ramReloaded = System.Text.Json.JsonSerializer.Deserialize(System.Text.Json.JsonSerializer.Serialize(comparisonPack, MetaSpriteSerializerContext.Default.MetaSpritePack), MetaSpriteSerializerContext.Default.MetaSpritePack)!;
var restoredSpeed = ramReloaded.BackgroundLayers[0].WidescreenPosition.RamSpeed;
Check(restoredSpeed.Enabled && restoredSpeed.Format == 0 && restoredSpeed.Address == 0x72 && restoredSpeed.Multiplier == 0.0625 && restoredSpeed.Maximum == 2.5, "RAM speed settings must persist.");
ramSpeed.AddressHex = "invalid"; bool invalidSpeed = false; try { ramSpeed.Validate(); } catch(InvalidDataException) { invalidSpeed = true; }
Check(invalidSpeed, "Invalid speed address drafts must not use a stale address."); ramSpeed.AddressHex = "$72";
ramSpeed.Format = 2; ramSpeed.Address = uint.MaxValue; invalidSpeed = false; try { ramSpeed.Validate(); } catch(InvalidDataException) { invalidSpeed = true; }
Check(invalidSpeed, "16-bit speed addresses must not overflow."); ramSpeed.Format = 0; ramSpeed.Address = 0x72;
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.ram-speed", ramSpeedPacket); }
ramSpeed.Enabled = false;
comparisonPack.BackgroundLayers[0].UseWidescreenPosition = false;
if(args.Length > 0) { File.WriteAllBytes(args[0] + ".background.widescreen", widescreenPacket); }
comparisonPack.BackgroundLayers[0].ExtendIntoMargins = false;
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
if(args.Length >= 3 && args[1] == "--snes-hires-obj-math") {
    GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), onlyHiresObjectMath: true);
}
if(args.Length >= 3 && args[1] == "--snes-bob-blend-rewind") {
    GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), onlyBobBlendObjectMath: true);
}
if(args.Length >= 3 && (args[1] == "--snes-bob-blend-rewind-mode5" || args[1] == "--snes-bob-blend-rewind-mode6")) {
    string fixture = args[1].EndsWith("mode5") ? "mode5-hires-math-obj-interlace-bob-blend-field-both-screens" : "mode6-hires-math-operand-obj-interlace-bob-blend-field-both-screens-window";
    GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), fixtureFilter: fixture);
}
if(args.Length >= 3 && args[1] == "--snes-weave-rewind") {
    GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), onlyWeaveObjectMath: true);
}
if(args.Length >= 3 && args[1] == "--snes-weave-checkpoint") {
    GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), onlyWeaveObjectMath: true, checkpointOnly: true);
}
if(args.Length >= 3 && args[1] == "--snes-weave-checkpoint-pseudo") {
    GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), fixtureFilter: "pseudo-hires-math-operand-obj-interlace-both-screens", checkpointOnly: true);
}
if(args.Length >= 3 && args[1] == "--snes-weave-rewind-pseudo") {
    GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), fixtureFilter: "pseudo-hires-math-operand-obj-interlace-both-screens");
}
if(args.Length >= 3 && args[1] == "--snes-hires") {
	GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
}
if(args.Length >= 3 && args[1] == "--snes-obj-interlace") {
	GameplayFixture.RunSnesHiRes(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), onlyObjInterlace: true);
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
if(args.Length >= 3 && args[1] == "--nes-raster") {
	GameplayFixture.RunNesRasterSuite(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.nes")));
}
if(args.Length >= 3 && args[1] == "--widescreen") {
	GameplayFixture.RunWidescreenSuite(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gba")));
}
if(args.Length >= 3 && args[1] == "--pce-priority") {
	GameplayFixture.RunPcePrioritySuite(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.pce")));
}
if(args.Length >= 3 && args[1] == "--sms-legacy") {
	GameplayFixture.RunSmsLegacySuite(Path.GetFullPath(args[2]), Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture.sms"));
}
if(args.Length >= 3 && args[1] == "--gba-affine") {
	GameplayFixture.RunGbaAffine(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gba")));
}
if(args.Length >= 3 && args[1] == "--gba-capture-metadata") {
	GameplayFixture.RunGbaCaptureMetadata(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gba")));
}
if(args.Length >= 3 && args[1] == "--gameplay-gb") {
	GameplayFixture.Run(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), gameboyOnly: true);
}
if(args.Length >= 3 && args[1] == "--gameplay") {
	GameplayFixture.Run(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
	GameplayFixture.VerifyPackActivation(Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")), Path.GetFullPath(args[0]));
}
if(args.Length >= 3 && args[1] == "--foreground") {
	GameplayFixture.RunForegroundSuite(Path.GetFullPath(args[2]));
}
if(args.Length >= 3 && args[1] == "--viewer-refresh") {
	GameplayFixture.RunViewerRefreshSuite(Path.GetFullPath(args[2]), Path.GetFullPath(Path.Combine("Tests", "MetaSprites", "obj", "gameplay-fixture.gb")));
}
if(args.Length >= 2 && args[1] == "--matcher-benchmark") { MatcherBenchmark.Run(); }

if(args.Length>=3 && args[1]=="--animation-state") {
	GameplayFixture.RunAnimationStateSuite(Path.GetFullPath(args[2]),Path.GetFullPath(args[0]));
}

if(args.Length>=3 && args[1]=="--animation-state-shared") {
	GameplayFixture.RunAnimationStateSuite(Path.GetFullPath(args[2]),Path.GetFullPath(args[0]),shared:true);
}

if(args.Length>=3 && args[1]=="--animation-gameplay") {
	GameplayFixture.RunAnimationGameplaySuite(Path.GetFullPath(args[2]));
}

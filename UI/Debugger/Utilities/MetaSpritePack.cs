using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Mesen.Debugger.Utilities;

// Coordinates and origins are in native pixels, independent of replacement scale.
public sealed class MetaSpritePack
{
	public int Version { get; set; } = 1;
	public string RomHash { get; set; } = "";
	public string System { get; set; } = "";
	public int NextGroupId { get; set; } = 1;
	public int NextBackgroundId { get; set; } = 1;
	public List<MetaSpriteGroup> Groups { get; set; } = new();
	public List<MetaBackgroundLayer> BackgroundLayers { get; set; } = new();
	public MetaSavedCamera? SavedCamera { get; set; }

	public string GetSpriteDefinitionsSnapshot() => JsonSerializer.Serialize(new MetaSpritePack {
		Groups = Groups, NextGroupId = NextGroupId
	}, MetaSpriteSerializerContext.Default.MetaSpritePack);
	public string GetBackgroundDefinitionsSnapshot() => JsonSerializer.Serialize(new MetaSpritePack {
		BackgroundLayers = BackgroundLayers, NextBackgroundId = NextBackgroundId, SavedCamera = SavedCamera
	}, MetaSpriteSerializerContext.Default.MetaSpritePack);

	public int AlignPoseOrigins(MetaSpriteGroup source)
	{
		if(source.PoseFamilyId == 0 || !Groups.Contains(source)) { return 0; }
		int offsetX = source.OriginX - source.Width / 2;
		int offsetY = source.OriginY - source.Height;
		int count = 0;
		foreach(var pose in Groups.Where(g => g != source && g.PoseFamilyId == source.PoseFamilyId)) {
			pose.OriginX = pose.Width / 2 + offsetX;
			pose.OriginY = pose.Height + offsetY;
			count++;
		}
		return count;
	}

	public int AlignPoseArtworkOrigins(MetaSpriteGroup source)
	{
		if(source.PoseFamilyId == 0 || !Groups.Contains(source) || source.ReplacementPath.Length == 0) { return 0; }
		MetaSpritePlacement.ValidateSpriteCanvas(source.ReplacementWidth, source.ReplacementHeight, source.ReplacementScale);
		int offsetX = source.ReplacementOriginX - source.ReplacementWidth / source.ReplacementScale / 2;
		int offsetY = source.ReplacementOriginY - source.ReplacementHeight / source.ReplacementScale;
		var poses = Groups.Where(g => g != source && g.PoseFamilyId == source.PoseFamilyId && g.ReplacementPath.Length > 0).ToList();
		// Validate the entire family before changing any anchors.
		foreach(var pose in poses) { MetaSpritePlacement.ValidateSpriteCanvas(pose.ReplacementWidth, pose.ReplacementHeight, pose.ReplacementScale); }
		foreach(var pose in poses) {
			pose.ReplacementOriginX = pose.ReplacementWidth / pose.ReplacementScale / 2 + offsetX;
			pose.ReplacementOriginY = pose.ReplacementHeight / pose.ReplacementScale + offsetY;
		}
		return poses.Count;
	}

	public void Save(string path)
	{
		Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
		string temporary = path + ".tmp";
		File.WriteAllText(temporary, JsonSerializer.Serialize(this, MetaSpriteSerializerContext.Default.MetaSpritePack));
		File.Move(temporary, path, true);
	}

	public static MetaSpritePack Load(string path, string romHash, string system)
	{
		if(!File.Exists(path)) {
			return new() { RomHash = romHash, System = system };
		}
		MetaSpritePack pack = JsonSerializer.Deserialize(File.ReadAllText(path), MetaSpriteSerializerContext.Default.MetaSpritePack)
			?? throw new InvalidDataException("Empty metasprite pack.");
		if(pack.Version != 1 || pack.RomHash != romHash || pack.System != system) {
			throw new InvalidDataException("Metasprite pack version, ROM, or system does not match.");
		}
		if(pack.Groups == null || pack.Groups.Any(g => g == null || g.Id < 1 || g.Name == null || g.PoseName == null || g.PoseFamilyId < 0 || g.ReplacementPath == null
			|| g.Width < 1 || g.Height < 1 || g.Width > 2048 || g.Height > 2048 || g.ReplacementScale < 1 || g.ReplacementScale > 10
			|| g.Members == null || g.Members.Count == 0 || g.Members.Any(m => m == null || m.Width < 1 || m.Height < 1
				|| m.Width > 128 || m.Height > 128 || m.X < 0 || m.Y < 0 || m.X + m.Width > g.Width || m.Y + m.Height > g.Height
				|| m.Pixels == null || m.Pixels.Length != m.Width * m.Height))
			|| pack.Groups.Select(g => g.Id).Distinct().Count() != pack.Groups.Count) {
			throw new InvalidDataException("Invalid metasprite group definitions.");
		}
		pack.NextGroupId = Math.Max(pack.NextGroupId, pack.Groups.Select(g => g.Id).DefaultIfEmpty(0).Max() + 1);
		if(pack.BackgroundLayers == null || pack.BackgroundLayers.Any(layer => layer == null)
			|| pack.BackgroundLayers.Select(layer => layer.Id).Distinct().Count() != pack.BackgroundLayers.Count) {
			throw new InvalidDataException("Invalid background layer definitions.");
		}
		foreach(var layer in pack.BackgroundLayers) { layer.Validate(); }
		pack.ValidateSavedCamera();
		foreach(var group in pack.Groups) {
			if(group.Animation == null) { throw new InvalidDataException("Missing metasprite animation settings."); } group.Animation.Validate();
			MetaMemoryCondition.ValidateAll(group.MemoryConditions);
			if(group.AlternateArtwork == null || group.AlternateArtwork.Count > 64) { throw new InvalidDataException("Invalid alternate artwork list."); }
			foreach(var alternate in group.AlternateArtwork) { alternate.Validate(); }
		}
		pack.NextBackgroundId = Math.Max(pack.NextBackgroundId, pack.BackgroundLayers.Select(layer => layer.Id).DefaultIfEmpty(0).Max() + 1);
		foreach(var group in pack.Groups.Where(g => g.ReplacementPath.Length > 0 && g.ReplacementWidth == 0)) {
			// Packs written before independent replacement origins used the captured origin.
			group.ReplacementOriginX = group.OriginX;
			group.ReplacementOriginY = group.OriginY;
		}
		return pack;
	}
	public void ValidateSavedCamera() {
		SavedCamera?.Validate();
		if(BackgroundLayers.Any(layer => layer.UseSavedCamera) && SavedCamera == null) { throw new InvalidDataException("Linked backgrounds require a saved CPU camera."); }
	}
}

public sealed class MetaSavedCamera
{
	public MetaCameraAxis X { get; set; } = new();
	public MetaCameraAxis Y { get; set; } = new();
	public void Validate() { if(X == null || Y == null) { throw new InvalidDataException("Invalid saved CPU camera."); } X.Validate(); Y.Validate(); }
}

// Artwork coordinates are native pixels; PNG dimensions include ArtworkScale.
// Runtime adapters supply the selected hardware layer's scroll at each pixel/scanline.
public sealed class MetaSlideRamSpeed : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private bool _enabled;
	private string? _addressDraft;
	public bool Enabled { get => _enabled; set { _enabled = value; PropertyChanged?.Invoke(this, new(nameof(Enabled))); } }
	public int Format { get; set; } = 0;
	public uint Address { get; set; } = 0;
	public double Multiplier { get; set; } = 0.0625;
	public double Maximum { get; set; } = 4;
	[JsonIgnore] public string AddressHex { get => _addressDraft ?? Address.ToString("X4"); set { _addressDraft = value; if(TryAddress(value, out uint address)) { Address = address; } PropertyChanged?.Invoke(this, new(nameof(AddressHex))); } }
	private static bool TryAddress(string text, out uint address) => uint.TryParse(text.Trim().TrimStart('$'), System.Globalization.NumberStyles.AllowHexSpecifier, System.Globalization.CultureInfo.InvariantCulture, out address);
	public void Validate() {
		if(Format < 0 || Format > 5 || (Format >= 2 && Address == uint.MaxValue) || (_addressDraft != null && !TryAddress(_addressDraft, out _))
			|| !double.IsFinite(Multiplier) || Multiplier < 0 || Multiplier > 64 || !double.IsFinite(Maximum) || Maximum < 0 || Maximum > 64) {
			throw new InvalidDataException("Invalid player speed format, hexadecimal address, multiplier or maximum.");
		}
	}
}

public sealed class MetaWidescreenPosition : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	public int Position { get; set; } = 3;
	public int CameraMin { get; set; } = 0;
	public int CameraMax { get; set; } = 1024;
	public double SlideSpeed { get; set; } = 0;
	public uint ReleaseDelay { get; set; } = 0;
	public MetaSlideRamSpeed RamSpeed { get; set; } = new();
	private bool _useRamFlags;
	public bool UseRamFlags { get => _useRamFlags; set { _useRamFlags = value; PropertyChanged?.Invoke(this, new(nameof(UseRamFlags))); } }
	public string LeftFlags { get; set; } = "";
	public string RightFlags { get; set; } = "";
	public List<MetaMemoryCondition> ParseFlags(string text) {
		var result = MetaMemoryConditionText.Parse(text ?? "");
		if(result.Count > 8) { throw new InvalidDataException("Use at most eight boundary conditions per edge."); }
		return result;
	}
	public void Validate() {
		if(Position < 0 || Position > 3 || CameraMin < -16777216 || CameraMin > 16777216 || CameraMax < -16777216 || CameraMax > 16777216 || !double.IsFinite(SlideSpeed) || SlideSpeed < 0 || SlideSpeed > 64 || ReleaseDelay > 600) { throw new InvalidDataException("Invalid PNG gameplay positioning settings."); }
		if(RamSpeed == null) { throw new InvalidDataException("Missing player speed settings."); } RamSpeed.Validate();
		if(UseRamFlags) { var left = ParseFlags(LeftFlags); var right = ParseFlags(RightFlags); if(left.Count + right.Count == 0) { throw new InvalidDataException("Enter a left or right boundary condition for this PNG."); } }
	}
}

public sealed class MetaBackgroundLayer : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private int _artworkScale = 1, _x, _y;
	private bool _ignoreRasterScroll;
	public static int? ResolveGameboyRole(int tilemap, bool backgroundMap, bool windowMap)
	{
		if(tilemap < 0 || tilemap > 1) { return null; }
		if(tilemap == (backgroundMap ? 1 : 0)) { return 0; }
		if(tilemap == (windowMap ? 1 : 0)) { return 1; }
		return null;
	}
	public int Id { get; set; }
	public string Name { get; set; } = "";
	public int NativeLayer { get; set; }
	// 0 native layer, 1 final display overlay, 2 independent GBA hardware priority.
	private int _compositionPosition;
	public int CompositionPosition { get => _compositionPosition; set { if(_compositionPosition != value) { _compositionPosition = value; PropertyChanged?.Invoke(this, new(nameof(CompositionPosition))); } } }
	public int GbaPriority { get; set; }
	public int SnesPriority { get; set; } = 1;
	private int _drawOrder;
	public int DrawOrder { get => _drawOrder; set { if(_drawOrder != value) { _drawOrder = value; PropertyChanged?.Invoke(this, new(nameof(DrawOrder))); } } }
	public string ArtworkPath { get; set; } = "";
	public MetaPngAnimation Animation { get; set; } = new();
	public int ArtworkWidth { get; set; }
	public int ArtworkHeight { get; set; }
	public int ArtworkScale { get => _artworkScale; set { if(_artworkScale != value) { _artworkScale = value; PropertyChanged?.Invoke(this, new(nameof(ArtworkScale))); } } }
	public int X { get => _x; set { if(_x != value) { _x = value; PropertyChanged?.Invoke(this, new(nameof(X))); } } }
	public int Y { get => _y; set { if(_y != value) { _y = value; PropertyChanged?.Invoke(this, new(nameof(Y))); } } }
	public bool FollowScroll { get; set; } = true;
	public double ScrollSpeedX { get; set; } = 1;
	public double ScrollSpeedY { get; set; } = 1;
	public bool IgnoreRasterScroll { get => _ignoreRasterScroll; set { if(_ignoreRasterScroll != value) { _ignoreRasterScroll = value; PropertyChanged?.Invoke(this, new(nameof(IgnoreRasterScroll))); } } }
	public bool HideOriginalLayer { get; set; }
	private bool _extendIntoMargins;
	public bool ExtendIntoMargins { get => _extendIntoMargins; set { _extendIntoMargins = value; PropertyChanged?.Invoke(this, new(nameof(ExtendIntoMargins))); } }
	private bool _useWidescreenPosition;
	public bool UseWidescreenPosition { get => _useWidescreenPosition; set { _useWidescreenPosition = value; PropertyChanged?.Invoke(this, new(nameof(UseWidescreenPosition))); } }
	private MetaWidescreenPosition _widescreenPosition = new();
	public MetaWidescreenPosition WidescreenPosition { get => _widescreenPosition; set { _widescreenPosition = value; PropertyChanged?.Invoke(this, new(nameof(WidescreenPosition))); } }
	private bool _useCpuCamera;
	public bool UseCpuCamera { get => _useCpuCamera; set { _useCpuCamera = value; PropertyChanged?.Invoke(this, new(nameof(UseCpuCamera))); PropertyChanged?.Invoke(this, new(nameof(ScrollSourceIndex))); PropertyChanged?.Invoke(this, new(nameof(UseHardwareScroll))); } }
	[JsonIgnore] public int ScrollSourceIndex { get => UseCpuCamera ? 1 : 0; set => UseCpuCamera = value == 1; }
	[JsonIgnore] public bool UseHardwareScroll => !UseCpuCamera;
	public MetaCameraAxis CameraX { get; set; } = new() { Format = 2 };
	public MetaCameraAxis CameraY { get; set; } = new();
	private bool _useSavedCamera;
	public bool UseSavedCamera { get => _useSavedCamera; set { _useSavedCamera = value; PropertyChanged?.Invoke(this, new(nameof(UseSavedCamera))); PropertyChanged?.Invoke(this, new(nameof(CameraEditable))); } }
	[JsonIgnore] public bool CameraEditable => !UseSavedCamera;
	public int ScrollReferenceScanline { get; set; } = 32;
	public bool WrapX { get; set; }
	public bool WrapY { get; set; }
	public bool Enabled { get; set; }
	public List<MetaMemoryCondition> MemoryConditions { get; set; } = new();
	public override string ToString() => $"Background {Id:D3}" + (Name.Length > 0 ? $" — {Name}" : "");

	public void Validate()
	{
		if(Id < 1 || Name == null || ArtworkPath == null || NativeLayer < 0 || NativeLayer > 7) {
			throw new InvalidDataException("Invalid background layer identity.");
		}
		if(Animation == null) { throw new InvalidDataException("Missing background animation settings."); } Animation.Validate();
		if(DrawOrder < 0 || DrawOrder > 9) { throw new InvalidDataException("Background draw order must be 0 through 9."); }
		if(CompositionPosition < 0 || CompositionPosition > 6 || GbaPriority < 0 || GbaPriority > 3 || SnesPriority < 1 || SnesPriority > 15) { throw new InvalidDataException("Invalid PNG composition position or hardware priority."); }
		MetaSpritePlacement.ValidateCanvas(ArtworkWidth, ArtworkHeight, ArtworkScale);
		MetaMemoryCondition.ValidateAll(MemoryConditions);
		if(UseWidescreenPosition) { if(WidescreenPosition == null) { throw new InvalidDataException("Missing PNG gameplay positioning settings."); } WidescreenPosition.Validate(); }
		ValidateScrollSpeed(ScrollSpeedX); ValidateScrollSpeed(ScrollSpeedY);
		if(CameraX == null || CameraY == null) { throw new InvalidDataException("Missing CPU camera axes."); }
		if(!UseSavedCamera) { CameraX.Validate(); CameraY.Validate(); }
		if(ScrollReferenceScanline < 0 || ScrollReferenceScanline > 239) { throw new InvalidDataException("NES scroll reference scanline must be 0 through 239."); }
	}
	public static int ValidateScrollSpeed(double speed) {
		if(!double.IsFinite(speed) || speed < -16 || speed > 16) { throw new InvalidDataException("Scroll multipliers must be between -16 and 16."); }
		return (int)Math.Round(speed * 1000, MidpointRounding.AwayFromZero);
	}

	public bool TryGetArtworkPixel(int screenX, int screenY, int scrollX, int scrollY, out int artworkX, out int artworkY)
	{
		Validate();
		long x = screenX + (FollowScroll ? (long)Math.Floor((long)scrollX * ValidateScrollSpeed(ScrollSpeedX) / 1000.0) : 0) - X;
		long y = screenY + (FollowScroll ? (long)Math.Floor((long)scrollY * ValidateScrollSpeed(ScrollSpeedY) / 1000.0) : 0) - Y;
		int width = ArtworkWidth / ArtworkScale, height = ArtworkHeight / ArtworkScale;
		if(WrapX) { x = (x % width + width) % width; }
		if(WrapY) { y = (y % height + height) % height; }
		artworkX = artworkY = 0;
		if(x < 0 || y < 0 || x >= width || y >= height) { return false; }
		artworkX = (int)x * ArtworkScale;
		artworkY = (int)y * ArtworkScale;
		return true;
	}

	// Subpixel coordinates identify a sample inside one native output pixel.
	public bool TryGetArtworkPixel(int screenX, int screenY, int scrollX, int scrollY,
		int subpixelX, int subpixelY, int outputScale, out int artworkX, out int artworkY)
	{
		if(outputScale < 1 || subpixelX < 0 || subpixelY < 0 || subpixelX >= outputScale || subpixelY >= outputScale) {
			throw new ArgumentOutOfRangeException(nameof(outputScale), "Use a positive output scale and subpixel coordinates inside that pixel.");
		}
		if(!TryGetArtworkPixel(screenX, screenY, scrollX, scrollY, out artworkX, out artworkY)) { return false; }
		artworkX += (int)((long)subpixelX * ArtworkScale / outputScale);
		artworkY += (int)((long)subpixelY * ArtworkScale / outputScale);
		return true;
	}
}

public sealed class MetaCameraAxis : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private int _format;
	private int _pageSize = 256;
	private bool _signedPage;
	public bool SignedPage { get => _signedPage; set { _signedPage = value; PropertyChanged?.Invoke(this, new(nameof(SignedPage))); } }
	private string? _addressDraft, _pageDraft;
	public int Format { get => _format; set { _format = value; PropertyChanged?.Invoke(this, new(nameof(Format))); PropertyChanged?.Invoke(this, new(nameof(IsPaged))); } }
	public uint Address { get; set; }
	public uint PageAddress { get; set; }
	public int PageSize { get => _pageSize; set { _pageSize = value; PropertyChanged?.Invoke(this, new(nameof(PageSize))); } }
	[JsonIgnore] public bool IsPaged => Format == 4;
	[JsonIgnore] public string AddressHex { get => _addressDraft ?? Address.ToString("X4"); set { _addressDraft = value; if(TryAddress(value, out uint address)) { Address = address; } PropertyChanged?.Invoke(this, new(nameof(AddressHex))); } }
	[JsonIgnore] public string PageAddressHex { get => _pageDraft ?? PageAddress.ToString("X4"); set { _pageDraft = value; if(TryAddress(value, out uint address)) { PageAddress = address; } PropertyChanged?.Invoke(this, new(nameof(PageAddressHex))); } }
	private static bool TryAddress(string text, out uint address) => uint.TryParse(text.Trim().TrimStart('$'), System.Globalization.NumberStyles.AllowHexSpecifier, System.Globalization.CultureInfo.InvariantCulture, out address);
	public void Validate() {
		if(Format < 0 || Format > 4 || PageSize < 1 || PageSize > 65536 || ((Format == 2 || Format == 3) && Address == uint.MaxValue)
			|| (Format != 0 && _addressDraft != null && !TryAddress(_addressDraft, out _)) || (IsPaged && _pageDraft != null && !TryAddress(_pageDraft, out _))) { throw new InvalidDataException("Invalid CPU camera format, hex address or page size."); }
	}
	public IEnumerable<uint> Addresses() {
		Validate(); if(Format == 0) { yield break; } yield return Address;
		if(Format == 2 || Format == 3) { yield return Address + 1; }
		if(IsPaged) { yield return PageAddress; }
	}
	public int Decode(Func<uint, byte> read) {
		Validate(); if(Format == 0) { return 0; }
		int first = read(Address); if(Format == 1) { return first; }
		int second = read(IsPaged ? PageAddress : Address + 1);
		if(IsPaged && SignedPage && second >= 128) { second -= 256; }
		return Format == 3 ? first * 256 + second : second * (IsPaged ? PageSize : 256) + first;
	}
	public MetaCameraAxis Copy() { Validate(); return new() { Format = Format, Address = Address, PageAddress = PageAddress, PageSize = PageSize, SignedPage = SignedPage }; }
}

public static class MetaBackgroundRuntimePacket
{
	public const uint Magic = 0x3147424D; // MBG1
	public const uint ConditionalMagic = 0x3247424D; // MBG2 adds per-layer byte/mask conditions
	public const uint ComparisonMagic = 0x3347424D; // MBG3 adds comparison operators
	public const uint ScrollMagic = 0x3447424D; // MBG4 adds per-axis scroll multipliers
	public const uint RasterMagic = 0x3547424D; // MBG5 adds NES frame-wide scroll tracking
	public const uint HideLayerMagic = 0x3647424D; // MBG6 adds native layer suppression
	public const uint CameraMagic = 0x3747424D; // MBG7 adds CPU camera axes
	public const uint SignedCameraMagic = 0x3847424D; // MBG8 adds per-axis signed pages
	public const uint WidescreenMagic = 0x3947424D; // MBG9 adds decorative margin extension
	public const uint PositionMagic = 0x4147424D; // MBGA adds per-background gameplay positioning
	public const uint SmoothPositionMagic = 0x4247424D; // MBGB uses milli-pixel speeds and a release delay
	public const uint RamSpeedMagic = 0x4347424D; // MBGC adds player RAM speed
	public const uint CompositionMagic = 0x4447424D; // MBGD adds independent composition position
	public const uint GbaPriorityMagic = 0x4547424D; // MBGE adds independent GBA priority
	public const uint SnesPriorityMagic = 0x4647424D; // MBGF adds independent SNES priority
	public const uint NesBandsMagic = 0x4747424D; // MBGG insertion bands: NES, GB/GBC, PCE, SMS and WS
	public const uint AnimationMagic = 0x4847424D; // MBGH adds background PNG sequences.
	public static byte[] Create(MetaSpritePack pack, Func<MetaBackgroundLayer, (int Width, int Height, uint[] Pixels)> loadArtwork,
		Func<MetaPngAnimationFrame,int,(int Width,int Height,uint[] Pixels)>? loadFrame = null)
	{
		pack.ValidateSavedCamera();
		using MemoryStream stream = new();
		using BinaryWriter writer = new(stream);
		// Keep older conditionless definitions editable, but never activate them.
		// Stable sorting preserves saved order for legacy packs and equal slots.
		var layers = pack.BackgroundLayers.Where(layer => layer.Enabled && layer.MemoryConditions.Count > 0).OrderBy(layer => layer.CompositionPosition switch {4 => 0, 5 => 1, 0 => 2, 6 => 3, 1 => 5, _ => 4}).ThenBy(layer => layer.CompositionPosition == 2 ? -layer.GbaPriority : layer.CompositionPosition == 3 ? layer.SnesPriority : 0).ThenBy(layer => layer.DrawOrder).ToList();
		if(layers.Count > 128 || layers.Select(layer => layer.Id).Distinct().Count() != layers.Count) { throw new InvalidDataException("Invalid background layers."); }
		bool conditional = layers.Any(layer => layer.MemoryConditions.Count > 0);
		bool comparisons = layers.Any(layer => layer.MemoryConditions.Any(c => c.Comparison != MetaMemoryComparison.Equal));
		bool customScroll = layers.Any(layer => layer.ScrollSpeedX != 1 || layer.ScrollSpeedY != 1);
		bool rasterScroll = layers.Any(layer => layer.IgnoreRasterScroll);
		bool hideLayer = layers.Any(layer => layer.HideOriginalLayer);
		bool camera = layers.Any(layer => layer.UseCpuCamera);
		bool signedCamera = layers.Any(layer => layer.UseCpuCamera && (layer.UseSavedCamera ? pack.SavedCamera!.X.SignedPage || pack.SavedCamera.Y.SignedPage : layer.CameraX.SignedPage || layer.CameraY.SignedPage));
		bool position = layers.Any(layer => layer.UseWidescreenPosition);
		bool ramSpeed = layers.Any(layer => layer.UseWidescreenPosition && layer.WidescreenPosition.RamSpeed.Enabled);
		bool composition = layers.Any(layer => layer.CompositionPosition != 0);
		bool gbaPriority = layers.Any(layer => layer.CompositionPosition == 2);
		bool snesPriority = layers.Any(layer => layer.CompositionPosition == 3);
		bool nesBands = layers.Any(layer => layer.CompositionPosition >= 4);
		bool animated = layers.Any(layer => layer.Animation.Enabled);
		if(gbaPriority && pack.System != "Gba") { throw new InvalidDataException("Independent GBA priority requires a GBA pack."); }
		if(snesPriority && pack.System != "Snes") { throw new InvalidDataException("Independent SNES priority requires a SNES pack."); }
		if(nesBands && pack.System != "Nes" && pack.System != "Gameboy" && pack.System != "Pce" && pack.System != "Sms" && pack.System != "Ws") { throw new InvalidDataException("Sprite insertion bands require a NES, Game Boy, PCE, SMS or WonderSwan pack."); }
		nesBands |= animated; composition |= animated; // MBGH retains every MBGG field.
		snesPriority |= nesBands; // MBGG retains all MBGF fields.
		gbaPriority |= snesPriority; // MBGF retains all MBGE fields.
		ramSpeed |= composition;
		position |= composition;
		bool widescreen = position || layers.Any(layer => layer.ExtendIntoMargins);
		signedCamera |= widescreen;
		camera |= widescreen;
		hideLayer |= camera;
		rasterScroll |= hideLayer;
		customScroll |= rasterScroll;
		comparisons |= customScroll;
		writer.Write(animated ? AnimationMagic : nesBands ? NesBandsMagic : snesPriority ? SnesPriorityMagic : gbaPriority ? GbaPriorityMagic : composition ? CompositionMagic : ramSpeed ? RamSpeedMagic : position ? SmoothPositionMagic : widescreen ? WidescreenMagic : signedCamera ? SignedCameraMagic : camera ? CameraMagic : hideLayer ? HideLayerMagic : rasterScroll ? RasterMagic : customScroll ? ScrollMagic : comparisons ? ComparisonMagic : conditional ? ConditionalMagic : Magic); writer.Write(layers.Count);
		foreach(var layer in layers) {
			layer.Validate();
			var artwork = loadArtwork(layer);
			if(artwork.Width != layer.ArtworkWidth || artwork.Height != layer.ArtworkHeight || artwork.Pixels.Length != checked(artwork.Width * artwork.Height)) {
				throw new InvalidDataException("Background PNG dimensions changed.");
			}
			writer.Write(layer.Id); writer.Write(layer.NativeLayer); writer.Write(layer.X); writer.Write(layer.Y);
			writer.Write((layer.FollowScroll ? 1 : 0) | (layer.WrapX ? 2 : 0) | (layer.WrapY ? 4 : 0));
			writer.Write(layer.ArtworkScale); writer.Write(artwork.Width); writer.Write(artwork.Height);
			if(customScroll) { writer.Write(MetaBackgroundLayer.ValidateScrollSpeed(layer.ScrollSpeedX)); writer.Write(MetaBackgroundLayer.ValidateScrollSpeed(layer.ScrollSpeedY)); }
			if(rasterScroll) { writer.Write(layer.IgnoreRasterScroll ? 1 : 0); writer.Write(layer.ScrollReferenceScanline); }
			if(hideLayer) { writer.Write(layer.HideOriginalLayer ? 1 : 0); }
			if(camera) {
				writer.Write(layer.UseCpuCamera ? 1 : 0);
				foreach(var axis in layer.UseSavedCamera ? new[] {pack.SavedCamera!.X, pack.SavedCamera.Y} : new[] {layer.CameraX, layer.CameraY}) { writer.Write(axis.Format); writer.Write(axis.Address); writer.Write(axis.PageAddress); writer.Write(axis.PageSize); if(signedCamera) { writer.Write(axis.SignedPage ? 1 : 0); } }
			}
			if(widescreen) { writer.Write(layer.ExtendIntoMargins ? 1 : 0); }
			if(position) {
				var p = layer.UseWidescreenPosition ? layer.WidescreenPosition : new MetaWidescreenPosition();
				writer.Write(layer.UseWidescreenPosition ? 1 : 0); writer.Write(p.Position); writer.Write(p.CameraMin); writer.Write(p.CameraMax); writer.Write((int)Math.Round(p.SlideSpeed * 1000)); writer.Write(p.ReleaseDelay); writer.Write(p.UseRamFlags ? 1 : 0);
				foreach(string text in new[] {p.LeftFlags, p.RightFlags}) {
					var flags = p.UseRamFlags ? p.ParseFlags(text) : new List<MetaMemoryCondition>(); writer.Write(flags.Count);
					foreach(var c in flags) { writer.Write(c.Address); writer.Write(c.Value); writer.Write(c.Mask); writer.Write((int)c.Comparison); }
				}
				if(ramSpeed) { var s = p.RamSpeed; writer.Write(s.Enabled ? 1 : 0); writer.Write(s.Format); writer.Write(s.Address); writer.Write((uint)Math.Round(s.Multiplier * 1000000)); writer.Write((uint)Math.Round(s.Maximum * 1000)); }
			}
			if(composition) { writer.Write(layer.CompositionPosition); }
			if(gbaPriority) { writer.Write(layer.GbaPriority); }
			if(snesPriority) { writer.Write(layer.SnesPriority); }
			foreach(uint pixel in artwork.Pixels) { writer.Write(pixel); }
			if(conditional) {
				writer.Write(layer.MemoryConditions.Count);
				foreach(var condition in layer.MemoryConditions) {
					writer.Write(condition.Address); writer.Write(condition.Value); writer.Write(condition.Mask);
					if(comparisons) { writer.Write((int)condition.Comparison); }
				}
			}
			if(animated) { MetaSpriteRuntimePacket.WriteAnimation(writer,layer.Animation,layer.ArtworkScale,0,0,loadFrame==null ? null : (frame,scale) => {
				var image=loadFrame(frame,scale); MetaSpritePlacement.ValidateCanvas(image.Width,image.Height,scale); return image;
			}); }
		}
		return stream.ToArray();
	}
}

public sealed class MetaSpriteGroup : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private string _name = "";
	private string _poseName = "";
	public int PoseFamilyId { get; set; }
	public string PoseName { get => _poseName; set { _poseName = value; PropertyChanged?.Invoke(this, new(nameof(PoseName))); PropertyChanged?.Invoke(this, new(nameof(DisplayName))); } }
	private int _originX, _originY;
	private int _replacementScale = 1, _replacementOriginX, _replacementOriginY;
	private bool _enabled;
	public bool Enabled { get => _enabled; set { _enabled = value; PropertyChanged?.Invoke(this, new(nameof(Enabled))); } }
	public int Id { get; set; }
	public string Name { get => _name; set { _name = value; PropertyChanged?.Invoke(this, new(nameof(Name))); PropertyChanged?.Invoke(this, new(nameof(DisplayName))); } }
	public int Width { get; set; }
	public int Height { get; set; }
	public int OriginX { get => _originX; set { _originX = value; PropertyChanged?.Invoke(this, new(nameof(OriginX))); } }
	public int OriginY { get => _originY; set { _originY = value; PropertyChanged?.Invoke(this, new(nameof(OriginY))); } }
	public int ReplacementScale { get => _replacementScale; set { _replacementScale = value; PropertyChanged?.Invoke(this, new(nameof(ReplacementScale))); } }
	// Replacement origin is independent of the captured canvas, allowing larger artwork.
	public int ReplacementOriginX { get => _replacementOriginX; set { _replacementOriginX = value; PropertyChanged?.Invoke(this, new(nameof(ReplacementOriginX))); } }
	public int ReplacementOriginY { get => _replacementOriginY; set { _replacementOriginY = value; PropertyChanged?.Invoke(this, new(nameof(ReplacementOriginY))); } }
	public int ReplacementWidth { get; set; }
	public int ReplacementHeight { get; set; }
	public string ReplacementPath { get; set; } = "";
	public MetaPngAnimation Animation { get; set; } = new();
	public List<MetaSpriteMember> Members { get; set; } = new();
	public List<MetaMemoryCondition> MemoryConditions { get; set; } = new();
	public List<MetaAlternateArtwork> AlternateArtwork { get; set; } = new();
	[JsonIgnore]
	public string DisplayName => $"Meta {Id:D3}" + (Name.Length > 0 ? $" — {Name}" : "") + (PoseName.Length > 0 ? $" ({PoseName})" : "");
	public override string ToString() => DisplayName;

	public void LinkAsPoseOf(MetaSpriteGroup source)
	{
		if(source.PoseFamilyId == 0) { source.PoseFamilyId = source.Id; }
		PoseFamilyId = source.PoseFamilyId;
		Name = source.Name;
		// Preserve the anchor's offset from bottom center across differently sized poses.
		OriginX = Width / 2 + source.OriginX - source.Width / 2;
		OriginY = Height + source.OriginY - source.Height;
		ReplacementScale = source.ReplacementScale;
	}
	public void UseExportedArtwork(string path)
	{
		if(string.IsNullOrWhiteSpace(path)) { throw new InvalidDataException("Exported artwork needs a PNG path."); }
		MetaSpritePlacement.ValidateCanvas(Width, Height, 1);
		ReplacementPath = path;
		ReplacementWidth = Width; ReplacementHeight = Height; ReplacementScale = 1;
		ReplacementOriginX = OriginX; ReplacementOriginY = OriginY;
		Enabled = true;
	}
}

// Conditions refer to the owning console CPU's mapped byte addresses.
// Missing snapshots fail closed; an empty list is unconditional.
public enum MetaMemoryComparison { Equal, NotEqual, Greater, Less, GreaterOrEqual, LessOrEqual }

public sealed class MetaMemoryCondition
{
	public uint Address { get; set; }
	public int Value { get; set; }
	public int Mask { get; set; } = 255;
	public MetaMemoryComparison Comparison { get; set; } = MetaMemoryComparison.Equal;
	public void Validate()
	{
		if(Value < 0 || Value > 255 || Mask < 0 || Mask > 255) { throw new InvalidDataException("Memory conditions require byte values and masks."); }
		if(!Enum.IsDefined(Comparison)) { throw new InvalidDataException("Invalid memory comparison operator."); }
	}
	public static void ValidateAll(List<MetaMemoryCondition> conditions)
	{
		if(conditions == null || conditions.Count > 64 || conditions.Any(c => c == null)) { throw new InvalidDataException("Invalid memory condition list."); }
		foreach(var condition in conditions) { condition.Validate(); }
	}
	public static bool Matches(List<MetaMemoryCondition> conditions, Func<uint, byte?> readSnapshot)
	{
		ValidateAll(conditions);
		foreach(var condition in conditions) {
			byte? value = readSnapshot(condition.Address);
			if(!value.HasValue) { return false; }
			int left = value.Value & condition.Mask, right = condition.Value & condition.Mask;
			bool matches = condition.Comparison switch {
				MetaMemoryComparison.Equal => left == right, MetaMemoryComparison.NotEqual => left != right,
				MetaMemoryComparison.Greater => left > right, MetaMemoryComparison.Less => left < right,
				MetaMemoryComparison.GreaterOrEqual => left >= right, MetaMemoryComparison.LessOrEqual => left <= right,
				_ => false
			};
			if(!matches) { return false; }
		}
		return true;
	}
}

public static class MetaMemoryConditionText
{
	public static List<MetaMemoryCondition> Parse(string text)
	{
		List<MetaMemoryCondition> result = new();
		foreach(string line in text.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)) {
			if(string.IsNullOrWhiteSpace(line)) { continue; }
			var match = System.Text.RegularExpressions.Regex.Match(line, @"^\s*(\$?[0-9A-Fa-f]+|0[xX][0-9A-Fa-f]+)\s*(==|!=|>=|<=|>|<|=)\s*(\$?[0-9A-Fa-f]+|0[xX][0-9A-Fa-f]+)(?:\s*/\s*(\$?[0-9A-Fa-f]+|0[xX][0-9A-Fa-f]+))?\s*$");
			if(!match.Success) { throw new InvalidDataException("Use one hexadecimal ADDRESS OP VALUE / MASK condition per line; OP is ==, !=, >, <, >= or <=."); }
			static uint Hex(string value) {
				if(value.StartsWith("$")) { value = value[1..]; }
				else if(value.StartsWith("0x", StringComparison.OrdinalIgnoreCase)) { value = value[2..]; }
				try { return Convert.ToUInt32(value, 16); }
				catch(Exception ex) when(ex is FormatException || ex is OverflowException || ex is ArgumentException) { throw new InvalidDataException("Conditions require hexadecimal addresses, byte values, and masks."); }
			}
			uint value = Hex(match.Groups[3].Value), mask = match.Groups[4].Success ? Hex(match.Groups[4].Value) : 255;
			if(value > 255 || mask > 255) { throw new InvalidDataException("Condition values and masks must be 00 through FF."); }
			MetaMemoryComparison comparison = match.Groups[2].Value switch {
				"!=" => MetaMemoryComparison.NotEqual, ">" => MetaMemoryComparison.Greater, "<" => MetaMemoryComparison.Less,
				">=" => MetaMemoryComparison.GreaterOrEqual, "<=" => MetaMemoryComparison.LessOrEqual, _ => MetaMemoryComparison.Equal
			};
			result.Add(new() { Address = Hex(match.Groups[1].Value), Value = (int)value, Mask = (int)mask, Comparison = comparison });
		}
		MetaMemoryCondition.ValidateAll(result);
		return result;
	}
	public static string Format(List<MetaMemoryCondition> conditions) => string.Join(Environment.NewLine,
		conditions.Select(c => $"${c.Address:X} {Operator(c.Comparison)} ${c.Value:X2} / ${c.Mask:X2}"));
	public static string Operator(MetaMemoryComparison comparison) => comparison switch {
		MetaMemoryComparison.Equal => "==", MetaMemoryComparison.NotEqual => "!=", MetaMemoryComparison.Greater => ">",
		MetaMemoryComparison.Less => "<", MetaMemoryComparison.GreaterOrEqual => ">=", MetaMemoryComparison.LessOrEqual => "<=",
		_ => throw new InvalidDataException("Invalid memory comparison operator.")
	};
}

public sealed class MetaAlternateArtwork : INotifyPropertyChanged
{
	public event PropertyChangedEventHandler? PropertyChanged;
	private int _scale = 1, _originX, _originY;
	private string _name = "";
	public string Name { get => _name; set { if(_name == value) { return; } _name = value; PropertyChanged?.Invoke(this, new(nameof(Name))); } }
	public override string ToString() => Name.Length > 0 ? Name : System.IO.Path.GetFileName(Path);
	public string Path { get; set; } = "";
	public MetaPngAnimation Animation { get; set; } = new();
	public int Scale { get => _scale; set { if(_scale == value) { return; } _scale = value; PropertyChanged?.Invoke(this, new(nameof(Scale))); } }
	public int OriginX { get => _originX; set { if(_originX == value) { return; } _originX = value; PropertyChanged?.Invoke(this, new(nameof(OriginX))); } }
	public int OriginY { get => _originY; set { if(_originY == value) { return; } _originY = value; PropertyChanged?.Invoke(this, new(nameof(OriginY))); } }
	public List<MetaMemoryCondition> MemoryConditions { get; set; } = new();
	public void Validate()
	{
		if(string.IsNullOrWhiteSpace(Path) || Scale < 1 || Scale > 10 || MemoryConditions == null) { throw new InvalidDataException("Alternate artwork requires a PNG, scale, and a memory condition list."); }
		if(Animation == null) { throw new InvalidDataException("Missing alternate animation settings."); } Animation.Validate();
		if(OriginX < -4096 || OriginX > 4096 || OriginY < -4096 || OriginY > 4096) { throw new InvalidDataException("Alternate artwork origin is out of range."); }
		MetaMemoryCondition.ValidateAll(MemoryConditions);
	}
}

public static class MetaSpritePlacement
{
	// Output coordinates are expressed in the selected HD output scale.
	public static (int X, int Y) GetTopLeft(MetaSpriteGroup group, int worldOriginX, int worldOriginY, int outputScale)
	{
		if(outputScale < 1) { throw new ArgumentOutOfRangeException(nameof(outputScale)); }
		return (checked((worldOriginX - group.ReplacementOriginX) * outputScale), checked((worldOriginY - group.ReplacementOriginY) * outputScale));
	}

	public static void ValidateCanvas(int width, int height, int artworkScale)
	{
		ValidateSpriteCanvas(width, height, artworkScale);
		if(width % artworkScale != 0 || height % artworkScale != 0) {
			throw new InvalidDataException("Use a PNG canvas up to 8192 pixels with dimensions divisible by the chosen scale (1–10).");
		}
	}
	public static void ValidateSpriteCanvas(int width, int height, int artworkScale)
	{
		if(artworkScale < 1 || artworkScale > 10 || width < 1 || height < 1 || width > 8192 || height > 8192) {
			throw new InvalidDataException("Use a PNG canvas from 1 to 8192 pixels and a scale from 1 to 10.");
		}
	}
}

// A candidate's coordinates are the current frame position. Members store positions relative to the group canvas.
// Exact RGBA matching is the initial policy; palette-independent pattern matching will be added with runtime capture.
public static class MetaSpriteMatcher
{
	public static List<(int X, int Y, int[] Members)> FindMatches(MetaSpriteGroup group, IReadOnlyList<MetaSpriteMember> candidates)
	{
		List<(int X, int Y, int[] Members)> matches = new();
		if(group.Members.Count == 0) { return matches; }
		MetaSpriteMember anchor = group.Members[0];
		for(int i = 0; i < candidates.Count; i++) {
			if(!SameGraphic(anchor, candidates[i])) { continue; }
			int left = candidates[i].X - anchor.X, top = candidates[i].Y - anchor.Y;
			List<int> used = new() { i };
			for(int memberIndex = 1; memberIndex < group.Members.Count; memberIndex++) {
				MetaSpriteMember member = group.Members[memberIndex];
				int found = -1;
				for(int j = 0; j < candidates.Count; j++) {
					MetaSpriteMember candidate = candidates[j];
					// Reject unrelated positions before walking the matched members.
					if(candidate.X != left + member.X || candidate.Y != top + member.Y || !SameGraphic(member, candidate)) { continue; }
					bool alreadyUsed = false;
					foreach(int index in used) {
						if(index == j || (candidate.RuntimeSpriteIndex >= 0 && candidates[index].RuntimeSpriteIndex == candidate.RuntimeSpriteIndex)) {
							alreadyUsed = true;
							break;
						}
					}
					if(alreadyUsed) { continue; }
					found = j;
					break;
				}
				if(found < 0) { break; }
				used.Add(found);
			}
			if(used.Count == group.Members.Count) { matches.Add((left + group.OriginX, top + group.OriginY, used.ToArray())); }
		}
		return matches;
	}

	private static bool SameGraphic(MetaSpriteMember a, MetaSpriteMember b) => a.Width == b.Width && a.Height == b.Height && a.Pixels.AsSpan().SequenceEqual(b.Pixels);
}

public sealed class MetaSpriteMember
{
	// Used only to avoid counting wrapped copies twice; never persisted or used as graphic identity.
	[JsonIgnore] public int RuntimeSpriteIndex { get; set; } = -1;
	public int X { get; set; }
	public int Y { get; set; }
	public int Width { get; set; }
	public int Height { get; set; }
	// Transparent BGRA snapshot. No OAM index or VRAM address is used as identity.
	public uint[] Pixels { get; set; } = Array.Empty<uint>();
}

[JsonSerializable(typeof(MetaSpritePack))]
[JsonSourceGenerationOptions(WriteIndented = true)]
internal partial class MetaSpriteSerializerContext : JsonSerializerContext { }

public static class MetaSpriteRuntimePacket
{
	public const uint Magic = 0x3150534D; // MSP1, little endian
	public const uint ConditionalMagic = 0x3250534D; // MSP2 adds per-group byte/mask conditions
	public const uint AlternateMagic = 0x3350534D; // MSP3 adds alternate PNGs
	public const uint ComparisonMagic = 0x3450534D; // MSP4 adds comparison operators
	public const uint AnimationMagic = 0x3550534D; // MSP5 adds default/alternate animation sequences.
	internal static void WriteAnimation(BinaryWriter writer, MetaPngAnimation animation, int scale, int originX, int originY,
		Func<MetaPngAnimationFrame, int, (int Width, int Height, uint[] Pixels)>? loadFrame)
	{
		animation.Validate();
		if(!animation.Enabled) { writer.Write(0); return; }
		if(loadFrame == null) { throw new InvalidDataException("Animation PNG loader is required."); }
		writer.Write(animation.Frames.Count); writer.Write((int)animation.Playback); writer.Write(animation.RestartOnActivation ? 1 : 0);
		foreach(var frame in animation.Frames) {
			var image=loadFrame(frame,scale); MetaSpritePlacement.ValidateSpriteCanvas(image.Width,image.Height,scale);
			if(image.Pixels.Length!=checked(image.Width*image.Height)) { throw new InvalidDataException("Invalid animation PNG pixels."); }
			if(writer.BaseStream.Length+20L+image.Pixels.LongLength*4>536870912) { throw new InvalidDataException("Animation replacement packet exceeds 512 MiB."); }
			writer.Write(frame.Duration); writer.Write(frame.OriginX ?? originX); writer.Write(frame.OriginY ?? originY);
			writer.Write(image.Width); writer.Write(image.Height); foreach(uint pixel in image.Pixels) { writer.Write(pixel); }
		}
	}
	public static byte[] Create(MetaSpritePack pack, Func<MetaSpriteGroup, (int Width, int Height, uint[] Pixels)> loadArtwork,
		Func<MetaAlternateArtwork, (int Width, int Height, uint[] Pixels)>? loadAlternate = null,
		Func<MetaPngAnimationFrame, int, (int Width, int Height, uint[] Pixels)>? loadFrame = null)
	{
		using MemoryStream stream = new();
		using BinaryWriter writer = new(stream);
		var groups = pack.Groups.Where(g => g.Enabled).ToList();
		bool animated = groups.Any(g => g.Animation.Enabled || g.AlternateArtwork.Any(a => a.MemoryConditions.Count>0 && a.Animation.Enabled));
		bool alternates = groups.Any(g => g.AlternateArtwork.Any(a => a.MemoryConditions.Count > 0));
		bool comparisons = groups.Any(g => g.MemoryConditions.Any(c => c.Comparison != MetaMemoryComparison.Equal)
			|| g.AlternateArtwork.Where(a => a.MemoryConditions.Count > 0).Any(a => a.MemoryConditions.Any(c => c.Comparison != MetaMemoryComparison.Equal)));
		comparisons |= animated;
		alternates |= comparisons; // MSP4 retains the alternate-image count even when zero.
		bool conditional = alternates || groups.Any(g => g.MemoryConditions.Count > 0);
		writer.Write(animated ? AnimationMagic : comparisons ? ComparisonMagic : alternates ? AlternateMagic : conditional ? ConditionalMagic : Magic);
		writer.Write(groups.Count);
		foreach(var group in groups) {
			var artwork = loadArtwork(group);
			MetaSpritePlacement.ValidateSpriteCanvas(artwork.Width, artwork.Height, group.ReplacementScale);
			if(artwork.Pixels.Length != checked(artwork.Width * artwork.Height)) { throw new InvalidDataException("Invalid replacement pixel buffer."); }
			writer.Write(group.Id);
			writer.Write(group.OriginX); writer.Write(group.OriginY);
			writer.Write(group.ReplacementOriginX); writer.Write(group.ReplacementOriginY);
			writer.Write(group.ReplacementScale);
			writer.Write(artwork.Width); writer.Write(artwork.Height);
			writer.Write(group.Members.Count);
			foreach(var member in group.Members) {
				writer.Write(member.X); writer.Write(member.Y); writer.Write(member.Width); writer.Write(member.Height);
				foreach(uint pixel in member.Pixels) { writer.Write(pixel); }
			}
			foreach(uint pixel in artwork.Pixels) { writer.Write(pixel); }
			if(conditional) {
				MetaMemoryCondition.ValidateAll(group.MemoryConditions);
				writer.Write(group.MemoryConditions.Count);
				foreach(var condition in group.MemoryConditions) {
					writer.Write(condition.Address); writer.Write(condition.Value); writer.Write(condition.Mask);
					if(comparisons) { writer.Write((int)condition.Comparison); }
				}
			}
			if(alternates) {
				if(group.AlternateArtwork.Count > 64) { throw new InvalidDataException("Too many alternate PNGs."); }
				var activeAlternates = group.AlternateArtwork.Where(a => a.MemoryConditions.Count > 0).ToList();
				writer.Write(activeAlternates.Count);
				foreach(var alternate in activeAlternates) {
					alternate.Validate();
					if(loadAlternate == null) { throw new InvalidDataException("Alternate PNG loader is required."); }
					var image = loadAlternate(alternate);
					MetaSpritePlacement.ValidateSpriteCanvas(image.Width, image.Height, alternate.Scale);
					if(image.Pixels.Length != checked(image.Width * image.Height)) { throw new InvalidDataException("Invalid alternate PNG pixels."); }
					writer.Write(alternate.OriginX); writer.Write(alternate.OriginY); writer.Write(alternate.Scale);
					writer.Write(image.Width); writer.Write(image.Height);
					foreach(uint pixel in image.Pixels) { writer.Write(pixel); }
					writer.Write(alternate.MemoryConditions.Count);
					foreach(var condition in alternate.MemoryConditions) {
						writer.Write(condition.Address); writer.Write(condition.Value); writer.Write(condition.Mask);
						if(comparisons) { writer.Write((int)condition.Comparison); }
					}
					if(animated) { WriteAnimation(writer,alternate.Animation,alternate.Scale,alternate.OriginX,alternate.OriginY,loadFrame); }
				}
			}
			if(animated) { WriteAnimation(writer,group.Animation,group.ReplacementScale,group.ReplacementOriginX,group.ReplacementOriginY,loadFrame); }
		}
		return stream.ToArray();
	}
}

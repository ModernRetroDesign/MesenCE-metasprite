using Avalonia;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Config;
using Mesen.Debugger.Utilities;
using Mesen.Interop;
using Mesen.Utilities;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Mesen.Debugger.ViewModels;

public partial class SpriteViewerViewModel
{
	public ObservableCollection<MetaSpriteGroup> MetaGroups { get; } = new();
	[ObservableProperty] public partial MetaSpriteGroup? SelectedMetaGroup { get; set; }
	[ObservableProperty] public partial string MetaStatus { get; set; } = "Ctrl-click sprites to select group members.";
	[ObservableProperty] public partial bool SetMetaOrigin { get; set; }
	[ObservableProperty] public partial bool SelectMetaRectangle { get; set; }
	[ObservableProperty] public partial bool ShowMetaMatches { get; set; }
	[ObservableProperty] public partial string MetaMatchStatus { get; set; } = "";
	private readonly Dictionary<int, HashSet<int>> _matchedMetaGroupsBySprite = new();
	[ObservableProperty] public partial Bitmap? MetaPreview { get; set; }
	[ObservableProperty] public partial double MetaPreviewWidth { get; set; } = 1;
	[ObservableProperty] public partial double MetaPreviewHeight { get; set; } = 1;
	[ObservableProperty] public partial int MetaPreviewOriginX { get; set; }
	[ObservableProperty] public partial int MetaPreviewOriginY { get; set; }
	public bool PreviewUsesReplacement { get; private set; }
	private readonly HashSet<int> _metaSelection = new();
	private readonly Dictionary<int, PixelPoint> _metaWrapOffsets = new();
	private MetaSpritePack _metaPack = new();
	private string _metaPackPath = "";
	private bool _metaLoadFailed;
	private MetaSpriteGroup? _removedMetaGroup;
	private int _removedMetaGroupIndex;
	public bool MetaSelectionDragging { get; set; }

	partial void OnShowMetaMatchesChanged(bool value) => RefreshTab();

	private unsafe void UpdateLiveMetaMatches()
	{
		_matchedMetaGroupsBySprite.Clear();
		MetaMatchStatus = "";
		if(!ShowMetaMatches || MetaGroups.Count == 0 || _data.PpuState == null || _data.PpuToolsState == null || _data.Palette == null) { return; }
		try {
			DebugSpriteInfo[] sprites = Array.Empty<DebugSpriteInfo>();
			uint[] pixels = Array.Empty<uint>();
			uint[] frame = new uint[ViewerBitmap.PixelSize.Width * ViewerBitmap.PixelSize.Height];
			fixed(uint* output = frame) {
				DebugApi.GetSpriteList(ref sprites, ref pixels, CpuType, new GetSpritePreviewOptions() { Background = SpriteBackground.Transparent, Filter = Config.Filter },
					_data.PpuState, _data.PpuToolsState, _data.Vram, _data.SpriteRam, _data.Palette.Value.GetRgbPalette(), (IntPtr)output);
			}
			List<MetaSpriteMember> candidates = new();
			foreach(var sprite in SpritePreviews.Where(s => s.Visibility != SpriteVisibility.Disabled && s.Width > 0 && s.Height > 0)) {
				uint[] graphic = new uint[sprite.Width * sprite.Height];
				Array.Copy(pixels, sprite.SpriteIndex * 128 * 128, graphic, 0, graphic.Length);
				var (main, a, b, c) = sprite.GetPreviewRect();
				foreach(var rect in new[] { main, a, b, c }.Where(r => r != default)) {
					candidates.Add(new() { X = (int)rect.X, Y = (int)rect.Y, Width = sprite.Width, Height = sprite.Height, Pixels = graphic, RuntimeSpriteIndex = sprite.SpriteIndex });
				}
			}
			List<string> descriptions = new();
			Dictionary<int, HashSet<int>> enabledClaims = new();
			List<Rect> highlights = ViewerBitmap.HighlightRects ?? new();
			foreach(var group in MetaGroups) {
				foreach(var match in MetaSpriteMatcher.FindMatches(group, candidates)) {
					descriptions.Add($"Meta {group.Id:D3}: origin {match.X}, {match.Y}");
					highlights.Add(new Rect(match.X - group.OriginX, match.Y - group.OriginY, group.Width, group.Height));
					highlights.Add(new Rect(match.X - 3, match.Y, 7, 1));
					highlights.Add(new Rect(match.X, match.Y - 3, 1, 7));
					foreach(int member in match.Members) {
						int index = candidates[member].RuntimeSpriteIndex;
						if(!_matchedMetaGroupsBySprite.TryGetValue(index, out var groups)) { _matchedMetaGroupsBySprite[index] = groups = new(); }
						groups.Add(group.Id);
						if(group.Enabled) {
							if(!enabledClaims.TryGetValue(index, out var claims)) { enabledClaims[index] = claims = new(); }
							claims.Add(group.Id);
						}
					}
				}
			}
			var conflicts = enabledClaims.Where(c => c.Value.Count > 1).ToList();
			if(conflicts.Count > 0) {
				descriptions.Add("Overlapping enabled groups in this snapshot. Earlier groups have gameplay priority; move a group earlier/later or disable a conflicting group.");
				foreach(var conflict in conflicts.Take(8)) {
					descriptions.Add($"Sprite {conflict.Key}: " + string.Join(", ", MetaGroups.Where(g => conflict.Value.Contains(g.Id)).Select(g => $"Meta {g.Id:D3}")));
				}
				if(conflicts.Count > 8) { descriptions.Add($"And {conflicts.Count - 8} more overlapping sprites."); }
			}
			ViewerBitmap.HighlightRects = highlights;
			MetaMatchStatus = descriptions.Count == 0 ? "No matching groups in this snapshot." : string.Join(Environment.NewLine, descriptions);
		} catch(Exception ex) { MetaMatchStatus = "Could not recognize groups: " + ex.Message; }
	}

	protected override void DisposeView()
	{
		if(SelectedMetaGroup != null) { SelectedMetaGroup.PropertyChanged -= MetaGroupPropertyChanged; }
		MetaPreview?.Dispose();
		base.DisposeView();
	}

	public void LoadMetaGroups()
	{
		_removedMetaGroup = null;
		_metaSelection.Clear();
		_metaWrapOffsets.Clear();
		SelectedMetaGroup = null;
		MetaGroups.Clear();
		_metaLoadFailed = false;
		try {
			string hash = EmuApi.GetRomHash(HashType.Sha1);
			_metaPackPath = MetaSpriteRuntimeLoader.GetPackPath(hash, CpuType);
			_metaPack = MetaSpritePack.Load(_metaPackPath, hash, CpuType.ToString());
			foreach(MetaSpriteGroup group in _metaPack.Groups) { MetaGroups.Add(group); }
			MetaStatus = "Ctrl-click sprites to select group members.";
			if(_metaPack.Groups.Any(g => g.Enabled)) { ApplyMetaReplacements(); }
		} catch(Exception ex) {
			_metaLoadFailed = true;
			MetaStatus = "Could not load groups: " + ex.Message;
		}
	}

	public void ToggleMetaSprite(SpritePreviewModel sprite, Rect? clickedRect = null)
	{
		if(!_metaSelection.Add(sprite.SpriteIndex)) {
			_metaSelection.Remove(sprite.SpriteIndex);
			_metaWrapOffsets.Remove(sprite.SpriteIndex);
		} else {
			Rect rect = clickedRect ?? sprite.GetPreviewRect().Item1;
			_metaWrapOffsets[sprite.SpriteIndex] = new PixelPoint((int)rect.X - sprite.PreviewX, (int)rect.Y - sprite.PreviewY);
		}
		MetaStatus = $"{_metaSelection.Count} sprites selected. Create a group to capture this pose.";
		UpdateMetaSelection();
	}

	public void SelectMetaSprites(Rect rectangle)
	{
		_metaSelection.Clear();
		_metaWrapOffsets.Clear();
		foreach(var sprite in SpritePreviews) {
			var (main, a, b, c) = sprite.GetPreviewRect();
			var copies = new[] { main, a, b, c }.Where(r => r != default && r.Intersects(rectangle))
				.OrderByDescending(r => { Rect intersection = r.Intersect(rectangle); return intersection.Width * intersection.Height; }).ToList();
			if(copies.Count > 0) {
				_metaSelection.Add(sprite.SpriteIndex);
				_metaWrapOffsets[sprite.SpriteIndex] = new PixelPoint((int)copies[0].X - sprite.PreviewX, (int)copies[0].Y - sprite.PreviewY);
			}
		}
		MetaStatus = $"{_metaSelection.Count} sprites selected.";
		UpdateMetaSelection();
	}

	public void ClearMetaSelection()
	{
		_metaSelection.Clear();
		_metaWrapOffsets.Clear();
		SelectionRect = default;
		MetaStatus = "Ctrl-click sprites to select group members.";
	}

	private PixelPoint MetaPosition(SpritePreviewModel sprite)
	{
		_metaWrapOffsets.TryGetValue(sprite.SpriteIndex, out PixelPoint offset);
		return new PixelPoint(sprite.PreviewX + offset.X, sprite.PreviewY + offset.Y);
	}

	private void UpdateMetaSelection()
	{
		var members = SpritePreviews.Where(s => _metaSelection.Contains(s.SpriteIndex)).ToList();
		if(members.Count > 0) {
			double left = members.Min(s => MetaPosition(s).X), top = members.Min(s => MetaPosition(s).Y);
			SelectionRect = new Rect(left, top, members.Max(s => MetaPosition(s).X + s.Width) - left, members.Max(s => MetaPosition(s).Y + s.Height) - top);
		} else {
			SelectionRect = default;
		}
	}

	public unsafe void CreateMetaGroup(bool asPose = false)
	{
		MetaSpriteGroup? poseSource = asPose ? SelectedMetaGroup : null;
		if(asPose && poseSource == null) { MetaStatus = "Choose an existing group before adding a pose."; return; }
		if(_metaLoadFailed || _data.PpuState == null || _data.PpuToolsState == null || _data.Palette == null) { return; }
		var selected = SpritePreviews.Where(s => _metaSelection.Contains(s.SpriteIndex) && s.Width > 0 && s.Height > 0).OrderBy(s => s.SpriteIndex).ToList();
		if(selected.Count == 0) { MetaStatus = "Select sprites with Ctrl-click first."; return; }
		try {
			// Obtain transparent previews independently of the viewer's backdrop setting.
			DebugSpriteInfo[] sprites = Array.Empty<DebugSpriteInfo>();
			uint[] previews = Array.Empty<uint>();
			uint[] frame = new uint[ViewerBitmap.PixelSize.Width * ViewerBitmap.PixelSize.Height];
			fixed(uint* output = frame) {
				DebugApi.GetSpriteList(ref sprites, ref previews, CpuType, new GetSpritePreviewOptions() { Background = SpriteBackground.Transparent, Filter = Config.Filter },
					_data.PpuState, _data.PpuToolsState, _data.Vram, _data.SpriteRam, _data.Palette.Value.GetRgbPalette(), (IntPtr)output);
			}
			int left = selected.Min(s => MetaPosition(s).X), top = selected.Min(s => MetaPosition(s).Y);
			MetaSpriteGroup group = new() {
				Id = _metaPack.NextGroupId,
				Width = selected.Max(s => MetaPosition(s).X + s.Width) - left,
				Height = selected.Max(s => MetaPosition(s).Y + s.Height) - top
			};
			group.OriginX = group.Width / 2;
			group.OriginY = group.Height;
			if(poseSource != null) {
				group.LinkAsPoseOf(poseSource);
				group.PoseName = "Pose " + (_metaPack.Groups.Count(g => g.PoseFamilyId == poseSource.PoseFamilyId) + 1);
			}
			foreach(var sprite in selected) {
				uint[] pixels = new uint[sprite.Width * sprite.Height];
				Array.Copy(previews, sprite.SpriteIndex * 128 * 128, pixels, 0, pixels.Length);
				group.Members.Add(new() { X = MetaPosition(sprite).X - left, Y = MetaPosition(sprite).Y - top, Width = sprite.Width, Height = sprite.Height, Pixels = pixels });
			}
			_metaPack.NextGroupId++;
			_metaPack.Groups.Add(group);
			MetaGroups.Add(group);
			SelectedMetaGroup = group;
			SaveMetaGroups();
		} catch(Exception ex) { MetaStatus = "Could not create group: " + ex.Message; }
	}

	public void MoveMetaGroup(int direction)
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0 || SelectedMetaGroup is not MetaSpriteGroup group || (direction != -1 && direction != 1)) { return; }
		int index = _metaPack.Groups.IndexOf(group);
		int destination = index + direction;
		if(index < 0 || destination < 0 || destination >= _metaPack.Groups.Count) { return; }
		_metaPack.Groups.RemoveAt(index);
		_metaPack.Groups.Insert(destination, group);
		try { _metaPack.Save(_metaPackPath); }
		catch(Exception ex) {
			_metaPack.Groups.RemoveAt(destination);
			_metaPack.Groups.Insert(index, group);
			MetaStatus = "Could not save group priority: " + ex.Message;
			return;
		}
		MetaGroups.Move(index, destination);
		ApplyMetaReplacements();
		RefreshTab();
		MetaStatus += $" Meta {group.Id:D3} moved {(direction < 0 ? "earlier" : "later")} in matching priority.";
	}

	public void RemoveMetaGroup()
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0 || SelectedMetaGroup is not MetaSpriteGroup group) { return; }
		int index = _metaPack.Groups.IndexOf(group);
		if(index < 0) { return; }
		_metaPack.Groups.RemoveAt(index);
		try { _metaPack.Save(_metaPackPath); }
		catch(Exception ex) {
			_metaPack.Groups.Insert(index, group);
			MetaStatus = "Could not remove group: " + ex.Message;
			return;
		}
		_removedMetaGroup = group;
		_removedMetaGroupIndex = index;
		MetaGroups.Remove(group);
		SelectedMetaGroup = MetaGroups.Count > 0 ? MetaGroups[Math.Min(index, MetaGroups.Count - 1)] : null;
		ApplyMetaReplacements();
		RefreshTab();
		MetaStatus += $" Removed Meta {group.Id:D3}. Undo removal restores it; PNG files are retained.";
	}

	public void AlignMetaPoseOrigins()
	{
		if(_metaLoadFailed || SelectedMetaGroup is not MetaSpriteGroup source) { return; }
		var previous = _metaPack.Groups.Select(g => (Group: g, X: g.OriginX, Y: g.OriginY)).ToList();
		int count = _metaPack.AlignPoseOrigins(source);
		if(count == 0) { MetaStatus = "Add an animation pose before aligning family origins."; return; }
		try { _metaPack.Save(_metaPackPath); }
		catch(Exception ex) {
			foreach(var item in previous) { item.Group.OriginX = item.X; item.Group.OriginY = item.Y; }
			MetaStatus = "Could not save pose origins: " + ex.Message;
			return;
		}
		ApplyMetaReplacements();
		RefreshTab();
		MetaStatus += $" Aligned {count} animation pose origins with Meta {source.Id:D3}.";
	}

	public void UndoRemoveMetaGroup()
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0 || _removedMetaGroup is not MetaSpriteGroup group) { return; }
		int index = Math.Min(_removedMetaGroupIndex, _metaPack.Groups.Count);
		_metaPack.Groups.Insert(index, group);
		try { _metaPack.Save(_metaPackPath); }
		catch(Exception ex) {
			_metaPack.Groups.RemoveAt(index);
			MetaStatus = "Could not restore group: " + ex.Message;
			return;
		}
		_removedMetaGroup = null;
		MetaGroups.Insert(index, group);
		SelectedMetaGroup = group;
		ApplyMetaReplacements();
		RefreshTab();
		MetaStatus += $" Restored Meta {group.Id:D3}.";
	}

	public void SaveMetaGroups()
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0) { return; }
		try { _metaPack.Save(_metaPackPath); MetaStatus = "Groups saved."; ApplyMetaReplacements(); }
		catch(Exception ex) { MetaStatus = "Could not save groups: " + ex.Message; }
	}

	public void ApplyMetaReplacements()
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0) { return; }
		if(CpuType != CpuType.Gameboy && CpuType != CpuType.Sms && CpuType != CpuType.Pce && CpuType != CpuType.Gba && CpuType != CpuType.Ws && CpuType != CpuType.Nes && CpuType != CpuType.Snes) {
			if(_metaPack.Groups.Any(g => g.Enabled)) { MetaStatus = "Groups saved. Gameplay replacement for this system is still being developed."; }
			return;
		}
		try {
			if(!MetaSpriteRuntimeLoader.Apply(_metaPack, _metaPackPath)) {
				MetaStatus = "Could not apply replacements: the ROM changed or this console layout is not supported yet.";
				return;
			}
			MetaStatus = _metaPack.Groups.Any(g => g.Enabled) ? "Replacements applied. Capture starts on the next complete frame." : "Groups saved; gameplay replacements disabled.";
			if(CpuType == CpuType.Pce && _metaPack.Groups.Any(g => g.Enabled)) { MetaStatus += " PCE currently requires the default filter and a consistent dot clock; fixed-resolution output is pending."; }
			if(CpuType == CpuType.Gba && _metaPack.Groups.Any(g => g.Enabled)) { MetaStatus += " GBA currently requires the default filter with color adjustment and frame blending disabled; NTSC and stereoscopic output are pending."; }
			if(CpuType == CpuType.Ws && _metaPack.Groups.Any(g => g.Enabled)) { MetaStatus += " WonderSwan currently requires the default filter with frame blending and LCD icons disabled; other output modes are pending."; }
			if(CpuType == CpuType.Nes && _metaPack.Groups.Any(g => g.Enabled)) { MetaStatus += " NES currently requires the default filter and standard PPU; existing HD packs and VS dual-console layouts are excluded."; }
			if(CpuType == CpuType.Snes && _metaPack.Groups.Any(g => g.Enabled)) { MetaStatus += " SNES currently requires low-resolution noninterlaced output, the default filter, neutral color settings, and no forced-resolution conversion."; }
		} catch(Exception ex) { MetaStatus = "Could not apply replacements: " + ex.Message; }
	}

	partial void OnSelectedMetaGroupChanging(MetaSpriteGroup? value)
	{
		if(SelectedMetaGroup != null) { SelectedMetaGroup.PropertyChanged -= MetaGroupPropertyChanged; }
	}

	private void MetaGroupPropertyChanged(object? sender, PropertyChangedEventArgs e) => RefreshMetaPreviewPlacement();

	partial void OnSelectedMetaGroupChanged(MetaSpriteGroup? value)
	{
		MetaPreview?.Dispose();
		MetaPreview = null;
		if(value == null) { return; }
		value.PropertyChanged += MetaGroupPropertyChanged;
		try {
			string path = Path.Combine(Path.GetDirectoryName(_metaPackPath)!, value.ReplacementPath);
			PreviewUsesReplacement = value.ReplacementPath.Length > 0 && File.Exists(path);
			MetaPreview = PreviewUsesReplacement ? new Bitmap(path) : BuildMetaBitmap(value);
			RefreshMetaPreviewPlacement();
		} catch(Exception ex) { MetaStatus = "Could not preview group: " + ex.Message; }
	}

	public void RefreshMetaPreviewPlacement()
	{
		if(SelectedMetaGroup is not MetaSpriteGroup group || MetaPreview == null) { return; }
		int scale = PreviewUsesReplacement ? Math.Max(1, group.ReplacementScale) : 1;
		MetaPreviewWidth = (double)MetaPreview.PixelSize.Width / scale;
		MetaPreviewHeight = (double)MetaPreview.PixelSize.Height / scale;
		MetaPreviewOriginX = PreviewUsesReplacement ? group.ReplacementOriginX : group.OriginX;
		MetaPreviewOriginY = PreviewUsesReplacement ? group.ReplacementOriginY : group.OriginY;
	}

	private static WriteableBitmap BuildMetaBitmap(MetaSpriteGroup group)
	{
		uint[] pixels = new uint[group.Width * group.Height];
		// Lower OAM indices win in the basic sprite preview. Hardware composition is handled separately.
		foreach(var member in group.Members.AsEnumerable().Reverse()) {
			for(int y = 0; y < member.Height; y++) {
				for(int x = 0; x < member.Width; x++) {
					uint color = member.Pixels[y * member.Width + x];
					if((color >> 24) != 0) { pixels[(y + member.Y) * group.Width + x + member.X] = color; }
				}
			}
		}
		WriteableBitmap bitmap = new(new PixelSize(group.Width, group.Height), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);
		using(var fb = bitmap.Lock()) {
			for(int y = 0; y < group.Height; y++) {
				int[] row = new int[group.Width];
				for(int x = 0; x < group.Width; x++) { row[x] = unchecked((int)pixels[y * group.Width + x]); }
				Marshal.Copy(row, 0, fb.Address + y * fb.RowBytes, row.Length);
			}
		}
		return bitmap;
	}

	public async Task ExportMetaGroup(Window window)
	{
		if(SelectedMetaGroup is not MetaSpriteGroup group) { return; }
		string? path = await FileDialogHelper.SaveFile(Path.GetDirectoryName(_metaPackPath), $"meta-{group.Id:D3}.png", window, FileDialogHelper.PngExt);
		if(path == null) { return; }
		try { using var bitmap = BuildMetaBitmap(group); bitmap.Save(path, PngBitmapEncoderOptions.Default); MetaStatus = "Group PNG exported."; }
		catch(Exception ex) { MetaStatus = "Could not export PNG: " + ex.Message; }
	}

	public async Task LoadMetaReplacement(Window window)
	{
		if(SelectedMetaGroup is not MetaSpriteGroup group) { return; }
		string? path = await FileDialogHelper.OpenFile(Path.GetDirectoryName(_metaPackPath), window, FileDialogHelper.PngExt);
		if(path == null) { return; }
		try {
			using Bitmap bitmap = new(path);
			int scale = group.ReplacementScale;
			// Preserve convenient automatic scale detection for an enlarged exported canvas.
			if(group.ReplacementPath.Length == 0 && bitmap.PixelSize.Width % group.Width == 0 && bitmap.PixelSize.Height % group.Height == 0
				&& bitmap.PixelSize.Width / group.Width == bitmap.PixelSize.Height / group.Height) {
				scale = bitmap.PixelSize.Width / group.Width;
			}
			MetaSpritePlacement.ValidateCanvas(bitmap.PixelSize.Width, bitmap.PixelSize.Height, scale);
			string destination = Path.Combine(Path.GetDirectoryName(_metaPackPath)!, $"meta-{group.Id:D3}-replacement.png");
			Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
			if(!string.Equals(Path.GetFullPath(path), Path.GetFullPath(destination), StringComparison.OrdinalIgnoreCase)) { File.Copy(path, destination, true); }
			if(group.ReplacementPath.Length == 0) {
				group.ReplacementOriginX = group.OriginX;
				group.ReplacementOriginY = group.OriginY;
			}
			group.ReplacementPath = Path.GetFileName(destination);
			group.ReplacementScale = scale;
			group.ReplacementWidth = bitmap.PixelSize.Width;
			group.ReplacementHeight = bitmap.PixelSize.Height;
			SaveMetaGroups();
			MetaPreview?.Dispose();
			MetaPreview = new Bitmap(destination);
			PreviewUsesReplacement = true;
			RefreshMetaPreviewPlacement();
			if(!group.Enabled) { MetaStatus = "Replacement PNG loaded. Enable the group and apply to use it in gameplay."; }
		} catch(Exception ex) { MetaStatus = "Could not load PNG: " + ex.Message; }
	}
}

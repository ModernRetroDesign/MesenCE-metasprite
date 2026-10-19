using Avalonia.Controls;
using Avalonia.Media.Imaging;
using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Debugger.Utilities;
using Mesen.Interop;
using Mesen.Utilities;
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Threading.Tasks;

namespace Mesen.Debugger.ViewModels;

public partial class SpriteViewerViewModel
{
	public ObservableCollection<MetaAlternateArtwork> MetaAlternates { get; } = new();
	[ObservableProperty] public partial MetaAlternateArtwork? SelectedMetaAlternate { get; set; }
	[ObservableProperty] public partial string MetaAlternateConditionsText { get; set; } = "";
	[ObservableProperty] public partial Bitmap? MetaAlternatePreview { get; set; }
	[ObservableProperty] public partial string MetaAlternatePreviewInfo { get; set; } = "";
	[ObservableProperty] public partial double MetaAlternatePreviewWidth { get; set; } = 1;
	[ObservableProperty] public partial double MetaAlternatePreviewHeight { get; set; } = 1;
	
	public bool ViewingMetaAlternate => SelectedMetaAlternate != null;
	public bool HasMetaGroup => SelectedMetaGroup != null;
	public bool HasMetaAlternates => MetaAlternates.Count > 0;
	public string MetaArtworkLabel => SelectedMetaAlternate is MetaAlternateArtwork alternate
		? $"PNG {MetaAlternates.IndexOf(alternate) + 2} / {MetaAlternates.Count + 1} — {alternate}"
		: $"Default PNG — 1 / {MetaAlternates.Count + 1}";
	private void RefreshMetaArtworkNavigation()
	{
		OnPropertyChanged(nameof(ViewingMetaAlternate)); OnPropertyChanged(nameof(HasMetaGroup)); OnPropertyChanged(nameof(HasMetaAlternates)); OnPropertyChanged(nameof(MetaArtworkLabel));
	}
	private readonly System.Collections.Generic.Dictionary<MetaAlternateArtwork, string> _invalidAlternateConditionDrafts = new();
	public void BrowseMetaArtwork(int direction)
	{
		if(MetaAlternates.Count == 0) { return; }
		if(!ValidateSelectedAlternateEdits()) { return; }
		int index = SelectedMetaAlternate == null ? 0 : MetaAlternates.IndexOf(SelectedMetaAlternate) + 1;
		index = (index + direction % (MetaAlternates.Count + 1) + MetaAlternates.Count + 1) % (MetaAlternates.Count + 1);
		SelectedMetaAlternate = index == 0 ? null : MetaAlternates[index - 1];
	}
	private bool ValidateSelectedAlternateEdits()
	{
		if(SelectedMetaAlternate is not MetaAlternateArtwork alternate) { return true; }
		try {
			ParseMetaConditions(MetaAlternateConditionsText); alternate.Validate();
			if(MetaAlternatePreview != null) { MetaSpritePlacement.ValidateSpriteCanvas(MetaAlternatePreview.PixelSize.Width, MetaAlternatePreview.PixelSize.Height, alternate.Scale); }
			return true;
		} catch(InvalidDataException ex) { MetaStatus = "Correct the alternate settings before changing PNG: " + ex.Message; return false; }
	}
	partial void OnSelectedMetaAlternateChanging(MetaAlternateArtwork? value)
	{
		if(SelectedMetaAlternate != null) { SelectedMetaAlternate.PropertyChanged -= MetaAlternatePropertyChanged; }
	}

	partial void OnSelectedMetaAlternateChanged(MetaAlternateArtwork? value)
	{
		if(value != null) { value.PropertyChanged += MetaAlternatePropertyChanged; }
		MetaAlternateConditionsText = value == null ? "" : _invalidAlternateConditionDrafts.TryGetValue(value, out string? draft) ? draft : MetaNamedConditionBindings.Text(value);
		RefreshMetaAlternatePreview();
		RefreshMetaArtworkNavigation();
	}
	partial void OnMetaAlternateConditionsTextChanged(string value)
	{
		if(SelectedMetaAlternate is not MetaAlternateArtwork alternate) { return; }
		try { alternate.MemoryConditions = ParseMetaConditions(value,alternate); _invalidAlternateConditionDrafts.Remove(alternate); }
		catch(InvalidDataException ex) { _invalidAlternateConditionDrafts[alternate] = value; MetaStatus = ex.Message; }
	}
	private void RefreshMetaAlternatePreview()
	{
		MetaAlternatePreview?.Dispose(); MetaAlternatePreview = null;
		MetaAlternatePreviewInfo = "";
		if(SelectedMetaAlternate is not MetaAlternateArtwork alternate) { return; }
		try {
			MetaAlternatePreview = new Bitmap(Path.Combine(Path.GetDirectoryName(_metaPackPath)!, alternate.Path));
			RefreshMetaAlternatePlacement();
		} catch(Exception ex) { MetaStatus = "Could not preview alternate PNG: " + ex.Message; }
	}
	private void MetaAlternatePropertyChanged(object? sender, PropertyChangedEventArgs e) { RefreshMetaAlternatePlacement(); RefreshMetaArtworkNavigation(); }
	private void RefreshMetaAlternatePlacement()
	{
		if(SelectedMetaAlternate is not MetaAlternateArtwork alternate || MetaAlternatePreview == null) { return; }
		int scale = Math.Max(1, alternate.Scale);
		MetaAlternatePreviewWidth = (double)MetaAlternatePreview.PixelSize.Width / scale;
		MetaAlternatePreviewHeight = (double)MetaAlternatePreview.PixelSize.Height / scale;
		MetaAlternatePreviewInfo = $"{alternate} — origin {alternate.OriginX}, {alternate.OriginY}; scale {alternate.Scale}×.";
	}
	public async Task AddMetaAlternate(Window window)
	{
		if(_metaLoadFailed || SelectedMetaGroup is not MetaSpriteGroup group || _metaPackPath.Length == 0) { return; }
		if(!ValidateSelectedAlternateEdits()) { return; }
		try {
			if(group.AlternateArtwork.Count >= 64) { MetaStatus = "A group can contain up to 64 alternate PNGs."; return; }
			string? path = await FileDialogHelper.OpenFile(Path.GetDirectoryName(_metaPackPath), window, FileDialogHelper.PngExt);
			if(path == null) { return; }
			if(!ReferenceEquals(SelectedMetaGroup, group) || EmuApi.GetRomHash(HashType.Sha1) != _metaPack.RomHash) {
				MetaStatus = "The game or group changed while choosing the PNG. Select the group and import again."; return;
			}
			AddMetaAlternatePng(path, false);
		} catch(Exception ex) { MetaStatus = "Could not add alternate PNG: " + ex.Message; }
	}
	public void AddMetaAlternatePng(string path, bool useCurrentConditions = true)
	{
		if(_metaLoadFailed || SelectedMetaGroup is not MetaSpriteGroup group || _metaPackPath.Length == 0) { return; }
		if(EmuApi.GetRomHash(HashType.Sha1) != _metaPack.RomHash) { MetaStatus = "The game changed. Reopen the sprite viewer."; return; }
		string? createdPath = null;
		bool saved = false;
		MetaAlternateArtwork? addedAlternate = null;
		var previousSelection = SelectedMetaAlternate;
		string previousConditions = MetaAlternateConditionsText;
		try {
			var conditions = useCurrentConditions ? ParseMetaConditions(MetaAlternateConditionsText) : new System.Collections.Generic.List<MetaMemoryCondition>();
			if(group.AlternateArtwork.Count >= 64) { MetaStatus = "A group can contain up to 64 alternate PNGs."; return; }
			using Bitmap bitmap = new(path);
			int scale = Math.Max(1, group.ReplacementScale);
			MetaSpritePlacement.ValidateSpriteCanvas(bitmap.PixelSize.Width, bitmap.PixelSize.Height, scale);
			string destination = Path.Combine(Path.GetDirectoryName(_metaPackPath)!, $"meta-{group.Id:D3}-alternate-{Guid.NewGuid():N}.png");
			Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
			using(var source = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read)) {
				using var output = new FileStream(destination, FileMode.CreateNew, FileAccess.Write);
				createdPath = destination;
				source.CopyTo(output);
			}
			var alternate = new MetaAlternateArtwork { Name = Path.GetFileNameWithoutExtension(path), Path = Path.GetFileName(destination), Scale = scale,
				OriginX = group.ReplacementOriginX, OriginY = group.ReplacementOriginY, MemoryConditions = conditions };
			alternate.MemoryConditionExpression=useCurrentConditions ? MetaNamedConditionBindings.SavedExpression(previousConditions) : null;
			alternate.NamedConditionsResolved=true;
			addedAlternate = alternate;
			group.AlternateArtwork.Add(alternate); MetaAlternates.Add(alternate); SelectedMetaAlternate = alternate;
			saved = TrySaveMetaGroups();
			if(saved && conditions.Count == 0) { MetaStatus += " New alternate PNG saved. Add CPU memory conditions and save to activate it."; }
		} catch(Exception ex) { MetaStatus = "Could not add alternate PNG: " + ex.Message; }
		finally {
			if(!saved && addedAlternate != null) {
				string failure = MetaStatus;
				group.AlternateArtwork.Remove(addedAlternate); MetaAlternates.Remove(addedAlternate);
				SelectedMetaAlternate = previousSelection; MetaAlternateConditionsText = previousConditions;
				MetaStatus = failure;
			}
			if(!saved && createdPath != null) {
				try { File.Delete(createdPath); }
				catch(Exception ex) { MetaStatus += $" Could not remove unsaved PNG '{createdPath}': {ex.Message}"; }
			}
			RefreshMetaArtworkNavigation();
		}
	}
	public void RemoveMetaAlternate()
	{
		RemoveSelectedMetaAlternate();
	}
	private void ImportMetaAlternateReplacementPng(string path)
	{
		if(SelectedMetaAlternate is not MetaAlternateArtwork alternate || SelectedMetaGroup is not MetaSpriteGroup group) { return; }
		string previousPath = alternate.Path;
		string? createdPath = null;
		bool saved = false;
		try {
			// Keep this alternate's name, conditions, scale and alignment when changing its artwork.
			ParseMetaConditions(MetaAlternateConditionsText);
			using Bitmap bitmap = new(path);
			MetaSpritePlacement.ValidateSpriteCanvas(bitmap.PixelSize.Width, bitmap.PixelSize.Height, alternate.Scale);
			string destination = Path.Combine(Path.GetDirectoryName(_metaPackPath)!, $"meta-{group.Id:D3}-alternate-{Guid.NewGuid():N}.png");
			Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
			using(var source = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read)) {
				using var output = new FileStream(destination, FileMode.CreateNew, FileAccess.Write);
				createdPath = destination;
				source.CopyTo(output);
			}
			alternate.Path = Path.GetFileName(destination);
			saved = TrySaveMetaGroups();
			if(saved) { RefreshMetaAlternatePreview(); }
		} catch(Exception ex) { MetaStatus = "Could not load alternate PNG: " + ex.Message; }
		finally {
			if(!saved) {
				alternate.Path = previousPath;
				if(createdPath != null) {
					try { File.Delete(createdPath); }
					catch(Exception ex) { MetaStatus += $" Could not remove unsaved PNG '{createdPath}': {ex.Message}"; }
				}
			}
		}
	}
	public void SetSelectedMetaAlternateOrigin(int x, int y)
	{
		if(SelectedMetaAlternate is not MetaAlternateArtwork alternate) { return; }
		int oldX = alternate.OriginX, oldY = alternate.OriginY;
		alternate.OriginX = Math.Clamp(x, -4096, 4096); alternate.OriginY = Math.Clamp(y, -4096, 4096);
		if(!TrySaveMetaGroups()) { alternate.OriginX = oldX; alternate.OriginY = oldY; }
	}
	private void RemoveSelectedMetaAlternate()
	{
		if(SelectedMetaGroup is not MetaSpriteGroup group || SelectedMetaAlternate is not MetaAlternateArtwork alternate) { return; }
		int index = group.AlternateArtwork.IndexOf(alternate);
		if(index < 0) { return; }
		string previousConditions = MetaAlternateConditionsText;
		group.AlternateArtwork.RemoveAt(index); MetaAlternates.Remove(alternate);
		SelectedMetaAlternate = MetaAlternates.Count == 0 ? null : MetaAlternates[Math.Min(index, MetaAlternates.Count - 1)];
		if(!TrySaveMetaGroups()) {
			string failure = MetaStatus;
			group.AlternateArtwork.Insert(index, alternate); MetaAlternates.Insert(index, alternate);
			SelectedMetaAlternate = alternate; MetaAlternateConditionsText = previousConditions;
			MetaStatus = failure;
		}
		RefreshMetaArtworkNavigation();
	}
	public void MoveMetaAlternate(int direction)
	{
		if(SelectedMetaGroup is not MetaSpriteGroup group || SelectedMetaAlternate is not MetaAlternateArtwork alternate) { return; }
		int index = group.AlternateArtwork.IndexOf(alternate), target = index + direction;
		if(index < 0 || target < 0 || target >= group.AlternateArtwork.Count) { return; }
		group.AlternateArtwork.RemoveAt(index); group.AlternateArtwork.Insert(target, alternate);
		MetaAlternates.Move(index, target);
		if(!TrySaveMetaGroups()) {
			group.AlternateArtwork.RemoveAt(target); group.AlternateArtwork.Insert(index, alternate);
			MetaAlternates.Move(target, index);
		}
		RefreshMetaArtworkNavigation();
	}
}

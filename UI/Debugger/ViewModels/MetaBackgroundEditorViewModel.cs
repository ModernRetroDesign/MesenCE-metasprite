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

public partial class MetaBackgroundEditorViewModel : ObservableObject, IDisposable
{
	private List<MetaMemoryCondition> ParseNamedMemoryConditions(string text,IMetaMemoryConditionOwner? owner=null) =>
		MetaNamedConditionBindings.Parse(text,_metaPackPath,_metaPack.RomHash,_cpuType.ToString(),owner);
 public ObservableCollection<MetaBackgroundLayer> MetaBackgrounds { get; } = new();
 [ObservableProperty] public partial MetaBackgroundLayer? SelectedMetaBackground { get; set; }
 [ObservableProperty] public partial Bitmap? MetaBackgroundPreview { get; set; }
 [ObservableProperty] public partial string MetaBackgroundPreviewInfo { get; set; } = "";
 [ObservableProperty] public partial string MetaStatus { get; set; } = "";
 [ObservableProperty] public partial string CameraReadout { get; set; } = "";
 [ObservableProperty] public partial bool HasSavedCamera { get; set; }
 [ObservableProperty] public partial string SavedCameraInfo { get; set; } = "No saved CPU camera for this game.";
 private void RefreshSavedCameraInfo() {
  HasSavedCamera = _metaPack.SavedCamera != null;
  string Describe(MetaCameraAxis axis) => axis.Format == 0 ? "fixed" : axis.IsPaged ? $"${axis.Address:X} + page ${axis.PageAddress:X}, {axis.PageSize} px/page{(axis.SignedPage ? ", signed" : "")}" : $"${axis.Address:X}, {axis.Format switch {1 => "8-bit", 2 => "16-bit LE", _ => "16-bit BE"}}";
  SavedCameraInfo = HasSavedCamera ? $"Saved camera\nX: {Describe(_metaPack.SavedCamera!.X)}\nY: {Describe(_metaPack.SavedCamera.Y)}" : "No saved CPU camera for this game.";
 }
 private void SaveCameraChanges() {
  if(SelectedMetaBackground is MetaBackgroundLayer layer) { layer.MemoryConditions = ParseNamedMemoryConditions(MemoryConditionsText,layer); }
  SavePack();
 }
 public void SaveCpuCamera() {
  if(_metaLoadFailed || _metaPackPath.Length == 0 || SelectedMetaBackground is not MetaBackgroundLayer layer) { return; }
  var previous = _metaPack.SavedCamera;
  try {
   _metaPack.SavedCamera = layer.UseSavedCamera && previous != null ? new() {X = previous.X.Copy(), Y = previous.Y.Copy()} : new() {X = layer.CameraX.Copy(), Y = layer.CameraY.Copy()};
   SaveCameraChanges(); RefreshSavedCameraInfo(); ApplyMetaReplacements(); RefreshCameraReadout();
   MetaStatus += " CPU camera saved for this game; linked backgrounds updated.";
  } catch(Exception ex) { _metaPack.SavedCamera = previous; RefreshSavedCameraInfo(); MetaStatus = "Could not save CPU camera: " + ex.Message; }
 }
 public void LinkCpuCamera() {
  if(_metaLoadFailed || _metaPackPath.Length == 0 || _metaPack.SavedCamera == null || SelectedMetaBackground is not MetaBackgroundLayer layer) { return; }
  bool previousLink = layer.UseSavedCamera, previousSource = layer.UseCpuCamera, previousFollow = layer.FollowScroll;
  try {
   layer.UseSavedCamera = true; layer.UseCpuCamera = true; layer.FollowScroll = true;
   SaveCameraChanges(); ApplyMetaReplacements(); RefreshCameraReadout();
   MetaStatus += " Background linked to the saved CPU camera.";
  } catch(Exception ex) { layer.UseSavedCamera = previousLink; layer.UseCpuCamera = previousSource; layer.FollowScroll = previousFollow; MetaStatus = "Could not link CPU camera: " + ex.Message; }
 }
 public void UnlinkCpuCamera() {
  if(_metaLoadFailed || _metaPackPath.Length == 0 || _metaPack.SavedCamera == null || SelectedMetaBackground is not {UseSavedCamera: true} layer) { return; }
  var previousX = layer.CameraX; var previousY = layer.CameraY;
  try {
   SelectedMetaBackground = null;
   layer.CameraX = _metaPack.SavedCamera.X.Copy(); layer.CameraY = _metaPack.SavedCamera.Y.Copy(); layer.UseSavedCamera = false;
   SelectedMetaBackground = layer;
   SaveCameraChanges(); ApplyMetaReplacements(); RefreshCameraReadout();
   MetaStatus += " Background unlinked; current camera settings retained.";
  } catch(Exception ex) { SelectedMetaBackground = null; layer.CameraX = previousX; layer.CameraY = previousY; layer.UseSavedCamera = true; SelectedMetaBackground = layer; MetaStatus = "Could not unlink CPU camera: " + ex.Message; }
 }
 public void RefreshCameraReadout() {
  if(SelectedMetaBackground is not {UseCpuCamera: true} layer) { CameraReadout = ""; return; }
  try {
   byte Read(uint address) {
    if(address >= DebugApi.GetMemorySize(_cpuType.ToMemoryType())) { throw new InvalidDataException("Camera address is outside CPU memory."); }
    return DebugApi.GetMemoryValue(_cpuType.ToMemoryType(), address);
   }
   var xAxis = layer.UseSavedCamera ? _metaPack.SavedCamera!.X : layer.CameraX;
   var yAxis = layer.UseSavedCamera ? _metaPack.SavedCamera!.Y : layer.CameraY;
   int x = xAxis.Decode(Read), y = yAxis.Decode(Read);
   string Format(int value) => value < 0 ? $"{value} px" : $"{value} px (${value:X})";
   CameraReadout = $"Camera X: {Format(x)}\nCamera Y: {Format(y)}";
  } catch(Exception ex) { CameraReadout = ex.Message; }
 }
 [ObservableProperty] public partial string MemoryConditionsText { get; set; } = "";
 private MetaSpritePack _metaPack = new();
 private string _metaPackPath = "";
 private string _savedBackgroundDefinitions = "";
 private bool _metaLoadFailed;
 private readonly Dictionary<MetaBackgroundLayer, string> _invalidConditionDrafts = new();
 private CpuType _cpuType;
 public bool IsNes => _cpuType == CpuType.Nes;
 public bool SupportsSpriteBands => IsNes || _cpuType == CpuType.Gameboy || _cpuType == CpuType.Pce || _cpuType == CpuType.Sms || _cpuType == CpuType.Ws;
	public bool IsGba => _cpuType == CpuType.Gba;
	public bool IsSnes => _cpuType == CpuType.Snes;
 public IReadOnlyList<string> HardwareLayerNames => _cpuType switch {
  CpuType.Snes => new[] { "BG1", "BG2", "BG3", "BG4" },
  CpuType.Gba => new[] { "BG0", "BG1", "BG2", "BG3" },
  CpuType.Gameboy => new[] { "Background", "Window" },
  CpuType.Ws => new[] { "BG0 (background)", "BG1 (foreground)" },
  _ => new[] { "Background" }
 };
 public MetaBackgroundEditorViewModel(CpuType cpuType) { _cpuType = cpuType; }
 public void Load(CpuType? cpuType = null)
 {
  if(cpuType.HasValue) { _cpuType = cpuType.Value; OnPropertyChanged(nameof(IsNes)); OnPropertyChanged(nameof(IsGba)); OnPropertyChanged(nameof(IsSnes)); OnPropertyChanged(nameof(HardwareLayerNames)); OnPropertyChanged(nameof(SupportsSpriteBands)); }
  _invalidConditionDrafts.Clear();
  SelectedMetaBackground = null;
  MetaBackgrounds.Clear();
  _metaLoadFailed = false;
  try {
   string hash = EmuApi.GetRomHash(HashType.Sha1);
   _metaPackPath = MetaSpriteRuntimeLoader.GetPackPath(hash, _cpuType);
   _metaPack = MetaSpritePack.Load(_metaPackPath, hash, _cpuType.ToString());
   _savedBackgroundDefinitions = _metaPack.GetBackgroundDefinitionsSnapshot();
   foreach(var layer in _metaPack.BackgroundLayers) { MetaBackgrounds.Add(layer); }
   RefreshSavedCameraInfo();
   MetaStatus = "Import a background PNG to start.";
  } catch(Exception ex) { _metaLoadFailed = true; MetaStatus = "Could not load backgrounds: " + ex.Message; }
 }
 private void SavePack()
 {
  _metaPack.ValidateSavedCamera();
  if(EmuApi.GetRomHash(HashType.Sha1) != _metaPack.RomHash) { throw new InvalidDataException("The ROM changed. Reopen the background editor."); }
  _metaPack.ResolveNamedConditions(Path.GetDirectoryName(Path.GetFullPath(_metaPackPath))!);
  foreach(var layer in _metaPack.BackgroundLayers) {
   layer.Validate();
   if(layer.Enabled && layer.MemoryConditions.Count == 0) { throw new InvalidDataException($"Background {layer.Id:D3} requires at least one CPU memory condition before enabling gameplay replacement."); }
  }
  var latest = MetaSpritePack.Load(_metaPackPath, _metaPack.RomHash, _cpuType.ToString());
  if(latest.GetBackgroundDefinitionsSnapshot() != _savedBackgroundDefinitions) { throw new InvalidDataException("Background layers were saved by another editor. Reload saved backgrounds before editing again."); }
  MetaSpriteRuntimeLoader.ValidateBackgroundArtworkCanvases(_metaPack, _metaPackPath);
  latest.BackgroundLayers = _metaPack.BackgroundLayers;
  latest.SavedCamera = _metaPack.SavedCamera;
  latest.NextBackgroundId = Math.Max(latest.NextBackgroundId, _metaPack.NextBackgroundId);
  latest.Save(_metaPackPath);
  _savedBackgroundDefinitions = _metaPack.GetBackgroundDefinitionsSnapshot();
 }
 public void SaveMetaGroups()
 {
  if(_metaLoadFailed || _metaPackPath.Length == 0) { return; }
  try {
   if(SelectedMetaBackground is MetaBackgroundLayer layer) { layer.MemoryConditions = ParseNamedMemoryConditions(MemoryConditionsText,layer); }
   SavePack(); ApplyMetaReplacements(); RefreshMetaBackgroundPreview();
  }
  catch(Exception ex) { MetaStatus = "Could not save backgrounds: " + ex.Message; }
 }
	public PngAnimationViewModel? CreateAnimationEditor()
	{
		if(_metaLoadFailed || SelectedMetaBackground is not MetaBackgroundLayer layer || _metaPackPath.Length==0) { return null; }
		var pack=_metaPack;
		return new PngAnimationViewModel($"PNG Animation — Background {layer.Id:D3}",Path.GetDirectoryName(_metaPackPath)!,layer.Animation,layer.ArtworkScale,0,0,true,animation => {
			if(!ReferenceEquals(pack,_metaPack) || !pack.BackgroundLayers.Contains(layer) || EmuApi.GetRomHash(HashType.Sha1)!=pack.RomHash) { return (false,"The game or background changed. Reopen the animation editor."); }
			var previous=layer.Animation; layer.Animation=animation; bool saved=false;
			try { layer.MemoryConditions=ParseNamedMemoryConditions(MemoryConditionsText,layer); SavePack(); saved=true; ApplyMetaReplacements(); RefreshMetaBackgroundPreview(); }
			catch(Exception ex) { if(!saved) { layer.Animation=previous; } MetaStatus="Could not save background animation: "+ex.Message; }
			return (saved,MetaStatus);
		});
	}
 private void ApplyMetaReplacements()
 {
  try {
   var latest = MetaSpritePack.Load(_metaPackPath, _metaPack.RomHash, _cpuType.ToString());
   MetaStatus = MetaSpriteRuntimeLoader.Apply(latest, _metaPackPath) ? "Background definitions saved and applied." : "Saved; could not apply for this console layout.";
			if(_cpuType == CpuType.Snes && latest.BackgroundLayers.Any(layer => layer.Enabled)) {
				MetaStatus += " SNES uses the default filter and neutral color settings. Color math supports normal-width fields and mode 5/6 or pseudo-hires output. Hires OBJ-interlace math and Current Field/Weave/Bob/Bob Blend rewind are supported. Artwork follows enabled background layers, including transparent texels, with native priority and windows.";
			}
  } catch(Exception ex) { MetaStatus = "Saved; could not apply backgrounds: " + ex.Message; }
 }
 public void Dispose() { SelectedMetaBackground = null; MetaBackgroundPreview?.Dispose(); MetaBackgroundPreview = null; }
	public async Task ImportMetaBackground(Window window, int nativeLayer = 0, string context = "")
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0) { return; }
		MetaSpritePack requestedPack = _metaPack;
		string requestedPackPath = _metaPackPath;
		string? path = await FileDialogHelper.OpenFile(Path.GetDirectoryName(requestedPackPath), window, FileDialogHelper.PngExt);
		if(path == null) { return; }
		if(_metaLoadFailed || !ReferenceEquals(requestedPack, _metaPack) || requestedPackPath != _metaPackPath
			|| EmuApi.GetRomHash(HashType.Sha1) != requestedPack.RomHash) {
			MetaStatus = "The game changed while choosing the PNG. Select the tilemap and import again.";
			return;
		}
		ImportMetaBackgroundPng(path, nativeLayer, context);
	}

	private string PrepareBackgroundArtwork(string path, ref string? createdPath)
	{
		string sourcePath = Path.GetFullPath(path);
		string directory = Path.GetFullPath(Path.GetDirectoryName(_metaPackPath)!);
		if(string.Equals(Path.GetDirectoryName(sourcePath), directory, OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)) {
			return sourcePath;
		}
		Directory.CreateDirectory(directory);
		string destination = Path.Combine(directory, Path.GetFileName(sourcePath));
		for(int number = 2; File.Exists(destination) || Directory.Exists(destination); number++) {
			destination = Path.Combine(directory, $"{Path.GetFileNameWithoutExtension(sourcePath)}-{number:D3}{Path.GetExtension(sourcePath)}");
		}
		using var source = new FileStream(sourcePath, FileMode.Open, FileAccess.Read, FileShare.Read);
		using var output = new FileStream(destination, FileMode.CreateNew, FileAccess.Write);
		createdPath = destination;
		source.CopyTo(output);
		return destination;
	}

	public void ImportMetaBackgroundPng(string path, int nativeLayer = 0, string context = "")
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0) { return; }
		if(EmuApi.GetRomHash(HashType.Sha1) != _metaPack.RomHash) { MetaStatus = "The game changed. Reopen the background editor."; return; }
		string? createdPath = null;
		bool saved = false;
		try {
			using Bitmap bitmap = new(path);
			MetaSpritePlacement.ValidateCanvas(bitmap.PixelSize.Width, bitmap.PixelSize.Height, 1);
			int id = _metaPack.NextBackgroundId;
			string destination = PrepareBackgroundArtwork(path, ref createdPath);
			var layer = new MetaBackgroundLayer { Id = id, NativeLayer = nativeLayer, Name = Path.GetFileNameWithoutExtension(path), ArtworkPath = Path.GetFileName(destination), ArtworkWidth = bitmap.PixelSize.Width, ArtworkHeight = bitmap.PixelSize.Height,
				UseWidescreenPosition = true, WidescreenPosition = new() { Position = 0 } };
			_metaPack.BackgroundLayers.Add(layer);
			_metaPack.NextBackgroundId++;
			try { SavePack(); }
			catch { _metaPack.BackgroundLayers.Remove(layer); _metaPack.NextBackgroundId = id; throw; }
			saved = true;
			MetaBackgrounds.Add(layer);
			SelectedMetaBackground = layer;
			MetaStatus = "Background PNG imported for the selected tilemap. Set placement, then enable and save. " + context;
		} catch(Exception ex) {
			MetaStatus = (saved ? "Background saved; could not finish import: " : "Could not import background: ") + ex.Message;
			if(!saved && createdPath != null) {
				try { File.Delete(createdPath); }
				catch(Exception cleanupError) { MetaStatus += $" Could not remove unsaved PNG '{createdPath}': {cleanupError.Message}"; }
			}
		}
	}
	public async Task ReplaceBackgroundPng(Window window)
	{
		if(_metaLoadFailed || SelectedMetaBackground is not MetaBackgroundLayer layer) { return; }
		var pack = _metaPack;
		string? path = await FileDialogHelper.OpenFile(Path.GetDirectoryName(_metaPackPath), window, FileDialogHelper.PngExt);
		if(path == null) { return; }
		if(!ReferenceEquals(pack, _metaPack) || !ReferenceEquals(layer, SelectedMetaBackground)) { MetaStatus = "The background changed while choosing the PNG. Select it and load again."; return; }
		ReplaceBackgroundPngFile(path);
	}
	public void ReplaceBackgroundPngFile(string path)
	{
		if(_metaLoadFailed || SelectedMetaBackground is not MetaBackgroundLayer layer || _metaPackPath.Length == 0) { return; }
		if(EmuApi.GetRomHash(HashType.Sha1) != _metaPack.RomHash) { MetaStatus = "The game changed. Reopen the background editor."; return; }
		string oldPath = layer.ArtworkPath;
		int oldWidth = layer.ArtworkWidth, oldHeight = layer.ArtworkHeight;
		string? createdPath = null;
		bool saved = false;
		try {
			layer.MemoryConditions = ParseNamedMemoryConditions(MemoryConditionsText,layer);
			using Bitmap bitmap = new(path);
			MetaSpritePlacement.ValidateCanvas(bitmap.PixelSize.Width, bitmap.PixelSize.Height, layer.ArtworkScale);
			string destination = PrepareBackgroundArtwork(path, ref createdPath);
			layer.ArtworkPath = Path.GetFileName(destination);
			layer.ArtworkWidth = bitmap.PixelSize.Width; layer.ArtworkHeight = bitmap.PixelSize.Height;
			SavePack(); saved = true;
			ApplyMetaReplacements(); RefreshMetaBackgroundPreview();
		} catch(Exception ex) { MetaStatus = "Could not replace background PNG: " + ex.Message; }
		finally {
			if(!saved) {
				layer.ArtworkPath = oldPath; layer.ArtworkWidth = oldWidth; layer.ArtworkHeight = oldHeight;
				if(createdPath != null) {
					try { File.Delete(createdPath); }
					catch(Exception ex) { MetaStatus += " Could not remove unsaved PNG: " + ex.Message; }
				}
			}
		}
	}

	partial void OnSelectedMetaBackgroundChanging(MetaBackgroundLayer? value)
	{
		if(SelectedMetaBackground != null) {
			SelectedMetaBackground.PropertyChanged -= BackgroundPlacementChanged;
			SelectedMetaBackground.CameraX.PropertyChanged -= BackgroundPlacementChanged;
			SelectedMetaBackground.CameraY.PropertyChanged -= BackgroundPlacementChanged;
		}
	}
	partial void OnSelectedMetaBackgroundChanged(MetaBackgroundLayer? value)
	{
		if(value != null) { value.PropertyChanged += BackgroundPlacementChanged; value.CameraX.PropertyChanged += BackgroundPlacementChanged; value.CameraY.PropertyChanged += BackgroundPlacementChanged; }
		MemoryConditionsText = value == null ? "" : _invalidConditionDrafts.TryGetValue(value, out string? draft) ? draft : MetaNamedConditionBindings.Text(value);
		RefreshMetaBackgroundPreview();
		RefreshCameraReadout();
	}
	partial void OnMemoryConditionsTextChanged(string value)
	{
		if(SelectedMetaBackground is not MetaBackgroundLayer layer) { return; }
		try { layer.MemoryConditions = ParseNamedMemoryConditions(value,layer); _invalidConditionDrafts.Remove(layer); }
		catch(InvalidDataException ex) { _invalidConditionDrafts[layer] = value; MetaStatus = ex.Message; }
	}

	private void RefreshMetaBackgroundPreview()
	{
		MetaBackgroundPreview?.Dispose();
		MetaBackgroundPreview = null;
		MetaBackgroundPreviewInfo = "";
		if(SelectedMetaBackground is not MetaBackgroundLayer layer || _metaPackPath.Length == 0) { return; }
		try {
			MetaBackgroundPreview = new Bitmap(Path.Combine(Path.GetDirectoryName(_metaPackPath)!, layer.ArtworkPath));
			RefreshBackgroundPlacement();
		} catch(Exception ex) { MetaBackgroundPreviewInfo = "Could not preview background: " + ex.Message; }
	}
	private void BackgroundPlacementChanged(object? sender, PropertyChangedEventArgs e) { RefreshBackgroundPlacement(); RefreshCameraReadout(); }
	private void RefreshBackgroundPlacement()
	{
		if(SelectedMetaBackground is not MetaBackgroundLayer layer || MetaBackgroundPreview == null) { return; }
		int scale = Math.Max(1, layer.ArtworkScale);
		MetaBackgroundPreviewInfo = $"PNG {MetaBackgroundPreview.PixelSize.Width} × {MetaBackgroundPreview.PixelSize.Height}; native canvas {MetaBackgroundPreview.PixelSize.Width / (double)scale:g} × {MetaBackgroundPreview.PixelSize.Height / (double)scale:g}; placement {layer.X}, {layer.Y}.";
	}

	public void RemoveMetaBackground()
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0 || SelectedMetaBackground is not MetaBackgroundLayer layer) { return; }
		int index = _metaPack.BackgroundLayers.IndexOf(layer);
		if(index < 0) { return; }
		_metaPack.BackgroundLayers.RemoveAt(index);
		try {
			SavePack();
		} catch(Exception ex) {
			_metaPack.BackgroundLayers.Insert(index, layer);
			MetaStatus = "Could not remove background: " + ex.Message;
			return;
		}
		MetaBackgrounds.Remove(layer);
		SelectedMetaBackground = MetaBackgrounds.Count > 0 ? MetaBackgrounds[Math.Min(index, MetaBackgrounds.Count - 1)] : null;
		ApplyMetaReplacements();
		MetaStatus += " Background removed; its PNG is retained in the pack folder.";
	}


}

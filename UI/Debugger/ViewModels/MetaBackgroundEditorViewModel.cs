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
 public ObservableCollection<MetaBackgroundLayer> MetaBackgrounds { get; } = new();
 [ObservableProperty] public partial MetaBackgroundLayer? SelectedMetaBackground { get; set; }
 [ObservableProperty] public partial Bitmap? MetaBackgroundPreview { get; set; }
 [ObservableProperty] public partial string MetaBackgroundPreviewInfo { get; set; } = "";
 [ObservableProperty] public partial string MetaStatus { get; set; } = "";
 private MetaSpritePack _metaPack = new();
 private string _metaPackPath = "";
 private bool _metaLoadFailed;
 private CpuType _cpuType;
 public MetaBackgroundEditorViewModel(CpuType cpuType) { _cpuType = cpuType; }
 public void Load(CpuType? cpuType = null)
 {
  if(cpuType.HasValue) { _cpuType = cpuType.Value; }
  SelectedMetaBackground = null;
  MetaBackgrounds.Clear();
  _metaLoadFailed = false;
  try {
   string hash = EmuApi.GetRomHash(HashType.Sha1);
   _metaPackPath = MetaSpriteRuntimeLoader.GetPackPath(hash, _cpuType);
   _metaPack = MetaSpritePack.Load(_metaPackPath, hash, _cpuType.ToString());
   foreach(var layer in _metaPack.BackgroundLayers) { MetaBackgrounds.Add(layer); }
   MetaStatus = "Import a background PNG to start.";
  } catch(Exception ex) { _metaLoadFailed = true; MetaStatus = "Could not load backgrounds: " + ex.Message; }
 }
 private void SavePack()
 {
  if(EmuApi.GetRomHash(HashType.Sha1) != _metaPack.RomHash) { throw new InvalidDataException("The ROM changed. Reopen the background editor."); }
  foreach(var layer in _metaPack.BackgroundLayers) { layer.Validate(); }
  var latest = MetaSpritePack.Load(_metaPackPath, _metaPack.RomHash, _cpuType.ToString());
  latest.BackgroundLayers = _metaPack.BackgroundLayers;
  latest.NextBackgroundId = Math.Max(latest.NextBackgroundId, _metaPack.NextBackgroundId);
  latest.Save(_metaPackPath);
 }
 public void SaveMetaGroups()
 {
  if(_metaLoadFailed || _metaPackPath.Length == 0) { return; }
  try { SavePack(); ApplyMetaReplacements(); RefreshMetaBackgroundPreview(); }
  catch(Exception ex) { MetaStatus = "Could not save backgrounds: " + ex.Message; }
 }
 private void ApplyMetaReplacements()
 {
  try {
   var latest = MetaSpritePack.Load(_metaPackPath, _metaPack.RomHash, _cpuType.ToString());
   MetaStatus = MetaSpriteRuntimeLoader.Apply(latest, _metaPackPath) ? "Background definitions saved and applied." : "Saved; could not apply for this console layout.";
  } catch(Exception ex) { MetaStatus = "Saved; could not apply backgrounds: " + ex.Message; }
 }
 public void Dispose() { MetaBackgroundPreview?.Dispose(); MetaBackgroundPreview = null; }
	public async Task ImportMetaBackground(Window window, int nativeLayer = 0, string context = "")
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0) { return; }
		string? path = await FileDialogHelper.OpenFile(Path.GetDirectoryName(_metaPackPath), window, FileDialogHelper.PngExt);
		if(path == null) { return; }
		try {
			using Bitmap bitmap = new(path);
			MetaSpritePlacement.ValidateCanvas(bitmap.PixelSize.Width, bitmap.PixelSize.Height, 1);
			int id = _metaPack.NextBackgroundId;
			string destination = Path.Combine(Path.GetDirectoryName(_metaPackPath)!, $"background-{id:D3}.png");
			Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
			if(!string.Equals(Path.GetFullPath(path), Path.GetFullPath(destination), StringComparison.OrdinalIgnoreCase)) { File.Copy(path, destination, true); }
			var layer = new MetaBackgroundLayer { Id = id, NativeLayer = nativeLayer, Name = Path.GetFileNameWithoutExtension(path), ArtworkPath = Path.GetFileName(destination), ArtworkWidth = bitmap.PixelSize.Width, ArtworkHeight = bitmap.PixelSize.Height };
			_metaPack.BackgroundLayers.Add(layer);
			_metaPack.NextBackgroundId++;
			try { SavePack(); }
			catch { _metaPack.BackgroundLayers.Remove(layer); _metaPack.NextBackgroundId = id; throw; }
			MetaBackgrounds.Add(layer);
			SelectedMetaBackground = layer;
			MetaStatus = "Background PNG imported for the selected tilemap. Set placement, then enable and save. " + context;
		} catch(Exception ex) { MetaStatus = "Could not import background: " + ex.Message; }
	}

	partial void OnSelectedMetaBackgroundChanged(MetaBackgroundLayer? value) => RefreshMetaBackgroundPreview();

	private void RefreshMetaBackgroundPreview()
	{
		MetaBackgroundPreview?.Dispose();
		MetaBackgroundPreview = null;
		MetaBackgroundPreviewInfo = "";
		if(SelectedMetaBackground is not MetaBackgroundLayer layer || _metaPackPath.Length == 0) { return; }
		try {
			MetaBackgroundPreview = new Bitmap(Path.Combine(Path.GetDirectoryName(_metaPackPath)!, layer.ArtworkPath));
			int scale = Math.Max(1, layer.ArtworkScale);
			MetaBackgroundPreviewInfo = $"PNG {MetaBackgroundPreview.PixelSize.Width} × {MetaBackgroundPreview.PixelSize.Height}; native canvas {MetaBackgroundPreview.PixelSize.Width / (double)scale:g} × {MetaBackgroundPreview.PixelSize.Height / (double)scale:g}; placement {layer.X}, {layer.Y}.";
		} catch(Exception ex) { MetaBackgroundPreviewInfo = "Could not preview background: " + ex.Message; }
	}

	public void RemoveMetaBackground()
	{
		if(_metaLoadFailed || _metaPackPath.Length == 0 || SelectedMetaBackground is not MetaBackgroundLayer layer) { return; }
		int index = _metaPack.BackgroundLayers.IndexOf(layer);
		if(index < 0) { return; }
		_metaPack.BackgroundLayers.RemoveAt(index);
		try {
			foreach(var remaining in _metaPack.BackgroundLayers) { remaining.Validate(); }
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

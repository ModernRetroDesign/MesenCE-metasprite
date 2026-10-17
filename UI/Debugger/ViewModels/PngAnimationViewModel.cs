using Avalonia.Media.Imaging;
using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Debugger.Utilities;
using SkiaSharp;
using System;
using System.Collections.ObjectModel;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Mesen.Debugger.ViewModels;

public partial class PngAnimationViewModel : ObservableObject, IDisposable
{
	private readonly string _directory;
	private readonly int _scale, _originX, _originY;
	private readonly bool _background;
	private readonly Func<MetaPngAnimation,(bool Saved,string Status)> _save;
	private bool _selecting, _fromPlayback;
	private ulong _tick;
	private MetaPngTimeline? _timeline;
	private bool _measureDirty=true;
	private readonly Dictionary<string,Bitmap> _previews = new(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
	private readonly Queue<string> _previewOrder = new();
	private readonly Dictionary<string,(int Width,int Height)> _dimensions = new(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
	private int _sourceWidth=1, _sourceHeight=1;
	public string Title { get; }
	public string DirectoryPath => _directory;
	public bool HasFrame => SelectedFrame!=null;
	public int PreviewWidth => Preview?.PixelSize.Width ?? 1;
	public int PreviewHeight => Preview?.PixelSize.Height ?? 1;
	public double PreviewOriginX => (double)FrameOriginX*_scale*PreviewWidth/_sourceWidth;
	public double PreviewOriginY => (double)FrameOriginY*_scale*PreviewHeight/_sourceHeight;
	public string ScaleInfo => $"Shared artwork scale: {_scale}×. Origins use native pixels.";
	public ObservableCollection<MetaPngAnimationFrame> Frames { get; } = new();
	[ObservableProperty] public partial bool Enabled { get; set; }
	[ObservableProperty] public partial bool RestartOnActivation { get; set; }
	[ObservableProperty] public partial int PlaybackMode { get; set; }
	[ObservableProperty] public partial MetaPngAnimationFrame? SelectedFrame { get; set; }
	[ObservableProperty] public partial int FrameDuration { get; set; } = 1;
	[ObservableProperty] public partial bool OverrideOrigin { get; set; }
	[ObservableProperty] public partial int FrameOriginX { get; set; }
	[ObservableProperty] public partial int FrameOriginY { get; set; }
	[ObservableProperty] public partial Bitmap? Preview { get; set; }
	[ObservableProperty] public partial string FrameInfo { get; set; } = "No frames";
	[ObservableProperty] public partial string MemoryInfo { get; set; } = "";
	[ObservableProperty] public partial string Status { get; set; } = "Add PNG frames, then save and apply.";
	[ObservableProperty] public partial bool IsPlaying { get; set; }
	public PngAnimationViewModel(string title, string directory, MetaPngAnimation source, int scale, int originX, int originY,
		bool background, Func<MetaPngAnimation,(bool Saved,string Status)> save)
	{
		Title=title; _directory=Path.GetFullPath(directory); _scale=scale; _originX=originX; _originY=originY; _background=background; _save=save;
		RestartOnActivation=source.RestartOnActivation; Enabled=source.Enabled; PlaybackMode=(int)source.Playback;
		foreach(var frame in source.Frames) { Frames.Add(CopyFrame(frame)); }
		SelectedFrame=Frames.FirstOrDefault(); RefreshPreview();
	}
	private static MetaPngAnimationFrame CopyFrame(MetaPngAnimationFrame frame) => new() {Path=frame.Path,Duration=frame.Duration,OriginX=frame.OriginX,OriginY=frame.OriginY};
	public MetaPngAnimation BuildDraft()
	{
		var draft=new MetaPngAnimation {Enabled=Enabled,Playback=(MetaPngPlayback)PlaybackMode,RestartOnActivation=RestartOnActivation,Frames=Frames.Select(CopyFrame).ToList()};
		draft.Validate(); return draft;
	}
	private long ValidateAssets(MetaPngAnimation animation)
	{
		long bytes=0;
		var sizes=new Dictionary<string,long>(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
		foreach(var frame in animation.Frames) {
			string path=Path.GetFullPath(Path.Combine(_directory,frame.Path));
			if(!sizes.TryGetValue(path,out long size)) {
				using var codec=SKCodec.Create(path) ?? throw new InvalidDataException($"Could not read PNG '{Path.GetFileName(path)}'.");
				if(_background) { MetaSpritePlacement.ValidateCanvas(codec.Info.Width,codec.Info.Height,_scale); }
				else { MetaSpritePlacement.ValidateSpriteCanvas(codec.Info.Width,codec.Info.Height,_scale); }
				size=(long)codec.Info.Width*codec.Info.Height*4; sizes.Add(path,size);
			}
			bytes=checked(bytes+size); // Native transport currently stores each frame, including repeated paths.
		}
		return bytes;
	}
	public bool Save()
	{
		StopPreview();
		try {
			var draft=BuildDraft(); long bytes=draft.Enabled ? ValidateAssets(draft) : 0;
			if(draft.Enabled && bytes>=536870912) { throw new InvalidDataException("This sequence exceeds the current 512 MiB artwork limit. Reduce frame sizes or count."); }
			var result=_save(draft); Status=result.Status; return result.Saved;
		} catch(Exception ex) { Status=ex.Message; return false; }
	}
	public void AddFrame(string path)
	{
		StopPreview(); string? created=null;
		try {
			using(var codec=SKCodec.Create(path) ?? throw new InvalidDataException("Could not read PNG.")) {
				if(_background) { MetaSpritePlacement.ValidateCanvas(codec.Info.Width,codec.Info.Height,_scale); }
				else { MetaSpritePlacement.ValidateSpriteCanvas(codec.Info.Width,codec.Info.Height,_scale); }
			}
			if(Frames.Count>=256) { throw new InvalidDataException("A sequence supports up to 256 frames."); }
			string source=Path.GetFullPath(path), destination=source;
			if(!string.Equals(Path.GetDirectoryName(source),_directory,OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)) {
				Directory.CreateDirectory(_directory); destination=Path.Combine(_directory,Path.GetFileName(source));
				for(int number=2;File.Exists(destination) || Directory.Exists(destination);number++) { destination=Path.Combine(_directory,$"{Path.GetFileNameWithoutExtension(source)}-{number:D3}{Path.GetExtension(source)}"); }
				using var input=new FileStream(source,FileMode.Open,FileAccess.Read,FileShare.Read);
				using var output=new FileStream(destination,FileMode.CreateNew,FileAccess.Write); created=destination; input.CopyTo(output);
			}
			var frame=new MetaPngAnimationFrame {Path=Path.GetFileName(destination)};
			var check=new MetaPngAnimation {Enabled=true,Playback=(MetaPngPlayback)PlaybackMode,Frames=Frames.Select(CopyFrame).Append(frame).ToList()}; check.Validate();
			_measureDirty=true; Frames.Add(frame); SelectedFrame=frame; Enabled=true; Status="PNG frame added. Save and apply to use the sequence."; RefreshPreview();
		} catch(Exception ex) {
			Status=ex.Message;
			if(created!=null) { try { File.Delete(created); } catch(Exception cleanup) { Status+=" Could not remove incomplete PNG copy: "+cleanup.Message; } }
		}
	}
	public void RemoveFrame()
	{
		StopPreview(); if(SelectedFrame==null) { return; } int index=Frames.IndexOf(SelectedFrame); Frames.Remove(SelectedFrame); _measureDirty=true;
		SelectedFrame=Frames.Count==0 ? null : Frames[Math.Min(index,Frames.Count-1)]; if(Frames.Count==0) { Enabled=false; }
		RefreshPreview(); Status="Frame removed. Its PNG file is retained.";
	}
	public void Browse(int direction)
	{
		StopPreview(); if(Frames.Count==0) { return; } int index=Frames.IndexOf(SelectedFrame!);
		SelectedFrame=Frames[(index+direction+Frames.Count)%Frames.Count];
	}
	public void Move(int direction)
	{
		StopPreview(); if(SelectedFrame==null) { return; } int index=Frames.IndexOf(SelectedFrame), target=index+direction;
		if(target<0 || target>=Frames.Count) { return; } var selected=SelectedFrame; Frames.Move(index,target); SelectedFrame=selected; RefreshPreview();
	}
	public void StartPreview()
	{
		try {
			var draft=BuildDraft(); draft.Enabled=true; draft.Validate(); ValidateAssets(draft); _timeline=new(draft); _tick=0;
			ClearPreviews(); IsPlaying=true; SelectPlaybackFrame(); RefreshPreview(); Status="Preview runs at 60 frames/s. Gameplay follows emulation.";
		} catch(Exception ex) { IsPlaying=false; Status=ex.Message; }
	}
	public void StopPreview() => IsPlaying=false;
	public void AdvancePreview()
	{
		if(!IsPlaying || _timeline==null) { return; } _tick++; SelectPlaybackFrame();
		if(PlaybackMode==(int)MetaPngPlayback.PlayOnce && _tick>=_timeline.Duration) { IsPlaying=false; }
	}
	private void SelectPlaybackFrame()
	{
		int index=_timeline!.SelectFrame(_tick); if(index<0 || index>=Frames.Count) { StopPreview(); return; }
		_fromPlayback=true; try { SelectedFrame=Frames[index]; } finally { _fromPlayback=false; }
	}
	partial void OnSelectedFrameChanged(MetaPngAnimationFrame? value)
	{
		OnPropertyChanged(nameof(HasFrame));
		if(!_fromPlayback) { StopPreview(); } _selecting=true;
		try { FrameDuration=value?.Duration ?? 1; OverrideOrigin=value?.OriginX!=null || value?.OriginY!=null; FrameOriginX=value?.OriginX ?? _originX; FrameOriginY=value?.OriginY ?? _originY; }
		finally { _selecting=false; } RefreshPreview();
	}
	partial void OnPlaybackModeChanged(int value) { StopPreview(); }
	partial void OnFrameDurationChanged(int value) { if(!_selecting && SelectedFrame!=null) { StopPreview(); SelectedFrame.Duration=value; } }
	partial void OnFrameOriginXChanged(int value) { if(!_selecting && OverrideOrigin && SelectedFrame!=null) { StopPreview(); SelectedFrame.OriginX=value; } OnPropertyChanged(nameof(PreviewOriginX)); }
	partial void OnFrameOriginYChanged(int value) { if(!_selecting && OverrideOrigin && SelectedFrame!=null) { StopPreview(); SelectedFrame.OriginY=value; } OnPropertyChanged(nameof(PreviewOriginY)); }
	partial void OnPreviewChanged(Bitmap? value) { OnPropertyChanged(nameof(PreviewWidth)); OnPropertyChanged(nameof(PreviewHeight)); OnPropertyChanged(nameof(PreviewOriginX)); OnPropertyChanged(nameof(PreviewOriginY)); }
	public void SetPreviewOrigin(double x,double y)
	{
		if(SelectedFrame==null || Preview==null || !double.IsFinite(x) || !double.IsFinite(y)) { return; }
		OverrideOrigin=true;
		FrameOriginX=(int)Math.Clamp(Math.Round(x*_sourceWidth/PreviewWidth/_scale),-4096,4096);
		FrameOriginY=(int)Math.Clamp(Math.Round(y*_sourceHeight/PreviewHeight/_scale),-4096,4096);
	}
	partial void OnOverrideOriginChanged(bool value)
	{
		if(_selecting || SelectedFrame==null) { return; } StopPreview(); SelectedFrame.OriginX=value ? FrameOriginX : null; SelectedFrame.OriginY=value ? FrameOriginY : null;
		if(!value) { _selecting=true; try { FrameOriginX=_originX; FrameOriginY=_originY; } finally { _selecting=false; } }
	}
	private void RefreshPreview()
	{
		Preview=null;
		FrameInfo=SelectedFrame==null ? "No frames" : $"Frame {Frames.IndexOf(SelectedFrame)+1} / {Frames.Count}: {SelectedFrame.Path}";
		try {
			if(_measureDirty) { MemoryInfo=$"Approximate decoded sequence: {ValidateAssets(BuildDraft())/1048576.0:F1} MiB"; _measureDirty=false; }
			if(SelectedFrame!=null) {
				string path=Path.Combine(_directory,SelectedFrame.Path);
				if(!_previews.TryGetValue(path,out var bitmap)) {
					using var codec=SKCodec.Create(path) ?? throw new InvalidDataException("Could not read PNG preview.");
					using var stream=File.OpenRead(path); bitmap=Bitmap.DecodeToWidth(stream,Math.Min(512,codec.Info.Width));
					_dimensions[path]=(codec.Info.Width,codec.Info.Height);
					_previews.Add(path,bitmap); _previewOrder.Enqueue(path);
					while(_previews.Count>3) { string old=_previewOrder.Dequeue(); _previews[old].Dispose(); _previews.Remove(old); _dimensions.Remove(old); }
				}
				_sourceWidth=_dimensions[path].Width; _sourceHeight=_dimensions[path].Height; Preview=bitmap;
			}
		} catch(Exception ex) { Status=ex.Message; }
	}
	private void ClearPreviews() { Preview=null; foreach(var bitmap in _previews.Values) { bitmap.Dispose(); } _previews.Clear(); _previewOrder.Clear(); _dimensions.Clear(); }
	public void Dispose() { StopPreview(); ClearPreviews(); }
}

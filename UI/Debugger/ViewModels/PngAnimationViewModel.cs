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
	private const int DefaultFrameDuration = 8;
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
	private bool _alignedPreview;
	private double _previewScale=1, _previewMinX, _previewMinY, _previewCanvasWidth=1, _previewCanvasHeight=1;
	public string Title { get; }
	public string DirectoryPath => _directory;
	public bool HasFrame => SelectedFrame!=null;
	public int PreviewWidth => _alignedPreview ? (int)Math.Ceiling(_previewCanvasWidth) : Preview?.PixelSize.Width ?? 1;
	public int PreviewHeight => _alignedPreview ? (int)Math.Ceiling(_previewCanvasHeight) : Preview?.PixelSize.Height ?? 1;
	public double PreviewImageWidth => _alignedPreview ? _sourceWidth*_previewScale : PreviewWidth;
	public double PreviewImageHeight => _alignedPreview ? _sourceHeight*_previewScale : PreviewHeight;
	public double PreviewImageLeft => _alignedPreview ? (-FrameOriginX*_scale-_previewMinX)*_previewScale : 0;
	public double PreviewImageTop => _alignedPreview ? (-FrameOriginY*_scale-_previewMinY)*_previewScale : 0;
	public double PreviewOriginX => PreviewImageLeft+(double)FrameOriginX*_scale*PreviewImageWidth/_sourceWidth;
	public double PreviewOriginY => PreviewImageTop+(double)FrameOriginY*_scale*PreviewImageHeight/_sourceHeight;
	public string ScaleInfo => $"Shared artwork scale: {_scale}×. Origins use native pixels.";
	public ObservableCollection<MetaPngAnimationFrame> Frames { get; } = new();
	[ObservableProperty] public partial bool Enabled { get; set; }
	[ObservableProperty] public partial bool RestartOnActivation { get; set; }
	[ObservableProperty] public partial int PlaybackMode { get; set; }
	[ObservableProperty] public partial MetaPngAnimationFrame? SelectedFrame { get; set; }
	[ObservableProperty] public partial int FrameDuration { get; set; } = DefaultFrameDuration;
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
				bytes=checked(bytes+size); // Unique paths are an upper estimate; identical content also shares storage.
			}
		}
		return bytes;
	}
	public bool Save()
	{
		StopPreview();
		try {
			var draft=BuildDraft(); if(draft.Enabled) { ValidateAssets(draft); }
			// The packet writer enforces the actual encoded size after content sharing.
			var result=_save(draft); Status=result.Status; return result.Saved;
		} catch(Exception ex) { Status=ex.Message; return false; }
	}
	public void AddFrames(IEnumerable<string> paths)
	{
		var selected=paths.ToList(); if(selected.Count==0) { return; }
		StopPreview();
		if(selected.Count>256-Frames.Count) { Status="A sequence supports up to 256 frames. No frames were added."; return; }
		try { ValidateExistingFrames(); } catch(Exception ex) { Status=ex.Message; return; }
		int added=0;
		foreach(string path in selected) {
			if(!AddFrameCore(path,false)) {
				string error=Status; if(added>0) { SelectedFrame=Frames[^1]; }
				Status=$"Added {added} of {selected.Count} frames. "+error; return;
			}
			added++;
		}
		SelectedFrame=Frames[^1];
		Status=$"Added {added} PNG frame(s) in picker order. Save and apply to use the sequence.";
	}
	public bool AddFrame(string path)
	{
		try { ValidateExistingFrames(); } catch(Exception ex) { Status=ex.Message; return false; }
		return AddFrameCore(path,true);
	}
	private void ValidateExistingFrames() => new MetaPngAnimation {Playback=(MetaPngPlayback)PlaybackMode,Frames=Frames.Select(CopyFrame).ToList()}.Validate();
	private bool AddFrameCore(string path,bool refresh)
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
			var frame=new MetaPngAnimationFrame {Path=Path.GetFileName(destination),Duration=DefaultFrameDuration};
			new MetaPngAnimation {Enabled=true,Playback=(MetaPngPlayback)PlaybackMode,Frames=new() {frame}}.Validate();
			_measureDirty=true; Frames.Add(frame); Enabled=true;
			if(refresh) { SelectedFrame=frame; }
			Status="PNG frame added. Save and apply to use the sequence."; return true;
		} catch(Exception ex) {
			Status=ex.Message;
			if(created!=null) { try { File.Delete(created); } catch(Exception cleanup) { Status+=" Could not remove incomplete PNG copy: "+cleanup.Message; } }
			return false;
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
			ClearPreviews(); PreparePlaybackCanvas(draft); IsPlaying=true; SelectPlaybackFrame(); RefreshPreview(); Status="Preview runs at 60 frames/s, aligned at the shared origin. Gameplay follows emulation.";
		} catch(Exception ex) { IsPlaying=false; Status=ex.Message; }
	}
	private void PreparePlaybackCanvas(MetaPngAnimation draft)
	{
		double minX=0,minY=0,maxX=0,maxY=0;
		var dimensions=new Dictionary<string,(int Width,int Height)>(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
		foreach(var frame in draft.Frames) {
			string path=Path.GetFullPath(Path.Combine(_directory,frame.Path));
			if(!dimensions.TryGetValue(path,out var size)) {
				using var codec=SKCodec.Create(path) ?? throw new InvalidDataException("Could not read PNG preview.");
				size=(codec.Info.Width,codec.Info.Height); dimensions.Add(path,size);
			}
			double left=-(double)(frame.OriginX ?? _originX)*_scale, top=-(double)(frame.OriginY ?? _originY)*_scale;
			minX=Math.Min(minX,left); minY=Math.Min(minY,top); maxX=Math.Max(maxX,left+size.Width); maxY=Math.Max(maxY,top+size.Height);
		}
		_previewMinX=minX; _previewMinY=minY;
		_previewScale=Math.Min(1,Math.Min(512/Math.Max(1,maxX-minX),512/Math.Max(1,maxY-minY)));
		_previewCanvasWidth=Math.Max(1,(maxX-minX)*_previewScale); _previewCanvasHeight=Math.Max(1,(maxY-minY)*_previewScale);
		_alignedPreview=true; NotifyPreviewLayout();
	}
	public void StopPreview() { IsPlaying=false; if(_alignedPreview) { _alignedPreview=false; NotifyPreviewLayout(); } }
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
		try { FrameDuration=value?.Duration ?? DefaultFrameDuration; OverrideOrigin=value?.OriginX!=null || value?.OriginY!=null; FrameOriginX=value?.OriginX ?? _originX; FrameOriginY=value?.OriginY ?? _originY; }
		finally { _selecting=false; } RefreshPreview();
	}
	partial void OnPlaybackModeChanged(int value) { StopPreview(); }
	partial void OnFrameDurationChanged(int value) { if(!_selecting && SelectedFrame!=null) { StopPreview(); SelectedFrame.Duration=value; } }
	partial void OnFrameOriginXChanged(int value) { if(!_selecting && OverrideOrigin && SelectedFrame!=null) { StopPreview(); SelectedFrame.OriginX=value; } OnPropertyChanged(nameof(PreviewOriginX)); }
	partial void OnFrameOriginYChanged(int value) { if(!_selecting && OverrideOrigin && SelectedFrame!=null) { StopPreview(); SelectedFrame.OriginY=value; } OnPropertyChanged(nameof(PreviewOriginY)); }
	private void NotifyPreviewLayout()
	{
		foreach(string property in new[] {nameof(PreviewWidth),nameof(PreviewHeight),nameof(PreviewImageWidth),nameof(PreviewImageHeight),nameof(PreviewImageLeft),nameof(PreviewImageTop),nameof(PreviewOriginX),nameof(PreviewOriginY)}) { OnPropertyChanged(property); }
	}
	partial void OnPreviewChanged(Bitmap? value) => NotifyPreviewLayout();
	public void SetPreviewOrigin(double x,double y)
	{
		if(SelectedFrame==null || Preview==null || !double.IsFinite(x) || !double.IsFinite(y)) { return; }
		// Resolve the click before stopping aligned playback changes the layout.
		int originX=(int)Math.Clamp(Math.Round((x-PreviewImageLeft)*_sourceWidth/PreviewImageWidth/_scale),-4096,4096);
		int originY=(int)Math.Clamp(Math.Round((y-PreviewImageTop)*_sourceHeight/PreviewImageHeight/_scale),-4096,4096);
		StopPreview(); OverrideOrigin=true; FrameOriginX=originX; FrameOriginY=originY;
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
			if(_measureDirty) { MemoryInfo=$"Decoded artwork estimate (unique paths): {ValidateAssets(BuildDraft())/1048576.0:F1} MiB"; _measureDirty=false; }
			if(SelectedFrame!=null) {
				string path=Path.Combine(_directory,SelectedFrame.Path);
				if(!_previews.TryGetValue(path,out var bitmap)) {
					using var codec=SKCodec.Create(path) ?? throw new InvalidDataException("Could not read PNG preview.");
					// Bound both axes: portrait backgrounds must not decode a full-height
					// thumbnail just because their width is already below the limit.
					using var stream=File.OpenRead(path);
					bitmap=codec.Info.Height>codec.Info.Width
						? Bitmap.DecodeToHeight(stream,Math.Min(512,codec.Info.Height))
						: Bitmap.DecodeToWidth(stream,Math.Min(512,codec.Info.Width));
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

using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Mesen.Debugger.ViewModels;
using Mesen.Utilities;
using System;
using System.IO;
using Avalonia.Controls;
using Avalonia.Threading;
using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Mesen.Debugger.Controls;
using Mesen.Interop;

namespace Mesen.Debugger.Windows;

public class TextReplacementWindow : MesenWindow
{
	private readonly TextReplacementViewModel _model;
	private readonly DispatcherTimer _snapshotTimer = new() { Interval=TimeSpan.FromMilliseconds(100) };
	private readonly DispatcherTimer _previewTimer = new() { Interval=TimeSpan.FromMilliseconds(180) };
	private Bitmap? _gameplaySnapshot;
	private DateTime _snapshotStarted;
	[Obsolete("For designer only")]
	public TextReplacementWindow() : this(new TextReplacementViewModel(Path.Combine(Path.GetTempPath(),"mesen-text-designer-"+Guid.NewGuid().ToString("N"),"pack.json"),
		"designer","Nes",new[] {"Background"},0,-1,-1,-1,() => false)) { }
	public TextReplacementWindow(TextReplacementViewModel model)
	{
		AvaloniaXamlLoader.Load(this); _model=model; DataContext=model;
		_previewTimer.Tick+=(_,_) => { _previewTimer.Stop(); if(_model.SelectedProfile?.FontPath.Length>0) { _model.RefreshPreview(); } };
		_model.PropertyChanged+=(_,e) => { if(e.PropertyName==nameof(TextReplacementViewModel.DisplayRegion)) { _previewTimer.Stop(); _previewTimer.Start(); } };
		_snapshotTimer.Tick+=ReadGameplaySnapshot;
		Opened+=(_,_) => RequestGameplaySnapshot();
		Closed+=(_,_) => { _snapshotTimer.Stop(); _previewTimer.Stop(); _gameplaySnapshot?.Dispose(); _model.Dispose(); };
	}
	private void UseViewerSelection(object? sender,RoutedEventArgs e) => _model.UseViewerSelection();
	private void MatchSource(object? sender,RoutedEventArgs e) => _model.MatchSourceRegion();
	private void RefreshGameplay(object? sender,RoutedEventArgs e) => RequestGameplaySnapshot();
	private void RequestGameplaySnapshot() {
		try { if(!_model.IsCurrentGame) { throw new InvalidOperationException("The loaded game changed. Reopen the editor."); } DebugApi.RequestTextPreview(); _snapshotStarted=DateTime.UtcNow; _snapshotTimer.Start();
			this.FindControl<TextBlock>("gameplaySnapshotStatus")!.Text="Waiting for a gameplay frame. If paused, advance a frame or resume before refreshing. Orange marks the display region.";
		} catch(Exception ex) { this.FindControl<TextBlock>("gameplaySnapshotStatus")!.Text="Gameplay snapshot unavailable: "+ex.Message; }
	}
	private void ReadGameplaySnapshot(object? sender,EventArgs e) {
		try {
			if(!_model.IsCurrentGame) { throw new InvalidOperationException("The loaded game changed. Reopen the editor."); }
			uint count=DebugApi.ReadTextPreview(IntPtr.Zero,0,out var size);
			if(count==0) { if(DateTime.UtcNow-_snapshotStarted>TimeSpan.FromSeconds(5)) { _snapshotTimer.Stop(); } return; }
			if(size.Width==0 || size.Height==0 || count!=(ulong)size.Width*size.Height || count>4096*4096) { throw new InvalidDataException("Invalid gameplay snapshot dimensions."); }
			using var surface=new WriteableBitmap(new PixelSize((int)size.Width,(int)size.Height),new Vector(96,96),PixelFormat.Bgra8888,AlphaFormat.Opaque);
			using(var buffer=surface.Lock()) { DebugApi.ReadTextPreview(buffer.Address,count,out _); }
			using var stream=new MemoryStream(); surface.Save(stream,PngBitmapEncoderOptions.Default); stream.Position=0; var bitmap=new Bitmap(stream);
			var picker=this.FindControl<TextRegionSelector>("textDisplayPicker")!;
			picker.Image=bitmap; picker.CanvasWidth=(int)size.Width; picker.CanvasHeight=(int)size.Height;
			var old=_gameplaySnapshot; _gameplaySnapshot=bitmap; old?.Dispose(); _snapshotTimer.Stop();
			this.FindControl<TextBlock>("gameplaySnapshotStatus")!.Text="Orange: display region in gameplay pixels. Drag outside to draw, inside to move, or use the edge/corner handles. Refresh font preview after changing style or numeric dimensions.";
		} catch(Exception ex) { _snapshotTimer.Stop(); this.FindControl<TextBlock>("gameplaySnapshotStatus")!.Text="Gameplay snapshot unavailable: "+ex.Message; }
	}
	private void Add(object? sender,RoutedEventArgs e) => _model.AddProfile();
	private void Remove(object? sender,RoutedEventArgs e) => _model.RemoveProfile();
	private void Capture(object? sender,RoutedEventArgs e) => _model.AddCapturedTile();
	private void SelectionCellSize(object? sender,RoutedEventArgs e) => _model.UseSelectionCellSize();
	private void CaptureSelectedCell(object? sender,RoutedEventArgs e) => _model.AddSelectedSourceCell();
	private void Preview(object? sender,RoutedEventArgs e) => _model.RefreshPreview();
	private async void Import(object? sender,RoutedEventArgs e)
	{
		string? path=await FileDialogHelper.OpenFile(_model.DirectoryPath,this,"ttf","otf");
		if(path!=null) { _model.ImportFont(path); }
	}
	private void Save(object? sender,RoutedEventArgs e) { _model.Save(); }
	private void Apply(object? sender,RoutedEventArgs e) { _model.SaveAndApply(); }
	private void Cancel(object? sender,RoutedEventArgs e) => Close();
}

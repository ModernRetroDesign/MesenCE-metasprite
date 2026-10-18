using Avalonia.Interactivity;
using Avalonia.Input;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Mesen.Debugger.ViewModels;
using Mesen.Utilities;
using System;

namespace Mesen.Debugger.Windows;

public class PngAnimationWindow : MesenWindow
{
	private readonly PngAnimationViewModel _model;
	private readonly DispatcherTimer _timer;
	[Obsolete("For designer only")]
	public PngAnimationWindow() : this(new PngAnimationViewModel("PNG Animation",Environment.CurrentDirectory,new(),1,0,0,false,_ => (false,"Open animation editing from a sprite or background viewer."))) { }
	public PngAnimationWindow(PngAnimationViewModel model)
	{
		AvaloniaXamlLoader.Load(this); _model=model; DataContext=model;
		_timer=new DispatcherTimer {Interval=TimeSpan.FromSeconds(1.0/60)};
		_timer.Tick+=(_,_) => _model.AdvancePreview(); Opened+=(_,_) => _timer.Start();
		Closed+=(_,_) => { _timer.Stop(); _model.Dispose(); };
	}
	private async void Add(object? sender,RoutedEventArgs e) { var paths=await FileDialogHelper.OpenFiles(_model.DirectoryPath,this,FileDialogHelper.PngExt); if(paths.Count>0) { _model.AddFrames(paths); } }
	private void Remove(object? sender,RoutedEventArgs e) => _model.RemoveFrame();
	private void Previous(object? sender,RoutedEventArgs e) => _model.Browse(-1);
	private void Next(object? sender,RoutedEventArgs e) => _model.Browse(1);
	private void Earlier(object? sender,RoutedEventArgs e) => _model.Move(-1);
	private void Later(object? sender,RoutedEventArgs e) => _model.Move(1);
	private void Play(object? sender,RoutedEventArgs e) => _model.StartPreview();
	private void Stop(object? sender,RoutedEventArgs e) => _model.StopPreview();
	private void Save(object? sender,RoutedEventArgs e) { if(_model.Save()) { Close(); } }
	private void Cancel(object? sender,RoutedEventArgs e) => Close();
	private void Origin(object? sender,PointerPressedEventArgs e) { if(sender is Canvas canvas) { var point=e.GetPosition(canvas); _model.SetPreviewOrigin(point.X,point.Y); e.Handled=true; } }
}

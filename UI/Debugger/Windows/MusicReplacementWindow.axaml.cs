using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using System;
using Mesen.Debugger.ViewModels;
using Mesen.Utilities;
using System.Threading.Tasks;

namespace Mesen.Debugger.Windows;

public class MusicReplacementWindow : MesenWindow
{
	private async void OnManageNamedConditions(object? sender,RoutedEventArgs e) => await NamedConditionWindow.OpenForCurrentGame(this);
	private readonly MusicReplacementViewModel _model;
	private readonly DispatcherTimer _poll;
	public MusicReplacementWindow() { AvaloniaXamlLoader.Load(this); _model = new(); DataContext = _model; _poll = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) }; _poll.Tick += (_, _) => _model.RefreshPlayback(); Opened += (_, _) => _poll.Start(); Closed += (_, _) => _poll.Stop(); }
	private async void Import(object? sender, RoutedEventArgs e) { string? path = await FileDialogHelper.OpenFile(null, this, "ogg", "wav"); if(path != null) { _model.Import(path); } }
	private void RemoveTrack(object? sender, RoutedEventArgs e) => _model.RemoveTrack();
	private void AddRule(object? sender, RoutedEventArgs e) => _model.AddRule();
	private void RemoveRule(object? sender, RoutedEventArgs e) => _model.RemoveRule();
	private void Earlier(object? sender, RoutedEventArgs e) => _model.MoveRule(-1);
	private void Later(object? sender, RoutedEventArgs e) => _model.MoveRule(1);
	private void Save(object? sender, RoutedEventArgs e) => _model.SaveApply();
	private void Reload(object? sender, RoutedEventArgs e) => _model.Reload();
	private void Preview(object? sender, RoutedEventArgs e) => _model.Playback(0);
	private void AddEffectRule(object? sender, RoutedEventArgs e) => _model.AddEffectRule();
	private void PreviewEffect(object? sender, RoutedEventArgs e) => _model.Playback(4);
	private void StopEffects(object? sender, RoutedEventArgs e) => _model.Playback(5);
	private void Stop(object? sender, RoutedEventArgs e) => _model.Playback(1);
	private void Pause(object? sender, RoutedEventArgs e) => _model.Playback(2);
	private void Resume(object? sender, RoutedEventArgs e) => _model.Playback(3);
}

using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Mesen.Config;
using Mesen.Debugger.Controls;
using Mesen.Debugger.Utilities;
using Mesen.Debugger.ViewModels;
using Mesen.Interop;
using System;
using System.ComponentModel;

namespace Mesen.Debugger.Windows
{
	public class SpriteViewerWindow : MesenWindow, INotificationHandler
	{
		private SpriteViewerViewModel _model;
		private PixelPoint? _metaDragStart;
		private IPointer? _metaDragPointer;
		private Rect _metaSelectionBeforeDrag;

		[Obsolete("For designer only")]
		public SpriteViewerWindow() : this(CpuType.Snes) { }

		public SpriteViewerWindow(CpuType cpuType)
		{
			InitializeComponent();

			ScrollPictureViewer scrollViewer = this.GetControl<ScrollPictureViewer>("picViewer");
			PictureViewer picViewer = scrollViewer.InnerViewer;
			Grid spriteGrid = this.GetControl<Grid>("spriteGrid");
			var listView = this.GetControl<DataBoxControl.DataBox>("ListView");
			_model = new SpriteViewerViewModel(cpuType, picViewer, scrollViewer, spriteGrid, listView, this);
			DataContext = _model;

			_model.Config.LoadWindowSettings(this);

			if(Design.IsDesignMode) {
				return;
			}

			MouseViewerModelEvents.InitEvents(_model, this, picViewer);
			picViewer.PositionClicked += PicViewer_PositionClicked;
			picViewer.PointerReleased += OnMetaSelectionReleased;
			picViewer.PointerCaptureLost += OnMetaSelectionCaptureLost;
		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}

		protected override void OnOpened(EventArgs e)
		{
			base.OnOpened(e);
			if(Design.IsDesignMode) {
				return;
			}

			_model.RefreshData();
			_model.LoadMetaGroups();
		}

		protected override void OnClosing(WindowClosingEventArgs e)
		{
			base.OnClosing(e);
			_model.Config.SaveWindowSettings(this);
			ConfigManager.Config.Debug.SpriteViewer = _model.Config;
		}

		private void PicViewer_PositionClicked(object? sender, PositionClickedEventArgs e)
		{
			if(e.Properties.IsLeftButtonPressed && (_metaDragStart != null || !e.OriginalEvent.KeyModifiers.HasFlag(KeyModifiers.Control))) {
				if(_metaDragStart == null && e.OriginalEvent is PointerPressedEventArgs && sender is PictureViewer viewer) {
					_metaSelectionBeforeDrag = _model.SelectionRect;
					var pressedSprite = _model.GetMatchingSprite(e.Position, out _);
					_model.SelectedSprite = pressedSprite;
					_model.UpdateSelection(pressedSprite);
					_metaDragStart = e.Position;
					_metaDragPointer = e.OriginalEvent.Pointer;
					_model.MetaSelectionDragging = true;
					e.OriginalEvent.Pointer.Capture(viewer);
				}
				if(_metaDragStart is PixelPoint start) {
					_model.SelectionRect = new Rect(Math.Min(start.X, e.Position.X), Math.Min(start.Y, e.Position.Y), Math.Abs(start.X - e.Position.X) + 1, Math.Abs(start.Y - e.Position.Y) + 1);
				}
				e.Handled = true;
				return;
			}
			if(e.OriginalEvent is not PointerPressedEventArgs) {
				e.Handled = true;
				return;
			}
			PixelPoint p = e.Position;
			SpritePreviewModel? sprite = _model.GetMatchingSprite(p, out Rect clickedRect);
			if(sprite != null && e.Properties.IsLeftButtonPressed && e.OriginalEvent.KeyModifiers.HasFlag(KeyModifiers.Control)) {
				_model.ToggleMetaSprite(sprite, clickedRect);
				e.Handled = true;
				return;
			}
			_model.SelectedSprite = sprite;
			_model.UpdateSelection(sprite);
			e.Handled = true;
		}

		private void OnSettingsClick(object sender, RoutedEventArgs e)
		{
			_model.Config.ShowSettingsPanel = !_model.Config.ShowSettingsPanel;
		}

		private void OnCreateMetaGroup(object? sender, RoutedEventArgs e) => _model.CreateMetaGroup();
		private void OnCreateMetaPose(object? sender, RoutedEventArgs e) => _model.CreateMetaGroup(true);
		private void OnMoveMetaGroupEarlier(object? sender, RoutedEventArgs e) => _model.MoveMetaGroup(-1);
		private void OnMoveMetaGroupLater(object? sender, RoutedEventArgs e) => _model.MoveMetaGroup(1);
		private void OnAlignMetaPoseOrigins(object? sender, RoutedEventArgs e) => _model.AlignMetaPoseOrigins();
		private void OnReloadMetaGroups(object? sender, RoutedEventArgs e) => _model.LoadMetaGroups();
		private void OnAlignMetaPoseArtworkOrigins(object? sender, RoutedEventArgs e) => _model.AlignMetaPoseArtworkOrigins();
		private void OnRemoveMetaGroup(object? sender, RoutedEventArgs e) => _model.RemoveMetaGroup();
		private void OnUndoRemoveMetaGroup(object? sender, RoutedEventArgs e) => _model.UndoRemoveMetaGroup();
		private void OnApplyMetaReplacements(object? sender, RoutedEventArgs e) => _model.SaveMetaGroups();
		private void OnClearMetaSelection(object? sender, RoutedEventArgs e) => _model.ClearMetaSelection();
		private void OnMetaSelectionReleased(object? sender, PointerReleasedEventArgs e)
		{
			if(_metaDragStart == null || e.InitialPressMouseButton != MouseButton.Left || !ReferenceEquals(e.Pointer, _metaDragPointer)) { return; }
			_metaDragStart = null;
			_metaDragPointer = null;
			_model.MetaSelectionDragging = false;
			_model.SelectMetaSprites(_model.SelectionRect);
			e.Pointer.Capture(null);
			e.Handled = true;
		}
		private void OnMetaSelectionCaptureLost(object? sender, PointerCaptureLostEventArgs e)
		{
			if(_metaDragStart == null || !ReferenceEquals(e.Pointer, _metaDragPointer)) { return; }
			_metaDragStart = null;
			_metaDragPointer = null;
			_model.MetaSelectionDragging = false;
			_model.SelectionRect = _metaSelectionBeforeDrag;
		}
		private void OnSaveMetaGroups(object? sender, RoutedEventArgs e)
		{
			_model.SaveMetaGroups();
			RefreshMetaBindings();
		}
		private async void OnExportMetaGroup(object? sender, RoutedEventArgs e) => await _model.ExportMetaGroup(this);
		private async void OnLoadMetaReplacement(object? sender, RoutedEventArgs e) => await _model.LoadMetaReplacement(this);
		private async void OnAddMetaAlternate(object? sender, RoutedEventArgs e) => await _model.AddMetaAlternate(this);
		private void OnPreviousMetaArtwork(object? sender, RoutedEventArgs e) => _model.BrowseMetaArtwork(-1);
		private async void OnEditMetaAnimation(object? sender,RoutedEventArgs e) { var model=_model.CreateMetaAnimationEditor(); if(model!=null) { await new PngAnimationWindow(model).ShowDialog(this); } }
		private void OnNextMetaArtwork(object? sender, RoutedEventArgs e) => _model.BrowseMetaArtwork(1);
		private void OnRemoveMetaAlternate(object? sender, RoutedEventArgs e) => _model.RemoveMetaAlternate();
		private void OnMoveMetaAlternateEarlier(object? sender, RoutedEventArgs e) => _model.MoveMetaAlternate(-1);
		private void OnMoveMetaAlternateLater(object? sender, RoutedEventArgs e) => _model.MoveMetaAlternate(1);
		private void OnMetaAlternateOriginPressed(object? sender, PointerPressedEventArgs e)
		{
			if(_model.SelectedMetaAlternate == null || sender is not Canvas canvas || !e.GetCurrentPoint(canvas).Properties.IsLeftButtonPressed) { return; }
			Point position = e.GetPosition(canvas);
			_model.SetSelectedMetaAlternateOrigin((int)Math.Round(position.X), (int)Math.Round(position.Y));
			e.Handled = true;
		}
		private void OnMetaOriginPressed(object? sender, PointerPressedEventArgs e)
		{
			if(_model.SelectedMetaGroup == null || sender is not Canvas canvas || !e.GetCurrentPoint(canvas).Properties.IsLeftButtonPressed) { return; }
			Point position = e.GetPosition(canvas);
			_model.SetSelectedMetaOrigin((int)Math.Round(position.X), (int)Math.Round(position.Y));
			e.Handled = true;
		}
		private void RefreshMetaBindings()
		{
			var group = _model.SelectedMetaGroup;
			var alternate = _model.SelectedMetaAlternate;
			_model.SelectedMetaGroup = null;
			_model.SelectedMetaGroup = group;
			if(alternate != null && group?.AlternateArtwork.Contains(alternate) == true) { _model.SelectedMetaAlternate = alternate; }
		}

		public void ProcessNotification(NotificationEventArgs e)
		{
			if(e.NotificationType == ConsoleNotificationType.CodeBreak || e.NotificationType == ConsoleNotificationType.StateLoaded) {
				_model.ListView.ForceRefresh();
			}

			ToolRefreshHelper.ProcessNotification(this, e, _model.RefreshTiming, _model, _model.RefreshData);
		}
	}
}


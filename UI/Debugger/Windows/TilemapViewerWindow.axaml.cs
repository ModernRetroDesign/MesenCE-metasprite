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
	public class TilemapViewerWindow : MesenWindow, INotificationHandler
	{
		private TilemapViewerViewModel _model;
		private PictureViewer _picViewer;

		[Obsolete("For designer only")]
		public TilemapViewerWindow() : this(CpuType.Snes) { }

		public TilemapViewerWindow(CpuType cpuType)
		{
			InitializeComponent();

			ScrollPictureViewer scrollViewer = this.GetControl<ScrollPictureViewer>("picViewer");
			_picViewer = scrollViewer.InnerViewer;
			_model = new TilemapViewerViewModel(cpuType, _picViewer, scrollViewer, this);
			DataContext = _model;

			_model.Config.LoadWindowSettings(this);

			if(Design.IsDesignMode) {
				return;
			}

			MouseViewerModelEvents.InitEvents(_model, this, _picViewer);
			_picViewer.Source = _model.ViewerBitmap;
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
			_model.BackgroundEditor.Load();
		}

		protected override void OnClosing(WindowClosingEventArgs e)
		{
			base.OnClosing(e);
			_model.Config.SaveWindowSettings(this);
			ConfigManager.Config.Debug.TilemapViewer = _model.Config;
		}

		private void OnSettingsClick(object sender, RoutedEventArgs e)
		{
			_model.Config.ShowSettingsPanel = !_model.Config.ShowSettingsPanel;
		}
		private async void OnReplaceMetaBackground(object? sender, RoutedEventArgs e) => await _model.BackgroundEditor.ReplaceBackgroundPng(this);
		private async void OnManageNamedConditions(object? sender,RoutedEventArgs e) => await NamedConditionWindow.OpenForCurrentGame(this);
		private async void OnEditTextReplacement(object? sender,RoutedEventArgs e)
		{
			try { await new TextReplacementWindow(_model.CreateTextReplacementEditor()).ShowDialog(this); }
			catch(Exception ex) { _model.BackgroundEditor.MetaStatus="Could not open text editor: "+ex.Message; }
		}
		private async void OnEditBackgroundAnimation(object? sender,RoutedEventArgs e) { var model=_model.BackgroundEditor.CreateAnimationEditor(); if(model!=null) { await new PngAnimationWindow(model).ShowDialog(this); } }
		private async void OnImportMetaBackground(object? sender, RoutedEventArgs e)
		{
			var target = _model.GetBackgroundImportLayer();
			if(target.Layer is not int layer) { _model.BackgroundEditor.MetaStatus = target.Context; return; }
			await _model.BackgroundEditor.ImportMetaBackground(this, layer, target.Context);
		}
		private async void OnExportMetaBackground(object? sender, RoutedEventArgs e) => await _model.ExportBackgroundPng(this);
		private void OnRemoveMetaBackground(object? sender, RoutedEventArgs e) => _model.BackgroundEditor.RemoveMetaBackground();
		private void OnSaveMetaBackgrounds(object? sender, RoutedEventArgs e) => _model.BackgroundEditor.SaveMetaGroups();
		private void OnReloadMetaBackgrounds(object? sender, RoutedEventArgs e) => _model.BackgroundEditor.Load();
		private void OnSaveCpuCamera(object? sender, RoutedEventArgs e) => _model.BackgroundEditor.SaveCpuCamera();
		private void OnLinkCpuCamera(object? sender, RoutedEventArgs e) => _model.BackgroundEditor.LinkCpuCamera();
		private void OnUnlinkCpuCamera(object? sender, RoutedEventArgs e) => _model.BackgroundEditor.UnlinkCpuCamera();

		public void ProcessNotification(NotificationEventArgs e)
		{
			ToolRefreshHelper.ProcessNotification(this, e, _model.RefreshTiming, _model, _model.RefreshData);
		}
	}
}

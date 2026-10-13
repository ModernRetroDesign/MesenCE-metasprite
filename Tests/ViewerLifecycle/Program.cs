using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.Loader;
using System.Reflection;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Threading;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Mesen.Debugger.ViewModels;
using Mesen.Debugger.Utilities;
using Mesen.Interop;
using Mesen.Utilities;
using Mesen.Config;
using Mesen.Debugger.Controls;
using Mesen.Controls;

internal static class Program
{
	[DllImport("MesenCore.dll")]
	private static extern void ConfigureMetaSoftwareRendererTest();
	[STAThread]
	private static void Main(string[] args)
	{
		AssemblyLoadContext.Default.Resolving += (context, name) => {
			string path = Path.Combine(AppContext.BaseDirectory, name.Name + ".dll");
			return File.Exists(path) ? context.LoadFromAssemblyPath(path) : null;
		};
		int pointerIndex = Array.IndexOf(args, "--pointer-suite");
		if(pointerIndex >= 0) {
			string uiHome = Path.Combine(AppContext.BaseDirectory, "artifacts", "pointer-ui-" + Guid.NewGuid().ToString("N"));
			Directory.CreateDirectory(uiHome);
			typeof(ConfigManager).GetField("_homeFolder", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, uiHome);
			typeof(ConfigManager).GetField("_config", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, Configuration.CreateConfig());
			AppBuilder.Configure<Mesen.App>().UsePlatformDetect().SetupWithoutStarting();
		} else { AppBuilder.Configure<Application>().UsePlatformDetect().SetupWithoutStarting(); }
		if(pointerIndex >= 0) { CheckSelection(pointerIndex + 1 < args.Length ? args[pointerIndex + 1] : "Nes", false, false, true); return; }
		int coldIndex = Array.IndexOf(args, "--cold-start");
		int stateIndex = Array.IndexOf(args, "--state");
		int settingsIndex = Array.IndexOf(args, "--settings");
		if(coldIndex >= 0) { CheckColdStartup(args[coldIndex + 1], args[coldIndex + 2], coldIndex + 3 < args.Length && !args[coldIndex + 3].StartsWith("--") ? args[coldIndex + 3] : "Tests/MetaSprites/obj/gameplay-fixture.sfc", stateIndex >= 0 ? args[stateIndex + 1] : null, Array.IndexOf(args, "--scroll-turn") >= 0, settingsIndex >= 0 ? args[settingsIndex + 1] : null, Array.IndexOf(args, "--software-renderer") >= 0); return; }
		if(Array.IndexOf(args, "--preview-benchmark") >= 0) { BenchmarkPreviews(); return; }
		int selectionIndex = Array.IndexOf(args, "--selection-suite");
		if(selectionIndex >= 0) { CheckSelection(selectionIndex + 1 < args.Length ? args[selectionIndex + 1] : "Snes", Array.IndexOf(args, "--startup-timing") >= 0, Array.IndexOf(args, "--recognition-benchmark") >= 0); return; }
		var model = new SpritePreviewModel();
		var info = new DebugSpriteInfo { Width = 8, Height = 8, SpriteIndex = 0 };
		var layout = new DebugSpritePreviewInfo();
		uint[] pixels = new uint[128 * 128];
		model.Init(ref info, pixels, layout);
		DynamicBitmap original = model.SpritePreview ?? throw new Exception("Preview missing.");
		int invalidations = 0;
		original.Invalidated += (_, _) => invalidations++;
		model.Init(ref info, pixels, layout);
		if(!ReferenceEquals(original, model.SpritePreview)) { throw new Exception("Unchanged dimensions replaced the bitmap."); }
		if(invalidations != 0) { throw new Exception("Unchanged pixels invalidated the preview."); }
		pixels[0] = 0xFF123456;
		pixels[63] = 0xFFABCDEF;
		model.Init(ref info, pixels, layout);
		CheckPreviewPixels(original, pixels, 64);
		if(invalidations != 1) { throw new Exception("Changed pixels did not invalidate exactly once."); }
		model.Init(ref info, pixels, layout);
		if(invalidations != 1) { throw new Exception("The updated pixel cache did not retain its contents."); }
		pixels[0] = 0;
		pixels[63] = 0;
		model.Init(ref info, pixels, layout);
		CheckPreviewPixels(original, pixels, 64);
		if(invalidations != 2) { throw new Exception("Returning to transparent pixels did not update the preview."); }
		info.Width = 16;
		model.Init(ref info, pixels, layout);
		if(ReferenceEquals(original, model.SpritePreview)) { throw new Exception("Dimension change did not replace the bitmap."); }
		CheckReleased(original);
		DynamicBitmap replacement = model.SpritePreview ?? throw new Exception("Replacement preview missing.");
		CheckPreviewPixels(replacement, pixels, 128);
		using(var bitmapLock = replacement.Lock()) { }
		model.ReleasePreview();
		if(model.SpritePreview != null) { throw new Exception("Release did not clear the binding."); }
		CheckReleased(replacement);
		model.ReleasePreview();
		Console.WriteLine("PASS: real Avalonia bitmap retention, dimension replacement, disposal and repeated release; no window created.");
		Console.WriteLine("PASS: unchanged previews stay valid; first/last pixel changes, transparency restoration and resize update the actual bitmap.");
		CheckGroupPng();
		CheckCreationSave();
		CheckGroupLabelBinding();
	}

	private static void CheckPointerInteraction(CpuType cpu)
	{
		var window = new Mesen.Debugger.Windows.SpriteViewerWindow(cpu);
		using var model = (SpriteViewerViewModel)window.DataContext!;
		try {
			var picture = window.FindControl<ScrollPictureViewer>("picViewer")!.InnerViewer;
			using var bitmap = new WriteableBitmap(new PixelSize(256, 256), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);
			picture.Source = bitmap; picture.Zoom = 1;
			picture.LeftClipSize = 0; picture.RightClipSize = 0; picture.TopClipSize = 0; picture.BottomClipSize = 0;
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900));
			model.SpritePreviews = new() {
				new() { SpriteIndex = 0, PreviewX = 10, PreviewY = 10, Width = 8, Height = 8, Visibility = SpriteVisibility.Visible },
				new() { SpriteIndex = 1, PreviewX = 252, PreviewY = 252, Width = 8, Height = 8, WrapWidth = 256, WrapHeight = 256, Visibility = SpriteVisibility.Visible }
			};
			var pointer = new Avalonia.Input.Pointer(17, Avalonia.Input.PointerType.Mouse, true);
			var pressed = new Avalonia.Input.PointerPointProperties(Avalonia.Input.RawInputModifiers.LeftMouseButton, Avalonia.Input.PointerUpdateKind.LeftButtonPressed);
			var moved = new Avalonia.Input.PointerPointProperties(Avalonia.Input.RawInputModifiers.LeftMouseButton, Avalonia.Input.PointerUpdateKind.Other);
			var released = new Avalonia.Input.PointerPointProperties(Avalonia.Input.RawInputModifiers.None, Avalonia.Input.PointerUpdateKind.LeftButtonReleased);
			Point RootPoint(Point point) => picture.TranslatePoint(new Point(point.X * picture.MinWidth / 256, point.Y * picture.MinHeight / 256), window) ?? throw new Exception("PictureViewer has no window transform.");
			void Press(Point point, Avalonia.Input.KeyModifiers modifiers = Avalonia.Input.KeyModifiers.None) => picture.RaiseEvent(new Avalonia.Input.PointerPressedEventArgs(picture, pointer, window, RootPoint(point), 0, pressed, modifiers, 1));
			void Move(Point point) => picture.RaiseEvent(new Avalonia.Input.PointerEventArgs(Avalonia.Input.InputElement.PointerMovedEvent, picture, pointer, window, RootPoint(point), 1, moved, Avalonia.Input.KeyModifiers.None));
			void Release(Point point) => picture.RaiseEvent(new Avalonia.Input.PointerReleasedEventArgs(picture, pointer, window, RootPoint(point), 2, released, Avalonia.Input.KeyModifiers.None, Avalonia.Input.MouseButton.Left));
			foreach(int zoom in new[] { 1, 2, 4 }) {
				picture.Zoom = zoom;
				foreach(bool reverse in new[] { false, true }) {
					model.ClearMetaSelection();
					Point start = reverse ? new Point(19, 19) : new Point(8, 8), end = reverse ? new Point(8, 8) : new Point(19, 19);
					Press(start); Move(end); Release(end);
					if(model.MetaSelectionDragging || pointer.Captured != null || model.SelectionRect != new Rect(10, 10, 8, 8)) { throw new Exception($"Actual routed rectangle drag did not select the sprite and release capture: dragging={model.MetaSelectionDragging}, captured={pointer.Captured}, rect={model.SelectionRect}, zoom={zoom}, reverse={reverse}."); }
				}
			}
			Press(new Point(11, 11), Avalonia.Input.KeyModifiers.Control); Release(new Point(11, 11));
			if(model.SelectionRect != default) { throw new Exception("Actual routed Ctrl-click did not toggle the selected sprite off."); }
			Press(new Point(1, 1), Avalonia.Input.KeyModifiers.Control); Release(new Point(1, 1));
			if(model.SelectionRect != new Rect(-4, -4, 8, 8)) { throw new Exception("Actual routed Ctrl-click lost wrapped sprite coordinates."); }
			Rect previous = model.SelectionRect;
			Press(new Point(8, 8)); Move(new Point(19, 19)); pointer.Capture(null);
			if(model.MetaSelectionDragging || model.SelectionRect != previous) { throw new Exception("Capture loss failed to cancel rectangle selection."); }
			Console.WriteLine($"PASS: actual {cpu} SpriteViewerWindow/PictureViewer routed pointer drag, Ctrl toggling, wrapped selection and capture-loss cancellation; no visible window.");
			model.Config.ShowSettingsPanel = true;
			model.RefreshData(); model.LoadMetaGroups(); Dispatcher.UIThread.RunJobs();
			model.SelectMetaSprites(new Rect(-4096, -4096, 8192, 8192));
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900)); Dispatcher.UIThread.RunJobs();
			var createButton = Avalonia.VisualTree.VisualExtensions.GetVisualDescendants(window).OfType<Button>().Single(button => button.Content as string == "Create meta group");
			createButton.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
			var group = model.SelectedMetaGroup ?? throw new Exception("Actual window could not create a group: " + model.MetaStatus);
			if(!group.Enabled || !model.PreviewUsesReplacement) { throw new Exception("Actual window creation did not assign/enable replacement PNG."); }
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900)); Dispatcher.UIThread.RunJobs();
			var canvas = window.FindControl<Canvas>("metaCanvas")!;
			void ClickOrigin(Point point) => canvas.RaiseEvent(new Avalonia.Input.PointerPressedEventArgs(canvas, pointer, window,
				canvas.TranslatePoint(point, window) ?? throw new Exception("Origin canvas has no window transform."), 3, pressed, Avalonia.Input.KeyModifiers.None, 1));
			ClickOrigin(new Point(2.2, 3.2));
			if(group.ReplacementOriginX != 2 || group.ReplacementOriginY != 3 || model.MetaPreviewOriginX != 2 || model.MetaPreviewOriginY != 3
				|| Canvas.GetLeft(canvas.Children[1]) != 2 || Canvas.GetTop(canvas.Children[2]) != 3) { throw new Exception("Actual origin canvas click did not update the artwork crosshair."); }
			model.LoadMetaGroups(); model.SelectedMetaGroup = model.MetaGroups[0];
			if(model.SelectedMetaGroup.ReplacementOriginX != 2 || model.SelectedMetaGroup.ReplacementOriginY != 3) { throw new Exception("Canvas-origin coordinates did not persist through reload."); }
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900)); Dispatcher.UIThread.RunJobs();
			window.FindControl<NumericUpDown>("metaReplacementOriginX")!.Value = 4;
			window.FindControl<NumericUpDown>("metaReplacementOriginY")!.Value = 5;
			if(model.SelectedMetaGroup.ReplacementOriginX != 4 || model.SelectedMetaGroup.ReplacementOriginY != 5
				|| model.MetaPreviewOriginX != 4 || model.MetaPreviewOriginY != 5) { throw new Exception("Numeric artwork controls did not update group/crosshair coordinates."); }
			var saveButton = Avalonia.VisualTree.VisualExtensions.GetVisualDescendants(window).OfType<Button>().Single(button => button.Content as string == "Save group edits");
			saveButton.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
			model.LoadMetaGroups(); model.SelectedMetaGroup = model.MetaGroups[0];
			if(model.SelectedMetaGroup.ReplacementOriginX != 4 || model.SelectedMetaGroup.ReplacementOriginY != 5) { throw new Exception("Numeric artwork controls did not persist through Save/reload."); }
			string packPath = Path.Combine(ConfigManager.HdPackFolder, "MetaSprites", cpu.ToString(), EmuApi.GetRomHash(HashType.Sha1), "groups.json");
			var scaleControl = window.FindControl<NumericUpDown>("metaReplacementScale")!;
			scaleControl.Value = 3; ClickScaleSave();
			void ClickScaleSave() => saveButton.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
			if(!model.MetaStatus.StartsWith("Replacements applied.") || MetaSpritePack.Load(packPath, EmuApi.GetRomHash(HashType.Sha1), cpu.ToString()).Groups[0].ReplacementScale != 3) { throw new Exception("Partial native canvas scale did not save/apply for the default PNG."); }
			scaleControl.Value = 1; ClickScaleSave();
			if(!model.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Correcting the artwork scale did not restore saving."); }
			string sourcePng = Path.Combine(Path.GetDirectoryName(packPath)!, model.SelectedMetaGroup.ReplacementPath);
			model.AddMetaAlternatePng(sourcePng, false);
			var firstAlternate = model.SelectedMetaAlternate ?? throw new Exception("Adding a draft alternate did not select its preview.");
			window.Measure(new Size(1000, 900)); window.Arrange(new Rect(0, 0, 1000, 900)); Dispatcher.UIThread.RunJobs();
			var alternateOriginCanvas = window.FindControl<Canvas>("metaAlternateCanvas")!;
			alternateOriginCanvas.RaiseEvent(new Avalonia.Input.PointerPressedEventArgs(alternateOriginCanvas, pointer, window,
				alternateOriginCanvas.TranslatePoint(new Point(1.2, 2.2), window) ?? throw new Exception("Alternate origin canvas has no transform."), 3, pressed, Avalonia.Input.KeyModifiers.None, 1));
			if(firstAlternate.OriginX != 1 || firstAlternate.OriginY != 2) { throw new Exception("Always-enabled alternate origin click failed."); }
			string retainedPng = Path.Combine(Path.GetDirectoryName(packPath)!, firstAlternate.Path);
			string baseArtworkPath = model.SelectedMetaGroup.ReplacementPath;
			model.ImportMetaReplacementPng(sourcePng);
			if(firstAlternate.Path == Path.GetFileName(retainedPng) || model.SelectedMetaGroup.ReplacementPath != baseArtworkPath
				|| model.SelectedMetaAlternate != firstAlternate || firstAlternate.MemoryConditions.Count != 0 || !File.Exists(retainedPng)) {
				throw new Exception("Loading artwork while viewing an alternate changed the default or lost alternate settings.");
			}
			string replacementPath = firstAlternate.Path;
			string beforeFailedArtworkImport = File.ReadAllText(packPath);
			int artworkFileCount = Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length;
			Directory.CreateDirectory(packPath + ".tmp");
			try { model.ImportMetaReplacementPng(sourcePng); }
			finally { Directory.Delete(packPath + ".tmp"); }
			if(firstAlternate.Path != replacementPath || File.ReadAllText(packPath) != beforeFailedArtworkImport
				|| Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length != artworkFileCount) {
				throw new Exception("Failed alternate artwork import changed its saved path or left an unsaved PNG.");
			}
			if(firstAlternate.MemoryConditions.Count != 0 || !model.ViewingMetaAlternate || !model.HasMetaAlternates) { throw new Exception("New alternate draft was not saved inactive in the shared preview."); }
			var previousButton = window.FindControl<Button>("metaArtworkPrevious")!;
			var nextButton = window.FindControl<Button>("metaArtworkNext")!;
			var removeButton = window.FindControl<Button>("metaArtworkRemove")!;
			var defaultPanel = window.FindControl<StackPanel>("metaDefaultArtworkPanel")!;
			var alternatePanel = window.FindControl<StackPanel>("metaAlternateArtworkPanel")!;
			void Click(Button button) => button.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
			Click(previousButton);
			if(model.ViewingMetaAlternate || removeButton.IsEnabled || !model.MetaArtworkLabel.StartsWith("Default PNG") || !defaultPanel.IsVisible || alternatePanel.IsVisible) { throw new Exception("Previous did not show the default PNG with remove disabled."); }
			Click(nextButton);
			if(model.SelectedMetaAlternate != firstAlternate || !removeButton.IsEnabled || defaultPanel.IsVisible || !alternatePanel.IsVisible) { throw new Exception("Next did not show the alternate with remove enabled."); }
			model.AddMetaAlternatePng(sourcePng, false);
			var secondAlternate = model.SelectedMetaAlternate!;
			Click(nextButton);
			if(model.ViewingMetaAlternate) { throw new Exception("Next did not wrap from the last alternate to the default PNG."); }
			Click(previousButton);
			if(model.SelectedMetaAlternate != secondAlternate) { throw new Exception("Previous did not wrap from default to the last alternate."); }
			window.FindControl<NumericUpDown>("metaAlternateScale")!.Value = 3;
			Click(nextButton);
			if(model.ViewingMetaAlternate || secondAlternate.Scale != 3) { throw new Exception("Browsing failed to preserve partial native canvas scale edits."); }
			model.SelectedMetaAlternate = null; Click(saveButton);
			if(!model.MetaStatus.StartsWith("Replacements applied.") || MetaSpritePack.Load(packPath, EmuApi.GetRomHash(HashType.Sha1), cpu.ToString()).Groups[0].AlternateArtwork[1].Scale != 3) { throw new Exception("Saving another PNG failed to preserve an unselected alternate's partial canvas scale."); }
			secondAlternate.Scale = 1; model.SelectedMetaAlternate = secondAlternate;
			model.MetaAlternateConditionsText = "invalid condition";
			var conditionsGroup = model.SelectedMetaGroup;
			model.MetaMemoryConditionsText = "invalid group condition";
			model.SelectedMetaGroup = null; model.SelectedMetaGroup = conditionsGroup;
			model.SelectedMetaAlternate = secondAlternate;
			if(model.MetaMemoryConditionsText != "invalid group condition" || model.MetaAlternateConditionsText != "invalid condition") {
				throw new Exception("Switching groups discarded invalid CPU condition drafts.");
			}
			model.MetaMemoryConditionsText = "";
			Click(nextButton);
			if(model.SelectedMetaAlternate != secondAlternate || model.MetaAlternateConditionsText != "invalid condition") { throw new Exception("Browsing discarded invalid alternate condition edits."); }
			model.MetaAlternateConditionsText = "";
			string beforeFailedRemoval = File.ReadAllText(packPath);
			Directory.CreateDirectory(packPath + ".tmp");
			try { Click(removeButton); }
			finally { Directory.Delete(packPath + ".tmp"); }
			if(model.SelectedMetaAlternate != secondAlternate || model.MetaAlternates.Count != 2 || !removeButton.IsEnabled
				|| File.ReadAllText(packPath) != beforeFailedRemoval) { throw new Exception("Failed alternate removal lost the selected PNG or changed saved definitions."); }
			Click(removeButton);
			if(model.SelectedMetaAlternate != firstAlternate || model.MetaAlternates.Count != 1) { throw new Exception("Remove did not select the remaining alternate."); }
			model.MetaAlternateConditionsText = "$0 = $00"; Click(saveButton);
			if(MetaSpritePack.Load(packPath, EmuApi.GetRomHash(HashType.Sha1), cpu.ToString()).Groups[0].AlternateArtwork[0].MemoryConditions.Count != 1) { throw new Exception("Draft alternate activation conditions did not persist."); }
			Click(removeButton);
			if(model.ViewingMetaAlternate || removeButton.IsEnabled || model.HasMetaAlternates || !File.Exists(retainedPng)
				|| MetaSpritePack.Load(packPath, EmuApi.GetRomHash(HashType.Sha1), cpu.ToString()).Groups[0].AlternateArtwork.Count != 0) {
				throw new Exception("Removing the last alternate failed to return to default, persist removal or retain source artwork.");
			}
			Console.WriteLine($"PASS: actual {cpu} previous/next/remove buttons browse and wrap default/alternates, save draft conditions and retain removed PNG files.");
			string oversizedInput = Path.Combine(ConfigManager.HomeFolder, "oversized-partial-input.png");
			using(var oversizedBitmap = new WriteableBitmap(new PixelSize(17, 19), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul)) {
				using(var pixels = oversizedBitmap.Lock()) {
					var pngRow = Enumerable.Repeat(unchecked((int)0xFF00FF00), 17).ToArray();
					for(int y = 0; y < 19; y++) { if(y == 18) { pngRow[16] = unchecked((int)0xFFFF00FF); } Marshal.Copy(pngRow, 0, pixels.Address + y * pixels.RowBytes, 17); }
				}
				oversizedBitmap.Save(oversizedInput, PngBitmapEncoderOptions.Default);
			}
			model.SelectedMetaGroup!.ReplacementScale = 2;
			model.ImportMetaReplacementPng(oversizedInput);
			if(!model.MetaStatus.StartsWith("Replacements applied.") || model.SelectedMetaGroup.ReplacementScale != 2
				|| model.MetaPreview?.PixelSize != new PixelSize(17, 19) || model.MetaPreviewWidth != 8.5 || model.MetaPreviewHeight != 9.5) {
				throw new Exception("Oversized default PNG lost its full canvas or selected scale during import.");
			}
			model.AddMetaAlternatePng(oversizedInput, false);
			if(model.SelectedMetaAlternate?.Scale != 2 || model.MetaAlternatePreview?.PixelSize != new PixelSize(17, 19)) {
				throw new Exception("Oversized alternate import reset the selected scale or changed its PNG canvas.");
			}
			model.LoadMetaGroups(); model.SelectedMetaGroup = model.MetaGroups[0];
			if(model.SelectedMetaGroup.ReplacementWidth != 17 || model.SelectedMetaGroup.ReplacementHeight != 19
				|| model.SelectedMetaGroup.ReplacementScale != 2 || model.SelectedMetaGroup.AlternateArtwork[0].Scale != 2) {
				throw new Exception("Oversized PNG canvas/scale did not persist across pack reload.");
			}
			Console.WriteLine($"PASS: actual {cpu} imports/persists a 17x19 default and alternate PNG at scale 2, with full fractional preview bounds.");
			string screenshotPath = Path.Combine(ConfigManager.HomeFolder, "sprite-viewer-layout.png");
			var content = (DockPanel)window.Content!;
			// The offscreen content render does not include the Window background.
			content.Background = window.Background;
			content.Measure(new Size(1000, 900)); content.Arrange(new Rect(0, 0, 1000, 900));
			Dispatcher.UIThread.RunJobs();
			using(var rendered = new RenderTargetBitmap(new PixelSize(1000, 900), new Vector(96, 96))) {
				rendered.Render(content); rendered.Save(screenshotPath, PngBitmapEncoderOptions.Default);
			}
			Console.WriteLine("LAYOUT: " + screenshotPath);
			Console.WriteLine($"PASS: actual {cpu} window creates enabled PNG artwork; routed preview-origin click updates crosshair bindings and persists through reload.");
		} finally { window.Close(); }
	}

	private static void CheckGroupLabelBinding()
	{
		var group = new MetaSpriteGroup { Id = 17 };
		var label = new TextBlock { DataContext = group };
		using var binding = label.Bind(TextBlock.TextProperty, new Avalonia.Data.Binding(nameof(MetaSpriteGroup.DisplayName)));
		if(label.Text != "Meta 017") { throw new Exception("Numbered group label binding is missing."); }
		group.Name = "Standing"; group.PoseName = "Idle";
		Dispatcher.UIThread.RunJobs();
		if(label.Text != "Meta 017 — Standing (Idle)") { throw new Exception("Group label did not update after name/pose edits."); }
		group.Name = ""; group.PoseName = "";
		Dispatcher.UIThread.RunJobs();
		if(label.Text != "Meta 017") { throw new Exception("Clearing optional group labels lost the persistent number."); }
		Console.WriteLine("PASS: actual Avalonia group label binding updates immediately for group/pose renames and clearing optional names.");
	}

	private static void CheckColdStartup(string sourceHome, string mode, string romPath, string? statePath, bool scrollTurn, string? settingsPath, bool softwareRenderer)
	{
		if(mode != "preloaded" && mode != "queued" && mode != "disabled") { throw new ArgumentException("Use preloaded, queued or disabled activation mode."); }
		if(statePath != null && !File.Exists(statePath)) { throw new FileNotFoundException("Gameplay save state missing.", statePath); }
		CpuType cpu = Path.GetExtension(romPath).Equals(".nes", StringComparison.OrdinalIgnoreCase) ? CpuType.Nes : CpuType.Snes;
		if(scrollTurn && (cpu != CpuType.Nes || statePath == null)) { throw new ArgumentException("Scroll-turn reproduction requires a NES gameplay save state."); }
		string home = Path.Combine(AppContext.BaseDirectory, "artifacts", "cold-" + mode + "-" + Guid.NewGuid().ToString("N"));
		string sourceAssets = Path.Combine(Path.GetFullPath(sourceHome), "HdPacks");
		foreach(string source in Directory.GetFiles(sourceAssets, "*", SearchOption.AllDirectories)) {
			string destination = Path.Combine(home, "HdPacks", Path.GetRelativePath(sourceAssets, source));
			Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
			File.Copy(source, destination);
		}
		typeof(ConfigManager).GetField("_homeFolder", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, home);
		typeof(ConfigManager).GetField("_config", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, settingsPath != null ? Configuration.Deserialize(Path.GetFullPath(settingsPath)) : Configuration.CreateConfig());
		// Preserve gameplay/video settings, but keep all test output in its own home.
		var preferences = ConfigManager.Config.Preferences;
		preferences.OverrideAviFolder = preferences.OverrideMovieFolder = preferences.OverrideSaveDataFolder = false;
		preferences.OverrideSaveStateFolder = preferences.OverrideScreenshotFolder = preferences.OverrideWaveFolder = false;
		Type loader = typeof(SpriteViewerViewModel).Assembly.GetType("Mesen.Debugger.Utilities.MetaSpriteRuntimeLoader")!;
		MethodInfo load = loader.GetMethod("LoadForGame", BindingFlags.Public | BindingFlags.Static)!;
		EmuApi.InitDll();
		EmuApi.InitializeEmu(home, IntPtr.Zero, IntPtr.Zero, true, true, true, true);
		ConfigManager.Config.ApplyConfig();
		SoftwareRendererView? softwareView = null;
		if(softwareRenderer) {
			ConfigureMetaSoftwareRendererTest();
			softwareView = new SoftwareRendererView { DataContext = new SoftwareRendererViewModel() };
			EmuApi.SetRendererSize(512, 480);
		}
		try {
			bool applied = false;
			using var listener = new NotificationListener();
			listener.OnNotification += e => {
				if(e.NotificationType == ConsoleNotificationType.RefreshSoftwareRenderer && softwareView != null) {
					softwareView.UpdateSoftwareRenderer(Marshal.PtrToStructure<SoftwareRendererFrame>(e.Parameter));
					return;
				}
				if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
				string hash = EmuApi.GetRomHash(HashType.Sha1);
				if(mode == "disabled") {
					// Change only the isolated copy. Keep recognition metadata for the
					// same viewer workload, but omit native replacement capture/rendering.
					string path = Path.Combine(home, "HdPacks", "MetaSprites", cpu.ToString(), hash, "groups.json");
					var disabledPack = MetaSpritePack.Load(path, hash, cpu.ToString());
					foreach(var group in disabledPack.Groups) { group.Enabled = false; }
					foreach(var layer in disabledPack.BackgroundLayers) { layer.Enabled = false; }
					disabledPack.Save(path);
				}
				void Apply() {
					var decoding = Stopwatch.StartNew();
					applied = (bool)load.Invoke(null, new object[] { hash, cpu })!;
					Console.WriteLine($"COLD ACTIVATION: {mode}, {decoding.Elapsed.TotalMilliseconds:F3} ms, applied={applied}, frame={EmuApi.GetTimingInfo(cpu).FrameCount}.");
				}
				if(mode == "queued") { Dispatcher.UIThread.Post(Apply); } else { Apply(); }
			};
			if(!EmuApi.LoadRom(Path.GetFullPath(romPath))) { throw new Exception("Cold-start ROM failed to load."); }
			DebugApi.InitializeDebugger();
			try {
				if(statePath != null) {
					EmuApi.Pause();
					int stateLoads = 0;
					listener.OnNotification += e => { if(e.NotificationType == ConsoleNotificationType.StateLoaded) { stateLoads++; } };
					EmuApi.LoadStateFile(Path.GetFullPath(statePath));
					if(stateLoads != 1) { throw new Exception("Gameplay save state was not successfully loaded."); }
					Console.WriteLine("GAMEPLAY STATE: " + Path.GetFileName(statePath));
					using var preview = EmuApi.GetSaveStatePreview(statePath);
					preview?.Save(Path.Combine(home, "starting-scene.png"), PngBitmapEncoderOptions.Default);
				}
				using var viewer = new SpriteViewerViewModel(cpu, new PictureViewer(), new ScrollPictureViewer(), new Grid(), new Grid(), new Window());
				string hash = EmuApi.GetRomHash(HashType.Sha1);
				var pack = MetaSpritePack.Load(Path.Combine(home, "HdPacks", "MetaSprites", cpu.ToString(), hash, "groups.json"), hash, cpu.ToString());
				// Populate recognition metadata without activating again: only the
				// selected GameLoaded path may decode/apply assets in this comparison.
				foreach(var group in pack.Groups) { viewer.MetaGroups.Add(group); }
				const int viewerId = 19072;
				int cameraX = -1, previousCameraX = -1, stableCameraFrames = 0;
				bool cameraMoved = false;
				bool cameraMovedAfterReversal = false;
				int movementPhase = 0;
				int boundaryCameraX = -1;
				uint movementFrames = 0, boundaryFrame = 0, reversalFrame = 0;
				listener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.ViewerRefresh || e.Parameter.ToInt32() != viewerId) { return; }
					viewer.RefreshData();
					if(!scrollTurn) { return; }
					NesPpuState ppu = DebugApi.GetPpuState<NesPpuState>(cpu);
					int x = ((ppu.VideoRamAddr & 0x1F) << 3) | ((ppu.VideoRamAddr & 0x400) != 0 ? 256 : 0);
					x -= (int)(ppu.Cycle & ~7u) + 16;
					cameraX = (x + ppu.ScrollX + 512) % 512;
					movementFrames++;
					if(previousCameraX >= 0 && cameraX != previousCameraX) {
						cameraMoved = true; stableCameraFrames = 0;
						if(movementPhase == 2) { cameraMovedAfterReversal = true; }
					}
					else { stableCameraFrames++; }
					previousCameraX = cameraX;
					if(movementPhase == 0 && cameraMoved && stableCameraFrames >= 30) {
						movementPhase = 1; boundaryFrame = movementFrames; boundaryCameraX = cameraX;
						Console.WriteLine($"SCROLL TURN: camera stationary for 30 frames at scroll X={cameraX}, movement frame={movementFrames}; continue right for 30 frames.");
					} else if(movementPhase == 1 && cameraX != boundaryCameraX) {
						movementPhase = 0;
						Console.WriteLine($"SCROLL TURN: camera resumed before reversal at movement frame={movementFrames}; keep walking right.");
					} else if(movementPhase == 1 && movementFrames - boundaryFrame >= 30) {
						DebugApi.SetInputOverrides(0, new DebugControllerState { Left = true });
						movementPhase = 2; reversalFrame = movementFrames;
						Console.WriteLine($"SCROLL TURN: reverse left at movement frame={movementFrames}, scroll X={cameraX}.");
					} else if(movementPhase == 2 && ((cameraMovedAfterReversal && stableCameraFrames >= 30) || movementFrames - reversalFrame >= 360)) {
						DebugApi.SetInputOverrides(0, default);
						movementPhase = 3;
						Console.WriteLine($"SCROLL TURN: neutral input at movement frame={movementFrames}, scroll X={cameraX}.");
					}
				};
				DebugApi.SetViewerUpdateTiming(viewerId, scrollTurn ? 120 : 8, 12, cpu);
				if(scrollTurn) { DebugApi.SetInputOverrides(0, new DebugControllerState { Right = true }); }
				if(statePath != null) { EmuApi.Resume(); }
				var timer = Stopwatch.StartNew();
				double previousSeconds = 0;
				uint previousFrame = EmuApi.GetTimingInfo(cpu).FrameCount;
				long previousAllocated = GC.GetTotalAllocatedBytes(true);
				int previousGen0 = GC.CollectionCount(0), previousGen1 = GC.CollectionCount(1), previousGen2 = GC.CollectionCount(2);
				string report = "seconds,frames_per_second,allocated_bytes,gen0,gen1,gen2,camera_x,movement_phase\n";
				bool observedMatches = false;
				while(timer.Elapsed.TotalSeconds < 60) {
					Dispatcher.UIThread.RunJobs();
					observedMatches |= viewer.MetaMatchStatus.StartsWith("Meta ", StringComparison.Ordinal);
					double seconds = timer.Elapsed.TotalSeconds;
					if(seconds - previousSeconds >= 5) {
						uint frame = EmuApi.GetTimingInfo(cpu).FrameCount;
						double fps = (frame - previousFrame) / (seconds - previousSeconds);
						long allocated = GC.GetTotalAllocatedBytes(true);
						int gen0 = GC.CollectionCount(0), gen1 = GC.CollectionCount(1), gen2 = GC.CollectionCount(2);
						Console.WriteLine($"COLD TIMING: {mode}, {seconds:F1}s, {fps:F2} emulated FPS, {allocated - previousAllocated} bytes, GC {gen0 - previousGen0}/{gen1 - previousGen1}/{gen2 - previousGen2}, camera X={cameraX}, phase={movementPhase}; excludes desktop presentation.");
						report += FormattableString.Invariant($"{seconds:F3},{fps:F3},{allocated - previousAllocated},{gen0 - previousGen0},{gen1 - previousGen1},{gen2 - previousGen2},{cameraX},{movementPhase}\n");
						previousAllocated = allocated; previousGen0 = gen0; previousGen1 = gen1; previousGen2 = gen2;
						previousSeconds = seconds; previousFrame = frame;
						if(scrollTurn) {
							EmuApi.TakeScreenshot();
							Console.WriteLine("SCENE MATCHES: " + viewer.MetaMatchStatus.Replace(Environment.NewLine, "; "));
						}
						if(softwareView?.DataContext is SoftwareRendererViewModel softwareModel && softwareModel.FrameSurface is DynamicBitmap surface) {
							using var snapshot = new WriteableBitmap(surface.PixelSize, surface.Dpi, PixelFormat.Bgra8888, AlphaFormat.Premul);
							using(var framebuffer = snapshot.Lock()) {
								surface.CopyPixels(new PixelRect(0, 0, surface.PixelSize.Width, surface.PixelSize.Height), framebuffer.Address, framebuffer.RowBytes * framebuffer.Size.Height, framebuffer.RowBytes);
							}
							snapshot.Save(Path.Combine(home, $"composited-{(int)seconds:D2}.png"), PngBitmapEncoderOptions.Default);
						}
					}
					System.Threading.Thread.Sleep(1);
				}
				DebugApi.RemoveViewerId(viewerId, cpu);
				if(scrollTurn) {
					DebugApi.SetInputOverrides(0, default);
					Console.WriteLine($"SCROLL TURN RESULT: camera moved={cameraMoved}, reversal reached={movementPhase >= 2}, completed={movementPhase == 3}.");
				}
				if(!applied) { throw new Exception("Cold-start saved pack activation did not succeed."); }
				if(settingsPath != null && pack.Groups.Count > 0 && !observedMatches) { throw new Exception("Configured run never recognized a supplied group; timings do not verify actual replacement artwork."); }
				if(softwareRenderer && (softwareView?.DataContext is not SoftwareRendererViewModel renderedModel || renderedModel.FrameSurface == null)) { throw new Exception("Software renderer never delivered a frame."); }
				File.WriteAllText(Path.Combine(home, "startup-timing.csv"), report);
				Console.WriteLine("Cold timing saved: " + Path.Combine(home, "startup-timing.csv"));
			} finally { DebugApi.ReleaseDebugger(); }
		} finally {
			EmuApi.Stop(); EmuApi.Release();
			if(softwareView?.DataContext is SoftwareRendererViewModel softwareModel) {
				softwareModel.FrameSurface?.Dispose(); softwareModel.EmuHudSurface?.Dispose(); softwareModel.ScriptHudSurface?.Dispose();
			}
		}
	}

	private static void CheckSelection(string system, bool startupTiming, bool recognitionBenchmark, bool pointerTesting = false)
	{
		var (cpu, extension) = system switch {
			"Snes" => (CpuType.Snes, "sfc"), "Nes" => (CpuType.Nes, "nes"),
			"Gameboy" => (CpuType.Gameboy, "gb"), "Gbc" => (CpuType.Gameboy, "gbc"),
			"Gba" => (CpuType.Gba, "gba"), "Pce" => (CpuType.Pce, "pce"),
			"Sms" => (CpuType.Sms, "sms"), "Ws" => (CpuType.Ws, "wsc"),
			_ => throw new ArgumentException("Unknown selection test system: " + system)
		};
		string romPath = Path.GetFullPath("Tests/MetaSprites/obj/gameplay-fixture." + extension);
		string home = Path.Combine(AppContext.BaseDirectory, "artifacts", "selection-" + Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(home);
		typeof(ConfigManager).GetField("_homeFolder", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, home);
		typeof(ConfigManager).GetField("_config", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, Configuration.CreateConfig());
		EmuApi.InitDll();
		EmuApi.InitializeEmu(home, IntPtr.Zero, IntPtr.Zero, true, true, true, true);
		ConfigManager.Config.ApplyConfig();
		try {
			if(!EmuApi.LoadRom(romPath)) { throw new Exception("Selection fixture ROM failed to load: " + system); }
			System.Threading.Thread.Sleep(150);
			EmuApi.Pause();
			DebugApi.InitializeDebugger();
			try {
				if(pointerTesting) { CheckPointerInteraction(cpu); return; }
				using var viewer = new SpriteViewerViewModel(cpu, new PictureViewer(), new ScrollPictureViewer(), new Grid(), new Grid(), new Window());
				var main = new SpritePreviewModel { SpriteIndex = 0, PreviewX = 10, PreviewY = 10, Width = 8, Height = 8, Visibility = SpriteVisibility.Visible };
				var wrapped = new SpritePreviewModel { SpriteIndex = 1, PreviewX = 252, PreviewY = 252, Width = 8, Height = 8, WrapWidth = 256, WrapHeight = 256, Visibility = SpriteVisibility.Visible };
				var disabled = new SpritePreviewModel { SpriteIndex = 2, PreviewX = 0, PreviewY = 0, Width = 8, Height = 8, Visibility = SpriteVisibility.Disabled };
				viewer.SpritePreviews = new() { main, wrapped, disabled };
				viewer.SelectMetaSprites(new Rect(0, 0, 4, 4));
				if(viewer.SelectionRect != new Rect(-4, -4, 8, 8)) { throw new Exception("Wrapped rectangle selection did not retain the selected copy's origin."); }
				viewer.ToggleMetaSprite(main);
				if(viewer.SelectionRect != new Rect(-4, -4, 22, 22)) { throw new Exception("Ctrl-click addition did not preserve the wrapped member offset."); }
				viewer.ToggleMetaSprite(wrapped);
				if(viewer.SelectionRect != new Rect(10, 10, 8, 8)) { throw new Exception("Ctrl-click removal did not update group bounds."); }
				viewer.SelectMetaSprites(new Rect(18, 10, 1, 8));
				if(viewer.SelectionRect != default) { throw new Exception("Edge contact selected a sprite without positive overlap."); }
				viewer.ToggleMetaSprite(wrapped, new Rect(-4, 252, 8, 8));
				if(viewer.SelectionRect != new Rect(-4, 252, 8, 8)) { throw new Exception("Ctrl-click did not retain the clicked wrap copy."); }
				viewer.ClearMetaSelection();
				if(viewer.SelectionRect != default) { throw new Exception("Clearing group selection retained its bounds."); }
				Console.WriteLine("PASS: actual viewer model rectangle selection, disabled-sprite exclusion, wrapped-copy offsets, Ctrl-click toggling, edge contact and clear.");
				viewer.LoadMetaGroups();
				viewer.RefreshData();
				Dispatcher.UIThread.RunJobs();
				if(viewer.SpritePreviews.Count == 0 || viewer.SpritePreviews[0].Width <= 0 || viewer.SpritePreviews[0].Height <= 0) { throw new Exception("Actual sprite snapshot was not refreshed: " + system); }
				viewer.ToggleMetaSprite(viewer.SpritePreviews[0]);
				viewer.CreateMetaGroup();
				if(viewer.MetaGroups.Count != 1 || !viewer.MetaGroups[0].Enabled || !viewer.MetaStatus.StartsWith("Replacements applied.")) {
					throw new Exception("Actual Create flow failed: " + viewer.MetaStatus);
				}
				string hash = EmuApi.GetRomHash(HashType.Sha1);
				string packPath = Path.Combine(home, "HdPacks", "MetaSprites", cpu.ToString(), hash, "groups.json");
				var saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(saved.Groups.Count != 1 || !saved.Groups[0].Enabled || !File.Exists(Path.Combine(Path.GetDirectoryName(packPath)!, saved.Groups[0].ReplacementPath))) {
					throw new Exception("Actual Create flow did not persist its enabled PNG group in the default folder.");
				}
				viewer.SelectedMetaGroup!.Name = "Standing";
				viewer.SetSelectedMetaOrigin(2, 3);
				if(!viewer.PreviewUsesReplacement || viewer.MetaPreviewOriginX != 2 || viewer.MetaPreviewOriginY != 3) {
					throw new Exception("Artwork origin editing did not update the preview crosshair.");
				}
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(saved.Groups[0].Name != "Standing" || saved.Groups[0].ReplacementOriginX != 2 || saved.Groups[0].ReplacementOriginY != 3) {
					throw new Exception("Named group/artwork origin changes did not persist.");
				}
				viewer.CreateMetaGroup(true);
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(saved.Groups.Count != 2 || saved.NextGroupId != 3 || saved.Groups[0].PoseFamilyId != 1 || saved.Groups[1].PoseFamilyId != 1
					|| saved.Groups[1].Id != 2 || !saved.Groups[1].Enabled || saved.Groups[1].Name != "Standing"
					|| saved.Groups[1].OriginX != saved.Groups[0].OriginX || saved.Groups[1].OriginY != saved.Groups[0].OriginY
					|| !File.Exists(Path.Combine(Path.GetDirectoryName(packPath)!, saved.Groups[1].ReplacementPath))) {
					throw new Exception("Actual Create-pose flow did not preserve family/origin or automatically save enabled artwork.");
				}
				viewer.LoadMetaGroups();
				if(viewer.MetaGroups.Count != 2 || viewer.MetaGroups[0].Id != 1 || viewer.MetaGroups[1].Id != 2) { throw new Exception("Actual viewer reload lost the created groups."); }
				Console.WriteLine($"PASS: actual {system} snapshot -> selection -> Create -> default PNG/JSON save -> native activation -> viewer reload; no visible window.");
				Console.WriteLine($"PASS: {system} named group, artwork origin/crosshair and enabled pose family survive actual authoring and reload.");
				viewer.SelectedMetaGroup = viewer.MetaGroups[0];
				viewer.MetaGroups[1].ReplacementOriginX = -15; viewer.MetaGroups[1].ReplacementOriginY = -20;
				string beforeFailedAlignment = File.ReadAllText(packPath);
				Directory.CreateDirectory(packPath + ".tmp");
				try { viewer.AlignMetaPoseArtworkOrigins(); }
				finally { Directory.Delete(packPath + ".tmp"); }
				if(viewer.MetaGroups[1].ReplacementOriginX != -15 || viewer.MetaGroups[1].ReplacementOriginY != -20
					|| File.ReadAllText(packPath) != beforeFailedAlignment) { throw new Exception("Failed PNG pose alignment did not restore prior anchors/definitions."); }
				viewer.AlignMetaPoseArtworkOrigins();
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(saved.Groups[1].ReplacementOriginX != 2 || saved.Groups[1].ReplacementOriginY != 3
					|| saved.Groups[1].OriginX != viewer.MetaGroups[1].OriginX || saved.Groups[1].OriginY != viewer.MetaGroups[1].OriginY) {
					throw new Exception("PNG pose alignment failed to persist aligned artwork anchors while preserving native anchors.");
				}
				string alternateSource = Path.Combine(Path.GetDirectoryName(packPath)!, viewer.SelectedMetaGroup.ReplacementPath);
				var importGroup = viewer.SelectedMetaGroup;
				var previousPreview = viewer.MetaPreview;
				string beforeFailedImport = File.ReadAllText(packPath);
				string previousArtwork = importGroup.ReplacementPath;
				int replacementFiles = Directory.GetFiles(Path.GetDirectoryName(packPath)!, "meta-*-replacement-*.png").Length;
				viewer.MetaMemoryConditionsText = "invalid condition";
				viewer.ImportMetaReplacementPng(alternateSource);
				if(importGroup.ReplacementPath != previousArtwork || !ReferenceEquals(previousPreview, viewer.MetaPreview)
					|| File.ReadAllText(packPath) != beforeFailedImport || !File.Exists(alternateSource)
					|| Directory.GetFiles(Path.GetDirectoryName(packPath)!, "meta-*-replacement-*.png").Length != replacementFiles) {
					throw new Exception("Replacement save failure changed existing artwork/preview or left an orphan PNG.");
				}
				viewer.MetaMemoryConditionsText = "";
				viewer.ImportMetaReplacementPng(alternateSource);
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				if(importGroup.ReplacementPath == previousArtwork || saved.Groups[0].ReplacementPath != importGroup.ReplacementPath
					|| !viewer.PreviewUsesReplacement || ReferenceEquals(previousPreview, viewer.MetaPreview)
					|| !File.Exists(alternateSource) || saved.Groups[0].ReplacementOriginX != 2 || saved.Groups[0].ReplacementOriginY != 3) {
					throw new Exception("Replacement import did not save/update preview while preserving artwork origin and source PNG.");
				}
				Console.WriteLine($"PASS: {system} replacement PNG import persists artwork/origin; failed saves preserve prior preview and clean up unsaved copies.");
				viewer.MetaAlternateConditionsText = "$0 = $03 / $0F\n$1 = $07";
				viewer.AddMetaAlternatePng(alternateSource);
				if(viewer.MetaAlternates.Count != 1 || viewer.SelectedMetaAlternate == null || !viewer.MetaStatus.StartsWith("Replacements applied.")) {
					throw new Exception("Actual conditional alternate PNG import failed: " + viewer.MetaStatus);
				}
				viewer.SetSelectedMetaAlternateOrigin(4, 5);
				saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
				var authoredAlternate = saved.Groups[0].AlternateArtwork;
				if(authoredAlternate.Count != 1 || authoredAlternate[0].OriginX != 4 || authoredAlternate[0].OriginY != 5
					|| authoredAlternate[0].MemoryConditions.Count != 2 || authoredAlternate[0].MemoryConditions[0].Mask != 15
					|| authoredAlternate[0].MemoryConditions[1].Address != 1) { throw new Exception("Actual alternate conditions/origin did not persist."); }
				string beforeFailedAlternate = File.ReadAllText(packPath);
				int alternateFiles = Directory.GetFiles(Path.GetDirectoryName(packPath)!, "meta-*-alternate-*.png").Length;
				viewer.MetaMemoryConditionsText = "invalid condition";
				viewer.AddMetaAlternatePng(alternateSource);
				if(viewer.MetaAlternates.Count != 1 || File.ReadAllText(packPath) != beforeFailedAlternate
					|| Directory.GetFiles(Path.GetDirectoryName(packPath)!, "meta-*-alternate-*.png").Length != alternateFiles) {
					throw new Exception("Alternate save failure left an orphan PNG or changed prior definitions.");
				}
				viewer.MetaMemoryConditionsText = "";
				Console.WriteLine($"PASS: {system} alternate PNG authoring with two masked CPU-byte conditions and independent origin; failed saves preserve existing assets/definitions.");
				using(var tilemap = new TilemapViewerViewModel(cpu, new PictureViewer(), new ScrollPictureViewer(), new Window())) {
					tilemap.OnGameLoaded();
					Dispatcher.UIThread.RunJobs();
					string exported = Path.Combine(home, "tilemap.png");
					using(var snapshot = tilemap.CaptureBackgroundPngSnapshot()) { snapshot.Save(exported, PngBitmapEncoderOptions.Default); }
					byte[] transparentExport = File.ReadAllBytes(exported);
					foreach(var backdrop in new[] { TilemapBackground.Black, TilemapBackground.White, TilemapBackground.Magenta }) {
						tilemap.Config.Background = backdrop;
						using var snapshot = tilemap.CaptureBackgroundPngSnapshot();
						snapshot.Save(exported, PngBitmapEncoderOptions.Default);
						if(!transparentExport.SequenceEqual(File.ReadAllBytes(exported))) { throw new Exception("Tilemap export depends on the viewer backdrop."); }
					}
					tilemap.SelectionRect = new Rect(0, 0, 8, 8);
					tilemap.ExportBackgroundPng(new Window()).GetAwaiter().GetResult();
					tilemap.ExportBackgroundPng(new Window()).GetAwaiter().GetResult();
					string exportStem = $"tilemap-{cpu}-layer-{tilemap.SelectedTab.Layer:D2}";
					foreach(string suffix in new[] { "", "-002" }) {
						string exportedTilemapPath = Path.Combine(Path.GetDirectoryName(packPath)!, exportStem + suffix + ".png");
						if(!File.Exists(exportedTilemapPath) || !transparentExport.SequenceEqual(File.ReadAllBytes(exportedTilemapPath))) { throw new Exception("Default tilemap export naming, transparency or full canvas failed: " + tilemap.BackgroundEditor.MetaStatus); }
					}
					tilemap.BackgroundEditor.ImportMetaBackgroundPng(exported, 0);
					var background = tilemap.BackgroundEditor.SelectedMetaBackground ?? throw new Exception("Actual tilemap PNG import failed: " + tilemap.BackgroundEditor.MetaStatus);
					string beforeUngatedBackground = File.ReadAllText(packPath);
					background.Enabled = true;
					tilemap.BackgroundEditor.SaveMetaGroups();
					if(File.ReadAllText(packPath) != beforeUngatedBackground || !tilemap.BackgroundEditor.MetaStatus.Contains("requires at least one CPU memory condition")) {
						throw new Exception("Background editor allowed gameplay activation without a configured CPU check.");
					}
					background.X = -7; background.Y = 11; background.FollowScroll = true; background.WrapX = true; background.Enabled = true;
					background.MemoryConditions = new() { new() { Address = 0, Value = 0, Mask = 15 } };
					tilemap.BackgroundEditor.MemoryConditionsText = "invalid background condition";
					tilemap.BackgroundEditor.SelectedMetaBackground = null;
					tilemap.BackgroundEditor.SelectedMetaBackground = background;
					if(tilemap.BackgroundEditor.MemoryConditionsText != "invalid background condition") { throw new Exception("Switching backgrounds discarded invalid condition text."); }
					tilemap.BackgroundEditor.SaveMetaGroups();
					if(File.ReadAllText(packPath) != beforeUngatedBackground) { throw new Exception("Invalid background condition text overwrote the pack."); }
					tilemap.BackgroundEditor.MemoryConditionsText = MetaMemoryConditionText.Format(background.MemoryConditions);
					tilemap.BackgroundEditor.SaveMetaGroups();
					if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Actual background activation failed: " + tilemap.BackgroundEditor.MetaStatus); }
					string backgroundAssetPath = Path.Combine(Path.GetDirectoryName(packPath)!, background.ArtworkPath);
					byte[] originalBackgroundAsset = File.ReadAllBytes(backgroundAssetPath);
					string beforeChangedCanvas = File.ReadAllText(packPath);
					try {
						using var changedCanvas = new WriteableBitmap(new PixelSize(1, 1), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);
						changedCanvas.Save(backgroundAssetPath, PngBitmapEncoderOptions.Default);
						tilemap.BackgroundEditor.SaveMetaGroups();
						if(File.ReadAllText(packPath) != beforeChangedCanvas || !tilemap.BackgroundEditor.MetaStatus.Contains("dimensions changed")) { throw new Exception("Edited background PNG dimensions were committed before validation."); }
						tilemap.BackgroundEditor.ReplaceBackgroundPngFile(exported);
						if(background.ArtworkPath == Path.GetFileName(backgroundAssetPath) || background.X != -7 || background.Y != 11
							|| background.MemoryConditions.Count != 1 || !background.Enabled || !File.Exists(backgroundAssetPath)
							|| !tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Replacing the resized background failed to preserve placement/conditions or activate."); }
					} finally { File.WriteAllBytes(backgroundAssetPath, originalBackgroundAsset); }
					string beforeFailedBackgroundReplacement = File.ReadAllText(packPath);
					string retainedBackgroundPath = background.ArtworkPath;
					var retainedBackgroundPreview = tilemap.BackgroundEditor.MetaBackgroundPreview;
					int beforeReplacementPngCount = Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length;
					Directory.CreateDirectory(packPath + ".tmp");
					try { tilemap.BackgroundEditor.ReplaceBackgroundPngFile(exported); }
					finally { Directory.Delete(packPath + ".tmp"); }
					if(background.ArtworkPath != retainedBackgroundPath || File.ReadAllText(packPath) != beforeFailedBackgroundReplacement
						|| tilemap.BackgroundEditor.MetaBackgroundPreview != retainedBackgroundPreview
						|| Directory.GetFiles(Path.GetDirectoryName(packPath)!, "*.png").Length != beforeReplacementPngCount) {
						throw new Exception("Failed background replacement changed saved artwork/preview or left an unsaved PNG.");
					}
					viewer.LoadMetaGroups(); viewer.SaveMetaGroups();
					saved = MetaSpritePack.Load(packPath, hash, cpu.ToString());
					if(saved.Groups.Count != 2 || saved.BackgroundLayers.Count != 1 || saved.BackgroundLayers[0].X != -7 || saved.BackgroundLayers[0].Y != 11
						|| !saved.BackgroundLayers[0].Enabled || !saved.BackgroundLayers[0].FollowScroll || !saved.BackgroundLayers[0].WrapX
						|| saved.BackgroundLayers[0].MemoryConditions.Count != 1 || saved.BackgroundLayers[0].MemoryConditions[0].Mask != 15) {
						throw new Exception("Tilemap layer placement/conditions or sprite groups were lost during shared-pack saving.");
					}
					tilemap.BackgroundEditor.Load();
					if(tilemap.BackgroundEditor.MetaBackgrounds.Count != 1) { throw new Exception("Tilemap editor reload lost the imported background."); }
					string assetDirectory = Path.GetDirectoryName(packPath)!;
					int assetCount = Directory.GetFiles(assetDirectory, "background-*.png").Length;
					string priorJson = File.ReadAllText(packPath);
					tilemap.BackgroundEditor.ImportMetaBackgroundPng(exported, -1);
					var afterFailure = MetaSpritePack.Load(packPath, hash, cpu.ToString());
					if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Could not import background:") || afterFailure.NextBackgroundId != 2
						|| tilemap.BackgroundEditor.MetaBackgrounds.Count != 1 || File.ReadAllText(packPath) != priorJson
						|| Directory.GetFiles(assetDirectory, "background-*.png").Length != assetCount || !File.Exists(exported)) {
						throw new Exception("Failed background import left an orphan PNG or changed existing definitions/source artwork.");
					}
					Console.WriteLine($"PASS: {system} actual tilemap snapshot PNG export/import, placement/scroll settings, CPU-byte condition, native activation and shared-pack preservation.");
					Console.WriteLine($"PASS: {system} background import save failure rolls back new PNG and ID while retaining prior JSON/source artwork.");
					using(var otherBackgroundEditor = new MetaBackgroundEditorViewModel(cpu)) {
						otherBackgroundEditor.Load();
						otherBackgroundEditor.SelectedMetaBackground = otherBackgroundEditor.MetaBackgrounds[0];
						otherBackgroundEditor.SelectedMetaBackground.X = -17;
						otherBackgroundEditor.SaveMetaGroups();
						if(!otherBackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Second background editor save failed."); }
						string latestBackgrounds = File.ReadAllText(packPath);
						tilemap.BackgroundEditor.SelectedMetaBackground = tilemap.BackgroundEditor.MetaBackgrounds[0];
						tilemap.BackgroundEditor.SaveMetaGroups();
						if(!tilemap.BackgroundEditor.MetaStatus.Contains("saved by another editor")) { throw new Exception("Stale background save was accepted."); }
						tilemap.BackgroundEditor.RemoveMetaBackground();
						tilemap.BackgroundEditor.ImportMetaBackgroundPng(exported, 0);
						if(File.ReadAllText(packPath) != latestBackgrounds || tilemap.BackgroundEditor.MetaBackgrounds.Count != 1
							|| Directory.GetFiles(assetDirectory, "background-*.png").Length != assetCount) {
							throw new Exception("Stale background save/remove/import overwrote newer layers or left unsaved artwork.");
						}
						tilemap.BackgroundEditor.Load();
						tilemap.BackgroundEditor.SelectedMetaBackground = tilemap.BackgroundEditor.MetaBackgrounds[0];
						if(tilemap.BackgroundEditor.SelectedMetaBackground.X != -17) { throw new Exception("Background reload lost newer placement."); }
						tilemap.BackgroundEditor.SelectedMetaBackground.X = -7;
						tilemap.BackgroundEditor.SaveMetaGroups();
						if(!tilemap.BackgroundEditor.MetaStatus.StartsWith("Background definitions saved and applied.")) { throw new Exception("Background reload did not restore editing."); }
						Console.WriteLine($"PASS: {system} stale background save/remove/import preserve newer layers and assets; reload restores editing.");
					}
				}
				using(var otherViewer = new SpriteViewerViewModel(cpu, new PictureViewer(), new ScrollPictureViewer(), new Grid(), new Grid(), new Window())) {
					otherViewer.LoadMetaGroups();
					otherViewer.SelectedMetaGroup = otherViewer.MetaGroups[0];
					otherViewer.SelectedMetaGroup.Name = "Updated in second editor";
					otherViewer.SaveMetaGroups();
					if(!otherViewer.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Second editor could not save group changes: " + otherViewer.MetaStatus); }
					string latestDefinitions = File.ReadAllText(packPath);
					viewer.SaveMetaGroups();
					if(File.ReadAllText(packPath) != latestDefinitions || !viewer.MetaStatus.Contains("saved by another editor")) {
						throw new Exception("Stale Sprite Viewer overwrote newer group definitions.");
					}
					viewer.LoadMetaGroups(); viewer.SelectedMetaGroup = viewer.MetaGroups[0];
					if(viewer.SelectedMetaGroup.Name != "Updated in second editor") { throw new Exception("Reload did not retrieve the second editor's changes."); }
					viewer.SelectedMetaGroup.Name = "Standing"; viewer.SaveMetaGroups();
					if(!viewer.MetaStatus.StartsWith("Replacements applied.")) { throw new Exception("Reload did not restore group editing."); }
					Console.WriteLine($"PASS: {system} stale Sprite Viewer saves preserve newer definitions; reload restores editing.");
				}
				if(recognitionBenchmark) {
					MethodInfo refresh = typeof(SpriteViewerViewModel).GetMethod("InternalRefreshTab", BindingFlags.NonPublic | BindingFlags.Instance)!;
					for(int i = 0; i < 30; i++) { refresh.Invoke(viewer, null); }
					string expectedMatches = viewer.MetaMatchStatus;
					long allocated = GC.GetAllocatedBytesForCurrentThread();
					var timer = Stopwatch.StartNew();
					const int iterations = 240;
					for(int i = 0; i < iterations; i++) {
						refresh.Invoke(viewer, null);
						if(viewer.MetaMatchStatus != expectedMatches) { throw new Exception("Repeated snapshot refresh changed recognition results."); }
					}
					double elapsedMs = timer.Elapsed.TotalMilliseconds;
					long bytes = GC.GetAllocatedBytesForCurrentThread() - allocated;
					Console.WriteLine($"RECOGNITION BENCHMARK: {system}, {elapsedMs / iterations:F4} ms/refresh, {bytes / iterations} bytes/refresh; actual paused viewer snapshot, excludes desktop presentation.");
				}
				Type loader = typeof(SpriteViewerViewModel).Assembly.GetType("Mesen.Debugger.Utilities.MetaSpriteRuntimeLoader")!;
				MethodInfo preload = loader.GetMethod("LoadForGame", BindingFlags.Public | BindingFlags.Static)!;
				int loads = 0;
				bool applied = false;
				uint before = uint.MaxValue, after = uint.MaxValue;
				ulong beforeClock = ulong.MaxValue, afterClock = ulong.MaxValue;
				double preloadMs = 0;
				using var listener = new NotificationListener();
				listener.OnNotification += e => {
					if(e.NotificationType != ConsoleNotificationType.GameLoaded) { return; }
					loads++;
					TimingInfo beforeTiming = EmuApi.GetTimingInfo(cpu);
					before = beforeTiming.FrameCount; beforeClock = beforeTiming.MasterClock;
					var timer = Stopwatch.StartNew();
					applied = (bool)preload.Invoke(null, new object[] { hash, cpu })!;
					preloadMs = timer.Elapsed.TotalMilliseconds;
					TimingInfo afterTiming = EmuApi.GetTimingInfo(cpu);
					after = afterTiming.FrameCount; afterClock = afterTiming.MasterClock;
				};
				if(!EmuApi.LoadRom(romPath) || loads != 1 || !applied || before != after || beforeClock != afterClock) {
					throw new Exception($"Pre-emulation PNG activation failed: loads={loads}, applied={applied}, frames={before}/{after}, clocks={beforeClock}/{afterClock}.");
				}
				Console.WriteLine($"PASS: {system} saved PNG decode/native activation completed during GameLoaded with frame/clock unchanged ({before}/{beforeClock}); {preloadMs:F3} ms.");
				Console.WriteLine($"CLEANUP: {system} activation check returned; finishing viewer scope.");
				if(startupTiming) {
					const int viewerId = 19071;
					listener.OnNotification += e => {
						if(e.NotificationType == ConsoleNotificationType.ViewerRefresh && e.Parameter.ToInt32() == viewerId) { viewer.RefreshData(); }
					};
					DebugApi.SetViewerUpdateTiming(viewerId, 8, 12, cpu);
					EmuApi.Resume();
					var timer = Stopwatch.StartNew();
					uint previousFrame = EmuApi.GetTimingInfo(cpu).FrameCount;
					double previousSeconds = 0;
					string report = "seconds,frames_per_second\n";
					while(timer.Elapsed.TotalSeconds < 60) {
						Dispatcher.UIThread.RunJobs();
						double seconds = timer.Elapsed.TotalSeconds;
						if(seconds - previousSeconds >= 5) {
							uint frame = EmuApi.GetTimingInfo(cpu).FrameCount;
							double fps = (frame - previousFrame) / (seconds - previousSeconds);
							Console.WriteLine($"STARTUP TIMING: {system}, {seconds:F1}s, {fps:F2} emulated FPS; actual viewer model refresh, excludes desktop presentation.");
							report += FormattableString.Invariant($"{seconds:F3},{fps:F3}\n");
							previousFrame = frame; previousSeconds = seconds;
						}
						System.Threading.Thread.Sleep(1);
					}
					DebugApi.RemoveViewerId(viewerId, cpu);
					EmuApi.Pause();
					File.WriteAllText(Path.Combine(home, "startup-timing.csv"), report);
					Console.WriteLine("Startup timing saved: " + Path.Combine(home, "startup-timing.csv"));
				}
			} finally {
				Console.WriteLine($"CLEANUP: {system} releasing debugger.");
				DebugApi.ReleaseDebugger();
				Console.WriteLine($"CLEANUP: {system} debugger released.");
			}
		} finally {
			Console.WriteLine($"CLEANUP: {system} stopping emulator.");
			EmuApi.Stop();
			Console.WriteLine($"CLEANUP: {system} releasing emulator.");
			EmuApi.Release();
			Console.WriteLine($"PASS: {system} viewer verification shutdown completed.");
		}
	}

	private static void CheckCreationSave()
	{
		MethodInfo save = typeof(SpriteViewerViewModel).GetMethod("SaveNewMetaGroup", BindingFlags.NonPublic | BindingFlags.Static)
			?? throw new Exception("Create-group save helper missing.");
		string directory = Path.Combine(AppContext.BaseDirectory, "artifacts", "creation-" + Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(directory);
		string packPath = Path.Combine(directory, "groups.json");
		var pack = new MetaSpritePack { RomHash = "creation-fixture", System = "Snes" };
		MetaSpriteGroup NewGroup() => new() {
			Id = pack.NextGroupId, Width = 2, Height = 1, OriginX = 1, OriginY = 1,
			Members = new() { new() { Width = 2, Height = 1, Pixels = new uint[] { 0, 0xFF123456 } } }
		};
		var group = NewGroup();
		save.Invoke(null, new object[] { pack, group, packPath, (Action)(() => pack.Save(packPath)) });
		var restored = MetaSpritePack.Load(packPath, pack.RomHash, pack.System);
		if(restored.Groups.Count != 1 || restored.NextGroupId != 2 || !restored.Groups[0].Enabled
			|| restored.Groups[0].ReplacementPath != "meta-001.png" || !File.Exists(Path.Combine(directory, group.ReplacementPath))) {
			throw new Exception("Creation did not automatically save and enable the group PNG.");
		}
		string existing = Path.Combine(directory, "meta-002.png");
		File.WriteAllText(existing, "existing user artwork");
		group = NewGroup();
		string originalPack = File.ReadAllText(packPath);
		bool rejected = false;
		try { save.Invoke(null, new object[] { pack, group, packPath, (Action)(() => throw new IOException("Simulated definition save failure.")) }); }
		catch(TargetInvocationException ex) when(ex.InnerException is IOException) { rejected = true; }
		if(!rejected || pack.Groups.Count != 1 || pack.NextGroupId != 2 || File.ReadAllText(packPath) != originalPack
			|| File.ReadAllText(existing) != "existing user artwork" || File.Exists(Path.Combine(directory, group.ReplacementPath))) {
			throw new Exception("Failed creation did not roll back its PNG and group while retaining existing files.");
		}
		group = NewGroup();
		save.Invoke(null, new object[] { pack, group, packPath, (Action)(() => pack.Save(packPath)) });
		if(!group.Enabled || group.ReplacementPath == "meta-002.png" || !File.Exists(Path.Combine(directory, group.ReplacementPath))
			|| File.ReadAllText(existing) != "existing user artwork" || pack.NextGroupId != 3) {
			throw new Exception("Creation retry overwrote artwork or failed to reuse the rolled-back ID.");
		}
		Console.WriteLine("PASS: actual Create save helper exports/assigns/enables PNG, preserves existing artwork, rolls back failed saves and retries with the same group ID.");
	}

	private static void CheckPreviewPixels(DynamicBitmap bitmap, uint[] expected, int count)
	{
		using var frame = bitmap.Lock(true);
		int[] actual = new int[count];
		Marshal.Copy(frame.FrameBuffer.Address, actual, 0, count);
		for(int i = 0; i < count; i++) {
			if(unchecked((uint)actual[i]) != expected[i]) { throw new Exception($"Preview bitmap pixel {i} did not update."); }
		}
	}

	private static void BenchmarkPreviews()
	{
		const int count = 128, iterations = 240;
		foreach(int size in new[] { 8, 64 }) {
			var models = new SpritePreviewModel[count];
			var sprites = new DebugSpriteInfo[count];
			var layout = new DebugSpritePreviewInfo();
			var pixels = new uint[count * 128 * 128];
			for(int i = 0; i < count; i++) {
				models[i] = new();
				sprites[i] = new() { SpriteIndex = (short)i, Width = (ushort)size, Height = (ushort)size };
				Array.Fill(pixels, 0xFF204080u, i * 128 * 128, size * size);
				models[i].Init(ref sprites[i], pixels, layout);
			}
			foreach(bool changing in new[] { false, true }) {
				for(int iteration = 0; iteration < 30; iteration++) {
					for(int i = 0; i < count; i++) { models[i].Init(ref sprites[i], pixels, layout); }
				}
				var timer = Stopwatch.StartNew();
				for(int iteration = 0; iteration < iterations; iteration++) {
					for(int i = 0; i < count; i++) {
						if(changing) { pixels[i * 128 * 128 + size * size - 1] ^= 0x000000FF; }
						models[i].Init(ref sprites[i], pixels, layout);
					}
				}
				timer.Stop();
				Console.WriteLine($"Preview model: {count} sprites {size}x{size}, {(changing ? "changing" : "unchanged")}: {timer.Elapsed.TotalMilliseconds / iterations:F4} ms/refresh.");
			}
			foreach(var model in models) { model.ReleasePreview(); }
		}
	}

	private static void CheckGroupPng()
	{
		var group = new MetaSpriteGroup { Width = 4, Height = 3 };
		group.Members.Add(new() { X = 0, Y = 1, Width = 2, Height = 1, Pixels = new uint[] { 0, 0xFFFF0000 } });
		group.Members.Add(new() { X = 3, Y = 2, Width = 1, Height = 1, Pixels = new uint[] { 0xFF0000FF } });
		group.Members.Add(new() { X = 2, Y = 0, Width = 1, Height = 1, Pixels = new uint[] { 0x80FFFF00 } });
		MethodInfo builder = typeof(SpriteViewerViewModel).GetMethod("BuildMetaBitmap", BindingFlags.NonPublic | BindingFlags.Static)
			?? throw new Exception("Viewer group bitmap builder missing.");
		using var bitmap = (WriteableBitmap)(builder.Invoke(null, new object[] { group }) ?? throw new Exception("Group bitmap missing."));
		string directory = Path.Combine(AppContext.BaseDirectory, "artifacts");
		Directory.CreateDirectory(directory);
		string path = Path.Combine(directory, "transparent-group.png");
		bitmap.Save(path, PngBitmapEncoderOptions.Default);
		using var loaded = new Bitmap(path);
		if(loaded.PixelSize != new PixelSize(4, 3)) { throw new Exception("PNG canvas changed."); }
		using var decoded = new WriteableBitmap(loaded.PixelSize, new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);
		using var frame = decoded.Lock();
		loaded.CopyPixels(frame);
		int[] row = new int[4];
		Type runtimeLoader = typeof(SpriteViewerViewModel).Assembly.GetType("Mesen.Debugger.Utilities.MetaSpriteRuntimeLoader")
			?? throw new Exception("Gameplay PNG loader missing.");
		MethodInfo loadPng = runtimeLoader.GetMethod("LoadPng", BindingFlags.NonPublic | BindingFlags.Static)
			?? throw new Exception("Gameplay PNG decoder missing.");
		MethodInfo validateBackgrounds = runtimeLoader.GetMethod("ValidateBackgroundSupport", BindingFlags.Public | BindingFlags.Static)!;
		foreach(CpuType cpu in new[] { CpuType.Nes, CpuType.Gameboy, CpuType.Gba, CpuType.Pce, CpuType.Sms, CpuType.Snes, CpuType.Ws }) {
			var draft = new MetaBackgroundLayer { Enabled = true, NativeLayer = 4 };
			var draftPack = new MetaSpritePack { System = cpu.ToString(), BackgroundLayers = new() { draft } };
			validateBackgrounds.Invoke(null, new object[] { draftPack });
			draft.MemoryConditions.Add(new() { Address = 0, Value = 0 });
			bool rejected = false;
			try { validateBackgrounds.Invoke(null, new object[] { draftPack }); }
			catch(TargetInvocationException ex) when(ex.InnerException is InvalidDataException) { rejected = true; }
			if(!rejected) { throw new Exception("Active unsupported background layer accepted for " + cpu); }
			draft.Enabled = false; validateBackgrounds.Invoke(null, new object[] { draftPack });
			draft.Enabled = true; draft.NativeLayer = 0; validateBackgrounds.Invoke(null, new object[] { draftPack });
		}
		Console.WriteLine("PASS: background support validation ignores inactive drafts but rejects active unsupported layers across all target systems.");
		var gameplay = ((int Width, int Height, uint[] Pixels))(loadPng.Invoke(null, new object[] { path, 1 })
			?? throw new Exception("Gameplay PNG pixels missing."));
		if(gameplay.Width != 4 || gameplay.Height != 3 || gameplay.Pixels.Length != 12) { throw new Exception("Gameplay PNG dimensions changed."); }
		for(int y = 0; y < 3; y++) {
			Marshal.Copy(frame.Address + y * frame.RowBytes, row, 0, 4);
			for(int x = 0; x < 4; x++) {
				uint expected = y == 0 && x == 2 ? 0x80FFFF00u : y == 1 && x == 1 ? 0xFFFF0000u : y == 2 && x == 3 ? 0xFF0000FFu : 0;
				uint display = unchecked((uint)row[x]);
				if((display >> 24) != (expected >> 24)) { throw new Exception($"PNG alpha mismatch at {x},{y}."); }
				for(int shift = 0; shift <= 16; shift += 8) {
					int difference = Math.Abs((int)((display >> shift) & 255) - (int)((expected >> shift) & 255));
					if(difference > ((expected >> 24) == 128 ? 1 : 0)) { throw new Exception($"PNG display color mismatch at {x},{y}."); }
				}
				if(gameplay.Pixels[y * 4 + x] != expected) { throw new Exception($"Gameplay PNG alpha/color mismatch at {x},{y}."); }
			}
		}
		Console.WriteLine("PASS: actual viewer export and shared gameplay PNG loader preserve transparent canvas, partial alpha and relative member placement: " + path);
		string corrupt = Path.Combine(directory, "corrupt.png");
		File.WriteAllBytes(corrupt, new byte[] { 137, 80, 78, 71 });
		var partialCanvas = ((int Width, int Height, uint[] Pixels))loadPng.Invoke(null, new object[] { path, 2 })!;
		if(partialCanvas.Width != 4 || partialCanvas.Height != 3 || !partialCanvas.Pixels.SequenceEqual(gameplay.Pixels)) { throw new Exception("Partial native canvas dropped PNG pixels or changed scale."); }
		foreach(var invalid in new[] { (Path: corrupt, Scale: 1), (Path: path, Scale: 0), (Path: path, Scale: 11) }) {
			bool rejected = false;
			try { loadPng.Invoke(null, new object[] { invalid.Path, invalid.Scale }); }
			catch(TargetInvocationException ex) when(ex.InnerException is InvalidDataException) { rejected = true; }
			if(!rejected) { throw new Exception("Invalid PNG or canvas scale was accepted."); }
		}
		Console.WriteLine("PASS: gameplay PNG loader preserves partial native canvases and rejects corrupt data/out-of-range scale.");
		uint[] rampPixels = new uint[256];
		for(uint alpha = 1; alpha < 256; alpha++) { rampPixels[alpha] = (alpha << 24) | 0x00E741A5; }
		var ramp = new MetaSpriteGroup { Width = 256, Height = 1 };
		ramp.Members.Add(new() { Width = 256, Height = 1, Pixels = rampPixels });
		using var rampBitmap = (WriteableBitmap)builder.Invoke(null, new object[] { ramp })!;
		string rampPath = Path.Combine(directory, "alpha-ramp.png");
		rampBitmap.Save(rampPath, PngBitmapEncoderOptions.Default);
		var rampImage = ((int Width, int Height, uint[] Pixels))loadPng.Invoke(null, new object[] { rampPath, 1 })!;
		for(int alpha = 0; alpha < 256; alpha++) {
			if(rampImage.Pixels[alpha] != rampPixels[alpha]) { throw new Exception("Gameplay PNG color/alpha changed at alpha " + alpha); }
		}
		Console.WriteLine("PASS: exact gameplay RGB/alpha round trip across all 256 alpha values.");
	}

	private static void CheckReleased(DynamicBitmap bitmap)
	{
		try { using var bitmapLock = bitmap.Lock(); }
		catch(ObjectDisposedException) { return; }
		throw new Exception("Obsolete bitmap remained usable after release.");
	}
}

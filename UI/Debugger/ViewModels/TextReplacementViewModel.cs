using Avalonia.Media.Imaging;
using Avalonia;
using System.ComponentModel;
using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Debugger.Utilities;
using SkiaSharp;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace Mesen.Debugger.ViewModels;

public partial class TextReplacementViewModel : ObservableObject, IDisposable
{
	private readonly string _packPath, _hash, _system;
	private readonly Func<bool> _sameGame;
	private readonly Func<MetaSpritePack,bool>? _apply;
	public bool CanApply => _apply!=null;
	private readonly Dictionary<MetaTextProfile,string> _mappingDrafts=new(), _conditionDrafts=new();
	private readonly int _tileIndex, _tileAddress, _palette;
	public sealed record SourceTile(uint Index,uint Address,int Palette);
	private IReadOnlyDictionary<(int X,int Y),SourceTile>? _sourceTiles;
	private int _capturedX,_capturedY;
	public void SetSourceTiles(IReadOnlyDictionary<(int X,int Y),SourceTile> tiles,int x,int y) { _sourceTiles=tiles; _capturedX=x; _capturedY=y; }
	public void UseSelectionCellSize() {
		if(SelectedProfile is not {} p) { return; }
		if(p.Width is <8 or >128 || p.Height is <8 or >128 || p.Width%8!=0 || p.Height%8!=0) { Status="Select one character cell, in multiples of 8 pixels up to 128×128."; return; }
		p.CellWidth=p.Width; p.CellHeight=p.Height; p.CellOriginX=p.X%p.CellWidth; p.CellOriginY=p.Y%p.CellHeight;
		OnPropertyChanged(nameof(SelectedProfile)); Status="Source cell size and grid origin taken from the selection. Select the full dialogue region afterwards, or disable the region limit.";
	}
	private bool _loading;
	private string _savedDefinitions;
	public string DirectoryPath => Path.GetDirectoryName(_packPath)!;
	public IReadOnlyList<string> LayerNames { get; }
	public IReadOnlyList<string> MatchKinds { get; }=new[] {"Tile index","Graphics address"};
	public ObservableCollection<MetaTextProfile> Profiles { get; }=new();
	public bool HasProfile => SelectedProfile!=null;
	public Bitmap? SourcePreview { get; private set; }
	public string SourcePreviewInfo => SourcePreview==null ? "Source snapshot unavailable. Refresh Tilemap Viewer and reopen this editor to capture it. Numeric regions remain available." : "Source snapshot: the hardware tilemap selected when this editor opened.";
	public bool IsCurrentGame => _sameGame();
	public int SourceWidth => SourcePreview?.PixelSize.Width ?? 256;
	public int SourceHeight => SourcePreview?.PixelSize.Height ?? 240;
	private Rect _viewerSelection;
	private MetaTextProfile? _observedProfile;
	private bool _settingRegion;
	public Rect SourceRegion {
		get => SelectedProfile is {} p ? new Rect(p.X,p.Y,p.Width,p.Height) : default;
		set { if(SelectedProfile is not {} p) { return; } _settingRegion=true;
			p.UseRegion=true; p.X=(int)value.X; p.Y=(int)value.Y; p.Width=Math.Max(1,(int)value.Width); p.Height=Math.Max(1,(int)value.Height);
			_settingRegion=false; OnPropertyChanged(); }
	}
	public Rect DisplayRegion {
		get => SelectedProfile is {} p ? new Rect(p.DisplayX,p.DisplayY,p.DisplayWidth,p.DisplayHeight) : default;
		set { if(SelectedProfile is not {} p) { return; } _settingRegion=true;
			p.UseDisplayRegion=true; p.DisplayX=(int)value.X; p.DisplayY=(int)value.Y; p.DisplayWidth=Math.Max(1,(int)value.Width); p.DisplayHeight=Math.Max(1,(int)value.Height);
			_settingRegion=false; OnPropertyChanged(); }
	}
	public void SetSourcePreview(Bitmap bitmap,Rect selection) {
		SourcePreview?.Dispose(); SourcePreview=bitmap; _viewerSelection=selection;
		OnPropertyChanged(nameof(SourcePreview)); OnPropertyChanged(nameof(SourceWidth)); OnPropertyChanged(nameof(SourceHeight)); OnPropertyChanged(nameof(SourcePreviewInfo));
	}
	public void UseViewerSelection() {
		if(_viewerSelection.Width<=0 || _viewerSelection.Height<=0) { Status="Select a rectangle in Tilemap Viewer before opening this editor, or draw one in the source preview."; return; }
		SourceRegion=SnapSource(_viewerSelection,SourceWidth,SourceHeight); Status="Viewer selection assigned as the source capture region.";
	}
	public static Rect SnapSource(Rect selection,int width,int height) {
		double x=Math.Clamp(Math.Floor(selection.X/8)*8,0,Math.Max(0,width-8)), y=Math.Clamp(Math.Floor(selection.Y/8)*8,0,Math.Max(0,height-8));
		return new Rect(x,y,Math.Max(1,Math.Min(width-x,Math.Ceiling(selection.Right/8)*8-x)),Math.Max(1,Math.Min(height-y,Math.Ceiling(selection.Bottom/8)*8-y)));
	}
	public void MatchSourceRegion() { DisplayRegion=SourceRegion; RefreshPreview(); }
	private void RegionPropertyChanged(object? sender,PropertyChangedEventArgs e) {
		if(_settingRegion) { return; }
		if(e.PropertyName is nameof(MetaTextProfile.X) or nameof(MetaTextProfile.Y) or nameof(MetaTextProfile.Width) or nameof(MetaTextProfile.Height)) { OnPropertyChanged(nameof(SourceRegion)); }
		if(e.PropertyName is nameof(MetaTextProfile.DisplayX) or nameof(MetaTextProfile.DisplayY) or nameof(MetaTextProfile.DisplayWidth) or nameof(MetaTextProfile.DisplayHeight)) { OnPropertyChanged(nameof(DisplayRegion)); }
	}
	public string CapturedTileInfo => _tileIndex<0 && _tileAddress<0 ? "Select a tile in the tilemap viewer before opening this editor to capture its identity."
		: $"Captured tile: index {(_tileIndex<0 ? "unavailable" : "$"+_tileIndex.ToString("X"))}, graphics {(_tileAddress<0 ? "unavailable" : "$"+_tileAddress.ToString("X"))}, palette {(_palette<0 ? "unavailable" : "$"+_palette.ToString("X"))}.";
	[ObservableProperty] public partial MetaTextProfile? SelectedProfile { get; set; }
	[ObservableProperty] public partial string MappingText { get; set; }="";
	[ObservableProperty] public partial string ConditionsText { get; set; }="";
	[ObservableProperty] public partial string CaptureCharacter { get; set; }="A";
	[ObservableProperty] public partial bool CapturePalette { get; set; }=true;
	[ObservableProperty] public partial string PreviewText { get; set; }="ABC abc 0123";
	[ObservableProperty] public partial Bitmap? Preview { get; set; }
	[ObservableProperty] public partial string Status { get; set; }="Cached font overlays support NES, GB/GBC, GBA, PCE, SMS and SNES/WonderSwan tile backgrounds.";
	public TextReplacementViewModel(string packPath,string hash,string system,IReadOnlyList<string> layerNames,int layer,
		int tileIndex,int tileAddress,int palette,Func<bool> sameGame,Func<MetaSpritePack,bool>? apply=null)
	{
		_packPath=packPath; _hash=hash; _system=system; LayerNames=layerNames; _sameGame=sameGame;
		_apply=apply;
		_tileIndex=tileIndex; _tileAddress=tileAddress; _palette=palette;
		var pack=MetaSpritePack.Load(packPath,hash,system);
		_savedDefinitions=pack.GetTextDefinitionsSnapshot();
		foreach(var profile in pack.TextProfiles) { Profiles.Add(profile); }
		if(Profiles.Count==0) { AddProfile(Math.Clamp(layer,0,layerNames.Count-1)); }
		else { SelectedProfile=Profiles[0]; }
	}
	public void AddProfile(int layer=0)
	{
		if(Profiles.Count>=64) { Status="A pack supports up to 64 text profiles."; return; }
		int id=1; while(Profiles.Any(p => p.Id==id)) { id++; }
		var profile=new MetaTextProfile {Id=id,Name=$"Text {id}",NativeLayer=layer,Proportional=true};
		Profiles.Add(profile); SelectedProfile=profile;
	}
	public void RemoveProfile()
	{
		if(SelectedProfile is not MetaTextProfile profile) { return; }
		int index=Profiles.IndexOf(profile); Profiles.Remove(profile); _mappingDrafts.Remove(profile); _conditionDrafts.Remove(profile);
		SelectedProfile=Profiles.Count==0 ? null : Profiles[Math.Min(index,Profiles.Count-1)];
	}
	partial void OnSelectedProfileChanged(MetaTextProfile? value)
	{
		if(_observedProfile!=null) { _observedProfile.PropertyChanged-=RegionPropertyChanged; }
		_observedProfile=value; if(value!=null) { value.PropertyChanged+=RegionPropertyChanged; }
		_loading=true;
		try {
			MappingText=value==null ? "" : _mappingDrafts.GetValueOrDefault(value) ?? MetaTextMappingText.Format(value.Mappings);
			ConditionsText=value==null ? "" : _conditionDrafts.GetValueOrDefault(value) ?? MetaMemoryConditionText.Format(value.MemoryConditions);
		} finally { _loading=false; }
		OnPropertyChanged(nameof(HasProfile)); ClearPreview();
		OnPropertyChanged(nameof(SourceRegion)); OnPropertyChanged(nameof(DisplayRegion));
	}
	partial void OnMappingTextChanged(string value) { if(!_loading && SelectedProfile!=null) { _mappingDrafts[SelectedProfile]=value; } }
	partial void OnConditionsTextChanged(string value) { if(!_loading && SelectedProfile!=null) { _conditionDrafts[SelectedProfile]=value; } }
	public void AddCapturedTile() => AddSourceCell(false);
	public void AddSelectedSourceCell() => AddSourceCell(true);
	private void AddSourceCell(bool selected)
	{
		if(SelectedProfile is not MetaTextProfile profile) { return; }
		try {
			int tile=profile.MatchKind==MetaTextMatchKind.TileIndex ? _tileIndex : _tileAddress;
			if(tile<0 && !selected && profile.CellWidth==8 && profile.CellHeight==8) { throw new InvalidDataException("The captured tile has no identity for this match type."); }
			if(!MetaTextProfile.IsCharacter(CaptureCharacter)) { throw new InvalidDataException("Enter one printable character to map the captured tile."); }
			int palette=CapturePalette && _palette>=0 ? _palette : -1;
			uint[] pattern=new[] {(uint)tile};
			if(selected || _sourceTiles!=null || profile.CellWidth!=8 || profile.CellHeight!=8) {
				if(profile.CellWidth%8!=0 || profile.CellHeight%8!=0) { throw new InvalidDataException("Source cell dimensions must be multiples of 8."); }
				int x=selected ? profile.X : _capturedX,y=selected ? profile.Y : _capturedY;
				var parts=new List<uint>(); int? exactPalette=null;
				for(int dy=0;dy<profile.CellHeight;dy+=8) { for(int dx=0;dx<profile.CellWidth;dx+=8) {
					if(_sourceTiles==null || !_sourceTiles.TryGetValue((x+dx,y+dy),out var source)) { throw new InvalidDataException("The full source cell is unavailable in this tilemap snapshot. Refresh the viewer and reopen the editor."); }
					if(CapturePalette && exactPalette.HasValue && exactPalette!=source.Palette) { throw new InvalidDataException("This source cell uses different palettes. Disable palette matching to capture it as a wildcard."); }
					exactPalette=source.Palette; parts.Add(profile.MatchKind==MetaTextMatchKind.TileIndex ? source.Index : source.Address);
				} }
				pattern=parts.ToArray(); tile=(int)pattern[0]; palette=CapturePalette ? exactPalette!.Value : -1;
				profile.CellOriginX=x%profile.CellWidth; profile.CellOriginY=y%profile.CellHeight; OnPropertyChanged(nameof(SelectedProfile));
			}
			var mappings=MetaTextMappingText.Parse(MappingText);
			if(mappings.Any(m => m.GetTiles().SequenceEqual(pattern) && m.Palette==palette)) { throw new InvalidDataException("That tile grid/palette is already mapped. Edit its existing line."); }
			mappings.Add(new() {Tile=pattern[0],TilePattern=pattern.Length>1 ? pattern : Array.Empty<uint>(),Palette=palette,Character=CaptureCharacter});
			MappingText=MetaTextMappingText.Format(mappings); Status="Complete source cell added as one character.";
		} catch(Exception ex) { Status=ex.Message; }
	}
	private void UpdateDraft(MetaTextProfile profile)
	{
		profile.Mappings=MetaTextMappingText.Parse(_mappingDrafts.GetValueOrDefault(profile) ?? MetaTextMappingText.Format(profile.Mappings));
		profile.MemoryConditions=MetaMemoryConditionText.Parse(_conditionDrafts.GetValueOrDefault(profile) ?? MetaMemoryConditionText.Format(profile.MemoryConditions));
		profile.Validate();
		if(profile.NativeLayer>=LayerNames.Count) { throw new InvalidDataException($"{profile.Name}: select a hardware layer available on this system."); }
	}
	private string FontFile(MetaTextProfile profile)
	{
		if(!MetaTextProfile.IsFontPath(profile.FontPath)) { throw new InvalidDataException("Select a .ttf or .otf font first."); }
		string path=Path.Combine(DirectoryPath,profile.FontPath);
		if(!File.Exists(path) || new FileInfo(path).Length>32*1024*1024) { throw new InvalidDataException("Font file is missing or exceeds 32 MiB."); }
		return path;
	}
	public bool ImportFont(string source)
	{
		if(SelectedProfile is not MetaTextProfile profile) { return false; }
		try {
			if(!_sameGame()) { throw new InvalidOperationException("The loaded game changed. Reopen the text editor."); }
			source=Path.GetFullPath(source);
			string filename=Path.GetFileName(source);
			if(!MetaTextProfile.IsFontPath(filename) || new FileInfo(source).Length>32*1024*1024) { throw new InvalidDataException("Choose a .ttf or .otf font no larger than 32 MiB."); }
			using var typeface=SKTypeface.FromFile(source) ?? throw new InvalidDataException("The selected file is not a readable font.");
			Directory.CreateDirectory(DirectoryPath);
			string destination=Path.Combine(DirectoryPath,filename);
			if(!string.Equals(source,Path.GetFullPath(destination),OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)) {
				for(int n=2; File.Exists(destination); n++) { destination=Path.Combine(DirectoryPath,$"{Path.GetFileNameWithoutExtension(filename)}-{n:D3}{Path.GetExtension(filename)}"); }
				File.Copy(source,destination,false);
			}
			profile.FontPath=Path.GetFileName(destination); OnPropertyChanged(nameof(SelectedProfile));
			Status="Font imported. Refresh preview to inspect the tile-aligned text."; return true;
		} catch(Exception ex) { Status="Could not import font: "+ex.Message; return false; }
	}
	public void RefreshPreview()
	{
		if(SelectedProfile is not MetaTextProfile profile) { return; }
		try {
			UpdateDraft(profile);
			if(profile.Proportional || profile.UseDisplayRegion || profile.OutlineEnabled || profile.ShadowEnabled || profile.FontSize>profile.CellHeight || profile.CellWidth!=8 || profile.CellHeight!=8) {
				byte[] encodedRun=MetaTextPreview.Render(profile,DirectoryPath,PreviewText); using var runStream=new MemoryStream(encodedRun); var runBitmap=new Bitmap(runStream);
				ClearPreview(); Preview=runBitmap; Status="Cached text-run preview: font advances/kerning, full glyph bounds and outline/shadow colors. Palette preview uses white. Display-region edges intentionally clip text."; return;
			}
			using var typeface=SKTypeface.FromFile(FontFile(profile)) ?? throw new InvalidDataException("Could not load font.");
			using var font=new SKFont(typeface,profile.FontSize*profile.RasterScale) {Edging=SKFontEdging.Antialias};
			var characters=PreviewText.EnumerateRunes().Take(128).ToArray();
			int scale=profile.RasterScale, cellWidth=profile.CellWidth*scale, cellHeight=profile.CellHeight*scale;
			int columns=Math.Min(16,Math.Max(1,characters.Length)), rows=Math.Max(1,(characters.Length+columns-1)/columns);
			int width=columns*cellWidth, height=rows*cellHeight;
			if((long)width*height>8*1024*1024) { throw new InvalidDataException("Preview exceeds 8 million pixels; reduce cell size or raster scale."); }
			using var bitmap=new SKBitmap(width,height,SKColorType.Bgra8888,SKAlphaType.Premul);
			using var canvas=new SKCanvas(bitmap); canvas.Clear(SKColors.Transparent);
			uint rgba=uint.Parse(profile.Color,NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture);
			using var paint=new SKPaint {Color=profile.UsePaletteColor ? SKColors.White : new SKColor((byte)(rgba>>24),(byte)(rgba>>16),(byte)(rgba>>8),(byte)rgba)};
			uint fill=uint.Parse(profile.FillColor,NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture);
			using var fillPaint=new SKPaint {Color=new SKColor((byte)(fill>>24),(byte)(fill>>16),(byte)(fill>>8),(byte)fill)};
			int missing=0;
			for(int i=0;i<characters.Length;i++) {
				string text=characters[i].ToString();
				if(Rune.IsControl(characters[i])) { continue; }
				using var builder=new SKTextBlobBuilder();
				var run=builder.AllocateRun(font,font.CountGlyphs(text),0,0); font.GetGlyphs(text,run.Glyphs);
				if(run.Glyphs.Contains((ushort)0)) { missing++; }
				using var blob=builder.Build();
				float x=(i%columns)*cellWidth, y=(i/columns)*cellHeight;
				canvas.Save(); canvas.ClipRect(new SKRect(x,y,x+cellWidth,y+cellHeight));
				if(profile.FillEnabled) { canvas.DrawRect(x,y,cellWidth,cellHeight,fillPaint); }
				if(blob!=null) { canvas.DrawText(blob,x+profile.OffsetX*scale,y+cellHeight-font.Metrics.Descent+profile.OffsetY*scale,paint); }
				canvas.Restore();
			}
			using var image=SKImage.FromBitmap(bitmap); using var encoded=image.Encode(SKEncodedImageFormat.Png,100); using var stream=encoded.AsStream();
			var preview=new Bitmap(stream); ClearPreview(); Preview=preview;
			Status=$"Tile-aligned preview: {profile.Mappings.Count} mappings; {width}×{height} pixels. "+(profile.UsePaletteColor ? "Palette preview uses white. " : "")
				+(missing>0 ? $"Font lacks {missing} preview glyph(s). " : "")+"Use Save and apply for gameplay. Source cells may span multiple tiles.";
		} catch(Exception ex) { ClearPreview(); Status="Could not preview text: "+ex.Message; }
	}
	public bool Save()
	{
		try {
			if(!_sameGame()) { throw new InvalidOperationException("The loaded game changed. Reopen the text editor."); }
			foreach(var profile in Profiles) {
				UpdateDraft(profile);
				if(profile.FontPath.Length>0) { using var font=SKTypeface.FromFile(FontFile(profile)) ?? throw new InvalidDataException("Could not load font for "+profile.Name); }
			}
			var latest=MetaSpritePack.Load(_packPath,_hash,_system);
			if(latest.GetTextDefinitionsSnapshot()!=_savedDefinitions) { throw new InvalidOperationException("Text profiles changed in another editor. Reopen this editor before saving."); }
			latest.TextProfiles=Profiles.ToList(); latest.Save(_packPath);
			_savedDefinitions=latest.GetTextDefinitionsSnapshot();
			Status="Text profiles saved."; return true;
		} catch(Exception ex) { Status="Could not save text profiles: "+ex.Message; return false; }
	}
	public bool SaveAndApply()
	{
		if(!CanApply) { Status="Gameplay activation is unavailable in this editor context."; return false; }
		if(!Save()) { return false; }
		try {
			if(!_sameGame()) { throw new InvalidOperationException("The loaded game changed."); }
			var pack=MetaSpritePack.Load(_packPath,_hash,_system);
			if(!_apply!(pack)) { throw new InvalidOperationException("This console or filter layout is unsupported."); }
			Status="Text profiles saved and applied as cached overlays. Gameplay uses supported native tile modes and compatible video filters."; return true;
		} catch(Exception ex) { Status="Profiles saved; could not apply: "+ex.Message; return false; }
	}
	private void ClearPreview() { var old=Preview; Preview=null; old?.Dispose(); }
	public void Dispose() { if(_observedProfile!=null) { _observedProfile.PropertyChanged-=RegionPropertyChanged; } SourcePreview?.Dispose(); SourcePreview=null; ClearPreview(); }
}

using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using System;

namespace Mesen.Debugger.Controls;

// Coordinates always use native source/gameplay pixels, independent of preview zoom.
public sealed class TextRegionSelector : Control
{
	public static readonly StyledProperty<Bitmap?> ImageProperty = AvaloniaProperty.Register<TextRegionSelector,Bitmap?>(nameof(Image));
	public static readonly StyledProperty<Bitmap?> OverlayProperty = AvaloniaProperty.Register<TextRegionSelector,Bitmap?>(nameof(Overlay));
	public static readonly StyledProperty<Rect> RegionProperty = AvaloniaProperty.Register<TextRegionSelector,Rect>(nameof(Region), defaultBindingMode: Avalonia.Data.BindingMode.TwoWay);
	public static readonly StyledProperty<int> CanvasWidthProperty = AvaloniaProperty.Register<TextRegionSelector,int>(nameof(CanvasWidth),256);
	public static readonly StyledProperty<int> CanvasHeightProperty = AvaloniaProperty.Register<TextRegionSelector,int>(nameof(CanvasHeight),240);
	public static readonly StyledProperty<int> SnapProperty = AvaloniaProperty.Register<TextRegionSelector,int>(nameof(Snap),1);
	public static readonly StyledProperty<int> SnapXProperty = AvaloniaProperty.Register<TextRegionSelector,int>(nameof(SnapX),0);
    public static readonly StyledProperty<int> SnapYProperty = AvaloniaProperty.Register<TextRegionSelector,int>(nameof(SnapY),0);
    public static readonly StyledProperty<bool> DrawModeProperty = AvaloniaProperty.Register<TextRegionSelector,bool>(nameof(DrawMode),true,defaultBindingMode: Avalonia.Data.BindingMode.TwoWay);
	public static readonly StyledProperty<bool> ShowOverlayProperty = AvaloniaProperty.Register<TextRegionSelector,bool>(nameof(ShowOverlay),true);
	public Bitmap? Image { get => GetValue(ImageProperty); set => SetValue(ImageProperty,value); }
	public Bitmap? Overlay { get => GetValue(OverlayProperty); set => SetValue(OverlayProperty,value); }
	public Rect Region { get => GetValue(RegionProperty); set => SetValue(RegionProperty,value); }
	public int CanvasWidth { get => GetValue(CanvasWidthProperty); set => SetValue(CanvasWidthProperty,value); }
	public int CanvasHeight { get => GetValue(CanvasHeightProperty); set => SetValue(CanvasHeightProperty,value); }
	public int Snap { get => GetValue(SnapProperty); set => SetValue(SnapProperty,value); }
	public int SnapX { get => GetValue(SnapXProperty); set => SetValue(SnapXProperty,value); }
    public int SnapY { get => GetValue(SnapYProperty); set => SetValue(SnapYProperty,value); }
    private int StepX => Math.Max(1,SnapX>0 ? SnapX : Snap);
    private int StepY => Math.Max(1,SnapY>0 ? SnapY : Snap);
    public bool DrawMode { get => GetValue(DrawModeProperty); set => SetValue(DrawModeProperty,value); }
	public bool ShowOverlay { get => GetValue(ShowOverlayProperty); set => SetValue(ShowOverlayProperty,value); }
	public event EventHandler? EditCompleted;
	private Point _start;
	private Rect _original;
	private bool _drag;
	private int _edges; // left/right/top/bottom; 0 moves, -1 creates
	static TextRegionSelector() { AffectsRender<TextRegionSelector>(ImageProperty,OverlayProperty,RegionProperty,CanvasWidthProperty,CanvasHeightProperty,SnapProperty,SnapXProperty,SnapYProperty,ShowOverlayProperty); }
	public TextRegionSelector() { Focusable=true; ClipToBounds=true; MinHeight=180; }
	private double Scale => Math.Max(.001,Math.Min(Bounds.Width/Math.Max(1,CanvasWidth),Bounds.Height/Math.Max(1,CanvasHeight)));
	private Point Origin => new((Bounds.Width-CanvasWidth*Scale)/2,(Bounds.Height-CanvasHeight*Scale)/2);
	private Point Native(Point point) => new(Math.Clamp((point.X-Origin.X)/Scale,0,CanvasWidth),Math.Clamp((point.Y-Origin.Y)/Scale,0,CanvasHeight));
	private static double Round(double value,double step) => Math.Round(value/step)*step;
	public override void Render(DrawingContext context)
	{
		base.Render(context); context.FillRectangle(Brushes.DimGray,new Rect(Bounds.Size));
		var canvas=new Rect(Origin,new Size(CanvasWidth*Scale,CanvasHeight*Scale));
		context.FillRectangle(Brushes.Black,canvas);
		if(Image!=null) { context.DrawImage(Image,new Rect(Image.Size),canvas); }
		Rect r=Region;
		var visible=new Rect(Origin.X+r.X*Scale,Origin.Y+r.Y*Scale,r.Width*Scale,r.Height*Scale);
		using(context.PushClip(canvas)) {
			if(ShowOverlay && Overlay!=null) { context.DrawImage(Overlay,new Rect(Overlay.Size),visible); }
			var color=StepX>1 || StepY>1 ? Brushes.Cyan : Brushes.Orange;
			context.DrawRectangle(null,new Pen(color,2),visible);
			foreach(var p in new[] {visible.TopLeft,visible.TopRight,visible.BottomLeft,visible.BottomRight,
				new Point(visible.Center.X,visible.Top),new Point(visible.Center.X,visible.Bottom),new Point(visible.Left,visible.Center.Y),new Point(visible.Right,visible.Center.Y)}) {
				context.DrawRectangle(color,new Pen(Brushes.Black,1),new Rect(p.X-3,p.Y-3,6,6));
			}
		}
	}
	protected override void OnPointerPressed(PointerPressedEventArgs e)
	{
		base.OnPointerPressed(e); if(!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed) { return; }
		Focus(); _start=Native(e.GetPosition(this)); _original=Region; _edges=0;
		double hit=7/Scale;
		if(!DrawMode && !e.KeyModifiers.HasFlag(KeyModifiers.Alt) && _original.Inflate(hit).Contains(_start)) {
			if(Math.Abs(_start.X-_original.Left)<hit) { _edges|=1; }
			else if(Math.Abs(_start.X-_original.Right)<hit) { _edges|=2; }
			if(Math.Abs(_start.Y-_original.Top)<hit) { _edges|=4; }
			else if(Math.Abs(_start.Y-_original.Bottom)<hit) { _edges|=8; }
		} else { _edges=-1; }
		_drag=true; e.Pointer.Capture(this); e.Handled=true;
	}
	protected override void OnPointerMoved(PointerEventArgs e)
	{
		base.OnPointerMoved(e); if(!_drag) { return; }
		Point p=Native(e.GetPosition(this)); double stepX=StepX,stepY=StepY;
		double dx=Round(p.X-_start.X,stepX),dy=Round(p.Y-_start.Y,stepY);
		Rect r=_original;
		if(_edges==-1) {
            double x=Math.Floor((Math.Min(_start.X,p.X)+1e-7)/stepX)*stepX,y=Math.Floor((Math.Min(_start.Y,p.Y)+1e-7)/stepY)*stepY;
            double right=Math.Ceiling((Math.Max(_start.X,p.X)-1e-7)/stepX)*stepX,bottom=Math.Ceiling((Math.Max(_start.Y,p.Y)-1e-7)/stepY)*stepY;
			r=new Rect(x,y,Math.Max(stepX,right-x),Math.Max(stepY,bottom-y));
		} else if(_edges==0) {
			r=new Rect(Math.Clamp(Round(r.X+dx,stepX),0,Math.Max(0,CanvasWidth-r.Width)),Math.Clamp(Round(r.Y+dy,stepY),0,Math.Max(0,CanvasHeight-r.Height)),r.Width,r.Height);
		} else {
			double left=r.Left,top=r.Top,right=r.Right,bottom=r.Bottom;
			if((_edges&1)!=0) { left=Math.Clamp(Round(left+dx,stepX),0,Math.Max(0,right-stepX)); }
			if((_edges&2)!=0) { right=Math.Clamp(Round(right+dx,stepX),left+stepX,Math.Max(left+stepX,CanvasWidth)); }
			if((_edges&4)!=0) { top=Math.Clamp(Round(top+dy,stepY),0,Math.Max(0,bottom-stepY)); }
			if((_edges&8)!=0) { bottom=Math.Clamp(Round(bottom+dy,stepY),top+stepY,Math.Max(top+stepY,CanvasHeight)); }
			r=new Rect(left,top,right-left,bottom-top);
		}
		SetCurrentValue(RegionProperty,new Rect(r.X,r.Y,Math.Min(r.Width,CanvasWidth-r.X),Math.Min(r.Height,CanvasHeight-r.Y)));
		e.Handled=true;
	}
	protected override void OnPointerReleased(PointerReleasedEventArgs e)
	{
		base.OnPointerReleased(e); if(!_drag) { return; } _drag=false; SetCurrentValue(DrawModeProperty,false); e.Pointer.Capture(null); EditCompleted?.Invoke(this,EventArgs.Empty); e.Handled=true;
	}
	protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs e) { base.OnPointerCaptureLost(e); _drag=false; }
	protected override void OnKeyDown(KeyEventArgs e)
	{
		base.OnKeyDown(e); int dx=e.Key==Key.Left ? -1 : e.Key==Key.Right ? 1 : 0,dy=e.Key==Key.Up ? -1 : e.Key==Key.Down ? 1 : 0;
		if(dx==0 && dy==0) { return; } int multiplier=e.KeyModifiers.HasFlag(KeyModifiers.Shift) ? 10 : 1,stepX=StepX*multiplier,stepY=StepY*multiplier; Rect r=Region;
		if(e.KeyModifiers.HasFlag(KeyModifiers.Control)) { r=new Rect(r.X,r.Y,Math.Clamp(r.Width+dx*stepX,StepX,Math.Max(StepX,CanvasWidth-r.X)),Math.Clamp(r.Height+dy*stepY,StepY,Math.Max(StepY,CanvasHeight-r.Y))); }
		else { r=new Rect(Math.Clamp(r.X+dx*stepX,0,Math.Max(0,CanvasWidth-r.Width)),Math.Clamp(r.Y+dy*stepY,0,Math.Max(0,CanvasHeight-r.Height)),r.Width,r.Height); }
		SetCurrentValue(RegionProperty,r); EditCompleted?.Invoke(this,EventArgs.Empty); e.Handled=true;
	}
}

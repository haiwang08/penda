# Vector master: assets/penda.svg. Render matching geometry at each Windows icon size.
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
function RoundedPath([single]$x,[single]$y,[single]$w,[single]$h,[single]$r) {
 $p = [Drawing.Drawing2D.GraphicsPath]::new()
 $d = $r*2
 $p.AddArc($x,$y,$d,$d,180,90); $p.AddArc($x+$w-$d,$y,$d,$d,270,90)
 $p.AddArc($x+$w-$d,$y+$h-$d,$d,$d,0,90); $p.AddArc($x,$y+$h-$d,$d,$d,90,90); $p.CloseFigure()
 return ,$p
}
$frames = @()
foreach ($size in @(16,20,24,32,40,48,64,128,256)) {
 $factor = 4
 $canvas = [Drawing.Bitmap]::new($size*$factor,$size*$factor)
 $g = [Drawing.Graphics]::FromImage($canvas)
 $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
 $g.ScaleTransform($size*$factor/256.0,$size*$factor/256.0)
 $blue = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#2463EB'))
 $bg = RoundedPath 8 8 240 240 58; $g.FillPath($blue,$bg); $bg.Dispose()
 $p = [Drawing.Drawing2D.GraphicsPath]::new()
 $p.StartFigure();$p.AddLine(66,54,133,54);$p.AddBezier(133,54,177,54,198,75,198,110);$p.AddBezier(198,110,198,145,177,166,133,166);$p.AddLine(133,166,98,166);$p.AddLine(98,166,98,204);$p.AddLine(98,204,66,204);$p.CloseFigure()
 $p.StartFigure();$p.AddLine(98,84,131,84);$p.AddBezier(131,84,155,84,166,92,166,110);$p.AddBezier(166,110,166,128,155,136,131,136);$p.AddLine(131,136,98,136);$p.CloseFigure()
 $g.FillPath([Drawing.Brushes]::White,$p);$p.Dispose()
 $colors = @('#6EE7D0','#B4D5FF','#FFFFFF')
 for($i=0;$i -lt 3;$i++) { $brush=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($colors[$i]));$dot=RoundedPath (122+$i*26) 186 16 18 4;$g.FillPath($brush,$dot);$dot.Dispose();$brush.Dispose() }
 $g.Dispose();$blue.Dispose()
 $image = [Drawing.Bitmap]::new($size,$size)
 $resample = [Drawing.Graphics]::FromImage($image);$resample.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic;$resample.DrawImage($canvas,0,0,$size,$size);$resample.Dispose();$canvas.Dispose()
 $memory=[IO.MemoryStream]::new();$image.Save($memory,[Drawing.Imaging.ImageFormat]::Png);$frames+=,@{Size=$size;Bytes=$memory.ToArray()};$memory.Dispose()
 if($size -eq 256) {$image.Save((Join-Path $root 'assets/penda.png'),[Drawing.Imaging.ImageFormat]::Png)}
 $image.Dispose()
}
$stream=[IO.File]::Create((Join-Path $root 'assets/penda.ico'));$writer=[IO.BinaryWriter]::new($stream)
$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]$frames.Count)
$offset=6+16*$frames.Count
foreach($frame in $frames) { $edge=if($frame.Size -eq 256){0}else{$frame.Size};$writer.Write([byte]$edge);$writer.Write([byte]$edge);$writer.Write([UInt16]0);$writer.Write([UInt16]1);$writer.Write([UInt16]32);$writer.Write([UInt32]$frame.Bytes.Length);$writer.Write([UInt32]$offset);$offset+=$frame.Bytes.Length }
foreach($frame in $frames){$writer.Write([byte[]]$frame.Bytes)}
$writer.Dispose();$stream.Dispose()

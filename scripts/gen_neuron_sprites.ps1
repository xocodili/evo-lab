Add-Type -AssemblyName System.Drawing

function Save-Sheet {
  param([string]$Path, [scriptblock[]]$Frames)
  $count = $Frames.Count
  $bmp = New-Object System.Drawing.Bitmap (16 * $count), 16
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
  for ($i = 0; $i -lt $count; $i++) {
    $tile = New-Object System.Drawing.Bitmap 16, 16
    $tg = [System.Drawing.Graphics]::FromImage($tile)
    $tg.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    & $Frames[$i] $tg
    $tg.Dispose()
    $g.DrawImage($tile, 16 * $i, 0)
    $tile.Dispose()
  }
  $g.Dispose()
  $dir = Split-Path $Path
  if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
  $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
}

$orange = [System.Drawing.Color]::FromArgb(255, 255, 140, 40)
$teal = [System.Drawing.Color]::FromArgb(255, 80, 200, 160)
$tealDark = [System.Drawing.Color]::FromArgb(255, 60, 180, 140)
$purple = [System.Drawing.Color]::FromArgb(255, 180, 120, 220)
$penO = New-Object System.Drawing.Pen $orange, 1
$brushTeal = New-Object System.Drawing.SolidBrush $teal
$brushTealD = New-Object System.Drawing.SolidBrush $tealDark
$brushPur = New-Object System.Drawing.SolidBrush $purple

$perceptorFrames = @(
  {
    param($tg)
    $tg.DrawEllipse($penO, 2, 4, 11, 9)
    $tg.FillRectangle([System.Drawing.Brushes]::Black, 2, 7, 12, 6)
    $tg.FillEllipse($brushTeal, 5, 5, 5, 4)
  },
  {
    param($tg)
    $tg.DrawEllipse($penO, 2, 3, 11, 10)
    $tg.FillEllipse($brushTealD, 4, 5, 7, 6)
    $tg.FillEllipse([System.Drawing.Brushes]::White, 6, 6, 2, 2)
  }
)

$actuatorFrames = @(
  {
    param($tg)
    $tg.FillRectangle($brushPur, 7, 2, 2, 12)
    $tg.DrawLine([System.Drawing.Pens]::Plum, 8, 14, 5, 15)
    $tg.DrawLine([System.Drawing.Pens]::Plum, 8, 14, 11, 15)
  },
  {
    param($tg)
    $tg.FillRectangle($brushPur, 7, 2, 2, 12)
    $tg.DrawLine([System.Drawing.Pens]::Magenta, 8, 14, 4, 15)
    $tg.DrawLine([System.Drawing.Pens]::Plum, 8, 14, 12, 13)
  },
  {
    param($tg)
    $tg.FillRectangle($brushPur, 7, 2, 2, 12)
    $tg.DrawLine([System.Drawing.Pens]::Magenta, 8, 14, 12, 15)
    $tg.DrawLine([System.Drawing.Pens]::Plum, 8, 14, 4, 13)
  },
  {
    param($tg)
    $tg.FillRectangle($brushPur, 7, 2, 2, 12)
    $tg.DrawLine([System.Drawing.Pens]::Magenta, 8, 14, 4, 15)
    $tg.DrawLine([System.Drawing.Pens]::Plum, 8, 14, 12, 13)
  }
)

$root = Split-Path $PSScriptRoot -Parent
$res = Join-Path $root "resources\sprites"
$assets = Join-Path $root "assets\sprites"

Save-Sheet (Join-Path $res "perceptor_sprites.png") $perceptorFrames
Save-Sheet (Join-Path $res "actuator_sprites.png") $actuatorFrames
Copy-Item (Join-Path $res "perceptor_sprites.png") $assets -Force
Copy-Item (Join-Path $res "actuator_sprites.png") $assets -Force
Write-Output "Generated neuron sprite sheets in $res"

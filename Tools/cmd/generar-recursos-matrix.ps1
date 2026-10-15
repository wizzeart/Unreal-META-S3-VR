$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$branding = Join-Path $projectRoot 'Branding'
$androidRes = Join-Path $projectRoot 'Build\Android\res'

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw 'Se necesita ffmpeg para generar los tamaños de Android.'
}

function Export-Png([string]$source, [string]$filter, [string]$target) {
    & ffmpeg -y -loglevel error -i $source -vf $filter -frames:v 1 $target
    if ($LASTEXITCODE -ne 0) { throw "No se pudo generar $target" }
}

$iconSource = Join-Path $branding 'app-icon-source.png'
$coverSource = Join-Path $branding 'cover-source.png'
$portraitSource = Join-Path $branding 'portrait-source.png'
foreach ($source in @($iconSource, $coverSource, $portraitSource)) {
    if (-not (Test-Path -LiteralPath $source)) { throw "Falta $source" }
}

Export-Png $iconSource 'scale=512:512:flags=lanczos' (Join-Path $branding 'app-icon.png')
Export-Png $iconSource 'crop=1100:800:77:25,scale=512:372:flags=lanczos,pad=512:512:0:70:color=0x020819' (Join-Path $branding 'app-mark.png')
Export-Png $coverSource 'scale=1280:720:force_original_aspect_ratio=increase:flags=lanczos,crop=1280:720' (Join-Path $branding 'cover.png')
Export-Png $portraitSource 'scale=720:1280:force_original_aspect_ratio=increase:flags=lanczos,crop=720:1280' (Join-Path $branding 'portrait.png')

$mark = Join-Path $branding 'app-mark.png'
foreach ($entry in @(
    @('drawable-ldpi', 36), @('drawable-mdpi', 48),
    @('drawable-hdpi', 72), @('drawable-xhdpi', 96), @('drawable', 48)
)) {
    $folder = Join-Path $androidRes $entry[0]
    Export-Png $mark "scale=$($entry[1]):$($entry[1]):flags=lanczos" (Join-Path $folder 'icon.png')
}

Copy-Item -LiteralPath (Join-Path $branding 'cover.png') -Destination (Join-Path $androidRes 'drawable\downloadimageh.png') -Force
Copy-Item -LiteralPath (Join-Path $branding 'portrait.png') -Destination (Join-Path $androidRes 'drawable\downloadimagev.png') -Force
Export-Png (Join-Path $branding 'cover.png') 'scale=640:360:flags=lanczos' (Join-Path $androidRes 'drawable\splashscreen_landscape.png')
Export-Png (Join-Path $branding 'portrait.png') 'scale=360:640:flags=lanczos' (Join-Path $androidRes 'drawable\splashscreen_portrait.png')
Write-Host 'Recursos Matrix VR Show generados.'

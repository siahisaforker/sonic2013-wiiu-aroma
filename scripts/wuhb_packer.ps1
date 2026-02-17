Param(
    [string]$OutDir = "./out",
    [string]$RpxPath = "./bin/RSDKv4.rpx",
    [string]$Choice = ''
)

$Root = Split-Path -Parent (Resolve-Path $PSScriptRoot)
$IconDir = Join-Path $Root "icon"
$ResolvedOut = Resolve-Path -LiteralPath $OutDir -ErrorAction SilentlyContinue | ForEach-Object { $_.ProviderPath }
if (-not $ResolvedOut) { $ResolvedOut = Join-Path $Root "out" }
$OutDir = $ResolvedOut

Write-Host "WUHB Packer (PowerShell) - creates a .wuhb (zip) containing the RPX and icon"

if (-not (Test-Path $RpxPath)) {
    Write-Error "RPX not found at $RpxPath"
    exit 1
}

if ($Choice -ne '') {
    $choice = $Choice
} else {
    Write-Host "Choose icon:`n  1) Sonic 1`n  2) Sonic 2"
    try {
        $choice = Read-Host "Select 1 or 2"
    } catch {
        Write-Error "No interactive terminal available; provide -Choice 1 or 2 when running non-interactively."
        exit 1
    }
}
if ($choice -ne '1' -and $choice -ne '2') { Write-Error "Invalid choice: $choice (use 1 or 2)"; exit 1 }

$iconName = "Sonic $choice"
$exts = @('png','tga','jpg','jpeg')
$found = $null
$foundBanner = $null

# Candidate directories (per-game subfolder or top-level icon folder)
$candidates = @(Join-Path $IconDir $iconName, (Join-Path $IconDir "Sonic$choice"), (Join-Path $IconDir "sonic$choice"), $IconDir)
foreach ($dir in $candidates) {
    foreach ($e in $exts) {
        $p = Join-Path $dir ("icon.$e")
        if (Test-Path $p -and -not $found) { $found = $p }
        $b = Join-Path $dir ("banner.$e")
        if (Test-Path $b -and -not $foundBanner) { $foundBanner = $b }
    }
    if ($found) { break }
}
if (-not $found) { Write-Error "Could not find icon (icon.png/jpg/etc.) for $iconName in $IconDir or subfolders"; exit 1 }

$magick = (Get-Command magick -ErrorAction SilentlyContinue) -or (Get-Command convert -ErrorAction SilentlyContinue)

$pkgDir = Join-Path $OutDir ("wuhb_pack_{0}" -f (Get-Date -Format yyyyMMdd_HHmmss))
New-Item -Path (Join-Path $pkgDir 'wiiu\apps\RSDKv4') -ItemType Directory -Force | Out-Null

 $targetIcon = Join-Path $pkgDir 'wiiu\apps\RSDKv4\icon.png'
if ($magick) {
    Write-Host "Converting icon to PNG using ImageMagick..."
    if (Get-Command magick -ErrorAction SilentlyContinue) {
        & magick "$found" -resize 256x256 "$targetIcon"
    } else {
        & convert "$found" -resize 256x256 "$targetIcon"
    }
} else {
    Write-Host "ImageMagick not found; copying icon as-is (must be PNG)."
    Copy-Item -Path $found -Destination $targetIcon -Force
}

# Optional banner
$targetBanner = $null
if ($foundBanner) {
    $targetBanner = Join-Path $pkgDir 'wiiu\apps\RSDKv4\banner.png'
    if ($magick) {
        Write-Host "Converting banner to PNG using ImageMagick..."
        if (Get-Command magick -ErrorAction SilentlyContinue) {
            & magick "$foundBanner" -resize 1280x720 "$targetBanner"
        } else {
            & convert "$foundBanner" -resize 1280x720 "$targetBanner"
        }
    } else {
        Copy-Item -Path $foundBanner -Destination $targetBanner -Force
    }
}



Copy-Item -Path $RpxPath -Destination (Join-Path $pkgDir 'wiiu\apps\RSDKv4\RSDKv4.rpx') -Force

Set-Content -Path (Join-Path $pkgDir 'wiiu\apps\RSDKv4\metadata.txt') -Value @(
    ("title=Sonic {0}" -f $choice),
    "game=RSDKv4",
    ("source=Sonic {0}" -f $choice),
    ("pack_time={0}" -f (Get-Date -Format o))
)

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$outFile = Join-Path $OutDir ("Sonic{0}.wuhb" -f $choice)

# Require a WUHB CLI and use it exclusively
$wuhbCmd = $env:WUHB_CMD
if (-not $wuhbCmd) {
    foreach ($c in @('wuhb','wuhbtool','wuhbpack','wuhbcreate')) {
        if (Get-Command $c -ErrorAction SilentlyContinue) { $wuhbCmd = $c; break }
    }
}

if (-not $wuhbCmd) {
    $wuhbCmd = (Get-Command wuhbtool -ErrorAction SilentlyContinue).Path
    if (-not $wuhbCmd -and (Test-Path "/opt/devkitpro/tools/bin/wuhbtool")) { $wuhbCmd = "/opt/devkitpro/tools/bin/wuhbtool" }
}

if (-not $wuhbCmd) {
    Write-Error "wuhbtool not found. Install devkitPro tools in WSL or set WUHB_CMD to its path."
    exit 1
}

$rpxIn = Join-Path $pkgDir 'wiiu\apps\RSDKv4\RSDKv4.rpx'
$contentDir = Join-Path $pkgDir 'wiiu'

Write-Host "Creating .wuhb using $wuhbCmd..."
try {
    $args = @($rpxIn, $outFile, '--content', $contentDir, '--icon', $targetIcon, '--name', ("Sonic {0}" -f $choice), '--short-name', ("Sonic{0}" -f $choice), '--author', 'RSDKv4 Packager')
    if ($targetBanner) { $args += @('--tv-image', $targetBanner, '--drc-image', $targetBanner) }
    & $wuhbCmd @args | Out-Null
    Write-Host "Wuhb package created: $outFile"
} catch {
    Write-Error "wuhbtool failed to create package. Check the tool and try again."
    exit 1
}

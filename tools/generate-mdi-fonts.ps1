param()

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$fontOutputDirectory = Join-Path $repoRoot 'src\fonts'
$mdiVersion = '7.4.47'
$converterVersion = '1.5.3'
$mdiRange = '983040-991231'
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$tempDirectory = Join-Path $tempBase (
    'HomeTiles-mdi-fonts-' + [guid]::NewGuid().ToString('N'))

$targets = @(
    @{
        Size = 40
        LayoutMacro = 'DEVICE_LAYOUT_1024X600'
    },
    @{
        Size = 32
        LayoutMacro = 'DEVICE_LAYOUT_480X480'
    }
)

New-Item -ItemType Directory -Path $tempDirectory | Out-Null

try {
    & npm.cmd pack "@mdi/font@$mdiVersion" `
        --pack-destination $tempDirectory | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to download @mdi/font@$mdiVersion."
    }

    $archive = Get-ChildItem -LiteralPath $tempDirectory -Filter '*.tgz' |
        Select-Object -First 1
    if (-not $archive) {
        throw 'Downloaded MDI package archive was not found.'
    }

    & tar.exe -xf $archive.FullName -C $tempDirectory
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to extract the downloaded MDI package.'
    }

    $packageDirectory = Join-Path $tempDirectory 'package'
    $sourceFont = Join-Path `
        $packageDirectory 'fonts\materialdesignicons-webfont.woff'
    $sourceLicense = Join-Path $packageDirectory 'LICENSE'
    if (-not (Test-Path -LiteralPath $sourceFont)) {
        throw "MDI source font was not found: $sourceFont"
    }
    if (-not (Test-Path -LiteralPath $sourceLicense)) {
        throw "MDI license was not found: $sourceLicense"
    }

    foreach ($target in $targets) {
        $size = [int]$target.Size
        $fontName = "mdi_icons_$size"
        $outputPath = Join-Path $fontOutputDirectory "$fontName.c"
        $generatedPath = Join-Path $tempDirectory "$fontName.c"

        & npx.cmd --yes "lv_font_conv@$converterVersion" `
            --bpp 4 `
            --size $size `
            --font $sourceFont `
            --range $mdiRange `
            --format lvgl `
            --lv-font-name $fontName `
            --output $generatedPath
        if ($LASTEXITCODE -ne 0) {
            throw "Unable to generate $fontName."
        }

        $source = [IO.File]::ReadAllText($generatedPath)
        $stableOptions =
            " * Opts: --bpp 4 --size $size --font " +
            "materialdesignicons-webfont.woff --range $mdiRange " +
            "--format lvgl --lv-font-name $fontName"
        $source = [regex]::Replace(
            $source,
            '(?m)^ \* Opts: .+$',
            $stableOptions,
            1)
        $source = [regex]::Replace(
            $source,
            '(?ms)^#ifdef LV_LVGL_H_INCLUDE_SIMPLE\r?\n' +
            '#include "lvgl\.h"\r?\n#else\r?\n' +
            '#include "lvgl/lvgl\.h"\r?\n#endif',
            '#include "lvgl.h"',
            1)
        $macro = "MDI_ICONS_$size"
        $guardedBlock = @"
#include "src/devices/device_select.h"

#ifndef $macro
#if defined($($target.LayoutMacro))
#define $macro 1
#else
#define $macro 0
#endif
#endif
"@
        $defaultPattern =
            "(?m)^#ifndef $macro\r?\n#define $macro 1\r?\n#endif"
        if (-not [regex]::IsMatch($source, $defaultPattern)) {
            throw "Generated macro block was not found in $outputPath."
        }
        $source = [regex]::Replace(
            $source,
            $defaultPattern,
            $guardedBlock.TrimEnd(),
            1)
        $source = $source.TrimEnd("`r", "`n") + "`n"
        [IO.File]::WriteAllText(
            $outputPath,
            $source,
            [Text.UTF8Encoding]::new($false))
        Write-Output "Generated $outputPath"
    }

    Copy-Item -LiteralPath $sourceLicense `
        -Destination (Join-Path `
            $fontOutputDirectory 'MaterialDesignIcons-LICENSE.txt') `
        -Force
}
finally {
    $resolvedTemp = [IO.Path]::GetFullPath($tempDirectory)
    if ($resolvedTemp.StartsWith(
            $tempBase,
            [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path $resolvedTemp -Leaf).StartsWith(
            'HomeTiles-mdi-fonts-',
            [StringComparison]::Ordinal)) {
        Remove-Item -LiteralPath $resolvedTemp -Recurse -Force `
            -ErrorAction SilentlyContinue
    }
}

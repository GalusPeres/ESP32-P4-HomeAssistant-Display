param(
    [ValidateSet('repo-short-tail', 'repo-a8204', 'repo-guition-jc8012-rx-single-block')]
    [string]$EspHostedRxVariant = 'repo-a8204'
)

$ErrorActionPreference = 'Stop'

$coreVersion = '3.3.7'
$arduinoEsp32 = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\esp32'
$ar = Get-ChildItem (Join-Path $arduinoEsp32 'tools') -Recurse -Filter 'riscv32-esp-elf-ar.exe' |
    Select-Object -First 1 -ExpandProperty FullName

if (-not $ar) {
    throw 'riscv32-esp-elf-ar.exe was not found in the Arduino ESP32 installation.'
}

$repoRoot = Split-Path $PSScriptRoot -Parent
$txFixDirectory = Join-Path $repoRoot 'tools\esp-hosted-3.3.7-tx-fix'
$rxFixDirectory = Join-Path $repoRoot 'tools\esp-hosted-3.3.7-rx-fix'
$variants = @('esp32p4-libs', 'esp32p4_es-libs')

foreach ($variant in $variants) {
    $archive = Join-Path $arduinoEsp32 "tools\$variant\$coreVersion\lib\libespressif__esp_hosted.a"
    if (-not (Test-Path -LiteralPath $archive)) {
        throw "ESP-Hosted archive not found: $archive"
    }

    $backup = "$archive.hometiles-unpatched-backup"
    if (-not (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $archive -Destination $backup
    }

    $objects = @(
        Get-ChildItem -LiteralPath (Join-Path $txFixDirectory $variant) -Filter '*.obj'
        Get-ChildItem -LiteralPath (Join-Path $rxFixDirectory $variant) -Filter '*.obj' |
            Where-Object Name -ne 'sdio_drv.c.obj'
    )
    $sdioObject = if ($EspHostedRxVariant -eq 'repo-short-tail') {
        Join-Path (Join-Path $rxFixDirectory $variant) 'sdio_drv.c.obj'
    } else {
        Join-Path (Join-Path (Join-Path $rxFixDirectory 'baseline-a8204') $variant) 'sdio_drv.c.obj'
    }
    if (-not (Test-Path -LiteralPath $sdioObject)) {
        throw "ESP-Hosted RX object not found: $sdioObject"
    }
    $objects += Get-Item -LiteralPath $sdioObject

    # JC8012P4A1 V1 is fixed to the pre-v3 P4 silicon profile, which links the
    # esp32p4_es archive. Always inject either the proven single-block reader or
    # its stock baseline so consecutive local builds cannot leak the workaround
    # into another pre-v3 device profile.
    if ($variant -eq 'esp32p4_es-libs') {
        $portSdioVariant = if ($EspHostedRxVariant -eq 'repo-guition-jc8012-rx-single-block') {
            'guition-jc8012-rx-single-block'
        } else {
            'baseline-a8204'
        }
        $portSdioObject = Join-Path `
            (Join-Path (Join-Path $rxFixDirectory $portSdioVariant) $variant) `
            'port_esp_hosted_host_sdio.c.obj'
        if (-not (Test-Path -LiteralPath $portSdioObject)) {
            throw "ESP-Hosted SDIO port object not found: $portSdioObject"
        }
        $objects += Get-Item -LiteralPath $portSdioObject
    }

    $archiveChanged = $false
    $verifyDirectory = Join-Path $env:TEMP ("hometiles-esp-hosted-verify-" + [Guid]::NewGuid())
    New-Item -ItemType Directory -Path $verifyDirectory | Out-Null
    Push-Location $verifyDirectory
    try {
        foreach ($object in $objects) {
            $memberPath = Join-Path $verifyDirectory $object.Name
            Remove-Item -LiteralPath $memberPath -Force -ErrorAction SilentlyContinue
            & $ar x $archive $object.Name
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to extract $($object.Name) from $archive"
            }
            $expected = (Get-FileHash -Algorithm SHA256 $object.FullName).Hash
            $actual = if (Test-Path -LiteralPath $memberPath) {
                (Get-FileHash -Algorithm SHA256 $memberPath).Hash
            } else {
                ''
            }
            if ($actual -eq $expected) {
                continue
            }

            & $ar rs $archive $object.FullName
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to inject $($object.Name) into $archive"
            }
            $archiveChanged = $true

            Remove-Item -LiteralPath $memberPath -Force -ErrorAction SilentlyContinue
            & $ar x $archive $object.Name
            if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $memberPath)) {
                throw "Failed to verify $($object.Name) in $archive"
            }
            $actual = (Get-FileHash -Algorithm SHA256 $memberPath).Hash
            if ($actual -ne $expected) {
                throw "Verification failed for $($object.Name) in $archive"
            }
        }
    }
    finally {
        Pop-Location
        Remove-Item -LiteralPath $verifyDirectory -Recurse -Force
    }

    $status = if ($archiveChanged) { 'patched and verified' } else { 'already patched' }
    Write-Host "ESP-Hosted $status ($EspHostedRxVariant): $archive"
}

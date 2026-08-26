param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('tab5', 'waveshare_b4', 'waveshare_4_3', 'waveshare_7', 'waveshare_7b', 'waveshare_7b_rev3_1', 'waveshare_8', 'waveshare_10_1', 'waveshare_s3_touch_lcd_4b', 'layout_test_1024x600', 'layout_test_480x480', 'guition_jc8012p4a1', 'guition_jc8012p4a1_v2', 'guition_jc1060p470c', 'guition_jc1060p470c_v2', 'guition_esp32_4848s040')]
    [string]$Profile,

    [string]$OutputDirectory,

    [string[]]$ExtraDefine = @(),

    [ValidateSet('auto', 'repo-short-tail', 'repo-a8204')]
    [string]$EspHostedRxVariant = 'auto',

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$sketchProfiles = Join-Path $repoRoot 'sketch.yaml'
$hiddenSketchProfiles = Join-Path $repoRoot 'sketch.yaml.hometiles-local-build'
$arduinoCli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
$libraries = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino\libraries'
$repoLibraries = Join-Path $repoRoot 'third_party'

if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "Arduino CLI was not found: $arduinoCli"
}
if (-not (Test-Path -LiteralPath $sketchProfiles)) {
    throw "Sketch profiles were not found: $sketchProfiles"
}
if (Test-Path -LiteralPath $hiddenSketchProfiles) {
    throw "Temporary profile file already exists: $hiddenSketchProfiles"
}

$node = Get-Command node -ErrorAction Stop
& $node.Source (Join-Path $PSScriptRoot 'generate-web-assets.mjs') --check
if ($LASTEXITCODE -ne 0) {
    throw 'Generated WebUI assets are stale. Run tools/generate-web-assets.mjs.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-mqtt-packet-safety.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'MQTT packet safety regression test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-weather-popup-release.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Weather popup release interaction test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-s3-popup-refresh.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'ESP32-S3 popup refresh contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-duplicate-light-entity-updates.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Duplicate light entity update test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-s3-render-backpressure.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'ESP32-S3 render backpressure test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-s3-weather-batching.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'ESP32-S3 weather work budget test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-guition-s3-update-check-resync.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Guition S3 update-check display guard test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-admin-cover-editor.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Admin Cover editor contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-guition-jc8012-v2-profile.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Guition JC8012P4A1 V2 profile contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-jc1060-sd-power.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Guition JC1060P470C SD power contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-waveshare-7b-profile.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Waveshare Touch LCD 7B / 7B-C profile contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-guition-jc1060-v2-profile.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Guition JC1060P470C V2 profile contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-waveshare-s3-4b-profile.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Waveshare ESP32-S3 Touch LCD 4B profile contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-waveshare-4-3-profile.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Waveshare Touch LCD 4.3 profile contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-waveshare-10-brightness-floor.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Waveshare Touch LCD 10.1 brightness floor contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-new-device-profile-integration.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'New device profile integration contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-p4-camera-presenter.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'ESP32-P4 camera presenter contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-camera-bridge-timeout.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Camera Bridge timeout contract test failed.'
}
& $node.Source (Join-Path $PSScriptRoot 'test-import-latest-arduino-build.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Arduino build importer contract test failed.'
}

$defines = @{
    tab5 = 'DEVICE_M5STACKS_TAB5'
    waveshare_b4 = 'DEVICE_WAVESHARE_4B'
    waveshare_4_3 = 'DEVICE_WAVESHARE_TOUCH_LCD_4_3'
    waveshare_7 = 'DEVICE_WAVESHARE_TOUCH_LCD_7'
    waveshare_7b = 'DEVICE_WAVESHARE_TOUCH_LCD_7B'
    waveshare_7b_rev3_1 = 'DEVICE_WAVESHARE_TOUCH_LCD_7B'
    waveshare_8 = 'DEVICE_WAVESHARE_TOUCH_LCD_8'
    waveshare_10_1 = 'DEVICE_WAVESHARE_TOUCH_LCD_10_1'
    layout_test_1024x600 = 'DEVICE_LAYOUT_TEST_1024X600'
    layout_test_480x480 = 'DEVICE_LAYOUT_TEST_480X480'
    guition_jc8012p4a1 = 'DEVICE_GUITION_JC8012P4A1'
    guition_jc8012p4a1_v2 = 'DEVICE_GUITION_JC8012P4A1_V2'
    guition_jc1060p470c = 'DEVICE_GUITION_JC1060P470C'
    guition_jc1060p470c_v2 = 'DEVICE_GUITION_JC1060P470C_V2'
    guition_esp32_4848s040 = 'DEVICE_GUITION_ESP32_4848S040'
    waveshare_s3_touch_lcd_4b = 'DEVICE_WAVESHARE_S3_TOUCH_LCD_4B'
}

$nativeS3Profiles = @(
    'guition_esp32_4848s040',
    'waveshare_s3_touch_lcd_4b'
)
$isNativeS3 = $Profile -in $nativeS3Profiles

$profileLines = Get-Content -LiteralPath $sketchProfiles
$insideProfile = $false
$fqbn = $null
foreach ($line in $profileLines) {
    if ($line -match '^  ([^:\s][^:]*):\s*$') {
        $insideProfile = $Matches[1] -eq $Profile
        continue
    }
    if ($insideProfile -and $line -match '^    fqbn:\s*(.+?)\s*$') {
        $fqbn = $Matches[1]
        break
    }
}
if (-not $fqbn) {
    throw "FQBN for profile '$Profile' was not found in sketch.yaml."
}

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repoRoot "build\local-safe-$Profile"
} elseif (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot $OutputDirectory
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$resolvedEspHostedRxVariant = if ($EspHostedRxVariant -ne 'auto') {
    $EspHostedRxVariant
} else {
    'repo-a8204'
}

# The Arduino profile command reinstalls the ESP32 platform immediately before
# compiling and can silently overwrite the patched ESP-Hosted archive. Apply
# and verify the fixes first, then compile by FQBN while sketch.yaml is hidden.
if (-not $isNativeS3) {
    & (Join-Path $PSScriptRoot 'apply-esp-hosted-3.3.7-fixes-local.ps1') `
        -EspHostedRxVariant $resolvedEspHostedRxVariant
}

$commonFlags = "-DLV_CONF_INCLUDE_SIMPLE -I$repoRoot -I$libraries"
$extraDefineFlags = @()
foreach ($define in $ExtraDefine) {
    if ($define -notmatch '^[A-Za-z_][A-Za-z0-9_]*(=.*)?$') {
        throw "Invalid extra compiler define: $define"
    }
    $extraDefineFlags += "-D$define"
}
$cppFlags = "-DHOMETILES_CI_TARGET -D$($defines[$Profile]) $($extraDefineFlags -join ' ') $commonFlags"
$cFlags = $cppFlags

Move-Item -LiteralPath $sketchProfiles -Destination $hiddenSketchProfiles
try {
    [string[]]$cleanArgs = @()
    if ($Clean) {
        $cleanArgs += '--clean'
    }
    & $arduinoCli compile @cleanArgs `
        --fqbn $fqbn `
        --export-binaries `
        --output-dir $OutputDirectory `
        --libraries $repoLibraries `
        --build-property "compiler.c.extra_flags=$cFlags" `
        --build-property "compiler.cpp.extra_flags=$cppFlags" `
        $repoRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Arduino build failed for profile '$Profile'."
    }
}
finally {
    if (Test-Path -LiteralPath $hiddenSketchProfiles) {
        Move-Item -LiteralPath $hiddenSketchProfiles -Destination $sketchProfiles
    }
}

$firmwareBin = Join-Path $OutputDirectory 'HomeTiles.ino.bin'
if (-not (Test-Path -LiteralPath $firmwareBin)) {
    $alternateBins = @(Get-ChildItem -LiteralPath $OutputDirectory -File -Filter '*.ino.bin')
    if ($alternateBins.Count -ne 1) {
        throw "Firmware binary was not created: $firmwareBin"
    }
    Copy-Item -LiteralPath $alternateBins[0].FullName -Destination $firmwareBin
}
$otaSlotBytes = 0x680000
$firmwareBytes = (Get-Item -LiteralPath $firmwareBin).Length
if ($firmwareBytes -gt $otaSlotBytes) {
    throw "Firmware is $firmwareBytes bytes, larger than the $otaSlotBytes-byte OTA slot."
}

$stringsToolName = if ($isNativeS3) {
    'xtensa-esp32s3-elf-strings.exe'
} else {
    'riscv32-esp-elf-strings.exe'
}
$stringsTool = Get-ChildItem `
    (Join-Path $env:LOCALAPPDATA 'Arduino15\packages\esp32\tools') `
    -Recurse -Filter $stringsToolName |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $stringsTool) {
    throw "$stringsToolName was not found."
}

if (-not $isNativeS3) {
    $firmwareStrings = & $stringsTool $firmwareBin
    $fatalAssertions = $firmwareStrings |
        Select-String -Pattern 'pkt_rxbuff|copy_buff'
    if ($fatalAssertions) {
        throw "Stock ESP-Hosted allocation assert found in $firmwareBin"
    }
    $rpcSerializationMarker = $firmwareStrings |
        Select-String -SimpleMatch 'HomeTiles RPC sync serialization active'
    if (-not $rpcSerializationMarker) {
        throw "ESP-Hosted RPC serialization marker missing from $firmwareBin"
    }
    $sdioRxRecoveryMarker = $firmwareStrings |
        Select-String -SimpleMatch 'HomeTiles SDIO RX recovery active (a8204f9 raw PKT_LEN + pending drain)'
    if (-not $sdioRxRecoveryMarker) {
        throw "ESP-Hosted PKT_LEN/pending RX recovery marker missing from $firmwareBin"
    }
    $sdioRxShortTailMarker = $firmwareStrings |
        Select-String -SimpleMatch 'HomeTiles SDIO RX 512-byte padding disabled (CMD53 short tail, 4-byte aligned)'
    if ($resolvedEspHostedRxVariant -eq 'repo-short-tail' -and
        -not $sdioRxShortTailMarker) {
        throw "ESP-Hosted short-tail CMD53 RX marker missing from $firmwareBin"
    }
    if ($resolvedEspHostedRxVariant -eq 'repo-a8204' -and
        $sdioRxShortTailMarker) {
        throw "Unexpected ESP-Hosted short-tail CMD53 RX marker found in a8204 baseline build: $firmwareBin"
    }
    $obsoletePktLenDrop = $firmwareStrings |
        Select-String -SimpleMatch 'PKT_LEN reg all-ones (bus read error); dropping read'
    if ($obsoletePktLenDrop) {
        throw "Obsolete masked PKT_LEN drop path found in $firmwareBin"
    }
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $firmwareBin).Hash
Write-Host "Safe firmware build completed: $firmwareBin"
Write-Host "SHA256: $hash"
if (-not $isNativeS3) {
    Write-Host "ESP-Hosted RX variant: $resolvedEspHostedRxVariant"
}

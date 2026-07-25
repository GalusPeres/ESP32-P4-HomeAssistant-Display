param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('tab5', 'waveshare_b4', 'waveshare_7', 'waveshare_8', 'waveshare_10_1', 'layout_test_1024x600', 'guition_jc8012p4a1', 'guition_jc1060p470c')]
    [string]$Profile,

    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$sketchProfiles = Join-Path $repoRoot 'sketch.yaml'
$hiddenSketchProfiles = Join-Path $repoRoot 'sketch.yaml.hometiles-local-build'
$arduinoCli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
$libraries = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Arduino\libraries'

if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "Arduino CLI was not found: $arduinoCli"
}
if (-not (Test-Path -LiteralPath $sketchProfiles)) {
    throw "Sketch profiles were not found: $sketchProfiles"
}
if (Test-Path -LiteralPath $hiddenSketchProfiles) {
    throw "Temporary profile file already exists: $hiddenSketchProfiles"
}

$defines = @{
    tab5 = 'DEVICE_M5STACKS_TAB5'
    waveshare_b4 = 'DEVICE_WAVESHARE_4B'
    waveshare_7 = 'DEVICE_WAVESHARE_TOUCH_LCD_7'
    waveshare_8 = 'DEVICE_WAVESHARE_TOUCH_LCD_8'
    waveshare_10_1 = 'DEVICE_WAVESHARE_TOUCH_LCD_10_1'
    layout_test_1024x600 = 'DEVICE_LAYOUT_TEST_1024X600'
    guition_jc8012p4a1 = 'DEVICE_GUITION_JC8012P4A1'
    guition_jc1060p470c = 'DEVICE_GUITION_JC1060P470C'
}

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

# The Arduino profile command reinstalls the ESP32 platform immediately before
# compiling and can silently overwrite the patched ESP-Hosted archive. Apply
# and verify the fixes first, then compile by FQBN while sketch.yaml is hidden.
& (Join-Path $PSScriptRoot 'apply-esp-hosted-3.3.7-fixes-local.ps1')

$commonFlags = "-DLV_CONF_INCLUDE_SIMPLE -I$repoRoot -I$libraries"
$cppFlags = "-DHOMETILES_CI_TARGET -D$($defines[$Profile]) $commonFlags"
$cFlags = $cppFlags

Move-Item -LiteralPath $sketchProfiles -Destination $hiddenSketchProfiles
try {
    & $arduinoCli compile `
        --clean `
        --fqbn $fqbn `
        --export-binaries `
        --output-dir $OutputDirectory `
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
    throw "Firmware binary was not created: $firmwareBin"
}

$stringsTool = Get-ChildItem `
    (Join-Path $env:LOCALAPPDATA 'Arduino15\packages\esp32\tools') `
    -Recurse -Filter 'riscv32-esp-elf-strings.exe' |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $stringsTool) {
    throw 'riscv32-esp-elf-strings.exe was not found.'
}

$fatalAssertions = & $stringsTool $firmwareBin |
    Select-String -Pattern 'pkt_rxbuff|copy_buff'
if ($fatalAssertions) {
    throw "Stock ESP-Hosted allocation assert found in $firmwareBin"
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $firmwareBin).Hash
Write-Host "Safe firmware build completed: $firmwareBin"
Write-Host "SHA256: $hash"

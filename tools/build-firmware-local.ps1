param(
    [Parameter(Mandatory = $true)]
    [string]$Profile,

    [string]$OutputDirectory,

    [string[]]$ExtraDefine = @(),

    [ValidateSet('auto', 'repo-short-tail', 'repo-a8204', 'repo-guition-jc8012-rx-single-block')]
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
# Preserve the case-insensitive profile names accepted by PowerShell ValidateSet.
$Profile = $Profile.ToLowerInvariant()
$profileJson = & $node.Source (Join-Path $PSScriptRoot 'device-catalog.js') --profile $Profile
if ($LASTEXITCODE -ne 0) {
    throw "Unknown or invalid build profile: $Profile"
}
$buildProfile = $profileJson | ConvertFrom-Json
& $node.Source (Join-Path $PSScriptRoot 'generate-device-profiles.mjs') --check
if ($LASTEXITCODE -ne 0) {
    throw 'Generated device profiles are stale. Run tools/generate-device-profiles.mjs.'
}
& $node.Source (Join-Path $PSScriptRoot 'generate-web-assets.mjs') --check
if ($LASTEXITCODE -ne 0) {
    throw 'WebUI asset verification failed. Install host dependencies with npm ci --ignore-scripts, then run node tools/generate-web-assets.mjs.'
}
& $node.Source (Join-Path $PSScriptRoot 'run-tests.mjs')
if ($LASTEXITCODE -ne 0) {
    throw 'Host regression suite failed.'
}

$isNativeS3 = $buildProfile.chipFamily -eq 'ESP32-S3'

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
    $buildProfile.rxVariant
}

if ($resolvedEspHostedRxVariant -eq 'repo-guition-jc8012-rx-single-block' -and
    $Profile -ne 'guition_jc8012p4a1') {
    throw 'The JC8012 single-block RX variant is limited to the exact Guition V1 profile.'
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
$cppFlags = "-DHOMETILES_CI_TARGET -D$($buildProfile.define) $($extraDefineFlags -join ' ') $commonFlags"
$cFlags = $cppFlags
$elfFlags = $buildProfile.elfFlags

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
        --build-property "compiler.c.elf.extra_flags=$elfFlags" `
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
    if ($resolvedEspHostedRxVariant -ne 'repo-short-tail' -and
        $sdioRxShortTailMarker) {
        throw "Unexpected ESP-Hosted short-tail CMD53 RX marker found in baseline build: $firmwareBin"
    }
    $sdioRxSingleBlockMarker = $firmwareStrings |
        Select-String -SimpleMatch 'HomeTiles Issue30 RX single-block workaround active: max_blocks_per_CMD53=1'
    if ($resolvedEspHostedRxVariant -eq 'repo-guition-jc8012-rx-single-block' -and
        -not $sdioRxSingleBlockMarker) {
        throw "ESP-Hosted JC8012 single-block RX marker missing from $firmwareBin"
    }
    if ($resolvedEspHostedRxVariant -ne 'repo-guition-jc8012-rx-single-block' -and
        $sdioRxSingleBlockMarker) {
        throw "Unexpected JC8012 single-block RX marker found in $firmwareBin"
    }
    $obsoletePktLenDrop = $firmwareStrings |
        Select-String -SimpleMatch 'PKT_LEN reg all-ones (bus read error); dropping read'
    if ($obsoletePktLenDrop) {
        throw "Obsolete masked PKT_LEN drop path found in $firmwareBin"
    }

    $firmwareMap = Join-Path $OutputDirectory 'HomeTiles.ino.map'
    $hasJc8012SdioWrapper = (Test-Path -LiteralPath $firmwareMap) -and
        ((Get-Content -LiteralPath $firmwareMap -Raw).Contains('__wrap_esp_hosted_get_default_sdio_config'))
    if ($Profile -eq 'guition_jc8012p4a1' -and -not $hasJc8012SdioWrapper) {
        throw "JC8012 V1 SDIO configuration wrapper missing from $firmwareMap"
    }
    if ($Profile -ne 'guition_jc8012p4a1' -and $hasJc8012SdioWrapper) {
        throw "Unexpected JC8012 V1 SDIO configuration wrapper found in $firmwareMap"
    }
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $firmwareBin).Hash
Write-Host "Firmware compilation completed: $firmwareBin"
Write-Host "SHA256: $hash"
if (-not $isNativeS3) {
    Write-Host "ESP-Hosted RX variant: $resolvedEspHostedRxVariant"
}

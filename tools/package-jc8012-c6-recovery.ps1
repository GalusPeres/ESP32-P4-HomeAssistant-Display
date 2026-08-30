param(
    [string]$OutputDirectory = 'build\issue30-jc8012-combined'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
if (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot $OutputDirectory
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$partitionCsv = Get-Content -LiteralPath (Join-Path $repoRoot 'partitions.csv') -Raw
if ($partitionCsv -notmatch '(?m)^app0,\s+app,\s+ota_0,\s+0x10000,\s+0x680000,' -or
    $partitionCsv -notmatch '(?m)^app1,\s+app,\s+ota_1,\s+0x690000,\s+0x680000,') {
    throw 'The recovery packager requires the checked 16-MB HomeTiles OTA partition layout.'
}

$c6Url = 'https://espressif.github.io/arduino-esp32/hosted/esp32c6-v2.11.6.bin'
$c6ExpectedBytes = 1191424
$c6ExpectedSha256 = 'E32FBA3864AB4DB82C287A922DB83B7093D7D8592730D7A620887B7CFDF401E0'
$c6Image = Join-Path $OutputDirectory 'esp32c6-v2.11.6.bin'

$needsDownload = $true
if (Test-Path -LiteralPath $c6Image) {
    $existingHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $c6Image).Hash
    $existingBytes = (Get-Item -LiteralPath $c6Image).Length
    $needsDownload = $existingHash -ne $c6ExpectedSha256 -or
        $existingBytes -ne $c6ExpectedBytes
}
if ($needsDownload) {
    Invoke-WebRequest -UseBasicParsing -Uri $c6Url -OutFile $c6Image
}

$c6Bytes = (Get-Item -LiteralPath $c6Image).Length
$c6Hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $c6Image).Hash
if ($c6Bytes -ne $c6ExpectedBytes -or $c6Hash -ne $c6ExpectedSha256) {
    throw "Official ESP32-C6 image verification failed: bytes=$c6Bytes sha256=$c6Hash"
}

$arduinoOutput = Join-Path $OutputDirectory 'arduino'
$node = Get-Command node -ErrorAction Stop
& $node.Source (Join-Path $PSScriptRoot 'generate-web-assets.mjs') --check
if ($LASTEXITCODE -ne 0) {
    throw 'Generated WebUI assets are stale.'
}
foreach ($testName in @(
    'test-jc8012-c6-recovery.mjs',
    'test-p4-camera-presenter.mjs',
    'test-camera-bridge-timeout.mjs',
    'test-mqtt-packet-safety.mjs'
)) {
    & $node.Source (Join-Path $PSScriptRoot $testName)
    if ($LASTEXITCODE -ne 0) {
        throw "Recovery preflight test failed: $testName"
    }
}

& (Join-Path $PSScriptRoot 'build-firmware-local.ps1') `
    -Profile guition_jc8012p4a1 `
    -OutputDirectory $arduinoOutput `
    -ExtraDefine HOMETILES_JC8012_C6_RECOVERY `
    -EspHostedRxVariant repo-a8204
if ($LASTEXITCODE -ne 0) {
    throw 'The JC8012P4A1 V1 recovery firmware build failed.'
}

$mergedInput = Join-Path $arduinoOutput 'HomeTiles.ino.merged.bin'
if (-not (Test-Path -LiteralPath $mergedInput)) {
    throw "Merged firmware image was not created: $mergedInput"
}
if ((Get-Item -LiteralPath $mergedInput).Length -ne 0x1000000) {
    throw 'The recovery image must be the complete 16-MB merged P4 image.'
}

$outputBin = Join-Path $OutputDirectory 'HomeTilesBeta.bin'
Copy-Item -LiteralPath $mergedInput -Destination $outputBin -Force

$app1Offset = 0x690000
$app1Size = 0x680000
$emptyApp1Sha256 = 'BEC9891CAFA7ECC1B3259DAE79C5330D2585F64ABB431557C553D60128CEEE77'
$headerSize = 64
$payloadOffset = 0x1000
$payloadEnd = $app1Offset + $payloadOffset + $c6ExpectedBytes
if ($payloadEnd -gt 0xD10000) {
    throw 'The ESP32-C6 recovery payload does not fit inside app1.'
}

$emptyCheckStream = [IO.File]::OpenRead($outputBin)
$emptyCheckHash = [Security.Cryptography.IncrementalHash]::CreateHash(
    [Security.Cryptography.HashAlgorithmName]::SHA256)
try {
    $emptyCheckStream.Position = $app1Offset
    $emptyCheckBuffer = New-Object byte[] 65536
    $remaining = $app1Size
    while ($remaining -gt 0) {
        $requested = [Math]::Min($emptyCheckBuffer.Length, $remaining)
        $readNow = $emptyCheckStream.Read($emptyCheckBuffer, 0, $requested)
        if ($readNow -ne $requested) {
            throw 'Could not verify the empty app1 staging partition.'
        }
        $emptyCheckHash.AppendData($emptyCheckBuffer, 0, $readNow)
        $remaining -= $readNow
    }
    $actualEmptyHash = [Convert]::ToHexString($emptyCheckHash.GetHashAndReset())
}
finally {
    $emptyCheckHash.Dispose()
    $emptyCheckStream.Dispose()
}
if ($actualEmptyHash -ne $emptyApp1Sha256) {
    throw "Refusing to overwrite nonempty app1 staging partition: $actualEmptyHash"
}

function Get-RecoveryHeaderCrc32([byte[]]$Data, [int]$Length) {
    [uint32]$crc = [uint32]::MaxValue
    for ($index = 0; $index -lt $Length; $index++) {
        $crc = $crc -bxor [uint32]$Data[$index]
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($crc -band 1) -ne 0) {
                $crc = [uint32](($crc -shr 1) -bxor [uint32]3988292384)
            } else {
                $crc = [uint32]($crc -shr 1)
            }
        }
    }
    return [uint32](-bnot $crc)
}

$header = New-Object byte[] $headerSize
$magic = [Text.Encoding]::ASCII.GetBytes('HTC6R001')
[Array]::Copy($magic, 0, $header, 0, $magic.Length)
[Array]::Copy([BitConverter]::GetBytes([uint32]$headerSize), 0, $header, 8, 4)
[Array]::Copy([BitConverter]::GetBytes([uint32]$payloadOffset), 0, $header, 12, 4)
[Array]::Copy([BitConverter]::GetBytes([uint32]$c6ExpectedBytes), 0, $header, 16, 4)
$hashBytes = [Convert]::FromHexString($c6ExpectedSha256)
[Array]::Copy($hashBytes, 0, $header, 20, $hashBytes.Length)
$header[52] = 2
$header[53] = 11
$header[54] = 6
$header[55] = 1  # Streaming SDIO image.
$headerCrc32 = Get-RecoveryHeaderCrc32 $header 56
[Array]::Copy([BitConverter]::GetBytes([uint32]$headerCrc32), 0, $header, 56, 4)

$outputStream = [IO.File]::Open(
    $outputBin,
    [IO.FileMode]::Open,
    [IO.FileAccess]::ReadWrite,
    [IO.FileShare]::None)
$inputStream = [IO.File]::OpenRead($c6Image)
try {
    $outputStream.Position = $app1Offset
    $outputStream.Write($header, 0, $header.Length)
    $outputStream.Position = $app1Offset + $payloadOffset
    $inputStream.CopyTo($outputStream)
    $outputStream.Flush($true)
}
finally {
    $inputStream.Dispose()
    $outputStream.Dispose()
}

$verifyStream = [IO.File]::OpenRead($outputBin)
$verifyHeader = New-Object byte[] $headerSize
$verifyPayload = New-Object byte[] $c6ExpectedBytes
try {
    $verifyStream.Position = $app1Offset
    if ($verifyStream.Read($verifyHeader, 0, $verifyHeader.Length) -ne $verifyHeader.Length) {
        throw 'Could not read the packaged recovery header.'
    }
    $verifyStream.Position = $app1Offset + $payloadOffset
    $readTotal = 0
    while ($readTotal -lt $verifyPayload.Length) {
        $readNow = $verifyStream.Read(
            $verifyPayload, $readTotal, $verifyPayload.Length - $readTotal)
        if ($readNow -le 0) {
            throw 'Could not read the packaged ESP32-C6 payload.'
        }
        $readTotal += $readNow
    }
}
finally {
    $verifyStream.Dispose()
}

if (-not [Linq.Enumerable]::SequenceEqual[byte]($header, $verifyHeader)) {
    throw 'Packaged recovery header verification failed.'
}
$verifyPayloadHash = [Convert]::ToHexString(
    [Security.Cryptography.SHA256]::HashData($verifyPayload))
if ($verifyPayloadHash -ne $c6ExpectedSha256) {
    throw "Packaged ESP32-C6 payload hash mismatch: $verifyPayloadHash"
}

$outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputBin).Hash
$appBin = Join-Path $arduinoOutput 'HomeTiles.ino.bin'
$appBytes = (Get-Item -LiteralPath $appBin).Length
$appHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $appBin).Hash

Write-Host "Recovery beta: $outputBin"
Write-Host "Recovery beta bytes: $((Get-Item -LiteralPath $outputBin).Length)"
Write-Host "Recovery beta SHA256: $outputHash"
Write-Host "P4 app bytes: $appBytes"
Write-Host "P4 app SHA256: $appHash"
Write-Host "Embedded C6 bytes: $c6Bytes"
Write-Host "Embedded C6 SHA256: $c6Hash"

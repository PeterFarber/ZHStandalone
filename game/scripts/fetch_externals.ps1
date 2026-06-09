# Download vendored third-party sources into build/external (CMake build).
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Ext = Join-Path $Root "build\external"
New-Item -ItemType Directory -Force -Path $Ext | Out-Null

function Ensure-Dir($name) {
    $p = Join-Path $Ext $name
    if (-not (Test-Path $p)) { New-Item -ItemType Directory -Force -Path $p | Out-Null }
    return $p
}

function Download-File($url, $dest) {
    if (Test-Path $dest) { return }
    Write-Host "[fetch] $url"
    Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
}

function Ensure-Zip($url, $zipName, $folderName, $renameTo) {
    $dir = Join-Path $Ext $renameTo
    if (Test-Path $dir) { return }
    $zip = Join-Path $Ext $zipName
    Download-File $url $zip
    Write-Host "[fetch] unzip $zipName -> $renameTo"
    Expand-Archive -Path $zip -DestinationPath $Ext -Force
    $extracted = Join-Path $Ext $folderName
    if ($renameTo -ne $folderName -and (Test-Path $extracted)) {
        Move-Item $extracted $dir
    }
    Remove-Item $zip -ErrorAction SilentlyContinue
}

Ensure-Zip `
    "https://github.com/lsalzman/enet/archive/refs/heads/master.zip" `
    "enet-master.zip" "enet-master" "enet"

Ensure-Zip `
    "https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip" `
    "json.zip" "json-3.11.3" "nlohmann"

$maDir = Ensure-Dir "miniaudio"
$maHeader = Join-Path $maDir "miniaudio.h"
Download-File "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" $maHeader

$stbDir = Ensure-Dir "stb"
Download-File "https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h" (Join-Path $stbDir "stb_truetype.h")

Ensure-Zip `
    "https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip" `
    "glm.zip" "glm-1.0.1" "glm"

Ensure-Zip `
    "https://github.com/zeux/volk/archive/refs/heads/master.zip" `
    "volk.zip" "volk-master" "volk"

Write-Host "[fetch] done: $Ext"

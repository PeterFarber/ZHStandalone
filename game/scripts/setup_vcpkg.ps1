param(
    [string]$VcpkgRoot = $(Join-Path $env:USERPROFILE "vcpkg")
)

$ErrorActionPreference = "Stop"

function Ensure-Vcpkg {
    if (-not (Test-Path (Join-Path $VcpkgRoot "vcpkg.exe"))) {
        if (Test-Path $VcpkgRoot) {
            Write-Host 'vcpkg folder exists but vcpkg.exe missing:' $VcpkgRoot
        } else {
            Write-Host 'Cloning vcpkg to' $VcpkgRoot
            git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
        }
        Push-Location $VcpkgRoot
        & .\bootstrap-vcpkg.bat -disableMetrics
        Pop-Location
    }
    if (-not (Test-Path (Join-Path $VcpkgRoot "vcpkg.exe"))) {
        throw "vcpkg.exe not found after bootstrap ($VcpkgRoot)"
    }
    return (Resolve-Path $VcpkgRoot).Path
}

$root = Ensure-Vcpkg
$env:VCPKG_ROOT = $root
Write-Host 'VCPKG_ROOT=' $root
Write-Output $root

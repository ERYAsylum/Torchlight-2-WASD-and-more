$ErrorActionPreference = "Stop"

$cleanPath = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
    [Environment]::GetEnvironmentVariable("Path", "User")
Remove-Item Env:PATH -ErrorAction SilentlyContinue
Remove-Item Env:Path -ErrorAction SilentlyContinue
$env:Path = $cleanPath

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = $null
if (Test-Path -LiteralPath $vswhere) {
    $installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if ($installPath) {
        $candidate = Join-Path $installPath "MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path -LiteralPath $candidate) {
            $msbuild = $candidate
        }
    }
}

if (-not $msbuild) {
    $fallbacks = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    )
    $msbuild = $fallbacks | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

if (-not $msbuild) {
    throw "MSBuild 2022 introuvable. Installez Visual Studio 2022 ou Build Tools avec Desktop development with C++."
}

$solution = Join-Path $PSScriptRoot "TL2TrueControlerSupport.sln"
Write-Host "MSBuild: $msbuild"
Write-Host "Compilation Release|Win32 de TL2TrueControlerSupport v0.0.9..."
& $msbuild $solution /restore /p:Configuration=Release /p:Platform=Win32 /m
if ($LASTEXITCODE -ne 0) {
    throw "TL2TrueControlerSupport build failed."
}

$dist = Join-Path $PSScriptRoot "dist"
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "TL2TrueControlerSupport.ini") -Destination (Join-Path $dist "TL2TrueControlerSupport.ini") -Force

$exe = Join-Path $dist "TL2TrueControlerSupport.exe"
$configExe = Join-Path $dist "TL2TrueControlerSupportConfig.exe"
$d3d9 = Join-Path $dist "d3d9.dll"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "TL2TrueControlerSupport.exe absent apres compilation."
}
if (-not (Test-Path -LiteralPath $configExe)) {
    throw "TL2TrueControlerSupportConfig.exe absent apres compilation."
}
if (-not (Test-Path -LiteralPath $d3d9)) {
    throw "d3d9.dll absent apres compilation."
}

Write-Host "Build termine. Fichiers prets dans: $dist"

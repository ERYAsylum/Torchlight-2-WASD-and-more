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
    throw "MSBuild 2022 introuvable. Installez Visual Studio 2022 ou Build Tools avec 'Desktop development with C++', MSVC v143 x86/x64 et le Windows SDK."
}

$solution = Join-Path $PSScriptRoot "TL2TrueKeyboardMove.sln"
Write-Host "MSBuild: $msbuild"
Write-Host "Compilation Release|Win32 de la solution complète v1.0.02z1 (mod, tests de mouvement/overlay et proxy MSWSOCK)..."
& $msbuild $solution /restore /p:Configuration=Release /p:Platform=Win32 /m
if ($LASTEXITCODE -ne 0) {
    throw "TL2TrueKeyboardMove solution build failed."
}

$tests = Join-Path $PSScriptRoot "dist\tests\CameraMathTests.exe"
if (-not (Test-Path -LiteralPath $tests)) {
    throw "CameraMathTests.exe absent après compilation."
}
& $tests
if ($LASTEXITCODE -ne 0) {
    throw "Core movement and overlay placement tests failed."
}

Copy-Item -LiteralPath (Join-Path $PSScriptRoot "TL2TrueKeyboardMove.ini") -Destination (Join-Path $PSScriptRoot "dist\TL2TrueKeyboardMove.ini") -Force

$dll = Join-Path $PSScriptRoot "dist\TL2TrueKeyboardMove.dll"
$proxy = Join-Path $PSScriptRoot "dist\mswsockproxy\MSWSOCK.dll"
if (-not (Test-Path -LiteralPath $dll)) { throw "TL2TrueKeyboardMove.dll absent après compilation." }
if (-not (Test-Path -LiteralPath $proxy)) { throw "MSWSOCK.dll absent après compilation." }

Write-Host "Build terminé. Fichiers prêts dans: $(Join-Path $PSScriptRoot 'dist')"

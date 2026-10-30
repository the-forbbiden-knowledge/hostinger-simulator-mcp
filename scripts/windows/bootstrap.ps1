param([ValidateSet('windows-msvc','windows-ninja')][string]$Preset = 'windows-msvc')
$ErrorActionPreference = 'Stop'
Set-Location (Resolve-Path (Join-Path $PSScriptRoot '../..'))
cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
if ($Preset -eq 'windows-msvc') {
  cmake --build --preset $Preset --config Release
  if ($LASTEXITCODE -ne 0) { throw "Build failed." }
  ctest --preset $Preset -C Release --output-on-failure
} else {
  cmake --build --preset $Preset
  if ($LASTEXITCODE -ne 0) { throw "Build failed." }
  ctest --preset $Preset --output-on-failure
}
if ($LASTEXITCODE -ne 0) { throw "Tests failed." }
Write-Host "Native build and tests passed ($Preset)."

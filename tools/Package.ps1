param(
    [Parameter(Mandatory = $true)][string]$BuildDir
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root 'dist'
$mod = Join-Path $dist 'mod'

$versionLine = Select-String -Path (Join-Path $root 'src\Version.hpp') -Pattern 'WA_VERSION_STRING "([^"]+)"'
$version = $versionLine.Matches[0].Groups[1].Value

foreach ($file in 'WootingAnalog.dll', 'WootingAnalogCheck.exe') {
    if (-not (Test-Path (Join-Path $BuildDir $file))) { throw "Missing $file in $BuildDir - build first" }
}

if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
$pluginDir = Join-Path $mod 'red4ext\plugins\WootingAnalog'
$scriptDir = Join-Path $mod 'r6\scripts\WootingAnalog'
New-Item -ItemType Directory -Force $pluginDir, $scriptDir | Out-Null

Copy-Item (Join-Path $BuildDir 'WootingAnalog.dll') $pluginDir
Copy-Item (Join-Path $BuildDir 'WootingAnalogCheck.exe') $pluginDir
Copy-Item (Join-Path $root 'scripts\WootingAnalog\*.reds') $scriptDir

$zip = Join-Path $dist "WootingAnalog-$version.zip"
Compress-Archive -Path (Join-Path $mod '*') -DestinationPath $zip

Copy-Item (Join-Path $root 'tools\Install-WootingAnalog.ps1') $dist
Copy-Item (Join-Path $root 'tools\Setup.bat') $dist
Copy-Item (Join-Path $root 'README.md') $dist

Write-Host ""
Write-Host "Packaged Wooting Analog $version"
Get-ChildItem -Recurse $dist -File | ForEach-Object {
    Write-Host ("  {0,-70} {1,10:N0} bytes" -f $_.FullName.Substring($dist.Length + 1), $_.Length)
}

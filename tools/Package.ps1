param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [string]$OutDir
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function New-Zip([string]$Source, [string]$Destination) {
    $root = (Resolve-Path $Source).Path.TrimEnd('\') + '\'
    $stream = [IO.File]::Open($Destination, [IO.FileMode]::Create)
    $zip = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem -Recurse -File $Source) {
            $name = $file.FullName.Substring($root.Length).Replace('\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $name, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally {
        $zip.Dispose()
        $stream.Dispose()
    }
}

$root = Split-Path -Parent $PSScriptRoot
$dist = if ($OutDir) { $OutDir } else { Join-Path $root 'dist' }
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

Copy-Item (Join-Path $root 'tools\Install-WootingAnalog.ps1') $dist
Copy-Item (Join-Path $root 'tools\Setup.bat') $dist
Copy-Item (Join-Path $root 'README.md') $dist

$setupZip = Join-Path ([IO.Path]::GetTempPath()) "WootingAnalog-$version-setup.zip"
New-Zip $dist $setupZip
Move-Item -Force $setupZip (Join-Path $dist "WootingAnalog-$version-setup.zip")

$zip = Join-Path $dist "WootingAnalog-$version.zip"
New-Zip $mod $zip

Write-Host ""
Write-Host "Packaged Wooting Analog $version"
Get-ChildItem -Recurse $dist -File | ForEach-Object {
    Write-Host ("  {0,-70} {1,10:N0} bytes" -f $_.FullName.Substring($dist.Length + 1), $_.Length)
}

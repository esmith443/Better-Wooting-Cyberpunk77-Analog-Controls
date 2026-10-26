param(
    [string]$GamePath,
    [switch]$CheckOnly,
    [switch]$Uninstall,
    [switch]$Force,
    [switch]$NoPause
)

$ErrorActionPreference = 'Continue'
$script:problems = 0
$script:warnings = 0
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Say([string]$Tag, [string]$Text, [string]$Color = 'Gray') {
    Write-Host ("  [{0}] " -f $Tag) -NoNewline -ForegroundColor $Color
    Write-Host $Text
}
function Ok([string]$Text) { Say 'ok' $Text 'Green' }
function Warn([string]$Text) { $script:warnings++; Say '!!' $Text 'Yellow' }
function Bad([string]$Text) { $script:problems++; Say 'XX' $Text 'Red' }
function Info([string]$Text) { Say '--' $Text 'DarkGray' }
function Hint([string]$Text) { Write-Host ("       " + $Text) -ForegroundColor DarkCyan }
function Section([string]$Text) { Write-Host ""; Write-Host $Text -ForegroundColor Cyan }

function Finish([int]$Code) {
    Write-Host ""
    if (-not $NoPause) { Write-Host "Press any key to close..."; [void][Console]::ReadKey($true) }
    exit $Code
}

function FileVersion([string]$Path) {
    try {
        $v = (Get-Item $Path).VersionInfo
        if ($v.ProductVersion) { return $v.ProductVersion.Trim() }
        return $v.FileVersion
    } catch { return $null }
}

function Find-Game {
    $candidates = New-Object System.Collections.Generic.List[string]

    $steam = $null
    try { $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath } catch {}
    if (-not $steam) { try { $steam = (Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam' -ErrorAction Stop).InstallPath } catch {} }
    if ($steam) {
        $libraries = @($steam)
        $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
        if (Test-Path $vdf) {
            foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) {
                $libraries += $m.Groups[1].Value -replace '\\\\', '\'
            }
        }
        foreach ($lib in $libraries) { $candidates.Add((Join-Path $lib 'steamapps\common\Cyberpunk 2077')) }
    }

    foreach ($key in 'HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games\1423049311', 'HKLM:\SOFTWARE\GOG.com\Games\1423049311') {
        try { $candidates.Add((Get-ItemProperty $key -ErrorAction Stop).path) } catch {}
    }

    $manifests = Join-Path $env:ProgramData 'Epic\EpicGamesLauncher\Data\Manifests'
    if (Test-Path $manifests) {
        foreach ($item in Get-ChildItem $manifests -Filter *.item -ErrorAction SilentlyContinue) {
            try {
                $json = Get-Content $item.FullName -Raw | ConvertFrom-Json
                if ($json.DisplayName -like 'Cyberpunk 2077*') { $candidates.Add($json.InstallLocation) }
            } catch {}
        }
    }

    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c 'bin\x64\Cyberpunk2077.exe'))) { return (Resolve-Path $c).Path }
    }
    return $null
}

Write-Host ""
Write-Host "Wooting Analog for Cyberpunk 2077 - setup and health check" -ForegroundColor White
Write-Host "-----------------------------------------------------------"

Section "Game"
if ($GamePath) {
    if (-not (Test-Path (Join-Path $GamePath 'bin\x64\Cyberpunk2077.exe'))) { Bad "No Cyberpunk2077.exe under '$GamePath'"; Finish 1 }
    $game = (Resolve-Path $GamePath).Path
} else {
    $game = Find-Game
    if (-not $game) {
        Bad "Cyberpunk 2077 not found automatically."
        Hint "Run again with:  Setup.bat -GamePath ""D:\path\to\Cyberpunk 2077"""
        Finish 1
    }
}
Ok "Found: $game"
$gameVersion = FileVersion (Join-Path $game 'bin\x64\Cyberpunk2077.exe')
Info "Game version $gameVersion"

$pluginDir = Join-Path $game 'red4ext\plugins\WootingAnalog'
$scriptDir = Join-Path $game 'r6\scripts\WootingAnalog'

if ($Uninstall) {
    Section "Uninstall"
    foreach ($dir in $pluginDir, $scriptDir) {
        if (Test-Path $dir) { Remove-Item -Recurse -Force $dir; Ok "Removed $dir" } else { Info "Not present: $dir" }
    }
    Info "Your movement keys go back to normal at the next game start (Input Loader rebuilds its cache)."
    Finish 0
}

Section "Requirements"
$red4ext = Join-Path $game 'red4ext\RED4ext.dll'
$loader = @('winmm.dll', 'version.dll') | Where-Object { Test-Path (Join-Path $game "bin\x64\$_") }
if ((Test-Path $red4ext) -and $loader) {
    Ok ("RED4ext {0}" -f (FileVersion $red4ext))
} else {
    Bad "RED4ext is not installed (needed to load the plugin)."
    Hint "Get it from https://github.com/wopss/RED4ext/releases"
}

if (Test-Path (Join-Path $game 'engine\tools\scc.exe')) { Ok "redscript" }
else { Bad "redscript is not installed."; Hint "Get it from https://github.com/jac3km4/redscript/releases" }

$inputLoader = Join-Path $game 'red4ext\plugins\input_loader\input_loader.dll'
if (Test-Path $inputLoader) { Ok "Input Loader" }
else { Bad "Input Loader is not installed (it applies the analog bindings)."; Hint "Get it from https://github.com/jackhumbert/cyberpunk2077-input-loader/releases" }

if (Test-Path (Join-Path $game 'red4ext\plugins\mod_settings')) { Ok "Mod Settings (in-game settings page available)" }
else { Info "Mod Settings not installed: settings live in red4ext\plugins\WootingAnalog\WootingAnalog.ini" }

Section "Conflicts"
$ilLog = Join-Path $game 'red4ext\logs\input_loader.log'
if (Test-Path $ilLog) {
    $log = Get-Content $ilLog -Raw
    if ($log -match 'is not a known vanilla file') {
        Bad "The game's own input config files in r6\config were replaced by an old mod."
        Hint "This is what the old 'Analog Keyboard Movement and Driving Binds' mod does. It breaks Input Loader."
        Hint "Fix: Steam > Cyberpunk 2077 > Properties > Installed Files > Verify integrity (or restore your backup)."
    } elseif ($log -match 'is the vanilla') {
        Ok "r6\config input files are vanilla"
    } else {
        Info "Input Loader has not checked the config files yet (start the game once)"
    }
} else {
    Info "No Input Loader log yet"
}

$inputDir = Join-Path $game 'r6\input'
if (Test-Path $inputDir) {
    $clashes = Get-ChildItem $inputDir -Filter *.xml -Recurse -ErrorAction SilentlyContinue | Where-Object {
        (Get-Content $_.FullName -Raw) -match 'name="(MovementActions|LeftX_Axis_Vehicle|Acceleration_Axis|Deceleration_Axis)"'
    }
    if ($clashes) {
        foreach ($c in $clashes) { Warn "r6\input\$($c.Name) also changes movement or driving inputs; one of the mods may not work." }
    } else { Ok "No other input mods touch movement or driving" }
}

$redsLog = Get-ChildItem (Join-Path $game 'r6\logs') -Filter 'redscript*.log' -ErrorAction SilentlyContinue |
    Where-Object { $_.Length -gt 0 } | Sort-Object LastWriteTime | Select-Object -Last 1
if ($redsLog) {
    $reds = Get-Content $redsLog.FullName -Raw
    if ($reds -match 'REDScript compilation has failed') {
        Warn "redscript failed to compile on the last start, so NO redscript mods (this one included) are running."
        $culprits = [regex]::Matches($reds, '(?m)^- (.+)$') | ForEach-Object { $_.Groups[1].Value.Trim() } | Select-Object -Unique
        if ($culprits) { Hint ("Caused by: " + ($culprits -join ', ')) }
        Hint "Update or remove those mods. Until then analog walking still works, but driving context, the"
        Hint "in-game settings page and analog throttle need redscript."
    } elseif ($reds.Length -gt 0) {
        Ok "redscript compiled fine on the last start"
    }
}

Section "Wooting"
$sdkPaths = @(@(
    (Join-Path $env:ProgramW6432 'wooting-analog-sdk\wooting_analog_sdk.dll'),
    (Join-Path $env:ProgramW6432 'WootingAnalogSDK\wooting_analog_sdk.dll')
) | Where-Object { $_ -and (Test-Path $_) })
if ($sdkPaths) {
    Ok ("Wooting Analog SDK installed ({0})" -f $sdkPaths[0])
} else {
    Warn "Wooting Analog SDK not installed - the mod stays off until it is."
    Hint "Install or update Wootility (it installs the SDK), or get the .msi from"
    Hint "https://github.com/WootingKb/wooting-analog-sdk/releases"
}

$wooting = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match '^USB\\VID_31E3&PID_([0-9A-F]{4})\\' -or $_.InstanceId -match '^USB\\VID_03EB&PID_FF0[12]\\' }
if ($wooting) {
    foreach ($dev in $wooting) {
        if ($dev.InstanceId -match 'PID_([0-9A-F]{4})') {
            $productId = [Convert]::ToInt32($Matches[1], 16)
            $mode = switch ($productId -band 0xF) { 0 { 'Xbox' } 1 { 'Classic' } 2 { 'None' } default { 'unknown' } }
            Ok ("Wooting keyboard connected (USB {0}, Wootility gamepad mode: {1})" -f $Matches[1], $mode)
            if ($mode -eq 'Xbox' -or $mode -eq 'Classic') {
                Warn "Wootility's gamepad mode is on. The mod does not need it."
                Hint "If any keys are bound to gamepad inputs in Wootility (old guides told you to bind WASD to the"
                Hint "left stick), remove those binds or set Settings > Gamepad mode to None to avoid double input."
            }
        }
    }
} else {
    Warn "No Wooting keyboard found right now. Plug it in before starting the game."
}

$check = Join-Path $here 'mod\red4ext\plugins\WootingAnalog\WootingAnalogCheck.exe'
if (-not (Test-Path $check)) { $check = Join-Path $pluginDir 'WootingAnalogCheck.exe' }
if ((Test-Path $check) -and $sdkPaths -and $wooting) {
    $out = & $check --summary 2>&1
    if ($LASTEXITCODE -eq 0) {
        Ok "The SDK can read the keyboard"
        $out | Where-Object { $_ -match '^(Keyboard|Game keybinds):' } | ForEach-Object { Info $_ }
    } else {
        Bad "The keyboard check failed:"
        $out | Select-Object -Last 3 | ForEach-Object { Hint $_ }
    }
}

$source = Join-Path $here 'mod'
if (-not $CheckOnly) {
    Section "Install"
    if (-not (Test-Path $source)) {
        Bad "No mod files next to this script (expected a 'mod' folder). Run build.bat first or use the release zip."
    } elseif ($script:problems -gt 0 -and -not $Force) {
        Warn "Not installing until the problems above are fixed (or run with -Force)."
    } else {
        $vortex = Test-Path (Join-Path $game 'red4ext\plugins\__folder_managed_by_vortex')
        if ($vortex -and -not $Force) {
            Warn "This game is managed by Vortex. Install the zip with Vortex instead, so it can track the files:"
            $zip = Get-ChildItem $here -Filter 'WootingAnalog-*.zip' | Select-Object -First 1
            if ($zip) { Hint ("Vortex > Mods > Install From File > " + $zip.FullName) }
            Hint "Or run Setup.bat -Force to copy the files directly anyway."
        } else {
            try {
                if (Test-Path $scriptDir) { Remove-Item -Recurse -Force $scriptDir }
                Copy-Item -Recurse -Force (Join-Path $source '*') $game
                Ok "Installed to $game"
                Info "Settings: $pluginDir\WootingAnalog.ini (created on first start)"
            } catch {
                Bad "Copy failed: $($_.Exception.Message)"
                Hint "Close the game and try again, or run this as administrator if the game is in Program Files."
            }
        }
    }
}

Section "Last game start"
$status = Join-Path $pluginDir 'WootingAnalog.status.txt'
if (Test-Path $status) {
    Get-Content $status | ForEach-Object { Info $_ }
    if ((Get-Content $status -Raw) -match 'active: no') {
        Warn "The mod was inactive last time; the reason is on the 'status:' line above."
    }
} elseif (Test-Path (Join-Path $pluginDir 'WootingAnalog.dll')) {
    Info "Start the game once; the mod then writes a status report here."
} else {
    Info "Not installed yet."
}

Section "Summary"
if ($script:problems -eq 0 -and $script:warnings -eq 0) { Ok "Everything looks good. Start the game and walk around: light presses walk, full presses run." }
elseif ($script:problems -eq 0) { Ok ("Ready, with {0} warning(s) above worth a look." -f $script:warnings) }
else { Bad ("{0} problem(s) above need fixing." -f $script:problems) }
Finish ([int]($script:problems -gt 0))

# Better Wooting // Cyberpunk 2077 Analog Controls

> Your Wooting reads every millimeter of travel. Night City only heard *on* and *off*. Not anymore, choom.

Real analog movement and driving on a Wooting keyboard. Feather **W** to creep through a
Militech checkpoint, slam it to sprint. Ride the throttle through Kabuki, trail-brake into
corners, and steer with the precision of an Arasaka-grade wheel.

No gamepad emulation in Wootility. No edited game files. Your keybinds stay yours.

## // FEATURES

- **Plug-and-jack:** detects your Wooting at launch and wires itself in. No keyboard, no changes.
- **Analog everything:** walk, run, sprint, steer, throttle, brake. Bikes, cars, tanks, remote-hacked rides.
- **Keyboard prompts stay keyboard:** no Xbox icons flickering on your HUD.
- **Reads your binds:** remapped keys and other layouts just work.
- **In-game tuning:** deadzone, full-press point and response curve per action via Mod Settings.
- **Debug mode:** live on-screen meters for every key, plus logs and `Ctrl+Shift+F10` snapshots.
- **Patch-proof:** rebuilds its bindings from the game's own files every launch.

## // REQUIREMENTS

| Chrome | Why |
|---|---|
| [RED4ext](https://github.com/wopss/RED4ext/releases) | loads the plugin |
| [redscript](https://github.com/jac3km4/redscript/releases) | runs the script layer |
| [Input Loader](https://github.com/jackhumbert/cyberpunk2077-input-loader/releases) | applies the analog bindings |
| [Mod Settings](https://github.com/jackhumbert/mod_settings/releases) | *optional:* in-game settings page |
| [Codeware](https://github.com/psiberx/cp2077-codeware/releases) | *optional:* debug meters |
| Wootility | installs the Wooting Analog SDK |

## // INSTALL

1. Grab `WootingAnalog-x.y.z.zip` from [Releases](../../releases).
2. **Vortex:** *Install From File*, then deploy. **Manual:** drop `red4ext` and `r6` into your game folder.
3. Wootility → Settings → Gamepad mode → **None**.
4. Jack in. Your HUD will say `Wooting ... detected`.

Analog throttle and brake come online from the second launch.

> Ran the old *Analog Keyboard Movement and Driving Binds* mod? Verify your game files first, it overwrote `inputUserMappings.xml`.

## // TUNING

**Settings → Mods → Wooting Analog**, or `red4ext/plugins/WootingAnalog/WootingAnalog.ini`.

| Knob | Does |
|---|---|
| Deadzone | key travel ignored at the top |
| Full press at | travel that counts as maxed out |
| Response curve | 1 = linear, higher = finer control on light presses |

Can't sprint? Turn on **Debug mode** and push **Sent: stick Y** past the red line. Lower *Full press at* if you can't reach it.

## // BUILD

```
git clone --recursive https://github.com/esmith443/Better-Wooting-Cyberpunk77-Analog-Controls
build.bat
```

Needs Visual Studio 2022+ with the C++ workload. Output lands in `dist/`.

## // FLATLINED?

`tools/Setup.bat -CheckOnly` scans your rig and names the problem. Logs: `red4ext/logs/WootingAnalog-*.log`.

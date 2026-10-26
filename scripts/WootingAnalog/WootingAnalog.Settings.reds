enum WootingAnalogInputMethod {
  VirtualController = 0,
  DirectEvents = 1
}

@if(ModuleExists("ModSettingsModule"))
public class WootingAnalogSettings extends IScriptable {
  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.displayName", "Enable Wooting Analog")
  @runtimeProperty("ModSettings.description", "Off: your movement keys work like normal keys right away, and the mod stays fully off from the next launch.")
  public let enabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.displayName", "Debug mode")
  @runtimeProperty("ModSettings.description", "Shows live meters of how far you press each key and writes detailed logs. Ctrl+Shift+F10 saves a snapshot to the log.")
  public let debugMode: Bool = false;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.displayName", "Debug meter size")
  @runtimeProperty("ModSettings.description", "Size of the debug meters in the top left corner")
  @runtimeProperty("ModSettings.step", "0.05")
  @runtimeProperty("ModSettings.min", "0.3")
  @runtimeProperty("ModSettings.max", "1.5")
  @runtimeProperty("ModSettings.dependency", "debugMode")
  public let debugMeterSize: Float = 0.6;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.displayName", "Keep keyboard button prompts")
  @runtimeProperty("ModSettings.description", "Stop the prompts switching to Xbox icons while you move with the analog keys")
  public let keepKeyboardPrompts: Bool = true;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.displayName", "Input method")
  @runtimeProperty("ModSettings.description", "Virtual controller is the tested default. Direct engine events is experimental.")
  @runtimeProperty("ModSettings.displayValues.VirtualController", "Virtual controller")
  @runtimeProperty("ModSettings.displayValues.DirectEvents", "Direct engine events (experimental)")
  public let inputMethod: WootingAnalogInputMethod = WootingAnalogInputMethod.VirtualController;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Walking")
  @runtimeProperty("ModSettings.displayName", "Deadzone")
  @runtimeProperty("ModSettings.description", "Key travel ignored at the top of the stroke (0-1)")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "0.5")
  public let movementDeadzone: Float = 0.08;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Walking")
  @runtimeProperty("ModSettings.displayName", "Full press at")
  @runtimeProperty("ModSettings.description", "Key travel that already counts as fully pressed (0-1)")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.3")
  @runtimeProperty("ModSettings.max", "1.0")
  public let movementFullPress: Float = 0.80;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Walking")
  @runtimeProperty("ModSettings.displayName", "Response curve")
  @runtimeProperty("ModSettings.description", "1 = linear, higher = finer control on light presses")
  @runtimeProperty("ModSettings.step", "0.1")
  @runtimeProperty("ModSettings.min", "0.5")
  @runtimeProperty("ModSettings.max", "3.0")
  public let movementCurve: Float = 1.0;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Steering")
  @runtimeProperty("ModSettings.displayName", "Deadzone")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "0.5")
  public let steeringDeadzone: Float = 0.06;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Steering")
  @runtimeProperty("ModSettings.displayName", "Full press at")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.3")
  @runtimeProperty("ModSettings.max", "1.0")
  public let steeringFullPress: Float = 0.90;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Steering")
  @runtimeProperty("ModSettings.displayName", "Response curve")
  @runtimeProperty("ModSettings.description", "Higher keeps small corrections gentle at speed")
  @runtimeProperty("ModSettings.step", "0.1")
  @runtimeProperty("ModSettings.min", "0.5")
  @runtimeProperty("ModSettings.max", "3.0")
  public let steeringCurve: Float = 1.4;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Throttle")
  @runtimeProperty("ModSettings.displayName", "Deadzone")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "0.5")
  public let throttleDeadzone: Float = 0.06;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Throttle")
  @runtimeProperty("ModSettings.displayName", "Full press at")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.3")
  @runtimeProperty("ModSettings.max", "1.0")
  public let throttleFullPress: Float = 0.90;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Throttle")
  @runtimeProperty("ModSettings.displayName", "Response curve")
  @runtimeProperty("ModSettings.step", "0.1")
  @runtimeProperty("ModSettings.min", "0.5")
  @runtimeProperty("ModSettings.max", "3.0")
  public let throttleCurve: Float = 1.0;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Brake")
  @runtimeProperty("ModSettings.displayName", "Deadzone")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "0.5")
  public let brakeDeadzone: Float = 0.06;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Brake")
  @runtimeProperty("ModSettings.displayName", "Full press at")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.3")
  @runtimeProperty("ModSettings.max", "1.0")
  public let brakeFullPress: Float = 0.85;

  @runtimeProperty("ModSettings.mod", "Wooting Analog")
  @runtimeProperty("ModSettings.category", "Brake")
  @runtimeProperty("ModSettings.displayName", "Response curve")
  @runtimeProperty("ModSettings.step", "0.1")
  @runtimeProperty("ModSettings.min", "0.5")
  @runtimeProperty("ModSettings.max", "3.0")
  public let brakeCurve: Float = 1.0;

  private let m_applied: String;

  public func Register() -> Void {
    ModSettings.RegisterListenerToClass(this);
    ModSettings.RegisterListenerToModifications(this);
    this.ApplyIfChanged();
  }

  public cb func OnModSettingsChange() -> Void {
    this.ApplyIfChanged();
  }

  public func ApplyIfChanged() -> Void {
    let values = "enabled " + (this.enabled ? "on" : "off")
      + ", debug " + (this.debugMode ? "on" : "off") + " (meters " + FloatToStringPrec(this.debugMeterSize, 2) + ")"
      + ", keyboard prompts " + (this.keepKeyboardPrompts ? "on" : "off")
      + ", method " + IntToString(EnumInt(this.inputMethod))
      + ", walking " + FloatToStringPrec(this.movementDeadzone, 2) + "/" + FloatToStringPrec(this.movementFullPress, 2) + "/" + FloatToStringPrec(this.movementCurve, 2)
      + ", steering " + FloatToStringPrec(this.steeringDeadzone, 2) + "/" + FloatToStringPrec(this.steeringFullPress, 2) + "/" + FloatToStringPrec(this.steeringCurve, 2)
      + ", throttle " + FloatToStringPrec(this.throttleDeadzone, 2) + "/" + FloatToStringPrec(this.throttleFullPress, 2) + "/" + FloatToStringPrec(this.throttleCurve, 2)
      + ", brake " + FloatToStringPrec(this.brakeDeadzone, 2) + "/" + FloatToStringPrec(this.brakeFullPress, 2) + "/" + FloatToStringPrec(this.brakeCurve, 2);
    if Equals(values, this.m_applied) {
      return;
    }
    this.m_applied = values;
    this.Apply();
    WootingAnalog_Log("Mod Settings applied: " + values + " (deadzone/full press/curve)");
  }

  private func Apply() -> Void {
    WootingAnalog_SetOption("enabled", this.enabled ? 1.0 : 0.0);
    WootingAnalog_SetOption("debug", this.debugMode ? 1.0 : 0.0);
    WootingAnalog_SetOption("meterScale", this.debugMeterSize);
    WootingAnalog_SetOption("keepKeyboardPrompts", this.keepKeyboardPrompts ? 1.0 : 0.0);
    WootingAnalog_SetOption("backend", Cast<Float>(EnumInt(this.inputMethod)));
    WootingAnalog_SetOption("movement.deadzone", this.movementDeadzone);
    WootingAnalog_SetOption("movement.fullpress", this.movementFullPress);
    WootingAnalog_SetOption("movement.curve", this.movementCurve);
    WootingAnalog_SetOption("steering.deadzone", this.steeringDeadzone);
    WootingAnalog_SetOption("steering.fullpress", this.steeringFullPress);
    WootingAnalog_SetOption("steering.curve", this.steeringCurve);
    WootingAnalog_SetOption("throttle.deadzone", this.throttleDeadzone);
    WootingAnalog_SetOption("throttle.fullpress", this.throttleFullPress);
    WootingAnalog_SetOption("throttle.curve", this.throttleCurve);
    WootingAnalog_SetOption("brake.deadzone", this.brakeDeadzone);
    WootingAnalog_SetOption("brake.fullpress", this.brakeFullPress);
    WootingAnalog_SetOption("brake.curve", this.brakeCurve);
  }
}

@if(ModuleExists("ModSettingsModule"))
public func WootingAnalog_CreateSettings() -> ref<IScriptable> {
  let settings = new WootingAnalogSettings();
  settings.Register();
  return settings;
}

@if(!ModuleExists("ModSettingsModule"))
public func WootingAnalog_CreateSettings() -> ref<IScriptable> {
  return null;
}

@if(ModuleExists("ModSettingsModule"))
public func WootingAnalog_RefreshSettings(settings: ref<IScriptable>) -> Void {
  let values = settings as WootingAnalogSettings;
  if IsDefined(values) {
    values.ApplyIfChanged();
  }
}

@if(!ModuleExists("ModSettingsModule"))
public func WootingAnalog_RefreshSettings(settings: ref<IScriptable>) -> Void {}

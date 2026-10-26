enum WootingAnalogContext {
  Unknown = 0,
  OnFoot = 1,
  Driving = 2,
  Tank = 3,
  Neutral = 4
}

public class WootingAnalogTick extends DelayCallback {
  public let system: wref<WootingAnalogSystem>;

  public func Call() -> Void {
    if IsDefined(this.system) {
      this.system.OnTick();
    }
  }
}

public class WootingAnalogMeterTick extends DelayCallback {
  public let system: wref<WootingAnalogSystem>;

  public func Call() -> Void {
    if IsDefined(this.system) {
      this.system.OnMeterTick();
    }
  }
}

public class WootingAnalogSystem extends ScriptableSystem {
  private let m_player: wref<PlayerPuppet>;
  private let m_inMenu: Bool;
  private let m_menuListener: ref<CallbackHandle>;
  private let m_vehicleListener: ref<CallbackHandle>;
  private let m_lastContext: Int32 = 99;
  private let m_tickID: DelayID;
  private let m_settings: ref<IScriptable>;

  private let m_debug: Bool;
  private let m_overlay: ref<IScriptable>;
  private let m_meterTickID: DelayID;
  private let m_meterRunning: Bool;
  private let m_meterAttempts: Int32;
  private let m_lastPosition: Vector4;
  private let m_lastTime: Float;
  private let m_hasPosition: Bool;
  private let m_speed: Float;
  private let m_lastKBM: Bool = true;
  private let m_lastLocomotion: Int32 = 999;
  private let m_debugTicks: Int32;
  private let m_gameplayTicks: Int32;
  private let m_hudTicks: Int32;

  private func OnAttach() -> Void {
    let defs = GetAllBlackboardDefs();
    let ui = GameInstance.GetBlackboardSystem(this.GetGameInstance()).Get(defs.UI_System);
    if IsDefined(ui) {
      this.m_menuListener = ui.RegisterListenerBool(defs.UI_System.IsInMenu, this, n"OnMenuChanged");
      this.m_inMenu = ui.GetBool(defs.UI_System.IsInMenu);
    }
    this.m_settings = WootingAnalog_CreateSettings();
    this.Push();
  }

  private func OnDetach() -> Void {
    let defs = GetAllBlackboardDefs();
    let ui = GameInstance.GetBlackboardSystem(this.GetGameInstance()).Get(defs.UI_System);
    if IsDefined(ui) && IsDefined(this.m_menuListener) {
      ui.UnregisterListenerBool(defs.UI_System.IsInMenu, this.m_menuListener);
    }
    this.m_menuListener = null;
    this.StopTick();
    this.StopMeters();
    WootingAnalog_SetContext(EnumInt(WootingAnalogContext.Neutral));
  }

  private final func OnPlayerAttach(request: ref<PlayerAttachRequest>) -> Void {
    let player = request.owner as PlayerPuppet;
    if !IsDefined(player) {
      return;
    }
    this.m_player = player;
    this.m_hasPosition = false;

    let psm = player.GetPlayerStateMachineBlackboard();
    if IsDefined(psm) {
      this.m_vehicleListener = psm.RegisterListenerInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle, this, n"OnVehicleStateChanged");
    }

    this.m_gameplayTicks = 0;
    this.m_hudTicks = 0;
    this.StartTick();
    this.Push();
  }

  private final func OnPlayerDetach(request: ref<PlayerDetachRequest>) -> Void {
    let player = this.m_player;
    if IsDefined(player) && IsDefined(this.m_vehicleListener) {
      let psm = player.GetPlayerStateMachineBlackboard();
      if IsDefined(psm) {
        psm.UnregisterListenerInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle, this.m_vehicleListener);
      }
    }
    this.m_vehicleListener = null;
    this.m_player = null;
    this.StopTick();
    this.StopMeters();
    this.Push();
  }

  protected cb func OnMenuChanged(value: Bool) -> Bool {
    this.m_inMenu = value;
    this.Push();
    return false;
  }

  protected cb func OnVehicleStateChanged(value: Int32) -> Bool {
    this.Push();
    return false;
  }

  public func OnTick() -> Void {
    WootingAnalog_RefreshSettings(this.m_settings);
    this.Push();

    let context = this.m_lastContext;
    if context == EnumInt(WootingAnalogContext.OnFoot) || context == EnumInt(WootingAnalogContext.Driving) || context == EnumInt(WootingAnalogContext.Tank) {
      if this.m_gameplayTicks < 1000 {
        this.m_gameplayTicks += 1;
      }
      if WootingAnalog_HudReady(Cast<Float>(this.m_gameplayTicks) * 0.25) {
        if this.m_hudTicks < 12 {
          this.m_hudTicks += 1;
        } else {
          this.Announce();
        }
      }
    }

    this.UpdateDebug();
    this.StartTick();
  }

  private func StartTick() -> Void {
    let tick = new WootingAnalogTick();
    tick.system = this;
    this.m_tickID = GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(tick, 0.25, false);
  }

  private func StopTick() -> Void {
    GameInstance.GetDelaySystem(this.GetGameInstance()).CancelCallback(this.m_tickID);
  }

  private func Push() -> Void {
    let context = this.ComputeContext();
    if context != this.m_lastContext {
      if this.m_debug {
        WootingAnalog_Log("context " + this.ContextName(context) + " (vehicle state " + IntToString(this.VehicleState()) + ", in menu " + (this.m_inMenu ? "yes" : "no") + ")");
      }
      this.m_lastContext = context;
      WootingAnalog_SetContext(context);
    }
  }

  private func VehicleState() -> Int32 {
    let player = this.m_player;
    if !IsDefined(player) {
      return -1;
    }
    let psm = player.GetPlayerStateMachineBlackboard();
    if !IsDefined(psm) {
      return -1;
    }
    return psm.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle);
  }

  private func ComputeContext() -> Int32 {
    if this.m_inMenu {
      return EnumInt(WootingAnalogContext.Neutral);
    }
    let player = this.m_player;
    if !IsDefined(player) {
      return EnumInt(WootingAnalogContext.Neutral);
    }
    let psm = player.GetPlayerStateMachineBlackboard();
    if !IsDefined(psm) {
      return EnumInt(WootingAnalogContext.OnFoot);
    }

    let defs = GetAllBlackboardDefs();
    let vehicleState = psm.GetInt(defs.PlayerStateMachine.Vehicle);
    let game = player.GetGame();

    if vehicleState == EnumInt(gamePSMVehicle.Driving) || vehicleState == EnumInt(gamePSMVehicle.DriverCombat) {
      let vehicle: wref<VehicleObject>;
      if VehicleComponent.GetVehicle(game, player.GetEntityID(), vehicle) && IsDefined(vehicle as TankObject) {
        return EnumInt(WootingAnalogContext.Tank);
      }
      return EnumInt(WootingAnalogContext.Driving);
    }

    if vehicleState == EnumInt(gamePSMVehicle.Default) {
      let remoteID = psm.GetEntityID(defs.PlayerStateMachine.EntityIDVehicleRemoteControlled);
      let remote: wref<VehicleObject>;
      if EntityID.IsDefined(remoteID) && VehicleComponent.GetVehicleFromID(game, remoteID, remote) && remote.IsVehicleRemoteControlled() {
        return EnumInt(WootingAnalogContext.Driving);
      }
      return EnumInt(WootingAnalogContext.OnFoot);
    }

    return EnumInt(WootingAnalogContext.Neutral);
  }

  public func ContextName(context: Int32) -> String {
    switch context {
      case 1: return "on foot";
      case 2: return "driving";
      case 3: return "tank";
      case 4: return "menu / passenger";
      default: return "unknown";
    }
  }

  private func Announce() -> Void {
    let text = WootingAnalog_TakeAnnouncement();
    if StrLen(text) == 0 {
      return;
    }
    let message: SimpleScreenMessage;
    message.isShown = true;
    message.duration = 6.0;
    message.message = text;
    let defs = GetAllBlackboardDefs();
    GameInstance.GetBlackboardSystem(this.GetGameInstance()).Get(defs.UI_Notifications).SetVariant(defs.UI_Notifications.WarningMessage, ToVariant(message), true);
  }

  public func Speed() -> Float {
    return this.m_speed;
  }

  public func LocomotionName() -> String {
    if this.m_lastLocomotion == 999 {
      return "?";
    }
    return EnumValueToString("gamePSMDetailedLocomotionStates", Cast<Int64>(this.m_lastLocomotion));
  }

  public func LastUsedKeyboard() -> Bool {
    return this.m_lastKBM;
  }

  private func UpdateDebug() -> Void {
    let debug = WootingAnalog_IsDebug();
    if debug && !this.m_debug || !debug && this.m_debug {
      this.m_debug = debug;
      if debug {
        WootingAnalog_Log("debug mode on: speed, locomotion and prompt device are logged here");
        this.StartMeters();
      } else {
        this.StopMeters();
      }
    }
    if !debug {
      return;
    }

    let player = this.m_player;
    if !IsDefined(player) {
      return;
    }

    let now = EngineTime.ToFloat(GameInstance.GetSimTime(player.GetGame()));
    let position = player.GetWorldPosition();
    if this.m_hasPosition && now > this.m_lastTime {
      let dx = position.X - this.m_lastPosition.X;
      let dy = position.Y - this.m_lastPosition.Y;
      this.m_speed = SqrtF(dx * dx + dy * dy) / (now - this.m_lastTime);
    }
    this.m_lastPosition = position;
    this.m_lastTime = now;
    this.m_hasPosition = true;

    let kbm = player.PlayerLastUsedKBM();
    if kbm && !this.m_lastKBM || !kbm && this.m_lastKBM {
      this.m_lastKBM = kbm;
      WootingAnalog_Log("prompts switched to " + (kbm ? "KEYBOARD" : "CONTROLLER") + " (speed " + FloatToStringPrec(this.m_speed, 2) + " m/s)");
    }

    let psm = player.GetPlayerStateMachineBlackboard();
    if IsDefined(psm) {
      let locomotion = psm.GetInt(GetAllBlackboardDefs().PlayerStateMachine.LocomotionDetailed);
      if locomotion != this.m_lastLocomotion {
        this.m_lastLocomotion = locomotion;
        WootingAnalog_Log("locomotion " + this.LocomotionName() + " at " + FloatToStringPrec(this.m_speed, 2) + " m/s");
      }
    }

    this.m_debugTicks += 1;
    if this.m_debugTicks >= 4 && this.m_speed > 0.05 {
      this.m_debugTicks = 0;
      let sent: String;
      if this.m_lastContext == EnumInt(WootingAnalogContext.Driving) {
        sent = "throttle " + FloatToStringPrec(WootingAnalog_Value(WootingAnalogValue.RightTrigger), 2) + " brake " + FloatToStringPrec(WootingAnalog_Value(WootingAnalogValue.LeftTrigger), 2) + " steer " + FloatToStringPrec(WootingAnalog_Value(WootingAnalogValue.StickX), 2);
      } else {
        sent = this.LocomotionName() + ", stick x " + FloatToStringPrec(WootingAnalog_Value(WootingAnalogValue.StickX), 2) + " y " + FloatToStringPrec(WootingAnalog_Value(WootingAnalogValue.StickY), 2);
      }
      WootingAnalog_Log("moving " + FloatToStringPrec(this.m_speed, 2) + " m/s, " + this.ContextName(this.m_lastContext) + ", sent " + sent + ", prompts " + (kbm ? "keyboard" : "controller"));
    }

    if WootingAnalog_TakeSnapshotRequest() {
      WootingAnalog_Log("snapshot: speed " + FloatToStringPrec(this.m_speed, 2) + " m/s, locomotion " + this.LocomotionName() + ", vehicle state " + IntToString(this.VehicleState()) + ", context " + this.ContextName(this.m_lastContext) + ", in menu " + (this.m_inMenu ? "yes" : "no") + ", prompts " + (kbm ? "keyboard" : "controller"));
    }
  }

  private func StartMeters() -> Void {
    if this.m_meterRunning {
      return;
    }
    this.m_meterRunning = true;
    this.OnMeterTick();
  }

  private func StopMeters() -> Void {
    if this.m_meterRunning {
      GameInstance.GetDelaySystem(this.GetGameInstance()).CancelCallback(this.m_meterTickID);
      this.m_meterRunning = false;
    }
    this.m_overlay = WootingAnalog_UpdateOverlay(this.m_overlay, this, false);
  }

  public func OnMeterTick() -> Void {
    if !this.m_meterRunning {
      return;
    }
    this.m_overlay = WootingAnalog_UpdateOverlay(this.m_overlay, this, true);
    if !IsDefined(this.m_overlay) {
      this.m_meterAttempts += 1;
      if this.m_meterAttempts == 60 {
        WootingAnalog_Log("on-screen meters could not attach to the HUD yet (still trying)");
      }
    }
    let tick = new WootingAnalogMeterTick();
    tick.system = this;
    this.m_meterTickID = GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(tick, 0.03, false);
  }
}

@wrapMethod(SingleplayerMenuGameController)
protected cb func OnInitialize() -> Bool {
  WootingAnalog_SetContext(EnumInt(WootingAnalogContext.Neutral));
  return wrappedMethod();
}

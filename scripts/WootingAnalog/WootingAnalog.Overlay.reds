@if(ModuleExists("Codeware"))
public class WootingAnalogMeter extends IScriptable {
  public let root: ref<inkCanvas>;
  public let fill: ref<inkRectangle>;
  public let markerA: ref<inkRectangle>;
  public let markerB: ref<inkRectangle>;
  public let value: ref<inkText>;
  public let signed: Bool;

  public static func BarWidth() -> Float {
    return 520.0;
  }

  public static func Make(parent: ref<inkCompoundWidget>, label: String, top: Float, signed: Bool) -> ref<WootingAnalogMeter> {
    let meter = new WootingAnalogMeter();
    meter.signed = signed;
    let width = WootingAnalogMeter.BarWidth();

    let row = new inkCanvas();
    row.SetAnchor(inkEAnchor.TopLeft);
    row.SetMargin(inkMargin(32.0, top, 0.0, 0.0));
    row.SetSize(Vector2(900.0, 40.0));
    row.Reparent(parent);
    meter.root = row;

    let text = WootingAnalogMeter.Text(label, 28);
    text.SetMargin(inkMargin(0.0, 2.0, 0.0, 0.0));
    text.Reparent(row);

    let back = new inkRectangle();
    back.SetAnchor(inkEAnchor.TopLeft);
    back.SetMargin(inkMargin(230.0, 6.0, 0.0, 0.0));
    back.SetSize(Vector2(width, 26.0));
    back.SetTintColor(HDRColor(0.10, 0.12, 0.16, 1.0));
    back.SetOpacity(0.95);
    back.Reparent(row);

    let fill = new inkRectangle();
    fill.SetAnchor(inkEAnchor.TopLeft);
    fill.SetMargin(inkMargin(230.0, 6.0, 0.0, 0.0));
    fill.SetSize(Vector2(0.0, 26.0));
    fill.Reparent(row);
    meter.fill = fill;

    meter.markerA = WootingAnalogMeter.Marker(row);
    meter.markerB = WootingAnalogMeter.Marker(row);
    if signed {
      let centre = WootingAnalogMeter.Marker(row);
      centre.SetMargin(inkMargin(230.0 + width * 0.5 - 1.0, 2.0, 0.0, 0.0));
      centre.SetTintColor(HDRColor(0.6, 0.6, 0.65, 1.0));
    }

    let value = WootingAnalogMeter.Text("0.00", 28);
    value.SetMargin(inkMargin(230.0 + width + 18.0, 2.0, 0.0, 0.0));
    value.Reparent(row);
    meter.value = value;
    return meter;
  }

  public static func Text(content: String, size: Int32) -> ref<inkText> {
    let text = new inkText();
    text.SetAnchor(inkEAnchor.TopLeft);
    text.SetFontFamily("base\\gameplay\\gui\\fonts\\raj\\raj.inkfontfamily");
    text.SetFontStyle(n"Medium");
    text.SetFontSize(size);
    text.SetTintColor(HDRColor(0.90, 0.95, 1.00, 1.0));
    text.SetText(content);
    return text;
  }

  public static func Marker(parent: ref<inkCompoundWidget>) -> ref<inkRectangle> {
    let marker = new inkRectangle();
    marker.SetAnchor(inkEAnchor.TopLeft);
    marker.SetSize(Vector2(3.0, 34.0));
    marker.SetVisible(false);
    marker.Reparent(parent);
    return marker;
  }

  private func PlaceMarker(marker: ref<inkRectangle>, at: Float, color: HDRColor) -> Void {
    let width = WootingAnalogMeter.BarWidth();
    let x = this.signed ? width * 0.5 + at * width * 0.5 : at * width;
    marker.SetMargin(inkMargin(230.0 + x - 1.5, 2.0, 0.0, 0.0));
    marker.SetTintColor(color);
    marker.SetVisible(true);
  }

  public func ShowTravel(travel: Float, deadzone: Float, full: Float) -> Void {
    let width = WootingAnalogMeter.BarWidth();
    let clamped = ClampF(travel, 0.0, 1.0);
    this.fill.SetMargin(inkMargin(230.0, 6.0, 0.0, 0.0));
    this.fill.SetSize(Vector2(clamped * width, 26.0));
    if clamped <= deadzone {
      this.fill.SetTintColor(HDRColor(0.45, 0.45, 0.50, 1.0));
    } else {
      if clamped >= full {
        this.fill.SetTintColor(HDRColor(0.30, 1.00, 0.45, 1.0));
      } else {
        this.fill.SetTintColor(HDRColor(1.00, 0.82, 0.20, 1.0));
      }
    }
    this.PlaceMarker(this.markerA, deadzone, HDRColor(0.65, 0.65, 0.70, 1.0));
    this.PlaceMarker(this.markerB, full, HDRColor(1.0, 1.0, 1.0, 1.0));
    this.value.SetText(FloatToStringPrec(clamped, 2));
  }

  public func ShowOutput(output: Float, sprintLine: Bool) -> Void {
    let width = WootingAnalogMeter.BarWidth();
    let color = AbsF(output) > 0.9 ? HDRColor(0.30, 1.00, 0.45, 1.0) : HDRColor(0.37, 0.96, 1.00, 1.0);
    if this.signed {
      let clamped = ClampF(output, -1.0, 1.0);
      let half = width * 0.5;
      let length = AbsF(clamped) * half;
      let left = clamped >= 0.0 ? half : half - length;
      this.fill.SetMargin(inkMargin(230.0 + left, 6.0, 0.0, 0.0));
      this.fill.SetSize(Vector2(length, 26.0));
    } else {
      this.fill.SetMargin(inkMargin(230.0, 6.0, 0.0, 0.0));
      this.fill.SetSize(Vector2(ClampF(output, 0.0, 1.0) * width, 26.0));
    }
    this.fill.SetTintColor(color);
    if sprintLine {
      this.PlaceMarker(this.markerA, 0.9, HDRColor(1.0, 0.35, 0.35, 1.0));
      this.PlaceMarker(this.markerB, -0.9, HDRColor(1.0, 0.35, 0.35, 1.0));
    }
    this.value.SetText(FloatToStringPrec(output, 2));
  }
}

@if(ModuleExists("Codeware"))
public class WootingAnalogOverlay extends IScriptable {
  private let m_root: ref<inkCanvas>;
  private let m_title: ref<inkText>;
  private let m_info: ref<inkText>;
  private let m_game: ref<inkText>;
  private let m_forward: ref<WootingAnalogMeter>;
  private let m_back: ref<WootingAnalogMeter>;
  private let m_left: ref<WootingAnalogMeter>;
  private let m_right: ref<WootingAnalogMeter>;
  private let m_stickX: ref<WootingAnalogMeter>;
  private let m_stickY: ref<WootingAnalogMeter>;
  private let m_throttle: ref<WootingAnalogMeter>;
  private let m_brake: ref<WootingAnalogMeter>;
  private let m_scale: Float;

  public func Build() -> Bool {
    let window = GameInstance.GetInkSystem().GetLayer(n"inkHUDLayer").GetVirtualWindow();
    if !IsDefined(window) {
      return false;
    }

    let root = new inkCanvas();
    root.SetName(n"WootingAnalogDebug");
    root.SetAnchor(inkEAnchor.TopLeft);
    root.SetMargin(inkMargin(72.0, 320.0, 0.0, 0.0));
    root.SetSize(Vector2(980.0, 640.0));
    root.SetRenderTransformPivot(Vector2(0.0, 0.0));
    this.m_root = root;
    this.ApplyScale();

    let background = new inkRectangle();
    background.SetAnchor(inkEAnchor.TopLeft);
    background.SetSize(Vector2(980.0, 640.0));
    background.SetTintColor(HDRColor(0.0, 0.0, 0.0, 1.0));
    background.SetOpacity(0.6);
    background.Reparent(root);

    this.m_title = WootingAnalogMeter.Text("WOOTING ANALOG  |  DEBUG", 34);
    this.m_title.SetTintColor(HDRColor(0.37, 0.96, 1.00, 1.0));
    this.m_title.SetMargin(inkMargin(32.0, 18.0, 0.0, 0.0));
    this.m_title.Reparent(root);

    this.m_info = WootingAnalogMeter.Text("", 26);
    this.m_info.SetMargin(inkMargin(32.0, 64.0, 0.0, 0.0));
    this.m_info.Reparent(root);

    let top = 112.0;
    let step = 44.0;
    this.m_forward = WootingAnalogMeter.Make(root, "Forward key", top, false);
    this.m_back = WootingAnalogMeter.Make(root, "Back key", top + step, false);
    this.m_left = WootingAnalogMeter.Make(root, "Left key", top + step * 2.0, false);
    this.m_right = WootingAnalogMeter.Make(root, "Right key", top + step * 3.0, false);
    this.m_stickY = WootingAnalogMeter.Make(root, "Sent: stick Y", top + step * 4.0 + 16.0, true);
    this.m_stickX = WootingAnalogMeter.Make(root, "Sent: stick X", top + step * 5.0 + 16.0, true);
    this.m_throttle = WootingAnalogMeter.Make(root, "Sent: throttle", top + step * 6.0 + 16.0, false);
    this.m_brake = WootingAnalogMeter.Make(root, "Sent: brake", top + step * 7.0 + 16.0, false);

    this.m_game = WootingAnalogMeter.Text("", 26);
    this.m_game.SetMargin(inkMargin(32.0, top + step * 8.0 + 34.0, 0.0, 0.0));
    this.m_game.Reparent(root);

    root.Reparent(window);
    return true;
  }

  public func SetVisible(visible: Bool) -> Void {
    if IsDefined(this.m_root) {
      this.m_root.SetVisible(visible);
    }
  }

  private func ApplyScale() -> Void {
    let scale = WootingAnalog_Value(WootingAnalogValue.MeterScale);
    if scale <= 0.0 {
      scale = 0.6;
    }
    if AbsF(scale - this.m_scale) > 0.001 {
      this.m_scale = scale;
      this.m_root.SetScale(Vector2(scale, scale));
    }
  }

  public func Update(system: ref<WootingAnalogSystem>) -> Void {
    this.ApplyScale();
    let context = Cast<Int32>(WootingAnalog_Value(WootingAnalogValue.Context));
    let driving = context == 2 || context == 3;

    let fwdDz = driving ? WootingAnalog_Value(WootingAnalogValue.ThrottleDeadzone) : WootingAnalog_Value(WootingAnalogValue.MoveDeadzone);
    let fwdFull = driving ? WootingAnalog_Value(WootingAnalogValue.ThrottleFullPress) : WootingAnalog_Value(WootingAnalogValue.MoveFullPress);
    let backDz = driving ? WootingAnalog_Value(WootingAnalogValue.BrakeDeadzone) : WootingAnalog_Value(WootingAnalogValue.MoveDeadzone);
    let backFull = driving ? WootingAnalog_Value(WootingAnalogValue.BrakeFullPress) : WootingAnalog_Value(WootingAnalogValue.MoveFullPress);
    let sideDz = driving ? WootingAnalog_Value(WootingAnalogValue.SteerDeadzone) : WootingAnalog_Value(WootingAnalogValue.MoveDeadzone);
    let sideFull = driving ? WootingAnalog_Value(WootingAnalogValue.SteerFullPress) : WootingAnalog_Value(WootingAnalogValue.MoveFullPress);

    let forward = driving ? WootingAnalog_Value(WootingAnalogValue.Accelerate) : WootingAnalog_Value(WootingAnalogValue.Forward);
    let back = driving ? WootingAnalog_Value(WootingAnalogValue.Brake) : WootingAnalog_Value(WootingAnalogValue.Back);
    let left = driving ? WootingAnalog_Value(WootingAnalogValue.SteerLeft) : WootingAnalog_Value(WootingAnalogValue.Left);
    let right = driving ? WootingAnalog_Value(WootingAnalogValue.SteerRight) : WootingAnalog_Value(WootingAnalogValue.Right);

    this.m_forward.ShowTravel(forward, fwdDz, fwdFull);
    this.m_back.ShowTravel(back, backDz, backFull);
    this.m_left.ShowTravel(left, sideDz, sideFull);
    this.m_right.ShowTravel(right, sideDz, sideFull);
    this.m_stickY.ShowOutput(WootingAnalog_Value(WootingAnalogValue.StickY), !driving);
    this.m_stickX.ShowOutput(WootingAnalog_Value(WootingAnalogValue.StickX), false);
    this.m_throttle.ShowOutput(WootingAnalog_Value(WootingAnalogValue.RightTrigger), false);
    this.m_brake.ShowOutput(WootingAnalog_Value(WootingAnalogValue.LeftTrigger), false);

    let method = WootingAnalog_Value(WootingAnalogValue.Backend) > 0.5 ? "direct events" : "virtual controller";
    if WootingAnalog_Value(WootingAnalogValue.KeepKeyboardPrompts) > 0.5 {
      method += " + keyboard prompts";
    }
    let state = WootingAnalog_Value(WootingAnalogValue.Enabled) < 0.5 ? "SWITCHED OFF" : (WootingAnalog_Value(WootingAnalogValue.DeviceConnected) > 0.5 ? "keyboard OK" : "KEYBOARD MISSING");
    let throttle = WootingAnalog_Value(WootingAnalogValue.AnalogThrottle) > 0.5 ? "analog throttle" : "digital throttle";
    this.m_info.SetText(system.ContextName(context) + "  |  " + method + "  |  " + state + "  |  " + throttle);

    let prompts = system.LastUsedKeyboard() ? "keyboard" : "CONTROLLER";
    let moving = driving ? "in vehicle" : system.LocomotionName();
    this.m_game.SetText("Game: " + FloatToStringPrec(system.Speed(), 1) + " m/s, " + moving + ", prompts " + prompts + "   (Ctrl+Shift+F10: snapshot)");
  }
}

@if(ModuleExists("Codeware"))
public func WootingAnalog_UpdateOverlay(overlay: ref<IScriptable>, system: ref<WootingAnalogSystem>, show: Bool) -> ref<IScriptable> {
  let meters = overlay as WootingAnalogOverlay;
  if !show {
    if IsDefined(meters) {
      meters.SetVisible(false);
    }
    return overlay;
  }
  if !IsDefined(meters) {
    meters = new WootingAnalogOverlay();
    if !meters.Build() {
      return null;
    }
    WootingAnalog_Log("on-screen meters shown");
  }
  meters.SetVisible(true);
  meters.Update(system);
  return meters;
}

@if(ModuleExists("Codeware"))
public func WootingAnalog_HudReady(gameplaySeconds: Float) -> Bool {
  return gameplaySeconds >= 1.0 && IsDefined(GameInstance.GetInkSystem().GetLayer(n"inkHUDLayer").GetVirtualWindow());
}

@if(!ModuleExists("Codeware"))
public func WootingAnalog_HudReady(gameplaySeconds: Float) -> Bool {
  return gameplaySeconds >= 4.0;
}

public class WootingAnalogNoOverlay extends IScriptable {}

@if(!ModuleExists("Codeware"))
public func WootingAnalog_UpdateOverlay(overlay: ref<IScriptable>, system: ref<WootingAnalogSystem>, show: Bool) -> ref<IScriptable> {
  if show && !IsDefined(overlay) {
    WootingAnalog_Log("on-screen meters need Codeware; debug mode is only writing logs");
    return new WootingAnalogNoOverlay();
  }
  return overlay;
}

public static native func WootingAnalog_SetContext(context: Int32) -> Void
public static native func WootingAnalog_SetOption(name: String, value: Float) -> Void
public static native func WootingAnalog_GetStatus() -> String
public static native func WootingAnalog_GetDeviceName() -> String
public static native func WootingAnalog_GetVersion() -> String
public static native func WootingAnalog_IsActive() -> Bool
public static native func WootingAnalog_TakeAnnouncement() -> String
public static native func WootingAnalog_IsDebug() -> Bool
public static native func WootingAnalog_Log(message: String) -> Void
public static native func WootingAnalog_TakeSnapshotRequest() -> Bool
public static native func WootingAnalog_GetValue(index: Int32) -> Float

enum WootingAnalogValue {
  Forward = 0,
  Back = 1,
  Left = 2,
  Right = 3,
  Accelerate = 4,
  Brake = 5,
  SteerLeft = 6,
  SteerRight = 7,
  StickX = 8,
  StickY = 9,
  LeftTrigger = 10,
  RightTrigger = 11,
  Context = 12,
  Backend = 13,
  KeepKeyboardPrompts = 14,
  Active = 15,
  DeviceConnected = 16,
  AnalogThrottle = 17,
  Enabled = 18,
  MeterScale = 19,
  MoveDeadzone = 20,
  MoveFullPress = 21,
  MoveCurve = 22,
  SteerDeadzone = 23,
  SteerFullPress = 24,
  SteerCurve = 25,
  ThrottleDeadzone = 26,
  ThrottleFullPress = 27,
  ThrottleCurve = 28,
  BrakeDeadzone = 29,
  BrakeFullPress = 30,
  BrakeCurve = 31
}

public func WootingAnalog_Value(value: WootingAnalogValue) -> Float {
  return WootingAnalog_GetValue(EnumInt(value));
}

# -*- coding: utf-8 -*-
"""
이동/회전 입력을 VRSmoothLocomotionSubsystem에 맞게 맞춘다.
- IA_Move / IA_Turn의 데드존 모디파이어를 뺀다 (템플릿 기본 0.7 / 0.8은 스틱을 거의 끝까지 밀어야 반응함).
  데드존은 VRSmoothLocomotionSubsystem의 DeadZone / TurnDeadZone이 처리한다.
  (데드존 값은 EditInstanceOnly라 파이썬으로 바꿀 수 없어서 모디파이어를 빼는 방식으로 함)
- 왼쪽 스틱 좌/우 = 옆걸음(IA_Strafe)을 만들고 IMC_Default에 연결한다 → 왼쪽 스틱으로 대각선 이동
스틱 좌우 배치(이동 = 왼쪽, 회전 = 오른쪽)는 swap_locomotion_sticks.py가 맡는다.
OpenXR은 에디터를 켤 때 입력 매핑을 읽으므로, 실행 후 에디터를 다시 켜야 VR 프리뷰에 반영된다.
열린 에디터에서 원격 실행으로 보내도 되고, 커맨드렛으로 돌려도 된다. 여러 번 실행해도 결과는 같다.
"""
import unreal

ACTIONS_DIR = "/Game/VRTemplate/Input/Actions"
STRAFE_DIR = "/Game/VRPuzzle/Input"
STRAFE_NAME = "IA_Strafe"
IMC_PATH = "/Game/VRTemplate/Input/IMC_Default"
STRAFE_KEYS = [
    "OculusTouch_Left_Thumbstick_X",
    "ValveIndex_Left_Thumbstick_X",
    "MixedReality_Left_Thumbstick_X",
    "Vive_Left_Trackpad_X",
]

eal = unreal.EditorAssetLibrary


def remove_dead_zone(action):
    mods = list(action.get_editor_property("modifiers"))
    kept = [m for m in mods if not isinstance(m, unreal.InputModifierDeadZone)]
    if len(kept) != len(mods):
        action.set_editor_property("modifiers", kept)
        eal.save_loaded_asset(action, only_if_is_dirty=False)
        unreal.log("{}: 데드존 모디파이어 {}개 뺌".format(action.get_name(), len(mods) - len(kept)))
    else:
        unreal.log("{}: 데드존 모디파이어 없음 (그대로)".format(action.get_name()))


# 1) 이동/회전 데드존 모디파이어 제거
for name in ["IA_Move", "IA_Turn"]:
    remove_dead_zone(unreal.load_asset(ACTIONS_DIR + "/" + name))

# 2) 옆걸음 액션
strafe_path = STRAFE_DIR + "/" + STRAFE_NAME
if eal.does_asset_exist(strafe_path):
    strafe = unreal.load_asset(strafe_path)
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    strafe = tools.create_asset(STRAFE_NAME, STRAFE_DIR, unreal.InputAction, unreal.InputAction_Factory())
    strafe.set_editor_property("value_type", unreal.InputActionValueType.AXIS1D)
    strafe.set_editor_property("action_description", "왼쪽 스틱 좌/우 옆걸음 (VRSmoothLocomotionSubsystem이 읽음)")
    eal.save_loaded_asset(strafe, only_if_is_dirty=False)
    unreal.log("{} 만들었습니다".format(strafe_path))

# 3) IMC_Default에 왼쪽 스틱 좌/우 -> IA_Strafe 연결 (이미 있으면 건너뜀)
imc = unreal.load_asset(IMC_PATH)
existing = set()
for m in imc.get_editor_property("mappings"):
    act = m.get_editor_property("action")
    if act and act.get_path_name() == strafe.get_path_name():
        existing.add(str(m.get_editor_property("key").get_editor_property("key_name")))
added = 0
for key_name in STRAFE_KEYS:
    if key_name in existing:
        continue
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    imc.map_key(strafe, key)
    added += 1
    unreal.log("IMC_Default: {} -> {}".format(key_name, STRAFE_NAME))
if added:
    eal.save_loaded_asset(imc, only_if_is_dirty=False)
unreal.log("IMC_Default: 옆걸음 매핑 {}개 추가".format(added))

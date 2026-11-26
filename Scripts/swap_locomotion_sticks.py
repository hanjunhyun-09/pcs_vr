# -*- coding: utf-8 -*-
"""
VR 템플릿 IMC_Default의 이동/회전 조이스틱을 일반적인 VR 배치로 바꾼다.
- 이동 (IA_Move): 오른쪽 스틱 위/아래  -> 왼쪽 스틱 위/아래
- 회전 (IA_Turn): 왼쪽 스틱 좌/우     -> 오른쪽 스틱 좌/우
(Vive 트랙패드도 같은 방식으로 좌/우를 바꾼다)
OpenXR은 에디터를 켤 때 입력 매핑을 읽으므로, 실행 후 에디터를 다시 켜야 VR 프리뷰에 반영된다.
열린 에디터에서 원격 실행으로 보내도 되고, 커맨드렛으로 돌려도 된다. 여러 번 실행해도 결과는 같다.
"""
import unreal

IMC_PATH = "/Game/VRTemplate/Input/IMC_Default"

# 액션별: (바꿀 키 이름의 앞부분 -> 새 앞부분), 키 이름 끝(축)
SWAPS = {
    "IA_Move": ("_Right_", "_Left_", ("Thumbstick_Y", "Trackpad_Y")),
    "IA_Turn": ("_Left_", "_Right_", ("Thumbstick_X", "Trackpad_X")),
}

imc = unreal.load_asset(IMC_PATH)
mappings = imc.get_editor_property("mappings")
changed = 0
for i in range(len(mappings)):
    m = mappings[i]  # 구조체 복사본이므로 고친 뒤 mappings[i]에 다시 넣는다
    act = m.get_editor_property("action")
    if not act or act.get_name() not in SWAPS:
        continue
    old_side, new_side, suffixes = SWAPS[act.get_name()]
    key_name = str(m.get_editor_property("key").get_editor_property("key_name"))
    if old_side not in key_name or not key_name.endswith(suffixes):
        continue
    new_name = key_name.replace(old_side, new_side, 1)
    new_key = unreal.Key()
    new_key.set_editor_property("key_name", new_name)
    m.set_editor_property("key", new_key)
    mappings[i] = m
    unreal.log("{}: {} -> {}".format(act.get_name(), key_name, new_name))
    changed += 1

if changed:
    imc.set_editor_property("mappings", mappings)
    unreal.EditorAssetLibrary.save_loaded_asset(imc, only_if_is_dirty=False)
unreal.log("IMC_Default: {}개 매핑을 바꿨습니다".format(changed))

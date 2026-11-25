# -*- coding: utf-8 -*-
"""
L_06_Ending: 엔딩 크레딧 글자만 build_l06_ending.py 의 CREDIT_PAGES 와 똑같이 맞춘다 (맵 전체를 다시 만들지 않음).
크레딧 쪽(pages가 있는 장면)을 찾아 바꾸므로 카메라·연출은 그대로 둔다. 여러 번 실행해도 결과는 같다.

  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="<프로젝트 절대 경로>/Scripts/patch_l06_credits.py"
"""
import unreal

MAP = "/Game/VRPuzzle/Maps/L_06_Ending"
CREDIT_PAGES = [
    "착시 측정 연구소\n\n컴과고 VR 프로젝트",
    "기획 · 개발\n\n한준현",
    "등장인물\n\n강한결\n오세진\n오나연\n권혁수",
    "만든 도구\n\nUnreal Engine 5\nVR Template\nOpenXR",
    "플레이해 주셔서 감사합니다",
]

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(MAP):
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)

done = 0
for a in eas.get_all_level_actors():
    if not isinstance(a, unreal.EndingCutscene):
        continue
    shots = list(a.get_editor_property("shots"))
    for s in shots:
        if len(s.get_editor_property("pages")) > 0:
            s.set_editor_property("pages", [unreal.Text(t) for t in CREDIT_PAGES])
            done += 1
    a.set_editor_property("shots", shots)

if done == 0:
    raise RuntimeError("크레딧 장면(pages)을 찾지 못함")
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log("[patch_l06_credits] 크레딧 %d곳 수정" % done)

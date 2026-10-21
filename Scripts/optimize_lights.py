# -*- coding: utf-8 -*-
"""
VR 프레임 최적화: 맵마다 그림자를 만드는 조명을 가장 센 2개만 남기고 나머지는 그림자를 끈다.
Movable 포인트 라이트의 그림자는 6방향을 따로 그려야 해서 VR(두 눈)에서 가장 비싼 작업 중 하나.
작은 보조 조명(문 위, 책상 스탠드, 창문 빛 등)은 그림자가 없어도 차이가 거의 없다.

맵 빌드 스크립트(build_l0X_*.py)를 다시 돌린 뒤에는 이 스크립트도 다시 돌린다. 여러 번 실행해도 결과는 같다.
실행 (에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/optimize_lights.py"
"""
import unreal

MAPS = ["L_01_Isolation", "L_02_Perspective", "L_03_Observation", "L_04_Device"]
KEEP = 2

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log_warning("[Lights] " + msg)


for m in MAPS:
    path = "/Game/VRPuzzle/Maps/" + m
    unreal.EditorLoadingAndSavingUtils.load_map(path)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    lights = []
    for a in eas.get_all_level_actors():
        if isinstance(a, (unreal.PointLight, unreal.SpotLight)):
            lc = a.get_component_by_class(unreal.LocalLightComponent)
            if lc and lc.get_editor_property("cast_shadows"):
                # 세기 x 범위로 "얼마나 넓게 밝히는지" 어림
                score = lc.get_editor_property("intensity") * lc.get_editor_property("attenuation_radius")
                lights.append((score, a.get_actor_label(), lc))
    lights.sort(key=lambda t: -t[0])
    kept = [label for _, label, _ in lights[:KEEP]]
    for _, label, lc in lights[KEEP:]:
        lc.set_editor_property("cast_shadows", False)
    unreal.EditorLoadingAndSavingUtils.save_map(world, path)
    log("%s: 그림자 조명 %d개 → %d개 (남김: %s)" % (m, len(lights), min(len(lights), KEEP), ", ".join(kept)))
log("완료")

# -*- coding: utf-8 -*-
"""
L_04_Device: 중앙 코어행 엘리베이터가 L_05_Core로 출발하게 고친다 (맵 전체를 다시 만들지 않음 → dress_grime.py 잡동사니 유지).
build_l04_device.py의 같은 부분과 똑같이 맞춘다. 열린 에디터에서 원격 실행으로 보내도 되고, 커맨드렛으로 돌려도 된다.
여러 번 실행해도 결과는 같다.
"""
import unreal

MAP = "/Game/VRPuzzle/Maps/L_04_Device"
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(MAP):
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}

elevator = actors["S4_Elevator"]
elevator.set_editor_property("next_level", "/Game/VRPuzzle/Maps/L_05_Core")
elevator.set_editor_property("out_of_service", False)

sign = actors.get("S4_Sign_Elevator")
if sign:
    sign.set_editor_property("text", unreal.Text("5. 중앙 코어\n(엘리베이터)"))

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
unreal.log_warning("[L04] 중앙 코어행 엘리베이터 → L_05 연결 완료")

# -*- coding: utf-8 -*-
"""
L_00_Intro: 택배 상자 뒤쪽에 오세진의 쪽지(노션 "대사 초안 > 0. 인트로: 택배 속 쪽지")를 세운다 (맵 전체를 다시 만들지 않음).
플레이어는 상자 앞(-X)에서 +X를 보고 시작하므로, 쪽지 앞면이 -X를 보게 둔다.
열린 에디터에서 원격 실행으로 보내도 되고, 커맨드렛으로 돌려도 된다. 여러 번 실행해도 결과는 같다.

  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="<프로젝트 절대 경로>/Scripts/patch_l00_note.py"
"""
import unreal

MAP = "/Game/VRPuzzle/Maps/L_00_Intro"
LABEL = "S0_ParcelNote"
NOTE = ("한결아, 오랜만이야.\n"
        "코어가 아직 돌아가고 있어. 20년째.\n"
        "출입 권한이 살아 있는 카드는 네 것뿐이야.\n"
        "지도에 길을 적어 뒀어. 부탁해. — 세진")

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(MAP):
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)

for a in eas.get_all_level_actors():
    if a.get_actor_label() == LABEL:
        eas.destroy_actor(a)

# 상자 안쪽 뒷벽에 기대 놓은 45 x 15cm 쪽지: 카드·지도(상자 바닥 가운데) 뒤에 두고, 위로 50도 젖혀서 내려다보면 읽히게
note = eas.spawn_actor_from_class(unreal.PuzzleSign, unreal.Vector(35.0, 0.0, 62.0), unreal.Rotator(roll=0.0, pitch=50.0, yaw=180.0))
note.set_actor_label(LABEL)
note.set_folder_path("Story")
note.set_editor_property("text", unreal.Text(NOTE))
note.set_editor_property("font_size", 34)
note.set_editor_property("draw_size", unreal.Vector2D(900, 300))
note.set_editor_property("pixels_per_cm", 20.0)
note.set_editor_property("visible_at_start", True)
note.set_editor_property("text_color", unreal.LinearColor(0.08, 0.08, 0.1, 1.0))
note.set_editor_property("background_color", unreal.LinearColor(0.85, 0.83, 0.78, 1.0))

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log("[patch_l00_note] 택배 쪽지 배치 완료")

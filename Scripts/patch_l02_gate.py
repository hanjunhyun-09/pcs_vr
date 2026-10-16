# -*- coding: utf-8 -*-
"""
L_02_Perspective: 출구 복도 끝 관측실 보안문을 지나면 L_03으로 넘어가게 고친다 (맵 전체를 다시 만들지 않음).
build_l02_perspective.py의 같은 부분과 똑같이 맞춘다. 열린 에디터에서 원격 실행으로 보내도 되고, 커맨드렛으로 돌려도 된다.
여러 번 실행해도 결과는 같다.
"""
import unreal

MAP = "/Game/VRPuzzle/Maps/L_02_Perspective"
MAT_DIR = "/Game/VRPuzzle/Materials"
ORIGIN = unreal.Vector(20000.0, 0.0, 0.0)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
MI_FLOOR = unreal.load_asset(MAT_DIR + "/MI_VRP_GridFloor")
MI_WALL = unreal.load_asset(MAT_DIR + "/MI_VRP_GridWall")
MI_CEIL = unreal.load_asset(MAT_DIR + "/MI_VRP_GridCeiling")


def V(x, y, z):
    return unreal.Vector(ORIGIN.x + x, ORIGIN.y + y, ORIGIN.z + z)


def box(label, x1, x2, y1, y2, z1, z2, mi, folder="Geometry/Exit"):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, V((x1 + x2) / 2.0, (y1 + y2) / 2.0, (z1 + z2) / 2.0))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.static_mesh_component.set_static_mesh(CUBE)
    a.static_mesh_component.set_material(0, mi)
    a.set_actor_scale3d(unreal.Vector((x2 - x1) / 100.0, (y2 - y1) / 100.0, (z2 - z1) / 100.0))
    return a


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(MAP):
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}

# 이전 실행/옛 끝 벽 정리
for label in ("Exit_WallEnd", "Exit_WallEnd_1", "Exit_WallEnd_2", "Exit_WallEnd_Header",
              "Gate_Floor", "Gate_Ceiling", "Gate_WallLeft", "Gate_WallRight", "Gate_WallEnd", "S2_StageGate"):
    if label in actors:
        eas.destroy_actor(actors[label])

box("Exit_WallEnd_1", 2220, 2240, 100, 140, 0, 300, MI_WALL)
box("Exit_WallEnd_2", 2220, 2240, 260, 300, 0, 300, MI_WALL)
box("Exit_WallEnd_Header", 2220, 2240, 140, 260, 220, 300, MI_WALL)
box("Gate_Floor", 2240, 2460, 100, 300, -10, 0, MI_FLOOR)
box("Gate_Ceiling", 2240, 2460, 80, 320, 300, 310, MI_CEIL)
box("Gate_WallLeft", 2240, 2460, 80, 100, 0, 300, MI_WALL)
box("Gate_WallRight", 2240, 2460, 300, 320, 0, 300, MI_WALL)
box("Gate_WallEnd", 2440, 2460, 100, 300, 0, 300, MI_WALL)

door = actors["S2_ObservationDoor"]
door.set_actor_location(V(2230, 200, 0), False, False)

gate = eas.spawn_actor_from_class(unreal.PuzzleStageGate, V(2230, 200, 0))
gate.set_actor_label("S2_StageGate")
gate.set_folder_path("Navigation")
gate.set_editor_property("next_level", "/Game/VRPuzzle/Maps/L_03_Observation")
gate.set_editor_property("door", door)

sign = actors.get("S2_Sign_Observation")
if sign:
    sign.set_editor_property("text", unreal.Text("3. 관측실"))

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
unreal.log_warning("[L02] 관측실 보안문 → L_03 연결 완료")

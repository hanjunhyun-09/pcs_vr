# -*- coding: utf-8 -*-
"""
L_04_Device (4스테이지: 장치 시험실)를 만든다.
노션 "맵 구조 설계 > L_04_Device" / "4스테이지 퍼즐 설계" / "대사 초안" 기준, 분위기는 Statik 참고:
피험자가 장치에 손을 넣고 서 있는 자리를 관찰 카메라와 한쪽 거울(관찰실) 너머에서 지켜보던 시험실.
20년 동안 버려져 있어서 벽·바닥이 더럽다 (질감은 setup_grime_materials.py, 잡동사니는 dress_grime.py).

실행 방법 (둘 중 하나, C++ 빌드 + setup_grime_materials.py 후):
  1) 에디터를 닫은 상태에서:
     UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_l04_device.py"
  2) 에디터를 켠 채로: Python 원격 실행으로 이 파일을 보내면 열린 에디터에서 바로 만들어진다
  → 다음에 dress_grime.py를 돌려서 바닥 잡동사니를 다시 깐다

다시 실행하면 L_04_Device 안의 액터를 모두 지우고 처음부터 다시 만든다.
크기 단위는 cm. 스테이지 4번이라 원점은 X = 400m (40000).

구역 (원점 기준, 바닥 높이 0, 북쪽 = +Y. 북쪽을 보고 서면 +X가 왼쪽):
  도착 복도      x 60~260,  y -340~0     (L_03에서 들어옴, 남쪽 벽 문 x 100~220은 장치를 차면 닫힘)
  장치 시험실    x 0~700,   y 0~600      천장 3m
                 책상 (350, 330) + 장치 받침대, 피험자 자리 (350, 225) 바닥에 노란 테이프
                 북쪽 벽: 왼쪽부터 환풍기 x 510~610 · 칠판 x 200~500 · 모니터 x 70~190
                 서쪽 벽(오른쪽): 한쪽 거울 y 180~480 → 너머에 관찰실 x -280~0 (책상, 의자, 옛 모니터, 관찰 기록)
                 북동쪽 구석: 삼각대 위 관찰 카메라 (빨간 녹화 표시등)
  출구 엘리베이터 동쪽 벽 y 100~220 너머 (중앙 코어행 → L_05_Core)
"""
import math

import unreal

MAP = "/Game/VRPuzzle/Maps/L_04_Device"
MAT_DIR = "/Game/VRPuzzle/Materials"
ORIGIN = unreal.Vector(40000.0, 0.0, 0.0)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")
BP_DESK = eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Props/BP_Desk")


def log(msg):
    unreal.log_warning("[L04] " + msg)


def V(x, y, z):
    return unreal.Vector(ORIGIN.x + x, ORIGIN.y + y, ORIGIN.z + z)


def ensure_mi(name, parent, params):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
        parent_now = mi.get_editor_property("parent")
        if parent_now and parent_now.get_name() == "M_VRP_Surface":
            return mi   # setup_grime_materials.py가 실제 질감 + 때로 바꾼 MI는 그대로 둔다
    else:
        mi = at.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in params.items():
        if isinstance(v, tuple):
            mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
        else:
            mel.set_material_instance_scalar_parameter_value(mi, k, v)
    eal.save_loaded_asset(mi, False)
    return mi


def ensure_glass_material():
    """M_VRP_OneWayGlass: 관찰실 한쪽 거울. 어두운 반투명 + 반사. 방이 밝으면 거울처럼 어둡고, 방 불을 끄면 너머 관찰실이 비쳐 보인다"""
    path = MAT_DIR + "/M_VRP_OneWayGlass"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = at.create_asset("M_VRP_OneWayGlass", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -200)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(0.02, 0.025, 0.03, 1.0))
    op = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 0)
    op.set_editor_property("parameter_name", "Opacity")
    op.set_editor_property("default_value", 0.55)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 150)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.08)
    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 300)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.6)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("M_VRP_OneWayGlass 생성")
    return mat


# ---------------------------------------------------------------- 레벨 도우미 (build_l03_observation.py와 같은 방식)
def spawn(cls, loc, yaw=0.0, label=None, folder=None, pitch=0.0, roll=0.0):
    a = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    if label:
        a.set_actor_label(label)
    if folder:
        a.set_folder_path(folder)
    return a


def box(label, x1, x2, y1, y2, z1, z2, mi, folder="Geometry", collision=True, movable=False):
    """x1~x2, y1~y2, z1~z2 범위를 채우는 상자"""
    center = V((x1 + x2) / 2.0, (y1 + y2) / 2.0, (z1 + z2) / 2.0)
    a = spawn(unreal.StaticMeshActor, center, label=label, folder=folder)
    smc = a.static_mesh_component
    if movable:
        smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    smc.set_static_mesh(CUBE)
    smc.set_material(0, mi)
    if not collision:
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        smc.set_editor_property("can_ever_affect_navigation", False)
    a.set_actor_scale3d(unreal.Vector((x2 - x1) / 100.0, (y2 - y1) / 100.0, (z2 - z1) / 100.0))
    return a


def prop(label, mesh, x, y, z, size, mi, yaw=0.0, pitch=0.0, roll=0.0, folder="Props", movable=False):
    """size = (x, y, z) cm. 소품은 충돌·내비게이션 없음 (막힘 방지)"""
    a = spawn(unreal.StaticMeshActor, V(x, y, z), yaw=yaw, pitch=pitch, roll=roll, label=label, folder=folder)
    smc = a.static_mesh_component
    if movable:
        smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    smc.set_static_mesh(mesh)
    smc.set_material(0, mi)
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    smc.set_editor_property("can_ever_affect_navigation", False)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return a


def point_light(label, x, y, z, candela, color=(1.0, 0.97, 0.9), radius=1000.0, shadows=True):
    """켜고 끄는 조명이라 전부 Movable (노션: 4스테이지 장비 조명은 Movable)"""
    a = spawn(unreal.PointLight, V(x, y, z), label=label, folder="Lighting")
    lc = a.get_component_by_class(unreal.PointLightComponent)
    lc.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", candela)
    lc.set_editor_property("attenuation_radius", radius)
    lc.set_editor_property("light_color", unreal.Color(r=int(color[0] * 255), g=int(color[1] * 255), b=int(color[2] * 255), a=255))
    lc.set_editor_property("cast_shadows", shadows)
    return a


def sign_at(label, text, loc, yaw=0.0, pitch=0.0, size=(800, 500), font=36, ppc=10.0, visible=True,
            text_color=(0.05, 0.05, 0.05), bg=(0.85, 0.83, 0.78, 1.0), folder="Story"):
    a = spawn(unreal.PuzzleSign, loc, yaw=yaw, pitch=pitch, label=label, folder=folder)
    a.set_editor_property("text", unreal.Text(text))
    a.set_editor_property("font_size", font)
    a.set_editor_property("draw_size", unreal.Vector2D(size[0], size[1]))
    a.set_editor_property("pixels_per_cm", ppc)
    a.set_editor_property("visible_at_start", visible)
    a.set_editor_property("text_color", unreal.LinearColor(text_color[0], text_color[1], text_color[2], 1.0))
    a.set_editor_property("background_color", unreal.LinearColor(bg[0], bg[1], bg[2], bg[3]))
    return a


def sign(label, text, x, y, z, **kw):
    return sign_at(label, text, V(x, y, z), **kw)


def toggle_target(label, x, y, z, name, yaw=0.0, start_on=False):
    a = spawn(unreal.PuzzleToggleTarget, V(x, y, z), yaw=yaw, label=label, folder="Puzzle/Equipment")
    a.set_editor_property("display_name", name)
    a.set_editor_property("start_on", start_on)
    return a


def attach(child, parent):
    child.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, False)


# ================================================================ 시작
grid = unreal.load_asset(MAT_DIR + "/M_VRP_Grid")
solid = unreal.load_asset(MAT_DIR + "/M_VRP_Solid")
MI_FLOOR = ensure_mi("MI_VRP_DevFloor", grid, {"Color": (0.16, 0.16, 0.17), "LineColor": (0.24, 0.24, 0.26), "LineWidth": 2.0, "Roughness": 0.6})
MI_WALL = ensure_mi("MI_VRP_DevWall", grid, {"Color": (0.30, 0.31, 0.30), "LineColor": (0.24, 0.25, 0.24), "LineWidth": 2.0, "Roughness": 0.85})
MI_CEIL = ensure_mi("MI_VRP_DevCeiling", grid, {"Color": (0.14, 0.14, 0.15), "LineColor": (0.2, 0.2, 0.21), "LineWidth": 1.5, "Roughness": 0.9})
MI_METAL = ensure_mi("MI_VRP_DevMetal", solid, {"Color": (0.35, 0.36, 0.38), "Emissive": 0.0, "Roughness": 0.4})
MI_BOARD = ensure_mi("MI_VRP_Chalkboard", solid, {"Color": (0.06, 0.13, 0.09), "Emissive": 0.0, "Roughness": 0.95})
MI_DARK = ensure_mi("MI_VRP_DevDark", solid, {"Color": (0.04, 0.04, 0.045), "Emissive": 0.0, "Roughness": 0.5})
MI_SCREEN = ensure_mi("MI_VRP_MonitorGlow", solid, {"Color": (0.2, 0.45, 0.6), "Emissive": 1.2, "Roughness": 0.3})
MI_RED = ensure_mi("MI_VRP_WarningGlow", solid, {"Color": (1.0, 0.08, 0.04), "Emissive": 6.0, "Roughness": 0.3})
MI_PANEL = ensure_mi("MI_VRP_CeilingPanel", solid, {"Color": (1.0, 0.97, 0.9), "Emissive": 3.0, "Roughness": 0.5})
MI_PAPER = ensure_mi("MI_VRP_DevPaper", solid, {"Color": (0.72, 0.68, 0.56), "Emissive": 0.03, "Roughness": 0.9})
MI_TAPE = ensure_mi("MI_VRP_FloorTape", solid, {"Color": (0.75, 0.6, 0.12), "Emissive": 0.0, "Roughness": 0.8})
MI_PLASTIC = ensure_mi("MI_VRP_OldPlastic", solid, {"Color": (0.55, 0.52, 0.44), "Emissive": 0.0, "Roughness": 0.7})
MI_FABRIC = ensure_mi("MI_VRP_OldFabric", solid, {"Color": (0.12, 0.13, 0.16), "Emissive": 0.0, "Roughness": 0.95})
MI_CARDBOARD = ensure_mi("MI_VRP_Cardboard", solid, {"Color": (0.42, 0.3, 0.17), "Emissive": 0.0, "Roughness": 0.95})
MI_REC = ensure_mi("MI_VRP_RecLamp", solid, {"Color": (1.0, 0.05, 0.03), "Emissive": 8.0, "Roughness": 0.3})
MI_LAMP_WARM = ensure_mi("MI_VRP_BoothLamp", solid, {"Color": (1.0, 0.75, 0.4), "Emissive": 4.0, "Roughness": 0.5})
MI_GLASS = ensure_glass_material()
MI_TRIM = unreal.load_asset(MAT_DIR + "/MI_VRP_Trim")
MI_WARN = unreal.load_asset(MAT_DIR + "/MI_VRP_Warning")

# 노출 값은 인트로 맵과 똑같이 맞춘다
unreal.EditorLoadingAndSavingUtils.load_map("/Game/VRPuzzle/Maps/L_00_Intro")
intro_pp = None
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.PostProcessVolume):
        intro_pp = a.get_editor_property("settings")

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
for a in eas.get_all_level_actors():
    if isinstance(a, (unreal.WorldSettings, unreal.Brush)) and not isinstance(a, unreal.Volume):
        continue
    eas.destroy_actor(a)
log("기존 액터 정리")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("kill_z", -500.0)   # 바닥보다 5m 아래

# ---------------------------------------------------------------- 지오메트리
W, D, H = 700.0, 600.0, 300.0   # 방 가로(x), 세로(y), 천장 (노션: 3m)
T = 20.0
DOOR_H = 220.0

box("Floor", 0, W, 0, D, -10, 0, MI_FLOOR)
box("Ceiling", -T, W + T, -T, D + T, H, H + 10, MI_CEIL)
box("Wall_North", -T, W + T, D, D + T, 0, H, MI_WALL)
# 남쪽 벽: 들어온 문 x 100~220
box("Wall_South_1", -T, 100, -T, 0, 0, H, MI_WALL)
box("Wall_South_2", 220, W + T, -T, 0, 0, H, MI_WALL)
box("Wall_South_Header", 100, 220, -T, 0, DOOR_H, H, MI_WALL)
# 서쪽 벽: 한쪽 거울 y 180~480, 높이 90~230
MIR = (180.0, 480.0, 90.0, 230.0)
box("Wall_West_1", -T, 0, 0, MIR[0], 0, H, MI_WALL)
box("Wall_West_2", -T, 0, MIR[1], D, 0, H, MI_WALL)
box("Wall_West_Below", -T, 0, MIR[0], MIR[1], 0, MIR[2], MI_WALL)
box("Wall_West_Above", -T, 0, MIR[0], MIR[1], MIR[3], H, MI_WALL)
box("Mirror_Glass", -12, -8, MIR[0], MIR[1], MIR[2], MIR[3], MI_GLASS, folder="Geometry/Mirror")
box("Mirror_FrameTop", -6, 0, MIR[0] - 6, MIR[1] + 6, MIR[3], MIR[3] + 6, MI_TRIM, folder="Geometry/Mirror")
box("Mirror_FrameBottom", -6, 4, MIR[0] - 6, MIR[1] + 6, MIR[2] - 6, MIR[2], MI_TRIM, folder="Geometry/Mirror")
# 동쪽 벽: 엘리베이터 문 y 100~220
box("Wall_East_1", W, W + T, 0, 100, 0, H, MI_WALL)
box("Wall_East_2", W, W + T, 220, D, 0, H, MI_WALL)
box("Wall_East_Header", W, W + T, 100, 220, DOOR_H, H, MI_WALL)

# 도착 복도 (폭 2m, 천장 3m) + 뒤쪽 보안문 (L_03에서 들어온 문, 닫힌 채)
box("Arrive_Floor", 60, 260, -340, 0, -10, 0, MI_FLOOR, folder="Geometry/Arrive")
box("Arrive_Ceiling", 40, 280, -340, 0, 300, 310, MI_CEIL, folder="Geometry/Arrive")
box("Arrive_WallLeft", 260, 280, -340, -T, 0, 300, MI_WALL, folder="Geometry/Arrive")
box("Arrive_WallRight", 40, 60, -340, -T, 0, 300, MI_WALL, folder="Geometry/Arrive")
box("Arrive_WallEnd", 60, 260, -340, -320, 0, 300, MI_WALL, folder="Geometry/Arrive")

# 관찰실 (한쪽 거울 너머, 들어갈 수 없음): x -280~-20, y 150~510, 천장 2.6m
BOOTH = (-280.0, -20.0, 150.0, 510.0)
box("Booth_Floor", BOOTH[0], BOOTH[1], BOOTH[2], BOOTH[3], -10, 0, MI_FLOOR, folder="Geometry/Booth")
box("Booth_Ceiling", BOOTH[0] - T, BOOTH[1], BOOTH[2] - T, BOOTH[3] + T, 260, 270, MI_CEIL, folder="Geometry/Booth")
box("Booth_WallBack", BOOTH[0] - T, BOOTH[0], BOOTH[2] - T, BOOTH[3] + T, 0, 260, MI_WALL, folder="Geometry/Booth")
box("Booth_WallSouth", BOOTH[0], BOOTH[1], BOOTH[2] - T, BOOTH[2], 0, 260, MI_WALL, folder="Geometry/Booth")
box("Booth_WallNorth", BOOTH[0], BOOTH[1], BOOTH[3], BOOTH[3] + T, 0, 260, MI_WALL, folder="Geometry/Booth")

# ---------------------------------------------------------------- 후처리 / 꺼지지 않는 빛
pp = spawn(unreal.PostProcessVolume, V(0, 0, 0), label="PostProcess", folder="Lighting")
pp.set_editor_property("unbound", True)
if intro_pp is not None:
    pp.set_editor_property("settings", intro_pp)

# 불을 다 꺼도 방의 윤곽은 보이게 (노션 학교 확인: 어두운 방이 VR에서 너무 어둡지 않은지)
point_light("Fill_Light", 350, 300, 280, 1.5, color=(0.5, 0.6, 0.8), radius=1000, shadows=False)
point_light("Arrive_Light", 160, -170, 280, 12, radius=500)
# 관찰실 책상 스탠드 (20년째 켜져 있는 희미한 백열등). 방 불을 끄면 거울 너머로 관찰실이 보인다
point_light("Booth_Lamp", -200, 400, 120, 6, color=(1.0, 0.7, 0.4), radius=400)

# ---------------------------------------------------------------- 도착 / 플레이어 / 내비게이션
arrive_door = spawn(unreal.PuzzleDoor, V(160, -310, 0), yaw=90.0, label="S4_ArrivalDoor", folder="Navigation")
arrive_door.set_editor_property("door_color", unreal.LinearColor(0.2, 0.22, 0.25, 1.0))
spawn(unreal.PlayerStart, V(160, -200, 1), yaw=90.0, label="PlayerStart", folder="Navigation")

nav = spawn(unreal.NavMeshBoundsVolume, V(350, 150, 150), label="NavMeshBounds", folder="Navigation")
nav.set_actor_scale3d(unreal.Vector(900 / 200.0, 1100 / 200.0, 400 / 200.0))

# 들어온 문: 처음엔 열려 있고, 장치를 차면 닫힘
entry_door = spawn(unreal.PuzzleDoor, V(160, -10, 0), yaw=90.0, label="S4_EntryDoor", folder="Puzzle")
entry_door.set_editor_property("start_open", True)
entry_door.set_editor_property("door_color", unreal.LinearColor(0.28, 0.3, 0.33, 1.0))

# ================================================================ 책상 + 장치 받침대 + 종이
DESK = (350.0, 330.0)
DESK_TOP = 74.5
STAND = (350.0, 225.0)   # 피험자 자리
spawn(BP_DESK, V(DESK[0], DESK[1], 0), yaw=90.0, label="S4_Desk", folder="Props")   # 서랍 쪽이 남쪽(플레이어 쪽)

# 받침대: 앞(+X)이 플레이어 쪽(남쪽)을 보게
device = spawn(unreal.PuzzleHandDevice, V(DESK[0], 312, DESK_TOP), yaw=-90.0, label="S4_HandDevice", folder="Puzzle")
device.set_editor_property("solution", [1, 0, 3, 2])   # ▲ ● ◆ ■
sign("S4_Sign_Rack", "피험 장치 · 양손을 넣으십시오", DESK[0], 290, DESK_TOP + 0.3, yaw=-90.0, pitch=90.0,
     size=(560, 56), font=28, ppc=20.0, bg=(0.12, 0.12, 0.12, 1.0), text_color=(0.85, 0.8, 0.6), folder="Puzzle")

# 피험자 자리: 바닥 노란 테이프 사각형 (80 x 80, 군데군데 떨어져 나감)
TAPE = 4.0
for i, (x1, x2, y1, y2) in enumerate(((-40, 40, -40, -40 + TAPE), (-40, 25, 40 - TAPE, 40), (-40, -40 + TAPE, -40, 40), (40 - TAPE, 40, -40, 10))):
    box("S4_Tape_%d" % i, STAND[0] + x1, STAND[0] + x2, STAND[1] + y1, STAND[1] + y2, 0, 0.3, MI_TAPE, folder="Props/Tape", collision=False)

# 찢어진 기호표 (책상 위 왼쪽, 종이 밑). 환풍기를 켜면 종이가 날아가며 드러남
TABLE = (DESK[0] + 55, 345.0)
symbol_table = sign("S4_SymbolTable", "●  빛\n▲  (찢어짐)\n■  (찢어짐)\n◆  (찢어짐)", TABLE[0], TABLE[1], DESK_TOP + 0.25,
                    yaw=-90.0, pitch=90.0, size=(440, 380), font=44, ppc=20.0, visible=False,
                    bg=(0.78, 0.74, 0.62, 1.0), text_color=(0.12, 0.1, 0.08), folder="Puzzle")
papers = []
for i, (dx, dy, yaw) in enumerate(((-6, -4, 8.0), (5, 3, -14.0), (-2, 6, 27.0), (7, -6, -3.0), (0, 0, 45.0))):
    p = prop("S4_Paper_%d" % i, CUBE, TABLE[0] + dx, TABLE[1] + dy, DESK_TOP + 0.6 + i * 0.25, (30, 24, 0.2), MI_PAPER,
             yaw=yaw, folder="Puzzle/Papers", movable=True)
    papers.append(p)

# ================================================================ 북쪽 벽: 칠판 + 숨은 글씨
WALL_Y = D
box("S4_Chalkboard", 200, 500, WALL_Y - 6, WALL_Y, 95, 235, MI_BOARD, folder="Puzzle/Board")
box("S4_ChalkboardFrameTop", 195, 505, WALL_Y - 8, WALL_Y, 235, 241, MI_TRIM, folder="Puzzle/Board")
box("S4_ChalkboardFrameBottom", 195, 505, WALL_Y - 10, WALL_Y, 89, 95, MI_TRIM, folder="Puzzle/Board")
CHALK = (0.82, 0.84, 0.8)
board_hint = sign("S4_Board_Hint", "왼손은 고르고, 오른손은 움직인다.", 350, WALL_Y - 6.5, 205, yaw=-90.0,
                  size=(2200, 120), font=60, ppc=10.0, bg=(0, 0, 0, 0), text_color=CHALK, folder="Puzzle/Board")
board_memo = sign("S4_Board_Memo", "나연이 시연 참가 반대!!\n신입한테 맡길 일 아님 — 세진", 380, WALL_Y - 6.5, 130, yaw=-90.0,
                  size=(1700, 200), font=48, ppc=10.0, bg=(0, 0, 0, 0), text_color=CHALK, folder="Puzzle/Board")
# 숨은 글씨: 칠판 바로 앞에 겹친 별도 판 (조명 OFF + 경고등 ON에서만 보임)
hidden = sign("S4_Board_Hidden", "장치 푸는 법 (소장님 몰래)\n경고 → 빛 → 바람 → 화면", 350, WALL_Y - 7, 165, yaw=-90.0,
              size=(2400, 300), font=72, ppc=10.0, visible=False, bg=(0, 0, 0, 0), text_color=(1.0, 0.25, 0.2),
              folder="Puzzle/Board")

# ================================================================ ● 천장 형광등 2개 (+ 떨어져 매달린 고장 난 등 하나)
ROOM_LIGHT_POS = ((200, 300), (500, 300))
room_lights = [point_light("S4_RoomLight_%d" % i, x, y, 285, 30, radius=1100) for i, (x, y) in enumerate(ROOM_LIGHT_POS)]
room_panels = []
for i, (x, y) in enumerate(ROOM_LIGHT_POS):
    box("S4_LightHousing_%d" % i, x - 65, x + 65, y - 18, y + 18, H - 6, H, MI_METAL, folder="Puzzle/Equipment", collision=False)
    room_panels.append(box("S4_CeilingPanel_%d" % i, x - 60, x + 60, y - 14, y + 14, H - 7, H - 6, MI_PANEL,
                           folder="Puzzle/Equipment", collision=False, movable=True))
room_light = toggle_target("S4_Toggle_RoomLight", 350, 300, 290, "● 천장 조명", start_on=True)
room_light.set_editor_property("lights", room_lights)
room_light.set_editor_property("show_when_on", room_panels)

# 한쪽 사슬이 끊어져 비스듬히 매달린 등 (꺼져 있음, 장식)
prop("Broken_LightHousing", CUBE, 560, 480, 245, (130, 36, 6), MI_METAL, roll=0.0, pitch=24.0, folder="Props/Broken")
prop("Broken_LightChain", CYLINDER, 500, 480, 272, (1, 1, 50), MI_METAL, folder="Props/Broken")

# ================================================================ ▲ 빨간 경고등 (천장 가운데)
box("S4_WarningHousing", 330, 370, 280, 320, H - 12, H, MI_DARK, folder="Puzzle/Equipment", collision=False)
warning_glow = box("S4_WarningGlow", 336, 364, 286, 314, H - 20, H - 12, MI_RED, folder="Puzzle/Equipment", collision=False,
                   movable=True)
warning_lights = [point_light("S4_WarningLight", 350, 300, H - 30, 40, color=(1.0, 0.1, 0.05), radius=1200, shadows=False)]
warning = toggle_target("S4_Toggle_Warning", 350, 300, H - 25, "▲ 빨간 경고등")
warning.set_editor_property("lights", warning_lights)
warning.set_editor_property("show_when_on", [warning_glow])

# ================================================================ ■ 모니터 (북쪽 벽, 보는 사람 오른쪽)
MON = (70.0, 190.0, 100.0, 200.0)   # x1, x2, z1, z2
box("S4_MonitorHousing", MON[0] - 6, MON[1] + 6, WALL_Y - 14, WALL_Y, MON[2] - 6, MON[3] + 6, MI_PLASTIC, folder="Puzzle/Equipment")
box("S4_MonitorBezel", MON[0] - 1, MON[1] + 1, WALL_Y - 15, WALL_Y - 14, MON[2] - 1, MON[3] + 1, MI_DARK, folder="Puzzle/Equipment",
    collision=False)
monitor_glow = box("S4_MonitorScreen", MON[0], MON[1], WALL_Y - 16, WALL_Y - 15, MON[2], MON[3], MI_SCREEN,
                   folder="Puzzle/Equipment", collision=False, movable=True)
monitor_text = sign("S4_MonitorText", "참가자 신호 상태", (MON[0] + MON[1]) / 2.0, WALL_Y - 16.5, (MON[2] + MON[3]) / 2.0, yaw=-90.0,
                    size=(1150, 950), font=40, ppc=10.0, visible=False, bg=(0, 0, 0, 0), text_color=(0.75, 0.95, 1.0),
                    folder="Puzzle/Equipment")
monitor_lights = [point_light("S4_MonitorLight", (MON[0] + MON[1]) / 2.0, WALL_Y - 70, 150, 8, color=(0.4, 0.75, 1.0), radius=450,
                              shadows=False)]
monitor = toggle_target("S4_Toggle_Monitor", (MON[0] + MON[1]) / 2.0, WALL_Y - 25, 150, "■ 모니터")
monitor.set_editor_property("lights", monitor_lights)
monitor.set_editor_property("show_when_on", [monitor_glow, monitor_text])

# ================================================================ ◆ 환풍기 (북쪽 벽, 보는 사람 왼쪽, 높이 1.2 ~ 2.2m)
FAN_C = (560.0, 170.0)   # x, z
box("S4_FanHousing", FAN_C[0] - 50, FAN_C[0] + 50, WALL_Y - 4, WALL_Y, FAN_C[1] - 50, FAN_C[1] + 50, MI_DARK, folder="Puzzle/Equipment")
for i, dz in enumerate((-45, 45)):
    box("S4_FanFrame_H%d" % i, FAN_C[0] - 50, FAN_C[0] + 50, WALL_Y - 14, WALL_Y - 4, FAN_C[1] + dz - 4, FAN_C[1] + dz + 4, MI_METAL,
        folder="Puzzle/Equipment")
for i, dx in enumerate((-45, 45)):
    box("S4_FanFrame_V%d" % i, FAN_C[0] + dx - 4, FAN_C[0] + dx + 4, WALL_Y - 14, WALL_Y - 4, FAN_C[1] - 50, FAN_C[1] + 50, MI_METAL,
        folder="Puzzle/Equipment")
hub = box("S4_FanHub", FAN_C[0] - 6, FAN_C[0] + 6, WALL_Y - 12, WALL_Y - 6, FAN_C[1] - 6, FAN_C[1] + 6, MI_METAL,
          folder="Puzzle/Equipment", collision=False, movable=True)
for i in range(4):
    blade = prop("S4_FanBlade_%d" % i, CUBE, FAN_C[0], WALL_Y - 9, FAN_C[1], (80, 1.5, 10), MI_METAL, pitch=45.0 * i,
                 folder="Puzzle/Equipment", movable=True)   # 가운데를 지나는 막대 4개 → 날개 8장
    attach(blade, hub)
fan_lights = [point_light("S4_FanBackLight", FAN_C[0], WALL_Y - 40, FAN_C[1], 6, color=(0.9, 0.95, 1.0), radius=300, shadows=False)]
fan = toggle_target("S4_Toggle_Fan", FAN_C[0], WALL_Y - 9, FAN_C[1], "◆ 환풍기", yaw=-90.0)   # 앞(+X) = 벽에서 나오는 방향 = 회전축
fan.set_editor_property("lights", fan_lights)
fan.set_editor_property("spin_when_on", [hub])

# 장치 다이얼 순서 ● ▲ ■ ◆ = 조명, 경고등, 모니터, 환풍기
device.set_editor_property("targets", [room_light, warning, monitor, fan])

# ================================================================ 관찰 카메라 (북동쪽 구석, 피험자 자리를 봄)
CAM = (640.0, 540.0)
cam_yaw = unreal.MathLibrary.find_look_at_rotation(V(CAM[0], CAM[1], 165), V(STAND[0], STAND[1], 120)).yaw
for i in range(3):
    leg_yaw = 120.0 * i
    lx = CAM[0] + 22 * math.cos(math.radians(leg_yaw))
    ly = CAM[1] + 22 * math.sin(math.radians(leg_yaw))
    prop("Camera_Leg_%d" % i, CYLINDER, (CAM[0] + lx) / 2.0, (CAM[1] + ly) / 2.0, 75, (2.5, 2.5, 155), MI_METAL,
         yaw=leg_yaw, pitch=-8.0 if i else 8.0, folder="Props/Camera")
prop("Camera_Head", CUBE, CAM[0], CAM[1], 152, (10, 10, 6), MI_METAL, folder="Props/Camera")
cam_body = prop("Camera_Body", CUBE, CAM[0], CAM[1], 165, (30, 14, 16), MI_DARK, yaw=cam_yaw, pitch=-10.0, folder="Props/Camera")
fwd = unreal.MathLibrary.get_forward_vector(unreal.Rotator(0, -10.0, cam_yaw))
prop("Camera_Lens", CYLINDER, CAM[0] + fwd.x * 19, CAM[1] + fwd.y * 19, 165 + fwd.z * 19, (10, 10, 8), MI_DARK,
     yaw=cam_yaw, pitch=80.0, folder="Props/Camera")
rec_mesh = prop("Camera_RecLamp", SPHERE, CAM[0] + fwd.x * 10, CAM[1] + fwd.y * 10, 175, (2, 2, 2), MI_REC, folder="Props/Camera", movable=True)
rec_light = point_light("Camera_RecLight", CAM[0] + fwd.x * 12, CAM[1] + fwd.y * 12, 176, 0.6, color=(1.0, 0.1, 0.05), radius=80, shadows=False)

# ================================================================ 관찰실 (거울 너머): 책상, 의자, 옛 모니터, 관찰 기록, 커피잔
B_DESK = (-120.0, 330.0)
prop("Booth_DeskTop", CUBE, B_DESK[0], B_DESK[1], 74, (60, 200, 4), MI_PLASTIC, folder="Props/Booth")
for i, (dx, dy) in enumerate(((-26, -96), (26, -96), (-26, 96), (26, 96))):
    prop("Booth_DeskLeg_%d" % i, CUBE, B_DESK[0] + dx, B_DESK[1] + dy, 36, (3, 3, 72), MI_METAL, folder="Props/Booth")
prop("Booth_CRT", CUBE, B_DESK[0] - 8, B_DESK[1] - 50, 100, (40, 38, 34), MI_PLASTIC, yaw=12.0, folder="Props/Booth")
prop("Booth_CRTScreen", CUBE, B_DESK[0] + 12.5, B_DESK[1] - 46, 102, (1, 30, 24), MI_DARK, yaw=12.0, folder="Props/Booth")
prop("Booth_Mug", CYLINDER, B_DESK[0] + 10, B_DESK[1] + 60, 81, (8, 8, 10), MI_PAPER, folder="Props/Booth")
prop("Booth_LampBase", CYLINDER, B_DESK[0] - 15, B_DESK[1] + 75, 78, (12, 12, 3), MI_METAL, folder="Props/Booth")
prop("Booth_LampArm", CYLINDER, B_DESK[0] - 15, B_DESK[1] + 75, 95, (1.5, 1.5, 34), MI_METAL, folder="Props/Booth")
prop("Booth_LampShade", CYLINDER, B_DESK[0] - 8, B_DESK[1] + 75, 112, (14, 14, 10), MI_LAMP_WARM, pitch=30.0, folder="Props/Booth")
# 바퀴 달린 의자: 거울 쪽으로 돌려진 채 비어 있음
CHAIR = (-60.0, 300.0)
prop("Booth_ChairSeat", CUBE, CHAIR[0], CHAIR[1], 48, (46, 46, 8), MI_FABRIC, yaw=15.0, folder="Props/Booth")
prop("Booth_ChairBack", CUBE, CHAIR[0] - 22, CHAIR[1] - 4, 80, (6, 44, 50), MI_FABRIC, yaw=15.0, pitch=-8.0, folder="Props/Booth")
prop("Booth_ChairPost", CYLINDER, CHAIR[0], CHAIR[1], 25, (5, 5, 40), MI_METAL, folder="Props/Booth")
prop("Booth_ChairBase", CYLINDER, CHAIR[0], CHAIR[1], 4, (55, 55, 3), MI_METAL, folder="Props/Booth")
# 관찰 기록 (책상 위, 거울 쪽에서 읽힘)
sign("Booth_Log", "피험자 관찰 기록  2006.11.13\n피험 장치 01 · 착용자 오나연\n반응 정상. 피험자 불안 호소\n\"손이 안 보여서 무서워요\"\n→ 시연 예정대로 진행 (권)",
     B_DESK[0] + 20, B_DESK[1] + 15, 76.5, yaw=0.0, pitch=60.0, size=(560, 420), font=34, ppc=14.0,
     bg=(0.78, 0.75, 0.66, 1.0), text_color=(0.1, 0.1, 0.12), folder="Props/Booth")

# ================================================================ 오래된 집기 (방 안, 길을 막지 않게 벽 쪽)
# 남서쪽 구석: 서랍 하나가 열린 철제 서류함
prop("Cabinet_Body", CUBE, 40, 40, 66, (55, 45, 132), MI_METAL, folder="Props/Room")
prop("Cabinet_Drawer", CUBE, 40, 78, 95, (48, 32, 26), MI_METAL, folder="Props/Room")
prop("Cabinet_DrawerFace", CUBE, 40, 95, 95, (50, 2, 28), MI_METAL, folder="Props/Room")
# 남동쪽 구석: 쌓아 둔 상자 (하나는 쓰러짐)
prop("Boxes_0", CUBE, 640, 50, 22, (55, 45, 44), MI_CARDBOARD, yaw=5.0, folder="Props/Room")
prop("Boxes_1", CUBE, 638, 52, 62, (50, 40, 36), MI_CARDBOARD, yaw=-7.0, folder="Props/Room")
prop("Boxes_2", CUBE, 585, 70, 17, (45, 40, 34), MI_CARDBOARD, yaw=40.0, roll=85.0, folder="Props/Room")
# 쓰러진 의자 (서쪽 벽 거울 아래)
prop("FallenChair_Seat", CUBE, 60, 520, 22, (44, 44, 6), MI_FABRIC, roll=80.0, yaw=20.0, folder="Props/Room")
prop("FallenChair_Back", CUBE, 40, 545, 40, (6, 42, 46), MI_FABRIC, roll=80.0, yaw=20.0, folder="Props/Room")
prop("FallenChair_Post", CYLINDER, 85, 510, 22, (5, 5, 40), MI_METAL, roll=90.0, yaw=20.0, folder="Props/Room")

# ================================================================ 출구: 중앙 코어행 엘리베이터 (동쪽)
elevator = spawn(unreal.PuzzleElevator, V(W + 120, 160, 0), yaw=180.0, label="S4_Elevator", folder="Puzzle")
elevator.set_editor_property("next_level", "/Game/VRPuzzle/Maps/L_05_Core")
box("Elevator_Stripe", W - 4, W, 100, 220, DOOR_H, DOOR_H + 8, MI_WARN, folder="Geometry/Trim")

# ---------------------------------------------------------------- 글자 (대사 초안)
sign("S4_Sign_Title", "4. 장치 시험실\n피험 장치 동작 시험", 450, 1, 230, yaw=90.0, size=(700, 220), font=40,
     bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))
sign("S4_Sign_ArriveDoor", "3. 관측실", 160, -319, 255, yaw=90.0, size=(500, 90), font=34,
     bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))
sign("S4_Sign_Elevator", "5. 중앙 코어\n(엘리베이터)", W - 1, 330, 230, yaw=180.0, size=(600, 160), font=34,
     bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))
sign("S4_Sign_Mirror", "관찰실", -1, 330, 250, yaw=0.0, size=(300, 70), font=30,
     bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))

# ---------------------------------------------------------------- 진행 담당
director = spawn(unreal.DeviceStageDirector, V(0, 0, 400), label="S4_StageDirector", folder="Puzzle")
director.set_editor_property("device", device)
director.set_editor_property("room_light", room_light)
director.set_editor_property("warning_light", warning)
director.set_editor_property("monitor", monitor)
director.set_editor_property("fan", fan)
director.set_editor_property("board_texts", [board_hint, board_memo])
director.set_editor_property("hidden_text", hidden)
director.set_editor_property("symbol_table", symbol_table)
director.set_editor_property("papers", papers)
director.set_editor_property("monitor_text", monitor_text)
director.set_editor_property("entry_door", entry_door)
director.set_editor_property("exit_elevator", elevator)
director.set_editor_property("rec_lamps", [rec_mesh, rec_light])

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("L_04_Device 저장 완료")

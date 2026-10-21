# -*- coding: utf-8 -*-
"""
L_03_Observation (3스테이지: 관측실)를 만든다.
노션 "맵 구조 설계 > L_03_Observation" / "3스테이지 퍼즐 설계" / "대사 초안" 기준.

실행 방법 (둘 중 하나, C++ 빌드 후):
  1) 에디터를 닫은 상태에서:
     UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_l03_observation.py"
  2) 에디터를 켠 채로: Python 원격 실행으로 이 파일을 보내면 열린 에디터에서 바로 만들어진다

다시 실행하면 L_03_Observation 안의 액터를 모두 지우고 처음부터 다시 만든다.
크기 단위는 cm. 스테이지 3번이라 원점은 X = 300m (30000).

구역 (원점 기준, 바닥 높이 0, 천장 4.5m):
  도착 복도      x -320~0,    y 60~260   (뒤쪽 보안문 = L_02에서 들어온 문)
  앞방          x 0~600,     y 0~800    발자국(250, 400) → 셔터 쪽을 보면 "코어를 보지 마 / 1114"
  셔터 칸막이    x 600~620   셔터 y 340~460, 키패드 (600, 520, 120)
  안쪽 관측 구역  x 620~1400,  y 0~800    눈금 선 교차점(820, 250) → 파이프 원 / 동쪽 벽 도면(빈 칸 y 500~600, z 180~280)
  출구 복도      x 1420~2020, y 60~260   끝에 장치 시험실 보안문 → 지나가면 L_04_Device
"""
import math
import random

import unreal

MAP = "/Game/VRPuzzle/Maps/L_03_Observation"
MAT_DIR = "/Game/VRPuzzle/Materials"
ORIGIN = unreal.Vector(30000.0, 0.0, 0.0)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
BP_PCUBE = eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Interaction/BP_PerspectiveCube")
BP_DESK = eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Props/BP_Desk")

random.seed(1114)   # 조각 배치가 매번 같게


def log(msg):
    unreal.log_warning("[L03] " + msg)


def V(x, y, z):
    return unreal.Vector(ORIGIN.x + x, ORIGIN.y + y, ORIGIN.z + z)


def ensure_mi(name, parent, params):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
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


# ---------------------------------------------------------------- 레벨 도우미 (build_l02_perspective.py와 같은 방식)
def spawn(cls, loc, yaw=0.0, label=None, folder=None, pitch=0.0, roll=0.0):
    a = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    if label:
        a.set_actor_label(label)
    if folder:
        a.set_folder_path(folder)
    return a


def box(label, x1, x2, y1, y2, z1, z2, mi, folder="Geometry", collision=True):
    """x1~x2, y1~y2, z1~z2 범위를 채우는 상자"""
    center = V((x1 + x2) / 2.0, (y1 + y2) / 2.0, (z1 + z2) / 2.0)
    a = spawn(unreal.StaticMeshActor, center, label=label, folder=folder)
    smc = a.static_mesh_component
    smc.set_static_mesh(CUBE)
    smc.set_material(0, mi)
    if not collision:
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        smc.set_editor_property("can_ever_affect_navigation", False)
    a.set_actor_scale3d(unreal.Vector((x2 - x1) / 100.0, (y2 - y1) / 100.0, (z2 - z1) / 100.0))
    return a


def pipe(label, p0, p1, diameter, mi, folder):
    """p0 → p1 (월드 좌표) 원기둥. 충돌 없음 (머리 높이에 매달린 파이프에 걸리지 않게)"""
    d = p1 - p0
    length = d.length()
    mid = unreal.Vector((p0.x + p1.x) / 2.0, (p0.y + p1.y) / 2.0, (p0.z + p1.z) / 2.0)
    a = spawn(unreal.StaticMeshActor, mid, label=label, folder=folder)
    a.set_actor_rotation(unreal.MathLibrary.make_rot_from_z(d), False)
    smc = a.static_mesh_component
    smc.set_static_mesh(CYLINDER)
    smc.set_material(0, mi)
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    smc.set_editor_property("can_ever_affect_navigation", False)
    a.set_actor_scale3d(unreal.Vector(diameter / 100.0, diameter / 100.0, length / 100.0))
    return a


def point_light(label, x, y, z, candela, color=(1.0, 0.97, 0.9), radius=1000.0, shadows=True):
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


def lerp(a, b, t):
    """a + (b - a) * t (월드 좌표 벡터)"""
    return unreal.Vector(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t)


# ================================================================ 시작
grid = unreal.load_asset(MAT_DIR + "/M_VRP_Grid")
solid = unreal.load_asset(MAT_DIR + "/M_VRP_Solid")
MI_FLOOR = ensure_mi("MI_VRP_ObsFloor", grid, {"Color": (0.12, 0.14, 0.18), "LineColor": (0.22, 0.26, 0.32), "LineWidth": 2.0, "Roughness": 0.6})
MI_WALL = ensure_mi("MI_VRP_ObsWall", grid, {"Color": (0.20, 0.23, 0.28), "LineColor": (0.14, 0.16, 0.20), "LineWidth": 2.0, "Roughness": 0.85})
MI_CEIL = ensure_mi("MI_VRP_ObsCeiling", grid, {"Color": (0.10, 0.11, 0.14), "LineColor": (0.16, 0.18, 0.22), "LineWidth": 1.5, "Roughness": 0.9})
MI_BLUEPRINT = ensure_mi("MI_VRP_Blueprint", grid, {"Color": (0.04, 0.12, 0.30), "LineColor": (0.35, 0.55, 0.85), "LineWidth": 1.2, "GridSize": 25.0, "Roughness": 0.7})
MI_PAINT = ensure_mi("MI_VRP_Paint", solid, {"Color": (0.85, 0.85, 0.82), "Emissive": 0.15, "Roughness": 0.6})
MI_GAUGE = ensure_mi("MI_VRP_GaugeLine", solid, {"Color": (0.55, 0.5, 0.3), "Emissive": 0.25, "Roughness": 0.7})
MI_PIPE = ensure_mi("MI_VRP_Pipe", solid, {"Color": (0.9, 0.42, 0.08), "Emissive": 0.15, "Roughness": 0.4})
MI_HOLE = ensure_mi("MI_VRP_SlotHole", solid, {"Color": (0.01, 0.02, 0.04), "Emissive": 0.0, "Roughness": 0.9})
MI_TRIM = unreal.load_asset(MAT_DIR + "/MI_VRP_Trim")
MI_WARN = unreal.load_asset(MAT_DIR + "/MI_VRP_Warning")
MI_WINDOW = unreal.load_asset(MAT_DIR + "/MI_VRP_CoreWindow")

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
H = 450.0     # 천장 높이 (노션: 4.5m)
T = 20.0      # 벽 두께
DOOR_W, DOOR_H = 120.0, 220.0

box("Floor", 0, 1400, 0, 800, -10, 0, MI_FLOOR)
box("Ceiling", -T, 1400 + T, -T, 800 + T, H, H + 10, MI_CEIL)
box("Wall_South", -T, 1400 + T, -T, 0, 0, H, MI_WALL)
box("Wall_North", -T, 1400 + T, 800, 800 + T, 0, H, MI_WALL)
# 서쪽 벽: 입구 y 100~220
box("Wall_West_1", -T, 0, 0, 100, 0, H, MI_WALL)
box("Wall_West_2", -T, 0, 220, 800, 0, H, MI_WALL)
box("Wall_West_Header", -T, 0, 100, 220, DOOR_H, H, MI_WALL)
# 동쪽 벽: 출구 보안문 y 80~240, 높이 3m (문에 원 모양 홈이 들어갈 만큼 크게)
EXIT_W, EXIT_H = 160.0, 300.0
box("Wall_East_1", 1400, 1400 + T, 0, 80, 0, H, MI_WALL)
box("Wall_East_2", 1400, 1400 + T, 240, 800, 0, H, MI_WALL)
box("Wall_East_Header", 1400, 1400 + T, 80, 240, EXIT_H, H, MI_WALL)
# 앞방 ↔ 안쪽 칸막이: 셔터 y 340~460
box("Partition_1", 600, 600 + T, 0, 340, 0, H, MI_WALL)
box("Partition_2", 600, 600 + T, 460, 800, 0, H, MI_WALL)
box("Partition_Header", 600, 600 + T, 340, 460, DOOR_H, H, MI_WALL)
box("Shutter_Stripe", 596, 600, 340, 460, DOOR_H, DOOR_H + 8, MI_WARN, folder="Geometry/Trim")

# 코어가 보이는 창문 (안쪽 구역 북쪽 벽, 빛나는 유리)
box("Window_Glass", 820, 1220, 796, 800, 200, 380, MI_WINDOW, folder="Geometry/Window")
box("Window_FrameTop", 810, 1230, 792, 800, 380, 390, MI_TRIM, folder="Geometry/Window")
box("Window_FrameBottom", 810, 1230, 792, 800, 190, 200, MI_TRIM, folder="Geometry/Window")
box("Window_Mullion", 1015, 1025, 792, 800, 200, 380, MI_TRIM, folder="Geometry/Window")

# 도착 복도 (폭 2m, 천장 3m) + 뒤쪽 보안문 (L_02에서 들어온 문, 닫힌 채)
box("Arrive_Floor", -340, 0, 60, 260, -10, 0, MI_FLOOR, folder="Geometry/Arrive")
box("Arrive_Ceiling", -340, 0, 40, 280, 300, 310, MI_CEIL, folder="Geometry/Arrive")
box("Arrive_WallLeft", -340, -T, 40, 60, 0, 300, MI_WALL, folder="Geometry/Arrive")
box("Arrive_WallRight", -340, -T, 260, 280, 0, 300, MI_WALL, folder="Geometry/Arrive")
box("Arrive_WallEnd", -340, -320, 60, 260, 0, 300, MI_WALL, folder="Geometry/Arrive")

# 출구 복도 (폭 2m, 길이 6m, 천장 3m) + 끝의 장치 시험실 보안문
box("Exit_Floor", 1400, 2040, 60, 260, -10, 0, MI_FLOOR, folder="Geometry/Exit")
box("Exit_Ceiling", 1400, 2040, 40, 280, 300, 310, MI_CEIL, folder="Geometry/Exit")
box("Exit_WallLeft", 1400 + T, 2040, 40, 60, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Exit_WallRight", 1400 + T, 2040, 260, 280, 0, 300, MI_WALL, folder="Geometry/Exit")
# 끝 벽: 장치 시험실 보안문 y 100~220, 문 너머에 다음 스테이지로 넘어가는 작은 칸
box("Exit_WallEnd_1", 2020, 2040, 60, 100, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Exit_WallEnd_2", 2020, 2040, 220, 260, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Exit_WallEnd_Header", 2020, 2040, 100, 220, DOOR_H, 300, MI_WALL, folder="Geometry/Exit")
box("Gate_Floor", 2040, 2260, 60, 260, -10, 0, MI_FLOOR, folder="Geometry/Exit")
box("Gate_Ceiling", 2040, 2260, 40, 280, 300, 310, MI_CEIL, folder="Geometry/Exit")
box("Gate_WallLeft", 2040, 2260, 40, 60, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Gate_WallRight", 2040, 2260, 260, 280, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Gate_WallEnd", 2240, 2260, 60, 260, 0, 300, MI_WALL, folder="Geometry/Exit")

# ---------------------------------------------------------------- 조명 (어둡고 파란 빛) / 후처리
BLUE = (0.45, 0.6, 1.0)
point_light("Front_Light_1", 300, 200, 420, 18, color=BLUE, radius=900)
point_light("Front_Light_2", 300, 600, 420, 18, color=BLUE, radius=900)
point_light("Paint_Light", 450, 400, 380, 10, color=(0.8, 0.85, 1.0), radius=600, shadows=False)
point_light("Inner_Light_1", 850, 250, 420, 18, color=BLUE, radius=900)
point_light("Inner_Light_2", 1200, 550, 420, 18, color=BLUE, radius=900)
point_light("Desk_Light", 1050, 690, 220, 12, color=(1.0, 0.85, 0.6), radius=400)
point_light("Window_Glow", 1020, 760, 290, 15, color=(0.4, 0.6, 1.0), radius=700, shadows=False)
point_light("Blueprint_Glow", 1300, 550, 240, 6, color=(0.5, 0.7, 1.0), radius=500, shadows=False)
point_light("ExitDoor_Light", 1280, 160, 330, 10, color=(0.85, 0.9, 1.0), radius=400, shadows=False)
point_light("Arrive_Light", -170, 160, 280, 15, radius=500)
point_light("Exit_Light", 1720, 160, 280, 12, radius=600)

pp = spawn(unreal.PostProcessVolume, V(0, 0, 0), label="PostProcess", folder="Lighting")
pp.set_editor_property("unbound", True)
if intro_pp is not None:
    pp.set_editor_property("settings", intro_pp)

# ---------------------------------------------------------------- 도착 / 플레이어 / 내비게이션
arrive_door = spawn(unreal.PuzzleDoor, V(-310, 160, 0), label="S3_ArrivalDoor", folder="Navigation")
arrive_door.set_editor_property("door_color", unreal.LinearColor(0.2, 0.22, 0.25, 1.0))
spawn(unreal.PlayerStart, V(-200, 160, 1), yaw=0.0, label="PlayerStart", folder="Navigation")

nav = spawn(unreal.NavMeshBoundsVolume, V(850, 400, 200), label="NavMeshBounds", folder="Navigation")
nav.set_actor_scale3d(unreal.Vector(2500 / 200.0, 900 / 200.0, 500 / 200.0))

# ================================================================ 3-1 발자국과 페인트
FOOT = (250.0, 400.0)                       # 발자국 (정답 위치)
EYE_H = 160.0                               # 노션: 발자국 위치 눈높이 160cm에서 맞춰 만든다
EYE = V(FOOT[0], FOOT[1], EYE_H)
PAINT_PLANE_X = 597.0                       # 글씨가 모여 보이는 면 (셔터 칸막이 바로 앞)
PAINT_LINES = [("코어를", "보지", "마"), ("1114",)]

# 발자국 한 쌍 (흰 페인트)
for i, dy in enumerate((-13.0, 13.0)):
    fx, fy = FOOT[0], FOOT[1] + dy
    box("S3_Foot_%d_Sole" % i, fx - 4, fx + 18, fy - 5, fy + 5, 0, 0.4, MI_PAINT, folder="Puzzle/Paint", collision=False)
    box("S3_Foot_%d_Heel" % i, fx - 16, fx - 6, fy - 4, fy + 4, 0, 0.4, MI_PAINT, folder="Puzzle/Paint", collision=False)

# 글자 하나하나를 서로 다른 거리에 둔다: 실제 위치 = 눈 + (면 위 위치 - 눈) x s, 크기도 s배
# → 발자국 위 눈높이에서만 면 위의 한 줄 글씨로 겹쳐 보이고, 다른 곳에서는 흩어져 보인다
GLYPH = 34.0          # 면 위에서 글자 한 칸 (cm)
paint_pieces = []
for row, words in enumerate(PAINT_LINES):
    glyphs = list("  ".join(words)) if row == 0 else list(words[0])
    line_z = 260.0 - row * 75.0
    glyph = GLYPH if row == 0 else 44.0
    width = len(glyphs) * glyph
    for i, ch in enumerate(glyphs):
        if ch == " ":
            continue
        plane = V(PAINT_PLANE_X, FOOT[1] - width / 2.0 + glyph * (i + 0.5), line_z)
        s = random.uniform(0.38, 1.0)
        loc = lerp(EYE, plane, s)
        piece = sign_at("S3_Paint_%d_%02d" % (row, i), ch, loc, yaw=180.0, size=(240, 240), font=120,
                        ppc=240.0 / (glyph * s), text_color=(0.4, 0.4, 0.38), bg=(0, 0, 0, 0), folder="Puzzle/Paint")
        paint_pieces.append(piece)

paint_view = spawn(unreal.PuzzleViewPoint, V(FOOT[0], FOOT[1], 0), label="S3_PaintView", folder="Puzzle")
paint_view.set_editor_property("look_target", V(PAINT_PLANE_X, FOOT[1], 222))
paint_view.set_editor_property("pieces", paint_pieces)
paint_view.set_editor_property("piece_color", unreal.LinearColor(0.95, 0.95, 0.9, 1.0))

# 셔터 + 키패드 (손 높이 120cm, 셔터 오른쪽)
shutter = spawn(unreal.PuzzleDoor, V(610, 400, 0), label="S3_Shutter", folder="Puzzle")
shutter.set_editor_property("door_color", unreal.LinearColor(0.28, 0.3, 0.33, 1.0))
keypad = spawn(unreal.PuzzleKeypad, V(600, 530, 120), yaw=180.0, label="S3_Keypad", folder="Puzzle")
keypad.set_editor_property("code", "1114")
sign("S3_Sign_Keypad", "관측 구역 출입 · 비밀번호 4자리", 599, 530, 165, yaw=180.0, size=(560, 70), font=26,
     bg=(0.1, 0.12, 0.15, 1.0), text_color=(0.7, 0.8, 0.9))

# ================================================================ 3-2 파이프로 만드는 원
GAUGE = (820.0, 250.0)                      # 눈금 선 두 개가 만나는 곳 (정답 위치)
G_EYE = V(GAUGE[0], GAUGE[1], EYE_H)
RING_C = V(1405, 160, 215)                  # 원이 겹쳐 보이는 곳: 출구 보안문에 새겨진 빈 원 홈 (노션)
RING_R = 55.0
PIPE_D = 10.0

# 바닥의 오래된 측정 눈금 선 두 개 (+ 50cm마다 눈금)
box("S3_Gauge_X", 660, 1340, GAUGE[1] - 1.5, GAUGE[1] + 1.5, 0, 0.3, MI_GAUGE, folder="Puzzle/Gauge", collision=False)
box("S3_Gauge_Y", GAUGE[0] - 1.5, GAUGE[0] + 1.5, 30, 760, 0, 0.3, MI_GAUGE, folder="Puzzle/Gauge", collision=False)
for i, gx in enumerate(range(670, 1340, 50)):
    box("S3_GaugeTick_X_%d" % i, gx - 1, gx + 1, GAUGE[1] - 6, GAUGE[1] + 6, 0, 0.3, MI_GAUGE, folder="Puzzle/Gauge", collision=False)
for i, gy in enumerate(range(50, 760, 50)):
    box("S3_GaugeTick_Y_%d" % i, GAUGE[0] - 6, GAUGE[0] + 6, gy - 1, gy + 1, 0, 0.3, MI_GAUGE, folder="Puzzle/Gauge", collision=False)


def ring_point(theta):
    """벽(x = RING_C.x) 위의 원 위 점"""
    return unreal.Vector(RING_C.x, RING_C.y + RING_R * math.cos(theta), RING_C.z + RING_R * math.sin(theta))


# 출구 보안문 (잠금 2개, 폭 1.6m x 높이 3m). 문 바깥면(방 쪽, x 1406)에 빈 원 모양 홈
exit_door = spawn(unreal.PuzzleDoor, V(1410, 160, 0), label="S3_ExitDoor", folder="Puzzle")
exit_door.set_editor_property("width", EXIT_W)
exit_door.set_editor_property("height", EXIT_H)
door_panel = [c for c in exit_door.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "Panel"][0]

# 홈은 짧은 조각 24개. 문 판에 붙여서 문이 열리면 같이 밀려 들어간다. 파이프와 같이 맞추면 불이 들어온다
pipe_pieces = []
SEG = 24
for i in range(SEG):
    a0, a1 = 2 * math.pi * i / SEG, 2 * math.pi * (i + 1) / SEG
    groove = pipe("S3_RingGroove_%02d" % i, ring_point(a0), ring_point(a1), 4.0, MI_TRIM, "Puzzle/Pipes")
    groove.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    groove.attach_to_component(door_panel, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                               unreal.AttachmentRule.KEEP_WORLD, False)
    pipe_pieces.append(groove)

# 천장에 매달린 파이프 5개: 원을 5등분한 호를 서로 다른 거리(s)에 둔다 → 교차점 눈높이에서만 원 하나
# 원 아래쪽은 눈높이와 거의 같아서, 아래쪽 호는 가장 앞(교차점 쪽, s 작음)에 둔다 → 출구로 가는 길목을 덜 막음
ARC0 = -math.pi / 2 - math.pi / 5          # 첫 호가 원 맨 아래를 가운데로 갖도록
depths = [0.38] + random.sample([0.52, 0.64, 0.76, 0.88], 4)
for k in range(5):
    s = depths[k]
    a_start = ARC0 + 2 * math.pi * k / 5 + 0.03
    a_end = ARC0 + 2 * math.pi * (k + 1) / 5 - 0.03
    pts = []
    for j in range(7):
        q = ring_point(a_start + (a_end - a_start) * j / 6)
        pts.append(lerp(G_EYE, q, s))
    for j in range(6):
        pipe_pieces.append(pipe("S3_Pipe_%d_%d" % (k, j), pts[j], pts[j + 1], PIPE_D * s, MI_PIPE, "Puzzle/Pipes"))
    # 천장에서 내려온 매달림 줄 (호 가운데에서 천장까지)
    top = pts[3]
    pipe("S3_PipeWire_%d" % k, top, unreal.Vector(top.x, top.y, ORIGIN.z + H), 1.2, MI_TRIM, "Puzzle/Pipes")

pipe_view = spawn(unreal.PuzzleViewPoint, V(GAUGE[0], GAUGE[1], 0), label="S3_PipeView", folder="Puzzle")
pipe_view.set_editor_property("look_target", RING_C)
pipe_view.set_editor_property("pieces", pipe_pieces)
pipe_view.set_editor_property("piece_color", unreal.LinearColor(0.95, 0.45, 0.08, 1.0))

# ================================================================ 3-3 도면의 빠진 조각
# 동쪽 벽의 코어 도면 (가로 4.4m x 세로 3m, 벽에서 4cm 나옴). 빈 칸 1m: y 500~600, z 180~280
BX = 1396.0
box("S3_Blueprint_Left", BX, 1400, 330, 500, 80, 380, MI_BLUEPRINT, folder="Puzzle/Blueprint")
box("S3_Blueprint_Right", BX, 1400, 600, 770, 80, 380, MI_BLUEPRINT, folder="Puzzle/Blueprint")
box("S3_Blueprint_Bottom", BX, 1400, 500, 600, 80, 180, MI_BLUEPRINT, folder="Puzzle/Blueprint")
box("S3_Blueprint_Top", BX, 1400, 500, 600, 280, 380, MI_BLUEPRINT, folder="Puzzle/Blueprint")
box("S3_Blueprint_Hole", 1399, 1400, 500, 600, 180, 280, MI_HOLE, folder="Puzzle/Blueprint")
box("S3_Blueprint_FrameTop", BX - 2, 1400, 325, 775, 380, 386, MI_TRIM, folder="Puzzle/Blueprint")
box("S3_Blueprint_FrameBottom", BX - 2, 1400, 325, 775, 74, 80, MI_TRIM, folder="Puzzle/Blueprint")
sign("S3_Sign_BlueprintTitle", "인지 증폭 코어 · 내부 구조도 (2006 rev.3)", 1395, 550, 360, yaw=180.0, size=(1200, 80), font=30,
     bg=(0, 0, 0, 0), text_color=(0.6, 0.78, 1.0))
core_diagram = sign("S3_CoreDiagram", "출력 측정점 A ─── ◎ 증폭 코어 ─── 출력 측정점 B", 1395, 550, 110, yaw=180.0, size=(1500, 80), font=30,
                    bg=(0, 0, 0, 0), text_color=(0.25, 0.35, 0.5))

# 코어 내부 구조 그림 (도면 위의 선). 평소엔 흐리고, 빈 칸을 채우면 빛난다 (노션: "코어 내부 구조가 빛나며")
DX, DC = 1394.5, (550.0, 230.0)       # 그림 면 / 코어 가운데 (= 빈 칸 가운데)
MI_BPLINE = ensure_mi("MI_VRP_BlueprintLine", solid, {"Color": (0.4, 0.75, 1.0), "Emissive": 0.3, "Roughness": 0.5})
diagram = []


def dline(label, y0, z0, y1, z1):
    diagram.append(pipe(label, V(DX, y0, z0), V(DX, y1, z1), 2.5, MI_BPLINE, "Puzzle/Blueprint/Diagram"))


for name, r in (("Inner", 75.0), ("Outer", 95.0)):
    for i in range(32):
        a0, a1 = 2 * math.pi * i / 32, 2 * math.pi * (i + 1) / 32
        dline("S3_Diagram_%s_%02d" % (name, i), DC[0] + r * math.cos(a0), DC[1] + r * math.sin(a0),
              DC[0] + r * math.cos(a1), DC[1] + r * math.sin(a1))
for i in range(8):   # 증폭 코일 살
    a = 2 * math.pi * (i + 0.5) / 8
    dline("S3_Diagram_Spoke_%d" % i, DC[0] + 95 * math.cos(a), DC[1] + 95 * math.sin(a),
          DC[0] + 125 * math.cos(a), DC[1] + 125 * math.sin(a))
for side, sy in (("A", -1.0), ("B", 1.0)):   # 출력 측정점 A / B 로 가는 배선 + 측정점 네모
    y_end = DC[0] + sy * 195
    dline("S3_Diagram_Wire_%s" % side, DC[0] + sy * 95, DC[1], y_end, DC[1])
    for j, (ya, za, yb, zb) in enumerate(((-10, -10, 10, -10), (10, -10, 10, 10), (10, 10, -10, 10), (-10, 10, -10, -10))):
        dline("S3_Diagram_Probe_%s_%d" % (side, j), y_end + sy * 10 + ya, DC[1] + za, y_end + sy * 10 + yb, DC[1] + zb)
for i, dy in enumerate((-30.0, 0.0, 30.0)):   # 위쪽 출력관 3개
    z0 = DC[1] + math.sqrt(95 * 95 - dy * dy)
    dline("S3_Diagram_Out_%d" % i, DC[0] + dy, z0, DC[0] + dy, 345)

# 빛 묶음: 평소엔 흐린 파란 선, 3-3을 풀면 진행 담당이 Solve()로 켠다 (태그 CoreDiagram).
# 판정 영역을 바닥 아래에 둬서 플레이어가 서서 켜는 일은 없다
diagram_glow = spawn(unreal.PuzzleViewPoint, V(1300, 550, -3000), label="S3_CoreDiagramGlow", folder="Puzzle")
diagram_glow.set_editor_property("pieces", diagram)
diagram_glow.set_editor_property("piece_color", unreal.LinearColor(0.35, 0.75, 1.0, 1.0))
diagram_glow.set_editor_property("glow_radius", 1.0)
diagram_glow.set_editor_property("idle_emissive", 0.0)
diagram_glow.set_editor_property("solved_emissive", 4.0)
diagram_glow.set_editor_property("tags", ["CoreDiagram"])

slot = spawn(unreal.PuzzleBlueprintSlot, V(1399, 550, 230), yaw=180.0, label="S3_BlueprintSlot", folder="Puzzle")
slot.set_editor_property("slot_size", 100.0)

# 빈 칸에서 5m 떨어진 바닥의 엷은 ╳ (여기서 조각을 눈앞 1m에 들고 놓으면 5배로 딱 맞음)
MARK = (BX - 500.0, 550.0)
for i, yaw in enumerate((45.0, -45.0)):
    m = spawn(unreal.StaticMeshActor, V(MARK[0], MARK[1], 0.15), yaw=yaw, label="S3_StandMark_%d" % i, folder="Puzzle/Blueprint")
    m.static_mesh_component.set_static_mesh(CUBE)
    m.static_mesh_component.set_material(0, MI_GAUGE)
    m.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    m.set_actor_scale3d(unreal.Vector(0.4, 0.025, 0.003))

# 책상 (90 x 180, 상판 74.5cm) + 도면 조각 + 녹음기
DESK = (1050.0, 690.0)
DESK_TOP = 74.5
spawn(BP_DESK, V(DESK[0], DESK[1], 0), label="S3_Desk", folder="Props")

piece = spawn(BP_PCUBE, V(DESK[0], DESK[1] - 25, DESK_TOP + 1.5), label="S3_BlueprintPiece", folder="Puzzle")
piece.set_actor_scale3d(unreal.Vector(0.2, 0.2, 0.02))   # 20cm x 20cm x 2cm 판
piece.set_editor_property("color", unreal.LinearColor(0.15, 0.35, 0.85, 1.0))
piece.set_editor_property("emissive", 0.4)
piece.set_editor_property("tags", ["BlueprintPiece"])

# 조각을 잃어버렸을 때 새로 받는 보관함 (책상 옆)
dispenser = spawn(unreal.PuzzleDispenser, V(DESK[0] + 140, 760, 0), yaw=-90.0, label="S3_Dispenser_Piece", folder="Puzzle")
dispenser.set_editor_property("spawn_class", BP_PCUBE)
dispenser.set_editor_property("spawn_size", 20.0)
dispenser.set_editor_property("spawn_shape", unreal.Vector(1.0, 1.0, 0.1))
dispenser.set_editor_property("spawn_color", unreal.LinearColor(0.15, 0.35, 0.85, 1.0))
dispenser.set_editor_property("spawn_tags", ["BlueprintPiece"])
dispenser.set_editor_property("spawn_on_start", False)
dispenser.set_editor_property("replace_actor", piece)
sign("S3_Sign_Dispenser", "도면 조각 보관함", DESK[0] + 140, 799, 140, yaw=-90.0, size=(380, 80), font=28,
     bg=(0.15, 0.15, 0.15, 1.0), text_color=(0.6, 0.85, 1.0))

recorder = spawn(unreal.PuzzleRecorder, V(DESK[0] - 10, DESK[1] + 25, DESK_TOP), yaw=180.0, label="S3_Recorder", folder="Story")
recorder.set_editor_property("title", unreal.Text("음성 기록 #1  ·  2006.11.14 14:07"))
recorder.set_editor_property("lines", [
    unreal.Text("(경보음, 기계가 윙윙거리는 소리)"),
    unreal.Text("강한결: 소장님, 출력이 안정 범위를 넘었습니다.\n지금 멈춰야 해요!"),
    unreal.Text("권혁수: 투자자들이 보고 있어. 계속 진행해!"),
    unreal.Text("오나연: 선배… 코어가 자꾸 커져요.\n아니, 제가… 작아지는 것 같아요."),
    unreal.Text("(잡음, 녹음 끊김)"),
])

# ================================================================ 출구 문 (잠금 2개) + 출구 복도
lock_ring = sign("S3_Lock_Ring", "◯  잠김", 1399, 120, 330, yaw=180.0, size=(300, 80), font=30,
                 bg=(0.05, 0.05, 0.06, 1.0), text_color=(1.0, 0.3, 0.2), folder="Puzzle")
lock_bp = sign("S3_Lock_Blueprint", "▣  잠김", 1399, 200, 330, yaw=180.0, size=(300, 80), font=30,
               bg=(0.05, 0.05, 0.06, 1.0), text_color=(1.0, 0.3, 0.2), folder="Puzzle")

device_door = spawn(unreal.PuzzleDoor, V(2030, 160, 0), label="S3_DeviceDoor", folder="Puzzle")
device_door.set_editor_property("door_color", unreal.LinearColor(0.2, 0.22, 0.25, 1.0))
gate = spawn(unreal.PuzzleStageGate, V(2030, 160, 0), label="S3_StageGate", folder="Navigation")
gate.set_editor_property("next_level", "/Game/VRPuzzle/Maps/L_04_Device")
gate.set_editor_property("door", device_door)

# ---------------------------------------------------------------- 글자 (대사 초안)
sign("S3_Sign_Title", "3. 관측실\n시점 측정 구역", 1, 600, 300, yaw=0.0, size=(700, 220), font=40,
     bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))
sign("S3_Sign_ArriveDoor", "2. 원근 실험실", -319, 160, 255, yaw=0.0, size=(500, 90), font=34,
     bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))
sign("S3_Sign_Device", "4. 장치 시험실", 2019, 160, 260, yaw=180.0, size=(500, 160), font=34,
     bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))

# ---------------------------------------------------------------- 진행 담당
director = spawn(unreal.ObservationStageDirector, V(0, 0, 400), label="S3_StageDirector", folder="Puzzle")
director.set_editor_property("paint_view", paint_view)
director.set_editor_property("keypad", keypad)
director.set_editor_property("shutter", shutter)
director.set_editor_property("pipe_view", pipe_view)
director.set_editor_property("slot", slot)
director.set_editor_property("exit_door", exit_door)
director.set_editor_property("lock_lamps", [lock_ring, lock_bp])
director.set_editor_property("core_diagram", core_diagram)

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("L_03_Observation 저장 완료 (페인트 조각 %d, 파이프 조각 %d)" % (len(paint_pieces), len(pipe_pieces)))

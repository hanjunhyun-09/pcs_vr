# -*- coding: utf-8 -*-
"""
L_05_Core (5스테이지: 중앙 코어) 그레이박스를 만든다.
노션 "맵 구조 설계 > L_05_Core" / "5스테이지 퍼즐 설계" / "대사 초안" 기준.

실행 방법 (둘 중 하나, C++ 빌드 후):
  1) 에디터를 닫은 상태에서:
     UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_l05_core.py"
  2) 에디터를 켠 채로: Python 원격 실행으로 이 파일을 보내면 열린 에디터에서 바로 만들어진다

다시 실행하면 L_05_Core 안의 액터를 모두 지우고 처음부터 다시 만든다.
크기 단위는 cm. 스테이지 5번이라 원점은 X = 500m (50000). 원점 = 코어 홀 가운데 바닥.

구역 (원점 기준, 바닥 높이 0, 북쪽 = +Y):
  코어 홀        지름 18m 원형 (24각형 벽), 천장 10m. 가운데 받침대(지름 5.2m, 높이 50cm) 위 코어(지름 3m, 높이 5m)
                 코어 슬롯 3개는 받침대 위 1.5m: ● 서쪽(원근법 구역 쪽) / ▲ 북쪽(발코니 쪽) / ■ 동쪽(장치 구역 쪽)
  원근법 구역    서쪽 x -1712~-912, y -300~300 (8m x 6m), 천장 4m. 홀로 나가는 문은 좁은문(폭 60cm, x -902)
                 3m 코어 축 + 보관함(디스펜서). 좁은문에서 ● 슬롯까지 약 7.2m (노션: 6m 이상)
  시점 구역      북쪽 2층 발코니 x -450~450, y 620~900, 높이 4m. 텔레포트 발판으로 오르내림
                 관측 위치 ① (-250, 700) → ◆ ●, ② (250, 700) → ▲ ■. 뒤쪽 벽에 ▲ 부품 보관함
  장치 구역      동쪽 x 912~1412, y -250~250 (5m x 5m), 천장 3m. 장치 받침대 + 동쪽 벽 코어 제어판(램프 4개)
  엘리베이터     남쪽 벽 너머: 도착(x -130, L_04에서 타고 옴) / 출구(x 130, 코어가 멈추면 열림)
"""
import math
import random

import unreal

MAP = "/Game/VRPuzzle/Maps/L_05_Core"
MAT_DIR = "/Game/VRPuzzle/Materials"
ORIGIN = unreal.Vector(50000.0, 0.0, 0.0)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
BP_PCUBE = eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Interaction/BP_PerspectiveCube")
BP_GRAB = eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Interaction/BP_Keycard")   # 잡을 수 있는 큐브 (GrabComponent 포함)
BP_DESK = eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Props/BP_Desk")

random.seed(1114)   # 조각 배치가 매번 같게


def log(msg):
    unreal.log_warning("[L05] " + msg)


def V(x, y, z):
    return unreal.Vector(ORIGIN.x + x, ORIGIN.y + y, ORIGIN.z + z)


def lerp(a, b, t):
    return unreal.Vector(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t)


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


# ---------------------------------------------------------------- 레벨 도우미 (build_l04_device.py와 같은 방식)
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


def rbox(label, cx, cy, z1, z2, length, thick, yaw, mi, folder="Geometry/HallWall"):
    """가운데 (cx, cy), 로컬 X 길이 length, Y 두께 thick, yaw만큼 돌린 상자 (원형 홀 벽 조각)"""
    a = spawn(unreal.StaticMeshActor, V(cx, cy, (z1 + z2) / 2.0), yaw=yaw, label=label, folder=folder)
    smc = a.static_mesh_component
    smc.set_static_mesh(CUBE)
    smc.set_material(0, mi)
    a.set_actor_scale3d(unreal.Vector(length / 100.0, thick / 100.0, (z2 - z1) / 100.0))
    return a


def cyl(label, x, y, z1, z2, diameter, mi, folder="Geometry", collision=True, movable=False):
    a = spawn(unreal.StaticMeshActor, V(x, y, (z1 + z2) / 2.0), label=label, folder=folder)
    smc = a.static_mesh_component
    if movable:
        smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    smc.set_static_mesh(CYLINDER)
    smc.set_material(0, mi)
    if not collision:
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        smc.set_editor_property("can_ever_affect_navigation", False)
    a.set_actor_scale3d(unreal.Vector(diameter / 100.0, diameter / 100.0, (z2 - z1) / 100.0))
    return a


def pipe(label, p0, p1, diameter, mi, folder):
    """p0 → p1 (월드 좌표) 원기둥. 충돌 없음 (허공에 떠 있는 페인트 조각에 걸리지 않게)"""
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


PLATE = dict(bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))


def attach(child, parent):
    child.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, False)


def recorder(label, x, y, z, title, lines, yaw=-90.0):
    r = spawn(unreal.PuzzleRecorder, V(x, y, z), yaw=yaw, label=label, folder="Story/Recorders")
    r.set_editor_property("title", unreal.Text(title))
    r.set_editor_property("lines", [unreal.Text(t) for t in lines])
    return r


def part(label, x, y, z, size, color, tag, symbol, folder="Puzzle/Parts"):
    """잡아서 코어에 끼우는 부품 (잡을 수 있는 큐브 + 윗면 기호)"""
    p = spawn(BP_GRAB, V(x, y, z), label=label, folder=folder)
    p.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    p.set_editor_property("color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    p.set_editor_property("emissive", 0.6)
    p.set_editor_property("tags", [tag])
    mark = sign(label + "_Symbol", symbol, x, y, z + size[2] / 2.0 + 0.3, pitch=90.0, size=(200, 200), font=150, ppc=200.0 / (size[0] * 0.8),
                bg=(0, 0, 0, 0), text_color=(0.05, 0.05, 0.06), folder=folder)
    attach(mark, p)
    return p


# ================================================================ 시작
grid = unreal.load_asset(MAT_DIR + "/M_VRP_Grid")
solid = unreal.load_asset(MAT_DIR + "/M_VRP_Solid")
MI_FLOOR = ensure_mi("MI_VRP_CoreFloor", grid, {"Color": (0.15, 0.16, 0.18), "LineColor": (0.24, 0.26, 0.3), "LineWidth": 2.0, "Roughness": 0.6})
MI_WALL = ensure_mi("MI_VRP_CoreWall", grid, {"Color": (0.26, 0.28, 0.31), "LineColor": (0.2, 0.22, 0.25), "LineWidth": 2.0, "Roughness": 0.85})
MI_CEIL = ensure_mi("MI_VRP_CoreCeiling", grid, {"Color": (0.1, 0.1, 0.12), "LineColor": (0.16, 0.16, 0.18), "LineWidth": 1.5, "Roughness": 0.9})
MI_PGRID = ensure_mi("MI_VRP_CorePerspFloor", grid, {"Color": (0.85, 0.86, 0.88), "LineColor": (0.35, 0.38, 0.45), "LineWidth": 2.0, "Roughness": 0.7})
MI_METAL = ensure_mi("MI_VRP_CoreMetal", solid, {"Color": (0.32, 0.34, 0.37), "Emissive": 0.0, "Roughness": 0.4})
MI_DARK = ensure_mi("MI_VRP_CoreDark", solid, {"Color": (0.03, 0.035, 0.04), "Emissive": 0.0, "Roughness": 0.5})
MI_CORE = ensure_mi("MI_VRP_CoreGlow", solid, {"Color": (0.55, 0.75, 1.0), "Emissive": 2.0, "Roughness": 0.3})
MI_PAINT = ensure_mi("MI_VRP_CorePaint", solid, {"Color": (0.95, 0.9, 0.8), "Emissive": 0.15, "Roughness": 0.8})
MI_LAMP = ensure_mi("MI_VRP_PanelLampGlow", solid, {"Color": (1.0, 0.65, 0.2), "Emissive": 6.0, "Roughness": 0.3})
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

# ================================================================ 코어 홀 (지름 18m, 천장 10m)
R = 900.0          # 홀 안쪽 반지름
HH = 1000.0        # 홀 천장 높이 (노션: 10m)
T = 20.0
DOOR_H = 220.0
SEG = 24
SKIP = {0, 180, 255, 270, 285}   # 동(장치 구역) / 서(원근법 구역) / 남(엘리베이터 2대) 자리는 평평한 벽으로 따로 만든다
CHORD = R * math.cos(math.radians(7.5))   # 빠진 조각 자리의 평평한 벽 안쪽 면 (= 892)

# 바닥은 남쪽 벽까지만 (엘리베이터는 자기 바닥이 있어서 겹치면 깜빡임). 동·서 구역 바닥은 x ±1000부터
box("Hall_Floor", -1000, 1000, -R * math.sin(math.radians(67.5)) - T, 1000, -10, 0, MI_FLOOR)
box("Hall_Ceiling", -1000, 1000, -1000, 1000, HH, HH + 10, MI_CEIL)
seg_len = 2 * (R + T) * math.tan(math.radians(180.0 / SEG)) + 6
for k in range(SEG):
    deg = k * 360.0 / SEG
    if int(round(deg)) in SKIP:
        continue
    a = math.radians(deg)
    rbox("Hall_Wall_%02d" % k, (R + T / 2) * math.cos(a), (R + T / 2) * math.sin(a), 0, HH, seg_len, T, deg + 90.0, MI_WALL)

# 서쪽 평평한 벽 + 좁은문 (노션: 폭 60cm, 높이 220cm. 플레이어는 지나가지만 3m 축은 못 지나감)
NARROW = 30.0
box("West_Wall_S", -CHORD - T, -CHORD, -320, -NARROW, 0, HH, MI_WALL, folder="Geometry/HallWall")
box("West_Wall_N", -CHORD - T, -CHORD, NARROW, 320, 0, HH, MI_WALL, folder="Geometry/HallWall")
box("West_Wall_Header", -CHORD - T, -CHORD, -NARROW, NARROW, DOOR_H, HH, MI_WALL, folder="Geometry/HallWall")
box("NarrowDoor_Frame_S", -CHORD - T - 2, -CHORD + 2, -NARROW - 6, -NARROW, 0, DOOR_H, MI_WARN, folder="Geometry/Trim", collision=False)
box("NarrowDoor_Frame_N", -CHORD - T - 2, -CHORD + 2, NARROW, NARROW + 6, 0, DOOR_H, MI_WARN, folder="Geometry/Trim", collision=False)
box("NarrowDoor_Frame_Top", -CHORD - T - 2, -CHORD + 2, -NARROW - 6, NARROW + 6, DOOR_H, DOOR_H + 6, MI_WARN, folder="Geometry/Trim", collision=False)

# 동쪽 평평한 벽 + 장치 구역 문 (폭 120cm)
box("East_Wall_S", CHORD, CHORD + T, -270, -60, 0, HH, MI_WALL, folder="Geometry/HallWall")
box("East_Wall_N", CHORD, CHORD + T, 60, 270, 0, HH, MI_WALL, folder="Geometry/HallWall")
box("East_Wall_Header", CHORD, CHORD + T, -60, 60, DOOR_H, HH, MI_WALL, folder="Geometry/HallWall")

# 남쪽 평평한 벽 (24각형 꼭짓점 247.5° ~ 292.5°를 잇는 현) + 엘리베이터 문 2개 (x -190~-70 도착 / 70~190 출구)
SOUTH = -R * math.sin(math.radians(67.5))   # = -831
box("South_Wall_W", -370, -190, SOUTH - T, SOUTH, 0, HH, MI_WALL, folder="Geometry/HallWall")
box("South_Wall_M", -70, 70, SOUTH - T, SOUTH, 0, HH, MI_WALL, folder="Geometry/HallWall")
box("South_Wall_E", 190, 370, SOUTH - T, SOUTH, 0, HH, MI_WALL, folder="Geometry/HallWall")
box("South_Wall_Header_W", -190, -70, SOUTH - T, SOUTH, DOOR_H, HH, MI_WALL, folder="Geometry/HallWall")
box("South_Wall_Header_E", 70, 190, SOUTH - T, SOUTH, DOOR_H, HH, MI_WALL, folder="Geometry/HallWall")
box("Elevator_Stripe_W", -190, -70, SOUTH, SOUTH + 4, DOOR_H, DOOR_H + 8, MI_WARN, folder="Geometry/Trim", collision=False)
box("Elevator_Stripe_E", 70, 190, SOUTH, SOUTH + 4, DOOR_H, DOOR_H + 8, MI_WARN, folder="Geometry/Trim", collision=False)

# 기둥 4개 (노션: 홀의 기둥과 바닥에 페인트 조각)
for i, deg in enumerate((45, 135, 225, 315)):
    a = math.radians(deg)
    cyl("Hall_Pillar_%d" % i, 650 * math.cos(a), 650 * math.sin(a), 0, HH, 80, MI_METAL, folder="Geometry/Pillars")

# ================================================================ 코어 + 받침대 + 슬롯 3개
PED_H = 50.0
SLOT_Z = PED_H + 150.0   # 노션: 슬롯은 받침대 위 1.5m
CORE_R = 150.0
cyl("Core_Pedestal", 0, 0, 0, PED_H, 520, MI_METAL, folder="Core")
cyl("Core_PedestalRim", 0, 0, PED_H, PED_H + 4, 330, MI_TRIM, folder="Core", collision=False)
core = cyl("Core_Body", 0, 0, PED_H, PED_H + 500, CORE_R * 2, MI_CORE, folder="Core", movable=True)
cyl("Core_CapTop", 0, 0, PED_H + 500, PED_H + 530, 320, MI_METAL, folder="Core")
for i, z in enumerate((PED_H + 120, PED_H + 280, PED_H + 440)):   # 코어를 감은 금속 띠
    cyl("Core_Band_%d" % i, 0, 0, z, z + 12, CORE_R * 2 + 12, MI_METAL, folder="Core", collision=False)
# 천장에서 내려오는 굵은 배선 4개
for i, deg in enumerate((0, 90, 180, 270)):
    a = math.radians(deg + 45)
    cyl("Core_Cable_%d" % i, 120 * math.cos(a), 120 * math.sin(a), PED_H + 530, HH, 30, MI_DARK, folder="Core", collision=False)

core_lights = []
for i, deg in enumerate((45, 135, 225, 315)):
    a = math.radians(deg)
    core_lights.append(point_light("Core_Light_%d" % i, 320 * math.cos(a), 320 * math.sin(a), 300, 45, color=(0.55, 0.72, 1.0),
                                   radius=1400, shadows=False))

SLOT_FACE = CORE_R + 26.0   # 슬롯 판의 바깥 면 (코어 표면에서 26cm 나옴)


def slot_plate(name, deg, half):
    """코어 표면에서 튀어나온 슬롯 판. deg = 바깥쪽 방향, half = 판 반 폭"""
    a = math.radians(deg)
    c = (CORE_R + 13.0)
    p = spawn(unreal.StaticMeshActor, V(c * math.cos(a), c * math.sin(a), SLOT_Z), yaw=deg, label="Core_SlotPlate_" + name, folder="Core/Slots")
    p.static_mesh_component.set_static_mesh(CUBE)
    p.static_mesh_component.set_material(0, MI_DARK)
    p.set_actor_scale3d(unreal.Vector(0.26, half * 2 / 100.0, half * 2 / 100.0))
    return a


slot_labels = []

# ● 코어 축 슬롯 (서쪽, 원근법으로 크기를 맞춰 끼움. 노션: 규격 1.2m, 판정 85% ~ 115%)
slot_plate("Axis", 180.0, 75.0)
axis_slot = spawn(unreal.PuzzleBlueprintSlot, V(-SLOT_FACE, 0, SLOT_Z), yaw=180.0, label="S5_AxisSlot", folder="Puzzle/Core")
axis_slot.set_editor_property("slot_size", 120.0)
axis_slot.set_editor_property("sensor_depth", 120.0)   # 긴 축은 가운데가 슬롯 면에서 멀어서 감지 깊이를 넉넉히
axis_slot.set_editor_property("required_tag", "CoreAxis")
slot_labels.append(sign("S5_Label_Axis", "●  규격 1.2m", -SLOT_FACE - 1, 0, SLOT_Z + 105, yaw=180.0, size=(500, 90), font=40,
                        bg=(0.05, 0.06, 0.08, 1.0), text_color=(0.6, 0.65, 0.7), folder="Puzzle/Core"))

# ▲ 부품 슬롯 (북쪽, 발코니 쪽)
slot_plate("Triangle", 90.0, 45.0)
tri_socket = spawn(unreal.PuzzleCoreSocket, V(0, SLOT_FACE, SLOT_Z), yaw=90.0, label="S5_TriangleSocket", folder="Puzzle/Core")
tri_socket.set_editor_property("required_tag", "CorePartTriangle")
slot_labels.append(sign("S5_Label_Triangle", "▲", 0, SLOT_FACE + 1, SLOT_Z + 75, yaw=90.0, size=(200, 120), font=70,
                        bg=(0.05, 0.06, 0.08, 1.0), text_color=(0.6, 0.65, 0.7), folder="Puzzle/Core"))

# ■ 부품 슬롯 (동쪽, 장치 구역 쪽)
slot_plate("Square", 0.0, 45.0)
sq_socket = spawn(unreal.PuzzleCoreSocket, V(SLOT_FACE, 0, SLOT_Z), yaw=0.0, label="S5_SquareSocket", folder="Puzzle/Core")
sq_socket.set_editor_property("required_tag", "CorePartSquare")
slot_labels.append(sign("S5_Label_Square", "■", SLOT_FACE + 1, 0, SLOT_Z + 75, yaw=0.0, size=(200, 120), font=70,
                        bg=(0.05, 0.06, 0.08, 1.0), text_color=(0.6, 0.65, 0.7), folder="Puzzle/Core"))

# ================================================================ 조명 / 후처리
pp = spawn(unreal.PostProcessVolume, V(0, 0, 0), label="PostProcess", folder="Lighting")
pp.set_editor_property("unbound", True)
if intro_pp is not None:
    pp.set_editor_property("settings", intro_pp)

for i, deg in enumerate((0, 90, 180, 270)):
    a = math.radians(deg + 22.5)
    point_light("Hall_Fill_%d" % i, 560 * math.cos(a), 560 * math.sin(a), 850, 22, color=(0.75, 0.82, 1.0), radius=1600, shadows=False)
point_light("Elevator_Light", 0, -700, 350, 15, radius=700)
point_light("Balcony_Light", 0, 780, 700, 12, color=(0.85, 0.9, 1.0), radius=800, shadows=False)

# ================================================================ 도착 / 출구 엘리베이터 + 플레이어 + 내비게이션
ELEV_Y = SOUTH - 120.0
arrival = spawn(unreal.PuzzleElevator, V(-130, ELEV_Y, 0), yaw=90.0, label="S5_ArrivalElevator", folder="Navigation")
arrival.set_editor_property("arrival_only", True)
arrival.set_editor_property("next_level", "")
spawn(unreal.PlayerStart, V(-130, ELEV_Y - 18, 1), yaw=90.0, label="PlayerStart", folder="Navigation")

exit_elevator = spawn(unreal.PuzzleElevator, V(130, ELEV_Y, 0), yaw=90.0, label="S5_ExitElevator", folder="Puzzle")
exit_elevator.set_editor_property("next_level", "")
exit_elevator.set_editor_property("out_of_service", True)   # 마지막 스테이지: 문은 열리지만 출발하지 않음 (엔딩 연출은 나중에)

nav = spawn(unreal.NavMeshBoundsVolume, V(-120, -50, 300), label="NavMeshBounds", folder="Navigation")
nav.set_actor_scale3d(unreal.Vector(3500 / 200.0, 2000 / 200.0, 800 / 200.0))

# ================================================================ 원근법 구역 (서쪽 8m x 6m, 천장 4m)
PX1, PX2, PY, PH = -1712.0, -CHORD - T, 300.0, 400.0
box("Persp_Floor", PX1, -1000, -PY, PY, -10, 0, MI_PGRID, folder="Geometry/Perspective")   # x -1000 ~ 벽은 홀 바닥
box("Persp_Ceiling", PX1 - T, PX2, -PY - T, PY + T, PH, PH + 10, MI_CEIL, folder="Geometry/Perspective")
box("Persp_Wall_N", PX1 - T, PX2, PY, PY + T, 0, PH, MI_WALL, folder="Geometry/Perspective")
box("Persp_Wall_S", PX1 - T, PX2, -PY - T, -PY, 0, PH, MI_WALL, folder="Geometry/Perspective")
box("Persp_Wall_W", PX1 - T, PX1, -PY, PY, 0, PH, MI_WALL, folder="Geometry/Perspective")
point_light("Persp_Light_1", -1500, 0, 380, 20, radius=900)
point_light("Persp_Light_2", -1100, 0, 380, 20, radius=900)

# 3m 코어 축 (기준 60cm, 배치 3m → 5배. 손으로는 못 잡고 레이로만 → 끌려오며 작아짐)
axis = spawn(BP_PCUBE, V(-1350, 120, 26), label="S5_CoreAxis", folder="Puzzle/Perspective")
axis.set_actor_scale3d(unreal.Vector(3.0, 0.5, 0.5))
axis.set_editor_property("start_ratio", 300.0 / 60.0)
axis.set_editor_property("max_scale", 8.0)
axis.set_editor_property("color", unreal.LinearColor(0.15, 0.35, 0.85, 1.0))
axis.set_editor_property("emissive", 0.3)
axis.set_editor_property("tags", ["CoreAxis"])
axis.set_editor_property("frozen_until_grabbed", True)
box("Persp_AxisCradle_1", -1460, -1440, 85, 155, 0, 6, MI_METAL, folder="Puzzle/Perspective")
box("Persp_AxisCradle_2", -1260, -1240, 85, 155, 0, 6, MI_METAL, folder="Puzzle/Perspective")

dispenser = spawn(unreal.PuzzleDispenser, V(-1620, -240, 0), yaw=90.0, label="S5_Dispenser_Axis", folder="Puzzle/Perspective")
dispenser.set_editor_property("spawn_class", BP_PCUBE)
dispenser.set_editor_property("spawn_size", 60.0)
dispenser.set_editor_property("spawn_shape", unreal.Vector(1.0, 0.17, 0.17))
dispenser.set_editor_property("spawn_color", unreal.LinearColor(0.15, 0.35, 0.85, 1.0))
dispenser.set_editor_property("spawn_tags", ["CoreAxis"])
dispenser.set_editor_property("spawn_on_start", False)
dispenser.set_editor_property("replace_actor", axis)
sign("S5_Sign_Dispenser", "코어 축 예비품 보관함", -1620, -299, 140, yaw=90.0, size=(420, 80), font=28,
     bg=(0.15, 0.15, 0.15, 1.0), text_color=(0.6, 0.85, 1.0))

sign("S5_Sign_PerspTitle", "원근법 구역\n코어 축 규격 시험실", PX1 + 1, 0, 260, yaw=0.0, size=(700, 220), font=40, **PLATE)
sign("S5_Sign_AxisSpec", "코어 축 · 규격 1.2m\n(시험용 확대 상태: 3m)", -1350, PY - 1, 150, yaw=-90.0, size=(560, 160), font=32,
     bg=(0.85, 0.83, 0.78, 1.0), text_color=(0.08, 0.08, 0.1))
sign("S5_Sign_NarrowDoor", "원근법 구역", -CHORD + 1, 0, 270, yaw=0.0, size=(420, 80), font=32, **PLATE)

# ================================================================ 시점 구역 (북쪽 2층 발코니, 높이 4m)
BZ = 400.0
box("Balcony_Floor", -450, 450, 620, 900, BZ - 20, BZ, MI_FLOOR, folder="Geometry/Balcony")
box("Balcony_Edge", -450, 450, 616, 620, BZ - 24, BZ, MI_TRIM, folder="Geometry/Balcony", collision=False)
# 난간 110cm (노션) - 손잡이 + 기둥 + 아래쪽 막이(물건이 굴러 떨어지지 않게)
box("Balcony_Rail", -450, 450, 618, 626, BZ + 105, BZ + 110, MI_METAL, folder="Geometry/Balcony")
box("Balcony_Kick", -450, 450, 618, 624, BZ, BZ + 15, MI_METAL, folder="Geometry/Balcony")
for i, x in enumerate(range(-450, 451, 100)):
    box("Balcony_Post_%d" % i, x - 3, x + 3, 618, 626, BZ, BZ + 105, MI_METAL, folder="Geometry/Balcony")
for i, x in enumerate((-300, 300)):   # 발코니를 받치는 기둥
    cyl("Balcony_Support_%d" % i, x, 660, 0, BZ - 20, 40, MI_METAL, folder="Geometry/Balcony")

# 오르내리는 텔레포트 발판 (노션: 계단보다 멀미 적음). 도착 위치는 반대쪽 발판에서 떨어져 있게
lift_up = spawn(unreal.PuzzleLiftPad, V(300, 480, 0), label="S5_LiftUp", folder="Navigation")
lift_up.set_editor_property("destination", V(320, 780, BZ))
lift_down = spawn(unreal.PuzzleLiftPad, V(-330, 780, BZ), label="S5_LiftDown", folder="Navigation")
lift_down.set_editor_property("destination", V(-300, 480, 0))
sign("S5_Sign_LiftUp", "관측 발코니 ↑", 300, 619, BZ - 60, yaw=-90.0, size=(420, 80), font=32, **PLATE)
sign("S5_Sign_LiftDown", "↓ 홀로 내려가기", -330, 899, BZ + 160, yaw=-90.0, size=(420, 80), font=32, **PLATE)

# 관측 위치 ① ② : 발자국·눈금 없이 난간의 번호판만 (노션)
EYE_H = 160.0
VIEWS = [
    # 이름, 번호판, 관측 위치(바닥), 조각이 모여 보이는 곳, 기호 2개
    ("A", "①", (-250.0, 700.0), (-380.0, -250.0, 150.0), ("◆", "●")),
    ("B", "②", (250.0, 700.0), (380.0, -250.0, 150.0), ("▲", "■")),
]
SYM = 120.0   # 모여 보이는 면 위 기호 크기 (cm)


def symbol_segments(sym):
    """면 위 (u 오른쪽, v 위) 선분 목록. 채워진 기호(● ■)는 안쪽 선을 하나 더 그린다"""
    h = SYM / 2.0
    segs = []

    def poly(pts, split=2):
        for i in range(len(pts)):
            p0, p1 = pts[i], pts[(i + 1) % len(pts)]
            for j in range(split):
                t0, t1 = j / float(split), (j + 1) / float(split)
                segs.append(((p0[0] + (p1[0] - p0[0]) * t0, p0[1] + (p1[1] - p0[1]) * t0),
                             (p0[0] + (p1[0] - p0[0]) * t1, p0[1] + (p1[1] - p0[1]) * t1)))

    if sym == "◆":
        poly([(0, h), (h, 0), (0, -h), (-h, 0)])
        poly([(0, h * 0.5), (h * 0.5, 0), (0, -h * 0.5), (-h * 0.5, 0)], 1)
    elif sym == "●":
        for r in (h * 0.85, h * 0.45):
            n = 12 if r > h * 0.5 else 8
            poly([(r * math.cos(2 * math.pi * i / n), r * math.sin(2 * math.pi * i / n)) for i in range(n)], 1)
    elif sym == "▲":
        poly([(0, h), (h * 1.05, -h * 0.8), (-h * 1.05, -h * 0.8)])
        poly([(0, h * 0.35), (h * 0.45, -h * 0.5), (-h * 0.45, -h * 0.5)], 1)
    elif sym == "■":
        s = h * 0.85
        poly([(-s, s), (s, s), (s, -s), (-s, -s)])
        poly([(-s * 0.5, s * 0.5), (s * 0.5, s * 0.5), (s * 0.5, -s * 0.5), (-s * 0.5, -s * 0.5)], 1)
    return segs


view_actors = []
for name, number, (vx, vy), (tx, ty, tz), symbols in VIEWS:
    eye = V(vx, vy, BZ + EYE_H)
    target = V(tx, ty, tz)
    f = target - eye
    f = f.normal()
    up = unreal.Vector(0, 0, 1)
    right = up.cross(f).normal()        # 보는 사람의 오른쪽
    vup = f.cross(right).normal()       # 보는 사람의 위
    pieces = []
    for si, sym in enumerate(symbols):
        center = target + right * ((si - 0.5) * SYM * 1.9)
        for j, ((u0, v0), (u1, v1)) in enumerate(symbol_segments(sym)):
            p0 = center + right * u0 + vup * v0
            p1 = center + right * u1 + vup * v1
            s = random.uniform(0.38, 0.95)   # 선분마다 다른 거리 → 관측 위치에서만 기호 하나로 겹쳐 보임
            pieces.append(pipe("S5_Paint_%s_%d_%02d" % (name, si, j), lerp(eye, p0, s), lerp(eye, p1, s), 7.0 * s, MI_PAINT,
                               "Puzzle/View/" + name))
    view = spawn(unreal.PuzzleViewPoint, V(vx, vy, BZ), label="S5_View" + name, folder="Puzzle/View")
    view.set_editor_property("look_target", target)
    view.set_editor_property("pieces", pieces)
    view.set_editor_property("piece_color", unreal.LinearColor(0.95, 0.9, 0.8, 1.0))
    view_actors.append(view)
    # 난간 번호판 (관측 위치 옆 난간 아래쪽, 발코니 쪽에서 읽힘. 바로 앞에 두면 시선을 가려 기호가 안 보임)
    sign("S5_Sign_View" + name, number, vx + (-70.0 if vx < 0 else 70.0), 627, BZ + 75, yaw=90.0, size=(160, 160), font=110,
         bg=(0.12, 0.13, 0.15, 1.0), text_color=(0.95, 0.9, 0.7), folder="Puzzle/View")
    log("관측 위치 %s: 조각 %d개" % (number, len(pieces)))

# 발코니 뒤쪽 벽 보관함 (둘 다 맞추면 열림 → ▲ 부품)
LK = (0.0, 900.0, BZ + 62.0)   # 가운데 x, 벽 면 y, 바닥 z
box("Locker_Back", -45, 45, LK[1] - 12, LK[1], LK[2], LK[2] + 80, MI_DARK, folder="Puzzle/Locker")
box("Locker_Bottom", -45, 45, LK[1] - 44, LK[1], LK[2] - 8, LK[2], MI_METAL, folder="Puzzle/Locker")
box("Locker_Top", -45, 45, LK[1] - 44, LK[1], LK[2] + 80, LK[2] + 88, MI_METAL, folder="Puzzle/Locker")
box("Locker_Side_W", -45, -40, LK[1] - 44, LK[1], LK[2], LK[2] + 80, MI_METAL, folder="Puzzle/Locker")
box("Locker_Side_E", 40, 45, LK[1] - 44, LK[1], LK[2], LK[2] + 80, MI_METAL, folder="Puzzle/Locker")
locker_door = spawn(unreal.PuzzleDoor, V(0, LK[1] - 46, LK[2]), yaw=90.0, label="S5_LockerDoor", folder="Puzzle/Locker")
locker_door.set_editor_property("width", 80.0)
locker_door.set_editor_property("height", 80.0)
locker_door.set_editor_property("thickness", 3.0)
locker_door.set_editor_property("door_color", unreal.LinearColor(0.3, 0.32, 0.35, 1.0))
sign("S5_Sign_Locker", "관측 기록 보관함", 0, LK[1] - 47, LK[2] + 105, yaw=-90.0, size=(400, 70), font=28, **PLATE)
tri_part = part("S5_TrianglePart", 0, LK[1] - 24, LK[2] + 8, (22, 22, 14), (0.95, 0.75, 0.2), "CorePartTriangle", "▲",
                folder="Puzzle/Locker")

# ================================================================ 장치 구역 (동쪽 5m x 5m, 천장 3m)
DX1, DX2, DY, DH = CHORD + T, 1412.0, 250.0, 300.0
box("Dev_Floor", 1000, DX2, -DY, DY, -10, 0, MI_FLOOR, folder="Geometry/Device")   # 벽 ~ x 1000은 홀 바닥
box("Dev_Ceiling", DX1, DX2 + T, -DY - T, DY + T, DH, DH + 10, MI_CEIL, folder="Geometry/Device")
box("Dev_Wall_N", DX1, DX2 + T, DY, DY + T, 0, DH, MI_WALL, folder="Geometry/Device")
box("Dev_Wall_S", DX1, DX2 + T, -DY - T, -DY, 0, DH, MI_WALL, folder="Geometry/Device")
box("Dev_Wall_E", DX2, DX2 + T, -DY, DY, 0, DH, MI_WALL, folder="Geometry/Device")
point_light("Dev_Light", 1160, 0, 280, 10, radius=700)
sign("S5_Sign_DeviceDoor", "장치 구역", CHORD - 1, 0, 270, yaw=180.0, size=(420, 80), font=32, **PLATE)

DESK = (1180.0, 0.0)
DESK_TOP = 74.5
spawn(BP_DESK, V(DESK[0], DESK[1], 0), yaw=0.0, label="S5_Desk", folder="Props")   # 서랍 쪽이 서쪽(플레이어 쪽)
device = spawn(unreal.PuzzleHandDevice, V(DESK[0] - 18, 0, DESK_TOP), yaw=180.0, label="S5_HandDevice", folder="Puzzle/Device")
device.set_editor_property("solution", [3, 0, 1, 2])   # ◆ ● ▲ ■ (노션: 정답 배열만 바꿈)
sign("S5_Sign_Rack", "코어 제어 장치 · 양손을 넣으십시오", DESK[0] - 40, 0, DESK_TOP + 0.3, yaw=180.0, pitch=90.0,
     size=(560, 56), font=28, ppc=20.0, bg=(0.12, 0.12, 0.12, 1.0), text_color=(0.85, 0.8, 0.6), folder="Puzzle/Device")
code_board = sign("S5_CodeBoard", "지도 기록 · 코어 정지 코드\n?  ?  ?  ?", 1050, DY - 1, 170, yaw=-90.0, size=(600, 200), font=40,
                  bg=(0.78, 0.74, 0.62, 1.0), text_color=(0.12, 0.1, 0.08), folder="Puzzle/Device")

# 동쪽 벽 코어 제어판 + 램프 4개 (다이얼 순서 ● ▲ ■ ◆, 플레이어가 보기에 왼쪽부터)
box("S5_Panel", DX2 - 12, DX2, -170, 170, 95, 245, MI_METAL, folder="Puzzle/Device")
sign("S5_Sign_Panel", "코어 제어판", DX2 - 13, 0, 228, yaw=180.0, size=(1000, 160), font=90, bg=(0, 0, 0, 0),
     text_color=(0.06, 0.06, 0.07), folder="Puzzle/Device")
lamps = []
for i, (sym, ly) in enumerate((("●", -105), ("▲", -35), ("■", 35), ("◆", 105))):
    box("S5_LampHousing_%d" % i, DX2 - 18, DX2 - 12, ly - 22, ly + 22, 160, 204, MI_DARK, folder="Puzzle/Device")
    glow = box("S5_LampGlow_%d" % i, DX2 - 22, DX2 - 18, ly - 17, ly + 17, 165, 199, MI_LAMP, folder="Puzzle/Device",
               collision=False, movable=True)
    light = point_light("S5_LampLight_%d" % i, DX2 - 60, ly, 182, 6, color=(1.0, 0.65, 0.25), radius=350, shadows=False)
    sign("S5_LampLabel_%d" % i, sym, DX2 - 13, ly, 130, yaw=180.0, size=(300, 300), font=220, ppc=10.0, bg=(0, 0, 0, 0),
         text_color=(0.06, 0.06, 0.07), folder="Puzzle/Device")
    lamp = spawn(unreal.PuzzleToggleTarget, V(DX2 - 20, ly, 182), label="S5_Lamp_%d" % i, folder="Puzzle/Device")
    lamp.set_editor_property("display_name", sym + " 램프")
    lamp.set_editor_property("lights", [light])
    lamp.set_editor_property("show_when_on", [glow])
    lamps.append(lamp)
device.set_editor_property("targets", lamps)

# 제어판 아래 받침 + ■ 부품 (장치가 풀리면 나옴)
box("S5_PartTray", DX2 - 50, DX2 - 12, -45, 45, 84, 90, MI_TRIM, folder="Puzzle/Device")
sq_part = part("S5_SquarePart", DX2 - 30, 0, 101, (20, 20, 20), (0.35, 0.85, 0.95), "CorePartSquare", "■", folder="Puzzle/Device")

# ================================================================ 음성 기록 (코어 남쪽 책상 위 녹음기 4대)
LOG_DESK = (0.0, -380.0)
spawn(BP_DESK, V(LOG_DESK[0], LOG_DESK[1], 0), yaw=90.0, label="S5_LogDesk", folder="Props")   # 서랍 쪽이 남쪽(엘리베이터 쪽)
log_axis = recorder("S5_Log_Axis", -60, LOG_DESK[1] - 15, DESK_TOP, "음성 기록 #2  ·  2006.11.14 13:55", [
    "권혁수: 오늘 시연만 성공하면 연구소는 살아남는다.\n오나연 연구원, 코어만 똑바로 보면 돼.",
    "오나연: 네, 소장님. …한결 선배가 측정하고 있으니까 괜찮아요.",
])
log_tri = recorder("S5_Log_Triangle", -20, LOG_DESK[1] - 15, DESK_TOP, "음성 기록 #3  ·  2006.11.14 14:20", [
    "(멀리서 울리는 경보음, 문 닫히는 소리)",
    "권혁수: 측정 기록 전부 가져와. 수치는… 정상으로 고쳐.\n이건 가스 누출이야. 알아들었나?",
])
log_sq = recorder("S5_Log_Square", 20, LOG_DESK[1] - 15, DESK_TOP, "음성 기록 #4  ·  2006.11.15", [
    "오세진: (지친 목소리) 나연아, 들리면 대답해.\n모니터에 네 신호가 아직 떠. 내가 꼭 찾을게.",
])
log_final = recorder("S5_Log_Final", 60, LOG_DESK[1] - 15, DESK_TOP, "오나연", [
    "오나연: 한결 선배, 들려요?\n여기선 모든 게 너무 크게 보여요.",
    "오나연: 선배 측정이 맞았어요. 선배 잘못 아니에요.",
    "오나연: 우리 언니한테도 전해 주세요. 이제 괜찮다고.",
    "(코어 소리가 잦아들고 조용해짐)",
])
sign("S5_Sign_Logs", "코어 기록 재생기  ·  ●  ▲  ■  ·  ?", LOG_DESK[0], LOG_DESK[1] - 46, DESK_TOP + 0.3, yaw=-90.0, pitch=90.0,
     size=(700, 56), font=26, ppc=20.0, bg=(0.12, 0.12, 0.12, 1.0), text_color=(0.7, 0.8, 0.9), folder="Story")

# ================================================================ 글자 (대사 초안)
sign("S5_Sign_Title", "5. 중앙 코어\n인지 증폭 코어 홀", -270, SOUTH + 1, 300, yaw=90.0, size=(700, 220), font=40, **PLATE)
sign("S5_Sign_Exit", "출구", 130, SOUTH + 1, 255, yaw=90.0, size=(300, 80), font=34, **PLATE)
exit_note = sign("S5_ExitNote", "코어 불빛이 꺼지는 거, 밖에서도 보였어.\n고마워, 한결아. 20년 만에 나연이를 보내줬어.\n이제 진짜 기록을 세상에 보여주자. — 세진",
                 270, SOUTH + 1, 150, yaw=90.0, size=(900, 260), font=30, visible=False,
                 bg=(0.85, 0.83, 0.78, 1.0), text_color=(0.08, 0.08, 0.1))

# ---------------------------------------------------------------- 진행 담당
director = spawn(unreal.CoreStageDirector, V(0, 0, HH + 100), label="S5_StageDirector", folder="Puzzle")
director.set_editor_property("core_mesh", core)
director.set_editor_property("core_lights", core_lights)
director.set_editor_property("axis_slot", axis_slot)
director.set_editor_property("triangle_socket", tri_socket)
director.set_editor_property("square_socket", sq_socket)
director.set_editor_property("slot_labels", slot_labels)
director.set_editor_property("narrow_door_location", V(-CHORD - T / 2, 0, 0))
director.set_editor_property("view_a", view_actors[0])
director.set_editor_property("view_b", view_actors[1])
director.set_editor_property("locker_door", locker_door)
director.set_editor_property("triangle_part", tri_part)
director.set_editor_property("balcony_floor_z", ORIGIN.z + BZ)
director.set_editor_property("device", device)
director.set_editor_property("panel_lamps", lamps)
director.set_editor_property("square_part", sq_part)
director.set_editor_property("device_zone_min", V(DX1, -DY, 0))
director.set_editor_property("device_zone_max", V(DX2, DY, DH))
director.set_editor_property("code_board", code_board)
director.set_editor_property("log_axis", log_axis)
director.set_editor_property("log_triangle", log_tri)
director.set_editor_property("log_square", log_sq)
director.set_editor_property("log_final", log_final)
director.set_editor_property("exit_elevator", exit_elevator)
director.set_editor_property("exit_note", exit_note)

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("L_05_Core 저장 완료")

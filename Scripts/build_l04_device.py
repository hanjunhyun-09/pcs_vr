# -*- coding: utf-8 -*-
"""
L_04_Device (4스테이지: 장치 시험실)를 만든다.
노션 "맵 구조 설계 > L_04_Device" / "4스테이지 퍼즐 설계" / "대사 초안" 기준.

실행 방법 (둘 중 하나, C++ 빌드 후):
  1) 에디터를 닫은 상태에서:
     UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_l04_device.py"
  2) 에디터를 켠 채로: Python 원격 실행으로 이 파일을 보내면 열린 에디터에서 바로 만들어진다

다시 실행하면 L_04_Device 안의 액터를 모두 지우고 처음부터 다시 만든다.
크기 단위는 cm. 스테이지 4번이라 원점은 X = 400m (40000).

구역 (원점 기준, 바닥 높이 0):
  도착 복도      x -340~0,   y 60~260   (뒤쪽 보안문 = L_03에서 들어온 문, 천장 3m)
  장치 시험실    x 0~600,    y 0~500    천장 3m. 들어온 문은 서쪽 벽 y 100~220 (장치를 차면 닫힘)
                 북쪽 벽(y 500): 북쪽을 보고 서면 왼쪽부터 환풍기 x 470~570 · 칠판 x 150~450 · 모니터 x 20~140
                 → 책상 앞에서 한 번에 보임 (북쪽을 볼 때 +X가 왼쪽)
                 책상 (300, 330), 플레이어는 책상 남쪽(y 약 230)에 서서 북쪽을 봄 → 칠판까지 약 2.6m
  출구 엘리베이터 동쪽 벽 y 100~220 너머 (중앙 코어행, L_05 준비 중이라 도착만 하고 출발하지 않음)
"""
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
BP_DESK = eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Props/BP_Desk")


def log(msg):
    unreal.log_warning("[L04] " + msg)


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
MI_BOARD = ensure_mi("MI_VRP_Chalkboard", solid, {"Color": (0.06, 0.13, 0.09), "Emissive": 0.0, "Roughness": 0.95})
MI_DARK = ensure_mi("MI_VRP_DevDark", solid, {"Color": (0.04, 0.04, 0.045), "Emissive": 0.0, "Roughness": 0.5})
MI_METAL = ensure_mi("MI_VRP_DevMetal", solid, {"Color": (0.35, 0.36, 0.38), "Emissive": 0.0, "Roughness": 0.4})
MI_SCREEN = ensure_mi("MI_VRP_MonitorGlow", solid, {"Color": (0.2, 0.45, 0.6), "Emissive": 1.2, "Roughness": 0.3})
MI_RED = ensure_mi("MI_VRP_WarningGlow", solid, {"Color": (1.0, 0.08, 0.04), "Emissive": 6.0, "Roughness": 0.3})
MI_PANEL = ensure_mi("MI_VRP_CeilingPanel", solid, {"Color": (1.0, 0.97, 0.9), "Emissive": 3.0, "Roughness": 0.5})
MI_PAPER = ensure_mi("MI_VRP_DevPaper", solid, {"Color": (0.82, 0.8, 0.72), "Emissive": 0.05, "Roughness": 0.9})
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
H = 300.0     # 천장 높이 (노션: 3m)
T = 20.0      # 벽 두께
DOOR_H = 220.0

box("Floor", 0, 600, 0, 500, -10, 0, MI_FLOOR)
box("Ceiling", -T, 600 + T, -T, 500 + T, H, H + 10, MI_CEIL)
box("Wall_South", -T, 600 + T, -T, 0, 0, H, MI_WALL)
box("Wall_North", -T, 600 + T, 500, 500 + T, 0, H, MI_WALL)
# 서쪽 벽: 들어온 문 y 100~220
box("Wall_West_1", -T, 0, 0, 100, 0, H, MI_WALL)
box("Wall_West_2", -T, 0, 220, 500, 0, H, MI_WALL)
box("Wall_West_Header", -T, 0, 100, 220, DOOR_H, H, MI_WALL)
# 동쪽 벽: 엘리베이터 문 y 100~220
box("Wall_East_1", 600, 600 + T, 0, 100, 0, H, MI_WALL)
box("Wall_East_2", 600, 600 + T, 220, 500, 0, H, MI_WALL)
box("Wall_East_Header", 600, 600 + T, 100, 220, DOOR_H, H, MI_WALL)

# 도착 복도 (폭 2m, 천장 3m) + 뒤쪽 보안문 (L_03에서 들어온 문, 닫힌 채)
box("Arrive_Floor", -340, 0, 60, 260, -10, 0, MI_FLOOR, folder="Geometry/Arrive")
box("Arrive_Ceiling", -340, 0, 40, 280, 300, 310, MI_CEIL, folder="Geometry/Arrive")
box("Arrive_WallLeft", -340, -T, 40, 60, 0, 300, MI_WALL, folder="Geometry/Arrive")
box("Arrive_WallRight", -340, -T, 260, 280, 0, 300, MI_WALL, folder="Geometry/Arrive")
box("Arrive_WallEnd", -340, -320, 60, 260, 0, 300, MI_WALL, folder="Geometry/Arrive")

# ---------------------------------------------------------------- 후처리 / 꺼지지 않는 아주 약한 빛
pp = spawn(unreal.PostProcessVolume, V(0, 0, 0), label="PostProcess", folder="Lighting")
pp.set_editor_property("unbound", True)
if intro_pp is not None:
    pp.set_editor_property("settings", intro_pp)

# 불을 다 꺼도 방의 윤곽은 보이게 (노션 학교 확인: 어두운 방이 VR에서 너무 어둡지 않은지)
point_light("Fill_Light", 300, 250, 280, 1.5, color=(0.5, 0.6, 0.8), radius=900, shadows=False)
point_light("Arrive_Light", -170, 160, 280, 15, radius=500)

# ---------------------------------------------------------------- 도착 / 플레이어 / 내비게이션
arrive_door = spawn(unreal.PuzzleDoor, V(-310, 160, 0), label="S4_ArrivalDoor", folder="Navigation")
arrive_door.set_editor_property("door_color", unreal.LinearColor(0.2, 0.22, 0.25, 1.0))
spawn(unreal.PlayerStart, V(-200, 160, 1), yaw=0.0, label="PlayerStart", folder="Navigation")

nav = spawn(unreal.NavMeshBoundsVolume, V(240, 250, 150), label="NavMeshBounds", folder="Navigation")
nav.set_actor_scale3d(unreal.Vector(1300 / 200.0, 600 / 200.0, 400 / 200.0))

# 들어온 문: 처음엔 열려 있고, 장치를 차면 닫힘
entry_door = spawn(unreal.PuzzleDoor, V(-10, 160, 0), label="S4_EntryDoor", folder="Puzzle")
entry_door.set_editor_property("start_open", True)
entry_door.set_editor_property("door_color", unreal.LinearColor(0.28, 0.3, 0.33, 1.0))

# ================================================================ 책상 + 장치 거치대 + 종이
DESK = (300.0, 330.0)
DESK_TOP = 74.5
spawn(BP_DESK, V(DESK[0], DESK[1], 0), yaw=90.0, label="S4_Desk", folder="Props")   # 서랍 쪽이 남쪽(플레이어 쪽)

# 거치대: 앞(+X)이 플레이어 쪽(남쪽)을 보게
device = spawn(unreal.PuzzleHandDevice, V(DESK[0], 315, DESK_TOP), yaw=-90.0, label="S4_HandDevice", folder="Puzzle")
device.set_editor_property("solution", [1, 0, 3, 2])   # ▲ ● ◆ ■
sign("S4_Sign_Rack", "피험 장치 거치대 · 양손을 넣으십시오", DESK[0], 293, DESK_TOP + 0.3, yaw=-90.0, pitch=90.0,
     size=(640, 60), font=30, ppc=20.0, bg=(0.12, 0.12, 0.12, 1.0), text_color=(0.85, 0.8, 0.6), folder="Puzzle")

# 찢어진 기호표 (책상 오른쪽 위, 종이 밑). 환풍기를 켜면 종이가 날아가며 드러남
TABLE = (DESK[0] + 55, 345.0)
symbol_table = sign("S4_SymbolTable", "●  빛\n▲  (찢어짐)\n■  (찢어짐)\n◆  (찢어짐)", TABLE[0], TABLE[1], DESK_TOP + 0.25,
                    yaw=-90.0, pitch=90.0, size=(440, 380), font=44, ppc=20.0, visible=False,
                    bg=(0.86, 0.84, 0.76, 1.0), text_color=(0.12, 0.1, 0.08), folder="Puzzle")
papers = []
for i, (dx, dy, yaw) in enumerate(((-6, -4, 8.0), (5, 3, -14.0), (-2, 6, 27.0), (7, -6, -3.0), (0, 0, 45.0))):
    p = spawn(unreal.StaticMeshActor, V(TABLE[0] + dx, TABLE[1] + dy, DESK_TOP + 0.6 + i * 0.25), yaw=yaw,
              label="S4_Paper_%d" % i, folder="Puzzle/Papers")
    smc = p.static_mesh_component
    smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    smc.set_static_mesh(CUBE)
    smc.set_material(0, MI_PAPER)
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    smc.set_editor_property("can_ever_affect_navigation", False)
    p.set_actor_scale3d(unreal.Vector(0.30, 0.24, 0.002))
    papers.append(p)

# ================================================================ 북쪽 벽: 칠판 + 숨은 글씨
WALL_Y = 500.0
box("S4_Chalkboard", 150, 450, WALL_Y - 6, WALL_Y, 95, 235, MI_BOARD, folder="Puzzle/Board")
box("S4_ChalkboardFrameTop", 145, 455, WALL_Y - 8, WALL_Y, 235, 241, MI_TRIM, folder="Puzzle/Board")
box("S4_ChalkboardFrameBottom", 145, 455, WALL_Y - 10, WALL_Y, 89, 95, MI_TRIM, folder="Puzzle/Board")
CHALK = (0.88, 0.9, 0.86)
board_hint = sign("S4_Board_Hint", "왼손은 고르고, 오른손은 움직인다.", 300, WALL_Y - 6.5, 205, yaw=-90.0,
                  size=(2200, 120), font=60, ppc=10.0, bg=(0, 0, 0, 0), text_color=CHALK, folder="Puzzle/Board")
board_memo = sign("S4_Board_Memo", "나연이 시연 참가 반대!!\n신입한테 맡길 일 아님 — 세진", 330, WALL_Y - 6.5, 130, yaw=-90.0,
                  size=(1700, 200), font=48, ppc=10.0, bg=(0, 0, 0, 0), text_color=CHALK, folder="Puzzle/Board")
# 숨은 글씨: 칠판 바로 앞에 겹친 별도 판 (조명 OFF + 경고등 ON에서만 보임)
hidden = sign("S4_Board_Hidden", "장치 푸는 법 (소장님 몰래)\n경고 → 빛 → 바람 → 화면", 300, WALL_Y - 7, 165, yaw=-90.0,
              size=(2400, 300), font=72, ppc=10.0, visible=False, bg=(0, 0, 0, 0), text_color=(1.0, 0.25, 0.2),
              folder="Puzzle/Board")

# ================================================================ ● 천장 조명
ROOM_LIGHT_POS = ((150, 250), (450, 250))
room_lights = [point_light("S4_RoomLight_%d" % i, x, y, 285, 30, radius=1000) for i, (x, y) in enumerate(ROOM_LIGHT_POS)]
room_panels = [box("S4_CeilingPanel_%d" % i, x - 50, x + 50, y - 20, y + 20, H - 2, H, MI_PANEL, folder="Puzzle/Equipment",
                   collision=False, movable=True) for i, (x, y) in enumerate(ROOM_LIGHT_POS)]
room_light = toggle_target("S4_Toggle_RoomLight", 300, 250, 290, "● 천장 조명", start_on=True)
room_light.set_editor_property("lights", room_lights)
room_light.set_editor_property("show_when_on", room_panels)

# ================================================================ ▲ 빨간 경고등 (천장 가운데)
box("S4_WarningHousing", 280, 320, 230, 270, H - 12, H, MI_DARK, folder="Puzzle/Equipment", collision=False)
warning_glow = box("S4_WarningGlow", 286, 314, 236, 264, H - 20, H - 12, MI_RED, folder="Puzzle/Equipment", collision=False,
                   movable=True)
warning_lights = [point_light("S4_WarningLight", 300, 250, H - 30, 40, color=(1.0, 0.1, 0.05), radius=1100, shadows=False)]
warning = toggle_target("S4_Toggle_Warning", 300, 250, H - 25, "▲ 빨간 경고등")
warning.set_editor_property("lights", warning_lights)
warning.set_editor_property("show_when_on", [warning_glow])

# ================================================================ ■ 모니터 (북쪽 벽, 보는 사람 오른쪽)
MON = (20.0, 140.0, 100.0, 200.0)   # x1, x2, z1, z2 (북쪽을 보면 x가 작은 쪽이 오른쪽)
box("S4_MonitorHousing", MON[0] - 5, MON[1] + 5, WALL_Y - 10, WALL_Y, MON[2] - 5, MON[3] + 5, MI_DARK, folder="Puzzle/Equipment")
monitor_glow = box("S4_MonitorScreen", MON[0], MON[1], WALL_Y - 11, WALL_Y - 10, MON[2], MON[3], MI_SCREEN,
                   folder="Puzzle/Equipment", collision=False, movable=True)
monitor_text = sign("S4_MonitorText", "참가자 신호 상태", (MON[0] + MON[1]) / 2.0, WALL_Y - 11.5, (MON[2] + MON[3]) / 2.0, yaw=-90.0,
                    size=(1150, 950), font=40, ppc=10.0, visible=False, bg=(0, 0, 0, 0), text_color=(0.75, 0.95, 1.0),
                    folder="Puzzle/Equipment")
monitor_lights = [point_light("S4_MonitorLight", (MON[0] + MON[1]) / 2.0, WALL_Y - 60, 150, 8, color=(0.4, 0.75, 1.0), radius=450,
                              shadows=False)]
monitor = toggle_target("S4_Toggle_Monitor", (MON[0] + MON[1]) / 2.0, WALL_Y - 20, 150, "■ 모니터")
monitor.set_editor_property("lights", monitor_lights)
monitor.set_editor_property("show_when_on", [monitor_glow, monitor_text])

# ================================================================ ◆ 환풍기 (북쪽 벽, 보는 사람 왼쪽, 높이 1.2 ~ 2.2m)
FAN_C = (520.0, 170.0)   # x, z (북쪽을 보면 x가 큰 쪽이 왼쪽)
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
    blade = spawn(unreal.StaticMeshActor, V(FAN_C[0], WALL_Y - 9, FAN_C[1]), pitch=45.0 * i, label="S4_FanBlade_%d" % i,
                  folder="Puzzle/Equipment")
    smc = blade.static_mesh_component
    smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    smc.set_static_mesh(CUBE)
    smc.set_material(0, MI_METAL)
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    smc.set_editor_property("can_ever_affect_navigation", False)
    blade.set_actor_scale3d(unreal.Vector(0.8, 0.015, 0.1))   # 80cm x 10cm 날개 (가운데를 지나는 막대 4개 → 날개 8장)
    attach(blade, hub)
fan_lights = [point_light("S4_FanBackLight", FAN_C[0], WALL_Y - 40, FAN_C[1], 6, color=(0.9, 0.95, 1.0), radius=300, shadows=False)]
fan = toggle_target("S4_Toggle_Fan", FAN_C[0], WALL_Y - 9, FAN_C[1], "◆ 환풍기", yaw=-90.0)   # 앞(+X) = 벽에서 나오는 방향 = 회전축
fan.set_editor_property("lights", fan_lights)
fan.set_editor_property("spin_when_on", [hub])

# 장치 다이얼 순서 ● ▲ ■ ◆ = 조명, 경고등, 모니터, 환풍기
device.set_editor_property("targets", [room_light, warning, monitor, fan])

# ================================================================ 출구: 중앙 코어행 엘리베이터 (동쪽)
elevator = spawn(unreal.PuzzleElevator, V(720, 160, 0), yaw=180.0, label="S4_Elevator", folder="Puzzle")
elevator.set_editor_property("next_level", "/Game/VRPuzzle/Maps/L_05_Core")
elevator.set_editor_property("out_of_service", True)   # L_05 준비 중
box("Elevator_Stripe", 596, 600, 100, 220, DOOR_H, DOOR_H + 8, MI_WARN, folder="Geometry/Trim")

# ---------------------------------------------------------------- 글자 (대사 초안)
sign("S4_Sign_Title", "4. 장치 시험실\n피험 장치 동작 시험", 1, 380, 230, yaw=0.0, size=(700, 220), font=40,
     bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))
sign("S4_Sign_ArriveDoor", "3. 관측실", -319, 160, 255, yaw=0.0, size=(500, 90), font=34,
     bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))
sign("S4_Sign_Elevator", "5. 중앙 코어\n(엘리베이터 — 준비 중)", 599, 380, 230, yaw=180.0, size=(600, 160), font=34,
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

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("L_04_Device 저장 완료")

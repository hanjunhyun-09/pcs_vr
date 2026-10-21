# -*- coding: utf-8 -*-
"""
L_02_Perspective (2스테이지: 원근 실험실)를 만든다.
노션 "맵 구조 설계 > L_02_Perspective" / "2스테이지 퍼즐 설계" / "대사 초안" 기준.

실행 (에디터를 닫은 상태에서, C++ 빌드 후):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_l02_perspective.py"

다시 실행하면 L_02_Perspective 안의 액터를 모두 지우고 처음부터 다시 만든다.
크기 단위는 cm. 스테이지 2번이라 원점은 X = 200m (20000).

구역 (원점 기준, 바닥 높이 0):
  A. 선반 구역   x 0~1000,    y 0~1200   천장 6m, 북쪽 벽에 높은 선반(높이 3m, 깊이 2m, x 300~1000)
  B. 통로        x 1000~1020, y 0~250    (폭 2.5m, 높이 2.5m) 앞을 거대 상자(2.45m)가 막고 있음
  발코니         x 1000~1600, y 0~350    난간 110cm (y = 350)
  C. 아래층      x 1000~1600, y 350~1200 바닥 높이 -200, 압력판 (1350, 1000)
  출구 복도      x 1620~2220, y 100~300  끝에 관측실 보안문 (지나가면 L_03으로)
  도착 엘리베이터 x -224~0,   y 200~400  (서쪽 벽, 문 = x 0)
"""
import unreal

MAP = "/Game/VRPuzzle/Maps/L_02_Perspective"
MAT_DIR = "/Game/VRPuzzle/Materials"
ORIGIN = unreal.Vector(20000.0, 0.0, 0.0)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
BP_PCUBE = eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Interaction/BP_PerspectiveCube")


def log(msg):
    unreal.log("[L02] " + msg)


def V(x, y, z):
    return unreal.Vector(ORIGIN.x + x, ORIGIN.y + y, ORIGIN.z + z)


# ---------------------------------------------------------------- 머티리얼 도우미
def expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def connect(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("연결 실패: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def scalar(mat, name, value, x, y):
    e = expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def vector(mat, name, color, x, y):
    e = expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    return e


def const(mat, value, x, y):
    e = expr(mat, unreal.MaterialExpressionConstant, x, y)
    e.set_editor_property("r", value)
    return e


def binop(mat, cls, a, b, x, y, a_out="", b_out=""):
    e = expr(mat, cls, x, y)
    connect(a, a_out, e, "A")
    connect(b, b_out, e, "B")
    return e


def unop(mat, cls, a, x, y, a_out=""):
    e = expr(mat, cls, x, y)
    connect(a, a_out, e, "")
    return e


def mask(mat, src, channel, x, y):
    e = expr(mat, unreal.MaterialExpressionComponentMask, x, y)
    for c in ("r", "g", "b", "a"):
        e.set_editor_property(c, c == channel)
    connect(src, "", e, "")
    return e


def grid_lines(mat, coords, width, x0, y0):
    """coords(격자 단위 좌표)에서 정수선 가까이면 1. width = 선 굵기(격자 단위). 성분별 결과(벡터)"""
    f = unop(mat, unreal.MaterialExpressionFrac, coords, x0, y0)
    inv = unop(mat, unreal.MaterialExpressionOneMinus, f, x0, y0 + 80)
    d = binop(mat, unreal.MaterialExpressionMin, f, inv, x0 + 150, y0)                 # 가장 가까운 선까지 거리
    half = binop(mat, unreal.MaterialExpressionMultiply, width, const(mat, 0.5, x0, y0 + 160), x0 + 150, y0 + 160)
    diff = binop(mat, unreal.MaterialExpressionSubtract, half, d, x0 + 300, y0)
    soft = binop(mat, unreal.MaterialExpressionMultiply, half, const(mat, 0.4, x0 + 150, y0 + 240), x0 + 300, y0 + 160)
    ratio = binop(mat, unreal.MaterialExpressionDivide, diff, soft, x0 + 450, y0)
    return unop(mat, unreal.MaterialExpressionSaturate, ratio, x0 + 600, y0)


def ensure_grid_material():
    """M_VRP_Grid: 월드 좌표 1m 격자 (바닥·벽·천장 공통). 면의 방향에 맞는 두 축만 선을 긋는다"""
    path = MAT_DIR + "/M_VRP_Grid"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = at.create_asset("M_VRP_Grid", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    base = vector(mat, "Color", (0.55, 0.56, 0.57), -400, -500)
    line_col = vector(mat, "LineColor", (0.22, 0.23, 0.24), -400, -350)
    size = scalar(mat, "GridSize", 100.0, -1900, 0)
    width_cm = scalar(mat, "LineWidth", 2.0, -1900, 150)
    rough = scalar(mat, "Roughness", 0.8, -400, 400)

    wp = expr(mat, unreal.MaterialExpressionWorldPosition, -1900, -150)
    coords = binop(mat, unreal.MaterialExpressionDivide, wp, size, -1700, -100)
    width = binop(mat, unreal.MaterialExpressionDivide, width_cm, size, -1700, 150)
    lines = grid_lines(mat, coords, width, -1500, -100)

    # 면에 수직인 축의 선은 지운다: (1 - |노멀|) 을 곱함
    nrm = expr(mat, unreal.MaterialExpressionVertexNormalWS, -1100, 300)
    absn = unop(mat, unreal.MaterialExpressionAbs, nrm, -950, 300)
    keep = unop(mat, unreal.MaterialExpressionOneMinus, absn, -800, 300)
    masked = binop(mat, unreal.MaterialExpressionMultiply, lines, keep, -700, 100)
    r = mask(mat, masked, "r", -550, 0)
    g = mask(mat, masked, "g", -550, 100)
    b = mask(mat, masked, "b", -550, 200)
    m1 = binop(mat, unreal.MaterialExpressionMax, r, g, -400, 50)
    m2 = binop(mat, unreal.MaterialExpressionMax, m1, b, -250, 100)

    lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, -150, -400)
    connect(base, "", lerp, "A")
    connect(line_col, "", lerp, "B")
    connect(m2, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("M_VRP_Grid 생성")
    return mat


def ensure_perspective_grid_material():
    """M_VRP_PerspectiveGrid: 원근 물체 표시용 파란 격자 (면마다 4칸, 물체 크기를 따라 커짐).
    파라미터 이름은 M_VRP_Solid와 같다 (Color, Emissive) → C++ SetSolidColor 그대로 사용"""
    path = MAT_DIR + "/M_VRP_PerspectiveGrid"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = at.create_asset("M_VRP_PerspectiveGrid", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = vector(mat, "Color", (0.1, 0.35, 0.9), -500, -500)
    emissive = scalar(mat, "Emissive", 0.3, -500, 500)
    count = scalar(mat, "GridCount", 4.0, -1700, 150)
    width = scalar(mat, "LineWidth", 0.08, -1700, 300)
    rough = scalar(mat, "Roughness", 0.45, -150, 600)

    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1700, 0)
    coords = binop(mat, unreal.MaterialExpressionMultiply, uv, count, -1550, 50)
    lines = grid_lines(mat, coords, width, -1400, 0)
    r = mask(mat, lines, "r", -700, 0)
    g = mask(mat, lines, "g", -700, 100)
    line = binop(mat, unreal.MaterialExpressionMax, r, g, -550, 50)

    # 칸 = 조금 어두운 색, 선 = 밝은 색
    dark = binop(mat, unreal.MaterialExpressionMultiply, color, const(mat, 0.55, -500, -350), -350, -450)
    bright = binop(mat, unreal.MaterialExpressionAdd, color, const(mat, 0.35, -500, -250), -350, -300)
    lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, -200, -400)
    connect(dark, "", lerp, "A")
    connect(bright, "", lerp, "B")
    connect(line, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # 발광: 전체 x Emissive + 선은 항상 은은하게
    glow = binop(mat, unreal.MaterialExpressionMultiply, lerp, emissive, -50, 300)
    line_glow = binop(mat, unreal.MaterialExpressionMultiply, line, binop(mat, unreal.MaterialExpressionMultiply, color, const(mat, 0.6, -350, 450), -200, 450), -50, 450)
    total = binop(mat, unreal.MaterialExpressionAdd, glow, line_glow, 100, 350)
    mel.connect_material_property(total, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("M_VRP_PerspectiveGrid 생성")
    return mat


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


# ---------------------------------------------------------------- 레벨 도우미 (build_l01_isolation.py와 같은 방식)
def spawn(cls, loc, yaw=0.0, label=None, folder=None, pitch=0.0, roll=0.0):
    a = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    if label:
        a.set_actor_label(label)
    if folder:
        a.set_folder_path(folder)
    return a


def box(label, x1, x2, y1, y2, z1, z2, mi, folder="Geometry"):
    """x1~x2, y1~y2, z1~z2 범위를 채우는 상자"""
    center = V((x1 + x2) / 2.0, (y1 + y2) / 2.0, (z1 + z2) / 2.0)
    a = spawn(unreal.StaticMeshActor, center, label=label, folder=folder)
    smc = a.static_mesh_component
    smc.set_static_mesh(CUBE)
    smc.set_material(0, mi)
    a.set_actor_scale3d(unreal.Vector((x2 - x1) / 100.0, (y2 - y1) / 100.0, (z2 - z1) / 100.0))
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


def sign(label, text, x, y, z, yaw=0.0, pitch=0.0, size=(800, 500), font=36, visible=True,
         text_color=(0.05, 0.05, 0.05), bg=(0.85, 0.83, 0.78, 1.0), folder="Story"):
    a = spawn(unreal.PuzzleSign, V(x, y, z), yaw=yaw, pitch=pitch, label=label, folder=folder)
    a.set_editor_property("text", unreal.Text(text))
    a.set_editor_property("font_size", font)
    a.set_editor_property("draw_size", unreal.Vector2D(size[0], size[1]))
    a.set_editor_property("visible_at_start", visible)
    a.set_editor_property("text_color", unreal.LinearColor(text_color[0], text_color[1], text_color[2], 1.0))
    a.set_editor_property("background_color", unreal.LinearColor(bg[0], bg[1], bg[2], bg[3]))
    return a


# ================================================================ 시작
grid = ensure_grid_material()
ensure_perspective_grid_material()
solid = unreal.load_asset(MAT_DIR + "/M_VRP_Solid")
MI_FLOOR = ensure_mi("MI_VRP_GridFloor", grid, {"Color": (0.30, 0.31, 0.32), "LineColor": (0.62, 0.64, 0.66), "LineWidth": 2.5, "Roughness": 0.7})
MI_WALL = ensure_mi("MI_VRP_GridWall", grid, {"Color": (0.62, 0.63, 0.64), "LineColor": (0.30, 0.31, 0.33), "LineWidth": 2.0, "Roughness": 0.85})
MI_CEIL = ensure_mi("MI_VRP_GridCeiling", grid, {"Color": (0.42, 0.43, 0.44), "LineColor": (0.30, 0.31, 0.32), "LineWidth": 1.5, "Roughness": 0.9})
MI_SHELF = ensure_mi("MI_VRP_GridShelf", grid, {"Color": (0.38, 0.42, 0.46), "LineColor": (0.80, 0.82, 0.84), "LineWidth": 3.0, "Roughness": 0.6})
MI_PIT = ensure_mi("MI_VRP_GridPit", grid, {"Color": (0.22, 0.23, 0.25), "LineColor": (0.55, 0.57, 0.6), "LineWidth": 3.0, "Roughness": 0.7})
MI_TRIM = unreal.load_asset(MAT_DIR + "/MI_VRP_Trim")
MI_WARN = unreal.load_asset(MAT_DIR + "/MI_VRP_Warning")
MI_WINDOW = ensure_mi("MI_VRP_CoreWindow", solid, {"Color": (0.15, 0.45, 0.9), "Emissive": 2.5, "Roughness": 0.1})
MI_SWITCH = ensure_mi("MI_VRP_SwitchPanel", solid, {"Color": (0.1, 0.5, 1.0), "Emissive": 1.2, "Roughness": 0.3})

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
world.get_world_settings().set_editor_property("kill_z", -700.0)   # 아래층(-2m)보다 5m 아래

# ---------------------------------------------------------------- 지오메트리
H = 600.0     # 천장 높이 (노션: 6m)
T = 20.0      # 벽 두께
LOW = -200.0  # 아래층 바닥 높이

# 바닥
box("A_Floor", 0, 1000, 0, 1200, -10, 0, MI_FLOOR)
box("Balcony_Floor", 1000, 1600, 0, 350, LOW, 0, MI_FLOOR)          # 발코니 (아래층 쪽 옆면이 벽이 됨)
box("C_Floor", 1000, 1600, 350, 1200, LOW - 10, LOW, MI_PIT)
box("Ceiling", -T, 1600 + T, -T, 1200 + T, H, H + 10, MI_CEIL)

# 바깥 벽
box("Wall_South", -T, 1600 + T, -T, 0, 0, H, MI_WALL)
box("Wall_North", -T, 1600 + T, 1200, 1200 + T, LOW, H, MI_WALL)
box("Wall_West_1", -T, 0, 0, 240, 0, H, MI_WALL)                    # 엘리베이터 문 y 240~360
box("Wall_West_2", -T, 0, 360, 1200, 0, H, MI_WALL)
box("Wall_West_Header", -T, 0, 240, 360, 220, H, MI_WALL)
box("Wall_East_Pit", 1600, 1600 + T, 350, 1200, LOW, H, MI_WALL)
box("Wall_East_1", 1600, 1600 + T, 0, 140, 0, H, MI_WALL)            # 출구 문 y 140~260
box("Wall_East_2", 1600, 1600 + T, 260, 350, 0, H, MI_WALL)
box("Wall_East_Header", 1600, 1600 + T, 140, 260, 220, H, MI_WALL)

# A ↔ C 칸막이 (아래층 바닥부터 천장까지) + 통로 B (폭 2.5m, 높이 2.5m)
box("Partition", 1000, 1000 + T, 350, 1200, LOW, H, MI_WALL)
box("B_Wall", 1000, 1000 + T, 250, 350, 0, H, MI_WALL)
box("B_Header", 1000, 1000 + T, 0, 250, 250, H, MI_WALL)
box("B_Stripe", 996, 1000, 0, 250, 250, 258, MI_WARN, folder="Geometry/Trim")

# 높은 선반 (높이 3m, 깊이 2m) + 앞 모서리 띠
# 깊이 1.5m였을 때는 뒤쪽 창문·스위치 판(선반 위 70~80cm) + 앞 낭떠러지 + 에이전트 반경(35cm) 때문에 윗면 NavMesh가 전부 깎여 텔레포트 불가였음
box("S2_Shelf", 300, 1000, 1000, 1200, 0, 300, MI_SHELF)
box("S2_ShelfEdge", 300, 1000, 996, 1000, 290, 300, MI_WARN, folder="Geometry/Trim")

# 코어가 보이는 창문 (선반 위 북쪽 벽, 빛나는 유리)
box("Window_Glass", 350, 950, 1196, 1200, 380, 560, MI_WINDOW, folder="Geometry/Window")
box("Window_FrameTop", 340, 960, 1192, 1200, 560, 570, MI_TRIM, folder="Geometry/Window")
box("Window_FrameBottom", 340, 960, 1192, 1200, 370, 380, MI_TRIM, folder="Geometry/Window")
box("Window_Mullion", 645, 655, 1192, 1200, 380, 560, MI_TRIM, folder="Geometry/Window")

# 발코니 난간 (높이 110cm, y = 350)
box("Rail_Top", 1020, 1600, 345, 355, 104, 110, MI_WARN, folder="Geometry/Rail")
box("Rail_Mid", 1020, 1600, 347, 353, 55, 59, MI_TRIM, folder="Geometry/Rail")
for i, px in enumerate(range(1030, 1600, 100)):
    box("Rail_Post_%d" % i, px - 3, px + 3, 347, 353, 0, 104, MI_TRIM, folder="Geometry/Rail")

# 출구 복도 (폭 2m, 길이 6m, 천장 3m) + 끝의 관측실 보안문
box("Exit_Floor", 1600, 2240, 100, 300, -10, 0, MI_FLOOR, folder="Geometry/Exit")
box("Exit_Ceiling", 1600, 2240, 80, 320, 300, 310, MI_CEIL, folder="Geometry/Exit")
box("Exit_WallLeft", 1620, 2240, 80, 100, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Exit_WallRight", 1620, 2240, 300, 320, 0, 300, MI_WALL, folder="Geometry/Exit")
# 끝 벽: 관측실 보안문 자리 y 140~260 (높이 220)
box("Exit_WallEnd_1", 2220, 2240, 100, 140, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Exit_WallEnd_2", 2220, 2240, 260, 300, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Exit_WallEnd_Header", 2220, 2240, 140, 260, 220, 300, MI_WALL, folder="Geometry/Exit")
# 보안문 너머 짧은 통로 (여기 들어서면 L_03으로 넘어감)
box("Gate_Floor", 2240, 2460, 100, 300, -10, 0, MI_FLOOR, folder="Geometry/Exit")
box("Gate_Ceiling", 2240, 2460, 80, 320, 300, 310, MI_CEIL, folder="Geometry/Exit")
box("Gate_WallLeft", 2240, 2460, 80, 100, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Gate_WallRight", 2240, 2460, 300, 320, 0, 300, MI_WALL, folder="Geometry/Exit")
box("Gate_WallEnd", 2440, 2460, 100, 300, 0, 300, MI_WALL, folder="Geometry/Exit")

# ---------------------------------------------------------------- 조명 / 후처리
point_light("A_Light_1", 250, 300, 560, 70, radius=1200)
point_light("A_Light_2", 250, 900, 560, 70, radius=1200)
point_light("A_Light_3", 600, 600, 560, 60, radius=1200)
point_light("Shelf_Light", 650, 1125, 520, 25, radius=500)
point_light("Switch_Glow", 920, 1150, 430, 10, color=(0.3, 0.6, 1.0), radius=300, shadows=False)   # 스위치가 아래에서도 보이게
point_light("Window_Glow", 650, 1150, 470, 12, color=(0.4, 0.6, 1.0), radius=600, shadows=False)
corridor_1 = point_light("B_Light_1", 875, 125, 420, 45, radius=600)
corridor_2 = point_light("B_Light_2", 1110, 125, 230, 20, radius=400)
point_light("Balcony_Light", 1300, 175, 560, 50, radius=1000)
point_light("C_Light", 1300, 800, 300, 60, radius=1000)
point_light("Exit_Light", 1920, 200, 280, 18, radius=600)

pp = spawn(unreal.PostProcessVolume, V(0, 0, 0), label="PostProcess", folder="Lighting")
pp.set_editor_property("unbound", True)
if intro_pp is not None:
    pp.set_editor_property("settings", intro_pp)

# ---------------------------------------------------------------- 도착 엘리베이터 / 플레이어 / 내비게이션
arrival = spawn(unreal.PuzzleElevator, V(-112, 300, 0), yaw=0.0, label="S2_ArrivalElevator", folder="Navigation")
arrival.set_editor_property("arrival_only", True)
arrival.set_editor_property("next_level", "")
spawn(unreal.PlayerStart, V(-130, 300, 1), yaw=0.0, label="PlayerStart", folder="Navigation")

# 텔레포트 가능 영역: 아래층(-2m)은 빼서 난간 너머로 내려갈 수 없게
nav = spawn(unreal.NavMeshBoundsVolume, V(1000, 600, 320), label="NavMeshBounds", folder="Navigation")
nav.set_actor_scale3d(unreal.Vector(2480 / 200.0, 1260 / 200.0, 760 / 200.0))

# ---------------------------------------------------------------- 퍼즐 부품
# 2-1 높은 선반: 입구 옆 디스펜서 (30cm 큐브) + 선반 위 통로 전원 스위치
dispenser_a = spawn(unreal.PuzzleDispenser, V(160, 40, 0), yaw=90.0, label="S2_Dispenser_Entrance", folder="Puzzle")
dispenser_a.set_editor_property("spawn_class", BP_PCUBE)
dispenser_a.set_editor_property("spawn_size", 30.0)
dispenser_a.set_editor_property("spawn_on_start", True)

# 스위치 뒤 빛나는 판 (80cm, 아래에서도 눈에 띄게)
box("S2_SwitchPanel", 880, 960, 1198, 1200, 370, 450, MI_SWITCH, folder="Puzzle")
shelf_switch = spawn(unreal.PuzzleButton, V(920, 1198, 410), yaw=-90.0, label="S2_ShelfSwitch", folder="Puzzle")
shelf_switch.set_editor_property("cap_color", unreal.LinearColor(0.1, 0.6, 1.0, 1.0))

# 2-2 거대 상자 (기준 30cm, 배치 2.45m → 8.17배, 최대 10배). 통로 B 입구를 막음
giant = spawn(BP_PCUBE, V(875, 128, 123.5), label="S2_GiantBox", folder="Puzzle")
giant.set_actor_scale3d(unreal.Vector(2.45, 2.45, 2.45))
giant.set_editor_property("start_ratio", 2.45 / 0.3)
giant.set_editor_property("max_scale", 10.0)
giant.set_editor_property("color", unreal.LinearColor(0.42, 0.45, 0.5, 1.0))
giant.set_editor_property("emissive", 0.1)
giant.set_editor_property("tags", ["MetalBox"])
giant.set_editor_property("frozen_until_grabbed", True)   # 처음 레이로 잡기 전엔 고정 (다른 물체에 밀려 통로가 열리지 않게)

# 2-3 압력판 (2m x 2m, 아래층) + 난간 옆 디스펜서 (작은 금속 상자, 원래 상자를 대신함)
plate = spawn(unreal.PuzzlePressurePlate, V(1350, 1000, LOW), label="S2_PressurePlate", folder="Puzzle")
dispenser_c = spawn(unreal.PuzzleDispenser, V(1560, 40, 0), yaw=90.0, label="S2_Dispenser_Balcony", folder="Puzzle")
dispenser_c.set_editor_property("spawn_class", BP_PCUBE)
dispenser_c.set_editor_property("spawn_size", 30.0)
dispenser_c.set_editor_property("spawn_on_start", False)
dispenser_c.set_editor_property("spawn_color", unreal.LinearColor(0.42, 0.45, 0.5, 1.0))
dispenser_c.set_editor_property("spawn_max_scale", 10.0)
dispenser_c.set_editor_property("spawn_tags", ["MetalBox"])
dispenser_c.set_editor_property("replace_actor", giant)

# 출구 문 (발코니 동쪽 벽) + 관측실 보안문 (가까이 가면 열리고, 지나가면 L_03으로)
exit_door = spawn(unreal.PuzzleDoor, V(1610, 200, 0), label="S2_ExitDoor", folder="Puzzle")
observation_door = spawn(unreal.PuzzleDoor, V(2230, 200, 0), label="S2_ObservationDoor", folder="Puzzle")
observation_door.set_editor_property("door_color", unreal.LinearColor(0.2, 0.22, 0.25, 1.0))
gate = spawn(unreal.PuzzleStageGate, V(2230, 200, 0), label="S2_StageGate", folder="Navigation")
gate.set_editor_property("next_level", "/Game/VRPuzzle/Maps/L_03_Observation")
gate.set_editor_property("door", observation_door)

# ---------------------------------------------------------------- 글자 (대사 초안 그대로)
sign("S2_Sign_Title", "2. 원근 실험실\n착시 측정 연구소",
     40, 1, 300, yaw=90.0, size=(700, 220), font=40, bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))
sign("S2_Sign_Log_1", "실험 기록 보드\n2006.03  보이는 크기와 실제 크기의 오차 측정.\n평균 오차 12%.",
     520, 1, 170, yaw=90.0, size=(900, 300), font=34, bg=(0.9, 0.9, 0.85, 1.0))
sign("S2_Sign_Log_2", "실험 기록 보드\n2006.07  측정 대상의 크기가 실제로 변함. 재현 확인.\n— 외부에 알리지 말 것. (권혁수)",
     999, 600, 170, yaw=180.0, size=(1000, 300), font=34, bg=(0.9, 0.9, 0.85, 1.0))
sign("S2_Sign_Log_3", "실험 기록 보드\n2006.10  코어 출력 3배 증가. 공개 시연 일정 확정.",
     1250, 1, 170, yaw=90.0, size=(900, 260), font=34, bg=(0.9, 0.9, 0.85, 1.0))
sign("S2_Sign_ShelfNote",
     "강한결 측정 노트\n11/13 코어 출력이 안정 범위를 넘을 조짐.\n내일 시연 전 반드시 재측정할 것.\n\n한결 선배 측정은 틀린 적 없음! 내일 시연 파이팅 ✌ — 나연",
     600, 1199, 400, yaw=-90.0, size=(1000, 460), font=32, bg=(0.95, 0.93, 0.85, 1.0))
sign("S2_Sign_SwitchLabel", "▶ 통로 전원 ◀", 920, 1197, 475, yaw=-90.0, size=(520, 110), font=44, bg=(0.1, 0.12, 0.15, 1.0), text_color=(0.6, 0.85, 1.0))
sign("S2_Sign_ShelfArrow", "↑  선반 위: 통로 전원 스위치", 650, 999, 230, yaw=-90.0, size=(900, 130), font=48, bg=(0.1, 0.12, 0.15, 1.0), text_color=(0.6, 0.85, 1.0))
sign("S2_Sign_Plate", "압력판  ·  필요 무게 ≥ 3", 1350, 1199, 0, yaw=-90.0, size=(1200, 200), font=80, bg=(0.85, 0.5, 0.05, 1.0))
sign("S2_Sign_DispenserA", "큐브 디스펜서", 160, 1, 140, yaw=90.0, size=(360, 80), font=28, bg=(0.15, 0.15, 0.15, 1.0), text_color=(0.6, 0.85, 1.0))
sign("S2_Sign_DispenserC", "금속 상자 디스펜서", 1560, 1, 140, yaw=90.0, size=(420, 80), font=28, bg=(0.15, 0.15, 0.15, 1.0), text_color=(0.85, 0.85, 0.9))
sign("S2_Sign_Observation", "3. 관측실", 2219, 200, 260, yaw=180.0, size=(500, 160), font=34, bg=(0.2, 0.22, 0.25, 1.0), text_color=(0.9, 0.9, 0.85))

# ---------------------------------------------------------------- 진행 담당
director = spawn(unreal.PerspectiveStageDirector, V(0, 0, 400), label="S2_StageDirector", folder="Puzzle")
director.set_editor_property("shelf_switch", shelf_switch)
director.set_editor_property("corridor_lights", [corridor_1, corridor_2])
director.set_editor_property("giant_box", giant)
director.set_editor_property("plate", plate)
director.set_editor_property("exit_door", exit_door)

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("L_02_Perspective 저장 완료")

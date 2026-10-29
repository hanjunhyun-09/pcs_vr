# -*- coding: utf-8 -*-
"""
L_06_Ending (엔딩 컷신: 연구소 밖) 를 만든다.
노션 "스토리 > 엔딩" / "대사 초안 > 엔딩: 연구소 밖" 기준. 연출은 컷신 (카메라 장면 + 페이드).

실행 방법 (C++ 빌드 후, 에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_l06_ending.py"

다시 실행하면 L_06_Ending 안의 액터를 모두 지우고 처음부터 다시 만든다.
L_05_Core 출구 엘리베이터의 다음 레벨도 L_06_Ending 으로 연결한다.
크기 단위는 cm. 스테이지 6번이라 원점은 X = 600m (60000). 원점 = 연구소 정문 앞 바닥 가운데.

20년 동안 버려진 연구소 (+X 쪽이 건물, 플레이어가 나오는 방향 = -X):
  본관        x 0~1400, y -1600~1000, 2층(높이 820). 기둥·층 띠·창틀로 나눈 앞면, 옥상 난간·실외기·환기탑·물탱크·안테나
              창문은 깨짐 / 판자로 막음 / 때 낀 유리가 섞여 있고, 창틀 아래로 녹물이 흘러내림
  별관        x 150~1250, y -2900~-1600, 1층(높이 480). 반쯤 열린 녹슨 하역 셔터 + 하역장, 옆 비상계단
  코어동      본관 뒤 x 2600, 지름 1400 원통 + 돔 (중앙 코어 홀이 있는 건물, 이제 불이 꺼짐)
  정문        계단 3단 + 기둥 2개 + 한쪽이 내려앉은 차양, 녹슨 셔터 (컷신 2번 장면에서 올라감)
  마당        갈라진 콘크리트, 웅덩이, 틈마다 잡초, 쓰러진 울타리, 녹슨 드럼통, 기울어진 가로등, 마른 나무
  하늘        엔진 기본 하늘 구(BP_Sky_Sphere): 흘러가는 구름 + 새벽 해 색

장면 (EndingCutscene.Shots 순서):
  1 Wide    울타리 너머 마당 끝에서 연구소 전체            6초
  2 Door    정문, 셔터가 올라감                            5초
  3 Note    상자 위 세진의 쪽지                            8초
  4 Sky     코어동 돔 위로 구름이 흐르는 하늘              6초
  5 Credits 크레딧 넘기기                                  16초  → 인트로로
"""
import math
import random

import unreal

MAP = "/Game/VRPuzzle/Maps/L_06_Ending"
L05 = "/Game/VRPuzzle/Maps/L_05_Core"
MAT_DIR = "/Game/VRPuzzle/Materials"
TEX = "/Game/VRPuzzle/Textures/"
ORIGIN = unreal.Vector(60000.0, 0.0, 0.0)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone")

random.seed(2006)   # 배치가 매번 같게 (시연 연도)


def log(msg):
    unreal.log_warning("[L06] " + msg)


def V(x, y, z):
    return unreal.Vector(ORIGIN.x + x, ORIGIN.y + y, ORIGIN.z + z)


# ---------------------------------------------------------------- 머티리얼
def solid_mi(name, color, rough=0.9, emissive=0.0):
    path = MAT_DIR + "/" + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else \
        at.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, SOLID)
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Emissive", emissive)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    eal.save_loaded_asset(mi, False)
    return mi


def tex_set(path_prefix):
    return {"Color": unreal.load_asset(path_prefix + "_Color"), "Normal": unreal.load_asset(path_prefix + "_Normal"),
            "Roughness": unreal.load_asset(path_prefix + "_Roughness")}


def surface_mi(name, texs, color, tile, normal=1.2, grime=0.9, grime_h=120.0, stain=0.8, streak=0.9, metallic=0.0,
               rough=1.0, grime_color=(0.22, 0.18, 0.12)):
    """M_VRP_Surface (setup_grime_materials.py) 기반: 실제 질감 + 바닥 쪽 때 + 얼룩 + 흘러내린 자국"""
    path = MAT_DIR + "/" + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else \
        at.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, SURFACE)
    mel.clear_all_material_instance_parameters(mi)
    mel.set_material_instance_texture_parameter_value(mi, "BaseColorTex", texs["Color"])
    mel.set_material_instance_texture_parameter_value(mi, "NormalTex", texs["Normal"])
    mel.set_material_instance_texture_parameter_value(mi, "RoughnessTex", texs["Roughness"])
    vec = lambda k, c: mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(c[0], c[1], c[2], 1.0))
    sc = lambda k, v: mel.set_material_instance_scalar_parameter_value(mi, k, v)
    vec("Color", color)
    vec("GrimeColor", grime_color)
    sc("TileSize", tile)
    sc("RoughnessScale", rough)
    sc("NormalStrength", normal)
    sc("GrimeAmount", grime)
    sc("GrimeHeight", grime_h)
    sc("StainAmount", stain)
    sc("StreakAmount", streak)
    sc("FloorZ", ORIGIN.z)
    sc("Metallic", metallic)
    sc("LineWidth", 0.0)
    eal.save_loaded_asset(mi, False)
    return mi


# ---------------------------------------------------------------- 레벨 도우미
def spawn(cls, loc, yaw=0.0, label=None, folder=None, pitch=0.0, roll=0.0):
    a = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    if label:
        a.set_actor_label(label)
    if folder:
        a.set_folder_path(folder)
    return a


def mesh(label, shape, center, size, mi, folder, yaw=0.0, pitch=0.0, roll=0.0, movable=False, shadow=True):
    """기본 도형(100cm 기준)을 size(cm) 크기로, center 에 놓는다"""
    a = spawn(unreal.StaticMeshActor, V(*center), yaw=yaw, pitch=pitch, roll=roll, label=label, folder=folder)
    smc = a.static_mesh_component
    if movable:
        smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    smc.set_static_mesh(shape)
    smc.set_material(0, mi)
    smc.set_editor_property("cast_shadow", shadow)
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)   # 컷신 전용 맵이라 충돌 필요 없음
    a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return a


def box(label, x1, x2, y1, y2, z1, z2, mi, folder="Geometry", **kw):
    """x1~x2, y1~y2, z1~z2 범위를 채우는 상자"""
    return mesh(label, CUBE, ((x1 + x2) / 2.0, (y1 + y2) / 2.0, (z1 + z2) / 2.0), (x2 - x1, y2 - y1, z2 - z1), mi, folder, **kw)


def obox(label, center, size, mi, folder="Geometry", **kw):
    """가운데 기준 상자 (돌려서 놓을 때)"""
    return mesh(label, CUBE, center, size, mi, folder, **kw)


def cyl(label, x, y, z1, z2, d, mi, folder="Geometry", **kw):
    return mesh(label, CYLINDER, (x, y, (z1 + z2) / 2.0), (d, d, z2 - z1), mi, folder, **kw)


def rod(label, p0, p1, d, mi, folder):
    """p0 → p1 을 잇는 원기둥 (파이프, 나뭇가지, 울타리 기둥)"""
    dx, dy, dz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    c = ((p0[0] + p1[0]) / 2.0, (p0[1] + p1[1]) / 2.0, (p0[2] + p1[2]) / 2.0)
    a = spawn(unreal.StaticMeshActor, V(*c), label=label, folder=folder)
    smc = a.static_mesh_component
    smc.set_static_mesh(CYLINDER)
    smc.set_material(0, mi)
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    a.set_actor_rotation(unreal.MathLibrary.make_rot_from_z(unreal.Vector(dx, dy, dz)), False)
    a.set_actor_scale3d(unreal.Vector(d / 100.0, d / 100.0, length / 100.0))
    return a


def sign(label, text, x, y, z, yaw=0.0, pitch=0.0, roll=0.0, size=(800, 500), font=36, ppc=10.0,
         text_color=(0.05, 0.05, 0.05), bg=(0.85, 0.83, 0.78, 1.0), folder="Story"):
    a = spawn(unreal.PuzzleSign, V(x, y, z), yaw=yaw, pitch=pitch, roll=roll, label=label, folder=folder)
    a.set_editor_property("text", unreal.Text(text))
    a.set_editor_property("font_size", font)
    a.set_editor_property("draw_size", unreal.Vector2D(size[0], size[1]))
    a.set_editor_property("pixels_per_cm", ppc)
    a.set_editor_property("text_color", unreal.LinearColor(text_color[0], text_color[1], text_color[2], 1.0))
    a.set_editor_property("background_color", unreal.LinearColor(bg[0], bg[1], bg[2], bg[3]))
    return a


def camera(label, pos, target, fov=75.0):
    """pos 에서 target 을 바라보는 카메라"""
    dx, dy, dz = target[0] - pos[0], target[1] - pos[1], target[2] - pos[2]
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    cam = spawn(unreal.CameraActor, V(*pos), yaw=yaw, pitch=pitch, label=label, folder="Cutscene/Cameras")
    cc = cam.get_component_by_class(unreal.CameraComponent)
    cc.set_editor_property("field_of_view", fov)
    cc.set_editor_property("constrain_aspect_ratio", False)
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    return cam


def shot(cam, duration, end_cam=None, fade_in=0.8, fade_out=0.8, fade_color=(0, 0, 0), sign_actor=None,
         pages=None, move_actor=None, move_offset=(0, 0, 0)):
    s = unreal.EndingShot()
    s.set_editor_property("camera", cam)
    if end_cam:
        s.set_editor_property("end_camera", end_cam)
    s.set_editor_property("duration", duration)
    s.set_editor_property("fade_in", fade_in)
    s.set_editor_property("fade_out", fade_out)
    s.set_editor_property("fade_color", unreal.LinearColor(fade_color[0], fade_color[1], fade_color[2], 1.0))
    if sign_actor:
        s.set_editor_property("sign", sign_actor)
    if pages:
        s.set_editor_property("pages", [unreal.Text(p) for p in pages])
    if move_actor:
        s.set_editor_property("move_actor", move_actor)
        s.set_editor_property("move_offset", unreal.Vector(*move_offset))
    return s


def jitter(v, amount):
    return v + random.uniform(-amount, amount)


# ================================================================ 맵 열기 / 정리
if eal.does_asset_exist(MAP):
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    for a in eas.get_all_level_actors():
        if isinstance(a, (unreal.WorldSettings, unreal.Brush)) and not isinstance(a, unreal.Volume):
            continue
        eas.destroy_actor(a)
    log("기존 액터 정리")
else:
    les.new_level(MAP)
    log("새 레벨 만듦")

# 이전 버전(평평한 하늘 벽, 단색 건물)에서 쓰던 머티리얼은 지운다
for old in ("MI_VRP_EndSkyLow", "MI_VRP_EndSkyHigh", "MI_VRP_EndFacade", "MI_VRP_EndShutter", "MI_VRP_EndAsphalt",
            "MI_VRP_EndWindow", "MI_VRP_EndCrate"):
    if eal.does_asset_exist(MAT_DIR + "/" + old):
        eal.delete_asset(MAT_DIR + "/" + old)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("kill_z", -1000.0)

SOLID = unreal.load_asset(MAT_DIR + "/M_VRP_Solid")
SURFACE = unreal.load_asset(MAT_DIR + "/M_VRP_Surface")
T_PLASTER = tex_set(TEX + "Surfaces/T_Plaster007")
T_CONCRETE = tex_set(TEX + "Surfaces/T_Concrete036")
T_CONCRETE2 = tex_set(TEX + "Walls/T_Concrete046")
T_RUST = tex_set(TEX + "Surfaces/T_Metal021")

# 벽: 누렇게 바랜 회반죽, 바닥 쪽 1.5m까지 때, 창 아래로 흘러내린 자국
MI_PLASTER = surface_mi("MI_VRP_EndPlaster", T_PLASTER, (0.62, 0.6, 0.53), 260.0, normal=1.6, grime=0.95, grime_h=150.0,
                        stain=0.9, streak=1.0)
MI_CONC = surface_mi("MI_VRP_EndConcrete", T_CONCRETE, (0.55, 0.55, 0.53), 250.0, normal=1.4, grime=0.9, grime_h=120.0,
                     stain=0.85, streak=0.9)
MI_CONC_DARK = surface_mi("MI_VRP_EndConcreteDark", T_CONCRETE, (0.36, 0.36, 0.35), 250.0, normal=1.4, grime=0.8,
                          stain=0.9, streak=0.8)
MI_YARD = surface_mi("MI_VRP_EndYard", T_CONCRETE2, (0.42, 0.41, 0.38), 300.0, normal=1.6, grime=0.0, stain=1.0,
                     streak=0.0, grime_color=(0.18, 0.16, 0.1))
MI_RUST = surface_mi("MI_VRP_EndRust", T_RUST, (0.55, 0.42, 0.32), 120.0, metallic=0.5, grime=0.6, stain=0.8, streak=0.6)
MI_RUST_DARK = surface_mi("MI_VRP_EndRustDark", T_RUST, (0.32, 0.24, 0.18), 120.0, metallic=0.4, grime=0.6, stain=0.8,
                          streak=0.5)

MI_DARK = solid_mi("MI_VRP_EndDark", (0.02, 0.02, 0.022), rough=0.8)
MI_GLASS = solid_mi("MI_VRP_EndGlass", (0.07, 0.08, 0.075), rough=0.25)
MI_SHARD = solid_mi("MI_VRP_EndShard", (0.2, 0.22, 0.21), rough=0.2)
MI_PLANK = solid_mi("MI_VRP_EndPlank", (0.28, 0.2, 0.13), rough=0.95)
MI_PLANK_OLD = solid_mi("MI_VRP_EndPlankOld", (0.2, 0.17, 0.13), rough=1.0)
MI_GRASS = solid_mi("MI_VRP_EndGrass", (0.13, 0.15, 0.07), rough=1.0)
MI_WEED = solid_mi("MI_VRP_EndWeed", (0.09, 0.12, 0.04), rough=1.0)
MI_WEED_DRY = solid_mi("MI_VRP_EndWeedDry", (0.2, 0.17, 0.09), rough=1.0)
MI_MOSS = solid_mi("MI_VRP_EndMoss", (0.07, 0.1, 0.035), rough=1.0)
MI_VINE = solid_mi("MI_VRP_EndVine", (0.05, 0.08, 0.03), rough=1.0)
MI_BUSH_DRY = solid_mi("MI_VRP_EndBushDry", (0.16, 0.13, 0.07), rough=1.0)
MI_BARK = solid_mi("MI_VRP_EndBark", (0.12, 0.1, 0.08), rough=1.0)
MI_PUDDLE = solid_mi("MI_VRP_EndPuddle", (0.015, 0.018, 0.02), rough=0.35)
MI_CRACK = solid_mi("MI_VRP_EndCrack", (0.03, 0.03, 0.03), rough=1.0)
MI_TAPE = solid_mi("MI_VRP_EndTape", (0.75, 0.6, 0.08), rough=0.6)
MI_TANK = solid_mi("MI_VRP_EndTank", (0.4, 0.42, 0.42), rough=0.7)
MI_RUBBLE = unreal.load_asset(MAT_DIR + "/MI_VRP_Rubble")

# ================================================================ 땅 (갈라진 콘크리트 마당 + 풀밭 + 진입로)
box("Ground_Field", -9000, 9000, -9000, 9000, -30, -2, MI_GRASS, folder="Ground", shadow=False)
box("Ground_Yard", -3200, 3400, -3300, 1800, -2, 0, MI_YARD, folder="Ground", shadow=False)
box("Ground_Road", -9000, -3200, -350, 350, -2, 0, MI_YARD, folder="Ground", shadow=False)

# 갈라진 틈 (가늘고 긴 검은 줄을 꺾어 이어서)
for i in range(26):
    x, y = random.uniform(-3000, -150), random.uniform(-3000, 1600)
    yaw = random.uniform(0, 180)
    for k in range(random.randint(2, 4)):
        L = random.uniform(80, 220)
        obox("Crack_%d_%d" % (i, k), (x, y, 0.4), (L, random.uniform(2, 5), 1), MI_CRACK, folder="Ground/Cracks", yaw=yaw,
             shadow=False)
        x += math.cos(math.radians(yaw)) * L * 0.5
        y += math.sin(math.radians(yaw)) * L * 0.5
        yaw += random.uniform(-40, 40)

# 웅덩이
# 웅덩이: 타원 몇 개를 겹쳐 모양을 불규칙하게
for i, (x, y, sx, sy) in enumerate(((-900, -500, 260, 160), (-1700, 300, 180, 120), (-500, 700, 140, 90),
                                    (-2300, -900, 220, 140), (-300, -1100, 120, 80))):
    for k in range(3):
        mesh("Puddle_%d_%d" % (i, k), CYLINDER, (jitter(x, sx * 0.3), jitter(y, sy * 0.3), 0.5 + k * 0.05),
             (sx * random.uniform(0.5, 0.9), sy * random.uniform(0.5, 0.9), 1), MI_PUDDLE, "Ground/Puddles",
             yaw=random.uniform(0, 180), shadow=False)

# 틈마다 자란 잡초: 가는 풀잎 여러 장을 벌어지게 (마른 풀 섞기)
for i in range(150):
    x, y = random.uniform(-3100, -40), random.uniform(-3200, 1700)
    dry = random.random() < 0.4
    for k in range(random.randint(5, 9)):
        h = random.uniform(18, 60)
        mesh("Weed_%d_%d" % (i, k), CONE, (jitter(x, 10), jitter(y, 10), h / 2.0), (random.uniform(1.5, 3.5), random.uniform(1.5, 3.5), h),
             MI_WEED_DRY if dry or random.random() < 0.2 else MI_WEED, "Nature/Weeds", yaw=random.uniform(0, 360),
             pitch=random.uniform(-28, 28), roll=random.uniform(-28, 28), shadow=False)

# ================================================================ 본관 (2층)
BX1, BX2, BY1, BY2, BH = 0.0, 1400.0, -1600.0, 1000.0, 820.0
DW, DH = 160.0, 300.0          # 정문 반폭, 높이
FLOOR2 = 410.0
box("Main_Body", BX1 + 40, BX2, BY1, BY2, 0, BH, MI_PLASTER, folder="Lab/Main")
# 앞면 벽 (정문 구멍만 남기고)
box("Main_Front_L", BX1, BX1 + 40, BY1, -DW, 0, BH, MI_PLASTER, folder="Lab/Main")
box("Main_Front_R", BX1, BX1 + 40, DW, BY2, 0, BH, MI_PLASTER, folder="Lab/Main")
box("Main_Front_Top", BX1, BX1 + 40, -DW, DW, DH, BH, MI_PLASTER, folder="Lab/Main")
box("Main_DoorDark", BX1 + 40, BX1 + 160, -DW, DW, 0, DH, MI_DARK, folder="Lab/Main")
# 콘크리트 아랫단(굽도리)과 층 띠, 옥상 난간
box("Main_Plinth", BX1 - 15, BX1 + 2, BY1, BY2, 0, 90, MI_CONC_DARK, folder="Lab/Main")
box("Main_Band", BX1 - 20, BX1 + 2, BY1, BY2, FLOOR2 - 20, FLOOR2 + 15, MI_CONC, folder="Lab/Main")
box("Main_Parapet_F", BX1 - 25, BX1 + 20, BY1 - 20, BY2 + 20, BH, BH + 110, MI_CONC, folder="Lab/Main")
box("Main_Parapet_B", BX2 - 20, BX2 + 20, BY1 - 20, BY2 + 20, BH, BH + 110, MI_CONC, folder="Lab/Main")
box("Main_Parapet_S", BX1, BX2, BY1 - 20, BY1 + 20, BH, BH + 110, MI_CONC, folder="Lab/Main")
box("Main_Parapet_N", BX1, BX2, BY2 - 20, BY2 + 20, BH, BH + 110, MI_CONC, folder="Lab/Main")
box("Main_Coping", BX1 - 30, BX1 + 25, BY1 - 25, BY2 + 25, BH + 110, BH + 125, MI_CONC_DARK, folder="Lab/Main")
# 앞면 기둥 (4m 간격으로 앞으로 튀어나옴)
for i, y in enumerate(range(int(BY1), int(BY2) + 1, 400)):
    if abs(y) < DW + 60:
        continue
    box("Main_Pilaster_%d" % i, BX1 - 25, BX1 + 2, y - 30, y + 30, 0, BH + 110, MI_CONC, folder="Lab/Main/Pilasters")


def window(label, x_face, yc, zc, w, h, kind, folder):
    """앞면(-X를 바라봄) 창문: 창틀 + 창턱 + 상태(깨짐 / 판자 / 때 낀 유리)"""
    box(label + "_Hole", x_face - 1, x_face + 30, yc - w / 2, yc + w / 2, zc - h / 2, zc + h / 2, MI_DARK, folder=folder)
    box(label + "_Sill", x_face - 14, x_face + 2, yc - w / 2 - 12, yc + w / 2 + 12, zc - h / 2 - 10, zc - h / 2, MI_CONC_DARK,
        folder=folder)
    box(label + "_Lintel", x_face - 6, x_face + 2, yc - w / 2 - 8, yc + w / 2 + 8, zc + h / 2, zc + h / 2 + 12, MI_CONC_DARK,
        folder=folder)
    # 녹물 자국: 창턱 양 끝에서 아래로 길게
    for side in (-1, 1):
        length = random.uniform(80, 170)
        box(label + "_Rust%d" % side, x_face - 3, x_face - 1, yc + side * (w / 2 - 8) - 4, yc + side * (w / 2 - 8) + 4,
            zc - h / 2 - 10 - length, zc - h / 2 - 10, MI_RUST_DARK, folder=folder, shadow=False)
    if kind == "boarded":
        for k in range(3):
            obox(label + "_Board%d" % k, (x_face - 6, yc + jitter(0, 10), zc - h / 3 + k * h / 3),
                 (3, w + 30, 22), MI_PLANK if k % 2 == 0 else MI_PLANK_OLD, folder=folder, roll=jitter(0, 9))
    elif kind == "broken":
        # 창틀 가장자리에 남은 유리 조각
        for k in range(5):
            edge_y = yc + random.choice((-1, 1)) * (w / 2 - random.uniform(5, 25))
            obox(label + "_Shard%d" % k, (x_face + 4, edge_y, zc + random.uniform(-h / 2 + 15, h / 2 - 15)),
                 (1, random.uniform(10, 30), random.uniform(15, 40)), MI_SHARD, folder=folder, roll=random.uniform(-35, 35))
    else:
        box(label + "_Glass", x_face + 2, x_face + 4, yc - w / 2, yc + w / 2, zc - h / 2, zc + h / 2, MI_GLASS, folder=folder)
        box(label + "_Mullion", x_face, x_face + 2, yc - 3, yc + 3, zc - h / 2, zc + h / 2, MI_RUST_DARK, folder=folder)


KINDS = ["glass", "broken", "boarded", "glass", "broken", "glass", "boarded", "broken", "glass", "broken", "glass", "boarded"]
win_i = 0
for floor, zc in ((1, 230), (2, 610)):
    for y in range(int(BY1) + 200, int(BY2) - 100, 400):
        if floor == 1 and abs(y) < DW + 120:
            continue
        window("Main_Win_%d_%d" % (floor, y), BX1, y, zc, 220, 170, KINDS[win_i % len(KINDS)], "Lab/Main/Windows")
        win_i += 1

# 정문: 계단 3단, 기둥, 한쪽이 내려앉은 차양, 녹슨 셔터
for k in range(3):
    box("Entry_Step_%d" % k, -60 - k * 40, BX1, -DW - 140 + k * 10, DW + 140 - k * 10, 0, 45 - k * 15, MI_CONC_DARK,
        folder="Lab/Entry")
for side in (-1, 1):
    cyl("Entry_Column_%d" % side, -330, side * (DW + 90), 0, 360, 45, MI_CONC, folder="Lab/Entry")
obox("Entry_Canopy", (-190, 0, 372), (420, 2 * DW + 320, 22), MI_CONC_DARK, folder="Lab/Entry", roll=4.0, pitch=-3.0)
obox("Entry_Canopy_Chunk", (-420, DW + 60, 12), (70, 50, 24), MI_CONC_DARK, folder="Lab/Entry", yaw=25, roll=12)
shutter = box("Entry_Shutter", BX1 - 12, BX1 - 2, -DW, DW, 0, DH, MI_RUST, folder="Lab/Entry", movable=True)
for k in range(6):   # 셔터 위 가로 골
    z = 20 + k * 48
    obox("Entry_ShutterRib_%d" % k, (BX1 - 14, 0, z), (3, 2 * DW, 6), MI_RUST_DARK, folder="Lab/Entry", movable=True).attach_to_actor(
        shutter, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
# 출입 금지 테이프 (끊어져 늘어진 것 포함)
obox("Entry_Tape_A", (-335, 0, 140), (2, 2 * DW + 180, 8), MI_TAPE, folder="Lab/Entry", roll=8)
obox("Entry_Tape_B", (-335, -DW - 30, 60), (2, 160, 8), MI_TAPE, folder="Lab/Entry", roll=-55)

# 간판: 글자마다 따로, 하나는 떨어져 땅에, 하나는 기울어짐 (20년 방치)
LETTERS = ["착", "시", " ", "측", "정", " ", "연", "구", "소"]
for i, ch in enumerate(LETTERS):
    if ch == " ":
        continue
    y = -440 + i * 110
    if ch == "소":
        sign("End_Letter_" + str(i), ch, -120, 260, 3, yaw=200.0, pitch=88.0, size=(220, 260), font=200, ppc=2.0,
             bg=(0, 0, 0, 0), text_color=(0.12, 0.1, 0.08), folder="Story/Letters")
        continue
    roll = -14.0 if ch == "구" else jitter(0, 1.5)
    sign("End_Letter_" + str(i), ch, -27, y, 720 - (12 if ch == "구" else 0), yaw=180.0, roll=roll, size=(220, 260),
         font=200, ppc=2.0, bg=(0, 0, 0, 0), text_color=(0.12, 0.1, 0.08), folder="Story/Letters")
sign("End_Warning", "경고: 가스 누출 사고로 인해 출입을 금지합니다.\n2006. 11. 14. 안전관리팀", -27, -DW - 260, 175, yaw=180.0,
     roll=-3.0, size=(700, 260), font=30, bg=(0.62, 0.5, 0.16, 1.0), text_color=(0.08, 0.06, 0.03), folder="Story")

# 앞면 배관: 세로 빗물관 2개 (하나는 중간이 끊겨 기울어짐) + 가로 배관
rod("Pipe_Down_A", (-12, BY1 + 60, BH + 100), (-12, BY1 + 60, 0), 14, MI_RUST, "Lab/Pipes")
rod("Pipe_Down_B_Top", (-12, BY2 - 60, BH + 100), (-12, BY2 - 60, 380), 14, MI_RUST, "Lab/Pipes")
rod("Pipe_Down_B_Fall", (-12, BY2 - 60, 360), (-140, BY2 - 20, 0), 14, MI_RUST, "Lab/Pipes")
rod("Pipe_Horiz", (-20, BY1 + 60, 460), (-20, -DW - 200, 460), 18, MI_RUST_DARK, "Lab/Pipes")

# 덩굴 (벽 아래에서 위로 구불구불) + 바닥 쪽 이끼
for i in range(9):
    y = random.uniform(BY1 + 50, BY2 - 50)
    if abs(y) < DW + 100:
        continue
    x, z = -3.0, 0.0
    top = random.uniform(250, 760)
    k = 0
    while z < top:
        dz = random.uniform(40, 80)
        dy = random.uniform(-30, 30)
        rod("Vine_%d_%d" % (i, k), (x, y, z), (x, y + dy, z + dz), random.uniform(3, 6), MI_VINE, "Nature/Vines")
        for leaf in range(random.randint(2, 5)):   # 납작한 잎 여러 장
            obox("Vine_Leaf_%d_%d_%d" % (i, k, leaf), (x - 3, jitter(y + dy * 0.5, 20), z + random.uniform(0, dz)),
                 (1, random.uniform(10, 18), random.uniform(7, 12)), MI_VINE if leaf % 3 else MI_MOSS, folder="Nature/Vines",
                 roll=random.uniform(0, 180), shadow=False)
        y += dy
        z += dz
        k += 1
for i in range(14):
    y = random.uniform(BY1, BY2)
    if abs(y) < DW + 60:
        continue
    mesh("Moss_%d" % i, SPHERE, (-6, y, random.uniform(0, 40)), (14, random.uniform(80, 200), random.uniform(40, 90)),
         MI_MOSS, "Nature/Moss")

# 건물 앞 덤불
for i in range(16):
    y = random.choice((random.uniform(BY1, -DW - 250), random.uniform(DW + 250, BY2)))
    x = random.uniform(-160, -40)
    for k in range(random.randint(7, 12)):   # 작은 덩어리를 뭉쳐 울퉁불퉁하게, 일부는 말라 죽은 색
        s = random.uniform(30, 75)
        mi = random.choice((MI_VINE, MI_MOSS, MI_MOSS, MI_BUSH_DRY))
        mesh("Bush_%d_%d" % (i, k), SPHERE, (jitter(x, 55), jitter(y, 80), random.uniform(s * 0.3, s * 1.1)),
             (s * random.uniform(0.8, 1.3), s * random.uniform(0.8, 1.3), s * random.uniform(0.6, 1.0)), mi, "Nature/Bushes",
             yaw=random.uniform(0, 360))

# 옥상: 실외기, 환기탑, 물탱크(다리 위), 안테나, 무너진 난간 조각
for i, (x, y) in enumerate(((300, -1200), (300, -900), (700, 500))):
    box("Roof_HVAC_%d" % i, x - 90, x + 90, y - 110, y + 110, BH, BH + 140, MI_TANK, folder="Lab/Roof")
    cyl("Roof_HVAC_Fan_%d" % i, x, y, BH + 140, BH + 150, 120, MI_RUST_DARK, folder="Lab/Roof")
for i, (x, y, h) in enumerate(((900, -300, 380), (1050, -300, 300))):
    cyl("Roof_Stack_%d" % i, x, y, BH, BH + h, 70, MI_RUST, folder="Lab/Roof")
    cyl("Roof_StackCap_%d" % i, x, y, BH + h, BH + h + 15, 95, MI_RUST_DARK, folder="Lab/Roof")
for k, (dx, dy) in enumerate(((-80, -80), (80, -80), (-80, 80), (80, 80))):
    rod("Roof_TankLeg_%d" % k, (1100 + dx, 700 + dy, BH), (1100 + dx * 0.7, 700 + dy * 0.7, BH + 260), 10, MI_RUST_DARK, "Lab/Roof")
cyl("Roof_Tank", 1100, 700, BH + 260, BH + 520, 260, MI_RUST, folder="Lab/Roof")
mesh("Roof_TankTop", CONE, (1100, 700, BH + 560), (270, 270, 80), MI_RUST_DARK, "Lab/Roof")
rod("Roof_Antenna", (200, 800, BH), (200, 800, BH + 700), 8, MI_RUST_DARK, "Lab/Roof")
for k in range(4):
    z = BH + 300 + k * 110
    rod("Roof_AntennaBar_%d" % k, (200, 800 - 80 + k * 10, z), (200, 800 + 80 - k * 10, z + (8 if k == 2 else 0)), 5,
        MI_RUST_DARK, "Lab/Roof")
obox("Roof_ParapetChunk", (-70, 600, 25), (60, 140, 50), MI_CONC, folder="Lab/Main", yaw=17, roll=-8)

# ================================================================ 별관 (1층, 하역장) + 비상계단
WX1, WX2, WY1, WY2, WH = 150.0, 1250.0, -2900.0, -1600.0, 480.0
box("Wing_Body", WX1, WX2, WY1, WY2, 0, WH, MI_CONC, folder="Lab/Wing")
box("Wing_Parapet", WX1 - 15, WX2 + 15, WY1 - 15, WY2, WH, WH + 70, MI_CONC_DARK, folder="Lab/Wing")
box("Wing_DockHole", WX1 - 2, WX1 + 40, -2550, -2050, 120, 470, MI_DARK, folder="Lab/Wing")
obox("Wing_DockShutter", (WX1 - 25, -2300, 400), (10, 500, 150), MI_RUST, folder="Lab/Wing", pitch=-12, roll=4)
box("Wing_DockPlatform", WX1 - 350, WX1, -2700, -1900, 0, 120, MI_CONC_DARK, folder="Lab/Wing")
window("Wing_Win_0", WX1, -1800, 300, 160, 120, "boarded", "Lab/Wing/Windows")
window("Wing_Win_1", WX1, -2750, 300, 160, 120, "broken", "Lab/Wing/Windows")
# 비상계단 (별관 남쪽 벽, 지그재그 계단 + 층참)
SX = 700.0
for k in range(2):
    z0, z1 = k * 240, (k + 1) * 240
    rod("Stair_Stringer_%d" % k, (SX - 300 + k * 300, WY1 - 70, z0), (SX + k * 300, WY1 - 70, z1), 12, MI_RUST_DARK, "Lab/Stair")
    for s in range(8):
        t = (s + 0.5) / 8.0
        obox("Stair_Tread_%d_%d" % (k, s), (SX - 300 + k * 300 + 300 * t, WY1 - 70, z0 + 240 * t), (28, 110, 4), MI_RUST,
             folder="Lab/Stair")
    box("Stair_Landing_%d" % k, SX + k * 300 - 10, SX + k * 300 + 140, WY1 - 130, WY1, z1 - 6, z1, MI_RUST, folder="Lab/Stair")
    rod("Stair_Rail_%d" % k, (SX - 300 + k * 300, WY1 - 125, z0 + 100), (SX + k * 300, WY1 - 125, z1 + 100), 5, MI_RUST_DARK,
        "Lab/Stair")
rod("Stair_Post_A", (SX + 130, WY1 - 125, 0), (SX + 130, WY1 - 125, 240), 8, MI_RUST_DARK, "Lab/Stair")
rod("Stair_Post_B", (SX + 430, WY1 - 125, 0), (SX + 430, WY1 - 125, 480), 8, MI_RUST_DARK, "Lab/Stair")

# ================================================================ 코어동 (본관 뒤 원통 + 돔: 불 꺼진 중앙 코어 홀)
CX, CY = 2600.0, -200.0
cyl("Core_Drum", CX, CY, 0, 1500, 1400, MI_CONC_DARK, folder="Lab/Core")
cyl("Core_Ring", CX, CY, 1150, 1210, 1430, MI_RUST_DARK, folder="Lab/Core")
mesh("Core_Dome", SPHERE, (CX, CY, 1500), (1400, 1400, 900), MI_CONC, "Lab/Core")
rod("Core_Spire", (CX, CY, 1900), (CX, CY, 2300), 20, MI_RUST_DARK, "Lab/Core")
obox("Core_Bridge", ((BX2 + CX - 700) / 2.0, CY, 560), (CX - 700 - BX2, 300, 260), MI_CONC, folder="Lab/Core")
for k in range(6):   # 원통 둘레 세로 창 띠 (모두 깨지거나 어두움)
    a = math.radians(140 + k * 16)
    px, py = CX + 705 * math.cos(a), CY + 705 * math.sin(a)
    obox("Core_Slit_%d" % k, (px, py, 900), (6, 70, 380), MI_DARK, folder="Lab/Core", yaw=math.degrees(a))

# ================================================================ 마당: 울타리, 드럼통, 가로등, 마른 나무, 잔해
FX = -3100.0
for i, y in enumerate(range(-3200, 1800, 250)):
    if -350 < y < 350:   # 진입로 자리: 기둥만 남은 문
        continue
    fallen = -1700 < y < -1100
    lean = random.uniform(-6, 6)
    if fallen:   # 쓰러진 울타리: 마당 쪽으로 넘어짐
        obox("Fence_Fallen_%d" % i, (FX + 110, y + 125, 4), (220, 250, 3), MI_RUST_DARK, folder="Yard/Fence", yaw=jitter(0, 6))
        continue
    rod("Fence_Post_%d" % i, (FX, y, 0), (FX + math.sin(math.radians(lean)) * 220, y, 220), 7, MI_RUST_DARK, "Yard/Fence")
    rod("Fence_Rail_%d" % i, (FX, y, 205), (FX, y + 250, 205 + jitter(0, 6)), 4, MI_RUST_DARK, "Yard/Fence")
    for w in range(4):   # 철망 대신 성긴 철사
        z = 40 + w * 45
        rod("Fence_Wire_%d_%d" % (i, w), (FX, y, z), (FX, y + 250, z + jitter(0, 10)), 1.5, MI_RUST_DARK, "Yard/Fence")
rod("Gate_Post_L", (FX, -350, 0), (FX, -350, 250), 14, MI_RUST_DARK, "Yard/Fence")
rod("Gate_Post_R", (FX, 350, 0), (FX, 350, 250), 14, MI_RUST_DARK, "Yard/Fence")
obox("Gate_Leaf", (FX + 160, -300, 110), (330, 6, 200), MI_RUST, folder="Yard/Fence", yaw=68, roll=6)

for i, (x, y, tipped) in enumerate(((-700, -1450, False), (-640, -1360, False), (-760, -1330, True), (-1900, 1200, False),
                                    (-1820, 1290, True), (-300, 820, False))):
    if tipped:
        mesh("Barrel_%d" % i, CYLINDER, (x, y, 30), (58, 58, 88), MI_RUST, "Yard/Props", pitch=90, yaw=random.uniform(0, 180))
    else:
        cyl("Barrel_%d" % i, x, y, 0, 88, 58, MI_RUST, folder="Yard/Props")

for i, (x, y) in enumerate(((-1300, -700), (-1350, -620))):   # 썩은 팔레트
    for k in range(5):
        obox("Pallet_%d_%d" % (i, k), (x, y - 40 + k * 20, 8 + i * 16), (110, 12, 3), MI_PLANK_OLD, folder="Yard/Props",
             yaw=8 * i)

rod("Lamp_Pole", (-1500, -1000, 0), (-1380, -960, 620), 16, MI_RUST_DARK, "Yard/Props")
obox("Lamp_Head", (-1370, -950, 625), (80, 30, 18), MI_RUST_DARK, folder="Yard/Props", yaw=20, pitch=12)


def dead_tree(label, x, y, h):
    rod(label + "_Trunk", (x, y, 0), (jitter(x, 30), jitter(y, 30), h), h * 0.06, MI_BARK, "Nature/Trees")
    for k in range(6):
        z = h * random.uniform(0.45, 0.95)
        a = random.uniform(0, 360)
        L = h * random.uniform(0.2, 0.4)
        rod(label + "_Branch%d" % k, (x, y, z), (x + math.cos(math.radians(a)) * L, y + math.sin(math.radians(a)) * L,
                                                 z + L * random.uniform(0.3, 0.8)), h * 0.025, MI_BARK, "Nature/Trees")


dead_tree("Tree_A", -2500, 1400, 900)
dead_tree("Tree_B", -2700, -2600, 750)
dead_tree("Tree_C", -600, -2600, 650)

for i in range(40):   # 무너진 콘크리트 조각
    s = random.uniform(15, 70)
    obox("Rubble_%d" % i, (random.uniform(-2600, -80), random.uniform(-3000, 1600), s * 0.25), (s, s * 0.8, s * 0.5), MI_RUBBLE,
         folder="Yard/Rubble", yaw=random.uniform(0, 180), roll=random.uniform(-15, 15))

# ================================================================ 세진의 쪽지 (정문 계단 앞 상자 위, 대사 초안 "엔딩: 연구소 밖")
NOTE_X = -320.0
obox("Note_Crate", (NOTE_X, 0, 35), (60, 60, 70), MI_PLANK_OLD, folder="Story", yaw=7)
for k in range(3):   # 상자 판자 줄
    obox("Note_CrateSlat_%d" % k, (NOTE_X - 31, 0, 12 + k * 23), (2, 62, 4), MI_PLANK, folder="Story", yaw=7)
note = sign("End_SejinNote", "코어 불빛이 꺼지는 거, 밖에서도 보였어.\n고마워, 한결아. 20년 만에 나연이를 보내줬어.\n이제 진짜 기록을 세상에 보여주자. — 세진",
            NOTE_X, 0, 71, yaw=180.0, pitch=90.0, size=(560, 300), font=22, ppc=10.0, folder="Story")

# ================================================================ 크레딧 판 (하늘에 떠 있음)
CREDIT_POS = (-1200.0, 0.0, 2500.0)
credits = sign("End_Credits", "착시 측정 연구소", CREDIT_POS[0], CREDIT_POS[1], CREDIT_POS[2], yaw=180.0,
               size=(1200, 700), font=56, ppc=4.0, bg=(0.02, 0.02, 0.025, 1.0), text_color=(0.92, 0.9, 0.85), folder="Cutscene")

# ================================================================ 조명 / 하늘 (새벽: 낮게 뜬 해가 마당 쪽 왼편에서 건물 앞면을 비춤)
sun = spawn(unreal.DirectionalLight, V(0, 0, 3000), pitch=-14.0, yaw=25.0, label="Sun", folder="Lighting")
slc = sun.get_component_by_class(unreal.DirectionalLightComponent)
slc.set_mobility(unreal.ComponentMobility.MOVABLE)
slc.set_editor_property("intensity", 10.0)
slc.set_editor_property("light_color", unreal.Color(r=255, g=200, b=158, a=255))

sky = spawn(unreal.SkyLight, V(0, 0, 2000), label="SkyLight", folder="Lighting")
skc = sky.get_component_by_class(unreal.SkyLightComponent)
skc.set_mobility(unreal.ComponentMobility.MOVABLE)
skc.set_editor_property("real_time_capture", False)   # 실시간 캡처는 SkyAtmosphere가 있어야 해서 끔
skc.set_editor_property("intensity", 2.0)

# 구름 하늘: 엔진 기본 하늘 구. 해 위치(Sun 액터)에 맞춰 지평선·구름 색이 정해진다
# 명령줄 실행에서는 엔진 콘텐츠가 에셋 목록에 없어서 load_blueprint_class 대신 클래스 경로로 직접 불러온다
SKY_BP = unreal.load_class(None, "/Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere_C")
sky_sphere = spawn(SKY_BP, V(0, 0, 0), label="SkySphere", folder="Lighting")
for prop, value in (("Directional light actor", sun), ("Cloud speed", 1.5), ("Cloud opacity", 1.4),
                    ("Colors determined by sun position", True), ("Sun brightness", 40.0)):
    try:
        sky_sphere.set_editor_property(prop, value)
    except Exception as e:
        log("하늘 구 속성 설정 실패: %s (%s)" % (prop, e))
try:
    sky_sphere.call_method("RefreshMaterial")
except Exception as e:
    log("하늘 구 RefreshMaterial 실패: %s" % e)

fog = spawn(unreal.ExponentialHeightFog, V(0, 0, 0), label="Fog", folder="Lighting")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_editor_property("fog_density", 0.035)
fc.set_editor_property("fog_height_falloff", 0.4)
fc.set_editor_property("start_distance", 1500.0)
for prop in ("fog_inscattering_luminance", "fog_inscattering_color"):
    try:
        fc.set_editor_property(prop, unreal.LinearColor(0.85, 0.66, 0.52, 1.0))
        break
    except Exception:
        pass

pp = spawn(unreal.PostProcessVolume, V(0, 0, 0), label="PostProcess", folder="Lighting")
pp.set_editor_property("unbound", True)
st = pp.get_editor_property("settings")
# 새벽 햇빛(10 lux) 기준 장면 밝기 ≈ EV 3. 범위를 좁혀 장면이 바뀔 때 노출이 출렁이지 않게
st.set_editor_property("override_auto_exposure_min_brightness", True)
st.set_editor_property("auto_exposure_min_brightness", 2.0)
st.set_editor_property("override_auto_exposure_max_brightness", True)
st.set_editor_property("auto_exposure_max_brightness", 4.0)
st.set_editor_property("override_auto_exposure_speed_up", True)
st.set_editor_property("auto_exposure_speed_up", 20.0)
st.set_editor_property("override_auto_exposure_speed_down", True)
st.set_editor_property("auto_exposure_speed_down", 20.0)
st.set_editor_property("override_vignette_intensity", True)
st.set_editor_property("vignette_intensity", 0.6)
pp.set_editor_property("settings", st)

# 플레이어는 컷신 동안 숨겨지지만 폰이 생길 자리는 필요
spawn(unreal.PlayerStart, V(-600, 0, 1), yaw=180.0, label="PlayerStart", folder="Navigation")

# ================================================================ 카메라 + 컷신
EYE = 165.0
cam_wide = camera("Cam1_Wide", (-3350, -1500, EYE), (600, -500, 520))
cam_wide_end = camera("Cam1_Wide_End", (-2800, -1250, EYE), (600, -500, 500))
cam_door = camera("Cam2_Door", (-900, -160, EYE), (0, 0, 190))
cam_note = camera("Cam3_Note", (NOTE_X - 70, 0, 150), (NOTE_X + 5, 0, 71), fov=60.0)
cam_sky = camera("Cam4_Sky", (-1600, 900, 120), (2600, -200, 1900))
cam_sky_end = camera("Cam4_Sky_End", (-1600, 900, 120), (2600, -200, 2600))
cam_credits = camera("Cam5_Credits", (CREDIT_POS[0] - 400, 0, CREDIT_POS[2]), CREDIT_POS, fov=70.0)

CREDIT_PAGES = [
    "착시 측정 연구소\n\n컴과고 VR 프로젝트",
    "기획 · 개발\n\n(이름을 적어 주세요)",
    "등장인물\n\n강한결\n오세진\n오나연\n권혁수",
    "만든 도구\n\nUnreal Engine 5\nVR Template\nOpenXR",
    "플레이해 주셔서 감사합니다",
]

director = spawn(unreal.EndingCutscene, V(0, 0, 0), label="EndingCutscene", folder="Cutscene")
director.set_editor_property("shots", [
    shot(cam_wide, 6.0, end_cam=cam_wide_end, fade_in=1.5),
    shot(cam_door, 5.0, move_actor=shutter, move_offset=(0, 0, DH - 20)),
    shot(cam_note, 8.0, sign_actor=note),
    shot(cam_sky, 6.0, end_cam=cam_sky_end, fade_out=1.5, fade_color=(1, 1, 1)),
    shot(cam_credits, 16.0, sign_actor=credits, pages=CREDIT_PAGES, fade_in=1.5, fade_out=1.5, fade_color=(1, 1, 1)),
])
director.set_editor_property("next_level", "/Game/VRPuzzle/Maps/L_00_Intro")

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("L_06_Ending 저장 완료 (액터 %d개)" % len(eas.get_all_level_actors()))

# ================================================================ L_05 출구 엘리베이터 → L_06_Ending
unreal.EditorLoadingAndSavingUtils.load_map(L05)
linked = False
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "S5_ExitElevator":
        if str(a.get_editor_property("next_level")) != MAP or a.get_editor_property("out_of_service"):
            a.set_editor_property("next_level", MAP)
            a.set_editor_property("out_of_service", False)
            unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), L05)
        linked = True
if linked:
    log("L_05_Core 출구 엘리베이터 → L_06_Ending 연결 확인")
else:
    unreal.log_error("[L06] L_05_Core 에서 S5_ExitElevator 를 찾지 못함")

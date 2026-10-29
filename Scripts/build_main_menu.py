# -*- coding: utf-8 -*-
"""
L_MainMenu (메인 화면) 을 만든다.
엔딩 맵(L_06_Ending)의 버려진 연구소 마당을 복사해서, 시간을 해 질 녘으로 바꾸고 코어동에 아직 꺼지지 않은 빛을 켠다.
(엔딩 = 새벽, 코어가 꺼짐 / 메인 화면 = 해 질 녘, 코어가 아직 돌아감)

실행 방법 (C++ 빌드 후, 에디터를 닫은 상태에서, build_l06_ending.py 를 먼저 돌린 뒤):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_main_menu.py"

다시 실행하면 L_MainMenu 를 지우고 L_06_Ending 에서 다시 복사해 만든다.

배치 (L_06_Ending 과 같은 좌표, 원점 X = 600m):
  플레이어   마당 x -1300 에서 연구소(+X)를 바라봄
  조작대     플레이어 앞 85cm, 허리 높이 45도 기울어진 녹슨 판: [게임 시작] [종료] 버튼 (VR: 손 대고 트리거 / 데스크톱: 바라보고 E)
  제목       눈앞 4m, 눈높이보다 조금 위에 떠 있음 (TITLE 을 바꾸면 게임 이름이 바뀜)
"""
import math

import unreal

TITLE = "착시 측정 연구소"
MAP = "/Game/VRPuzzle/Maps/L_MainMenu"
SRC = "/Game/VRPuzzle/Maps/L_06_Ending"
MAT_DIR = "/Game/VRPuzzle/Materials"
ORIGIN = unreal.Vector(60000.0, 0.0, 0.0)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")


def log(msg):
    unreal.log_warning("[MENU] " + msg)


def V(x, y, z):
    return unreal.Vector(ORIGIN.x + x, ORIGIN.y + y, ORIGIN.z + z)


def solid_mi(name, color, rough=0.9, emissive=0.0):
    path = MAT_DIR + "/" + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else \
        at.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(MAT_DIR + "/M_VRP_Solid"))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Emissive", emissive)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    eal.save_loaded_asset(mi, False)
    return mi


def spawn(cls, loc, yaw=0.0, label=None, folder=None, pitch=0.0, roll=0.0):
    a = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    if label:
        a.set_actor_label(label)
    if folder:
        a.set_folder_path(folder)
    return a


def box(label, x1, x2, y1, y2, z1, z2, mi, folder="Menu"):
    a = spawn(unreal.StaticMeshActor, V((x1 + x2) / 2.0, (y1 + y2) / 2.0, (z1 + z2) / 2.0), label=label, folder=folder)
    smc = a.static_mesh_component
    smc.set_static_mesh(CUBE)
    smc.set_material(0, mi)
    a.set_actor_scale3d(unreal.Vector((x2 - x1) / 100.0, (y2 - y1) / 100.0, (z2 - z1) / 100.0))
    return a


def sign(label, text, x, y, z, yaw=180.0, size=(800, 500), font=36, ppc=10.0, text_color=(0.05, 0.05, 0.05),
         bg=(0.85, 0.83, 0.78, 1.0), folder="Menu"):
    a = spawn(unreal.PuzzleSign, V(x, y, z), yaw=yaw, label=label, folder=folder)
    a.set_editor_property("text", unreal.Text(text))
    a.set_editor_property("font_size", font)
    a.set_editor_property("draw_size", unreal.Vector2D(size[0], size[1]))
    a.set_editor_property("pixels_per_cm", ppc)
    a.set_editor_property("text_color", unreal.LinearColor(text_color[0], text_color[1], text_color[2], 1.0))
    a.set_editor_property("background_color", unreal.LinearColor(bg[0], bg[1], bg[2], bg[3]))
    return a


def color(r, g, b):
    return unreal.Color(r=r, g=g, b=b, a=255)


# ================================================================ 엔딩 맵 복사
if eal.does_asset_exist(MAP):
    eal.delete_asset(MAP)
if not eal.duplicate_asset(SRC, MAP):
    raise RuntimeError("L_06_Ending 복사 실패")
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
log("L_06_Ending 복사")

# 컷신 전용 액터는 지운다 (컷신 진행, 카메라, 크레딧, 쪽지, 플레이어 시작 자리)
actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
for label, a in list(actors.items()):
    if isinstance(a, (unreal.EndingCutscene, unreal.CameraActor, unreal.PlayerStart)) or label in ("End_Credits", "End_SejinNote"):
        eas.destroy_actor(a)
        actors.pop(label)

# ================================================================ 해 질 녘 + 아직 살아 있는 코어
sun = actors["Sun"]
sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-4.0, yaw=35.0), False)
slc = sun.get_component_by_class(unreal.DirectionalLightComponent)
slc.set_editor_property("intensity", 4.0)
slc.set_editor_property("light_color", color(255, 165, 120))

sky_sphere = actors["SkySphere"]
try:
    sky_sphere.call_method("RefreshMaterial")
except Exception as e:
    log("하늘 구 RefreshMaterial 실패: %s" % e)
actors["SkyLight"].get_component_by_class(unreal.SkyLightComponent).set_editor_property("intensity", 1.2)

MI_CORE_GLOW = solid_mi("MI_VRP_MenuCoreGlow", (0.45, 0.85, 1.0), rough=0.3, emissive=12.0)
for label, a in actors.items():
    if label == "Core_Ring" or label.startswith("Core_Slit_"):
        a.static_mesh_component.set_material(0, MI_CORE_GLOW)
glow = spawn(unreal.PointLight, V(2600, -200, 1250), label="Core_GlowLight", folder="Lighting")
glc = glow.get_component_by_class(unreal.PointLightComponent)
glc.set_mobility(unreal.ComponentMobility.MOVABLE)
glc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
glc.set_editor_property("intensity", 400.0)
glc.set_editor_property("attenuation_radius", 4000.0)
glc.set_editor_property("light_color", color(110, 200, 255))
glc.set_editor_property("cast_shadows", False)

pp = actors["PostProcess"]
st = pp.get_editor_property("settings")
st.set_editor_property("auto_exposure_min_brightness", 1.0)
st.set_editor_property("auto_exposure_max_brightness", 3.0)
pp.set_editor_property("settings", st)

# ================================================================ 플레이어 + 메뉴판
PX = -1300.0
spawn(unreal.PlayerStart, V(PX, 0, 1), yaw=0.0, label="PlayerStart", folder="Navigation")

MI_BOARD = unreal.load_asset(MAT_DIR + "/MI_VRP_EndRustDark")
MI_POST = unreal.load_asset(MAT_DIR + "/MI_VRP_EndRustDark")

# 조작대: 플레이어 앞 85cm, 허리 높이에서 플레이어 쪽으로 45도 기울어진 판 (시야를 가리지 않고, 손이 닿고, 데스크톱에서도 화면 아래쪽에 보임)
TILT = 45.0
DX, DZ = PX + 85.0, 112.0                                   # 판 가운데
N = (-math.cos(math.radians(TILT)), math.sin(math.radians(TILT)))   # 판 앞면 방향 (x, z): 플레이어 쪽 + 위
U = (math.sin(math.radians(TILT)), math.cos(math.radians(TILT)))    # 판 위쪽 방향 (x, z): 비탈 위로


def on_board(along, y, out=0.0):
    """판 가운데에서 비탈 위로 along, 옆으로 y, 앞면에서 out 만큼 떨어진 자리"""
    return V(DX + U[0] * along + N[0] * out, y, DZ + U[1] * along + N[1] * out)


board = spawn(unreal.StaticMeshActor, V(DX, 0, DZ), yaw=180.0, pitch=TILT, label="Menu_Board", folder="Menu")
board.static_mesh_component.set_static_mesh(CUBE)
board.static_mesh_component.set_material(0, MI_BOARD)
board.set_actor_scale3d(unreal.Vector(0.04, 1.3, 0.55))   # 두께 4 / 폭 130 / 비탈 길이 55
box("Menu_Post", DX + 2, DX + 12, -6, 6, 0, DZ - 10, MI_POST)

start_btn = spawn(unreal.PuzzleButton, on_board(-6, -30, 2.5), yaw=180.0, pitch=TILT, label="Menu_StartButton", folder="Menu")
start_btn.set_editor_property("cap_color", unreal.LinearColor(0.2, 0.75, 0.3, 1.0))
quit_btn = spawn(unreal.PuzzleButton, on_board(-6, 30, 2.5), yaw=180.0, pitch=TILT, label="Menu_QuitButton", folder="Menu")
quit_btn.set_editor_property("cap_color", unreal.LinearColor(0.8, 0.2, 0.15, 1.0))

LABEL = dict(size=(300, 80), font=40, ppc=10.0, bg=(0.05, 0.05, 0.05, 0.92), text_color=(0.92, 0.9, 0.85))
for label, text, y in (("Menu_StartLabel", "게임 시작", -30), ("Menu_QuitLabel", "종료", 30)):
    s = sign(label, text, 0, 0, 0, **LABEL)
    s.set_actor_location_and_rotation(on_board(14, y, 2.5), unreal.Rotator(roll=0.0, pitch=TILT, yaw=180.0), False, False)
hint = sign("Menu_Hint", "버튼에 손을 대고 트리거  (데스크톱: 바라보고 E)", 0, 0, 0, size=(900, 60), font=24, ppc=10.0,
            bg=(0.0, 0.0, 0.0, 0.0), text_color=(0.85, 0.82, 0.75))
hint.set_actor_location_and_rotation(on_board(-22, 0, 2.5), unreal.Rotator(roll=0.0, pitch=TILT, yaw=180.0), False, False)

# 제목: 눈앞 4m, 눈높이보다 조금 위에 떠 있음 (연구소 건물 앞)
sign("Menu_Title", TITLE, PX + 400, 0, 250, size=(1600, 260), font=130, ppc=4.0, bg=(0.0, 0.0, 0.0, 0.0),
     text_color=(0.95, 0.93, 0.88), folder="Menu")

director = spawn(unreal.MainMenuDirector, V(PX, 0, 0), label="MainMenuDirector", folder="Menu")
director.set_editor_property("start_button", start_btn)
director.set_editor_property("quit_button", quit_btn)
director.set_editor_property("hint_sign", hint)
director.set_editor_property("start_level", "/Game/VRPuzzle/Maps/L_00_Intro")

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("L_MainMenu 저장 완료")

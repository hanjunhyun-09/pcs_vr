# -*- coding: utf-8 -*-
"""
L_01_Isolation (1스테이지: 출입문 + 복도 + 격리실 + 엘리베이터) 그레이박스를 만든다.
노션 "맵 구조 설계" / "1스테이지 퍼즐 설계" / "대사 초안" 기준.

실행 (에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_l01_isolation.py"
또는 에디터 Output Log의 Cmd 창(Python 모드)에서:
  py "<프로젝트 경로>/Scripts/build_l01_isolation.py"

다시 실행하면 L_01_Isolation 안의 액터를 모두 지우고 처음부터 다시 만든다.
크기 단위는 cm. 스테이지 1번이라 원점은 X = 100m (10000).
"""
import unreal

MAP = "/Game/VRPuzzle/Maps/L_01_Isolation"
MAT_DIR = "/Game/VRPuzzle/Materials"
BP_DIR = "/Game/VRPuzzle/Blueprints/Interaction"
ORIGIN = unreal.Vector(10000.0, 0.0, 0.0)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")


def log(msg):
    unreal.log("[L01] " + msg)


def V(x, y, z):
    return unreal.Vector(ORIGIN.x + x, ORIGIN.y + y, ORIGIN.z + z)


# ---------------------------------------------------------------- 머티리얼
def ensure_solid_material():
    path = MAT_DIR + "/M_VRP_Solid"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = at.create_asset("M_VRP_Solid", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -100)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    emissive = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 150)
    emissive.set_editor_property("parameter_name", "Emissive")
    emissive.set_editor_property("default_value", 0.0)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 300)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.75)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 100)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(emissive, "", mul, "B")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("M_VRP_Solid 생성")
    return mat


def ensure_mi(name, parent, color):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
        parent_now = mi.get_editor_property("parent")
        if parent_now and parent_now.get_name() == "M_VRP_Surface":
            return mi   # setup_grime_materials.py가 실제 질감 + 때로 바꾼 MI는 그대로 둔다
    else:
        mi = at.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", parent)
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    eal.save_loaded_asset(mi)
    return mi


# ---------------------------------------------------------------- 블루프린트 (GrabComponent 붙이기)
def ensure_grab_bp(name, parent_class, simulate_on_drop):
    path = BP_DIR + "/" + name
    if eal.does_asset_exist(path):
        return eal.load_blueprint_class(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = at.create_asset(name, BP_DIR, None, factory)

    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    mesh_handle = None
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        if str(lib.get_variable_name(lib.get_data(h))) == "Mesh":
            mesh_handle = h
            break
    grab_class = unreal.load_object(None, "/Game/VRTemplate/Blueprints/GrabComponent.GrabComponent_C")
    params = unreal.AddNewSubobjectParams(parent_handle=mesh_handle, new_class=grab_class, blueprint_context=bp)
    new_handle, fail_reason = sub.add_new_subobject(params)
    sub.rename_subobject(new_handle, unreal.Text("GrabComponent"))
    grab = lib.get_object(lib.get_data(new_handle))
    grab.set_editor_property("bSimulateOnDrop", simulate_on_drop)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    log(name + " 생성")
    return eal.load_blueprint_class(path)


# ---------------------------------------------------------------- 레벨 도우미
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


def point_light(label, x, y, z, candela, color=(1.0, 0.97, 0.9), radius=650.0, shadows=True):
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
solid = ensure_solid_material()
MI_WALL = ensure_mi("MI_VRP_Wall", solid, (0.46, 0.5, 0.48))
MI_FLOOR = ensure_mi("MI_VRP_Floor", solid, (0.16, 0.17, 0.18))
MI_CEIL = ensure_mi("MI_VRP_Ceiling", solid, (0.3, 0.31, 0.32))
MI_TRIM = ensure_mi("MI_VRP_Trim", solid, (0.12, 0.13, 0.14))
MI_BED = ensure_mi("MI_VRP_Bed", solid, (0.3, 0.38, 0.45))
MI_SHEET = ensure_mi("MI_VRP_Sheet", solid, (0.75, 0.76, 0.74))
MI_DESK = ensure_mi("MI_VRP_Desk", solid, (0.35, 0.27, 0.2))
MI_WARN = ensure_mi("MI_VRP_Warning", solid, (0.8, 0.55, 0.05))

BP_KEYCARD = ensure_grab_bp("BP_Keycard", unreal.PuzzleGrabbable, True)
BP_PCUBE = ensure_grab_bp("BP_PerspectiveCube", unreal.PerspectiveCube, False)
# 카드 리더기 외형 블루프린트 (Scripts/build_card_reader.py가 만든다)
BP_READER = eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Props/BP_CardReader")

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
world.get_world_settings().set_editor_property("kill_z", -500.0)

# ---------------------------------------------------------------- 지오메트리
# 구역 (cm, 원점 기준)
#   출입문 앞(V)  x -300~0,    y -150~150
#   복도 A        x 0~800,     y -100~100
#   격리실(R)     x 800~1200,  y -150~350   (5m x 4m, 천장 2.8m)
#   복도 B        x 200~800,   y 150~350
#   엘리베이터    x 0~200,     y 150~350   (PuzzleElevator가 직접 만듦)
H = 280.0   # 천장 높이
T = 20.0    # 벽 두께
DOOR_W, DOOR_H = 120.0, 220.0

box("Floor", -320, 1220, -170, 370, -10, 0, MI_FLOOR)
box("Ceiling", -320, 1220, -170, 370, H, H + 10, MI_CEIL)

# 출입문 앞 (V)
box("V_WallBack", -320, -300, -170, 170, 0, H, MI_WALL)
box("V_WallLeft", -320, 0, -170, -150, 0, H, MI_WALL)
box("V_WallRight", -320, -10, 150, 170, 0, H, MI_WALL)
# 출입문이 있는 벽 (x = 0)
box("Gate_WallL", -10, 10, -170, -60, 0, H, MI_WALL)
box("Gate_WallR", -10, 10, 60, 150, 0, H, MI_WALL)
box("Gate_Header", -10, 10, -60, 60, DOOR_H, H, MI_WALL)
box("Gate_Stripe", -14, -10, -60, 60, DOOR_H, DOOR_H + 8, MI_WARN, folder="Geometry/Trim")

# 복도 A
box("A_WallLeft", 10, 780, -120, -100, 0, H, MI_WALL)
box("A_WallRight", 10, 780, 100, 120, 0, H, MI_WALL)

# 격리실 남쪽 벽 (들어온 문 y -60~60, 잠긴 문 y 190~310)
box("R_WallSouth_1", 780, 800, -170, -60, 0, H, MI_WALL)
box("R_WallSouth_2", 780, 800, 60, 190, 0, H, MI_WALL)
box("R_WallSouth_3", 780, 800, 310, 370, 0, H, MI_WALL)
box("R_Header_Entry", 780, 800, -60, 60, DOOR_H, H, MI_WALL)
box("R_Header_Exit", 780, 800, 190, 310, DOOR_H, H, MI_WALL)
box("R_WallNorth", 1200, 1220, -170, 370, 0, H, MI_WALL)
box("R_WallWest", 800, 1200, -170, -150, 0, H, MI_WALL)
box("R_WallEast", 800, 1200, 350, 370, 0, H, MI_WALL)
box("R_Baseboard", 800, 1200, -150, -146, 0, 10, MI_TRIM, folder="Geometry/Trim")

# 복도 B
box("B_WallLeft", 210, 780, 130, 150, 0, H, MI_WALL)
box("B_WallRight", 210, 780, 350, 370, 0, H, MI_WALL)

# 격리실 가구: 침대 (서쪽 벽), 책상 (북쪽 벽)
# (BP_Bed / BP_Desk는 Scripts/build_furniture.py가 만든다. 피벗 = 바닥 중앙)
spawn(eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Props/BP_Bed"), V(1060, -105, 0), label="R_Bed", folder="Props")
spawn(eal.load_blueprint_class("/Game/VRPuzzle/Blueprints/Props/BP_Desk"), V(1155, 210, 0), label="R_Desk", folder="Props")

# ---------------------------------------------------------------- 조명 / 후처리
point_light("V_Light", -150, 0, 265, 22)
point_light("A_Light_1", 200, 0, 265, 16)
point_light("A_Light_2", 600, 0, 265, 16)
room_light_1 = point_light("R_Light_1", 920, 100, 265, 40)
room_light_2 = point_light("R_Light_2", 1100, 100, 265, 40)
emergency = point_light("R_EmergencyLight", 1000, 340, 250, 5, color=(1.0, 0.15, 0.08), radius=700, shadows=False)
point_light("B_Light_1", 350, 250, 265, 16)
point_light("B_Light_2", 650, 250, 265, 16)

pp = spawn(unreal.PostProcessVolume, V(0, 0, 0), label="PostProcess", folder="Lighting")
pp.set_editor_property("unbound", True)
if intro_pp is not None:
    pp.set_editor_property("settings", intro_pp)

# ---------------------------------------------------------------- 플레이어 / 내비게이션
spawn(unreal.PlayerStart, V(-200, 0, 1), yaw=0.0, label="PlayerStart", folder="Navigation")

nav = spawn(unreal.NavMeshBoundsVolume, V(450, 100, 100), label="NavMeshBounds", folder="Navigation")
nav.set_actor_scale3d(unreal.Vector(1540 / 200.0, 540 / 200.0, 300 / 200.0))

room_entry = spawn(unreal.TriggerBox, V(1040, 100, 150), label="S1_RoomEntryVolume", folder="Puzzle")
room_entry.get_component_by_class(unreal.BoxComponent).set_editor_property("box_extent", unreal.Vector(160, 250, 150))

# ---------------------------------------------------------------- 퍼즐 부품
# 0) 출입문 + 카드 리더기 (문 오른쪽, 높이 120cm)
entrance_door = spawn(unreal.PuzzleDoor, V(0, 0, 0), label="S1_EntranceDoor", folder="Puzzle")
entrance_reader = spawn(BP_READER, V(-10, 95, 120), yaw=180.0, label="S1_EntranceReader", folder="Puzzle")
entrance_reader.set_editor_property("accept_tag", "KeycardPlayer")
entrance_reader.set_editor_property("accepted_message", unreal.Text("출입 승인 — 강한결 / 코어 측정팀"))
entrance_reader.set_editor_property("denied_message", unreal.Text("인식 실패"))

player_card = spawn(BP_KEYCARD, V(-150, 30, 100), label="S1_PlayerKeycard", folder="Puzzle")
player_card.set_actor_scale3d(unreal.Vector(0.172, 0.108, 0.004))
player_card.set_editor_property("tags", ["KeycardPlayer"])
player_card.set_editor_property("color", unreal.LinearColor(0.15, 0.4, 0.85, 1.0))
player_card.set_editor_property("holster_to_player", True)

# 1) 들어온 문 (들어오면 닫힘)
entry_door = spawn(unreal.PuzzleDoor, V(790, 0, 0), label="S1_RoomEntryDoor", folder="Puzzle")
entry_door.set_editor_property("start_open", True)

# 텔레포트 발판 (들어온 문에서 3m 이상)
pad = spawn(unreal.PuzzleTeleportPad, V(1050, 260, 0.5), label="S1_TeleportPad", folder="Puzzle")

# 2) 배전함 레버 (동쪽 벽, 높이 120cm, 발판에서 1m 이내)
lever = spawn(unreal.PuzzleLever, V(1050, 350, 120), yaw=-90.0, label="S1_PowerLever", folder="Puzzle")

# 3) 잠긴 문 + 리더기 + 봉쇄 해제 버튼
exit_door = spawn(unreal.PuzzleDoor, V(790, 250, 0), label="S1_ExitDoor", folder="Puzzle")
exit_reader = spawn(BP_READER, V(800, 160, 120), yaw=0.0, label="S1_ExitReader", folder="Puzzle")
exit_reader.set_editor_property("accept_tag", "KeycardAdmin")
exit_reader.set_editor_property("powered", False)
exit_reader.set_editor_property("accepted_message", unreal.Text("관리자 승인 — 봉쇄 해제 버튼을 눌러 주십시오."))
exit_reader.set_editor_property("denied_message", unreal.Text("권한 없음 — 강한결 / 코어 측정팀\n격리실 봉쇄 해제는 관리자 카드가 필요합니다."))
exit_button = spawn(unreal.PuzzleButton, V(800, 335, 110), yaw=0.0, label="S1_UnlockButton", folder="Puzzle")
exit_button.set_editor_property("hidden_at_start", True)

admin_card = spawn(BP_KEYCARD, V(1010, -35, 1), yaw=25.0, label="S1_AdminKeycard", folder="Puzzle")
admin_card.set_actor_scale3d(unreal.Vector(0.172, 0.108, 0.004))
admin_card.set_editor_property("tags", ["KeycardAdmin"])
admin_card.set_editor_property("color", unreal.LinearColor(0.95, 0.35, 0.05, 1.0))
admin_card.set_editor_property("emissive", 1.5)
admin_card.set_editor_property("glow_pulse", True)
# 바닥에 납작하게 놓여 있어서 손이 바닥 가까이(약 20cm)만 와도 잡히게 한다
admin_card.set_editor_property("grab_assist_extent", unreal.Vector(14, 12, 15))

# 4) 엘리베이터 + 원근 큐브 (엘리베이터 앞 1m)
elevator = spawn(unreal.PuzzleElevator, V(100, 250, 0), yaw=0.0, label="S1_Elevator", folder="Puzzle")
elevator.set_editor_property("next_level", "/Game/VRPuzzle/Maps/L_02_Perspective")
cube = spawn(BP_PCUBE, V(300, 250, 10), label="S1_PerspectiveCube", folder="Puzzle")
cube.set_actor_scale3d(unreal.Vector(0.2, 0.2, 0.2))

# ---------------------------------------------------------------- 글자 (대사 초안 그대로)
sign("S1_Sign_GasWarning",
     "경고: 가스 누출 사고로 인해 출입을 금지합니다.\n2006. 11. 14. 안전관리팀",
     -12, 0, 250, yaw=180.0, size=(900, 200), font=34, bg=(0.85, 0.65, 0.1, 1.0))
sign("S1_Sign_EntryLog",
     "출입 기록\n2006.11.14 07:52 강한결 — 입장\n2006.11.14 08:10 오나연 — 입장\n2006.11.14 08:31 오세진 — 입장\n2006.11.14 14:03 — 이후 기록 없음",
     400, -99, 160, yaw=90.0, size=(800, 420), font=32, bg=(0.25, 0.27, 0.26, 1.0), text_color=(0.85, 0.85, 0.8))
sign("S1_Sign_SafetyRules",
     "연구소 안전 수칙\n1. 실험 물체는 손으로 단단히 잡으십시오. [그립 버튼]\n2. 장비는 버튼으로 작동합니다. [트리거 버튼]\n3. 지정된 위치로만 이동하십시오. [스틱 → 이동]",
     1199, 60, 165, yaw=180.0, size=(1000, 420), font=34, bg=(0.9, 0.9, 0.85, 1.0))
schedule = sign("S1_Sign_Schedule",
     "인지 증폭 코어 공개 시연\n11월 14일 14:00 / 중앙 코어 홀\n참가자는 격리실에서 대기 / 투자사 참관 예정",
     1050, 349, 205, yaw=-90.0, size=(800, 300), font=32, visible=False)
roster = sign("S1_Sign_Roster",
     "시연 참가자 대기 명단\n참가자: —\n\n오나연 ← 소장님 지정",
     1155, 210, 76, pitch=90.0, yaw=180.0, size=(500, 360), font=30, visible=False, bg=(0.95, 0.94, 0.9, 1.0))
nametag = sign("S1_Sign_Nametag",
     "오나연 | 원근 실험실",
     1080, -105, 51, pitch=90.0, yaw=90.0, size=(420, 110), font=34, visible=False, bg=(0.95, 0.95, 0.95, 1.0))
sign("S1_Sign_LeverLabel", "격리실 전원 — 비상 봉쇄 중",
     1050, 349, 160, yaw=-90.0, size=(500, 90), font=28, bg=(0.85, 0.65, 0.1, 1.0))
sign("S1_Sign_UnlockLabel", "봉쇄 해제",
     801, 335, 128, yaw=0.0, size=(200, 70), font=26, bg=(0.15, 0.15, 0.15, 1.0), text_color=(0.95, 0.95, 0.9))
sign("S1_Sign_Rule4",
     "4. 실험 물체를 멀리 두지 마십시오.\n크기가 변할 수 있습니다.",
     380, 151, 150, yaw=90.0, size=(700, 220), font=34, bg=(0.9, 0.9, 0.85, 1.0))

# ---------------------------------------------------------------- 진행 담당
director = spawn(unreal.IsolationStageDirector, V(0, 0, 300), label="S1_StageDirector", folder="Puzzle")
director.set_editor_property("entrance_reader", entrance_reader)
director.set_editor_property("entrance_door", entrance_door)
director.set_editor_property("room_entry_door", entry_door)
director.set_editor_property("room_entry_volume", room_entry)
director.set_editor_property("pad", pad)
director.set_editor_property("lever", lever)
director.set_editor_property("exit_reader", exit_reader)
director.set_editor_property("exit_button", exit_button)
director.set_editor_property("exit_door", exit_door)
director.set_editor_property("cube", cube)
director.set_editor_property("elevator", elevator)
director.set_editor_property("room_lights", [room_light_1, room_light_2])
director.set_editor_property("emergency_lights", [emergency])
director.set_editor_property("reveal_on_power", [schedule, roster, nametag])

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("L_01_Isolation 저장 완료")

# -*- coding: utf-8 -*-
"""
카드 리더기 블루프린트(BP_CardReader, 부모 = C++ PuzzleCardReader)에 외형 부품을 붙이고,
맵에 놓인 PuzzleCardReader를 BP_CardReader로 교체한다.

- 동작(카드 인식, LED 색, 메시지 화면)은 C++ 그대로. 상속받은 Body(본체 10x15x4cm)와 Led는 건드리지 않는다.
  Body 색은 C++ BeginPlay에서 어두운 회색으로 칠해진다.
- 추가 부품: 벽 고정판, 나사 4개, 앞면 패널, 카드 투입구, LED 테두리, 작은 표시창, 경고 띠, 케이블 글랜드
- 머티리얼 인스턴스(M_VRP_Solid 기반): MI_VRP_ReaderPlate, MI_VRP_ReaderFace, MI_VRP_ReaderSlot, MI_VRP_ReaderGlass
- 교체 시 속성(AcceptTag, bPowered, 메시지, DetectRadius, 이름/폴더)을 복사하고
  IsolationStageDirector의 EntranceReader / ExitReader 참조를 새 액터로 바꾼다.

좌표: 액터 원점 = 벽 표면, +X = 벽 바깥(플레이어 쪽). 단위 cm.

실행 (에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_card_reader.py"
"""
import unreal

MAPS = ["/Game/VRPuzzle/Maps/L_00_Intro", "/Game/VRPuzzle/Maps/L_01_Isolation"]
MAT_DIR = "/Game/VRPuzzle/Materials"
BP_DIR = "/Game/VRPuzzle/Blueprints/Props"
BP_NAME = "BP_CardReader"

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYL = unreal.load_asset("/Engine/BasicShapes/Cylinder")

COPY_PROPS = ("accept_tag", "powered", "accepted_message", "denied_message", "detect_radius")


def log(msg):
    unreal.log("[CardReader] " + msg)


def ensure_mi(name, color, rough):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = at.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, unreal.load_asset(MAT_DIR + "/M_VRP_Solid"))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    eal.save_loaded_asset(mi, False)
    return mi


# ---------------------------------------------------------------- 블루프린트
def ensure_bp():
    path = BP_DIR + "/" + BP_NAME
    if eal.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.PuzzleCardReader)
        bp = at.create_asset(BP_NAME, BP_DIR, None, factory)
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    root = None
    added = []
    for h in handles:
        data = lib.get_data(h)
        if str(lib.get_variable_name(data)) == "Root":
            root = h
        elif not lib.is_inherited_component(data) and not lib.is_default_scene_root(data) and lib.is_component(data):
            added.append(h)
    if root is None:
        raise RuntimeError("상속된 Root 컴포넌트를 찾지 못함")
    if added:
        sub.delete_subobjects(root, added, bp)
    return bp, root


def part(bp, root, name, mesh, center, size, mi, rot=(0.0, 0.0, 0.0)):
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = sub.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError("부품 추가 실패: %s (%s)" % (name, fail))
    sub.rename_subobject(handle, unreal.Text(name))
    comp = lib.get_object(lib.get_data(handle))
    comp.set_editor_property("static_mesh", mesh)
    comp.set_editor_property("relative_location", unreal.Vector(*center))
    comp.set_editor_property("relative_rotation", unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    comp.set_editor_property("relative_scale3d", unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    comp.set_editor_property("override_materials", [mi])
    if size[0] < 1.0 or min(size) < 1.0:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return comp


def disc_x(bp, root, name, center, diameter, depth, mi):
    """앞(+X)을 보는 원판/원기둥"""
    return part(bp, root, name, CYL, center, (diameter, diameter, depth), mi, rot=(0.0, 90.0, 0.0))


# ================================================================ 머티리얼
MI_PLATE = ensure_mi("MI_VRP_ReaderPlate", (0.30, 0.34, 0.36), 0.5)    # 칠한 금속 고정판 (차가운 회청색)
MI_FACE = ensure_mi("MI_VRP_ReaderFace", (0.20, 0.21, 0.22), 0.6)      # 회색 플라스틱 앞면
MI_SLOT = ensure_mi("MI_VRP_ReaderSlot", (0.01, 0.01, 0.01), 0.9)      # 투입구 안쪽
MI_GLASS = ensure_mi("MI_VRP_ReaderGlass", (0.02, 0.03, 0.035), 0.08)  # 표시창 유리
MI_SCREW = unreal.load_asset(MAT_DIR + "/MI_VRP_Handle")
MI_WARN = unreal.load_asset(MAT_DIR + "/MI_VRP_Warning")

# ================================================================ 외형 (본체 = x 0~4, y ±5, z ±7.5)
bp, root = ensure_bp()
part(bp, root, "BackPlate", CUBE, (0.5, 0, 0), (1, 13, 20), MI_PLATE)
for sy in (1, -1):
    for sz in (1, -1):
        disc_x(bp, root, "Screw_%s%s" % ("T" if sz > 0 else "B", "L" if sy > 0 else "R"), (1.1, 5.5 * sy, 8.8 * sz), 0.9, 0.4, MI_SCREW)
part(bp, root, "FacePanel", CUBE, (4.15, 0, -2.0), (0.3, 8.6, 8.5), MI_FACE)
# 카드 투입구: 검은 홈 + 위아래 립
part(bp, root, "CardSlot", CUBE, (4.35, 0, -1.5), (0.4, 6.8, 0.7), MI_SLOT)
part(bp, root, "SlotLipTop", CUBE, (4.55, 0, -0.85), (0.6, 7.4, 0.6), MI_FACE)
part(bp, root, "SlotLipBottom", CUBE, (4.55, 0, -2.15), (0.6, 7.4, 0.6), MI_FACE)
# LED 테두리 (C++ Led: x 4, z 5, 지름 2.5)
disc_x(bp, root, "LedBezel", (4.1, 0, 5.0), 3.6, 0.5, MI_SLOT)
# 작은 표시창
part(bp, root, "Display", CUBE, (4.1, 0, 2.2), (0.25, 6.0, 1.8), MI_GLASS)
# 카드 그림 대신 노란 경고 띠
part(bp, root, "WarnStripe", CUBE, (4.35, 0, -5.4), (0.2, 7.0, 0.8), MI_WARN)
# 아래쪽 케이블 글랜드
part(bp, root, "CableGland", CYL, (1.5, 0, -10.8), (1.6, 1.6, 2.2), MI_SCREW)

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_loaded_asset(bp, False)
BP_CLASS = eal.load_blueprint_class(BP_DIR + "/" + BP_NAME)
log(BP_NAME + " 저장")


# ================================================================ 맵 교체
def is_bp_reader(a):
    return a.get_class().get_name() == BP_NAME + "_C"


for m in MAPS:
    unreal.EditorLoadingAndSavingUtils.load_map(m)
    actors = eas.get_all_level_actors()
    old = [a for a in actors if isinstance(a, unreal.PuzzleCardReader) and not is_bp_reader(a)]
    if not old:
        log(m + ": 교체할 리더기 없음")
        continue
    directors = [a for a in actors if isinstance(a, unreal.IsolationStageDirector)]
    for a in old:
        label = a.get_actor_label()
        new = eas.spawn_actor_from_class(BP_CLASS, a.get_actor_location(), a.get_actor_rotation())
        new.set_actor_scale3d(a.get_actor_scale3d())
        for p in COPY_PROPS:
            new.set_editor_property(p, a.get_editor_property(p))
        new.set_folder_path(a.get_folder_path())
        new.set_editor_property("tags", a.get_editor_property("tags"))
        for d in directors:
            for p in ("entrance_reader", "exit_reader"):
                if d.get_editor_property(p) == a:
                    d.set_editor_property(p, new)
                    log("%s.%s -> 새 리더기" % (d.get_actor_label(), p))
        eas.destroy_actor(a)
        new.set_actor_label(label)
        log("%s: %s 교체" % (m, label))
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.EditorLoadingAndSavingUtils.save_map(world, m)
log("완료")

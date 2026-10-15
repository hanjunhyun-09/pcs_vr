# -*- coding: utf-8 -*-
"""
격리실 가구 블루프린트(BP_Bed, BP_Desk)를 기본 도형(큐브/원기둥/구) 조합으로 만들고,
L_01_Isolation의 박스 가구(R_Bed, R_BedSheet, R_BedPillow, R_DeskTop, R_DeskLegs)를 이것으로 교체한다.

- 블루프린트: /Game/VRPuzzle/Blueprints/Props
- 머티리얼 인스턴스(M_VRP_Solid 기반): MI_VRP_BedFrame, MI_VRP_Mattress, MI_VRP_Blanket, MI_VRP_Pillow,
                                     MI_VRP_DeskWood, MI_VRP_DeskMetal, MI_VRP_DeskDrawer, MI_VRP_Handle
  → 색/거칠기는 이 인스턴스만 바꾸면 블루프린트 전체에 반영된다.
- 다시 실행하면 블루프린트의 부품을 지우고 새로 만든다 (레벨에 놓인 인스턴스는 그대로 갱신됨).

피벗: 바닥 중앙. 침대는 +X가 머리 쪽, 책상은 +X가 벽(뒤) 쪽.
  침대 200 x 90, 이불 윗면 높이 50cm (이름표 사인이 51cm에 놓임)
  책상 90 x 180, 상판 윗면 높이 74.5cm (명단 사인이 76cm에 놓임)

실행 (에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/build_furniture.py"
"""
import unreal

MAP = "/Game/VRPuzzle/Maps/L_01_Isolation"
MAT_DIR = "/Game/VRPuzzle/Materials"
BP_DIR = "/Game/VRPuzzle/Blueprints/Props"
ORIGIN = unreal.Vector(10000.0, 0.0, 0.0)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYL = unreal.load_asset("/Engine/BasicShapes/Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")


def log(msg):
    unreal.log("[Furniture] " + msg)


# ---------------------------------------------------------------- 머티리얼
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


# ---------------------------------------------------------------- 블루프린트 도우미
class BPBuilder(object):
    def __init__(self, name):
        path = BP_DIR + "/" + name
        if eal.does_asset_exist(path):
            self.bp = unreal.load_asset(path)
        else:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", unreal.Actor)
            self.bp = at.create_asset(name, BP_DIR, None, factory)
        self.path = path
        handles = sub.k2_gather_subobject_data_for_blueprint(self.bp)
        self.root = handles[0]
        old = [h for h in handles[1:]]
        if old:
            sub.delete_subobjects(self.root, old, self.bp)

    def part(self, name, mesh, center, size, mi, rot=(0.0, 0.0, 0.0)):
        """center/size는 cm. 기본 도형은 100cm 크기이므로 size/100이 스케일. rot = (roll, pitch, yaw)"""
        params = unreal.AddNewSubobjectParams(parent_handle=self.root, new_class=unreal.StaticMeshComponent, blueprint_context=self.bp)
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
        return comp

    def box(self, name, center, size, mi):
        return self.part(name, CUBE, center, size, mi)

    def post(self, name, center, diameter, height, mi):
        """세로 원기둥"""
        return self.part(name, CYL, center, (diameter, diameter, height), mi)

    def bar_y(self, name, center, diameter, length, mi):
        """Y축 방향으로 누운 원기둥"""
        return self.part(name, CYL, center, (diameter, diameter, length), mi, rot=(90.0, 0.0, 0.0))

    def finish(self):
        unreal.BlueprintEditorLibrary.compile_blueprint(self.bp)
        eal.save_loaded_asset(self.bp, False)
        log(self.path + " 저장")
        return eal.load_blueprint_class(self.path)


# ================================================================ 머티리얼
MI_FRAME = ensure_mi("MI_VRP_BedFrame", (0.42, 0.47, 0.46), 0.45)      # 칠이 벗겨진 회녹색 금속
MI_MATTRESS = ensure_mi("MI_VRP_Mattress", (0.78, 0.77, 0.72), 0.9)    # 누렇게 바랜 흰 매트리스
MI_BLANKET = ensure_mi("MI_VRP_Blanket", (0.24, 0.31, 0.38), 0.95)     # 회청색 담요
MI_PILLOW = ensure_mi("MI_VRP_Pillow", (0.86, 0.86, 0.83), 0.9)
MI_WOOD = ensure_mi("MI_VRP_DeskWood", (0.36, 0.24, 0.15), 0.55)       # 오래된 합판 상판
MI_DMETAL = ensure_mi("MI_VRP_DeskMetal", (0.16, 0.17, 0.18), 0.45)
MI_DRAWER = ensure_mi("MI_VRP_DeskDrawer", (0.55, 0.57, 0.56), 0.5)
MI_HANDLE = ensure_mi("MI_VRP_Handle", (0.7, 0.7, 0.68), 0.25)

# ================================================================ 침대 (병원/연구소 철제 침대)
b = BPBuilder("BP_Bed")
for sx, sy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
    b.post("Leg_%s%s" % ("H" if sx > 0 else "F", "L" if sy > 0 else "R"), (95 * sx, 42 * sy, 14), 5, 28, MI_FRAME)
b.box("RailL", (0, 42, 26), (190, 3, 8), MI_FRAME)
b.box("RailR", (0, -42, 26), (190, 3, 8), MI_FRAME)
b.box("Base", (0, 0, 25), (192, 82, 3), MI_FRAME)
# 머리판
b.post("HeadPostL", (97.5, 42, 47.5), 5, 95, MI_FRAME)
b.post("HeadPostR", (97.5, -42, 47.5), 5, 95, MI_FRAME)
b.box("HeadPanel", (97.5, 0, 70), (3, 84, 40), MI_FRAME)
b.bar_y("HeadBar", (97.5, 0, 93), 4, 88, MI_FRAME)
# 발판
b.post("FootPostL", (-97.5, 42, 35), 5, 70, MI_FRAME)
b.post("FootPostR", (-97.5, -42, 35), 5, 70, MI_FRAME)
b.box("FootPanel", (-97.5, 0, 50), (3, 84, 25), MI_FRAME)
b.bar_y("FootBar", (-97.5, 0, 68), 4, 88, MI_FRAME)
# 매트리스 / 담요 / 베개
b.box("Mattress", (0, 0, 37), (190, 84, 18), MI_MATTRESS)
b.box("Blanket", (-27.5, 0, 48), (135, 88, 4), MI_BLANKET)
b.box("BlanketSideL", (-27.5, 44, 41), (135, 2, 18), MI_BLANKET)
b.box("BlanketSideR", (-27.5, -44, 41), (135, 2, 18), MI_BLANKET)
b.box("BlanketFoot", (-95, 0, 41), (2, 88, 18), MI_BLANKET)
b.box("SheetFold", (36, 0, 48.5), (12, 89, 5), MI_PILLOW)
b.part("Pillow", SPHERE, (72, 0, 52), (30, 62, 12), MI_PILLOW)
BP_BED = b.finish()

# ================================================================ 책상 (철제 사무용 책상 + 서랍장)
d = BPBuilder("BP_Desk")
d.box("Top", (0, 0, 72.5), (90, 180, 4), MI_WOOD)
d.box("ApronFront", (-40, -22, 66), (2, 125, 8), MI_DMETAL)
d.box("ApronBack", (40, -22, 66), (2, 125, 8), MI_DMETAL)
d.box("LegFL", (-41, -86, 35.25), (4, 4, 70.5), MI_DMETAL)
d.box("LegBL", (41, -86, 35.25), (4, 4, 70.5), MI_DMETAL)
d.box("LegBar", (0, -86, 10), (78, 3, 3), MI_DMETAL)
d.box("ModestyPanel", (42, -22, 50), (1.5, 125, 35), MI_DMETAL)
# 오른쪽 서랍장
d.box("Pedestal", (0, 64, 39), (80, 45, 62), MI_DRAWER)
d.box("Plinth", (0, 64, 4), (76, 41, 8), MI_DMETAL)
for i, (z, h) in enumerate(((60, 17), (41, 17), (19.5, 22))):
    d.box("Drawer%d" % (i + 1), (-40.75, 64, z), (1.5, 42, h), MI_DRAWER)
    d.box("Handle%d" % (i + 1), (-42.5, 64, z + h / 2.0 - 5), (2, 14, 2), MI_HANDLE)
BP_DESK = d.finish()

# ================================================================ L_01 배치 교체
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
OLD = ("R_Bed", "R_BedSheet", "R_BedPillow", "R_DeskTop", "R_DeskLegs", "R_Desk")
for a in eas.get_all_level_actors():
    if a.get_actor_label() in OLD:
        eas.destroy_actor(a)


def place(cls, x, y, yaw, label):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(ORIGIN.x + x, ORIGIN.y + y, ORIGIN.z), unreal.Rotator(0, 0, yaw))
    a.set_actor_label(label)
    a.set_folder_path("Props")
    return a


place(BP_BED, 1060, -105, 0.0, "R_Bed")
place(BP_DESK, 1155, 210, 0.0, "R_Desk")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("L_01_Isolation 가구 교체 완료")

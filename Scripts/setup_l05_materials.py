# -*- coding: utf-8 -*-
"""
L_05_Core (5스테이지 중앙 코어)의 벽·바닥·천장·금속에 실제 질감 + 20년 방치된 때를 입힌다.
setup_grime_materials.py와 같은 방식이지만, 원본 jpg를 다시 가져오지 않고 이미 프로젝트에 들어 있는 텍스처와
M_VRP_Surface를 그대로 쓴다 (원본 _Downloads 폴더가 없는 PC에서도 실행됨).

- build_l05_core.py가 만든 MI(MI_VRP_Core*)의 부모를 M_VRP_Surface로 바꾼다 → 맵을 다시 만들지 않아도 바로 적용
  (build_l05_core.py의 ensure_mi는 부모가 이미 M_VRP_Surface인 MI를 건드리지 않으므로 맵을 다시 만들어도 유지됨)
- 코어 빛(MI_VRP_CoreGlow), 원판 그림(MI_VRP_CorePaint), 제어판 램프(MI_VRP_PanelLampGlow)는 진행 담당 / 퍼즐이
  M_VRP_Solid의 색·발광을 직접 바꾸므로 그대로 둔다

실행 (에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/setup_l05_materials.py"
여러 번 실행해도 결과는 같다.
"""
import unreal

MAT_DIR = "/Game/VRPuzzle/Materials"
TEX_DIR = "/Game/VRPuzzle/Textures/Surfaces"

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning("[L05Mat] " + msg)


def texture_set(asset_id):
    """setup_grime_materials.py가 가져온 텍스처 (T_<ID>_Color / _Normal / _Roughness)"""
    base = TEX_DIR + "/T_" + asset_id
    texs = {"Color": unreal.load_asset(base + "_Color"), "NormalDX": unreal.load_asset(base + "_Normal"),
            "Roughness": unreal.load_asset(base + "_Roughness")}
    for k, t in texs.items():
        if t is None:
            raise RuntimeError("텍스처 없음: %s_%s (setup_grime_materials.py를 먼저 실행해야 함)" % (base, k))
    return texs


def setup_mi(name, parent, texs, color, tile, rough=1.0, normal=1.0, line=None, grime=0.7, grime_h=60.0, stain=0.5,
             streak=0.6, floor_z=0.0, metallic=0.0, grime_color=(0.30, 0.25, 0.18)):
    """setup_grime_materials.py의 setup_mi와 같음. line = (LineWidth cm, LineColor) 또는 None"""
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = at.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    mel.set_material_instance_texture_parameter_value(mi, "BaseColorTex", texs["Color"])
    mel.set_material_instance_texture_parameter_value(mi, "NormalTex", texs["NormalDX"])
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
    sc("FloorZ", floor_z)
    sc("Metallic", metallic)
    if line:
        sc("LineWidth", line[0])
        vec("LineColor", line[1])
    else:
        sc("LineWidth", 0.0)
    eal.save_loaded_asset(mi, False)
    log(name + " → M_VRP_Surface")
    return mi


# ================================================================ 시작
surface = unreal.load_asset(MAT_DIR + "/M_VRP_Surface")
if surface is None:
    raise RuntimeError("M_VRP_Surface 없음 (setup_grime_materials.py를 먼저 실행해야 함)")

concrete_old = texture_set("Concrete036")    # 오래된 콘크리트
tiles_ind = texture_set("Tiles140")          # 부식된 산업용 타일
plaster_old = texture_set("Plaster007")      # 낡은 회벽
metal_rust = texture_set("Metal021")         # 녹슨 금속

# ---- 코어 홀 + 장치 구역: 차가운 푸른빛 콘크리트, 물 흐른 자국이 많은 높은 벽 (천장 10m)
setup_mi("MI_VRP_CoreWall", surface, concrete_old, (0.42, 0.47, 0.55), 280.0, normal=1.4, grime=0.9, grime_h=90.0,
         stain=0.6, streak=0.9)
setup_mi("MI_VRP_CoreFloor", surface, tiles_ind, (0.55, 0.58, 0.63), 160.0, stain=0.65)
setup_mi("MI_VRP_CoreCeiling", surface, concrete_old, (0.3, 0.32, 0.37), 280.0, grime=0.0, stain=0.7)

# ---- 원근법 구역 바닥: 2스테이지처럼 실제 질감 위에 1m 격자 페인트 선 (축을 줄이고 키울 때 거리감)
setup_mi("MI_VRP_CorePerspFloor", surface, concrete_old, (0.62, 0.62, 0.62), 250.0,
         line=(3.0, (0.85, 0.82, 0.62)), stain=0.6)

# ---- 금속: 받침대, 기둥, 코어 띠, 난간, 발코니, 제어판 / 어두운 금속: 슬롯 판, 배선, 램프 틀, 원판 바깥 고리, 보관함
setup_mi("MI_VRP_CoreMetal", surface, metal_rust, (0.55, 0.58, 0.62), 120.0, metallic=0.6, grime=0.0, stain=0.35,
         streak=0.25)
setup_mi("MI_VRP_CoreDark", surface, metal_rust, (0.13, 0.14, 0.16), 120.0, metallic=0.5, rough=0.9, grime=0.0,
         stain=0.3, streak=0.15)

log("완료")

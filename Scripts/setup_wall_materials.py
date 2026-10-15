# -*- coding: utf-8 -*-
"""
벽 머티리얼에 실제 질감(ambientCG CC0 텍스처)을 입힌다.

- 텍스처 가져오기: <프로젝트>/../_Downloads/Textures/<ID>/<ID>_2K-JPG_{Color,NormalDX,Roughness}.jpg
  → /Game/VRPuzzle/Textures/Walls
- M_VRP_WallTextured: 월드 정렬(World Aligned) 매핑이라 크기를 늘린 큐브 벽에서도 텍스처가 늘어나지 않는다.
  파라미터: Color(색 틴트, 기존 M_VRP_Solid와 같은 이름), TileSize(cm), RoughnessScale, NormalStrength,
            BaseColorTex / NormalTex / RoughnessTex
- MI_VRP_Wall의 부모를 M_VRP_WallTextured로 바꾼다 → 이 머티리얼을 쓰는 모든 맵의 벽에 바로 적용된다.
  (build_l01_isolation.py를 다시 돌려도 MI_VRP_Wall은 이미 있으므로 부모가 유지된다)
- MI_VRP_Wall_Concrete: 콘크리트 질감 대안 (필요한 벽에 직접 지정)

실행 (에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/setup_wall_materials.py"
또는 에디터 Output Log(Cmd 창 Python 모드)에서:
  py "<프로젝트 경로>/Scripts/setup_wall_materials.py"
"""
import os
import shutil
import unreal

MAT_DIR = "/Game/VRPuzzle/Materials"
TEX_DIR = "/Game/VRPuzzle/Textures/Walls"
SRC_DIR = os.path.normpath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "..", "_Downloads", "Textures"))

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

WA_TEXTURE = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture")
WA_NORMAL = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal")


def log(msg):
    unreal.log("[WallMat] " + msg)


# ---------------------------------------------------------------- 텍스처
def import_texture(asset_id, kind):
    """kind: Color / NormalDX / Roughness"""
    name = "T_%s_%s" % (asset_id, "Normal" if kind == "NormalDX" else kind)
    path = TEX_DIR + "/" + name
    if not eal.does_asset_exist(path):
        src = os.path.join(SRC_DIR, asset_id, "%s_2K-JPG_%s.jpg" % (asset_id, kind))
        if not os.path.exists(src):
            raise RuntimeError("텍스처 파일 없음: " + src)
        # Interchange는 destination_name을 무시하고 파일 이름을 에셋 이름으로 쓰므로, 원하는 이름으로 복사해서 가져온다
        staged_dir = os.path.join(SRC_DIR, "_staged")
        os.makedirs(staged_dir, exist_ok=True)
        staged = os.path.join(staged_dir, name + ".jpg")
        shutil.copyfile(src, staged)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", staged)
        task.set_editor_property("destination_path", TEX_DIR)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        at.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    if kind == "NormalDX":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    elif kind == "Roughness":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    eal.save_loaded_asset(tex, False)
    return tex


def import_set(asset_id):
    return {k: import_texture(asset_id, k) for k in ("Color", "NormalDX", "Roughness")}


# ---------------------------------------------------------------- 머티리얼
def connect(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("연결 실패: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def tex_param(mat, name, tex, x, y, sampler):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("texture", tex)
    e.set_editor_property("sampler_type", sampler)
    return e


def scalar_param(mat, name, value, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def ensure_wall_material(texs):
    path = MAT_DIR + "/M_VRP_WallTextured"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = at.create_asset("M_VRP_WallTextured", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    tile = scalar_param(mat, "TileSize", 200.0, -1100, 0)

    # 색: 월드 정렬 텍스처 x Color 틴트
    t_col = tex_param(mat, "BaseColorTex", texs["Color"], -1100, -400, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    wa_col = mel.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -700, -400)
    wa_col.set_material_function(WA_TEXTURE)
    connect(t_col, "", wa_col, "TextureObject")
    connect(tile, "", wa_col, "TextureSize")
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, -600)
    tint.set_editor_property("parameter_name", "Color")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    col_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, -450)
    connect(wa_col, "XYZ Texture", col_mul, "A")
    connect(tint, "", col_mul, "B")
    mel.connect_material_property(col_mul, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # 거칠기
    t_rough = tex_param(mat, "RoughnessTex", texs["Roughness"], -1100, 200, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    wa_rough = mel.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -700, 200)
    wa_rough.set_material_function(WA_TEXTURE)
    connect(t_rough, "", wa_rough, "TextureObject")
    connect(tile, "", wa_rough, "TextureSize")
    mask_r = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -450, 200)
    mask_r.set_editor_property("r", True)
    mask_r.set_editor_property("g", False)
    mask_r.set_editor_property("b", False)
    mask_r.set_editor_property("a", False)
    connect(wa_rough, "XYZ Texture", mask_r, "")
    rough_scale = scalar_param(mat, "RoughnessScale", 1.0, -450, 350)
    rough_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 220)
    connect(mask_r, "", rough_mul, "A")
    connect(rough_scale, "", rough_mul, "B")
    mel.connect_material_property(rough_mul, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # 노멀 (강도 조절: XY에 NormalStrength를 곱함)
    t_nrm = tex_param(mat, "NormalTex", texs["NormalDX"], -1100, 600, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    wa_nrm = mel.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -700, 600)
    wa_nrm.set_material_function(WA_NORMAL)
    connect(t_nrm, "", wa_nrm, "TextureObject")
    connect(tile, "", wa_nrm, "TextureSize")
    strength = scalar_param(mat, "NormalStrength", 1.0, -700, 850)
    flat = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -450, 850)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    nrm_lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -250, 650)
    connect(flat, "", nrm_lerp, "A")
    connect(wa_nrm, "XYZ Texture", nrm_lerp, "B")
    connect(strength, "", nrm_lerp, "Alpha")
    mel.connect_material_property(nrm_lerp, "", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("M_VRP_WallTextured 생성")
    return mat


def setup_mi(name, parent, texs, color, tile, rough=1.0, normal=1.0):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = at.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_texture_parameter_value(mi, "BaseColorTex", texs["Color"])
    mel.set_material_instance_texture_parameter_value(mi, "NormalTex", texs["NormalDX"])
    mel.set_material_instance_texture_parameter_value(mi, "RoughnessTex", texs["Roughness"])
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "TileSize", tile)
    mel.set_material_instance_scalar_parameter_value(mi, "RoughnessScale", rough)
    mel.set_material_instance_scalar_parameter_value(mi, "NormalStrength", normal)
    eal.save_loaded_asset(mi, False)
    log(name + " 설정")
    return mi


# ================================================================ 시작
plaster = import_set("PaintedPlaster017")
concrete = import_set("Concrete046")
wall_mat = ensure_wall_material(plaster)

# 기존 벽: 2000년대 연구소 느낌의 칠한 회벽 (기존 회녹색 톤 유지)
setup_mi("MI_VRP_Wall", wall_mat, plaster, (0.62, 0.68, 0.65), 180.0, rough=1.0, normal=1.8)
# 대안: 노출 콘크리트
setup_mi("MI_VRP_Wall_Concrete", wall_mat, concrete, (0.85, 0.85, 0.85), 250.0, rough=1.0, normal=1.2)
log("완료")

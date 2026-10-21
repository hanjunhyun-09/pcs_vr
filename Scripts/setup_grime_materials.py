# -*- coding: utf-8 -*-
"""
20년 동안 폐쇄된 연구소 느낌: 벽·바닥·천장에 실제 질감(ambientCG CC0) + 때·얼룩·물 흐른 자국을 입힌다.

- 텍스처 가져오기: <프로젝트>/../_Downloads/Textures/<ID>/<ID>_2K-JPG_{Color,NormalDX,Roughness}.jpg
  → /Game/VRPuzzle/Textures/Surfaces
  (Plaster007 낡은 회벽, Concrete036 오래된 콘크리트, Tiles141 더러운 바닥 타일, Tiles140 부식된 산업용 타일, Metal021 녹슨 금속)
- M_VRP_Surface: 월드 정렬 텍스처 x Color 틴트
    + 격자선 (LineWidth > 0일 때. 2스테이지처럼 거리감이 필요한 방은 실제 질감 위에 페인트 선으로 1m 격자를 남긴다)
    + 때: 벽 아래쪽 띠(GrimeHeight), 큰 얼룩(StainAmount), 위에서 흘러내린 세로 자국(StreakAmount, 벽에만)
- 기존 MI(벽/바닥/천장)의 부모를 M_VRP_Surface로 바꾼다 → 맵을 다시 만들지 않아도 바로 적용된다.
  맵 빌드 스크립트의 ensure_mi는 부모가 이미 M_VRP_Surface인 MI는 건드리지 않는다.

실행 (에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/setup_grime_materials.py"
여러 번 실행해도 된다 (M_VRP_Surface는 매번 새로 만든다).
"""
import os
import shutil
import unreal

MAT_DIR = "/Game/VRPuzzle/Materials"
TEX_DIR = "/Game/VRPuzzle/Textures/Surfaces"
SRC_DIR = os.path.normpath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "..", "_Downloads", "Textures"))

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

WA_TEXTURE = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture")
WA_NORMAL = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal")


def log(msg):
    unreal.log_warning("[Grime] " + msg)


# ---------------------------------------------------------------- 텍스처 (setup_wall_materials.py와 같은 방식)
def import_texture(asset_id, kind):
    """kind: Color / NormalDX / Roughness"""
    name = "T_%s_%s" % (asset_id, "Normal" if kind == "NormalDX" else kind)
    path = TEX_DIR + "/" + name
    # 원본이 바뀌어도 늘 다시 가져온다 (작은 텍스처라 빠름)
    src = os.path.join(SRC_DIR, asset_id, "%s_2K-JPG_%s.jpg" % (asset_id, kind))
    if not os.path.exists(src):
        raise RuntimeError("텍스처 파일 없음: " + src)
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


def existing_set(prefix):
    """setup_wall_materials.py가 이미 가져온 텍스처 (T_<ID>_Color 등)"""
    base = "/Game/VRPuzzle/Textures/Walls/T_" + prefix
    return {"Color": unreal.load_asset(base + "_Color"), "NormalDX": unreal.load_asset(base + "_Normal"),
            "Roughness": unreal.load_asset(base + "_Roughness")}


def import_noise_texture():
    """얼룩 무늬용 흑백 텍스처 (Metal021의 거칠기 맵: 넓게 번진 구름 같은 무늬라 때·얼룩 모양으로 좋음)"""
    name = "T_GrimeNoise"
    path = TEX_DIR + "/" + name
    # 없을 때만 가져온다. 매번 다시 가져오면 가져오기(비동기)가 아래 설정(선형·마스크)을 덮어써서 얼룩이 사라진다
    if not eal.does_asset_exist(path):
        src = os.path.join(SRC_DIR, "Metal021", "Metal021_2K-JPG_Roughness.jpg")
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
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("max_texture_size", 512)   # 흐린 얼룩이라 작아도 충분 (메모리 절약)
    eal.save_loaded_asset(tex, False)
    return tex


# ---------------------------------------------------------------- 머티리얼 도우미 (build_l02_perspective.py와 같은 방식)
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


def const3(mat, v, x, y):
    e = expr(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    e.set_editor_property("constant", unreal.LinearColor(v[0], v[1], v[2], 1.0))
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


def mask(mat, src, channel, x, y, src_out=""):
    e = expr(mat, unreal.MaterialExpressionComponentMask, x, y)
    for c in ("r", "g", "b", "a"):
        e.set_editor_property(c, c == channel)
    connect(src, src_out, e, "")
    return e


def tex_param(mat, name, tex, x, y, sampler):
    e = expr(mat, unreal.MaterialExpressionTextureObjectParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("texture", tex)
    e.set_editor_property("sampler_type", sampler)
    return e


def world_aligned(mat, tex_obj, tile, func, x, y):
    e = expr(mat, unreal.MaterialExpressionMaterialFunctionCall, x, y)
    e.set_material_function(func)
    connect(tex_obj, "", e, "TextureObject")
    connect(tile, "", e, "TextureSize")
    return e


GRIME_NOISE = None   # build_surface_material()가 정함 (얼룩 무늬 흑백 텍스처)


def noise(mat, position, scale, x, y):
    """0 ~ 1 얼룩 정도 (1 = 진한 얼룩). 계산 노이즈 대신 흑백 얼룩 텍스처를 한 번 읽는다 (VR에서 훨씬 가벼움).
    3D 위치를 (x + z, y - z)로 펼쳐 읽어서 바닥·벽 어느 면이든 무늬가 생긴다. scale = 1 / 무늬 한 장의 크기(cm)"""
    r = mask(mat, position, "r", x - 450, y)
    g = mask(mat, position, "g", x - 450, y + 60)
    b = mask(mat, position, "b", x - 450, y + 120)
    u = binop(mat, unreal.MaterialExpressionAdd, r, b, x - 330, y)
    v = binop(mat, unreal.MaterialExpressionSubtract, g, b, x - 330, y + 80)
    uv = binop(mat, unreal.MaterialExpressionAppendVector, u, v, x - 220, y + 40)
    uv = binop(mat, unreal.MaterialExpressionMultiply, uv, const(mat, scale, x - 220, y + 120), x - 120, y + 40)
    t = expr(mat, unreal.MaterialExpressionTextureSample, x, y)
    t.set_editor_property("texture", GRIME_NOISE)
    t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    connect(uv, "", t, "UVs")
    # 어두운(매끈한) 곳일수록 1에 가깝게. 값 분포: 10% 0.34 / 50% 0.48 / 90% 0.59 (원본 측정)
    return unop(mat, unreal.MaterialExpressionOneMinus, mask(mat, t, "r", x + 150, y), x + 250, y)


def threshold(mat, src, low, gain, x, y):
    """saturate((src - low) * gain)"""
    s = binop(mat, unreal.MaterialExpressionSubtract, src, const(mat, low, x - 150, y + 60), x, y)
    m = binop(mat, unreal.MaterialExpressionMultiply, s, const(mat, gain, x - 150, y + 120), x + 150, y)
    return unop(mat, unreal.MaterialExpressionSaturate, m, x + 300, y)


def grid_lines(mat, coords, width, x0, y0):
    """coords(격자 단위 좌표)에서 정수선 가까이면 1. width = 선 굵기(격자 단위). 성분별 결과(벡터)"""
    f = unop(mat, unreal.MaterialExpressionFrac, coords, x0, y0)
    inv = unop(mat, unreal.MaterialExpressionOneMinus, f, x0, y0 + 80)
    d = binop(mat, unreal.MaterialExpressionMin, f, inv, x0 + 150, y0)
    half = binop(mat, unreal.MaterialExpressionMultiply, width, const(mat, 0.5, x0, y0 + 160), x0 + 150, y0 + 160)
    diff = binop(mat, unreal.MaterialExpressionSubtract, half, d, x0 + 300, y0)
    soft = binop(mat, unreal.MaterialExpressionMultiply, half, const(mat, 0.4, x0 + 150, y0 + 240), x0 + 300, y0 + 160)
    soft = binop(mat, unreal.MaterialExpressionMax, soft, const(mat, 0.0001, x0 + 300, y0 + 240), x0 + 400, y0 + 200)
    ratio = binop(mat, unreal.MaterialExpressionDivide, diff, soft, x0 + 450, y0)
    return unop(mat, unreal.MaterialExpressionSaturate, ratio, x0 + 600, y0)


# ---------------------------------------------------------------- M_VRP_Surface
def build_surface_material(default_texs):
    global GRIME_NOISE
    GRIME_NOISE = import_noise_texture()
    path = MAT_DIR + "/M_VRP_Surface"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = at.create_asset("M_VRP_Surface", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    X = -2600
    tile = scalar(mat, "TileSize", 200.0, X, 0)

    # ---- 텍스처 (월드 정렬: 크기를 늘린 큐브에서도 텍스처가 늘어나지 않음)
    t_col = tex_param(mat, "BaseColorTex", default_texs["Color"], X, -700, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    t_rgh = tex_param(mat, "RoughnessTex", default_texs["Roughness"], X, 500, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    t_nrm = tex_param(mat, "NormalTex", default_texs["NormalDX"], X, 900, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    wa_col = world_aligned(mat, t_col, tile, WA_TEXTURE, X + 350, -700)
    wa_rgh = world_aligned(mat, t_rgh, tile, WA_TEXTURE, X + 350, 500)
    wa_nrm = world_aligned(mat, t_nrm, tile, WA_NORMAL, X + 350, 900)

    tint = vector(mat, "Color", (1.0, 1.0, 1.0), X + 350, -900)
    base = binop(mat, unreal.MaterialExpressionMultiply, wa_col, tint, X + 700, -750, a_out="XYZ Texture")

    # ---- 격자선 (LineWidth 0이면 없음). 면에 수직인 축의 선은 지운다
    grid_size = scalar(mat, "GridSize", 100.0, X, -300)
    line_w = scalar(mat, "LineWidth", 0.0, X, -200)
    line_col = vector(mat, "LineColor", (0.25, 0.25, 0.26), X + 700, -550)
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, X, -450)
    coords = binop(mat, unreal.MaterialExpressionDivide, wp, grid_size, X + 200, -400)
    width = binop(mat, unreal.MaterialExpressionDivide, line_w, grid_size, X + 200, -250)
    lines = grid_lines(mat, coords, width, X + 350, -400)
    vn = expr(mat, unreal.MaterialExpressionVertexNormalWS, X + 700, -150)
    absn = unop(mat, unreal.MaterialExpressionAbs, vn, X + 850, -150)
    keep = unop(mat, unreal.MaterialExpressionOneMinus, absn, X + 1000, -150)
    masked = binop(mat, unreal.MaterialExpressionMultiply, lines, keep, X + 1100, -350)
    m1 = binop(mat, unreal.MaterialExpressionMax, mask(mat, masked, "r", X + 1250, -450), mask(mat, masked, "g", X + 1250, -350), X + 1400, -400)
    line_mask = binop(mat, unreal.MaterialExpressionMax, m1, mask(mat, masked, "b", X + 1250, -250), X + 1550, -350)
    # 선은 페인트라 바탕 질감의 명암을 조금 남긴다
    tex_lum = mask(mat, wa_col, "g", X + 1250, -600, src_out="XYZ Texture")
    line_tex = binop(mat, unreal.MaterialExpressionMultiply, line_col,
                     binop(mat, unreal.MaterialExpressionAdd, binop(mat, unreal.MaterialExpressionMultiply, tex_lum, const(mat, 0.5, X + 1400, -650), X + 1550, -650),
                           const(mat, 0.6, X + 1550, -550), X + 1700, -620), X + 1850, -600)
    painted = expr(mat, unreal.MaterialExpressionLinearInterpolate, X + 2000, -700)
    connect(base, "", painted, "A")
    connect(line_tex, "", painted, "B")
    connect(line_mask, "", painted, "Alpha")

    # ---- 때
    wall_mask = unop(mat, unreal.MaterialExpressionOneMinus, mask(mat, absn, "b", X + 1000, 100), X + 1150, 100)   # 벽 = 1, 바닥·천장 = 0
    wz = mask(mat, wp, "b", X + 200, 150)
    # 1) 벽 아래쪽 띠: 바닥(FloorZ)에서 GrimeHeight까지 점점 옅어짐
    floor_z = scalar(mat, "FloorZ", 0.0, X + 200, 250)
    grime_h = scalar(mat, "GrimeHeight", 60.0, X + 200, 350)
    above = binop(mat, unreal.MaterialExpressionSubtract, wz, floor_z, X + 400, 200)
    band0 = unop(mat, unreal.MaterialExpressionSaturate, binop(mat, unreal.MaterialExpressionDivide, above, grime_h, X + 550, 250), X + 700, 250)
    band1 = unop(mat, unreal.MaterialExpressionOneMinus, band0, X + 850, 250)
    band = binop(mat, unreal.MaterialExpressionMultiply, band1, band1, X + 1000, 250)
    band_n = noise(mat, wp, 1.0 / 180.0, X + 850, 350)   # 띠 경계를 들쭉날쭉하게
    band = binop(mat, unreal.MaterialExpressionMultiply, band, binop(mat, unreal.MaterialExpressionAdd, band_n, const(mat, 0.7, X + 1000, 400), X + 1150, 380), X + 1300, 280)
    band = binop(mat, unreal.MaterialExpressionMultiply, band, wall_mask, X + 1450, 250)
    band = binop(mat, unreal.MaterialExpressionMultiply, band, scalar(mat, "GrimeAmount", 0.7, X + 1450, 350), X + 1600, 250)
    # 2) 큰 얼룩 (모든 면)
    stain_n = noise(mat, wp, 1.0 / 450.0, X + 400, 550)
    stain = threshold(mat, stain_n, 0.4, 3.0, X + 600, 550)
    stain = binop(mat, unreal.MaterialExpressionMultiply, stain, scalar(mat, "StainAmount", 0.5, X + 1100, 650), X + 1300, 550)
    # 3) 물 흐른 세로 자국 (벽에만): Z 방향으로 길게 늘인 노이즈
    stretched = binop(mat, unreal.MaterialExpressionMultiply, wp, const3(mat, (1.0, 1.0, 0.06), X + 200, 750), X + 400, 750)
    streak_n = noise(mat, stretched, 1.0 / 120.0, X + 550, 750)
    streak = threshold(mat, streak_n, 0.42, 4.0, X + 750, 750)
    # 벽 전체에 고르게 있으면 줄무늬처럼 보이므로, 물이 새던 몇 군데(큰 노이즈 덩어리)에만 남긴다
    patch_n = noise(mat, wp, 1.0 / 900.0, X + 550, 950)
    patch = threshold(mat, patch_n, 0.32, 3.0, X + 750, 950)
    streak = binop(mat, unreal.MaterialExpressionMultiply, streak, patch, X + 1100, 800)
    # 천장에서 새어 내려온 자국이라 위쪽이 진하고 아래로 갈수록 옅어짐 (바닥에서 2.5m 위가 가장 진함)
    high = unop(mat, unreal.MaterialExpressionSaturate, binop(mat, unreal.MaterialExpressionDivide, above, const(mat, 250.0, X + 900, 1100), X + 1050, 1050), X + 1200, 1050)
    high = binop(mat, unreal.MaterialExpressionAdd, binop(mat, unreal.MaterialExpressionMultiply, high, const(mat, 0.6, X + 1200, 1150), X + 1350, 1050),
                 const(mat, 0.4, X + 1350, 1150), X + 1500, 1050)
    streak = binop(mat, unreal.MaterialExpressionMultiply, streak, high, X + 1150, 900)
    streak = binop(mat, unreal.MaterialExpressionMultiply, streak, wall_mask, X + 1250, 750)
    streak = binop(mat, unreal.MaterialExpressionMultiply, streak, scalar(mat, "StreakAmount", 0.6, X + 1250, 850), X + 1400, 750)

    dirt = binop(mat, unreal.MaterialExpressionAdd, band, stain, X + 1750, 400)
    dirt = unop(mat, unreal.MaterialExpressionSaturate, binop(mat, unreal.MaterialExpressionAdd, dirt, streak, X + 1900, 500), X + 2050, 500)

    grime_col = vector(mat, "GrimeColor", (0.30, 0.25, 0.18), X + 2000, -450)
    dirty = binop(mat, unreal.MaterialExpressionMultiply, painted, grime_col, X + 2200, -500)
    final_col = expr(mat, unreal.MaterialExpressionLinearInterpolate, X + 2400, -650)
    connect(painted, "", final_col, "A")
    connect(dirty, "", final_col, "B")
    connect(dirt, "", final_col, "Alpha")
    mel.connect_material_property(final_col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # 거칠기: 텍스처 x RoughnessScale, 때 낀 곳은 더 거칠게
    rough = binop(mat, unreal.MaterialExpressionMultiply, mask(mat, wa_rgh, "r", X + 700, 500, src_out="XYZ Texture"),
                  scalar(mat, "RoughnessScale", 1.0, X + 700, 600), X + 900, 500)
    rough = binop(mat, unreal.MaterialExpressionAdd, rough, binop(mat, unreal.MaterialExpressionMultiply, dirt, const(mat, 0.2, X + 2050, 650), X + 2200, 600), X + 2350, 550)
    mel.connect_material_property(unop(mat, unreal.MaterialExpressionSaturate, rough, X + 2500, 550), "", unreal.MaterialProperty.MP_ROUGHNESS)

    # 금속성 (녹슨 금속 등)
    mel.connect_material_property(scalar(mat, "Metallic", 0.0, X + 2400, 700), "", unreal.MaterialProperty.MP_METALLIC)

    # 노멀 (NormalStrength로 세기 조절)
    flat = const3(mat, (0.0, 0.0, 1.0), X + 700, 1000)
    nrm = expr(mat, unreal.MaterialExpressionLinearInterpolate, X + 900, 900)
    connect(flat, "", nrm, "A")
    connect(wa_nrm, "XYZ Texture", nrm, "B")
    connect(scalar(mat, "NormalStrength", 1.0, X + 700, 1100), "", nrm, "Alpha")
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("M_VRP_Surface 생성")
    return mat


def setup_mi(name, parent, texs, color, tile, rough=1.0, normal=1.0, line=None, grime=0.7, grime_h=60.0, stain=0.5,
             streak=0.6, floor_z=0.0, metallic=0.0, grime_color=(0.30, 0.25, 0.18)):
    """line = (LineWidth cm, LineColor) 또는 None"""
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
plaster_old = import_set("Plaster007")
concrete_old = import_set("Concrete036")
tiles_dirty = import_set("Tiles141")
tiles_ind = import_set("Tiles140")
metal_rust = import_set("Metal021")
plaster_clean = existing_set("PaintedPlaster017")
concrete_plain = existing_set("Concrete046")

surface = build_surface_material(plaster_old)

# ---- 1스테이지 격리실 (벽은 이미 질감이 있음 → 같은 회벽에 때만 더함)
setup_mi("MI_VRP_Wall", surface, plaster_clean, (0.62, 0.68, 0.65), 180.0, normal=1.8, grime=0.8, stain=0.6, streak=0.7)
setup_mi("MI_VRP_Wall_Concrete", surface, concrete_plain, (0.85, 0.85, 0.85), 250.0, normal=1.2)
setup_mi("MI_VRP_Floor", surface, tiles_dirty, (0.75, 0.75, 0.72), 120.0, stain=0.6)
setup_mi("MI_VRP_Ceiling", surface, concrete_plain, (0.6, 0.6, 0.6), 250.0, stain=0.7, grime=0.0)
setup_mi("MI_VRP_Trim", surface, metal_rust, (0.55, 0.57, 0.6), 120.0, metallic=0.6, stain=0.3, grime=0.0, streak=0.2)

# ---- 2스테이지 원근 실험실: 실제 질감 + 1m 격자 페인트 선 (거리감 때문에 격자는 꼭 남긴다)
setup_mi("MI_VRP_GridWall", surface, plaster_old, (0.6, 0.61, 0.6), 220.0, normal=1.5,
         line=(2.5, (0.16, 0.17, 0.19)), grime=0.85, grime_h=80.0, stain=0.6, streak=0.75)
setup_mi("MI_VRP_GridFloor", surface, concrete_old, (0.6, 0.6, 0.6), 250.0,
         line=(3.0, (0.85, 0.82, 0.62)), stain=0.6)
setup_mi("MI_VRP_GridCeiling", surface, concrete_old, (0.6, 0.6, 0.6), 250.0, line=(1.5, (0.25, 0.25, 0.26)), grime=0.0, stain=0.7)
setup_mi("MI_VRP_GridShelf", surface, metal_rust, (0.55, 0.6, 0.66), 150.0, metallic=0.5,
         line=(3.0, (0.85, 0.85, 0.8)), grime=0.0, stain=0.3, streak=0.4)
setup_mi("MI_VRP_GridPit", surface, concrete_old, (0.55, 0.55, 0.56), 250.0, line=(3.0, (0.75, 0.72, 0.55)),
         floor_z=-200.0, stain=0.8, streak=0.9, grime=0.9)

# ---- 3스테이지 관측실: 어둡고 오래된 콘크리트 + 부식된 타일 바닥
setup_mi("MI_VRP_ObsWall", surface, concrete_old, (0.45, 0.5, 0.58), 250.0, normal=1.4, grime=0.9, grime_h=90.0,
         stain=0.6, streak=0.9)
setup_mi("MI_VRP_ObsFloor", surface, tiles_ind, (0.6, 0.62, 0.66), 150.0, stain=0.6)
setup_mi("MI_VRP_ObsCeiling", surface, concrete_old, (0.35, 0.37, 0.42), 250.0, grime=0.0, stain=0.7)

# ---- 4스테이지 장치 시험실: 낡은 회벽 + 더러운 타일
setup_mi("MI_VRP_DevWall", surface, plaster_old, (0.66, 0.68, 0.64), 220.0, normal=1.5, grime=0.85, grime_h=70.0,
         stain=0.6, streak=0.75)
setup_mi("MI_VRP_DevFloor", surface, tiles_dirty, (0.72, 0.72, 0.7), 120.0, stain=0.65)
setup_mi("MI_VRP_DevCeiling", surface, concrete_old, (0.55, 0.55, 0.55), 250.0, grime=0.0, stain=0.7)
setup_mi("MI_VRP_DevMetal", surface, metal_rust, (0.6, 0.62, 0.65), 120.0, metallic=0.6, grime=0.0, stain=0.3, streak=0.2)

log("완료")

# -*- coding: utf-8 -*-
"""
엔딩(L_06_Ending)과 메인 화면(L_MainMenu) 주변에 산맥 배경을 두른다.

- 마당(반경 90m 풀밭) 바깥을 둥글게 감싸는 지형 메시 하나 (SM_VRP_Mountains)
    90~260m   낮은 언덕 (풀밭과 이어짐) + 침엽수 숲
    200m~2.5km 점점 높아지는 산맥, 먼 곳은 최고 약 900m 봉우리 (꼭대기 눈)
    -X 쪽(진입로 방향)은 골짜기로 열어 둔다
- 지형 머티리얼 M_VRP_Terrain: 높이·경사에 따라 풀 → 마른 풀 → 바위 → 눈 (월드 좌표 기준이라 UV 필요 없음)
- 메시는 파이썬으로 OBJ를 만들어 가져온다 (Saved/Generated/SM_VRP_Mountains.obj). 모양을 바꾸려면 아래 숫자를 고치고 다시 실행.

실행 (에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="<프로젝트 절대 경로>/Scripts/dress_mountains.py"
여러 번 실행해도 결과는 같다 (이전 산 액터를 지우고 다시 놓음).
build_l06_ending.py를 다시 돌렸으면 이 스크립트도 다시 돌린다. build_main_menu.py는 엔딩 맵을 복사하므로 그다음에 돌려도 산이 따라간다.
"""
import math
import os
import random

import unreal

MAPS = ["/Game/VRPuzzle/Maps/L_06_Ending", "/Game/VRPuzzle/Maps/L_MainMenu"]
ORIGIN = (60000.0, 0.0, 0.0)          # 두 맵의 마당 원점 (cm)
MESH_DIR = "/Game/VRPuzzle/Props/Background"
MESH_NAME = "SM_VRP_Mountains"
MAT_DIR = "/Game/VRPuzzle/Materials"
TERRAIN_MAT = MAT_DIR + "/M_VRP_Terrain"
TREE_MI = "MI_VRP_PineTree"
ACTOR_LABEL = "BG_Mountains"
PLACE_IN_MAPS = globals().get("PLACE_IN_MAPS", True)   # False면 에셋만 만든다 (열린 에디터에서 시험할 때)

# ---------------------------------------------------------------- 모양 (단위: m)
INNER_R = 88.0          # 이 안은 마당 풀밭 아래로 숨김
FIELD_Z = -0.3
RINGS = [88, 95, 105, 118, 132, 150, 170, 195, 225, 260, 300, 350, 410, 480, 560, 650, 750, 860, 980,
         1110, 1260, 1430, 1620, 1830, 2060, 2300, 2550]
SEGMENTS = 512
VALLEY_YAW = math.pi    # 골짜기 방향 (-X, 진입로 쪽)
TREE_COUNT = 900

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("[MOUNTAINS] " + msg)


# ---------------------------------------------------------------- 노이즈
random.seed(1984)
_PERM = list(range(256))
random.shuffle(_PERM)
_PERM += _PERM


def _hash(ix, iy):
    return _PERM[(_PERM[ix & 255] + iy) & 255] / 255.0


def _fade(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(x, y):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    ix, iy = int(ix), int(iy)
    a, b = _hash(ix, iy), _hash(ix + 1, iy)
    c, d = _hash(ix, iy + 1), _hash(ix + 1, iy + 1)
    u, v = _fade(fx), _fade(fy)
    return (a + (b - a) * u) * (1 - v) + (c + (d - c) * u) * v


def fbm(x, y, octaves=4):
    s, amp, norm = 0.0, 1.0, 0.0
    for _ in range(octaves):
        s += value_noise(x, y) * amp
        norm += amp
        x, y, amp = x * 2.03 + 17.1, y * 2.03 - 9.7, amp * 0.5
    return s / norm


def ridged(x, y, octaves=5):
    s, amp, norm = 0.0, 1.0, 0.0
    for _ in range(octaves):
        n = 1.0 - abs(value_noise(x, y) * 2.0 - 1.0)
        s += n * n * amp
        norm += amp
        x, y, amp = x * 2.1 - 4.3, y * 2.1 + 7.9, amp * 0.5
    return s / norm


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


def height(x, y):
    """(x, y)는 마당 원점 기준 m. 지면 높이 m"""
    r = max(math.hypot(x, y), 1.0)
    th = math.atan2(y, x)
    dth = abs((th - VALLEY_YAW + math.pi) % (2.0 * math.pi) - math.pi)
    valley = 1.0 - 0.72 * math.exp(-(dth / 0.24) ** 2)
    hills = smoothstep(INNER_R, 260.0, r) * (5.0 + 16.0 * fbm(x / 140.0, y / 140.0, 3))
    env = smoothstep(200.0, 1000.0, r) * 380.0 + smoothstep(900.0, 2200.0, r) * 330.0
    mount = env * (0.28 + 0.95 * ridged(x / 650.0, y / 650.0) ** 1.5) * valley
    inner = 1.0 - smoothstep(INNER_R, INNER_R + 6.0, r)
    return (hills + mount) * (1.0 - inner) + FIELD_Z * inner


def normal_at(x, y, eps=2.0):
    dx = (height(x + eps, y) - height(x - eps, y)) / (2.0 * eps)
    dy = (height(x, y + eps) - height(x, y - eps)) / (2.0 * eps)
    l = math.sqrt(dx * dx + dy * dy + 1.0)
    return (-dx / l, -dy / l, 1.0 / l)


# ---------------------------------------------------------------- 메시 만들기 (UE 좌표, m)
class MeshBuilder:
    def __init__(self):
        self.pos, self.nrm, self.uv = [], [], []
        self.groups = {}

    def vert(self, p, n, uv):
        self.pos.append(p)
        self.nrm.append(n)
        self.uv.append(uv)
        return len(self.pos)          # OBJ는 1부터

    def tri(self, group, a, b, c, outward):
        """outward(UE 좌표) 쪽이 앞면이 되게 감는 방향을 맞춘다.
        UE 5.3 Interchange는 OBJ (x, y, z)를 UE (x, -y, z)로 가져오므로(Z가 위, Y만 뒤집음),
        OBJ에는 Y를 뒤집어 쓰고 감는 방향도 그 공간(오른손, 반시계 = 앞면)에서 계산한다"""
        pa, pb, pc = self.pos[a - 1], self.pos[b - 1], self.pos[c - 1]
        A = (pa[0], -pa[1], pa[2])
        B = (pb[0], -pb[1], pb[2])
        C = (pc[0], -pc[1], pc[2])
        u = (B[0] - A[0], B[1] - A[1], B[2] - A[2])
        v = (C[0] - A[0], C[1] - A[1], C[2] - A[2])
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        o = (outward[0], -outward[1], outward[2])
        if n[0] * o[0] + n[1] * o[1] + n[2] * o[2] < 0:
            b, c = c, b
        self.groups.setdefault(group, []).append((a, b, c))

    def write_obj(self, path):
        """메시 하나(OBJ 그룹 이름 = 에셋 이름), 재질 칸은 그룹(Terrain, Trees)마다 하나"""
        with open(path, "w", encoding="utf-8") as f:
            f.write("# pcs_vr dress_mountains.py\n")
            for p in self.pos:   # m → cm, UE (X, Y, Z) → OBJ (X, -Y, Z)
                f.write("v %.1f %.1f %.1f\n" % (p[0] * 100.0, -p[1] * 100.0, p[2] * 100.0))
            for n in self.nrm:
                f.write("vn %.4f %.4f %.4f\n" % (n[0], -n[1], n[2]))
            for t in self.uv:
                f.write("vt %.4f %.4f\n" % t)
            f.write("g %s\n" % MESH_NAME)
            f.write("s 1\n")     # 부드러운 음영 (없으면 Interchange가 면마다 각진 음영을 만들어 체크무늬처럼 보임)
            for group, tris in self.groups.items():
                f.write("usemtl %s\n" % group)
                for a, b, c in tris:
                    f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, b, b, b, c, c, c))


def build_terrain(mb):
    ids = []
    for r in RINGS:
        row = []
        for s in range(SEGMENTS):
            th = 2.0 * math.pi * s / SEGMENTS
            x, y = r * math.cos(th), r * math.sin(th)
            row.append(mb.vert((x, y, height(x, y)), normal_at(x, y), (x / 50.0, y / 50.0)))
        ids.append(row)
    for i in range(len(RINGS) - 1):
        for s in range(SEGMENTS):
            s2 = (s + 1) % SEGMENTS
            a, b, c, d = ids[i][s], ids[i][s2], ids[i + 1][s], ids[i + 1][s2]
            mb.tri("Terrain", a, b, d, (0, 0, 1))
            mb.tri("Terrain", a, d, c, (0, 0, 1))


def add_cone(mb, cx, cy, z0, z1, radius, sides=7):
    """z0 높이 밑둥 원(반지름 radius) → z1 꼭짓점. 옆면만"""
    slope = radius / max(z1 - z0, 0.1)
    apex = mb.vert((cx, cy, z1), (0.0, 0.0, 1.0), (0.5, 1.0))
    ring = []
    for k in range(sides):
        a = 2.0 * math.pi * k / sides
        ca, sa = math.cos(a), math.sin(a)
        l = math.sqrt(1.0 + slope * slope)
        ring.append(mb.vert((cx + radius * ca, cy + radius * sa, z0), (ca / l, sa / l, slope / l), (k / sides, 0.0)))
    for k in range(sides):
        a, b = ring[k], ring[(k + 1) % sides]
        mid = 2.0 * math.pi * (k + 0.5) / sides
        mb.tri("Trees", a, b, apex, (math.cos(mid), math.sin(mid), 0.3))


def build_trees(mb):
    rng = random.Random(77)
    placed, tries = 0, 0
    while placed < TREE_COUNT and tries < TREE_COUNT * 30:
        tries += 1
        r = math.sqrt(rng.uniform(100.0 ** 2, 520.0 ** 2))
        th = rng.uniform(0.0, 2.0 * math.pi)
        x, y = r * math.cos(th), r * math.sin(th)
        # 숲은 무리 지어 자란다
        if fbm(x / 90.0 + 31.0, y / 90.0 - 12.0, 3) < 0.52:
            continue
        # 진입로 골짜기 바닥은 비운다
        dth = abs((th - VALLEY_YAW + math.pi) % (2.0 * math.pi) - math.pi)
        if dth < 0.12:
            continue
        n = normal_at(x, y, 3.0)
        if n[2] < 0.8:      # 가파른 비탈에는 안 자람
            continue
        z = height(x, y) - 0.6
        h = rng.uniform(9.0, 19.0)
        w = h * rng.uniform(0.22, 0.3)
        add_cone(mb, x, y, z + 1.2, z + h * 0.72, w)
        add_cone(mb, x, y, z + h * 0.42, z + h, w * 0.66)
        placed += 1
    return placed


# ---------------------------------------------------------------- 머티리얼
TERRAIN_HLSL = """
float z = WP.z / 100.0;
float n = Noise;
float slope = N.z;
float3 grass = float3(0.10, 0.16, 0.055) * (0.75 + 0.5 * n);
float3 dry   = float3(0.24, 0.22, 0.12);
float3 rock  = float3(0.25, 0.24, 0.22) * (0.75 + 0.5 * n);
float3 snow  = float3(0.86, 0.88, 0.92);
float3 c = lerp(grass, dry, saturate(n * 1.4 - 0.5) * 0.5);
float hT = saturate((z - 170.0) / 260.0);
float rockMask = max(saturate((0.66 - slope) * 5.0), hT * saturate((0.95 - slope) * 3.0 + 0.2));
c = lerp(c, rock, rockMask);
float snowMask = saturate((z - 420.0 - n * 80.0) / 60.0) * saturate((slope - 0.4) * 4.0);
return lerp(c, snow, snowMask);
"""


def make_terrain_material():
    if eal.does_asset_exist(TERRAIN_MAT):
        mat = unreal.load_asset(TERRAIN_MAT)
        mel.delete_all_material_expressions(mat)
    else:
        mat = at.create_asset("M_VRP_Terrain", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 0)
    xy = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -700, 100)
    xy.set_editor_property("r", True)
    xy.set_editor_property("g", True)
    xy.set_editor_property("b", False)
    xy.set_editor_property("a", False)
    mel.connect_material_expressions(wp, "", xy, "")

    noise_tex = unreal.load_asset("/Game/VRPuzzle/Textures/Surfaces/T_GrimeNoise")
    samples = []
    for i, scale in enumerate((160000.0, 9000.0)):   # 큰 얼룩(1.6km) + 잔무늬(90m)
        div = mel.create_material_expression(mat, unreal.MaterialExpressionDivide, -500, 60 + i * 160)
        div.set_editor_property("const_b", scale)
        mel.connect_material_expressions(xy, "", div, "A")
        ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -300, 60 + i * 160)
        ts.set_editor_property("texture", noise_tex)
        ts.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)   # T_GrimeNoise는 마스크 텍스처
        mel.connect_material_expressions(div, "", ts, "UVs")
        samples.append(ts)
    lerp_n = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -100, 120)
    lerp_n.set_editor_property("const_alpha", 0.4)
    mel.connect_material_expressions(samples[0], "R", lerp_n, "A")
    mel.connect_material_expressions(samples[1], "R", lerp_n, "B")

    nrm = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -300, -120)

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, 100, 0)
    custom.set_editor_property("code", TERRAIN_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "Terrain colour")
    inputs = []
    for name in ("WP", "N", "Noise"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    mel.connect_material_expressions(wp, "", custom, "WP")
    mel.connect_material_expressions(nrm, "", custom, "N")
    mel.connect_material_expressions(lerp_n, "", custom, "Noise")
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 100, 200)
    rough.set_editor_property("r", 0.95)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 100, 280)
    spec.set_editor_property("r", 0.25)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat, False)
    return mat


def make_tree_material():
    path = MAT_DIR + "/" + TREE_MI
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else \
        at.create_asset(TREE_MI, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(MAT_DIR + "/M_VRP_Solid"))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(0.035, 0.065, 0.035, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Emissive", 0.0)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", 0.95)
    eal.save_loaded_asset(mi, False)
    return mi


# ---------------------------------------------------------------- 메시 가져오기
def import_mesh(obj_path):
    """UE 5.3은 OBJ를 Interchange로 가져온다: 에셋 이름 = OBJ 그룹 이름(MESH_NAME), 재질 칸 = usemtl 이름.
    가져온 결과의 크기로 축이 맞는지 확인한다 (높이 = Z)"""
    # 예전 실행이 남긴, 이 폴더의 다른 메시는 지운다 (재질은 Materials 폴더에 따로 있음)
    for p in eal.list_assets(MESH_DIR, recursive=False):
        if p.split("/")[-1].split(".")[0] != MESH_NAME and isinstance(unreal.load_asset(p), unreal.StaticMesh):
            eal.delete_asset(p)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", obj_path)
    task.set_editor_property("destination_path", MESH_DIR)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    at.import_asset_tasks([task])
    meshes = [unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]
    meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
    if not meshes:
        raise RuntimeError("산 메시 가져오기 실패: " + obj_path)
    mesh = meshes[0]
    ext = mesh.get_bounds().box_extent
    if ext.z > ext.x * 0.5:
        raise RuntimeError("산 메시 축이 틀림 (크기 %s): OBJ 좌표 변환을 확인" % ext)
    return mesh


def place(mesh, terrain_mat, tree_mi):
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for map_path in MAPS:
        if not eal.does_asset_exist(map_path):
            log("맵 없음, 건너뜀: " + map_path)
            continue
        unreal.EditorLoadingAndSavingUtils.load_map(map_path)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        for a in eas.get_all_level_actors():
            if a.get_actor_label().startswith(ACTOR_LABEL):
                eas.destroy_actor(a)
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*ORIGIN), unreal.Rotator(0, 0, 0))
        a.set_actor_label(ACTOR_LABEL)
        a.set_folder_path("Background")
        smc = a.static_mesh_component
        smc.set_static_mesh(mesh)
        for i, slot in enumerate(smc.get_material_slot_names()):
            smc.set_material(i, tree_mi if "Tree" in str(slot) else terrain_mat)
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        smc.set_editor_property("cast_shadow", False)       # 멀리 있는 큰 메시라 그림자는 끔 (VR 프레임 절약)
        unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
        log("%s: 산 배치" % map_path)


# ---------------------------------------------------------------- 실행
mb = MeshBuilder()
build_terrain(mb)
trees = build_trees(mb)
out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Generated")
os.makedirs(out_dir, exist_ok=True)
obj_path = os.path.join(out_dir, MESH_NAME + ".obj")   # Interchange는 파일 이름을 에셋 이름으로 쓴다
mb.write_obj(obj_path)
log("OBJ: 정점 %d개, 나무 %d그루 → %s" % (len(mb.pos), trees, obj_path))

terrain_mat = make_terrain_material()
tree_mi = make_tree_material()
mesh = import_mesh(obj_path)
b = mesh.get_bounds()
log("메시 크기(cm): 중심 %s, 반경 %s, 재질 칸 %s" % (b.origin, b.box_extent, [str(s.material_slot_name) for s in mesh.static_materials]))

if PLACE_IN_MAPS:
    place(mesh, terrain_mat, tree_mi)

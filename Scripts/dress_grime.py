# -*- coding: utf-8 -*-
"""
연구소 안 스테이지 맵(L_01 ~ L_04)에 20년 동안 버려진 흔적을 깐다: 벽을 따라 쌓인 먼지 더미, 떨어진 콘크리트 조각, 누렇게 바랜 종이.
- 바닥 액터(이름에 Floor가 들어간 StaticMeshActor)를 찾아 그 가장자리(벽 쪽 45cm 띠)에만 놓는다 → 퍼즐·길을 막지 않음
- 액터 수백 개 대신 AInstancedClutter 하나에 (메시, 머티리얼) 묶음별 인스턴스로 그린다 (드로우콜 절약)
- 모두 충돌·내비게이션·그림자 없음 (원근 큐브 놓기, 텔레포트, 레이 잡기에 영향 없음)
- 라벨이 Grime_ 으로 시작하는 액터는 이 스크립트가 만든 것: 다시 실행하면 지우고 새로 깐다 (맵을 다시 만든 뒤에도 다시 돌리면 됨)
- 벽·바닥 자체의 때와 얼룩은 setup_grime_materials.py (머티리얼)

실행 (에디터를 닫은 상태에서, setup_grime_materials.py 후):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/dress_grime.py"
"""
import math
import random

import unreal

MAPS = ["L_01_Isolation", "L_02_Perspective", "L_03_Observation", "L_04_Device"]
MAT_DIR = "/Game/VRPuzzle/Materials"
EDGE = 45.0          # 벽에서 이 거리(cm) 안쪽 띠에만 놓는다
PER_METER = 1.4      # 가장자리 1m마다 평균 잡동사니 수

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")


def log(msg):
    unreal.log_warning("[Grime] " + msg)


def ensure_solid_mi(name, color, rough):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = at.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(MAT_DIR + "/M_VRP_Solid"))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Emissive", 0.0)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    eal.save_loaded_asset(mi, False)
    return mi


def ensure_rubble_mi():
    """콘크리트 조각: M_VRP_Surface + 오래된 콘크리트 질감을 잘게"""
    path = MAT_DIR + "/MI_VRP_Rubble"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = at.create_asset("MI_VRP_Rubble", MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(MAT_DIR + "/M_VRP_Surface"))
    tex = "/Game/VRPuzzle/Textures/Surfaces/T_Concrete036_"
    mel.set_material_instance_texture_parameter_value(mi, "BaseColorTex", unreal.load_asset(tex + "Color"))
    mel.set_material_instance_texture_parameter_value(mi, "NormalTex", unreal.load_asset(tex + "Normal"))
    mel.set_material_instance_texture_parameter_value(mi, "RoughnessTex", unreal.load_asset(tex + "Roughness"))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(0.7, 0.68, 0.64, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "TileSize", 40.0)
    mel.set_material_instance_scalar_parameter_value(mi, "GrimeAmount", 0.0)
    mel.set_material_instance_scalar_parameter_value(mi, "StreakAmount", 0.0)
    mel.set_material_instance_scalar_parameter_value(mi, "StainAmount", 0.6)
    eal.save_loaded_asset(mi, False)
    return mi


MI_RUBBLE = ensure_rubble_mi()
MI_DUST = ensure_solid_mi("MI_VRP_Dust", (0.12, 0.1, 0.08), 1.0)
MI_PAPER_OLD = ensure_solid_mi("MI_VRP_PaperOld", (0.62, 0.56, 0.42), 0.95)
MI_PAPER_DARK = ensure_solid_mi("MI_VRP_PaperStained", (0.42, 0.37, 0.28), 0.95)


GROUPS = {}   # (메시, 머티리얼) -> 트랜스폼 목록. dress()가 맵마다 비우고 마지막에 인스턴스 액터 하나로 만든다


def piece(label, mesh, loc, size, mi, yaw=0.0, pitch=0.0, roll=0.0):
    """잡동사니 하나를 묶음에 추가 (액터를 따로 만들지 않음 → AInstancedClutter 하나로 그림)"""
    xf = unreal.Transform(loc, unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                          unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    GROUPS.setdefault((mesh, mi), []).append(xf)


def edge_point(rect, rng):
    """rect = (x1, y1, x2, y2). 가장자리 띠 안의 임의 점 (긴 변일수록 자주 고름)"""
    x1, y1, x2, y2 = rect
    w, h = x2 - x1, y2 - y1
    side = rng.uniform(0, 2 * (w + h))
    inset = rng.uniform(4.0, EDGE) * rng.uniform(0.3, 1.0)   # 벽에 붙을수록 많게
    if side < w:
        return x1 + side, y1 + inset
    side -= w
    if side < w:
        return x1 + side, y2 - inset
    side -= w
    if side < h:
        return x1 + inset, y1 + side
    return x2 - inset, y1 + side - h


def corners(rect):
    x1, y1, x2, y2 = rect
    return [(x1, y1, 1, 1), (x2, y1, -1, 1), (x1, y2, 1, -1), (x2, y2, -1, -1)]


def dress(map_name):
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/VRPuzzle/Maps/" + map_name)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = eas.get_all_level_actors()
    removed = 0
    for a in actors:
        if a.get_actor_label().startswith("Grime_"):
            eas.destroy_actor(a)
            removed += 1

    floors = []
    for a in eas.get_all_level_actors():
        label = a.get_actor_label()
        if isinstance(a, unreal.StaticMeshActor) and "Floor" in label and not label.startswith("Booth"):
            o, e = a.get_actor_bounds(False)
            if e.x > 60 and e.y > 60:
                floors.append((label, (o.x - e.x, o.y - e.y, o.x + e.x, o.y + e.y), o.z + e.z))

    GROUPS.clear()
    rng = random.Random(sum(ord(c) for c in map_name))   # 맵마다 같은 배치
    count = 0
    for label, rect, top in floors:
        w, h = rect[2] - rect[0], rect[3] - rect[1]
        n = int(2 * (w + h) / 100.0 * PER_METER)
        for i in range(n):
            x, y = edge_point(rect, rng)
            kind = rng.random()
            tag = "Grime_%s_%03d" % (label, i)
            if kind < 0.45:
                # 떨어진 콘크리트·회벽 조각 (2 ~ 12cm)
                s = rng.uniform(2.0, 12.0)
                piece(tag, CUBE, unreal.Vector(x, y, top + s * 0.3), (s * rng.uniform(0.7, 1.4), s * rng.uniform(0.6, 1.2), s * rng.uniform(0.4, 0.8)),
                      MI_RUBBLE, yaw=rng.uniform(0, 360), pitch=rng.uniform(-25, 25), roll=rng.uniform(-25, 25))
            elif kind < 0.8:
                # 누렇게 바랜 종이 (A4 크기, 바닥에 납작)
                mi = MI_PAPER_OLD if rng.random() < 0.6 else MI_PAPER_DARK
                piece(tag, CUBE, unreal.Vector(x, y, top + 0.15 + rng.uniform(0, 0.3)), (29.7, 21.0, 0.15), mi,
                      yaw=rng.uniform(0, 360), pitch=rng.uniform(-3, 3), roll=rng.uniform(-3, 3))
            else:
                # 낮은 먼지 더미
                r = rng.uniform(15.0, 40.0)
                piece(tag, SPHERE, unreal.Vector(x, y, top), (r, r * rng.uniform(0.6, 1.0), rng.uniform(1.5, 4.0)), MI_DUST,
                      yaw=rng.uniform(0, 360))
            count += 1
        # 구석마다 큰 먼지 더미 + 조각 몇 개
        for j, (cx, cy, sx, sy) in enumerate(corners(rect)):
            r = rng.uniform(40.0, 70.0)
            piece("Grime_%s_Corner%d" % (label, j), SPHERE, unreal.Vector(cx + sx * r * 0.25, cy + sy * r * 0.25, top),
                  (r, r * 0.8, rng.uniform(3.0, 6.0)), MI_DUST, yaw=rng.uniform(0, 360))
            count += 1

    clutter = eas.spawn_actor_from_class(unreal.InstancedClutter, unreal.Vector(0, 0, 0))
    clutter.set_actor_label("Grime_Clutter")
    clutter.set_folder_path("Grime")
    groups = []
    for (mesh, mi), xfs in GROUPS.items():
        g = unreal.ClutterGroup()
        g.set_editor_property("mesh", mesh)
        g.set_editor_property("material", mi)
        g.set_editor_property("transforms", xfs)
        groups.append(g)
    clutter.set_editor_property("groups", groups)
    # 컴포넌트는 OnConstruction(에디터)과 BeginPlay(게임)에서 만들어진다
    unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/VRPuzzle/Maps/" + map_name)
    log("%s: 이전 %d개 지움, %d개 깔기 (바닥 %d개, 인스턴스 묶음 %d개)" % (map_name, removed, count, len(floors), len(GROUPS)))


for m in MAPS:
    dress(m)
log("완료")

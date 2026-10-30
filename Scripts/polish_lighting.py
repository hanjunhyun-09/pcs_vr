# -*- coding: utf-8 -*-
"""
인트로(L_00_Intro)와 메인 화면(L_MainMenu)의 조명·분위기를 다듬는다.

인트로: 검은 공간에 상자만 보이던 것을, 낡은 백열등 한 줄기 빛이 먼지 낀 공기 속으로 떨어지는 느낌으로.
  - 높이 안개에 볼류메트릭 안개를 켜서 스포트라이트 빛줄기가 보이게 (안개 자체 색은 검게 → 빛이 닿은 곳만 뿌옇게)
  - 스포트라이트를 따뜻한 전구 색으로, 상자 뒤에 차가운 약한 역광을 더해 상자 윤곽과 바닥이 어둠에서 살짝 떠오르게
  - 후처리: 비네트, 필름 그레인, 그림자 쪽을 조금 차갑게
메인 화면: 해 질 녘 연구소 마당이 주황 한 색으로 덮이고 앞쪽이 새까맣던 것을
  - 하늘빛을 차갑고 조금 밝게 (그림자는 푸르게, 해가 닿는 곳은 주황 → 색이 나뉘어 깊이감)
  - 볼류메트릭 안개 + 해의 빛줄기, 노출을 조금 올림
  - 조작대 위 비상등: 버튼 글자가 읽히게 차가운 빛 한 줄기

VR 성능: 볼류메트릭 안개는 GPU를 쓰지만 두 맵 모두 작고 그림자 조명이 1~2개라 부담이 크지 않다.
맵 빌드 스크립트(build_main_menu.py 등)를 다시 돌린 뒤에는 이 스크립트도 다시 돌린다. 여러 번 실행해도 결과는 같다.
실행 (두 맵을 연 에디터가 없는 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="Scripts/polish_lighting.py"
"""
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MENU_ORIGIN = unreal.Vector(60000.0, 0.0, 0.0)   # build_main_menu.py 의 ORIGIN
MENU_PLAYER_X = -1300.0                          # build_main_menu.py 의 PX


def log(msg):
    unreal.log_warning("[Polish] " + msg)


def find(label):
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


def find_class(cls):
    return [a for a in eas.get_all_level_actors() if isinstance(a, cls)]


def ensure_light(cls, label, loc, rot=unreal.Rotator(0, 0, 0)):
    """label 이 있으면 옮기고, 없으면 만든다 (여러 번 실행해도 하나만)"""
    a = find(label)
    if a is None:
        a = eas.spawn_actor_from_class(cls, loc, rot)
        a.set_actor_label(label)
        a.set_folder_path("Lighting")
    a.set_actor_location_and_rotation(loc, rot, False, False)
    return a


def rgb(r, g, b):
    return unreal.Color(r=r, g=g, b=b, a=255)


def set_pp(settings, **values):
    for key, value in values.items():
        settings.set_editor_property("override_" + key, True)
        settings.set_editor_property(key, value)


def v4(x, y, z, w=1.0):
    return unreal.Vector4(x, y, z, w)


def save():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.EditorLoadingAndSavingUtils.save_map(world, world.get_path_name().split(".")[0])


# ================================================================ 인트로
unreal.EditorLoadingAndSavingUtils.load_map("/Game/VRPuzzle/Maps/L_00_Intro")

spot = find("Intro_Spotlight").get_component_by_class(unreal.SpotLightComponent)
spot.set_editor_property("light_color", rgb(255, 232, 205))        # 오래된 백열전구 (너무 주황이면 상자가 장난감처럼 보임)
spot.set_editor_property("volumetric_scattering_intensity", 6.0)

fog = find_class(unreal.ExponentialHeightFog)[0].get_component_by_class(unreal.ExponentialHeightFogComponent)
fog.set_editor_property("fog_density", 0.05)
fog.set_editor_property("fog_height_falloff", 0.2)
fog.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))   # 빛이 없는 곳은 검게
fog.set_editor_property("enable_volumetric_fog", True)
fog.set_editor_property("volumetric_fog_scattering_distribution", 0.6)   # 옆에서 봐도 빛줄기가 보이게
fog.set_editor_property("volumetric_fog_albedo", rgb(235, 225, 210))     # 먼지 낀 공기

rim = ensure_light(unreal.PointLight, "Intro_RimLight", unreal.Vector(110.0, 40.0, 45.0))
rim_c = rim.get_component_by_class(unreal.PointLightComponent)
rim_c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
rim_c.set_editor_property("intensity", 6.0)
rim_c.set_editor_property("light_color", rgb(150, 180, 255))
rim_c.set_editor_property("attenuation_radius", 260.0)
rim_c.set_editor_property("cast_shadows", False)
rim_c.set_editor_property("volumetric_scattering_intensity", 0.0)

pp = find_class(unreal.PostProcessVolume)[0]
s = pp.get_editor_property("settings")
set_pp(s, vignette_intensity=0.55, film_grain_intensity=0.2, bloom_intensity=0.6,
       color_contrast=v4(1.08, 1.08, 1.08), color_saturation=v4(0.9, 0.9, 0.9),
       color_gain_shadows=v4(0.94, 0.98, 1.06))
pp.set_editor_property("settings", s)
save()
log("L_00_Intro: 볼류메트릭 빛줄기, 전구 색, 역광, 후처리")

# ================================================================ 메인 화면
unreal.EditorLoadingAndSavingUtils.load_map("/Game/VRPuzzle/Maps/L_MainMenu")

sun = find("Sun").get_component_by_class(unreal.DirectionalLightComponent)
sun.set_editor_property("light_color", rgb(255, 182, 138))
sun.set_editor_property("volumetric_scattering_intensity", 2.0)

sky = find("SkyLight").get_component_by_class(unreal.SkyLightComponent)
sky.set_editor_property("intensity", 1.8)
sky.set_editor_property("light_color", rgb(195, 212, 255))          # 그림자 쪽을 푸르게

fog = find("Fog").get_component_by_class(unreal.ExponentialHeightFogComponent)
fog.set_editor_property("enable_volumetric_fog", True)
fog.set_editor_property("volumetric_fog_scattering_distribution", 0.7)

lamp_loc = MENU_ORIGIN + unreal.Vector(MENU_PLAYER_X + 85.0, 0.0, 250.0)
lamp = ensure_light(unreal.SpotLight, "Menu_ConsoleLamp", lamp_loc, unreal.Rotator(0, -90.0, 0))
lamp_c = lamp.get_component_by_class(unreal.SpotLightComponent)
lamp_c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
lamp_c.set_editor_property("intensity", 22.0)
lamp_c.set_editor_property("light_color", rgb(185, 220, 255))
lamp_c.set_editor_property("outer_cone_angle", 35.0)
lamp_c.set_editor_property("inner_cone_angle", 12.0)
lamp_c.set_editor_property("attenuation_radius", 400.0)
lamp_c.set_editor_property("cast_shadows", False)
lamp_c.set_editor_property("volumetric_scattering_intensity", 1.5)

pp = find("PostProcess")
s = pp.get_editor_property("settings")
set_pp(s, auto_exposure_bias=0.6, film_grain_intensity=0.15, bloom_intensity=0.8,
       color_saturation=v4(0.85, 0.85, 0.85), color_contrast=v4(1.05, 1.05, 1.05),
       color_gain_shadows=v4(0.92, 0.97, 1.08))
pp.set_editor_property("settings", s)
save()
log("L_MainMenu: 차가운 하늘빛, 해 빛줄기, 노출, 조작대 비상등")
log("완료")

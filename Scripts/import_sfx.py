# -*- coding: utf-8 -*-
"""
Scripts/make_sfx.py 로 만든 효과음 WAV를 /Game/VRPuzzle/Audio/SFX 로 임포트한다.
VR에서 소리가 나는 방향이 들리도록 거리 감쇠 설정(ATT_SFX)을 만들어 모든 효과음에 붙인다.
어디서 어떤 소리를 낼지는 C++ PuzzleSfx (Source/pcs_vr/VRPuzzle/PuzzleSfx.cpp) 가 정한다.

실행 방법 (먼저 make_sfx.py 로 WAV를 만들고, 에디터를 닫은 상태에서):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="<프로젝트 절대 경로>/Scripts/import_sfx.py"
  WAV 폴더를 따로 넘기지 않으면 <프로젝트>/Saved/SFX 에서 찾는다.
  (스크립트 경로는 절대 경로로. 상대 경로는 엔진 Binaries 폴더 기준으로 찾아서 실패함)

다시 실행하면 같은 이름의 에셋을 덮어쓴다 (소리를 고친 뒤 다시 넣을 때).
"""
import glob
import os
import sys

import unreal

DEST = "/Game/VRPuzzle/Audio/SFX"
ATT_NAME = "ATT_SFX"

# 이 거리(cm) 안에서는 최대 음량, 거기서 FALLOFF 만큼 더 멀어지면 들리지 않는다
INNER_RADIUS = 300.0
FALLOFF = 2000.0


def log(msg):
    unreal.log("[import_sfx] " + msg)


args = [a for a in sys.argv[1:] if a.strip()]
if args:
    src_dir = " ".join(args)
else:
    src_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "SFX")
if not os.path.isdir(src_dir):
    raise RuntimeError("WAV 폴더가 없음 (먼저 make_sfx.py 실행): " + src_dir)

files = sorted(glob.glob(os.path.join(src_dir, "SFX_*.wav")))
if not files:
    raise RuntimeError("SFX_*.wav 가 없음: " + src_dir)

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# ---------------------------------------------------------------- 거리 감쇠 설정
att_path = DEST + "/" + ATT_NAME
att = unreal.load_asset(att_path) if eal.does_asset_exist(att_path) else None
if att is None:
    att = tools.create_asset(ATT_NAME, DEST, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
settings = att.get_editor_property("attenuation")
settings.set_editor_property("spatialize", True)
settings.set_editor_property("attenuate", True)
settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
settings.set_editor_property("attenuation_shape_extents", unreal.Vector(INNER_RADIUS, 0.0, 0.0))
settings.set_editor_property("falloff_distance", FALLOFF)
att.set_editor_property("attenuation", settings)
eal.save_loaded_asset(att, only_if_is_dirty=False)
log("%s: 반지름 %.0fcm, 감쇠 거리 %.0fcm" % (ATT_NAME, INNER_RADIUS, FALLOFF))

# ---------------------------------------------------------------- WAV 임포트
tasks = []
for path in files:
    name = os.path.splitext(os.path.basename(path))[0]
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", False)
    tasks.append(t)

tools.import_asset_tasks(tasks)

done = 0
for t in tasks:
    for path in t.get_editor_property("imported_object_paths"):
        wave = unreal.load_asset(path)
        if not isinstance(wave, unreal.SoundWave):
            continue
        wave.set_editor_property("looping", False)
        wave.set_editor_property("attenuation_settings", att)
        eal.save_loaded_asset(wave, only_if_is_dirty=False)
        log("%-18s %.2f초" % (wave.get_name(), wave.get_editor_property("duration")))
        done += 1

log("임포트 완료: %d / %d" % (done, len(files)))

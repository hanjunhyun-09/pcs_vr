# -*- coding: utf-8 -*-
"""
노션 "배경음 (BGM)" 페이지의 곡 7개를 /Game/VRPuzzle/Audio/BGM 으로 임포트하고 Looping을 켠다.
맵별 재생은 C++ UBGMSubsystem (Source/pcs_vr/VRPuzzle/BGMSubsystem.cpp) 이 맵 이름을 보고 알아서 한다.

실행 방법 (에디터를 닫은 상태에서, 곡 파일 BGM_*.flac 또는 BGM_*.wav 가 들어 있는 폴더를 넘김):
  UnrealEditor-Cmd.exe pcs_vr.uproject -run=pythonscript -script="<프로젝트 절대 경로>/Scripts/import_bgm.py D:/내려받은/BGM폴더"
  (스크립트 경로는 절대 경로로. 상대 경로는 엔진 Binaries 폴더 기준으로 찾아서 실패함)

다시 실행하면 같은 이름의 에셋을 덮어쓴다 (곡을 고친 뒤 다시 넣을 때).
"""
import glob
import os
import sys

import unreal

DEST = "/Game/VRPuzzle/Audio/BGM"
NAMES = [
    "BGM_00_MainMenu",
    "BGM_01_Isolation",
    "BGM_02_Perspective",
    "BGM_03_Observation",
    "BGM_04_Device",
    "BGM_05_Core",
    "BGM_06_Ending",
]


def log(msg):
    unreal.log("[import_bgm] " + msg)


args = [a for a in sys.argv[1:] if a.strip()]
if not args:
    raise RuntimeError("곡 파일이 있는 폴더를 넘겨 주세요: -script=\"Scripts/import_bgm.py <폴더>\"")
src_dir = " ".join(args)
if not os.path.isdir(src_dir):
    raise RuntimeError("폴더가 없음: " + src_dir)

tasks = []
for name in NAMES:
    found = [p for ext in ("flac", "wav") for p in glob.glob(os.path.join(src_dir, name + "." + ext))]
    if not found:
        log("건너뜀 (파일 없음): " + name)
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", found[0])
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", False)
    tasks.append(t)

if not tasks:
    raise RuntimeError("임포트할 곡이 없음: " + src_dir)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

eal = unreal.EditorAssetLibrary
done = 0
for t in tasks:
    for path in t.get_editor_property("imported_object_paths"):
        wave = unreal.load_asset(path)
        if not isinstance(wave, unreal.SoundWave):
            continue
        wave.set_editor_property("looping", True)   # 끝과 처음이 이어지게 만든 곡 (노션 메모)
        eal.save_loaded_asset(wave, only_if_is_dirty=False)
        try:
            length = "%.1f초" % wave.get_editor_property("duration")
        except Exception:
            length = "?초"
        log("%s  %s  looping=%s" % (wave.get_name(), length, wave.get_editor_property("looping")))
        done += 1

log("임포트 완료: %d / %d" % (done, len(NAMES)))

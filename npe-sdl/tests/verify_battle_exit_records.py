"""Verify saved native/C frame and full-record evidence from the exit probe.

Run from the repository root after reference_battle_selection_entry.mjs
--exit-records, fixture generation, and the C/SDL probe. Reference binaries
remain immutable; only isolated diagnostic RAM inputs are changed.
"""
import hashlib
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def successful(path):
    value = Path(path).read_text(encoding="utf-8-sig").strip()
    assert value == "0", (str(path), value)
    return 0


prior = json.loads(Path("tmp/live-coordinate-report.json").read_text(encoding="utf-8-sig"))
for path, expected in prior["immutable_sha256"].items():
    assert sha(path) == expected, path
assert sha("npe-sdl/save.c") == prior["source_sha256"]["npe-sdl/save.c"]
targets = json.loads(Path("tmp/battle-exit-records-check-targets.json").read_text())
codes = {}
for label in ["final-build", "target-build", "markers-native", "defeat-repair-build",
              "defend-target-native", "defend-target-generator", "defend-target-build"]:
    path = "tmp/battle-exit-records-" + label + ".exit"
    codes[path] = successful(path)
for target in targets:
    path = "tmp/battle-exit-records-final-" + target + ".exit"
    codes[path] = successful(path)
for label in ["final-build", "target-build", "defeat-repair-build", "defend-target-build"]:
    log = Path("tmp/battle-exit-records-" + label + ".log").read_text(encoding="utf-8-sig")
    assert "warning:" not in log and "error:" not in log, label

rows = json.loads(Path("tmp/native-battle-exit-records.json").read_text())["rows"]
assert len(rows) == 7
totals, evidence = {}, {}
for route in ["direct", "main"]:
    commits = entry_records = exit_records = 0
    for index, row in enumerate(rows):
        native = "tmp/native-battle-exit-records-" + str(index)
        actual = "tmp/native-battle-exit-records-c-" + str(index) + "-" + route
        for commit in range(len(row["commits"])):
            ref = Path(native + "-commit-" + str(commit) + ".idx")
            out = Path(actual + "-entry-" + str(commit) + ".idx")
            assert len(ref.read_bytes()) == 28800 and out.read_bytes() == ref.read_bytes()
            evidence[str(ref)] = sha(ref)
            evidence[str(out)] = sha(out)
            commits += 1
        for step, nav in enumerate(row["menuNavigation"]):
            for commit in range(len(nav["frames"])):
                suffix = "-menu-" + str(step) + "-" + str(commit) + ".idx"
                ref, out = Path(native + suffix), Path(actual + suffix)
                assert out.read_bytes() == ref.read_bytes()
                evidence[str(ref)] = sha(ref)
                evidence[str(out)] = sha(out)
                commits += 1
        for phase, state in [("entry", row["after"]), ("exit", row["menuNavigation"][-1]["after"])]:
            for member in range(row["config"]["party"]):
                for label, key in [("map", "residentMapRecords"), ("battle", "residentBattlePartyRecords")]:
                    out = Path(actual + "-" + phase + "-" + label + "-record-" + str(member) + ".bin")
                    data = out.read_bytes()
                    assert len(data) == 148 and data == bytes(state[key][member]), str(out)
                    evidence[str(out)] = sha(out)
                    if phase == "entry":
                        entry_records += 1
                    else:
                        exit_records += 1
    totals[route] = {"pixel_commits": commits, "full_entry_records": entry_records,
                     "full_exit_records": exit_records}

save_paths = 0
for index, row in enumerate(rows):
    end = row["menuNavigation"][-1]["after"]
    if end["mode"] != 1:
        continue
    for route in ["cold", "warm"]:
        for member in range(3):
            out = Path(f"tmp/native-battle-exit-records-c-{index}-save-{route}-map-record-{member}.bin")
            data = out.read_bytes()
            assert len(data) == 148
            if member < row["config"]["party"]:
                assert data == bytes(end["residentMapRecords"][member]), str(out)
            else:
                # Native exit capture covers the participating prefix.
                # The probe separately checks preservation of all3 map
                # slots; keep both save outputs for the unused slots.
                cold = Path(f"tmp/native-battle-exit-records-c-{index}-save-cold-map-record-{member}.bin")
                assert data == cold.read_bytes(), str(out)
            evidence[str(out)] = sha(out)
        save_paths += 1

sources = ["npe-sdl/main.c", "npe-sdl/world.c", "npe-sdl/save.c", "npe-sdl/npe.h",
           "npe-sdl/CMakeLists.txt", "npe-sdl/tests/sdl_walk_battle_entry_probe.c",
           "npe-sdl/tests/reference_battle_selection_entry.mjs",
           "npe-sdl/tests/generate_enemy_record_entry_fixture.py",
           "npe-sdl/tests/battle_exit_records_native.h", "npe-sdl/tests/verify_battle_exit_records.py",
           "npe-sdl/tests/sdl_defeat_animation_probe.c", "npe-sdl/tests/battle_target_execution_probe.c",
           "npe-sdl/tests/reference_battle_target_execution.mjs",
           "npe-sdl/tests/generate_battle_defend_targets_fixture.py",
           "npe-sdl/tests/battle_defend_targets_native.h"]
report = {"passed": True, "scope": "Seven diagnostic battle exits with full physical party records and actual SDL commits",
          "paths": len(rows), "routes": totals, "own_save_cold_and_running_paths": save_paths,
          "build_targets": len(set(targets + ["npe-sdl"])), "short_checks": len(targets),
          "build_warnings": 0, "command_exit_codes": codes,
          "immutable_sha256": prior["immutable_sha256"],
          "source_sha256": {path: sha(path) for path in sources},
          "native_fixture_sha256": {path: sha(path) for path in ["tmp/native-battle-exit-records.json",
              "tmp/native-battle-defend-target-execution-all.json", "tmp/native-battle-target-execution-all.json",
              "npe-sdl/tests/battle_target_execution_native.h", "npe-sdl/tests/defeat_animation_native.h"]},
          "evidence_sha256": evidence, "own_save_format": "NPESAV22",
          "own_save_source_unchanged": True, "bound_world_bytes": 3528,
          "all_exit_actions_and_lifecycles_proven": False,
          "num_zero_other_actions_complete": False, "num255_complete": False,
          "actor_voice_playback_complete": False, "physical_audio_time_proven": False}
Path("tmp/battle-exit-records-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps({k: report[k] for k in ["passed", "paths", "routes", "own_save_cold_and_running_paths", "short_checks"]}))

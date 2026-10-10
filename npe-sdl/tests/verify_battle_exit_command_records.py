"""Check persisted native/C exit records, frames and inventories from five paths.

Run from the repository root after the reference, fixture generation, and
tmp/run_exit_command_checks.py. Native end states are never seeded into C.
"""
import hashlib
import json
import sys
from pathlib import Path


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


phase_resident = "--phase-resident" in sys.argv
command_resident = "--command-resident" in sys.argv or phase_resident
menu_resident = "--menu-resident" in sys.argv or command_resident
reentry_actions = "--reentry-actions" in sys.argv or menu_resident
record_reentry = "--record-reentry" in sys.argv or reentry_actions
prefix = "tmp/battle-phase-resident-" if phase_resident else "tmp/battle-command-resident-" if command_resident else "tmp/battle-menu-resident-" if menu_resident else "tmp/battle-record-reentry-actions-" if reentry_actions else "tmp/battle-record-reentry-" if record_reentry else "tmp/battle-exit-command-records-"
fixture = "tmp/native-battle-phase-resident" if phase_resident else "tmp/native-battle-command-resident" if command_resident else "tmp/native-battle-menu-resident" if menu_resident else "tmp/native-battle-reentry-actions" if reentry_actions else "tmp/native-battle-record-reentry" if record_reentry else "tmp/native-battle-exit-command-records"
prior = json.loads(Path("tmp/battle-exit-records-report.json").read_text(encoding="utf-8-sig"))
for path, expected in prior["immutable_sha256"].items():
    assert sha(path) == expected, path
if not menu_resident:
    assert sha("npe-sdl/save.c") == prior["source_sha256"]["npe-sdl/save.c"]
targets = json.loads(Path(prefix + "check-targets.json").read_text())
codes = {}
labels = ["native", "generator", "configure", "build", "final-build"] if record_reentry else ["final-native", "generator", "final-build", "final-item-repair-build"]
if phase_resident:
    labels.append("final-fixture-build")
for label in [*labels, *["final-"+t for t in targets]]:
    path = prefix + label + ".exit"
    assert Path(path).read_text(encoding="utf-8-sig").strip() == "0", path
    codes[path] = 0
if menu_resident:
    path = prefix + "load-native.exit" if command_resident else "tmp/battle-menu-rows-load-native.exit"
    assert Path(path).read_text().strip() == "0", path
    codes[path] = 0
build_labels = ["build", "final-build"] if record_reentry else ["final-build", "final-item-repair-build"]
if phase_resident:
    build_labels.append("final-fixture-build")
for label in build_labels:
    build = Path(prefix + label + ".log").read_text(encoding="utf-8-sig")
    assert "warning:" not in build and "error:" not in build
rows = json.loads(Path(fixture + ".json").read_text())["rows"]
assert len(rows) == (3 if record_reentry else 5)
if command_resident:
    assert [row["reentry"]["menuInput"]["battlePreviousKind"] for row in rows] == [129, 2, 255]
evidence = {}


def compare(path, expected, length):
    data = Path(path).read_bytes()
    assert len(data) == length and data == expected, path
    evidence[path] = sha(path)


def inventory(state):
    return b"".join(int(v).to_bytes(2, "big") for v in state["commandInventory"])


totals = {}
for route in ["direct", "main"]:
    commits = entry_records = exit_records = reentry_frames = lifecycle_records = action_frames = action_records = menu_states = 0
    for index, row in enumerate(rows):
        native = fixture + "-" + str(index)
        actual = fixture + "-c-" + str(index) + "-" + route
        for commit in range(len(row["commits"])):
            ref = native + "-commit-" + str(commit) + ".idx"
            compare(actual + "-entry-" + str(commit) + ".idx", Path(ref).read_bytes(), 28800)
            evidence[ref] = sha(ref)
            commits += 1
        for step, nav in enumerate(row["menuNavigation"]):
            for commit in range(len(nav["frames"])):
                suffix = "-menu-" + str(step) + "-" + str(commit) + ".idx"
                ref = native + suffix
                compare(actual + suffix, Path(ref).read_bytes(), 28800)
                evidence[ref] = sha(ref)
                commits += 1
        end = row["menuNavigation"][-1]["after"]
        if phase_resident:
            compare(actual + "-exit-phase.bin", bytes([end["battleMenu"]]), 1)
        compare(actual + "-inventory.bin", inventory(end), 6)
        for phase, state in [("entry", row["after"]), ("exit", end)]:
            for member in range(row["config"]["party"]):
                for label, key in [("map", "residentMapRecords"), ("battle", "residentBattlePartyRecords")]:
                    path = actual + "-" + phase + "-" + label + "-record-" + str(member) + ".bin"
                    compare(path, bytes(state[key][member]), 148)
                    if phase == "entry":
                        entry_records += 1
                    else:
                        exit_records += 1
        if record_reentry:
            for commit in range(len(row["reentry"]["frames"])):
                ref = native + "-reentry-" + str(commit) + ".idx"
                compare(actual + "-reentry-" + str(commit) + ".idx", Path(ref).read_bytes(), 28800)
                evidence[ref] = sha(ref)
                commits += 1
                reentry_frames += 1
            for phase in ["retained", "input", "after"]:
                state = row["reentry"][phase]
                for label, key in [("map", "allMapRecords"), ("battle", "allBattleRecords")]:
                    for member, expected in enumerate(state[key]):
                        path = actual + "-" + phase + "-" + label + "-record-" + str(member) + ".bin"
                        compare(path, bytes(expected), 148)
                        lifecycle_records += 1
            if reentry_actions:
                for step, nav in enumerate(row["reentry"]["actions"]):
                    if menu_resident:
                        expected_menu = [nav["after"]["battleRow"], nav["after"]["battleItemMode"]]
                        if command_resident:
                            expected_menu += [nav["after"]["battleActor"], nav["after"]["battlePreviousKind"]]
                        if phase_resident:
                            expected_menu += [nav["after"]["battleMenu"]]
                        compare(actual + f"-action-state-{step}-menu.bin",
                                bytes(expected_menu), len(expected_menu))
                        menu_states += 1
                    for commit in range(len(nav["frames"])):
                        suffix = f"-action-{step}-{commit}.idx"
                        ref = native + suffix
                        compare(actual + suffix, Path(ref).read_bytes(), 28800)
                        evidence[ref] = sha(ref)
                        commits += 1
                        action_frames += 1
                    for label, key in [("map", "allMapRecords"), ("battle", "allBattleRecords")]:
                        for member, expected in enumerate(nav["after"][key]):
                            compare(actual + f"-action-state-{step}-{label}-record-{member}.bin", bytes(expected), 148)
                            action_records += 1
    totals[route] = {"pixel_commits": commits, "full_entry_records": entry_records,
                     "full_exit_records": exit_records}
    if record_reentry:
        assert totals[route] == {"pixel_commits": 233 if phase_resident else 239 if menu_resident else 212 if reentry_actions else 105, "full_entry_records": 14, "full_exit_records": 14}
        assert reentry_frames == 24 and lifecycle_records == 81
        totals[route].update(reentry_pixel_commits=reentry_frames, full_lifecycle_records=lifecycle_records)
        if reentry_actions:
            assert action_frames == (128 if phase_resident else 134 if menu_resident else 107) and action_records == (342 if phase_resident else 396 if menu_resident else 180)
            totals[route].update(action_pixel_commits=action_frames, full_action_records=action_records)
            if menu_resident:
                assert menu_states == (38 if phase_resident else 44)
                totals[route]["resident_menu_states"] = menu_states
    else:
        assert totals[route] == {"pixel_commits": 133, "full_entry_records": 22, "full_exit_records": 22}

save_paths = 0
for index, row in enumerate(rows):
    end = row["menuNavigation"][-1]["after"]
    if end["mode"] != 1:
        continue
    for route in ["cold", "warm"]:
        actual = fixture + f"-c-{index}-save-{route}"
        compare(actual + "-inventory.bin", inventory(end), 6)
        for member in range(3):
            path = actual + f"-map-record-{member}.bin"
            if record_reentry:
                expected = bytes(end["allMapRecords"][member])
            elif member < row["config"]["party"]:
                expected = bytes(end["residentMapRecords"][member])
            else:
                # The C probe also compares all three slots with its pre-save
                # world. Native captures contain only participating slots.
                expected = Path(fixture + f"-c-{index}-save-cold-map-record-{member}.bin").read_bytes()
            compare(path, expected, 148)
        save_paths += 1
assert save_paths == 6
warm_loads = []
if menu_resident:
    warm_file = "tmp/native-battle-phase-load.json" if phase_resident else "tmp/native-battle-command-rows-load.json" if command_resident else "tmp/native-battle-menu-rows-load.json"
    warm_loads = json.loads(Path(warm_file).read_text())
    assert [row["entry"] for row in warm_loads] == ["system", "title", "new", "empty"]
    for row in warm_loads:
        assert row["beforeBytes"] == row["afterBytes"] == [129, 166]
        own_prefix = "tmp/battle-phase-own-" if phase_resident else "tmp/battle-command-rows-own-" if command_resident else "tmp/battle-menu-rows-own-"
        base = f"{own_prefix}{row['entry']}-base.dat"
        altered = f"{own_prefix}{row['entry']}-altered.dat"
        data = Path(base).read_bytes()
        assert data[:8] == b"NPESAV22"
        compare(altered, data, len(data))
        evidence[base] = sha(base)
sources = ["npe-sdl/main.c", "npe-sdl/world.c", "npe-sdl/save.c", "npe-sdl/npe.h",
           "npe-sdl/CMakeLists.txt", "npe-sdl/tests/sdl_walk_battle_entry_probe.c",
           "npe-sdl/tests/reference_battle_selection_entry.mjs",
           "npe-sdl/tests/generate_enemy_record_entry_fixture.py",
           "npe-sdl/tests/battle_exit_command_records_native.h",
           "npe-sdl/tests/verify_battle_exit_command_records.py", "npe-sdl/tests/item_reservation_probe.c",
           "tmp/run_exit_command_checks.py"]
if record_reentry:
    sources += ["npe-sdl/tests/battle_record_reentry_native.h", "tmp/run_record_reentry.py", "tmp/run_record_reentry_checks.py",
                "npe-sdl/resident_records.inc", "npe-sdl/world_enemy_loader.inc"]
if reentry_actions:
    sources += ["npe-sdl/tests/battle_reentry_actions_native.h", "npe-sdl/text.c"]
if menu_resident:
    sources += ["npe-sdl/tests/battle_menu_resident_native.h", "npe-sdl/tests/battle_menu_rows_load_native.h",
                "npe-sdl/tests/battle_menu_rows_load_probe.c", "npe-sdl/tests/reference_battle_selection_load.mjs",
                "npe-sdl/resident_resources.inc", "tmp/run_battle_menu_resident.py"]
if command_resident:
    sources += ["npe-sdl/tests/battle_command_resident_native.h", "npe-sdl/tests/battle_command_rows_load_native.h",
                "tmp/run_battle_command_resident.py", "tmp/audit_battle_menu_controls.py"]
if phase_resident:
    sources += ["npe-sdl/tests/battle_phase_resident_native.h", "npe-sdl/tests/battle_phase_load_native.h",
                "tmp/run_battle_phase_resident.py", "tmp/dump_battle_menu_phase.py",
                "npe-sdl/tests/sdl_enemy_target_flow_probe.c",
                "npe-sdl/tests/sdl_battle_skill_resident_probe.c",
                "npe-sdl/tests/sdl_battle_item_resident_probe.c"]
report = {"passed": True, "scope": "Resident battle menu phase: mutated public input, retained LCD buttons, actor changes, exits/reentry and four warm constructors" if phase_resident else "Resident current actor and previous command: shared bytes, mutated defense-copy consumers, public reentry and four warm constructors" if command_resident else "Two resident list-selection bytes: public reentry, defense rounds, actual menus and four warm constructors" if menu_resident else "Three exits, changed party size, public reentry and one complete defense round with all nine physical records" if reentry_actions else "Three diagnostic exits, changed party size and public reentry with all nine physical records" if record_reentry else "Five diagnostic exits with retained item IDs, statuses and counters",
          "paths": len(rows), "routes": totals, "own_save_cold_and_running_paths": save_paths,
          "build_targets": len(targets)+1, "short_checks": len(targets), "build_warnings": 0,
          "command_exit_codes": codes, "immutable_sha256": prior["immutable_sha256"],
          "source_sha256": {path: sha(path) for path in sources},
          "native_fixture_sha256": {fixture+".json": sha(fixture+".json")},
          "evidence_sha256": evidence, "own_save_format": "NPESAV22",
          "own_save_source_unchanged": not menu_resident, "bound_world_bytes": 3533 if phase_resident else 3532 if command_resident else 3530 if menu_resident else 3528,
          "all_exit_actions_and_lifecycles_proven": False,
          "num_zero_other_actions_complete": False, "num255_complete": False,
          "actor_voice_playback_complete": False, "physical_audio_time_proven": False}
if menu_resident:
    report.update(own_save_format_unchanged=True, warm_constructor_paths=len(warm_loads),
                  all_ui_control_state_bound=False)
    path = "tmp/native-battle-phase-load.json" if phase_resident else "tmp/native-battle-command-rows-load.json" if command_resident else "tmp/native-battle-menu-rows-load.json"
    report["native_fixture_sha256"][path] = sha(path)
if command_resident:
    actors = sorted({nav["after"]["battleActor"] for row in rows for nav in row["reentry"]["actions"]})
    assert actors == [0, 1, 2]
    report.update(mutated_previous_command_bytes=[129, 2, 255], current_actor_values=actors,
                  actor_out_of_range_execution_proven=False, menu_phase_bound=phase_resident,
                  all_global_writers_proven=False)
    path = "tmp/battle-menu-controls-references.asm"
    report["evidence_sha256"][path] = sha(path)
if phase_resident:
    assert all(row["reentry"]["menuInput"]["battleMenu"] == 30 for row in rows)
    assert [row["menuNavigation"][-1]["after"]["battleMenu"] for row in rows] == [58, 31, 31]
    assert all(row["reentry"]["after"]["battleMenu"] == 10 for row in rows)
    report.update(phase_input_bytes=[30, 30, 30], phase_exit_bytes=[58, 31, 31],
                  phase_reentry_bytes=[10, 10, 10], all_selector_phase_writers_proven=False)
    path = "tmp/battle-menu-phase-source.asm"
    report["evidence_sha256"][path] = sha(path)
Path(prefix+"report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
print(json.dumps({key: report[key] for key in ["passed", "paths", "routes", "own_save_cold_and_running_paths", "short_checks"]}))

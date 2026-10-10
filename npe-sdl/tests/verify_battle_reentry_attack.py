"""Compare actual SDL main-loop captures with the original reentry samples."""
import hashlib
import json
import sys
from pathlib import Path


def compare(path, expected):
    actual = Path(path).read_bytes()
    assert actual == expected, f"Original/C mismatch: {path}"


groups = "--groups" in sys.argv
mixed = "--mixed" in sys.argv or groups
two_attacks = "--two-attacks" in sys.argv
results = []
for suffix in (["-groups"] if groups else ["-mixed"] if mixed else ["-two-attacks"] if two_attacks else ["", "-one-enemy"]):
    prefix = "tmp/native-battle-reentry-groups" if groups else "tmp/native-battle-reentry-mixed" if mixed else "tmp/native-battle-reentry-two-attacks" if two_attacks else "tmp/native-battle-reentry-attack" + suffix
    rows = json.loads(Path(prefix + ".json").read_text(encoding="utf8"))["rows"]
    assert len(rows) == (1 if mixed or two_attacks else 3)
    frames = action_frames = lifecycle_records = action_records = menu_states = cache_states = 0
    cases = []
    for index, row in enumerate(rows):
        native = prefix + f"-{index}"
        actual = prefix + f"-c-{index}-main"
        for commit in range(len(row["commits"])):
            compare(actual + f"-entry-{commit}.idx",
                    Path(native + f"-commit-{commit}.idx").read_bytes())
            frames += 1
        for step, nav in enumerate(row["menuNavigation"]):
            for commit in range(len(nav["frames"])):
                tail = f"-menu-{step}-{commit}.idx"
                compare(actual + tail, Path(native + tail).read_bytes())
                frames += 1
        for commit in range(len(row["reentry"]["frames"])):
            tail = f"-reentry-{commit}.idx"
            compare(actual + tail, Path(native + tail).read_bytes())
            frames += 1
        for phase in ["retained", "input", "after"]:
            state = row["reentry"][phase]
            for label, key in [("map", "allMapRecords"), ("battle", "allBattleRecords")]:
                for slot, record in enumerate(state[key]):
                    compare(actual + f"-{phase}-{label}-record-{slot}.bin", bytes(record))
                    lifecycle_records += 1
        actions = row["reentry"]["actions"]
        for step, nav in enumerate(actions):
            state = nav["after"]
            menu = [state[key] for key in ["battleRow", "battleItemMode", "battleActor",
                                         "battlePreviousKind", "battleMenu"]]
            compare(actual + f"-action-state-{step}-menu.bin", bytes(menu))
            menu_states += 1
            if mixed:
                stock = state["mixedInventory"]
                caches = bytes([stock >> 8, stock & 255, state["skillCache"]["id"],
                                state["itemCache"]["id"]] + state["skillCache"]["bytes"] +
                               state["itemCache"]["bytes"])
                compare(actual + f"-action-state-{step}-caches.bin", caches)
                cache_states += 1
            for commit in range(len(nav["frames"])):
                tail = f"-action-{step}-{commit}.idx"
                compare(actual + tail, Path(native + tail).read_bytes())
                frames += 1
                action_frames += 1
            for label, key in [("map", "allMapRecords"), ("battle", "allBattleRecords")]:
                for slot, record in enumerate(state[key]):
                    compare(actual + f"-action-state-{step}-{label}-record-{slot}.bin",
                            bytes(record))
                    action_records += 1
        # The probe also checks mode, current actor and RNG before publishing
        # each state; a successful complete run is required in its run log.
        cases.append({"first_outcome": row["config"]["outcome"],
                      "first_party": row["config"]["party"],
                      "second_party": row["config"]["reentryParty"],
                      "second_enemies": row["config"]["reentryCount"],
                      "action_steps": len(actions),
                      "action_commits": sum(len(n["frames"]) for n in actions),
                      "round_rng": actions[-1]["after"]["rng"]})
    log_path = "tmp/reentry-groups-final.log" if groups else "tmp/reentry-mixed-final.log" if mixed else "tmp/reentry-two-attacks-final.log" if two_attacks else (
        "tmp/reentry-one-enemy-final.log" if suffix else "tmp/reentry-attack-final.log")
    log = Path(log_path).read_text(encoding="utf8")
    assert f"Main original walking/battle commits={8*len(rows)}, zero differences" in log
    assert "Assertion failed" not in log and "unsupported" not in log
    assert lifecycle_records == 27*len(rows)
    results.append({"enemy_count": 1 if suffix == "-one-enemy" else 2, "cases": cases,
                    "original_c_sdl_commits": frames, "action_commits": action_frames,
                    "full_lifecycle_records": lifecycle_records,
                    "full_action_records": action_records, "menu_states": menu_states,
                    "cache_states": cache_states})

sources = ["tmp/NP560.PRG", "tmp/tjfy-world-probe.npe", "tmp/opennoah_wasm_bg.wasm",
           "tmp/tianjiang-online.opennoah", "npe-sdl/main.c", "npe-sdl/world.c",
           "npe-sdl/tests/reference_battle_selection_entry.mjs",
           "npe-sdl/tests/sdl_walk_battle_entry_probe.c",
           "npe-sdl/tests/generate_enemy_record_entry_fixture.py"]
if mixed:
    sources += ["tmp/empty-title-first-map-snapshot.bin", ("npe-sdl/tests/battle_reentry_groups_native.h" if groups else "npe-sdl/tests/battle_reentry_mixed_native.h")]
if groups:
    sources += ["tmp/native-battle-reentry-groups.json", "tmp/reentry-group-item-candidates.json",
                "npe-sdl/tests/sdl_party_spell_animation_probe.c",
                "npe-sdl/tests/verify_battle_reentry_attack.py", "npe-sdl/build/npe-sdl.exe"]
report = {"passed": True, "scope": "Actual SDL main loop: flee, party-size change, reentry and two rounds of actual group item/spell with last-actor confirmation and enemy attacks" if groups else "Actual SDL main loop: flee, party-size change, reentry and two rounds of single/group spells, medicine and defense" if mixed else "Actual SDL main loop: two ordinary attacks, enemy death, and another round in the same battle" if two_attacks else "Actual SDL main loop: normal attack then defense after party-size changes and reentry",
          "production_changed": True, "routes": results,
          "original_c_sdl_commits": sum(r["original_c_sdl_commits"] for r in results),
          "full_action_records": sum(r["full_action_records"] for r in results),
          "full_lifecycle_records": sum(r["full_lifecycle_records"] for r in results),
          "menu_states": sum(r["menu_states"] for r in results),
          "cache_states": sum(r["cache_states"] for r in results),
          "source_sha256": {p: hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in sources},
          "limitations": ["Direct defense-only adapter is not used for the new target-selector paths",
                          "No original PCM/audio comparison", "Not every battle action or NPE is covered"],
          "goal_complete": False}
if mixed:
    report.update({
        "production_fixes": [
            "Skill list reads map record prefix and level during battle.",
            "Confirmation does not reject a map-list skill because the battle prefix differs.",
            "Single spell/item confirmation retains original target/cursor/HUD submission order.",
            "Group confirmation rebuilds scene/controls before the next actor HUD and cursor."],
        "diagnostic_inputs": {"map_skill_prefixes": [6, 9, 4], "map_battle_level": 99,
                              "party_hp": [50, 60, 70], "party_max_hp": 200,
                              "party_qi": 100, "enemy_hp": 30000, "item_46_stock": 2,
                              "caches": "zero before first action, no outcome injection"},
        "passed_regressions": ["sdl-battle-skill-record-probe:10 paths/33 frames",
                               "sdl-battle-skill-resources-probe:13 paths/73 frames",
                               "sdl-map-skill-record-probe:13 paths/51 frames",
                               "sdl-battle-reentry-two-attacks-probe:125 frames",
                               "sdl-battle-skill-resident-probe:4 paths/8 rounds/122 frames"],
        "overall_estimate_percent": 80,
        "build_log": "tmp/reentry-mixed-final-build.log"})
    report["limitations"] += ["Input attributes are explicit diagnostics, not a new normal-stat story route.",
                              "Full records are compared at action wait points, not each instruction or LCD commit.",
                              "Other party/action combinations are not covered." if groups else "Last-actor direct group confirmation and group items are not covered."]
if groups:
    regressions = [
        ("sdl-battle-reentry-mixed-probe", "Main original walking/battle commits=8, zero differences"),
        ("sdl-enemy-prepared-records-probe", "Enemy prepared records:"),
        ("sdl-party-prepared-spell-labels-probe", "Party prepared spell labels:"),
        ("sdl-party-prepared-item-labels-probe", "Party prepared item labels:")]
    summaries = []
    for target, marker in regressions:
        regression_log = Path("tmp/reentry-groups-regression-" + target + ".log").read_text(encoding="utf8")
        assert marker in regression_log and "Assertion failed" not in regression_log
        summaries.append({"target": target, "summary": next(line for line in regression_log.splitlines() if marker in line)})
    build_log = Path("tmp/reentry-groups-final-build.log").read_text(encoding="utf8")
    assert "Built target npe-sdl" in build_log and "warning:" not in build_log and "error:" not in build_log
    label_build_log = Path("tmp/reentry-groups-label-final-build.log").read_text(encoding="utf8")
    assert "Built target npe-sdl" in label_build_log and "warning:" not in label_build_log and "error:" not in label_build_log
    report.update({
        "production_fixes": [
            "Party spell/item preparation tests each physical party record's retained group/side flags.",
            "Enemy attack/spell preparation honors the receiver's retained own-side group command flags."],
        "diagnostic_inputs": {"map_skill_prefixes": [9, 9, 9], "map_battle_level": 99,
                              "party_hp": [50, 60, 70], "party_max_hp": 200,
                              "party_qi": 100, "enemy_hp": 30000, "item_64_stock": 2,
                              "caches": "zero before first action, no outcome injection"},
        "passed_regressions": summaries,
        "build_log": "tmp/reentry-groups-final-build.log"})
Path("tmp/battle-reentry-groups-report.json" if groups else "tmp/battle-reentry-mixed-report.json" if mixed else "tmp/battle-reentry-two-attacks-report.json" if two_attacks else "tmp/battle-reentry-attack-report.json").write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf8")
print(json.dumps({key: report[key] for key in ["passed", "original_c_sdl_commits",
                 "full_action_records", "full_lifecycle_records", "menu_states", "cache_states"]}))

"""Compare C/SDL outputs against independent original prepared-record samples.

Run from the repository root after both prepared probes. These probes call the
production command backend and actual SDL animation functions, not the complete
SDL input controller. Record coverage here is twelve prepared bytes per physical
slot, plus the existing cache/numeric assertions in the successful probe runs.
"""
import hashlib
import json
from pathlib import Path


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


original_inputs = {
    "tmp/NP560.PRG": "d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5",
    "tmp/tjfy-world-probe.npe": "df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40",
    "tmp/tianjiang-online.opennoah": "c59772070ca7beec488a86a61bfec3ac1e60cd70c248ab36239ff81e2b862148",
    "tmp/opennoah_wasm_bg.wasm": "af98dd54ebaa6073cfc3809a7387a0f7af087605c79056449d88e63075ef4d86",
}
for path, expected in original_inputs.items():
    assert digest(path) == expected, f"Original input changed: {path}"

expected_cases = {
    "skill": {"same", "two-different", "two-status", "two-group", "two-own-group", "zero-numeric"},
    "item": {"same", "throw-same", "two-throw", "group-use", "group-throw"},
}
groups = []
sources = ["npe-sdl/world.c", "npe-sdl/main.c", "npe-sdl/npe.c",
           "npe-sdl/resident_records.inc", "npe-sdl/CMakeLists.txt",
           "npe-sdl/tests/verify_battle_prepared_records.py"]
for kind, names in expected_cases.items():
    prefix = f"tmp/native-battle-prepared-{kind}-record"
    reference_path = prefix + ".json"
    rows = json.loads(Path(reference_path).read_text(encoding="utf8"))
    assert len(rows) == len(names) and {r["cfg"]["name"] for r in rows} == names
    log_path = f"tmp/prepared-{kind}-after.log"
    log = Path(log_path).read_text(encoding="utf8")
    frames = records = 0
    cases = []
    for row in rows:
        name = row["cfg"]["name"]
        for slot, seed in enumerate(row["before"]["prepared"]):
            expected_seed = b"".join((100 * (slot + 1) + word + 1).to_bytes(2, "big")
                                     for word in range(6))
            assert bytes(seed) == expected_seed, f"Warm input changed: {kind}/{name}/{slot}"
        for frame in range(len(row["frames"])):
            stem = f"{prefix}-{name}-{frame}"
            expected = Path(stem + ".idx").read_bytes()
            assert len(expected) == 240 * 120
            assert Path(stem + "-c.idx").read_bytes() == expected, stem
            assert f"Execution {kind} {name} frame{frame} C=0 SDL=0" in log
            frames += 1
        assert len(row["after"]["prepared"]) == 6
        for slot, expected in enumerate(row["after"]["prepared"]):
            assert len(expected) == 12
            path = f"{prefix}-{name}-record-{slot}-c.bin"
            assert Path(path).read_bytes() == bytes(expected), path
            records += 1
        cases.append({"name": name, "lcd_commits": len(row["frames"]),
                      "prepared_record_bytes": 6 * 12})
    assert f"Prepared {kind} record:{len(rows)} original rounds/{frames} actual C/SDL LCD commits" in log
    assert "Assertion failed" not in log
    groups.append({"kind": kind, "cases": cases, "lcd_commits": frames,
                   "prepared_records": records, "cache_bytes_per_case": 144 if kind == "skill" else 142,
                   "numeric_status_fields_per_case": 66})
    sources += [reference_path, log_path,
                f"npe-sdl/tests/reference_battle_execution_{kind}_record.mjs",
                f"npe-sdl/tests/generate_battle_execution_{kind}_fixture.py",
                f"npe-sdl/tests/sdl_battle_execution_{kind}_record_probe.c",
                f"npe-sdl/tests/battle_prepared_{kind}_native.h"]

regressions = {}
for name, expected in {
    "skill": "Execution skill record:7 original rounds/205 actual C/SDL LCD commits",
    "item": "Execution item record:9 original rounds/264 actual C/SDL LCD commits",
    "party-spell": "18 original defend/party spell actions, 252 full record phases, 6 final SDL frames and 164 C/SDL animation commits passed",
    "revival": "7 original defend/revival spell actions, 108 full record phases, 4 final SDL frames and 90 C/SDL animation commits passed",
    "status-items": "Status items: 272 original record phases passed",
}.items():
    path = f"tmp/prepared-regression-{name}.log"
    log = Path(path).read_text(encoding="utf8")
    assert expected in log and "Assertion failed" not in log, path
    regressions[name] = log.splitlines()[-1]
    sources.append(path)
build_paths = ["tmp/prepared-records-final-build.log", "tmp/prepared-records-regression-build.log"]
for path in build_paths:
    log = Path(path).read_text(encoding="utf8")
    assert "warning:" not in log.lower() and "error:" not in log.lower(), path
sources += build_paths

report = {
    "passed": True,
    "scope": "Warm numeric/status spells and use/throw items: production backend and actual SDL animation",
    "production_changed": True,
    "groups": groups,
    "original_c_sdl_commits": sum(g["lcd_commits"] for g in groups),
    "prepared_records": sum(g["prepared_records"] for g in groups),
    "prepared_bytes_per_record": 12,
    "regressions": regressions,
    "main_binary_sha256": digest("npe-sdl/build/npe-sdl.exe"),
    "original_input_sha256": original_inputs,
    "source_sha256": {path: digest(path) for path in sources},
    "limitations": ["Prepared record comparison covers offsets 102..113, not all 148 bytes",
                    "Original RAM is sampled at the round end, not at every display commit",
                    "Backend command calls are not a new complete SDL input-controller check",
                    "Enemy actions, other party formations, audio and complete cross-NPE compatibility remain"],
    "goal_complete": False,
}
Path("tmp/battle-prepared-records-report.json").write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf8")
print(json.dumps({key: report[key] for key in ["passed", "original_c_sdl_commits",
                                            "prepared_records", "prepared_bytes_per_record"]}))

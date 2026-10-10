"""Verify own-save restoration of actual NPE special-map states."""
import hashlib
import json
from pathlib import Path


def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def text(path):
    return Path(path).read_text(encoding="utf8")


npe = Path("tmp/tianming-v13-map-continuation.npe").read_bytes()
log = text("tmp/special-map-save-final.log")
rows = [json.loads(line) for line in log.splitlines() if line.startswith("{")]
assert len(rows) == 4 and all(row.get("passed", True) for row in rows)
assert "Assertion failed" not in log and "four checksum-valid malformed V24 slots rejected transactionally" in log
prefix = len(Path("tmp/special-map-regression-terrain.npes.canonical").read_bytes())
cases = []
for number, version, width, height in [(139, b"NPESAV22", 36, 78), (140, b"NPESAV24", 0, 201)]:
    path = f"tmp/special-map-{number}-save.npes"
    wire = Path(path).read_bytes()
    assert wire[:8] == version
    assert int.from_bytes(wire[8:16], "little") == len(npe)
    assert int.from_bytes(wire[16:24], "little") == fnv(npe)
    assert int.from_bytes(wire[24:28], "little") == len(wire) - 36
    assert int.from_bytes(wire[28:36], "little") == fnv(wire[36:])
    extension = wire[prefix:]
    count = width * height
    if version == b"NPESAV22":
        stored = 0
        assert not extension
    else:
        assert extension[:2] == bytes([width, height]) and int.from_bytes(extension[2:4], "little") == count
        stored = max(count, 3500) if version == b"NPESAV24" else count
        assert len(extension) == 196 + 2 * stored
        before = Path(f"tmp/special-map-{number}-before-buffer.bin").read_bytes()
        assert extension[196:] == before[:2 * stored]
    assert Path(f"tmp/special-map-{number}-before-save.idx").read_bytes() == Path(f"tmp/special-map-{number}-cold-load.idx").read_bytes()
    assert Path(path + ".after-move").read_bytes() == Path(path + ".closed").read_bytes()
    state = next(row for row in rows if row.get("save_checked") and row["map"] == number)
    assert state["warm_diff"] == state["cold_diff"] == state["move_diff"] == 0
    assert f"special-map-save map{number} closed save equals live after-move slot" in log
    cases.append({"map": number, "version": version.decode(), "logical_words": count,
                  "serialized_words": stored, "save_bytes": len(wire),
                  "warm_cold_and_next_movement": True, "closed_save_identical": True})

startups = []
for suffix in ["startup", "moved-startup"]:
    startup = text(f"tmp/special-map-save-{suffix}.log")
    assert "3500 grid words,C pixels=0,SDL pixels=0" in startup
    assert "actual main cold own-save startup,resource attachment,live heap and first SDL submission passed" in startup
    assert "Assertion failed" not in startup
    startups.append(suffix)

regressions = []
for target in ["dynamic-terrain-save-probe", "sdl-dynamic-terrain-save-probe",
               "terrain-snapshot-lifecycle-probe", "map-runtime-chain-probe",
               "sdl-host-path-probe", "sdl-live-restore-probe"]:
    result = text(f"tmp/special-map-save-regression-{target}.log")
    assert "Assertion failed" not in result
    summary = result.splitlines()[-1]
    assert "passed" in summary or "0 failures" in summary
    regressions.append({"target": target, "summary": summary})
for path in ["tmp/special-map-save-final-build.log", "tmp/special-map-save-release-build.log"]:
    result = text(path)
    assert "Built target npe-sdl" in result and "warning:" not in result and "error:" not in result
assert "map140 save=-1" in text("tmp/special-map-save-first-check.log")
assert "map140 cold pixels=15204" in text("tmp/special-map-save-second-check.log")
sources = ["tmp/tianming-v13-map-continuation.npe", "npe-sdl/save.c", "npe-sdl/save_terrain.inc",
           "npe-sdl/rpg_runtime.inc", "npe-sdl/world.c", "npe-sdl/npe.h",
           "npe-sdl/tests/sdl_map_continuation_probe.c", "npe-sdl/tests/sdl_dynamic_terrain_save_probe.c",
           "npe-sdl/tests/verify_special_map_save.py", "npe-sdl/build/npe-sdl.exe"]
report = {"passed": True, "scope": "Own save: actual Tianming exit paths, wrapped viewport, retained map buffer, warm/cold/closed restoration, next movement and actual SDL main startup",
          "cases": cases, "actual_main_startups": startups,
          "malformed_v24_transaction_checks": 4, "regressions": regressions,
          "source_sha256": {p: hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in sources},
          "limitations": ["Pixel reference is the engine's own pre-save frame, not original LCD.",
                          "Only one next-movement input per map is checked.",
                          "This restores the fixed map-buffer allocation, not the entire guest RAM or heap layout.",
                          "Other event/resource state and original PCM remain incomplete."],
          "overall_estimate_percent": 80, "goal_complete": False}
Path("tmp/special-map-save-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf8")
print(json.dumps({"passed": True, "cases": cases, "startups": len(startups),
                  "malformed_checks": 4, "regressions": len(regressions)}))

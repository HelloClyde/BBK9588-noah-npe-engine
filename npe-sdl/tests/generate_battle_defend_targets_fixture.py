"""Add native defense execution samples without replacing old references."""
import json
from pathlib import Path

rows = json.loads(Path("tmp/native-battle-defend-target-execution-all.json").read_text())
old = json.loads(Path("tmp/native-battle-target-execution-all.json").read_text())
assert len(rows) == len(old) == 6
lines = ["/* Native target resolver state during the next defender's action. */",
         "#ifndef BATTLE_DEFEND_TARGETS_NATIVE_H", "#define BATTLE_DEFEND_TARGETS_NATIVE_H",
         "static const uint8_t native_defend_execution[][3]={"]
for row, prior in zip(rows, old):
    assert "error" not in row and row["config"] == prior["config"]
    assert row["events"] == prior["events"]
    assert [s for s in row["execution"] if s["execution"] == 1] == prior["execution"]
    defender = 3 if row["config"]["side"] else 2
    samples = [s for s in row["execution"] if s["execution"] == defender and s["actor"] == 0]
    assert samples and all(s["targets"] == samples[0]["targets"] for s in samples)
    lines.append("{" + ",".join(map(str, samples[0]["targets"])) + "},")
lines += ["};", "#endif", ""]
Path("npe-sdl/tests/battle_defend_targets_native.h").write_text("\n".join(lines), encoding="utf-8")
print("6 original defender execution arrays; old six references unchanged")

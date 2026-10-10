from pathlib import Path
import json

rows = json.loads(Path("tmp/native-defeat-animation.json").read_text(encoding="utf-8"))
assert len(rows) == 2
lines = ["#ifndef DEFEAT_ANIMATION_NATIVE_H", "#define DEFEAT_ANIMATION_NATIVE_H",
         '#include "negative_hp_native.h"',
         "typedef struct { const char *name; unsigned party,character,scripted,frames,seed,party_actions,fast,enemy,final_actor,direct,icon; unsigned actions[3]; NegativeHpRecord before[6],after[6]; } DefeatAnimationCase;",
         "static const DefeatAnimationCase defeat_animation_cases[]={"]

def records(rs):
    result = []
    for r in rs:
        values = [r[k] for k in ["hp", "maximum", "qi", "mental", "attack", "defense", "level", "experience", "attackTimer", "defenseTimer"]]
        status = [v if v < 32768 else v - 65536 for v in r["status"]]
        result.append("{" + ",".join(map(str, values)) + ",{" + ",".join(map(str, status)) + "}}")
    return ",".join(result)

for row in rows:
    c, e = row["config"], row["events"]
    frames = json.loads(Path("tmp/native-" + c["name"] + "-commits.json").read_text(encoding="utf-8"))
    assert len(frames) == 13 and e[-1]["mode"] == 7
    assert c["party"] == 1 and len(e[0]["records"]) == 4
    lines.append('{"%s",1,%u,%u,13,4,1,0,1,0,1,2,{5,0,0},{%s},{%s}},' %
                 (c["name"], c["character"], c["scripted"], records(e[0]["records"]), records(e[-1]["records"])))
lines += ["};", "#endif", ""]
Path("npe-sdl/tests/defeat_animation_native.h").write_text("\n".join(lines), encoding="utf-8", newline="\n")

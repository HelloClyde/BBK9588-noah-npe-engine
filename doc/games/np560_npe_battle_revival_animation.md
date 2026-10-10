# 战斗复活仙术的原厂动画

更新：2026-10-06。生产程序使用 C11 + SDL2，角色姿态和施法动画直接读取当前 NPE。原厂运行时只用于隔离参考采集。

## 已完成范围

接入复活类型 16～19 的施法、原厂“复活”文字、归位和回合刷新。独立原厂输入覆盖《天降风云》原仙术 1“回魂咒”的类型 19；其名称、费用和目标标记均来自当前 NPE，生产代码没有技能或角色编号特判。

四组参考共 **7 次实际行动、108 个全体记录阶段、四张最终 SDL 整屏、90 次 C／实际 SDL 显示提交**，全部零差异。本类完整像素去重 51 张；与已有动画合并为 **1447 次提交、去重 522 张**。静态 334 张另计，去重数量不直接相加。

| 原厂输入夹具 | 当前 NPE 角色资源 | 原始目标体力 | 施法组 | 提交次数 |
| --- | --- | --- | --- | --- |
| 复活中间队员 | 第一角色 | 0 | 3 | 23 |
| 复活负体力中间队员 | 第二角色 | −1（字值 65535） | 6 | 23 |
| 复活末位队员 | 第三角色 | 0 | 9 | 23 |
| 仅施法者能行动，复活中间队员 | 第二角色 | 0，另一队员仍倒地 | 6 | 21 |

每组通过原厂仙术菜单和战斗目标箭头选术、选人。前三组另一名存活队员排队防御；末组仅一次复活行动后直接结束回合。每个操作阶段核对三名队员和三个敌人的完整体／气、属性、经验、四状态及计时字；C 与实际 SDL 逐次比较完整 240×120 像素，包括重复底图，最后比较实际 SDL 整屏。

复活目标体力提交到上限 30000；施法者气 500→420。已倒下队员本轮不补做行动。末组未被选中的队员保持倒地，下一轮仍不能收集指令。

## 原厂顺序与修复

1. `39:5D76..605C` 先显示施法和飘字，再进入数值服务。C 在原有体／气提交之前通知显示回调，保留既有复活计算、状态和行动标志规则。
2. 施法组取当前角色记录字节 85／86；自方仙术沿 `2F:D124` 清除第二特效组，不把技能特效自行叠加到友方。所有帧数、尺寸和像素读取当前 NPE。
3. `30:1850..187C` 对复活类型反转有符号体力门槛：只给显示时仍倒下的目标绘制文字。`30:24F0..2522` 取原厂“复活”字符串；字形来自既有原厂 12×12 字库，位于队员 `(x+10,y+4)`，三次各上移 7。
4. `32:EE48..EE6E` 只有排队的群体／自方两个标记同时为 1 才跳过施法者准备。此前把所有自方仙术都跳过的实现已修正。单体自方仙术清除施法者包含端点的 32×40 矩形并重画，保留原厂对相邻倒地角色边界的擦除；没有使用坐标或角色补丁。
5. 飘字期间保留倒地姿态，提交体力后由下一行动或回合刷新恢复正常姿态。只剩施法者的夹具也核对了直接进入回合刷新、重新显示仙术图标的完整顺序。

## 夹具与验证边界

这些是隔离 RAM 夹具，三名队员复用所选角色的资源，属性由第一角色诊断记录提供。替换姓名、八个姿态和四个动画 ID 时均读取当前 NPE；采集器在原厂入战后核对人数、姿态和施法组。倒地体力在入战前写入地图记录，随后再设置战斗诊断属性，避免拿陈旧的存活姿态缓存作为参考。早期不正确的缓存采样已被重新采集，不计入验收。

原 NPE／PRG 与连续剧情存档未改，存档格式仍为 NPESAV15。既有六组角色仙术的 164 次提交继续通过。正常属性完整通关和 SDL 从头到尾实玩已按用户要求取消，本项不重新安排。

本轮不证明混合角色队伍、连续重复复活、群体复活或类型 16～18 的独立动画。后者的数值已有原指令和强制分派证据，不能把类型 19 的画面样本扩大为全部类型的逐帧证据。其他状态／攻防文字、道具、胜利／离场动画、声音、真实等待单位、随机启动状态及其他 NPE 的完整适配仍待完成；SDL 的 15／5／30 毫秒等待仍是临时换算。

## 复现与证据

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_revival_animation.mjs
python npe-sdl/tests/generate_revival_animation_native.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target sdl-revival-animation-probe -j 4
npe-sdl/build/sdl-revival-animation-probe.exe tmp/tjfy-world-probe.npe --all-frames
python tmp/count-revival-animation.py
```

原厂参考：`tmp/native-revival-animation.json`、`tmp/native-revival-animation-*-commits.json` 及对应 `*-commit-*.idx`／操作阶段 `.idx`。生成器和参考头：`npe-sdl/tests/generate_revival_animation_native.py`、`revival_animation_native.h`；实际 C／SDL 探针：`sdl_revival_animation_probe.c`。源证据：`tmp/revival-animation-source.asm`、`tmp/party-spell-floating-clean.asm` 及既有复活提交反汇编。

全量构建、ctest 5/5、本项及 9 项相关回归通过。日志：`tmp/revival-animation-all-build.log`、`tmp/revival-animation-ctest.log`、`tmp/revival-animation-check.log`；计数：`tmp/revival-animation-counts.json`。相关回归日志：`tmp/revival-animation-regression-*.log`、`tmp/revival-regression-party-spell.log`。

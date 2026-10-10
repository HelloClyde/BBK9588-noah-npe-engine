# 战斗投掷道具的原厂动画

更新：2026-10-06。生产程序为纯 C11 + SDL2，直接读取当前 NPE；原厂运行时用于离线参考采集。

## 已完成范围

投掷已接入角色对敌施法动画、敌方数字／状态文字飘字、归位、击倒清除与回合刷新。六组原厂菜单输入的 **18 次实际行动、252 个全体记录阶段、六张最终 SDL 整屏、155 次 C／实际 SDL 提交**均零差异。本类按完整 28800 字节像素去重 75 张；与已有动画合并为 **1798 次提交、去重 619 张**，静态 334 张另计。

| 当前 NPE 道具夹具 | 角色资源／敌槽 | 原厂效果与显示 | 提交次数 |
| --- | --- | --- | --- |
| 飞刀，类型 31 | 第一角色／第一敌槽 | −40，体 20000→19960 | 25 |
| 毒飞刀，类型 31 | 第二角色／第三敌槽 | −60，体 20000→19940；没有附加中毒 | 25 |
| 五毒散，类型 2，正毒状态 10 | 第三角色／中间敌槽 | “免疫”，毒状态 10→5，体力不变 | 25 |
| 五毒散，类型 2，毒状态 0 | 第一角色／第一敌槽 | “毒”，回合末另显示 −198；体 20000→19802，毒状态 −5→−4 | 30 |
| 长剑，类型 30 | 第二角色／中间敌槽 | 有角色动作与归位，没有伤害或数字 | 25 |
| 飞刀，目标体力 20 | 第三角色／第三敌槽 | 显示 −40，体力归零，后续动作前清除倒地敌人 | 25 |

ID 159／160／68／27 只用于测试定位。生产分派只使用当前道具类型、效果、群体和阵营字段；角色动画组和姿态也读取当前 NPE。每个场景三名队员复用同一个所选角色资源，另外两人排队防御，敌人负混乱用于隔离随机行动。

原厂与实际 SDL 均经过旗帜菜单、道具、投掷、背包、选敌和队列执行。核对每个阶段全体六条记录的体／气、属性、等级、经验、四状态及计时字，检查背包在选人前 1→0、执行不再次扣除，以及实际命令类型、道具和目标。原厂提交包含重复底图，C 和实际 SDL 均逐次比较完整画面，没有丢弃重复帧。

## 原厂规则

1. 使用和投掷共用原行动 2：`2F:D296..D532`。投掷选择角色记录 +85 的对敌动画组，使用取 +86 的自方组；本次三个角色的对敌组为 2／5／8，生产代码从文件读取。第二特效组保持零。
2. `2D:4584..4644` 将类型 31 的两个执行类型字节转为 1，把道具攻击字 `7414` 写入体力效果 `7926`，并把增攻效果 `792A` 清零。因此显示体力伤害，不能显示“攻”，也不能根据“毒飞刀”的名称添加中毒。
3. 类型 2 按四个状态字处理原厂“毒／免疫”等文字，回合末中毒继续走已有独立毒伤显示。长剑类型 30 不擅自解释成攻击或数字；仍保留原厂角色动作和三次恢复底图。
4. 使用道具与投掷分别保留各自命令种类，共用显示描述和动作事件。投掷显示回调位于实际效果提交之前，类型转换只改变显示描述，不修改 NPE 或存档。
5. 选中敌人移动、三次飘字、恢复原位与下一行动前刷新沿已有原厂通用流程。第三敌槽归位保留相邻敌人边界重绘，击倒后按新体力清除缓存。

## 验证与剩余范围

全量构建、ctest 5/5、本项及 12 项相关回归通过：使用道具、复活仙术、角色仙术、敌方状态与中毒动画，道具 SDL 流程、状态道具 C／SDL、复合效果 SDL、预留、敌方行动、离场状态及空列表道具。原 NPE／PRG SHA256 和 NPESAV15 不变。

本次独立画面证据覆盖当前 NPE 的单体类型 2／30／31 投掷。其它 NPE 的群体／复合／负效果、道具 255、非连续队伍、混合角色及连续投掷仍需独立对照。胜利／离场动画、声音、等待真实单位、随机启动状态和其它 NPE 完整机制继续实现。SDL 的 15／5／30 毫秒等待仍是临时换算，画面匹配不证明时长复刻。

正常属性完整通关和 SDL 从头到尾实玩已按用户要求取消；保留既有角色属性诊断主线到结局的结果。

## 复现

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_throw_animation.mjs
python npe-sdl/tests/generate_throw_animation_native.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target sdl-throw-animation-probe -j 4
npe-sdl/build/sdl-throw-animation-probe.exe tmp/tjfy-world-probe.npe --all-frames
python tmp/count-throw-animation.py
```

参考：`tmp/native-throw-animation.json`、各场景 `*-commits.json`、`*-commit-*.idx` 和操作阶段 `.idx`。采集器、生成器、参考头和探针位于 `npe-sdl/tests/reference_throw_animation.mjs`、`generate_throw_animation_native.py`、`throw_animation_native.h`、`sdl_throw_animation_probe.c`。

源证据：`tmp/battle-throw-animation-source.asm`、`tmp/battle-item-animation-source.asm`、`tmp/party-spell-floating-clean.asm`。日志：`tmp/throw-animation-check.log`、`tmp/throw-animation-all-build.log`、`tmp/throw-animation-ctest.log`、`tmp/throw-animation-regression-*.log`；完整像素计数：`tmp/throw-animation-counts.json`。

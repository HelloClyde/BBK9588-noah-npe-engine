# 角色仙术动画：原厂逐帧对照

更新于 2026-10-06。纯 C／SDL 已接入角色类型 1／2 仙术的施法动画、技能特效、飘字、归位和回合刷新。六组原厂完整输入共 **18 次实际行动、252 个全体记录阶段、六张最终 SDL 整屏、164 次 C／实际 SDL 提交**全部零差异。本类按完整 28800 字节去重 82 张；与既有动画合并为 **1357 次提交、去重 475 张**。静态 334 张另计。

## 原厂输入与资源覆盖

| 场景 | 当前 NPE 角色资源 | 原厂显示 | 提交数 |
| --- | --- | --- | --- |
| 剑气决，单敌 | 第一角色 | 施法组 2、特效组 48、-90 | 29 |
| 回峰落叶剑，三敌 | 第一角色 | 施法组 2、特效组 63、各 -130 | 29 |
| 治疗术，第三队员 | 第一角色 | 施法组 3、+100 | 25 |
| 佛光普照，三队员 | 第二角色 | 施法组 6、各 +200 | 25 |
| 封魔咒，第三敌槽 | 第三角色 | 施法组 8、特效组 42、“封” | 31 |
| 唯心唯灵，第三队员 | 第二角色 | 施法组 6、“全解” | 25 |

每组由原厂仙术图标／个人仙术列表选术，再通过目标页选人或选择整组；其余两人从旗帜菜单排队防御。敌人保持负混乱以跳过行动，避免把敌人随机目标混入本类参考。C 与实际 SDL 均比较每一次提交，包括重复底图；六个完整记录在每个操作阶段核对体、气、攻、防、等级、经验、状态及计时字，最终再核对 SDL 整屏。

这些是隔离 RAM 夹具。第二、第三角色夹具保留第一角色诊断属性，只从当前 NPE 复制所选角色的姓名、姿态图像和动画组；组内三人复用同一种角色资源。采集器在入战后断言三人在队、八个姿态与两个施法组均等于当前 NPE 的记录，防止把未入队记录或错误偏移作为参考。早期失败的扩展夹具已重新采集，未计入验收。本项不代表不同角色混合组队或自然属性完整通关。

## 还原规则

1. `npe_world_battle_cast_magic` 在执行阶段准备显示数据，通知 BEGIN／DAMAGE；原有体、气和状态结算继续使用 NPE 原始效果字段。新增 `NPE_BATTLE_ANIMATION_PARTY_SPELL=11` 只属于 C 显示回调，不是 NPE 指令编码。
2. 施法组读取当前角色第 12 段记录的字节 85／86，分别用于敌方／友方目标。`2F:CF6A..CF88` 和 `2F:D0FA..D118` 从战斗字节 `64D9/64DA` 取组号。原厂装载路径 `2E:83E8..84FC` 对应地图 `6A4D..6A50` 四个动画 ID；姿态图像来自 NPE 字节 19～34。生产代码不按角色 ID、名字或技能 ID区分资源。
3. `39:5E2A..5EBA` 在施法者原坐标播放角色组，非零像素覆盖当前底图。每帧提交动画和恢复底图，最后保留一次重复恢复提交。角色没有被人为隐藏或移动。
4. 敌方目标的技能特效由 NPE `effect_type` 指定，单体位于目标原坐标，群体为 `(20,10)`；友方仙术在 `2F:D124` 把第二特效组设为零，使用角色自身的友方施法组。帧数、尺寸和像素均由当前 NPE 动画表读取。
5. 敌方受术记录在 `32:E552..E7BC` 左移 2；先清除原位置的含端点 40×50 区域，再透明绘制。归位清除位移后的矩形并不透明重画。`31:849C..869A` 还会重画所选敌槽的前一个实体，修复第三敌槽归位清除相邻敌人两个边界像素的问题，未使用坐标补丁。
6. 友方选人显示使用该记录已经排队的防御姿态；归位仍按各记录的防御、低体力或正常姿态选择。施法者仅在自方群体仙术时跳过准备，单体自方仙术保留原厂矩形清除，详见后续[复活显示证据](np560_npe_battle_revival_animation.md)。选中记录归位时从 x+2 开始清除，再在原坐标绘制，见[道具动画的相邻倒地像素证据](np560_npe_battle_item_animation.md)。保持同回合已有画面缓存。
7. 数值飘字使用原厂 5×10 数字：敌方原坐标 `(x+10,y+6)`、友方 `(x+10,y+4)`，每次上移 7，共三次。治疗保留正号，伤害保留负号。
8. `30:1880..2522` 使用效果类型选择文字。类型 2 按 NPE 状态字选择原厂“毒／眠／乱／封”“解毒／解眠／解乱／解封”“免疫／全解”；显示决策使用状态效果相加后的有符号字。类型 1 的攻／防文字选择也沿原分支。字形来自已提取的原厂 12×12 字库，透明绘制并同样上移；本轮独立逐帧证据覆盖“封”和“全解”，其他标签分支仍需独立画面对照。

## 复现与证据

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_party_spell_animation.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_party_spell_animation.mjs --extra
python npe-sdl/tests/generate_party_spell_animation_native.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target sdl-party-spell-animation-probe -j 4
npe-sdl/build/sdl-party-spell-animation-probe.exe tmp/tjfy-world-probe.npe --all-frames
python tmp/count-party-spell.py --extra
```

原厂参考：`tmp/native-party-spell-animation.json`、`tmp/native-party-spell-extra.json` 及对应 `*-commit-*.idx`／`*-commits.json`。测试生成器和生成头：`npe-sdl/tests/generate_party_spell_animation_native.py`、`party_spell_animation_native.h`；实际 SDL 输入与纯 C 帧探针：`sdl_party_spell_animation_probe.c`。源证据：`tmp/party-spell-clean-source.asm`、`tmp/party-spell-floating-clean.asm` 及已有姿态／归位反汇编。通过日志：`tmp/party-spell-check.log`；完整像素合并计数：`tmp/party-spell-counts.json`。

全量构建、ctest 5/5、本项及 20 项相关回归均通过。日志：`tmp/party-spell-all-build.log`、`tmp/party-spell-ctest.log`、`tmp/party-spell-check.log`、`tmp/party-spell-regression-*.log`。

全量构建、ctest 5/5、本项及 20 项相关回归均通过。日志：`tmp/party-spell-all-build.log`、`tmp/party-spell-ctest.log`、`tmp/party-spell-check.log`、`tmp/party-spell-regression-*.log`。

当前 NPE／PRG 文件未修改，保存格式仍为 NPESAV15。本轮不增加连续主线剧情节点，也不安排用户已取消的正常属性完整通关或 SDL 全程实玩。

## 尚待还原

复活仙术显示、其他状态文字及增减属性飘字的独立原厂输入；混合角色、其他人数／目标、非连续在队槽、死亡重选、连续施法及其他绘制模式／移动插值；道具、胜利和离场动画；声音、固件等待真实单位、开机随机状态及其他 NPE 完整机制。当前 SDL 等待 15／5／30 毫秒仍为临时换算，不能用帧匹配声称播放时长已复刻。

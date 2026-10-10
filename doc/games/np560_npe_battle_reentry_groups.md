# 连续入战后的实际群体道具／仙术两轮

2026-10-10。生产为独立C11＋SDL2；原程序仅生成离线参考。名称、图像、群体／阵营标志、费用与效果读取当前NPE，没有加入游戏、人物或道具编号特例。

## 实际资源与输入

扫描18份NPE的176项道具表，共找到103份群体道具记录，保留路径、哈希及原字节于`tmp/reentry-group-item-candidates.json`。本批使用《天降风云》原记录64“药王帖”：类型1、体力300、使用标志3、群体1、阵营1；仙术9为“佛光普照”。没有改动资源或缓存标志来制造群体道具。

沿既有实际main路径：单人逃跑，换为三人队伍，实际移动再次进入两个敌人的战斗。完成第二场入战后，只播种一次允许的诊断输入：三人地图等级99、技能前缀均为9；保留已装载的不同战斗技能前缀。三人体力50／60／70，上限200，气100／120，速度999／998／997；敌人体力30000、攻击和速度1、无技能和状态；库存仅保留药王帖两份，共享技能和道具缓存从零开始。其它完整人物记录保留。

| 回合 | 第一名 | 第二名 | 第三名 |
| --- | --- | --- | --- |
| 第一轮 | 药王帖（群体） | 防御 | 佛光普照（群体，末位确认执行整轮） |
| 第二轮 | 佛光普照（群体） | 防御 | 药王帖（群体，末位确认执行整轮） |

药王帖在步骤7确认时由2降为1，步骤23确认时由1降为0。两轮结束气为100／100／80及80／100／80；体力包含后续敌人伤害，分别为176／200／174及171／200／175。C只设置原采样的动作前输入，后续使用真实菜单按键，没有注入结果、参考像素或预排命令。

## 611像素差异及生产修复

第一轮动作13的提交0～31一致；提交32在第三队员区域出现611像素差异，C与实际SDL差异相同。最初按“施法后姿态恢复”暂记；反汇编后确定是后续敌方攻击的受击准备：原画面保留待机姿态，C错误覆盖为受击姿态。

原`32:EE04..EE6E`逐物理队员读取`64D3`、`64D4`，两者均为1时直接跳过该队员准备绘制。条件使用被处理队员本人的保留指令标志，既适用于执行者，也适用于接收者；不只检查当前施法者的技能。原恢复阶段`31:7EC2..7F40`、`80F0..828C`继续按选中记录恢复。

`npe_render_battle_party_spell_stage`改为按各队员的保留群体／阵营标志判断；`render_battle_enemy_party_stage`补齐相同条件。因此群体仙术／道具队员随后受敌方攻击或仙术时，准备画面保持原顺序。完整摘录为`tmp/reentry-groups-render-source.asm`，修复前源码及失败画面保存在`tmp/reentry-groups-before/`，失败日志为`tmp/reentry-groups-first-check.log`。原参考、夹具及预期画面在生产修复期间未改变。

## 验证

- 24个操作步骤、146次第二场动作提交；包含初次入战、逃跑及再入战，共178次原版／C索引画面／实际SDL读回零差异。
- 等待点的3份地图及6份战斗完整148字节人物记录，共216份；再入战保留、输入及完成三个阶段另27份完整记录一致。
- 24份菜单五字节、24份290字节库存／技能／道具缓存快照一致；模式、当前行动者及随机状态也在等待点核对。
- 独立比较器重新读取原文件与C输出，确认上述全部画面、完整记录、菜单及缓存相同。正式日志`tmp/reentry-groups-final.log`，报告`tmp/battle-reentry-groups-report.json`。
- 主程序及受影响目标构建成功，无编译警告。四项必要短回归通过：既有混合两轮226次画面；敌方9条221次；角色仙术标签6条198次；道具标签4条92次。

角色仙术标签旧SDL探针未播种原参考明确写入的地图69DE／69DF／69E0；只修改战斗宿主字段，导致列表入口不能选择参考技能。其纯C动作全部画面仍一致。本批补齐原诊断地图输入，预期和断言不变；原失败日志`tmp/reentry-groups-party-label-before.log`保留。之后6条实际SDL路径通过。构建日志为`tmp/reentry-groups-final-build.log`及`tmp/reentry-groups-label-final-build.log`。

## 重复入口

```powershell
node npe-sdl/tests/reference_battle_selection_entry.mjs --reentry-groups
python npe-sdl/tests/generate_enemy_record_entry_fixture.py --reentry-groups
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-battle-reentry-groups-probe
./npe-sdl/build/sdl-battle-reentry-groups-probe.exe tmp/tjfy-world-probe.npe *> tmp/reentry-groups-final.log
python npe-sdl/tests/verify_battle_reentry_attack.py --groups
```

独立比较器另读取四份`tmp/reentry-groups-regression-<target>.log`及构建日志；对应目标为`sdl-battle-reentry-mixed-probe`、`sdl-enemy-prepared-records-probe`、`sdl-party-prepared-spell-labels-probe`、`sdl-party-prepared-item-labels-probe`。除混合两轮目标只传NPE路径，其余加`--all-frames`。

## 剩余范围

本批覆盖表中实际群体指令、末位确认、完整两轮和随后敌方普通攻击；没有把它计为全部战斗行为完成。完整人物记录采样为操作等待点，不是逐指令或每次LCD提交。更多队伍／特殊类型／混合动作、其它NPE后续、原PCM及新增状态恢复继续。正常属性完整通关、SDL全程手动实玩和原机存档互通按用户要求排除。整体仍粗估约80%，模块状态以[根计划](../../plan.md)为准。

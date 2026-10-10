# 跨NPE连续入口与行走对白初始化

> 2026-10-10后续更正：零距离移动的错误拒绝已修复。原DIV在除数0时设置V，原服务跳过零步循环并继续后继；旧WASM execution_fault不能证明原机陷阱。xab实际新游戏完成12个入口／743次SDL提交、进入地图13。详见[NPC零距离移动](np560_npe_npc_move_zero.md)。以下保留当轮历史结果。

更新：2026-10-10。修复一处实际游戏错误：行走触发事件漏设原重新初始化标志。生产代码仍为纯C11＋SDL2，直接读取当前NPE，未增加游戏名称、剧情结果或资源特例。整体仍粗估约80%，完整引擎未完成。

## 已修复的实际错误

从《我的世界》的原标题确认新游戏，先与NPC35交谈，再由(6,10)向下触发行走事件：当前NPE地图1的触发表要求模式2／NPC4。旧C进入模式2时保留`map_init_pending=0`，此前对白已经关闭，因而跳过新的NPC装载，最终报`mode-2 followup is unsupported`。

原碰撞服务source `3A:073A..0742`在写入模式／事件后，无条件写`52:7AB4=1`。原NPC控制器`3B:71BE..7212`依赖该标志进入新的NPC装载。生产`world_step_service`现在在触发记录写入后按此设为1；没有移除对白准备检查，也没有直接指定NPC4的对白、后继或目的地图。

修复后的相同普通属性路径完成新的对白／剧情，进入地图4，随后累计完成9个不同入口、1915次实际SDL提交。该单独修复检查未提高人物属性，也未注入剧情进度、NPC状态或条件。

## 原指令及必要回归

`reference_move_trigger_entry.py`执行未修改的原runtime `1D:0000`碰撞函数，以当前NPE的实际触发记录构造隔离RAM；其它碰撞项置空。原代码与数学服务在执行前逐字节核对，调用返回、栈还原且无缺失ROM、未知指令或非法控制流。

| 原记录 | 初始初始化标志 | 原结果 | 生产C结果 |
| --- | ---: | --- | --- |
| 《三分天下》地图1，模式5／事件1 | 0 | 标志1，模式5／事件1 | 一致 |
| 同一记录 | 0xA5A5 | 标志1，模式5／事件1 | 一致 |
| 《我的世界》地图1，模式2／NPC4 | 0 | 标志1，模式2／事件4 | 一致 |
| 同一记录 | 0xA5A5 | 标志1，模式2／事件4 | 一致 |

独立编译修复前的`world.c`，这4组均失败：标志保留了原值；当前4组全部通过。旧源码、失败结果及原参考保持，未替换参考或修改生产源码来构建旧版本。

三个实际受影响短回归通过：行走事件及遇敌／追击后继矩阵（直接C与实际main各71次原画面提交）、付款后的NPC重新装载／单次扣费／乞丐后继、文本控制与220次故事重入。生产主程序、新探针和上述目标构建无警告。

这些4组参考核对原碰撞状态，不是其它游戏的原完整LCD、完整文件装载或整条剧情的新采样。

## 连续运行入口

`sdl_cross_npe_followup_probe.c`直接包含生产`main.c`，以普通启动参数运行原标题、新游戏、移动、交谈和显示调度。与上一批每份只走一个入口相比，本批每份尝试12个不同入口；规划范围没有新入口时停止搜索。

- 路径搜索只在World副本中进行，关闭副本遇敌；逐字节检查真实World未改变。实际主循环保留遇敌和巡走，遇敌或NPC移动打断路线后重新规划。
- 实际控制器处理NPC对白、故事、选择、商店返回和战斗。普通战斗使用逃跑输入，剧情战斗使用实际攻击；没有存活敌人时选择防御继续实际结算，不注入胜利。
- 用户已授权诊断人物属性。本批最终集合在战斗动作前提高活动队员的体／气及攻防，并在后续动作前重新补满，用于缩短战斗检查。敌人、阵型、目标、指令、NPC状态、剧情、条件和后继均由当前资源及生产服务决定。该选项只在探针中，生产新游戏属性不变。
- 主机种子固定1，等待使用虚拟时钟，音频设备关闭。每次实际SDL提交逐像素比较C索引缓冲与renderer的ARGB读回；不是原完整LCD参考。

## 结果

| NPE | 已完成不同入口 | SDL提交 | 检查停止位置 |
| --- | ---: | ---: | --- |
| 三分天下 | 12 | 1116 | 达到本次12入口限制 |
| 天明V1.0 | 12 | 1576 | 达到限制 |
| 天明V1.1 | 12 | 1561 | 达到限制 |
| 天明V1.2 | 12 | 1566 | 达到限制 |
| 天明V1.3 | 12 | 1728 | 达到限制 |
| 天机之江湖启示录 | 12 | 1249 | 达到限制 |
| 天降风云V1.2 | 12 | 825 | 达到限制 |
| 天降风云V1.3 | 12 | 825 | 达到限制 |
| 天降风云xab | 9 | 532 | 第10个入口的零距离NPC移动失败 |
| 异域传说1 | 8 | 507 | 本次规划范围未再找到新入口 |
| 异域传说2 | 8 | 507 | 同上 |
| 异域传说3 | 8 | 507 | 同上 |
| 我的世界 | 9 | 1511 | 同上 |
| 新仙剑后传 | 12 | 1555 | 达到限制 |
| 新月奇缘 | 12 | 2578 | 达到限制 |
| 英雄传奇 | 12 | 602 | 达到限制 |
| 诺亚论剑 | 12 | 1004 | 达到限制 |
| 阿诺创世纪 | 12 | 874 | 达到限制 |

17份完成本次短检查，1份保持失败。包括失败前已经完成的9个入口，合计198个入口：127个NPC交谈、21个行走事件、50个自动出口；20623次实际SDL提交均完成C／SDL比较。只统计完成后继的入口，不把规划或被遇敌打断的路线计入完成数。

独立检查直接读取原NPE人物记录核对新游戏坐标，核对实际触发与规划入口、去重、后继顺序、SDL提交序号、源文件指纹及完成路径的运行时NPE指纹、原碰撞状态和旧版本失败。总体`all_cross_game_checks_passed=false`，未隐藏失败或降低xab入口上限。

### xab零距离移动

实际流程在地图1、进度0，经NPC2→金钱事件2→模式18／事件1进入NPC移动。NPC23位于(21,21)，事件目标也是(21,21)，步数为0。

既有未修改原服务参考`native-npc-move-resident-zero-fault.json`已观察同类零步数在source `3C:8DF4`除零并返回`execution_fault`。这支持当前拒绝零距离事件的原因，但**不是xab整条原流程的动态对照**。该游戏为什么在本路径进入此状态、原机在此入口如何处理仍需核对；本批没有擅自将移动视为成功、修改坐标或应用后继。

## 诊断过程与限制

首轮普通属性检查发现《我的世界》的真实初始化错误。另有正常战斗失败；xab最初只发送攻击，面对没有存活敌人的阵型无法选中目标，是探针策略不足。改用实际防御结算后继续到上述零距离移动。提高一次属性仍不足以通过《新月奇缘》的长战斗，最终探针按已授权的诊断方式在每次动作前补满人物属性，生产规则未改。

最终失败尾部额外记录NPC移动字段，最后一条记录没有提交计数。统计器已改为从全部原始记录取累计计数，恢复该失败路径的532次提交；原始记录与通过／失败结果未改。

本批去重只覆盖同图／同进度的不同NPC、行走事件和出口，没有穷举同一NPC的重复条件分支。规划不能穿过已访问的触发格，也未覆盖零宽度地图规划；“未再找到新入口”不等于游戏完成。仍缺各游戏全部后续、原完整LCD、原声及真实时序。正常属性完整通关与SDL全程手动实玩继续按用户要求排除。

## 文件与重查

- 原碰撞：`tests/reference_move_trigger_entry.py`、`tmp/move-trigger-entry-native/report.json`。
- C状态检查：`tests/move_trigger_entry_probe.c`、`tmp/move-trigger-entry-c-checks.json`及`tmp/move-trigger-entry-before-checks.json`。
- 连续入口：`tests/run_cross_npe_followups.py`、`tmp/cross-npe-followup/final2/report.json`。
- 独立汇总：`tests/verify_cross_npe_followups.py`、`tmp/cross-npe-followup/verified-report.json`。
- 首轮及中间结果保留在`first/`、`trigger-fix/`、`xab-check/`与`final/`，最终实际运行记录在`final2/`。

```powershell
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
$env:PYTHONIOENCODING = 'utf-8'
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl move-trigger-entry-probe sdl-cross-npe-followup-probe -j4
D:/ProgramData/miniconda3/python.exe npe-sdl/tests/reference_move_trigger_entry.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --directory 'D:/Downloads/NP560plus游戏全集'
Get-ChildItem -LiteralPath tmp/move-trigger-entry-native -Filter '*.path' | ForEach-Object {
    & npe-sdl/build/move-trigger-entry-probe.exe $_.FullName ([System.IO.Path]::ChangeExtension($_.FullName, '.txt'))
}
D:/ProgramData/miniconda3/python.exe npe-sdl/tests/run_cross_npe_followups.py --directory 'D:/Downloads/NP560plus游戏全集' --output tmp/cross-npe-followup/final2 --limit 12 --boost
```

连续检查当前按实际失败返回非零，不意味着已完成的17份全部失败。独立汇总还读取保存的旧版本、普通属性修复路径、4组C状态及必要回归结果；它明确保持xab失败。原机存档互通不在本项目验收范围。

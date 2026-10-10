# 原版战斗字母键分派

更新：2026-10-10。本批修复生产SDL输入，使用既有原画面／状态参考及新增原分派函数诊断。整体仍粗估约80%，原声及完整跨NPE兼容未完成。

## 原规则及实现差异

| 字母键（含大写） | 原动作 | 原生键值 |
| --- | --- | --- |
| I | 上 | 0x82 |
| J | 左 | 0x80 |
| K | 确认 | 0x86 |
| L | 右 | 0x81 |
| M | 下 | 0x83 |

原战斗入口物理源码47:5246～54B8，对应运行时2A:5246～54B8。47:53A4／53EA两张跳转表分别处理小写／大写I～M，47:5430处理原生键；它们跳至同一组处理函数。此前SDL主战斗只转换方向键等输入，M还作为打开仙术的宿主快捷键；嵌套仙术／道具／目标列表也缺少部分字母键。

原共享道具列表4B:B5A0～B70C、仙术列表4C:312E～327A亦直接支持两种大小写及对应原生键。调用链49:A734／47:8C18等将原键继续送入列表分派。字母键是引擎既有输入，不依赖《天降风云》的名称或剧情。

## 生产修复

`npe-sdl/main.c`增加限定于原菜单调用者的`native_menu_navigation_key`，接入五处：

1. 主战斗输入，包括行动图标、子菜单及状态页。
2. 普通攻击的多敌人目标选择。
3. 道具的友方／敌方目标选择。
4. 仙术列表及友方／敌方目标选择。
5. 共享道具列表，包含地图／战斗使用者。

M打开仙术的旧分支删除。SDL2在Shift／Caps Lock下仍以小写字母作为键符，因此这些修饰键继续使用原动作。问答通过原有`quiz_sdl_key`读取字母；本批转换未接入问答输入。

仅生产`main.c`有功能修改，NPE、剧情、战斗数值、存档格式和音频实现本批未修改。其它地图／系统菜单输入仍需按各自原分派继续核对。

## 原分派诊断

`reference_battle_letter_keys.py`运行上述三段未改动原代码，并逐字节检查实际CODE映射与PRG相同。下游行动／绘制服务使用记录入口和原键的返回适配器；明确提供阻塞位、列表状态及原键，客机代码自行写入，宿主不写客机RAM，不装载NAND或进入游戏。

共66例通过：战斗26、道具19、仙术21。覆盖15种字母／原生导航键、F4、无关键；战斗还包括84／85无动作及输入阻塞，仙术包括左右键的列表门槛。战斗传入参数和列表读取的原键均保留原值。本诊断证明分派与门槛，不提供新增原LCD或完整游戏／物理时序证据。

第一版诊断使用20段小入口，CODE映射检查拒绝了错误映射；改为已有03段完整PRG映射后，三段实际代码检查通过。数据快照按客机物理小端word解释。这两项均是采集工具修正，未改被测分派代码或期望动作。

原结果：`tmp/battle-letter-native/report.json`，66项结果、PRG／QEMU／代码段哈希、CPU状态与结果字节保留。

## 修复前后与SDL检查

修复前以原字母键执行三组既有探针，实际主循环第一菜单有392像素差异；仙术和道具列表没有执行K确认，阶段检查失败。失败日志保存在`tmp/battle-letter-before-*.log`。

修复后参考帧和参考状态保持，结果如下：

| 路径 | 每轮C／SDL画面对照 | 输入轮次 |
| --- | --- | --- |
| 实际main混合动作两轮 | 226 | 方向键、字母、Shift、Caps Lock |
| 仙术列表／目标／不足与取消 | 33 | 同上 |
| 道具列表／用药／投掷／目标与取消 | 56 | 同上 |
| 实际main多敌人连续攻击 | 125 | 方向键、字母 |

合计1510次C／SDL画面对照及各探针的人物、行动、库存、缓存状态检查通过。这里沿用既有独立原参考，1510不是新增原机帧数；实际main检查含明确诊断输入，不代表正常属性完整通关。

受影响的共享地图道具列表另有6条原路径、18次画面对照通过。问答I～M在无修饰、Shift、Caps Lock及两者同时启用时的20条字面输入检查通过；六条原问答状态路径、10次实际main／SDL画面对照通过。

### 旧问答探针入口修订

该探针将当前V22存档裁切为历史V18时，没有去掉后加的4字节`WTBL`尾标记，生成了不合法旧档；修改前生产代码同样在第100行失败。正确去掉尾标记后，旧版本迁移检查通过。

空槽现在直接进入原新游戏构造器，旧探针仍在`npe_world_restore`钩子准备其诊断输入和开始测量，导致永远等不到结束。将同一原输入和测量起点接至`npe_world_restart_game`，位于选槽清屏之后，六条既有参考全部通过。修订后的探针分别链接修改前／后生产main，均通过相同10次对照；未改原参考、存档代码或生产初始化。

## 重现

在仓库根目录，配置SDL2构建后：

```powershell
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
cmake --build npe-sdl/build --target npe-sdl sdl-battle-reentry-mixed-probe sdl-battle-skill-record-probe sdl-battle-item-record-probe sdl-battle-reentry-two-attacks-probe sdl-map-item-record-probe sdl-quiz-state-probe -j 6
python npe-sdl/tests/reference_battle_letter_keys.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe
$env:NPE_PROBE_BATTLE_KEYS='letters' # 也可为shift／caps；删除变量使用方向键
npe-sdl/build/sdl-battle-reentry-mixed-probe.exe tmp/tjfy-world-probe.npe
npe-sdl/build/sdl-battle-skill-record-probe.exe tmp/tjfy-world-probe.npe
npe-sdl/build/sdl-battle-item-record-probe.exe tmp/tjfy-world-probe.npe
npe-sdl/build/sdl-battle-reentry-two-attacks-probe.exe tmp/tjfy-world-probe.npe
Remove-Item Env:NPE_PROBE_BATTLE_KEYS
npe-sdl/build/sdl-map-item-record-probe.exe tmp/tjfy-world-probe.npe
npe-sdl/build/sdl-quiz-state-probe.exe tmp/tjfy-world-probe.npe
```

四份构建日志无warning／error；主程序已重建。已有本批完整日志时，`python npe-sdl/tests/verify_battle_letter_keys.py`只核对这些结果并汇总到`tmp/battle-letter-report.json`，不会另跑游戏。模块范围以[根计划](../../plan.md)为准。

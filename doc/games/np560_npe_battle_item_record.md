# 战斗道具菜单与执行共享记录（2026-10-08）

生产程序使用 C11＋SDL2，直接装载当前 NPE 的名称、说明、图像和数值。PRG／WASM 仅用于离线对照。本轮接入战斗使用／投掷菜单、选择预扣和逐行动共享142字节缓存；胜利后的掉落／学技共享装载已在[后续记录](np560_npe_battle_postwin_record.md)修正和验证；完整记录别名、原音频和时基仍未完成。

## 原服务与改动

- 原 `38:46F0..46FC` 确认选择时调用运行时 `22:C370`，共用编号 `7A72` 与 `739E..742B` 的142字节道具记录。相同非零编号保留驻留字节，换编号才读取 NPE。
- `38:47E0..4850` 按缓存阵营拒绝错误用法。`38:4930/4A38/4A8C` 把类型／阵营／群体复制到人物命令的 `64D1/64D4/64D3`；类型另有 `64D2` 副本。本轮宿主保存执行所需三字段，没有宣称完整148字节命令已建模。
- 库存扫描只按正的有符号数量选择1～254，不提前解码所有资源。列表按可见名称、选中说明的顺序装载共享记录；窗口内移动保留名称网格，跨页和从目标页返回重新绘制。错误用法提示覆盖保留画面，2000ms后恢复，不额外重读资源。
- 原 `38:4DA0..4DB6` 在目标页前扣数量。F4取消目标页退还预扣，命令字段仍驻留；群体资源自动提交。执行不重复扣数量，也不再用出手时的库存数量拒绝已经排队的道具。
- 空列表首次进入保留旧缓存和说明索引。确认所选 `FFFF` 截成255；取消目标回到空列表时绘制255的图标／说明。沿用已有第255库存词与地图首角色名称前词的有限别名；完整角色字节别名仍待办。
- 道具目标页与仙术目标页的提交顺序不同：原 `38:4F1A..4F46` 依次提交场景、体／气、光标、控制、目标箭头、状态；仙术原 `3A:0EAA..0ECE` 的体／气在控制之后。我方切换三次、敌方两次；无关方向／按键不多提交画面。
- 原 `2D:44AA..44F6` 出手再次调用共享装载，数值／状态取当前缓存，分派使用已保存的命令字段。投掷类型31在 `2D:4584..4644` 把攻击词移到体力效果，并把当前执行类型改为1。宿主渲染按同一规则显示；完整命令副本和执行后类型驻留字节未建模。
- 新 `npe_world_item_view` 和 `npe_world_battle_item_view` 提供只读视图，实际SDL施放动画与浮动数字不重新读取 NPE、不改缓存。复合数值按原 `35:EF88..F198/F6B8..F96C` 的防御／攻击／气／体首个非零词显示，替换旧体力优先和仅正数判断。

源码摘录：`tmp/battle-item-record-source.asm`、`battle-item-record-extra-source.asm`、`battle-item-record-target-source.asm`、`battle-item-record-target-order-source.asm`、`battle-item-record-group-source.asm`。最后一份表明群体字节2等非标准值还有独立的执行分支，尚未完整接入；本轮不宣称所有异常标志通过。

## 原动态与实际 C／SDL 对照

使用原启动和真实战斗快照，每条路径单独运行原 WASM。只改诊断RAM的队伍、库存和缓存，不改原代码、NPE、文件IO或LCD服务。共享缓存诊断名称为“缓存道具”，说明为“旧说明”，其余保留字节填A5；此文本仅在测试夹具中，生产读取真实资源。

| 范围 | 原路径 | 实际对照 |
| --- | --- | --- |
| 菜单、目标、拒绝、预扣／退还 | 同编号取消、我方切换／取消、群体排队、换编号、两物品往返、错误阵营、投掷选敌、空包取消／255目标取消、多物品目标取消、13项跨页返回 | 11条路径，56次C与实际SDL完整240×120提交；同时核对142字节、说明索引、列表位置、库存和已建模命令字段 |
| 逐行动及动画 | 同编号、换编号、缓存标志冲突、连续相同／不同用药、投掷同编号／换编号／标志冲突、连续不同投掷 | 9条短战斗，264次C与实际SDL完整提交；回合末核对142字节、库存和6×11人物／敌人字段 |
| 既有真实资源动画回归 | 8组使用道具、6组投掷 | 分别196／155次C与实际SDL动画提交，312／252个完整记录阶段和8／6张最终画面 |

菜单原外层返回控制／下一角色共75次LCD单列，未计入56次模态对照。菜单每次提交的RAM取原推进批次采样；执行探针直接调用生产排队后端与真实SDL动画，只核对回合末原RAM，不能冒充逐LCD即时原状态或新的完整输入流程。

关键诊断结果：道具46同编号缓存体7／气3，使体50→57、气100→103；换编号重读真实资源使体50→90。确认后把缓存类型改2、群体改1、阵营改0，原执行仍用排队时的单体友方类型1。投掷159同编号攻击17／气3使敌人体1000→983、气100→97；换编号重读后体1000→960。不同投掷159→160的末缓存为160，敌人体923／气97。所有诊断排队后库存均0，执行正常。

原参考和生成器：

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_battle_item_record.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_battle_execution_item_record.mjs
python npe-sdl/tests/generate_battle_item_record_fixture.py
python npe-sdl/tests/generate_battle_execution_item_fixture.py
cmake --build npe-sdl/build --target npe-sdl sdl-battle-item-record-probe sdl-battle-execution-item-record-probe -j 4
& npe-sdl/build/sdl-battle-item-record-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-battle-execution-item-record-probe.exe tmp/tjfy-world-probe.npe
```

数据：`tmp/native-battle-item-record.json`、`native-battle-execution-item-record.json`及对应LCD参考 `.idx`；夹具在 `npe-sdl/tests/battle_item_record_native.h`、`battle_execution_item_native.h`。汇总和定向构建／验证结果见 `tmp/battle-item-record-report.json` 与 `battle-item-record-*-final.log`。

## 修正和边界

- 初次执行参考的第二次投掷错误地再次按Down，把原保留的投掷选项切回使用。修正为读取 `7AA2` 后选择，失败日志 `tmp/native-battle-execution-item-record-first.log` 保留，第二次投掷独立重取成功。
- 菜单参考曾把行／列误采为 `7A44/7A42`，改为已定位的 `7A20/7A22`，全部路径重新采样。原错误日志 `tmp/native-battle-item-record-first.log` 和宿主失败 `tmp/battle-item-record-first.log` 保留。
- 目标页错用仙术提交顺序产生334像素差异，按原源码修正后56次全部一致；失败日志 `tmp/battle-item-record-second.log` 保留。
- 新目标复制时重复CMake目标的配置失败已修正，原日志 `tmp/battle-item-record-second-configure.log` 保留。旧动画探针补齐暖重开／短开场入口和失败时退出事件；旧道具流程探针改为观察实际指针绘制，取消无效按键的多余提交计数，效果／库存／计时断言保持。
- 本轮不升级 `NPESAV19`；共享缓存仍属于原运行驻留状态，既有存档布局保持。原NPE／PRG／QEMU文件SHA256保持。
- 原 mask-ROM 音频、物理时基、全部异常标志／文件IO、完整RAM别名、胜利装载和更多其它NPE机制继续。沿用已有诊断剧情结局；用户取消的正常属性完整通关和SDL从头到尾实玩保持取消。

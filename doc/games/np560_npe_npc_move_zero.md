# NPC零距离移动：DIV标志与xab实际后继

2026-10-10。修正此前从WASM的`execution_fault`推断原机应中止的错误。生产代码仍为纯C11＋SDL2，直接读取NPE；整体仍粗估约80%，完整引擎未完成。

## 原指令与控制流

原始指令说明：[CN1916884A](https://patents.google.com/patent/CN1916884A/zh)，PDF第28页（说明书第25页）。本批完整目视当地原PDF该页，图保存在`tmp/npc-zero-div/patent-28.png`。DIV的除数为0时设置V溢出标志；该页没有规定抛出宿主异常。

原固件的NPC移动服务：

- source `3C:8D78..8DDE`计算有符号位移和最大绝对值步数。
- `3C:8DF4/8E08`分别执行DIV。后面的方向选择重新读取原位移，不读取V或依赖除零后的商。
- `3C:8E52..8EAA`在两轴位移为0时写朝向0，并将循环字节初始化为1。
- `3C:90B6..90C2`将1与步数0作有符号比较，跳过整个移动／绘图／等待循环。因此零除后的商没有被用于移动，不必为未指定的商编造一个硬件值。
- `3C:910A..915A`照常发布缓存中的后继模式和事件；外层控制器沿用既有模式0→1收束。

既有`native-npc-move-resident-zero-fault.json`及`native-npc-script-move-zero-fault.json`保持不变。它们记录的是WASM参考的宿主执行错误，不能证明DMCU在该入口发生硬件陷阱。旧文档中以此拒绝零距离服务的结论撤回。

## xab为何会进入零距离

当前《天降风云xab》地图1的NPC23原位置是(16,45)，巡走标志为0；唯一NPC移动记录是`00000017001515000000`，目标(21,21)，后继模式0／事件0。NPC2对白的后继经过金钱事件2进入这条移动记录。

按原整数步长，第一次移动的两轴位移为5、-24，步长为0、-1，结束在(16,21)；再次交谈后沿X移动到(21,21)。后续再交谈就触发(21,21)→(21,21)。本批没有修改NPE记录、NPC坐标、条件或剧情后继来避开这一入口。

旧实际main路径已完成9个入口，在第10个入口中止，留有当前坐标和目标均(21,21)的记录。修复后从原标题／新游戏重新运行，前9个已完成后继的状态、输入和SDL提交计数逐项保持；第532次提交后完成第10个入口，随后完成第11个交谈和第12个出口，进入地图13。

## 修复

`world.c`移除`steps==0`时直接返回失败的检查。原有条件除法只在非零步数时计算商；零步数走原方向写入、跳过移动循环并执行当前缓存后继。

独立字节比较证明本批生产`world.c`相对保存版本只有这一处检查／说明变化。没有增加游戏名称、NPC编号、坐标或剧情结果特例；音频生产代码未改。

## 验证范围

### 原数值服务与C

`reference_npc_move_zero.py`执行未修改的runtime `1F:8BA2..9170`，使用原LINK／保存寄存器前缀；原数学服务也逐字节核对。排除入口地图绘制`2F:6AB8`，仅将绘图面选择器`03:4224`接成诊断RETS；不是原完整绘图执行。控制流确认零步数没有进入移动、擦除、绘制和等待循环，栈恢复，未出现缺失ROM／未知指令／非法控制流。

| 输入 | 原数值结果 | C结果 |
| --- | --- | --- |
| xab实际NPC23／槽6／事件1，(21,21)→同位置 | 坐标不变，方向0，初始化1，发布0／0 | 显示服务保留初始化1，公共接口按外层规则收束为1／0 |
| 诊断NPC14／槽0／事件1，(6,9)→同位置 | 坐标不变，方向0，发布1／77 | 一致 |
| 诊断NPC2／槽8／事件261选第五缓存，(255,255)→同位置 | 坐标不变，方向0，发布18／2 | 一致 |

这3组使用隔离数值状态；第二、三组不是自然剧情。原数值结果是按指令说明运行的模型辅助证据，不声称真实设备已动态采样。修复前C全部返回-1，修复后数字／显示接口全部通过；显示接口各仅提交入口地图一次、没有移动等待、保持C自身入口像素和随机状态。零距离样本没有原LCD参考。

### 实际xab主循环

既有`sdl_cross_npe_followup_probe.c`和输入策略未改。实际原标题／新游戏连续完成12个入口、743次C／SDL逐像素比较；仅在一场战斗中采用已授权的人物属性诊断。没有注入剧情、NPC、资源、敌人或胜利结果。仍使用固定种子、虚拟等待、关闭音频设备；这不是完整游戏或原声验证。

此前17份其它NPE的通过记录沿用；本批重查修复前唯一失败的xab。不把这些分批短路径称为18款游戏完整兼容。

### 必要回归和旧探针接线

4项受影响短回归通过，保留原像素参考：字／字节移动297次、返回分派48次、实际连续NPC链64次、普通返回16次，共425次原版／C／实际SDL提交零差异。

首次连续链回归的所有64次画面与游戏状态一致，但音乐重播计数失败。旧探针只监听section通知，遗漏当前生产优先使用的资源区间通知。保存的旧world和当前world都得到同样结果：重播期`plays=0`、原预期1，而游标0、初始化0、名义等待1974ms均正确。

连续链及普通返回探针现在也监听资源区间通知，检查实际偏移／长度对应当前NPE的section22，回调为原地图回调，并转发真实通知。没有降低预期计数或修改原参考；两条旧接线源码、首次失败和旧／新world诊断保持。此为探针修正，不是新增音频生产修复。

主程序和相关目标构建无警告；独立汇总`tmp/npc-zero-div/verified-report.json`通过。

## 文件与重查

- 原数值：`npe-sdl/tests/reference_npc_move_zero.py`、`tmp/npc-zero-div/native/report.json`。
- C入口：`npe-sdl/tests/npc_move_zero_probe.c`、`before-checks.json`、`after-checks.json`。
- 实际主循环：`tmp/npc-zero-div/xab-followup/report.json`。
- 旧探针诊断：`chain-diagnostic.json`、`regressions-before-observer.json`；最终`regressions.json`。
- 独立检查：`npe-sdl/tests/verify_npc_move_zero.py`。

```powershell
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
cmake --build npe-sdl/build --target npe-sdl npc-move-zero-probe sdl-cross-npe-followup-probe -j4
D:/ProgramData/miniconda3/python.exe npe-sdl/tests/reference_npc_move_zero.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --xab 'D:/Downloads/NP560plus游戏全集/天降风云xab.npe'
D:/ProgramData/miniconda3/python.exe npe-sdl/tests/run_cross_npe_followups.py --directory 'D:/Downloads/NP560plus游戏全集' --output tmp/npc-zero-div/xab-followup --filter xab --limit 12 --boost
```

准确原声、其它NPE后续、共享状态和其余实际游戏差异继续；原机存档互通、正常属性完整通关、SDL全程手动实玩仍排除。

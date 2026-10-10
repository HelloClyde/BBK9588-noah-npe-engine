# 地图、系统菜单及选人页的原版字母键

更新：2026-10-10。生产引擎仍为独立C11＋SDL2，游戏内容从当前NPE读取。

## 原版规则

| 输入 | 地图 | 菜单及选人页 |
| --- | --- | --- |
| I／i | 向上移动；出口提示选择第一项 | 等同上键 |
| M／m | 向下移动；出口提示选择第二项 | 等同下键 |
| J／j | 向左移动；出口提示忽略 | 等同左键，仍受当前页规则限制 |
| L／l | 向右移动；出口提示忽略 | 等同右键，仍受当前页规则限制 |
| K／k | 物件动作或确认出口 | 确认 |
| 空格／原生0x90 | NPC选择／对话 | 按各页原有确认映射处理 |
| F4／原生0x8E | 进入系统菜单 | 按原返回分派 |

原地图完整函数为物理37:1312～1C90，运行地址1A:1312～1C90。
四向分派在37:1624～1756；出口选择在37:18D6～19E6。
不支持的地图键在37:19D8直接返回。U／E／Q不是原地图的使用、装备或对话快捷键。

系统、道具列表和目标页共用4B:B5A0～B70C（运行2E:B5A0）。
它将I／M分派到36:337A／47A2（运行19），将K分派到37:CC72（运行1A）。
36:3878～38DE及4C9A～4D00处理音乐页的7A8A选择；该页标志7A54是6。
36:3D06～3D7E及5130后的分支处理页12的道具／装备目标循环。
仙术列表及选人使用4C:312E～327A（运行2F），保留7A40对左右方向的限制。

证据源保存于`tmp/map-system-letter-native-source.asm`、`map-system-letter-selectors.asm`和`map-system-letter-routing-ends.asm`。

## 生产修复

- 主循环在地图模式1和系统模式6按原语义转换I／J／K／L／M，出口分派在转换之后执行。
- 地图I不再打开背包；移除原版没有的地图U／E／Q及系统根菜单U／E入口。
- 删除旧`show_inventory`适配界面及其英文桌面弹窗。用药／装备继续从“物品”菜单进入，NPC对话使用空格。
- 接入地图用药目标、装备目标、仙术选人／列表／目标和音乐设置的字母导航。
- 无效地图键直接返回，下一次循环避免重复提交地图。若期间周期计时到期，则仍提交原NPC周期画面；按实际计时处理后的截止值判定，避免音频处理期间跨过截止时刻而漏画。
- 转换限定于上述调用者。问答文字、商店原J／L特殊分派及战斗原菜单处理保持各自规则。

本次只修改生产`npe-sdl/main.c`，没有修改NPE、原固件、效果服务或存档格式。

## 原程序分派诊断

`reference_map_letter_keys.py`执行未修改的原地图完整函数。
全PRG调用者和驻留镜像在03:0000启动，执行前核对CODE读取与原函数逐字节相同；不是以C转换表作为参考。

64条输入覆盖大小写字母、原生方向／确认、F4、空格及0x90、无效键、两种出口选择中的上下／左右／无效键。
核对服务入口、原键、方向、出口选择、游戏模式及初始化标志，全部一致；栈回到6000，无缺失ROM或非法控制。
行走、物件／NPC选择、LCD及周期服务是诊断返回适配器。此诊断证明原输入分派，不证明完整动作、LCD或物理时序。
没有使用NAND或主机写入来改变客体内存。结果为`tmp/map-system-letter-native/report.json`。

## 原画面及实际SDL流程

复用既有原厂独立画面、字段和输入序列，只将方向／确认事件改为原版字母。
修复前五组字母路径失败，方向键基线均通过；出口探针对齐缓存后亦是方向键通过、字母键失败。

| 实际生产调用者 | 原参考路径 | 每种输入的LCD提交对照 |
| --- | --- | --- |
| 地图主循环／视口移动 | 19 | 94 |
| 系统主循环／音乐设置／返回 | 9 | 77 |
| 地图仙术列表及选人 | 13 | 51 |
| 地图用药列表及目标 | 6 | 18 |
| 装备列表及目标 | 9 | 36 |
| 出口选择及菜单往返 | 10 | 127 |
| 合计 | 66 | 403 |

方向键、字母键、Shift、Caps Lock四种输入共1612次实际C／SDL原LCD对照。
这里每次同时检查C索引像素和SDL显示像素，不将二者重复计数；原参考帧也没有因四种输入而算作四批新原样本。
同一行走／出口探针还有直接C调用检查，未计入1612。

额外负向输入：19条地图路径各插入Q／U／E，共57次；9条系统根菜单路径各插入U／E，共18次。
75次检查均不多提交LCD、不改变所检查的状态，地图检查还要求诊断时钟不变，然后按原路径继续移动／返回。
对齐保留LCD的探针在修复前仍有306像素差异，修复后通过。

周期边界探针在无效Q之后让4000截止时刻发生在音频选择与周期处理之间。
第一次抑制重绘的实现会漏掉周期LCD，产生536像素差异；修正为比较实际处理后的截止值后，原NPC位置／方向、共享随机状态及SDL周期画面均一致。
普通到期路径也通过。所测是原逻辑周期输出，不声明物理时钟精度。

问答6条状态路径／10次实际主循环画面及20个字母／修饰键检查通过；结局探针改用原空格对话，37帧告别动画及返回NPE标题通过。

### 诊断输入对齐

出口旧探针在构造后移动视口，却沿用初始105格缓存。补齐与该原输入视口对应的缓存后，修复前生产代码即通过方向键127帧；原参考未改。
无效键诊断同时提供原保留LCD和地图帧缓存，测量从该输入之后开始。
旧NPC周期探针把每个SDL等待切片当成完整4000毫秒；现音频泵可分成10毫秒切片。修订为检查切片界限及实际逻辑时钟4000，原NPC和LCD参考未改。
这些都是隔离服务输入，未用来宣称真实剧情路线完成。

## 重现

```powershell
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
cmake --build npe-sdl/build --target npe-sdl sdl-walk-viewport-probe sdl-system-menu-return-probe sdl-map-skill-record-probe sdl-map-item-record-probe sdl-equip-item-record-probe sdl-exit-resident-probe sdl-npc-periodic-probe sdl-ending-flow-probe sdl-quiz-state-probe -j 6
python npe-sdl/tests/reference_map_letter_keys.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe
$env:NPE_PROBE_BATTLE_KEYS='letters' # shift／caps；删除此变量使用方向键
foreach($target in @('sdl-walk-viewport-probe','sdl-system-menu-return-probe','sdl-map-skill-record-probe','sdl-map-item-record-probe','sdl-equip-item-record-probe','sdl-exit-resident-probe')) {
    & ('npe-sdl/build/'+$target+'.exe') tmp/tjfy-world-probe.npe
}
Remove-Item Env:NPE_PROBE_BATTLE_KEYS
$env:NPE_PROBE_IGNORED_SHORTCUTS='1'
npe-sdl/build/sdl-walk-viewport-probe.exe tmp/tjfy-world-probe.npe
npe-sdl/build/sdl-system-menu-return-probe.exe tmp/tjfy-world-probe.npe
Remove-Item Env:NPE_PROBE_IGNORED_SHORTCUTS
$env:NPE_PROBE_IGNORED_DEADLINE='1'
npe-sdl/build/sdl-npc-periodic-probe.exe tmp/tjfy-world-probe.npe
Remove-Item Env:NPE_PROBE_IGNORED_DEADLINE
```

完整日志已有时，`python npe-sdl/tests/verify_map_system_letter_keys.py`只核对日志并汇总到`tmp/map-system-letter-report.json`，不重新运行游戏或生成原参考。

## 剩余范围

本批结束时穿戴页U尚未接入；后续已按原4B:B5D6～B5F8／3F:8364（运行22:8364）完成控制器、库存、属性及局部画面，见[穿戴页U卸装](np560_npe_wear_unequip.md)。当前状态以根计划为准。
其它特殊按键、文字／动画调用者、原声及完整跨NPE行为仍需补齐。本批不是整个引擎完成的证明。

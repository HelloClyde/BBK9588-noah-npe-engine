# 共享道具记录缓存（2026-10-08）

生产程序保持纯 C11＋SDL2，名称、说明和属性从当前 NPE 装载。原 PRG／WASM 只用于离线核对。本轮接入道具增减提示、物件拾取提示、穿戴页及新游戏构造；商店、地图道具菜单、战斗等其它调用点仍需按原顺序接入，整个缓存模块和引擎未完成。

后续同日已接入[商店缓存](np560_npe_shop_item_cache.md)、[商店选择边界](np560_npe_shop_selection.md)及[地图使用菜单缓存](np560_npe_map_item_record.md)。本页“本轮”范围是较早的基础装载工作；其它入口继续按后续计划核对。

## 原代码与实现

原 `3F:C370..C854`（运行时 `22:C370`）使用编号字节 `7A72` 和 `739E..742B` 的142字节缓冲。穿戴页的选择编号 `7AB2` 是另一字段。

- 参数只取低字节。编号0每次清零全部142字节；相同非零编号直接返回旧记录，不重读文件。
- 换编号读取 NPE 第2段的四字节偏移表，以一基编号选择记录。名称读12字节，再强制写第13字节为0。
- 说明从缓存偏移13开始，原循环最多写101字节，遇到0结束；结束符后已有字节保持。因此换编号不会清空说明尾部。
- 固定字段单独覆盖；文件中的顺序和缓存中的顺序不同。C 的 `item_record_raw[41]` 是缓存固定字段的派生视图，给现有属性消费者使用，不是另一份原固件状态。
- `npe_world_item` 返回的名称、说明和原始字段指针指向同一个世界的缓存，下次装载会覆盖它们。消费者跨装载保留字段时须按原服务顺序复制。
- 实际拾取和模式12提示改读当前缓存名称；原文分别为“得到 ”加名称、得到／失去后带双引号的名称，提示不添加说明或数量。

固定字段对照（字节偏移，均保留原大端字节顺序）：

| NPE记录偏移 | 142字节缓存偏移 |
| --- | --- |
| 0～11名称 | 0～11；12强制0 |
| 12类型 | 138 |
| 13、15、17、19、21、23、25 | 114、116、118、120、122、124、126 |
| 27、29、31、33 | 134、130、132、136 |
| 35、36、37 | 139、140、141 |
| 38价格 | 128 |
| 40起说明 | 13起，最多101字节 |

源码摘录：`tmp/object-service-original-source.asm`、`item-record-new-game-source.asm`、`item-record-consumer-source.asm`和`item-record-party-prepare-source.asm`。`item-record-character-prepare-source.asm`实际是地图网格服务，不能用来证明装备属性准备。

## 生命周期修正

原新游戏 `46:D0FC..D104` 先清编号。角色加载 `2E:88C8` 为三条记录分别调用 `39:188E`，六次道具装载按装备槽 `[0,3,1,2,4,5]` 执行，包括未活动队员。C冷新游戏和暖重开都执行这些装载；暖重开先保留旧142字节、清编号，再按当前 NPE 的角色装备覆盖。当前《天降风云》初始装备有空槽，最终缓存全零。这个结果来自记录及装载流程，没有写死游戏编号。

原系统读档、标题读档和传送保持共享道具记录。本轮 C 暖恢复复制现有缓存，主机冷解码使用冷零状态；142字节不写入 `NPESAV19`，线格式没有升级。

普通空装备选择在 `38:19BC..19BE` 跳过装载，清可见名称／说明并把 `7AB2` 设0，`7A72` 和142字节仍保留上一件。回到同一件时继续使用缓存旧记录。说明翻页的 `47:57AE/5A2A` 则会装载所选编号0，从而清共享缓存。C已分开这两种调用；后者本轮是源码与C检查，未把驻留 N／P 写入当作已证明的原公共输入送达。

初版“所有暖构造都保留缓存”和“普通空槽选择清缓存”的假设均被动态采样推翻，生产改动和测试已修正。原失败日志：`tmp/native-item-record-lifetime-preservation-assumption-failed.log`、`native-item-record-equipment-clear-assumption-failed.log`；诊断状态另保存在`native-item-record-equipment-attempt.json`。

## 本轮证据

使用原启动、新游戏、真实UI保存后的快照，每个路径单独启动原 WASM 进程。实际 F4 公共输入定位活动 RPG RAM，再退出菜单；只设置诊断事件、缓存和装备字段，原代码、NPE、文件IO和绘图服务保持。诊断缓存写入有效GBK名称“缓存道具”、说明“旧说明”，其它字节为A5，便于区分重读和驻留。

| 路径 | 对照 |
| --- | --- |
| 模式12装载1、同编号1、换为2、清0 | 4路径，16次原LCD／实际C与SDL整屏对照，含每条结束后的F4菜单提交 |
| 穿戴非空槽→空槽→同一非空槽 | 3次原LCD／实际C与SDL整屏对照，同时核对选择编号、缓存编号、142字节和说明索引 |
| 标题新游戏、空槽开始、系统读档、标题读档、传送、装备导航 | 6组原缓存生命周期采样；前5组核对对应C服务终态，装备组上述3幅核对 |

19次完整240×120画面中，C像素及实际SDL ARGB读回均零差异。模式12参考包含原Space结束提示和F4打开菜单；C调用实际事件调度及菜单绘制。每次提交同时核对全部142字节。原快照的RAM采样为每次推进后的状态，不宣称逐指令原RAM或物理按键时间一致。

额外C／源码检查：257沿用编号1而无需文件、256清零、暖恢复与新游戏／传送生命周期、冷解码为零、缓存变化不改变V19存档字节、符合翻页条件的空槽清缓存。原装备N／P公共输入送达仍未证明。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_item_record_cache.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_item_record_lifetime.mjs
python npe-sdl/tests/generate_item_record_cache_fixture.py
cmake --build npe-sdl/build --target npe-sdl sdl-item-record-cache-probe -j 4
& npe-sdl/build/sdl-item-record-cache-probe.exe tmp/tjfy-world-probe.npe
```

原参考：`tmp/native-item-record-cache.json`、`native-item-record-lifetime.json`及对应19张 `.idx`。生成器和夹具：`npe-sdl/tests/generate_item_record_cache_fixture.py`、`item_record_cache_native.h`。汇总：`tmp/item-record-cache-report.json`。

最终定向7目标构建退出0，无警告；新增探针与5项相关短回归均退出0：穿戴71次、模式12道具60次、拾取29次源码支持的SDL提交、新游戏24次、暖存读18组。拾取29次仍不计原拾取整屏对照。声明数组边界警告已修正，初版日志保存在`tmp/item-record-cache-array-declaration-warning.log`。不重跑ctest或长综合路线；取消的正常属性全程及SDL从头到尾验收保持取消。

原NPE、PRG和QEMU SHA256与上一轮一致；生产程序不依赖PRG／WASM／QEMU运行。

## 下一步

- 按原调用顺序接入商店、地图道具菜单／使用、属性页面装备名称、战斗道具／掉落与装备计算；不能仅把所有资源读取机械替换成会修改缓存的调用。
- 原加载在文件IO前写编号，部分读取失败仍继续；C现有文件读取失败处理尚未完整模拟。
- 无结束符说明后的完整RAM别名／越界字符串读取、其它服务覆盖交错、非零初始装备的跨NPE原动态对照。
- 共享技能记录、原mask-ROM音频、物理时基及其它NPE机制继续。已有诊断剧情结局沿用，整个目标保持未完成。

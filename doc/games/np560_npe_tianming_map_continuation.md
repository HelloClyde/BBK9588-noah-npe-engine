# 天明V1.3地图139／140：完整装载、出口与地图名提示

2026-10-10后续：[特殊地图自身存档](np560_npe_special_map_save.md)已修复实际零宽度地图140的回绕视口、固定7000字节地图缓冲冷恢复及关闭后保存；地图139／140冷暖读取和下一次移动、两次实际主程序重启及六项必要短回归通过。画面参考为自身保存前画面，特殊地图完整原LCD及其它后继仍未完成。

2026-10-10补充：[地图名驻留续读](np560_npe_map_name_tail.md)已修复12字节名称及单个终止符的原装载，末尾首字节消费零尾字节后继续读取后面的暖驻留状态；八条原strlen／取字路径和C／实际SDL输出通过，两条出口各7次提交继续通过。[原扩展字库](np560_npe_extended_fonts.md)也已接入。下方“扩展表未提取”及“暖邻接未验证”属于早期批次范围；特殊地图完整原LCD仍未核对。

更新：2026-10-10。共用16像素文字绘制器和原字库已修复，两条实际SDL出口完成地图名提示及恢复。运行引擎仍为纯C11＋SDL2，原PRG／QEMU只用于开发参考。源NPE字节未改，没有按地图编号绕过失败。下面保留先前装载与失败定位记录。

## 名称提示修复与原文字服务

`reference_text16_bytes.py`执行未改动的原`28:1F08`字符串服务、原
`17:9244`汉字查找、`28:FD38`ASCII查找及`18:657A`复制。
原16行字体D的字形背板从PRG原字节复制到诊断RAM，字体基址同步重定位；
查找后的两个诊断适配器记录参数、32字节字形和坐标，跳过LCD绘制。
原解析、查找和复制代码逐字节检查通过，正常返回且栈恢复。

原服务以首字节是否小于128区分8像素／16像素字距：

- `1E`发送给原ASCII字形表，有实际图案，占8像素。
- 名称139末尾的`CB`与紧随的零字节组成`CB00`，进入原无效字形分支，
  从原字库的`A1A1`槽取得空白备用字形，占16像素。
- `FEA1`同样走原备用字形；不按现有8836槽的范围自行绘制。

9组输入包括两条当前NPE名称、普通字母／汉字／标点、控制／DEL、末尾首字节、
无效字节对、80开头及FE开头，共30字形。每个原字形与PRG原数据核对；
C输出与原采样字形及坐标组成的240×120帧逐像素一致。
参考包含两个明确的零字节尾部填充，不证明原地图名之后的暖邻接字节、
完整LCD服务或原物理时序。原扩展字形的另外两张闪存表尚未提取，C继续明确报错。

`gbk16-native.bin`升级为`NPEF16V2`，追加00..1F与7F的33个原字形，
原95 ASCII／8836汉字字形区逐字节不变；12像素字库不变。
原查找和直接PRG核对也纠正了采样器初稿的名称偏移／DS A1基址设置，
无效初稿保留为`tmp/native-text16-bytes/invalid-setup-report.json`，不计通过。

两条原文件出口在实际`game_world_step_display`中各完成7次提交，
换图、名称提示及底图恢复通过；`map_name_result=0`，原视口坐标保持。
结果：`tmp/tianming-map-continuation-fixed-sdl.jsonl`。
原字形参考与C／SDL输出一致性是两类检查，没有宣称两张地图完整原LCD一致。

生产主程序和四个受影响探针构建通过。普通文字／标题、普通地图出口和四项
缺失／损坏字体启动检查通过。旧`sdl-leave-battle-probe`在首组第4次提交
仍有2148像素差异：临时链接旧文字绘制器后产生同样失败和相同像素文件哈希。
前四次离队提示提交零差异；入战差异与这次文字改动无关，保留为下一项待定位，
没有把失败改记成通过。汇总：`tmp/text16-map-notice-report.json`。

```powershell
python npe-sdl/tests/reference_text16_bytes.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --npe tmp/tianming-v13-map-continuation.npe
python npe-sdl/tools/extract_np560_gbk_font.py tmp/NP560.PRG npe-sdl/assets/gbk12.bin npe-sdl/assets/gbk12-native.bin npe-sdl/assets/gbk16-native.bin
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl npe-text-test sdl-map-continuation-probe -j4
npe-sdl/build/npe-text-test.exe npe-sdl/build/assets/gbk12-native.bin npe-sdl/build/assets/gbk16-native.bin tmp/native-text16-bytes --text16-bytes
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
npe-sdl/build/sdl-map-continuation-probe.exe tmp/tianming-v13-map-continuation.npe 139 140
```

## 完整原装载对照

此前两图只有原网格阶段参考。本轮`reference_map_stream.py`运行原目录查找和完整地图装载服务；文件适配器提供当前NPE的真实字节，原装载循环不变。

| 地图 | 原宽×高 | 网格字节 | 顺序事件尾字节 | 原最终文件位置 | C结果 |
| --- | --- | ---: | ---: | ---: | --- |
| 139 | 36×78 | 5616 | 1159 | 1665918 | 网格、名称、已建模456字节事件表一致 |
| 140 | 0×201 | 0 | 1090 | 1665745 | 名称、已建模456字节事件表一致 |

139完整装载越过下一目录项1280字节，140零网格后继续读取事件表。两条原调用均正常返回，栈恢复；没有缺失ROM、不支持指令或非法控制报告。C的文件EOF检查通过，源字节保持不变。

参考在`tmp/map-stream-native/tianming-v13-map139-full/`及`...map140-full/`，包括请求序列、原RAM、网格及报告。`map-stream-probe`检查网格和456字节事件字段，并运行C资源装载／绘图。这里没有完整原RAM或原LCD等价证明。

原NPE SHA256：`ccf3f9c0929a7d713204816e2bd2db785d7ca79e482e377fc4da8d29d53af7f0`；原PRG SHA256保持`d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5`。

## 文件引用与原出生坐标

新增`map-references-probe`用生产解析器扫描全部140张地图的12个出口、两条传送记录，包括有效目标和两个出口分支，没有按进度或坐标过滤。

- 地图6，出口第3槽，主目标139，出生(17,64)，进度门槛0。
- 地图82，出口第3槽，主目标140，出生(16,28)，进度门槛0。

结构图从地图1可以连接到两个目标，不能当作无引用记录跳过。结构图包括条件未满足的边，不证明从新游戏自然可达，也不覆盖暖保留／RAM改写目标。输出：`tmp/tianming-v13-map-references.json`。

`reference_map_spawn.py`新增可选`--npe/--maps/--name`，从原出口读取出生输入，运行已有原坐标计算切片；默认模式和参考目录保持原样。

| 目标 | 原视口偏移 | 原屏内坐标 | 实际C／SDL换图坐标 |
| --- | --- | --- | --- |
| 139 | (10,60) | (7,4) | 一致 |
| 140 | (241,24) | (31,4) | 一致；保留零宽度的原字节回绕结果 |

参考：`tmp/tianming-exit-spawn-native/report.json`。它只证明原坐标切片，不含LCD或零宽度后继。

## 实际SDL中断点

新增`sdl-map-continuation-probe`在当前NPE原出口旁构造短场景，保留原对象、NPC和事件，用实际`game_world_step_display`发送一次向上移动。两条路径均完成地图装载及视口切换，随后在地图名提示处返回-1。

两条路径各5次提交至中断，另各有1次源地图起图；SDL像素与C一致。没有原机LCD参考，不能算原像素对照。报告`tmp/tianming-map-continuation-sdl.jsonl`保留两项`passed:false`，探针退出1。

第一版探针等待未接通虚拟超时，已终止自己的进程并补齐；之后两项失败来自实际文字绘制器，没有修改生产代码避开失败。

原资源的12字节名称：

- 139：`42 6F 68 61 77 2D 74 B6 BC BB E1 CB`，末尾`CB`在名称内没有后续字节。
- 140：`1E D0 B5 D0 C7 BF C7 BC D5 00 E4 00`，开头为控制字节`1E`；原零字节结束处保持。

`npe_render_map_name_notice`交给`npe_render_timed_notice`，最终`draw_gbk_text16_rows`拒绝这两种输入，SDL出口控制器在提示提交前中断。139地图绘图独立检查返回0，两项名称绘制独立检查均返回-1。

下一步核对原16像素文字服务的控制字节、不完整双字节及备用字形规则，修复共同绘制服务，不能改名称、换字或省掉原提示。

## 更正上一轮队伍检查的解释

140第一条队伍原记录为`83 02 82 00 17 04 05`。后继模式`0x17`即23，事件号`0x0405`即1029；当前`npe_world_finish_party_event`执行经验事件，可能升级并回复体／气。上一轮在完整后继后仍要求体力不变，该断言不适用，不能证明地图RAM重叠错误。原模式23依据见[经验服务](np560_npe_mode23_experience.md)。

上述地图名称批次当时没有特殊队伍记录的独立原动态对照，原队伍报告的两项未解决状态保留。后续新增10条原队伍／经验服务输入和输出对照，以及36次实际SDL初始后继；天明队伍短检查20次入／离队、2次队首服务通过。详见[特殊队伍服务](np560_npe_special_party_followups.md)。再后续[模式4入口批次](np560_npe_mode4_rejected_entry.md)完成共用原分派核对及C修正，两种实际队伍状态均按两次F4返回地图；同10组实际SDL输出增加到42次，完整天明原LCD仍未核对。

旧离队入战2148像素失败后续也已定位：验证场景保留新游戏地图缓存，且非1活动字节比较混用了地图／战斗分区。修正探针后16组104次原C／SDL对照通过，生产规则与原参考不变；历史失败报告保持原样。见[离队入战修订](np560_npe_leave_battle.md)。

## 复现

```powershell
python npe-sdl/tests/reference_map_stream.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --npe "D:/Downloads/NP560plus游戏全集/天明游戏V1.3.npe" --map 139 --name tianming-v13-map139-full
python npe-sdl/tests/reference_map_stream.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --npe "D:/Downloads/NP560plus游戏全集/天明游戏V1.3.npe" --map 140 --name tianming-v13-map140-full
python npe-sdl/tests/reference_map_spawn.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --npe "D:/Downloads/NP560plus游戏全集/天明游戏V1.3.npe" --maps 139 140 --name tianming-exit-spawn-native
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target map-stream-probe map-references-probe sdl-map-continuation-probe -j 2
Copy-Item -LiteralPath "D:/Downloads/NP560plus游戏全集/天明游戏V1.3.npe" -Destination tmp/tianming-v13-map-continuation.npe
npe-sdl/build/map-stream-probe.exe tmp/tianming-v13-map-continuation.npe 139 tmp/map-stream-native/tianming-v13-map139-full/grid.bin tmp/map-stream-native/tianming-v13-map139-full/ram.bin
npe-sdl/build/map-stream-probe.exe tmp/tianming-v13-map-continuation.npe 140 tmp/map-stream-native/tianming-v13-map140-full/grid.bin tmp/map-stream-native/tianming-v13-map140-full/ram.bin
npe-sdl/build/map-references-probe.exe tmp/tianming-v13-map-continuation.npe
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
npe-sdl/build/sdl-map-continuation-probe.exe tmp/tianming-v13-map-continuation.npe 139 140
```

三个相关探针最终构建无警告。两条原完整装载、C装载对照及两条原坐标参考通过；实际SDL两条路径按上述缺口退出1。生产未改，未扩大到无关回归。整体仍粗估约80%，原声与其它游戏兼容继续。

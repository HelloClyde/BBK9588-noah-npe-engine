# 特殊地图自身存档：原视口与固定地图缓冲冷恢复

更新：2026-10-10。实现为纯C11＋SDL2。本批修复《天明V1.3》实际出口进入地图139／140后的自身存读档，不修改NPE，没有按游戏或地图编号添加生产分支。整体仍粗估约80%，完整引擎尚未完成。

## 缺口与原版行为

当前NPE为`tmp/tianming-v13-map-continuation.npe`，SHA256：
`ccf3f9c0929a7d713204816e2bd2db785d7ca79e482e377fc4da8d29d53af7f0`。

| 实际出口 | 目标尺寸 | 人物坐标 | 视口偏移 | 屏幕坐标 |
| --- | --- | --- | --- | --- |
| 地图6／出口3 →139 | 36×78 | 17,64 | 10,60 | 7,4 |
| 地图82／出口3 →140 | 0×201 | 16,28 | 241,24 | 31,4 |

原`48:2290..23B2`视口构造接受字节坐标和零／小尺寸；减法结果按字节回绕。地图140的偏移241与屏幕坐标31来自此规则，不应按普通地图范围夹紧。

原地图分配服务固定分配7000字节，地址由驻留指针`52:797E`保存。原`0C:BF0C..BFB4`网格流仅装载宽高乘积有符号低word表示的格数；宽为零时不覆盖旧缓冲。原`4C:E15A..E27E`刷新105格缓存，读取线性索引而不按逻辑网格范围截断。地图140因此会读取此前地图82保留的缓冲格241～255。

此前自身存档存在两个独立问题：

1. 普通视口范围校验拒绝地图140，保存返回`-1`。
2. 只修复视口校验后可以保存，但V23只保存逻辑格数；该地图格数为零。重启读取丢失旧地图缓冲，冷恢复与保存前相差15204个像素。运行中读取因旧缓冲仍在而未暴露此问题。

失败记录保留在`tmp/special-map-save-first-check.log`和`tmp/special-map-save-second-check.log`，修改前文件保留在`tmp/special-map-save-before/`。

## 生产修复

- `save.c`保留普通地图校验，另接受与原视口构造完全一致的字节回绕结果；仍检查人物坐标与视口关系，不放行任意宿主坐标。
- `save_terrain.inc`按当前105格视口读取范围判断是否需要保留逻辑网格之外的固定地图缓冲；需要时写自身格式`NPESAV24`。
- `rpg_runtime.inc`关闭资源时保留逻辑网格及至少3500个word的固定缓冲；冷读取并重新装载资源后叠加快照，再刷新视口。
- `world.c`新地图／重新开始清除旧快照标志。新增快照标志用于自身恢复，不增加驻留地址绑定。

生产判断依据实际尺寸、视口与缓冲，不依据地图139／140编号。不序列化宿主指针、原堆地址或完整RAM。

### 自身格式

| 写出版本 | 使用条件 | 本批实际文件 |
| --- | --- | --- |
| NPESAV22 | 旧投影足以表达当前状态 | 地图139，2362字节 |
| NPESAV23 | 需要完整地形记录与逻辑网格 | 既有动态地形短回归继续通过 |
| NPESAV24 | 当前视口读取逻辑网格以外的固定缓冲 | 地图140，9558字节 |

V24保留V22前缀；扩展为宽、高各1字节、逻辑格数2字节、192字节地形记录，以及`max(逻辑格数,3500)`个小端word。地图140逻辑格数仍为零，保存的3500个word为7000字节；不能把固定缓冲长度写成逻辑格数。

原文件指纹、长度、校验和及尺寸／格数检查继续生效。读取保留自身V1～V23路径并新增V24；本批只运行受影响的必要回归，没有逐个重新验收全部历史格式。

## 验证结果

两个实际出口各7次SDL提交通过。地图139／140均完成：

- 保存、运行中读取、关闭后冷读取，全部三份148字节地图人物记录保持。
- 保存前、运行中读取后及冷读取后的C／SDL画面一致。
- 读取后执行一次真实左键输入，返回值、位置、视口、模式、事件和画面一致；地图139该步阻挡，地图140实际移动。
- 地图140全部3500个缓冲word与保存前逐字节一致。
- 移动后保存，再关闭资源后保存；两个地图的文件均逐字节一致。

地图140原位置及移动后的文件分别经实际主程序`--continue`冷启动；首个C／SDL提交像素差均为零，192字节记录与3500个缓冲word保持。

四种重新计算有效校验和的新格式坏档——格数不符、截短、追加尾部、视口不一致——均被拒绝，当前World未发生变化。

**画面参考是本引擎保存前的画面，不是原完整LCD。** 每个地图只核对了读取后的一个移动输入；这不等于特殊地图完整剧情或后续行为已经完成。

### 必要回归

以下六项退出码均为0：

| 目标 | 核对范围 |
| --- | --- |
| dynamic-terrain-save-probe | V23完整记录、动态网格、冷暖读取、复存及V22 |
| sdl-dynamic-terrain-save-probe | 既有V23实际主程序冷启动 |
| terrain-snapshot-lifecycle-probe | 关闭、保存、重新装载及重开清理四组 |
| map-runtime-chain-probe | 实际地图堆、门后继、冷暖恢复及重新开始 |
| sdl-host-path-probe | UTF-8文件路径、自身存档及SDL标题 |
| sdl-live-restore-probe | 18组实际SDL读取、音乐与随机保留 |

主程序及修改目标构建无警告。独立脚本`npe-sdl/tests/verify_special_map_save.py`检查文件布局、指纹／校验和、缓冲导出、帧导出、关闭后保存、两次实际主程序启动及六项日志；结果为`tmp/special-map-save-report.json`，`passed=true`。

## 重查入口

在仓库根目录运行，先将`C:/msys64/ucrt64/bin`加入PATH：

```powershell
cmake --build npe-sdl/build --target npe-sdl sdl-map-continuation-probe sdl-dynamic-terrain-save-probe
npe-sdl/build/sdl-map-continuation-probe.exe tmp/tianming-v13-map-continuation.npe --save 139 140 *> tmp/special-map-save-final.log
npe-sdl/build/sdl-dynamic-terrain-save-probe.exe tmp/tianming-v13-map-continuation.npe tmp/special-map-140-save.npes *> tmp/special-map-save-startup.log
npe-sdl/build/sdl-dynamic-terrain-save-probe.exe tmp/tianming-v13-map-continuation.npe tmp/special-map-140-save.npes.after-move *> tmp/special-map-save-moved-startup.log
D:/ProgramData/miniconda3/python.exe npe-sdl/tests/verify_special_map_save.py
```

独立校验还读取本批已生成的六项回归日志、常规V22文件和构建日志；重跑上述入口不会重新运行六项回归。输入与代码未改时沿用既有结果。

## 仍未完成

其它游戏依赖的事件／资源状态及新增状态恢复继续；原音频、其它动作／队伍组合、跨NPE后继及特殊地图完整原LCD仍有缺口。本批恢复固定地图缓冲，不扩展为完整地址空间或原堆布局的序列化。

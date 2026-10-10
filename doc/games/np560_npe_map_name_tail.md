# 地图名驻留装载与零尾字节后的续读

2026-10-10。纯C＋SDL已补齐地图名称的原驻留装载，以及末尾汉字首字节消费零终止符后继续取字的行为。没有按地图编号分派生产规则。

## 原程序依据

- 原地图装载 `0C:BE84..BEE2`（PRG源 `29:BE84..BEE2`）逐字节写入 `52:78E4..78EF` 的12字节名称，然后只清零 `52:78F0`。`78F1`保留。
- 原提示 `21:FE26`（源 `3E:FE26`）调用 `31:AED0` 测字节strlen，以长度居中画框。`21:FF86`调用 `28:1F08` 绘制同一驻留地址，没有将字符串复制到另一个零填充缓冲。
- `28:1F08`在每轮先检查x是否达到239，再读首字节。首字节小于128占8像素；其它首字节与紧随字节组成字形参数，占16像素。尾字节为零仍消费两个字节，下一轮继续检查后面的首字节。

源码摘录：`tmp/tianming-map-loader-source.asm`、`tmp/map-name-terminator-source.asm`、`tmp/map-name-terminator-notice-source.asm`。原PRG SHA256保持 `d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5`。

## 具体缺口与修复

此前C读取NPE中的有界名称绘制，驻留地图装载没有写入这13个字节。对于末尾`CB`等首字节，C绘制`CB00`备用字形后直接结束，忽略原程序下一轮可读到的驻留状态。

- `rpg_runtime.inc`在装载地图时写入当前NPE的12字节名称及一个零字节，保留`78F1`及后面的其它驻留字段。
- 实际地图名提示从当前地图所属运行时读取名称，按原字节strlen确定框宽及位置，再沿驻留字节取字；被消费的零尾字节不会提前终止后续读取。
- 原8／16像素字形查表合并为同一C函数，继续使用已提取的原常用、控制和扩展字体。
- 到x239停止读取；此前开始的字形按LCD宽度保留可见部分。普通有界文字接口保持原来的调用约定。

没有新增World字段或改变自身存档格式。本批验证同一运行时的暖换图保留；原机存档互通仍排除，整个资源关闭／自身恢复的共享状态范围继续按根计划处理。

## 原参考与验证

`tests/reference_map_name_tail.py`使用此前未修改原完整地图装载得到的139／140 RAM输入，核对NPE及原PRG哈希。原strlen、字符串、字形查找及复制指令保持不变；查找后的两个诊断适配器仅记录32字节字形和坐标，跳过LCD输出。

八条路径、89次取字包括：

| 输入 | 原strlen | 原返回x | 字形数 |
| --- | ---: | ---: | ---: |
| 139原装载 | 12 | 176 | 10 |
| 140原装载 | 9 | 156 | 5 |
| 139暖尾ASCII | 12 | 184 | 11 |
| 139暖尾汉字 | 12 | 192 | 11 |
| 139暖尾控制字形 | 12 | 184 | 11 |
| 139暖尾右侧汉字 | 12 | 248 | 18 |
| 139暖尾无零字节 | 12 | 240 | 18 |
| 140已有终止位置，暖尾不读取 | 9 | 156 | 5 |

后六条属于隔离诊断状态，不宣称原NPE自然产生这些暖尾。原调用全部正常返回、栈恢复，无缺失ROM、不支持指令或非法控制。结果为 `tmp/native-map-name-tail/report.json`。

新SDL探针在修复前退出1：八组均发现驻留名称未装载，五组续读画面分别缺21／89／26／199／168像素。失败保存在 `tmp/sdl-map-name-tail-before.log`。

修复后八组驻留名称和按原字形／坐标组成的像素全部一致，实际SDL颜色读回一致；换到另一地图再返回时`78F1`保留。字形参考及窗口组合不是原完整LCD采样；右侧输出是C／SDL裁剪检查。

主程序及四个相关检查目标无警告重建。新探针、139／140两条实际SDL出口（各7次提交）、9组既有文字服务及150组扩展字形C／SDL检查全部通过。没有扩大到全流程通关或其它未受影响功能。日志为 `tmp/<目标名>-map-name-tail.log`；汇总为 `tmp/map-name-tail-report.json`。

## 复现

```powershell
python npe-sdl/tests/reference_map_name_tail.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --npe tmp/tianming-v13-map-continuation.npe
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-map-name-tail-probe npe-text-test sdl-map-continuation-probe sdl-extended-fonts-probe -j4
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
npe-sdl/build/sdl-map-name-tail-probe.exe tmp/tianming-v13-map-continuation.npe tmp/native-map-name-tail
```

地图名这条暖续读缺口已修正。其它文字调用者、特殊地图完整原LCD、完整共享状态恢复、原声及其它NPE完整动态兼容继续，整体仍粗估约80%。

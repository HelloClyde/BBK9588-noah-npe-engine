# NPC 专用字体与标记段

2026-10-10。生产 C／SDL 对白绘制已接入原 NPC 控制字节的三套专用字体及标记段。文字仍从当前 NPE 读取，未按游戏或 NPC 编号替换。

## 原规则与实现

原 NPC 格式器为 `1F:2B72`，行渲染为 `26:459C`；专用字体选择为 `31:598A`，字形包装为 `33:37C6`，实际取字／复制为 `21:0000..02DA`。

| 行内输入 | 原行为 |
| --- | --- |
| `6`＋一个字节 | 选择字体8，绘制后一字节的8×16字形，前进8像素 |
| `8`＋一个字节 | 选择字体23，其余同上 |
| 其它非零且小于9的控制字节 | 选择字体9，从下一字节起逐字节绘制，直到7或零；7被消费 |
| 普通字节 | 沿用原 ASCII／D 字形查找，双字节前进16像素 |

标记段中的高位字节及6／8保持单字节字体9绘制，不嵌套解释。排版阶段沿用前批已实现的原整对切行、标记段换行时的7前后缀、CR／LF消费和同次调用行缓冲保留。行渲染保留原 LCD 裁剪和结束规则。

`npe-sdl/text.c`新增专用字形和行绘制；故事文本继续走自己的原字节格式器。本批未新增 World 字段或改变自身存档格式。

## 原字库来源

原字体描述表位于运行时 `2F:6BFA`，对应 PRG 代码映射源 `4C:6BFA`／文件偏移 `2C6BFA`。每项10字节，包含相对字库地址、字形字节数、宽和高。三项均为16字节、8×16。

原驻留字库基址对应 PRG 文件偏移 `30D000`，三个字体的文件偏移分别为 `314800`、`315800`、`3893F0`。`tools/extract_np560_npc_fonts.py`核对固定 PRG SHA256，按原描述表提取并转换 DMCU 字节顺序，生成 `native_npc_fonts.inc`。共768字形、12288数据字节，SHA256：

`0c371fc9e98f81dcc9a98a200bffaa338e78f8ad21de55fecb0086bf641d31f0`

这些数据编入纯 C 主程序；运行时不读取 PRG、NAND，也不依赖模拟器。既有12／16像素资产及其格式保持原样。

## 验证范围

`tests/reference_npc_rich_text.py`执行未修改的原字体描述表、字体选择、字形复制、NPC格式器及行解析代码，执行前核对十段原代码／数据一致。闪存传输、字形复制后的 LCD 输出、行队列和提交采用诊断适配器。

- 三套字体的全部768字形与原取字函数复制结果逐字节一致。
- 12份隔离输入覆盖控制6／8、标记段、混合普通／双字节、标记内高位／控制字节、行末与跨行、CR／LF、其它小控制字节、零尾字节及未闭合标记；共163次原字形／坐标记录。
- `sdl-dialogue-glyphs-probe --rich-text`通过生产 NPC 页面读取和绘制路径，检查 C 像素、实际 SDL 读回、分页状态与临时堆恢复。
- 修复前12组均有画面差异，分页状态无差异；修复后12组全部零差异。
- 必要回归通过：既有26组 NPC／故事字形路径；NPC实际 main 的12条路径／32页／5次恢复；故事实际 main 的11条路径／21次提交。主程序及三个受影响探针构建通过，无编译警告。

输出见 `tmp/native-npc-rich-text/report.json`、`fonts.bin`及各`.input`／`.idx`；修复前后日志为 `tmp/sdl-npc-rich-text-before.log`／`tmp/sdl-npc-rich-text-after.log`，汇总为 `tmp/npc-rich-text-report.json`。

18份现有 NPE 的4684条去重 NPC 原对白流中未发现1～8控制字节。原文件名称／哈希、每份流数和扫描边界记录在 `tmp/npc-rich-text-inventory.json`。该静态范围不证明其它文字调用者、运行时保留字节或未来资源不会使用这些分支。

参考来自原字形与坐标组合，未采样原完整 LCD，也没有新增自然剧情触发或跨调用暖栈证明。12份输入仅追加到探针内存读取视图，原 NPE 文件和资源未改写。原声、游戏依赖的共享状态、其它文字调用者及跨 NPE 完整兼容继续，整体仍粗估约80%。

## 复现

```powershell
python npe-sdl/tools/extract_np560_npc_fonts.py tmp/NP560.PRG npe-sdl/native_npc_fonts.inc
python npe-sdl/tests/reference_npc_rich_text.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe
cmake --build npe-sdl/build --target npe-sdl sdl-dialogue-glyphs-probe sdl-npc-text-service-probe sdl-story-text-service-probe -j4
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
npe-sdl/build/sdl-dialogue-glyphs-probe.exe tmp/native-npc-rich-text --rich-text
npe-sdl/build/sdl-dialogue-glyphs-probe.exe tmp/native-dialogue-glyphs
npe-sdl/build/sdl-npc-text-service-probe.exe tmp/tjfy-world-probe.npe
npe-sdl/build/sdl-story-text-service-probe.exe tmp/tjfy-world-probe.npe
```

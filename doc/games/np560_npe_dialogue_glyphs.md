# NPC／剧情扩展字形与原矩形文字排版

后续已补齐NPC专用字体及标记段，见[NPC富文本](np560_npe_npc_rich_text.md)。下文保留本批当时的验证范围。

2026-10-10。实际C＋SDL对白路径已接入原D扩展字形、控制字形及备用字形，补齐原矩形格式器的字节切行规则。生产代码从当前NPE读取文字，不按游戏、NPC编号或特定字节替换文本。

## 原程序与缺口

NPC源`3B:7440`调用`03:42B4`向量，再跳到`1F:2B72`（PRG源`3C:2B72..3190`）。故事源`3F:91FE`调用`29:E552`（源`46:E552..E7E0`）。两者传入左、上、右、下及字符串指针两个word，但使用不同排版规则。

NPC格式器按`(右−左+1)/8`和`(下−上+1)/16`决定列／行数；大于80的首字节与后一字节一起进入行缓冲，只剩一格时补空格、留到下一行。CR／LF按原规则消费；行渲染为`26:459C`，普通字形经`32:7B42`进入ASCII／D查找。

故事格式器在16像素模式按`(右−左)/8`确定一行字节数，再调用原`28:1F08`绘制行缓冲：

- 每读一个高位字节增加计数；满行时计数为奇数，退回该行最后一个字节，下一行重新处理。
- 此规则没有按GBK首／尾字节配对，因此带低位尾字节的扩展字符及特殊字节在行尾的行为与宿主按字宽换行不同。
- 行缓冲在同一调用的各行之间保留。最后一个首字节消费零尾字节后，原字符串函数还可能读取此前行缓冲的尾部，直到下一首字节为零或到LCD边界。
- 两路普通字形查找处理D控制字符、DEL、两段扩展字体及`A1A1`备用字形。`FEA1`采用备用字形，不是常用区之后的新索引。NPC富文本的6／7／8专用字体与标记绘制不属于上述普通字形完成范围。

此前`story_text_glyph`仅接受可打印ASCII和部分常用双字节，遇到扩展或特殊字节返回错误；实际NPC／故事页面随之失败。`world_story_text_draw`还跳过控制字形，并按宿主字宽换行。

反汇编：`tmp/window-formatter-original-source.asm`、`tmp/text-resident-focused-source.asm`、`tmp/window-story-original-source.asm`及`tmp/dialogue-vectors-original-source.asm`。

## 修改

`npe-sdl/text.c`的实际NPC／故事路径共用已核对的`glyph16`，分别调用NPC整对切行和故事字节／奇偶切行，保留各自LCD边界。单次调用的行缓冲初值为零，并在其各行间保留。NPE中的特殊字节保持原样。

未新增World字段或改变自身存档格式。本次尚未复刻跨调用的原栈残留字节；只有实际游戏依赖的具体表现才继续加入实现，不把完整原调用栈作为单独验收门槛。

## 原参考

`tests/reference_dialogue_glyphs.py`执行未修改的两种原矩形格式器、行字符串解析及字形查找。NPC行队列提交／结束、闪存传输和查找后的LCD输出采用诊断适配器；PRG和NAND输入核对固定SHA256，执行前核对九段原代码未修改。队列／结束适配器只返回，不改原行缓冲、分支或字形参数。

| 输入 | 范围 |
| --- | --- |
| 六份原NPE对白 | 《天机》NPC155、《新仙剑后传》NPC28、《天明》V1.0～V1.2 NPC230、V1.3 NPC168；直接提取含特殊字节的原页面 |
| 八份隔离诊断 | 两段扩展表、7F尾字节、备用字形、控制／DEL、两种行尾、末尾单首字节、故事矩形及底边裁剪 |

14份输入经12条NPC、14条故事原格式器路径，共487次原字形及坐标记录。每条路径明确清零诊断调用者栈，保留原函数在各行间的局部字节。六份原对白的页面边界依据已有原读取服务规则提取，本批没有重新执行原NPC条件选择器或完整控制器。

以原采样的字形和坐标组成参考像素。这不是原完整LCD采样、自然剧情触发或跨调用暖栈证明。输出为`tmp/native-dialogue-glyphs/report.json`及对应`.input`、`.idx`；六份原游戏复制到该目录供探针读取，原始资源字节及哈希不变。

## 修复前后与短回归

新`sdl-dialogue-glyphs-probe`调用生产`npe_render_world_npc_page`／`npe_render_world_story_page`，同时检查分页位置、续页标志、临时堆释放及实际SDL颜色读回。隔离诊断仅扩展探针的内存文件读取视图，原游戏资源字节不变，不写回原文件。

- 修复前26条路径均返回错误；六份原对白的分页状态仍正确，部分像素已提前停止绘制。新仙剑样本像素本身相同但函数仍返回错误，不能算通过。
- 基线`text.c`由本批改动的前态重建，其SHA256与上一批已完成地图名报告中的`4dfceafa13e08861b015cd27ebdec98899e8d0338d9cdcabe112f890f4006f10`一致；与完全相同的当前探针、依赖对象及最终参考输入链接。失败日志为`tmp/sdl-dialogue-glyphs-before.log`。
- 修复后26条路径分别对照对应NPC／故事原格式器，C像素、SDL像素、分页状态及堆恢复全部零差异，日志为`tmp/sdl-dialogue-glyphs-after.log`。
- 受影响短回归通过：NPC实际main的12条路径／32页／5次恢复；故事核心87页及文字状态恢复；故事实际main的11条路径／21次提交；150组原字形及全部14551个原12像素解包／16像素字形。
- 主程序与五个相关检查目标构建通过，无新增编译警告。首次探针构造中的重复释放和扩展文件视图解析问题已修正；最终探针和基线均完整运行。
- 本批先按故事格式器给NPC取参考，随后沿`03:42B4`发现该依据错误；已改为分别执行原NPC和故事格式器，并重跑基线、生产检查及受影响回归。扩展采样记录移到独立RAM后，诊断复制器的DS切换错误亦已修正。最终计数为26条原路径／487字形，不沿用早先14条／256字形的范围。

本批汇总：`tmp/dialogue-glyphs-report.json`。字库及常用文字回归没有扩大到全程手动游玩、正常属性通关或其它无关模块。

## 复现

```powershell
python npe-sdl/tests/reference_dialogue_glyphs.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-dialogue-glyphs-probe sdl-npc-text-service-probe story-text-service-probe sdl-story-text-service-probe sdl-extended-fonts-probe -j4
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
npe-sdl/build/sdl-dialogue-glyphs-probe.exe tmp/native-dialogue-glyphs
```

NPC专用富文本字体／标记绘制、原声、游戏依赖的共享状态、其余文字调用者与跨NPE动态兼容继续；整体仍粗估约80%，不将引擎标为完成。

# 原C／D文字边缘绘制

更新：2026-10-10。独立C／SDL绘制已补齐本批原函数参考覆盖的边缘行为；完整引擎仍未完成。

## 原函数依据

`reference_native_font_edges.py`直接执行原`28:1F08`字符串函数、`17:9244`汉字函数、`28:FD38`ASCII函数和`18:6454..66DA`行绘制／汉字裁切服务。相关六段代码在原执行器内逐字节核对；驻留字库原字节搬到诊断RAM，绘制函数未替换。每次绘制前将整个原显示缓冲设为色阶7，绘制后通过原复制服务保存14400字节。

参考来自完整RAM显示缓冲，转成240×120色阶像素。没有字形绘制或LCD绘制适配器；未验证物理LCD传输、显示时序，也没有运行自然剧情路线。诊断没有装载NPE，只提供明确字符串、字库描述符和坐标。

原行为分别为：

- 字符串循环在读取每个字符前检查有符号word坐标；`x >= 239`或`y >= 119`时直接停止。首零停止、高位首字节消费两字节及字距沿用原规则，横坐标按word加法继续。
- 汉字拒绝负坐标，合法坐标处按剩余宽高裁切。被拒绝的字仍由字符串服务消费并增加横坐标。
- ASCII采用6／8像素宽度，只裁切底部高度。横向写入沿原倒序行排列的显示缓冲继续：越过右边缘的像素可写到上一行开头；左侧负一坐标可写到下一行末尾。
- 负一纵坐标的ASCII在屏幕外开始，后续可见行保持原字形行号。未绘制位保留已有色阶，前景色0同样写入。

## 生产修正

此前12／16点绘制在整个字形放不下时返回错误，可能提前终止菜单；紧凑12点也重复了这个检查。修复前，同一原参考第1个右边缘汉字检查实际失败，见`tmp/font-edges-before-failure.log`。

`text.c`现在共用原坐标和字形行绘制服务：12点普通／紧凑与16点文字分别保留字距，汉字裁切与ASCII缓冲行间写入分别处理。文字到达原循环停止坐标时正常返回，标题反色使用的16点局部行绘制继续使用同一服务。

本批既有生产源文件只修改`text.c`；字体、NPE资源、游戏规则和存档格式保持原数据及既有实现。开发参考工具继续与生产程序分开，便携程序无需原PRG、NAND或DMCU执行器。

## 已完成检查

| 检查 | 数量与范围 |
| --- | --- |
| 新原显示缓冲 | C12字距6、C12字距8、D16字距8各20组，共60组；普通、右／下／双边缘、停止坐标、负一坐标、首零、零尾字节、空串、前景0 |
| 新C／实际SDL对照 | 普通与Release各60组，逐像素比较整个240×120缓冲及SDL色阶映射 |
| 原12点字符串回归 | 普通与Release各36组，128次原字形／坐标参考 |
| 商店／仙术说明 | 8条商店路径25帧、4条仙术说明路径16帧原LCD／状态 |
| 剧情文字／标题选槽 | 11条剧情文字路径21帧、标题选槽24次C／SDL对照 |
| 其它文字 | 9组原16点字形／坐标参考及基本文字检查 |
| 构建与交付 | 主程序、相关探针和Release构建0警告；更新便携ZIP后14项交付检查通过 |

参考：`tmp/native-font-edges/report.json`、`frames.bin`。汇总：`tmp/font-edges-report.json`。当前包检查：`tmp/font-edges-portable-report.json`、`tmp/npe-portable-report.json`。

本批证明上述输入下的可见显示缓冲行为；不证明所有有符号word坐标、屏幕外写入对邻接RAM的影响、其它字符模式、跨调用局部缓冲或所有文字调用者已经等价。准确原声、共享状态和跨NPE完整兼容继续，整体仍粗估80%。

## 重现

在仓库根目录执行：

```powershell
python npe-sdl/tests/reference_native_font_edges.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-native-font-edges-probe -j 4
npe-sdl/build/sdl-native-font-edges-probe.exe npe-sdl/assets/gbk12-native.bin npe-sdl/assets/gbk16-native.bin tmp/native-font-edges/frames.bin
```

`verify_native_font_edges.py`核对已完成日志、原参考、生产改动和当前交付包，不重跑游戏或重新生成参考。

# 原12点菜单字形与备用行为

后续更新：本记录的右／下边缘待核对项已在[原文字边缘绘制](np560_npe_native_font_edges.md)中按60组原完整显示缓冲修正；其它范围仍依当前计划。

2026-10-10。已修正菜单、名称和说明共用12点取字函数的非原版拒绝行为。整体引擎仍未完成，进度粗估保持80%。

## 原程序与发现

原字符串函数为CPU `28:1F08`，PRG `45:1F08..221C`。默认字符模式中，小于`80`的非零字节消费一字节，调用`28:FD38`；其它字节消费两字节，调用`17:9244`。ASCII字距由`51:2924`提供，双字节字距由`51:291A`提供。首字节为零时停止；高位首字节后的零仍作为尾字节消费，随后继续读后面的字节。

原C字库分支为`17:99B8..9DA2`，备用分支`17:9D64`从驻留字库基址加`9000`读取原`A1A1`。原ASCII函数直接使用字码对应的驻留槽，包括`01..1F`与`7F`。两段闪存扩展表和驻留字形来源沿用[扩展字库记录](np560_npe_extended_fonts.md)。

此前宿主取字函数按常规GBK检查尾字节，遇到`CB20`等输入返回错误，导致菜单绘制提前退出；控制字码和`7F`也被拒绝。修复前，同一原参考的第1组紧凑字符串检查实际失败，日志为`tmp/native12-fallback-before-failure.log`。

## 生产修正

- `text.c`共用12点取字函数按原三段范围选择字形，其余双字节码使用原`A1A1`。`80`、`FF`首字节同样消费两字节。
- 12点普通／紧凑绘制在首字节为零时停止。边界内的高位首字节消费零尾字节，后续文字位置保持原字距。
- 新增`native12_control_glyphs.inc`，保留原ASCII表`00..1F`及`7F`的33槽、792字节。源为经哈希校验的PRG偏移`30F000 + code * 32`，每槽取前24字节，保持原行掩码；提取工具新增`--controls`选项。
- 两份字体文件的布局、字形和哈希保持不变。生产程序继续独立运行，原PRG／NAND仅用于开发时提取和核对。
- 旧文字测试关于“末尾高位首字节应报错”的宿主假设已改为原备用字形对照，依据是独立原函数采样。

## 已完成检查

`reference_text12_bytes.py`执行原字符串解析、取字、ASCII复制和12点扩展解包。驻留字库映射到诊断RAM；闪存传输服务使用已验证与厂制物理NAND一致的原字节；取字后的LCD调用边界改为捕获。原相关代码区逐字节核对，结束状态没有缺失ROM、未支持指令或非法控制，原栈恢复。

- 36组字符串、128次字形和坐标，覆盖6／8像素ASCII字距、全部非零低位控制字码及DEL、有效扩展边界、驻留字形、无效尾字节、`80`／`FF`、末尾高位字节、零尾字节继续读取、首零停止。
- 普通构建和Release构建各36组实际C／SDL输出与原字形／坐标合成参考一致。
- 既有商店8条路径25帧、仙术说明4条路径16帧的原完整LCD／状态回归通过。
- 既有150组原字形／SDL、9组16点字符串及基本文字检查通过；原字库文件和原参考未重写。
- 主程序、相关探针和独立Release构建无警告；便携ZIP更新后的14项交付检查通过。

汇总报告：`tmp/native12-fallback-report.json`。原采样：`tmp/native-text12-bytes/report.json`、`strings.bin`。当前便携检查：`tmp/native12-fallback-portable-report.json`与`tmp/npe-portable-report.json`。

## 范围与继续工作

新36组参考来自原字形缓冲区和坐标，并非原完整LCD截图；原完整LCD回归仅指既有商店与仙术路径。诊断字符串末尾提供两个明确零字节，不能据此证明所有调用者的暖邻接字节或跨调用局部缓冲等价。右边缘裁切、其它字符模式、剩余格式器和调用者仍需具体核对；准确原声与完整跨NPE兼容继续。

## 重现

在仓库根目录执行，原输入版本与哈希见扩展字库记录：

```powershell
python npe-sdl/tests/reference_text12_bytes.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --physical-nand E:/qemu-dmcu-build/np560-from-package-20260918-v1/system.nand
python npe-sdl/tools/extract_np560_gbk_font.py tmp/NP560.PRG tmp/NP560_NAND_IMAGE.bin npe-sdl/assets/gbk12-native.bin npe-sdl/assets/gbk16-native.bin --controls npe-sdl/native12_control_glyphs.inc
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-native12-bytes-probe -j 4
npe-sdl/build/sdl-native12-bytes-probe.exe npe-sdl/assets/gbk12-native.bin npe-sdl/assets/gbk16-native.bin tmp/native-text12-bytes/strings.bin
```

`verify_native12_bytes.py`检查已完成日志、原参考绑定、字体保持及当前交付包，不自行重跑游戏或重新生成参考。

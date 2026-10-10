# 原扩展字库接入 C＋SDL

2026-10-10后续：[地图名驻留续读](np560_npe_map_name_tail.md)已补齐地图名称末尾首字节消费零尾字节后的暖读取。下方末尾所列该地图名缺口为本批当时范围；其它文字调用者与原完整LCD仍需继续。

2026-10-10。RPG使用的原C／D字库已补齐两段扩展表。正式程序只读取生成的字体资源，不加载PRG、NAND或执行DMCU指令。游戏文字仍来自当前NPE。

## 数据来源与地址

原PRG：`tmp/NP560.PRG`，SHA256为
`d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5`。
升级包原NAND_IMAGE槽：`tmp/NP560_NAND_IMAGE.bin`，保留512字节头，SHA256为
`44c3c0332ceab48e95202effc1a6c4f52a07384c4149becf039384ea0c0330ce`。

原`17:9244`选择字形，扩展表通过`28:12A4`读取；12像素字形再由原`29:C3C0`解包。

| 原字库 | 代码区间 | 槽数 | 原读取字节地址 | 每槽字节 |
| --- | --- | ---: | --- | ---: |
| C，12像素，第一段 | `81..A0 / 40..FE` | 6112 | `0262:BA9C` | 18 |
| C，12像素，第二段 | `A8..FE / 40..A0` | 8439 | `0264:685C` | 18 |
| D，16像素，第一段 | `81..A0 / 40..FE` | 6112 | `0266:B9BA` | 32 |
| D，16像素，第二段 | `A8..FE / 40..A0` | 8439 | `0269:B5BA` | 32 |

原查表包含尾字节`7F`，不能按现代GBK有效性检查直接拒绝这些槽。
这些地址是原服务的32位字节地址，不是PRG地址或NAND物理块偏移。

原卷基址`E93`、每块`20000`字节；相应厂制NAND的OOB标签为`FC4..FC9`，物理块依次为148、3224、2200、1176、150、3226。物理NAND每字的两个字节需交换，随后整个`0262BA9C..026DD49A`区间的727550字节与升级包源数据完全一致；升级包内偏移为原读取地址减`110000`，不额外裁去512字节头。
字库连续源区SHA256为`49ca6ea6e54b5ac998dc7e1a84ed421c5b87a7b16e5e9446b5691ae5d3c42627`。

## 生产实现

- 12像素每字18个输入字节：前12字节为各行低8位，末6字节依次保存相邻两行的高4位，低半字节在前。按原函数反转12位、保留原额外左移及16位截断，产生24字节行掩码。
- `gbk12-native.bin`保留`NPEFONT1`布局，原ASCII和已接入的常用原字形不变；补入扩展表，以及原C分支实际接受的`A1..AF / A1..FE`和`B0..FD / A1..FE`驻留槽。其它槽使用原`A1A1`备用字形。生成不再使用Noto来源的`gbk12.bin`。
- `gbk16-native.bin`升级为`NPEF16V3`，保留全部V2普通／控制字形数据及偏移，末尾追加14551个原闪存字形。渲染器按原191列／97列公式选择两段；原无效双字节备用行为保持。
- 12像素绘制和分页接受原扩展区的`7F`槽；无字形时先返回错误，修复原绘制器先读取未初始化步长的问题。
- 标题存档选择页原来只接受V1头，现同步接受生产V3头；数字字形和原画面不变。

| 文件 | 大小 | SHA256 |
| --- | ---: | --- |
| `gbk12-native.bin` | 579872 | `a7b1d94bfb8edc3f082c2a8dfba064c57d00fe2a98b2a9a996af5c521f931391` |
| `gbk16-native.bin` | 752488 | `c122ccf2562dc2c35f20ef0a6ea44f2c06ea8db1c4eaa669e2869f03ce598e43` |

## 验证与范围

`reference_extended_fonts.py`执行未改动的原取字算术及解包函数。诊断只替换闪存传输入口，把上述已验证原数据映射到外部RAM；取字后的LCD调用边界改为捕获坐标和字形缓冲区。原取字、原解包及所用复制／乘法函数入口逐字节检查。

- 150组12／16像素取字覆盖两段的首末、若干中间行、`7E/7F/80`边界、补入的驻留区域及`FEA1`备用字形，原地址、读取长度、坐标和字形全部一致。
- 原解包函数逐个处理全部14551个12像素扩展槽，349224输出字节全部一致。
- 16像素扩展表的465632字节与已验证原闪存数据全部一致。
- C与生产`draw_pixels`的150次实际SDL输出一致。此处参考来自原字形缓冲区及坐标，**不是原完整LCD或硬件时序参考**。
- 既有7709个12像素原字形、V2的286848个字形字节保持不变。
- 9组既有原16像素文字服务、标题／存档选择页、标题读档3幅原完整LCD与玩家／NPC状态、4组生产字体错误启动检查通过。主程序及三个受影响目标无警告构建。

首轮全字库探针30秒结束时仍在解包，已调整为55秒窗口并完成。随后发现诊断驻留字库背板长度不足以覆盖后部16像素汉字，已扩展至原完整表末尾；未修改原取字函数或用空白作为预期。

输出为`tmp/native-extended-fonts/report.json`及`tmp/extended-fonts-report.json`。本批补齐原C／D扩展字库；末尾首字节后的暖邻接状态、其它文字表现、原音频和各NPE完整运行仍需继续，整体完成度不因本批检查自动提高。

## 重现

从仓库根目录运行；PRG与NAND_IMAGE输入需为上述版本与哈希。

```powershell
python npe-sdl/tests/reference_extended_fonts.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --physical-nand E:/qemu-dmcu-build/np560-from-package-20260918-v1/system.nand
python npe-sdl/tools/extract_np560_gbk_font.py tmp/NP560.PRG tmp/NP560_NAND_IMAGE.bin npe-sdl/assets/gbk12-native.bin npe-sdl/assets/gbk16-native.bin
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl npe-text-test sdl-extended-fonts-probe sdl-title-load-probe -j 4
python npe-sdl/tests/verify_extended_fonts.py
```

# Noah NPE / BBK 9588

9588 原生 BDA 移植，保留上游 C 游戏逻辑与界面流程，平台层使用 SDK 公开 API。

## 安装

将 `NPE9588.bda` 放入设备应用目录；把整个 `NPE` 目录复制到 A 盘根目录，
字体路径必须为 `A:\NPE\assets\gbk12-native.bin` 和 `gbk16-native.bin`。
游戏 NPE 文件自行放入 A 盘任意目录，启动时通过系统文件选择器打开。
程序在 `A:\NPE\saves` 下按游戏指纹保存三个独立存档，日志为 `A:\NPE\NPE.LOG`。
建议将游戏放入 `A:\NPE`，文件选择器默认从该目录打开。

## 操作

方向键移动；确定键确认，在剧情/地图需要动作时映射为空格；返回键映射 F4。
触屏面板：ACT=空格动作，BACK/MENU=F4 打开或关闭系统菜单，PREV=P，NEXT=N，EXIT=退出。
ABC 切换英文输入键盘，PAD 返回按键面板；A/B/C/D 用于答题。
触屏采用深色圆角按键、方向图标和按下反馈；英文键盘按 QWERTY 错位排列，
确认键以薄荷绿突出，键盘切换键以浅蓝突出。字母和按键轮廓使用原生像素绘制。
装备页需要卸下装备时，可在英文键盘上输入 U。
原始 240×120 游戏画面显示于屏幕上部，保留像素比例。

## 构建

克隆独立应用仓库，初始化固定 SDK 子模块后运行：

```powershell
git submodule update --init sdk
./sdk/scripts/setup_toolchain.ps1
python bbk9588/build.py
python bbk9588/check_package.py
```

输出在 `dist/bbk9588`。需要 SDK 的 MIPS GCC 工具链和 Python。
构建清单记录所用上游版本与 BDA SHA-256；不依赖 SDL2 DLL 或宿主 C 运行库。
音频仍使用上游解码器，在同一线程协作调度，并转为 22050 Hz / 16 位单声道。
游戏主调用树使用独立 2 MiB 栈；NPE 与字体由固件堆分配，内存不足时加载失败。

## 验证边界

此移植沿用上游开发版的游戏兼容范围。模拟器验证不代表真机验证或全游戏通关。
测试记录见本目录 `qa/VERIFICATION.md`。
9588 移植层采用 MIT，固定上游基线未附带源码开源许可证；原机字库的再分发许可未明确授予。
安装包的 `SOURCE_NOTICE.md` 说明这些范围；不能将原机字库视为 OFL 字体。
构建包不包含游戏、完整固件、NAND 镜像或个人存档。

## 模拟器回归

`prepare_emulator.py` 从模拟器附带的官方压缩镜像生成本项目专用 NAND，传入
本地 NPE 游戏后部署 BDA 和字体。它拒绝覆盖已有测试 NAND。

```powershell
$emuRoot = Read-Host '模拟器目录'
$gameFile = Read-Host '游戏 NPE 文件路径'
python bbk9588/prepare_emulator.py --emulator-root $emuRoot --game $gameFile
```

用模拟器 `emu.web.frontend` 启动输出镜像，端口设为 8026。
使用模拟器附带的 Python 及 QEMU，启动时传入 `--boot-mode nand` 和此副本的
`--nand-image` 路径；不要连接到其他项目正在运行的 guest。
在《英雄传奇》新游戏地图上运行 `verify_emulator.py`，验证触屏菜单、两个存档槽、
物理方向键、移动与恢复后的完整存档一致性；`capture_audio.py` 检查实际 PCM 包。
这些脚本仅面向本项目独立 NAND，不用于设备上的个人存档。

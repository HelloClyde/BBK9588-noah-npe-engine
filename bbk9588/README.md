# Noah NPE / BBK 9588

9588 原生 BDA 移植，保留上游 C 游戏逻辑与界面流程，平台层使用 SDK 公开 API。

## 安装

将安装包中的 `应用` 和 `NPE` 两个目录复制、合并到设备 `A:\` 根目录，保持目录层级：

```text
A:\应用\程序\NPE9588.bda
A:\NPE\assets\gbk12-native.bin
A:\NPE\assets\gbk16-native.bin
```

安装包根目录的 `README.md` 是安装说明；`说明` 目录含许可证、来源、构建信息和验证资料，留在电脑即可。
游戏 NPE 文件自行放入 A 盘任意目录，启动时通过系统文件选择器打开。
后缀匹配不区分大小写，只使用一个 `npe` 过滤项，避免重复列出同一文件。
程序在 `A:\NPE\saves` 下按游戏指纹保存三个独立存档，日志为 `A:\NPE\NPE.LOG`。
建议将游戏放入 `A:\NPE`，文件选择器默认从该目录打开。

## 操作

方向键移动；确定键确认，在剧情/地图需要动作时映射为空格；返回键映射 F4。
十字方向面板支持点按和长按，中心 ACT=空格动作，MENU=F4，PREV=P，NEXT=N，EXIT=退出。
KEYS 打开字母/数字页，SYM 打开完整符号页，FN 打开 F1–F12、导航和修饰键页；PAD 返回。
包含全部 95 个可打印 ASCII 字符及 Tab、Backspace、Enter、Esc、方向/翻页/编辑键。
Shift 输入一次后自动关闭；Caps/Ctrl/Alt 再点一次关闭。A/B/C/D 用于答题。
V-/V+ 调节音量，MUTE 完全静音/恢复。音频诊断在 A:\NPE\AUDIO.LOG。
SYM/FN 不是中文输入法；当前不支持中文录入，游戏中文显示不受影响。
触屏采用深色圆角按键、方向图标和按下反馈；英文键盘按 QWERTY 错位排列，
数字、符号和功能键分页面提供；确认键以薄荷绿突出，键盘切换键以浅蓝突出。字母和按键轮廓使用原生像素绘制。
装备页需要卸下装备时，可在英文键盘上输入 U。
顶部的橙金色 NOAH NPE 图片标识嵌入 BDA，不需要外部图片资源。
点击标识显示系统 msgbox：“作者：HelloClyde”。确定关闭后回到原游戏/键盘页。
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
alpha.8 在固件消息等待、纹理转换与绘图前后补音，优先填充已可写的 PCM 队列。
`audio_decode.c` 与 `sound_sdl.c` 单独使用 `-O2` 编译，内联音频循环的整数运算辅助函数；
其他模块保持 `-Os`，不改变解码算法和换曲/循环等待。
顶部与软键盘缓存；仅游戏画面变化时上传和拷贝 240×120 区域，降低连续重绘负担。
正常退出后的 `A:\NPE\AUDIO.LOG` 新增最大供音间隔、超过 23 ms 的供音间隔次数、
最大解码回调耗时、完整/仅游戏重绘次数。供音间隔较长不等于已发生欠载；
系统作者消息框的阻塞时间不计入该间隔，原引擎换曲/循环等待仍保留。
游戏主调用树使用独立 2 MiB 栈；NPE 与字体由固件堆分配，内存不足时加载失败。

## 验证边界

此移植沿用上游开发版的游戏兼容范围。模拟器验证不代表真机验证或全游戏通关。
测试记录见本目录 `qa/VERIFICATION.md`。
9588 移植层采用 MIT，上游引擎采用 GPL-2.0-only；原机字库的再分发许可未明确授予。
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

使用 `verify_emulator.py --out <目录>` 可将本次回归结果写入单独目录，保留历史记录。

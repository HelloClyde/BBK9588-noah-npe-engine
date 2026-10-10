# 阿诺创世纪开场的跨节顺序读取

2026-10-10。新增原函数证据与定向检查，生产游戏代码未在本批修改。运行程序仍为独立 C11＋SDL2。

## 原版实际行为

《阿诺创世纪》第一场景声明71帧，首帧长度14410字节，而第14节只有475530字节，即33帧。此前严格元数据检查把第34帧超出本节记为待办，不能据此判定原引擎无法播放或文件残缺。

原 `41:ACBA..B0EA` 只读取第一份帧描述符，随后按首帧长度连续读取整个文件，不限制在第14节内。第34帧开始读取后续节的原字节；71帧之后文件位置为2341646，未到文件EOF。原函数正常返回、恢复栈并释放缓冲，未报告缺失ROM或不支持指令。没有补帧、替换图像或修改NPE。

原NPE SHA256：`0bd0e23f18461ce13e5745dba16c7acb1f9318a718d1671599eced6ccfc811a9`。原PRG SHA256：`d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5`。

## 对照与入口状态

`reference_scene_animation_runtime.py` 增加当前NPE、输出目录和原入口RAM采样参数。原装载、分配、绘图与释放函数逐段核对未修改代码；文件IO、显示提交、等待及声音服务使用已说明的诊断边界。开始前原RAM由原初始化函数与明确的目录输入构成。

第一轮C对照在音频目录缓存处失败：C资源构造器已经初始化这些缓存，隔离原场景入口没有执行相应声音目录初始化。没有删除比较区域或修改预期输出。第二轮在原函数调用之前采集完整入口RAM；两条隔离C路径从这份相同输入开始，先逐字节确认输入已恢复，再运行被测服务。原参考画面和结果未注入C渲染器。

| 路径 | 原版参照与结果 |
| --- | --- |
| 独立资源绑定 | 71帧完整128KiB状态、28800索引像素、读取位置／次数／失败计数及释放后状态一致 |
| 活World资源绑定 | 同71帧和最终状态一致 |
| 实际`play_scene_group`控制器 | 普通C构造，未注入上述入口RAM；完整71帧索引与实际SDL读回一致，正常结束并释放流 |

共71张原帧、213次C／SDL比较。报告为`tmp/arno-opening-report.json`；最终原参考为`tmp/arno-opening-native-aligned/report-opening.json`。第一轮初始化差异和诊断探针的Windows窄字符参数失败日志保留。生产程序使用SDL2main的UTF-8参数，后者不是生产程序的中文路径缺陷；对照使用字节相同的ASCII路径副本。

本批也重新构建并运行了此前失败的完整`enemy-action-probe`。其闲置角色战后存读状态与后续敌方行动检查当前全部通过；日志`tmp/enemy-action-current-build.log`、`tmp/enemy-action-current-check.log`。这项是旧失败的当前复核，不记为本批新增生产修复。

## 复现

```powershell
python npe-sdl/tests/reference_scene_animation_runtime.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --npe "D:/Downloads/NP560plus游戏全集/阿诺创世纪.npe" --output tmp/arno-opening-native-aligned --case opening --capture-input
Copy-Item -LiteralPath "D:/Downloads/NP560plus游戏全集/阿诺创世纪.npe" -Destination tmp/arno-opening-original.npe
python npe-sdl/tests/generate_scene_animation_runtime_fixture.py --report tmp/arno-opening-native-aligned/report-opening.json --npe tmp/arno-opening-original.npe --output npe-sdl/tests/arno_opening_runtime_native.h
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
cmake --build npe-sdl/build --target arno-opening-runtime-probe -j2
npe-sdl/build/arno-opening-runtime-probe.exe tmp/arno-opening-original.npe tmp/arno-opening-native-aligned/opening-input-ram.bin
```

新目标构建无警告。没有修改生产规则，因此未扩大到无关模块回归。

## 尚未证明

这是当前NPE第一场景的原装载／绘图及SDL控制器对照，不是完整原固件启动、物理LCD传输、原声或设备时序证明，也不代表《阿诺创世纪》全游戏动态兼容。其它场景、剧情、状态和音频继续按计划处理；整体粗估仍约80%。用户排除的原机存档互通、正常属性完整通关和SDL全程手动实玩保持排除。

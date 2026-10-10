# 音频预填之后的启播状态

2026-10-10。生产程序补入原启播时的结束标志清理；整体仍粗估约80%，音频模块仍为部分完成。

## 具体差异与修改

原资源入口先经`25:E24C/E2FA`和`2F:66F2`预填双PCM半缓冲。后者会按零输出或游标大于有效尾指针置`89.11`。正式启播的`08:31AC`在`31BE`清除此标志，再设置`89.4`并执行`4702`。

旧C在预填前初始化驱动标志，预填后直接开始等待，遗漏了`31BE`的清理。当前`sound_start_wait()`在成功预填之后、静音判断和启动等待之前清`89.11`，保留`89.3`回调活动位及其它当前控制位。预填失败仍进入原有错误清理路径；候选头数量的播放结束判断没有在本批改成未知的原codec耗尽行为。

这是已确认的驱动状态差异。没有找到当前18份NPE自然开场因该标志提前结束的实例，因此不宣称本批修复了已复现的自然音轨截断。

## 原指令参考

`tests/reference_audio_start_boundary.py`执行未修改的原`08:31AC`、`4702`、Timer2等待和`24:9A1E`。代码总线逐字节检查相关原服务，所有8条路径返回调用者、恢复栈，无缺失ROM、不支持指令或非法控制流。

| 明确提供的启播前89 | 原启播后89 | 原Timer2计数 |
| --- | --- | ---: |
| 5050 | 5050 | 200 |
| 5850 | 5050 | 200 |
| D850 | D050 | 200 |
| D858 | D058 | 200 |
| 5C50 | 5450 | 200 |
| 5854 | 5054 | 200 |
| 5850，静音 | 5050 | 0 |
| D858，静音 | D058 | 0 |

输入覆盖有限／注册回调、回调活动、暂停、生产者活动和静音。输入标志与零DAC目标由外围诊断明确提供，**未执行codec、自然EOF或资源重开**。计数不能扩大为完整渐变／物理启播时间。

原PRG SHA256：`d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5`。原参考保存于`tmp/audio-start-boundary-native/`。

## C接入与短回归

`sdl-start-boundary-probe`使用生产声音代码和真实SDL dummy设备，先实际解码、准备当前NPE的两个候选PCM半缓冲，再在明确的预填边界提供上述诊断标志。逐项比较原启播状态、静音位与等待计数；511字节分块跨越等待结束，检查已准备的两半、增益、服务计数和前台完成令牌没有被重复推进。

旧源码在7条带结束位的路径中失败；修复后8条均零差异。其它7项必要短检查通过：初始PCM、前台请求等待、结束门控、完整候选PCM服务、暂停／恢复、服务忙标志及声音专项。22段／10,486帧候选PCM保持，主程序和相关目标构建无警告。

旧源码对照之后的一次增量构建沿用了旧诊断二进制，仍显示7条失败。保存该日志后明确重编译诊断目标，新日志8条通过；没有修改原参考或生产规则来消除这个构建问题。失败与最终日志均保留。

独立汇总：`tests/verify_audio_start_boundary.py`；结果：`tmp/audio-start-boundary-report.json`。原文件、候选解码器、人物规则和自身存档代码保持。

## 重查

在仓库根目录：

```powershell
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
$env:PYTHONIOENCODING = 'utf-8'
D:/ProgramData/miniconda3/python.exe npe-sdl/tests/reference_audio_start_boundary.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-start-boundary-probe -j4
npe-sdl/build/sdl-start-boundary-probe.exe tmp/tjfy-world-probe.npe tmp/audio-start-boundary-native/states.bin
```

汇总检查还读取保留的修复前日志、7项短回归记录和构建日志，不是仅运行新探针就自动补齐全部历史证据。

## 剩余工作

准确原声解码／PCM参考、完整DAC渐变输出与实际循环／结束行为继续待完成。当前的状态边界证据和候选PCM回归不能替代这些缺口。其它事件、共享状态、时序及跨NPE后续兼容仍按根`plan.md`推进。

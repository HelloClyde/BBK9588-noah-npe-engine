# 音频启停DAC状态（2026-10-10）

## 原版规则与生产接入

`sound_sdl.c/h` 已把原DAC数值服务接入播放生命周期，保留左右当前值、保存值、输出控制／GPIO字和最后一次REP的NOP数量。

- 停止：原 `08:475E` 保存左右当前word，调用 `08:3462` 逐单位到零，清GPIO31.6；50个Timer2计数后关闭REG72使能。清理 `08:3292` 再清两个当前word。SDL停止／清理阶段已按这个状态顺序处理。
- 普通新播放：原 `08:31F0..31FC` 在88.4清零时读取首个已准备PCM字1784，覆盖4A68／4A6A。不能把普通重播目标理解为上一轨末值。SDL当前NPE的单声道路径使用预填后 `half[0][0]`。
- 启动：原 `08:4702` 清当前DAC、开启REG72；非静音先完成原等待，再置GPIO31.6并调用 `08:33FA` 向目标逐单位恢复。静音跳过GPIO、等待和恢复，但仍执行清DAC和输出使能操作。
- 暂停恢复 `08:320E` 保留旧目标，原88.4分支亦保留分别的左右目标。本批数值服务可处理这些目标；不宣称SDL已新增完整暂停接口。
- SDL单声道适配器在交付完整16位word后更新逻辑双路当前值；只交付首字节不更新。显式替换音轨保存该值，后续新播放再按新轨首字初始化目标。自身游戏存档布局不增加这些播放器字段。

原基础反汇编为 `tmp/npe-audio-lifecycle-original-source.asm`，已有[8条渐变数值轨迹](np560_npe_audio_lifecycle.md)证明符号比较、左右顺序和仅右路写入才执行REP。参数95表示96次NOP；本批仍按该服务推进状态。

## 本批原程序证据

`reference_audio_dac_state.py` 使用固定原PRG与隔离QEMU，调用原31AC／320E／3262以及原DAC、Timer2和清理服务。执行前按CODE核对5个原区域。诊断输入包括第一PCM字、保存／当前左右值、驱动／静音标志和GPIO；未载入NAND，没有宿主写运行中客机RAM。

**明确边界：** 24:9A1E和24:9B02传输启动被替换为RETS，避免物理DTU改写捕获的DAC状态。该参考不含codec、实际NPE播放、完整传输或可听见的渐变时间。

10条路径涵盖正／负首字、独立目标保留、两种静音启动、普通／静音停止、空闲停止、停止后新启动和暂停恢复。原结果明确确认首字覆盖、静音仍清DAC、静音停止仍等待50、空闲停止保留状态，以及200／250／50／0计数。报告为 `tmp/audio-dac-state-native/report.json`；原输出生成 `audio_dac_state_native.h`。

`sdl-dac-state-probe` 调用实际生产的DAC与等待阶段，10组当前值、保存值、控制字、GPIO和计数均与原参考一致。另通过当前NPE的实际候选PCM，检查首字目标、非零完整word／奇数字节保留、替换音轨、重复停止和静音状态。

## 相关回归

改动前声音、初始双半缓冲和前台请求等待3项基线通过。接入后以下5项受影响检查通过：

1. 声音：22段资源、原数值／服务参考、候选PCM、尾部与跨回调分块。
2. 初始PCM：4组增益／静音状态、两半保留、输出与分块。
3. 前台等待：原200／250／450／50／0／0计数、请求完成通知和真实SDL唤醒。
4. 问答声音：6条原路径与6条初始停止路径。
5. 场景动画：281次既有原C／实际SDL画面对照、有限声音与事件后继。

生产主程序和6个相关目标构建无警告／错误。已有候选PCM与参考未更换。审计脚本 `verify_audio_dac_state.py` 读取结果生成 `tmp/audio-dac-state-report.json`；生产差异及原文件保存在 `tmp/audio-dac-state-*.diff` 和 `tmp/audio-dac-state-before/`。

## 剩余音频工作

**本批完成的是DAC状态与控制顺序，可听见的渐变波形尚未接入。** REP数量没有直接换算成SDL样本；仍需确定渐变执行与输出时间的关系，再输出对应波形。等待阶段继续输出静音，不能宣称已经恢复完整渐变或循环间隔。

原QEMU指令时钟代码也明确是部分周期模型，不能把诊断PLL或未分类指令的默认估计当作原机声学时间。准确同NPE原解码／PCM、实际耗尽与完整循环／结束仍需新证据；不以候选解码或其它来源WAV替代这些要求。整个引擎仍未完成，整体粗估约80%。

```powershell
python npe-sdl/tests/reference_audio_dac_state.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe
python npe-sdl/tests/generate_audio_dac_state_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-dac-state-probe -j 4
npe-sdl/build/sdl-dac-state-probe.exe tmp/tjfy-world-probe.npe
python npe-sdl/tests/verify_audio_dac_state.py
```

# NPE 模式8：选择题与填空题原版对照

日期：2026-10-07。生产实现仍是 C11＋SDL2；原厂 PRG／WASM 只用于离线采样，不参与运行。

## 本轮修正

旧 SDL 实现把题干放在地图对白中、重复添加 A～D 前缀，并用英文桌面消息框答题；没有填空输入。现在直接读取当前 NPE 的题干、五个字符串及控制字节7，显示原版独立题目页。

- 选择题使用12行字体，ASCII推进6像素、汉字12像素；题干矩形(0,10)..(240,82)，四项位于(0/120,84/96)。选项已有 A. 等前缀，不另造文字。四向和 I/J/K/L/M 按原规则移动／确认，边界不循环，A～D不直接交卷。
- 非零输入类型采用原文本输入服务。最多32个ASCII字节；大小写、空格和允许的标点原样保留，不做修剪或大小写转换。原删除键是0x89。正确项0取A字符串，其他取B，按原strcmp逐字节比较。
- 当前 NPE 共5320题，946题类型0、4374题类型1。该统计只说明此文件，不推定其它NPE。
- 固定题只更新正确／错误统计及NPE后续模式／事件，取消了错误的连续答题计数递增。固定题不弹出答对／答错窗口。
- 分类题显示原固件中文答题提示，名义等待800ms后恢复题面；三次答错及F4按原奖钱流程退出。低16位运算产生的65521等奖励按原程序保留，不改为扣钱。
- 超时条件为原4000ms周期累计超过7次；清空输入首字节、选择上下文置5，先提示时间到，再记错。SDL按名义周期实现，不宣称物理时基已复刻。
- 输入页下方“英文”是固件共享的24×16位图，不来自游戏标题或题库。由原NAND镜像0x23251BB的10字节位图头及192字节4bpp数据提取，载入原引擎显示的384像素全部相同；数据SHA256为`2c260624988b9bfb3e727c7aa36ed6645f6af325a5f944e273f2dbc6259339d1`。生产只嵌入这192字节，没有PRG／NAND运行依赖。

## 原始依据

- 原服务raw2C:B8C2..D5C6：控制字节、问答类型、布局、方向网格、ASCII输入、计数及后续。
- raw46:E552：12行字的题干矩形格式化；raw4C:A0B6..A1E8：有界文本。
- raw4A:1EEE..20B8：正奖励分支及低16位计算、金额上限、状态复位。
- 运行时32:3528／3544／355A／356E／3588／359A字符串位于raw4F同地址，PRG字内交换后解码；不能把运行时bank32直接当PRG原始bank32。
- raw46:91FA／49:3CBE／50:2DD6／4B:0AF6：英文键盘初始化及共享位图。

源码转储保留于`tmp/quiz-original-source.asm`、`quiz-box-source.asm`、`quiz-text-score-source.asm`、`quiz-keyboard-source.asm`、`quiz-keyboard-label-source.asm`及`quiz-keyboard-blit-source.asm`。跳转表数据不作为指令证据。

## 验证范围

原参考采样器`npe-sdl/tests/reference_quiz_flow.mjs`从已有真实UI存档恢复，在隔离RAM中指定模式8／题号、计数和边界条件，保留当前NPE及原服务。字母和原辅助键使用7A76注入；该方式验证服务输入，不证明物理键盘扫描或消抖。输出为`tmp/native-quiz-flow.json`及对应LCD索引帧。

19条短流程包括固定题正确／错误、边界及I/J/K/L/M、长题干、填空正确／大小写／删除／空格／32字节限制、分类0／1继续答题、第三次答错、F4奖励、超时、65535统计上限。分类使用初始RNG11，对应原选题272／3397及后续853／5303；填空进入后原前题缓存复位0。

`tests/sdl_quiz_flow_probe.c`包含实际`main.c`，通过SDL事件接口交付按键、实际纹理与软件renderer提交并读回240×120整屏。19条流程137次提交C索引像素及SDL颜色均零差异，按阶段核对忽略键无提交，并核对终态模式、事件、上下文、题号缓存、输入、计数和金额。原参考结束在首个地图初始化提交，后续NPC周期不计入该服务；早期较长采样发现了536像素NPC变化，未用它修改问答规则。

`tests/quiz_flow_probe.c`另直接核对70张固定题中间状态及题面，夹具由`tests/generate_quiz_fixture.py`从原参考生成。两种探针不依赖原WASM执行C端。

全量构建无警告，ctest5/5；两项问答、标题、新游戏、暖读档、存档提示、脚本入战及行走入战共8项回归退出0。旧综合探针的第一处遭遇数值断言只差共享RNG，诊断日志证实开音乐为第二次LCG；数值夹具隔离音乐后通过该段，原音乐开关另由84次实际SDL入战提交匹配。综合探针仍在1327行、问答以前的地图81行走断言失败，原因未核实，不计为通过。最终构建／回归状态见`tmp/quiz-service-report.json`。复现入口：

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_quiz_flow.mjs
python npe-sdl/tests/generate_quiz_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target quiz-flow-probe sdl-quiz-flow-probe npe-sdl -j4
& npe-sdl/build/quiz-flow-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-quiz-flow-probe.exe tmp/tjfy-world-probe.npe
```

## 仍未完成

- 固定题统计分离、NPESAV16／旧档迁移以及新游戏／暖恢复／冷读档的问答驻留范围已在后续完成，见[问答统计与驻留证据](np560_npe_quiz_state.md)。后续 19 条答题流程在独立非零统计下重新核对全部 22 字，仍为 137 次实际 C／SDL 整屏零差异。
- 后续已核对原服务只读取8个控制字节、紧接8／9未读取，四份不同NPE题库仅见类型0／1；补齐返回模式1的保留槽重播，见[问答声音与路线证据](np560_npe_quiz_audio_and_farmer_route.md)。其它输入类型／输入法、其它后续链、物理时基／重复键仍待完成。
- 其它引擎缓存、冷启动、声音PCM及跨NPE适配继续。本轮不证明整个模拟器完成。

正常属性完整通关和SDL从头到尾实玩已取消，沿用已有诊断剧情结局；没有重跑全程。

后续记录：地图81首个失败已定位为夹具阻挡预期错误并修正，39次实际main.c／SDL短路线及开场短段通过；旧长综合探针后续未重跑，详见[路线证据](np560_npe_quiz_audio_and_farmer_route.md)。

# NPE 问答控制、返回音乐与农夫短路线

日期：2026-10-07。运行程序仍为 C11＋SDL2；原 PRG／WASM 仅作离线参考。正常属性完整通关和 SDL 从头到尾实玩已按用户要求取消，剧情连通性沿用已有诊断结局。

## 题目控制字节

原服务 raw2C:C5D4..C6B6 在五个 NUL 结尾字符串后依次读取：正确项1字节、正确后续模式1字节／事件2字节、错误后续模式1字节／事件2字节、输入类型1字节，共8字节。进入布局前没有读取紧接的第8／9字节（从0编号）；不能把这两字节猜作声音或新题型。raw2C:C6BC／C94E 以类型0进入网格，其它值进入文本；后续 D5AA／D5B0 的清理还区分恰为1，所以未采样的其它类型不能直接宣称完全支持。

`tmp/quiz-controls-inventory.json` 检查5个路径、4份不同哈希的 NPE：

| 文件 | 题数 | 类型0 | 类型1 | 第8／9字节(0,0) | (0,1) |
| --- | ---: | ---: | ---: | ---: | ---: |
| 天降风云诊断原文件 | 5320 | 946 | 4374 | 1035 | 4285 |
| sound-cross-game-0 | 5385 | 930 | 4455 | 1100 | 4285 |
| sound-cross-game-1 | 5385 | 930 | 4455 | 1100 | 4285 |
| sound-cross-game-2 | 5385 | 930 | 4455 | 1100 | 4285 |

`hero-original-for-inventory.npe` 与最后一份相同，未重复计算覆盖量。没有空字符串、其它输入类型或现有 C 解析器拒绝的题目。这是资源结构检查，未验证另三款的完整答题 UI。当前 C 仍要求字符串后至少10字节并校验正确项／后续模式；本轮没有为未出现的8字节记录或非法值更改解析规则。

原反汇编见 `tmp/quiz-original-source.asm`。raw50:2FF2..3014 是注册回调，不是播放声音；raw4F:8E5C..8EBA 恢复输入覆盖层，另存于 `tmp/quiz-sound-tail-source.asm`。这里按具体调用解析物理地址，不把固定 bank 偏移当成通用映射。

## 问答返回音乐

问答进入和选择阶段保留现有地图播放；返回模式1后，原通用地图控制器 raw37:1340／4E:A3F4 停止旧播放并从头循环播放保留的地图槽，没有重新抽曲。

旧 C 主程序仅画回地图，没有重播；计算答题候选状态时，后端声音回调还可能在反馈之前提前播放。现在 `main.c` 在计算候选状态时暂时隔离声音观察器，反馈结束、应用状态并回到地图时调用 `npe_world_sound_map_resume`。保留槽和共享 RNG 不变，未增加游戏／地图／题号特例。

`tests/reference_quiz_audio.mjs` 通过真实 UI 快照及诊断 RAM 指定题号／计数／音乐设置，采集6条原流程：固定题答对／答错的音乐开与关、分类 F4 退出、分类第三次答错。原事件见 `tmp/native-quiz-audio.json`：开启时仅在地图返回发出 stop→play（段20＋保留槽，循环）；关闭时无声音请求。

`tests/sdl_quiz_audio_probe.c` 包含实际 `main.c`，对照上述6条流程，再检查6个 C 初始停音场景。后6个不是额外原厂采样。使用真实 SDL 播放器和已解码声音，暂停设备以检查游标：反馈前无重播，返回后游标从128复位0，关闭时保持128。C 一次播放请求替换旧播放，对照的是原 stop→play 的重新起播效果，不声称观察器输出两个事件。固定题、F4、三错分别检查0／800／2400ms名义反馈截止和 RNG；原 PCM、真实时钟相位及其它后续模式链不在本次范围。

## 地图81农夫路线

旧综合探针把 `RRURRUUU` 的每一步都预期为可走。原地图入口虽然写入(12,13)，底边视口调整后实际为(12,12)；七次移动已经到(16,9)，再上被农夫阻挡。原短路线 `RRURRUUUU` 的最后两键都阻挡，现有 C 移动规则符合原版。修正测试夹具末步预期，没有修改生产移动规则。

`tests/reference_farmer_route.mjs` 从真实首图快照开始，用诊断 RAM 将一个原出口指向地图81、进度3及原出生坐标；当前 NPE 地图／NPC／缓存由原引擎加载。关闭音乐、冻结目标 NPC 巡走以隔离路线，这是局部入口测试。原入口一帧和9个按键38次提交，产物为 `tmp/native-farmer-route.json` 和对应索引帧。

`tests/sdl_farmer_route_probe.c` 先用 C 原地图装载绘制面初始化，再连续调用实际主程序 SDL 按键分派，按键之间不重置保留画面。入口加38次提交共39次 C 索引／SDL 读回整屏零差异，并逐键核对模式、地图、进度、方向和视口。

诊断 `--walk-preview` 首屏仍有174像素差异：其通用预览会裁剪显示负原点 NPC，原地图装载忽略该记录。以上39次从原地图装载面开始，不能当作预览首屏差异已修复。

新增 `tjfy-opening-probe` 只运行原综合源码的开场短段，检查农夫进度4及存读档后即退出。本次退出0。旧综合探针的这处首个失败已解决；后续长路线没有重跑，不计整个综合探针通过。

## 构建与回归

定向构建主程序、问答／路线及相关目标，无 warning／error；日志 `tmp/quiz-controls-build.log`。7项相关回归均退出0，汇总 `tmp/quiz-controls-regressions.json`：问答画面、问答存档状态、问答声音、农夫 SDL 路线、开场短段、音乐菜单、运行中读档。ctest 5/5，日志 `tmp/quiz-controls-ctest.log`。没有执行正常属性或 SDL 全程游玩。

NPESAV16 未改；原 NPE／PRG 哈希保持，汇总于 `tmp/quiz-controls-report.json`。本轮不增加既有静态／战斗截图计数，也不把声音请求检查计为像素检查。

复现已完成的局部验证：

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_quiz_audio.mjs
python npe-sdl/tests/generate_quiz_audio_fixture.py
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_farmer_route.mjs
python npe-sdl/tests/generate_farmer_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target sdl-quiz-audio-probe sdl-farmer-route-probe tjfy-opening-probe -j4
& npe-sdl/build/sdl-quiz-audio-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-farmer-route-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/tjfy-opening-probe.exe tmp/tjfy-world-probe.npe
```

其它输入类型／输入法、其它问答后续链、绘图／脚本／对象驻留缓存、冷启动及种子来源、物理时基／重复键、PCM／增益和其它 NPE 的完整机制继续待办。整体模拟器尚未完成。

后续更新：实际初始入口174像素差异现已修复，农夫探针撤销手动补入装载面，39次从实际首屏开始匹配；见[实际入口证据](np560_npe_initial_map_display.md)。上述首轮范围保留作为历史记录。

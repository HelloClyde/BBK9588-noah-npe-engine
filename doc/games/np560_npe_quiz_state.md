# NPE 问答统计、存档与驻留会话

日期：2026-10-07。运行实现为 C11＋SDL2，原 PRG／WASM 仅用于离线参考。

## 原版行为与实现

原统计区为 `7684..76AF` 的 44 字节：分类 0～9 各有正确／错误两个无符号 16 位统计，固定题另占 `76AC/76AE`。旧 C 实现把固定题写入分类 5，现已分为 22 个独立统计值；答题仍按原上限 65535 饱和。

原读档 `3C:4098..40CC` 恢复 44 字节；新游戏 `46:D1A8..D1BE` 清零这一区域。对应源码在 `tmp/quiz-save-codec-original-source.asm` 和 `tmp/new-game-lifetime-original-source.asm`。

| 入口 | 22 个统计值 | 问答会话 |
| --- | --- | --- |
| 运行中系统读档／标题读档 | 从槽位恢复 | 保留运行中的值 |
| 运行中标题新游戏／空槽新游戏 | 全部清零 | 保留运行中的值 |
| 冷启动后读取槽位 | 从槽位恢复 | 保持冷初始零值 |

会话核对字段为 `7AD0` 连续计数、`7ADA` 错误次数、`7ADE` 前题、`7A52` 分类／固定题号、`7AE0` 输入类型、`7AD8` 输入长度及 `77DC` 起前 32 字节。它们不属于原存档范围。C 暖恢复和暖构造保留这些值，冷解码不再恢复旧 C 档错误保存的会话字段；题目入口会重新准备当前 NPE 的题面。

原缓冲第 33 字节与地图数字绘制临时内存重叠，本次地图返回后为 ASCII `7`，不能当作有效输入字符。C 保存前 32 字节并拥有第 33 字节 NUL，不把这个临时别名差异宣称为完整 RAM 字节匹配。`quiz_question_index` 是 C 的缓存，暖入口保留、题面有效标志失效；没有将它当作额外原 RAM 字段的证据。

同时修正标题读档地图绘制：使用原地图装载绘制顺序和驻留背景色，保留供标题控制器的一次提交。暖标题背景为 0 时原先上下边缘差 1920 像素，已复现并修复；冷标题背景为 2 的原流程继续匹配。

## NPESAV16 与旧档

新写出的 C 独立格式为 `NPESAV16`，不兼容原机槽位文件。保留 V15 字段偏移，在尾部增加固定题正确／错误两个 16 位值；原先的五个会话字节写零并保留为占位。游戏指纹、负效果低字、第四地图状态、追击和动态地形等既有字段保持其语义。

读取接受 V1～V15。V3～V15 的分类 5 曾混入固定题统计：

- 当前 NPE 分类 5 没有题目时，把旧合计迁移到固定题统计，分类 5 清零。
- 分类 5 有题目或目录无法确定时，旧档缺少可分离历史，保留旧合计在两个独立字段中。随后分别更新；这不能恢复已经丢失的两类历史次数。
- V1/V2 没有问答统计，使用冷初始值。所有旧格式的会话占位字段被消费但不恢复。

迁移依据当前 NPE 目录，不使用游戏名称、地图号或题号例外。

## 验证与边界

`tests/reference_quiz_state.mjs` 经原 UI 保存、读档和构造，采集系统、标题、新游戏、空槽、真正重新创建原运行时后的冷读档及冷边界读档六条短流程。各统计设置不同值以识别覆盖，驻留字段使用可区分的会话及 32 字节输入。产物为 `tmp/native-quiz-state.json`、原持久数据和 10 张索引帧。

`tests/sdl_quiz_state_probe.c` 包含实际 `main.c`，执行对应 SDL 菜单输入，核对全部 22 个统计、上述会话字段和前 32 字节；10 次 C 索引帧和实际 SDL 读回整屏均零差异。原冷参考实际重启；C 同时检查冷解码和独立运行中标题入口，不把后者当成原固件冷启动复刻。

存档局部检查覆盖 V16 独立统计往返、V1～V15 各历史前缀、空／非空分类 5 迁移以及有正确校验和但截短或多出尾字节的 V16 拒绝；失败不改变已有世界。非空分类 5 迁移夹具只重分配题库目录计数，不证明另一款完整游戏。

问答服务 19 条短流程重新采样，分类 5 与固定题统计同时非零，全部 22 字逐阶段／终态比较通过；70 张固定题面、9 次忽略键以及 137 次实际 `main.c`／SDL 整屏继续匹配。全量构建无警告，ctest 5/5；三项问答及九项标题／构造／恢复／槽位／缓存／地图仙术／路径／失败提示回归均退出 0。日志与状态汇总见 `tmp/quiz-state-report.json`。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_quiz_state.mjs
python npe-sdl/tests/generate_quiz_state_fixture.py
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_quiz_flow.mjs
python npe-sdl/tests/generate_quiz_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j4
& npe-sdl/build/sdl-quiz-state-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/quiz-flow-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-quiz-flow-probe.exe tmp/tjfy-world-probe.npe
ctest --test-dir npe-sdl/build --output-on-failure
```

其它问答输入类型、控制字节 8／9、声音与输入法、物理时基、其它驻留缓存、冷启动来源及跨 NPE 机制继续待办。旧综合探针在地图 81 行走断言的失败尚未解决，本轮未重跑或计为通过。正常属性完整通关和 SDL 全程实玩继续取消；整个模拟器尚未完成。

后续更新：原控制字节8／9读取范围、模式1返回音乐及地图81首个夹具断言已核对／修正，见[问答声音与路线证据](np560_npe_quiz_audio_and_farmer_route.md)；长综合探针后续未复验，其它输入类型、输入法、时基、驻留缓存、冷启动、PCM及跨NPE仍待办。

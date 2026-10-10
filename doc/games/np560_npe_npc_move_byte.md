# NPC移动的字运算、坐标255和循环回绕

> 2026-10-10后续更正：零距离移动的错误拒绝已修复。原DIV在除数0时设置V，原服务跳过零步循环并继续后继；旧WASM execution_fault不能证明原机陷阱。xab实际新游戏完成12个入口／743次SDL提交、进入地图13。详见[NPC零距离移动](np560_npe_npc_move_zero.md)。以下保留当轮历史结果。

> 2026-10-07后续更正：旧292次样本尚未覆盖延迟的地图返回初始化；现补到297次。旧“没有额外地图提交”的结论撤回，详见[NPC返回初始化](np560_npe_npc_move_return.md)。以下保留当轮历史记录。

## 原服务

反汇编 `tmp/npc-script-move-source.asm`：

- `3C:8D78..8D90` 两轴相减保留16位，再按有符号字处理。
- `33:1724` 的绝对值仍为字；-32768取绝对值后仍为有符号-32768。比较两轴、选择步数和方向也按有符号字。
- `3C:8DF4/8E08` 预先做有符号除法；零步数沿用已记录的原参考除零故障。
- `3C:9032..9044/9072..9084` 位置写回字节。255是合法NPC坐标。
- `3C:90AA..90C2` 先把递增步数写回字节，再和有符号步数字比较。正步数至少255时，循环索引255→0→1，不会越过上限。

C现在显式折叠为有符号16位差值，绝对值保留-32768的回绕；显示循环采用原字节索引。负步数跳过移动循环但保留方向写入和后续调度。数字接口对非终止循环返回 `NPE_NPC_MOVE_NONTERMINATING`（-3），不修改世界、不生成完成状态；带显示的服务继续循环，直到主机关闭或回调取消。

## 原动态对照

原参考脚本 `tests/reference_npc_move_byte.mjs` 在原首图上诊断驻留目标及NPC起点，保持当前NPE的地图、图像和其它NPC。巡走／音乐／遇敌被关闭以隔离服务。这些是服务样本，不是自然剧情或全程游玩。

| 样本 | 原结果 | LCD提交 |
| --- | --- | ---: |
| (2,9)→(65535,9) | 有符号差-3，终点(255,9) | 7 |
| (6,2)→(6,65535) | 有符号差-3，终点(6,255) | 4 |
| (253,9)→(255,9) | 合法坐标255 | 3 |
| (255,9)→(254,9) | 合法起点255 | 2 |
| (0,9)→(255,9) | 步数255回绕到0／1，仍模式18 | 275 |
| 两轴差-32768 | 方向2，位置不变，直接后续1／0 | 1 |

共6条、292次实际main.c及SDL索引／颜色整屏零差异；五条终止、一条非终止片段。非终止样本观察过255→0→1，并在新步数1的绘制后截取；原每次advance可能已更新下一阶段RAM，初版截取只拿到该步擦除帧，已补足绘制帧。实际C在相同275提交后请求主机关闭，保持模式18／事件1／进度1，没有执行后续。关闭请求以后的主机刷新不作为原LCD证据。

原JSON `tmp/native-npc-move-byte.json`；像素 `tmp/native-npc-move-byte-*-*.idx`；采样日志 `tmp/npc-move-byte-native.log`、`tmp/npc-move-byte-edge-native.log`、`tmp/npc-move-byte-counter-native.log`。`--edge-only`／`--counter-only`只增补相应案例；无参数时独立生成完整6条。

C等待次数从原记录位置变化提取：非终止片段有257步，关闭打断最后一次等待，前256次名义141ms。五条终止路径的20个NPC终态、方向及后续匹配；C随机状态11保持，不新增原RNG定位或物理时基证据。

## 主机位置表示及NPESAV17

运行时NPC覆盖改用16位容器，`NPE_NPC_POSITION_NONE`（65535）表示不存在覆盖；实际坐标写入仍按原字节，255不会误作“回到NPE默认位置”。地图装载、传送、出口和暖恢复仍清除覆盖，采用当前NPE记录。

NPESAV17保持V16所有字段偏移，末尾追加4字节、小端20位有效标记，一位对应一对NPC坐标。有效位为1时旧字节区域的255是坐标；为0时字节区域必须是FF／FF，表示无覆盖。高12位为零；不匹配、短尾和多余尾部事务拒绝。驻留移动缓存继续不序列化。

V1～V16继续读取；旧位置字节255保留旧版本的“无覆盖”意义。旧版本无法区分255坐标和哨兵，不能推测或恢复这种历史。旧V5～V11的事件坐标迁移保持。

局部检查：V17的(255,9)、(6,255)、(255,255)及范围外坐标读写；暖恢复继续重建NPE；坏有效位／保留位及失败事务；V16的有效普通坐标与FF／FF迁移；问答探针覆盖V1～V16前缀、V17统计及尾部拒绝；地图四状态旧V14和非法尾部继续匹配。UTF-8 NPE／字体／存档路径及实际标题也通过。

## 构建与进度

定向10目标构建无warning／error，日志 `tmp/npc-move-byte-build.log`；新探针最终构建 `tmp/npc-move-byte-probe-final-build.log`。九项相关回归退出0，汇总 `tmp/npc-move-byte-regressions.json`，报告 `tmp/npc-move-byte-report.json`。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_npc_move_byte.mjs
python npe-sdl/tests/generate_npc_move_byte_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target sdl-npc-move-byte-probe npe-sdl -j6
& npe-sdl/build/sdl-npc-move-byte-probe.exe tmp/tjfy-world-probe.npe
```

原NPE／PRG哈希保持。其它NPC／图组、真实连续后续和声音、缺失NPC／冷空槽及其它RAM越界行为、绘图交错、真实硬件除零／时基／输入扫描、完整冷启动／PCM和跨NPE机制仍需实现。模式18及模拟器整体未完成；没有重跑ctest、长综合探针或取消的正常属性通关／SDL全程实玩。

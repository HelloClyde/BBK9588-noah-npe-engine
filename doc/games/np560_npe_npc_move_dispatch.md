# NPC脚本移动：返回调度和地图外目标

> 2026-10-07后续更正：旧39次样本尚未覆盖延迟的地图返回初始化；现补到48次。旧“没有额外地图提交”的结论撤回，详见[NPC返回初始化](np560_npe_npc_move_return.md)。以下保留当轮历史记录。

## 原规则与修复

模式18反汇编见 `tmp/npc-script-move-source.asm`。`3C:8BCC..8BD4` 对无效低事件字节设置模式0；`3C:910A..915A` 结束时写入缓存后续模式和事件，随后进入通用调度。当前C的数字接口已经包含简单模式收束，不将其即时模式1等同于原服务函数内每条指令的状态。

七条原短流程核对事件0、6、256，后续模式0／事件77、模式25／事件78、模式1／事件0，以及事件261选择驻留第五槽。每条无效事件只提交入口地图；每条三步移动有7次提交，返回正常地图时无额外提交。原版结束后还观察3次advance，没有新的LCD提交。现有C整体控制器行为匹配，未因内部模式收束增加重画。

`3C:8D78..8E0A` 直接计算缓存目标与当前位置之差，没有检查地图宽高。两条原诊断短路径从(53,9)到(56,9)、从(6,28)到(6,31)；当前地图宽55、高30。它们各执行三步、四次提交，并按缓存后续1／0返回。所有绘图调用继续按屏幕范围处理。

修复前C因地图边界拒绝第一条路径；实际main.c先提交入口地图，再报“不支持”，随后提交额外静态画面，提交数断言失败，见 `tmp/npc-move-dispatch-before-fix.log`。`world.c`现移除该地图宽高限制。

主机存档此前也拒绝范围外NPC覆盖，导致这种运行状态可以保存却不能解码。`save.c`移除宽高校验，保留现有成对255表示“没有覆盖”的规则。NPESAV16字节格式不变。两条范围外状态的host冷解码保留坐标；运行中恢复按既有原规则重建当前NPE的NPC坐标。后两项为C读写一致性验证，未新增原版存档采样。

## 对照范围

- 原参考 `tests/reference_npc_move_dispatch.mjs`；JSON `tmp/native-npc-move-dispatch.json`；帧 `tmp/native-npc-move-dispatch-*-*.idx`。
- 两批原采样日志 `tmp/npc-move-dispatch-native.log`、`tmp/npc-move-dispatch-geometry-native.log`。`--geometry-only`只增补两条坐标路径；无此选项时独立生成全部9条。
- 诊断参数只改模式、事件、驻留槽、NPC起点、视口、巡走标志、音乐及遇敌开关。地图、图像和其它NPC来自当前原NPE；不是自然剧情或完整游玩。
- `sdl-npc-move-dispatch-probe`通过实际main.c，39次完整索引面和实际SDL像素均零差异；9条终态20个NPC、事件和进度匹配。逐提交RAM可能已跨多个原调用，不作为逐指令状态证据。
- C随机状态11保持、名义等待参数15／141ms验证；原RNG未新增动态定位，不宣称本轮原随机流或物理时基对照。

定向构建6目标无warning／error，日志 `tmp/npc-move-dispatch-final-build.log`。五项回归退出0：本轮9条／39次、既有14条／87次移动画面、12组驻留缓存、标题读档3提交、开场短段。汇总 `tmp/npc-move-dispatch-regressions.json`，报告 `tmp/npc-move-dispatch-report.json`。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_npc_move_dispatch.mjs
python npe-sdl/tests/generate_npc_move_dispatch_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target sdl-npc-move-dispatch-probe npe-sdl -j6
& npe-sdl/build/sdl-npc-move-dispatch-probe.exe tmp/tjfy-world-probe.npe
```

## 剩余边界

范围外短路径已验证，不代表全部16位目标语义完成。缓存字的有符号差值、原字节步数循环的255回绕、主机NPC覆盖的255哨兵冲突仍需处理；当前C不能用这些结果证明坐标255及非终止路径完全复刻。其它NPC／图组、真实连续后续事件与声音、缺失NPC／冷空槽、绘图服务交错、真实硬件除零／时基／输入扫描和其它NPE机制继续待办。

原NPE／PRG哈希与NPESAV16格式保持；没有重跑ctest、长综合探针或用户取消的正常属性通关／SDL全程实玩。模式18和模拟器整体未完成。

## 后续：字／字节回绕与NPESAV17

有符号字差值、-32768绝对值回绕、NPC坐标255及字节循环回绕已接入，6条样本292次实际C／SDL匹配；NPESAV17区分255与无值并兼容旧档。见[NPC字／字节回绕](np560_npe_npc_move_byte.md)。非终止路径为运行片段，不计通关；其它服务边界继续待办。

# NPC 周期移动：C 服务与 SDL 接入

日期：2026-10-07。直接翻译原厂移动与碰撞规则，图像、NPC 记录和地图仍取当前 NPE。

## 原厂执行顺序

原始 PRG 的 `3C:D5C8..D5E8` 在 type 4、地图模式、交互标志为零时，先调用追击服务，再调用 `25:8392`（source `42:8392..8778`）。因此追击选择战斗后，本次 NPC 服务仍执行。既有 SDL 四秒计时器现在接入这两步；关闭菜单／对白时到期事件的消耗方式保持既有处理。原计时服务的 256 个 64 Hz 计数证据见[追击计时](np560_npe_chase_group10.md)。

移动服务按槽 0～19 的顺序执行：

1. ID 为零，或文件 +7 的移动标志不等于 1，跳过。
2. 调用共享固件 RNG，以返回值 `& 3` 选择下、右、上、左。
3. 检查尝试到达的位置；通过则移动一格，保存方向。
4. 受阻时保留原位置，继续抽取，直至方向与尝试方向不同；只转向，不再尝试移动。

移动不按屏幕可见性或剧情范围筛选。碰撞 `40:6C3C..70DE` 使用线性格号 `Y*width+X`，依次检查八个对象、其余十九个 NPC、十二个出口以及主角的屏幕坐标加滚动偏移。阻挡坐标不按 ID、剧情范围或对象领取标志筛选。最后仅允许地形图块编号 **0～11**，不能复用主角更宽的可行走图块集合。NPC 按槽依次更新，后面的槽会看到前面槽的新位置。

RNG 沿现有原厂递推，NPC、遇敌、战斗和选曲共用一份状态；未添加游戏／NPC 编号特例。

## 原厂记录和验证

参考运行器使用原 PRG 与真实冷启动后的第一张地图快照。首次实际周期前后比较 RAM，按原厂 LCG 递推查找唯一非平凡的状态变化，动态定位 resident RNG；未把宿主地址写入生产代码。

原始八个移动 NPC 的一次更新，RNG 从 `11` 到 `2189689227`，恰好八次调用。其后保持同一原厂实例，对指定字段作诊断布置、设置 RNG，并等待真实周期事件；不再用可能移动 RAM 缓冲区的快照恢复延续旧地址。原始 NPE／PRG 文件不写回。

| 对照 | 组数 | 覆盖内容 |
| --- | --- | --- |
| 未改 NPC 的首个周期 | 1 | 八个 NPC 实际移动、共享 RNG、全部二十槽记录 |
| 四方向畅通 | 4 | 从当前 NPE 自动寻找四邻均畅通的位置，四方向各移动一格 |
| 主角、NPC、隐藏 NPC、ID 为零的槽、对象、出口 | 6 | 阻挡后原地转向及重抽取；不自行过滤记录 |
| 不可行地形 | 1 | 原地转向 |
| 剧情范围外的移动 NPC | 1 | 仍参与移动及 RNG |
| 移动标志为 2 | 1 | 跳过，无 RNG 调用 |
| 左边界 | 1 | 不减为负坐标，按原剩余检查与转向 |

**15 组、300 个 NPC 位置／朝向记录以及每组 RNG 尾状态全部通过。** C 存读档保持移动坐标、方向和 RNG，仍使用 NPESAV15。

首个周期后的完整地图在 C 和实际 SDL 各一次比较 **零像素差异**。该原厂参考帧与上一轮待修的 `empty-title-cold-after.idx` 完全相同，因此原先后续状态的 **536 像素差异已消除**。SDL 测试经过实际主程序的新游戏入口和计时循环，在地图选曲之后注入采样到的周期前 RNG 状态；这不证明启动播种或首个事件的时基相位已一致。

未到期、菜单抑制、非地图模式、过期事件不积压和 Uint32 回绕检查通过；追击的静止计时／移动／战斗／冷却回归通过。新游戏首图、六条存档／标题路径、三槽、场景 281 次、逃跑 68 次（含四次地图返回）通过。全量构建无警告，ctest **5/5** 通过。已有画面统计不重复相加，不进行用户取消的长时通关验收。

## 剩余范围

- 原厂向右边界的比较允许 X 恰为 width，碰撞读下一行的线性图块；C 保留该线性语义。超出整张地图的读访问涉及原厂堆中的相邻内容，C 当前拒绝读取；该边界及坐标 255 的覆盖表示仍需继续核对。
- 原厂擦除与前后景重绘服务 `44:E84C`、`4B:79FA` 后续已完成24组邻接／重叠／边缘对照及15条主程序路径，见[NPC保留画面重绘](np560_npe_npc_compositor.md)。本项移动记录的完整画面范围仍是原始首周期；追击交错及其它重绘路径继续核对。
- 对象领取／脚本移动等服务之后的坐标缓存联动、真实 64 Hz 相位、冷启动 RNG 和其它 NPE 机制仍待完成。整体模拟器未标完成。

## 复现与证据

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_npc_periodic.mjs
python npe-sdl/tests/generate_npc_periodic_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-npc-periodic-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-chase-flow-probe.exe tmp/tjfy-world-probe.npe
ctest --test-dir npe-sdl/build --output-on-failure
```

输入快照由 `reference_empty_title.mjs` 和探索脚本 `tmp/new-game-state-explore.mjs` 从原厂标题空槽构造。原厂记录 `tmp/native-npc-periodic.json`，帧 `tmp/native-npc-periodic-0.idx`，只读反汇编 `tmp/npc-periodic-original-source.asm`；夹具 [npc_periodic_native.h](../../npe-sdl/tests/npc_periodic_native.h)。日志 `tmp/npc-periodic-{native,all-build,probe-build,check,ctest,chase-regression,initial-regression,save-regression,scene-regression,flee-regression,slots-regression}.log`。

本轮原文件 SHA-256 未改变：NPE `DF4B52DF6B69E1105A5EA6100E43F35F832ED859C10CB8714879E44AD4D8CB40`；PRG `D555609E03ECE17F42C1881BEEB88361572F18621161DE67BC9DC765857089F5`。离线参考运行器只用于验证，C／SDL 成品不依赖它。

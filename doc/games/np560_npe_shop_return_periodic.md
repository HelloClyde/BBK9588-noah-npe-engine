# 商店返回后的驻留背景色与 NPC 周期

更新：2026-10-07。原版 PRG／WASM 只用于离线采样，生产程序仍为 C11 + SDL2，直接读取当前 NPE。

## 原服务与修复

原商店初始化 `3F:B4C2..B4CE` 调用 `03:41BC`，参数为前景 15、背景 0；买入和卖出共用这一入口。`4F:1E6E..1E98` 关闭列表时设置地图模式及地图初始化标志，没有重新设置图形颜色。地图初始化和 NPC 绘制尾部调用的 `03:4214` 按驻留背景色清除上下四行，源码见 `4D:1A54..1B68`。

C 原来的商店返回路径复制先前保留的零色边缘，所以关闭当下的画面已经匹配。但是 `NpeWorld.lcd_background` 仍为初始值 2，下一次 NPC 周期重新清边缘时产生 1920 个像素差异。本轮先在实际主程序中复现这一失败，再修改 `npe_world_shop_begin`，记录原商店入口留下的背景色 0。关闭商店调用地图初始化后直接采用该颜色，更新地图边缘缓存，移除这个入口复制旧边缘的替代步骤。

背景色是运行中的图形状态，暖读档仍沿用前一轮的驻留字段保留逻辑；不加入 NPESAV15。没有按地图、NPC、物品或游戏编号加入特例。

## 原版采样范围

基线为真实启动、新游戏、原厂 UI 保存后的快照 `tmp/native-title-load-saved.snapshot`。每次恢复快照后，通过自然 NPC 周期找出活动 RPG RAM，并从同次周期的实际 LCG 变化识别独立驻留堆中的 RNG 地址，不假设它与 RPG RAM 的相对位置。

随后只设置商店服务的模式、事件、上下文、刷新标志及输入。第四条卖出路径另把物品 45 的诊断库存设为 2，用于打开非空数量页；空卖出路径保持原库存。原 NPE、NPC、背景色和 RNG 没有注入。输入先运行原商店初始化，再关闭列表或取消数量页，最后等待一个自然 NPC 周期。这是受控服务入口证据；实际 NPC 对话进入商店的既有 45 次对照另有回归，不能把本轮入口说成从开机按键一路走到商店。

| 局部流程 | 原版／C／SDL 提交数 |
| --- | ---: |
| 买入列表→关闭→NPC 周期 | 3 |
| 买入数量页→取消→关闭→NPC 周期 | 5 |
| 空卖出列表→关闭→NPC 周期 | 3 |
| 非空卖出数量页→取消→关闭→NPC 周期 | 5 |
| 合计 | 16 |

每条的返回图和周期图上下四行均为 0。周期产生 524 个真实 NPC 像素变化，7 个 NPC 的记录发生变化；全部 13 个非零 NPC 的编号、坐标及朝向均作比较。每条周期 RNG 从 2189689227 到 1661376854，恰为 9 次原 LCG 调用。

## 实际 C／SDL 验证

`sdl-shop-return-periodic-probe` 包含实际 `main.c`，从未修改的 NPE 创建世界，再按观测到的输入状态设置位置、NPC 巡走覆盖、库存、钱及 RNG。首屏由生产商店入口绘制，关闭及周期均使用生产控制器；SDL 软件渲染器读回整个 240×120 LCD。

16 次 C 整屏和 SDL 读回逐提交零差异。每阶段结束比较模式／事件／上下文／地图／进度、位置／视口／方向、钱／库存、音乐开关及槽、商店选中项／首尾／行／数量／确认选择／说明起止、13 个非零 NPC 及共享 RNG。原 RAM 在主机推进结束后读取，不宣称每次提交瞬间的 CPU 状态。

C 探针只控制周期截止时间来触发事件，未证明真实设备的物理时基相位。自然周期的基线识别过程也不计入上述 16 次提交。

来源与复现：

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_shop_return_periodic.mjs
python npe-sdl/tests/generate_shop_return_periodic_fixture.py
cmake --build npe-sdl/build --target sdl-shop-return-periodic-probe -j 4
& npe-sdl/build/sdl-shop-return-periodic-probe.exe tmp/tjfy-world-probe.npe
```

源元数据：`tmp/native-shop-return-periodic.json`；原版日志：`tmp/shop-return-periodic-native.log`；源指令：`tmp/shop-return-periodic-original-source.asm`、`system-slot-edge-clear-original-source.asm`。初始失败：`shop-return-periodic-before-fix.log`；修复结果：`shop-return-periodic-fixed.log`，最终回归：`shop-return-periodic-regression-shop-return.log`。

## 最终构建与回归

全量构建退出0且无警告，ctest 5/5通过。10项相关局部探针均退出0：新增商店返回周期、原商店关闭／数量UI／窗口／边界、NPC对话入口、系统菜单返回、暖存读档、商店状态读档及NPC绘制。原窗口82次、数量UI44次、边界60次、关闭33次、NPC对话入口45次及系统菜单返回77次整屏对照继续通过。最终状态与原文件哈希见 `tmp/shop-return-periodic-report.json`。

## 后续范围

其它模式背景色、绘图面和脚本／对象／完整 LCD 缓存，原厂写失败及损坏槽，冷继续及真实时基／启动 RNG，声音 PCM／增益／时序、更多商店／战斗边界及跨 NPE 机制仍待完成。正常属性完整通关和 SDL 全程实玩已取消，不恢复为验收门槛。

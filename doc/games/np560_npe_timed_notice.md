# 原定时提示：按键提前结束与上下文初始化

日期：2026-10-07。运行时为纯 C11／SDL2；原 PRG 和参考 WASM 只用于离线核对。

## 原服务与修正

原提示服务 `20:4050 → 4F:D530 → 4E:F282` 设置定时器，并在等待循环中同时检查：

1. `4F:4C34` 的键盘扫描退出条件。
2. `type=4,code=0xF0` 的计时消息。

`44:E528` 读取键盘矩阵 `01:7160`，全释放值为0xFF；有键时调用 `4E:BE70` 的消抖／重复判定，通过后在 `44:E5A4..E5AC` 将 `A2:2A5C` 置1。`4F:4C34` 检查该值及 `2A7A` 的特殊抑制状态。因此此前“普通键忽略，只能等超时”的结论遗漏了退出分支。

证据来自原 PRG 直接解码：`tmp/leave-timer-original-source.asm`，引用定位辅助为 `tmp/notice-flags-references.asm`。原参考实际输入另验证方向、确认、取消和F4均提前结束提示。

`main.c` 新共用 `wait_timed_notice`：普通 SDL 按键结束当前提示，结束键在服务内消费；无输入时等待名义2000ms；窗口关闭请求送回外层并停止后续。未知的内部 SDLK_UNKNOWN 不作为游戏键。队伍变化、存档失败、地图物品拒绝、任务物品拒绝、地图法术拒绝和战斗物品拒绝共用该等待规则，画面和后续事件仍走各自原入口。

本轮没有模拟整机电源／唤醒抑制或硬件消抖相位；原64Hz时基、重复键相位及SDL墙钟偏差仍属于时基待办。原 `stepsPerSecond=0` 的接口输出不能证明墙钟等价。

## 原厂动态对照

### 连续离队与战斗

`reference_leave_battle.mjs` 的原UI加载及未修改当前NPE入口保持不变。七组无输入完整流程、七种单提示按键、两组连续提示只按一次键，共16组104次原LCD提交。

实际 `main.c` 的事件分派、提示及战斗四段首屏，104次 C／SDL整屏零差异。无输入七组逐提交比较126个已建模角色字段、模式／上下文及NPC；按键组的一次 `advance` 可结束多个阶段，其RAM仅为整次调用结束状态，故只比较每组最终状态，不把该样本当成各提交的中间状态。所有入战紧凑记录与人数／目标核对通过。

原连续两次离队仅在第20次 `advance` 按确认或F4：第一提示立即恢复，第二提示继续等待，到第127次才恢复。因此一次输入只结束一个提示。C受控时钟于第一提示250ms送键，第二提示独立等待2000ms；检查输入只消费一次，所有8次画面提交及最终状态匹配。250ms是C注入时刻，不是原硬件计时换算。无输入另检查2000ms和Uint32回绕；窗口关闭停止后续沿旧局部探针继续通过。

数据：`tmp/native-leave-battle.json`、`native-leave-notice-input.json`、`native-leave-notice-chain-input.json`、`native-leave-notice-steps.json`及对应`.idx`；生成器 `generate_leave_battle_fixture.py` 不使用C生成原参考。

### 存档读取失败

`reference_empty_save.mjs --notice-keys` 使用原系统空槽入口，确认／F4两组各有提示、底图恢复、返回地图三次原提交；按键在第20次调用结束提示。采样到返回地图即停止，避免把随后NPC周期提交计入提示服务。

`sdl-save-feedback-probe` 使用两组实测底图及提示／恢复参考，四次实际C／SDL提交匹配；六条系统／标题主程序路径、无键超时、确认／F4提前结束及关闭窗口通过。原厂真正写入失败及损坏槽元数据仍未独立验证，保存失败文字继续是原源码支持的C／SDL检查。

数据：`tmp/native-empty-save-notice-keys.json`和`native-empty-notice-key-16/64-*.idx`。

## 回归暴露的初始化遗漏

任务道具原 `npc-dialogue` 三页画面匹配，但结束时C误进模式4、上下文246。定位到 `npe_world_init` 未初始化后来增加的 `mode2_context`，其值来自未初始化栈内存；NPC对白结束将它误判为商店上下文。旧探针还用Return推进保留式NPC对白，现改为该服务原确认键Space。

原真实启动／新游戏／UI保存快照，在F4前后 `7A68` 均为0（`tmp/native-notice-cold-context.json`）。C冷初始化补零；任务道具探针先将整个世界填0xCC，再确认新游戏上下文为0及正常续接。21组原规则、11张原整屏、三页对白及后续状态通过。成功暖恢复仍显式保留主机 `mode2_context`，没有把冷初始化规则套到暖恢复；18条实际SDL存读档及商店驻留状态回归通过。NPESAV15格式未增加字段。

## 构建与结果

全量构建退出0且无警告，ctest5/5；12项相关探针全部退出0：离队、模式10、失败提示、地图物品、任务物品、地图法术、战斗物品、暖恢复、商店状态恢复、标题首次加载、队伍缓存及脚本入战。

日志：`tmp/notice-input-all-build.log`、`notice-input-ctest.log`、`notice-input-final-*.log`。报告：`tmp/notice-input-report.json`。原NPE／PRG SHA256未改变。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_leave_battle.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_leave_battle.mjs --timing-only
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_leave_battle.mjs --chain-input
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_leave_battle.mjs --cold-context
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_empty_save.mjs --notice-keys
python npe-sdl/tests/generate_leave_battle_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j4
& npe-sdl/build/sdl-leave-battle-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-save-feedback-probe.exe tmp/tjfy-world-probe.npe
```

其它服务缓存、原厂存档错误、完整冷启动、实际时基、PCM／增益、更多战斗边界及跨NPE仍未完成。正常属性完整通关和SDL从头到尾实玩保持取消；现有剧情到结局结果保留，不重复全程。

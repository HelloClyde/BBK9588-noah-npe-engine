# NP560 NPE：系统菜单无效输入、音乐返回与 NPC 周期

更新：2026-10-07。生产为纯 C11／SDL2，原厂 PRG／WASM 仅作离线参考。

## 原厂参考及证据边界

参考使用此前真实标题 boot→新游戏→原厂 UI 存档得到的 `tmp/native-title-load-saved.snapshot`。关闭音乐的基线通过原厂系统菜单实际确认产生，没有直接改 RAM 音乐标志。

每次恢复快照后，通过真实 F4 输入使 RPG 模式从 1 变 6，以此识别活动 RAM，避免把序列化副本当作活动记录。原运行时恢复可分别搬迁 RPG RAM 和驻留堆；早期假定二者固定相对地址的 RNG 采样失败，未作为证据。最终每组在记录完整路径之后再等待一次自然 NPC 周期，用原 LCG 状态变化独立找到当组 RNG 地址，然后读取冻结的路径前／后 RAM。

C 使用观测到的入口 LCD、NPC 和 RNG 状态隔离菜单服务。四组原厂返回后的周期提交在 C 中通过一个待处理截止时间复现；这验证控制器顺序、NPC／RNG 和画面，不证明 SDL 与设备物理 tick 相位一致。后续用于定位 RNG 的自然周期不计入路径的 77 次提交。

## 输入及场景保留

`4B:B67C..B704` 分派方向、确认和菜单键；原厂实际输入确认：主菜单、系统子菜单、系统选槽中的 Left／Right／普通 Cancel 均无显示提交，F4 返回地图。音乐弹窗的这三个输入也忽略。

旧 C 对无效按键重新提交菜单，并把普通 Cancel 当作退回；第一条原厂对照以“额外提交”失败。现仅处理该上下文接受的按键，并记录完整菜单状态与选择信息，没有变化时不重画。记录缓存由真正的绘图操作失效；再次打开菜单仍绘制。

新增菜单进入前场景保留缓冲。普通 F4 和音乐弹窗取消按 `50:15A8..15D6` 恢复这幅场景；弹窗绘制不覆盖场景。音乐确认按 `37:D010..D024` 的 map-init 返回，沿用已修复的原地图初始化入口。所绘文字和位图来自当前 NPE，未替换为改编文案。

## 背景色及返回后的周期

`03:4214→30:1A54`（源 `4D:1A54..1B68`）按驻留图形背景色清除矩形；系统菜单 `49:0080..008A` 设背景色 0。此前 NPC 绘制尾部和地图初始化仍固定清成 2，系统返回后下一次 NPC 周期产生 1920 像素差异。

`NpeWorld.lcd_background` 现记录运行中的背景色。地图初始化默认 2、系统菜单入口设 0；NPC 尾部与地图初始化清边缘时使用该值。暖读档保留图形驻留颜色，冷主机解码仍初始化默认值；没有把该字段加入 NPESAV15。

原 dispatcher `3C:D5C8..D5E8` 在回到模式 1、出口选择为零时处理 type4：先追击再 NPC，共享 RNG；`D71A` 提交，随后重启周期。C 不再用其它同步对话的处理方式吞掉模式 6 返回后的待处理周期，周期更新也清除“已绘图但等待后续提交”的旧标记，使该幅地图真正提交。更新后的边缘同步到现有地图转换缓存。

四组原厂长菜单路径各有“恢复／返回地图→NPC 周期”两次提交，第二幅与第一幅相差 536 个实际 NPC 像素。每组共享 RNG 从 11 到 2189689227，恰为 8 次原 LCG 调用；C 坐标／朝向／RNG 及两幅完整画面均匹配。其余五组路径未消耗共享 RNG。

## 实际主程序验证

`sdl-system-menu-return-probe` 包含真实 `main.c`，使用独立存档路径和未修改 NPE。关闭音乐的 C 基线也通过真实系统菜单确认产生。

| 路径 | 原厂提交数 |
| --- | ---: |
| 主菜单无效键及 F4 | 2 |
| 系统子菜单无效键及 F4 | 6 |
| 选槽无效键及 F4、随后周期 | 8 |
| 音乐开→开，含弹窗无效键 | 10 |
| 音乐开→关 | 9 |
| 音乐关→开 | 9 |
| 音乐关→关、随后周期 | 11 |
| 音乐开、取消后周期 | 11 |
| 音乐关、取消后周期 | 11 |
| 合计 | 77 |

77 次 C 整屏与 SDL 读回逐次零差异；12 个无效输入没有提交。每个阶段结束检查模式／事件／地图／进度／坐标／视口／方向／钱、音乐开关及保留槽、13 个非零 NPC 和共享 RNG。一个主机推进可能产生两次原提交，RAM 状态在推进结束后读取，因此不宣称它是每次提交瞬间的 CPU 状态。

来源：`tests/reference_system_menu_return.mjs`、`generate_system_menu_return_fixture.py`、`system_menu_return_native.h`、`sdl_system_menu_return_probe.c`；原元数据 `tmp/native-system-menu-return.json`、日志 `system-menu-return-rng-fixed-native.log`。源码另见 `system-menu-timer-original-source.asm`、`system-slot-menu-original-source.asm`、`system-slot-edge-clear-original-source.asm`。

失败：`tmp/system-menu-return-before-fix.log`（无效输入额外提交）、`system-menu-return-ui-fixed.log`（周期边缘差异）；修复后 `system-menu-return-color-fixed.log`。采样地址假设失败保留在 `system-menu-return-rng-native.log`。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_system_menu_return.mjs
python npe-sdl/tests/generate_system_menu_return_fixture.py
cmake --build npe-sdl/build --target sdl-system-menu-return-probe -j 4
& npe-sdl/build/sdl-system-menu-return-probe.exe tmp/tjfy-world-probe.npe
```

## 最终构建与相关回归

最终全量构建退出0且无警告，ctest 5/5通过。12项相关局部回归全部退出0：菜单返回、系统槽返回、标题加载、音乐菜单、NPC周期、NPC绘制、出口选择、出口对话、暖恢复、商店状态恢复、三槽及存档反馈。最终77次逐提交证据见 `tmp/system-menu-return-regression-menu-return.log`，汇总见 `tmp/system-menu-return-report.json`。

探针控制返回时的待处理周期截止时间，只验证该事件的处理顺序及结果；不宣称设备物理时基一致。原NPE／PRG哈希不变。

## 尚待完成

其它模式及菜单的背景色／绘图面生命周期、更多周期和菜单交错、脚本／对象／完整 LCD 缓存；实际原厂写入失败及损坏槽；冷继续和真实时基／启动 RNG；声音 PCM／增益／时序、商店／战斗边界及跨 NPE 机制。整体未完成；不恢复取消的正常属性通关或 SDL 全程实玩。

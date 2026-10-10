# 实际主程序初始地图绘制

日期：2026-10-07。生产程序仍为 C11＋SDL2，当前 NPE 直接提供地图、角色及 NPC 资源。原 PRG／WASM 仅作离线参考。

## 问题及修正

上一轮农夫路线探针在首次等待输入时手动用 C 原地图装载面替换了诊断预览首屏，因此39次匹配只能证明替换后的短路线。去掉该替换、从实际入口首个提交开始比较后，复现 C 索引和 SDL 读回均差174像素，见 `tmp/map-entry-before.log`。

根因是 `run` 首次显示模式1、尚无保留地图面时调用通用 `draw_world`，其旧合成按Y排序并裁剪显示负起点 NPC。原地图装载服务 raw4C:6AB8..6BF8 按地图→静止主角→20个NPC槽→4个怪物槽绘制，NPC服务和原mode2不把负起点裁入屏幕；原末尾恢复上下四行。既有 `npe_render_world_load` 已实现这条原路径。

现在主循环首次显示模式1、还没有地图保留面时使用 `npe_render_world_load`，提交后保留这张实际入口画面。已有有效保留面的新游戏／读档后续继续使用缓存，未修改通用合成函数、移动、碰撞或RNG规则，没有地图／NPC／游戏编号特例。

依据为 `tmp/walk-exit-map-draw-source.asm` 及 `tmp/chase-packed-blit-source.asm`，参见[原行走与地图装载](np560_npe_walk_display.md)和[NPC合成](np560_npe_npc_compositor.md)。本次仅修正初始主程序入口，未据此宣称其它脚本重绘服务已完成。

## 实际入口验证

`tests/sdl_farmer_route_probe.c` 已移除手动绘制、替换纹理和重设保留面的步骤。开始绘制之前仅设置与原夹具一致的诊断音乐和方向；首屏直接来自 `run`，随后连续SDL方向输入，按键之间不替换画面或缓存。

沿用上一轮原引擎真正装载地图81的参考 `tmp/native-farmer-route.json` 和索引帧，原采样的控制出口／进度、关闭音乐及冻结巡走条件保持。入口1次加9键38次提交，共39次实际C索引／SDL整屏读回均零差异；状态逐键检查七次移动及两次阻挡。日志 `tmp/map-entry-regression-sdl-farmer-route-probe.log`。

定向构建主程序、路线、新游戏、标题读档、暖读档和视口6个目标，无warning／error。5项相关回归退出0：

- 农夫实际入口与连续路线39次提交；
- 既有新游戏构造／驻留状态；
- 既有真实标题占用槽读档；
- 运行中存读档18个控制器／音乐／RNG场景；
- 普通／边缘视口实际主程序94次提交。

构建日志 `tmp/map-entry-build.log`，回归汇总 `tmp/map-entry-regressions.json`，完整状态及原文件哈希 `tmp/map-entry-report.json`。本轮没有重新运行ctest或长综合探针，也没有正常属性完整通关或SDL全程实玩；取消的两项验收保持取消。

复现本项已完成的局部检查：

```powershell
cmake --build npe-sdl/build --target npe-sdl sdl-farmer-route-probe sdl-new-game-lifetime-probe sdl-title-load-probe sdl-live-restore-probe sdl-walk-viewport-probe -j4
& npe-sdl/build/sdl-farmer-route-probe.exe tmp/tjfy-world-probe.npe
```

NPESAV16及原NPE／PRG文件保持。其它脚本／对象／LCD驻留缓存、其它问答类型及输入法、完整冷启动／种子来源、物理时基／重复键、PCM／增益和跨NPE机制仍待完成。

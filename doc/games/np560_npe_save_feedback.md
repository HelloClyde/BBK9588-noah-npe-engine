# 存读档失败反馈与标题空槽

日期：2026-10-07。C11 + SDL2 运行时；原厂参考只用于离线核对。

## 原厂行为

- 系统菜单读档失败调用 `03:4050`，文字为固件 `31:45EE`（源码 `4E:45EE`）的“读取进度失败!”，参数 2000。结束后设置地图模式和初始化标志，开音乐时重播保留槽。
- 保存失败调用同一服务，使用 `31:45E0` 的“保存进度失败!”；成功和失败都返回地图。
- 标题选中空槽走 `3A:9000` 的初始化分支，开始新游戏；占用槽解码失败走 `3A:9076..90DE`，重新准备槽选择界面并停音，没有系统失败弹窗。

源码：`tmp/save-feedback-original-source.asm` 与 `tmp/music-menu-draw-input-source.asm`。提示服务链为 `20:4050→32:D530←4F:D530→31:F282←4E:F282`。原稿的字符串是 DMCU 字序，读取原 PRG 后交换字内字节，得到 GBK 原文；生产程序保存原字节，不依赖 PRG。

提示外框包含边界 `(63,48)..(177,72)`，黑色索引 0；内框 `(64,49)..(176,71)`，边色 15、内部 5，16 点文字从 `(68,52)` 绘制。来源 `3E:FD80` 服务，其宽度来自六个双字节汉字及一个单字节叹号，共 13 个半字单元。

定时服务 `50:463E→31:7286←4E:7286` 将毫秒换为 64Hz 计数。等待循环检查计时消息 `type=4, code=0xF0`，也有 `4F:4C34` 的提前退出条件。此前方向、确认和菜单键不会提前关闭提示的结论已撤回，见下方补充。主机实现等待 2000ms；真实 tick 相位仍不由主机近似证明。

## 动态参考

`tests/reference_empty_save.mjs` 使用原厂快照，经真实系统菜单进入空第一槽。两组覆盖开关 0／1，每组捕获三个提交：提示、恢复进入前底图、地图。`tmp/native-empty-save.json` 记录状态和声音；提示在第 0 个主机逻辑 tick，恢复底图在第 103 个、地图在第 105 个。开音乐第 104 个 tick 重播保留槽，关闭时没有声音事件。每组恢复底图与进入前像素完全相同。

`tests/reference_empty_title.mjs` 从真实启动运行到标题，按 `[Down,Confirm,Confirm]` 选择空第一槽；得到新游戏地图，保存 `tmp/native-empty-title.json` 和四次地图提交。它用于确认空槽分支；原开场的包装音频事件不作为本轮声音等价性证据。

作废实验保留在 `tmp/empty-save-unsupported-slot-experiment.*`：把保存槽改为零没有触发写入失败，不算失败提示参考。直接把战斗快照改为标题后选择空槽的夹具未准备好构造资源，得到错误图像，也不作为标题初始化证据。动态写入失败和标题占用槽损坏参考仍未完整采集。

## 实现与验证

- `npe_render_save_failure` 绘制原文字及边界，底图来自当前保留 LCD。
- `show_save_failure` 提交提示、等待、提交恢复底图；普通按键忽略。SDL 关闭请求保留给外层处理，不继续返回地图。
- 系统空槽／损坏档读取失败与写入失败均接入该提示，结束后关槽选择和子菜单，再按原地图初始化分支恢复音乐。
- 标题缺失文件开始新游戏。当前 C 存档存在但解码失败时保留槽选择、回到第一槽并停音，避免误当作新游戏。这个主机文件判断用于 C 格式；原厂依据槽位元数据，损坏档等级显示及分类尚待核对。
- 标题新游戏及标题空槽共用已有新游戏入口，角色／地图起点取当前 NPE。

`sdl-save-feedback-probe`：系统读失败提示 C 整屏与原厂零差异；实际 SDL 的提示与恢复底图两次零差异。保存提示由原字符串及同一服务源码支持，SDL 与 C 输出相同，未称为独立原厂写失败画面对照。受控主机时钟检查 2000ms、不提前按键结束、窗口关闭；五条实际主程序输入路径覆盖系统空档、损坏 C 档、模拟写失败、标题空档开始新游戏、标题损坏 C 档保留选择。

三槽回归已改为原空槽行为：标题空第一槽开始新游戏后，经系统菜单加载第二槽再移动和覆盖。18 组存读档音乐检查继续通过。

全量构建无警告，ctest 5/5、音乐弹窗20个状态回归通过；日志 `tmp/save-feedback-all-build.log`、`tmp/save-feedback-ctest.log`、`tmp/save-feedback-menu-regression.log`。原 NPE／PRG 的 SHA256 与修改前相同。

```powershell
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_empty_save.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_empty_title.mjs
& npe-sdl/build/sdl-save-feedback-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-slot-flow-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-live-restore-probe.exe tmp/tjfy-world-probe.npe
```

日志：`tmp/save-feedback-check.log`、`tmp/save-feedback-slot-regression.log`、`tmp/save-feedback-live-regression.log`，原厂 `tmp/empty-save-native.log`、`tmp/empty-title-native.log`。NPESAV15 未改变，不宣称支持原机存档格式互读。

## 仍有的差异

上一轮原厂标题空槽首图曾与C相差553像素，后续状态相差780像素。现已按角色+78、NPC双字节图组及+6方向字段修正：四组原厂首图12次C对照和两个实际SDL入口均为零差异。详见[新游戏字段与显示证据](np560_npe_new_game_display.md)。其后的536像素差异已由[NPC周期移动](np560_npe_npc_periodic.md)消除：15组位置／方向和RNG对照通过，原始首周期C及SDL整屏零差异。更多计时相位、重绘与缓存联动仍待核对，不从截图写死坐标。

其它待办：损坏档元数据、标题失败重新准备界面的原厂整屏、真正的原厂写入失败、真实计时相位及已有声音／跨 NPE 机制。现有诊断剧情结局沿用，不安排正常属性完整通关或 SDL 全程实玩。

## 2026-10-07：系统返回地图补充

[系统槽返回对照](np560_npe_system_slot_return.md)已通过真实boot后的空第二槽路径逐提交验证提示／恢复底图／返回地图；保存、成功读取及失败三条路径共33次C／SDL零差异。修正此前未独立覆盖的系统子菜单位置／反色及返回地图上下四行：03:4214按菜单背景色0清除，不能复制进入菜单前颜色2的地图边缘。保存／失败保持NPC巡走，成功读取重建NPC。真正原厂写入失败及损坏槽／标题失败界面仍待核对。

## 后续修订：共享等待服务输入差异

[离队提示的新对照](np560_npe_leave_battle.md)确认原参考在七种输入掩码下提前返回；同一03:4050服务的4F:4C34状态条件及参考调度仍待还原。C目前等待2000ms且忽略普通键，旧受控时钟／按键检查仅验证C当前行为，不证明原输入一致；该差异列入plan.md。正常属性及SDL全程验收继续取消。

## 后续修订：共享提示输入已修复

[定时提示](np560_npe_timed_notice.md)按原键盘扫描补齐提前退出。原空槽确认／F4两组的提示／恢复4次实际C／SDL整屏匹配；无输入两秒、提前结束及窗口关闭和六条系统／标题路径通过。原厂真正写失败、损坏槽元数据和物理相位仍待办。

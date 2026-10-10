# 原厂音乐设置弹窗与输入

## 2026-10-06：资源与控制器证据

`tmp/music-menu-original-source.asm`、`tmp/music-menu-draw-input-source.asm`
来自未修改的原 PRG；既有 `tmp/npe-sound-dispatch-source.asm` 保留地图重入调用。

- `37:CF4C..CF9E` 在系统菜单第三项进入音乐选择，写菜单类型 6。
  当前 `7A50=1` 时先选第二项，否则选第一项；进入弹窗不修改设置。
- `4F:1FFA..207C` 将当前 NPE 的音乐面板画到 `(105,70)`。
  两项文字在面板位图中，程序不重写成英文或另一套中文。
  反色矩形包含边界：第一项 `(113,78)..(155,95)`，第二项
  `(113,95)..(155,111)`，两个矩形共用第 95 行。
- 当前 NPE 的面板位于主位图组倒数第 69 项，59×50 像素。
  18 个现有 NPE 只读扫描均符合此结构；实际索引按各文件的位图数量计算。
  这些文件的面板字节相同，不等于它们的其它引擎机制已经完成。
- `37:CFD0..D024` 确认第一项发起保留地图槽的播放请求并置音乐开，
  第二项停音并置音乐关；随后模式为 1，地图初始化标志为 1。
- `37:1312..1366` 消费地图初始化标志，音乐开时再次请求保留地图槽。
  C 弹窗确认开时也发出这两次请求，确认关时只发出停音，不抽选曲 RNG。
  请求间的原机时间间隔仍待核对。

## 独立原厂输入

`npe-sdl/tests/reference_music_menu.mjs` 在原厂前端执行六组隔离诊断：
恢复既有快照，选择地图模式及音乐初值，再通过原控制器按键打开系统菜单。
不改 NPE、PRG 或连续剧情存档。输出 `tmp/native-music-menu.json`、
`tmp/native-music-*-before.idx` 和 `tmp/native-music-*-frame-*.idx`，
并生成 `npe-sdl/tests/music_menu_native.h`。

| 输入 | 原厂结果 | C／SDL 处理 |
| --- | --- | --- |
| 上／下 | 两项循环选择 | 同样选择，不改音乐 |
| 左／右 | 忽略 | 保留画面和选择，不重复提交 |
| 普通取消掩码 32 | 忽略 | Backspace 忽略 |
| 菜单掩码 64（F4） | 回地图，设置不变 | F4 返回地图；主机 Esc 沿既有菜单键映射处理 |
| 确认 | 应用所选项并回地图 | Enter／Space／小键盘 Enter 确认 |

六组覆盖开→开、开→关、关→开、关→关及两个初值下的 F4 取消。
期间关闭、重新启用和重复选开均来自明确的确认输入，不把移动光标当作切换。

## C／实际 SDL 对照

生产程序 `main.c` 的系统菜单入口调用 `show_music_choice`，已移除
`Music setting`／`Music on`／`Music off` 桌面提示框。面板读取当前 NPE，
保留进入前的 LCD；普通无效输入不重绘。确认或 F4 后关闭系统菜单返回地图，
音乐标志同步到主事件循环。

`sdl_music_menu_probe.c` 用原厂进入前 LCD 作为保留背景，核对 **20 个弹窗
状态、两张不同整屏**：纯 C 叠图和 SDL 实际提交均零差异。此背景夹具不证明
整个地图渲染器或菜单进入前的缓存已经独立验收。六张原厂最终地图帧未计入
本项像素对照。主程序另用实际系统菜单输入进入弹窗、移动和确认，检查返回
地图且没有桌面提示框。

另修正地图上的原厂 F4 菜单键：F4 可以打开系统菜单，在任一模式6菜单层
按 F4 直接回地图。原厂独立按键记录 `tmp/music-menu-f4.json` 证实此路径，
实际 SDL 系统子菜单输入也通过；主机 Esc 的既有菜单入口保留。

同时检查：音乐选择不改变 C 随机状态；确认开两次保留槽请求、确认关一次
停音、取消无声音请求；无效键不增加弹窗提交。声音请求次数按原 PRG 调用
核对，尚非原机音频服务的完整动态时间线。

全量构建、ctest 5/5、属性菜单、三槽存档、声音调度和 SDL 声音回归通过。
日志 `tmp/music-menu-check.log`、`tmp/music-menu-all-build.log`、
`tmp/music-menu-ctest.log`、`tmp/music-menu-regression-*.log`。
计数 `tmp/music-menu-counts.json`，资源扫描 `tmp/music-menu-resource-corpus.json`。
不把本项的重复状态叠加到既有静态 334 张、战斗 2119 次／723 张或剧情 77 页。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_music_menu.mjs
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-music-menu-probe.exe tmp/tjfy-world-probe.npe
ctest --test-dir npe-sdl/build --output-on-failure
```

原 NPE／PRG 哈希与 NPESAV15 不变。开场／其它场景音画、其它模式重入、
读档保留音乐槽、开机随机状态、原固件 PCM／增益及真实等待时序继续实现。
不安排用户取消的正常属性完整通关或 SDL 全程实玩。


## 系统菜单返回补充（2026-10-07）

[系统菜单返回证据](np560_npe_system_menu_return.md)补充无效按键、进入前场景恢复、音乐确认／取消及随后NPC周期的9条局部流程，共77次实际C／SDL提交全部匹配。驻留背景色用于NPC清边缘，暖读档保留且不写入NPESAV15。设备真实计时相位、其它模式背景色和PCM仍待核对；不恢复已取消的两项全程验收。

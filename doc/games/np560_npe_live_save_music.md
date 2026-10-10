# 运行中存读档的音乐槽与声音请求

日期：2026-10-07。生产代码仍是 C11 + SDL2；原厂 WASM 只作离线参考。

## 原厂证据

`npe-sdl/tests/reference_music_slot_load.mjs` 从原厂快照出发，通过实际菜单写入第一槽，再修改运行中的进度、音乐开关和音乐槽，分别从系统菜单和标题读回。

12 组覆盖两个入口、开关 0／1 和槽 0／1／2。存档进度为 1，加载前运行进度为 4；全部读回 1，音乐开关和槽保持加载前值。输出为 `tmp/native-music-slot-load.json`，日志 `tmp/music-slot-load-native.log`。

| 入口 | 音乐关 | 音乐开 |
| --- | --- | --- |
| 系统菜单成功读档 | 无声音事件 | 停止原播放，再循环播放 20＋保留槽 |
| 标题成功读档 | 无声音事件 | 无声音事件 |

标题的停音位于打开读档选择界面的 `3A:8F74`，不能解释成所有成功加载后统一停音。系统读档成功分支 `37:D088..D094` 设置地图初始化标志；系统存档返回分支 `37:D0D0..D13A` 也设置该标志。地图控制器据此重播保留槽。系统存档的门控组合由这一源码分支及既有地图控制器支持，本轮 12 组动态参考针对读档，不将它们记成 18 组原厂参考。

源码节选：`tmp/music-save-load-original-source.asm`、`tmp/music-load-codec-map-source.asm`；系统菜单原分支见 `tmp/music-menu-draw-input-source.asm`。原厂存档解码复制 RPG 数据；音乐槽 `52:7A04` 是字节，不是字。音乐开关为 `52:7A50`，原 RNG 位于 A2 常驻对象＋0x7E，不能从当前地图数据或存档音乐槽推算。

## C 实现

- `npe_world_load` 保持冷解码用途：可传入未初始化目标，用于启动、槽位元数据和诊断存档，恢复保存的 C 随机状态，保持无声。
- 新增 `npe_world_restore` 用于已初始化运行时：事务解码成功后保留当前音乐开关、槽、常驻随机状态、声音请求及两套回调。解码失败不改变世界，也不发声音请求。
- 系统存档／读档成功返回地图时发一次保留槽请求，不重新选曲；标题打开读档选择时停音，成功后不播放。主循环不会因回调丢失而意外重新绑定、重播。
- SDL 播放器在一次播放请求内部重置游标。原厂参考中的“stop＋play”不要求 C 引擎额外发一次独立停音请求。
- F9 是主机快捷入口，使用运行中恢复并重播保留槽；`--continue` 是冷启动入口，继续使用冷解码。这两个入口没有原机键位等价性，不计入原厂按键对照。

NPESAV15 编码不变，没有按游戏、角色、物品编号增加特例。静音时保留设置；资源仍从当前 NPE 读取。

## 验证范围

`sound-dispatch-probe` 通过 12 组运行中恢复及不存在文件的失败检查；包含有效种子零、回调保留、静音门控和冷解码回归。

`sdl-live-restore-probe` 包含实际 `main.c` 与 SDL 声音播放器，使用真实系统菜单／标题按键流程；静音由原音乐弹窗选择。18 组覆盖 12 次读档和 6 次存档返回，检查恢复的进度、音乐开关／槽、回调、当前声音段、循环状态和游标是否重置。暂停音频设备消除检测游标的时间竞争；不对真实时间或 PCM 波形作结论。测试使用独立文件身份及独立存档，完成后删除。

```powershell
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_music_slot_load.mjs
& npe-sdl/build/sound-dispatch-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-live-restore-probe.exe tmp/tjfy-world-probe.npe
```

日志：`tmp/live-restore-c-check.log`、`tmp/live-restore-sdl-check.log`；全量构建及ctest 5/5通过，日志 `tmp/live-restore-all-build.log`、`tmp/live-restore-ctest.log`；三槽和音乐菜单回归分别为 `tmp/live-restore-slot-regression.log`、`tmp/live-restore-menu-regression.log`。本轮不新增原厂画面计数。

## 剩余范围

系统空槽／损坏C档失败提示及返回已接入，标题空槽开始新游戏；损坏档元数据和标题失败界面仍需核对，见[存档反馈](np560_npe_save_feedback.md)。冷启动音乐槽／RNG 初始状态、加载后其它服务的完整随机调用时点、PCM／增益和真实播放时序仍需核对。其它 NPE 的存读档和游戏机制未由《天降风云》这些局部检查证明。

沿用已有诊断剧情结局；不安排用户取消的正常属性完整通关和 SDL 全程实玩。


## 补充：NPC与商店驻留状态

运行中读档在既有音乐／RNG／出口字段之外，继续保留原slot codec未覆盖的NPC及商店字段。系统与标题选槽入口6组状态对照通过；既有18组实际SDL存读档、声音调度与三槽流程回归通过。字段映射、失败事务与诊断入口限制见[读档驻留状态](np560_npe_shop_state_load.md)。本补充不新增音频或画面证明。

# NP560 NPE：真实标题启动、占用槽读档与 NPC 重建

2026-10-10后续：[标题选槽与返回](np560_npe_title_slot_controls.md)已补齐共享槽号、保留画面、取消清屏、空槽判断及读取失败退出。下方保留原首次读档批次范围。

更新：2026-10-07。生产实现仍为独立 C11／SDL2，原厂 PRG／WASM 只用于离线参考。

## 参考入口与坐标

旧诊断将战斗快照的模式直接改成标题，没有初始化标题构造资源；读档后屏幕坐标出现 71／188。因此旧样本只支持原 slot codec 已证明的驻留字段保留，不作为标题坐标或画面证据。

本次使用真实启动生命周期：标题启动→新游戏→原厂系统菜单保存→导出原持久数据→释放运行时→新建运行时并安装持久数据→真实标题启动→选择占用槽读取。运行时不能二次 boot，同一实例重启的失败实验未纳入证据。

原厂首次读档地图的屏幕坐标为 7／1、视口偏移 36／1、方向 1，逻辑位置为 43／2；地图 1、进度 1、事件 0、钱 100。这些值与保存状态一致。它们仅用于离线夹具；生产位置来自当前 NPE 和存档。

## NPC 生命周期修复

原 slot codec `3C:40D4..4100` 恢复 `69C6` 开始的 `1BC` 字节（至 `6B81`）。地图 NPC 记录位于 `6CA2`，不在这段存档中；返回地图时从当前 NPE 重新装载。持久 NPC 状态 `6FBA` 是另一段数据，不能与地图巡走记录混同。

旧 C 暖恢复保留主机诊断存档的 NPC 巡走位置，首次地图有 536 个像素差异。现 `npe_world_restore` 成功后清除 NPC 位置／方向覆盖，回到当前 NPE 的地图记录。不存在或损坏档仍不替换当前世界。`npe_world_load` 冷解码和 NPESAV15 线格式保持兼容，继续读取诊断覆盖。

测试主机存档的 NPC 覆盖取自原厂读档后较晚一次周期记录，故意与首次地图不同；它不是原厂保存时的巡走数据或原厂存档线格式。冷解码先验证这些覆盖被保存，再经实际标题控制器加载并验证重建后的记录。未添加游戏、NPC、图像或坐标编号特例。

## 验证范围

`sdl-title-load-probe` 包含实际 `main.c`，使用独立存档路径和未修改的原 NPE，逐次比较完整 240×120 索引画面与 SDL 读回像素。

| 提交 | C 整屏差异 | SDL 差异 |
| --- | ---: | ---: |
| 标题向下选择读取 | 0 | 0 |
| 占用槽界面 | 0 | 0 |
| 首次加载地图 | 0 | 0 |

主角坐标、视口、方向、地图、模式、事件、进度、钱及 13 个非零 NPC 的编号、位置、方向匹配。原失败保留于 `tmp/title-load-before-fix.log`，最终为 `tmp/title-load-fixed.log`。全量构建退出 0、无警告，ctest 5/5；8 项相关回归退出 0，详见 `tmp/title-load-regressions.json`。原 NPE／PRG 哈希未变。

参考及夹具：`tests/reference_title_load.mjs`、`reference_title_load_npc.mjs`、`generate_title_load_fixture.py`、`title_load_native.h`、`sdl_title_load_probe.c`；元数据 `tmp/native-title-reload.json`，汇总 `tmp/title-load-report.json`。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_title_load.mjs --load-only
python npe-sdl/tests/generate_title_load_fixture.py
cmake --build npe-sdl/build --target sdl-title-load-probe -j 4
& npe-sdl/build/sdl-title-load-probe.exe tmp/tjfy-world-probe.npe
```

## 2026-10-10后续

[结束返回标题及后继](np560_npe_ending_return.md)已补齐原标题初始化、控制字节、读档页音乐保留、暖读取事件号保留，以及结束释放地图后占用槽重新分配。两条占用槽后继／10次原C与SDL画面通过，本篇原3次冷标题加载画面对照保持。

## 尚待完成

- 系统保存／失败保留巡走、成功读档重建 NPC 及返回画面已由[系统槽返回](np560_npe_system_slot_return.md)验证；其它槽和模式交错继续。
- 冷主机 `--continue` 的运行初始化，追击／脚本／对象／LCD 缓存。
- 真实启动计时相位／共享 RNG；本次原厂读档第 23 tick 的较晚 NPC 提交未作等价性验收。
- 商店、战斗边界，声音 PCM／增益／时序及跨 NPE 机制。

本次不增加声音或 RNG 等价性结论，不证明整体完全复刻。沿用已有诊断剧情结局，用户取消的正常属性完整通关及 SDL 全程实玩不恢复。

# NP560 NPE：读档保留NPC与商店驻留状态

更新：2026-10-07。实现仍为独立C11／SDL2；原厂PRG／WASM只用于离线参考。本轮不新增剧情路线、LCD整屏或PCM采样。

## 原厂行为与修复

原slot codec源3C:3DC2..4396恢复RPG记录、地图、进度、物品／NPC等持久数据，并写模式1；它没有写下表驻留字段。系统及标题选槽入口的实际读档结果保留这些字段。此前C的`npe_world_restore`调用冷主机解码后整体替换世界，导致这11个字段变零；初始探针以断言失败报告，未称通过。

| 原驻留地址 | C字段 | 类型 |
| --- | --- | --- |
| 7938 | event_context_id | word |
| 7A68 | mode2_context | byte |
| 7A4A | shop_ui | byte |
| 7A48 | shop_quantity | byte |
| 7A0C | shop_choice | byte |
| 7A46 | shop_row | byte |
| 7AC6 | shop_first_item | word |
| 7AB6 | shop_last_item | word |
| 7A4C | shop_selected_item | word |
| 7ABE | shop_description_start | word |
| 7ABC | shop_description_end | word |

成功的运行中恢复现从当前世界保留这些字段，同时恢复存档中的RPG记录。它们没有添加进NPESAV15：冷主机解码仍按其原初始化处理，不从存档恢复驻留UI；读档失败不替换当前世界。

原7A0E种类字段也在本轮采样中保留。C的`shop_ready`和`shop_list_ready`用于活动UI与目录索引，并不是7A0E的直接存储。成功读档回地图，活动标记及旧目录索引失效；下次NPC商店入口按当时上下文建立买入／卖出目录。这避免带着旧活动标记进入角色状态页。对话页等未被本轮证明的缓存不从旧世界复制。

## 采样和C／SDL检查

原厂每组先通过系统菜单实际存档（进度1），再把进度变4，进入系统或标题选槽界面。在最终确认读档之前设置诊断驻留值，确认读档后观察原厂状态。标题入口通过模式7进入原厂选槽流程，未覆盖从游戏退出到标题的完整生命周期。

| 驻留配置 | 系统入口 | 标题入口 |
| --- | ---: | ---: |
| 买入上下文13、选中46、首尾45／54、数量99、说明82／97 | 通过 | 通过 |
| 空卖出上下文10、选中FFFF、首尾9／37、数量0、说明末FFFF | 通过 | 通过 |
| 上下文FFFF、NPC字节FF、数量FF、说明FFFE／FFFF | 通过 | 通过 |

每组保存前、选槽前、读档确认前及读档后状态留在`tmp/native-shop-state-load.json`。读档后进度从4回1、模式1，11个映射字段及原种类字段与确认前一致。采样不依赖修改原NPE或PRG。

`sdl-shop-state-load-probe`复用实际main.c的系统／标题读档流程：6组C直接恢复、6组实际C／SDL控制器检查通过；另外6组冷主机解码检查通过。不存在及截断档各6组，失败后整个NpeWorld逐字节不变。C使用独立的SHOP-LOAD-01诊断副本及存档路径，结束后清理；原游戏存档身份不改变。

全量构建无警告、ctest5/5和8项局部回归通过：18组实际SDL音乐存读档、8组出口驻留恢复、声音调度、三槽流程、存档反馈、商店边界60次、NPC入口45次和商店关闭33次C／SDL逐提交匹配。后面三个为已有图像回归，不计作本轮新增画面采样。原NPE／PRG哈希及NPESAV15不变。

证据：`tests/reference_shop_state_load.mjs`、`generate_shop_state_load_fixture.py`、`shop_state_load_native.h`、`sdl_live_restore_probe.c`；`tmp/shop-state-load-before-fix.log`保留原失败，`shop-state-load-fixed.log`为最终6组结果；原厂日志`shop-state-load-native.log`、全量构建`shop-state-load-all-build.log`，汇总`shop-state-load-report.json`。

```powershell
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-shop-state-load-probe.exe tmp/tjfy-world-probe.npe
ctest --test-dir npe-sdl/build --output-on-failure
```

## 未证明与后续

- 已定位旧标题71／188异常：战斗快照直接改标题未初始化构造资源。后续真实boot、原UI存档及持久数据重启确认屏幕7／1、视口36／1、方向1，并修复首次读档NPC重建；见[真实标题读档](np560_npe_title_load.md)。本节驻留字段保留由codec及独立字段采样支持，不引用旧异常坐标作为证据。
- 其它NPC／脚本／对象／LCD缓存、模式重入及买卖交错；空买入目录、更多价格／金额和售空边界。
- 战斗边界、真实时基／启动RNG、PCM／增益及其它NPE机制。

已有剧情诊断结局继续沿用，不安排用户取消的正常属性完整通关或SDL全程实玩。

## 2026-10-07：商店种类的独立驻留字段

新游戏生命周期对照补充建模 `shop_kind` 对应 `7A0E`，与 C 目录活动标记 `shop_ready` 分开。新游戏及运行中读档保留种类；目录活动仍失效。两处真实 NPC 商店入口同时设置种类和活动标记，下一次进入按原入口重新装载。

本轮六组原厂商店/NPC 读档回归新增种类保留断言，全部通过；不增加存档字段或改动 NPESAV15。见[新游戏构造与驻留状态](np560_npe_new_game_lifetime.md)及 `tmp/new-game-lifetime-final-shop-state.log`。

# 战斗状态页的资源缓冲生命周期

更新于2026-10-09，接续[战斗查看状态](np560_npe_battle_status_view.md)。生产仍为C11与SDL2；原函数参照仅用于核对，资源从当前NPE读取。

## 已接入的原流程

打开状态页的原`42:8E72..8E9A`先读52:7A88并解锁，再重读该句柄并释放，随后清52:7AA4一个字节。释放后句柄槽和52:79A6的指针仍保留；句柄0也调用两个服务，不使用“0则跳过”的启动清理规则。

F4返回的原`36:E4DC..E51C`分配32768字节、锁定，将句柄／指针写回上述槽，标志高字节置1，然后调用原`31:DEF6`读取当前NPE第9段的打包战斗资源。状态页继续按上一批的六次提交恢复画面。

新增纯C释放服务`npe_rpg_battle_resources_release`；实际SDL状态页打开／取消通过`npe_rpg_runtime_battle_resources`调用释放／分配与文件读取。复用已对照的原堆及装载服务，未添加固定缓冲地址或期望堆头。

52:7AA4／7AA5作为完整共享word接入World和运行时，高字节是资源标志，低字节按原版保留。单字节写入只改对应半字，整字读写一致。World总绑定3536字节。暖恢复／重新开始保留两字节；自身NPESAV22不序列化该瞬时状态，四对保存副本一致，冷读取初值0。

## 验证范围与结果

### 原服务与完整内存

五组独立参照用原启动／地图／战斗分配函数建立堆链，再调用未修改的原解锁、释放、重新分配、锁定和打包读取函数。诊断调用者按状态页协议清高标志字节，文件读取及提示是明确的适配边界；不将这一层说成完整原UI调用者RAM验证。

| 案例 | 输入与结束位置 |
| --- | --- |
| cycle-full | 完整读，释放后重新装载一次 |
| cycle-short | 每次只供5字节，两次释放／重新装载，保留未读尾部 |
| cycle-eof | 每次供0字节，两次释放／重新装载 |
| cycle-released | 完整读，重装一次，最终释放并保留槽值 |
| cycle-zero-release | 耗尽句柄后分配返回0、EOF，再按原协议解锁／释放0 |

每例C完整131072字节RAM、18字节读取记录与原版一致，合计655360／90字节。C输出另存为`tmp/battle-status-resource-c-*-ram.bin`与`*-trace.bin`供独立复核。低字节165保留，最终标志为1或0符合结束位置。所有堆节点由分配服务建立，无原结束堆头预置。

首轮最终释放例发现参照标志为6，C为0。诊断调用者误用写R5的字指令，随后写的是RB5（R2低字节），把句柄6留入标志。核对原42:8E92的`MOVB RB5,#0`编码4950后修正诊断指令，重做五组参照；生产释放代码未改，五组通过。

### 原公共输入与实际SDL

四条原公共输入路径沿用单人、两人、三人和行动角色／保留选人不同的场景，新增低字节166作为输入。打开状态页后完整word为00A6，返回后为01A6；上下切人和无效按键保持。

直接C／SDL和实际main／SDL均附着真实NPE资源运行时，合计142次画面提交、96份六字节控制状态、864份完整148字节人物记录与原版一致。原参考没有修改固件；只在输入前注入诊断属性及标志低字节，未预置期望结束状态。实际main包含状态页空闲等待，不额外提交画面。

四种原暖恢复入口在完整word写81A6后均保留，C一致。旧状态页、连续战斗菜单、仙术／道具／目标选择、状态覆盖、恢复入口和资源完整RAM回归保持。

## 构建与证据

33目标构建无警告，32项短检查全部退出0；首轮只有资源循环参照失败，修正参照后只重建／重跑两个共用源码的资源目标，其它31项生产结果保持。独立汇总通过，原NPE、PRG、WASM与三张字库哈希保持。

- 原服务：`reference_rpg_resource_lifecycle.py --status-cycle --qemu <qemu路径>`，输出`tmp/battle-status-resource-native/report.json`。
- 原公共输入：`reference_battle_status_view.mjs --resources`，输出`tmp/native-battle-status-resources.json`及画面。
- 原暖恢复：`reference_battle_selection_load.mjs --resource-flags`。
- 夹具：`generate_battle_status_resources_fixture.py`、`generate_battle_status_view_fixture.py --resources`。
- 检查：`battle-status-resource-cycle-probe`、`sdl-battle-status-resources-probe`、`battle-resource-flags-load-probe`。
- 必要回归：`tmp/run_battle_status_resources_checks.py`，退出码`tmp/battle-status-resources-final-*.exit`。
- 独立复核：`verify_battle_status_resources.py`，报告`tmp/battle-status-resources-report.json`，保存源码、原资料、RAM、读取记录、画面和存档哈希。

## 仍需完成

其它标志写入者／消费者和战斗UI资源生命周期、其它选择器、异常人物索引、完整文件失败后继尚未完成；本批不证明全部启动／退出、所有战斗UI及完整客机RAM行为。连续战斗其它动作、原音频、物理时序和其它NPE完整动态适配继续。

整体仍粗估约80%。原机存档互通、正常属性完整通关和SDL全程人工游玩继续按用户要求排除。

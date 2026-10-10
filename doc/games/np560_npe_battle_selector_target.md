# 战斗选择器的当前物理目标

更新：2026-10-09。运行实现使用C11／SDL2，从当前NPE读取资源；原固件和前端仅用于开发对照。

## 独立状态及原版规则

原52:7AC0是从1开始的物理目标编号，独立于人物记录64CE（记录偏移126）和宿主SDL的敌方游标。此前人物目标已驻留，但这个全局字节没有共享绑定；现在通过`battle_display_target`接入World及资源运行时读写。原3C:2644读取它判断目标所属阵营，不能把它当成每次都随友方游标变化的副本。

| 操作 | 原位置／观察 | 当前实现 |
| --- | --- | --- |
| 入战 | 49:8E24 | 当前物理目标置为队伍前缀人数＋1 |
| 道具／仙术选择 | 38:4C50／3A:0CFA | 发布人物记录中的初始目标 |
| 友方目标移动 | 原公共输入 | 更新当前人物64CE，保留7AC0；HUD按选中人物更新 |
| 敌方目标移动 | 原公共输入 | 更新当前人物64CE，同时更新7AC0 |
| 目标取消后返回列表 | 原公共输入 | 保留移动后的目标；列表取消才恢复首敌人 |
| 列表取消 | 36:E164／E3B8 | 恢复当前人物首敌人目标并发布7AC0 |
| 确认后换角色 | 原公共输入 | 敌方选择回到下一人物记录目标，友方选择保留全局目标 |
| 执行前目标解析 | 40:17BA | 解析完成后发布当前人物64CE |
| 敌人目标准备 | 32:E0BA等 | 发布实际物理目标编号 |
| 胜利结束 | 原离场观察 | 当前全局目标归零 |
| 暖读档／重开 | 系统读档、标题读档、新游戏、空槽开始 | 保留7AC0，冷初始化为0 |

后两项执行准备的全部动作组合尚未证明；本批连续战斗检查覆盖防御与敌人普通攻击。围攻／联合等其它7AC0写入者和全部异常目标消费者仍待完成。

World当前绑定3537字节。`save.c`增加暖恢复复制，自身NPESAV22载荷未增加该临时字段；四对改变它之前／之后的存档逐字节相同，冷读取仍为0。

## 对照范围

两个原公共输入脚本增加独立的`--selector-target`模式；只增加7AC0观察，13条仙术／14条道具原输入及资源观察保持。去掉新增观察字段后，全部JSON与上一批资源参考一致；原画面文件也逐字节一致。

| 路线 | 原公共路径 | 实际SDL菜单画面 | 完整控制／缓存与目标快照 |
| --- | ---: | ---: | ---: |
| 仙术 | 13 | 73 | 108 |
| 道具 | 14 | 96 | 139 |
| 合计 | 27 | 169 | 247 |

每个画面同时比较C像素与SDL读回；每个状态检查World和附着的资源运行时读取的7AC0，保存单字节结果供独立验证器再次比较。外层157次原LCD没有计入这两个菜单探针，不能据此宣称完整外层控制器已经一致。

七条已有离场和三条连续战斗参考原先已采集`displayTarget`。本批加强两路生产C／SDL探针，新增134份入战、离场、再次入战和动作后的全局目标对照。这些是使用已有原参考新增C检查，未描述为新原采样。

四种原暖恢复入口保留诊断值129，与C一致；同时检查7AC8值166及四对自身保存副本。上一批资源、画面、完整人物记录和关闭状态的保存输出也在本批重新比较。

## 修复与验证记录

首轮45项检查中，旧`sdl-battle-item-flow-probe`场景1失败。其夹具在入战后才增加队员，导致物理人数和敌人槽位仍按单人入口保存，而SDL目标列表按两人遍历。现改为在入战前设置队伍并断言物理人数正确；原输入、使用次数／目标断言、效果值与原参考保持。15个道具场景和四组效果计时流程通过；首轮全部日志保留为`tmp/battle-selector-target-first-batch-*`。

最终构建46目标无警告，45项必要短检查全部退出0，独立验证器再次核对原资源哈希、菜单画面、缓存、全局目标、暖恢复存档和既有回归保存输出。

## 证据与重现

- 原参考脚本：`npe-sdl/tests/reference_battle_skill_record.mjs --selector-target`、`reference_battle_item_record.mjs --selector-target`及`reference_battle_selection_load.mjs --selector-target`。
- 原观察输出：`tmp/native-battle-{skill,item}-selector-target.json`、同前缀画面和`tmp/native-battle-selector-target-load.json`。
- 原位置核对：`tmp/battle-selector-target-writers-source.asm`和`tmp/battle-selector-target-all-writers-source.asm`；后一文件的局部窗口首行可能位于上一条指令内部，应以实际写入地址及完整相关函数为准。
- C保存输出：`tmp/battle-selector-target-c-{skill,item}-*`、`tmp/native-battle-resources-*-c-*-display-target.bin`及`tmp/battle-selector-target-own-*`。
- 构建／检查：`tmp/run_battle_selector_target_checks.py`、`tmp/battle-selector-target-check-targets.json`和`tmp/battle-selector-target-final-*`。
- 独立验证：`npe-sdl/tests/verify_battle_selector_target.py`；报告`tmp/battle-selector-target-report.json`。

Python／Node仅用于开发验证，交付运行时仍为C11／SDL2。

## 剩余工作

其它7AC0写入者／消费者、围攻／联合入口、直接修改全局目标后的全部绘图、0／129／255等异常物理目标、其它选择器和执行控制记录、连续战斗其它动作仍未完成。完整启动／退出与文件关闭顺序、原PCM和物理时序、其它NPE完整动态运行继续；整体工程仍估计约80%。

原机存档互通、正常属性完整通关和SDL全程实玩继续按用户要求取消。

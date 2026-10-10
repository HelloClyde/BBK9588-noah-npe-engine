# NP560 NPE 普通攻击与投掷敌方目标页

日期：2026-10-06。实现仍为 C11 + SDL2；原厂 WASM 仅用于采样，不是 C 版运行依赖。

## 采样来源与范围

读取相同原版《天降风云》NPE 与 NP560.PRG，使用原厂运行时的战斗快照。
独立编组夹具只在 RAM 脚本组槽中增加原 NPE 敌人 51，让原厂重新执行进场加载和坐标计算；
不编辑敌人记录、NPE 或 PRG。投掷夹具另在 RAM 背包中提供原 NPE 道具 159「飞刀」、160「毒飞刀」各一件。
这些是独立界面夹具，不属于连续主线角色属性诊断存档。

- NPE SHA256：`df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40`
- PRG SHA256：`d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5`
- 采样脚本：`tmp/battle-native-enemy-target.mjs`。
- 新增样本：`two-attack`、`three-attack`、`two-throw`、`three-throw`、`two-throw-second`。
- 单存活敌人分支：`single-attack`、`single-throw`，保留两个原物理空槽。

普通攻击样本目标页显示气 60/60，投掷样本为 999/999：前者进入目标页时重新加载人物数据，
夹具只对战斗记录改气、未改地图人物记录。因此对照按各自原厂实际帧设置 C 侧显示值，不将两组截图混用。

## 原厂菜单与输入

| 流程 | 原厂菜单值 | 选中图标 | F4 / Menu64 返回 |
| --- | --- | --- | --- |
| 多敌普通攻击 | 17 | 剑，编号 1 | 默认战斗菜单 10 |
| 投掷列表 | 33 | 全屏道具列表 | 使用／投掷子菜单 32 |
| 多敌投掷选人 | 37 | 旗帜，编号 2 | 投掷列表 33，保留道具选中项 |
| 使用／投掷子菜单 | 32 | 旗帜 | 五项指令菜单 31 |
| 五项指令菜单 | 31 | 旗帜 | 旗帜图标 30 |

普通攻击 `40:17D2` 路径在 `40:1B82..1B8C` 写入菜单 17；
F4 取消路径 `36:DFE2..E008` 改回 10。
投掷选人 F4 返回列表路径 `36:E1A0..E2C4` 还原背包数量并写入菜单 33。
菜单截图与按键值直接取自运行时，未凭菜单名字推断。

两种选敌页均：

- 进入时选择第一名存活敌人，跳过空槽；Up 逆向、Down 正向循环。
- Left、Right、Cancel32（SDL Escape）不改变目标且不取消。
- Confirm 提交，Menu64（SDL F4／Backspace）取消。
- 仅一名存活敌人时自动提交，不出现选敌页，也不需要第二次确认。
- 黑色指针使用当前 NPE 战斗资源 5；移动沿用原厂局部清除，保留与敌图重叠的擦除像素。

## C / SDL 修改

`main.c` 的 `show_battle_enemy_target` 为普通攻击与投掷实现上述页面，保留局部像素缓冲；
普通攻击主循环已删除旧的 level3 目标处理，避免每轮全屏重绘抹掉原厂擦除结果。
投掷先选择敌人再提交原有 C 战斗命令，取消返回道具列表，保留第二件道具的选中位置；
退出列表返回使用／投掷子菜单。确认后返回战斗，不再弹出自拟英文「Item used」结果框。
道具名称、图像、说明、作用与伤害仍读取当前 NPE，没有写死飞刀或野猪数据到运行代码。

## 验证

新增 **46 张 240×120 整屏**逐像素零差异：

- 普通攻击双敌／三敌各 10 张，共 20 张。
- 投掷双敌／三敌各 10 张，共 20 张。
- 两道具列表首项、第二项、取消选人后第二项，共 3 张。
- 退出列表／道具子菜单／指令菜单，共 3 张。

连同此前 53 张，`magic-list-probe` 现校验 **99 张整屏**。
覆盖首目标、上下切换／循环、空槽跳过、Right、Left、Cancel，以及多层返回。
这不是动作动画帧或伤害数字动画的验收。

新增 `sdl-enemy-target-flow-probe` 通过七个实际 SDL 按键场景：
双敌普通攻击确认与取消、单敌攻击自动提交、选择毒飞刀并投掷、取消后重新选人、
取消投掷并退出列表、单敌投掷自动提交。断言目标、命令次数、数量、第二项保留、
重新进入时首选目标、返回子菜单与没有桌面结果弹窗。

构建成功；ctest 5/5；原七个 SDL 仙术场景、围攻场景与战斗规则探针回归通过。

```powershell
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
ctest --test-dir npe-sdl/build --output-on-failure
& npe-sdl/build/magic-list-probe.exe tmp/tjfy-world-probe.npe npe-sdl/assets/gbk12-native.bin
& npe-sdl/build/sdl-enemy-target-flow-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-magic-flow-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-group-attack-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/enemy-action-probe.exe tmp/tjfy-world-probe.npe
```

日志：`tmp/attack-throw-pixel.log`、`tmp/attack-throw-sdl.log`、
`tmp/attack-throw-magic-regression.log`、`tmp/attack-throw-group-regression.log`、`tmp/attack-throw-rules.log`。

## 尚未完成

此次确认单人战斗最终数量及取消结果。原厂在进入投掷选人时已预扣一件，F4 再返还；
C 当前在行动执行时扣数量。多人待执行命令的道具预留、取消退还及存读档需继续核对，
不能将此次界面验收称作全部道具结算完成。

其它战斗道具的多人目标与结果、状态覆盖、逐行动动画、攻防计时、装备状态、
非零气效果、右下群体指令仍待复刻。没有重跑完整主线，尚不能声称原属性 SDL 通关。

后续投掷预扣／取消退还与角色返回已完成，图标导航使整屏对照增至 174 张。具体范围及其它未完成分支见[预扣记录](np560_npe_battle_item_reservation.md)。

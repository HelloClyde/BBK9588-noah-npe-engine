# 仙术、用药与投掷的目标准备记录

更新：2026-10-10。生产代码仍为独立 C11＋SDL2，数值与类型读取当前 NPE／共享资源记录；原固件与 WASM 仅用于独立参考采样。

## 本次修复

`world.c` 在施法／道具角色动画结束后、浮动数字显示前，更新执行者的六目标记录（人物记录偏移102..113，原地址64B6起）。

- 单体服务 `2D:419C..4298` 与 `4A56..4B52` 只清前三个word，再给选定物理目标写标记1。数值效果由 `35:EF1A..F96C` 依次取非零防御、攻击、气，否则取体（包括0）覆盖；其余目标值保留。不能套用普通攻击清六个word的范围。
- 群体仙术数值服务覆盖目标阵营的记录前缀，不清其它word。群体标记1的道具还在 `2D:46DC..4742` 将第一个word标为1；敌方投掷不会把这个标记移到敌人前缀。
- 类型31投掷保留原转换时机：角色动画结束后改变执行类型，将攻击效果作为体力效果；准备记录随后取转换后的浮动数值。
- 数值飘字直接读取执行者的原人物记录，单体显示限制在保存的选定目标。`30:1614..1644` 检查准备值；己方目标还在 `30:1682..16A2` 检查目标物理记录的活动字节43，敌方目标不使用这一门槛。显示坐标读取各目标记录124／125，纵向偏移由选定目标阵营决定（己方4、敌方6）。这补齐了敌方群体数值行动显示保留的己方旧数字的行为。
- 原有回合末清前四word、普通攻击清六word、入战／胜利清六word的范围沿用。本次没有增加World字段或修改自身存档格式。

## 独立原版参考

参考采样器 `reference_battle_execution_skill_record.mjs`、`reference_battle_execution_item_record.mjs` 新增 `--prepared-records`。恢复原快照，通过原输入排队指令后，在诊断RAM的六份物理人物记录中分别种入101..106、201..206等不同值，用来辨别覆盖与保留范围；原PRG、NPE、资源包与主线存档保持。

共享缓存的诊断名称／数值仅用于夹具，不进入生产逻辑。群体道具夹具在原列表确认前改驻留群体标记，随后由原控制器保存命令；执行时缓存的群体标记改回0，用来检查是否仍按排队时的标记执行。

| 原路径 | LCD提交 | 主要核对内容 |
| --- | ---: | --- |
| 仙术 `same` | 25 | 复合单体回复、未选中后两值保留 |
| 仙术 `zero-numeric` | 25 | 数值全零时以体0覆盖标记，不显示飘字 |
| 仙术 `two-different` | 42 | 第二人攻击第二敌人，目标值90 |
| 仙术 `two-status` | 44 | 第二人施状态仙术，目标标记1 |
| 仙术 `two-group` | 42 | 群体伤害130及保留的己方旧数字 |
| 仙术 `two-own-group` | 38 | 群体回复200，活动记录显示门槛 |
| 道具 `same` | 25 | 单体复合回复，气3优先于体7 |
| 道具 `throw-same` | 25 | 投掷第二敌人，气3优先于转换后的体17 |
| 道具 `two-throw` | 38 | 两次投掷及换资源，第二目标值60 |
| 道具 `group-use` | 25 | 群体用药及排队时群体标记 |
| 道具 `group-throw` | 25 | 群体投掷、首word标记和暖保留显示 |

合计 **11条路径、354次240×120原版／C／SDL整屏提交、66份12字节准备记录**。同时通过各回合144／142字节共享缓存、六记录共66项数值／状态字段、命令技能编号、费用顺序和库存断言。记录比较范围是102..113，不能称为完整148字节一致；原RAM取回合末样本，不是每次LCD的即时RAM。

两个C探针复用实际 `main.c` 动画与SDL呈现函数，调用生产排队／执行后端；这里不把后端调用计为新的完整SDL输入控制器验收。

## 验证与失败记录

初始检查复现攻击仙术及投掷的旧值残留：分别在42次、25次画面一致后，准备记录检查失败。补入写入后，群体伤害又出现151像素的旧数字遗漏；补查原显示服务后修正读取来源、活动门槛及纵向偏移。己方群体回复／用药对照用于纠正把所有保留敌方数字也显示出来的中间实现。零数值路径还纠正了“无非零值时保留标记1”的中间判断：原数值服务用体0覆盖该标记，不显示飘字。最终十一条原参考均保持，不用修改期望值消除差异。

受影响的既有短回归通过：7条仙术执行205提交、9条道具执行264提交、我方仙术动画164提交、复活动画90提交、状态道具272个记录阶段。后两类“记录阶段”是各探针已声明的字段集合，不是完整148字节。最终主程序及所需目标构建无警告。

独立校验器 `npe-sdl/tests/verify_battle_prepared_records.py` 重读原JSON／LCD与C输出，检查十一条路径、66份准备记录、完成日志、原输入哈希及当前源文件；报告为 `tmp/battle-prepared-records-report.json`。

复现（已有独立原参考时，在仓库根目录运行）：

```powershell
python npe-sdl/tests/generate_battle_execution_skill_fixture.py --prepared-records
python npe-sdl/tests/generate_battle_execution_item_fixture.py --prepared-records
cmake --build npe-sdl/build --target sdl-battle-prepared-skill-record-probe sdl-battle-prepared-item-record-probe -j 4
npe-sdl/build/sdl-battle-prepared-skill-record-probe.exe tmp/tjfy-world-probe.npe
npe-sdl/build/sdl-battle-prepared-item-record-probe.exe tmp/tjfy-world-probe.npe
```

校验器还要求本文所列必要短回归的成功日志；日志统一为 `tmp/prepared-*.log`。原源摘录见 `tmp/battle-effect-vector-clear-source.asm`、`battle-effect-vector-branch-source.asm`、`battle-prepared-floating-filter-source.asm`、`battle-prepared-floating-coordinate-source.asm` 和既有 `battle-spell-prepared-record-source.asm`。

## 仍需完成

- 敌方动作的准备记录及其暖保留；群体状态／攻防标签的其它残留显示组合。
- 非三人队伍、更多连续回合、动作交错、记录重用和其它NPE的具体行为。
- 原音频、扩展字库与根计划中的其余模块。本批不宣称完整引擎完成，整体仍粗估约80%。

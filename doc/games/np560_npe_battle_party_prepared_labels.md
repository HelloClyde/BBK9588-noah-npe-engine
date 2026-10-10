# 角色状态、增益与复活标签，以及防御后毒伤的底图恢复

更新：2026-10-10。生产引擎仍是直接读取当前NPE的C11＋SDL2；原PRG和OpenNOAH仅用于参考采样。

## 实现

- 角色仙术／道具执行时，状态、增益和复活标签读取实际执行者物理人物记录的目标、类型、群体字段和六个准备word。群体显示遍历当前物理记录，保留原准备值的影响，不按预期接收阵营重新裁剪。
- 负状态标签使用保存的选中目标状态：全组可能一起显示“毒”或“解毒”，不改为逐目标生成标签。标签先判断原版首个非零状态效果；四项正免疫的角色分支直接显示“全抗”，与敌方的继续执行分支不同。
- 正增益同时存在时按原版优先显示防御标签。非复活标签要求相应准备word有符号值大于零；零值、负值不显示。
- 坐标及活动字段门槛取决于原选中目标的物理阵营，保留原有的三次上浮位置和体力门槛。
- 类型16～19的复活标签有独立显示门槛：把准备数值换为选中目标最大体力乘5的有符号低16位，再除10。这个值不决定实际恢复体力；乘法回绕成负数时原版仍复活，但不显示“复活”标签。
- 防御后的原LCD画面与恢复底图分别保留。毒伤第一次数字显示使用临时防御画面，随后显示恢复底图。底图按物理队员倒序恢复，选中队员的清除区域向右偏移2像素，再按原坐标重画；邻接角色边缘被擦除的效果同样保留。

生产代码没有针对诊断技能编号、道具编号或游戏名的特判。所有动作参数、角色姿势和动画读取当前NPE／已装载记录；没有新增持久World字段或存档格式。新增的`last_action`仅属于SDL显示上下文。

## 原版参考与覆盖范围

参考采样恢复原版隔离快照，通过原菜单排队仙术／道具和防御。仅改变隔离RAM中的队伍、属性及准备值，原PRG、NPE、在线资源包及WASM保持原哈希。

准备记录设为各槽不同的值101～106、201～206等；双人队伍的执行者另设101／0／65535／104／105／106，以核对零值与有符号负值门槛。动作上下文沿用初始化种子61，没有逐动作重播种或调整随机数来匹配画面；这不证明原机开机随机状态。

| 路径 | 原C／实际SDL画面提交 |
| --- | ---: |
| 群体负状态，选中目标最终为负 | 36 |
| 群体负状态，选中目标最终不为负 | 36 |
| 群体四项负状态 | 42 |
| 单体四项正免疫 | 25 |
| 群体正攻防增益 | 25 |
| 双人队伍，准备零／负值，防御接毒伤 | 34 |
| 道具群体攻击增益 | 25 |
| 道具群体防御增益 | 25 |
| 道具群体复活，准备值含零 | 21 |
| 道具群体复活，显示计算回绕成负值 | 21 |
| **合计** | **290** |

十条路径全部比较原索引像素与C绘图，再由实际SDL主循环输入菜单按键并读取每次提交的像素。行动中59份、回合末59份准备记录一致，每份只比较`+102..113`的12字节。另比较已有声明的属性／状态字段，不将其称为完整148字节记录一致。

原RAM及执行者／目标等附加信息在逻辑tick结束时采样，不是每个LCD提交时的RAM快照。SDL探针使用虚拟视频与音频驱动；本批不验证实际窗口缩放、音频或整段游戏操作。

## 定位证据

旧状态标签在第一条群体路径有233个像素差异；旧复活显示在最大体力10000的回绕路径有215个像素差异。按原准备记录、状态来源及显示门槛修正后消失。

双人队伍路径第23次提交还有4个像素残留。原第22次提交在`(170,87)`、`(169,88)`、`(170,88)`、`(170,89)`保留第二队员防御姿势，第23次底图恢复后全部为零。原执行者为第二物理队员、选中目标为第一物理队员；第一队员倒序恢复时清除区域右边界从168移到170，擦掉这4个相邻像素。接入实际偏移与恢复顺序后，C和SDL全部一致。

四项负状态使用的效果动画有变长帧记录。旧C诊断绘制逐帧读取独立目录，误画487个像素；诊断改用生产已实现的连续首帧长度资源流。生产SDL动画读取逻辑没有为此改动。

原源证据：

- `30:15AE..1610`：复活显示值的有符号低字计算。
- `30:1648..169E`、`1850..187C`、`2522`：选中阵营的活动／坐标分支、体力门槛及正准备值门槛。
- `30:1BC4..1BE2`、`1C40..1FDC`：角色全抗分支与保存目标的负状态标签。
- `39:601E..605C`：临时LCD、隐藏恢复、飘字及毒伤调用顺序。
- `31:7EC2..7F40`、`80F0..828C`：选中队员清除区域偏移、重读坐标、按记录姿势绘图及倒序循环。
- `36:71DE..7678`：毒伤数字与底图交替显示。

摘录保存在`tmp/battle-party-prepared-status-label-source.asm`、`tmp/battle-party-prepared-label-loop-source.asm`、`tmp/battle-party-action-display-order-source.asm`、`tmp/battle-defend-plane-order-source.asm`和`tmp/battle-poison-animation-source.asm`。

## 必要回归

主程序和本批9个受影响探针重新构建无警告，7项短回归全部通过：

| 范围 | 原C／实际SDL画面提交 |
| --- | ---: |
| 既有角色仙术6条路径 | 164 |
| 既有道具8条路径 | 196 |
| 既有复活4条路径 | 90 |
| 角色仙术准备记录6条路径 | 216 |
| 用药／投掷准备记录5条路径 | 138 |
| 普通攻击／防御9条路径 | 151 |
| 敌方状态／中毒6条路径 | 178 |

准备记录探针自动检查全部画面，仅接受NPE路径一个参数；其它探针使用`--all-frames`。第一次批量回归误给准备记录探针多传这个选项，入口参数断言失败；修正调用参数后这两项和随后尚未执行的检查通过，没有改生产代码或原参考以绕过失败。

## 复查

在仓库根目录执行：

```powershell
node npe-sdl/tests/reference_party_spell_animation.mjs --prepared-labels
node npe-sdl/tests/reference_item_animation.mjs --prepared-labels
python npe-sdl/tests/generate_party_prepared_labels_fixture.py
$targets=@('sdl-party-prepared-spell-labels-probe','sdl-party-prepared-item-labels-probe','sdl-party-spell-animation-probe','sdl-item-animation-probe','sdl-revival-animation-probe','sdl-battle-prepared-skill-record-probe','sdl-battle-prepared-item-record-probe','sdl-attack-defend-probe','sdl-enemy-spell-status-probe')
cmake --build npe-sdl/build --target npe-sdl $targets -j 4 *> tmp/party-prepared-label-final-build.log
if($LASTEXITCODE -ne 0){throw 'build failed'}
foreach($target in $targets){
    $log='tmp/party-prepared-label-regression-'+$target+'.log'
    $exe='npe-sdl/build/'+$target+'.exe'
    if($target -like '*prepared-*-record-probe'){
        & $exe tmp/tjfy-world-probe.npe *> $log
    }else{
        & $exe tmp/tjfy-world-probe.npe --all-frames *> $log
    }
    if($LASTEXITCODE -ne 0){throw $target}
}
python npe-sdl/tests/verify_party_prepared_labels.py
```

报告为`tmp/party-prepared-labels-report.json`，绑定原输入、代码、夹具、日志及主程序哈希。

本批完成角色状态／复活标签的这些具体路径。更多混合连续动作、队伍组合、特殊标志、跨NPE后续、原音频及扩展字库仍未完成，以根`plan.md`为准。

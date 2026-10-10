# 模式23经验与模式24 NPC状态服务

2026-10-10。独立C11＋SDL2运行时直接读取NPE，原PRG／QEMU／WASM只用于离线参考。

## 已确认并修正

### 模式23经验

原runtime `27:C8B6`、source `44:C8B6..CBE2`没有提示／等待调用，直接修改首名地图角色记录。

- 经验奖励是门槛与完整事件word乘积的低16位，再做无符号除100；原`C8E4`的经验相加也先回绕到16位，随后才比较升级门槛。旧C用未回绕的和判断升级，65530＋10时错误升级；原结果是经验4、不升级。
- 成长属性先执行16位加法，再使用有符号比较决定是否封顶999。旧C先按宽无符号和截到999，遗漏加法回绕及高位值的原比较规则。普通封顶、32766及65534输入的原结果已核对；这些高值是明确的隔离算术输入，不声称自然游戏出现过所有边界。
- 保留第五项增长未封顶时读取防御值的原赋值，不自行修正原程序规则。
- 原`CBD0`在升级或未升级时都请求地图重新初始化。C已补齐。

### 模式24 NPC状态

原runtime `30:383C`、source `4D:383C..3948`接受完整事件word的1～249；其它编号不改NPC数组。合法字节先零扩展、减一并写回低字节，随后再次零扩展检查：所以0变255、1变0、2变1、255变254。旧文档把这里误解为零饱和，旧C的`>0`保护与该解释一致，但与原代码及两个执行器的结果不符；现已纠正。

原`393A`无论编号是否有效都请求地图重新初始化。C补入`npe_world_apply_npc_state_event`，数值后继和SDL原始模式入口共用；保留完整事件word。主程序也接入直接进入模式23／24的分派，随后沿已有地图／音乐初始化流程返回。

## 原程序证据

`reference_simple_events.mjs`从原UI保存的快照进入23／24；18条隔离输入包括升级、不升级、等级99、零门槛、两种经验回绕、三种成长属性边界、NPC60的0／1／2／255、NPC1／249的0及0／250／65535非法事件。保存三名完整148字节角色、250个NPC状态和原LCD输出。

`reference_simple_events_native.py`用独立QEMU执行同两段原函数：只有03段的诊断入口写入，原服务区域逐字节与PRG检查；恢复稀疏输入的整个52段64KiB并核对，函数正常返回且栈恢复，无缺失ROM、未知指令或非法控制流。18组角色及NPC输出与上述WASM完全一致，原输出只改变预期的角色／NPC字节、模式与初始化标志；整个52段的其它字节保持。此为原数值函数的隔离状态证明，不是物理设备采样或所有C RAM的对照。

NPC0回绕由两套执行器及源指令共同确认，不以旧文档或C结果作为预期。反汇编为`tmp/experience-audit-source.asm`、`tmp/npc-decrement-audit-source.asm`。

## 真实NPE入口与C／SDL

《天降风云》地图1选择事件2缓存为`0B 18 00 01 00 3C`：默认第二选项调用24／NPC60。`reference_npc_decrement_choice.mjs`保持原NPE表和程序，隔离设置NPC60为0、1、2，原选择输入得到255、0、1；共6次原LCD提交。初稿在16步释放段之后过早送确认，原输入服务未受理，模式保持17；改用既有参考的64步就绪段后通过。未改游戏资源或原预期画面。

生产`main.c`的18个直接服务入口及上述3条原NPE选择链，共24张完整240×120索引画面与实际SDL读回逐像素一致。18组数值后端和21组显示终态的全部444字节角色、250个NPC状态及模式／事件／初始化匹配；资源绑定一致且无运行时错误。参考的UI RAM是advance结束状态，不宣称每次提交瞬间或硬件时钟相位相同。

修复前新数值探针12／18组不符，修复后全部通过。新探针初稿的字段名及指针写法编译错误已修正，失败日志保持；最终主程序和检查目标构建无警告。

## 必要短回归及历史纠正

- 天明139／140特殊队伍：10组原人物状态继续一致，42次C／SDL输出；包含真实23／1029后继。没有新采样其原完整LCD。
- 既有剧情后继46次、选择27次原C／SDL对照继续通过。
- 上批治疗12条原路径、46张原参考和56次C／SDL提交继续通过。
- 旧`tjfy-world-probe`的零状态期望按两套原程序及实际NPE取消入口改为255；本批没有重跑其长综合流程。旧模式24说明同步纠正。

日志及独立报告：`tmp/simple-events/`；`run-manifest.json`将当前生产源码及实际执行二进制绑定到本批运行，`verify_simple_events.py`另比较保存的原／C帧及原CPU角色／NPC输出。历史通过报告留存，不改写上批源码哈希。

## 复现

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_simple_events.mjs
& 'D:/ProgramData/miniconda3/python.exe' npe-sdl/tests/reference_simple_events_native.py
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_npc_decrement_choice.mjs
python npe-sdl/tests/generate_simple_events_fixture.py
cmake --build npe-sdl/build --target npe-sdl sdl-simple-events-probe -j4
& npe-sdl/build/sdl-simple-events-probe.exe tmp/tjfy-world-probe.npe
python npe-sdl/tests/verify_simple_events.py
```

独立汇总还读取本批已完成的四项回归及运行清单。参考需要现有离线原包／WASM／快照／QEMU，主程序不需要这些开发文件。原声、其它事件及状态组合、完整跨NPE动态兼容仍未完成，整体仍粗估约80%。按用户要求不加入原机存档互通、正常属性完整通关或SDL全程手动实玩。

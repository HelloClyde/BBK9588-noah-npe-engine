# 模式22治疗服务与选择后的绘图面恢复

2026-10-10。生产实现仍为独立C11＋SDL2，直接读取NPE；原PRG与参考WASM仅用于离线对照。

## 确认的缺口

旧`world_resolve_simple_mode`以无符号余额／费用比较，直接扣钱、恢复角色并返回地图；遗漏原提示、等待和地图重新初始化。SDL分派也没有独立的模式22路径。新增原参考探针在修复前因没有预期提交而失败，日志保留为`tmp/healing-service/check-before.log`。

## 原服务及实现

原source `4C:D4B6..D5DC`、runtime `2F:D4B6`：

- 事件完整word是16位有符号费用，不取低字节；余额使用32位有符号判断，扣除按32位回绕。负费用／负余额是隔离算术输入，不声称游戏自然产生这些值。
- 足额时扣费，恢复三个物理地图角色记录的体力与气至各自上限，包括未入队角色。
- 成功先选择保留面、清零并提交，再显示原`4C:A970`文字“休息一天后主角恢复体力”；提示没有自行加写“气”。
- 不足时保留原背景，显示`4C:A988`的“身上的金钱不足”，不扣费、不改变角色数值。
- 两条路径调用公共2000毫秒提示服务，普通按键可结束等待且由提示消费；结束后恢复背景，才设置模式1和地图初始化标志，费用word保留。

生产`npe_world_healing_effect`与`npe_world_finish_healing_event`分离数值和后继。非显示调用者执行两步；实际主程序保留模式22，经`show_healing_event`显示、等待、恢复后完成，随后沿既有地图／音乐初始化流程返回。窗口关闭返回-2，已执行的数值修改保持，后继不伪造完成。

固定提示来自原固件；客栈的选择图片、是否进入治疗及价格仍读取当前NPE。没有加入游戏名称或游戏编号特例，没有改变存档格式。

## 选择后的保留面遗漏

实际《天降风云》地图5第一条选择缓存为`16 01 00 0A 00 01`：第一分支治疗10，第二分支返回地图。原`45:D39A..D3A2`在确认后选择保留绘图面1，不额外提交。旧C只有可见的选择按钮画面，余额不足提示因此残留按钮，新增入口对照相差225像素。

现在选择显示时保存进入前的背景；确认后恢复该绘图源及地图边缘，不增加LCD提交。成功清屏、余额不足提示及拒绝治疗三条真实资源分支均匹配。这个背景属于同步界面状态，没有写入剧情资源或自身存档。

## 原参考范围及结果

- `reference_healing_service.mjs`：9条隔离原服务路径，覆盖足额、不足、刚好足额、免费、65535／32768费用、负余额与正值回绕；33次原LCD提交。
- `reference_healing_choice.mjs`：由原保存／读取界面实际重新装载地图5，使用未修改的NPE选择表；足额、不足、拒绝三条链，13次原LCD提交。初次只改地图号未重新装载，仍读到地图1驻留表而失败，日志保留；最终通过原界面完成装载。较长64步输入段的样本另带周期重复提交，最终使用16步键段并在返回地图后停止；没有改原图像或预期数值来适配C。
- `sdl-healing-service-probe`执行生产main的渲染、选择输入和服务分派；46张完整240×120原索引画面与C及实际SDL读回逐像素一致。实际NPE表6字节前后保持，角色记录／余额的资源RAM绑定一致。
- 无按键2000毫秒和Uint32回绕、按键消费、两种结果的窗口关闭、9组数值服务及真实选择的非显示后端通过。额外10次重复画面对照，总计56次C／SDL提交。
- 原参考的RAM快照为每次advance／输入段结束状态，不是逐LCD提交瞬间状态。C顺序另由原反汇编及提交断言支撑。名义SDL等待检查不证明硬件64Hz或消抖相位完全一致。

## 必要短回归

主程序及新探针构建通过且无警告；选择27次、金钱48次、剧情后继46次既有原C／SDL对照通过。旧事件链回归的音乐通知计数失败，在隔离重建的改动前生产代码上同样复现；重建world的CRLF SHA256与上一批权威哈希完全一致。NPC状态和实际音乐游标均正确，仅旧探针未监听范围通知。补入现有`NpeSoundRangeObserver`接线后，70帧原参考的两种事件链、关闭传播及数值调用通过；原预期与画面未改。

失败记录、隔离基线、最终构建／检查及四项短回归均在`tmp/healing-service/`；独立保存帧比较汇总为`tmp/healing-service/report.json`。本批没有重跑完整剧情、正常属性通关、全程SDL手动实玩或原机存档互通。

## 复现

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_healing_service.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_healing_choice.mjs
python npe-sdl/tests/generate_healing_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-healing-service-probe -j4
& npe-sdl/build/sdl-healing-service-probe.exe tmp/tjfy-world-probe.npe
python npe-sdl/tests/verify_healing_service.py
```

独立报告还读取本批已完成的四项短回归日志。参考工具需要现有离线原包／WASM／快照；主程序无需这些开发文件。原声、共享状态的其它实际差异、剩余事件与跨NPE完整动态兼容继续；整体仍粗估约80%，目标未完成。

# NPE 结束返回标题及新游戏／读档后继

更新：2026-10-10。生产实现为独立C11／SDL2，直接读取当前NPE。原PRG、QEMU和原WASM仅用于开发对照。

## 原规则与修复

| 调用点 | 原行为 | C实现 |
| --- | --- | --- |
| 模式21，4A:F59A..F5D2 | 两次重读7A6A，先解锁后释放地图缓冲；写模式7、初始化1 | 新资源服务和World分派接入实际运行时；地图缓存失效，保留句柄和发布指针 |
| 结束资源清理 | 不释放战斗缓冲，不改7AA4、事件号或音乐 | 撤去旧结束函数的停播和额外状态重置 |
| 标题初始化，3A:8A4E..8A6A／48:0DA2 | 先提交清屏，再重设7A7A／7A7C／7A7E为1，绘制当前NPE标题，最后清初始化标志 | 实际main按同一顺序清屏、重设标题／槽位选择和阶段、提交标题；箭头同步选择字节 |
| 标题读档页，3A:8F26..8F42 | 设置槽位阶段2，不停播 | 补阶段字节，撤去原错误停播；旧注释引用的8F74实际属于退出分支 |
| 结束后的地图重建 | 新游戏46:D1C6、占用槽3A:9024重新分配7000字节地图缓冲 | 下一次构造／读取的地图装载先分配新缓冲，不写入已释放的旧发布指针 |
| 运行中读取存档 | 7936不在原slot codec中，事件号保留 | 暖恢复保留当前事件号；自身冷解码仍读取保存字段，存档格式不变 |

没有添加游戏名、地图号、剧情或图像的生产特例。标题仍由当前NPE的背景、名称及原界面资源绘制。

## 原资源与状态对照

`reference_rpg_resource_lifecycle.py --ending`运行未修改的原结束分派片段及原堆函数。F606处提供受控返回；原片段F59A..F5D4和各堆／装载函数在执行前按原PRG字节核对。文件读取使用诊断适配，堆节点全部由原分配函数生成。

六组输入覆盖战斗资源标志高字节1／0／2、地图句柄0、重复结束和结束后重新分配。每组完整128KiB驻留RAM、18字节读取记录与C零差异。事件BEEF保留、初始化变1；战斗标志保持，释放后旧句柄／指针保持。

报告：`tmp/ending-return/native/report.json`；C对应完整输出为`tmp/ending-return/c-ending-*-ram.bin`和`*-trace.bin`。这是明确服务范围的完整内存对照，不是原完整主循环或文件系统的证明。

## 实际main／SDL后继

`reference_ending_return.mjs`恢复原已装载地图快照，寻找实际运行的驻留RAM；仅在入口设置模式21、事件BEEF、初始化0和旧标题控制字节。程序和NPE不修改。这是隔离服务输入，不计为新增自然剧情通关。

音乐开／关分别执行两种后继：

| 路径 | 原整屏提交 |
| --- | ---: |
| 结束→标题清屏／标题→下→上→新游戏 | 每条7次，共14次 |
| 结束→标题清屏／标题→下→槽位→占用槽读取 | 每条5次，共10次 |

四条合计24次原完整240×120索引画面，与生产C组合结果和实际SDL读回均零差异。模式、事件、初始化、地图／进度、玩家／视口、标题控制字节、地图及战斗句柄／标志匹配。C堆位图确认标题期间地图缓冲释放，新游戏／读取后重新占用；原数值后端另证明对应释放与重新分配的堆规则。

C使用自己的隔离三槽文件提供相同游戏数据，暖读取重新装载NPE的NPC；没有要求原机存档格式互通。进入读档页和占用槽读取均无原音频命令；新游戏仅在音乐开时停止并重播保留地图音轨。实际C通知顺序匹配，不以这些通知证明PCM或物理播放时序。

原始输出：`tmp/ending-return/original/report.json`及24张`*-*.idx`；最终C日志：`sdl-save-final-check.log`。

## 必要回归与修复前证据

- 修复前World代码从前批原文件保存，并校验前批哈希。两条音乐状态下原结束控制路径均失败：初始化仍0、地图未释放、额外停播；记录`before-final-check.log`。
- 实际结尾NPC184→故事200..203→37幅告别动画→模式21→当前NPE标题保持通过。
- 原标题占用槽首次加载3次画面、系统槽返回33次画面、场景／开场281次既有原画面对照通过。
- 九条显式shutdown、九条直接close及八条实际main退出路径通过。
- 主程序与受影响目标构建无警告。汇总`tmp/ending-return/report.json`通过，校验器直接比较上述原／C保存文件及必要日志。

首次把标题绘制状态强行写成背景0，导致冷标题占用槽读取的1920个边框像素错误；已经撤去该赋值，原参考未改。开场探针原将标题清屏当作第63幅动画；现单独检查清屏，原62幅开场动画参考保持。首次占用槽后继的画面已经匹配，但事件号被自身暖恢复覆盖；修正其保留规则后状态和画面均通过。失败日志保留。

## 复现

```powershell
python npe-sdl/tests/reference_rpg_resource_lifecycle.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --ending
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_ending_return.mjs
python npe-sdl/tests/generate_ending_resource_fixture.py
python npe-sdl/tests/generate_ending_return_fixture.py
cmake --build npe-sdl/build --target ending-resource-probe sdl-ending-return-probe npe-sdl -j 4
& npe-sdl/build/ending-resource-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-ending-return-probe.exe tmp/tjfy-world-probe.npe
python npe-sdl/tests/verify_ending_return.py
```

## 剩余范围

原声解码／PCM、完整输出渐变及循环结束时序，其它事件／队伍／动作组合、共享状态生命周期和跨NPE完整动态兼容继续。当前参考未证明完整原标题RAM、全部输入／槽位组合、物理计时或原声音。自身冷解码与新增状态恢复继续随相关模块核对。

整体仍粗估约80%，目标保持未完成。原机存档互通、正常属性完整通关及SDL全程手玩按用户要求排除。

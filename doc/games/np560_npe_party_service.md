# 原版模式10入队／离队服务

日期：2026-10-07。运行时仍为纯C11／SDL2；原厂PRG及WASM仅用于离线参考。

## 原厂行为与实现

原服务位于`3F:0F60..146A`，完整反汇编保存于`tmp/real-party-service-original-source.asm`。

- 事件号先截为字节，再检查1..3；越界清模式为0。当前NPE地图第6组以7字节记录提供加入标志、角色编号、后续模式和事件。
- 活动标志严格比较1。重复入队、未入队的离队不显示提示，但仍沿后续事件并请求地图初始化。
- 加入先检查三槽是否已有该编号且活动；否则只选择第二／第三槽。第二槽不是1时，加入第三槽角色交换两个完整记录。第一槽不会被移动。
- 离队只检查第二／第三槽；第二槽离队且第三槽为1时，交换记录并清除最后槽的活动标志。其它标志值不参与活动判断。
- 原程序交换148字节角色记录；C交换所有已经建模的角色字段，包括属性、装备、技能、成长及四个地图状态词。未建模字节不宣称已与C逐字节对应。
- 名称取当前NPE的角色记录，后缀取原固件`4C:A9C0/A9CA`的“离开队伍”／“加入队伍”。沿用原居中16像素字库提示框，保留进入前LCD。
- `03:4050`的参数2000ms：提示显示后等待；此前普通按键忽略的结论已撤回（见下方后续输入差异）；恢复底图后才赋后续模式／事件。主程序沿原`7AB4=1`请求地图初始化。关闭窗口时保留未执行的后续。

代码入口：`world.c`的`npe_world_change_party`和`npe_world_finish_party_event`；`main.c`的`show_party_change`、`show_party_event`与模式10分支；`text.c`的`npe_render_party_notice`。没有游戏／地图／NPC／物品编号特判，没有新增存档字段。

## 参考输入与证据范围

`tests/reference_party_service.mjs`从原厂真实启动、新游戏、原UI保存所得快照恢复。通过F4定位活动RAM，再返回地图；使用原三名各自不同的148字节记录，诊断调整第二／第三槽排列及活动标志，不复制第一角色造出队员，也不修改属性。

为隔离服务，原厂将**当前原NPE中未改写的第6组记录**放入原服务缓存`78A2..78A6`，保留原地图1及其LCD／NPC状态。C探针在内存副本中把同一7字节原记录放入地图1第6组首记录，经过实际`advance_world_events`、真实解析器、提示函数及地图返回渲染。原NPE／PRG磁盘文件保持原样。

这验证原厂模式10服务及C／SDL调度，不把诊断缓存入口当成自然地图转换或全程游玩的证据。参考文件`tmp/native-party-service.json`保留来源地图／事件号、角色原始字节、前后字段及逐次提交；`.idx`保存实际原厂LCD，不由C生成参考图。

48组为两种排列×六种活动标志×加入角色2／3与离开角色2／3，另加12组原NPE地图120的两次入队后续链。合计60组、38次提示、136次原厂LCD提交；实际main.c调度的136次C／SDL整屏提交均零差异。每阶段对照三名角色共126个字段，并核对NPC位置／方向和菜单人数缓存。

`tmp/party-service-record-integrity.json`另核对180个原厂完整148字节记录：除活动字节+43外，每个字节随原角色身份保留。C探针另外检查名义2000ms等待、计时回绕、普通F4忽略、关闭窗口停止后续、事件0／4／255／256清模式和257截为1。

原厂一次`advance`可包含完整提示等待及三次提交，原采样的tick标记不能作为物理64Hz相位证明。C的等待使用注入时钟；真实tick相位和原机墙钟偏差仍属于时基待办。

## 验证命令

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_party_service.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_party_chain.mjs
python npe-sdl/tests/generate_party_service_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target sdl-party-service-probe -j 4
& npe-sdl/build/sdl-party-service-probe.exe tmp/tjfy-world-probe.npe
```

参考采样需已有原厂快照／PRG／WASM包；它们不属于C运行时依赖。最终构建／定向回归结果见`tmp/party-service-report.json`及`party-service-*.log`。正常属性完整通关及SDL从头到尾实玩继续取消。

## 构建与剩余工作

全量构建退出0、无警告；扩展链夹具的最终探针构建及运行退出0。ctest5/5和10项相关回归退出0，日志为`tmp/party-service-all-build.log`、`party-service-chain-build.log`、`party-service-check.log`、`party-service-ctest.log`与`party-service-regression-*.log`。

本轮未证明原厂队伍事件缓存装载／空槽／重入、连续离队后转战斗的全部提交及物理tick相位；这些与其它通用引擎待办保留。整体目标仍未完成。

## 后续更新：队伍缓存与初始空槽

缓存装载／空槽重入现以10组原厂UI读档对照完成。初始空第三槽增加1次原厂提交，模式10合计61组137次实际C／SDL整屏匹配，详见[队伍缓存](np560_npe_party_cache.md)。本页上轮60组136次保持原统计；原冷标题缓存、离队链转战斗完整提交及物理tick相位仍待核对。

## 后续更新：离队链与输入差异

七组连续离队转战斗46次实际C／SDL提交现匹配，详见[离队转战斗](np560_npe_leave_battle.md)。另补采七种按键及提交指令偏移，发现原参考提前返回，C仍忽略普通键；本页此前“普通F4忽略”仅是C实现检查，不能作为原版输入相同的证明。原4F:4C34提前返回条件及参考调度仍待还原，输入等待服务不宣称完全复刻。

## 后续修订：提示输入已接通

[定时提示](np560_npe_timed_notice.md)补齐原键盘扫描证据，C共用等待已支持按键提前结束并消费结束键；连续提示只按一次不会跳过第二提示。16组104次离队对照及本页61组137次回归通过。此前“普通键忽略”的实现检查作废，实际硬件时基仍未证明。

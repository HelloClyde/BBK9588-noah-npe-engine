# 原版队伍事件缓存与空槽重入

更新：2026-10-07。纯C11／SDL2运行时直接使用当前NPE资源；PRG／WASM仅用于离线参考。

## 原装载器的保留规则

重新从未修改的`tmp/NP560.PRG`反汇编`29:BDBE..E138`，保存为`tmp/party-cache-loader-original-source.asm`。队伍事件循环`29:DFB8..E12E`按三槽、6字节步长装载：只有索引小于当前第6组记录数才读取。它跳过NPE记录ID，写加入标志、角色编号、后续模式和后续事件；没有清除未使用槽。缓存中字节+3不参与服务，也不在这段代码里改写。

`tests/reference_party_cache_load.mjs`从真实启动／原UI保存快照恢复，通过原UI保存诊断目标地图，再从系统或标题读回。装载前以当前原NPE的真实事件字段设置三槽缓存，另给未使用的+3字节设置128／129／130以观察装载范围。

共10组，目标地图的第6组数量分别为0、1、2、3，两种读档入口均验证。只有提供的前若干事件覆盖缓存，余槽和全部+3字节保留旧值。目标地图编号只用于离线夹具；C生产代码不含对应编号例外。原厂结果为`tmp/native-party-cache-load.json`，保留所有前后缓存与角色／NPC状态。

## C实现与空事件的返回

- `NpeWorld.party_events[3]`保存运行中的事件字段。`npe_world_load_party_events`先校验并装载临时副本，只覆盖当前地图有的记录；没有目录的已支持短地图维持原有装载行为。
- 地图初始化、脚本换图和出口换图经过共同装载入口；运行中读档从当前缓存开始，再覆盖目标地图的实际记录。新游戏／主机冷解码从零缓存开始。这不宣称原机冷标题启动的全部驻留缓存生命周期已验收。
- `npe_world_party_event`按截为字节的1..3号读取缓存，包括地图当前未提供的旧槽。模式10的后端及实际SDL调度均使用它。
- 原服务能读取初始第三槽的全零记录，先留下模式0／事件0。原通用调度`4A:F498/F4A0 ->F5EE..F602`随后将越界模式改为1，并请求地图初始化，保留事件词。C主调度补齐此通用路径，没有把空槽当作缺失NPE记录忽略。

实际原通用调度反汇编另存`tmp/party-cache-dispatcher-original-source.asm`。初始空第三槽使用`tests/reference_party_empty.mjs`采集，与上轮48组单事件和12组连续入队合计61组137次原厂提交。实际`main.c`／SDL全部137次整屏零差异；后端模式0／事件0与调度后的模式1／事件0分别检查。

## 缓存验证范围

`party-cache-probe`直接调用C的初始化、存读档、模式10及脚本换图接口，对照10组原厂系统／标题UI读档缓存。验证0～3记录的覆盖／保留、8组地图缺失第三记录仍执行旧事件、重复恢复、主机冷解码、损坏档保持整个原状态和非法完整目录不部分覆盖；脚本换图另检查三槽更新。C缓存只建模服务实际使用的字段，未把原未使用字节+3算作已建模字段。

只改缓存后生成的两份`NPESAV15`逐字节相同，证明新运行缓存没有进入存档。此次没有新增存档格式或版本。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_party_cache_load.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_party_empty.mjs
python npe-sdl/tests/generate_party_cache_fixture.py
python npe-sdl/tests/generate_party_service_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target party-cache-probe sdl-party-service-probe -j 4
& npe-sdl/build/party-cache-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-party-service-probe.exe tmp/tjfy-world-probe.npe
```

构建、局部回归及原文件哈希见`tmp/party-cache-report.json`和对应日志。10组原厂UI记录与C直接恢复分开陈述；137次SDL提交来自模式10隔离服务及调度，未用它证明完整自然换图或原机64Hz相位。

连续离队转战斗的完整提交、其它服务缓存、冷启动状态、真实时基、声音及跨NPE待办继续。用户取消的正常属性完整通关和SDL从头到尾实玩继续不安排。

## 最终结果

全量构建退出0且无警告；ctest5/5通过。缓存10组、模式10服务137次、原角色页面95次、系统返回77次、商店状态读档、暖存读档、真实标题加载、存档失败反馈、NPC绘制及群攻10项相关探针均退出0。最终日志：`tmp/party-cache-final-all-build.log`、`party-cache-ctest.log`、`party-cache-regression-*.log`；原文件哈希见报告。

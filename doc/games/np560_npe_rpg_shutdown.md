# 主循环退出的地图与战斗资源清理

更新：2026-10-09。生产代码为C11／SDL2；原PRG与QEMU只用于开发对照。

## 实现与原调用点

`npe_rpg_resources_release` 按原3C:D50E..D564执行：

1. 重读52:7A6A并解锁地图缓冲，再次重读同一槽并释放。
2. 读取52:7AA4；高字节恰好等于1才清理战斗缓冲。
3. 战斗清理分别重读52:7A88执行解锁／释放，将高字节置0；52:7AA5低字节保持。

地图清理不检查句柄是否为0；战斗高字节2、129、255均跳过。释放后保留地图／战斗句柄与已发布指针。堆可能移动载荷，旧指针不保证仍指向原图像。

`npe_rpg_runtime_shutdown` 将这个服务接入附着World的实际资源运行时；`npe_rpg_runtime_close` 调用它后清理启动缓冲。生产main通过既有 `npe_free` 走到清理路径。场景／人物动画流仍在运行时关闭中释放；完整原版各流／文件关闭的相对顺序尚未证明。

没有新增World字段或绑定，仍3536字节。save.c与模拟器自身NPESAV22保持。

## 原版参考与范围

参考执行未修改的退出清理片段和原堆／装载服务。原PRG逻辑3C:D50E对应当前原程序执行地址1F:D50E，1F:D568处插入受控返回，停在文件关闭之前。输入设置与NPE定位／读取使用诊断适配；堆节点由原分配函数生成，不植入采样堆头。

九组输入为高字节1、0、2、129、255，地图句柄0、战斗句柄0、两个句柄0，以及重复清理两次；低字节均为A6。每组比较完整131072字节RAM及18字节读取记录。

附着真实NPE／World的C运行时另执行九条显式shutdown和九条直接close路径。显式路径的46字节控制／堆位图结果逐字节对应原参考；直接close检查共享资源word及运行时已释放。各次关闭后重新构造，验证资源可重新分配。

实际main／SDL八条退出路径覆盖标题入口的开场动画首帧（高字节0／1／2／129／255）、地图（0／1）和原剧情战斗入口（1）。通过实际SDL提交后排入SDL_QUIT，生产main退出并调用npe_free；在World栈对象仍存活时保存清理前控制记录、清理后word、SDL提交和释放次数。没有在main返回后访问其World指针。

## 检查与复现

新增目标 `rpg-shutdown-probe`、`sdl-rpg-shutdown-probe`。必要回归沿用前批40项短检查，43目标构建无警告，42项短检查全部退出0。独立汇总重新对照九组完整RAM／读取记录、运行时与main保存状态，并核对前批774画面、1006份人物记录、134份资源快照及自身存读保存输出保持。报告 `tmp/rpg-shutdown-report.json` 已通过。

- 原参考：`npe-sdl/tests/reference_rpg_resource_lifecycle.py --qemu tmp/np560-gzip-win-build/qemu-system-dmcu.exe --shutdown`。
- 原结果：`tmp/rpg-shutdown-native/report.json`及九组RAM／trace文件。
- 生成夹具：`npe-sdl/tests/generate_rpg_shutdown_fixture.py`。
- 构建与检查：`tmp/run_rpg_shutdown_checks.py`。
- 独立汇总：`npe-sdl/tests/verify_rpg_shutdown.py`。
- C结果：`tmp/rpg-shutdown-c-*`、`tmp/rpg-shutdown-runtime-*`、`tmp/rpg-shutdown-close-*`、`tmp/rpg-shutdown-main-*`。

## 首轮问题与修正

原参考首次失败来自旧的“发布指针仍指向完整载荷”断言。地图释放会触发堆整理，该断言不适用于退出后的旧指针；仅在新shutdown模式撤去它，原其它参考模式保持。完整RAM对照验证实际移动／保留行为。失败日志为 `tmp/rpg-shutdown-first-native-failed.*`。

首轮回归六项失败源于同一个旧入战探针：关闭数字路线的运行时后才与SDL路线比较World。现在关闭会按原版清资源标志。探针先保留入战结果、检查关闭后的正确标志，再比较关闭前的两个入口结果；原参考画面／记录和生产代码保持。首轮日志为 `tmp/rpg-shutdown-first-batch-*`。

## 仍未完成

完整原版启动／退出RAM与流／文件关闭顺序、其它共享字段和UI资源生命周期、连续战斗其它动作与记录重用、原PCM／物理时序以及其它NPE完整动态运行继续。整体工程仍粗估约80%；原机存档互通、正常属性完整通关及SDL全程实玩按用户要求取消。

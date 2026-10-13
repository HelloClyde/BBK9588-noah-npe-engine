# 主角行走：碰撞顺序与保留 LCD

更新：2026-10-07。C11 实现直接读取当前 NPE 的地图、角色及 NPC 图组，SDL2 显示原厂提交顺序。没有游戏、NPC 或坐标编号例外。

## 原厂调用及实现

`3A:0000..0774` 的主角碰撞扫描依次为八个物件位置、二十个非零且剧情范围有效的 NPC、十二个出口、四个固定怪物、剧情触发位置，最后调用地形服务。物件的位置比较不检查 ID 或已拾取标志；物件、NPC 和固定怪物使用展平位置比较。C 原先把剧情触发及出口放在 NPC 前面，且没有主角物件碰撞；现按原顺序修正。原厂的“上方 NPC 与出口同格”样本现在受阻，不再换图。

`3B:1452..1B0E` 处理行走显示。`46:BBF2..BE8A` 在旧位置绘制四帧，索引为 **0、1、2、0**，每帧先擦除旧图块，再以 mode2 绘制当前 NPE 的角色位图并提交。下面有 NPC 记录时前三帧只擦上图块，第四帧擦上下两块；这个坐标检查不看 NPC ID。`4A:908E..923C` 的全擦除顺序为下块、上块。

受阻时先逐槽绘制 NPC、检查上下相邻关系，再处理固定怪物。首个上下相邻者决定主角遮挡，立即提交静止帧并返回；没有上下相邻者时仍播放四帧动作。NPC 每次绘制保留原有上下四行恢复服务，隐藏或屏外非零 NPC 也执行；固定怪物绘制没有这段尾部恢复。

成功移动先播放旧位置四帧，再擦除旧主角。滚屏由 `3F:D700..DBDE` 在包含端点的 `(0,4)..(239,115)` 矩形移动 16 像素，按新视口绘制新露出的图块。随后按槽绘制 NPC：首个上下相邻 NPC 会提交一次并停止这个 NPC 循环，但固定怪物循环仍继续；最后再提交一次。因此相邻 NPC 样本为六次提交，普通移动为五次。

`npe_world_step_with_display` 将显示放在坐标改变、选曲／遭遇处理之前；原有 `npe_world_step` 继续作为无显示的数值接口。主程序方向键已调用显示入口并保存结果 LCD，后续普通绘制复用该画面。追击擦除／绘制与 NPC 周期绘制共用原有实现，方向键后的追击绘制不额外调用 NPC 周期。

## 等待与输入

原厂 `46:BDDC` 调用 `03:411C`，经 `4F:6062..60CE`、`50:463E..4656` 到 `4E:7286..7338`。参数 **10 是毫秒**，计算 `max(1, floor(10*64/1000))`，得到一个 64 Hz tick，写入共享 RAM `0494`。`0496` 属于另一条周期计时路径。前台检查到 `0494` 为零后生成事件 `4/F0`，等待返回，随后 `46:BDE2` 才提交 LCD。普通事件被消费；固件另有类型3的特殊事件提前返回分支，其物理功能尚未完整映射。

2026-10-13 重新按原 PRG 字节核查并修正 SDL：移除网页呈现策略引入的每姿态固定67毫秒，恢复等待后提交。等待以 SDL 游戏时钟的公共64Hz相位计算，参数10等待下一个tick边界，毫秒截止时间向上取整。时点1234毫秒的四次等待结束于1235、1250、1266、1282毫秒；不会每帧重新固定等待16毫秒。关闭窗口中断尚未提交的姿态，数值接口不等待。粗时钟、事件处理和绘图耗时可能使提交晚于截止时间；该换算不声称真机恒定15.625毫秒帧率。

原 PRG SHA-256 为 `d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5`。原初始化 `32:DC44..DC4E` 设置TBC为 `8009`，芯片资料图24/25的低四位 `1001` 对应64Hz；`08:40F4..40FA` 每次时基中断递减非零0494。其后每四tick的后台回调不限制前台到期检查。新的 `sdl-walk-timing-probe` 不依赖游戏、固件或私有参考帧，覆盖不同起始相位、四姿态连续等待、普通输入消费、退出前不提交、零等待和32位时钟回绕。

## 原厂参考与验证范围

参考入口 [reference_walk_display.mjs](../../npe-sdl/tests/reference_walk_display.mjs) 使用 `certified_original_v1` 原文件和同一真实首图快照；每组恢复后等待 15 次无输入推进以释放按键，再诊断注入 NPC 记录及关闭声音／随机遭遇标志。注入后实际 LCD 是输入帧，随后收集一次方向键后的全部实际提交，不通过 C 生成参考图。每次恢复重新定位 RAM，并以原始 NPC 图组记录排除陈旧内存副本。

共 **11 组、48 次原厂提交**。其中十组地图行走／受阻的地图、剧情进度、模式、朝向及视口状态也与原厂一致。方向键计时器从非零测试时点重置，等待期间未触发周期服务。共 **42 次**已通过纯 C 整屏比较、直接 SDL 比较及主程序真实方向键入口的逐提交比较；SDL 共 **84 次**零差异。每次主程序返回后的额外静态重绘也与末帧一致，单独检查，不增加原厂提交数。上下、左右受阻、左右滚屏、移动后的上下相邻 NPC、隐藏相邻 NPC 均包含在样本中。

**此前六次是 `up-empty` 出口换图流程的部分记录，当时没有计为通过；本轮已补齐七次并通过，详见下节。** 探针以退出 0 表示上述具名十组范围通过，并明确打印出口待核对计数。尚待补充正常上下移动、非中心／上下／地图边界滚屏、非零固定怪物、多个相邻槽及物件／触发重叠的原厂样本。受阻或剧情触发后的遭遇／追击处理、换图完整提交、脚本／对象缓存联动、跨 NPE 机制也尚未全部还原。没有宣称整个行走模块或模拟器完成。

最终全量构建无警告，ctest 5/5。九项局部探针均退出0，覆盖行走、NPC移动／共享RNG、NPC重绘、追击重绘、边缘恢复、短路径追击战及冷却、庙中九步触发、开门后十六步通行、存档／标题；不重跑用户取消的正常属性完整通关或 SDL 全程实玩。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_walk_display.mjs
python npe-sdl/tests/generate_walk_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-walk-pose-probe.exe tmp/tjfy-world-probe.npe
```

原厂证据：`tmp/native-walk.json`、`native-walk-*-before/commit-*/after.idx`，夹具 [walk_native.h](../../npe-sdl/tests/walk_native.h)，日志 `tmp/walk-final-sdl-walk-pose-probe.log`，计数 `tmp/walk-counts.json`、回归 `tmp/walk-final-*.log`。源码记录 `tmp/walk-{controller,player,step,player-erase,collision}-original-source.asm`、`walk-scroll-source.asm`、`walk-wait-timer-source.asm`；参考及文件哈希报告 `tmp/walk-reference-report.json`。原 NPE SHA-256 为 `DF4B52DF6B69E1105A5EA6100E43F35F832ED859C10CB8714879E44AD4D8CB40`，PRG 为 `D555609E03ECE17F42C1881BEEB88361572F18621161DE67BC9DC765857089F5`；NPESAV15 不变。


## 出口换图：完整七次提交

本轮重新捕获原厂同一出口，延长推进到五秒计时提示结束。旧记录仅推进32次，停在地图名称框等待中，不能称为完整出口流程。新记录在第303次推进捕获恢复帧，最终模式1、地图91、进度1、朝向2、屏幕坐标7／4、视口5／8。**参考JSON中每张提交的state是该次advance结束后读取的状态；它不证明LCD提交瞬间的CPU状态。**

七次实际提交依次为：

| 提交索引 | 原厂画面 | C／SDL实现 |
| :--- | :--- | :--- |
| 0～3 | 原位置0、1、2、0动作 | 既有受阻前缀与行走服务 |
| 4 | 新地图与静止主角 | `npe_render_world_load` |
| 5 | 当前地图名称提示 | `npe_render_map_name_notice` |
| 6 | 恢复提交4的地图 | 保留LCD恢复 |

原厂`48:21E0..241A`装载地图，`23B2..23D0`先选择地图音乐，`23E4`装入图块缓存，`23E8`调用地图显示。`4C:6AB8..6BF8`按地图图块、静止主角、20个NPC槽、4个固定怪物槽的顺序绘制，没有Y排序或相邻NPC判断；末尾恢复上下四行并提交。此入口接入出口显示，其它刷新场景尚不能统一认定使用同一路径。

`48:23EC..2400`用原地图名称指针`52:78E4`调用`03:4050`，持续时间`0x1388=5000ms`。`3E:FD80`保存背景区域，按GBK字节数乘8计算宽度；外框X为`120-4*字节数-5 ..120+4*字节数+5`，Y48..72；文字从左边缘+5、Y52开始，用原16像素字库。`4E:F282..F310`等待计时事件、消耗普通输入，结束后恢复保存区域并提交。C取当前NPE地图名称，无游戏文字或编号例外。

SDL真实方向键入口先提交新地图，再提交名称框，等待名义5000ms后恢复地图并缓存，后续静态绘制复用这张LCD。定向测试在4999ms发送确认和F4，仍不能结束；到5000ms恢复。等待超过四秒的周期服务被作为模态期间到期处理，返回后重新计时，不补执行NPC更新。关闭窗口在提示期间中断后续恢复。初始新游戏路径绕过该装载器，不增加名称提示。

七次C／直接SDL／真实主程序整屏均零差异；直接SDL与主程序合计14次。旧42次地图行走回归通过，合计49次已验证的行走／出口参考提交、98次SDL比较；关闭路径的六次及主循环额外静态帧单独检查，不增加计数。前六张与旧捕获完全一致，提交4／6相同，原NPE与PRG哈希未变。存档反馈、内力不足提示回归通过，最终全量构建无警告，ctest 5/5。不重跑已取消的两项全程验收。

当前证据只证明具名出口的一次完整路径及上述输入／显示服务。其它出口类型、进度门控与跨模式装载、提交瞬间临时模式及特殊计时事件、真实ISR相位、其它地图重叠层级仍需核对；正常上下／非中心／边界滚屏、受阻后的遭遇／追击、脚本与物件缓存等原待办继续保留。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_walk_display.mjs --exit-full
python npe-sdl/tests/generate_walk_fixture.py --exit-full
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-walk-exit-probe.exe tmp/tjfy-world-probe.npe
```

本轮原厂文件`tmp/native-walk-exit.json`、`native-walk-exit-0-before/commit-*/after.idx`，夹具[walk_exit_native.h](../../npe-sdl/tests/walk_exit_native.h)；源指令`tmp/walk-exit-map-load-source.asm`、`walk-exit-map-draw-source.asm`、`walk-map-tiles-source.asm`及`save-feedback-original-source.asm`。最终回归`tmp/walk-exit-regression-{sdl-walk-exit-probe,sdl-walk-pose-probe,sdl-save-feedback-probe,sdl-negative-qi-probe}.log`，构建`walk-exit-final-build.log`、测试`walk-exit-ctest.log`，哈希／帧一致性`walk-exit-reference-report.json`。


## 普通移动的视口更新

`3B:1682..18B6`在四帧动作后修改现有屏幕位置与偏移。C原先每步复用`npe_map_spawn_viewport`，这是入图居中服务；对非中心角色和底部位置会算出不同状态，甚至让单次行走跳动多个视口格。现按普通行走服务逐格更新，只修改当前方向轴。

| 方向 | 原厂更新规则 |
| :--- | :--- |
| 下 | 屏幕Y<4时增加屏幕Y；否则偏移Y+7<地图高时增加偏移；到边缘后只在屏幕Y+1<7时增加屏幕Y |
| 右 | 屏幕X<7时增加屏幕X；否则偏移X+15<地图宽时增加偏移；到边缘后只在屏幕X+1<15时增加屏幕X |
| 上 | 屏幕Y>4时减少屏幕Y；否则偏移Y>0时减少偏移；到边缘后只在屏幕Y-1>0时减少屏幕Y |
| 左 | 屏幕X>7时减少屏幕X；否则偏移X>0时减少偏移；到边缘后只在屏幕X>0时减少屏幕X |

顶部行1向上仍保持行1，即使目标图块允许通过，也播放四帧并再次提交静止主角。原厂行2向上能到行1。这一非对称规则保留，未自行改成到行0。

碰撞起点也按`3A:001C..0044`取偏移加屏幕位置；不使用C中独立保留的入图目标坐标作为起点。成功行走后C逻辑坐标与当前视口一致，包括原厂屏幕边缘保持的情况。其它交互入口和地图装载后的坐标同步尚需继续核对，不能据此宣称全部坐标服务完成。

参考入口[reference_walk_viewport.mjs](../../npe-sdl/tests/reference_walk_viewport.mjs)分普通视口矩阵13组与边缘矩阵6组，直接执行`certified_original_v1`的原PRG。每组恢复原首图、释放按键，重新唯一定位RAM；诊断关闭NPC移动、声音和随机遭遇，注入视口以及从当前NPE读取的105个缓存词。注入前逐一核对原厂缓存与NPE，参考图始终来自实际LCD；字段注入不重建LCD，所以输入可能保留原位置画面，验收范围是原服务对这张保留LCD的操作，不是注入视口的全新初图。

最初一个底部候选触发原出口82，已从普通移动矩阵换为底部地形受阻位置，未把该未完成换图计为通过。19组当前样本覆盖正常上下、上下滚屏、四方向非中心位置、左右最后一次滚动及边缘屏幕移动、底部屏幕移动／受阻、顶行保持。94次原厂提交通过C、直接SDL及实际主程序逐张比较，SDL合计188次；19组地图／进度／模式／朝向／视口／逻辑位置及位置／视口存读档检查通过。原入图居中算法与其中九组原厂行走结果不同。主循环额外静态帧单独检查，不增加参考提交计数。

最终全量构建无警告，七项局部回归退出0，ctest 5/5；包含旧42次行走、完整出口七次、追击战／冷却、庙中九步触发、开门十六步通行、18组运行中存读档与保留音乐。原NPE与PRG哈希、NPESAV15保持。没有重跑取消的正常属性完整通关或SDL全程实玩。

未完成范围仍包含非零固定怪物、多槽遮挡、物件／触发／出口组合、其它出口与进度门控、受阻／触发后的遭遇和追击、脚本／物件缓存、其它重绘层级、入图后的其它交互坐标、小地图／坐标255堆边界、真实计时相位与跨NPE机制。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_walk_viewport.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_walk_viewport.mjs --edges
python npe-sdl/tests/generate_walk_fixture.py --viewport
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-walk-viewport-probe.exe tmp/tjfy-world-probe.npe
```

证据：`tmp/native-walk-viewport.json`／`native-walk-edge.json`、对应before／commit／after帧，夹具[walk_viewport_native.h](../../npe-sdl/tests/walk_viewport_native.h)，最终日志`tmp/walk-viewport-regression-*.log`、`walk-viewport-all-build.log`、`walk-viewport-ctest.log`；计数、旧算法比较及原文件哈希`tmp/walk-viewport-counts.json`。


## 多NPC／固定怪物与不同服务的门控

本轮核对固定怪物相关源指令，撤销把同一个综合active条件套在碰撞和行走显示上的行为。各入口检查的字段不同：

| 服务 | 原厂条件 |
| :--- | :--- |
| 碰撞`3A:045C..05EE` | NPC编号非零、展平位置匹配、剧情范围有效；0／0范围始终有效。不要求敌人编号／图组非零 |
| 行走槽循环`3B:1586`／`1A2C` | NPC字`7620`非零时调用绘图并检查上下相邻；相邻关系忽略剧情隐藏和图组 |
| 相邻`4B:6B1E..6C8E` | NPC编号非零且位于主角正上／下方；即使绘图不可见也能决定停止槽循环 |
| 绘图`47:F862..FAA0`与`4B:B2C4..B430` | X偏移到偏移+14，Y偏移+1到偏移+6，剧情范围有效，图组按有符号word必须>0；没有NPC的边缘清除尾部 |
| 地图装载`4C:6B4E..6BB4` | 敌人字节`7625`非零时调用同一绘图服务；与行走NPC字门控分别处理 |

`npe_world_step_with_display`按碰撞入口的NPC编号和剧情条件检查固定怪物。行走受阻与移动后的显示均使用槽循环自己的NPC门控，然后调用独立绘图条件。地图装载继续使用敌人字节；其它怪物交互和刷新服务尚未统一认定与这些入口相同。

原厂样本确认：敌人编号0、图组0而NPC编号非零的形成记录仍阻挡目标位置；NPC编号0的记录不阻挡且不参与行走槽扫描。敌人编号0、NPC编号非零、正图组的下方怪物仍会绘制。隐藏怪物不绘制，却仍可能决定相邻静止提交。固定怪物最下方的视口行7不绘制，行6可绘制；不通过NPC边缘清除来遮掉怪物多出的部分。

多NPC样本复核了原循环：成功行走时首个上下相邻NPC决定一次中间提交并停止NPC扫描，之后固定怪物循环继续；首个相邻怪物决定停止怪物扫描，最后提交。受阻时NPC先扫描，遇到首个上下相邻者立即提交静止帧并返回，因此后续怪物可能完全不绘制。没有相邻者时仍四帧动作。这些顺序没有按屏幕Y重新排序。

参考入口[reference_walk_actors.mjs](../../npe-sdl/tests/reference_walk_actors.mjs)恢复同一原首图并释放按键，重新定位RAM，以原模式／地图／进度／视口、主角及原NPC0图组和位置排除陈旧内存副本。诊断关闭音乐／随机遭遇与NPC移动，注入20个NPC和四个固定怪物的运行中记录；保留并捕获实际LCD作为输入，参考图不由C合成。最初一次RAM查找因旧槽数据命中多个候选而拒绝继续，随后加强唯一定位并重新捕获完整24组。

24组共106次原厂提交，通过C整屏、直接SDL106次及真实主程序106次逐次比较；地图、剧情、模式、朝向和视口也与原厂一致。主程序额外静态帧单独核对，不增加计数。修复前探针在zero-enemy样本的状态比较退出非零；最终完整矩阵退出0，没有排除该样本。覆盖NPC／怪物上下相邻、两种槽顺序与同格覆盖、隐藏角色、三个零字段、怪物首末可见行和受阻静止单次提交。

最终全量构建无警告，八项局部回归退出0，ctest 5/5；旧行走42次、视口94次和出口七次、NPC保留绘图、追击战／冷却、庙中触发和开门通行均通过。原NPE／PRG哈希及NPESAV15不变，不重跑取消的正常属性完整通关或SDL全程实玩。剩余怪物其它刷新／交互／战斗服务、其它出口与进度门控、受阻／触发后的遭遇与追击、物件组合、脚本缓存和跨NPE等待办继续保留。

定时追击的旧SDL探针曾一次断言失败，增加诊断的单次重跑通过。检查到旧就绪标记可能让退出事件在最后行走动画期间发送，探针现等待最终行走完成的原子信号再退出，原位置／冷却／模式断言保持。三次重复及最终回归全部退出0，记录为`tmp/walk-actors-chase-repeat-{1,2,3}.log`及`walk-actors-chase-diagnostic.log`；这项同步只修改探针，生产退出仍能中断行走。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_walk_actors.mjs
python npe-sdl/tests/generate_walk_fixture.py --actors
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-walk-actor-probe.exe tmp/tjfy-world-probe.npe
```

证据：`tmp/native-walk-actors.json`及before／commit／after帧，夹具[walk_actor_native.h](../../npe-sdl/tests/walk_actor_native.h)；原源指令`tmp/walk-monster-visibility-source.asm`、`walk-monster-adjacency-source.asm`、`walk-monster-draw-source.asm`、`walk-step-original-source.asm`、`walk-collision-original-source.asm`、`walk-exit-map-draw-source.asm`。失败日志`tmp/walk-actors-before-fix.log`，最终`walk-actors-regression-*.log`、`walk-actors-final-build.log`、`walk-actors-ctest.log`；计数／哈希`walk-actors-counts.json`。

## 受阻后的遭遇和追击（2026-10-07）

原控制器37:1762调用行走，37:176E比较7A86与3；只有出口进入独立分支。受阻kind0和普通行走kind1都进入37:17FE..18CE。7A5C非零、7AC4为零时检查遭遇；没有触发随机战才调用追击。C原先受阻立即返回，导致撞墙不增加计数、不消耗随机数，也不更新追击冷却／位置，现与普通移动共用后续调用。出口仍保留原独立返回。

37:1826用SGE比较计数与1000，因此计数为65535时按-1处理，递增后回绕为零，消耗一次LCG随机数而不进入战斗。C撤销无符号阈值比较。后续余数与计数仍按无符号比较，未改成另一套遭遇概率。

3A:5820..5F56追击服务写保留LCD，不调用提交。原厂目标短路径的最终前台画面均等于最后一张行走提交，追击移动发生在其后。主程序将保留图与已提交图分开：行走返回的保留图继续缓存，但不会在事件循环顶部提前显示；下一次方向键／周期从保留图继续绘制，其它实际提交会清除延迟标记。

参考入口[reference_walk_continuation.mjs](../../npe-sdl/tests/reference_walk_continuation.mjs)使用同一原快照和原PRG，先通过真实四秒NPC周期的前后内存变化唯一定位LCG状态，再逐例设种子11。诊断注入NPC／追击运行记录、随机遭遇开关和计数。首次105词缓存与当前NPE核对后，逐例恢复相同NPE缓存；使用一次真实、无追击的非滚屏行走同步原LCD保留图与前台，再捕获目标方向键的实际输入／提交。准备动作不计目标47次提交，参考画面不由C生成，原文件不改写。

早期采样在重设视口时未同步滚屏后的缓存，且周期结束不保证保留LCD已提交，曾造成129像素假差异；这些采样已排除并重新捕获完整十二组。保留的`walk-continuation-display-before-fix.log`和早期`check.log`不能作为生产显示缺陷证据。修复前的数值探针在首组追击位置断言失败，不依赖画面输入。

最终十二组47次原厂提交均通过C整屏、直接SDL及真实主程序方向键入口，各自47次；SDL合计94次。模式、位置、朝向、视口、两个追击槽的冷却／位置、遭遇计数与共享RNG一致。相邻NPC样本一次静止提交，其余受阻四次，普通移动五次。主程序探针额外提交直接失败，避免提前显示追击图被同帧计数掩盖。

自动回归批次末项曾误填不存在的sdl-tile-flow-probe，PowerShell沿用了前一条退出码；该项排除，随后以正确的sdl-tile-change-flow-probe单独执行并确认退出0。计数报告只列实际完成的11个日志，不将命令错误作为通过证据。

全量构建无警告，11项局部探针退出0，ctest 5/5。原NPE／PRG哈希及NPESAV15不变，不重跑全程。此矩阵限定为非接触、未触发随机战且7AC4为零；随机／接触入战完整后续、全零体力、剧情触发后的模式覆盖、出口进度拒绝的保留图以及脚本／对象缓存仍需独立核对。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_walk_continuation.mjs
python npe-sdl/tests/generate_walk_fixture.py --continuation
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-walk-continuation-probe.exe tmp/tjfy-world-probe.npe
```

记录：`tmp/native-walk-continuation.json`及before／commit／after实际帧，夹具[walk_continuation_native.h](../../npe-sdl/tests/walk_continuation_native.h)，`tmp/walk-continuation-counts.json`；日志`walk-continuation-before-fix.log`、`walk-continuation-native.log`、`walk-continuation-all-build.log`、`walk-continuation-ctest.log`、`walk-continuation-regression-*.log`。

## 随机、追击及剧情入战（2026-10-07）

原版入战读取地图中的体力字，不因全员倒地而拒绝入战，也不把零体力补成1。普通遭遇仍先增加计数／抽取随机数，再决定是否入战；随机战优先于同一步的追击接触。原49:8DF0..8E10将当前队员设置为首条记录，首位倒地时不跳到下一位存活队员。C随机、追击和剧情三个入口现保持这一行为；原剧情入口自动补血的推测注释及逻辑已撤销。

普通与剧情入战的显示顺序不同，分别来自原4E:7F8C..7FB0和4E:8BDC..8BFE：

| 入战服务 | 四次提交 |
| :--- | :--- |
| 随机／追击 | 场景与角色 → 当前队员光标 → 操作图标 → 体／气 |
| 剧情 | 场景与角色 → 当前队员光标 → 体／气 → 操作图标 |

新增通用C入口`npe_render_battle_entry_frame`，SDL主程序首次显示模式3时提交四个阶段，之后继续既有菜单显示。使用当前NPE的角色、敌人、图标与图像；体／气使用原GBK文字，没有游戏编号分支。阶段间未添加人为延时，原机器tick相位尚未验收。

原剧情入战的中间敌槽有睡眠免疫10；普通入战不执行这一赋值。C原先随机／追击复用了剧情免疫初始化，三敌编组的中间槽因此与原版不同，已改为仅剧情入口执行。普通两敌、三敌抽取的敌人及LCG尾状态也逐个核对。

### 普通入战证据

[reference_walk_battle_entry.mjs](../../npe-sdl/tests/reference_walk_battle_entry.mjs)恢复原首图快照，唯一定位原RAM及共享LCG，核对105词地图缓存。通过真实非滚屏行走同步保留LCD和前台后，注入诊断体力、遭遇计数／开关、NPC和追击记录；目标输入为受阻方向键，捕获全部实际显示提交。原NPE／PRG不修改，参考画面不由C生成。

11组覆盖零体力未入战、零／正体力强制遭遇、阈值前一次随机、随机优先于追击、零体力追击接触、先抽取遭遇再追击、两人／三人首位倒地及两敌／三敌编组。未入战组四次提交，其余每组四次行走加四次入战，合计84次。C、直接SDL与真实主程序每次提交均零差异（SDL共168次）；额外主程序提交会使探针失败。模式、体力、计数、RNG、追击位置／冷却／槽、敌人编组与活动战斗记录的属性／状态通过。

### 剧情入战证据

[reference_scripted_battle_entry.mjs](../../npe-sdl/tests/reference_scripted_battle_entry.mjs)恢复原洞中战斗快照，先执行一次真实模式1分派，再设置模式3及原地图战斗事件，以区别只在模式3触发的重绘。注入体力和待清理战斗状态，不改敌人或地图资源。五组覆盖体力130、0、65535（有符号-1）、两人首位倒地、三人混合倒地；20次实际提交均与C、直接SDL和主程序零差异（SDL40次）。首位光标、体／气、角色状态清除、中间敌槽免疫与活动记录属性通过。

最初剧情显示探针在第三次提交有936像素差异，追踪原剧情函数确认HUD先于图标；修正后正常体力四帧通过，零体力数值断言明确记录C为1、原版为0，见`tmp/scripted-entry-before-fix.log`。删除补血后五组完整退出0。参考程序第一次多例恢复时拒绝了两个RAM候选；现只在每例结束后使旧诊断RAM签名失效，下一次恢复仍要求唯一候选。该失败采样不列为通过。

空敌槽在原RAM中可能保留上次记录的属性／状态；本轮捕获这些字段，但只核对有敌人编号的槽，不宣称空槽保留机制完成。零体力入战后的操作／败战、模式覆盖、动作抑制字节、其它剧情编组与离场副作用、脚本／对象缓存、跨NPE及时基／启动RNG继续保留待办。现有诊断剧情已到结局，不重复用户取消的正常属性完整通关和SDL全程实玩。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_walk_battle_entry.mjs
python npe-sdl/tests/generate_walk_battle_entry_fixture.py
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_scripted_battle_entry.mjs
python npe-sdl/tests/generate_scripted_battle_entry_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-walk-battle-entry-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-scripted-battle-entry-probe.exe tmp/tjfy-world-probe.npe
```

证据：`tmp/native-walk-battle-entry.json`、`native-scripted-entry.json`及各自实际提交帧，夹具`tests/walk_battle_entry_native.h`、`scripted_battle_entry_native.h`；`tmp/walk-battle-entry-check.log`、`scripted-entry-check.log`、`scripted-entry-before-fix.log`。原函数在`tmp/walk-battle-random-entry-source.asm`、`battle-entry-refresh-dispatch.asm`及`mode-analysis/scripted_battle_source_v655_reachable.asm`。

全量构建及最终生产目标构建无警告，ctest 5/5；受阻续接、追击、庙中触发、开门通行、战斗生命周期、敌人行动、声音调度、逃跑动画、败战动画九项局部回归均退出0。敌人行动旧探针将普通／追击中间槽免疫断言为10，现按独立原版采样修正为0，剧情的10断言保留；初次失败见`tmp/battle-entry-enemy-action-old-expectation.log`。败战探针缺少已有开场等待输入助手，辅助线程五秒超时后主程序仍等待输入；确认进程仍在运行后结束该次探针，补入同一助手并重跑通过全部26次原版动画提交。停等日志`tmp/battle-entry-defeat-startup-wait.log`不作为通过证据，生产开场未改变。

最终计数／哈希及九项实际退出码见`tmp/battle-entry-counts.json`和`battle-entry-regressions.json`；构建日志`battle-entry-all-build.log`、`battle-entry-production-build.log`，`battle-entry-ctest.log`及`battle-entry-regression-*.log`。16组共104次C和208次SDL比较通过，原NPE／PRG哈希与NPESAV15不变。

## 2026-10-07：地图触发续接与主菜单背景

### 触发并不终止方向键的后续服务

原碰撞3A:0698..0748设置模式、事件号、重绘标记，并将移动种类7A86设为0。37:176E仅对出口kind3单独分支，触发仍经过37:17FE..18CE的随机遭遇与追击。因此随机战先于追击，随机战与追击接触都能覆盖触发的模式和事件；非接触追击仍移动，首个冷却仍递减。C此前在触发后立即分派并返回、追击又拒绝非地图模式，现先完成原控制器续接再分派。周期调用的菜单门控保持在周期入口。

原碰撞不检查事件号或模式是否合法。事件号0的模式6触发有效；模式0／255也先触发并执行追击，再由4A:F48E..F4A0/F5EE..F602返回地图模式，保留事件号并重绘。解析器撤销相应提前过滤，规范化位于续接之后，不阻挡随机／接触战覆盖。这里核对的是合法目录中的模式字节，不代表超过原十槽容量的目录或其它损坏资源已适配。

### 模式分派后的底图

模式1重绘分支37:1324..136A调用4C:6AB8：地图、静止主角、NPC槽、固定怪物槽、上下边缘恢复。直接提交追击后的保留LCD与原厂第五次提交有1145像素差异，改用该地图服务后为零。

本轮模式6的原厂第五次提交，在菜单面板之外与最后一次行走提交完全一致；追击数值已更新，但新追击图像未出现在这个底图中。C原先复用追击后的保留LCD，菜单之外多出173像素，现保存最后行走提交作为菜单入口底图。随后方向键只改变菜单选框，继续保存入口底图，避免`draw_pixels`清除缓存后下一次切换重建整图。最初追加菜单切换检查时正是这个缓存失效导致1045像素差异；修复后八次上下及循环均为零。这里只证明具名触发和菜单切换范围，其它模式进出／脚本／领取对象后的LCD缓存仍需追踪。

### 当前NPE资源与原选框、字库

原主菜单49:0000..020E取当前资源基数后的金钱框与菜单图：RPG2.1文件末尾第74／73张，分别87×29、52×82。位置由原服务参数确定为(0,7)、(0,37)，不从游戏标题或固定图片编号选择。原compact12字体的金额位置为(36,16)，ASCII间距6；原字体和当前NPE框替换旧5×7数字。金额0、100、65535、999999均核对实际原厂LCD。

四个反色矩形的X均为8..42，Y分别为45..62、62..78、78..93、93..110，边界包含端点。它们不是等距选框。49:0170..01FE及方向服务36:337A／47A2支持这些参数；实际上下序列核对了全部四项和两个方向的循环。菜单选择字节为7AE2，7A54表示菜单层级；采样脚本分别读取，避免将层级误当选择。

### 原厂短路径与逐次提交

[reference_walk_battle_entry.mjs](../../npe-sdl/tests/reference_walk_battle_entry.mjs)的`--triggers`入口恢复原首图快照，唯一定位RAM／共享LCG，核对当前NPE的105词地图缓存，并先执行一次真实行走同步LCD。随后只在隔离原RAM中设置触发、遭遇和追击诊断条件，不修改原NPE／PRG，捕获原运行时的全部实际提交；参考画面不由C生成。12组如下：

| 样本 | 原厂提交 | 最终结果 |
| :--- | ---: | :--- |
| menu-trigger-chase | 5 | 菜单；追击移动 |
| menu-trigger-roll-chase | 5 | 金额0；遭遇未中，再追击 |
| menu-trigger-random | 8 | 随机入战覆盖菜单触发 |
| random-overrides-trigger-contact | 8 | 随机优先，接触追击未执行 |
| contact-overrides-trigger | 8 | 追击接触入战覆盖触发 |
| map-trigger-roll-chase | 5 | 地图触发；遭遇未中，再追击，再重绘 |
| map-trigger-random | 8 | 随机入战覆盖地图触发 |
| zero-event-menu-trigger | 5 | 事件0有效；金额999999 |
| trigger-range-denied | 4 | 进度不符，不触发；仍执行受阻续接 |
| trigger-chase-cooldown | 5 | 冷却2→1；金额65535 |
| zero-mode-trigger | 5 | 模式0先触发、追击，分派后回地图 |
| invalid-mode-trigger | 5 | 模式255同上 |

12组共71次，加第一组菜单四次下／四次上共8次，合计79次原厂提交。C／直接SDL／真实主程序逐次整屏零差异，SDL共158次；实际主程序的任何额外提交都会使探针失败。模式、事件、地图／进度、逻辑与屏幕／视口坐标、追击位置／冷却、遭遇计数、共享LCG、活动战斗属性／状态及金额也匹配。每个提交附近的状态在`advance`后读取，不宣称是CPU提交瞬间的完整快照。

修复前追击位置比较明确失败，见`tmp/walk-trigger-before-fix.log`；追加零／超界模式后，旧解析器在零模式的事件号检查失败，见`walk-trigger-mode-before-fix.log`，没有将这两个失败样本排除。最终完整探针退出0。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_walk_battle_entry.mjs --triggers
python npe-sdl/tests/generate_walk_battle_entry_fixture.py --triggers
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-walk-trigger-continuation-probe.exe tmp/tjfy-world-probe.npe
ctest --test-dir npe-sdl/build --output-on-failure
```

全量构建无警告，ctest 5/5；12项实际局部回归退出0：受阻续接、普通入战、剧情入战、庙中触发、开门通行、追击合成、边缘恢复、音乐菜单、存档失败反馈、三槽流程、战斗生命周期及声音调度。不重跑用户取消的正常属性完整通关和SDL全程实玩。

计数、逐项实际退出码及原NPE／PRG哈希见`tmp/walk-trigger-counts.json`、`walk-trigger-regressions.json`；原始状态与帧为`native-walk-trigger-continuation.json`及相应`.idx`，夹具为`tests/walk_trigger_continuation_native.h`，最终比较为`walk-trigger-check.log`。源码证据为`trigger-menu-original-source.asm`与`trigger-redraw-original-source.asm`，均解码原`NP560.PRG`。生产仍是纯C11＋SDL2，不链接参考PRG／WASM，NPESAV15不变，无游戏编号例外。

### 出口选择状态与下一步

7AC4是出口选择状态：原36:83BE清零，36:896A在出口选择提示中置1，37:1690及其方向／确认分支读取；遭遇和周期也据此门控。它不是独立的通用动作抑制开关。当前矩阵该字节为0，完整提示／方向／确认／取消服务仍待实现，不以新增一个孤立字段宣称完成。原源码另见`tmp/dis-exit-36-839c/np560-source-36-839c-exit_reachable.asm`。

其它触发模式、进度拒绝的出口保留图、脚本／对象服务后的缓存、其它模式进出底图与交互坐标、零体力后续操作／败战、空敌槽记录、其它剧情编组、真实tick相位／启动RNG、声音及跨NPE机制继续保留待办。整体目标尚未完成。


## 出口选择与进度门控

本节更新前文7AC4待办。该字节是选择状态，7A84是出口槽，7A86是移动类型；确认后原版仍保留选择字节和槽。当前实现与以下十组真实原厂短路径对照，不代表所有出口服务已完成。

### 解析与数值服务

当前NPE每出口21字节：+0/+1位置、+2类型；+3主地图BE16、+5主门槛BE16、+7/+8出生点、+10模式、+11事件；+12副地图BE16、+14副门槛BE16、+16/+17出生点、+19模式、+20事件。原加载器29:C420..CA08转换为24字节驻留记录，两个事件各扩成word，槽位从6B82开始。C读取当前NPE，不写固定地图名称或游戏编号。

37:1778..17C6先以无符号word比较主门槛；不足时保留遭遇计数，调用29:BBF2的静止帧并返回。四次旧位置行走、静止帧和主控制器的补交共六次。允许出口清零遭遇计数，36:83BE清选择，类型非零且两门槛满足时36:896A置选择1并画提示；副門槛不足直接走主出口。当前矩阵中副拒绝路径共八次提交。

37:19EA..1C78确认只选择目的地图和出生点，未更新模式／事件或清零7AC4。C保留这个行为；追加的确认后上下／左键样本验证原版仍在选择分支，上下反转地图上的选区，横向无提交。

### 显示与输入

- 原提示先提交全零屏，再以原16像素字库绘制。驻留字符串4C:A9EA是“请选择支线：”，在(38,15)；主／副地图名称首字符来自当前NPE，在(70,45)/(70,68)。原服务请求的是一个字符，未自行扩成完整名称。
- 初始反色区域为包含端点的(70,45)..(166,61)；37:1978..19B4在每次上下输入都反转这块和(70,68)..(166,84)，即使连续重复同方向。选择状态分别写1/2，图像反转不根据新旧选择差值优化。
- 无目标的空格、左右和返回键无提交；300tick空等待保持地图、选择和共享RNG。可交互目标的空格优先服务与存读档交错尚未覆盖，不据此声称所有空格均被忽略。
- 确认提交所选地图，显示当前NPE地图名，名义等待5000ms，恢复地图，再补交一次相同地图，共四次。控制器补交不能省略。
- F4进入系统菜单并返回时，50:15A8..15D6恢复保留背景而不是重画地图。本轮原提示恢复帧与提示入口帧逐字节相同。随后地图服务4C:6BB8..6BE4通过03:4214恢复上下四行；菜单保留平面此时来自空白提示，边缘应为0，不是旧常量2。修复整图恢复15397像素和后续地图边缘1920像素差异。主程序保留对应边缘，不以地图编号分支补丁修正。

### 对照范围

| 路径 | 原厂提交数 |
| --- | ---: |
| 确认主支线 | 10 |
| 下键确认副支线 | 11 |
| 上下上后确认 | 13 |
| 连续重复下下上上 | 14 |
| 左右返回空格后确认 | 10 |
| 提示空等待300tick | 6 |
| 副门槛不足自动主出口 | 8 |
| 主门槛不足拒绝 | 6 |
| 确认后方向键 | 12 |
| F4菜单往返再选副支线 | 13 |
| 合计 | 103 |

十组103次C、直接SDL103次、实际main入口SDL103次全部零差异；每阶段同时比较模式、事件、地图、进度、朝向、视口、选择字节、槽、遭遇计数和RNG。真实主程序若产生额外提交会失败。完整构建无警告，ctest 5/5；12项局部回归退出0，逐项见`tmp/exit-choice-regressions.json`。

参考程序[reference_exit_choice.mjs](../../npe-sdl/tests/reference_exit_choice.mjs)恢复原地图快照，唯一定位RAM/RNG，在隔离RAM中关闭移动／遭遇／声音并固定RNG11，设置出口槽及门槛。天降风云录147张地图的当前出口类型全部为0，所以双支线是原通用引擎诊断；不能据此宣布所有其它NPE已适配。参考帧均由不可变原PRG实际提交，未以C生成参考图。公开mask16是空格；确认样本在tick3向诊断RAM的7A76注入一次原Enter键0x86，随后释放，没有给前端非法mask128。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_exit_choice.mjs
python npe-sdl/tests/generate_exit_choice_fixture.py
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-exit-choice-probe.exe tmp/tjfy-world-probe.npe
ctest --test-dir npe-sdl/build --output-on-failure
```

原厂状态／图像为`tmp/native-exit-choice.json`及`native-exit-choice-*-*.idx`；夹具`tests/exit_choice_native.h`，实际日志`exit-choice-native-final.log`、`exit-choice-check.log`、`exit-choice-all-build.log`、`exit-choice-ctest.log`。计数及原NPE／PRG SHA256见`exit-choice-counts.json`；源码解码见`exit-choice-original-source.asm`、`exit-menu-close-original.asm`和`dis-exit-36-839c/np560-source-36-839c-exit_reachable.asm`。生产仍是纯C11＋SDL2，未链接PRG／WASM，NPESAV15不变。

前文七次普通出口样本在恢复地图后停止，其主程序探针在补交前退出；本轮八次自动主出口及四次确认加载证明完整控制器还有一次相同地图提交。历史七次局部检查继续成立，但不能把它理解为全部控制器提交已覆盖。

剩余：正选择状态与存读档／可交互目标的空格／对话交错，其它类型与模式、非法目的地、小地图／坐标边界；完整LCD保存／恢复平面、其它模式进出及脚本／对象缓存；真实tick相位／启动RNG、声音和跨NPE机制。沿用已有剧情诊断到结局的结果，不重跑已取消的正常属性完整通关及SDL全程实玩。整体目标尚未完成。

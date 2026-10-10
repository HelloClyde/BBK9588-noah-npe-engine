# 追击与 NPC 周期的保留 LCD 合成

日期：2026-10-07。原 128 像素差异已修复；本项 22 组追击与 16 组边缘周期全部匹配，不外推为整个引擎已经完成。

## 原厂顺序与 C 实现

`3C:D5E4..D71A` 依次执行追击、NPC 移动／擦除、主角与全部 NPC 的全局顺序重绘，再提交 LCD。追击切入战斗也不跳过 NPC 服务和这次地图提交；战斗画面是后续的独立绘制。

追击 `3A:5820..5F56` 按槽逐个处理：

1. 遇敌人号零，返回整个服务；剧情范围不符则跳到下一槽。
2. 冷却非零时减一并返回整个服务；即使本次从 1 减为 0，也不立即绘图或处理后槽。
3. 按原厂接触条件判断战斗；接触时不擦除这个追击对象，也不继续后槽。
4. 无接触时先调用 `3F:4C58..515C`，按左下、左上、右下、右上的顺序画四个缓存图块，mode0 的零色也覆盖。
5. 两轴按原厂规则各靠近一格；无需地图碰撞检查。
6. 新位置满足 `0<screenX<14 && 0<screenY<6` 才按当前 NPE `vectors[2]+graphic` 透明绘图，然后处理下一槽。

新增 `npe_render_world_periodic` 按以上顺序合成，再调用 NPC 周期合成。主程序定时器已替换追击坐标／冷却改变时的整图刷新分支；进入战斗时先提交这次保留地图，再绘战斗。移动逻辑与绘图共用原厂接触判断函数，未改变接触公式。

### 视口缓存末尾的真实别名

105 个图块从 `52:70B4` 起占 210 字节，紧接 `52:7186` 的 200 个访问地图字节。下边缘擦除可以读入这些字节，将每两个字节按 BE16 当作图片号，而不是第八行地图。C 从已有 `condition5_bytes` 字段读取该别名，允许其指向非 16×16 图片；无效图片号沿原厂忽略绘图服务错误的行为处理。mode0 在完整 120 行 LCD 内裁剪，可以覆盖底部四行。

原厂采样覆盖零字节、低字节为 1 和高字节为 1 三组，对应图片 0、1、256；没有按游戏／地图／追击编号添加例外。本轮不扩展到该 200 字节之后的其它 RAM 对象。

## 原厂参考与验证范围

`reference_chase_compositor.mjs` 使用原 PRG、真实首图快照与同一 live 实例，动态定位共享 RNG。每组修改诊断追击、NPC、主角与访问地图字段后，采集**实际保留 LCD**，再等待一次真实周期事件，保存所有提交。字段注入不刷新画面，原始 NPE／PRG 不写回。

22 组包括四向靠近、左右／上下边缘、双追击重叠及顺序反转、首／次槽冷却、最后冷却计数、剧情范围外首槽、空首槽、固定／移动／隐藏 NPC 与追击擦除交错，以及首槽接触战斗。所有组的两槽追击坐标／冷却、20 槽 NPC 坐标／方向、共享 RNG 尾状态和地图／战斗模式均匹配 C。

**22/22 组 C 整屏零差异。** 全部 22 组经过直接 SDL 输出和主程序新游戏、注入原厂输入、四秒计时与绘制分支，共 44 次 SDL 比较零差异。接触组额外验证主程序在战斗绘制前提交的保留地图，未将它计为完整战斗入口画面已核对。

### 已修复的 128 像素差异

`npc-over-new-chase` 初次采样中，原厂将之前擦除的 `(48..79,116..119)` 的 128 像素恢复为颜色 2，C 保留了原像素。补全 NPC 绘制服务反汇编后，发现此前止于分支目标 `4B:268E`，没有读到它之后的公共尾部：

1. `4B:268E..26A2` 调用 `03:4214`，清除包含边界矩形 `(0,0)..(240,3)`。
2. `4B:26A4..26BE` 清除 `(0,116)..(240,120)`。
3. `4B:26C0..26CE` 才恢复寄存器并返回。

矩形服务在 240×120 LCD 内裁剪并恢复颜色 2。可见性失败也跳到 `268E`，所以隐藏／屏外 NPC 仍执行这两次清除。主循环只对非零 ID 调用它，零 ID 不执行。原厂诊断中，跳过 NPC 调用可去除该 128 像素恢复；只跳过 NPC 的位图调用可去除角色图像但保留边缘恢复。该内存指令隔离只作定位，不作为未修改原厂画面的验收夹具。

C 已将两次清除置于每个非零 NPC 的绘图尾部，保持最终主角绘图在它们之前／之后的原顺序。透明 mode2 改用完整 LCD 裁剪，上下四行不再作为所有演员的统一裁剪边界。没有依据故障区域或特定 ID 添加补丁。原失败组现已包含主程序验收；默认完整探针退出 0。

### 底部恢复的独立对照（2026-10-07）

新增 `reference_chase_compositor.mjs --margin-matrix`。每个目标前都先执行同一底边追击擦除，重新制造实际脏画面，再注入下一组字段并等待一次周期，避免前一组恢复后掩盖后续结果。共 16 次周期、8 对输入／输出，各目标只有一次提交。报告检查每组输入确实等于前一次实际输出，并记录主角逻辑／屏幕坐标、滚动偏移及朝向。

| NPC 诊断条件 | 底部变化像素 | 原厂输出 |
| --- | ---: | --- |
| 全部 ID 为零 | 0 | 保留擦除的零色 |
| 非相邻且可见 | 128 | 恢复为颜色 2 |
| 主角上方／下方／同格 | 各 128 | 恢复为颜色 2 |
| 剧情范围外的上方 NPC | 128 | 恢复为颜色 2 |
| 屏外 NPC | 128 | 恢复为颜色 2 |
| 更换图组的上方 NPC | 128 | 恢复为颜色 2 |

所有恢复范围均为 `(48..79,116..119)`，模式仍为地图 1、RNG 尾状态仍为 11。该对照表明，不能将现象仅归因于相邻关系、可见 NPC 图像或某个图组。完整反汇编的 `4B:79FA..7B74` 相邻判断未见直接 LCD 绘制调用；这不证明其它服务没有副作用。

上述初次调查已由完整 NPC 尾部源码解释。新增 `sdl-chase-margin-probe` 复用实际主程序探针，比较全部 16 次周期的状态、C 整屏、直接 SDL 和主程序显示：C 16 张及 SDL 32 次均零差异，退出 0。与普通 22 组分开采集，共 38 个完整输入／输出阶段、76 次 SDL 比较。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_chase_compositor.mjs --margin-matrix
python npe-sdl/tests/report_chase_margins.py
python npe-sdl/tests/generate_chase_compositor_fixture.py --margin-matrix
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-chase-margin-probe.exe tmp/tjfy-world-probe.npe
```

证据：`tmp/native-chase-margin.json`、16 组 `native-chase-margin-*-before/commit-0/after.idx`、`tmp/chase-margin-report.json`、`tmp/chase-margin-matrix.log`，以及 `tmp/npc-adjacency-full-source.asm`。普通 22 组采集入口及输出文件名保持独立。

## 剩余范围

- 主角行走后追击服务的完整输入／提交顺序；本次主程序接入范围为四秒周期。
- 追击战后完整入口、成功逃跑的追击副作用和零体力边界。
- 脚本／领取对象后的视口缓存、初图／行走／菜单重绘、坐标255、真实时基／启动RNG及其它NPE机制。

不重跑用户取消的正常属性通关和 SDL 全程实玩。

## 复现与证据

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_chase_compositor.mjs
python npe-sdl/tests/generate_chase_compositor_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-chase-compositor-probe.exe tmp/tjfy-world-probe.npe
```

参考 `tmp/native-chase-compositor.json`，22 组 `native-chase-compositor-*-before/commit-0/after.idx`，日志 `tmp/chase-compositor-{native,check}.log`；夹具 [chase_compositor_native.h](../../npe-sdl/tests/chase_compositor_native.h)。反汇编 `tmp/chase-compositor-original-source.asm`、`chase-erase-original-source.asm`、`chase-packed-blit-source.asm`。

C 测试将地图记录复制到内存中的诊断段，并插入两个符合当前 NPE 格式的普通追击记录，其余资源保留原文件内容。主程序测试在新游戏后的测试入口注入该记录、原厂保留 LCD 和状态；这不代表原游戏首图天然带有这些追击对象。成品不依赖参考运行器或诊断段。NPESAV15 未改变。

本次全量构建无警告；旧NPC24组（C24张、SDL54次，含15条主程序的计时与无效键）、原始NPC移动与RNG、追击、六条存档／标题路径及两张首图、逃跑68次和ctest5/5通过。日志`tmp/npc-margin-{all-build,final-build,ctest}.log`及`tmp/npc-margin-sdl-*-probe.log`，两个新合成探针均退出0。完整 NPC 原厂源码现包含尾部至 `4B:26CE`，见 `tmp/new-game-display-original-source.asm` 和 `tmp/npc-draw-tail-full-source.asm`。

原文件SHA-256保持：NPE `DF4B52DF6B69E1105A5EA6100E43F35F832ED859C10CB8714879E44AD4D8CB40`，PRG `D555609E03ECE17F42C1881BEEB88361572F18621161DE67BC9DC765857089F5`。

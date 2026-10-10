# 新游戏朝向与 NPC 图组：原厂对照

日期：2026-10-07。此项验证新游戏首帧与标题空槽入口，不重复完整通关。

## 字段和绘制公式

主角初始朝向来自当前 NPE 角色记录的 **+78**。原厂 `2E:827E..82C8` 读取两个连续文件字节 +77/+78 至 `52:69D3/69D4`。此前 C 新游戏统一使用地图预览默认朝向 0；现在 `npe_world_new_game` 读取 +78。没有固定为《天降风云》的值 1。地图预览与读回的存档仍使用各自的朝向来源。

NPC 文件记录与运行时记录的步长不同。原厂 `29:C158..C402` 分字段读取；只列本项核对过的字段：

| NPE 文件偏移 | 内容 | 运行时记录偏移 |
| --- | --- | --- |
| +0～1 | ID，大端字 | +0～1 |
| +2、+3 | X、Y | +9、+10 |
| +4～5 | 图组号，大端字 | +2～3 |
| +6 | 朝向／图像偏移字节 | +8 |
| +7 | 自动移动标志 | +11 |
| +8～9、+10～11 | 进度下限、上限 | +4～5、+6～7 |

原厂 NPC 绘制 `4B:25A2..25CC` 使用：

```
图像编号 = 当前 NPE vectors[1] + (图组号 - 1) * 4 + 朝向字节 + 1
```

此前 C 错把 +4 当朝向，并把 +5 当零基图组。现读取完整图组字和 +6 朝向。原版直接相加朝向字节，未限制在 0～3；本文件有朝向 4 的静态人物，C 也允许此偏移，并保留资源范围和尺寸检查。脚本移动后的朝向覆盖仍优先使用。

## 独立原厂与 C／SDL 结果

参考运行器使用原 PRG，在真实冷启动准备资源后保存标题快照。通过“向下后选择字段由 1 变 2”动态定位活动 RAM；不使用图像数据中碰巧匹配的地图／模式字，也不固定宿主内存地址。

`reference_new_game_display.mjs` 恢复该标题快照，分别执行新游戏与读取第一空槽。每个入口分别将进入前朝向临时设为 0、3，仅用于验证重新构造的覆盖行为；不写剧情、地图、NPC、NPE 文件或生产存档。四组均进入地图 1、进度 1、朝向 1，且 NPC 运行时图组／方向与上述文件字段吻合。

- 四组各三次原厂首图提交共 12 次，均为同一张完整画面；C 逐张比较 **零像素差异**。另保留上一轮真实标题空槽首帧比较，也为零。
- 实际主程序“新游戏”及“标题空槽”两个入口各一次 SDL 整屏比较 **零像素差异**。SDL 在原分辨率读取真实渲染器输出。
- C 内存中的诊断副本分别替换 +78 为 0、1、2、3，均从该字段读取；原文件不写回。
- 六条实际存档／标题路径、三槽存档回归、场景的 281 次比较、逃跑的 68 次比较（含四次地图返回）通过。已有统计不与本项重复相加。
- 全量构建无警告，ctest **5/5** 通过；后续修改的两个 SDL 探针单独重建通过。

首次加入 SDL 地图读取的探针用了主程序默认放大窗口，读取整屏到原分辨率缓冲区导致探针越界；已将探针窗口设为 240×120 后复验。逃跑旧探针在开场播放完前超时；接入已有的逐帧确认输入后通过。两项均为验证程序的修正，生产窗口和开场不变。

## 范围与剩余工作

上一轮首图 553 像素差异已消除。原厂后续的 NPC 定时移动现已接入，[周期移动证据](np560_npe_npc_periodic.md)记录了15组状态／RNG对照及首周期C／SDL零差异。此前移动后536像素差异已消除；更多边界、缓存联动和时基相位继续核对，不从截图硬设NPC坐标。原厂损坏存档、声音精确时序及其它 NPE 完整机制仍需继续核对。NPESAV15 不变。

本项原文件 SHA-256：

- NPE：`DF4B52DF6B69E1105A5EA6100E43F35F832ED859C10CB8714879E44AD4D8CB40`
- PRG：`D555609E03ECE17F42C1881BEEB88361572F18621161DE67BC9DC765857089F5`

## 复现与证据

```powershell
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_empty_title.mjs
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_new_game_display.mjs
& npe-sdl/build/new-game-display-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-save-feedback-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-flee-animation-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-scene-animation-probe.exe tmp/tjfy-world-probe.npe
ctest --test-dir npe-sdl/build --output-on-failure
```

原厂记录 `tmp/native-new-game-display.json`、`tmp/native-new-game-*-commit-*.idx`；源指令 `tmp/new-game-display-original-source.asm` 和 `tmp/np560-source-29-bdbe-linear.asm`。本轮日志 `tmp/new-game-display-{all-build,check,sdl,ctest,flee-regression,scene-regression,slots-regression}.log`。原厂参考运行器仅为离线验证工具，C／SDL 成品不调用它。

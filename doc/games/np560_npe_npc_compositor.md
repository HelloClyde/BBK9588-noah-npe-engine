# NPC 周期擦除和保留 LCD 重绘

日期：2026-10-07。生产实现为 C11；参考运行器只用于采集原厂执行画面。

## 已实现的原厂规则

- `42:8506` 在每个移动标志为 1 的非零 NPC 尝试移动后，先调用 `44:E84C` 擦除旧位置；受阻时也擦除。该服务受可见范围和剧情范围门控。
- `48:0248..0482` 的可见范围为滚动 X 到 X+14、滚动 Y 到 Y+7，均包含边界；剧情范围 0/0 总是通过，其它范围按无符号包含比较。NPC 的周期移动本身不受这些可见性条件限制。
- 擦除从 15×7 视口图块缓存读取，先画 NPC 下半所在图块，再画上半所在图块；mode0 的零像素也覆盖底图。当前 C 对有效缓存位置从已绑定覆盖的地图读取相同图块。
- `4B:79FA` 只检查 ID 与坐标，不检查剧情范围。上下相邻的判断使用主角屏幕坐标加滚动偏移，而非另一个逻辑位置字段。
- `3C:D5F6..D71A` 按槽位找第一个上下相邻的 NPC。第一个在主角下面，或没有相邻 NPC，则先画主角再画全部 NPC；第一个在主角上面，则先画全部 NPC 再画主角。NPC 之间始终按槽位覆盖，不能按纵坐标排序。隐藏 NPC 也能决定该全局顺序。
- 玩家静止图使用 `46:BE02..BE72` 的中间帧公式，NPC 图组、朝向与图片仍读取当前 NPE。NPC 与主角均使用透明 mode2。
- 负起点不会裁入屏幕，mode2按完整240×120裁剪。后续补全`4B:268E..26CE`证实，每个非零NPC绘制调用尾部清除上下四行为2，即使剧情／视口门控失败；此前透明NPC样本的边缘保留来自这个尾部，而非通用裁剪。mode0擦除仍可覆盖底部四行，并读入视口后的访问地图字节。完整证据见[追击及边缘恢复](np560_npe_chase_compositor.md)。

新增 `npe_render_npc_periodic` 在调用者提供的保留 LCD 上执行擦除与重绘，不调用移动／RNG，不重新绘制整个地图，也不重画固定怪物。本项主程序最初仅在追击位置及冷却未变时调用它；后续[追击合成](np560_npe_chase_compositor.md)已接入完整追击槽处理、战斗前提交及NPC尾部清除，22组追击与16组边缘周期均匹配；同一状态下的无效按键保留这张地图。其它画面提交、菜单、状态改变和重新运行会使地图缓存失效。

## 原厂画面验证

`reference_npc_compositor.mjs` 从原厂真实首图快照启动，在同一 live 实例修改诊断 NPC／主角字段后，采集**当时实际保留的 LCD**，再等待真实周期事件并采集提交结果。字段注入不会重绘屏幕，因此每组输入画面明确保留前面的内容；C 使用该实际输入，不假设修改字段已经刷新地图。原始 NPE／PRG 不写回。

24 组完整画面覆盖：

- 单侧相邻、两侧相邻以及槽位顺序反转；主角置于中央的三组避免屏幕顶部隐藏相邻图像，验证全局分层。
- 剧情范围外的上方 NPC 仍决定主角顺序。
- 与主角同格、两个 NPC 重叠及槽位顺序反转。
- 左右边缘、顶部负起点、底部边缘。
- 相邻移动、受阻转向、NPC 重叠移动、左右移动和上下可见行移动。

24 组的 C 输出及实际 SDL 像素各一次整屏比较均零差异。另有 15 组无移动诊断经过实际主程序新游戏、四秒计时循环与绘制分支，注入原厂输入画面和字段后，在定时提交及无效 F12 输入后各比较一次 SDL，30 次均零差异。主程序正常 NPC 移动及共享 RNG 仍由既有 `sdl-npc-periodic-probe` 的原始首周期验证；不把本项移动画面夹具称为额外 RNG 对照。

最终全量构建无警告；ctest **5/5**、原始NPC周期／共享RNG、追击、新游戏、六条存档／标题路径、音乐菜单、逃跑68次（含四次地图返回）相关回归全部通过。日志 `tmp/npc-compositor-final-build.log` 和 `tmp/npc-compositor-regression-*.log`。

## 尚未完成的范围

- 追击坐标／冷却改变及战斗前提交已接入；原128像素变化已按NPC绘制尾部修复。固定怪物的全部保留效果和其它调用路径继续核对。
- 初始地图、主角行走、菜单恢复等仍使用既有完整地图渲染；本次周期证据不证明这些路径的所有分层也已复刻。
- 脚本／对象领取之后的视口缓存内容，尺寸小于视口的地图和邻接 RAM 内容仍待核对。C 不虚构有效 15×7 缓存以外的图块。
- 坐标 255 的覆盖表示、实际时基／启动 RNG、其它 NPE 的完整机制仍待完成。

不进行用户取消的正常属性完整通关或 SDL 全程实玩。

## 复现

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_npc_compositor.mjs
python npe-sdl/tests/generate_npc_compositor_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
& npe-sdl/build/sdl-npc-compositor-probe.exe tmp/tjfy-world-probe.npe
& npe-sdl/build/sdl-npc-periodic-probe.exe tmp/tjfy-world-probe.npe
```

证据：`tmp/native-npc-compositor.json`、24 对 `native-npc-compositor-*-before/after.idx`、`tmp/npc-compositor-native.log`、`tmp/npc-compositor-check.log`；源码反汇编 `tmp/npc-periodic-original-source.asm`、`npc-player-redraw-source.asm`、`npc-player-redraw-tail-source.asm`、`npc-bitmap-service-source.asm`；夹具 [npc_compositor_native.h](../../npe-sdl/tests/npc_compositor_native.h)。本项画面单列，不合并既有截图统计。

原文件 SHA-256 保持：NPE `DF4B52DF6B69E1105A5EA6100E43F35F832ED859C10CB8714879E44AD4D8CB40`，PRG `D555609E03ECE17F42C1881BEEB88361572F18621161DE67BC9DC765857089F5`。NPESAV15 未改变；生产代码无游戏、NPC、图组或坐标编号特例。

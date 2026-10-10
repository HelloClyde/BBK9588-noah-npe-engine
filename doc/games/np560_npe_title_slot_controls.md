# 标题读档：选槽、取消、空槽与失败返回

2026-10-10。生产程序继续使用独立 C11＋SDL2，标题和图像来自当前 NPE。

## 原版规则与修复

原 `NP560.PRG` 源地址 `3A:8A3A..9150` 为标题输入控制器。反汇编保存在 `tmp/title-slot-original-source.asm`；选槽初次绘制 `4C:5E20..5F62` 保存在 `tmp/title-slot-render-original-source.asm`。

- 选槽后发布一基槽号 `52:7A7A`。旧 C 只改变主机选择，第二槽仍留下共享槽号1。
- 上下移动在保留的 LCD 上依次反色旧行和新行，矩形包含上下边。相邻行共享边缘，重新生成整张菜单会改变保留像素。补入这段原操作，修正第三槽处8像素差异；未按游戏编号或参考截图逐像素补丁。
- F4取消先设标题阶段1，再调用标题初始化。初始化提交清屏、重置槽号1，并保留“读档”标题选择；下一次提交标题。旧 C 漏掉清屏及共享控制恢复。
- 空槽由当前公布的等级0判断，直接开始新游戏；不以主机文件是否存在替代原条件。检查包含本程序自己创建的无效第三槽文件，其等级仍为0。
- 占用槽读取失败的原分支 `3A:9076..90DE` 停止声音、设退出word1；C已接退出并保留槽号／标题阶段。最终宿主资源由现有销毁路径释放。这一失败分支依据原静态代码，未新增原固件文件故障的动态采样。
- 原标题／选槽控制器的I、M、K分别对应上移、下移、确认。补齐选槽别名及标题小键盘确认，空槽路径通过这些输入核对。

`initialize_title_controls()` 共用于初次标题、结束返回和选槽取消。菜单自身的保留面为主机 UI 状态，不改变自身存档格式。

## 验证范围

`reference_title_slot_controls.mjs` 使用未修改的原程序、当前NPE及先前真实保存产生的原持久数据，从原标题启动。没有注入剧情、NPC、库存或结果。普通按键记录两条路径：

| 路径 | 内容 | 原整屏画面 |
| --- | --- | ---: |
| cancel | 三槽上下循环、F4取消、标题选择、再次打开及取消 | 16 |
| empty | 打开读档、上移到第三空槽、确认开始新游戏 | 6 |

`sdl-title-slot-controls-probe` 包含实际 `main.c`；22张原整屏与C索引画面、实际SDL读回均零差异，槽号／选择／阶段、模式及初始化状态一致。原正常路径没有额外音频命令；空槽新游戏发出停止及地图音轨播放请求。

另在实际C标题已经显示占用等级后，删除探针自身的第一槽文件，再确认读取。实际文件读取失败后检查模式1、退出word1、一次停止请求和无选槽重绘。其前两张画面复用原open路径，所以合计24次对照，仍只有22张不同的原参考；这不是原机存档互通检查。所有自身测试文件均位于 `tmp/title-slot-controls/slots/`，结束时移除。

必要回归：结束返回24次、原标题占用槽首次读取3次、系统存读档返回33次、场景及开场281次，全部保持原预期通过。主程序和受影响目标构建通过，最终构建无警告。

最后新增的别名条件不改变这些回归的原按键路径，沿用其通过结果；新增别名由实际main的空槽路径覆盖。

保留失败证据：`before.log`显示修复前槽号1／原2；`first-fix.log`显示仅同步控制后的8像素差异。完整报告为 `tmp/title-slot-controls/report.json`。早期探针入口自身的字体路径、开场观察器和冷标题文件引用问题在取得生产失败证据前修正，不算生产问题。

## 复现

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_title_slot_controls.mjs
python npe-sdl/tests/generate_title_slot_controls_fixture.py
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-title-slot-controls-probe -j4
New-Item -ItemType Directory -Path tmp/title-slot-controls/slots -Force | Out-Null
& npe-sdl/build/sdl-title-slot-controls-probe.exe tmp/tjfy-world-probe.npe
```

原声解码、其它事件／状态组合和完整跨NPE动态兼容继续；本批不改变整体约80%的粗估，也不证明全部UI RAM或物理计时已复刻。

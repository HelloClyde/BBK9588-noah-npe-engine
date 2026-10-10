# 剧情文字页原版输入（2026-10-10）

## 生产修正

`npe-sdl/main.c` 的 `show_walk_story` 只接受空格翻页或结束，删除 Enter／小键盘 Enter 的额外入口。其它按键保持当前剧情、文字控制和画面，不重新读取文字或提交LCD。关闭窗口仍正常退出。

文字、标题和资源继续读取当前NPE。续页标志恰好为1才调用下一页读取，其它值结束并恢复原事件后继；本批未修改这套规则。

## 原依据

原分派 `43:6928..693C` 读取7A76键盘词，只接受0x20／0x90。`43:6940..6952` 检查7A1C是否恰好为1；`43:6956..698C` 恢复初始化标志与模式／事件后继。反汇编记录为 `tmp/story-input-dispatch-source.asm`，更宽范围为 `tmp/story-controller-original-source.asm`。

`reference_story_ignored_keys.mjs` 在未修改原程序、读取器和NPE资源的情况下，进入原故事4后提供明确诊断文字控制。0x86与k各覆盖more=0／1／2／255，共8条路径。直接通过原键盘词窗口送入无效键：模式、事件、文字指针、读取位置、续页标志、样式、矩形及画面保持，无LCD提交。同一窗口的8次原0x90空格均产生有效后继：more=1读取一页，其余值关闭故事。

这些是隔离输入诊断，不是正常剧情可达性、完整RAM、物理键盘扫描或时间精度证明。more=1的故事画面与已有故事4首屏重复；关闭后的原地图提交不计为故事页对照。原报告和新帧保存在 `tmp/story-ignored-original/`，既有参考文件未覆盖。

## 实际C／SDL验证

修复前，原11条实时文字路径／21次画面对照正常通过；插入Enter后错误读取并提交下一页，触发 `Unexpected story LCD on ignored key 1`。日志为 `tmp/story-input-before-valid.log` 和 `tmp/story-input-before-ignored.log`。

默认、Shift、Caps Lock三种输入均通过以下两组实际 `show_walk_story` 检查：

| 路径组 | 每种输入的画面对照 | 每种输入的无效键检查 |
| --- | ---: | ---: |
| 既有11条实时文字控制路径 | 21 | 378 |
| 新8条原more=0／1／2／255输入路径 | 10 | 180 |
| 三种输入合计 | 93 | 1674 |

无效输入包括Enter、小键盘Enter、K/Q/U/E/D/F、Tab、方向键、N/P、F4、Backspace和Esc。每次核对完整World、保留索引像素和资源读取计数，额外LCD提交立即失败；这不是完整资源堆或完整RAM比较。

受影响短回归：既有全屏故事23页、窗口故事54页，以及NPC／普通／全屏／窗口后继46次原C／SDL提交均通过。共216次实际C／SDL画面对照，含重复初始画面，不宣称216张新独立参考。

两个旧分页探针原先用Enter／Space／小键盘Enter轮流推进；现改为原Space输入，原页面及后继期待不变。生产程序和5个受影响SDL目标已重建，最终构建无警告／错误。首次多目标构建在自动刷新CMake后仍使用旧Make目标表，新增目标报无规则；重新调用已生成的目标后通过，首次日志保留。

## 重现与范围

```powershell
node npe-sdl/tests/reference_story_ignored_keys.mjs
python npe-sdl/tests/generate_story_ignored_fixture.py
cmake --build npe-sdl/build --target npe-sdl sdl-story-text-service-probe sdl-story-ignored-keys-probe sdl-story-pages-probe sdl-window-story-pages-probe sdl-dialogue-return-probe -j 4
$env:NPE_PROBE_STORY_IGNORED_KEYS='1'
foreach($target in @('sdl-story-text-service-probe','sdl-story-ignored-keys-probe')) {
  & ('npe-sdl/build/'+$target+'.exe') tmp/tjfy-world-probe.npe
}
Remove-Item Env:NPE_PROBE_STORY_IGNORED_KEYS
python npe-sdl/tests/verify_story_ignored_keys.py
```

Shift／Caps检查将 `NPE_PROBE_BATTLE_KEYS` 设为 `shift`／`caps`。审计脚本读取已有结果，不重跑游戏；报告为 `tmp/story-ignored-report.json`，生产差异为 `tmp/story-input-production.diff`。

准确原声、跨调用文字局部字节、其它特殊输入／动画、共享状态与完整跨NPE兼容仍未完成，整体粗估约80%。

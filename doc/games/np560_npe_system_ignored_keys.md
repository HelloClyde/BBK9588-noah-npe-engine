# 标题、系统、选槽与音乐的无效输入（2026-10-10）

## 生产修正

- 标题、系统根／子菜单、存读档选槽、音乐设置删除空格确认，使用原K／Enter。
- 标题无效键返回后跳过下一次重复画面提交；保留当前标题／选槽画面。进入初始化仍按原顺序清屏、绘制并消耗初始化标志。
- 标题根页F4忽略，选槽页F4仍取消并恢复标题。

生产修改限于 `npe-sdl/main.c`。内容继续读取当前NPE，主程序使用自身存档格式。

## 原依据与采样

原 `3A:8A3A..9150` 标题控制器的确认分支在8EAE..8EC6，仅接受0x86、K/k。8F18后的选槽确认也由同一分派进入。F4在9110..9144只取消阶段2。源记录为 `tmp/title-slot-original-source.asm`。

系统、选槽及音乐共用 `4B:B5A0..B70C`；确认分支B6CC..B6F0仅接受0x86、K/k。空格32／0x90和本批其它无效字母没有确认分支。

`reference_system_ignored_keys.mjs` 使用原解释器、NPE、真实原标题持久数据及已有原地图快照。九条路径为标题循环取消／空槽新游戏、系统根／子菜单、读档／存档选槽取消、音乐开／关／取消。

每个模式6／7的正常输入之前，通过7A76键盘词窗口送入空格32／0x90、Tab及Q/U/E/D/F大小写。共1027次输入，采样的模式、事件、初始化、标题槽／选择／阶段、菜单／选项、音乐设置和地图／进度保持，像素保持，无LCD提交。同一窗口的17次原0x86确认均实际产生后继画面，核对输入窗口能够送达服务。

这是明确输入窗口的逻辑控制验证，不证明物理键盘扫描、全RAM、实际时间精度或原PCM。新的采样输出在 `tmp/system-ignored-original/`，既有原画面和状态参考没有覆盖。

## 实际C／SDL

修复前，三个正常输入基线均通过。插入空格后，原标题错误启动新游戏并触发声音请求断言；系统与选槽错误打开下一层并额外提交LCD。失败日志保存在 `tmp/system-before-ignored-*.log`。

| 实际main控制器 | 每种输入的原参考画面 | 每种输入的无效键 |
| --- | ---: | ---: |
| 标题／选槽／读取失败 | 24 | 154 |
| 系统根／子菜单／音乐／返回 | 77 | 532 |
| 系统保存／读取／空槽失败 | 33 | 196 |
| 合计 | 134 | 882 |

默认、Shift、Caps Lock三种输入合计402次原参考C／SDL画面对照、2646次无效输入检查通过。无效键检查完整World（排除主机专用defer_story_events）、保留索引像素及三个共享标题控制字节；LCD提交立即失败。

存读档探针仍使用自身文件，正常保存、读取、空槽／实际文件读取失败，以及NPC保留／重建均符合既有原参考。独立音乐回归的60次C／SDL弹出页状态检查及完整word音乐设置通过，不把重复状态检查并入402次画面提交。

本批只在探针加入可选无效输入；系统槽和标题的正常输入增加I/M/K及Shift／Caps映射。原参考没有改变。生产主程序和四个受影响目标已重建，定向构建无警告／错误。

## 重现与剩余范围

```powershell
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
node npe-sdl/tests/reference_system_ignored_keys.mjs
cmake --build npe-sdl/build --target npe-sdl sdl-title-slot-controls-probe sdl-system-menu-return-probe sdl-system-slot-return-probe sdl-music-menu-probe -j 6
$env:NPE_PROBE_SYSTEM_IGNORED_KEYS='1'
foreach($target in @('sdl-title-slot-controls-probe','sdl-system-menu-return-probe','sdl-system-slot-return-probe')) {
  & ('npe-sdl/build/'+$target+'.exe') tmp/tjfy-world-probe.npe
}
Remove-Item Env:NPE_PROBE_SYSTEM_IGNORED_KEYS
python npe-sdl/tests/verify_system_ignored_keys.py
```

Shift／Caps设置 `NPE_PROBE_BATTLE_KEYS` 为 `shift`／`caps`。审计脚本核对已有最终日志，输出 `tmp/system-ignored-report.json`，不重新运行游戏。

其它输入／文字调用者、动画、共享状态、准确原声及完整跨NPE兼容继续；整体粗估仍约80%。原机存档互通、正常属性完整通关及SDL全程手玩不计入验收。

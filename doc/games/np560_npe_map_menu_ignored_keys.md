# 地图用药、装备与仙术的无效输入（2026-10-10）

## 生产修正

- 地图用药目标、装备目标、仙术选人／列表／目标删除空格确认，使用原 K／Enter。
- 用药目标页首次进入或有效选择／使用后才更新；无效键及数量耗尽后的被冻结输入保留画面，不重新读取资源或提交 LCD。

本批生产修改限于 `npe-sdl/main.c`。效果规则、NPE 数据及存档格式沿用现有实现。

## 原执行证据

原菜单分派 `4B:B5A0..B70C`、仙术分派 `4C:312E..327A` 仅将 K/k／0x86 送入确认服务，空格32／0x90无对应分支。源代码记录在 `tmp/map-menu-ignored-dispatch-source.asm`。

地图物品模式4另以实际原执行路径核对。`reference_map_menu_ignored_keys.mjs` 复用三个既有参考采样器的原入口及诊断库存、缓存、人物输入，生成独立临时采样器和输出；原参考文件没有覆盖。

四条原路径包括单体用药、装备、地图仙术以及用药耗尽。每个正常输入之前插入空格32／0x90、Tab以及Q/U/E/D/F大小写，共260次；采样状态与像素均保持，无原LCD提交。耗尽后另有上／下／确认三次输入，同样保持状态、无提交。

正常确认也通过同一个7A76键盘词窗口送入原0x86。对应正向状态及11张菜单画面与既有原参考一致，从而核对该注入窗口能够触发原服务。采样不证明物理键盘扫描或真实时间精度。

菜单结束后的地图周期提交次数会因诊断等待时间不同而变化；本批正向画面对照仅计算菜单画面，进入前及每阶段的采样游戏状态保持一致。

## C／SDL 结果

修复前，三个实际控制器的正常输入基线均通过；插入空格后均出现原版没有的LCD提交。失败日志为 `tmp/map-menu-before-ignored-*.log`。

| 控制器 | 既有原参考路径 | 每种输入的C／SDL画面 | 每种输入的无效键 |
| --- | ---: | ---: | ---: |
| 地图用药 | 6 | 18 | 161 |
| 装备 | 9 | 36 | 238 |
| 地图仙术 | 13 | 51 | 378 |
| 合计 | 28 | 105 | 777 |

默认、Shift、Caps Lock三种输入合计315次原参考C／SDL画面对照、2331次无效输入检查通过。每次无效输入检查完整World、保留索引像素及资源读取计数；LCD提交立即判失败。

必要回归：15条实际SDL用药流程、44张不同原参考屏幕、原人物数值和自身存读档检查通过；一至三人装备、耗尽、数量100及有符号负库存保护通过。回归屏幕检查同时含直接C调用，不并入315次实际C／SDL计数。

旧装备／仙术探针将原确认映射16发送为空格，已改为Enter。旧用药探针要求耗尽页重复调用渲染至少三次，已按原耗尽输入无提交证据改为最后一次使用仅重画一次；原数值和像素参考没有改动。首次回归失败保留在 `tmp/map-menu-regression-sdl-map-item-flow-probe.log`。

生产程序已重建，定向构建无警告／错误。汇总为 `tmp/map-menu-ignored-report.json`，审计脚本只核对已有结果，不重新执行游戏或生成参考。

## 复现

```powershell
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
node npe-sdl/tests/reference_map_menu_ignored_keys.mjs
cmake --build npe-sdl/build --target npe-sdl sdl-map-item-record-probe sdl-equip-item-record-probe sdl-map-skill-record-probe -j 6
$env:NPE_PROBE_MAP_MENU_IGNORED_KEYS='1'
foreach($target in @('sdl-map-item-record-probe','sdl-equip-item-record-probe','sdl-map-skill-record-probe')) {
  & ('npe-sdl/build/'+$target+'.exe') tmp/tjfy-world-probe.npe
}
Remove-Item Env:NPE_PROBE_MAP_MENU_IGNORED_KEYS
python npe-sdl/tests/verify_map_menu_ignored_keys.py
```

Shift／Caps版本设置 `NPE_PROBE_BATTLE_KEYS` 为 `shift`／`caps`。审计使用本批全部最终日志。

## 剩余范围

其它标题／系统输入、文字调用者、动画、共享状态、准确原声及完整跨NPE兼容仍按根计划继续；整体粗估约80%，未完成整个引擎。用户排除的原机存档互通及两项全程游玩验收不加入本批。

后续已完成标题、系统、存读档选槽、音乐设置的空格与标题无效重绘修正，见[标题与系统无效输入](np560_npe_system_ignored_keys.md)。其它调用者仍以根计划为准。

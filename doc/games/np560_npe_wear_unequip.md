# 原版穿戴页 U 卸装

更新：2026-10-10。纯 C＋SDL 主程序已接入；完整引擎仍未完成。

## 原函数与实现

- 原输入 `4B:B5D6..B5F8` 仅在穿戴页 `7A54=10` 调用 `3F:8364`（运行地址 `22:8364`），接受 U／u。
- 使用页面缓存编号 `7AB2`、人物 `7ADC` 和显示槽位 `7A2A`；空编号直接返回，无 LCD 提交和缓存改写。
- 库存只归还一件，使用原 16 位回绕。按 `69D8、69DA、69DB、69DC、69D9、69DD` 查找第一个匹配编号，清除后立即跳至 `87B6`，然后调用 `13:9444` 扣除效果、重建状态。
- 不增加装备类型与槽位校验；即使缓存编号没有匹配槽位，原函数仍归还库存并移除其效果。
- 擦除图标的 inclusive 矩形 `(9+38*s,80)..(41+38*s,112)`、十二空白名称单元和说明区 `(85,28)..(226,73)`；保留选框残迹及其它图标。完成后将 `7AB2` 清零，只提交一次。
- 保留 `7ABA／7ABE／7ABC` 说明长度及分页索引。后续 N／P 仍使用实际共享物品缓存，不重新加载原装备说明。
- 名称、图标与属性效果从当前 NPE 读取。生产程序不执行原固件或参考解释器。

此前反汇编阅读将六个比较分支误判为“清除全部匹配槽位”。实际重复编号运行保留其它装备，成功分支后的无条件跳转也证实只清除第一个；生产实现与计划均已纠正。

## 原版参考及验证

`reference_wear_unequip.mjs` 在原解释器的真实启动快照上提供诊断初始状态，通过原菜单及原 U/u 输入执行；期望值不来自 C 引擎。原 NPE 未修改。

| 原路径 | 覆盖 |
| --- | --- |
| six-slots | 六个合法装备位置，大小写 U 与重复卸装 |
| duplicate-ids | 多槽重复编号，只清除首个匹配，保留图标与共享缓存 |
| cached-selected-id | 页面缓存编号与实际槽位不同，包含无匹配编号 |
| wrong-slot-type | 原函数不验证槽位类型，卸装后 N/P |
| empty-slot | 空槽 U/u 无提交及分页边界 |
| two-members | 克隆人物的成员／槽位交错；仅作诊断 |
| two-native-members | 两个原生不同编号人物，成员／槽位交错 |
| word-wrap | 库存 65535→0、有符号属性及状态保留 |
| description-retained | 卸装后保留分页索引、共享缓存与后续浏览 |

9 条路径、101 次原 LCD 提交、18 次无提交输入。实际 `main.c`／SDL 的默认、Shift、Caps 三种输入共 **303 次整屏对照、54 次无提交检查**，全部一致。每个输入后核对三个人物的 444 字节记录、255 个库存 word、142 字节物品缓存、144 字节仙术缓存、页面索引与玩家／NPC／设置状态。

8 条有效人物状态路径实际按 F4 返回地图，再保存、用新文件／新 World 冷读取，核对装备、库存、属性与状态，并重绘地图；三种输入共24次通过。额外返回帧不计入原穿戴页画面对照。克隆同编号人物不属于该存档有效 ID 域，独立诊断不作存档验收。

修复前主控制器使用同一探针，前四次画面及状态一致，第五次 U 的原 LCD 提交缺失：`C=0 native=1`，见 `tmp/wear-unequip-before-controller.log`。只替换该旧控制器，核心对象保持当前版本，结论限于原 U 入口未接入。

必要回归：穿戴查看71次、说明服务57次、装备操作36次，合计 **164 次原参考 C／SDL 对照**。主程序及受影响探针已重建。

## 文件和复核

- `tmp/native-wear-unequip.json` 与 `native-wear-unequip-*.idx`：独立原状态及整屏输出。
- `npe-sdl/tests/wear_unequip_native.h`：只序列化原结果。
- `tmp/wear-unequip-report.json`：原输入、参考、代码、程序和日志哈希及范围。
- `npe-sdl/tests/verify_wear_unequip.py`：仅核对已有日志与参考，不重跑或生成期望值。

```powershell
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npe-sdl sdl-wear-unequip-probe
npe-sdl/build/sdl-wear-unequip-probe.exe tmp/tjfy-world-probe.npe
$env:NPE_PROBE_WEAR_KEYS='shift' # 或 caps
npe-sdl/build/sdl-wear-unequip-probe.exe tmp/tjfy-world-probe.npe
Remove-Item Env:NPE_PROBE_WEAR_KEYS
python npe-sdl/tests/verify_wear_unequip.py
```

这批证明上述卸装及后继路径。其它特殊输入、剩余事件与组合、准确原声和跨 NPE 完整兼容继续，以根 `plan.md` 为准。

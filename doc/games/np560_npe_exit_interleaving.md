# 出口选择与存读档、NPC对话交错

本节记录商店修复前的失败。当前三条路径45次提交已匹配，见[商店数据与入口](np560_npe_shop.md)。

2026-10-07。生产实现为C11＋SDL2；原PRG／WASM只用于离线采样。不重复用户取消的正常属性通关或SDL全程实玩。

## 运行中读档

原厂保存解码3C:416C..4396没有写出口选择字节7AC4和出口槽7A84。运行中从系统菜单或标题读档都保留已有值；冷启动仍从零初始化。此前C的`npe_world_restore`用冷启动状态替换这两个值，本轮改为保留运行中状态，不修改NPESAV15布局。

原厂矩阵：两个入口×选择1／2×槽3／10，共8组。保存时选择／槽为0／0、进度1；读档前进度4，选择和槽非零；读档后进度恢复1而两个选择字段保留。C数值和实际SDL主程序均通过，并检查冷启动清零、失败恢复不改原状态。

标题入口使用诊断RAM切换模式7；其读档后的屏幕坐标是71／188，系统入口是13／4。本矩阵只证明模式／事件／地图／进度／选择字段，**不证明标题入口的坐标或LCD完整复刻**。

## NPC空格入口和保留画面

- 原控制器37:139A..13E8先执行NPC选择，再检查7AC4。出口提示仍显示时，有目标的空格进入模式2，不能被出口按键门控吞掉。
- 原NPC选择器33:3772..3796从视口偏移＋屏幕坐标取得站位。C原先使用请求出生点；底部入图调整后两者可能不同，现使用实际视口位置。
- NPC原文和分页来自当前NPE。按原服务固定在(0,84)..(240,118)绘制，长段落裁剪，不随字数增高；保留第119行和底图。
- 各页从同一保留背景绘制，结束后先提交恢复背景，再执行控制器后续。出口提示路径恢复提示及边缘，然后返回地图。
- 原文本服务43:6930..693C只接受空格0x20或0x90。提示上对话样本追加Enter后零提交、模式不变；C的保留NPC页已按此忽略Enter。

## 两条实际主程序路径

| 路径 | 原厂提交 | C与SDL匹配 | 结果 |
| --- | ---: | ---: | --- |
| 出口提示上空格交谈 | 14 | 14 | 五页、Enter忽略、恢复提示／地图、后续方向全部匹配 |
| 确认换图后空格交谈 | 18 | 16 | 对话及背景恢复匹配；提交16开始出现商店差异 |

共32次原厂提交，30次C索引像素和30次实际SDL像素一致。**完整对话探针退出非零**：第二条提交16，原厂是“买”商品页，C仍是地图，两条显示路径各差15783像素；提交17尚未到达。不把该矩阵报告为全通过。

第二条结束阶段原厂进入模式4／事件1；当前C模式4主要用于属性／穿戴。原厂4F:43F6按事件2分支，另一分支初始化3F:B4BA又按上下文7938分支。需要还原统一的原厂模式4服务，按当前NPE及上下文选择属性或商店，不能按地图7／NPC1写特例。对话缓存7A0A／7A82仍为1／1，**这项转换已在后续定位到NPC控制器3B:76EC..7720：所选地图NPC记录的第12字节进入7A68，值≥10覆盖普通后续并进入商店，见商店记录**。

另一次在换图后对话阶段注入Enter的诊断产生13次提交，提前进入模式4；保留在`tmp/native-exit-dialogue-enter-injection.json`。当时未解释，不计为“换图后Enter忽略”通过证据。后续查到3B:7770..77C6是商店NPC的Enter快捷入口，新增第三条原厂／C／SDL13次路径已匹配；普通空格路径仍为18次。

## 剩余商店模块

- 当前NPE商品表、物品原文／位图、买价和卖价读取。
- 通用原厂入口／上下文、买卖列表、分页、选择和返回。
- 数量与背包变更、金钱扣增、缺钱／库存和数量边界。
- 局部原厂数值与逐帧对照；包含上面的现有失败路径。

## 证据与复现

原厂采样与C夹具：`tests/reference_exit_state_load.mjs`、`tests/reference_exit_choice.mjs --dialogue`、`tests/exit_state_load_native.h`、`tests/exit_dialogue_native.h`。参考LCD来自实际原厂显示提交，未由C画面生成。NPC运行中诊断条件只写隔离RAM；C探针修改自己的内存资源副本，原文件不变。

```powershell
cmake --build npe-sdl/build -j 4
ctest --test-dir npe-sdl/build --output-on-failure
./npe-sdl/build/sdl-exit-state-load-probe.exe tmp/tjfy-world-probe.npe
./npe-sdl/build/sdl-exit-dialogue-probe.exe tmp/tjfy-world-probe.npe
python tmp/report-exit-interleave.py
```

当时对话探针报非零；现有夹具已扩展为第三条路径，当前探针退出0、45次匹配。`exit-dialogue-check.log`和`exit-interleave-counts.json`保留旧失败；新结果见`shop-dialogue-check.log`。日志：`tmp/exit-dialogue-check.log`、`exit-dialogue-result.json`、`exit-state-load-check.log`、`exit-interleave-all-build.log`、`exit-interleave-ctest.log`；汇总`exit-interleave-counts.json`。全量构建无警告，ctest5/5，11项相关局部回归退出0；最后NPC按键修改后重新运行模式2、出口选择103次及8组恢复探针，均退出0。

源码证据：`tmp/exit-interleave-original-source.asm`、`music-load-codec-map-source.asm`、`shop-dispatch-original-source.asm`。`exit-shop-entry-original-source.asm`的4E:A3F4实际为音乐服务，不能用它解释商店转换。

原文件SHA256保持：NPE `DF4B52DF6B69E1105A5EA6100E43F35F832ED859C10CB8714879E44AD4D8CB40`；PRG `D555609E03ECE17F42C1881BEEB88361572F18621161DE67BC9DC765857089F5`。NPESAV15未变。其它对话／对象缓存、LCD层交错、真实时基／启动RNG、声音及跨NPE机制仍待完成。

# 模式4无效商店上下文与分层返回

更新：2026-10-10。共用入口和F4返回已接入独立C11＋SDL2程序；没有按游戏、地图或事件1796特判。

## 原版行为与生产修正

原模式4分派`32:43F6..446A`（PRG物理`4F:43F6`）将事件字2交给物品使用服务，其余事件使用共用商店服务。初始化标志`7AB4`非零时调用`22:B4BA`，随后清除标志，包括初始化拒绝上下文的情况。

`22:B4BA`（物理`3F:B4BA`）先将图形背景设为0，扫描下限／上限设为1／254，库存筛选设为0。按有符号上下文`7938`分派：大于10读取购买目录，等于10进入出售，小于10在`B740..B744`返回，不调用LCD提交。失败入口保留原UI、商店类型、选择项、边界、行号、数量、选择标志、交易方向、目录及说明页位置。原C入口在此误画人物属性页，且不清除初始化标志；现已修正。

F4服务`32:1E6E..1EF0`保留事件字：UI为1时进入地图模式并设置初始化标志；其它UI先设为1，恢复矩形`(29,4,211,111)`并提交，仍留在模式4。C共用返回服务按这两个分支处理，数量窗口自身的输入循环继续使用其保留底图。上／下列表导航及确认数量也按原UI和选择项条件分派。

原队伍服务`22:0F6E..0F76`在查事件表之前设置`7AB4=1`。本轮实际天明入口暴露C漏写这一标志；SDL队伍入口及数值事件入口已补齐，未改人物转换规则。

## 验证范围

- `reference_mode4_rejected_entry.mjs`执行未修改的在线原引擎与原NPE。只设置明确的诊断RAM输入，采样上下文0、9、65535及旧UI0／1／2，共5组。5个初始化都没有LCD提交，4组先恢复列表层，5组最终返回地图。
- `generate_mode4_rejected_entry_fixture.py`从原报告提取视口、NPC位置及入口参数，检查首次恢复画面与原保留画面相同。C探针借用原入口前的LCD作为诊断输入，不把C输出当作原参考。
- `sdl-mode4-rejected-entry-probe`使用生产`draw_world`、共用F4服务和SDL软件渲染器；5组初始化、状态保留及重复调度无额外提交，9次返回提交的28800个像素全部与原版一致，SDL颜色读回一致。
- 天明V1.3实际地图140第6组第2槽的两个队伍诊断状态，均由当前NPE记录产生模式4／事件1796／上下文0。实际队伍服务→后继→入口→两次F4完成，无多余入口画面，最后返回地图并保留事件1796。特殊队伍整批10组原人物／模式／事件状态继续一致，实际SDL输出42次，其中4次为新增菜单返回。没有天明此菜单的独立完整原LCD参考，不能把这42次均称为原像素匹配。
- 必要回归：普通商店数量／交易UI44次、关闭33次、关闭后的NPC周期16次，均为原C／SDL整屏对照通过。

旧关闭探针在进入商店前出现195像素差异，改动前的主程序也复现同一失败。它将新游戏视口改到诊断坐标，却保留了出生位置的105格缓存。现从既有原报告注入同一210字节缓存，33次关闭路径重新通过；原参考画面及报告没有重新采样。该修正只改探针的诊断输入。

## 文件与复现

生产入口：`world.c`的`npe_world_shop_mode_begin`、`npe_world_shop_return`，`main.c`的`draw_world`、`game_shop_return_display`及共用输入分支。

```powershell
python npe-sdl/tests/generate_mode4_rejected_entry_fixture.py
cmake --build npe-sdl/build --target npe-sdl sdl-mode4-rejected-entry-probe sdl-special-party-probe sdl-shop-ui-probe sdl-shop-close-probe sdl-shop-return-periodic-probe -j4
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
& npe-sdl/build/sdl-mode4-rejected-entry-probe.exe tmp/tjfy-world-probe.npe tmp/native-mode4-rejected-entry
& npe-sdl/build/sdl-special-party-probe.exe tmp/special-party.path tmp/special-party-mode4-fixed tmp/special-party-native
```

原记录：`tmp/native-mode4-rejected-entry/report.json`及对应idx；源指令：`tmp/shop-dispatch-original-source.asm`、`tmp/mode4-return-original-source.asm`。初始入口失败：`tmp/mode4-rejected-entry-before.log`；原像素通过：`tmp/mode4-rejected-entry-final.log`。关闭探针失败及基线：`tmp/mode4-regression-shop-close.log`、`tmp/mode4-baseline-shop-close.log`；修正结果：`tmp/mode4-regression-shop-close-cache-fixed.log`。汇总：`tmp/mode4-rejected-entry-final-report.json`。

本批没有完成原音频、扩展字库、其它事件／动作组合或全部游戏兼容；整体仍粗估约80%。

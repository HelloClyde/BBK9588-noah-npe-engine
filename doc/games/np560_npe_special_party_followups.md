# 天明V1.3特殊队伍服务与初始后继

更新：2026-10-10。运行程序仍为独立C11＋SDL2；PRG／QEMU仅用于离线原版参考，生产规则和NPE文件未修改。

## 原参考与结果

`reference_special_party.py`执行未修改的原模式10服务`22:0F60`、队首服务`29:EF8C`及经验服务`27:C8B6`。地图表来自此前原完整装载器的139／140 RAM样本，必须与当前NPE的SHA256一致。三名角色记录是C装载当前文件后的明确诊断输入：首名体／气设为1，加入事件分别使用第二槽未活动／活动两种状态；原CPU独立产生预期输出。提示等待以RETS替代，没有原LCD、输入或物理时序证明。

原队伍、经验、字符串和记录复制代码区域逐字节检查；10次调用正常返回，栈恢复，未报告缺失ROM或不支持指令。补充复制函数代码检查前后，10组原输入／输出哈希相同。

| 当前地图记录 | 原服务后继 | 结果 |
| --- | --- | --- |
| 139，第6组第1槽；加入标志5、角色0 | 模式5／事件5 | 按原既存队员选择分支处理，不能创造角色0；C实际对白后继返回地图／事件35 |
| 140，第6组第1槽；加入标志130、角色0 | 模式23／事件1029 | 原经验服务升到2级，体135、气64、剩余经验92；C一致 |
| 140，第6组第2槽；加入标志4、角色6 | 模式4／事件1796 | 服务状态一致；共用入口原行为及C修正已完成，两种队伍状态均经两次F4返回地图并保留事件1796，见[模式4入口](np560_npe_mode4_rejected_entry.md) |
| 140，第6组第3槽；加入标志4、角色10 | 模式10／事件1034 | 服务状态一致；C后继按低事件字节10越界返回地图 |
| 两图第3组队首记录；角色10／178 | 模式0，保留事件1 | 原三槽没有该角色，原服务不交换；C一致并返回地图 |

`sdl-special-party-probe`首先核对同一444字节输入，再核对三名完整记录的444字节原输出及模式／事件，共10组；实际`show_party_event`／`show_party_adjust_event`和初始后继完成36次SDL提交，SDL与C像素一致，运行时错误数为0。完整记录比较证明这些诊断输入下的转换，不证明C的人物初始构造或所有原记录状态都已完整复刻。

上述36次输出是初始队伍批次。后续模式4批次补齐队伍服务原入口设置的初始化标志，并接入共用无效上下文入口和分层返回；同10组原人物状态继续一致，输出42次，其中两种队伍状态新增4次菜单返回。共用菜单5组诊断入口及9次返回另有原完整LCD对照，天明139对白及140菜单没有独立完整原LCD参考。两个队首样本都没有匹配目标，不能当作新增成功换首样本。没有用这些短路径证明新游戏自然可达或整款游戏兼容。

## 跨NPE检查接入

旧检查跳过零尺寸及非标准队伍记录，并在完整后继之后要求人物属性不变。后者不适用于经验后继。

现在只有提供原参考、且参考报告的源文件SHA256匹配时才运行这些特殊记录；同时比较原输入与原输出，没有直接放宽体力断言或修改NPE。普通记录继续沿用身份保留检查，无原参考的其它特殊文件仍保留未解决状态。

只重跑受影响的天明V1.3：20次入／离队、2次队首服务、2个防御回合、236次SDL提交，6条特殊服务另核对原状态，未解决地图数0。此处“0”只对应队伍检查范围。

原18份报告中天明此前为16次入／离队、0次队首、216次提交及2张未解决地图。其它17份既有结果沿用，累计169次入／离队、5次队首、26个防御回合、1903次SDL提交；11份含此类事件的文件均完成短检查。滚动报告明确标记17份沿用、1份本轮运行，不把它称为本轮全量测试。其它7份没有此类事件，仍不计新增动态覆盖。

## 证据与复现

- 原报告：`tmp/special-party-native/report.json`；初稿留存：`tmp/special-party-native-initial-report.json`。
- 实际SDL：`tmp/special-party-final/cases.jsonl`、`run.log`。
- 天明跨NPE：`tmp/cross-npe-party/special-final/report.json`。
- 累计队伍短路径：`tmp/cross-npe-party/rolling-special/report.json`。

```powershell
cmake --build npe-sdl/build --target sdl-special-party-probe sdl-cross-npe-party-probe -j4
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
# manifest首行是NPE绝对路径，第二行是139 140；输出目录先创建。
npe-sdl/build/sdl-special-party-probe.exe tmp/special-party.path tmp/special-party-export --export
python npe-sdl/tests/reference_special_party.py --qemu E:/qemu-dmcu-win-build/qemu-system-dmcu.exe --npe tmp/tianming-v13-map-continuation.npe --inputs tmp/special-party-export
npe-sdl/build/sdl-special-party-probe.exe tmp/special-party.path tmp/special-party-final tmp/special-party-native
python npe-sdl/tests/run_cross_npe_party.py --directory D:/Downloads/NP560plus游戏全集 --filter 天明游戏V1.3 --output tmp/cross-npe-party/special-final --special-reference tmp/special-party-native
```

三个受影响探针构建通过，无警告；离队入战16组104次原C／SDL对照另已收尾，见[离队入战修订](np560_npe_leave_battle.md)。原声、扩展字库、其余事件／动作组合和游戏状态恢复继续，整体仍粗估约80%。

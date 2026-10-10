# 技能列表宽头像与说明区域保留

2026-10-10。修复地图技能页的四处绘制差异。生产仍为独立C11＋SDL2，原程序只生成离线参考；没有增加游戏名、技能名或指定头像编号的生产分支。

## 原规则与修复

| 原程序依据 | 生产修复 |
| --- | --- |
| `3B:5DDA`初次标志为零跳到`613E` | 保留更新跳过边框、名称与头像绘制；选择调用者只清除说明区域 |
| `3B:6366..6426` | 初次说明不额外清除整个矩形，保留宽头像伸入说明区的像素 |
| 物理`46:E552..E7E8`／运行`29:E552`调用`03:41A8` | 紧凑说明按实际字符格清除背景；CR／LF占空白格，短行末尾保留底图 |
| `47:589C`及`47:5B14` | 前后翻页均清除y59～110，文字仍从y60开始 |

改动在`text.c`的共用技能列表、紧凑说明和技能分页。图像继续读取当前NPE的人物第八图号；宽度或游戏名不作为特例。没有改变技能属性、费用、等级、缓存内容或存档格式。

## 分阶段失败与对照

旧六组头像参考的`wide-caster`首次技能列表有3804个C／SDL像素差异。差异位于x72～230、y60～110，C多清除整个说明矩形。旧源码、可执行文件及像素保存在`tmp/portrait-skill-clear-before/`。

只撤去入口清除后，原六条路径／36次画面对照通过；新增原采样进一步发现：

1. 同窗口换技能重画宽头像，3804像素差异。按初次标志跳过静态重绘后修正。
2. 100字节ASCII说明只画前景，2200像素差异。按占用字符格绘制背景后修正。
3. 后页漏清第59行，13像素差异。原前后分页清除坐标均为59，已修正。

`tmp/magic-description-retain-first-check.log`、`second-check.log`、`third-check.log`保留阶段失败；最终结果不覆盖它们。

## 原版采样和实际SDL验证

`reference_magic_description_retain.mjs`从原暖标题存档快照恢复，通过原地图菜单进入技能页。诊断指定宽头像资源、重复技能前缀、100字节ASCII或含CR／LF的缓存说明。原NPE、PRG、位图及原快照文件不修改。

| 路径 | 覆盖 | C／实际SDL画面 |
| --- | --- | ---: |
| wide-duplicate | 同窗口左右移动与保留更新 | 4 |
| wide-window | 十技能跨窗口和返回 | 6 |
| wide-pages | 初次、前后页和边界停止 | 4 |
| wide-controls | 宽底图上CR／LF字符格 | 2 |
| 合计 | 4条原路径 | 16 |

全部16次240×120索引画面及SDL读回零差异，并逐阶段核对144字节技能缓存、三份驻留头像、每名角色16项字段及页面／说明首尾状态。两个分页边界阶段确认零提交。六次外层关闭画面保留，但本批不比较其画面。

分页诊断把end设为89进入前后页控制分支，说明仍在原101字节缓存读取上限内，没有延长说明覆盖技能参数。这组证明列出的共享服务行为，不计为自然剧情或其它NPE完整流程。

## 受影响短回归

主程序及相关目标构建无警告。六项短检查退出0：

- 原头像技能6条／36次画面。
- 原地图共享技能13条／51次画面。
- 地图默认类型4条／17次画面。
- 地图道具／技能说明32次原页和7次选人画面。
- 装备说明57次画面。
- 装备查看71次画面。

原参考和预期图像均未为了通过回归而修改。结果日志为`tmp/magic-description-retain-<目标>-final.log`。

## 战斗旧探针后续定位（已收尾）

当时战斗共享技能旧探针的`same-party-target`第1次提交有4647个C／SDL像素差异；修改前`text.c`也复现。原失败日志`tmp/magic-description-retain-battle-before.log`及`tmp/magic-description-retain-sdl-battle-skill-record-probe-final.log`保留，上一批报告反映当时尚未定位的状态。

后续发现原战斗快照已持有打包图像资源，旧探针却在进入技能列表之前没有建立同样的资源上下文；共享说明服务延后打开资源，破坏目标页所需的图像分配顺序。每条原路径恢复独立快照，旧探针也应在复用栈上的World之前关闭上下文。

仅修改`tests/sdl_battle_skill_record_probe.c`：所有变体进入实际`show_battle_skill`前建立战斗资源，每例结束关闭上下文。先只补关闭仍有4647像素差异；补齐入口后10条原路径／33次C与SDL整屏及144字节缓存、待确认字段全部一致。资源版13条路径／73次画面也通过，原JSON、夹具和预期图像未改。没有为此修改生产渲染或游戏规则。

修正前探针源码、可执行文件和失败像素保存在`tmp/battle-target-lifetime-before/`；只补关闭的失败为`tmp/battle-target-lifetime-first-check.log`，最终结果为`tmp/battle-target-lifetime-second-check.log`及`tmp/battle-target-lifetime-resources-check.log`，两项构建均成功。本差异不再列入生产未完成项。

## 文件与范围

原JSON为`tmp/native-magic-description-retain.json`；夹具由`generate_magic_description_retain_fixture.py`生成；实际生产入口探针为`sdl_magic_description_retain_probe.c`，复用`main.c`；原格式器与后页反汇编为`tmp/magic-description-retain-source.asm`；最终报告为`tmp/magic-description-retain-report.json`。

本批收尾宽头像技能页已定位差异。其余事件／战斗组合、文字生命周期、原音频和完整通用兼容继续，整体仍粗估约80%，范围以[根计划](../../plan.md)为准。

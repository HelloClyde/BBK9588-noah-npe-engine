# Release 未初始化与记录边界修正（2026-10-10）

## 原问题与修改

上一批 Windows 便携构建使用 GCC 16、`-O3 -DNDEBUG`。编译成功但报告20条警告：
资源查看控制器的未初始化节号、NPC坐标的可能未初始化读取，以及战斗回合清理的
六槽记录数组越界。原日志为 `tmp/npe-portable-build.log`。

本批修改三份生产源文件：

- `world.c`／`npe.h`：NPC坐标查询改为返回状态，成功写入两个坐标返回0，无效参数
  返回-1并保持输出。全部12处生产调用在使用坐标前检查结果；移动、脚本、碰撞、
  相邻层次、可见性和绘制分别沿用现有错误／布尔返回约定。原坐标字段、覆盖值、
  `NPE_NPC_POSITION_NONE`判断及>=20槽的原记录回退算式保持相同。
- `world.c`：战斗回合清理在读取指令字节前检查物理槽号小于6，与既有记录读写接口
  的边界一致。有效的0～3个队员、0～3个敌人记录仍使用原槽号；不改变防御指令
  的保留规则。该检查用于避免宿主越界，不宣称补齐原引擎的任意非法队伍状态。
- `main.c`：资源查看节号初始化为0；有效查看入口继续由`first_bitmap`选择实际节。

`CMakeLists.txt`为这次选用的NPC周期和行走探针保留Release断言；两轮混合战斗
探针原已有该设置。生产编译继续使用`-Wall -Wextra -Wpedantic`，未关闭警告。

两份主程序构建和五份检查目标重建成功。相同`-O3`生产构建警告数从20降为0，
常用构建及相关检查目标也没有警告。备份为`tmp/npe-release-warning-before/`，
逐文件差异为`tmp/npe-release-warning-*.diff`。

## 原参考短路径

| 目标 | 本批结果 |
| --- | --- |
| `sdl-npc-periodic-probe` | 15条原NPC状态路径通过；位置、朝向、共享随机、存读档、计时／菜单门控及实际SDL周期地图保持 |
| `sdl-walk-pose-probe` | 10次输入、42次原提交；直接C／SDL各42次以及实际main的42次C／SDL对照均零差异 |
| `sdl-battle-reentry-mixed-probe` | 连续入战的两轮混合行动、226次原参考C／SDL画面、30个状态检查点均通过；人物／记录、菜单、库存／共享缓存检查保持 |

上述沿用已有原程序采样、夹具和像素文件，本批未重新生成参考，也不新增原版
独特画面统计。参考输入中的诊断属性属于服务／短路径检查，不是完整游戏通关。
NPE SHA-256仍为`df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40`。

NPC查询失败和越界槽号属于宿主接口边界；表中正常原路径证明已支持行为保持，
不能据此宣称所有错误资源、其它队伍组合或全部12处调用的剧情后继均已验证。

## 更新便携包

便携ZIP和安装目录已用修正后的生产程序重建。两个交付探针重建后，原14项
独立交付检查再次通过，包括四份NPE的中文路径与SDL标题、释放资源后的自身
冷读取、天明V24全部3500个缓冲word、三槽菜单短路径、实际有限启动及字体
失败处理。具体证据边界沿用[便携交付记录](np560_npe_portable_delivery.md)。

- ZIP：`npe-sdl/dist/npe-sdl-windows-x64-dev.zip`，2,059,212字节。
- 当前报告：`tmp/npe-release-warning-report.json`和`tmp/npe-portable-report.json`。
- 初始ZIP和交付报告保存在本批备份目录。
- 构建日志：`npe-release-warning-build.log`、`main-build.log`、`probe-build.log`及`delivery-build.log`，均位于`tmp`。
- 三项原路径日志：`tmp/npe-release-warning-sdl-*-probe.log`。
- 交付日志：`tmp/npe-release-warning-install.log`、`package.log`及`portable-verify.log`（后三项同前缀）。

## 重查

```powershell
cmake --build tmp/npe-portable-release-build --target npe-sdl -j 4
cmake --build tmp/npe-portable-probe-build --target sdl-npc-periodic-probe sdl-walk-pose-probe sdl-battle-reentry-mixed-probe -j 4
tmp/npe-portable-probe-build/sdl-npc-periodic-probe.exe tmp/tjfy-world-probe.npe
tmp/npe-portable-probe-build/sdl-walk-pose-probe.exe tmp/tjfy-world-probe.npe
tmp/npe-portable-probe-build/sdl-battle-reentry-mixed-probe.exe tmp/tjfy-world-probe.npe
```

本批解决已观察到的Release警告和对应C边界问题。准确原声、可听见的DAC渐变、
其它状态／事件组合与完整跨NPE兼容继续，整体仍粗估80%；整个目标尚未完成。

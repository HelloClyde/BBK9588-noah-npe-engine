# 独立 C＋SDL Windows 便携交付（2026-10-10）

后续更新：Release的20条警告已修正，15条原NPC状态及行走／混合战斗原画面对照通过；当前ZIP为2,059,212字节，14项交付检查再次通过。详见[Release坐标与记录边界](np560_npe_release_warnings.md)。下文保留初次便携交付的构建及诊断记录。

## 初次交付完成

新增 CMake 安装与 CPack ZIP 规则。生产构建关闭 `BUILD_TESTING`，只编译原有九份
C 源文件；安装阶段扫描 PE 依赖，复制所需非系统 DLL，并在缺失或冲突时失败。
此 Windows x64 构建只需附带 `SDL2.dll`，Windows 系统库由操作系统提供。

- 安装目录：`npe-sdl/dist/windows-x64/`。
- ZIP：`npe-sdl/dist/npe-sdl-windows-x64-dev.zip`，2,059,227 字节。
- 包含七份文件：主程序、SDL2 DLL、两份原版字体、字体来源、SDL2 声明、操作说明。
- 打包字体与现有原版字体 SHA-256 一致。游戏文件由使用者提供。
- 运行依赖为 NPE、已提取字体和 SDL2；固件与诊断工具用于开发参考。
- 当前构建／操作说明已整理，旧使用说明中已过期的快捷键和待办从当前使用章节移除。

`cmake/Packaging.cmake` 和 `InstallRuntime.cmake.in` 保持 CMake 3.16 起的接口；较新
CMake 的依赖路径规范化策略仅在该策略存在时启用。Windows 库搜索支持 SDL2
导入目标、pkg-config 库目录和 `NPE_RUNTIME_DIRECTORIES`。

## 独立运行检查

`verify_portable_package.py` 解压实际 ZIP 到系统临时目录中的中文、空格和 emoji
路径，从另一个工作目录执行。子进程 PATH 只保留 Windows 系统目录，排除开发
工具链及 MSYS2 运行库路径；诊断程序使用打包的相同 DLL 和字体。

14 项检查通过，报告：`tmp/npe-portable-report.json`。

| 检查 | 结果及范围 |
| --- | --- |
| 实际交付主程序 `--inspect` | 天降风云、三分天下、天明 V1.3、英雄传奇四份原中文 NPE 路径退出 0 |
| 既有路径探针 | 四张实际 SDL 标题帧与同 NPE／字体的 C 生成帧一致；四份显式诊断存档在释放地图资源后重新读取，字段一致；天明 V24 的3500个缓冲word全部保持 |
| 实际交付主程序标题／新游戏 | 两次启动分别运行 1.5 秒，未提前退出、未报告错误；之后仅终止本检查创建的进程 |
| 实际交付主程序字体失败路径 | 缺少 12 字体、无效 12 字体、缺少 16 字体均返回 1 并报告具体字体路径；工作目录内的有效字体不能掩盖程序目录缺失 |
| 既有三槽 SDL 菜单探针 | 短路径保存、重新进入读取、移动后覆盖、取消和复存通过；使用独立诊断副本及其自身存档 |

原四份 NPE 及 ZIP 提取文件的检查前后 SHA-256 均保持。诊断输出保留在报告
指定的临时目录，包内只有交付文件；探针复制到单独诊断目录运行。

**证据边界：** 标题像素和菜单存读档来自包含当前 `main.c` 的既有探针；实际
交付二进制单独验证了解析、有限启动和字体错误路径。1.5 秒存活不证明原版
LCD 一致，也不证明完整游戏通过。本批没有新增原厂逐帧证据或原声音频证据。

## 路径探针与当前存档格式

最初复用旧路径探针时，《天明 V1.3》保存失败。该探针在新游戏构造后直接保存，
跳过生产程序的地图资源初始化。当前自身存档会保存边缘视口读取的固定地图
缓冲，未绑定资源的 World 无法提供这份缓冲；原拒绝行为符合当前保存要求。

探针现先调用与生产程序相同的 `npe_world_resources_open`，再检查当前合法格式
V22／V23／V24和冷读取字段。此次天明写出 V24，9,562 字节；其余三份写出 V22，
2,366 字节。越界偏移和不一致屏幕坐标仍拒绝写入。生产游戏起点、保存校验及
存档格式未改。两份交付探针在 Release 构建中显式保留断言：
`-O3 -DNDEBUG -UNDEBUG`。

最终四份检查在解码前释放原地图资源，并逐word比较天明V24的7000字节缓冲。
V22不要求保存不影响当前视口的尾部；诊断中的此项错误假设已修正，失败日志
保存在 `tmp/npe-portable-before/cold-snapshot-assumption.log`。只重新构建路径探针
并重跑受影响的四条检查，其余十条已有结果沿用。

首轮失败记录在 `tmp/npe-portable-before/first-report.json` 和 `first-verify.log`。
初次检查脚本的环境变量大小写问题另保存在
`tmp/npe-portable-verify-initial-environment.log`，已修正。

## 构建记录及剩余工作

- `tmp/npe-portable-configure.log`、`npe-portable-build.log`：干净的 Release 生产构建。
- `tmp/npe-portable-install.log`、`npe-portable-package.log`：最终安装与 ZIP 打包退出 0。
- `tmp/npe-portable-probe-build.log`、`npe-portable-probe-rebuild.log`：两个定向检查目标。
- `tmp/npe-portable-verify.log`：14 项最终检查通过。
- `tmp/npe-portable-cold-verify.log`、`npe-portable-probe-cold-rebuild.log`：四份释放资源后的冷读取和V24完整缓冲检查通过。

生产构建成功，但 GCC 16 的 `-O3` 编译报告了未初始化／数组边界警告，保存在
报告及构建日志中，本批未修改生产代码或关闭警告；不能称为无警告构建。
这些警告与音频、共享状态及特殊行为的剩余核对继续处理。

Windows 便携交付流程已具备。非 Windows 安装命令尚未在本机验证。整个引擎仍
粗估约80%，准确原声、可听见的 DAC 渐变及完整跨游戏行为尚未完成。

重查命令（游戏文件使用实际路径）：

```powershell
cmake -S npe-sdl -B npe-sdl/build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build npe-sdl/build-release --target npe-sdl
cmake --install npe-sdl/build-release --prefix npe-sdl/dist/windows-x64
cmake --build npe-sdl/build-release --target package
python npe-sdl/tests/verify_portable_package.py --package npe-sdl/build-release/npe-sdl-windows-x64-dev.zip --probe-build tmp/npe-portable-probe-build --game 'D:/游戏/天降风云V1.3.npe'
```

这里 `--probe-build` 需提供已有两个探针可执行文件的目录；按改动选择检查目标。

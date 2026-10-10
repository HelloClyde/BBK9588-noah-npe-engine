# NPE 主机路径与边缘视口存档

2026-10-10更新：交付检查已使用实际便携ZIP和仅Windows系统PATH；路径探针补齐生产资源初始化，适配边缘视口的V24自身存档。四份路径和冷存档均通过，详见[当前交付记录](np560_npe_portable_delivery.md)。下文保留2026-10-06的历史格式及验证范围。

## 2026-10-06：实际缺陷与修复

原先声音探针用 Windows 窄字符 `fopen` 打不开中文文件名，这不证明主程序
打不开中文 NPE。检查当前源代码后确认：`main.c` 的 `read_file` 和 `save.c`
已经使用 `SDL_RWFromFile`，生产程序链接 SDL2main，Windows 命令行参数转为
UTF-8。四个真实中文路径的生产程序 `--inspect` 均成功。

实际缺口在 `text.c` 的两个字库装载函数：`SDL_GetBasePath` 返回 UTF-8，
但旧 `fopen` 按 Windows ANSI 代码页解释路径。程序位于中文／emoji 目录时
找不到原厂字库。现在 Windows 分支严格将 UTF-8 转为 UTF-16，再用 `_wfopen`
读取；其它平台保留 `fopen`。字库模块仍是纯 C，不新增 SDL 链接依赖。
字库内容、GBK 索引和绘制算法不变。

声音探针也改用 SDL 文件接口，从真实中文 NPE 路径读取，撤销必须先复制到
ASCII 路径才能检查的限制。

## 验证中发现的存档误拒绝

《天明游戏 V1.3》的原 NPE 新游戏起点是：地图 1，25×15 格，角色 `(15,11)`，
视口偏移 `(8,10)`，屏幕格 `(7,1)`。原 PRG `2E:8348..83B8` 装载角色记录
字节 80／81，`46:D28A..D2AA` 将其复制为新游戏偏移；`46:D2DC..D2F0`
固定屏幕格为 `(7,1)`。反汇编见 `tmp/host-path-new-game-source.asm`。

C 存档校验要求 `offset_y + 7 <= map.height`，误拒绝只覆盖最后五行的
合法边缘视口。现按既有地图渲染器的裁剪边界，要求视口起点在地图内，
保留屏幕格范围、角色逻辑坐标一致性及出口底边一行调整校验。
不修改 NPE 提供的起点，不调整任何游戏 ID。

四个实际新游戏地图态均写出／读回 `NPESAV15`，位置和完整视口保持相同。
越界 X／Y 起点与屏幕 X 不一致仍拒绝写入，已有诊断存档随后能完整读回。
格式未变；这不增加原厂存档兼容性。

## 定向检查与结果

`npe-sdl/tests/verify_host_paths.py` 将生产程序、专用探针、SDL2 DLL 和未修改的
字库安装到工作区 `tmp/中文安装 路径🎮/`，校验复制前后 SHA256。依次检查
原始路径《天降风云》《三分天下》《天明游戏 V1.3》《英雄传奇》：

- 生产程序 `--inspect` 从原中文路径打开、解析 NPE。
- 声音探针直接从该路径加载当前文件的 22 段资源。
- 12／16 像素字库从中文安装路径加载，缓冲内容与 SDL 读取的原字节相同。
- 使用含中文、空格和 emoji 的诊断存档名，读回随机状态、钱、位置、视口、
  队伍及四状态字段；错误坐标拒绝写入。
- 专用探针调用实际 `main.c` 标题循环，从安装目录定位字库，并检查 SDL
  提交整屏与相同 NPE／原字库生成的标题帧一致。这里是路径等价检查，
  不增加原厂截图统计，也不代表其它 NPE 的完整机制已经验收。
- 原始 NPE 的运行前后 SHA256 相同。

12 项检查通过，报告 `tmp/host-path-report.json`；各项日志为
`tmp/host-path-0..3-inspect/sound/host.log`，总日志 `tmp/host-path-check.log`。
全量构建、ctest 5/5、三槽 SDL 存档回归与声音调度回归通过；Linux GCC
对字库模块的 C11 语法检查通过。日志：`tmp/host-path-all-build.log`、
`tmp/host-path-ctest.log`、`tmp/host-path-slot-regression.log`、
`tmp/host-path-sound-regression.log`、`tmp/host-path-posix-check.log`。

```powershell
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build -j 4
python npe-sdl/tests/verify_host_paths.py
ctest --test-dir npe-sdl/build --output-on-failure
```

本轮不改连续剧情存档，不重跑正常属性完整通关或 SDL 全程实玩。

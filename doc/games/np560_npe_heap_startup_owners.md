# 启动缓冲区的资源所有者

2026-10-08。原函数和当前暖态内存确认了四块256字节缓冲区的分配顺序、句柄与指针槽。纯C新增`NpeResidentHeapOwnerService`、`npe_resident_heap_allocate_owned`与`npe_resident_heap_launcher_buffers`，实现为`npe-sdl/resident_heap_owner.inc`。

## 原代码和暖态指针

物理PRG38:23CE..2496（运行时1B:23CE..2496）按以下顺序调用原31:4974资源所有者服务，每次请求256字节。静态代码经CODE总线核对后，在独立客机中执行未修改的四次调用及分支。

| 顺序 | 句柄槽 | 指针槽 | 原暖态载荷地址 | 已观察内容 |
| --- | --- | --- | --- | --- |
| 1 | 52:574C | 52:5186 | 51:4A46 | `\*.npe`筛选字符串 |
| 2 | 52:574E | 52:518A | 51:4B4C | 起始0033与保留3333，用途未确认 |
| 3 | 52:5760 | 52:518E | 51:4C52 | `0:\应用程序` |
| 4 | 52:5626 | 52:5192 | 51:4D58 | `0:\应用程序\rpg2bin.npe` |

暖态客机指针扫描发现以上四个所有者，另有51:2978同样指向第一块。扫描使用客机指令访问正确段位，未假定WASM内存中的整片RAM连续映射；前四个推进批次的六张LCD和已采集人物／模式状态与未追踪基线一致。

先前候选52:5326及52:532C的三个连续字在这个暖态均为0，两次诊断另有12张LCD与基线一致。这只说明其当前内容，不能据此否定这些槽在其它阶段的用途。

## 资源所有者服务

原31:4974..4A30接受句柄槽、32位长度及指针槽：

1. 将长度低16位交给原分配器，由分配器执行16位偶数对齐。
2. 将返回句柄写入当前句柄槽，包括分配失败的0。
3. 重新读取句柄槽。若为0，调用所有者失败提示并返回0，原指针槽保持不变。
4. 成功时再读取句柄、锁定它，将返回地址的高低字写入指针槽，返回1。

分配器失败提示和所有者失败提示是两个独立边界。服务不清空载荷，不用捕获地址代替分配结果。四缓冲区接口仅对应分配前缀，调用方仍须负责堆初始化、文件服务及后续退出清理。

静态38:2C46..2C84依次从上述四个句柄槽调用33:1DF6；该清理服务及正常文件选择后转入RPG的完整生命周期还需接通。因此本轮没有把四块捕获堆头直接写入生产初始化，也没有宣称实际主程序管线完成。

## 独立验证与复现

六组原函数结果与C逐字节比较完整128KiB RAM、两个失败边界计数和返回值，共786432个RAM字节。覆盖原四次分配前缀、32位长度高位截断、0长度、FFFF偶数对齐回绕、句柄位图耗尽及堆空间不足。失败检查确认句柄写0而旧指针保持。提示由诊断计数器替代，未验证真实提示画面。

原堆生命周期16组和位图装载／绑定13组必要回归通过；四目标构建无警告。原NPE／PRG／JS／WASM／包五项哈希及NPESAV22保持。本轮未重跑已取消的全程验收。

```text
python npe-sdl/tests/reference_resident_heap_owner.py --qemu /path/to/qemu-system-dmcu
python npe-sdl/tests/generate_resident_heap_owner_fixture.py
cmake --build npe-sdl/build --config Release --target resident-heap-owner-probe npe-sdl
npe-sdl/build/resident-heap-owner-probe.exe
python tmp/finalize_resident_heap_owner.py
```

汇总：`tmp/resident-heap-owner-report.json`。原结果：`tmp/resident-heap-owner-native/report.json`。源码：`tmp/heap-startup-allocation-source.asm`。扫描工具：`tmp/prepare_battle_heap_pointer_scan.py`及生成的`tmp/battle-heap-pointer-scan.mjs`。

## 继续工作

后续已补齐[7000／32768资源分配、驻留读取及启动清理](np560_npe_rpg_resource_lifecycle.md)，七组原函数／C完整内存对照通过；实际world／main管线和完整生命周期仍未接通。

连接启动缓冲区退出清理、7000／32768字节RPG资源装载及真实world／main位图调用，继续其它RAM／控制字段、读取后的执行及图号31实际路径。原PCM、物理时基／输入与更多NPE动态适配仍需完成。工程粗估仍约80%，保留模拟器自身存读档；原机存档互通、正常属性完整通关及SDL全程实玩保持取消。

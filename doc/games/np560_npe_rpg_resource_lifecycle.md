# RPG资源分配、读取与启动清理

2026-10-08。纯C在`resident_heap_owner.inc`补齐7000字节地图缓冲分配、32768字节战斗资源分配、驻留位图组读取，以及启动缓冲区清理。它们已链接生产程序；实际world／main位图调用仍需连接。

## 原函数与C实现

| 原运行地址 | 作用 | C接口 |
| --- | --- | --- |
| 29:D1C8..D1FC | 分配7000，写7A6A，解析并锁定，写797E指针 | `npe_rpg_map_buffer_allocate` |
| 31:8B80..8BC4 | 分配32768，写7A88，解析并锁定，写79A6，置7AA4为1，调用驻留组读取 | `npe_rpg_battle_resources_allocate` |
| 31:DEF6..DF8A | 捕获79BE长度，从79C2定位，读取到当前79A6 | `npe_rpg_battle_packed_load` |
| 33:1DF6..1E20 | 句柄0跳过，其它先解锁再释放 | `npe_resident_heap_cleanup` |
| 1B:2C46..2C86 | 依次读取574C／574E／5760／5626并清理 | `npe_resident_heap_launcher_cleanup` |

地图和战斗分配失败后仍解析句柄0并写入返回指针，沿原流程继续。驻留组读取只把32位长度低16位传给文件服务；目标指针在seek之后从当前RAM读取。读返回数量没有成为原调用者的成功门槛，短读和EOF保留未写缓存字节。清理不清零句柄槽和旧指针槽。

`NpeRpgResourceService`提供seek及客机地址写入回调。它沿用原文件位置和RAM所有者，没有从宿主位图或预定载荷地址直接取图。本次原《天降风云》驻留组位于文件偏移1625365，长度21746；这些值由原NPE表提供，未硬编码进生产服务。

## 实际分配重建暖态链

独立原执行及C均从原堆初始化开始，调用已确认的四块256字节[启动缓冲区分配前缀](np560_npe_heap_startup_owners.md)，随后分配7000和32768。七个堆头的地址及前六个锁定句柄与此前暖态捕获一致：

```text
51:4A40 -> 51:4B46 -> 51:4C4C -> 51:4D52
        -> 51:4E58 -> 51:69B6 -> 51:E9BC -> 0
```

这一链由原分配器和C分配器分别算出，测试不预置捕获堆头。分配完成后尾块标记为自由0；此前捕获的暖态尾块为8000，是后续位图分配失败后解析句柄0所形成的锁定状态，两个阶段不能混用。

## 对照结果

七组独立原函数及C比较完整128KiB RAM和18字节IO记录，共917504个RAM字节、126字节IO记录。覆盖完整读取、5字节短读、EOF、高位长度、位图分配失败、四块启动缓冲区清理，以及句柄槽均为0的清理。

执行前逐段核对原CODE总线，结束时检查栈恢复、返回到调用者、无缺失ROM和未支持指令。文件seek/read与分配失败提示由诊断适配器提供；这不是原NAND、真实失败提示、位图解码或LCD的验收。

必要回归通过：资源所有者6组、堆生命周期16组、位图装载及人物／库存／缓存绑定13组。五目标构建无警告，五项原资源哈希和NPESAV22保持。

```text
python npe-sdl/tests/reference_rpg_resource_lifecycle.py --qemu /path/to/qemu-system-dmcu
python npe-sdl/tests/generate_rpg_resource_lifecycle_fixture.py
cmake --build npe-sdl/build --config Release --target rpg-resource-lifecycle-probe npe-sdl
npe-sdl/build/rpg-resource-lifecycle-probe.exe tmp/tjfy-world-probe.npe
python tmp/finalize_rpg_resource_lifecycle.py
```

原结果：`tmp/rpg-resource-lifecycle-native/report.json`。汇总：`tmp/rpg-resource-lifecycle-report.json`。原源码：`tmp/rpg-resource-lifecycle-source.asm`。

## 剩余工作

- 连接真实world／main调用、运行时客机RAM与文件句柄，接通位图解码／LCD；当前生产绘图入口仍使用无状态位图接口。
- 连接其它RAM／控制字段及读取后的执行，继续图号31实际游戏路径、重复入战与完整启动退出生命周期。
- 核对真实文件IO／EOF和失败提示，继续其它记录写入者、原PCM、物理时基／输入与更多NPE动态适配。

整体仍粗估约80%。原机存档互通、正常属性完整通关和SDL全程实玩保持取消，保留模拟器自身存读档。

后续已补齐[客机RAM位图绘制](np560_npe_guest_bitmap_draw.md)，35组原函数完整显示缓冲与C一致；实际world／main管线和LCD提交仍需接通。

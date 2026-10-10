# 战斗位图装载流程与驻留堆

2026-10-08。纯C实现原45:8ED8..91CC的装载流程，调用已实现的驻留堆服务；十三组独立原函数对照通过。该服务已链接生产程序，后续已新增[九份人物记录地址与阶段绑定](np560_npe_resident_record_binding.md)，同十三组隔离对照通过；`world.c`／`main.c`绘图入口仍使用无状态位图接口，其它RAM及执行字段未全部接通。因此图号31的实际游戏路径仍未完成，工程粗估保持约80%。

## 已实现的流程

- 图号0～29从当前79A6指针取得LE24偏移，目录索引按16位计算后有符号扩展；图号0也沿原分支读取。
- 图号大于29从当前79CA及16位回绕的6字节目录位移定位；两次各请求3字节，第二次覆盖局部缓冲第2字节开始的位置。短读保留已有局部字节，后续读取沿提供者实际推进的文件位置继续。
- 请求长度加1后按原堆规则分配，将句柄写入7A88，再从当前7A88解析地址。分配返回0时仍解析0，随后继续读取。
- 依据当前79CE及有符号16位偏移定位，将请求字节数交给客机RAM写入服务。目标地址由堆状态计算，没有固定为51:E9C2。
- 绘图类型按0／1／其它值调用三个边界；绘图后分别重新读取7A88进行解锁及释放，不缓存之前的句柄。

接口为`NpeBattleBitmapService`和`npe_battle_bitmap_service`，实现位于`npe-sdl/battle_bitmap_service.inc`。文件位置、短读、RAM访问和绘图由显式回调提供；生产入口不依赖QEMU、WASM或Python。

## 独立对照的范围

QEMU运行未修改的原装载函数、LE24转换函数及分配／锁定／解锁／释放／整理服务，执行前通过CODE总线核对原PRG字节，结束时检查正常返回、栈恢复及无缺失ROM／未支持指令。诊断调用者提供初始内存；文件seek／read、三个绘图入口及分配失败提示由RAM适配器替代。文件供应字节来自当前原NPE，短读限制是显式诊断输入。

C探针使用相同输入，由文件当前位置实际读取NPE；逐字节比较`51:0000..53:0000`完整128KiB及84字节调用记录。十三组共比较1,703,936字节RAM和1,092字节记录。

| 对照 | 内容 |
| --- | --- |
| fresh-514／fresh-30 | 重置堆、196字节及零长度读取、普通释放 |
| warm-34／warm-31 | 捕获的暖态堆链，分配失败后解析0；2字节及35328字节写入 |
| short-payload | 正文请求196字节，只返回5字节 |
| short-first-directory | 首次目录只返回1字节，第二次沿当前位置读取，改变偏移与长度 |
| zero-second-directory | 第二次目录返回0，保留重叠局部字节 |
| draw-global-handle | 绘图边界将7A88改为255，验证后续重新读取 |
| resident-29／resident-0／resident-1 | 驻留目录末项、零索引负位移及首项；没有文件读取或释放 |
| file-displacement-wrap／file-negative-offset | 目录位移65538→2及有符号偏移／32位文件位置 |

warm-31在此隔离环境中算出目标51:E9C2，35328字节完整内容及六份战斗／三份地图148字节记录均与当前NPE一致，连续5058字节窗口也一致。这证明装载服务的地址和写入流程，不证明主程序的人物字段同步、位图解码、LCD或该游戏路径已通过。

暖态堆链另有七次原客机读取、42张LCD与基线相同的证据：四块256字节、一块7000字节、一块32768字节及锁定的零句柄尾块。这里只将其作为诊断初始输入，未硬编码进生产启动流程。7000／32768的调用源码已定位；四块256的启动来源仍需查清，不能由链形状推断其用途。

## 必要回归与复现

四目标构建无警告；新C探针退出0，相关SDL位图边界80提交及完整人物字节172提交均通过。自身NPESAV22以及原NPE、PRG、参考JS／WASM／包的五项哈希保持。

```text
python npe-sdl/tests/reference_battle_bitmap_service.py --qemu /path/to/qemu-system-dmcu
python npe-sdl/tests/generate_battle_bitmap_service_fixture.py
cmake --build npe-sdl/build --config Release --target battle-bitmap-service-probe npe-sdl
npe-sdl/build/battle-bitmap-service-probe.exe tmp/tjfy-world-probe.npe
```

原结果：`tmp/battle-bitmap-service-native/report.json`、十三份完整RAM／调用记录及CPU状态；汇总：`tmp/battle-bitmap-service-report.json`。原源码：`tmp/battle-bitmap-file-service-source.asm`和`tmp/npe-bitmap-u24-convert-source.asm`。第一次采样在观察工具65536字节上限处失败，改为分两段读取后取得完整结果，失败日志保留。

## 剩余工作

- 四块256字节启动分配所有者已确认；[RPG资源分配／读取及启动清理](np560_npe_rpg_resource_lifecycle.md)也已通过原内存对照。继续完整生命周期及实际主程序装载与绘图调用。
- 九份人物记录已有阶段绑定；库存与技能／道具缓存已有[资源地址绑定](np560_npe_resident_resource_binding.md)及原内存对照。继续其它RAM与控制字段，并接到实际主程序装载回调和绘图调用。
- 核对真实文件服务的EOF／短读及分配失败提示，接入客机位图解码和LCD；本次适配器不是这些原服务的验收。
- 完成图号31实际状态覆盖路径及其它驻留写入者；原PCM、物理时基／输入和更多NPE动态适配仍待完成。

原机存档互通、正常属性完整通关及SDL从头到尾实玩不在验收范围，保留模拟器自身存读档。

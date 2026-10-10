# NPC移动事件的驻留缓存

## 原固件规则

原地图装载器 `29:D8BC..DAE0` 处理目录组2。循环上限5；只有索引小于当前组计数时才读取记录，没有清空剩余槽。完整反汇编见 `tmp/party-cache-loader-original-source.asm`。

| 文件记录 | 原驻留地址（首槽） | 含义 |
| --- | --- | --- |
| +0，字 | 不写入服务槽 | 文件记录号，读取后丢弃 |
| +2，字 | 7652，字 | NPC编号 |
| +4，字节 | 不写入服务槽 | 保留字节，读取后丢弃 |
| +5，字节 | 7658，字 | 目标X，零扩展 |
| +6，字节 | 765A，字 | 目标Y，零扩展 |
| +7，字节 | 7654，字节 | 后续模式 |
| +8，字 | 7656，字 | 后续事件 |

槽跨度10字节，7655的未使用字节保持原值。C只建模服务读取的字段，不声称建模该未使用字节。驻留X／Y采用16位，不能把保留旧槽的字坐标缩成字节。

模式18在 `3C:8BAE..8BC8` 取事件号低8位，范围1～5，从7652表取参数；不按当前地图资源记录数判定槽是否存在。对应服务反汇编见 `tmp/npc-script-move-source.asm`。

## 原版动态证据

`tests/reference_npc_move_cache_load.mjs` 使用原参考运行时及原UI保存／加载。诊断只改待保存地图／位置、暖标题入口和驻留槽，以隔离装载器；不代替自然换图或全程游玩。

系统和标题入口分别加载地图2、27、1、18、107，对应0、1、2、3、4条移动记录，共10组。标题新游戏和空槽启动再各1组，共12组。结果在 `tmp/native-npc-move-cache-load.json`；日志分别是 `tmp/npc-move-cache-native.log`、`tmp/npc-move-cache-restart-native.log`。

每组给五槽设置不同值。高位坐标用于检测保留槽是否被截断；未执行这些越界目标。原装载器仅覆盖前N槽的服务字段，其余槽保持，包括诊断的坐标高字节。当前NPE没有五条移动记录的地图，不能把这些样本称为五条资源装载的动态验证。

该快照的首图前两槽来自NPE，其余为零；这是快照时的观测，不新增完整冷启动证据。暖新游戏／空槽重载首图前两项，其余三项保留。

## C实现及局部验证

`NpeWorld.npc_move_events[5]` 保存服务字段，不保存资源指针、文件记录号或未使用字节。地图装载、脚本传送及出口共用装载器，冷构造从零开始；暖存档恢复和暖新游戏从活动缓存开始，再仅覆盖当前NPE的前项。模式18读取驻留槽。

`npc-move-cache-probe` 对照12组原缓存字段，同时验证重复恢复、冷host解码、保留第五槽执行、低事件字节、加载后修改测试资源不影响服务、脚本传送、短旧布局及失败事务。缓存不序列化；只改变缓存的两个NPESAV16输出逐字节相同。

旧移动显示夹具在修改其测试内存中的资源后显式装入缓存；零距离克隆也独立装入诊断缓存，不允许生产执行时回读资源来迁就夹具。

定向构建7目标无warning／error：`tmp/npc-move-cache-final-build.log`。六项局部回归退出0：新缓存探针、脚本移动14条／87次实际main.c与SDL画面对照、队伍缓存、新游戏8条／24次提交、标题读档3次提交、开场短段。汇总 `tmp/npc-move-cache-regressions.json`，报告 `tmp/npc-move-cache-report.json`。这些画面沿用既有原版帧，本轮新增的是缓存RAM证据，不重复采样移动画面。

```powershell
& 'D:/Program Files/nodejs/node.exe' npe-sdl/tests/reference_npc_move_cache_load.mjs
python npe-sdl/tests/generate_npc_move_cache_fixture.py
cmake -S npe-sdl -B npe-sdl/build
cmake --build npe-sdl/build --target npc-move-cache-probe sdl-npc-script-move-probe npe-sdl -j6
& npe-sdl/build/npc-move-cache-probe.exe tmp/tjfy-world-probe.npe
```

## 尚未完成

缺失NPC／非法目标和冷零槽服务、其它后续模式与声音、不同NPC／图组和服务交错画面、真实硬件除零与时基／输入扫描、其它NPE机制仍需核对。缓存保留实现不等于整个模式18或模拟器完成。原NPE／PRG哈希和NPESAV16格式保持；没有重跑ctest、长综合路线或用户取消的正常属性通关／SDL全程实玩。

## 后续：返回调度与地图外目标

新增9条短路径39次实际C／SDL整屏对照，既有移动和驻留缓存回归通过；移除NPC脚本目标及主机读档的额外地图宽高限制。见[NPC移动返回与边界](np560_npe_npc_move_dispatch.md)。坐标255／步数回绕等边界继续待办。

## 后续：字／字节回绕与NPESAV17

有符号字差值、-32768绝对值回绕、NPC坐标255及字节循环回绕已接入，6条样本292次实际C／SDL匹配；NPESAV17区分255与无值并兼容旧档。见[NPC字／字节回绕](np560_npe_npc_move_byte.md)。非终止路径为运行片段，不计通关；其它服务边界继续待办。

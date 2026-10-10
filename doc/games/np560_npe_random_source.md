# NPE 使用的固件随机服务

`03:4124` 的 resident 跳表项在原始 source 映像 `20:4124`，解码为
`SJMP 33:6566`。该 resident 地址的 source 对应 `50:6566`，短函数
调用 `31:BB5A`；后者的 source 对应 `4E:BB5A..BBF6`。反汇编列表在
`tmp/mode-analysis/rng_*_v655_reachable.asm`。

`4E:BB66..BB7E` 从 resident 全局指针 `A2:29B0/29B2` 所指对象的
`+0x7E` 处取得 32 位状态。`4E:BB80..BBA6` 使用原厂 32 位乘法辅助函数
`33:07B0`，以 `0x514F111F` 乘当前状态并加 1，按低 32 位保存。
`4E:BBBE..BBEA` 重新读取状态的高字，屏蔽最高位后返回。因此一次调用为：

```c
state = state * UINT32_C(0x514f111f) + 1u;
return (state >> 16) & 0x7fffu;
```

C 运行时现在用这段递推驱动遇敌、编组、战斗和逃跑。真实《天降风云》
资源探针用固定状态检验编组及逃跑两条分支。原厂启动时对该全局状态的
赋值与所有调用次序仍待核对；C 版当前给新游戏使用固定初始状态，
`npe_world_seed` 可为测试设定状态。

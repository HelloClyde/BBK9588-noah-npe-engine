# NPE声音最后一字的字节交换更正

2026-10-07。修正纯C读取器对奇数帧音轨最后一个16位字的处理。生产程序仍为C11＋SDL2；修正的是有原PRG指令证据的压缩输入顺序，不代表缺失mask-ROM codec已完成。

## 错误与原指令依据

原 `2C:0E1E`（PRG源 `49:0E1E..0E5C`）接收far指针和word数量，对每个word执行 `ROR #8` 并写回。调用者 `25:E218` 以实际读取长度除二为word数量，交换完整的偶数长度输入。

旧C代码先把逻辑载荷长度限制为 `21×header_count`；对最后一个伙伴字节超出这个名义长度的位置，错误地读取原位置而不交换。旧Python完整音轨基线及候选DLL差分探针也先裁掉最后一个文件字节后再交换，因此它们彼此一致却没有遵循原输入规则。

奇数帧资源实际长度为 `4+21×count+1`。例如第33段末文件字为 `00 4F`：原交换后为 `4F 00`，声明的最后一个压缩帧字节是 `4F`。不能在交换前丢掉文件末尾的 `4F`，把原位置的 `00` 当作最后一个编码字节；尾部字节的含义必须在word交换后判断。

## 新证据

`tests/reference_audio_tail_swap.py` 使用同一原PRG与Windows QEMU，对当前NPE全部12段奇数帧资源的真实最后一字调用未修改原 `2C:0E1E`。客机指令设置输入与前后哨兵，宿主没有改写客机内存。12条路径输出word、前后哨兵、返回标记及原栈位置匹配，没有缺失ROM或不支持指令；不调用codec、完整EOF或NAND。

`audio_tail_reference.h` 根据这些实际原指令输出恢复完整word交换，再由独立候选位字段解析器生成各段末帧37字段。实际C读取器逐字段比较。修复前探针首先在第17段末帧字段34失败：C为2，正确交换后为3；失败日志保留在 `tmp/audio-tail-before-fix.log`。修复后12段全部通过。

## 修复与候选PCM回归

`audio.c` 使用已通过长度校验的完整偶数资源，所有编码字节统一取全流 `position XOR 1`，撤掉最后一字的例外。未更改候选解码算法、帧数或播放长度，也没有填零／裁字节。

Python完整音轨基线与DLL差分工具同步改成**交换完整输入后再切21字节帧**。`tests/verify_audio_tail_pcm.py` 保存并复核旧截短规则的历史SHA256，再比较修正输入下C与独立Python候选输出。22段、10,486帧逐段完整PCM长度及SHA256一致；12段奇数帧音轨的PCM哈希改变，10段偶数帧保持。旧与新哈希同时保存在 `tmp/audio-tail-pcm/report.json`，历史报告未被伪装为新证据。

另在隔离Unicorn中直接解释已有指定SHA256的候选DLL，按更正后的真实输入执行第31／33／34段共69帧。三个完整候选PCM与独立模型逐样本一致，没有宿主原生DLL执行。此处仍是候选x86算法参考，不能称为NP560原ROM PCM。

五个相关目标构建无警告；声音／问答声音／场景动画三个短测试退出0，Python第17／20／33段三个参数例通过。六个已知首帧参考、22段循环尾部及281次实际main.c／SDL场景画面对照保持。原NPE／PRG哈希与NPESAV17不变，没有全程通关重测。

## 证据与剩余范围

汇总：`tmp/audio-tail-report.json`。原输入证据：`tmp/audio-tail-swap-native/report.json` 与各 `.cpu.txt`；PCM：`tmp/audio-tail-pcm/report.json`、`tmp/audio-tail-candidate-report.json`；构建及回归：`audio-tail-build.log`、`audio-tail-python-regression.log`、`sdl-{sound,quiz-audio,scene-animation}-probe-audio-tail.log`。

本轮不证明原流1024字节补充、B816／B84E游标、原codec返回样本数或完整EOF派发。仍不采用固定少一帧规则；DAC渐变输出／指令时间、AD92、系统音量／冷GPIO／请求阻塞、物理时序和其它引擎／NPE适配继续。

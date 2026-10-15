# 来源与许可范围

## 9588 移植层

`bbk9588/` 中原创平台层、软键盘、构建/验证脚本和文档采用
[MIT](bbk9588/LICENSE)，作者 HelloClyde。`npe-sdl/main.c` 中新增的
`NPE_BBK9588` 集成代码也按 MIT 授权。MIT 不涵盖游戏截图中的游戏美术或文字。
9588 专用构建工作流及新增根目录文档同样采用 MIT。

## 上游引擎

来源：[HelloClyde/noah-npe-engine](https://github.com/HelloClyde/noah-npe-engine)，
固定基线 `b7568fbf0fbff22e1e8f9761555a0e4a57fc907d`，作者 helloclyde。
保留原始 Git 历史、源码和文档。同步代码保留上游 GPL-2.0-only 授权、战斗等待修复，并包含 v0.1.4 的行走时基修正；
其完整许可证与原始来源声明保留在
[NPE-ENGINE-GPL-2.0.txt](npe-sdl/third-party/NPE-ENGINE-GPL-2.0.txt) 和
[NPE-ENGINE-NOTICE.md](npe-sdl/third-party/NPE-ENGINE-NOTICE.md)，安装包也携带两份文本。
9588 原创移植层仍单独按 MIT 授权，不将整个引擎标记为 MIT。原机字库、游戏和截图
不适用引擎的 GPL 授权。上游说明见 [README-upstream.md](README-upstream.md)。

## 字体与第三方内容

- 应用图标 `bbk9588/assets/npe-icon.png` 原样复用
  [BBKH1-NPE](https://github.com/HelloClyde/BBKH1-NPE) 提交
  `c4a18c76cfc7a3ace4a6232ee4443beeecd34c1a` 的 imagegen 学习机插画，
  沿用该项目的素材来源说明，不纳入移植层 MIT 授权。详情见
  [图标来源](bbk9588/assets/README.md)。
- 使用 `npe-sdl/assets/gbk12-native.bin`、`gbk16-native.bin` 及
  `npe-sdl/native12_control_glyphs.inc`。它们由 NP560 V3.80 原机资源提取，
  来源、输入文件 SHA-256 和再生工具见
  [FONT-PROVENANCE.txt](npe-sdl/assets/FONT-PROVENANCE.txt)。这些原机字库的
  独立再分发许可未在上游明确授予，本仓库没有为它们补授 MIT 或 OFL。
- 历史 `gbk12.bin` 来自 Noto / Adobe，使用
  [SIL OFL 1.1](npe-sdl/assets/FONT-LICENSE.txt)。它不用于 9588 构建或安装包。
  安装包保留上游字体许可和来源文件；OFL 文件不能解释为原机字库的授权。
- 原桌面版本的 SDL2 许可保留在
  [SDL2-LICENSE.txt](npe-sdl/third-party/SDL2-LICENSE.txt)。9588 版本使用 SDK 实现
  必需的 SDL API 子集，不包含或加载 SDL2 DLL。
- `sdk/` 来自 [bbk9588-bda-sdk](https://github.com/HelloClyde/bbk9588-bda-sdk)，
  固定提交 `9c4cc3333e9e959f70797d2bcdc1d0760ddb4471`，采用其 Apache-2.0 许可。

游戏 NPE、完整固件、NAND 镜像和个人存档不包含在本仓库或安装包中。
真实运行截图只用于说明移植效果，游戏内容的权利归原权利人。

顶部 NOAH NPE 标识由内置 GPT Image 根据 H1 图标参考生成，原图、提示词要点和
SHA-256 见 [素材说明](bbk9588/assets/README.md)。原图随源码提供，RGB565 数据嵌入 BDA。

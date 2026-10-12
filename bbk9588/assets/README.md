# NPE 应用图标来源

`npe-icon.png` 原样复用 [HelloClyde/BBKH1-NPE](https://github.com/HelloClyde/BBKH1-NPE)
的 `assets/npe-icon.png`，固定提交
`c4a18c76cfc7a3ace4a6232ee4443beeecd34c1a`。
SHA-256：`3aca6f7ff17bbcda409956fe0455670bdd3f3e80f14f6101cbbb9ed7df088ca3`。

上游说明：原图由内置 imagegen 生成。主体参考诺亚舟 NP560+ 的银色机身、
绿色液晶屏、两侧方向控制和黑色滑出键盘；橙金色圆角底板参考 H1 高光立体图标风格。
照片仅用于观察设备特征，未直接复制照片或原机图标资源。
完整生成提示词与素材说明见
[H1 原始记录](https://github.com/HelloClyde/BBKH1-NPE/blob/c4a18c76cfc7a3ace4a6232ee4443beeecd34c1a/assets/README.md)。

本项目不修改原始 RGBA 图片，由固定 9588 SDK `build_icons()` 分别缩放为
80×80、80×80、54×54、58×58，编码为 VX / RGB565 little-endian。
透明像素转换为固件使用的品红色键，半透明边缘与黑色背景合成。
构建不需要 imagegen、Pillow 或网络图像服务。

图标沿用 H1 项目的素材来源状态，不属于本项目原创平台代码的 MIT 授权范围。
H1 源码的 GPL 许可也不能替代图片的独立许可说明。
参见 [H1 来源声明](https://github.com/HelloClyde/BBKH1-NPE/blob/c4a18c76cfc7a3ace4a6232ee4443beeecd34c1a/NOTICE.md)。


## 顶部 NOAH NPE 图片标识

`header-logo.png` 是用户选择的内置 GPT Image 生成图，保留原始 RGBA 像素。
生成于 2026-10-13，参考上面的 H1 学习机图标，字样为 NOAH NPE。
SHA-256：`8f3bb6f655592cff0fdd656e57adffe9d966df8093bf2b6a676f82ac4d6f5bdf`。
提示词要点：透明横向应用标识、橙金与银色学习机图形、白色 NOAH、金色 NPE，
在原生小屏顶部缩小后仍可辨认；不包含界面背景或模拟器画面。
使用内置 imagegen，不使用 API/CLI 后备路径。

`bbk9588/header_asset.py` 调用固定 SDK 的 PNG 解码与面积重采样器，排除透明留白，
按比例编译成 100×24 RGB565 数据，半透明边缘与顶部深色背景合成。
生成的 `header_rgb565.h` 嵌入程序；构建时从原图重新生成，不修改原图文件。
该图片替换旧的像素图标和文字；右侧音量状态独立绘制，游戏区域仍从 y=28 开始。
生成图片作为图片素材收录，不将原机品牌或 H1 参考素材另行声明为原创 MIT 代码。

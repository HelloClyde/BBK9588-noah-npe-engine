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

/**
 * 片段着色器 —— 混合章节：半透明混合
 *
 * ============================================================
 *  功能
 * ============================================================
 * 输出带有 alpha 通道的颜色，配合 glEnable(GL_BLEND) 实现半透明效果。
 *
 * ============================================================
 *  与 discard 的区别
 * ============================================================
 * discard   → 片段要么完全显示，要么完全丢弃（二值透明度）
 * blending  → 片段可以与背景"混合"（连续透明度，如 30% 透明）
 *
 * ============================================================
 *  混合方程（C++ 端设置）
 * ============================================================
 * glEnable(GL_BLEND);
 * glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 *
 * 结果颜色 = src_color * src_alpha + dst_color * (1 - src_alpha)
 *
 * ============================================================
 *  渲染顺序要求
 * ============================================================
 * ★ 使用混合的透明物体必须：
 *   1. 在所有不透明物体之后绘制
 *   2. 按从远到近排序（back-to-front）
 *   3. 绘制时关闭深度写入：glDepthMask(GL_FALSE)
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;

// 外部传入的颜色（用于纯色半透明窗口）
uniform vec4 blendColor;
// 是否使用纹理（false = 使用 blendColor 纯色）
uniform bool useTexture;

void main()
{
    if (useTexture)
    {
        FragColor = texture(texture1, TexCoord);
    }
    else
    {
        FragColor = blendColor;
    }
}

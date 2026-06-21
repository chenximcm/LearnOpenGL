/**
 * 片段着色器 —— 混合章节：Alpha 丢弃（Discard）
 *
 * ============================================================
 *  功能
 * ============================================================
 * 根据纹理的 alpha 通道丢弃透明片段。
 *
 * ============================================================
 *  discard 关键字
 * ============================================================
 * 当片段着色器中执行 discard 时，该片段会被立即丢弃，
 * 不会写入颜色缓冲区，也不会更新深度缓冲区。
 *
 * ★ 适用场景：只有"全透明"和"全不透明"两种状态的纹理
 *   例如：草叶、树叶、栅栏 — 这些纹理的边缘通常是透明像素
 *
 * ★ 不适用场景：半透明的窗户、玻璃 — 需要真正的混合（Blending）
 *
 * ============================================================
 *  边缘问题处理
 * ============================================================
 * 当纹理的环绕方式为 GL_REPEAT 时，纹理边缘会与对面边缘插值，
 * 导致透明区域周围出现"有色光环"。
 *
 * 解决方案：将纹理环绕方式设为 GL_CLAMP_TO_EDGE
 *   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 *   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;

void main()
{
    vec4 texColor = texture(texture1, TexCoord);

    // ★ 当 alpha 值低于阈值时丢弃该片段
    //   阈值为 0.1（而非严格的 0.0）以避免边缘混叠
    if (texColor.a < 0.1)
        discard;

    FragColor = texColor;
}

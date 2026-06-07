/**
 * 片段着色器 —— 双纹理混合版本
 *
 * ============================================================
 *  功能
 * ============================================================
 * 用 mix() 函数按比例混合两张纹理。
 *
 * ============================================================
 *  纹理单元（核心概念）
 * ============================================================
 * 这里的 texture1 和 texture2 并不是直接绑定到纹理对象，
 * 而是绑定到「纹理单元」（Texture Unit）。
 *
 * C++ 端需要做：
 *   shader.setInt("texture1", 0);  // 对应 GL_TEXTURE0
 *   shader.setInt("texture2", 1);  // 对应 GL_TEXTURE1
 *
 *   glActiveTexture(GL_TEXTURE0);
 *   glBindTexture(GL_TEXTURE_2D, tex1);
 *   glActiveTexture(GL_TEXTURE1);
 *   glBindTexture(GL_TEXTURE_2D, tex2);
 *
 * ============================================================
 *  mix() 混合函数
 * ============================================================
 * mix(a, b, t) = a * (1 - t) + b * t
 *
 * 这里 t = 0.2，意味着：
 *   80% 来自 texture1（容器贴图）
 *   20% 来自 texture2（笑脸贴图）
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;
uniform sampler2D texture2;

void main()
{
    FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);
}

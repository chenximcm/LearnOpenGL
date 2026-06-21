/**
 * 片段着色器 —— 帧缓冲章节：多效果后处理
 *
 * ============================================================
 *  效果模式（通过 uniform int effect 切换）
 * ============================================================
 *  0 = Normal     — 直通，不做处理
 *  1 = Inversion  — 反色 (1 - color)
 *  2 = Grayscale  — 灰度（感知加权：0.2126R + 0.7152G + 0.0722B）
 *  3 = Sharpen    — 锐化（3×3 卷积核）
 *  4 = Blur       — 模糊（高斯 3×3 卷积核）
 *  5 = Edge       — 边缘检测（3×3 拉普拉斯算子）
 *
 * @version 330 core
 */
#version 330 core

in  vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform int effect;           // 当前效果编号 0~5
uniform float time;           // 时间（未来扩展用）
uniform bool flipVertical;    // 垂直翻转纹理（演示 OpenGL 纹理坐标与屏幕坐标的差异）

// 像素偏移量（1/texture_width, 1/texture_height），由 C++ 传入
uniform float texOffsetX;
uniform float texOffsetY;

void main()
{
    // 翻转后处理：交换 uv.y，模拟 FBO 纹理坐标系 (0,0 = 左下)
    // 与屏幕坐标系 (0,0 = 左上) 之间的差异
    vec2 uv = flipVertical ? vec2(TexCoords.x, 1.0 - TexCoords.y) : TexCoords;
    vec3 color = texture(screenTexture, uv).rgb;

    // === 效果 0: Normal（直通） ===
    if (effect == 0)
    {
        FragColor = vec4(color, 1.0);
        return;
    }

    // === 效果 1: Inversion（反色） ===
    if (effect == 1)
    {
        FragColor = vec4(1.0 - color, 1.0);
        return;
    }

    // === 效果 2: Grayscale（感知灰度） ===
    if (effect == 2)
    {
        float gray = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
        FragColor = vec4(vec3(gray), 1.0);
        return;
    }

    // === 效果 3~5: 3×3 卷积核采样 ===
    //
    // 采样 9 个邻近像素：
    //   [0] 左上   [1] 中上   [2] 右上
    //   [3] 左中   [4] 中心   [5] 右中
    //   [6] 左下   [7] 中下   [8] 右下
    //
    vec2 offsets[9] = vec2[](
        vec2(-texOffsetX,  texOffsetY),  // 左上
        vec2( 0.0,          texOffsetY),  // 中上
        vec2( texOffsetX,  texOffsetY),  // 右上
        vec2(-texOffsetX,  0.0),         // 左中
        vec2( 0.0,         0.0),         // 中心
        vec2( texOffsetX,  0.0),         // 右中
        vec2(-texOffsetX, -texOffsetY),  // 左下
        vec2( 0.0,        -texOffsetY),  // 中下
        vec2( texOffsetX, -texOffsetY)   // 右下
    );

    vec3 samples[9];
    for (int i = 0; i < 9; i++)
        samples[i] = texture(screenTexture, uv + offsets[i]).rgb;

    // === 效果 3: Sharpen（锐化） ===
    if (effect == 3)
    {
        float kernel[9] = float[](
            -1, -1, -1,
            -1,  9, -1,
            -1, -1, -1
        );
        vec3 result = vec3(0.0);
        for (int i = 0; i < 9; i++)
            result += samples[i] * kernel[i];
        FragColor = vec4(result, 1.0);
        return;
    }

    // === 效果 4: Blur（高斯模糊） ===
    if (effect == 4)
    {
        float kernel[9] = float[](
            1.0/16.0, 2.0/16.0, 1.0/16.0,
            2.0/16.0, 4.0/16.0, 2.0/16.0,
            1.0/16.0, 2.0/16.0, 1.0/16.0
        );
        vec3 result = vec3(0.0);
        for (int i = 0; i < 9; i++)
            result += samples[i] * kernel[i];
        FragColor = vec4(result, 1.0);
        return;
    }

    // === 效果 5: Edge Detection（边缘检测） ===
    if (effect == 5)
    {
        float kernel[9] = float[](
             1,  1,  1,
             1, -8,  1,
             1,  1,  1
        );
        vec3 result = vec3(0.0);
        for (int i = 0; i < 9; i++)
            result += samples[i] * kernel[i];
        FragColor = vec4(result, 1.0);
        return;
    }

    // 兜底
    FragColor = vec4(color, 1.0);
}

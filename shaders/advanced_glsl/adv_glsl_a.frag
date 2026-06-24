/**
 * 高级GLSL（Advanced GLSL） — 片段着色器 A
 *
 * ★ 演示内容：gl_FragCoord 内建变量
 *   - gl_FragCoord.xy: 当前片段在窗口中的像素坐标（原点在左下角）
 *   - gl_FragCoord.z:  当前片段的深度值
 *   - 用 gl_FragCoord.x 创建水平渐变效果，屏幕左侧偏红、右侧偏暖
 *
 * ★ 接口块（Interface Block）—— 接收端
 *   - 块名 VS_OUT 必须与顶点着色器的输出块名匹配
 *   - 实例名可不同（vs_out → fs_in）
 */

#version 330 core

in VS_OUT
{
    vec3 color;
    vec3 fragPos;
} fs_in;

out vec4 FragColor;

uniform float uTime;

void main()
{
    // ★ gl_FragCoord: 窗口空间坐标
    //   x ∈ [0, screenWidth], y ∈ [0, screenHeight]
    //   归一化 x 坐标 → 0.0（左侧）到 1.0（右侧）
    //
    // 注意：硬编码了窗口宽度 1100，生产代码中应通过 uniform 传入
    float gradient = gl_FragCoord.x / 1100.0;

    vec3 baseColor = fs_in.color;

    // 右侧叠加暖色渐变，展示 gl_FragCoord 的实际效果
    vec3 tinted = mix(baseColor, vec3(1.0, 0.75, 0.4), gradient * 0.3);

    FragColor = vec4(tinted, 1.0);
}

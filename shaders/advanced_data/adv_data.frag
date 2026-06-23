/**
 * 高级数据（Advanced Data） — 片段着色器
 *
 * 功能：直接输出顶点着色器传来的插值颜色。
 *
 * 本章重点是 CPU 端缓冲区管理，着色器仅用于验证渲染结果。
 */

#version 330 core

in  vec3 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}

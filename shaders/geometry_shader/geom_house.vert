/**
 * 顶点着色器 —— 造房子（Building Houses）
 *
 * 将 2D 屏幕坐标和颜色传递给几何着色器。
 * 几何着色器将每个"点"图元展开为一个房子形状（三角形带）。
 *
 * 输入：2D 位置 Location(0), RGB 颜色 Location(1)
 * 输出：裁剪空间位置和颜色
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out VS_OUT {
    vec3 color;
} vs_out;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    vs_out.color = aColor;
}

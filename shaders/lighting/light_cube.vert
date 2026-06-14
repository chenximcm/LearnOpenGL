/**
 * 顶点着色器 —— 光源表示（Light Source Representation）
 *
 * ============================================================
 *  作用
 * ============================================================
 * 这个着色器用于绘制代表「光源位置」的小立方体。
 * 它只需要位置信息 + MVP 矩阵，不需要纹理坐标。
 *
 * 与主物体的着色器的区别：
 *   主物体着色器 → coordinate_system.vert（带纹理坐标）
 *   光源着色器   → light_cube.vert（只有位置 + MVP）
 *
 * 为什么分开？
 *   光源立方体不需要光照计算，它只是一个位置标记。
 *   它的颜色在片段着色器中直接用 uniform lightColor 设置。
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}

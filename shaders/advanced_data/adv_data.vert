/**
 * 高级数据（Advanced Data） — 顶点着色器
 *
 * 功能：接收顶点位置和颜色，通过 MVP 矩阵变换后传出给片段着色器。
 *
 * ★ 本章核心关注点不在着色器本身，而在于 CPU 端如何把数据传入 VBO。
 *   本着色器只是最简 MVP 管线，用于验证三种缓冲区策略都能正确渲染。
 *
 * 布局：
 *   location = 0: aPos   (vec3) — 顶点位置
 *   location = 1: aColor (vec3) — 顶点颜色（RGB）
 */

#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 vColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vColor = aColor;
}

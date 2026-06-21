/**
 * 顶点着色器 —— 模板测试：描边专用（纯 MVP 变换）
 *
 * ============================================================
 *  功能
 * ============================================================
 * 仅进行标准的 MVP 矩阵变换，不需要纹理坐标。
 * 搭配 stencil_outline.frag 使用，用于渲染放大的描边物体。
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

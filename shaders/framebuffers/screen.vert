/**
 * 顶点着色器 —— 帧缓冲章节：全屏四边形
 *
 * 把一张纹理渲染到整个屏幕（后处理的核心）。
 * 顶点使用 NDC 坐标（无需 MVP 变换），直传纹理坐标。
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoords;

void main()
{
    // NDC 坐标直接作为裁剪空间坐标，不需要任何矩阵
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoords = aTexCoord;
}

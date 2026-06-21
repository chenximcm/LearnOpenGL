/**
 * 顶点着色器 —— 混合章节：透明物体专用
 *
 * ============================================================
 *  功能
 * ============================================================
 * 与坐标系统顶点着色器相同：标准 MVP 变换 + 纹理坐标直传。
 * 搭配 blend_transparent.frag 使用，用于渲染半透明的窗户/玻璃。
 *
 * 混合时深度写入需要关闭（glDepthMask(GL_FALSE)），
 * 透明物体必须按从远到近的顺序绘制。
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}

/**
 * 顶点着色器 —— 爆破物体（Exploding Objects）
 *
 * 将世界空间位置和纹理坐标传递给几何着色器。
 * 几何着色器负责沿三角形面法线方向位移顶点。
 *
 * 输入：模型顶点的 Position(0), Normal(1), TexCoords(2)
 * 输出：世界空间位置和纹理坐标
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
    vec2 texCoords;
} vs_out;

uniform mat4 model;

void main()
{
    // 变换到世界空间（几何着色器需要世界空间坐标计算面法线）
    gl_Position = model * vec4(aPos, 1.0);
    vs_out.texCoords = aTexCoords;
}

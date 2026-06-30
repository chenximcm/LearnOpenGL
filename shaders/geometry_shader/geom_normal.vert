/**
 * 顶点着色器 —— 法线可视化（Normal Visualization）
 *
 * 将顶点位置和法线变换到观察空间，传递给几何着色器。
 * 几何着色器将在每个顶点处生成法线方向的线段。
 *
 * 输入：模型顶点的 Position(0), Normal(1)
 * 输出：观察空间的位置和法线
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out VS_OUT {
    vec3 normal;       // 观察空间中的法线（已做非均匀缩放校正）
} vs_out;

uniform mat4 view;
uniform mat4 model;

void main()
{
    // 1. 变换顶点到观察空间
    gl_Position = view * model * vec4(aPos, 1.0);

    // 2. 变换法线到观察空间（使用法线矩阵）
    //    view * model 的非均匀缩放需要法线矩阵校正
    mat3 normalMatrix = mat3(transpose(inverse(view * model)));
    vs_out.normal = normalize(vec3(vec4(normalMatrix * aNormal, 0.0)));
}

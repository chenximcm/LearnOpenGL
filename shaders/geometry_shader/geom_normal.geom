/**
 * 几何着色器 —— 法线可视化（Normal Visualization）
 *
 * 输入：三角形（triangles），每个顶点带有观察空间法线
 * 输出：线段（line_strip），每个顶点生成一条法线线段
 *
 * 原理：
 *   对三角形的每个顶点，从该顶点位置沿法线方向画一条短线段，
 *   直观展示法线的方向和分布。
 *
 * @version 330 core
 */
#version 330 core

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in VS_OUT {
    vec3 normal;
} gs_in[];

uniform mat4 projection;

const float MAGNITUDE = 0.2;   // 法线显示长度（世界/观察空间单位）

void GenerateLine(int index)
{
    // 法线起点：顶点位置
    gl_Position = projection * gl_in[index].gl_Position;
    EmitVertex();

    // 法线终点：顶点位置 + 法线方向 × 长度
    gl_Position = projection * (gl_in[index].gl_Position +
                                vec4(gs_in[index].normal, 0.0) * MAGNITUDE);
    EmitVertex();

    EndPrimitive();   // 结束当前线段
}

void main()
{
    // 为三角形的 3 个顶点各生成一条法线线段
    GenerateLine(0);  // 顶点 A 的法线
    GenerateLine(1);  // 顶点 B 的法线
    GenerateLine(2);  // 顶点 C 的法线
}

/**
 * 几何着色器 —— 爆破物体（Exploding Objects）
 *
 * 输入：三角形（triangles）
 * 输出：三角形带（triangle_strip），顶点沿面法线方向位移
 *
 * 原理：
 *   1. 计算三角形面的法线（世界空间）
 *   2. 每个顶点沿面法线方向位移
 *   3. 位移量随时间正弦变化，产生"呼吸式爆破"效果
 *
 * @version 330 core
 */
#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in VS_OUT {
    vec2 texCoords;
} gs_in[];

out vec2 TexCoords;    // 传递给片段着色器

uniform mat4  view;
uniform mat4  projection;
uniform float time;      // 动态时间（用于呼吸式爆破）

/**
 * 计算三角形面的法线（世界空间）
 *
 * 使用三角形两条边的叉积：
 *   normal = cross(edge1, edge2)
 *
 * gl_in[].gl_Position 存储的是 VS 输出的世界空间位置。
 */
vec3 GetNormal()
{
    vec3 a = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 b = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
    return normalize(cross(a, b));
}

/**
 * 将顶点沿面法线方向位移
 *
 * @param position  顶点在世界空间中的位置
 * @param normal    面法线（世界空间）
 * @return          位移后的世界空间位置
 */
vec4 explode(vec4 position, vec3 normal)
{
    float magnitude = 0.8;   // 爆破强度
    // 使用 sin(time) 实现呼吸式效果：
    //   time=0     → direction = normal * 0.5 * 0.8   （中等位移）
    //   time=π/2   → direction = normal * 1.0 * 0.8   （最大位移）
    //   time=π     → direction = normal * 0.5 * 0.8   （回到中等）
    //   time=3π/2  → direction = normal * 0.0         （回到原位）
    vec3 direction = normal * ((sin(time) + 1.0) / 2.0) * magnitude;
    return position + vec4(direction, 0.0);
}

void main()
{
    // 计算面法线（3个顶点共享同一个面法线）
    vec3 normal = GetNormal();

    // 对每个顶点沿面法线位移，并应用 MVP 变换
    gl_Position = projection * view * explode(gl_in[0].gl_Position, normal);
    TexCoords   = gs_in[0].texCoords;
    EmitVertex();

    gl_Position = projection * view * explode(gl_in[1].gl_Position, normal);
    TexCoords   = gs_in[1].texCoords;
    EmitVertex();

    gl_Position = projection * view * explode(gl_in[2].gl_Position, normal);
    TexCoords   = gs_in[2].texCoords;
    EmitVertex();

    EndPrimitive();
}

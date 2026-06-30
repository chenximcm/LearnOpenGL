/**
 * 几何着色器 —— 造房子（Building Houses）
 *
 * 输入：点（points）—— 1 个顶点 / 图元
 * 输出：三角形带（triangle_strip）—— 5 个顶点 → 3 个三角形
 *
 * 每个输入点的位置决定房子的位置。
 * 房子形状：
 *       5(屋顶)
 *       /\
 *      /  \
 *    3/____\4
 *    |      |
 *    |______|
 *    1      2
 *
 * 三角形：(1,2,3) (2,3,4) (3,4,5)
 *
 * 屋顶顶点 (5) 使用白色（像雪），其余顶点使用输入点的颜色。
 *
 * @version 330 core
 */
#version 330 core

layout (points) in;
layout (triangle_strip, max_vertices = 5) out;

in VS_OUT {
    vec3 color;
} gs_in[];

out vec3 fColor;

void build_house(vec4 position)
{
    // ---- 房子主体（输入颜色） ----
    fColor = gs_in[0].color;

    // 1: 左下
    gl_Position = position + vec4(-0.2, -0.2, 0.0, 0.0);
    EmitVertex();

    // 2: 右下
    gl_Position = position + vec4( 0.2, -0.2, 0.0, 0.0);
    EmitVertex();

    // 3: 左上
    gl_Position = position + vec4(-0.2,  0.2, 0.0, 0.0);
    EmitVertex();

    // 4: 右上
    gl_Position = position + vec4( 0.2,  0.2, 0.0, 0.0);
    EmitVertex();

    // ---- 屋顶尖顶（白色 = 雪） ----
    gl_Position = position + vec4( 0.0,  0.4, 0.0, 0.0);
    fColor = vec3(1.0, 1.0, 1.0);      // 白色屋顶
    EmitVertex();

    EndPrimitive();
}

void main()
{
    // 在输入点的位置建造房子
    build_house(gl_in[0].gl_Position);
}

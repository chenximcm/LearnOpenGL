/**
 * 高级GLSL（Advanced GLSL） — 片段着色器 B
 *
 * ★ 演示内容：gl_FrontFacing 内建变量
 *   - gl_FrontFacing: bool 类型，当前片段属于正面（true）还是背面（false）
 *   - 正面：显示原始颜色
 *   - 背面：高亮显示为亮黄色，一眼就能看出多边形的朝向
 *
 *   需要搭配 glDisable(GL_CULL_FACE) 才能看到背面效果。
 *
 * ★ 接口块（Interface Block）—— 接收端
 *   - 块名 VS_OUT 必须与顶点着色器的输出块名匹配
 */

#version 330 core

in VS_OUT
{
    vec3 color;
    vec3 fragPos;
} fs_in;

out vec4 FragColor;

void main()
{
    // ★ gl_FrontFacing: 判断当前片段是否属于正面
    //   正面 → 正常颜色
    //   背面 → 亮黄色高亮（可一眼看到背面的三角形）
    if (gl_FrontFacing)
    {
        // 正面：原始颜色
        FragColor = vec4(fs_in.color, 1.0);
    }
    else
    {
        // 背面：亮黄色（演示 gl_FrontFacing 的效果）
        FragColor = vec4(1.0, 1.0, 0.3, 1.0);
    }
}

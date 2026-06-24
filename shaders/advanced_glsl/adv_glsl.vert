/**
 * 高级GLSL（Advanced GLSL） — 顶点着色器
 *
 * 本章核心演示：
 *   ① Uniform 缓冲对象（UBO）—— 在多个着色器程序间共享 projection / view 矩阵
 *   ② 接口块（Interface Block）—— 用 VS_OUT / FS_IN 块组织顶点输出
 *
 * 布局：
 *   location = 0: aPos   (vec3) — 顶点位置
 *   location = 1: aColor (vec3) — 顶点颜色（RGB）
 *
 * UBO（std140 布局）：
 *   | offset 0  | mat4 projection (64 bytes)
 *   | offset 64 | mat4 view       (64 bytes)
 *   总计: 128 bytes
 */

#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

// ============================================================
// ★ Uniform 缓冲对象（UBO）
//   - 绑定到 binding point 0
//   - 包含 projection + view 矩阵，多个着色器程序共享
//   - std140 布局保证 offset 可预测
// ============================================================
layout (std140) uniform Matrices
{
    mat4 projection;
    mat4 view;
};

// model 矩阵每个物体单独设置（不在 UBO 中）
uniform mat4 model;

// ============================================================
// ★ 接口块（Interface Block）
//   - 用命名块组织输出变量，替代零散的 out 声明
//   - 块名（VS_OUT）必须在下一个着色器中匹配
//   - 实例名（vs_out）可任意命名
// ============================================================
out VS_OUT
{
    vec3 color;      // 顶点颜色（插值传递）
    vec3 fragPos;    // 世界空间片段位置
} vs_out;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    vs_out.color   = aColor;
    vs_out.fragPos = vec3(worldPos);
    gl_Position    = projection * view * worldPos;
}

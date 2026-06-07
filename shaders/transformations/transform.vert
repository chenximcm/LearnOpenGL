/**
 * 顶点着色器 —— 变换章节
 *
 * ============================================================
 *  新增功能：变换矩阵
 * ============================================================
 * 相比纹理章节的顶点着色器，这里新增了一个 uniform mat4 变换矩阵。
 *
 *   gl_Position = transform * vec4(aPos, 1.0);
 *
 * 变换矩阵由 GLM 库在 C++ 端生成，包含了平移、旋转、缩放操作，
 * 通过 uniform 传递给着色器，在 GPU 上完成顶点变换。
 *
 * ============================================================
 *  为什么在顶点着色器中做变换？
 * ============================================================
 * 如果变换在 CPU 端做（逐顶点修改数据），每帧需要：
 *   1. 遍历所有顶点
 *   2. 乘以变换矩阵
 *   3. 重新上传到 GPU 显存（glBufferData / glBufferSubData）
 *
 * 如果在 GPU 端做（本例中的 uniform mat4 方式）：
 *   1. CPU 只计算一个 4×4 矩阵（16 个 float）
 *   2. 通过 glUniformMatrix4fv 上传（16 个 float = 64 字节）
 *   3. GPU 为每个顶点并行计算变换
 *
 * 即使是 100 万个顶点的模型，CPU 也只需上传 64 字节！
 * 这就是「尽量在 GPU 上做计算」的现代图形编程原则。
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 transform;

void main()
{
    gl_Position = transform * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}

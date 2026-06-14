/**
 * 顶点着色器 —— 基础光照（Basic Lighting）
 *
 * ============================================================
 *  核心变化：引入法线（Normal）
 * ============================================================
 * 相比颜色章节，这里顶点着色器多了两项输出：
 *
 *   ① FragPos —— 片段在世界空间中的位置
 *       用于片段着色器计算光线方向（lightDir）和视线方向（viewDir）
 *
 *   ② Normal  —— 变换到世界空间的法线向量
 *       用于片段着色器计算漫反射（diffuse）和镜面反射（specular）
 *
 * 法线变换：
 *   法线不能直接用 model 矩阵变换（非均匀缩放会破坏法线方向），
 *   需要用「法线矩阵」= transpose(inverse(model)) 的前 3×3 部分。
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;      // 片段在世界空间中的位置
out vec3 Normal;       // 世界空间中的法线方向
out vec2 TexCoord;     // 纹理坐标

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 1. 计算世界空间中的片段位置（用于光照计算）
    FragPos = vec3(model * vec4(aPos, 1.0));

    // 2. 用法线矩阵将法线变换到世界空间
    //    transpose(inverse(mat3(model))) 处理非均匀缩放
    Normal = mat3(transpose(inverse(model))) * aNormal;

    // 3. 传递纹理坐标
    TexCoord = aTexCoord;

    // 4. 标准 MVP 变换
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}

/**
 * 顶点着色器 —— 坐标系统章节
 *
 * ============================================================
 *  核心变化：MVP 矩阵
 * ============================================================
 * 相比变换章节的单个 transform 矩阵，这里拆分为三个矩阵：
 *
 *   gl_Position = projection * view * model * vec4(aPos, 1.0);
 *
 * 分别对应三个不同的坐标空间变换：
 *
 *   ① model      —— 模型矩阵（局部 → 世界）
 *      将物体从自己的局部坐标系变换到世界坐标系。
 *      包含平移、旋转、缩放，决定了物体在世界中的位置/朝向/大小。
 *      每个物体可以有自己独立的 model 矩阵（不同的位置/旋转）。
 *
 *   ② view       —— 视图矩阵（世界 → 视图）
 *      模拟「相机」的变换，将世界坐标变换到以相机为原点的坐标系。
 *      相当于把相机移到原点，并让物体的坐标相对于相机重新计算。
 *      整个场景共享同一个 view 矩阵。
 *
 *   ③ projection —— 投影矩阵（视图 → 裁剪）
 *      将视锥体（Frustum）映射到标准化设备坐标 [-1, 1]。
 *      透视投影（Perspective）产生近大远小的效果。
 *      正交投影（Orthographic）保持物体大小不变。
 *      整个场景共享同一个 projection 矩阵。
 *
 * ============================================================
 *  为什么拆成三个矩阵？
 * ============================================================
 * 如果只用一个 transform 矩阵，每帧需要重新计算所有物体的完整变换。
 * 拆成三个后：
 *   - projection 矩阵只需在窗口大小变化时重新计算（或不变）
 *   - view 矩阵只在相机移动时重新计算
 *   - model 矩阵每帧为每个物体单独计算（最频繁变化）
 *   分工明确，计算量大幅减少。
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

// ==================== MVP 矩阵 ====================
// 从右到左依次应用：model → view → projection
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 标准 MVP 变换管线
    gl_Position = projection * view * model * vec4(aPos, 1.0);

    // 纹理坐标直传片段着色器
    TexCoord = aTexCoord;
}

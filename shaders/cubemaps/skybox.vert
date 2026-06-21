/**
 * 天空盒顶点着色器
 *
 * ★ 关键技巧：移除 view 矩阵的平移分量
 *   将 view 矩阵转为 mat3 再转回 mat4，平移向量变为 0
 *   这样天空盒始终以摄像机为中心，不会"飞到"远处
 *
 *   输入顶点 = 立方体坐标（-1 ~ 1），输出 = cubemap 采样的方向向量
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    // ★ 移除平移：mat3(view) → mat4 丢弃了平移列
    //   等价于 view 矩阵但平移部分为 (0,0,0)
    mat4 viewNoTranslation = mat4(mat3(view));

    vec4 clipPos = projection * viewNoTranslation * vec4(aPos, 1.0);

    // ★ z = w：始终渲染在远平面（最大深度），这样天空盒在所有物体后面
    gl_Position = clipPos.xyww;

    // 输出顶点位置作为 cubemap 采样的方向向量
    TexCoords = aPos;
}

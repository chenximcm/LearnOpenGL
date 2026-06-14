/**
 * 片段着色器 —— 光照 / 颜色（Colors）
 *
 * ============================================================
 *  核心概念：颜色 = 光源颜色 × 物体颜色
 * ============================================================
 *
 * 在计算机图形学中，我们看到的物体颜色由两部分决定：
 *
 *   感知颜色 = 光源颜色 × 物体反射颜色
 *
 * 这是逐分量乘法（component-wise multiplication）：
 *
 *   result.r = light.r * object.r
 *   result.g = light.g * object.g
 *   result.b = light.b * object.b
 *
 * 例如：
 *   白色光  (1.0, 1.0, 1.0) × 红色物体 (1.0, 0.0, 0.0) = 红色 (1.0, 0.0, 0.0)
 *   白色光  (1.0, 1.0, 1.0) × 珊瑚色   (0.5, 0.0, 0.0) = 珊瑚色
 *   绿色光  (0.0, 1.0, 0.0) × 珊瑚色   (0.5, 0.0, 0.0) = 黑色 (0.0, 0.0, 0.0)
 *      → 物体只能反射红色分量，但绿色光源中没有红色，所以是黑色
 *
 * 下个章节（基础光照）将在这个概念基础上引入
 * 环境光（Ambient）、漫反射（Diffuse）、镜面反射（Specular）。
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

uniform vec3 objectColor;   // 物体的「固有颜色」（反射什么颜色的光）
uniform vec3 lightColor;    // 光源的颜色（发出什么颜色的光）

void main()
{
    // 逐分量乘法：光源颜色 × 物体颜色 = 人眼感知的颜色
    vec3 result = lightColor * objectColor;
    FragColor = vec4(result, 1.0);
}

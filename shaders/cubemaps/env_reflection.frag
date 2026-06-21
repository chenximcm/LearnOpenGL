/**
 * 环境映射 — 反射（Reflection）
 *
 * 公式：reflect(视线方向, 法线)
 *   视线方向 = normalize(片段位置 - 摄像机位置)
 *   反射向量 = reflect(视线方向, 法线)  ← 用这个采样 cubemap
 *
 * @version 330 core
 */
#version 330 core

in  vec3 FragPos;
in  vec3 Normal;
out vec4 FragColor;

uniform vec3      cameraPos;
uniform samplerCube skybox;
uniform int       envMode;  // 0=Reflection, 1=Refraction

void main()
{
    vec3 I = normalize(FragPos - cameraPos);   // 视线方向（片段 → 摄像机）
    vec3 R = reflect(I, normalize(Normal));     // 反射方向
    FragColor = vec4(texture(skybox, R).rgb, 1.0);
}

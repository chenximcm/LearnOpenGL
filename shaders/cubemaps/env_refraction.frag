/**
 * 环境映射 — 折射（Refraction）
 *
 * 公式：refract(视线方向, 法线, 折射率比值)
 *   视线方向 = normalize(片段位置 - 摄像机位置)
 *   折射向量 = refract(视线方向, 法线, ratio)
 *
 *   折射率比值 = 空气折射率 / 材质折射率
 *     空气 ≈ 1.00，玻璃 ≈ 1.52  →  ratio = 1.00 / 1.52
 *
 * @version 330 core
 */
#version 330 core

in  vec3 FragPos;
in  vec3 Normal;
out vec4 FragColor;

uniform vec3      cameraPos;
uniform samplerCube skybox;
uniform float     refractiveRatio;  // 默认 1.00 / 1.52 ≈ 0.658

void main()
{
    vec3 I = normalize(FragPos - cameraPos);    // 视线方向
    vec3 R = refract(I, normalize(Normal), refractiveRatio);  // 折射方向
    FragColor = vec4(texture(skybox, R).rgb, 1.0);
}

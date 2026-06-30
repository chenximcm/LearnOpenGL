/**
 * 片段着色器 —— 爆破物体
 *
 * 简化的纹理采样输出，配合爆破几何着色器使用。
 * 使用环境光 + 简易漫反射来保留立体感。
 *
 * @version 330 core
 */
#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D texture_diffuse1;   // 模型漫反射纹理
uniform vec3      lightDir;           // 世界空间光源方向（简化）
uniform vec3      viewPos;            // 摄像机位置

void main()
{
    vec3 color = texture(texture_diffuse1, TexCoords).rgb;

    // 简化光照：环境光 + 漫反射
    // 在没有法线信息的情况下，用固定亮度保留基本立体感
    vec3 ambient = color * 0.3;

    // 简单方向光（假设法线大致朝外）
    float diff = max(dot(vec3(0.0, 1.0, 0.0), normalize(lightDir)), 0.0);
    vec3 diffuse = color * diff * 0.7;

    FragColor = vec4(ambient + diffuse, 1.0);
}

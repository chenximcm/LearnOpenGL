/**
 * 天空盒片段着色器
 *
 * 用从顶点着色器传来的 3D 方向向量采样立方体贴图
 *
 * @version 330 core
 */
#version 330 core

in  vec3 TexCoords;
out vec4 FragColor;

uniform samplerCube skybox;

void main()
{
    FragColor = texture(skybox, TexCoords);
}

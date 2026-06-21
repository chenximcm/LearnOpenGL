/**
 * 环境映射顶点着色器（反射 / 折射共用）
 *
 * 输出：
 *   - FragPos   世界空间片段位置（用于计算视线方向）
 *   - Normal    世界空间法线（用于反射/折射计算）
 *
 * @version 330 core
 */
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal  = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}

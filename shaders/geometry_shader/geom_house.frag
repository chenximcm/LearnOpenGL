/**
 * 片段着色器 —— 造房子
 *
 * 直接输出几何着色器传递的颜色。
 *
 * @version 330 core
 */
#version 330 core

in vec3 fColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(fColor, 1.0);
}

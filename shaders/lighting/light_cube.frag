/**
 * 片段着色器 —— 光源表示
 *
 * ============================================================
 *  作用
 * ============================================================
 * 以纯色（光源颜色）绘制代表光源位置的小立方体。
 * 这个立方体本身「发光」，所以不受光照影响，
 * 直接输出 uniform lightColor 作为颜色。
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

uniform vec3 lightColor;

void main()
{
    FragColor = vec4(lightColor, 1.0);
}

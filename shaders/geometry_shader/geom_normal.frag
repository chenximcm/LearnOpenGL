/**
 * 片段着色器 —— 法线可视化
 *
 * 将所有法线线段渲染为黄色，便于观察。
 *
 * @version 330 core
 */
#version 330 core
out vec4 FragColor;

void main()
{
    // 黄色法线 —— 在大多数场景颜色中都很醒目
    FragColor = vec4(1.0, 1.0, 0.0, 1.0);
}

#version 330 core
out vec4 FragColor;

uniform float near;
uniform float far;
uniform bool  linearize;

void main()
{
    // gl_FragCoord.z 是非线性的深度值（NDC 映射到 [0,1]）
    float depth = gl_FragCoord.z;

    if (linearize)
    {
        // 将非线性深度转换为线性深度
        // 参考：https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/01%20Depth%20testing/
        float ndc = depth * 2.0 - 1.0;
        float linearDepth = (2.0 * near * far) / (far + near - ndc * (far - near));
        depth = linearDepth / far;  // 归一化到 [0, 1]
    }

    // 输出灰度图：越近越暗，越远越亮
    FragColor = vec4(vec3(1.0 - depth), 1.0);
}

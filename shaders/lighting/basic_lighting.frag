/**
 * 片段着色器 —— 基础光照 / Phong 光照模型
 *
 * ============================================================
 *  Phong 光照模型 = 环境光 + 漫反射 + 镜面反射
 * ============================================================
 *
 *  ① Ambient（环境光）
 *     模拟场景中无处不在的微弱光线。
 *     确保物体的背面不会完全漆黑。
 *     计算公式：ambient = ambientStrength × lightColor
 *
 *  ② Diffuse（漫反射）
 *     模拟光线照射到粗糙表面后的均匀反射。
 *     光线越垂直于表面，表面越亮。
 *     计算公式：diffuse = max(dot(normal, lightDir), 0) × lightColor
 *
 *  ③ Specular（镜面反射）
 *     模拟光滑表面的高光反射（如金属、塑料）。
 *     视线越接近反射方向，高光越亮。
 *     计算公式：specular = pow(max(dot(viewDir, reflectDir), 0), shininess)
 *                      × specularStrength × lightColor
 *
 * ============================================================
 *  最终颜色
 * ============================================================
 *   result = (ambient + diffuse + specular) × objectColor
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

in vec3 FragPos;       // 片段在世界空间中的位置
in vec3 Normal;        // 世界空间中的法线方向
in vec2 TexCoord;      // 纹理坐标

// ---- 光源参数 ----
uniform vec3 lightPos;         // 光源在世界空间中的位置
uniform vec3 lightColor;       // 光源颜色
uniform vec3 viewPos;          // 摄像机位置（用于计算视线方向）

// ---- 物体材质参数 ----
uniform vec3 objectColor;      // 物体固有颜色
uniform sampler2D texture1;    // 纹理
uniform bool  useTexture;      // 是否启用纹理

// ---- Phong 模型参数 ----
uniform float ambientStrength; // 环境光强度 [0.0, 1.0]  — 建议 0.1
uniform float specularStrength;// 镜面反射强度 [0.0, 1.0] — 建议 0.5
uniform float shininess;       // 反光度 [1, 256]         — 建议 32

void main()
{
    // ============================================================
    // ① 环境光（Ambient）
    // ============================================================
    // 环境光 = 光源颜色 × 环境光强度
    // 它提供一个基础亮度，防止背光面完全黑暗
    vec3 ambient = ambientStrength * lightColor;

    // ============================================================
    // ② 漫反射（Diffuse）
    // ============================================================
    // 关键概念：法线与光线方向的夹角决定漫反射强度
    //
    //   dot(normal, lightDir) > 0  → 表面面向光源（亮）
    //   dot(normal, lightDir) = 0  → 表面与光线平行（暗）
    //   dot(normal, lightDir) < 0  → 表面背对光源（无光）
    //
    // 用 max(dot(...), 0) 确保负值被截断为 0（背光面不产生漫反射）

    vec3 norm = normalize(Normal);                         // 确保法线是单位向量
    vec3 lightDir = normalize(lightPos - FragPos);         // 从片段指向光源的方向
    float diff = max(dot(norm, lightDir), 0.0);            // 漫反射强度系数
    vec3 diffuse = diff * lightColor;                      // 漫反射分量

    // ============================================================
    // ③ 镜面反射（Specular）
    // ============================================================
    // 关键概念：视线方向与反射方向的夹角决定高光强度
    //
    //   reflect(lightDir, norm) 计算光线在法线处的反射方向
    //   注意 lightDir 是从片段指向光源，取反后传入 reflect
    //
    //   shininess（反光度）控制高光范围：
    //     小值（如 8）→ 高光范围大（粗糙表面）
    //     大值（如 256）→ 高光范围小（光滑表面）

    vec3 viewDir = normalize(viewPos - FragPos);           // 从片段指向摄像机
    vec3 reflectDir = reflect(-lightDir, norm);            // 反射方向（lightDir 取反）
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess); // 镜面强度
    vec3 specular = specularStrength * spec * lightColor;  // 镜面反射分量

    // ============================================================
    // 合成最终颜色
    // ============================================================
    // Phong 模型：result = (ambient + diffuse + specular) × objectColor
    vec3 lighting = (ambient + diffuse + specular) * objectColor;

    // 可选的纹理叠加
    if (useTexture)
    {
        vec4 texColor = texture(texture1, TexCoord);
        FragColor = vec4(lighting * texColor.rgb, 1.0);
    }
    else
    {
        FragColor = vec4(lighting, 1.0);
    }
}

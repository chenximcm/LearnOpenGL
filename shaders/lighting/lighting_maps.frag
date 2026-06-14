/**
 * 片段着色器 —— 光照贴图（Lighting Maps）
 *
 * ============================================================
 *  核心变化：用纹理替代 uniform 控制材质属性
 * ============================================================
 *
 * 材质章节中，material.diffuse 和 material.specular 是统一颜色，
 * 整个物体使用相同的材质参数。
 *
 * 光照贴图章节用两种纹理替代：
 *
 *   ① Diffuse Map（漫反射贴图）
 *      材质漫反射颜色来自纹理采样。
 *      → 这就是平时说的「给物体贴图」，控制物体不同区域的颜色。
 *
 *   ② Specular Map（高光贴图）
 *      材质镜面反射颜色来自纹理采样。
 *      → 白色 (1,1,1)：该区域高光强（金属、光滑表面）
 *      → 黑色 (0,0,0)：该区域无高光（木头、粗糙表面）
 *
 * 这样同一个物体可以有不同的光照响应：
 *   木箱的木板部分：漫反射有木纹，高光为黑（无高光）
 *   木箱的铁箍部分：漫反射有铁色，高光为白（强高光）
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

// ============================================================
// 材质结构体 —— sampler2D 替代 vec3
// ============================================================
struct Material {
    sampler2D diffuse;      // 漫反射贴图（取代 material.diffuse）
    sampler2D specular;     // 镜面反射贴图（取代 material.specular）
    float shininess;        // 反光度（仍为 uniform）
};

// ============================================================
// 光源结构体
// ============================================================
struct Light {
    vec3 position;

    vec3 ambient;       // 环境光（通常 diffuse × 0.1 ~ 0.2）
    vec3 diffuse;       // 漫反射光（光源主色）
    vec3 specular;      // 镜面反射光（通常 = diffuse 或纯白）
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;

// 调试选项
uniform bool useSpecularMap;     // 是否使用高光贴图（false 时用 uniform 颜色替代）
uniform vec3 specularOverride;   // 替代高光贴图的统一颜色

void main()
{
    // ============================================================
    // 从纹理采样材质属性
    // ============================================================
    vec3 diffuseMap  = vec3(texture(material.diffuse,  TexCoord));
    vec3 specularMap = vec3(texture(material.specular, TexCoord));

    // 环境光：光源环境光 × 漫反射贴图（环境光通常用漫反射颜色）
    vec3 ambient = light.ambient * diffuseMap;

    // ============================================================
    // 漫反射
    // ============================================================
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * diffuseMap);

    // ============================================================
    // 镜面反射
    // ============================================================
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    // 选择使用高光贴图还是统一颜色
    vec3 specMapValue = useSpecularMap ? specularMap : specularOverride;
    vec3 specular = light.specular * (spec * specMapValue);

    // ============================================================
    // 合成
    // ============================================================
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}

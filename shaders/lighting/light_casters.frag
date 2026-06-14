/**
 * 片段着色器 —— 投光物（Light Casters）
 *
 * ============================================================
 *  三种光源类型
 * ============================================================
 *
 *  ① Directional Light（方向光 / 平行光）
 *     模拟太阳光。所有光线平行，光照方向不随位置变化。
 *     特点：无衰减，整个场景光照均匀。
 *     计算：lightDir = normalize(-light.direction)
 *
 *  ② Point Light（点光源）
 *     模拟灯泡。从一点向所有方向发光。
 *     特点：有衰减（距离越远光照越弱）。
 *     计算：lightDir = normalize(light.pos - FragPos)
 *           attenuation = 1.0 / (k0 + k1*d + k2*d²)
 *
 *  ③ Spotlight（聚光灯）
 *     模拟手电筒。在点光源基础上增加锥体限制。
 *     特点：只有锥体内部被照亮，边缘平滑过渡。
 *     计算：点光源计算 + cutOff/outerCutOff 锥体检测
 *
 * ============================================================
 *  光源类型切换
 * ============================================================
 *  通过 uniform int lightType 控制：
 *     0 = Directional Light
 *     1 = Point Light
 *     2 = Spotlight
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

// ============================================================
// 材质（复用光照贴图的方式）
// ============================================================
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

// ============================================================
// 光源（统一结构体，不同类型使用不同字段）
// ============================================================
struct Light {
    // 所有类型共用
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    // 方向光 / 聚光灯：光照方向
    // 方向光：从光源指向场景的方向（如 (0, -1, 0) 表示从上往下照）
    // 聚光灯：锥体的中心轴方向
    vec3 direction;

    // 点光源 / 聚光灯：光源位置
    vec3 position;

    // 点光源 / 聚光灯：衰减系数
    // attenuation = 1.0 / (constant + linear * d + quadratic * d²)
    float constant;
    float linear;
    float quadratic;

    // 聚光灯：锥体角度（余弦值）
    // cutOff       = cos(内锥角)  — 锥体内全亮
    // outerCutOff  = cos(外锥角)  — 锥体外全暗，中间平滑过渡
    float cutOff;
    float outerCutOff;
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;

// 光源类型：0=方向光, 1=点光源, 2=聚光灯
uniform int lightType;

void main()
{
    // ============================================================
    // 从纹理采样材质属性
    // ============================================================
    vec3 diffuseMap  = vec3(texture(material.diffuse,  TexCoord));
    vec3 specularMap = vec3(texture(material.specular, TexCoord));
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // ============================================================
    // 环境光（不受衰减和聚光灯锥体影响）
    // ============================================================
    vec3 ambient = light.ambient * diffuseMap;

    // ============================================================
    // 光源方向与衰减计算（因光源类型而异）
    // ============================================================
    vec3 lightDir;
    float attenuation = 1.0;

    if (lightType == 0)
    {
        // --- ① 方向光 ---
        // 所有光线平行，lightDir 全场景一致
        lightDir = normalize(-light.direction);
        // 方向光无衰减
    }
    else
    {
        // --- ② 点光源 / ③ 聚光灯 ---
        // 从片段位置指向光源位置
        vec3 posToLight = light.position - FragPos;
        float distance = length(posToLight);
        lightDir = normalize(posToLight);

        // 衰减：距离越远光照越弱
        // d = 距离
        // constant = 常数项（通常=1）
        // linear   = 一次项（控制衰减速度）
        // quadratic = 二次项（远距离快速衰减）
        attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    }

    // ============================================================
    // 漫反射（三种光源类型相同）
    // ============================================================
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * diffuseMap;

    // ============================================================
    // 镜面反射（三种光源类型相同）
    // ============================================================
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * specularMap;

    // ============================================================
    // 聚光灯锥体限制
    // ============================================================
    float spotlightIntensity = 1.0;
    if (lightType == 2)
    {
        // theta = lightDir 与 -light.direction 的夹角余弦
        // 当 theta > cutOff 时，片段在锥体内
        // 当 theta < outerCutOff 时，片段在锥体外
        // 中间区域平滑过渡
        float theta = dot(lightDir, normalize(-light.direction));
        float epsilon = light.cutOff - light.outerCutOff;
        spotlightIntensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    }

    // ============================================================
    // 合成最终颜色
    // ============================================================
    // 环境光不受衰减和聚光灯影响
    // 漫反射和镜面反射受衰减和聚光灯影响
    vec3 result = ambient + (diffuse + specular) * attenuation * spotlightIntensity;
    FragColor = vec4(result, 1.0);
}

/**
 * 片段着色器 —— 多光源（Multiple Lights）
 *
 * ============================================================
 *  同时使用多个光源
 * ============================================================
 *
 * 场景包含三种光源：
 *
 *   ① Directional Light（方向光/太阳）
 *      全场景均匀光照，从上方斜照。
 *
 *   ② 4 个 Point Lights（点光源）
 *      不同颜色、不同位置，各自独立衰减。
 *      产生五彩斑斓的局部光照效果。
 *
 *   ③ Spotlight（聚光灯/手电筒）
 *      跟随摄像机位置，照亮摄像机正对的方向。
 *      制造探照灯效果。
 *
 *  最终颜色 = 方向光贡献 + 各点光源贡献之和 + 聚光灯贡献
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

// ============================================================
// 材质
// ============================================================
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

// ============================================================
// 方向光
// ============================================================
struct DirLight {
    vec3 direction;     // 光照方向（从光源指向场景）
    vec3 ambient;       // 环境光分量（通常很暗）
    vec3 diffuse;       // 漫反射分量
    vec3 specular;      // 镜面反射分量
};

// ============================================================
// 点光源（可数组化，用循环处理）
// ============================================================
struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;     // 衰减常数项
    float linear;       // 衰减一次项
    float quadratic;    // 衰减二次项
};

// ============================================================
// 聚光灯（手电筒）
// ============================================================
struct SpotLight {
    vec3 position;
    vec3 direction;     // 锥体轴向
    float cutOff;       // 内锥角余弦
    float outerCutOff;  // 外锥角余弦

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

#define NR_POINT_LIGHTS 4

uniform Material material;
uniform DirLight dirLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform vec3 viewPos;

// ============================================================
// 光照计算函数声明
// ============================================================
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diffMap, vec3 specMap);
vec3 calcPointLight(PointLight light, vec3 normal, vec3 viewDir, vec3 diffMap, vec3 specMap);
vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 viewDir, vec3 diffMap, vec3 specMap);

void main()
{
    // ============================================================
    // 公共输入
    // ============================================================
    vec3 diffMap  = vec3(texture(material.diffuse,  TexCoord));
    vec3 specMap  = vec3(texture(material.specular, TexCoord));
    vec3 norm     = normalize(Normal);
    vec3 viewDir  = normalize(viewPos - FragPos);

    // ============================================================
    // ① 方向光
    // ============================================================
    vec3 result = calcDirLight(dirLight, norm, viewDir, diffMap, specMap);

    // ============================================================
    // ② 点光源（循环累加）
    // ============================================================
    for (int i = 0; i < NR_POINT_LIGHTS; i++)
        result += calcPointLight(pointLights[i], norm, viewDir, diffMap, specMap);

    // ============================================================
    // ③ 聚光灯（手电筒）
    // ============================================================
    result += calcSpotLight(spotLight, norm, viewDir, diffMap, specMap);

    FragColor = vec4(result, 1.0);
}

// ============================================================
// 方向光计算
// ============================================================
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diffMap, vec3 specMap)
{
    vec3 lightDir = normalize(-light.direction);

    // 漫反射
    float diff = max(dot(normal, lightDir), 0.0);

    // 镜面反射
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    // 合成
    vec3 ambient  = light.ambient  * diffMap;
    vec3 diffuse  = light.diffuse  * diff * diffMap;
    vec3 specular = light.specular * spec * specMap;

    return (ambient + diffuse + specular);
}

// ============================================================
// 点光源计算
// ============================================================
vec3 calcPointLight(PointLight light, vec3 normal, vec3 viewDir, vec3 diffMap, vec3 specMap)
{
    vec3 lightDir = normalize(light.position - FragPos);

    // 漫反射
    float diff = max(dot(normal, lightDir), 0.0);

    // 镜面反射
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    // 衰减
    float distance    = length(light.position - FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    // 合成
    vec3 ambient  = light.ambient  * diffMap;
    vec3 diffuse  = light.diffuse  * diff * diffMap;
    vec3 specular = light.specular * spec * specMap;

    return (ambient + diffuse + specular) * attenuation;
}

// ============================================================
// 聚光灯计算
// ============================================================
vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 viewDir, vec3 diffMap, vec3 specMap)
{
    vec3 lightDir = normalize(light.position - FragPos);

    // 漫反射
    float diff = max(dot(normal, lightDir), 0.0);

    // 镜面反射
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    // 衰减
    float distance    = length(light.position - FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    // 聚光灯锥体
    float theta     = dot(lightDir, normalize(-light.direction));
    float epsilon   = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    // 合成
    vec3 ambient  = light.ambient  * diffMap;
    vec3 diffuse  = light.diffuse  * diff * diffMap;
    vec3 specular = light.specular * spec * specMap;

    return (ambient + diffuse + specular) * attenuation * intensity;
}

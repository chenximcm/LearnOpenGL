/**
 * 片段着色器 —— 材质（Materials）
 *
 * ============================================================
 *  核心变化：引入 Material 和 Light 结构体
 * ============================================================
 *
 * 基础光照章节中，所有物体的环境光/漫反射/镜面反射使用相同的
 * 全局系数。材质章节将光照拆分为两个独立的结构体：
 *
 *   Material — 定义物体本身的「材质属性」
 *     .ambient   物体反射环境光的能力（颜色）
 *     .diffuse   物体反射漫反射光的能力（颜色）
 *     .specular  物体反射镜面反射光的能力（颜色）
 *     .shininess 反光度（控制高光范围）
 *
 *   Light — 定义光源的属性
 *     .position  光源位置（世界坐标）
 *     .ambient   光源发出的环境光（通常很暗）
 *     .diffuse   光源发出的漫反射光（通常最亮）
 *     .specular  光源发出的镜面反射光（通常较亮）
 *
 * ============================================================
 *  不同材质的视觉效果
 * ============================================================
 *   金属材质（Gold/Silver/Copper）：
 *     - ambient 较亮（自身有颜色）
 *     - diffuse 亮且有色相
 *     - specular 强且带金属色
 *     - shininess 较高
 *
 *   塑料材质（Red Plastic）：
 *     - ambient 很暗或黑色
 *     - diffuse 有鲜艳的颜色
 *     - specular 白色高光
 *     - shininess 适中
 *
 *   橡胶材质（Cyan Rubber）：
 *     - ambient 黑色
 *     - diffuse 暗淡
 *     - specular 很弱
 *     - shininess 低（高光范围大）
 *
 * @version 330 core
 */
#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

// ============================================================
// 材质结构体 —— 定义物体如何反射光线
// ============================================================
struct Material {
    vec3 ambient;       // 环境光反射颜色
    vec3 diffuse;       // 漫反射反射颜色
    vec3 specular;      // 镜面反射反射颜色
    float shininess;    // 反光度
};

// ============================================================
// 光源结构体 —— 定义光源发出的光线
// ============================================================
struct Light {
    vec3 position;      // 光源位置

    vec3 ambient;       // 环境光分量（通常用一个很小的值模拟环境反射）
    vec3 diffuse;       // 漫反射分量（光源的「颜色」）
    vec3 specular;      // 镜面反射分量（通常 = diffuse，白光高光）
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;   // 摄像机位置（用于镜面反射的视线方向）

void main()
{
    // ============================================================
    // ① 环境光（Ambient）
    // ============================================================
    // 环境光 = 光源环境光 × 材质环境光反射系数
    vec3 ambient = light.ambient * material.ambient;

    // ============================================================
    // ② 漫反射（Diffuse）
    // ============================================================
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    // 漫反射 = 光源漫反射 × (漫反射强度系数 × 材质漫反射反射颜色)
    vec3 diffuse = light.diffuse * (diff * material.diffuse);

    // ============================================================
    // ③ 镜面反射（Specular）
    // ============================================================
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // 镜面反射 = 光源镜面反射 × (镜面强度系数 × 材质镜面反射反射颜色)
    vec3 specular = light.specular * (spec * material.specular);

    // ============================================================
    // 合成：环境光 + 漫反射 + 镜面反射
    // ============================================================
    // 注意与基础光照章节的区别：
    //
    //   基础光照：
    //     result = (ambient + diffuse + specular) * objectColor
    //
    //   材质：
    //     result = ambient + diffuse + specular
    //     （颜色信息已经包含在 material.diffuse 中）
    //
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}

# 💡 OpenGL 学习笔记（十五）：模型加载 / Model Loading

> 跟随 [LearnOpenGL CN](https://learnopengl-cn.github.io/) 教程，使用 Assimp 库加载复杂 3D 模型，设计 Mesh 和 Model 类来管理顶点、纹理和多网格绘制。

---

## 📑 目录

- [1. 概述](#1-概述)
- [2. Assimp 数据结构](#2-assimp-数据结构)
- [3. Mesh 类设计](#3-mesh-类设计)
- [4. Model 类设计](#4-model-类设计)
- [5. Demo 操作指南](#5-demo-操作指南)
- [6. 常见问题](#6-常见问题)
- [7. 配套改动](#7-配套改动)
- [8. 下一步 / 预告](#8-下一步--预告)
- [9. 关键知识点总结](#9-关键知识点总结)

---

## 1. 概述

模型加载章节完成了从「手动定义立方体」到「加载任意 3D 模型」的跨越。核心是使用 **Assimp**（Open Asset Import Library）库，它将几十种 3D 模型格式（`.obj`、`.fbx`、`.collada` 等）统一成一份标准数据结构。

### 1.1 涉及的 3 个核心类

| 类 | 文件 | 职责 |
|---|---|---|
| `Vertex` + `Texture` | `mesh.h` | 顶点结构（位置/法线/纹理坐标/切线）和纹理描述 |
| `Mesh` | `mesh.h` | 单个网格：管理 VAO/VBO/EBO，封装一次 `glDrawElements` |
| `Model` | `model.h` | 模型入口：Assimp 加载、递归遍历场景树、生成 Mesh 数组 |

### 1.2 相比之前章节的变化

| 之前 | 现在 |
|---|---|
| 手动定义 36 个顶点 | Assimp 自动解析成千上万个顶点 |
| 手动计算法线 | 从 `mesh->mNormals` 直接读取 |
| 纹理路径硬编码 | 从 `.mtl` 文件中自动提取 |
| 一个 Cube 用 1 个 VAO | 每个 Mesh 拥有独立 VAO |
| 不支持复杂模型 | 任意 `.obj` / `.fbx` / `.gltf` |

---

## 2. Assimp 数据结构

```
Scene（aiScene）
 ├── mRootNode（aiNode）          ← 根节点
 │    ├── mMeshes[]               ← 指向 mMesh 数组索引
 │    ├── mChildren[]             ← 子节点（递归）
 │    └── ...
 ├── mMeshes[]（aiMesh）          ← 所有网格数据
 │    ├── mVertices[]             ← 顶点位置
 │    ├── mNormals[]              ← 法线
 │    ├── mTextureCoords[0][]     ← 纹理坐标（最多 8 组）
 │    ├── mFaces[]                ← 面（索引数组）
 │    └── mMaterialIndex          ← 指向 mMaterials 的索引
 ├── mMaterials[]（aiMaterial）   ← 材质属性
 └── ...
```

### 后处理选项

在 `importer.ReadFile()` 时设置：

| 选项 | 作用 | 建议 |
|------|------|------|
| `aiProcess_Triangulate` | 将多边形三角化 | ✅ **必须** |
| `aiProcess_FlipUVs` | 翻转 Y 轴纹理坐标（OpenGL 左下角原点 vs 图片左上角） | ✅ 推荐 |
| `aiProcess_GenNormals` | 若模型没有法线则自动生成 | ✅ 推荐 |
| `aiProcess_OptimizeMeshes` | 合并小网格减少 Draw Call | ✅ 推荐 |
| `aiProcess_CalcTangentSpace` | 计算切线空间（法线贴图需要） | ➕ 可选 |

---

## 3. Mesh 类设计

### 3.1 Vertex 结构

```cpp
struct Vertex {
    glm::vec3 Position;    // 位置
    glm::vec3 Normal;      // 法线
    glm::vec2 TexCoords;   // 纹理坐标
    glm::vec3 Tangent;     // 切线（法线贴图预留）
    glm::vec3 Bitangent;   // 副切线（法线贴图预留）
};
```

### 3.2 Mesh 类

```cpp
class Mesh {
    vector<Vertex> vertices;          // 顶点数组
    vector<unsigned int> indices;     // 索引数组（EBO）
    vector<Texture> textures;         // 纹理数组
    unsigned int VAO;                 // 顶点数组对象

    void Draw(Shader &shader) const;  // 绑定纹理 → 绘制（const 方法）
};
```

### 3.3 Draw 流程

1. 遍历 `textures`，激活对应纹理单元，设置 `material.texture_diffuseN` 等 sampler
2. 绑定 VAO，调用 `glDrawElements`

### 3.4 顶点属性布局

| Location | 属性 | 类型 | 偏移 |
|---|---|---|---|
| 0 | Position | vec3 | 0 |
| 1 | Normal | vec3 | 12 |
| 2 | TexCoords | vec2 | 24 |
| 3 | Tangent | vec3 | 32 |
| 4 | Bitangent | vec3 | 44 |

---

## 4. Model 类设计

### 4.1 核心流程

```cpp
Model::Model(path)
  └─ loadModel(path)
       ├─ Assimp::Importer.ReadFile(path, postProcessFlags)
       ├─ 提取 directory（纹理路径基准）
       └─ processNode(scene->mRootNode, scene)
            ├─ 对 node->mMeshes[] 每个索引：
            │    processMesh(scene->mMeshes[idx], scene)  →  Mesh
            └─ 对 node->mChildren[] 每个子节点：
                 processNode(child, scene)   ← 递归
```

### 4.2 processMesh 三步骤

1. **提取顶点**：遍历 `mVertices[]` / `mNormals[]` / `mTextureCoords[0][]`
2. **提取索引**：遍历 `mFaces[]`，每个 Face 包含三角形三个顶点的索引
3. **提取纹理**：通过 `mMaterialIndex` 查找 `aiMaterial`，加载 diffuse / specular / normal maps

### 4.3 纹理去重

```cpp
vector<Texture> textures_loaded;  // 全局缓存

// 每次加载前检查是否已加载过（按 path 比较）
for (auto &loaded : textures_loaded)
    if (loaded.path == newPath)
        return loaded;  // 复用已有纹理
```

---

## 5. Demo 操作指南

### 5.1 操作方式

| 操作 | 功能 |
|------|------|
| W / A / S / D | 摄像机前后左右移动 |
| 右键 + 拖拽 | 鼠标控制视角 |
| F | 开关聚光灯（手电筒） |
| Tab | 切换 ImGui 调试面板 |
| ESC | 退出 |

### 5.2 ImGui 面板功能

| 控制项 | 作用 |
|--------|------|
| **Model Transform — Rotate X / Y** | 模型绕 X/Y 轴旋转（范围 ±180°） |
| **Model Transform — Scale** | 模型缩放（0.1~3.0） |
| **Directional Light** | 方向光开关及环境光/漫反射/镜面反射颜色 |
| **Point Lights** | 4 个点光源独立开关，每个可调位置和颜色 |
| **Flashlight (F)** | 聚光灯开关 |
| **Show Light Spheres** | 是否显示点光源位置标记小球 |
| **Speed** | 摄像机移动速度 |
| **FOV** | 视野大小（10°~120°） |
| **Clear** | 背景颜色 |

### 5.3 推荐操作

1. **打开 / 关闭方向光**，观察模型从有主光源到完全依靠点光源的变化
2. **开启 Flashlight**，移动摄像机感受手电筒照亮模型局部
3. **旋转模型**（Rotate X / Y），从各个角度观察模型的复杂几何结构
4. **调节点光源位置**（Drag Pos），感受多光源在不同方向照亮模型的效果

---

## 6. 常见问题

### Q1：Assimp 的 DLL 版本不匹配怎么办？

Assimp 的 `.lib` 和 `.dll` 需要与编译器版本匹配。VS 2022 使用 v143 工具集，若使用 v140（VS 2015）编译的 assimp 库，会有 MSVCRT 链接警告，但通常可运行。

解决方案：
- 自行用 CMake 从源码编译 Assimp，选择 VS 2022 工具集
- 或使用预编译的 `assimp-vc140-mt.dll`（兼容 VS 2015~2022 均可运行）

### Q2：Debug 模式下报 MSVCP140D.dll 缺失？

从 LearnOpenGL 仓库下载的 `assimp-vc140-mt.dll` 是 Debug 构建，依赖 `MSVCP140D.dll` / `VCRUNTIME140D.dll`。有 VS 的机器上自带这些 DLL，若纯发布需要 Release 版或自行编译 Assimp。

### Q3：DLL 下载损坏或被截断？

`raw.githubusercontent.com` 在大陆下载大型文件容易截断。可以用 **jsDelivr CDN** 镜像替代：

```
https://cdn.jsdelivr.net/gh/JoeyDeVries/LearnOpenGL@master/dlls/assimp-vc140-mt.dll
```

### Q4：纹理加载路径错误？

纹理路径在模型文件（`.obj` / `.mtl`）中通常是相对路径，需要在 `processMesh` 中拼接模型所在目录作为基准路径，否则找不到纹理文件。

### Q5：纹理方向错误（上下颠倒或左右颠倒）？

OpenGL 的纹理坐标系原点在左下角，而图片文件（`.jpg` / `.png`）原点在左上角。有两种解决方式：
- 加载纹理前调用 `stbi_set_flip_vertically_on_load(true)`
- 或在 Assimp 后处理中开启 `aiProcess_FlipUVs`

### Q6：着色器 sampler uniform 为什么用 `setInt` 而非 `setFloat`？

纹理单元绑定后，GLSL sampler 类型的 uniform 统一用 `glUniform1i` 设置（即 `setInt`），而不是 `setFloat`。这是因为 sampler 接收的是纹理单元编号（整数），而不是浮点数：

```cpp
shader.setInt("material.texture_diffuse1", 0);  // ✅ 正确
shader.setFloat("material.texture_diffuse1", 0); // ❌ 错误
```

---

## 7. 配套改动

### 7.1 Shader 类增强

本章为 `shader.h` 新增了 `glm::vec3` 重载版本：

```cpp
// 新增：接受 glm::vec3 对象，不再需要手动传 3 个 float
void setVec3(const std::string& name, const glm::vec3& v) const;
```

这样写起来更简洁：
```cpp
// 之前：shader.setVec3("light.ambient", 0.2f, 0.2f, 0.2f);
// 现在：shader.setVec3("light.ambient", ambientColor);
```

### 7.2 项目配置变动

| 文件 | 改动 |
|---|---|
| `LearnOpenGL.vcxproj` | 添加 `assimp.lib` 链接依赖 + 生成后复制 DLL |
| `LearnOpenGL.vcxproj.filters` | 同步添加 mesh.h / model.h / model.vert / model.frag 条目 |

### 7.3 文件分布

- Mesh 和 Model 类放在 `src/common/mesh.h` 和 `src/common/model.h`（头文件实现，不含 `.cpp`）
- 模型文件放在 `models/backpack/`（`.obj + .mtl + .jpg`）
- 着色器放在 `shaders/model/model.vert` / `model.frag`，使用完整的 Phong 光照模型（方向光 + 点光源 + 聚光灯）

---

## 8. 下一步 / 预告

模型加载完成之后，进入 LearnOpenGL 的高级 OpenGL 章节：

| 内容 | 说明 |
|------|------|
| **深度测试** | 理解深度缓冲的工作原理 |
| **模板测试** | 用模板缓冲实现轮廓、反射等特效 |
| **混合** | 透明物体渲染 |
| **面剔除** | 背面剔除优化性能 |
| **帧缓冲** | 离屏渲染、后期处理 |
| **立方体贴图** | 天空盒、反射/折射效果 |

---

## 9. 关键知识点总结

### ✅ 需要掌握的

1. **Assimp 场景树**：Scene → Node → Mesh（索引）→ Material 的层次结构
2. **后处理选项**：`aiProcess_Triangulate`（必须）、`aiProcess_FlipUVs`、`aiProcess_GenNormals` 等
3. **Mesh 封装**：Vertex 结构体 + Mesh 类管理 VAO/VBO/EBO，封装 `glDrawElements`
4. **Model 递归加载**：`processNode` 递归遍历节点树 → `processMesh` 提取顶点/索引/纹理
5. **纹理去重**：全局 `textures_loaded` 缓存，按路径去重避免重复加载
6. **多光源渲染模型**：方向光 + 4 个点光源 + 聚光灯同时作用于模型
7. **纹理坐标翻转**：OpenGL 左下角 vs 图片左上角，`FlipUVs` 或 `stbi_set_flip`

### ✅ 已经掌握的技能链

```
窗口 → 三角形 → EBO/Shader封装 → 纹理 → 变换 → 坐标系统 → ImGui
→ 摄像机 → 颜色 → 基础光照 → 材质 → 光照贴图 → 投光物 → 多光源 → 🎯 模型加载
```

---

## 📎 相关资源

- [LearnOpenGL - Assimp](https://learnopengl.com/Model-Loading/Assimp)
- [LearnOpenGL - Mesh](https://learnopengl.com/Model-Loading/Mesh)
- [LearnOpenGL - Model](https://learnopengl.com/Model-Loading/Model)
- [Assimp GitHub](https://github.com/assimp/assimp)
- 背包模型：Berk Gedik 制作的 [Survival Guitar Backpack](https://www.artstation.com/artwork/4B8vZ)

---

> **📝 学习日期**：2026-06-15 | **下一个章节**：深度测试（Depth Testing）
>
> **项目代码**：[GitHub - LearnOpenGL](https://github.com/chenximcm/LearnOpenGL)

<style>
.codehilite { color: #d4d4d4; }
.codehilite .k, .codehilite .kc, .codehilite .kd, .codehilite .kn,
.codehilite .kp, .codehilite .kr, .codehilite .kt { color: #569cd6; }
.codehilite .s, .codehilite .sa, .codehilite .sb, .codehilite .sc,
.codehilite .dl, .codehilite .sd, .codehilite .s2, .codehilite .se,
.codehilite .sh, .codehilite .si, .codehilite .sx, .codehilite .sr,
.codehilite .s1, .codehilite .ss { color: #ce9178; }
.codehilite .c, .codehilite .ch, .codehilite .cm, .codehilite .cp,
.codehilite .cpf, .codehilite .c1, .codehilite .cs { color: #6a9955; }
.codehilite .mi, .codehilite .mf, .codehilite .mh, .codehilite .mo,
.codehilite .il, .codehilite .mb, .codehilite .mx { color: #b5cea8; }
.codehilite .nf, .codehilite .fm { color: #dcdcaa; }
.codehilite .nc, .codehilite .nd { color: #4ec9b0; }
.codehilite .ow, .codehilite .o { color: #d4d4d4; }
.codehilite .p { color: #d4d4d4; }
.codehilite .na { color: #9cdcfe; }
.codehilite .nb { color: #569cd6; }
.codehilite .err { color: #f44747; }
</style>

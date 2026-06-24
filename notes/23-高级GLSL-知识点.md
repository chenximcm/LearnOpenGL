# 💡 OpenGL 学习笔记（二十三）：高级GLSL / Advanced GLSL

> 跟随 [LearnOpenGL CN](https://learnopengl-cn.github.io/) 教程，学习 OpenGL 高级部分第八章 —— 高级GLSL（Advanced GLSL）。掌握 GLSL 内建变量、接口块（Interface Block）、Uniform 缓冲对象（UBO）以及 std140 内存布局。

---

## 📑 目录

- [1. 概述](#1-概述)
- [2. GLSL 内建变量](#2-glsl-内建变量)
  - [2.1 顶点着色器变量](#21-顶点着色器变量)
  - [2.2 片段着色器变量](#22-片段着色器变量)
- [3. 接口块（Interface Block）](#3-接口块interface-block)
- [4. Uniform 缓冲对象（UBO）](#4-uniform-缓冲对象ubo)
  - [4.1 概念与优势](#41-概念与优势)
  - [4.2 UBO 使用流程](#42-ubo-使用流程)
  - [4.3 完整代码示例](#43-完整代码示例)
- [5. std140 内存布局](#5-std140-内存布局)
  - [5.1 对齐规则](#51-对齐规则)
  - [5.2 布局计算示例](#52-布局计算示例)
- [6. Demo 操作指南](#6-demo-操作指南)
- [7. 关键知识点总结](#7-关键知识点总结)
- [8. 下一步 / 预告](#8-下一步--预告)

---

## 1. 概述

**高级GLSL（Advanced GLSL）** 是高级 OpenGL 第八章。前面章节已经大量使用 GLSL 编写着色器，本章深入探索 GLSL 的高级特性，让着色器编写更高效、更结构化。

### 1.1 三大核心主题

| 主题 | 用途 | 难度 |
|------|------|------|
| **GLSL 内建变量** | 利用 `gl_FragCoord`、`gl_FrontFacing` 等实现高级效果 | ⭐⭐ |
| **接口块（Interface Block）** | 用结构化块组织着色器间变量传递 | ⭐⭐ |
| **Uniform 缓冲对象（UBO）** | 在多个着色器间共享 uniform 数据，减少 API 调用 | ⭐⭐⭐ |

### 1.2 核心思路

```
GLSL 内建变量    →  着色器自带的"免费"输入/输出，无需声明
接口块           →  把零散的 in/out 变量打包成结构体
Uniform 缓冲对象 →  把 CPU 端的 uniform 设置从 O(N个着色器) 降为 O(1)
```

---

## 2. GLSL 内建变量

GLSL 提供了一些**不需要声明就可以直接使用**的内建变量，它们在渲染管线中传递关键信息。

### 2.1 顶点着色器变量

#### gl_PointSize

设置**点图元**的渲染大小（像素）。需要先在 C++ 端启用：

```cpp
glEnable(GL_PROGRAM_POINT_SIZE);
```

```glsl
// 根据深度缩放点大小 —— 远的点小，近的点大
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    gl_PointSize = gl_Position.z;  // 值越大，点越大
}
```

> **注意**：`gl_PointSize` 只在绘制 `GL_POINTS` 图元时生效。每个顶点可以有不同的点大小。

#### gl_VertexID

只读整数，存储**当前顶点的 ID**：

| 绘制方式 | gl_VertexID 含义 |
|----------|-----------------|
| `glDrawArrays` | 从渲染调用开始至今已处理的顶点数 |
| `glDrawElements` | 当前顶点的**索引值**（element buffer 中的值） |

```glsl
// 示例：用顶点 ID 做颜色变化
void main() {
    // 根据顶点 ID 给不同颜色
    float r = float(gl_VertexID % 3) / 2.0;
    float g = float((gl_VertexID / 3) % 3) / 2.0;
    // ...
}
```

### 2.2 片段着色器变量

#### gl_FragCoord

**输入**变量，包含当前片段在屏幕空间的信息：

| 分量 | 含义 | 范围 |
|------|------|------|
| `gl_FragCoord.x` | 窗口 x 坐标 | `[0, screenWidth]` |
| `gl_FragCoord.y` | 窗口 y 坐标 | `[0, screenHeight]` |
| `gl_FragCoord.z` | 深度值 | `[0.0, 1.0]` |

```glsl
// 示例：屏幕左半部分红色，右半部分绿色
void main() {
    if (gl_FragCoord.x < 400)
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);  // 左侧红色
    else
        FragColor = vec4(0.0, 1.0, 0.0, 1.0);  // 右侧绿色
}
```

> **注意**：`gl_FragCoord` 的坐标原点在窗口**左下角**，与大多数 UI 系统的左上角原点不同。

#### gl_FrontFacing

**输入**变量，`bool` 类型，指示当前片段属于**正面（true）**还是**背面（false）**。

```glsl
// 示例：正面用纹理，背面用纯色
if (gl_FrontFacing)
    FragColor = texture(tex, TexCoords);  // 正面：正常纹理
else
    FragColor = vec4(1.0, 0.0, 0.0, 1.0); // 背面：红色
```

> **注意**：需要 `glDisable(GL_CULL_FACE)` 或开启 `GL_FRONT` 剔除才能看到背面片段。如果开启了 `GL_BACK` 剔除，背面永远不会到达片段着色器。

#### gl_FragDepth

**输出**变量，手动设置片段的深度值。一旦写入 `gl_FragDepth`，OpenGL 将**禁用早期深度测试（Early-Z）**，因为 GPU 在着色器运行前不知道深度值。

```glsl
// 示例：将深度线性映射到 [0, 1]
void main() {
    float near = 0.1;
    float far  = 100.0;
    float linearDepth = (1.0 / gl_FragCoord.z - 1.0 / near) / (1.0 / far - 1.0 / near);
    gl_FragDepth = linearDepth;
}
```

**OpenGL 4.2+ 深度条件声明**（允许 GPU 做深度优化）：

```glsl
layout (depth_greater)  out float gl_FragDepth;  // 保证写入值 ≥ 原始值
layout (depth_less)     out float gl_FragDepth;  // 保证写入值 ≤ 原始值
layout (depth_unchanged) out float gl_FragDepth; // 保证写入值 = 原始值
layout (depth_any)      out float gl_FragDepth;  // 不保证（默认）
```

---

## 3. 接口块（Interface Block）

接口块用于**将多个输入/输出变量打包成一个具名结构体**，在着色器阶段之间传递。

### 3.1 为什么需要接口块？

**传统写法**（零散的 in/out）：

```glsl
// 顶点着色器
out vec3 vColor;
out vec2 vTexCoords;
out vec3 vNormal;

// 片段着色器
in vec3 vColor;
in vec2 vTexCoords;
in vec3 vNormal;
```

变量多了以后，零散声明不便管理，且变量名必须**严格匹配**。

**接口块写法**（打包成块）：

```glsl
// ─── 顶点着色器（输出端）───
out VS_OUT
{
    vec3 color;
    vec2 texCoords;
    vec3 normal;
} vs_out;

void main()
{
    vs_out.color     = aColor;
    vs_out.texCoords = aTexCoords;
    vs_out.normal    = aNormal;
}

// ─── 片段着色器（输入端）───
in VS_OUT
{
    vec3 color;
    vec2 texCoords;
    vec3 normal;
} fs_in;

void main()
{
    FragColor = vec4(fs_in.color, 1.0);
}
```

### 3.2 接口块规则

| 规则 | 说明 |
|------|------|
| **块名必须匹配** | 输出端 `VS_OUT` 和输入端 `VS_OUT` 必须一致 |
| **实例名可不同** | `vs_out` / `fs_in` 可任意命名 |
| **成员顺序** | 建议保持一致（GLSL 按名称匹配，不按顺序） |
| **跨着色器程序** | 块名匹配即可，两个着色器不必在同一个程序里 |

### 3.3 与分开声明的对比

| | 零散 in/out | 接口块 |
|---|---|---|
| 变量声明 | 逐个声明，重复劳动 | 一个块定义搞定 |
| 变量名匹配 | 必须完全一致 | 块名一致，实例名任意 |
| 添加/删除变量 | 两处都要改，容易漏 | 修改块定义，两处同步 |
| 可读性 | 变量多时混乱 | 结构化，一目了然 |

---

## 4. Uniform 缓冲对象（UBO）

### 4.1 概念与优势

**问题**：多个着色器程序都使用 `projection` 和 `view` 矩阵，每个程序都要分别 `glUniformMatrix4fv`，重复设置浪费 CPU 时间。

**UBO 的解决方案**：把共享的 uniform 数据放进一个 **GPU 缓冲区**，多个着色器程序通过 **binding point** 引用同一块数据。

```
┌──────────────┐     ┌───────────────┐     ┌─────────────────┐
│  Shader A    │────▶│ Binding Point │◀────│ uboMatrices     │
│  (Matrices)  │     │       0       │     │ (GPU Memory)    │
└──────────────┘     └───────────────┘     │ ┌─────────────┐ │
                         ▲                  │ │ projection  │ │
┌──────────────┐         │                  │ │ (64 bytes)  │ │
│  Shader B    │─────────┘                  │ ├─────────────┤ │
│  (Matrices)  │                            │ │ view        │ │
└──────────────┘                            │ │ (64 bytes)  │ │
                                            │ └─────────────┘ │
                                            └─────────────────┘
```

**UBO 优势**：

1. **减少 API 调用**：`projection` + `view` 更新一次，所有着色器共享
2. **更高 uniform 上限**：UBO 可达 16KB，远超 `GL_MAX_VERTEX_UNIFORM_COMPONENTS`
3. **逻辑分组**：相关 uniform 打包在一起，语义更清晰
4. **批量更新**：一次 `glBufferSubData` 更新多个 uniform

### 4.2 UBO 使用流程

完整的 UBO 使用分 4 个步骤：

```
步骤 1（GLSL）    定义 uniform 块，指定 std140 布局
步骤 2（C++）     获取每个着色器的块索引，绑定到 binding point
步骤 3（C++）     创建并分配 UBO 缓冲区
步骤 4（渲染循环） 更新 UBO 内容 → 绘制（着色器自动读取）
```

#### GLSL 端

```glsl
// ★ layout(std140) 是必须的 —— 保证内存布局可预测
layout (std140) uniform Matrices
{
    mat4 projection;
    mat4 view;
};

// model 矩阵每个物体不同，保留为普通 uniform
uniform mat4 model;
```

#### C++ 端 —— 绑定

```cpp
// ① 获取 uniform 块索引
unsigned int blockIndex = glGetUniformBlockIndex(shader.ID, "Matrices");

// ② 绑定到 binding point 0
glUniformBlockBinding(shader.ID, blockIndex, 0);
```

> **OpenGL 4.2+ 替代方案**：直接在着色器中声明 `layout(std140, binding = 0) uniform Matrices { ... };`，无需 C++ 端绑定。

#### C++ 端 —— 创建 UBO

```cpp
// ① 创建缓冲区（大小 = 2 个 mat4 = 128 bytes）
unsigned int uboMatrices;
glGenBuffers(1, &uboMatrices);
glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
glBindBuffer(GL_UNIFORM_BUFFER, 0);

// ② 将 UBO 绑定到 binding point 0
glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));
```

`glBindBufferRange` 参数说明：

| 参数 | 含义 |
|------|------|
| `GL_UNIFORM_BUFFER` | 缓冲区目标 |
| `0` | binding point（与着色器中的对应） |
| `uboMatrices` | UBO 对象 ID |
| `0` | 缓冲区起始偏移 |
| `2 * sizeof(glm::mat4)` | 使用范围大小 |

### 4.3 完整代码示例

```cpp
// ========== 初始化阶段 ==========

// 1. 为每个使用 UBO 的着色器绑定
unsigned int idxA = glGetUniformBlockIndex(shaderA.ID, "Matrices");
unsigned int idxB = glGetUniformBlockIndex(shaderB.ID, "Matrices");
glUniformBlockBinding(shaderA.ID, idxA, 0);
glUniformBlockBinding(shaderB.ID, idxB, 0);

// 2. 创建 UBO
unsigned int ubo;
glGenBuffers(1, &ubo);
glBindBuffer(GL_UNIFORM_BUFFER, ubo);
glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);

// 3. 绑定到 binding point 0
glBindBufferRange(GL_UNIFORM_BUFFER, 0, ubo, 0, 2 * sizeof(glm::mat4));

// ========== 渲染循环 ==========
while (!glfwWindowShouldClose(window))
{
    // 更新 projection（通常只在窗口大小变化时更新）
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
        (float)width / height, 0.1f, 100.0f);

    // 更新 view（每帧随摄像机变化）
    glm::mat4 view = camera.GetViewMatrix();

    // ★ 一次更新，两个着色器共享
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // 绘制物体 —— 只需设置 model 矩阵
    shaderA.use();
    shaderA.setMat4("model", modelA);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    shaderB.use();
    shaderB.setMat4("model", modelB);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}
```

---

## 5. std140 内存布局

### 5.1 对齐规则

`std140` 是 OpenGL 标准化的 uniform 块布局规范，保证在不同 GPU/驱动上偏移量一致。

| 类型 | 基准对齐（Base Alignment） | 说明 |
|------|--------------------------|------|
| `int` / `float` / `bool` | **N** (4 bytes) | 标量，4 字节对齐 |
| `vec2` | **2N** (8 bytes) | 双分量向量 |
| `vec3` / `vec4` | **4N** (16 bytes) | 都会被填充到 vec4 大小 |
| `mat4` | **4N × 4** (64 bytes) | 等价于 4 个 vec4 列向量 |
| 标量/向量数组 | 每个元素按 vec4 (16 bytes) 对齐 | 数组元素间可能有填充 |
| 结构体 | 按最大成员对齐，最终填充到 vec4 的倍数 | struct 末尾有填充 |

> **关键规则**：`vec3` 虽然只有 3 个 float (12 bytes)，但在 std140 中占用 16 bytes（等于 vec4）。这就是为什么 `mat4` 是 64 bytes 而不是 48 bytes。

### 5.2 布局计算示例

```glsl
layout (std140) uniform ExampleBlock
{
    float value;        // offset 0   (4 bytes)
    //  ── 填充 12 bytes ──  (因为下一个是 vec3，必须 16-byte 对齐)
    vec3 vector;        // offset 16  (12 bytes)
    //  ── 填充 4 bytes ──   (因为下一个是 mat4，必须 16-byte 对齐)
    mat4 matrix;        // offset 32  (64 bytes: col0@32, col1@48, col2@64, col3@80)
    float values[3];    // offset 96, 112, 128  (每个元素 16-byte 对齐!)
    bool boolean;       // offset 144 (4 bytes)
    int integer;        // offset 148 (4 bytes)
    //  ── 填充 8 bytes ──  (整个块填充到 vec4 的整数倍)
};
// 总大小: 160 bytes

// ★ 关键计算：
// values[1] 在 offset 112，而不是 offset 100（96+4）
//   因为数组元素以 16 bytes 对齐
```

**本 Demo 的 UBO 布局**（最简单的情况）：

```
Offset   Content             Size      对齐说明
─────────────────────────────────────────────
0        mat4 projection     64 bytes  4 个 vec4 列
64       mat4 view           64 bytes  4 个 vec4 列
─────────────────────────────────────────────
Total: 128 bytes
```

> **注意**：两个 `mat4` 紧挨着，因为 mat4 本身就是 16-byte 对齐的，下一个 mat4 从 offset 64 开始刚好是对齐的。

---

## 6. Demo 操作指南

### 运行方式

项目当前默认运行本章 Demo。`main()` 函数中的章节选择器已更新为调用 `runAdvancedGLSLDemo()`。

### 控件说明

| 操作 | 功能 |
|------|------|
| `ESC` | 退出程序 |
| `WASD` / 方向键 | 移动摄像机 |
| `右键拖动` | 旋转视角 |
| `Tab` | 显示/隐藏 ImGui 调试面板 |
| `1` | 显示全部（两个着色器） |
| `2` | 只显示 Shader A（gl_FragCoord 演示） |
| `3` | 只显示 Shader B（gl_FrontFacing 演示） |

### Demo 场景

```
  [Cube A1]  [Cube A2]  [Cube B1]  [Cube B2]
  ─────────────────────────────────────────────
  Shader A                Shader B
  gl_FragCoord 渐变       gl_FrontFacing 双色
```

- **左侧 2 个立方体**使用 Shader A，演示 `gl_FragCoord`：屏幕右侧叠加暖色渐变
- **右侧 2 个立方体**使用 Shader B，演示 `gl_FrontFacing`：正面正常颜色，背面亮黄色
- **两个着色器共享 UBO**：projection 和 view 矩阵只更新一次

### ImGui 面板内容

- UBO 内存布局可视化（std140 偏移图）
- uniform 块绑定信息（block index → binding point）
- 着色器选择模式
- 接口块说明
- std140 对齐规则速查表

---

## 7. 关键知识点总结

### 7.1 GLSL 内建变量

| 变量 | 阶段 | 方向 | 用途 |
|------|------|------|------|
| `gl_PointSize` | 顶点 | 输出 | 控制点精灵大小 |
| `gl_VertexID` | 顶点 | 输入 | 获取当前顶点索引/ID |
| `gl_FragCoord` | 片段 | 输入 | 获取窗口坐标和深度 |
| `gl_FrontFacing` | 片段 | 输入 | 判断正面/背面 |
| `gl_FragDepth` | 片段 | 输出 | 手动设置深度值 |

### 7.2 接口块

```glsl
// 输出端
out VS_OUT { vec3 color; vec3 normal; } vs_out;

// 输入端（块名必须匹配）
in VS_OUT { vec3 color; vec3 normal; } fs_in;
```

- **块名必须一致**，实例名可不同
- 比零散 in/out 更易维护
- 成员按名称匹配（不按顺序）

### 7.3 UBO 流程速查

```
0. GLSL:  layout(std140) uniform Matrices { mat4 proj; mat4 view; };
1. C++:   blockIndex = glGetUniformBlockIndex(shader, "Matrices");
2. C++:   glUniformBlockBinding(shader, blockIndex, 0);
3. C++:   glGenBuffers → glBufferData(UNIFORM_BUFFER, size, NULL, ...)
4. C++:   glBindBufferRange(UNIFORM_BUFFER, 0, ubo, 0, size);
5. 循环:  glBufferSubData(UNIFORM_BUFFER, offset, size, data);
```

### 7.4 std140 关键数字

| 类型 | 大小 |
|------|------|
| `float` | 4 bytes |
| `vec3` | **16 bytes**（不是 12!） |
| `mat4` | **64 bytes**（4 列 × vec4） |
| UBO 最大 | **16KB**（`GL_MAX_UNIFORM_BLOCK_SIZE`） |

---

## 8. 下一步 / 预告

掌握了高级 GLSL 和 UBO 后，下一章将学习 **几何着色器（Geometry Shader）**——在顶点着色器和片段着色器之间插入可编程阶段，可以实现：

- 动态生成/删除图元（爆炸效果、毛发渲染）
- 图元类型转换（点到四边形 —— 广告牌效果）
- 一次绘制多个实例的邻接计算

教程原文链接：[LearnOpenGL — Advanced GLSL](https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL)

中文版：[高级GLSL](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/08%20Advanced%20GLSL/)

---

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

# 🔺 OpenGL 学习笔记（二）：Hello Triangle

> 跟随 [LearnOpenGL CN](https://learnopengl-cn.github.io/) 教程，绘制第一个三角形，理解现代 OpenGL 的核心渲染流程。

---

## 📑 目录

- [1. 图形渲染管线](#1-图形渲染管线)
- [2. 顶点输入](#2-顶点输入)
- [3. VBO —— 顶点缓冲对象](#3-vbo--顶点缓冲对象)
- [4. VAO —— 顶点数组对象](#4-vao--顶点数组对象)
- [5. 着色器](#5-着色器)
- [6. 着色器程序](#6-着色器程序)
- [7. 绘制三角形](#7-绘制三角形)
- [8. 完整流程回顾](#8-完整流程回顾)
- [9. 关键知识点总结](#9-关键知识点总结)

---

## 1. 图形渲染管线

在 OpenGL 3.3 Core Profile 中，所有绘制操作都通过**可编程着色器管线**处理：

```
顶点数据
    ↓
┌──────────────────────┐
│  顶点着色器          │  可编程 — 处理每个顶点位置
│  (Vertex Shader)     │
└──────────────────────┘
    ↓
┌──────────────────────┐
│  图元组装            │  固定功能 — 将顶点组合成三角形/线/点
│  (Primitive Assembly)│
└──────────────────────┘
    ↓
┌──────────────────────┐
│  几何着色器          │  可选 — 可编程，增删改图元
│  (Geometry Shader)   │
└──────────────────────┘
    ↓
┌──────────────────────┐
│  光栅化              │  固定功能 — 将图元转换为片段（像素）
│  (Rasterization)     │
└──────────────────────┘
    ↓
┌──────────────────────┐
│  片段着色器          │  可编程 — 决定每个片段的颜色
│  (Fragment Shader)   │
└──────────────────────┘
    ↓
┌──────────────────────┐
│  测试与混合          │  固定功能 — 深度测试、模板测试、alpha 混合
│  (Tests & Blending)  │
└──────────────────────┘
    ↓
   帧缓冲 → 屏幕
```

> 💡 **核心区别**：旧版 OpenGL（固定管线）使用 `glBegin()/glEnd()` 直接绘制，**所有细节都由驱动决定**。现代 OpenGL（可编程管线）要求你**自己编写着色器**控制顶点变换和像素着色，灵活性远超旧版。

---

## 2. 顶点输入

### 2.1 定义顶点数据

```cpp
float vertices[] = {
    -0.5f, -0.5f, 0.0f,   // 左下角
     0.5f, -0.5f, 0.0f,   // 右下角
     0.0f,  0.5f, 0.0f    // 顶部
};
```

- 每个顶点用 **3 个 float** 表示：x, y, z
- 取值范围 **[-1, 1]** 表示**标准化设备坐标（NDC）**
- OpenGL 只处理 NDC 范围内的顶点，超出部分会被裁剪

### 2.2 标准化设备坐标（NDC）

```
(-1, 1) ┌──────────┐ (1, 1)
        │          │
        │  (0, 0)  │
        │          │
(-1,-1) └──────────┘ (1,-1)
```

- 无论窗口多大，NDC 始终是 **-1 到 1**
- 顶点着色器的输出必须位于 NDC 范围内才能显示
- 后续章节会学习通过**矩阵变换**将模型坐标映射到 NDC

---

## 3. VBO —— 顶点缓冲对象

### 3.1 什么是 VBO？

**VBO（Vertex Buffer Object）** 是存储在 **GPU 显存**中的缓冲区，用于存放顶点数据。

### 3.2 创建和填充

```cpp
unsigned int VBO;
glGenBuffers(1, &VBO);                                    // 生成 VBO
glBindBuffer(GL_ARRAY_BUFFER, VBO);                       // 绑定到 GL_ARRAY_BUFFER 目标
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);  // 上传数据
```

| 步骤 | 函数 | 说明 |
|------|------|------|
| **生成** | `glGenBuffers()` | 分配一个 VBO 对象（返回 ID） |
| **绑定** | `glBindBuffer()` | 将 VBO 绑定到目标，后续操作作用于该目标 |
| **上传** | `glBufferData()` | 将 CPU 数据复制到 GPU 显存 |

### 3.3 数据使用模式

| 模式 | 说明 | 使用场景 |
|------|------|----------|
| `GL_STATIC_DRAW` | 数据**一次写入**，**多次**绘制 | ✅ 顶点数据不变（推荐） |
| `GL_DYNAMIC_DRAW` | 数据**多次修改**，**多次**绘制 | 频繁更新的顶点 |
| `GL_STREAM_DRAW` | 数据**每次**绘制都改 | 每帧都在变的顶点 |

---

## 4. VAO —— 顶点数组对象

### 4.1 为什么需要 VAO？

VAO **记录**了如何从 VBO 中读取顶点数据（即顶点属性的解析方式）。没有 VAO，每次绘制前都要重新配置顶点属性。

### 4.2 VAO 的核心作用

```cpp
unsigned int VAO;
glGenVertexArrays(1, &VAO);
glBindVertexArray(VAO);   // 绑定 VAO → 之后的配置都会记录在这个 VAO 中

// 以下配置会被 VAO "记住"
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

glBindVertexArray(0);     // 解绑 VAO
```

### 4.3 `glVertexAttribPointer` 详解

```cpp
glVertexAttribPointer(
    0,                    // location = 0（对着色器中的 layout(location = 0)）
    3,                    // 每个顶点属性有 3 个分量（x, y, z）
    GL_FLOAT,             // 数据类型
    GL_FALSE,             // 是否归一化
    3 * sizeof(float),    // 步长（stride）— 下一个顶点距当前顶点的字节数
    (void*)0              // 偏移量（数据在缓冲中的起始位置）
);
```

```
内存布局（stride = 12 bytes = 3 × float）：

字节: 0         4         8         12        16        20        24
顶点: |── x ──|── y ──|── z ──|── x ──|── y ──|── z ──|── x ──|── y ──
       ↑ 顶点 0          ↑ stride            ↑ 顶点 1
       (offset = 0)                          (offset = 12)
```

### 4.4 VAO 与 VBO 的关系

```
VAO (记录配置)
 ┌─────────────────────┐
 │ 顶点属性 0:          │
 │   - 启用: true       │──── VBO (存顶点数据)
 │   - 格式: 3 floats   │     ┌──────┬──────┬──────┐
 │   - stride: 12       │     │ v0.x │ v0.y │ v0.z │
 │   - offset: 0        │     ├──────┼──────┼──────┤
 │   - VBO 绑定: VBO-1  │     │ v1.x │ v1.y │ v1.z │
 └─────────────────────┘     ├──────┼──────┼──────┤
                             │ v2.x │ v2.y │ v2.z │
                             └──────┴──────┴──────┘
```

> 🔑 **核心流程**：**VAO → VBO → 顶点属性**，顺序不能乱。先绑定 VAO，再绑定 VBO，再设置顶点属性指针。

---

## 5. 着色器

### 5.1 顶点着色器

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;    // 输入：位置属性

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);  // 输出到裁剪坐标
}
```

| 要点 | 说明 |
|------|------|
| `#version 330 core` | **必须在第一行**，前面不能有任何字符（包括空行/空格） |
| `layout (location = 0)` | 与 `glVertexAttribPointer(0, ...)` 的 location 对应 |
| `in vec3 aPos` | 从 CPU 传入的顶点属性（vec3 类型） |
| `gl_Position` | **内置变量**，顶点着色器的输出位置 |
| `vec4(..., 1.0)` | `w=1.0` 表示位置向量（`w=0.0` 表示方向向量） |

### 5.2 片段着色器

```glsl
#version 330 core
out vec4 FragColor;    // 输出：片段颜色

void main()
{
    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);  // 橙色 RGBA
}
```

| 要点 | 说明 |
|------|------|
| `out vec4 FragColor` | **自定义输出变量**，表示片段最终颜色 |
| `vec4(r, g, b, a)` | RGBA 颜色，范围 [0.0, 1.0]，当前为橙色 |
| 透明度 | a=1.0 表示完全不透明 |

### 5.3 着色器编译

```cpp
// 1. 创建着色器对象
unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);

// 2. 附加源码并编译
glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
glCompileShader(vertexShader);

// 3. 检查编译结果
int success;
char infoLog[512];
glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
if (!success)
{
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
}
```

### 5.4 常见编译错误

```
#version 前有空格/换行   →  "invalid processing instruction version"
拼写错误 / 语法错误      →  "syntax error"
in/out 变量类型不匹配     →  "type mismatch"
忘记定义 main()          →  "undefined entry point"
```

> ⚠️ **GLSL 写法注意**：
> - `vec4(1.0f, 0.5f, 0.2f, 1.0f)` 中的 `f` 后缀在 GLSL 中是**允许**的（但在 GLSL 中不是必需的）
> - `#version` 必须是着色器源码的**第一行**，前面不能有任何字符（包括 C++ 原始字符串 `R"(` 后的换行）

---

## 6. 着色器程序

### 6.1 链接流程

```cpp
unsigned int shaderProgram = glCreateProgram();

// 附着顶点和片段着色器
glAttachShader(shaderProgram, vertexShader);
glAttachShader(shaderProgram, fragmentShader);
glLinkProgram(shaderProgram);

// 检查链接结果
glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
if (!success) { /* 输出错误 */ }

// 链接完成后删除着色器对象（节省内存）
glDeleteShader(vertexShader);
glDeleteShader(fragmentShader);
```

```
           ┌──────────────┐
           │ 顶点着色器    │
           │ (已编译)      │────┐
           └──────────────┘    │
                               ├──→ 着色器程序 ←── 使用 glUseProgram()
           ┌──────────────┐    │      (已链接)
           │ 片段着色器    │────┘
           │ (已编译)      │
           └──────────────┘
```

### 6.2 使用着色器程序

```cpp
glUseProgram(shaderProgram);  // 激活着色器程序
// ... 绘制 ...
```

- 链接后，顶点和片段着色器是**成对绑定**的
- `glUseProgram()` 激活程序，后续绘制调用将使用该程序的着色器

---

## 7. 绘制三角形

### 7.1 绘制调用

```cpp
glUseProgram(shaderProgram);
glBindVertexArray(VAO);
glDrawArrays(GL_TRIANGLES, 0, 3);  // 绘制 3 个顶点 → 1 个三角形
```

### 7.2 `glDrawArrays` 参数

| 参数 | 值 | 说明 |
|------|------|------|
| `mode` | `GL_TRIANGLES` | 绘制三角形 |
| `first` | `0` | 从第 0 个顶点开始 |
| `count` | `3` | 使用 3 个顶点 |

### 7.3 图元类型

| 模式 | 说明 |
|------|------|
| `GL_TRIANGLES` | 每 3 个顶点构成一个独立三角形 |
| `GL_TRIANGLE_STRIP` | 三角形条带，每增加 1 个顶点形成新三角形 |
| `GL_TRIANGLE_FAN` | 三角形扇 |
| `GL_POINTS` | 点 |
| `GL_LINES` | 线段 |

> 💡 三角形渲染时，**顺时针**方向的顶点会被认为是正面（可通过 `glFrontFace()` 修改），而 **背面** 可以用 `glEnable(GL_CULL_FACE)` 剔除。

---

## 8. 完整流程回顾

### 初始化阶段

```
编译着色器             创建 VAO/VBO
 │                       │
 ├─ glCreateShader()     ├─ glGenVertexArrays()
 ├─ glShaderSource()     ├─ glBindVertexArray()
 ├─ glCompileShader()    ├─ glGenBuffers()
 ├─ glCreateProgram()    ├─ glBindBuffer()
 ├─ glAttachShader()     ├─ glBufferData()
 ├─ glLinkProgram()      ├─ glVertexAttribPointer()
 └─ glDeleteShader()     └─ glEnableVertexAttribArray()
       │                        │
       └────────┬───────────────┘
                ↓
         glUseProgram()
         glBindVertexArray()
```

### 渲染循环

```
while (!glfwWindowShouldClose(window))
{
    glClear(GL_COLOR_BUFFER_BIT);   // 清屏
    glUseProgram(shaderProgram);    // 激活 shader
    glBindVertexArray(VAO);         // 绑定 VAO
    glDrawArrays(GL_TRIANGLES, 0, 3); // 绘制
    glfwSwapBuffers(window);        // 交换缓冲
}
```

### 资源清理

```cpp
glDeleteVertexArrays(1, &VAO);
glDeleteBuffers(1, &VBO);
glDeleteProgram(shaderProgram);
glfwTerminate();
```

---

## 9. 关键知识点总结

### 🎯 核心概念

| 概念 | 要点 |
|------|------|
| **图形管线** | 顶点着色器 → 图元组装 → 光栅化 → 片段着色器 → 测试混合 |
| **VAO** | 记录顶点属性配置，避免每次重复设置 — 一次绑定，到处绘制 |
| **VBO** | GPU 显存中的顶点数据缓冲区，`glBufferData()` 上传 |
| **顶点着色器** | 处理每个顶点，输出 `gl_Position`（裁剪坐标） |
| **片段着色器** | 处理每个片段，输出最终颜色 |
| **着色器程序** | 链接顶点+片段着色器，通过 `glUseProgram()` 激活 |
| **glDrawArrays** | 非索引绘制，按顶点顺序绘制 |
| **NDC** | 标准化设备坐标 [-1, 1]，超出范围被裁剪 |

### 💡 最佳实践

```
绘制流程（顺序固定）:
glUseProgram(shader)
    ↓
glBindVertexArray(VAO)
    ↓
glDrawArrays(GL_TRIANGLES, 0, count)

配置流程（顺序固定）:
glBindVertexArray(VAO)
    ↓
glBindBuffer(GL_ARRAY_BUFFER, VBO)
    ↓
glVertexAttribPointer(location, size, type, ...)
    ↓
glEnableVertexAttribArray(location)
```

### ⚠️ 常见错误

```
❌ #version 前有换行/空格
   → GLSL 编译错误 "invalid processing instruction version"
   ✅ 修复: #version 必须在着色器源码的第一行

❌ 忘记 glUseProgram()
   → 绘制时使用默认程序（无着色器），画面空白

❌ 忘记 glBindVertexArray()
   → 未绑定 VAO，无法读取顶点数据，画面空白

❌ glVertexAttribPointer location 不匹配
   → C++ 代码的 location 与 GLSL 的 layout(location=?) 不一致

❌ 没有 glEnableVertexAttribArray()
   → 顶点属性处于禁用状态，不会被读取

❌ glDrawArrays(GL_TRIANGLES, 0, 3) 但顶点数不够
   → 读取越界，可能导致崩溃或非预期结果
```

### 📐 颜色的表示

| 颜色 | R | G | B | A |
|------|:-:|:-:|:-:|:-:|
| 🔴 红 | 1.0 | 0.0 | 0.0 | 1.0 |
| 🟢 绿 | 0.0 | 1.0 | 0.0 | 1.0 |
| 🔵 蓝 | 0.0 | 0.0 | 1.0 | 1.0 |
| 🟠 橙（本章用） | 1.0 | 0.5 | 0.2 | 1.0 |
| ⚪ 白 | 1.0 | 1.0 | 1.0 | 1.0 |
| ⚫ 黑 | 0.0 | 0.0 | 0.0 | 1.0 |

---

## 📎 相关资源

- [LearnOpenGL CN - Hello Triangle](https://learnopengl-cn.github.io/01%20Getting%20started/04%20Hello%20Triangle/)
- [GLSL 语言规范 (Khronos)](https://www.khronos.org/opengl/wiki/Core_Language_(GLSL))
- [OpenGL 绘制模式文档](https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawArrays.xhtml)
- [OpenGL 对象（VAO, VBO）概念](https://www.khronos.org/opengl/wiki/Vertex_Specification)

---

> **📝 学习日期**：2026-06-06 | **上一个章节**：[创建窗口](01-创建窗口-知识点.md)
>
> **下一个章节**：[EBO 与 Shader 封装](03-EBO与Shader封装-知识点.md) — 索引缓冲对象、索引绘制、Shader 类封装

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

# 🔺 OpenGL 学习笔记（三）：EBO 与 Shader 封装

> 跟随 [LearnOpenGL CN](https://learnopengl-cn.github.io/) 教程，学习索引缓冲对象（EBO）和着色器封装。

---

## 📑 目录

- [1. EBO —— 索引缓冲对象](#1-ebo--索引缓冲对象)
- [2. 索引绘制](#2-索引绘制)
- [3. VAO / VBO / EBO 完整流程](#3-vao--vbo--ebo-完整流程)
- [4. Shader 类封装](#4-shader-类封装)
- [5. Shader 类 API 详解](#5-shader-类-api-详解)
- [6. 使用 Shader 类重写程序](#6-使用-shader-类重写程序)
- [7. 关键知识点总结](#7-关键知识点总结)

---

## 1. EBO —— 索引缓冲对象

### 1.1 为什么需要 EBO？

绘制**矩形**需要 2 个三角形，如果不使用 EBO 需要 6 个顶点：

```
三角形 1: v0, v1, v3
三角形 2: v1, v2, v3

    v0 ────── v3
     │ \    │
     │   \  │
    v1 ────── v2

v0 = (0.5,  0.5)   右上
v1 = (0.5, -0.5)   右下
v2 = (-0.5, -0.5)  左下
v3 = (-0.5,  0.5)  左上
```

用 `glDrawArrays` 需要 6 个顶点 — **v1 和 v3 重复了两次**：

```cpp
float vertices[] = {
    // 第一个三角形
     0.5f,  0.5f, 0.0f,   // v0
     0.5f, -0.5f, 0.0f,   // v1  ← 重复
    -0.5f,  0.5f, 0.0f,   // v3  ← 重复
    // 第二个三角形
     0.5f, -0.5f, 0.0f,   // v1  ← 重复
    -0.5f, -0.5f, 0.0f,   // v2
    -0.5f,  0.5f, 0.0f    // v3  ← 重复
};
```

> 💡 一个矩形才 4 个顶点，但绘制需要 6 个 —— 浪费了 50%！
> 复杂模型中（如上万个三角形的网格），**EBO 能节省大量显存**。

### 1.2 什么是 EBO？

**EBO（Element Buffer Object）**，也叫 **IBO（Index Buffer Object，索引缓冲对象）**，是 GPU 显存中的一个缓冲区，用于存储**顶点索引**。

- EBO 不存顶点数据（那是 VBO 的事）
- EBO 存的是**整型索引**，每个索引指向 VBO 中一个顶点
- 绘制时 GPU 按索引顺序读取顶点

### 1.3 创建 EBO

```cpp
unsigned int indices[] = {
    0, 1, 3,   // 第一个三角形：v0 → v1 → v3
    1, 2, 3    // 第二个三角形：v1 → v2 → v3
};

unsigned int EBO;
glGenBuffers(1, &EBO);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
```

和 VBO 完全相同的工作流：
1. `glGenBuffers()` — 生成对象
2. `glBindBuffer()` — 绑定到目标（这次是 `GL_ELEMENT_ARRAY_BUFFER`）
3. `glBufferData()` — 上传数据到 GPU

### 1.4 数据使用模式

与 VBO 相同的三种模式：

| 模式 | 说明 | 使用场景 |
|------|------|----------|
| `GL_STATIC_DRAW` | 数据**一次写入**，**多次**绘制 | ✅ 索引不变（推荐） |
| `GL_DYNAMIC_DRAW` | 数据**多次修改**，**多次**绘制 | 动态变化的索引 |
| `GL_STREAM_DRAW` | 数据**每次**绘制都改 | 极少用 |

---

## 2. 索引绘制

### 2.1 glDrawElements

```cpp
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

| 参数 | 值 | 说明 |
|------|------|------|
| `mode` | `GL_TRIANGLES` | 图元类型 |
| `count` | `6` | 绘制的**索引个数**（不是顶点数） |
| `type` | `GL_UNSIGNED_INT` | 索引的数据类型 |
| `indices` | `(void*)0` | EBO 中的字节偏移量，`0` 表示从头开始 |

### 2.2 glDrawArrays vs glDrawElements

| 对比 | `glDrawArrays` | `glDrawElements` |
|------|----------------|------------------|
| 顶点数据 | 按顺序依次读取 | 按索引读取 |
| 重复顶点 | **需要重复存储** | 不重复，索引复用 |
| 内存效率 | 低（有重复顶点） | **高**（复用顶点） |
| 适用场景 | 简单图形（点、线、单个三角形） | **复杂网格**（矩形、模型） |

### 2.3 索引数据类型

| 类型 | OpenGL 枚举 | 字节数 | 最大顶点数 |
|------|-------------|--------|-----------|
| `unsigned char` | `GL_UNSIGNED_BYTE` | 1 | 256 |
| `unsigned short` | `GL_UNSIGNED_SHORT` | 2 | 65536 |
| `unsigned int` | `GL_UNSIGNED_INT` | 4 | 42 亿 |

```cpp
// 小模型用 unsigned short 节省内存
unsigned short indices[] = { 0, 1, 3, 1, 2, 3 };
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

// 大模型用 unsigned int
unsigned int indices[] = { 0, 1, 3, 1, 2, 3 };
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

> 💡 初学者推荐直接用 `GL_UNSIGNED_INT`，简单不易错。
> 在大型项目中，根据顶点数量选择最小的类型以节省显存。

### 2.4 线框模式

```cpp
glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);   // 线框模式
// 绘制...
glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);    // 恢复填充模式
```

用线框模式能清楚地看到矩形由**两个三角形**拼成。

---

## 3. VAO / VBO / EBO 完整流程

### 3.1 三者关系图

```
VAO (记录配置)
 ┌──────────────────────────┐
 │ VBO 绑定: VBO-1          │──── VBO ──── 顶点数据
 │  顶点属性 0:             │       ┌──────┬──────┬──────┐
 │    - 启用: true          │       │ v0.x │ v0.y │ v0.z │
 │    - 格式: 3 floats      │       ├──────┼──────┼──────┤
 │    - stride: 12          │       │ v1.x │ v1.y │ v1.z │
 │    - offset: 0           │       ├──────┼──────┼──────┤
 │                          │       │ v2.x │ v2.y │ v2.z │
 │ EBO 绑定: EBO-1          │       ├──────┼──────┼──────┤
 │                          │────   │ v3.x │ v3.y │ v3.z │
 └──────────────────────────┘       └──────┴──────┴──────┘
                                          ↖ 索引指向
                                    EBO ─── [0, 1, 3, 1, 2, 3]
```

### 3.2 完整配置流程

```cpp
// ===== 1. 生成对象 =====
unsigned int VAO, VBO, EBO;
glGenVertexArrays(1, &VAO);
glGenBuffers(1, &VBO);
glGenBuffers(1, &EBO);

// ===== 2. 绑定 VAO（记录后续配置） =====
glBindVertexArray(VAO);

// ===== 3. VBO：上传顶点数据 =====
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

// ===== 4. EBO：上传索引数据 =====
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

// ===== 5. 设置顶点属性 =====
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

// ===== 6. 解绑 =====
glBindBuffer(GL_ARRAY_BUFFER, 0);  // 解绑 VBO（不会影响 VAO 记录）
glBindVertexArray(0);              // 解绑 VAO
// ⚠️ 注意：不要在解绑 VAO 之前解绑 EBO！
```

### 3.3 ⚠️ EBO 解绑陷阱

```cpp
// ❌ 错误：先解绑 EBO 再解绑 VAO
glBindVertexArray(VAO);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);  // OK
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);    // ❌ VAO 会记住"没有 EBO"！
glBindVertexArray(0);                         // VAO 记录的 EBO = 无

// ✅ 正确：不解绑 EBO，直接解绑 VAO
glBindVertexArray(VAO);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);  // VAO 记录
glBindVertexArray(0);                         // VAO 记住 EBO，无需手动解绑
```

> **核心原因**：VAO 在解绑时会「快照」当前的 `GL_ELEMENT_ARRAY_BUFFER` 绑定。
> 如果解绑前已经把 EBO 解绑了，VAO 就永远丢失了 EBO 的关联。

### 3.4 渲染循环与清理

```cpp
// 渲染循环
while (!glfwWindowShouldClose(window))
{
    glClear(GL_COLOR_BUFFER_BIT);

    shader.use();
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glfwSwapBuffers(window);
    glfwPollEvents();
}

// 清理
glDeleteVertexArrays(1, &VAO);
glDeleteBuffers(1, &VBO);
glDeleteBuffers(1, &EBO);
```

### 3.5 内存对比

| 方式 | 顶点数 | 索引数 | 总内存 |
|------|--------|--------|--------|
| 无 EBO（`glDrawArrays`） | 6 × vec3 | 0 | 72 bytes |
| 有 EBO（`glDrawElements`） | 4 × vec3 | 6 × uint | 48 + 24 = **72 bytes** |
| 1000 个三角形网格（无 EBO） | 3000 × vec3 | 0 | 36000 bytes |
| 1000 个三角形网格（有 EBO） | ~500 × vec3 | 3000 × uint | **6000 + 12000 = 18000 bytes** |

> 真实 3D 模型中顶点数往往是三角形数的 1/2 ~ 1/3，EBO 可以节省 **40%-70%** 的显存。

---

## 4. Shader 类封装

### 4.1 为什么要封装？

前面的代码中，着色器编译/链接部分有大量重复代码：

```
每次都要：
  1. glCreateShader()
  2. glShaderSource()
  3. glCompileShader()
  4. glGetShaderiv() 检查错误
  5. glCreateProgram()
  6. glAttachShader()
  7. glLinkProgram()
  8. glGetProgramiv() 检查错误
  9. glDeleteShader()
```

每次新增着色器都要重复以上 **9 步**，非常繁琐。

> 💡 封装后只需要一行：`Shader shader(vsCode, fsCode);`

### 4.2 Shader 类设计

```
Shader 类
 ├── 构造函数: 编译 + 链接 + 检查错误，一步完成
 ├── use(): 激活程序
 ├── setBool() / setInt() / setFloat() : 设置 uniform
 ├── setVec3() / setVec4(): 设置向量 uniform
 └── (私有) compile(): 封装编译链接细节
     └── checkCompileErrors(): 统一错误检查
```

### 4.3 核心代码

```cpp
class Shader
{
public:
    unsigned int ID;

    // 构造函数：从源码字符串编译
    Shader(const char* vertexSource, const char* fragmentSource)
    {
        compile(vertexSource, fragmentSource);
    }

    // 激活着色器程序
    void use() const
    {
        glUseProgram(ID);
    }

    // uniform 设置
    void setFloat(const std::string& name, float value) const
    {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setVec4(const std::string& name, float x, float y, float z, float w) const
    {
        glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
    }

private:
    void compile(const char* vertexSource, const char* fragmentSource)
    {
        // 编译顶点着色器
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);
        checkCompileErrors(vertexShader, "VERTEX");

        // 编译片段着色器
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);
        checkCompileErrors(fragmentShader, "FRAGMENT");

        // 链接
        ID = glCreateProgram();
        glAttachShader(ID, vertexShader);
        glAttachShader(ID, fragmentShader);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        // 清理
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void checkCompileErrors(unsigned int shader, const std::string& type)
    {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::SHADER::" << type << "::COMPILATION_FAILED\n"
                          << infoLog << std::endl;
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                          << infoLog << std::endl;
            }
        }
    }
};
```

---

## 5. Shader 类 API 详解

### 5.1 构造函数

```cpp
// 方式一：从源码字符串编译（推荐学习阶段使用）
Shader shader(vertexShaderSource, fragmentShaderSource);

// 方式二：从文件读取编译（推荐实际项目使用）
Shader shader("shader.vs", "shader.fs", true);  // fromFile = true
```

### 5.2 uniform 设置方法

| 方法 | 对应 OpenGL 函数 | 用途 |
|------|------------------|------|
| `setBool(name, val)` | `glUniform1i` | 布尔值（GLSL 中 bool 用 int 表示） |
| `setInt(name, val)` | `glUniform1i` | 整型采样器（纹理单元） |
| `setFloat(name, val)` | `glUniform1f` | 浮点数（时间、偏移等） |
| `setVec3(name, x, y, z)` | `glUniform3f` | 三维向量（颜色、位置） |
| `setVec4(name, x, y, z, w)` | `glUniform4f` | 四维向量（RGBA 颜色） |

### 5.3 glGetUniformLocation

```cpp
// setFloat 内部实现原理：
void setFloat(const std::string& name, float value) const
{
    // 1. 查询 uniform 变量的位置（location）
    int location = glGetUniformLocation(ID, name.c_str());

    // 2. 在对应位置写入值
    glUniform1f(location, value);
}
```

> ⚠️ `glGetUniformLocation` 在每次 `setXXX` 时都会调用。如果频繁设置 uniform，可以缓存 location 以提高性能。但在学习阶段无需担心。

### 5.4 使用示例

```cpp
// 创建着色器
Shader shader(vertexShaderSource, fragmentShaderSource);

// 渲染循环
while (...)
{
    shader.use();                    // 激活

    // 设置 uniform（如果需要）
    shader.setFloat("uOffset", 0.5f);
    shader.setVec4("uColor", 1.0f, 0.5f, 0.2f, 1.0f);

    // 绘制
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
```

---

## 6. 使用 Shader 类重写程序

### 6.1 前后对比

**之前**（~60 行着色器编译代码）：

```cpp
// 顶点着色器编译...
unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
glCompileShader(vertexShader);
// 检查错误...
// 片段着色器编译...
unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
// ...
// 链接...
unsigned int shaderProgram = glCreateProgram();
glAttachShader(shaderProgram, vertexShader);
glAttachShader(shaderProgram, fragmentShader);
glLinkProgram(shaderProgram);
// 检查链接错误...
// 删除着色器对象...
glDeleteShader(vertexShader);
glDeleteShader(fragmentShader);

// 渲染循环
glUseProgram(shaderProgram);
```

**之后**（仅 1 行）：

```cpp
Shader shader(vertexShaderSource, fragmentShaderSource);

// 渲染循环
shader.use();
```

### 6.2 完整的 main.cpp 结构

```cpp
#include "shader.h"

int main()
{
    // 1-4. 初始化 GLFW/窗口/GLAD/视口（不变）

    // 5. 编译着色器 —— 从 ~60 行变为 1 行
    Shader shader(vertexShaderSource, fragmentShaderSource);

    // 6-7. 顶点数据 + VAO/VBO/EBO（不变）

    // 8. 渲染循环 —— 用 shader.use() 替代 glUseProgram()
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 9. 清理（不需要手动删除 shader 程序，因为 Shader 类不负责析构）
    // 在实际项目中应添加析构函数自动清理
    glDeleteProgram(shader.ID);  // 仍需要手动或通过析构释放
}
```

> ⚠️ 当前 `Shader` 类没有析构函数，使用完毕后需要手动 `glDeleteProgram(shader.ID)`。后续学习中会完善 RAII 资源管理。

---

## 7. 关键知识点总结

### 🎯 核心概念

| 概念 | 要点 |
|------|------|
| **EBO** | 存储顶点索引，复用 VBO 中的顶点，节省显存 |
| **glDrawElements** | 索引绘制，用 `GL_ELEMENT_ARRAY_BUFFER` 中的数据 |
| **VAO 记录 EBO** | VAO 解绑时「快照」当前 EBO，不能提前解绑 EBO |
| **索引类型** | `GL_UNSIGNED_BYTE` / `SHORT` / `INT`，按顶点数选择 |
| **线框模式** | `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` |
| **Shader 类** | 封装编译→链接→错误检查，提供 `use()` 和 uniform 设置 |
| **glGetUniformLocation** | 查询 uniform 变量位置的内部机制 |

### 💡 最佳实践

```
EBO 配置顺序（固定）:
glBindVertexArray(VAO)
    ↓
glBindBuffer(GL_ARRAY_BUFFER, VBO)      → 上传顶点
    ↓
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)  → 上传索引 ★
    ↓
glVertexAttribPointer(...) + glEnableVertexAttribArray(...)
    ↓
glBindVertexArray(0)                     ← 不要提前解绑 EBO！

绘制顺序（固定）:
shader.use()
    ↓
glBindVertexArray(VAO)
    ↓
glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0)
```

### ⚠️ 常见错误

```
❌ VAO 解绑前解绑了 EBO
   → VAO 丢失 EBO 关联，绘制时无索引数据
   ✅ 修复: 不要调用 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)

❌ glDrawElements 的 count = 顶点数而不是索引数
   → 只绘制了部分图元或越界
   ✅ 修复: count = 索引数组的长度（矩形为 6）

❌ glDrawElements 的 type 和索引数据类型不匹配
   → 崩溃或绘制错误
   ✅ 修复: 索引是 unsigned int → GL_UNSIGNED_INT

❌ 忘了 include "shader.h"
   → 编译错误
   ✅ 修复: #include "shader.h"

❌ Shader 类中的 uniform 方法名拼写错误
   → uniform 设置无效（不报错，静默失败）
   ✅ 修复: 使用统一的 setXxx 命名规范
```

### 📦 Shader 类后续扩展

当前 `Shader` 类还缺少：

| 特性 | 说明 |
|------|------|
| **析构函数** | 自动调用 `glDeleteProgram()`，防止内存泄漏 |
| **移动语义** | 禁止拷贝，支持移动，确保资源唯一所有权 |
| **更多 uniform 类型** | `setMat3`、`setMat4`、`setVec2` |
| **缓存 location** | 缓存 `glGetUniformLocation` 的结果，提高频繁调用的性能 |

这些内容会在后续学习中逐步完善。

---

## 📎 相关资源

- [LearnOpenGL CN - Hello Triangle (EBO 部分)](https://learnopengl-cn.github.io/01%20Getting%20started/04%20Hello%20Triangle/#_11)
- [OpenGL 文档 - glDrawElements](https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawElements.xhtml)
- [OpenGL 文档 - glPolygonMode](https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glPolygonMode.xhtml)
- [LearnOpenGL CN - 着色器 (Shader 类)](https://learnopengl-cn.github.io/01%20Getting%20started/05%20Shaders/)

---

> **📝 学习日期**：2026-06-06 | **上一个章节**：[Hello Triangle](02-Hello-Triangle-知识点.md)
>
> **下一个章节**：[纹理（Textures）](04-纹理-知识点.md) — 纹理坐标、纹理参数、纹理单元、图片加载

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

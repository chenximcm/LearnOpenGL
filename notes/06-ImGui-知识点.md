# 🖥️ OpenGL 学习笔记（六）：Dear ImGui 调试界面

> 集成 [Dear ImGui](https://github.com/ocornut/imgui) 到 OpenGL 项目，创建实时调试控制面板。

---

## 📑 目录

- [1. Dear ImGui 简介](#1-dear-imgui-简介)
- [2. ImGui 架构与文件结构](#2-imgui-架构与文件结构)
- [3. 集成流程](#3-集成流程)
- [4. 初始化 ImGui](#4-初始化-imgui)
- [5. 渲染循环中的 ImGui](#5-渲染循环中的-imgui)
- [6. 中文字体支持](#6-中文字体支持)
- [7. 常用控件速查](#7-常用控件速查)
- [8. 自定义调试面板](#8-自定义调试面板)
- [9. 清理资源](#9-清理资源)
- [10. 完整流程回顾](#10-完整流程回顾)
- [11. 关键知识点总结](#11-关键知识点总结)

---

## 1. Dear ImGui 简介

### 1.1 什么是 ImGui？

**Dear ImGui** 是一个面向 C++ 的**即时模式图形用户界面库**（Immediate Mode GUI）。

```
传统 GUI（保留模式）：                    ImGui（即时模式）：

创建窗口 → 保存窗口对象                  每帧调用 Begin/End
添加按钮 → 绑定回调函数                  每帧检查按钮返回值
监听事件 → 更新界面                      直接在渲染循环中写 UI
释放窗口 → 销毁对象                      无需手动管理生命周期

特点：代码量大，学习成本高                特点：零 Boilerplate，
      适合复杂应用                       适合调试工具/编辑器
```

### 1.2 为什么在 OpenGL 项目中使用 ImGui？

| 用途 | 说明 |
|------|------|
| 🔧 **调试控制面板** | 实时调整清屏颜色、线框模式、纹理选择等渲染参数 |
| 📊 **性能监控** | 显示 FPS、帧时间、顶点数等性能指标 |
| 🎮 **编辑器工具** | 场景编辑器、材质编辑器、动画控制器 |
| 📐 **变换控制** | 滑条控制旋转角度、缩放大小、平移位置 |
| 🧪 **快速原型** | 不用写 HTML/JS，直接在 C++ 中构建 UI |

### 1.3 官方资源

- **仓库**：[github.com/ocornut/imgui](https://github.com/ocornut/imgui)
- **Wiki**：[github.com/ocornut/imgui/wiki](https://github.com/ocornut/imgui/wiki)
- **示例集成**：`imgui/examples/` 目录下有 GLFW+OpenGL3、SDL+OpenGL3 等模板

---

## 2. ImGui 架构与文件结构

### 2.1 核心文件

ImGui 采用**核心 + 后端**架构：

```
vendor/imgui/
├── imgui.cpp              ★ 核心：窗口管理、控件逻辑、布局系统
├── imgui.h                ★ 核心头文件（所有公开 API）
├── imgui_demo.cpp          Demo 代码（ShowDemoWindow 函数）
├── imgui_draw.cpp          绘制函数（顶点缓冲构建）
├── imgui_tables.cpp        表格控件
├── imgui_widgets.cpp       控件实现（按钮、滑块、输入框等）
├── imgui_internal.h        内部实现（供后端使用）
├── imstb_*.h               stb 单头文件库（字体、文本编辑、矩形打包）
├── imconfig.h               配置选项
│
└── backends/
    ├── imgui_impl_glfw.cpp   ★ GLFW 后端：窗口事件、鼠标/键盘输入
    ├── imgui_impl_glfw.h
    ├── imgui_impl_opengl3.cpp ★ OpenGL3 后端：着色器、渲染
    ├── imgui_impl_opengl3.h
    └── imgui_impl_opengl3_loader.h  OpenGL 函数加载器
```

### 2.2 分层架构

```
┌──────────────────────────────────────────┐
│           你的应用代码                     │
│   （imgui_main.cpp，控件逻辑）            │
├──────────────────────────────────────────┤
│           ImGui 核心层                    │
│   （imgui.cpp, imgui_widgets.cpp 等）     │
├────────────────┬─────────────────────────┤
│  GLFW 后端      │  OpenGL3 后端           │
│ (窗口/输入)      │  (渲染)                │
│ imgui_impl_glfw │ imgui_impl_opengl3      │
└────────┴───────┴─────────────────────────┘
         │                │
         ↓                ↓
      GLFW 窗口         OpenGL 渲染
```

> 这种架构的好处：想换窗口库（SDL/GLFW）或图形 API（OpenGL/DirectX/Vulkan）时，只需换对应的后端文件，核心代码完全不变。

---

## 3. 集成流程

### 3.1 项目文件配置

```xml
<!-- Include 目录 -->
<AdditionalIncludeDirectories>
    vendor\imgui;              <!-- imgui.h -->
    vendor\imgui\backends;     <!-- imgui_impl_glfw.h -->
    ...
</AdditionalIncludeDirectories>

<!-- 需要编译的源文件 -->
<ClCompile Include="vendor\imgui\imgui.cpp" />
<ClCompile Include="vendor\imgui\imgui_demo.cpp" />
<ClCompile Include="vendor\imgui\imgui_draw.cpp" />
<ClCompile Include="vendor\imgui\imgui_tables.cpp" />
<ClCompile Include="vendor\imgui\imgui_widgets.cpp" />
<ClCompile Include="vendor\imgui\backends\imgui_impl_glfw.cpp" />
<ClCompile Include="vendor\imgui\backends\imgui_impl_opengl3.cpp" />
```

### 3.2 包含头文件

```cpp
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
```

### 3.3 完整生命周期

```
初始化阶段（只做一次）：
  创建上下文  →  加载字体  →  设置风格  →  初始化后端

每帧流程：
  开始新帧  →  添加控件  →  渲染 ImGui

清理阶段：
  Shutdown 后端  →  销毁上下文
```

---

## 4. 初始化 ImGui

### 4.1 创建上下文

```cpp
IMGUI_CHECKVERSION();                // 调试检查：确保头文件与 .cpp 版本一致
ImGui::CreateContext();              // 创建全局上下文（窗口状态、控件状态等）
ImGuiIO& io = ImGui::GetIO();        // 获取 IO 对象，用于配置
```

`IMGUI_CHECKVERSION()` 是一个调试宏，检查 `imgui.h` 和 `imgui.cpp` 是否来自同一版本。如果版本不匹配，会在调试输出中报告。

### 4.2 设置风格

```cpp
ImGui::StyleColorsDark();    // 深色主题（默认推荐，适合游戏/工具）
// ImGui::StyleColorsLight(); // 浅色主题
// ImGui::StyleColorsClassic(); // 经典 ImGui 风格
```

### 4.3 初始化后端

```cpp
// GLFW 后端初始化
// 参数 2：install_callbacks = true
//   true  → ImGui 接管 GLFW 的鼠标/键盘/滚轮回调
//   false → 自己手动调用 ImGui 的输入函数
// 通常设为 true 最简单
ImGui_ImplGlfw_InitForOpenGL(window, true);

// OpenGL3 后端初始化
// 参数：GLSL 版本字符串，必须与顶点着色器的 #version 一致
ImGui_ImplOpenGL3_Init("#version 330");
```

---

## 5. 渲染循环中的 ImGui

### 5.1 每帧流程

```cpp
while (!glfwWindowShouldClose(window))
{
    // ===== 1. 你的 OpenGL 渲染 =====
    glClear(GL_COLOR_BUFFER_BIT);
    // ... 绘制 3D 场景 ...

    // ===== 2. ImGui 开始新帧 =====
    ImGui_ImplOpenGL3_NewFrame();    // OpenGL3 后端新帧
    ImGui_ImplGlfw_NewFrame();       // GLFW 后端新帧
    ImGui::NewFrame();               // ImGui 核心新帧

    // ===== 3. 添加控件 =====
    ImGui::ShowDemoWindow();         // 可选的 Demo 窗口
    ImGui::Begin("我的面板");
    ImGui::Text("Hello, ImGui!");
    ImGui::End();

    // ===== 4. ImGui 渲染 =====
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // ===== 5. 交换缓冲 =====
    glfwSwapBuffers(window);
}
```

### 5.2 为什么先渲染场景再渲染 ImGui？

```
场景 → 先画到帧缓冲
ImGui → 叠加在场景之上

如果反过来：
  ImGui 先画 → 场景后画 → 场景会把 ImGui 覆盖掉！
```

ImGui 的渲染启用了**混合（Blending）**，它绘制的是半透明的 UI 层，叠加在 3D 场景之上。

---

## 6. 中文字体支持

### 6.1 为什么中文显示为乱码？

ImGui 默认字体 **ProggyClean** 只包含 ASCII 字符，没有中文字形。需要手动加载中文字体。

### 6.2 解决方案

```cpp
ImGuiIO& io = ImGui::GetIO();

// 在 ImGui 初始化时加载系统字体
io.Fonts->AddFontFromFileTTF(
    "C:/Windows/Fonts/msyh.ttc",       // 字体文件路径
    16.0f,                              // 字号大小
    nullptr,                            // 字体配置（默认）
    io.Fonts->GetGlyphRangesChineseFull()  // 完整中文字符集
);
```

### 6.3 字体文件说明

| 系统字体 | 文件 | 特点 |
|---------|------|------|
| **微软雅黑** | `msyh.ttc` | 清晰美观，UI 首选 |
| 黑体 | `simhei.ttf` | 笔画均匀 |
| 等线 | `deng.ttf` | 现代简洁 |

> ⚠️ 字体文件路径写死为 `C:/Windows/Fonts/`，这是 Windows 系统字体目录。如果字体文件不存在或路径不对，`AddFontFromFileTTF` 会返回 nullptr，ImGui 回退到默认字体（中文仍然乱码，但不会崩溃）。

### 6.4 字符范围

```cpp
// 常用字符范围（推荐在大多数情况下使用）：
io.Fonts->GetGlyphRangesChineseFull();
// 包含：CJK 统一表意文字（U+4E00–U+9FFF）、标点符号等

// 更精简的范围（节省纹理空间）：
io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
// 只包含常用的 3500+ 个简体中文字符
```

---

## 7. 常用控件速查

### 7.1 文本显示

```cpp
ImGui::Text("Hello, 世界");             // 纯文本
ImGui::Text("FPS: %.1f", io.Framerate);  // 格式化文本
ImGui::BulletText("带项目符号的文本");     // 带圆点
ImGui::Separator();                      // 分割线
ImGui::Spacing();                        // 间距
```

### 7.2 按钮

```cpp
if (ImGui::Button("点击我")) {
    // 按钮被点击时的回调
}

ImGui::SmallButton("小按钮");    // 更紧凑的按钮

ImGui::SameLine();               // 让下一个控件在同一行（不换行）
```

### 7.3 复选框

```cpp
static bool wireframeMode = false;
ImGui::Checkbox("线框模式", &wireframeMode);
// 用户勾选后 wireframeMode 自动变为 true
```

### 7.4 单选按钮

```cpp
static int textureMode = 0;
ImGui::RadioButton("双纹理混合", &textureMode, 0);
ImGui::RadioButton("仅纹理 1", &textureMode, 1);
ImGui::RadioButton("仅纹理 2", &textureMode, 2);
// textureMode 的值自动切换为 0/1/2
```

### 7.5 滑块

```cpp
float rotateSpeed = 50.0f;
ImGui::SliderFloat("旋转速度", &rotateSpeed, 0.0f, 360.0f, "%.0f°/秒");

float scaleValue = 0.5f;
ImGui::SliderFloat("缩放", &scaleValue, 0.1f, 2.0f);

int count = 10;
ImGui::SliderInt("数量", &count, 1, 100);
```

### 7.6 颜色选择器

```cpp
float clearColor[3] = { 0.2f, 0.3f, 0.3f };
ImGui::ColorEdit3("清屏颜色", clearColor);
// clearColor 数组自动更新
```

### 7.7 窗口控制

```cpp
// 基本窗口
ImGui::Begin("窗口标题");
// ... 控件 ...
ImGui::End();

// 自动调整大小
ImGui::Begin("小窗口", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

// 有关闭按钮的窗口
static bool showWindow = true;
ImGui::Begin("可关闭窗口", &showWindow);
// showWindow = false 时窗口关闭
ImGui::End();

// 无标题栏（纯面板）
ImGui::Begin("无边框", nullptr, ImGuiWindowFlags_NoTitleBar);
```

---

## 8. 自定义调试面板

### 8.1 面板设计

```
┌─ 🎛️ 调试控制面板 ──────────────────┐
│                                      │
│  性能信息                             │
│  ────────────────────────────────     │
│  FPS: 59.8                            │
│  帧时间: 16.72 ms                     │
│                                      │
│  渲染设置                             │
│  ────────────────────────────────     │
│  [■━━━━━━━━━━━━━□] 清屏颜色           │
│  [✓] 线框模式                        │
│                                      │
│  纹理选择                             │
│  ────────────────────────────────     │
│  ○ 双纹理混合   ○ 仅纹理1   ● 仅纹理2 │
│                                      │
│  变换控制                             │
│  ────────────────────────────────     │
│  [✓] 启用变换                        │
│  旋转速度 ━━━━━●━━━━━━━ 50°/秒        │
│  缩放大小 ━━━●━━━━━━━━━ 0.50          │
│                                      │
│  帮助                                │
│  ────────────────────────────────     │
│  ESC — 退出程序                      │
└──────────────────────────────────────┘
```

### 8.2 核心代码模式

```cpp
// 每个 ImGui 控件遵循「即时模式」模式：
//
//   ImGui::控件名(参数..., &绑定的变量);
//
// 绑定变量会在控件交互时自动更新，
// 你不需要写任何回调函数或事件处理代码。

// 示例：颜色选择器
float clearColor[3] = { 0.2f, 0.3f, 0.3f };
ImGui::ColorEdit3("清屏颜色", clearColor);
// → clearColor 数组内容自动更新
// → glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

// 示例：复选框
bool wireframeMode = false;
ImGui::Checkbox("线框模式", &wireframeMode);
// → wireframeMode 自动切换 true/false
// → glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);

// 示例：滑块
float rotateSpeed = 50.0f;
ImGui::SliderFloat("旋转速度", &rotateSpeed, 0.0f, 360.0f);
// → rotateSpeed 自动更新
// → trans = glm::rotate(trans, time * glm::radians(rotateSpeed), ...);
```

### 8.3 ⚠️ 常见陷阱：UI 变量绑定了，但渲染没跟上

这是 ImGui 新手最容易犯的错误——**UI 控件确实修改了变量值，但渲染代码根本没用这个变量**。

以纹理选择为例：

```cpp
// ❌ 错误写法

// UI 部分 —— 值确实变了
int textureMode = 0;
ImGui::RadioButton("双纹理混合", &textureMode, 0);
ImGui::RadioButton("仅 container", &textureMode, 1);
ImGui::RadioButton("仅 awesomeface", &textureMode, 2);

// 渲染部分 —— 完全没读 textureMode！
// 不管用户选什么，永远绑两张纹理
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, texture1);
glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, texture2);
// ↑ 错误：textureMode 被忽略了！
```

记住：**ImGui 只负责修改变量值，不负责让你的代码去读它。**

```cpp
// ✅ 正确写法

// 渲染部分根据 textureMode 做分支
if (textureMode == 0)
{
    // 双纹理混合
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture1);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texture2);
}
else if (textureMode == 1)
{
    // 仅 container：两个纹理单元都绑同一张
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture1);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texture1);
}
else
{
    // 仅 awesomeface
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture2);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texture2);
}
```

**检查清单**：每添加一个 ImGui 控件后，问自己三个问题：

| # | 问题 | 说明 |
|:-:|------|------|
| 1 | 控件绑定了哪个变量？ | `ImGui::SliderFloat("名", &变量, ...)` |
| 2 | 渲染代码在哪里读取这个变量？ | `glClearColor(变量[0], ...)` |
| 3 | 读取路径被正确执行了吗？ | 条件分支、函数参数等 |

> 本例中的纹理选择就是一个典型：RadioButton 确实修改了 `textureMode`，但纹理绑定部分的 `if/else` 压根没写，导致 UI 怎么点都没反应。

### 8.3 Demo 窗口

```cpp
// ImGui 内置了一个完整的 Demo 窗口，展示了所有控件的用法
// 强烈建议打开看看，比任何教程都直观
static bool showDemoWindow = true;
if (showDemoWindow)
    ImGui::ShowDemoWindow(&showDemoWindow);
```

Demo 窗口包含：
- 所有控件类型（按钮、滑块、树、表格、拖拽等）
- 布局示例（多列、分组、Tab）
- 样式编辑器（实时调整颜色/圆角/间距）
- 完整的交互示例

---

## 9. 清理资源

### 9.1 清理顺序

```cpp
// 1. 先 Shutdown 后端（释放 OpenGL 资源、取消 GLFW 回调）
ImGui_ImplOpenGL3_Shutdown();   // 释放 OpenGL 3 后端资源
ImGui_ImplGlfw_Shutdown();      // 取消 GLFW 回调注册

// 2. 再销毁 ImGui 上下文（释放所有 UI 状态）
ImGui::DestroyContext();

// 3. 最后清理你自己的 OpenGL 资源
glDeleteVertexArrays(1, &VAO);
glDeleteTextures(1, &texture);
glfwTerminate();
```

> ⚠️ **清理顺序很重要**：先 Shutdown 后端，再 Destroy 上下文。反了会导致崩溃。

---

## 10. 完整流程回顾

### 初始化

```
GLFW 初始化
    ↓
创建窗口（1200×800，大一点方便看 ImGui）
    ↓
GLAD 初始化
    ↓
编译着色器 + 创建 VAO/VBO/EBO + 加载纹理
    ↓
★ ImGui 初始化：
   CreateContext()
   io.Fonts->AddFontFromFileTTF("msyh.ttc", 16.0f, ..., 中文范围)
   StyleColorsDark()
   ImGui_ImplGlfw_InitForOpenGL(window, true)
   ImGui_ImplOpenGL3_Init("#version 330")
```

### 渲染循环

```
glClear(GL_COLOR_BUFFER_BIT)
    ↓
绘制 3D 场景（绑定纹理、计算变换矩阵、绘制矩形）
    ↓
★ ImGui 帧开始：
   ImGui_ImplOpenGL3_NewFrame()
   ImGui_ImplGlfw_NewFrame()
   ImGui::NewFrame()
    ↓
★ ImGui 控件代码：
   ShowDemoWindow()
   Begin("调试面板") → 各种控件 → End()
    ↓
★ ImGui 渲染：
   ImGui::Render()
   ImGui_ImplOpenGL3_RenderDrawData()
    ↓
glfwSwapBuffers(window)
```

### 清理

```
ImGui_ImplOpenGL3_Shutdown()
ImGui_ImplGlfw_Shutdown()
ImGui::DestroyContext()
    ↓
glDeleteVertexArrays / glDeleteTextures / glfwTerminate()
```

---

## 11. 关键知识点总结

### 🎯 核心概念

| 概念 | 要点 |
|------|------|
| **即时模式 GUI** | 每帧都重建 UI，没有持久化对象 |
| **核心 + 后端** | 核心库不依赖窗口系统或图形 API |
| **每帧三阶段** | NewFrame → 添加控件 → Render |
| **控件绑定** | 控件通过指针/引用直接读写你的变量 |
| **中文字体** | 需要 `AddFontFromFileTTF` 手动加载 |
| **初始化顺序** | CreateContext → 字体 → 风格 → 后端 |
| **清理顺序** | Shutdown 后端 → Destroy 上下文 |

### 📦 新增文件

| 文件 | 说明 |
|------|------|
| `vendor/imgui/*.cpp` | ImGui 核心源码（7 个文件） |
| `vendor/imgui/backends/imgui_impl_glfw.*` | GLFW 后端 |
| `vendor/imgui/backends/imgui_impl_opengl3.*` | OpenGL3 后端 |
| `src/imgui_main.cpp` | 集成 Demo（当前活动入口） |

### 💡 最佳实践

```
初始化：
IMGUI_CHECKVERSION() → 调试版本检查
CreateContext() → 只创建一次
AddFontFromFileTTF → 在 Init 后端之前
IniFilename = "imgui.ini" → 自动保存窗口布局

渲染循环：
先绘制 3D 场景 → 再渲染 ImGui（叠加层）
每帧必须完整执行 NewFrame → Render 流程
Begin/End 必须成对，否则断言失败

清理：
后端 Shutdown → DestroyContext（顺序固定！）

中文：
字体路径用 C:/Windows/Fonts/msyh.ttc
字体加载在 CreateContext 之后立即执行
字号 16.0f 适合 1080p 屏幕
```

### ⚠️ 常见错误

```
❌ 忘记 IMGUI_CHECKVERSION()
   → 头文件和 .cpp 版本不一致时会出现诡异崩溃
   ✅ 修复：在 CreateContext 之前调用

❌ Begin() 和 End() 不配对
   → 断言失败或布局错乱
   ✅ 修复：确保每个 Begin 都有对应的 End

❌ NewFrame 没调用就到 Render
   → 断言失败 "No NewFrame() called"
   ✅ 修复：每帧依次调用 NewFrame → 控件 → Render

❌ 清理顺序错误
   → 先 DestroyContext 再 Shutdown 后端 → 访问已释放内存
   ✅ 修复：先 Shutdown 后端，再 DestroyContext

❌ 中文字体路径不对
   → 中文仍然显示乱码（不会崩溃）
   ✅ 修复：用完整路径 C:/Windows/Fonts/msyh.ttc

❌ 字体加载在 Init 后端之后
   → 字体图集未正确上传到 GPU
   ✅ 修复：AddFontFromFileTTF 在 ImGui_ImplOpenGL3_Init 之前

❌ UI 变量绑定了，但渲染代码没使用
   → 控件修改了变量值，但渲染逻辑根本不读它
   → 用户怎么点都没反应（如纹理选择切了但画面不变）
   ✅ 修复：检查渲染代码是否真的读取了该变量
```

### 🔧 常用场景速查

| 需求 | 代码 |
|------|------|
| 显示 FPS | `ImGui::Text("FPS: %.1f", io.Framerate)` |
| 颜色选择 | `ImGui::ColorEdit3("颜色", float[3])` |
| 开关切换 | `ImGui::Checkbox("标签", &boolVar)` |
| 单选组 | `ImGui::RadioButton("名", &intVar, value)` |
| 滑块控制 | `ImGui::SliderFloat("名", &var, min, max)` |
| 按钮回调 | `if (ImGui::Button("名")) { /* ... */ }` |
| 显示图片 | `ImGui::Image((void*)(intptr_t)texID, size)` |
| 显示 Demo | `ImGui::ShowDemoWindow(&showDemo)` |

---

## 📎 相关资源

- [Dear ImGui GitHub](https://github.com/ocornut/imgui)
- [ImGui Wiki](https://github.com/ocornut/imgui/wiki)
- [ImGui 集成示例 (GLFW + OpenGL3)](https://github.com/ocornut/imgui/tree/master/backends)
- [ImGui 控件列表](https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples)
- [ImFontAtlas::AddFontFromFileTTF 文档](https://github.com/ocornut/imgui/blob/master/docs/FONTS.md)

---

> **📝 学习日期**：2026-06-07 | **上一个章节**：[变换](05-变换-知识点.md)
>
> **下一个章节**：[坐标系统（Coordinate Systems）](06-坐标系统-知识点.md) — 局部空间、世界空间、观察空间、裁剪空间、投影矩阵

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

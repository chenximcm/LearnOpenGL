# LearnOpenGL

跟随 [LearnOpenGL CN](https://learnopengl-cn.github.io/) 教程学习 OpenGL 的实践项目。

## 环境

- **语言**: C++ (C++17)
- **图形 API**: OpenGL 3.3
- **窗口管理**: GLFW
- **OpenGL 加载器**: GLAD
- **数学库**: GLM
- **调试 UI**: Dear ImGui
- **图像加载**: stb_image
- **构建工具**: Visual Studio 2022
- **平台**: Windows 11 (NVIDIA GeForce RTX 5080)

## 项目结构

```
LearnOpenGL/
├── src/                        # 源代码（按功能模块分文件夹）
│   ├── common/                 # 共享工具
│   │   ├── shader.h            # Shader 类（着色器编译/链接/Uniform 封装）
│   │   └── stb_image.cpp       # stb_image 实现文件
│   ├── triangle/               # Chapter 2-3: Hello Triangle + EBO
│   │   └── main.cpp            # （当前未启用）
│   ├── textures/               # Chapter 4: 纹理
│   │   └── textures_main.cpp   # （当前未启用）
│   ├── transformations/        # Chapter 5: 变换 + 双纹理
│   │   └── transformations_main.cpp  # （当前未启用）
│   ├── imgui/                  # Chapter 6: Dear ImGui 调试界面
│   │   └── imgui_main.cpp      # （当前未启用）
│   └── coordinates/            # Chapter 7: 坐标系统（当前启动项）
│       └── coordinates_main.cpp
├── shaders/                    # GLSL 着色器文件（按模块分文件夹）
│   ├── triangle/
│   │   ├── triangle.vert
│   │   └── triangle.frag
│   ├── textures/
│   │   ├── texture.vert        # 纹理顶点着色器
│   │   ├── texture.frag        # 纹理片段着色器（单纹理）
│   │   └── texture_combined.frag # 纹理片段着色器（双纹理混合）
│   ├── transformations/
│   │   └── transform.vert      # 变换顶点着色器
│   └── coordinates/
│       └── coordinate_system.vert # 坐标系统顶点着色器（MVP）
├── textures/                   # 纹理资源
│   ├── container.jpg           # 容器贴图
│   └── awesomeface.png         # 笑脸贴图
├── notes/                      # 学习笔记（详细知识点整理）
│   ├── 01-创建窗口-知识点.md
│   ├── 02-Hello-Triangle-知识点.md
│   ├── 03-EBO与Shader封装-知识点.md
│   ├── 04-纹理-知识点.md
│   ├── 05-变换-知识点.md
│   └── 06-ImGui-知识点.md
├── vendor/                     # 第三方依赖
│   ├── include/
│   │   ├── GLAD/               # OpenGL 函数指针加载
│   │   └── stb_image.h         # 图像加载头文件
│   ├── GLFW/                   # 窗口和输入管理（含 glfw3.dll）
│   ├── glm/                    # GLM 数学库（矩阵/向量变换）
│   └── imgui/                  # Dear ImGui 调试界面库
├── glad_src/                   # GLAD 源码
├── LearnOpenGL.sln             # Visual Studio 解决方案
├── LearnOpenGL.vcxproj         # 项目文件
└── README.md
```

> **多文件切换**：不同章节的入口写在对应 `src/*.cpp` 中，通过在 `.vcxproj` 中排除/包含文件来选择当前启动项。当前默认启动 `coordinates_main.cpp`。

## 学习进度

| 日期 | 章节 | 内容 | 代码入口 | 笔记 |
|------|------|------|----------|------|
| 2026-06-03 | 创建窗口 | 配置 GLFW + GLAD，显示窗口，设置清屏颜色 | — | [笔记](notes/01-创建窗口-知识点.md) |
| 2026-06-03 | Hello Triangle | VAO/VBO/着色器，绘制第一个三角形 | `main.cpp` | [笔记](notes/02-Hello-Triangle-知识点.md) |
| 2026-06-06 | EBO 与 Shader 封装 | 索引缓冲对象、索引绘制、Shader 类封装编译链接流程 | `main.cpp` | [笔记](notes/03-EBO与Shader封装-知识点.md) |
| 2026-06-07 | 纹理 | stb_image 加载图片、纹理坐标、纹理单元、双纹理混合 | `textures_main.cpp` | [笔记](notes/04-纹理-知识点.md) |
| 2026-06-07 | 变换 | GLM 数学库、平移/旋转/缩放矩阵、矩阵组合顺序、基于时间的动画 | `transformations_main.cpp` | [笔记](notes/05-变换-知识点.md) |
| 2026-06-07 | Dear ImGui 调试界面 | ImGui 集成、调试控制面板（清屏颜色、线框模式、纹理切换、FPS、变换参数） | `imgui_main.cpp` | [笔记](notes/06-ImGui-知识点.md) |
| 2026-06-07 | 坐标系统 | 局部→世界→视图→裁剪→屏幕、MVP 矩阵、深度测试、3D 立方体、十个立方体阵列 | `coordinates_main.cpp` | [笔记](notes/07-坐标系统-知识点.md) |

## 各章节知识点

### Chapter 1: 创建窗口
- GLFW 窗口创建与配置
- GLAD 加载 OpenGL 函数指针
- 视口（Viewport）与回调
- 渲染循环与双缓冲

### Chapter 2: Hello Triangle
- VAO / VBO / EBO 核心概念
- 顶点着色器与片段着色器（GLSL）
- 着色器编译与链接
- 三角形与矩形绘制

### Chapter 3: EBO 与 Shader 封装
- 索引缓冲对象（EBO）减少顶点重复
- `Shader` 类封装：读取文件 → 编译 → 链接 → Uniform 设置

### Chapter 4: 纹理
- 纹理坐标（UV）映射
- `stb_image.h` 加载图片
- `glTexImage2D` + `glGenerateMipmap`
- 纹理参数（包裹/过滤/Mipmap）
- 纹理单元（`glActiveTexture`）与双纹理混合
- 片段着色器 `sampler2D` + `mix()`

### Chapter 5: 变换
- GLM 数学库：`glm::translate` / `rotate` / `scale`
- 矩阵组合顺序（TRS：先缩放 → 再旋转 → 最后平移）
- `uniform mat4` 在顶点着色器中变换
- `glfwGetTime()` 驱动连续动画
- CPU vs GPU 变换性能对比

### Chapter 6: Dear ImGui 调试界面
- ImGui 在 GLFW + OpenGL3 中的初始化流程
- 自定义调试面板：
  - 清屏颜色调整（`ColorEdit3`）
  - 线框模式切换（`Checkbox`）
  - 纹理选择切换（`RadioButton`）
  - FPS 实时显示（`Text`）
  - 变换参数控制（`Slider`）
- ImGui 渲染管线与事件处理

### Chapter 7: 坐标系统
- **五个坐标空间**：局部(Local) → 世界(World) → 视图(View) → 裁剪(Clip) → 屏幕(Screen)
- **三大变换矩阵（MVP）**：
  - `model` 矩阵：局部 → 世界（每个物体独立）
  - `view` 矩阵：世界 → 视图（模拟相机，`glm::lookAt`）
  - `projection` 矩阵：视图 → 裁剪（`glm::perspective` 透视投影）
- **深度测试**：`glEnable(GL_DEPTH_TEST)` + `glClear(GL_DEPTH_BUFFER_BIT)`
- **3D 立方体**：24 个顶点 + 36 个索引，6 个面各自独立纹理坐标
- **十个立方体阵列**：同一份 VAO 数据，不同的 Model 矩阵实现多次绘制
- **线框模式**：`glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` 观察 3D 结构

## 依赖说明

| 库 | 用途 | 集成方式 |
|----|------|----------|
| [GLFW](https://www.glfw.org/) | 窗口创建、输入事件处理 | 预编译库 + DLL |
| [GLAD](https://glad.dav1d.de/) | OpenGL 函数指针加载 | 在线生成 `.c` + `.h` |
| [GLM](https://github.com/g-truc/glm) | 矩阵/向量数学运算 | Header-only |
| [stb_image](https://github.com/nothings/stb) | 图片文件加载 | Header-only（一个 `.cpp`） |
| [Dear ImGui](https://github.com/ocornut/imgui) | 即时模式调试 GUI | 源码集成 |

### 改动记录

- **2026-06-04**: 项目初始化。配置 Visual Studio 解决方案，集成 GLFW 和 GLAD，完成第一个窗口程序。
- **2026-06-06**: 完成 Hello Triangle（VAO/VBO/着色器），新增 EBO 索引绘制，创建 `Shader` 类封装编译链接流程。
- **2026-06-07**: 纹理章节 —— 集成 stb_image，支持纹理坐标、纹理单元、双纹理混合。
- **2026-06-07**: 变换章节 —— 集成 GLM 数学库，实现平移/旋转/缩放变换矩阵与实时动画。
- **2026-06-07**: Dear ImGui 集成 —— 添加 ImGui 依赖，创建实时调试控制面板，修复纹理选择 bug，调整 UI 布局。
- **2026-06-07**: 坐标系统 —— MVP 矩阵（model/view/projection）、深度测试（Z-buffer）、3D 立方体渲染、十个立方体阵列展示矩阵多样性。

> 📝 每次完成新章节后，在这里更新进度和改动记录。

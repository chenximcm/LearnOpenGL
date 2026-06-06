# LearnOpenGL

跟随 [LearnOpenGL CN](https://learnopengl-cn.github.io/) 教程学习 OpenGL 的实践项目。

## 环境

- **语言**: C++ (C++17)
- **图形 API**: OpenGL 3.3
- **窗口管理**: GLFW
- **OpenGL 加载器**: GLAD
- **构建工具**: Visual Studio 2022
- **平台**: Windows 11 (NVIDIA GeForce RTX 5080)

## 项目结构

```
LearnOpenGL/
├── src/                    # 源代码
│   ├── main.cpp            # 主程序入口
│   └── shader.h            # Shader 类（着色器封装）
├── vendor/                 # 第三方依赖
│   └── include/
│       ├── GLAD/           # OpenGL 函数指针加载
│       └── GLFW/           # 窗口和输入管理
├── LearnOpenGL.sln         # Visual Studio 解决方案
├── LearnOpenGL.vcxproj     # 项目文件
├── notes/                  # 学习笔记
│   ├── 01-创建窗口-知识点.md
│   ├── 02-Hello-Triangle-知识点.md
│   └── 03-EBO与Shader封装-知识点.md
└── README.md
```

## 学习进度

| 日期 | 内容 | 链接 |
|------|------|------|
| 2026-06-03 | 创建窗口 — 配置 GLFW + GLAD，显示窗口，设置清屏颜色 | [创建窗口](https://learnopengl-cn.github.io/01%20Getting%20started/02%20Creating%20a%20window/) |
| 2026-06-05 | 知识点整理 — 创建窗口章节知识博客 | [笔记](notes/01-创建窗口-知识点.md) |
| 2026-06-06 | Hello Triangle — VAO/VBO/着色器，绘制第一个三角形 | [Hello Triangle](https://learnopengl-cn.github.io/01%20Getting%20started/04%20Hello%20Triangle/) |
| 2026-06-06 | 知识点整理 — Hello Triangle 章节知识博客 | [笔记](notes/02-Hello-Triangle-知识点.md) |
| 2026-06-06 | EBO 与 Shader 封装 — 索引缓冲对象、索引绘制、Shader 类封装 | [EBO与Shader封装](notes/03-EBO与Shader封装-知识点.md) |

### 改动记录

- **2026-06-03**: 项目初始化。配置 Visual Studio 解决方案，集成 GLFW 和 GLAD，完成第一个窗口程序。
- **2026-06-06**: 完成 Hello Triangle（VAO/VBO/着色器），新增 EBO 索引绘制，创建 Shader 类封装编译链接流程。

> 📝 每次完成新章节后，在这里更新进度和改动记录。

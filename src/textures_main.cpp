/**
 * ============================================================
 *  纹理（Textures）— LearnOpenGL 第 6 章
 * ============================================================
 *
 * 知识点覆盖：
 *   1. 纹理坐标（UV 坐标）—— 让顶点告诉着色器「贴图的哪个角落在哪个顶点」
 *   2. stb_image.h —— 加载图片文件为像素数据
 *   3. glTexImage2D + glGenerateMipmap —— 把像素数据上传到 GPU
 *   4. 纹理参数（包裹/过滤/Mipmap）—— 控制纹理怎么贴
 *   5. 纹理单元（Texture Unit）—— 同时使用多张纹理
 *   6. 片段着色器中的 sampler2D —— 在 GPU 上采样纹理
 *
 * 运行方式：在 VS 中把 textures_main.cpp 设为「启动文件」，
 * 或在 CMake 中将 EXE 入口指向此文件。
 */

// ============================== 头文件 ==============================
#include <glad/glad.h>

// 告诉 GLFW 不要包含 OpenGL 头文件（由 GLAD 提供）
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

// 自定义 Shader 类（封装编译/链接/Uniform）
#include "shader.h"

// stb_image 的声明部分（实现在 stb_image.cpp 中）
#include "../vendor/include/stb_image.h"


// ============================== 回调函数 ==============================

/**
 * 窗口大小变化时的回调
 *
 * 当用户拖动窗口边缘改变大小时，自动调整视口，
 * 防止画面拉伸或只渲染到窗口的一部分。
 */
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


/**
 * 简单的输入处理（按 ESC 关闭窗口）
 */
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}


// ============================== 着色器文件 ==============================
//
// 着色器代码现在存放在独立的文件中（推荐做法）：
//   shaders/texture.vert          —— 顶点着色器
//   shaders/texture.frag          —— 片段着色器（单纹理）
//   shaders/texture_combined.frag —— 片段着色器（双纹理混合）
//
// 这样做的优点：
//   ✅ 语法高亮 —— 编辑器中 GLSL 代码有颜色
//   ✅ 独立编辑 —— 可以单独用文本编辑器打开着色器文件
//   ✅ 更容易复用 —— 后续章节可以直接使用这些着色器文件
//   ✅ 不用在 C++ 字符串里写转义符 —— #version 直接写，不用加 \n
//
// 如果想直接看 GLSL 源码，不用打开 C++ 文件：
//   shaders/texture.vert
//   shaders/texture.frag
//   shaders/texture_combined.frag


// ============================== 纹理加载工具函数 ==============================

/**
 * 从图片文件创建 OpenGL 纹理
 *
 * 这是纹理学习的核心函数，完整流程：
 *
 * 步骤 1  —— 生成纹理对象          glGenTextures
 * 步骤 2  —— 绑定纹理对象          glBindTexture
 * 步骤 3  —— 设置纹理参数           glTexParameteri（包裹/过滤/Mipmap）
 * 步骤 4  —— stbi_load 加载图片     CPU 端解码 JPEG/PNG → 像素数组
 * 步骤 5  —— 上传到 GPU            glTexImage2D
 * 步骤 6  —— 生成 Mipmap           glGenerateMipmap
 * 步骤 7  —— 释放 CPU 内存          stbi_image_free
 *
 * @param path     图片文件路径（相对于工作目录）
 * @param flipY    是否翻转 Y 轴（OpenGL 的纹理原点在左下角，图片原点通常
 *                 在左上角，所以一般需要翻转）
 * @return         创建成功的纹理 ID
 */
unsigned int loadTexture(const char* path, bool flipY = true)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // ====== 1. 设置纹理环绕方式（Texture Wrapping） ======
    //
    // 当纹理坐标超出 [0, 1] 范围时怎么办？有 4 种选择：
    //   GL_REPEAT          ➜ 重复平铺（默认）
    //   GL_MIRRORED_REPEAT ➜ 镜像重复
    //   GL_CLAMP_TO_EDGE   ➜ 拉伸边缘像素
    //   GL_CLAMP_TO_BORDER ➜ 显示设定的边框颜色
    //
    // S = 水平方向（相当于 U / X）
    // T = 垂直方向（相当于 V / Y）
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // ====== 2. 设置纹理过滤（Texture Filtering） ======
    //
    // 当纹理被放大/缩小时，OpenGL 如何计算像素颜色？
    //
    // 缩小（MIN）：
    //   GL_LINEAR           ➜ 取最近的 4 个纹素的加权平均（模糊但平滑）
    //   GL_NEAREST          ➜ 取最近的 1 个纹素（清晰但有锯齿）
    //   GL_LINEAR_MIPMAP_LINEAR  ➜ 三线性过滤（效果最好）
    //
    // 放大（MAG）：
    //   GL_LINEAR  ➜ 双线性插值（平滑）
    //   GL_NEAREST ➜ 最近邻（像素风）
    //
    // ★ 注意：放大过滤器不能使用 Mipmap 相关的选项
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // ====== 3. 加载图片 ======
    int width, height, nrChannels;

    // stb_image 默认 Y 轴原点在左上角，而 OpenGL 纹理原点在左下角
    // 所以需要垂直翻转，否则贴图是倒着的
    stbi_set_flip_vertically_on_load(flipY);

    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (data)
    {
        // ====== 4. 上传纹理数据到 GPU ======
        //
        // glTexImage2D 参数详解：
        //   1. target      ➜ GL_TEXTURE_2D（目标纹理类型）
        //   2. level       ➜ 0（Mipmap 层级，0 = 基础层）
        //   3. internalFmt ➜ 内部格式（GL_RGB / GL_RGBA，GPU 端存储格式）
        //   4+5. width,height  ➜ 图片尺寸
        //   6. border      ➜ 必须为 0（历史遗留参数）
        //   7. format      ➜ 输入数据格式（与 stbi 加载的通道数匹配）
        //   8. type        ➜ 数据类型（GL_UNSIGNED_BYTE = 每个通道 1 字节）
        //   9. pixels      ➜ 像素数据指针
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

        // ====== 5. 生成 Mipmap ======
        //
        // Mipmap 是什么？
        //   当物体远离相机时，纹理在屏幕上只占很少像素，
        //   如果还用原始分辨率采样，会产生「闪烁」和「摩尔纹」。
        //   Mipmap 是一系列逐级缩小的纹理（1/2, 1/4, 1/8...），
        //   OpenGL 自动选择合适的层级来采样，既省带宽又提高画质。
        glGenerateMipmap(GL_TEXTURE_2D);

        std::cout << "✓ 纹理加载成功: " << path
                  << " (" << width << " × " << height << ", "
                  << nrChannels << " 通道)" << std::endl;
    }
    else
    {
        std::cerr << "✗ 纹理加载失败: " << path << std::endl;
    }

    // ====== 6. 释放 CPU 上的图片数据 ======
    // 数据已经上传到 GPU 显存，CPU 端可以释放了
    stbi_image_free(data);

    return textureID;
}


// ============================== 主函数 ==============================

int main()
{
    // ====================================================================
    // 阶段 1：初始化 GLFW + 创建窗口
    // ====================================================================
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "LearnOpenGL - Textures", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "⨯ Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // ====================================================================
    // 阶段 2：初始化 GLAD
    // ====================================================================
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "⨯ Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // ====================================================================
    // 阶段 3：设置视口
    // ====================================================================
    glViewport(0, 0, 800, 600);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // ====================================================================
    // 阶段 4：编译着色器
    // ====================================================================
    //
    // 用两个着色器程序，分别对应：
    //   ① 单纹理（基础）
    //   ② 双纹理混合（进阶）
    // 你可以通过修改下方 draw 部分的代码来切换。
    Shader shader("shaders/texture.vert", "shaders/texture.frag", true);
    Shader shaderCombined("shaders/texture.vert", "shaders/texture_combined.frag", true);

    // ====================================================================
    // 阶段 5：顶点数据（★ 新增纹理坐标 ★）
    // ====================================================================
    //
    // 对比上一章的顶点数据，这里的每个顶点的信息从 3 个 float
    // 扩展到了 5 个 float：
    //
    //   旧：position (x, y, z)
    //   新：position (x, y, z) + texcoord (u, v)
    //
    // 纹理坐标（UV 坐标）：
    //   (0, 0) = 左下角  →  (1, 1) = 右上角
    //
    // 注意：OpenGL 中纹理坐标的 Y 轴方向是从下到上，
    // 但图片文件通常是自上而下存储的，所以需要用
    // stbi_set_flip_vertically_on_load(true) 来翻转。
    //
    // 对于矩形的 4 个顶点：
    //   右上：位置 (0.5,  0.5, 0.0) → 纹理 (1, 1)
    //   右下：位置 (0.5, -0.5, 0.0) → 纹理 (1, 0)
    //   左下：位置 (-0.5, -0.5, 0.0) → 纹理 (0, 0)
    //   左上：位置 (-0.5,  0.5, 0.0) → 纹理 (0, 1)
    float vertices[] = {
        // ---- 位置 -------    ---- 纹理坐标 ---
         0.5f,  0.5f, 0.0f,    1.0f, 1.0f,   // 右上角 (索引 0)
         0.5f, -0.5f, 0.0f,    1.0f, 0.0f,   // 右下角 (索引 1)
        -0.5f, -0.5f, 0.0f,    0.0f, 0.0f,   // 左下角 (索引 2)
        -0.5f,  0.5f, 0.0f,    0.0f, 1.0f    // 左上角 (索引 3)
    };

    unsigned int indices[] = {
        0, 1, 3,   // 第一个三角形：右上 → 右下 → 左上
        1, 2, 3    // 第二个三角形：右下 → 左下 → 左上
    };

    // ====================================================================
    // 阶段 6：VAO / VBO / EBO
    // ====================================================================
    unsigned int VAO, VBO, EBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // --- VBO ---
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // --- EBO ---
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // --- 顶点属性位置 0：顶点坐标（vec3）---
    // 每个顶点有 5 个 float，前 3 个是位置
    // 步长（stride）：5 * sizeof(float) = 20 字节
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // --- 顶点属性位置 1：纹理坐标（vec2）---
    // 从第 3 个 float 之后开始（偏移量 = 3 * sizeof(float) = 12 字节）
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ====================================================================
    // 阶段 7：加载纹理
    // ====================================================================
    //
    // 图片路径说明：
    //   以 textures/container.jpg 为例，路径相对于可执行文件的
    //   工作目录。
    //   在 VS 中调试运行时，工作目录默认是项目目录（.vcxproj 所在目录）。
    //   我们的 textures 文件夹也在项目根目录下，所以路径写为：
    //     "textures/container.jpg"
    //
    // ★ 使用 loadTexture 工具函数加载 ★
    unsigned int texture1 = loadTexture("textures/container.jpg");
    unsigned int texture2 = loadTexture("textures/awesomeface.png");

    // ====================================================================
    // 阶段 8：设置纹理单元
    // ====================================================================
    //
    // ★ 核心概念：纹理单元（Texture Unit） ★
    //
    // 为什么需要纹理单元？
    // 在片段着色器中，我们声明了多个 sampler2D uniform
    // （texture1, texture2），但它们只是「变量名」。
    // 我们必须通过 glActiveTexture + glBindTexture 将一个
    // 纹理单元绑定到具体的纹理，然后通过 setInt 告诉着色器
    // 某个 sampler2D 对应哪一号纹理单元。
    //
    // OpenGL 规范至少保证 16 个纹理单元（GL_TEXTURE0 ~ GL_TEXTURE15）。
    // 纹理单元 0 是默认激活的，所以单纹理时可以省略 glActiveTexture。
    shaderCombined.use();
    shaderCombined.setInt("texture1", 0);  // texture1 使用纹理单元 0
    shaderCombined.setInt("texture2", 1);  // texture2 使用纹理单元 1

    // ====================================================================
    // 阶段 9：设置清屏颜色
    // ====================================================================
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    // ====================================================================
    // 阶段 10：渲染循环
    // ====================================================================

    // 你可以通过修改下面的 USE_COMBINED 来选择模式：
    //   false = 单纹理模式（使用 shader）
    //   true  = 双纹理混合模式（使用 shaderCombined）
    const bool USE_COMBINED = true;

    while (!glfwWindowShouldClose(window))
    {
        // ----- 输入处理 -----
        processInput(window);

        // ----- 清屏 -----
        glClear(GL_COLOR_BUFFER_BIT);

        // ----- 选择着色器并设置纹理单元 -----
        if (USE_COMBINED)
        {
            // ---- 双纹理混合模式 ----
            shaderCombined.use();

            // 激活纹理单元 0 并绑定 texture1
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture1);

            // 激活纹理单元 1 并绑定 texture2
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texture2);
        }
        else
        {
            // ---- 单纹理模式 ----
            shader.use();

            // 单纹理时，texture 默认绑定在 GL_TEXTURE0
            // 所以可以省略 glActiveTexture
            glBindTexture(GL_TEXTURE_2D, texture1);
        }

        // ----- 绘制矩形（使用 EBO 索引）-----
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // ----- 交换缓冲 + 处理事件 -----
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    // ====================================================================
    // 阶段 11：清理
    // ====================================================================
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture1);
    glDeleteTextures(1, &texture2);

    glfwTerminate();
    return 0;
}

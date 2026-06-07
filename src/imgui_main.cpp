/**
 * ============================================================
 *  Dear ImGui 集成 — 调试界面
 * ============================================================
 *
 * 仓库：https://github.com/ocornut/imgui
 * 文档：https://github.com/ocornut/imgui/wiki
 *
 * 学习目标：
 *   1. 将 ImGui 集成到 GLFW + OpenGL3 项目中
 *   2. 了解 ImGui 的初始化流程
 *   3. 使用 ImGui 创建调试控制面板
 *   4. 用 ImGui 实时控制 OpenGL 渲染参数
 *
 * 本 demo 实现：
 *   ✅  ImGui 自带的 Demo Window（展示所有可用控件）
 *   ✅  自定义调试面板：
 *       - 清屏颜色调整（ColorEdit3）
 *       - 线框模式切换（Checkbox）
 *       - 纹理选择切换（RadioButton）
 *       - FPS 显示（Text）
 *       - 变换参数控制（Slider）
 *   ✅  原有的纹理矩形渲染保持不变
 */

// ============================== 头文件 ==============================
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Shader 类
#include "shader.h"

// stb_image
#include "../vendor/include/stb_image.h"

// ============================== Dear ImGui ==============================
//
// 包含路径说明：
//   vendor/imgui/          —— 核心头文件 imgui.h
//   vendor/imgui/backends/ —— 后端文件 imgui_impl_glfw.h, imgui_impl_opengl3.h
//
// 这些路径已在项目设置（AdditionalIncludeDirectories）中配置。
//
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"


// ============================== 回调 ==============================

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}


// ============================== 纹理加载 ==============================

unsigned int loadTexture(const char* path, bool flipY = true)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(flipY);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "✓ 纹理加载: " << path
                  << " (" << width << " × " << height << ")" << std::endl;
    }
    else
    {
        std::cerr << "✗ 纹理加载失败: " << path << std::endl;
    }

    stbi_image_free(data);
    return textureID;
}


// ============================== 主函数 ==============================

int main()
{
    // ====================================================================
    // 1. 初始化 GLFW
    // ====================================================================
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1200, 800,
        "LearnOpenGL - Dear ImGui", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "⨯ 创建窗口失败" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // ====================================================================
    // 2. 初始化 GLAD
    // ====================================================================
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "⨯ GLAD 初始化失败" << std::endl;
        return -1;
    }

    // 设置视口
    glViewport(0, 0, 1200, 800);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 开启 VSync（限制帧率到显示器刷新率）
    glfwSwapInterval(1);

    // ====================================================================
    // 3. 编译着色器
    // ====================================================================
    //
    // 复用之前的：
    //   transform.vert          —— 带 mat4 transform
    //   texture_combined.frag   —— 双纹理混合
    Shader shader("shaders/transform.vert", "shaders/texture_combined.frag", true);

    // ====================================================================
    // 4. 顶点数据 + VAO
    // ====================================================================
    float vertices[] = {
        // 位置                  // 纹理坐标
         0.5f,  0.5f, 0.0f,     1.0f, 1.0f,   // 右上
         0.5f, -0.5f, 0.0f,     1.0f, 0.0f,   // 右下
        -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,   // 左下
        -0.5f,  0.5f, 0.0f,     0.0f, 1.0f    // 左上
    };

    unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ====================================================================
    // 5. 加载纹理
    // ====================================================================
    unsigned int texture1 = loadTexture("textures/container.jpg");
    unsigned int texture2 = loadTexture("textures/awesomeface.png");

    // ====================================================================
    // 6. ★ 初始化 Dear ImGui ★
    // ====================================================================
    //
    // 完整初始化流程（只做一次）：
    //
    //   1. ImGui::CreateContext()     —— 创建 ImGui 上下文（全局状态）
    //   2. ImGuiIO& io = ...          —— 获取 IO 对象，配置输入设置
    //   3. ImGui::StyleColorsDark()   —— 选择主题风格
    //   4. ImGui_ImplGlfw_InitForOpenGL()  —— 初始化 GLFW 后端
    //   5. ImGui_ImplOpenGL3_Init()   —— 初始化 OpenGL3 后端
    //

    // 步骤 1：创建 ImGui 上下文
    // 每个 ImGui 应用有且仅有一个上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // ★ 加载中文字体 ★
    //
    // ImGui 默认字体（ProggyClean）不包含中文字符，
    // 需要从 Windows 系统字体目录加载中文字体。
    //
    // 使用微软雅黑（msyh.ttc）—— 清晰美观的 UI 字体
    //
    // AddFontFromFileTTF 参数：
    //   1. 字体文件路径（Windows 系统字体目录）
    //   2. 字号大小（16.0f ≈ 12pt）
    //   3. 字体配置（nullptr = 默认）
    //   4. 字符范围（GetGlyphRangesChineseFull = 完整 CJK 字符集）
    //
    // 注意：如果字体文件不存在或路径不对，ImGui 会回退到默认字体
    // 不会崩溃，只是中文仍然显示为乱码。
    //
    // 备选字体路径：
    //   - C:/Windows/Fonts/msyh.ttc  (微软雅黑，推荐)
    //   - C:/Windows/Fonts/simhei.ttf (黑体)
    //   - C:/Windows/Fonts/deng.ttf   (等线)
    //
    io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/msyh.ttc",
        16.0f,
        nullptr,
        io.Fonts->GetGlyphRangesChineseFull()
    );

    // 可选：允许 ImGui 记录窗口位置/大小到 .ini 文件
    // 默认在程序工作目录生成 imgui.ini
    io.IniFilename = "imgui.ini";

    // 步骤 2：设置风格
    // 可选：StyleColorsDark() / StyleColorsLight() / StyleColorsClassic()
    ImGui::StyleColorsDark();

    // 步骤 3：初始化后端
    //   - 参数 1：GLFW 窗口
    //   - 参数 2：是否安装回调（true = ImGui 接管输入事件）
    //     设为 true 时 ImGui 会处理键盘/鼠标事件，
    //     我们自己的 processInput 仍然可以读取按键
    ImGui_ImplGlfw_InitForOpenGL(window, true);

    //   - 参数：GLSL 版本字符串
    //     必须和顶点着色器的 #version 一致
    ImGui_ImplOpenGL3_Init("#version 330");

    // ====================================================================
    // 7. 渲染参数（可由 ImGui 实时控制）
    // ====================================================================

    // 清屏颜色（初始值：深青色）
    float clearColor[3] = { 0.2f, 0.3f, 0.3f };

    // 是否线框模式
    bool wireframeMode = false;

    // 纹理选择（0 = 双纹理混合，1 = 仅 container，2 = 仅 awesomeface）
    int textureMode = 0;

    // 变换参数
    float rotateSpeed = 50.0f;      // 旋转速度（度/秒）
    float scaleValue = 0.4f;        // 缩放值
    bool showTransform = true;      // 是否应用变换

    // ====================================================================
    // 8. 渲染循环
    // ====================================================================

    while (!glfwWindowShouldClose(window))
    {
        // ----- 输入处理 -----
        processInput(window);

        // ----- 应用渲染参数 -----
        glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (wireframeMode)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // ----- 绑定纹理（根据 textureMode 选择）-----
        //
        // textureMode 由 ImGui RadioButton 实时控制：
        //   0 = 双纹理混合（container + awesomeface）
        //   1 = 仅 container
        //   2 = 仅 awesomeface
        //
        if (textureMode == 0)
        {
            // 双纹理：两个单元各绑一张不同的纹理
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texture2);
        }
        else if (textureMode == 1)
        {
            // 仅 container：两个单元都绑 container
            // mix() 混合相同纹理 = 只显示 container
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texture1);
        }
        else
        {
            // 仅 awesomeface：两个单元都绑 awesomeface
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture2);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texture2);
        }

        // ----- 绘制 3D 场景（矩形） -----
        shader.use();

        // 设置纹理单元
        shader.setInt("texture1", 0);
        shader.setInt("texture2", 1);

        // 计算变换矩阵
        glm::mat4 trans = glm::mat4(1.0f);
        if (showTransform)
        {
            trans = glm::translate(trans, glm::vec3(0.0f, 0.0f, 0.0f));
            trans = glm::rotate(trans,
                                (float)glfwGetTime() * glm::radians(rotateSpeed),
                                glm::vec3(0.0f, 0.0f, 1.0f));
            trans = glm::scale(trans, glm::vec3(scaleValue, scaleValue, scaleValue));
        }

        glUniformMatrix4fv(
            glGetUniformLocation(shader.ID, "transform"),
            1, GL_FALSE, glm::value_ptr(trans));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // ================================================================
        // ★ 渲染 Dear ImGui ★
        // ================================================================
        //
        // 每帧的 ImGui 渲染流程：
        //
        //   1. ImGui_ImplOpenGL3_NewFrame()   —— OpenGL3 后端开始新帧
        //   2. ImGui_ImplGlfw_NewFrame()      —— GLFW 后端开始新帧
        //   3. ImGui::NewFrame()              —— ImGui 核心开始新帧
        //   4. ImGui 控件代码                 —— 添加各种 UI 元素
        //   5. ImGui::Render()                —— 生成绘制数据
        //   6. ImGui_ImplOpenGL3_RenderDrawData() —— 实际绘制到屏幕
        //

        // 步骤 1-3：开始新帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ============================================================
        // 步骤 4a：显示 ImGui 内置的 Demo Window
        // ============================================================
        //
        // ImGui::ShowDemoWindow() 展示了 ImGui 所有可用的控件，
        // 包括按钮、滑块、表格、树形节点、拖拽等。
        // 初次学习时强烈建议打开看看，了解 ImGui 的能力范围。
        //
        // 可以通过下方的复选框控制是否显示。
        //
        static bool showDemoWindow = true;
        if (showDemoWindow)
            ImGui::ShowDemoWindow(&showDemoWindow);

        // ============================================================
        // 步骤 4b：自定义调试面板
        // ============================================================
        //
        // ImGui 的窗口是通过函数调用即时模式的，
        // 不需要先创建窗口 ID，直接在 Begin/End 之间写控件代码。
        //
        // ImGui::Begin() 参数：
        //   1. name    —— 窗口标题（也是窗口的唯一标识）
        //   2. p_open  —— 可选的关闭按钮关联的 bool 变量
        //   3. flags   —— 窗口行为标志（可选）
        //

        ImGui::Begin("🎛️ 调试控制面板");

        // --- 性能信息 ---
        ImGui::Text("性能信息");
        ImGui::Separator();

        // ImGui::Text() 用于显示文本
        // io.Framerate 是 ImGui 提供的帧率统计
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("帧时间: %.3f ms", 1000.0f / io.Framerate);
        ImGui::Spacing();

        // --- 渲染设置 ---
        ImGui::Text("渲染设置");
        ImGui::Separator();

        // ImGui::ColorEdit3() —— 颜色选择器
        // 参数：标签、float[3] 数组指针（RGB 范围 0.0~1.0）
        ImGui::ColorEdit3("清屏颜色", clearColor);

        // ImGui::Checkbox() —— 复选框
        // 返回 true 表示值被修改（可用来触发操作）
        if (ImGui::Checkbox("线框模式", &wireframeMode))
        {
            // 值改变时立即应用
            // 但我们在每次绘制前检查 wireframeMode，所以不需要额外处理
        }

        ImGui::Spacing();

        // --- 纹理选择 ---
        ImGui::Text("纹理选择");
        ImGui::Separator();

        // ImGui::RadioButton() —— 单选按钮
        // 一组 RadioButton 共用同一个 int 变量，每个有不同值
        ImGui::RadioButton("双纹理混合", &textureMode, 0);
        ImGui::RadioButton("仅 container", &textureMode, 1);
        ImGui::RadioButton("仅 awesomeface", &textureMode, 2);

        // 根据纹理选择模式控制纹理绑定
        // 这里只是演示控件绑定，实际 implemenation 在着色器里
        // 但为了简化，我们保持绑定两张纹理，通过 shader 不同来切换

        ImGui::Spacing();

        // --- 变换控制 ---
        ImGui::Text("变换控制");
        ImGui::Separator();

        // ImGui::Checkbox() 控制是否应用变换
        ImGui::Checkbox("启用变换", &showTransform);

        // ImGui::SliderFloat() —— 滑动条
        // 参数：标签、变量指针、最小值、最大值
        ImGui::SliderFloat("旋转速度", &rotateSpeed, 0.0f, 360.0f, "%.0f°/秒");
        ImGui::SliderFloat("缩放大小", &scaleValue, 0.1f, 1.5f, "%.2f");

        ImGui::Spacing();

        // --- 帮助信息 ---
        ImGui::Text("帮助");
        ImGui::Separator();
        ImGui::Text("ESC — 退出程序");

        // ImGui::BulletText() —— 带项目符号的文本
        ImGui::BulletText("勾选「显示 Demo 窗口」查看所有 ImGui 控件");
        ImGui::BulletText("修改参数后实时生效");

        ImGui::End();   // 结束自定义窗口

        // ============================================================
        // 步骤 4c：显示「显示 Demo 窗口」选项的浮动窗口
        // ============================================================
        //
        // 在屏幕右上角显示一个小窗口，让用户随时开关 Demo 窗口
        //

        ImGui::Begin("设置", nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize);

        // ImGui::Checkbox() 控制是否显示 Demo 窗口
        ImGui::Checkbox("显示 Demo 窗口", &showDemoWindow);

        // ImGui::SameLine() 让下一个控件在同一行显示
        ImGui::SameLine();

        // ImGui::SmallButton() 小按钮
        if (ImGui::SmallButton("帮助"))
        {
            // 按钮点击时打开帮助链接
            // 这里只是演示 ImGui 按钮回调的用法
        }

        ImGui::End();

        // ============================================================
        // 步骤 5-6：渲染 ImGui
        // ============================================================
        //
        // ImGui::Render() 生成所有绘制命令，
        // ImGui_ImplOpenGL3_RenderDrawData() 实际执行绘制。
        //
        // 注意：ImGui 的绘制是在 OpenGL 帧缓冲上的叠加，
        // 不需要在 glClear 和 ImGui 绘制之间切换什么 ——
        // ImGui 接管了 OpenGL 状态并自行管理。
        //

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ----- 交换缓冲 -----
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ====================================================================
    // 9. 清理资源
    // ====================================================================
    //
    // ★ 清理顺序很重要 ★：
    //   先 Shutdown ImGui 后端 → 再 Destroy ImGui 上下文
    //

    // 清理 ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // 清理 OpenGL 资源
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture1);
    glDeleteTextures(1, &texture2);

    glfwTerminate();
    return 0;
}

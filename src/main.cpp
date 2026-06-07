/**
 * ============================================================
 *  LearnOpenGL — 坐标系统（Coordinate Systems）
 *  当前学习章节：MVP 矩阵变换 + 3D 立方体 + ImGui 调试面板
 * ============================================================
 */

// ================================================================
// 头文件
// ================================================================

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

// GLM 数学库（仅头文件，无需链接）
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// 自定义 Shader 类
#include "common/shader.h"

// stb_image（声明部分，实现在 common/stb_image.cpp 中）
#include "../vendor/include/stb_image.h"

// Dear ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"


// ================================================================
// 通用工具函数（所有示例共享）
// ================================================================

/**
 * 窗口大小变化时的回调
 */
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

/**
 * 从图片文件创建 OpenGL 纹理
 */
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
        std::cout << "✓ 纹理加载成功: " << path
                  << " (" << width << " × " << height << ", "
                  << nrChannels << " 通道)" << std::endl;
    }
    else
    {
        std::cerr << "✗ 纹理加载失败: " << path << std::endl;
    }

    stbi_image_free(data);
    return textureID;
}

/**
 * 初始化 GLFW 窗口的通用流程
 */
GLFWwindow* initGLFW(const char* title, int width = 800, int height = 600)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (window == NULL)
    {
        std::cout << "⨯ Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);
    return window;
}

/**
 * 初始化 GLAD 的通用流程
 */
bool initGLAD()
{
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "⨯ Failed to initialize GLAD" << std::endl;
        return false;
    }
    return true;
}


// ================================================================
// 坐标系统（Coordinate Systems + ImGui 调试面板）
// ================================================================

int runCoordinatesDemo()
{
    const unsigned int SCR_WIDTH  = 800;
    const unsigned int SCR_HEIGHT = 600;

    // 显示模式（1 = 单个立方体，2 = 十个立方体，3 = 十个立方体旋转）
    int displayMode = 1;
    bool wireframeMode = false;

    // ImGui 可调参数
    float clearColor[3] = { 0.2f, 0.3f, 0.3f };
    float fov = 45.0f;
    float camDistance = 3.0f;
    float camPosX = 0.0f;
    float camPosY = 0.0f;
    const float camMoveSpeed = 0.05f;
    bool showDebugPanel = true;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Coordinate Systems [1] Single Cube | SPACE: wireframe | 1/2/3: mode", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "⨯ Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // ========== 2. 初始化 GLAD ==========
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "⨯ Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // ========== 3. 启用深度测试 ==========
    glEnable(GL_DEPTH_TEST);

    // ========== 4. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 5. 编译着色器 ==========
    Shader shader("shaders/coordinates/coordinate_system.vert", "shaders/textures/texture_combined.frag", true);

    // ========== 6. 立方体顶点数据 ==========
    float vertices[] = {
        // ---- 位置 (xyz) ----    ---- 纹理坐标 (uv) ---
        // ============ 背面 (Z- 方向) ============
        -0.5f, -0.5f, -0.5f,        0.0f, 0.0f,   // 0: 左下后
         0.5f, -0.5f, -0.5f,        1.0f, 0.0f,   // 1: 右下后
         0.5f,  0.5f, -0.5f,        1.0f, 1.0f,   // 2: 右上后
        -0.5f,  0.5f, -0.5f,        0.0f, 1.0f,   // 3: 左上后
        // ============ 正面 (Z+ 方向) ============
        -0.5f, -0.5f,  0.5f,        0.0f, 0.0f,   // 4: 左下前
         0.5f, -0.5f,  0.5f,        1.0f, 0.0f,   // 5: 右下前
         0.5f,  0.5f,  0.5f,        1.0f, 1.0f,   // 6: 右上前
        -0.5f,  0.5f,  0.5f,        0.0f, 1.0f,   // 7: 左上前
        // ============ 左面 (X- 方向) ============
        -0.5f,  0.5f,  0.5f,        1.0f, 0.0f,   // 8: 左上前
        -0.5f,  0.5f, -0.5f,        0.0f, 0.0f,   // 9: 左上后
        -0.5f, -0.5f, -0.5f,        0.0f, 1.0f,   // 10:左下后
        -0.5f, -0.5f,  0.5f,        1.0f, 1.0f,   // 11:左下前
        // ============ 右面 (X+ 方向) ============
         0.5f,  0.5f,  0.5f,        0.0f, 0.0f,   // 12:右上前
         0.5f,  0.5f, -0.5f,        1.0f, 0.0f,   // 13:右上后
         0.5f, -0.5f, -0.5f,        1.0f, 1.0f,   // 14:右下后
         0.5f, -0.5f,  0.5f,        0.0f, 1.0f,   // 15:右下前
        // ============ 底面 (Y- 方向) ============
        -0.5f, -0.5f, -0.5f,        0.0f, 1.0f,   // 16:左下后
         0.5f, -0.5f, -0.5f,        1.0f, 1.0f,   // 17:右下后
         0.5f, -0.5f,  0.5f,        1.0f, 0.0f,   // 18:右下前
        -0.5f, -0.5f,  0.5f,        0.0f, 0.0f,   // 19:左下前
        // ============ 顶面 (Y+ 方向) ============
        -0.5f,  0.5f, -0.5f,        0.0f, 1.0f,   // 20:左上后
         0.5f,  0.5f, -0.5f,        1.0f, 1.0f,   // 21:右上后
         0.5f,  0.5f,  0.5f,        1.0f, 0.0f,   // 22:右上前
        -0.5f,  0.5f,  0.5f,        0.0f, 0.0f    // 23:左上前
    };

    unsigned int indices[] = {
         0,  1,  2,    2,  3,  0,   // 背面
         4,  5,  6,    6,  7,  4,   // 正面
         8,  9, 10,   10, 11,  8,   // 左面
        12, 13, 14,   14, 15, 12,   // 右面
        16, 17, 18,   18, 19, 16,   // 底面
        20, 21, 22,   22, 23, 20    // 顶面
    };

    // ========== 7. VAO / VBO / EBO ==========
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

    // ========== 8. 加载纹理 ==========
    unsigned int texture1 = loadTexture("textures/container.jpg");
    unsigned int texture2 = loadTexture("textures/awesomeface.png");

    // ========== 9. 纹理单元 ==========
    shader.use();
    shader.setInt("texture1", 0);
    shader.setInt("texture2", 1);

    // ========== 10. 十个立方体的位置 ==========
    glm::vec3 cubePositions[] = {
        glm::vec3( 0.0f,  0.0f,   0.0f),
        glm::vec3( 2.0f,  5.0f, -15.0f),
        glm::vec3(-1.5f, -2.2f,  -2.5f),
        glm::vec3(-3.8f, -2.0f, -12.3f),
        glm::vec3( 2.4f, -0.4f,  -3.5f),
        glm::vec3(-1.7f,  3.0f,  -7.5f),
        glm::vec3( 1.3f, -2.0f,  -2.5f),
        glm::vec3( 1.5f,  2.0f,  -2.5f),
        glm::vec3( 1.5f,  0.2f,  -1.5f),
        glm::vec3(-1.3f,  1.0f,  -1.5f)
    };

    // ========== 11. 清屏颜色 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 12. FPS 统计 ==========
    float prevTime = (float)glfwGetTime();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  坐标系统（带 ImGui 调试面板）" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  ESC       → 退出" << std::endl;
    std::cout << "  空格      → 切换线框/填充模式" << std::endl;
    std::cout << "  ← → / A D → 相机左右移动" << std::endl;
    std::cout << "  ↑ ↓ / W S → 相机上下移动" << std::endl;
    std::cout << "  Tab       → 显示/隐藏调试面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 13. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ----- 输入处理 -----
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // 空格：线框模式切换
        static bool spacePressed = false;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            if (!spacePressed)
            {
                wireframeMode = !wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK,
                              wireframeMode ? GL_LINE : GL_FILL);
                std::cout << (wireframeMode ? "◆ 线框模式" : "◆ 填充模式") << std::endl;
                spacePressed = true;
            }
        }
        else { spacePressed = false; }

        // 数字键 1/2/3：切换显示模式
        static bool onePressed = false;
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        {
            if (!onePressed) { displayMode = 1; std::cout << "■ 模式1：单个立方体" << std::endl; onePressed = true; }
        }
        else { onePressed = false; }

        static bool twoPressed = false;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        {
            if (!twoPressed) { displayMode = 2; std::cout << "■ 模式2：十个立方体" << std::endl; twoPressed = true; }
        }
        else { twoPressed = false; }

        static bool threePressed = false;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        {
            if (!threePressed) { displayMode = 3; std::cout << "■ 模式3：十个立方体旋转" << std::endl; threePressed = true; }
        }
        else { threePressed = false; }

        // 方向键 / WASD：相机移动
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camPosX -= camMoveSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camPosX += camMoveSpeed;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camPosY += camMoveSpeed;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camPosY -= camMoveSpeed;

        // ----- 清空颜色和深度缓冲 -----
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ----- ImGui：开始新帧 -----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ----- 绑定纹理 -----
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture2);

        // ----- 激活着色器 -----
        shader.use();

        // ----- 每帧重建投影/视图矩阵 -----
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );
        glm::mat4 view = glm::lookAt(
            glm::vec3(camPosX, camPosY, camDistance),
            glm::vec3(camPosX, camPosY, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        // ----- 绑定 VAO -----
        glBindVertexArray(VAO);

        // ----- 根据模式绘制 -----
        if (displayMode == 1)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::rotate(model,
                (float)glfwGetTime() * glm::radians(50.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model,
                (float)glfwGetTime() * glm::radians(30.0f),
                glm::vec3(1.0f, 0.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
        else if (displayMode == 2 || displayMode == 3)
        {
            for (unsigned int i = 0; i < 10; i++)
            {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, cubePositions[i]);

                float angle = 20.0f * (i + 1);
                model = glm::rotate(model,
                    glm::radians(angle),
                    glm::vec3(1.0f, 0.3f, 0.5f));

                if (displayMode == 3)
                {
                    model = glm::rotate(model,
                        (float)glfwGetTime() * glm::radians(25.0f),
                        glm::vec3(0.0f, 1.0f, 0.0f));
                }

                shader.setMat4("model", model);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }
        }

        // ----- ImGui 调试面板 -----
        if (showDebugPanel)
        {
            float curTime = (float)glfwGetTime();
            float fps = 1.0f / (curTime - prevTime + 0.0001f);
            prevTime = curTime;

            ImGui::Begin("Debug Panel - Coordinate Systems");

            ImGui::Text("FPS: %.1f  (%.1f ms)", fps, 1000.0f / fps);
            ImGui::Separator();

            ImGui::Text("Display Mode");
            ImGui::RadioButton("Single Cube",          &displayMode, 1);
            ImGui::RadioButton("10 Cubes (Static)",    &displayMode, 2);
            ImGui::RadioButton("10 Cubes (Rotating)",  &displayMode, 3);
            ImGui::Separator();

            if (ImGui::Checkbox("Wireframe Mode", &wireframeMode))
            {
                glPolygonMode(GL_FRONT_AND_BACK,
                    wireframeMode ? GL_LINE : GL_FILL);
            }
            ImGui::Separator();

            ImGui::Text("Render Settings");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::SliderFloat("Camera Distance", &camDistance, 1.0f, 20.0f, "%.1f");
            ImGui::SliderFloat("Camera X (Left/Right)", &camPosX, -5.0f, 5.0f, "%.2f");
            ImGui::SliderFloat("Camera Y (Up/Down)",    &camPosY, -5.0f, 5.0f, "%.2f");
            ImGui::ColorEdit3("Clear Color", clearColor);
            ImGui::Separator();

            ImGui::TextDisabled("ESC: Quit  |  Space: Wireframe  |  Tab: Panel");
            ImGui::TextDisabled("Arrows/WASD: Move Camera");

            ImGui::End();
        }

        // ----- ImGui：渲染 -----
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ----- 交换缓冲 + 事件处理 -----
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 14. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture1);
    glDeleteTextures(1, &texture2);
    glfwTerminate();
    return 0;
}


// ================================================================
// 主函数
// ================================================================

int main()
{
    std::cout << "运行最新章节：坐标系统（Coordinate Systems）" << std::endl;
    return runCoordinatesDemo();
}

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
// 摄像机（Camera）— 全局状态（供 GLFW 回调使用）
// ================================================================
//
// 摄像机本质 = 视图矩阵的一种直观理解方式。
// 我们通过定义位置(Position)、朝向(Front)、上向量(Up)三个要素，
// 构建一个「虚拟摄像机」，每帧用它生成 view 矩阵。
//
//    view = glm::lookAt(camPos, camPos + camFront, camUp)
//
// 其中 camFront 通过欧拉角 (Yaw/Pitch) 计算：
//
//    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch))
//    front.y = sin(glm::radians(pitch))
//    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch))
//
// 为什么用全局变量？
//   GLFW 的回调函数要求是普通函数指针，无法直接捕获 lambda，
//   所以摄像机状态需要在文件作用域中共享。
//
namespace CameraState {
    // ---- 位置与朝向 ----
    glm::vec3 camPos   = glm::vec3(0.0f, 0.0f,  3.0f);   // 摄像机位置
    glm::vec3 camFront = glm::vec3(0.0f, 0.0f, -1.0f);   // 摄像机朝向（初始看向 -Z）
    glm::vec3 camUp    = glm::vec3(0.0f, 1.0f,  0.0f);   // 世界空间的上方向

    // ---- 欧拉角 ----
    // yaw = -90° 让初始方向指向 -Z（配合 lookAt 的默认行为）
    float yaw   = -90.0f;   // 偏航角（绕 Y 轴左右看）
    float pitch =   0.0f;   // 俯仰角（绕 X 轴上下看）

    // ---- 鼠标控制 ----
    bool  firstMouse   = true;     // 首次鼠标输入标记（跳过跳变）
    float lastX        = 400.0f;   // 上一帧鼠标 X
    float lastY        = 300.0f;   // 上一帧鼠标 Y
    float mouseSensitivity = 0.1f; // 鼠标灵敏度

    // ---- 缩放 (FOV) ----
    float fov = 45.0f;

    // ---- 时间相关 ----
    float deltaTime    = 0.0f;     // 当前帧与上一帧的时间差
    float lastFrame    = 0.0f;     // 上一帧的时间
    float movementSpeed = 2.5f;    // 移动速度（单位/秒）

    // ---- 调试状态 ----
    bool mouseCaptured = false;    // 鼠标是否被捕获（用于自由视角）
    bool showDebugPanel = true;    // 是否显示 ImGui 调试面板
    bool wireframeMode  = false;   // 线框模式
    bool cubeRotate     = true;    // 立方体是否旋转
    float clearColor[3] = { 0.2f, 0.3f, 0.3f };  // 清屏颜色
}

/**
 * 根据当前的欧拉角（yaw/pitch）重新计算 camFront 向量
 *
 * 球面坐标 → 笛卡尔坐标的转换：
 *   x = r * cos(pitch) * sin(yaw)
 *   y = r * sin(pitch)
 *   z = r * cos(pitch) * cos(yaw)
 *
 * 由于我们的坐标系是 Y-up，且初始方向为 -Z：
 *   使用 x = cos(yaw) * cos(pitch)
 *        y = sin(pitch)
 *        z = sin(yaw) * cos(pitch)
 */
void updateCameraFront()
{
    glm::vec3 front;
    front.x = cos(glm::radians(CameraState::yaw)) * cos(glm::radians(CameraState::pitch));
    front.y = sin(glm::radians(CameraState::pitch));
    front.z = sin(glm::radians(CameraState::yaw)) * cos(glm::radians(CameraState::pitch));
    CameraState::camFront = glm::normalize(front);
}

/**
 * GLFW 鼠标移动回调
 *
 * 当鼠标捕获时，将鼠标的屏幕位移映射为欧拉角变化：
 *   水平移动 → yaw 变化（左右看）
 *   垂直移动 → pitch 变化（上下看）
 *
 * @param xpos 当前鼠标 X 坐标（屏幕像素）
 * @param ypos 当前鼠标 Y 坐标（屏幕像素）
 */
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!CameraState::mouseCaptured) return;

    if (CameraState::firstMouse)
    {
        CameraState::lastX = (float)xpos;
        CameraState::lastY = (float)ypos;
        CameraState::firstMouse = false;
    }

    // 计算鼠标位移量
    float xoffset = (float)xpos - CameraState::lastX;
    float yoffset = CameraState::lastY - (float)ypos;  // Y 轴翻转：屏幕 Y 向下，OpenGL 世界 Y 向上
    CameraState::lastX = (float)xpos;
    CameraState::lastY = (float)ypos;

    // 应用灵敏度并更新欧拉角
    xoffset *= CameraState::mouseSensitivity;
    yoffset *= CameraState::mouseSensitivity;
    CameraState::yaw   += xoffset;
    CameraState::pitch += yoffset;

    // 限制 pitch 范围，防止万向锁（Gimbal Lock）和翻转
    // ±89° 而不是 ±90° 以防止在极点处方向向量变得不稳定
    if (CameraState::pitch > 89.0f)  CameraState::pitch = 89.0f;
    if (CameraState::pitch < -89.0f) CameraState::pitch = -89.0f;

    // 更新朝向向量
    updateCameraFront();
}

/**
 * GLFW 滚轮回调 —— 缩放（调整 FOV）
 *
 * 滚轮向上滚 → FOV 减小 → 拉近（望远镜效果）
 * 滚轮向下滚 → FOV 增大 → 拉远（广角效果）
 */
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    CameraState::fov -= (float)yoffset;   // yoffset > 0 表示向上滚
    if (CameraState::fov < 1.0f)  CameraState::fov = 1.0f;
    if (CameraState::fov > 60.0f) CameraState::fov = 60.0f;
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
// 摄像机（Camera）— 自由视角 + 鼠标控制
// ================================================================

int runCameraDemo()
{
    using namespace CameraState;

    const unsigned int SCR_WIDTH  = 800;
    const unsigned int SCR_HEIGHT = 600;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Camera [FPS] | WASD: move | Right-click: capture mouse | Scroll: zoom",
        NULL, NULL);
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

    // ========== 3. 注册 GLFW 回调函数 ==========
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // ========== 4. 启用深度测试 ==========
    glEnable(GL_DEPTH_TEST);

    // ========== 5. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 6. 编译着色器 ==========
    // 复用坐标系统章节的着色器（已包含 MVP 矩阵 uniform）
    Shader shader("shaders/coordinates/coordinate_system.vert",
                  "shaders/textures/texture_combined.frag", true);

    // ========== 7. 立方体顶点/索引数据 ==========
    float vertices[] = {
        // ---- 位置 (xyz) ----    ---- 纹理坐标 (uv) ---
        // ============ 背面 (Z- 方向) ============
        -0.5f, -0.5f, -0.5f,        0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,        1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,        1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,        0.0f, 1.0f,
        // ============ 正面 (Z+ 方向) ============
        -0.5f, -0.5f,  0.5f,        0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,        1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,        1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,        0.0f, 1.0f,
        // ============ 左面 (X- 方向) ============
        -0.5f,  0.5f,  0.5f,        1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,        0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,        0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,        1.0f, 1.0f,
        // ============ 右面 (X+ 方向) ============
         0.5f,  0.5f,  0.5f,        0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,        1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,        1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,        0.0f, 1.0f,
        // ============ 底面 (Y- 方向) ============
        -0.5f, -0.5f, -0.5f,        0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,        1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,        1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,        0.0f, 0.0f,
        // ============ 顶面 (Y+ 方向) ============
        -0.5f,  0.5f, -0.5f,        0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,        1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,        1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,        0.0f, 0.0f
    };

    unsigned int indices[] = {
         0,  1,  2,    2,  3,  0,   // 背面
         4,  5,  6,    6,  7,  4,   // 正面
         8,  9, 10,   10, 11,  8,   // 左面
        12, 13, 14,   14, 15, 12,   // 右面
        16, 17, 18,   18, 19, 16,   // 底面
        20, 21, 22,   22, 23, 20    // 顶面
    };

    // ========== 8. VAO / VBO / EBO ==========
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

    // ========== 9. 加载纹理 ==========
    unsigned int texture1 = loadTexture("textures/container.jpg");
    unsigned int texture2 = loadTexture("textures/awesomeface.png");

    // ========== 10. 设置纹理单元 ==========
    shader.use();
    shader.setInt("texture1", 0);
    shader.setInt("texture2", 1);

    // ========== 11. 立方体位置阵列 ==========
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
    const int NUM_CUBES = sizeof(cubePositions) / sizeof(cubePositions[0]);

    // ========== 12. 清屏颜色 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 13. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  摄像机（自由视角 + 鼠标控制）" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  ESC           → 退出" << std::endl;
    std::cout << "  右键点击窗口  → 捕获/释放鼠标（自由视角）" << std::endl;
    std::cout << "  W A S D       → 前后左右移动" << std::endl;
    std::cout << "  鼠标移动      → 改变视角方向（鼠标捕获时）" << std::endl;
    std::cout << "  滚轮          → 拉近/拉远（FOV 缩放）" << std::endl;
    std::cout << "  空格          → 切换线框/填充模式" << std::endl;
    std::cout << "  Tab           → 显示/隐藏调试面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 14. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 14a. 计算 deltaTime（帧间时间差） =====
        //
        // deltaTime 是摄像机章节的核心概念之一：
        // 不需要它时，每秒帧数变化会导致移动速度变化（快慢机）。
        // 用 deltaTime 修正后，移动速度与帧率无关：
        //   实际位移 = 速度 × deltaTime
        //
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // ===== 14b. 输入处理 =====
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

        // Tab：切换 ImGui 面板
        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
        {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        }
        else { tabPressed = false; }

        // ===== WASD 摄像机移动（帧率无关） =====
        //
        // 关键理解：
        //   前后：沿 camFront 方向 / 反方向
        //   左右：沿 camFront 与 camUp 的叉积方向（右向量）
        //   不用 camUp 直接上下，而是用叉积确保「右」永远水平
        //
        float velocity = movementSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camPos += velocity * camFront;                       // 前进
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camPos -= velocity * camFront;                       // 后退
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camPos -= glm::normalize(glm::cross(camFront, camUp)) * velocity;  // 左移
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camPos += glm::normalize(glm::cross(camFront, camUp)) * velocity;  // 右移

        // ===== 右键切换鼠标捕获 =====
        // 捕获后：鼠标隐藏、自由旋转视角、无法操作 ImGui
        // 释放后：鼠标可见、可以操作 ImGui
        static bool rightClicked = false;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            if (!rightClicked)
            {
                mouseCaptured = !mouseCaptured;
                glfwSetInputMode(window, GLFW_CURSOR,
                    mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                if (mouseCaptured)
                {
                    // 重置 firstMouse，避免下一次捕获时视角跳变
                    firstMouse = true;
                    std::cout << "🖱 鼠标已捕获 —— 移动鼠标自由视角" << std::endl;
                }
                else
                {
                    std::cout << "🖱 鼠标已释放" << std::endl;
                }
                rightClicked = true;
            }
        }
        else { rightClicked = false; }

        // ===== 清理颜色和深度缓冲 =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== ImGui：开始新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 绑定纹理 =====
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture2);

        // ===== 激活着色器 =====
        shader.use();

        // ===== 构建投影矩阵（含滚轮缩放） =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );

        // ===== 构建视图矩阵（基于摄像机状态） =====
        //
        // 与坐标系统章节的核心区别：
        //   之前：lookAt(固定位置, 固定目标点, 固定上向量)
        //   现在：lookAt(动态位置, 位置+动态朝向, 固定上向量)
        //
        // camPos + camFront 表示「摄像机前方一点」，
        // 这样摄像机看向的方向随着 camFront 的变化而实时改变。
        //
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);

        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        // ===== 绘制立方体 =====
        glBindVertexArray(VAO);

        for (int i = 0; i < NUM_CUBES; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);

            float angle = 20.0f * (i + 1);
            model = glm::rotate(model, glm::radians(angle),
                                glm::vec3(1.0f, 0.3f, 0.5f));

            // 可选的额外旋转（用于展示动态场景）
            if (cubeRotate)
            {
                model = glm::rotate(model,
                    (float)glfwGetTime() * glm::radians(15.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f));
            }

            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // ===== ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Camera");

            // ---- 性能信息 ----
            float fps = 1.0f / (deltaTime + 0.0001f);
            ImGui::Text("FPS: %.1f  (%.1f ms)", fps, deltaTime * 1000.0f);
            ImGui::Separator();

            // ---- 摄像机信息（只读） ----
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Camera State");
            ImGui::Text("Position:  (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
            ImGui::Text("Front:     (%.2f, %.2f, %.2f)", camFront.x, camFront.y, camFront.z);
            ImGui::Text("Yaw:  %.1f°   Pitch: %.1f°", yaw, pitch);
            ImGui::Text("FOV:  %.1f°", fov);
            ImGui::Separator();

            // ---- 可调参数 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Controls");
            ImGui::SliderFloat("Move Speed", &movementSpeed, 0.5f, 20.0f, "%.1f");
            ImGui::SliderFloat("Mouse Sens.", &mouseSensitivity, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 1.0f, 60.0f, "%.0f°");
            ImGui::Separator();

            // ---- 调试选项 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Options");
            if (ImGui::Checkbox("Wireframe Mode", &wireframeMode))
            {
                glPolygonMode(GL_FRONT_AND_BACK,
                    wireframeMode ? GL_LINE : GL_FILL);
            }
            ImGui::Checkbox("Cube Rotate", &cubeRotate);
            ImGui::ColorEdit3("Clear Color", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            // ---- 鼠标捕获按钮 ----
            if (ImGui::Button(mouseCaptured ? "Release Mouse" : "Capture Mouse"))
            {
                mouseCaptured = !mouseCaptured;
                glfwSetInputMode(window, GLFW_CURSOR,
                    mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                if (mouseCaptured) firstMouse = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled(mouseCaptured ? "(Mouse captured)" : "(Mouse free)");

            ImGui::Separator();
            ImGui::TextDisabled("WASD: Move  |  RClick: Toggle mouse  |  Scroll: Zoom");
            ImGui::TextDisabled("Space: Wireframe  |  Tab: Panel  |  ESC: Quit");

            ImGui::End();
        }

        // ===== ImGui：渲染 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ===== 交换缓冲 + 事件处理 =====
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 15. 清理 ==========
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
    std::cout << "▶ 运行最新章节：摄像机（Camera）" << std::endl;
    return runCameraDemo();
}

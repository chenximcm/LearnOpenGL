/**
 * ============================================================
 *  LearnOpenGL — 高级 OpenGL
 *  当前学习章节：帧缓冲（Framebuffers）
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

// Mesh / Model 类（模型加载章节）
#include "common/mesh.h"
#include "common/model.h"

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
// 光照 / 颜色（Colors）
// ================================================================

int runColorsDemo()
{
    const unsigned int SCR_WIDTH  = 800;
    const unsigned int SCR_HEIGHT = 600;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Colors | Light × Object = Perceived Color", NULL, NULL);
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
    //
    // lightingShader — 用于渲染被光照的物体（主立方体）
    //   复用坐标系统的顶点着色器（带 MVP 和纹理坐标）
    //   使用自定义的 colors.frag 进行颜色计算
    //
    // lightCubeShader — 用于渲染代表光源的小立方体
    //   简单的顶点着色器（只有 MVP）+ 纯色片段着色器
    //
    Shader lightingShader("shaders/coordinates/coordinate_system.vert",
                          "shaders/lighting/colors.frag", true);
    Shader lightCubeShader("shaders/lighting/light_cube.vert",
                           "shaders/lighting/light_cube.frag", true);

    // ========== 6. 立方体顶点数据（含纹理坐标，复用坐标系统章节的数据） ==========
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

    // ========== 7. 创建 VAO / VBO / EBO ==========
    //
    // 主物体（被光照的立方体）：
    //   需要位置 + 纹理坐标两个属性
    //
    unsigned int cubeVAO, VBO, EBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 位置属性 (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 纹理坐标属性 (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 8. 光源立方体的 VAO（复用 VBO/EBO） ==========
    //
    // 重要概念：两个 VAO 共享同一份 VBO 数据。
    // 不同之处在于 lightVAO 只需要位置属性（location = 0），
    // 不需要纹理坐标属性，因为 light_cube.vert 只有 aPos 输入。
    //
    unsigned int lightVAO;
    glGenVertexArrays(1, &lightVAO);
    glBindVertexArray(lightVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);   // 复用 VBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);  // 复用 EBO

    // 只启用位置属性（光源着色器只需要位置）
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // ========== 9. 加载纹理 ==========
    unsigned int texture1 = loadTexture("textures/container.jpg");

    // ========== 10. 用户控制参数 ==========
    // ---- 颜色相关 ----
    glm::vec3 lightColor  = glm::vec3(1.0f);          // 光源颜色（默认白色）
    glm::vec3 objectColor = glm::vec3(1.0f, 0.5f, 0.31f);  // 物体颜色（默认珊瑚色）
    float lightColorArray[3]  = { 1.0f, 1.0f, 1.0f };
    float objectColorArray[3] = { 1.0f, 0.5f, 0.31f };

    // ---- 光源位置 ----
    glm::vec3 lightPos = glm::vec3(1.2f, 1.0f, 2.0f);
    float lightPosArray[3] = { 1.2f, 1.0f, 2.0f };

    // ---- 摄像机控制 ----
    glm::vec3 camPos   = glm::vec3(0.0f, 0.0f, 5.0f);
    float     fov      = 45.0f;
    float     camMoveSpeed = 0.08f;

    // ---- 调试 ----
    bool showDebugPanel = true;
    bool useTexture     = true;   // 是否在主立方体上使用纹理
    float clearColor[3] = { 0.1f, 0.1f, 0.1f };

    // ========== 11. 设置纹理单元 ==========
    lightingShader.use();
    lightingShader.setInt("texture1", 0);

    // ========== 12. 清屏颜色 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 13. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  颜色（Colors）—— 光照的基础" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  核心公式: Perceived = Light × Object" << std::endl;
    std::cout << "  ESC       → 退出" << std::endl;
    std::cout << "  A/D  ←/→  → 左右移动摄像机" << std::endl;
    std::cout << "  W/S  ↑/↓  → 上下移动摄像机" << std::endl;
    std::cout << "  Tab       → 显示/隐藏调试面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 14. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 14a. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Tab：切换 ImGui 面板
        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
        {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        }
        else { tabPressed = false; }

        // WASD / 方向键：摄像机移动
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camPos += glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camPos -= glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camPos -= glm::vec3(camMoveSpeed, 0.0f, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camPos += glm::vec3(camMoveSpeed, 0.0f, 0.0f);

        // ===== 14b. 清空缓冲 =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== 14c. ImGui：开始新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 14d. 构建 MVP 矩阵 =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // ================================================================
        // 第一部分：渲染主物体（被光照的立方体）
        // ================================================================

        lightingShader.use();

        // 设置 MVP 矩阵
        glm::mat4 model = glm::mat4(1.0f);
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setMat4("model", model);

        // ★★★ 核心：设置颜色 uniform ★★★
        //
        // 片段着色器 colors.frag 中计算：
        //   vec3 result = lightColor * objectColor;
        //
        // 改变这两个值就能观察到不同颜色的光照效果。
        //
        lightingShader.setVec3("lightColor",  lightColor.x,  lightColor.y,  lightColor.z);
        lightingShader.setVec3("objectColor", objectColor.x, objectColor.y, objectColor.z);

        // 可选：绑定纹理到主立方体
        if (useTexture)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture1);
        }

        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // ================================================================
        // 第二部分：渲染光源表示（小立方体标记光源位置）
        // ================================================================
        //
        // 与主物体的区别：
        //   - 使用不同的着色器（lightCubeShader）
        //   - 模型矩阵缩小到 0.2（表示它是一个「点光源」标记）
        //   - 位置固定在 lightPos
        //   - 颜色 = lightColor（看起来像它在发光）
        //

        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);

        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));  // 让光源立方体小一点
        lightCubeShader.setMat4("model", model);

        lightCubeShader.setVec3("lightColor", lightColor.x, lightColor.y, lightColor.z);

        glBindVertexArray(lightVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // ===== 14e. ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Colors");

            // ---- 性能信息 ----
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 颜色控制（核心） ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Colors");
            ImGui::Text("Perceived = Light × Object");

            ImGui::ColorEdit3("Light Color",  lightColorArray);
            ImGui::ColorEdit3("Object Color", objectColorArray);

            // 同步颜色数组到 glm::vec3
            lightColor  = glm::vec3(lightColorArray[0],  lightColorArray[1],  lightColorArray[2]);
            objectColor = glm::vec3(objectColorArray[0], objectColorArray[1], objectColorArray[2]);

            // ---- 计算结果预览 ----
            glm::vec3 result = lightColor * objectColor;
            float resultArray[3] = { result.r, result.g, result.b };
            ImGui::Text("Result (Light × Object):");
            ImGui::ColorEdit3("##Result", resultArray, ImGuiColorEditFlags_NoInputs);

            ImGui::Text("  R = %.2f × %.2f = %.2f", lightColor.r, objectColor.r, result.r);
            ImGui::Text("  G = %.2f × %.2f = %.2f", lightColor.g, objectColor.g, result.g);
            ImGui::Text("  B = %.2f × %.2f = %.2f", lightColor.b, objectColor.b, result.b);
            ImGui::Separator();

            // ---- 光源位置 ----
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Light Position");
            ImGui::SliderFloat("Light X", &lightPosArray[0], -3.0f, 3.0f, "%.1f");
            ImGui::SliderFloat("Light Y", &lightPosArray[1], -3.0f, 3.0f, "%.1f");
            ImGui::SliderFloat("Light Z", &lightPosArray[2], -5.0f, 5.0f, "%.1f");
            lightPos = glm::vec3(lightPosArray[0], lightPosArray[1], lightPosArray[2]);
            ImGui::Separator();

            // ---- 摄像机 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Camera");
            ImGui::SliderFloat("Move Speed", &camMoveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::Separator();

            // ---- 选项 ----
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Options");
            ImGui::Checkbox("Use Texture", &useTexture);
            ImGui::ColorEdit3("Clear Color", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            ImGui::TextDisabled("WASD/Arrows: Move Camera  |  Tab: Panel  |  ESC: Quit");

            ImGui::End();
        }

        // ===== 14f. ImGui：渲染 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ===== 14g. 交换缓冲 + 事件处理 =====
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 15. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture1);
    glfwTerminate();
    return 0;
}


// ================================================================
// 基础光照（Basic Lighting）— Phong 光照模型
// ================================================================
//
// 相比颜色章节（Light × Object = Perceived Color），
// 这里引入真正的光照计算 — Phong 光照模型，包含三个分量：
//
//   ① Ambient  （环境光） — 最低亮度，避免背面全黑
//   ② Diffuse  （漫反射） — 法线与光线的夹角决定亮度
//   ③ Specular（镜面反射）— 视线与反射光线的夹角决定高光
//
// 顶点数据变化：
//   之前： [位置 (3) | 纹理坐标 (2)]        = 5 float/vertex
//   现在： [位置 (3) | 法线 (3) | 纹理坐标 (2)] = 8 float/vertex
//

int runBasicLightingDemo()
{
    const unsigned int SCR_WIDTH  = 800;
    const unsigned int SCR_HEIGHT = 600;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Basic Lighting (Phong) | WASD: move  | Tab: panel", NULL, NULL);
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
    //
    // lightingShader — 主物体（应用 Phong 光照模型）
    // lightCubeShader — 光源标记（纯色小立方体，复用颜色章节）
    //
    Shader lightingShader("shaders/lighting/basic_lighting.vert",
                          "shaders/lighting/basic_lighting.frag", true);
    Shader lightCubeShader("shaders/lighting/light_cube.vert",
                           "shaders/lighting/light_cube.frag", true);

    // ========== 6. 顶点数据（带法线） ==========
    //
    // ★★★ 核心变化：每个顶点新增法线（Normal）★★★
    //
    // 数据结构： [位置 xyz] [法线 xyz] [纹理坐标 uv]
    // 每个顶点 8 个 float，stride = 32 字节
    //
    float vertices[] = {
        // ============ 背面 (Z-) 法线: (0,0,-1) ============
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        // ============ 正面 (Z+) 法线: (0,0,1) ============
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        // ============ 左面 (X-) 法线: (-1,0,0) ============
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        // ============ 右面 (X+) 法线: (1,0,0) ============
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        // ============ 底面 (Y-) 法线: (0,-1,0) ============
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        // ============ 顶面 (Y+) 法线: (0,1,0) ============
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
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
    //
    // 主立方体 VAO（cubeVAO）：
    //   启用所有 3 个顶点属性（位置 + 法线 + 纹理坐标）
    //   对应 basic_lighting.vert 的 layout 声明
    //
    // 光源 VAO（lightVAO）：
    //   只启用位置属性，复用同一份 VBO/EBO
    //
    unsigned int cubeVAO, lightVAO, VBO, EBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenVertexArrays(1, &lightVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // ---- cubeVAO：主物体（全部属性） ----
    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 位置属性 (location = 0) — 3 floats, stride 32
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 法线属性 (location = 1) — 3 floats, stride 32, offset 12
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 纹理坐标属性 (location = 2) — 2 floats, stride 32, offset 24
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ---- lightVAO：光源标记（只取位置） ----
    glBindVertexArray(lightVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    // 只启用位置属性（light_cube.vert 只有 layout 0）
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // ========== 8. 加载纹理 ==========
    unsigned int texture1 = loadTexture("textures/container.jpg");

    // ========== 9. 设置纹理单元 ==========
    lightingShader.use();
    lightingShader.setInt("texture1", 0);

    // ========== 10. 用户控制参数 ==========

    // ---- 光源 ----
    glm::vec3 lightPos     = glm::vec3(1.2f, 1.0f, 2.0f);
    glm::vec3 lightColor   = glm::vec3(1.0f);            // 白光
    float lightPosArray[3] = { 1.2f, 1.0f, 2.0f };
    float lightColorArray[3] = { 1.0f, 1.0f, 1.0f };

    // ---- 物体材质 ----
    glm::vec3 objectColor   = glm::vec3(1.0f, 0.5f, 0.31f);  // 珊瑚色
    float objectColorArray[3] = { 1.0f, 0.5f, 0.31f };

    // ---- Phong 参数 ----
    float ambientStrength  = 0.1f;    // 环境光强度
    float specularStrength = 0.5f;    // 镜面反射强度
    float shininess        = 32.0f;   // 反光度
    bool  useTexture       = true;    // 是否叠加纹理
    bool  lightAutoRotate  = false;   // 光源是否自动旋转

    // ---- 摄像机 ----
    glm::vec3 camPos   = glm::vec3(0.0f, 0.0f, 5.0f);
    float     fov      = 45.0f;
    float     camMoveSpeed = 0.08f;

    // ---- 调试 ----
    bool showDebugPanel = true;
    float clearColor[3] = { 0.1f, 0.1f, 0.1f };

    // ========== 11. 清屏颜色 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 12. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  基础光照（Basic Lighting）— Phong 模型" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  核心公式: (Ambient + Diffuse + Specular)" << std::endl;
    std::cout << "  ESC       → 退出" << std::endl;
    std::cout << "  WASD/箭头 → 摄像机移动" << std::endl;
    std::cout << "  Tab       → 显示/隐藏调试面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 13. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 13a. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Tab：切换 ImGui 面板
        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
        {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        }
        else { tabPressed = false; }

        // WASD / 方向键：摄像机移动
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camPos += glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camPos -= glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camPos -= glm::vec3(camMoveSpeed, 0.0f, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camPos += glm::vec3(camMoveSpeed, 0.0f, 0.0f);

        // ===== 13b. 光源自动旋转 =====
        if (lightAutoRotate)
        {
            float radius = glm::length(lightPos);
            float angle  = (float)glfwGetTime() * 0.8f;
            lightPos.x = cos(angle) * radius;
            lightPos.z = sin(angle) * radius;
            lightPosArray[0] = lightPos.x;
            lightPosArray[1] = lightPos.y;
            lightPosArray[2] = lightPos.z;
        }

        // ===== 13c. 清空缓冲 =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== 13d. ImGui：开始新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 13e. MVP 矩阵（与摄像机共用） =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // ================================================================
        // 第一部分：渲染主物体（应用 Phong 光照模型）
        // ================================================================

        lightingShader.use();

        // ---- MVP 矩阵 ----
        glm::mat4 model = glm::mat4(1.0f);
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setMat4("model", model);

        // ---- 光源参数 ----
        lightingShader.setVec3("lightPos",  lightPos.x,  lightPos.y,  lightPos.z);
        lightingShader.setVec3("lightColor", lightColor.x, lightColor.y, lightColor.z);
        lightingShader.setVec3("viewPos",   camPos.x,    camPos.y,    camPos.z);

        // ---- 物体材质 ----
        lightingShader.setVec3("objectColor", objectColor.x, objectColor.y, objectColor.z);
        lightingShader.setBool("useTexture", useTexture);

        // ---- Phong 模型参数 ----
        lightingShader.setFloat("ambientStrength",  ambientStrength);
        lightingShader.setFloat("specularStrength", specularStrength);
        lightingShader.setFloat("shininess", shininess);

        // 可选绑定纹理
        if (useTexture)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture1);
        }

        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // ================================================================
        // 第二部分：渲染光源标记（小立方体）
        // ================================================================

        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);

        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));
        lightCubeShader.setMat4("model", model);

        lightCubeShader.setVec3("lightColor", lightColor.x, lightColor.y, lightColor.z);

        glBindVertexArray(lightVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // ===== 13f. ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Basic Lighting");

            // ---- 性能 ----
            ImGui::Text("FPS: %.1f  (%.1f ms)", ImGui::GetIO().Framerate,
                        1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- Phong 光照模型概览 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Phong Lighting Model");
            ImGui::Text("Result = (Ambient + Diffuse + Specular) × Color");
            ImGui::Separator();

            // ---- 光源设置 ----
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "Light Source");
            ImGui::ColorEdit3("Light Color", lightColorArray);
            ImGui::SliderFloat("Light X", &lightPosArray[0], -4.0f, 4.0f, "%.1f");
            ImGui::SliderFloat("Light Y", &lightPosArray[1], -4.0f, 4.0f, "%.1f");
            ImGui::SliderFloat("Light Z", &lightPosArray[2], -5.0f, 5.0f, "%.1f");
            lightPos = glm::vec3(lightPosArray[0], lightPosArray[1], lightPosArray[2]);
            lightColor = glm::vec3(lightColorArray[0], lightColorArray[1], lightColorArray[2]);
            ImGui::Checkbox("Auto Rotate Light", &lightAutoRotate);
            ImGui::Separator();

            // ---- 材质 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Object Material");
            ImGui::ColorEdit3("Object Color", objectColorArray);
            objectColor = glm::vec3(objectColorArray[0], objectColorArray[1], objectColorArray[2]);
            ImGui::Separator();

            // ---- Phong 参数 ----
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Phong Parameters");

            // 环境光
            ImGui::SliderFloat("Ambient Strength", &ambientStrength, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("最低亮度，防止背面全黑。值越大物体整体越亮。");

            // 漫反射（没有单独参数，由法线与光线的 dot product 自动计算）

            // 镜面反射
            ImGui::SliderFloat("Specular Strength", &specularStrength, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("高光亮度。值为 0 时无高光（粗糙表面），值越大高光越亮。");

            ImGui::SliderFloat("Shininess", &shininess, 1.0f, 256.0f, "%.0f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("反光度控制高光范围。小值（8）→ 大范围高光（粗糙）；大值（256）→ 小范围高光（光滑）。");
            ImGui::Separator();

            // ---- 预览：各分量贡献 ----
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Component Preview");
            ImVec4 ambColor(ambientStrength, ambientStrength, ambientStrength, 1.0f);
            ImVec4 specColor(specularStrength * 0.5f, specularStrength * 0.5f, specularStrength * 0.5f, 1.0f);
            ImGui::ColorButton("Ambient",  ambColor, 0, ImVec2(60, 20)); ImGui::SameLine();
            ImGui::Text(" Ambient  = %.1f× lightColor", ambientStrength);
            ImGui::ColorButton("Specular", specColor, 0, ImVec2(60, 20)); ImGui::SameLine();
            ImGui::Text(" Specular = %.1f× spec × lightColor (shininess=%.0f)",
                        specularStrength, shininess);
            ImGui::Separator();

            // ---- 选项 ----
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "Options");
            ImGui::Checkbox("Use Texture", &useTexture);
            ImGui::ColorEdit3("Clear Color", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            // ---- 摄像机 ----
            ImGui::SliderFloat("Camera Speed", &camMoveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::Separator();

            ImGui::TextDisabled("WASD/Arrows: Move  |  Tab: Panel  |  ESC: Quit");

            ImGui::End();
        }

        // ===== 13g. ImGui：渲染 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ===== 13h. 交换缓冲 + 事件处理 =====
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 14. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture1);
    glfwTerminate();
    return 0;
}


// ================================================================
// 材质（Materials）— 分离 Material 与 Light 属性
// ================================================================
//
// 基础光照章节使用全局系数控制 Phong 模型：
//   result = (ambient + diffuse + specular) × objectColor
//
// 材质章节引入两个结构体，让控制更精细：
//
//   Material { ambient, diffuse, specular, shininess }
//     定义物体如何反射光线——每种反射分量可以有独立的颜色
//
//   Light { position, ambient, diffuse, specular }
//     定义光源的属性——环境光、漫反射光、镜面反射光可以不同颜色
//
// 材质不同，物体呈现的视觉效果截然不同。
// 本 Demo 内置了 10 种常见材质预设（金、银、青铜、铜、红塑料等）。
//

int runMaterialsDemo()
{
    const unsigned int SCR_WIDTH  = 800;
    const unsigned int SCR_HEIGHT = 600;

    // ========== 1. 材质预设数据 ==========
    //
    // 每个材质包含四个属性：
    //   ambient  — 环境光反射颜色（物体本身在暗处的颜色）
    //   diffuse  — 漫反射反射颜色（物体在光照下的主色）
    //   specular — 镜面反射反射颜色（高光的颜色）
    //   shininess — 反光度
    //
    struct MaterialPreset {
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
        float shininess;
        const char* name;
    };

    const int NUM_MATERIALS = 10;
    const MaterialPreset MATERIALS[NUM_MATERIALS] = {
        // 金 — 亮黄色，强金色高光，高反光
        { {0.247f, 0.224f, 0.064f}, {0.752f, 0.606f, 0.226f}, {0.628f, 0.556f, 0.366f}, 51.2f, "Gold" },
        // 银 — 灰白色，白色高光，高反光
        { {0.192f, 0.192f, 0.192f}, {0.507f, 0.507f, 0.507f}, {0.508f, 0.508f, 0.508f}, 51.2f, "Silver" },
        // 青铜 — 暖棕色，暖色高光
        { {0.212f, 0.127f, 0.054f}, {0.714f, 0.428f, 0.181f}, {0.394f, 0.272f, 0.167f}, 25.6f, "Bronze" },
        // 铜 — 红棕色，暖色高光，范围较大
        { {0.192f, 0.073f, 0.032f}, {0.703f, 0.270f, 0.083f}, {0.257f, 0.138f, 0.086f}, 12.8f, "Copper" },
        // 红塑料 — 无环境色，鲜红漫反射，白色高光
        { {0.000f, 0.000f, 0.000f}, {0.500f, 0.000f, 0.000f}, {0.700f, 0.600f, 0.600f}, 32.0f, "Red Plastic" },
        // 青橡胶 — 无环境色，暗青漫反射，很弱的高光
        { {0.000f, 0.000f, 0.000f}, {0.010f, 0.360f, 0.370f}, {0.220f, 0.430f, 0.460f}, 12.8f, "Cyan Rubber" },
        // 翡翠 — 深绿，亮绿漫反射，淡绿高光，高反光
        { {0.022f, 0.174f, 0.022f}, {0.076f, 0.614f, 0.075f}, {0.633f, 0.727f, 0.633f}, 76.8f, "Emerald" },
        // 黑曜石 — 深紫黑，暗紫漫反射，亮紫高光
        { {0.054f, 0.050f, 0.066f}, {0.183f, 0.171f, 0.225f}, {0.333f, 0.333f, 0.521f}, 38.4f, "Obsidian" },
        // 珍珠 — 暖粉色，浅粉漫反射，柔和白色高光
        { {0.250f, 0.207f, 0.207f}, {1.000f, 0.829f, 0.829f}, {0.297f, 0.297f, 0.297f}, 11.3f, "Pearl" },
        // 红宝石 — 深红，亮红漫反射，亮粉高光，高反光
        { {0.175f, 0.012f, 0.012f}, {0.614f, 0.041f, 0.041f}, {0.727f, 0.627f, 0.627f}, 76.8f, "Ruby" },
    };

    // 当前材质属性（初始为 Gold）
    int currentMaterial = 0;
    glm::vec3 matAmbient   = MATERIALS[0].ambient;
    glm::vec3 matDiffuse   = MATERIALS[0].diffuse;
    glm::vec3 matSpecular  = MATERIALS[0].specular;
    float     matShininess = MATERIALS[0].shininess;

    // 当前材质颜色数组（用于 ImGui ColorEdit3）
    float matAmbientArray[3]   = { matAmbient.r,   matAmbient.g,   matAmbient.b   };
    float matDiffuseArray[3]   = { matDiffuse.r,   matDiffuse.g,   matDiffuse.b   };
    float matSpecularArray[3]  = { matSpecular.r,  matSpecular.g,  matSpecular.b  };

    // ========== 2. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Materials | 10 material presets with editable Light properties",
        NULL, NULL);
    if (window == NULL)
    {
        std::cout << "⨯ Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // ========== 3. 初始化 GLAD ==========
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "⨯ Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
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
    //
    // lightingShader — 使用 materials.frag（Material + Light 结构体）
    //   顶点着色器复用 basic_lighting.vert（位置 + 法线 + 纹理坐标）
    //
    Shader lightingShader("shaders/lighting/basic_lighting.vert",
                          "shaders/lighting/materials.frag", true);
    Shader lightCubeShader("shaders/lighting/light_cube.vert",
                           "shaders/lighting/light_cube.frag", true);

    // ========== 6. 顶点数据（与基础光照章节相同） ==========
    float vertices[] = {
        // ============ 背面 (Z-) ============
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        // ============ 正面 (Z+) ============
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        // ============ 左面 (X-) ============
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        // ============ 右面 (X+) ============
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        // ============ 底面 (Y-) ============
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        // ============ 顶面 (Y+) ============
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
    };
    unsigned int indices[] = {
         0,  1,  2,    2,  3,  0,
         4,  5,  6,    6,  7,  4,
         8,  9, 10,   10, 11,  8,
        12, 13, 14,   14, 15, 12,
        16, 17, 18,   18, 19, 16,
        20, 21, 22,   22, 23, 20
    };

    // ========== 7. VAO / VBO / EBO ==========
    unsigned int cubeVAO, lightVAO, VBO, EBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenVertexArrays(1, &lightVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // cubeVAO：主物体（位置 + 法线 + 纹理坐标）
    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // lightVAO：光源标记（只取位置）
    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ========== 8. 用户控制参数 ==========

    // ---- 光源 ----
    glm::vec3 lightPos   = glm::vec3(1.2f, 1.0f, 2.0f);
    float lightPosArray[3] = { 1.2f, 1.0f, 2.0f };

    // 光源的三种分量可以有不同的颜色
    // ambient  通常很暗（模拟环境光）
    // diffuse  通常最亮（光源的主色）
    // specular 通常较亮（高光颜色）
    glm::vec3 lightAmbient  = glm::vec3(0.2f, 0.2f, 0.2f);   // 微弱白光
    glm::vec3 lightDiffuse  = glm::vec3(0.8f, 0.8f, 0.8f);   // 亮白光
    glm::vec3 lightSpecular = glm::vec3(1.0f, 1.0f, 1.0f);   // 纯白高光

    float lightAmbientArray[3]  = { 0.2f, 0.2f, 0.2f };
    float lightDiffuseArray[3]  = { 0.8f, 0.8f, 0.8f };
    float lightSpecularArray[3] = { 1.0f, 1.0f, 1.0f };

    bool lightAutoRotate = false;

    // ---- 摄像机 ----
    glm::vec3 camPos    = glm::vec3(0.0f, 0.0f, 5.0f);
    float     fov       = 45.0f;
    float camMoveSpeed  = 0.08f;

    // ---- 调试 ----
    bool showDebugPanel = true;
    float clearColor[3] = { 0.1f, 0.1f, 0.1f };

    // ========== 9. 清屏颜色 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 10. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  材质（Materials）— 10 种材质预设" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  核心概念: Material vs Light 结构体" << std::endl;
    std::cout << "  ESC       → 退出" << std::endl;
    std::cout << "  WASD/箭头 → 摄像机移动" << std::endl;
    std::cout << "  Tab       → 显示/隐藏调试面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 11. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 11a. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
        {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        }
        else { tabPressed = false; }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camPos += glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camPos -= glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camPos -= glm::vec3(camMoveSpeed, 0.0f, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camPos += glm::vec3(camMoveSpeed, 0.0f, 0.0f);

        // ===== 11b. 光源自动旋转 =====
        if (lightAutoRotate)
        {
            float radius = glm::length(lightPos);
            float angle  = (float)glfwGetTime() * 0.8f;
            lightPos.x = cos(angle) * radius;
            lightPos.z = sin(angle) * radius;
            lightPosArray[0] = lightPos.x;
            lightPosArray[1] = lightPos.y;
            lightPosArray[2] = lightPos.z;
        }

        // ===== 11c. 清空缓冲 =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== 11d. ImGui：开始新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 11e. MVP 矩阵 =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // ================================================================
        // 第一部分：渲染主物体（应用材质）
        // ================================================================

        lightingShader.use();

        // ---- MVP ----
        glm::mat4 model = glm::mat4(1.0f);
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setMat4("model", model);

        // ---- 设置材质 uniform（结构体成员用点号访问） ----
        lightingShader.setVec3("material.ambient",   matAmbient.r,  matAmbient.g,  matAmbient.b);
        lightingShader.setVec3("material.diffuse",   matDiffuse.r,  matDiffuse.g,  matDiffuse.b);
        lightingShader.setVec3("material.specular",  matSpecular.r, matSpecular.g, matSpecular.b);
        lightingShader.setFloat("material.shininess", matShininess);

        // ---- 设置光源 uniform（结构体成员用点号访问） ----
        lightingShader.setVec3("light.position",  lightPos.x, lightPos.y, lightPos.z);
        lightingShader.setVec3("light.ambient",   lightAmbient.r,  lightAmbient.g,  lightAmbient.b);
        lightingShader.setVec3("light.diffuse",   lightDiffuse.r,  lightDiffuse.g,  lightDiffuse.b);
        lightingShader.setVec3("light.specular",  lightSpecular.r, lightSpecular.g, lightSpecular.b);

        // ---- 摄像机位置（用于镜面反射的视线方向） ----
        lightingShader.setVec3("viewPos", camPos.x, camPos.y, camPos.z);

        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // ================================================================
        // 第二部分：渲染光源标记
        // ================================================================

        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);

        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));
        lightCubeShader.setMat4("model", model);
        // 光源标记的颜色使用 light.diffuse（最接近光源的「主色」）
        lightCubeShader.setVec3("lightColor", lightDiffuse.r, lightDiffuse.g, lightDiffuse.b);

        glBindVertexArray(lightVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // ===== 11f. ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Materials");

            // ---- 性能 ----
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 材质选择 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Material");

            // 材质预设下拉框
            // 构造预设名称列表（用 \0 分隔）
            // 选中的项写入 currentMaterial，然后更新材质属性
            {
                char comboItems[256] = {};
                int offset = 0;
                for (int i = 0; i < NUM_MATERIALS; i++)
                {
                    const char* name = MATERIALS[i].name;
                    for (const char* c = name; *c; c++)
                        comboItems[offset++] = *c;
                    comboItems[offset++] = '\0';
                }
                comboItems[offset] = '\0';

                int prevMaterial = currentMaterial;
                ImGui::Combo("Preset", &currentMaterial, comboItems);
                if (currentMaterial != prevMaterial)
                {
                    matAmbient   = MATERIALS[currentMaterial].ambient;
                    matDiffuse   = MATERIALS[currentMaterial].diffuse;
                    matSpecular  = MATERIALS[currentMaterial].specular;
                    matShininess = MATERIALS[currentMaterial].shininess;
                    matAmbientArray[0]  = matAmbient.r;  matAmbientArray[1]  = matAmbient.g;  matAmbientArray[2]  = matAmbient.b;
                    matDiffuseArray[0]  = matDiffuse.r;  matDiffuseArray[1]  = matDiffuse.g;  matDiffuseArray[2]  = matDiffuse.b;
                    matSpecularArray[0] = matSpecular.r; matSpecularArray[1] = matSpecular.g; matSpecularArray[2] = matSpecular.b;
                }
            }

            // ---- 材质属性编辑 ----
            ImGui::Text("Material Properties:");
            bool matEdited = false;
            matEdited |= ImGui::ColorEdit3("Ambient",  matAmbientArray);
            matEdited |= ImGui::ColorEdit3("Diffuse",  matDiffuseArray);
            matEdited |= ImGui::ColorEdit3("Specular", matSpecularArray);
            matEdited |= ImGui::SliderFloat("Shininess", &matShininess, 1.0f, 256.0f, "%.1f");
            if (matEdited)
            {
                matAmbient  = glm::vec3(matAmbientArray[0],  matAmbientArray[1],  matAmbientArray[2]);
                matDiffuse  = glm::vec3(matDiffuseArray[0],  matDiffuseArray[1],  matDiffuseArray[2]);
                matSpecular = glm::vec3(matSpecularArray[0], matSpecularArray[1], matSpecularArray[2]);
            }

            // 显示材质预览
            ImGui::Separator();
            ImGui::Text("Preview:");
            ImGui::ColorButton("Ambient",  ImVec4(matAmbient.r,  matAmbient.g,  matAmbient.b,  1.0f), 0, ImVec2(40, 20));
            ImGui::SameLine(); ImGui::Text(" Ambient");
            ImGui::ColorButton("Diffuse",  ImVec4(matDiffuse.r,  matDiffuse.g,  matDiffuse.b,  1.0f), 0, ImVec2(40, 20));
            ImGui::SameLine(); ImGui::Text(" Diffuse");
            ImGui::ColorButton("Specular", ImVec4(matSpecular.r, matSpecular.g, matSpecular.b, 1.0f), 0, ImVec2(40, 20));
            ImGui::SameLine(); ImGui::Text(" Specular  Shininess: %.0f", matShininess);

            ImGui::Separator();

            // ---- 光源属性 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Light Properties");
            ImGui::SliderFloat3("Position", lightPosArray, -4.0f, 4.0f, "%.1f");
            lightPos = glm::vec3(lightPosArray[0], lightPosArray[1], lightPosArray[2]);
            ImGui::Checkbox("Auto Rotate", &lightAutoRotate);

            ImGui::ColorEdit3("Ambient",  lightAmbientArray);
            ImGui::ColorEdit3("Diffuse",  lightDiffuseArray);
            ImGui::ColorEdit3("Specular", lightSpecularArray);
            lightAmbient  = glm::vec3(lightAmbientArray[0],  lightAmbientArray[1],  lightAmbientArray[2]);
            lightDiffuse  = glm::vec3(lightDiffuseArray[0],  lightDiffuseArray[1],  lightDiffuseArray[2]);
            lightSpecular = glm::vec3(lightSpecularArray[0], lightSpecularArray[1], lightSpecularArray[2]);

            ImGui::Separator();

            // ---- 选项 ----
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "Options");
            ImGui::SliderFloat("Camera Speed", &camMoveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::ColorEdit3("Clear Color", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            ImGui::TextDisabled("WASD/Arrows: Move  |  Tab: Panel  |  ESC: Quit");

            ImGui::End();
        }

        // ===== 11g. ImGui：渲染 + 交换缓冲 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 12. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glfwTerminate();
    return 0;
}


// ================================================================
// 光照贴图（Lighting Maps）
// ================================================================
//
// 核心变化：用纹理替代 uniform 控制材质属性
//
//   之前（材质章节）：
//     material.diffuse  = 统一颜色（整个物体相同）
//     material.specular = 统一颜色（整个物体相同）
//
//   现在（光照贴图）：
//     material.diffuse  = sampler2D（漫反射贴图，即平时说的「纹理」）
//     material.specular = sampler2D（高光贴图，控制哪些区域反光）
//
// Diffuse Map（漫反射贴图）：
//   控制物体不同区域的颜色。白色碎石纹路是漫反射贴图的内容。
//
// Specular Map（高光贴图）：
//   控制物体不同区域的镜面反射强度。
//   白色 = 该区域高光强（金属包角、光滑表面）
//   黑色 = 该区域无高光（木头、粗糙表面）
//
// 一个物体可以同时有光滑和粗糙区域——这就是光照贴图的威力。
//

int runLightingMapsDemo()
{
    const unsigned int SCR_WIDTH  = 800;
    const unsigned int SCR_HEIGHT = 600;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Lighting Maps | Diffuse + Specular texture control",
        NULL, NULL);
    if (window == NULL) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    // ========== 2. 初始化 GLAD ==========
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glEnable(GL_DEPTH_TEST);

    // ========== 3. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 4. 编译着色器 ==========
    Shader lightingShader("shaders/lighting/basic_lighting.vert",
                          "shaders/lighting/lighting_maps.frag", true);
    Shader lightCubeShader("shaders/lighting/light_cube.vert",
                           "shaders/lighting/light_cube.frag", true);

    // ========== 5. 顶点数据（与之前相同：位置 + 法线 + 纹理坐标） ==========
    float vertices[] = {
        // 背面 (Z-)
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        // 正面 (Z+)
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        // 左面 (X-)
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        // 右面 (X+)
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        // 底面 (Y-)
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        // 顶面 (Y+)
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
    };
    unsigned int indices[] = {
         0,  1,  2,    2,  3,  0,    4,  5,  6,    6,  7,  4,
         8,  9, 10,   10, 11,  8,   12, 13, 14,   14, 15, 12,
        16, 17, 18,   18, 19, 16,   20, 21, 22,   22, 23, 20
    };

    // ========== 6. VAO / VBO / EBO ==========
    unsigned int cubeVAO, lightVAO, VBO, EBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenVertexArrays(1, &lightVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ========== 7. 加载纹理 ==========
    //
    // ★★★ 核心：加载两张贴图 ★★★
    //
    //   container2.png              → 漫反射贴图（木箱纹理）
    //   container2_specular.png     → 高光贴图（白=金属包角，黑=木头）
    //
    std::cout << "\n--- 加载光照贴图 ---" << std::endl;
    unsigned int diffuseMap  = loadTexture("textures/container2.png");
    unsigned int specularMap = loadTexture("textures/container2_specular.png");

    // ========== 8. 设置纹理单元 ==========
    //
    // material.diffuse  → GL_TEXTURE0（默认纹理单元）
    // material.specular → GL_TEXTURE1
    //
    lightingShader.use();
    lightingShader.setInt("material.diffuse",  0);
    lightingShader.setInt("material.specular", 1);

    // ========== 9. 用户控制参数 ==========

    // ---- 光源 ----
    glm::vec3 lightPos   = glm::vec3(1.2f, 1.0f, 2.0f);
    float lightPosArray[3] = { 1.2f, 1.0f, 2.0f };

    glm::vec3 lightAmbient  = glm::vec3(0.2f, 0.2f, 0.2f);
    glm::vec3 lightDiffuse  = glm::vec3(0.8f, 0.8f, 0.8f);
    glm::vec3 lightSpecular = glm::vec3(1.0f, 1.0f, 1.0f);
    float lightAmbientArray[3]  = { 0.2f, 0.2f, 0.2f };
    float lightDiffuseArray[3]  = { 0.8f, 0.8f, 0.8f };
    float lightSpecularArray[3] = { 1.0f, 1.0f, 1.0f };

    bool lightAutoRotate = false;

    // ---- 材质 ----
    float shininess = 64.0f;
    bool useSpecularMap  = true;     // 启用高光贴图
    bool useSpecularMapPrev = true;

    // 当不启用高光贴图时的替代颜色
    glm::vec3 specularOverride   = glm::vec3(0.5f, 0.5f, 0.5f);
    float specularOverrideArray[3] = { 0.5f, 0.5f, 0.5f };

    // ---- 摄像机 ----
    glm::vec3 camPos   = glm::vec3(0.0f, 0.0f, 5.0f);
    float     fov      = 45.0f;
    float     camMoveSpeed = 0.08f;

    // ---- 调试 ----
    bool showDebugPanel = true;
    float clearColor[3] = { 0.1f, 0.1f, 0.1f };

    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 10. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  光照贴图（Lighting Maps）" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  核心: Diffuse Map + Specular Map" << std::endl;
    std::cout << "  ESC       → 退出" << std::endl;
    std::cout << "  WASD/箭头 → 摄像机移动" << std::endl;
    std::cout << "  Tab       → 面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 11. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 11a. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
        {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        }
        else { tabPressed = false; }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camPos += glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camPos -= glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camPos -= glm::vec3(camMoveSpeed, 0.0f, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camPos += glm::vec3(camMoveSpeed, 0.0f, 0.0f);

        // ===== 11b. 高光贴图切换提示 =====
        if (useSpecularMap != useSpecularMapPrev)
        {
            std::cout << (useSpecularMap ? "◆ 使用高光贴图" : "◆ 使用统一高光颜色") << std::endl;
            useSpecularMapPrev = useSpecularMap;
        }

        // ===== 11c. 光源自动旋转 =====
        if (lightAutoRotate)
        {
            float radius = glm::length(lightPos);
            float angle  = (float)glfwGetTime() * 0.8f;
            lightPos.x = cos(angle) * radius;
            lightPos.z = sin(angle) * radius;
            lightPosArray[0] = lightPos.x;
            lightPosArray[1] = lightPos.y;
            lightPosArray[2] = lightPos.z;
        }

        // ===== 11d. 清空缓冲 =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== 11e. ImGui：开始新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 11f. MVP 矩阵 =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // ================================================================
        // 第一部分：渲染主物体（使用光照贴图）
        // ================================================================

        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setMat4("model", glm::mat4(1.0f));

        // ---- 设置光源 ----
        lightingShader.setVec3("light.position",  lightPos.x, lightPos.y, lightPos.z);
        lightingShader.setVec3("light.ambient",   lightAmbient.r,  lightAmbient.g,  lightAmbient.b);
        lightingShader.setVec3("light.diffuse",   lightDiffuse.r,  lightDiffuse.g,  lightDiffuse.b);
        lightingShader.setVec3("light.specular",  lightSpecular.r, lightSpecular.g, lightSpecular.b);

        // ---- 设置材质 ----
        lightingShader.setFloat("material.shininess", shininess);
        lightingShader.setBool("useSpecularMap", useSpecularMap);
        lightingShader.setVec3("specularOverride",
            specularOverride.r, specularOverride.g, specularOverride.b);

        // ---- 设置摄像机位置 ----
        lightingShader.setVec3("viewPos", camPos.x, camPos.y, camPos.z);

        // ---- ★★★ 绑定纹理到对应的纹理单元 ★★★ ----
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularMap);

        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // ================================================================
        // 第二部分：渲染光源标记
        // ================================================================

        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));
        lightCubeShader.setMat4("model", model);
        lightCubeShader.setVec3("lightColor", lightDiffuse.r, lightDiffuse.g, lightDiffuse.b);

        glBindVertexArray(lightVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // ===== 11g. ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Lighting Maps");

            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 光照贴图说明 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Lighting Maps");
            ImGui::TextWrapped(
                "Diffuse Map: 物体颜色来自纹理\n"
                "Specular Map: 白色=高光强, 黑色=无高光");
            ImGui::Separator();

            // ---- 高光贴图开关 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Specular Map");
            ImGui::Checkbox("Use Specular Map", &useSpecularMap);
            if (!useSpecularMap)
            {
                ImGui::ColorEdit3("Fallback Color", specularOverrideArray);
                specularOverride = glm::vec3(
                    specularOverrideArray[0],
                    specularOverrideArray[1],
                    specularOverrideArray[2]);
            }
            ImGui::SliderFloat("Shininess", &shininess, 1.0f, 256.0f, "%.0f");
            ImGui::Separator();

            // ---- 光源 ----
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Light");
            ImGui::SliderFloat3("Position", lightPosArray, -4.0f, 4.0f, "%.1f");
            lightPos = glm::vec3(lightPosArray[0], lightPosArray[1], lightPosArray[2]);
            ImGui::Checkbox("Auto Rotate", &lightAutoRotate);

            ImGui::ColorEdit3("Ambient",  lightAmbientArray);
            ImGui::ColorEdit3("Diffuse",  lightDiffuseArray);
            ImGui::ColorEdit3("Specular", lightSpecularArray);
            lightAmbient  = glm::vec3(lightAmbientArray[0],  lightAmbientArray[1],  lightAmbientArray[2]);
            lightDiffuse  = glm::vec3(lightDiffuseArray[0],  lightDiffuseArray[1],  lightDiffuseArray[2]);
            lightSpecular = glm::vec3(lightSpecularArray[0], lightSpecularArray[1], lightSpecularArray[2]);
            ImGui::Separator();

            // ---- 预览 ----
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "Camera & Options");
            ImGui::SliderFloat("Speed", &camMoveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::ColorEdit3("Clear", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            ImGui::TextDisabled("WASD: Move  |  Tab: Panel  |  ESC: Quit");

            ImGui::End();
        }

        // ===== 11h. ImGui：渲染 + 交换缓冲 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 12. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &diffuseMap);
    glDeleteTextures(1, &specularMap);
    glfwTerminate();
    return 0;
}


// ================================================================
// 投光物（Light Casters）— 方向光 / 点光源 / 聚光灯
// ================================================================
//
// 三种光源类型的区别：
//
//   方向光（Directional Light）
//     光线平行，全场景光照均匀，无衰减。
//     模拟太阳光。用 direction 定义方向。
//
//   点光源（Point Light）
//     从一点向所有方向发光，有距离衰减。
//     模拟灯泡。用 position + 衰减系数定义。
//
//   聚光灯（Spotlight）
//     锥体光照，从一点向特定方向发光。
//     模拟手电筒。用 position + direction + cutOff 定义。
//
// 本 Demo 用 10 个立方体展示不同光源类型的效果差异。
//

int runLightCastersDemo()
{
    const unsigned int SCR_WIDTH  = 800;
    const unsigned int SCR_HEIGHT = 600;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Light Casters | 0:Dir 1:Point 2:Spot", NULL, NULL);
    if (window == NULL) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    // ========== 2. 初始化 GLAD ==========
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glEnable(GL_DEPTH_TEST);

    // ========== 3. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 4. 编译着色器 ==========
    Shader lightingShader("shaders/lighting/basic_lighting.vert",
                          "shaders/lighting/light_casters.frag", true);
    Shader lightCubeShader("shaders/lighting/light_cube.vert",
                           "shaders/lighting/light_cube.frag", true);

    // ========== 5. 顶点数据 ==========
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
    };
    unsigned int indices[] = {
         0,  1,  2,    2,  3,  0,    4,  5,  6,    6,  7,  4,
         8,  9, 10,   10, 11,  8,   12, 13, 14,   14, 15, 12,
        16, 17, 18,   18, 19, 16,   20, 21, 22,   22, 23, 20
    };

    // ========== 6. VAO / VBO / EBO ==========
    unsigned int cubeVAO, lightVAO, VBO, EBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenVertexArrays(1, &lightVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ========== 7. 加载纹理 ==========
    unsigned int diffuseMap  = loadTexture("textures/container2.png");
    unsigned int specularMap = loadTexture("textures/container2_specular.png");

    // ========== 8. 设置纹理单元 ==========
    lightingShader.use();
    lightingShader.setInt("material.diffuse",  0);
    lightingShader.setInt("material.specular", 1);

    // ========== 9. 立方体位置（10 个，展示距离衰减效果） ==========
    glm::vec3 cubePositions[] = {
        glm::vec3( 0.0f,  0.0f,  0.0f),
        glm::vec3( 2.0f,  0.5f,  0.0f),
        glm::vec3(-2.0f,  0.5f,  0.0f),
        glm::vec3( 0.0f,  2.0f,  0.0f),
        glm::vec3( 0.0f, -2.0f,  0.0f),
        glm::vec3( 3.0f,  1.5f, -2.0f),
        glm::vec3(-3.0f,  1.5f, -2.0f),
        glm::vec3( 1.5f, -1.5f, -3.0f),
        glm::vec3(-1.5f, -1.5f, -3.0f),
        glm::vec3( 0.0f,  0.0f, -4.0f)
    };
    const int NUM_CUBES = sizeof(cubePositions) / sizeof(cubePositions[0]);

    // ========== 10. 用户控制参数 ==========

    // ---- 光源类型 ----
    // 0 = Directional Light
    // 1 = Point Light
    // 2 = Spotlight
    // ★ 默认用点光源，一启动就能看到高光效果
    int lightType = 1;

    // ---- 光源颜色（三种类型共用） ----
    glm::vec3 lightAmbient  = glm::vec3(0.15f, 0.15f, 0.15f);
    glm::vec3 lightDiffuse  = glm::vec3(0.8f, 0.8f, 0.8f);
    glm::vec3 lightSpecular = glm::vec3(1.0f, 1.0f, 1.0f);
    float lightAmbientArray[3]  = { 0.15f, 0.15f, 0.15f };
    float lightDiffuseArray[3]  = { 0.8f,  0.8f,  0.8f  };
    float lightSpecularArray[3] = { 1.0f,  1.0f,  1.0f  };

    // ---- 方向光 / 聚光灯：方向 ----
    // 默认从右上往前照，让前面和顶面都能被照亮
    glm::vec3 lightDirection   = glm::vec3(-0.3f, -0.5f, -0.8f);
    float lightDirArray[3]     = { -0.3f, -0.5f, -0.8f };

    // ---- 点光源 / 聚光灯：位置 + 衰减 ----
    // 放在物体右前方，让 specular 高光清晰可见
    glm::vec3 lightPos   = glm::vec3(1.5f, 1.0f, 2.0f);
    float lightPosArray[3] = { 1.5f, 1.0f, 2.0f };

    // 衰减系数（适用于距离 ~32 的范围）
    float attenuationConstant  = 1.0f;
    float attenuationLinear    = 0.14f;
    float attenuationQuadratic = 0.07f;

    // ---- 聚光灯：锥体角度（角度制，着色器中转余弦） ----
    float cutOffDeg      = 12.5f;   // 内锥角
    float outerCutOffDeg = 17.5f;   // 外锥角

    // ---- 材质 ----
    float shininess = 64.0f;

    // ---- 摄像机 ----
    glm::vec3 camPos    = glm::vec3(0.0f, 0.0f, 6.0f);
    float     fov       = 45.0f;
    float     camMoveSpeed = 0.10f;

    // ---- 调试 ----
    bool showDebugPanel = true;
    float clearColor[3] = { 0.1f, 0.1f, 0.1f };
    bool lightAutoRotate = false;

    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 11. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  投光物（Light Casters）" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  默认: 点光源（一启动即可见高光）" << std::endl;
    std::cout << "  0: 方向光 (Directional)" << std::endl;
    std::cout << "  1: 点光源 (Point)" << std::endl;
    std::cout << "  2: 聚光灯 (Spotlight)" << std::endl;
    std::cout << "  ESC: 退出  |  WASD: 移动  |  Tab: 面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 12. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 12a. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // 数字键 0/1/2 切换光源类型
        static bool key0 = false, key1 = false, key2 = false;
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) { if (!key0) { lightType = 0; key0 = true; std::cout << "◆ 方向光 (Directional)" << std::endl; } } else { key0 = false; }
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) { if (!key1) { lightType = 1; key1 = true; std::cout << "◆ 点光源 (Point)" << std::endl; } } else { key1 = false; }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) { if (!key2) { lightType = 2; key2 = true; std::cout << "◆ 聚光灯 (Spotlight)" << std::endl; } } else { key2 = false; }

        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) { if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; } } else { tabPressed = false; }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camPos += glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camPos -= glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camPos -= glm::vec3(camMoveSpeed, 0.0f, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camPos += glm::vec3(camMoveSpeed, 0.0f, 0.0f);

        // ===== 12b. 光源自动旋转（仅 point / spotlight） =====
        if (lightAutoRotate && lightType > 0)
        {
            float radius = glm::length(lightPos);
            float angle  = (float)glfwGetTime() * 0.8f;
            lightPos.x = cos(angle) * radius;
            lightPos.z = sin(angle) * radius;
            lightPosArray[0] = lightPos.x;
            lightPosArray[1] = lightPos.y;
            lightPosArray[2] = lightPos.z;
        }

        // ===== 12c. 清空缓冲 =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== 12d. ImGui：开始新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 12e. MVP 矩阵 =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // ================================================================
        // 第一部分：渲染所有立方体
        // ================================================================

        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setInt("lightType", lightType);
        lightingShader.setVec3("viewPos", camPos.x, camPos.y, camPos.z);

        // ---- 光源通用属性 ----
        lightingShader.setVec3("light.ambient",   lightAmbientArray[0],  lightAmbientArray[1],  lightAmbientArray[2]);
        lightingShader.setVec3("light.diffuse",   lightDiffuseArray[0],  lightDiffuseArray[1],  lightDiffuseArray[2]);
        lightingShader.setVec3("light.specular",  lightSpecularArray[0], lightSpecularArray[1], lightSpecularArray[2]);

        // ---- 方向 / 位置 ----
        glm::vec3 dir = glm::normalize(lightDirection);
        lightingShader.setVec3("light.direction", dir.x, dir.y, dir.z);
        lightingShader.setVec3("light.position",  lightPosArray[0], lightPosArray[1], lightPosArray[2]);

        // ---- 衰减 ----
        lightingShader.setFloat("light.constant",  attenuationConstant);
        lightingShader.setFloat("light.linear",    attenuationLinear);
        lightingShader.setFloat("light.quadratic", attenuationQuadratic);

        // ---- 聚光灯锥体（传余弦值） ----
        lightingShader.setFloat("light.cutOff",      cos(glm::radians(cutOffDeg)));
        lightingShader.setFloat("light.outerCutOff", cos(glm::radians(outerCutOffDeg)));

        // ---- 材质 ----
        lightingShader.setFloat("material.shininess", shininess);

        // ---- 绑定纹理 ----
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularMap);

        // ---- 绘制 10 个立方体 ----
        glBindVertexArray(cubeVAO);
        for (int i = 0; i < NUM_CUBES; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);

            // 每个立方体有不同的旋转，让场景更生动
            float angle = 20.0f * (i + 1);
            model = glm::rotate(model, glm::radians(angle),
                                glm::vec3(1.0f, 0.3f, 0.5f));

            lightingShader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // ================================================================
        // 第二部分：渲染光源标记（仅点光源/聚光灯）
        // ================================================================

        if (lightType > 0)
        {
            lightCubeShader.use();
            lightCubeShader.setMat4("projection", projection);
            lightCubeShader.setMat4("view", view);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPosArray[0], lightPosArray[1], lightPosArray[2]));
            model = glm::scale(model, glm::vec3(0.2f));
            lightCubeShader.setMat4("model", model);
            lightCubeShader.setVec3("lightColor", lightDiffuseArray[0], lightDiffuseArray[1], lightDiffuseArray[2]);

            glBindVertexArray(lightVAO);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // ===== 12f. ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Light Casters");

            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 光源类型 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Light Type");
            ImGui::RadioButton("Directional", &lightType, 0); ImGui::SameLine();
            ImGui::RadioButton("Point",       &lightType, 1); ImGui::SameLine();
            ImGui::RadioButton("Spotlight",   &lightType, 2);

            // 切换时打印日志
            static int prevType = -1;
            if (lightType != prevType) {
                const char* names[] = { "方向光 (Directional)", "点光源 (Point)", "聚光灯 (Spotlight)" };
                std::cout << "◆ " << names[lightType] << std::endl;
                prevType = lightType;
            }
            ImGui::Separator();

            // ---- 光源颜色 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Light Colors");
            ImGui::ColorEdit3("Ambient",  lightAmbientArray);
            ImGui::ColorEdit3("Diffuse",  lightDiffuseArray);
            ImGui::ColorEdit3("Specular", lightSpecularArray);
            ImGui::Separator();

            // ---- 方向光 / 聚光灯：方向 ----
            if (lightType == 0 || lightType == 2)
            {
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Direction");
                ImGui::SliderFloat3("Light Dir", lightDirArray, -1.0f, 1.0f, "%.2f");
                // 保持方向向量为合理长度
                if (glm::length(glm::vec3(lightDirArray[0], lightDirArray[1], lightDirArray[2])) > 0.01f)
                    lightDirection = glm::vec3(lightDirArray[0], lightDirArray[1], lightDirArray[2]);
                else
                    lightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
            }

            // ---- 点光源 / 聚光灯：位置 ----
            if (lightType > 0)
            {
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Position");
                ImGui::SliderFloat3("Light Pos", lightPosArray, -5.0f, 5.0f, "%.1f");
                ImGui::Checkbox("Auto Rotate", &lightAutoRotate);
            }
            ImGui::Separator();

            // ---- 衰减（仅 point / spotlight） ----
            if (lightType > 0)
            {
                ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.8f, 1.0f), "Attenuation");
                ImGui::SliderFloat("Constant",  &attenuationConstant,  0.01f, 2.0f, "%.2f");
                ImGui::SliderFloat("Linear",    &attenuationLinear,    0.001f, 1.0f, "%.3f");
                ImGui::SliderFloat("Quadratic", &attenuationQuadratic, 0.001f, 2.0f, "%.3f");

                // 显示当前距离的衰减值预览
                float dist = glm::length(glm::vec3(lightPosArray[0], lightPosArray[1], lightPosArray[2]) - glm::vec3(0.0f));
                float atten = 1.0f / (attenuationConstant + attenuationLinear * dist + attenuationQuadratic * dist * dist);
                ImGui::Text("Attenuation at center: %.3f", atten);
            }

            // ---- 聚光灯锥体 ----
            if (lightType == 2)
            {
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Spotlight Cone");
                ImGui::SliderFloat("Inner Angle", &cutOffDeg, 1.0f, 90.0f, "%.1f°");
                ImGui::SliderFloat("Outer Angle", &outerCutOffDeg, 1.0f, 90.0f, "%.1f°");
                if (outerCutOffDeg < cutOffDeg) outerCutOffDeg = cutOffDeg;

                // 显示余弦值对照
                ImGui::Text("cos(inner=%.1f°) = %.3f", cutOffDeg, cos(glm::radians(cutOffDeg)));
                ImGui::Text("cos(outer=%.1f°) = %.3f", outerCutOffDeg, cos(glm::radians(outerCutOffDeg)));
            }
            ImGui::Separator();

            // ---- 材质 ----
            ImGui::SliderFloat("Shininess", &shininess, 1.0f, 256.0f, "%.0f");
            ImGui::Separator();

            // ---- 选项 ----
            ImGui::SliderFloat("Speed", &camMoveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::ColorEdit3("Clear", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            ImGui::TextDisabled("0/1/2: Light Type  |  WASD: Move  |  Tab: Panel");

            ImGui::End();
        }

        // ===== 12g. ImGui：渲染 + 交换缓冲 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 13. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &diffuseMap);
    glDeleteTextures(1, &specularMap);
    glfwTerminate();
    return 0;
}


// ================================================================
// 多光源（Multiple Lights）— 方向光 + 4 个点光源 + 聚光灯
// ================================================================
//
// 本 Demo 在一个场景中同时使用三种类型共 6 个光源：
//
//   ① 1 个 Directional Light（太阳）
//      全场景均匀光照，方向从右上斜照。
//
//   ② 4 个 Point Lights（不同颜色的点光源）
//      分布在场景不同位置，各自独立衰减。
//      用不同颜色标记光源位置。
//
//   ③ 1 个 SpotLight（手电筒）
//      跟随摄像机位置，照亮正前方区域。
//
// 着色器中用函数封装每种光源的计算逻辑，
// 最终颜色 = 方向光 + 各点光源之和 + 聚光灯。
//

int runMultipleLightsDemo()
{
    const unsigned int SCR_WIDTH  = 800;
    const unsigned int SCR_HEIGHT = 600;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Multiple Lights | Sun + 4 Point Lights + Flashlight",
        NULL, NULL);
    if (window == NULL) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    // ========== 2. 初始化 GLAD ==========
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glEnable(GL_DEPTH_TEST);

    // ========== 3. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 4. 编译着色器 ==========
    Shader lightingShader("shaders/lighting/basic_lighting.vert",
                          "shaders/lighting/multiple_lights.frag", true);
    Shader lightCubeShader("shaders/lighting/light_cube.vert",
                           "shaders/lighting/light_cube.frag", true);

    // ========== 5. 顶点数据 ==========
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
    };
    unsigned int indices[] = {
         0,  1,  2,    2,  3,  0,    4,  5,  6,    6,  7,  4,
         8,  9, 10,   10, 11,  8,   12, 13, 14,   14, 15, 12,
        16, 17, 18,   18, 19, 16,   20, 21, 22,   22, 23, 20
    };

    // ========== 6. VAO / VBO / EBO ==========
    unsigned int cubeVAO, lightVAO, VBO, EBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenVertexArrays(1, &lightVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ========== 7. 加载纹理 ==========
    unsigned int diffuseMap  = loadTexture("textures/container2.png");
    unsigned int specularMap = loadTexture("textures/container2_specular.png");

    // ========== 8. 设置纹理单元 ==========
    lightingShader.use();
    lightingShader.setInt("material.diffuse",  0);
    lightingShader.setInt("material.specular", 1);

    // ========== 9. 立方体位置 ==========
    glm::vec3 cubePositions[] = {
        glm::vec3( 0.0f,  0.0f,  0.0f),
        glm::vec3( 2.0f,  0.5f,  0.0f),
        glm::vec3(-2.0f,  0.5f,  0.0f),
        glm::vec3( 0.0f,  2.0f,  0.0f),
        glm::vec3( 0.0f, -2.0f,  0.0f),
        glm::vec3( 3.0f,  1.5f, -2.0f),
        glm::vec3(-3.0f,  1.5f, -2.0f),
        glm::vec3( 1.5f, -1.5f, -3.0f),
        glm::vec3(-1.5f, -1.5f, -3.0f),
        glm::vec3( 0.0f,  0.0f, -4.0f)
    };
    const int NUM_CUBES = sizeof(cubePositions) / sizeof(cubePositions[0]);

    // ========== 10. 光源参数 ==========

    // ---- 方向光（太阳） ----
    // 从右上斜照，全场景均匀
    glm::vec3 dirLightDirection    = glm::vec3(-0.3f, -0.5f, -0.5f);
    float dirLightDirArray[3]      = { -0.3f, -0.5f, -0.5f };
    float dirLightAmbient[3]       = { 0.08f, 0.08f, 0.08f };
    float dirLightDiffuse[3]       = { 0.5f,  0.5f,  0.5f };
    float dirLightSpecular[3]      = { 0.6f,  0.6f,  0.6f };
    bool  dirLightEnabled          = true;

    // ---- 点光源 * 4（不同颜色、不同位置） ----
    struct PointLightData {
        glm::vec3 position;
        glm::vec3 color;
        float constant, linear, quadratic;
        bool enabled;
    };

    const int NUM_POINT_LIGHTS = 4;
    PointLightData pointLights[NUM_POINT_LIGHTS] = {
        { { 0.7f,  0.2f,  2.0f}, {1.0f, 0.6f, 0.0f}, 1.0f, 0.7f,  1.8f,  true },  // 橙
        { { 2.3f, -2.3f, -4.0f}, {1.0f, 0.0f, 0.0f}, 1.0f, 0.35f, 0.44f, true },  // 红
        { {-3.0f,  2.0f, -3.0f}, {0.0f, 0.0f, 1.0f}, 1.0f, 0.22f, 0.20f, true },  // 蓝
        { { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, 1.0f, 0.14f, 0.07f, true },  // 白（底光）
    };

    // 同步到 float 数组供 ImGui 编辑
    float plPos[4][3];
    float plColor[4][3];
    for (int i = 0; i < NUM_POINT_LIGHTS; i++) {
        plPos[i][0] = pointLights[i].position.x;
        plPos[i][1] = pointLights[i].position.y;
        plPos[i][2] = pointLights[i].position.z;
        plColor[i][0] = pointLights[i].color.r;
        plColor[i][1] = pointLights[i].color.g;
        plColor[i][2] = pointLights[i].color.b;
    }

    // ---- 聚光灯（手电筒） ----
    float spotCutOffDeg      = 12.5f;
    float spotOuterCutOffDeg = 17.5f;
    float spotAmbient[3]     = { 0.0f, 0.0f, 0.0f };
    float spotDiffuse[3]     = { 1.0f, 1.0f, 1.0f };
    float spotSpecular[3]    = { 1.0f, 1.0f, 1.0f };
    bool  spotEnabled        = true;

    // ---- 材质 ----
    float shininess = 32.0f;

    // ---- 摄像机 ----
    glm::vec3 camPos    = glm::vec3(0.0f, 0.0f, 6.0f);
    float     fov       = 45.0f;
    float     camMoveSpeed = 0.10f;

    // ---- 调试 ----
    bool showDebugPanel = true;
    float clearColor[3] = { 0.1f, 0.1f, 0.1f };

    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 11. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  多光源（Multiple Lights）" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  场景: 1 方向光 + 4 点光源 + 1 聚光灯" << std::endl;
    std::cout << "  WASD: 移动（聚光灯跟随摄像机）" << std::endl;
    std::cout << "  F: 切换聚光灯开关" << std::endl;
    std::cout << "  Tab: 面板  |  ESC: 退出" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 12. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 12a. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        } else { tabPressed = false; }

        // F 键切换聚光灯
        static bool fPressed = false;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!fPressed) { spotEnabled = !spotEnabled; fPressed = true;
                std::cout << (spotEnabled ? "◆ 聚光灯开启" : "◆ 聚光灯关闭") << std::endl; }
        } else { fPressed = false; }

        // WASD 移动
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camPos += glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camPos -= glm::vec3(0.0f, camMoveSpeed, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camPos -= glm::vec3(camMoveSpeed, 0.0f, 0.0f);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camPos += glm::vec3(camMoveSpeed, 0.0f, 0.0f);

        // ===== 12b. 清空缓冲 =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== 12c. ImGui：开始新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 12d. MVP 矩阵 =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // ================================================================
        // 第一部分：渲染主物体（多光源计算）
        // ================================================================

        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setVec3("viewPos", camPos.x, camPos.y, camPos.z);

        // ---- 方向光 ----
        if (dirLightEnabled) {
            glm::vec3 dir = glm::normalize(glm::vec3(dirLightDirArray[0], dirLightDirArray[1], dirLightDirArray[2]));
            lightingShader.setVec3("dirLight.direction", dir.x, dir.y, dir.z);
            lightingShader.setVec3("dirLight.ambient",  dirLightAmbient[0],  dirLightAmbient[1],  dirLightAmbient[2]);
            lightingShader.setVec3("dirLight.diffuse",  dirLightDiffuse[0],  dirLightDiffuse[1],  dirLightDiffuse[2]);
            lightingShader.setVec3("dirLight.specular", dirLightSpecular[0], dirLightSpecular[1], dirLightSpecular[2]);
        } else {
            lightingShader.setVec3("dirLight.ambient",  0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("dirLight.diffuse",  0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("dirLight.specular", 0.0f, 0.0f, 0.0f);
        }

        // ---- 点光源 ----
        for (int i = 0; i < NUM_POINT_LIGHTS; i++)
        {
            std::string idx = "pointLights[" + std::to_string(i) + "]";

            if (pointLights[i].enabled) {
                lightingShader.setVec3(idx + ".position", pointLights[i].position.x, pointLights[i].position.y, pointLights[i].position.z);
                glm::vec3 amb = pointLights[i].color * 0.2f;
                glm::vec3 dif = pointLights[i].color * 0.8f;
                glm::vec3 spe = pointLights[i].color;
                lightingShader.setVec3(idx + ".ambient",   amb.x, amb.y, amb.z);
                lightingShader.setVec3(idx + ".diffuse",   dif.x, dif.y, dif.z);
                lightingShader.setVec3(idx + ".specular",  spe.x, spe.y, spe.z);
                lightingShader.setFloat(idx + ".constant",  pointLights[i].constant);
                lightingShader.setFloat(idx + ".linear",    pointLights[i].linear);
                lightingShader.setFloat(idx + ".quadratic", pointLights[i].quadratic);
            } else {
                lightingShader.setVec3(idx + ".ambient",  0.0f, 0.0f, 0.0f);
                lightingShader.setVec3(idx + ".diffuse",  0.0f, 0.0f, 0.0f);
                lightingShader.setVec3(idx + ".specular", 0.0f, 0.0f, 0.0f);
            }
        }

        // ---- 聚光灯（手电筒：跟随摄像机位置，指向原点） ----
        if (spotEnabled) {
            glm::vec3 spotPos = camPos;
            glm::vec3 spotDir = glm::normalize(glm::vec3(0.0f) - camPos);
            lightingShader.setVec3("spotLight.position",    spotPos.x, spotPos.y, spotPos.z);
            lightingShader.setVec3("spotLight.direction",   spotDir.x, spotDir.y, spotDir.z);
            lightingShader.setFloat("spotLight.cutOff",      cos(glm::radians(spotCutOffDeg)));
            lightingShader.setFloat("spotLight.outerCutOff", cos(glm::radians(spotOuterCutOffDeg)));
            lightingShader.setVec3("spotLight.ambient",  0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("spotLight.diffuse",  spotDiffuse[0],  spotDiffuse[1],  spotDiffuse[2]);
            lightingShader.setVec3("spotLight.specular", spotSpecular[0], spotSpecular[1], spotSpecular[2]);
            lightingShader.setFloat("spotLight.constant",  1.0f);
            lightingShader.setFloat("spotLight.linear",    0.09f);
            lightingShader.setFloat("spotLight.quadratic", 0.032f);
        } else {
            lightingShader.setVec3("spotLight.ambient",  0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("spotLight.diffuse",  0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("spotLight.specular", 0.0f, 0.0f, 0.0f);
        }

        // ---- 材质 ----
        lightingShader.setFloat("material.shininess", shininess);

        // ---- 绑定纹理 ----
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularMap);

        // ---- 绘制所有立方体 ----
        glBindVertexArray(cubeVAO);
        for (int i = 0; i < NUM_CUBES; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            float angle = 20.0f * (i + 1);
            model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
            lightingShader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // ================================================================
        // 第二部分：渲染点光源标记
        // ================================================================

        glBindVertexArray(lightVAO);
        for (int i = 0; i < NUM_POINT_LIGHTS; i++)
        {
            if (!pointLights[i].enabled) continue;

            lightCubeShader.use();
            lightCubeShader.setMat4("projection", projection);
            lightCubeShader.setMat4("view", view);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, pointLights[i].position);
            model = glm::scale(model, glm::vec3(0.15f));
            lightCubeShader.setMat4("model", model);
            lightCubeShader.setVec3("lightColor", pointLights[i].color.r, pointLights[i].color.g, pointLights[i].color.b);

            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // ===== 12e. ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Multiple Lights");

            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 场景光源概览 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Scene Lights");
            ImGui::Text("1 Directional + %d Point + 1 Spotlight", NUM_POINT_LIGHTS);
            ImGui::Separator();

            // ---- 方向光 ----
            ImGui::Checkbox("Directional Light (Sun)", &dirLightEnabled);
            if (dirLightEnabled) {
                ImGui::SliderFloat3("Sun Dir", dirLightDirArray, -1.0f, 1.0f, "%.2f");
                ImGui::ColorEdit3("Ambient",  dirLightAmbient);
                ImGui::ColorEdit3("Diffuse",  dirLightDiffuse);
                ImGui::ColorEdit3("Specular", dirLightSpecular);
            }
            ImGui::Separator();

            // ---- 点光源 ----
            for (int i = 0; i < NUM_POINT_LIGHTS; i++)
            {
                ImGui::PushID(i);
                ImGui::TextColored(ImVec4(plColor[i][0], plColor[i][1], plColor[i][2], 1.0f),
                    "Point Light %d", i);
                ImGui::Checkbox("Enable", &pointLights[i].enabled); ImGui::SameLine();
                if (ImGui::ColorEdit3("Color", plColor[i], ImGuiColorEditFlags_NoInputs)) {
                    pointLights[i].color = glm::vec3(plColor[i][0], plColor[i][1], plColor[i][2]);
                }
                if (pointLights[i].enabled) {
                    ImGui::SliderFloat3("Pos", plPos[i], -5.0f, 5.0f, "%.1f");
                    pointLights[i].position = glm::vec3(plPos[i][0], plPos[i][1], plPos[i][2]);
                }
                ImGui::PopID();
            }
            ImGui::Separator();

            // ---- 聚光灯 ----
            ImGui::Checkbox("Spotlight (Flashlight)", &spotEnabled);
            if (spotEnabled) {
                ImGui::Text("Position: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
                ImGui::SliderFloat("Inner", &spotCutOffDeg, 1.0f, 50.0f, "%.1f°");
                ImGui::SliderFloat("Outer", &spotOuterCutOffDeg, 1.0f, 50.0f, "%.1f°");
                ImGui::ColorEdit3("Diffuse",  spotDiffuse);
                ImGui::ColorEdit3("Specular", spotSpecular);
            }
            ImGui::Separator();

            // ---- 材质 & 选项 ----
            ImGui::SliderFloat("Shininess", &shininess, 1.0f, 256.0f, "%.0f");
            ImGui::SliderFloat("Speed", &camMoveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::ColorEdit3("Clear", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            ImGui::TextDisabled("WASD: Move  |  F: Flashlight  |  Tab: Panel  |  ESC: Quit");

            ImGui::End();
        }

        // ===== 12f. ImGui：渲染 + 交换缓冲 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 13. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &diffuseMap);
    glDeleteTextures(1, &specularMap);
    glfwTerminate();
    return 0;
}


// ================================================================
// 模型加载（Model Loading）— Assimp
// ================================================================
/**
 * 运行模型加载演示：
 *   加载 backpack.obj，用全光照（方向光 + 点光源 + 手电筒）渲染，
 *   并带 ImGui 调试面板控制光源参数。
 *
 * 章节：模型加载 — Assimp
 */
int runModelLoadingDemo()
{
    // ========== 1. 窗口初始化 ==========
    GLFWwindow* window = initGLFW(
        "LearnOpenGL - Model Loading (Assimp) | WASD: move  | Tab: panel"
    );
    if (!window) return -1;

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // ========== 2. GLAD & Depth ==========
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "⨯ Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glEnable(GL_DEPTH_TEST);

    // ========== 3. 着色器 ==========
    Shader modelShader(
        "shaders/model/model.vert",
        "shaders/model/model.frag",
        true   // fromFile
    );

    // ========== 4. 加载模型 ==========
    Model backpack("models/backpack/backpack.obj");

    // ========== 5. ImGui 初始化 ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.FontGlobalScale = 1.8f;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 6. 状态变量 ==========
    // ---- 摄像机 ----
    glm::vec3 camPos     = glm::vec3(0.0f, 1.5f, 5.0f);
    float     camYaw     = -90.0f;
    float     camPitch   = 0.0f;
    float     camMoveSpeed = 0.08f;
    float     camSensitivity = 0.1f;
    double    lastMX = 400, lastMY = 300;
    bool      firstMouse = true;

    // ---- 模型变换 ----
    float modelRotX = 0.0f, modelRotY = 0.0f;
    float modelScale = 1.0f;

    // ---- 投影 ----
    float fov = 60.0f;

    // ---- 光源开关 ----
    bool dirLightOn    = true;
    bool pointLightOn  = true;
    bool spotLightOn   = false;

    // ---- 光源参数（方向光） ----
    glm::vec3 dirLightDir(-0.2f, -1.0f, -0.3f);
    glm::vec3 dirAmbient(0.2f, 0.2f, 0.2f);
    glm::vec3 dirDiffuse(0.8f, 0.8f, 0.8f);
    glm::vec3 dirSpecular(1.0f, 1.0f, 1.0f);

    // ---- 光源参数（点光源） ----
    struct {
        glm::vec3 pos;
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
        float     constant, linear, quadratic;
    } pointLights[4] = {
        { glm::vec3( 2.0f,  2.0f,  2.0f), glm::vec3(0.1f), glm::vec3(0.8f), glm::vec3(1.0f), 1.0f, 0.09f, 0.032f },
        { glm::vec3(-2.0f,  1.5f,  1.5f), glm::vec3(0.1f), glm::vec3(0.8f), glm::vec3(1.0f), 1.0f, 0.09f, 0.032f },
        { glm::vec3( 1.5f,  0.5f, -2.0f), glm::vec3(0.1f), glm::vec3(0.8f), glm::vec3(1.0f), 1.0f, 0.09f, 0.032f },
        { glm::vec3(-1.5f,  1.0f, -1.5f), glm::vec3(0.1f), glm::vec3(0.8f), glm::vec3(1.0f), 1.0f, 0.09f, 0.032f },
    };
    int activePointLights = 4;

    // ---- 手电筒 ----
    float spotCutOff      = glm::cos(glm::radians(12.5f));
    float spotOuterCutOff = glm::cos(glm::radians(17.5f));

    // ---- 背景色 ----
    float clearColor[3] = { 0.1f, 0.1f, 0.1f };

    // ---- 模型光源控制 ----
    bool showLightSpheres = true;
    bool showDebugPanel = true;

    // ========== 7. 光源球体的 VAO（可视化点光源位置） ==========
    unsigned int sphereVAO = 0, sphereVBO = 0, sphereEBO = 0;
    // 简单的光源球体（使用经纬球体简化为 icosahedron 层级的小球）
    // 这里仅用于可视化，用一个简单的立方体代替
    {
        float vertices[] = {
            -0.15f, -0.15f, -0.15f,  0.15f, -0.15f, -0.15f,  0.15f,  0.15f, -0.15f,
             0.15f,  0.15f, -0.15f, -0.15f,  0.15f, -0.15f, -0.15f, -0.15f, -0.15f,
            -0.15f, -0.15f,  0.15f,  0.15f, -0.15f,  0.15f,  0.15f,  0.15f,  0.15f,
             0.15f,  0.15f,  0.15f, -0.15f,  0.15f,  0.15f, -0.15f, -0.15f,  0.15f,
            -0.15f,  0.15f,  0.15f, -0.15f,  0.15f, -0.15f, -0.15f, -0.15f, -0.15f,
            -0.15f, -0.15f, -0.15f, -0.15f, -0.15f,  0.15f, -0.15f,  0.15f,  0.15f,
             0.15f,  0.15f,  0.15f,  0.15f,  0.15f, -0.15f,  0.15f, -0.15f, -0.15f,
             0.15f, -0.15f, -0.15f,  0.15f, -0.15f,  0.15f,  0.15f,  0.15f,  0.15f,
            -0.15f, -0.15f, -0.15f,  0.15f, -0.15f, -0.15f,  0.15f, -0.15f,  0.15f,
             0.15f, -0.15f,  0.15f, -0.15f, -0.15f,  0.15f, -0.15f, -0.15f, -0.15f,
            -0.15f,  0.15f, -0.15f,  0.15f,  0.15f, -0.15f,  0.15f,  0.15f,  0.15f,
             0.15f,  0.15f,  0.15f, -0.15f,  0.15f,  0.15f, -0.15f,  0.15f, -0.15f,
        };
        unsigned int indices[] = {
            0,1,2, 0,2,3, 4,5,6, 4,6,7,
            8,9,10, 8,10,11, 12,13,14, 12,14,15,
            16,17,18, 16,18,19, 20,21,22, 20,22,23
        };
        glGenVertexArrays(1, &sphereVAO);
        glGenBuffers(1, &sphereVBO);
        glGenBuffers(1, &sphereEBO);
        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    // 用于渲染光源位置的小型着色器
    Shader lightShader(
        "shaders/lighting/light_cube.vert",
        "shaders/lighting/light_cube.frag",
        true
    );

    // ========== 8. 主循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ---- deltaTime（简化：固定 timestep ~16ms） ----
        float deltaTime = 0.016f;

        // ---- 输入 ----
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += camMoveSpeed * glm::vec3(0, 0, -1);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos += camMoveSpeed * glm::vec3(0, 0,  1);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos += camMoveSpeed * glm::vec3(-1, 0, 0);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += camMoveSpeed * glm::vec3( 1, 0, 0);
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) spotLightOn = !spotLightOn;

        // ---- 鼠标控制 ----
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        if (firstMouse) { lastMX = mx; lastMY = my; firstMouse = false; }
        double dx = mx - lastMX, dy = lastMY - my;
        lastMX = mx; lastMY = my;
        // Tab：切换 ImGui 调试面板
        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        } else { tabPressed = false; }

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            camYaw   += (float)dx * camSensitivity;
            camPitch += (float)dy * camSensitivity;
            camPitch  = glm::clamp(camPitch, -89.0f, 89.0f);
        }
        glm::vec3 front;
        front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front.y = sin(glm::radians(camPitch));
        front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front = glm::normalize(front);

        // ---- 矩阵 ----
        glm::mat4 view       = glm::lookAt(camPos, camPos + front, glm::vec3(0, 1, 0));
        glm::mat4 projection = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 0.1f, 100.0f);

        // ---- 模型矩阵 ----
        glm::mat4 modelMat = glm::mat4(1.0f);
        modelMat = glm::rotate(modelMat, glm::radians(modelRotX), glm::vec3(1, 0, 0));
        modelMat = glm::rotate(modelMat, glm::radians(modelRotY), glm::vec3(0, 1, 0));
        modelMat = glm::scale(modelMat, glm::vec3(modelScale));

        // ===== 9a. 清屏 =====
        glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== 9b. 渲染模型 =====
        modelShader.use();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        modelShader.setMat4("model", modelMat);

        // ---- 方向光 ----
        modelShader.setBool("dirLightEnabled", dirLightOn);
        modelShader.setVec3("dirLight.direction", dirLightDir);
        modelShader.setVec3("dirLight.ambient",  dirAmbient);
        modelShader.setVec3("dirLight.diffuse",  dirDiffuse);
        modelShader.setVec3("dirLight.specular", dirSpecular);

        // ---- 点光源 ----
        modelShader.setBool("pointLightsEnabled", pointLightOn);
        for (int i = 0; i < 4; i++) {
            std::string prefix = "pointLights[" + std::to_string(i) + "].";
            modelShader.setVec3(prefix + "position",  pointLights[i].pos);
            modelShader.setVec3(prefix + "ambient",   pointLights[i].ambient);
            modelShader.setVec3(prefix + "diffuse",   pointLights[i].diffuse);
            modelShader.setVec3(prefix + "specular",  pointLights[i].specular);
            modelShader.setFloat(prefix + "constant",  pointLights[i].constant);
            modelShader.setFloat(prefix + "linear",    pointLights[i].linear);
            modelShader.setFloat(prefix + "quadratic", pointLights[i].quadratic);
        }

        // ---- 聚光灯（手电筒） ----
        modelShader.setBool("spotLightEnabled", spotLightOn);
        modelShader.setVec3("spotLight.position", camPos);
        modelShader.setVec3("spotLight.direction", front);
        modelShader.setFloat("spotLight.cutOff",      spotCutOff);
        modelShader.setFloat("spotLight.outerCutOff", spotOuterCutOff);
        modelShader.setVec3("spotLight.ambient",  0.0f, 0.0f, 0.0f);
        modelShader.setVec3("spotLight.diffuse",  1.0f, 1.0f, 1.0f);
        modelShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        modelShader.setFloat("spotLight.constant",  1.0f);
        modelShader.setFloat("spotLight.linear",    0.09f);
        modelShader.setFloat("spotLight.quadratic", 0.032f);

        modelShader.setVec3("viewPos", camPos);

        // Draw the model
        backpack.Draw(modelShader);

        // ===== 9c. 渲染光源位置（小球体） =====
        if (showLightSpheres && pointLightOn) {
            lightShader.use();
            lightShader.setMat4("projection", projection);
            lightShader.setMat4("view", view);
            for (int i = 0; i < activePointLights; i++) {
                glm::mat4 lightModel = glm::translate(glm::mat4(1.0f), pointLights[i].pos);
                lightShader.setMat4("model", lightModel);
                lightShader.setVec3("lightColor", pointLights[i].diffuse);
                glBindVertexArray(sphereVAO);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }
        }

        // ===== 10. ImGui 面板 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (showDebugPanel)
        {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(360, 520), ImGuiCond_Once);
            ImGui::Begin("Debug Panel - Model Loading");

            ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "★ Model Loading (Assimp)");
            ImGui::Separator();

            // ---- 模型变换 ----
            ImGui::Text("Model Transform");
            ImGui::SliderFloat("Rotate X", &modelRotX, -180.0f, 180.0f);
            ImGui::SliderFloat("Rotate Y", &modelRotY, -180.0f, 180.0f);
            ImGui::SliderFloat("Scale", &modelScale, 0.1f, 3.0f);
            ImGui::Separator();

            // ---- 光源控制 ----
            ImGui::Checkbox("Directional Light", &dirLightOn);
            ImGui::Checkbox("Point Lights", &pointLightOn);
            ImGui::Checkbox("Flashlight (F)", &spotLightOn);
            ImGui::Checkbox("Show Light Spheres", &showLightSpheres);
            ImGui::Separator();

            // ---- 方向光颜色 ----
            ImGui::ColorEdit3("Dir Ambient",  (float*)&dirAmbient);
            ImGui::ColorEdit3("Dir Diffuse",  (float*)&dirDiffuse);
            ImGui::ColorEdit3("Dir Specular", (float*)&dirSpecular);
            ImGui::Separator();

            // ---- 点光源 ----
            if (pointLightOn) {
                for (int i = 0; i < activePointLights; i++) {
                    ImGui::PushID(i);
                    ImGui::Text("Point Light %d", i);
                    ImGui::DragFloat3("Pos", (float*)&pointLights[i].pos, 0.1f);
                    ImGui::ColorEdit3("Ambient", (float*)&pointLights[i].ambient);
                    ImGui::ColorEdit3("Diffuse", (float*)&pointLights[i].diffuse);
                    ImGui::PopID();
                }
                ImGui::Separator();
            }

            // ---- 摄像机 ----
            ImGui::SliderFloat("Speed", &camMoveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f");
            ImGui::ColorEdit3("Clear", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            ImGui::TextDisabled("WASD: Move  |  RClick+Drag: Look  |  F: Flashlight  |  Tab: Panel  |  ESC: Quit");
            ImGui::End();
        }

        // ===== 11. ImGui 渲染 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 12. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &sphereEBO);
    glfwTerminate();
    return 0;
}


// ================================================================
// 深度测试（Depth Testing）
// ================================================================
//
// 深度测试是「高级 OpenGL」部分的第一个章节。
// 它在片段着色器执行之后、像素写入帧缓冲之前，
// 通过比较片段的深度值与深度缓冲区中存储的值来决定片段是否可见。
//
// 核心知识点：
//   ① 深度缓冲（Depth Buffer / Z-Buffer）自动存储每个像素的深度值
//   ② glEnable(GL_DEPTH_TEST) 启用深度测试
//   ③ glDepthFunc() 设置比较函数（GL_LESS, GL_LEQUAL, GL_ALWAYS, ...）
//   ④ 深度值是非线性的（近平面附近精度高，远平面附近精度低）
//   ⑤ 深度冲突（Z-fighting）：两个面非常接近时交替写入，产生闪烁
//
// 本 Demo：
//   - 一个木纹地板 + 两个带表情贴图的立方体（不同深度）
//   - D 键 | ImGui 复选框 → 切换深度测试启用/禁用
//   - F 键 | ImGui 下拉框 → 切换深度比较函数
//   - V 键 → 切换深度可视化模式（显示深度缓冲区为灰度图）
//   - ImGui 面板查看当前深度函数、切换参数
//

int runDepthTestingDemo()
{
    const unsigned int SCR_WIDTH  = 800;
    const unsigned int SCR_HEIGHT = 600;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Depth Testing | Z:toggle | X:func | V:visualize | RClick:look",
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

    // ========== 3. 深度测试基础设置 ==========
    //
    // ★ 启用深度测试后，OpenGL 会为每个片段计算深度值，
    //   并与深度缓冲区中已有的值比较。默认函数是 GL_LESS：
    //   新片段深度 < 已有深度 → 通过（离摄像机更近）
    //
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);   // 默认值，显式写出以明确

    // ★ 明确关闭混合和面剔除，防止 ImGui 或其他遗留状态影响
    //   awesomeface.png 有透明通道，如果 GL_BLEND 意外打开，
    //   立方体正面的透明区域会变透明 → 露出背面
    glDisable(GL_BLEND);
    // ★ 启用面剔除：剔除背向三角形（默认 GL_BACK，即剔除 CW 绕序）
    //   - 如果立方体绕序正确（CCW 从外部看）→ 外表面正常显示
    //   - 如果立方体绕序错了（CW 从外部看）→ 外表面被剔除，立方体不可见
    //   - 这也解释了为什么关掉深度测试后 "背面可见，正面看不到"：
    //     因为所有面都渲染了，背面（后绘制）覆盖了正面
    glEnable(GL_CULL_FACE);

    // ========== 4. 鼠标回调（FPS 视角） ==========
    static float camYaw   = -90.0f;
    static float camPitch = 0.0f;
    static bool  firstMouse = true;
    static double lastMX = 400.0, lastMY = 300.0;

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
        // 仅在右键按下时旋转视角
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
            firstMouse = true;
            return;
        }
        if (firstMouse) {
            lastMX = xpos;
            lastMY = ypos;
            firstMouse = false;
        }
        double dx = xpos - lastMX;
        double dy = lastMY - ypos;
        lastMX = xpos;
        lastMY = ypos;
        camYaw   += (float)dx * 0.1f;
        camPitch += (float)dy * 0.1f;
        camPitch = glm::clamp(camPitch, -89.0f, 89.0f);
    });

    // ========== 5. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 6. 编译着色器 ==========
    //
    // 普通着色器：用于常规渲染（带纹理）
    //   顶点：坐标系统（MVP + 纹理坐标输出）
    //   片段：合并两张纹理
    //
    Shader shader("shaders/coordinates/coordinate_system.vert",
                  "shaders/textures/texture_combined.frag", true);

    // 深度可视化着色器：将深度值渲染为灰度图
    //   顶点：复用坐标系统的 MVP 变换
    //   片段：输出 gl_FragCoord.z 映射到灰度
    //
    Shader depthVisShader("shaders/coordinates/coordinate_system.vert",
                          "shaders/depth_testing/depth_visualize.frag", true);

    // ========== 7. 地板平面数据 ==========
    //
    // 一个大四边形（2 个三角形）作为地面，y = -0.51（略低于立方体底面）
    // 格式：[位置 xyz] [纹理坐标 uv]  → 每个顶点 5 float
    //
    float planeVertices[] = {
        // ★ 反转三角形绕序：从上方看应为 CCW（法线朝 +Y），
        //   否则被 glEnable(GL_CULL_FACE) 剔除。
        // ---- 位置 ----------    -- uv ---
        -5.0f, -0.51f, -5.0f,     0.0f, 2.0f,   // 0: 左下后
         5.0f, -0.51f,  5.0f,     2.0f, 0.0f,   // 1: 右下前 (原为右下后，交换顺序使法线朝上)
         5.0f, -0.51f, -5.0f,     2.0f, 2.0f,   // 2: 右下后
         5.0f, -0.51f,  5.0f,     2.0f, 0.0f,   // 3: 右下前
        -5.0f, -0.51f, -5.0f,     0.0f, 2.0f,   // 4: 左下后 (原为左下前，交换顺序使法线朝上)
        -5.0f, -0.51f,  5.0f,     0.0f, 0.0f    // 5: 左下前
    };

    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);

    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    // 位置属性 (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // 纹理坐标属性 (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 8. 立方体数据（带纹理坐标） ==========
    //
    // 复用坐标系统章节的顶点格式：
    //   5 floats/vertex = [位置 xyz] [纹理坐标 uv]
    //
    float cubeVertices[] = {
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

    unsigned int cubeIndices[] = {
        // ★ 用叉积验证过的正确绕序（CCW 从外部看）
        //   已修正：背面、右面、顶面的三角形顺序是反的
        //   修正方法：在 CCW → CW 的面中交换后两个顶点
         0,  2,  1,    2,  0,  3,   // 背面 (原 0,1,2  2,3,0 → 法线为 +Z 指向内, 需反转)
         4,  5,  6,    6,  7,  4,   // 正面 (✓ 法线 +Z 指向外)
         8,  9, 10,   10, 11,  8,   // 左面 (✓ 法线 -X 指向外)
        12, 14, 13,   14, 12, 15,   // 右面 (原 12,13,14 14,15,12 → 法线 -X 指向内)
        16, 17, 18,   18, 19, 16,   // 底面 (✓ 法线 -Y 指向外)
        20, 22, 21,   22, 20, 23    // 顶面 (原 20,21,22 22,23,20 → 法线 -Y 指向内)
    };

    unsigned int cubeVAO, cubeVBO, cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 9. 加载纹理 ==========
    //
    // ★ 跟教程一致：
    //   - 地板 → floor.jpg（金属板纹理，LearnOpenGL 官方资源）
    //   - 立方体 → container.jpg（主纹理）+ awesomeface.png（混合纹理）
    //
    unsigned int floorTex      = loadTexture("textures/floor.jpg", true);
    unsigned int containerTex  = loadTexture("textures/container.jpg");
    unsigned int faceTex       = loadTexture("textures/awesomeface.png");

    // ========== 10. 设置纹理单元 ==========
    shader.use();
    shader.setInt("texture1", 0);
    shader.setInt("texture2", 1);

    // ========== 11. 立方体位置 ==========
    // 两个立方体位于地板上方，前后重叠排列。
    // ★ cube.y = 0.01 避免立方体底面和地板 (y = -0.51) Z-fighting
    // ★ 两个立方体在视线方向上有重叠，这样开关深度测试才能看到明显区别
    struct { glm::vec3 pos; float rotAngle; } cubes[] = {
        { glm::vec3(-0.5f, 0.01f, -2.5f), 30.0f },   // 近（偏左）
        { glm::vec3( 0.5f, 0.01f, -3.5f), 45.0f },   // 远（偏右，与第一个在视线方向重叠）
    };
    const int NUM_CUBES = 2;

    // ========== 12. 深度函数定义 ==========
    struct DepthFuncDef {
        GLenum func;
        const char* name;
        const char* desc;
    };
    const DepthFuncDef depthFuncs[] = {
        { GL_LESS,     "GL_LESS",     "片段深度 < 缓冲深度 → 通过（默认）" },
        { GL_LEQUAL,   "GL_LEQUAL",   "片段深度 ≤ 缓冲深度 → 通过" },
        { GL_EQUAL,    "GL_EQUAL",    "片段深度 = 缓冲深度 → 通过" },
        { GL_NOTEQUAL, "GL_NOTEQUAL", "片段深度 ≠ 缓冲深度 → 通过" },
        { GL_GREATER,  "GL_GREATER",  "片段深度 > 缓冲深度 → 通过" },
        { GL_GEQUAL,   "GL_GEQUAL",   "片段深度 ≥ 缓冲深度 → 通过" },
        { GL_ALWAYS,   "GL_ALWAYS",   "始终通过（无深度测试效果）" },
        { GL_NEVER,    "GL_NEVER",    "永不通过（什么都不显示）" },
    };
    const int NUM_DEPTH_FUNCS = sizeof(depthFuncs) / sizeof(depthFuncs[0]);
    int currentDepthFunc = 0;  // 默认 GL_LESS

    // ========== 13. 用户控制参数 ==========

    // ---- 深度测试 ----
    bool depthTestEnabled  = true;
    bool visualizeDepth    = false;   // 是否显示深度灰度图
    bool linearizeDepth    = true;    // 深度可视化时是否线性化

    // ---- 摄像机 ----
    glm::vec3 camPos     = glm::vec3(0.0f, 2.0f, 5.0f);
    float     fov        = 45.0f;
    float     moveSpeed  = 0.08f;

    // ---- Z-fighting 演示 ----
    bool zfightingDemo    = false;
    float zfightingOffset = 0.005f;   // 两个重叠面之间的间距

    // ---- 调试 ----
    bool showDebugPanel = true;
    float clearColor[3] = { 0.1f, 0.1f, 0.1f };

    // ========== 14. 清屏颜色 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 15. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  深度测试（Depth Testing）" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  ESC           → 退出" << std::endl;
    std::cout << "  WASD / 箭头   → 前后左右移动" << std::endl;
    std::cout << "  右键拖动       → 旋转视角" << std::endl;
    std::cout << "  Z             → 切换深度测试 开/关（← 用 Z 避免和 WASD 中 D 冲突）" << std::endl;
    std::cout << "  X             → 切换深度函数" << std::endl;
    std::cout << "  V             → 切换深度可视化" << std::endl;
    std::cout << "  Tab           → 显示/隐藏调试面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 16. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 16a. 计算 deltaTime =====
        static float lastFrame = 0.0f;
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // ===== 16b. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // ---- Z 键：切换深度测试（★ 用 Z 而不是 D，避免和摄像机右移冲突！） -----
        static bool zPressed = false;
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        {
            if (!zPressed)
            {
                depthTestEnabled = !depthTestEnabled;
                if (depthTestEnabled) {
                    glEnable(GL_DEPTH_TEST);
                    std::cout << "📏 深度测试: 启用" << std::endl;
                } else {
                    glDisable(GL_DEPTH_TEST);
                    std::cout << "📏 深度测试: 禁用" << std::endl;
                }
                zPressed = true;
            }
        }
        else { zPressed = false; }

        // ---- X 键：切换深度函数 ----
        static bool xPressed = false;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
        {
            if (!xPressed)
            {
                currentDepthFunc = (currentDepthFunc + 1) % NUM_DEPTH_FUNCS;
                glDepthFunc(depthFuncs[currentDepthFunc].func);
                std::cout << "📏 深度函数: " << depthFuncs[currentDepthFunc].name
                          << " — " << depthFuncs[currentDepthFunc].desc << std::endl;
                xPressed = true;
            }
        }
        else { xPressed = false; }

        // ---- V 键：切换深度可视化 ----
        static bool vPressed = false;
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)
        {
            if (!vPressed) {
                visualizeDepth = !visualizeDepth;
                std::cout << (visualizeDepth ? "📊 深度可视化: 开启" : "📊 深度可视化: 关闭") << std::endl;
                vPressed = true;
            }
        }
        else { vPressed = false; }

        // ---- WASD / 箭头键：摄像机移动 ----
        float velocity = moveSpeed * deltaTime * 60.0f;  // 帧率无关的速度
        // 计算前方向（与摄像机视角一致）
        glm::vec3 front;
        front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front.y = sin(glm::radians(camPitch));
        front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camPos += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camPos -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camPos -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camPos += right * velocity;

        // ---- Tab 键：切换面板 ----
        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
        {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        }
        else { tabPressed = false; }

        // ===== 16c. 清空缓冲 =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== 16d. ImGui：开始新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 16e. MVP 矩阵 =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );
        glm::mat4 view = glm::lookAt(camPos, camPos + front, glm::vec3(0.0f, 1.0f, 0.0f));

        // ================================================================
        // 渲染主场景
        // ================================================================

        // 选择着色器：普通 VS 深度可视化
        Shader& activeShader = visualizeDepth ? depthVisShader : shader;
        activeShader.use();

        // ---- 深度可视化参数 ----
        if (visualizeDepth)
        {
            depthVisShader.setFloat("near", 0.1f);
            depthVisShader.setFloat("far", 100.0f);
            depthVisShader.setBool("linearize", linearizeDepth);
        }

        // ---- 设置 MVP（共享） -----
        activeShader.setMat4("projection", projection);
        activeShader.setMat4("view", view);

        // ================================================================
        // 第一部分：渲染地板
        // ================================================================

        if (!visualizeDepth)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, floorTex);
            // 地板不需要第二张纹理，但为了简单绑定一张
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, floorTex);
        }

        glm::mat4 planeModel = glm::mat4(1.0f);
        activeShader.setMat4("model", planeModel);

        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ================================================================
        // 第二部分：渲染立方体
        // ================================================================

        if (!visualizeDepth)
        {
            // 立方体统一用 container.jpg（避免 awesomeface.png 的透明通道干扰）
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, containerTex);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, containerTex);
        }

        glBindVertexArray(cubeVAO);

        for (int i = 0; i < NUM_CUBES; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubes[i].pos);
            model = glm::rotate(model, glm::radians(cubes[i].rotAngle), glm::vec3(1.0f, 0.3f, 0.5f));

            // 可选：Z-fighting 演示模式
            // 在第一个立方体前面非常近的位置多画一层
            if (zfightingDemo && i == 0)
            {
                // 绘制一个非常接近的快照平面
                // 实际上是将另一个小平面放在第一个立方体前方一点点
                // 这里我们用第二个"虚拟"绘制来模拟深度冲突
                // 通过略微偏移的 model 矩阵实现

                // 先画原始立方体
                activeShader.setMat4("model", model);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

                // 再画一个几乎相同的立方体，位置略微微调
                glm::mat4 zModel = model;
                zModel = glm::translate(zModel, glm::vec3(0.0f, 0.0f, zfightingOffset));
                activeShader.setMat4("model", zModel);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }
            else
            {
                activeShader.setMat4("model", model);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }
        }

        // ================================================================
        // 第三部分：Z-fighting 辅助可视化（在重叠处显示警告提示）
        // ================================================================
        // 通过 ImGui 面板展示警告

        // ===== 16f. ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Depth Testing");

            // ---- 性能 ----
            ImGui::Text("FPS: %.1f  (%.1f ms)", ImGui::GetIO().Framerate,
                        1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 深度测试核心控制 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Depth Test");

            // 启用/禁用
            if (ImGui::Checkbox("Enable Depth Test", &depthTestEnabled))
            {
                if (depthTestEnabled) {
                    glEnable(GL_DEPTH_TEST);
                } else {
                    glDisable(GL_DEPTH_TEST);
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled(depthTestEnabled ? "(ON)" : "(OFF)");

            // 当前深度函数状态
            if (depthTestEnabled)
            {
                ImGui::Text("Current Func: ");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s",
                                   depthFuncs[currentDepthFunc].name);
                ImGui::TextDisabled("%s", depthFuncs[currentDepthFunc].desc);

                // 深度函数选择器
                ImGui::Text("Depth Function:");
                if (ImGui::BeginCombo("##depthFunc", depthFuncs[currentDepthFunc].name))
                {
                    for (int i = 0; i < NUM_DEPTH_FUNCS; i++)
                    {
                        bool selected = (currentDepthFunc == i);
                        if (ImGui::Selectable(depthFuncs[i].name, selected))
                        {
                            currentDepthFunc = i;
                            glDepthFunc(depthFuncs[i].func);
                            std::cout << "📏 深度函数: " << depthFuncs[i].name
                                      << " — " << depthFuncs[i].desc << std::endl;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::Separator();

            // ---- 深度可视化 ----
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Visualization");
            ImGui::Checkbox("Depth Visualization (V)", &visualizeDepth);
            if (visualizeDepth)
            {
                ImGui::Checkbox("Linearize Depth", &linearizeDepth);
                ImGui::TextDisabled(
                    linearizeDepth
                    ? "显示线性深度：黑=近，白=远"
                    : "显示非线性深度：近处精度高（变化慢），远处精度低（变化快）");
            }
            ImGui::Separator();

            // ---- Z-fighting 演示 ----
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Z-Fighting Demo");
            ImGui::Checkbox("Enable Z-fighting", &zfightingDemo);
            if (zfightingDemo)
            {
                ImGui::SliderFloat("Offset", &zfightingOffset, 0.001f, 0.05f, "%.3f");
                ImGui::TextDisabled(
                    "在第一个立方体前绘制一个几乎重叠的面。\n"
                    "Offset 越小 → 重叠越严重 → Z-fighting 越明显。\n"
                    "解决方案：使用 glPolygonOffset 或增大 offset。");
            }
            ImGui::Separator();

            // ---- 摄像机控制 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Camera");
            ImGui::SliderFloat("Move Speed", &moveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::Separator();

            // ---- 清屏色 ----
            ImGui::ColorEdit3("Clear Color", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            // ---- 操作帮助 ----
            ImGui::TextDisabled(
                "WASD/Arrows: Move     |  RClick+Drag: Look\n"
                "Z: Toggle depth test  |  X: Cycle depth func\n"
                "V: Toggle visualize   |  Tab: Panel\n"
                "ESC: Quit");

            ImGui::End();
        }

        // ===== 16g. ImGui：渲染 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ===== 16h. 交换缓冲 + 事件处理 =====
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 17. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteTextures(1, &floorTex);
    glDeleteTextures(1, &containerTex);
    glDeleteTextures(1, &faceTex);
    glfwTerminate();
    return 0;
}


// ================================================================
// 第十七章：模板测试（Stencil Testing）—— 物体描边
// ================================================================

/**
 * ============================================================
 *  第十七章：模板测试（Stencil Testing）
 * ============================================================
 *
 * 模板测试是 OpenGL 高级部分的第二章（紧接深度测试）。
 *
 * ★ 模板缓冲区（Stencil Buffer）
 *   - 每个像素通常 8 位（256 种模板值）
 *   - 模板缓冲区与颜色缓冲区、深度缓冲区分辨率相同
 *   - 可以根据已绘制物体的"形状"来丢弃/保留后续的片段
 *
 * ★ 本 Demo 演示的经典应用：物体描边（Object Outlining）
 *   - 类似建模软件中选中物体的高亮边框效果
 *   - 三步走：
 *       Pass 1: 正常绘制场景（不写模板）
 *       Pass 2: 绘制物体 → 将其模板值写为 1（标记"这里有物体"）
 *       Pass 3: 放大物体 + 纯色着色器 + 只在模板值 ≠ 1 处绘制
 *               → 由于放大的物体在原物体之外是"新区域"（模板=0），
 *                  这些区域通过测试，被绘制 → 形成边框
 *
 * ★ 核心 API：
 *   - glStencilFunc(func, ref, mask)  — 设定模板测试比较函数
 *   - glStencilOp(sfail, dpfail, dppass) — 设定测试结果的动作
 *   - glStencilMask(mask)              — 控制写入模板值的位掩码
 *
 * 目标：理解模板测试的工作原理，掌握物体描边技术。
 */

int runStencilTestingDemo()
{
    const unsigned int SCR_WIDTH  = 1000;
    const unsigned int SCR_HEIGHT = 700;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // ★ 关键：请求 8 位模板缓冲区
    //   如果不设置此 Hint，GLFW 默认不分配模板缓冲区 → 模板测试失效
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Stencil Testing | Object Outlining | T:toggle O:outline C:color",
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

    // ========== 3. 基础 OpenGL 设置 ==========
    //
    // ★ 同时启用深度测试和模板测试
    //   模板测试发生在片段着色器之后、混合之前，
    //   与深度测试在同一管线阶段（两者同时起作用）。
    //
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);    // 默认深度函数

    glEnable(GL_STENCIL_TEST);
    // 模板测试默认值：
    //   glStencilFunc(GL_ALWAYS, 0, 0xFF)  — 始终通过，参考值=0，掩码=全位
    //   glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP) — 不修改模板值
    //   glStencilMask(0xFF)                 — 允许写入所有位

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    // ★ 设置清屏用的模板值（默认是 0，显式写出）
    glClearStencil(0);

    // ========== 4. 鼠标回调（FPS 视角） ==========
    static float camYaw   = -90.0f;
    static float camPitch = 0.0f;
    static bool  firstMouse = true;
    static double lastMX = 500.0, lastMY = 350.0;

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
            firstMouse = true;
            return;
        }
        if (firstMouse) {
            lastMX = xpos;
            lastMY = ypos;
            firstMouse = false;
        }
        double dx = xpos - lastMX;
        double dy = lastMY - ypos;
        lastMX = xpos;
        lastMY = ypos;
        camYaw   += (float)dx * 0.1f;
        camPitch += (float)dy * 0.1f;
        camPitch = glm::clamp(camPitch, -89.0f, 89.0f);
    });

    // ========== 5. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 6. 编译着色器 ==========
    //
    // 普通着色器：带纹理的场景渲染
    //   顶点：MVP + 纹理坐标输出
    //   片段：双纹理混合
    //
    Shader shader("shaders/coordinates/coordinate_system.vert",
                  "shaders/textures/texture_combined.frag", true);

    // 描边着色器：纯色输出，用于物体描边
    //   顶点：仅 MVP 变换（不需要纹理坐标）
    //   片段：输出纯色
    //
    Shader outlineShader("shaders/stencil_testing/stencil_single_color.vert",
                         "shaders/stencil_testing/stencil_outline.frag", true);

    // ========== 7. 地板平面数据 ==========
    //
    // 大四边形作为地面，y = -0.51（略低于立方体底面 y = -0.5）
    // 格式：[位置 xyz] [纹理坐标 uv] → 每个顶点 5 float
    //
    float planeVertices[] = {
        // ---- 位置 ----------    -- uv ---
        -5.0f, -0.51f, -5.0f,     0.0f, 3.0f,
         5.0f, -0.51f,  5.0f,     3.0f, 0.0f,
         5.0f, -0.51f, -5.0f,     3.0f, 3.0f,
         5.0f, -0.51f,  5.0f,     3.0f, 0.0f,
        -5.0f, -0.51f, -5.0f,     0.0f, 3.0f,
        -5.0f, -0.51f,  5.0f,     0.0f, 0.0f
    };

    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);

    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 8. 立方体数据（带纹理坐标） ==========
    //
    // 复用深度测试章节的顶点格式：5 floats/vertex = [位置 xyz] [纹理坐标 uv]
    // 完整立方体：6 面 × 4 顶点 = 24 顶点（非共享，为了正确的法线/UV）
    //
    float cubeVertices[] = {
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

    unsigned int cubeIndices[] = {
         0,  2,  1,    2,  0,  3,   // 背面
         4,  5,  6,    6,  7,  4,   // 正面
         8,  9, 10,   10, 11,  8,   // 左面
        12, 14, 13,   14, 12, 15,   // 右面
        16, 17, 18,   18, 19, 16,   // 底面
        20, 22, 21,   22, 20, 23    // 顶面
    };

    unsigned int cubeVAO, cubeVBO, cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 9. 加载纹理 ==========
    //
    // 地板 → floor.jpg（金属板纹理）
    // 立方体 → container.jpg（木箱纹理）
    //
    unsigned int floorTex     = loadTexture("textures/floor.jpg", true);
    unsigned int containerTex = loadTexture("textures/container.jpg");

    // ========== 10. 设置纹理单元 ==========
    shader.use();
    shader.setInt("texture1", 0);
    shader.setInt("texture2", 1);

    // ========== 11. 立方体位置 ==========
    //
    // 两个立方体：一近一远，用于展示描边效果
    // cube.y = 0.01 避免立方体底面和地板 Z-fighting
    //
    struct { glm::vec3 pos; float rotAngle; glm::vec3 rotAxis; } cubes[] = {
        { glm::vec3(-0.8f, 0.01f, -2.5f), 20.0f, glm::vec3(1.0f, 0.3f, 0.5f) },
        { glm::vec3( 0.8f, 0.01f, -3.5f), 35.0f, glm::vec3(0.2f, 1.0f, 0.4f) },
    };
    const int NUM_CUBES = 2;

    // ========== 12. 描边颜色预设 ==========
    //
    // 可循环切换的描边颜色列表
    //
    struct OutlineColor {
        glm::vec3 color;
        const char* name;
    };
    const OutlineColor outlineColors[] = {
        { glm::vec3(1.00f, 0.84f, 0.00f), "Gold (金色)"       },   // 教程推荐色
        { glm::vec3(0.04f, 0.28f, 0.26f), "Teal (青绿色)"     },   // 教程默认
        { glm::vec3(1.00f, 0.27f, 0.00f), "Orange (橙色)"     },
        { glm::vec3(0.00f, 1.00f, 0.50f), "Mint (薄荷绿)"     },
        { glm::vec3(1.00f, 0.08f, 0.58f), "Hot Pink (亮粉)"   },
        { glm::vec3(0.00f, 0.75f, 1.00f), "Sky Blue (天蓝)"   },
    };
    const int NUM_COLORS = sizeof(outlineColors) / sizeof(outlineColors[0]);
    int currentColorIdx = 0;

    // ========== 13. 用户控制参数 ==========

    // ---- 模板测试 ----
    bool  stencilTestEnabled = true;
    bool  showOutline        = true;

    // ---- 描边 ----
    float outlineScale  = 0.08f;  // 描边宽度（放大倍数，0.05 ~ 0.15 合适）

    // ---- 摄像机 ----
    glm::vec3 camPos     = glm::vec3(0.0f, 1.5f, 5.0f);
    float     fov        = 50.0f;
    float     moveSpeed  = 0.08f;

    // ---- 调试 ----
    bool  showDebugPanel = true;
    float clearColor[3]  = { 0.1f, 0.1f, 0.1f };

    // ========== 14. 清屏颜色 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 15. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  模板测试（Stencil Testing）— 物体描边" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  ESC           → 退出" << std::endl;
    std::cout << "  WASD / 箭头   → 前后左右移动" << std::endl;
    std::cout << "  右键拖动       → 旋转视角" << std::endl;
    std::cout << "  T             → 切换模板测试 开/关" << std::endl;
    std::cout << "  O             → 切换描边 开/关" << std::endl;
    std::cout << "  C             → 切换描边颜色" << std::endl;
    std::cout << "  鼠标滚轮       → 调整描边宽度" << std::endl;
    std::cout << "  Tab           → 显示/隐藏调试面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 16. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 16a. 计算 deltaTime =====
        static float lastFrame = 0.0f;
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // ===== 16b. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // ---- T 键：切换模板测试（★ 用 T 不用 S，避免和 WASD 中 S 后退冲突） ----
        static bool tPressed = false;
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
        {
            if (!tPressed)
            {
                stencilTestEnabled = !stencilTestEnabled;
                if (stencilTestEnabled) {
                    glEnable(GL_STENCIL_TEST);
                    std::cout << "🖊  模板测试: 启用" << std::endl;
                } else {
                    glDisable(GL_STENCIL_TEST);
                    std::cout << "🖊  模板测试: 禁用（描边效果消失）" << std::endl;
                }
                tPressed = true;
            }
        }
        else { tPressed = false; }

        // ---- O 键：切换描边可见性 ----
        static bool oPressed = false;
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        {
            if (!oPressed) {
                showOutline = !showOutline;
                std::cout << (showOutline ? "🖊  描边: 可见" : "🖊  描边: 隐藏") << std::endl;
                oPressed = true;
            }
        }
        else { oPressed = false; }

        // ---- C 键：切换描边颜色 ----
        static bool cPressed = false;
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        {
            if (!cPressed) {
                currentColorIdx = (currentColorIdx + 1) % NUM_COLORS;
                std::cout << "🖊  描边颜色: " << outlineColors[currentColorIdx].name << std::endl;
                cPressed = true;
            }
        }
        else { cPressed = false; }

        // ---- 鼠标滚轮：调整描边宽度 ----
        // ★ 通过 ImGui IO 来检测滚轮（ImGui 会捕获滚轮事件）
        float mouseWheel = io.MouseWheel;
        if (mouseWheel != 0.0f)
        {
            outlineScale += mouseWheel * 0.01f;
            outlineScale = glm::clamp(outlineScale, 0.02f, 0.25f);
            std::cout << "🖊  描边宽度: " << outlineScale << std::endl;
        }

        // ---- WASD / 箭头键：摄像机移动（S 是后退，同时触发模板切换是设计如此） ----
        float velocity = moveSpeed * deltaTime * 60.0f;
        glm::vec3 front;
        front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front.y = sin(glm::radians(camPitch));
        front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camPos += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camPos -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camPos -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camPos += right * velocity;

        // ---- Tab 键：切换面板 ----
        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
        {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        }
        else { tabPressed = false; }

        // ===== 16c. 清空缓冲 =====
        //
        // ★ 关键区别：加上 GL_STENCIL_BUFFER_BIT
        //   每次帧都需要清空模板缓冲区，否则上一帧的模板值会残留
        //
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // ===== 16d. ImGui：开始新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 16e. MVP 矩阵 =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );
        glm::mat4 view = glm::lookAt(camPos, camPos + front, glm::vec3(0.0f, 1.0f, 0.0f));

        // ================================================================
        // 第一部分：渲染地板（不参与模板写入）
        // ================================================================
        //
        // ★ 地板的模板值始终为 0（通过 glClear 清空为 0）
        //   地板不应该被描边，所以不需要写入模板缓冲区
        //
        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        // ★ 禁止写入模板缓冲（地板不参与模板标记）
        glStencilMask(0x00);

        // 绑定纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, floorTex);

        // 绘制地板
        glm::mat4 planeModel = glm::mat4(1.0f);
        shader.setMat4("model", planeModel);
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ================================================================
        // 第二部分：绘制立方体（正常渲染 + 写入模板值 1）
        // ================================================================
        //
        // ★ 模板测试设置为：
        //   - glStencilFunc(GL_ALWAYS, 1, 0xFF)：片段始终通过测试，参考值=1
        //   - glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE)：
        //       深度测试通过 → 用参考值 1 替换模板缓冲区中的值
        //       这样就"标记"了物体占据的像素
        //   - glStencilMask(0xFF)：允许写入全部 8 位
        //
        // ★ glStencilFunc 中的参考值（ref=1）与 glStencilOp 中 GL_REPLACE
        //   配合使用：当两个测试都通过时，将 ref 值写入模板缓冲区
        //
        glStencilFunc(GL_ALWAYS, 1, 0xFF);     // 始终通过，参考值=1
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);  // 深度通过→写入 ref=1
        glStencilMask(0xFF);                    // 允许写入模板值

        // 绑定立方体纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, containerTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, containerTex);

        glBindVertexArray(cubeVAO);

        for (int i = 0; i < NUM_CUBES; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubes[i].pos);
            model = glm::rotate(model, glm::radians(cubes[i].rotAngle), cubes[i].rotAxis);

            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // ================================================================
        // 第三部分：绘制描边（放大的立方体 + 纯色着色器）
        // ================================================================
        //
        // ★ 只在 showOutline 启用时才绘制描边
        // ★ 模板测试设置为：
        //   - glStencilFunc(GL_NOTEQUAL, 1, 0xFF)：模板值 ≠ 1 时才通过
        //       原始物体的区域模板值为 1 → 被排除
        //       放大部分超出原始物体的区域模板值为 0 → 通过测试 → 被绘制
        //       结果：只有边框（放大部分减去原始部分）被绘制
        //
        //   - glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP)：不修改模板值
        //       描边不应该改变模板缓冲区的值
        //
        //   - glStencilMask(0x00)：禁止写入模板缓冲
        //
        // ★ glDisable(GL_DEPTH_TEST)：禁用深度测试
        //   确保描边不会被其他物体遮挡（描边应该始终可见）
        //
        if (showOutline)
        {
            glStencilFunc(GL_NOTEQUAL, 1, 0xFF);   // 模板值 ≠ 1 → 通过
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); // 不修改模板值
            glStencilMask(0x00);                     // 禁止写入

            glDisable(GL_DEPTH_TEST);                // 描边不应被遮挡

            outlineShader.use();
            outlineShader.setMat4("projection", projection);
            outlineShader.setMat4("view", view);
            outlineShader.setVec3("outlineColor", outlineColors[currentColorIdx].color);

            glBindVertexArray(cubeVAO);

            for (int i = 0; i < NUM_CUBES; i++)
            {
                // ★ 关键：在局部空间放大模型
                //   translate * rotate * scale(outlineScale+1)
                //   放大的是物体本身，所以边框是均匀的
                //
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, cubes[i].pos);
                model = glm::rotate(model, glm::radians(cubes[i].rotAngle), cubes[i].rotAxis);
                model = glm::scale(model, glm::vec3(1.0f + outlineScale));

                outlineShader.setMat4("model", model);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }

            // ★ 恢复深度测试
            glEnable(GL_DEPTH_TEST);
        }

        // ★ 恢复模板测试到默认状态（供下一帧使用）
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask(0xFF);

        // ===== 16f. ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Stencil Testing");

            // ---- 性能 ----
            ImGui::Text("FPS: %.1f  (%.1f ms)", ImGui::GetIO().Framerate,
                        1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 模板测试核心概念 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Stencil Test — 物体描边");

            // 启用/禁用模板测试
            if (ImGui::Checkbox("Enable Stencil Test (T)", &stencilTestEnabled))
            {
                if (stencilTestEnabled) {
                    glEnable(GL_STENCIL_TEST);
                } else {
                    glDisable(GL_STENCIL_TEST);
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled(stencilTestEnabled ? "(ON)" : "(OFF)");

            // 模板测试状态说明
            if (!stencilTestEnabled)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                    "模板测试已禁用 → 描边效果失效（所有片段通过）");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                    "模板测试已启用 → Pass 2 标记物体区域，Pass 3 绘制边框");
            }
            ImGui::Separator();

            // ---- 描边控制 ----
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Outline Settings");

            ImGui::Checkbox("Show Outline (O)", &showOutline);

            // 描边颜色
            ImGui::Text("Outline Color:");
            if (ImGui::BeginCombo("##outlineColor", outlineColors[currentColorIdx].name))
            {
                for (int i = 0; i < NUM_COLORS; i++)
                {
                    bool selected = (currentColorIdx == i);
                    // 用颜色预览
                    ImVec4 colorPreview(outlineColors[i].color.r,
                                        outlineColors[i].color.g,
                                        outlineColors[i].color.b, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, colorPreview);
                    if (ImGui::Selectable(outlineColors[i].name, selected))
                    {
                        currentColorIdx = i;
                        std::cout << "🖊  描边颜色: " << outlineColors[i].name << std::endl;
                    }
                    ImGui::PopStyleColor();
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // 描边宽度
            if (ImGui::SliderFloat("Outline Scale (Scroll)", &outlineScale, 0.02f, 0.25f, "%.3f"))
            {
                // 值已在 slider 中更新
            }
            ImGui::TextDisabled("Scale = %.3f  → 物体放大 %.1f%%",
                outlineScale, outlineScale * 100.0f);
            ImGui::Separator();

            // ---- 当前模板状态 ----
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Current Stencil State");
            ImGui::TextDisabled(
                "Pass 1 (Floor):    stencilMask=0x00, no write\n"
                "Pass 2 (Objects):  func=GL_ALWAYS, ref=1, op=GL_REPLACE\n"
                "Pass 3 (Outline):  func=GL_NOTEQUAL, ref=1, no depth test");
            ImGui::Separator();

            // ---- 摄像机控制 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Camera");
            ImGui::SliderFloat("Move Speed", &moveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::Text("Position: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
            ImGui::Separator();

            // ---- 清屏色 ----
            ImGui::ColorEdit3("Clear Color", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            // ---- 操作帮助 ----
            ImGui::TextDisabled(
                "WASD/Arrows: Move      |  RClick+Drag: Look\n"
                "T: Toggle stencil test  |  O: Toggle outline\n"
                "C: Cycle outline color  |  Scroll: Outline width\n"
                "Tab: Panel              |  ESC: Quit");

            ImGui::End();
        }

        // ===== 16g. ImGui：渲染 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ===== 16h. 交换缓冲 + 事件处理 =====
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 17. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteTextures(1, &floorTex);
    glDeleteTextures(1, &containerTex);
    glfwTerminate();
    return 0;
}


// ================================================================
// 第十八章：混合（Blending）—— 透明与半透明渲染
// ================================================================

/**
 * ============================================================
 *  第十八章：混合（Blending）
 * ============================================================
 *
 * 混合是 OpenGL 高级部分的第三章。前面的深度测试和模板测试
 * 都在决定"片段是否通过"，而混合决定了"片段如何与已有颜色合并"。
 *
 * ★ 两种透明度处理方法：
 *
 * 1. Alpha 丢弃（Discard）
 *    - 使用 discard 关键字在片段着色器中丢弃透明像素
 *    - 深度缓冲区正常写入（非丢弃的像素）
 *    - 适用：只有"全透明/全不透明"的纹理（草叶、栅栏）
 *    - 不适用：半透明（比如 30% 透明的玻璃）
 *
 * 2. 混合（Blending）
 *    - glEnable(GL_BLEND)
 *    - glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
 *    - 公式: result = src * src_alpha + dst * (1 - src_alpha)
 *    - 深度写入必须关闭：glDepthMask(GL_FALSE)
 *    - ★ 关键：透明物体必须从远到近排序绘制
 *
 * ★ 本 Demo 演示的内容：
 *   - Alpha 丢弃：使用 awesomeface.png 的透明区域
 *   - 混合：多个半透明彩色窗口
 *   - 透明排序：按摄像机距离从远到近排序
 *   - 对比：开启/关闭混合时的差异
 *
 * 目标：理解 discard vs blend 的区别，掌握透明物体的正确渲染顺序。
 */

int runBlendingDemo()
{
    const unsigned int SCR_WIDTH  = 1000;
    const unsigned int SCR_HEIGHT = 700;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Blending | B:Blend G:Grass M:Mode Scroll:GrassScale",
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

    // ========== 3. 基础 OpenGL 设置 ==========
    //
    // ★ 启用深度测试（不透明物体需要）
    //   但透明物体绘制时要关闭深度写入
    //
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);  // 本章不需要模板测试

    // ★ 混合默认是关闭的，在渲染循环中根据需要开启
    glDisable(GL_BLEND);

    // ========== 4. 鼠标回调（FPS 视角） ==========
    static float camYaw   = -90.0f;
    static float camPitch = 0.0f;
    static bool  firstMouse = true;
    static double lastMX = 500.0, lastMY = 350.0;

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
            firstMouse = true;
            return;
        }
        if (firstMouse) {
            lastMX = xpos;
            lastMY = ypos;
            firstMouse = false;
        }
        double dx = xpos - lastMX;
        double dy = lastMY - ypos;
        lastMX = xpos;
        lastMY = ypos;
        camYaw   += (float)dx * 0.1f;
        camPitch += (float)dy * 0.1f;
        camPitch = glm::clamp(camPitch, -89.0f, 89.0f);
    });

    // ========== 5. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 6. 编译着色器 ==========
    //
    // 普通着色器：不透明物体（地板 + 立方体）
    //
    Shader shader("shaders/coordinates/coordinate_system.vert",
                  "shaders/textures/texture_combined.frag", true);

    // Alpha 丢弃着色器：用于草叶/植被
    //   片段着色器中使用 discard 丢弃透明像素
    //
    Shader discardShader("shaders/coordinates/coordinate_system.vert",
                         "shaders/blending/blend_discard.frag", true);

    // 半透明着色器：用于窗户/玻璃
    //   输出带有 alpha 的颜色，配合 glBlendFunc 混合
    //
    Shader blendShader("shaders/blending/blend_alpha.vert",
                       "shaders/blending/blend_transparent.frag", true);

    // ========== 7. 地板平面数据 ==========
    //
    // 格式：[位置 xyz] [纹理坐标 uv] → 每个顶点 5 float
    //
    float planeVertices[] = {
        // ---- 位置 ----------    -- uv ---
        -5.0f, -0.51f, -5.0f,     0.0f, 3.0f,
         5.0f, -0.51f,  5.0f,     3.0f, 0.0f,
         5.0f, -0.51f, -5.0f,     3.0f, 3.0f,
         5.0f, -0.51f,  5.0f,     3.0f, 0.0f,
        -5.0f, -0.51f, -5.0f,     0.0f, 3.0f,
        -5.0f, -0.51f,  5.0f,     0.0f, 0.0f
    };

    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);

    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 8. 立方体数据 ==========
    //
    // 两个不透明的容器立方体
    //
    float cubeVertices[] = {
        // ============ 背面 (Z-) ============
        -0.5f, -0.5f, -0.5f,     0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 1.0f,
        // ============ 正面 (Z+) ============
        -0.5f, -0.5f,  0.5f,     0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,     1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,     1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,     0.0f, 1.0f,
        // ============ 左面 (X-) ============
        -0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,     1.0f, 1.0f,
        // ============ 右面 (X+) ============
         0.5f,  0.5f,  0.5f,     0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,     0.0f, 1.0f,
        // ============ 底面 (Y-) ============
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,     0.0f, 0.0f,
        // ============ 顶面 (Y+) ============
        -0.5f,  0.5f, -0.5f,     0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,     0.0f, 0.0f
    };

    unsigned int cubeIndices[] = {
         0,  2,  1,    2,  0,  3,
         4,  5,  6,    6,  7,  4,
         8,  9, 10,   10, 11,  8,
        12, 14, 13,   14, 12, 15,
        16, 17, 18,   18, 19, 16,
        20, 22, 21,   22, 20, 23
    };

    unsigned int cubeVAO, cubeVBO, cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 9. 通用四边形数据（用于草地 + 透明窗户） ==========
    //
    // 一个简单的正方形（2 个三角形），法线朝 Z+ 方向
    // 格式：[位置 xyz] [纹理坐标 uv] → 每个顶点 5 float
    //
    // ★ 四边形顶点按 CCW 绕序（从 +Z 方向看），使正面朝向摄像机
    //   修复前：CW 绕序 → 面剔除下正面不可见，只能从背后看到
    //   修复后：CCW 绕序 → 正面可见，与 OpenGL 默认一致
    float quadVertices[] = {
        // ---- 位置 ----------    -- uv ---
        -0.5f, -0.5f,  0.0f,      0.0f, 0.0f,   // 左下
         0.5f,  0.5f,  0.0f,      1.0f, 1.0f,   // 右上
         0.5f, -0.5f,  0.0f,      1.0f, 0.0f,   // 右下

        -0.5f, -0.5f,  0.0f,      0.0f, 0.0f,   // 左下
        -0.5f,  0.5f,  0.0f,      0.0f, 1.0f,   // 左上
         0.5f,  0.5f,  0.0f,      1.0f, 1.0f    // 右上
    };

    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 10. 加载纹理 ==========
    //
    // marbleTex：大理石地板纹理（LearnOpenGL 官方资源）
    // containerTex：立方体的木箱纹理
    // grassTex：草地纹理（有 alpha 通道，官方 grass.png，用于 discard 演示）
    // windowTex：半透明窗户纹理（官方 blending_transparent_window.png）
    //
    unsigned int marbleTex     = loadTexture("textures/marble.jpg", true);
    unsigned int containerTex  = loadTexture("textures/container.jpg");

    // ★ 用于 alpha discard 的草地纹理 — 需要 alpha 通道
    unsigned int grassTex = loadTexture("textures/grass.png", true);

    // ★ 关键：为避免 discard 的边缘伪影，设环绕模式为 CLAMP
    glBindTexture(GL_TEXTURE_2D, grassTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // ★ 用于半透明混合的窗户纹理
    unsigned int windowTex = loadTexture("textures/window.png", true);
    glBindTexture(GL_TEXTURE_2D, windowTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // ========== 11. 纹理单元 ==========
    shader.use();
    shader.setInt("texture1", 0);
    shader.setInt("texture2", 1);

    discardShader.use();
    discardShader.setInt("texture1", 0);

    blendShader.use();
    blendShader.setInt("texture1", 0);

    // ========== 12. 场景物体位置 ==========

    // ---- 不透明立方体 ----
    struct { glm::vec3 pos; float rotAngle; glm::vec3 rotAxis; } cubes[] = {
        { glm::vec3(-0.8f, 0.01f, -2.5f),  20.0f, glm::vec3(1.0f, 0.3f, 0.5f) },
        { glm::vec3( 0.8f, 0.01f, -3.5f),  35.0f, glm::vec3(0.2f, 1.0f, 0.4f) },
    };
    const int NUM_CUBES = 2;

    // ---- 草地四边形（alpha discard） ----
    // 两个面朝摄像机的"草牌"，展示 discard 效果
    //
    struct { glm::vec3 pos; float yRot; float scale; } grasses[] = {
        { glm::vec3(-1.5f,  0.3f, -2.0f),  15.0f, 1.2f },
        { glm::vec3( 1.2f,  0.3f, -2.8f), -20.0f, 1.0f },
        { glm::vec3( 0.0f,  0.3f, -3.2f),  45.0f, 1.3f },
    };
    const int NUM_GRASSES = 3;

    // ---- 透明窗户（半透明混合） ----
    // 多个半透明窗户四边形（使用官方 window.png 纹理），分布在不同深度
    //
    struct WindowDef {
        glm::vec3 pos;
        float     yRot;
        glm::vec3 scale;
        const char* label;
    };
    WindowDef windows[] = {
        // 窗户 1 — 最近
        { glm::vec3(-1.5f,  0.5f, -1.5f),   0.0f, glm::vec3(1.2f, 1.5f, 1.0f), "Near"      },
        // 窗户 2 — 中间
        { glm::vec3( 0.0f,  0.5f, -2.2f),   0.0f, glm::vec3(1.2f, 1.5f, 1.0f), "Mid"       },
        // 窗户 3 — 远
        { glm::vec3( 1.5f,  0.5f, -2.8f),   0.0f, glm::vec3(1.2f, 1.5f, 1.0f), "Far"       },
        // 窗户 4 — 地板和立方体之间
        { glm::vec3(-0.5f,  0.3f, -3.5f),  25.0f, glm::vec3(1.2f, 1.5f, 1.0f), "Farthest"  },
    };
    const int NUM_WINDOWS = 4;

    // ========== 13. 用户控制参数 ==========

    // ---- 混合 ----
    bool  blendEnabled   = true;    // 是否启用混合
    bool  showGrass      = true;    // 是否显示草地（discard）

    // ---- 混合模式 ----
    // 0: GL_FUNC_ADD (默认叠加)
    // 1: GL_FUNC_SUBTRACT
    // 2: GL_FUNC_REVERSE_SUBTRACT
    int   blendEqMode    = 0;
    const char* blendEqNames[] = {
        "GL_FUNC_ADD (默认叠加)",
        "GL_FUNC_SUBTRACT (源减目标)",
        "GL_FUNC_REVERSE_SUBTRACT (目标减源)"
    };

    // ---- 摄像机 ----
    glm::vec3 camPos     = glm::vec3(0.0f, 1.5f, 5.5f);
    float     fov        = 50.0f;
    float     moveSpeed  = 0.08f;

    // ---- 调试 ----
    bool  showDebugPanel = true;
    float clearColor[3]  = { 0.1f, 0.1f, 0.1f };

    // ========== 14. 清屏设置 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    // ========== 15. 控制提示 ==========
    std::cout << "\n============================================" << std::endl;
    std::cout << "  混合（Blending）— Alpha 丢弃 & 半透明" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  ESC           → 退出" << std::endl;
    std::cout << "  WASD / 箭头   → 前后左右移动" << std::endl;
    std::cout << "  右键拖动       → 旋转视角" << std::endl;
    std::cout << "  B             → 切换混合 开/关" << std::endl;
    std::cout << "  G             → 切换草地显示（discard）" << std::endl;
    std::cout << "  M             → 切换混合方程模式" << std::endl;
    std::cout << "  鼠标滚轮       → 调整草地缩放" << std::endl;
    std::cout << "  Tab           → 显示/隐藏调试面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 16. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 16a. 计算 deltaTime =====
        static float lastFrame = 0.0f;
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // ===== 16b. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // ---- B 键：切换混合 ----
        static bool bPressed = false;
        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
        {
            if (!bPressed) {
                blendEnabled = !blendEnabled;
                std::cout << (blendEnabled ? "🎨 混合: 启用（窗户半透明）" : "🎨 混合: 禁用（窗户变为不透明）") << std::endl;
                bPressed = true;
            }
        }
        else { bPressed = false; }

        // ---- G 键：切换草地显示 ----
        static bool gPressed = false;
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
        {
            if (!gPressed) {
                showGrass = !showGrass;
                std::cout << (showGrass ? "🌿 草地（discard）: 可见" : "🌿 草地（discard）: 隐藏") << std::endl;
                gPressed = true;
            }
        }
        else { gPressed = false; }

        // ---- M 键：切换混合方程 ----
        static bool mPressed = false;
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS)
        {
            if (!mPressed) {
                blendEqMode = (blendEqMode + 1) % 3;
                std::cout << "🎨 混合方程: " << blendEqNames[blendEqMode] << std::endl;
                mPressed = true;
            }
        }
        else { mPressed = false; }

        // ---- 鼠标滚轮：调整草地缩放 ----
        static float grassScale = 1.0f;
        float mouseWheel = io.MouseWheel;
        if (mouseWheel != 0.0f)
        {
            grassScale += mouseWheel * 0.1f;
            grassScale = glm::clamp(grassScale, 0.5f, 2.5f);
            std::cout << "🌿 草地缩放: " << grassScale << std::endl;
        }

        // ---- WASD / 箭头键：摄像机移动 ----
        float velocity = moveSpeed * deltaTime * 60.0f;
        glm::vec3 front;
        front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front.y = sin(glm::radians(camPitch));
        front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camPos += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camPos -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camPos -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camPos += right * velocity;

        // ---- Tab 键：切换面板 ----
        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
        {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        }
        else { tabPressed = false; }

        // ===== 16c. 清空缓冲 =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ===== 16d. ImGui 新帧 =====
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== 16e. MVP 矩阵 =====
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );
        glm::mat4 view = glm::lookAt(camPos, camPos + front, glm::vec3(0.0f, 1.0f, 0.0f));

        // ================================================================
        // 第一部分：渲染不透明物体（地板 + 立方体）
        // ================================================================
        //
        // ★ 不透明物体必须先绘制，这样深度缓冲区中才有完整信息
        //   混合的透明物体需要知道"背后有什么"
        //
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);  // 不透明物体正常写入深度

        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        // ---- 地板 ----
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, marbleTex);   // ★ 官方 marble.jpg 大理石地板
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, marbleTex);

        glm::mat4 planeModel = glm::mat4(1.0f);
        shader.setMat4("model", planeModel);
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ---- 立方体 ----
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, containerTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, containerTex);

        glBindVertexArray(cubeVAO);
        for (int i = 0; i < NUM_CUBES; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubes[i].pos);
            model = glm::rotate(model, glm::radians(cubes[i].rotAngle), cubes[i].rotAxis);
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // ================================================================
        // 第二部分：渲染草地（Alpha Discard）
        // ================================================================
        //
        // ★ discard 的物体不需要排序 — 非丢弃的像素正常写入深度缓冲区，
        //   丢弃的像素不写入任何东西。
        //   所以 discard 物体可以在透明混合之前绘制，顺序无所谓。
        //
        // ★ discard 物体也不需要使用混合，因为片段要么完全可见，
        //   要么完全被丢弃。
        //
        if (showGrass)
        {
            // 不透明物体已写入深度，保持深度写入开启
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);

            discardShader.use();
            discardShader.setMat4("projection", projection);
            discardShader.setMat4("view", view);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, grassTex);  // ★ 官方 grass.png 纹理

            glBindVertexArray(quadVAO);

            for (int i = 0; i < NUM_GRASSES; i++)
            {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, grasses[i].pos);
                // ★ 旋转使四边形始终面朝摄像机（Billboarding 效果）
                model = glm::rotate(model, glm::radians(camYaw + grasses[i].yRot), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(grasses[i].scale * grassScale));

                discardShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        // ================================================================
        // 第三部分：渲染透明窗户（Blending）
        // ================================================================
        //
        // ★ 混合的核心规则：
        //   1. 先绘制所有不透明物体（已完成）
        //   2. 对透明物体按摄像机距离排序（从远到近）
        //   3. 关闭深度写入（glDepthMask(GL_FALSE)）
        //       — 透明物体不应阻挡后面的透明物体
        //   4. 从远到近依次绘制
        //
        // ★ 为什么从远到近？
        //   混合公式：result = src_alpha * src + (1-src_alpha) * dst
        //   dst 是"已经绘制的内容"，所以远处的必须先画好，
        //   近处的才能在它上面叠加混合。
        //

        // ---- 3a. 排序窗户（从远到近） ----
        // 计算每个窗户到摄像机的距离，按距离降序排列
        //
        std::map<float, int> sortedWindows;  // distance → window index
        for (int i = 0; i < NUM_WINDOWS; i++)
        {
            float distance = glm::length(camPos - windows[i].pos);
            // ★ 用负距离作为 key 实现降序（远的在前）
            //   即最远的 → 最大的距离 → 最小的负数 → 最先取出
            sortedWindows[-distance] = i;
        }

        // ---- 3b. 启用混合并绘制 ----
        //
        // ★ 注意：如果混合被禁用，窗户会像不透明物体一样绘制
        //   （按哪种顺序都可以，因为深度测试会处理遮挡）
        //
        if (blendEnabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }

        // 设置混合方程
        switch (blendEqMode) {
            case 0:
                glBlendEquation(GL_FUNC_ADD);
                break;
            case 1:
                glBlendEquation(GL_FUNC_SUBTRACT);
                break;
            case 2:
                glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                break;
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // ★ 关闭深度写入！透明物体不应阻挡后面的透明物体
        glDepthMask(GL_FALSE);

        blendShader.use();
        blendShader.setMat4("projection", projection);
        blendShader.setMat4("view", view);
        blendShader.setBool("useTexture", true);   // ★ 使用 window.png 纹理

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, windowTex);   // ★ 官方 blending_transparent_window.png

        glBindVertexArray(quadVAO);

        // 从远到近绘制（map 按 key 升序，负距离 → 最远的先出来）
        for (auto& entry : sortedWindows)
        {
            int i = entry.second;
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, windows[i].pos);
            model = glm::rotate(model, glm::radians(windows[i].yRot), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, windows[i].scale);

            blendShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // ---- 3c. 恢复状态 ----
        glDepthMask(GL_TRUE);   // 重新启用深度写入
        glDisable(GL_BLEND);    // 关闭混合

        // ===== 16f. ImGui 调试面板 =====
        if (showDebugPanel)
        {
            ImGui::Begin("Debug Panel - Blending");

            // ---- 性能 ----
            ImGui::Text("FPS: %.1f  (%.1f ms)", ImGui::GetIO().Framerate,
                        1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 混合核心控制 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Blending");

            // 启用/禁用混合
            if (ImGui::Checkbox("Enable Blending (B)", &blendEnabled))
            {
                // 不需要额外的 gl 调用，下一帧会处理
            }
            ImGui::SameLine();
            ImGui::TextDisabled(blendEnabled ? "(ON)" : "(OFF)");

            if (blendEnabled)
            {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                    "混合已启用 → 窗户半透明，按距离排序绘制");
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                    "混合已禁用 → 窗户变为不透明（无视 alpha）");
            }
            ImGui::Separator();

            // ---- 混合参数 ----
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Blend Settings");

            // 混合方程
            if (ImGui::BeginCombo("Blend Equation (M)", blendEqNames[blendEqMode]))
            {
                for (int i = 0; i < 3; i++)
                {
                    bool selected = (blendEqMode == i);
                    if (ImGui::Selectable(blendEqNames[i], selected))
                    {
                        blendEqMode = i;
                        std::cout << "🎨 混合方程: " << blendEqNames[i] << std::endl;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Text("glBlendFunc: GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA");
            ImGui::Text("Result = src * alpha + dst * (1 - alpha)");
            ImGui::TextDisabled("窗户使用官方 window.png 纹理（自带 alpha）");
            ImGui::Separator();

            // ---- 窗户信息 ----
            ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "Windows (sorted back→front)");
            std::map<float, int> displaySorted;
            for (int i = 0; i < NUM_WINDOWS; i++)
                displaySorted[-glm::length(camPos - windows[i].pos)] = i;
            for (auto& entry : displaySorted)
            {
                int i = entry.second;
                float dist = glm::length(camPos - windows[i].pos);
                ImGui::Text("  %s: dist=%.1f", windows[i].label, dist);
            }
            ImGui::Separator();

            // ---- 草地控制 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Vegetation (Discard)");
            ImGui::Checkbox("Show Grass (G)", &showGrass);
            ImGui::TextDisabled(
                "使用 discard 丢弃 alpha < 0.1 的片段。\n"
                "纹理：grass.png（CLAMP_TO_EDGE 环绕）\n"
                "滚轮调整缩放: %.1f", grassScale);
            ImGui::Separator();

            // ---- 渲染顺序说明 ----
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Rendering Order");
            ImGui::TextDisabled(
                "1. Floor + Cubes (opaque, depth writes ON)\n"
                "2. Grass quads (discard, depth writes ON)\n"
                "3. Windows (blended, depth writes OFF, sorted)");
            ImGui::Separator();

            // ---- 摄像机 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Camera");
            ImGui::SliderFloat("Move Speed", &moveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::Separator();

            ImGui::ColorEdit3("Clear Color", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            ImGui::TextDisabled(
                "WASD/Arrows: Move       |  RClick+Drag: Look\n"
                "B: Toggle blending       |  G: Toggle grass\n"
                "M: Blend equation mode   |  Scroll: Window alpha\n"
                "Tab: Panel               |  ESC: Quit");

            ImGui::End();
        }

        // ===== 16g. ImGui 渲染 =====
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ===== 16h. 交换缓冲 =====
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 17. 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteTextures(1, &marbleTex);
    glDeleteTextures(1, &containerTex);
    glDeleteTextures(1, &grassTex);
    glDeleteTextures(1, &windowTex);
    glfwTerminate();
    return 0;
}


// ================================================================
// 第十九章：面剔除（Face Culling）
// ================================================================

/**
 * ============================================================
 *  第十九章：面剔除（Face Culling）
 * ============================================================
 *
 * 面剔除是 OpenGL 高级部分第四章，解决"看不见的面要不要画"的问题。
 *
 * ★ 核心概念
 *   在一个封闭的 3D 立方体中，你最多只能看到 3 个面（正面视角），
 *   其余 3~5 个面背对着摄像机——画它们纯属浪费。
 *   启用面剔除后，OpenGL 会检查每个三角形的朝向，丢弃朝"错误方向"的。
 *
 * ★ 关键 API
 *   glEnable(GL_CULL_FACE)   — 启用面剔除
 *   glCullFace(GL_BACK)      — 剔除背面（默认）
 *   glCullFace(GL_FRONT)     — 剔除正面（能看到物体"内部"）
 *   glFrontFace(GL_CCW)      — 声明 CCW 绕序 = 正面（默认）
 *   glFrontFace(GL_CW)       — 声明 CW 绕序 = 正面
 *
 * ★ 绕序决定一切
 *   三角形的顶点提交顺序决定了方向：
 *      CCW（逆时针）= 正面（默认）
 *      CW（顺时针）  = 背面
 *   如果你的模型顶点绕序与默认不一致 → 正面被剔除 → 物体"消失"了
 *
 * ★ 本 Demo 演示的内容
 *   - 默认 GL_BACK 剔除：正常的背面消除
 *   - GL_FRONT 剔除：看到立方体内部（类似剖面效果）
 *   - 绕序切换：反转 front face 定义
 *   - 线框模式：可视化三角形的实际朝向
 *
 * 目标：理解面剔除的原理、掌握相关 API、识别绕序问题。
 */

int runFaceCullingDemo()
{
    const unsigned int SCR_WIDTH  = 1000;
    const unsigned int SCR_HEIGHT = 700;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Face Culling | F:Toggle 1/2/3:Mode Q:Wire R:Reverse",
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

    // ========== 3. 基础 OpenGL 设置 ==========
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);   // ★ 面剔除：默认剔除背面
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);

    // ========== 4. 鼠标回调 ==========
    static float camYaw   = -90.0f;
    static float camPitch = 0.0f;
    static bool  firstMouse = true;
    static double lastMX = 500.0, lastMY = 350.0;

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
            firstMouse = true;
            return;
        }
        if (firstMouse) { lastMX = xpos; lastMY = ypos; firstMouse = false; }
        double dx = xpos - lastMX, dy = lastMY - ypos;
        lastMX = xpos; lastMY = ypos;
        camYaw   += (float)dx * 0.1f;
        camPitch += (float)dy * 0.1f;
        camPitch = glm::clamp(camPitch, -89.0f, 89.0f);
    });

    // ========== 5. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 6. 编译着色器 ==========
    Shader shader("shaders/coordinates/coordinate_system.vert",
                  "shaders/textures/texture_combined.frag", true);

    // 纯色着色器（用于线框模式）
    Shader solidShader("shaders/coordinates/coordinate_system.vert",
                       "shaders/stencil_testing/stencil_outline.frag", true);

    // ========== 7. 地板平面 ==========
    float planeVertices[] = {
        -5.0f, -0.51f, -5.0f,     0.0f, 3.0f,
         5.0f, -0.51f,  5.0f,     3.0f, 0.0f,
         5.0f, -0.51f, -5.0f,     3.0f, 3.0f,
         5.0f, -0.51f,  5.0f,     3.0f, 0.0f,
        -5.0f, -0.51f, -5.0f,     0.0f, 3.0f,
        -5.0f, -0.51f,  5.0f,     0.0f, 0.0f
    };
    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 8. 立方体数据 ==========
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,     0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,     0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,     1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,     1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,     0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,     1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,     0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,     0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,     0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,     0.0f, 0.0f
    };
    unsigned int cubeIndices[] = {
         0,  2,  1,    2,  0,  3,    4,  5,  6,    6,  7,  4,
         8,  9, 10,   10, 11,  8,   12, 14, 13,   14, 12, 15,
        16, 17, 18,   18, 19, 16,   20, 22, 21,   22, 20, 23
    };
    unsigned int cubeVAO, cubeVBO, cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 9. 加载纹理 ==========
    unsigned int floorTex     = loadTexture("textures/marble.jpg", true);
    unsigned int containerTex = loadTexture("textures/container.jpg");

    // ========== 10. 纹理单元 ==========
    shader.use();
    shader.setInt("texture1", 0);
    shader.setInt("texture2", 1);

    // ========== 11. 立方体位置 ==========
    struct { glm::vec3 pos; float rotSpeed; glm::vec3 rotAxis; } cubes[] = {
        { glm::vec3(-0.8f, 0.01f, -2.5f),  25.0f, glm::vec3(1.0f, 0.3f, 0.5f) },
        { glm::vec3( 0.8f, 0.01f, -2.5f), -30.0f, glm::vec3(0.2f, 1.0f, 0.4f) },
    };
    const int NUM_CUBES = 2;

    // ========== 12. 用户控制参数 ==========
    bool  cullingEnabled = true;      // 面剔除开关
    int   cullMode       = 0;        // 0=GL_BACK, 1=GL_FRONT, 2=GL_FRONT_AND_BACK
    int   frontFace      = 0;        // 0=GL_CCW, 1=GL_CW
    bool  wireframe      = false;    // 线框模式
    bool  showFloor      = true;

    const char* cullModeNames[] = {
        "GL_BACK (剔除背面, 默认)",
        "GL_FRONT (剔除正面, 看到内部)",
        "GL_FRONT_AND_BACK (全部剔除)"
    };
    const char* frontFaceNames[] = {
        "GL_CCW (逆时针=正面, 默认)",
        "GL_CW (顺时针=正面)"
    };

    // ---- 摄像机 ----
    glm::vec3 camPos   = glm::vec3(0.0f, 1.5f, 5.0f);
    float     fov      = 50.0f;
    float     moveSpeed = 0.08f;

    bool  showDebugPanel = true;
    float clearColor[3]  = { 0.1f, 0.1f, 0.1f };

    // ========== 13. 清屏 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    std::cout << "\n============================================" << std::endl;
    std::cout << "  面剔除（Face Culling）" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  F      → 切换面剔除 开/关" << std::endl;
    std::cout << "  1/2/3  → 切换剔除模式（背面/正面/全部）" << std::endl;
    std::cout << "  R      → 切换绕序方向 (CCW/CW)" << std::endl;
    std::cout << "  Q      → 切换线框模式" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 14. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        static float lastFrame = 0.0f;
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // ---- 输入处理 ----
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        static bool fPressed = false;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!fPressed) {
                cullingEnabled = !cullingEnabled;
                if (cullingEnabled) glEnable(GL_CULL_FACE);
                else               glDisable(GL_CULL_FACE);
                std::cout << (cullingEnabled ? "🗑 面剔除: 启用" : "🗑 面剔除: 禁用") << std::endl;
                fPressed = true;
            }
        } else fPressed = false;

        static bool k1Pressed = false, k2Pressed = false, k3Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !k1Pressed) {
            k1Pressed = true; cullMode = 0;
            glCullFace(GL_BACK);
            std::cout << "🗑 剔除模式: " << cullModeNames[0] << std::endl;
        } else k1Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !k2Pressed) {
            k2Pressed = true; cullMode = 1;
            glCullFace(GL_FRONT);
            std::cout << "🗑 剔除模式: " << cullModeNames[1] << std::endl;
        } else k2Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !k3Pressed) {
            k3Pressed = true; cullMode = 2;
            glCullFace(GL_FRONT_AND_BACK);
            std::cout << "🗑 剔除模式: " << cullModeNames[2] << std::endl;
        } else k3Pressed = false;

        static bool rPressed = false;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            if (!rPressed) {
                frontFace = (frontFace + 1) % 2;
                glFrontFace(frontFace == 0 ? GL_CCW : GL_CW);
                std::cout << "🗑 绕序方向: " << frontFaceNames[frontFace] << std::endl;
                rPressed = true;
            }
        } else rPressed = false;

        static bool qPressed = false;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            if (!qPressed) {
                wireframe = !wireframe;
                if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                else           glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                std::cout << (wireframe ? "📐 线框模式: 启用" : "📐 线框模式: 禁用") << std::endl;
                qPressed = true;
            }
        } else qPressed = false;

        static bool gPressed = false;
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
            if (!gPressed) { showFloor = !showFloor; gPressed = true; }
        } else gPressed = false;

        // ---- 摄像机移动 ----
        float velocity = moveSpeed * deltaTime * 60.0f;
        glm::vec3 front;
        front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front.y = sin(glm::radians(camPitch));
        front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += right * velocity;
        // 箭头键始终有效
        if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) camPos += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) camPos -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) camPos -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) camPos += right * velocity;

        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        } else tabPressed = false;

        // ---- 清空缓冲 ----
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---- ImGui 新帧 ----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---- MVP 矩阵 ----
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view       = glm::lookAt(camPos, camPos + front, glm::vec3(0.0f, 1.0f, 0.0f));

        // ============================================================
        // 渲染地板
        // ============================================================
        if (showFloor) {
            shader.use();
            shader.setMat4("projection", projection);
            shader.setMat4("view", view);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, floorTex);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, floorTex);

            glm::mat4 planeModel = glm::mat4(1.0f);
            shader.setMat4("model", planeModel);
            glBindVertexArray(planeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // ============================================================
        // 渲染旋转立方体
        // ============================================================
        //
        // ★ 选择着色器：正常纹理 vs 线框单色
        //
        Shader& activeShader = wireframe ? solidShader : shader;
        activeShader.use();
        activeShader.setMat4("projection", projection);
        activeShader.setMat4("view", view);

        if (!wireframe) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, containerTex);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, containerTex);
        } else {
            // 线框模式：纯色（亮绿，容易分辨面朝向）
            solidShader.setVec3("outlineColor", 0.3f, 1.0f, 0.3f);
        }

        glBindVertexArray(cubeVAO);
        for (int i = 0; i < NUM_CUBES; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubes[i].pos);
            model = glm::rotate(model, glm::radians(cubes[i].rotSpeed * currentFrame), cubes[i].rotAxis);
            activeShader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // 恢复填充模式（即使没开线框也是无害的）
        if (wireframe) {
            // 已经在上面设置了，但为了安全在 ImGui 渲染前恢复
            // （ImGui 需要填充模式）
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        // ============================================================
        // ImGui 面板
        // ============================================================
        if (showDebugPanel) {
            ImGui::Begin("Debug Panel - Face Culling");

            ImGui::Text("FPS: %.1f  (%.1f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 面剔除开关 ----
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "★ Face Culling");
            if (ImGui::Checkbox("Enable Culling (F)", &cullingEnabled)) {
                if (cullingEnabled) glEnable(GL_CULL_FACE);
                else               glDisable(GL_CULL_FACE);
            }
            ImGui::SameLine();
            ImGui::TextDisabled(cullingEnabled ? "(ON)" : "(OFF)");

            if (!cullingEnabled) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                    "面剔除已禁用 → 所有面都会被绘制（包括背面）");
            }
            ImGui::Separator();

            // ---- 剔除模式 ----
            if (cullingEnabled) {
                ImGui::Text("Cull Mode:");
                if (ImGui::BeginCombo("##cullMode", cullModeNames[cullMode])) {
                    for (int i = 0; i < 3; i++) {
                        bool sel = (cullMode == i);
                        if (ImGui::Selectable(cullModeNames[i], sel)) {
                            cullMode = i;
                            GLenum modes[] = { GL_BACK, GL_FRONT, GL_FRONT_AND_BACK };
                            glCullFace(modes[i]);
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // 预览效果描述
                const char* hints[] = {
                    "效果: 正常 3D 渲染 (看不见背面)",
                    "效果: 看到立方体内部 (剖面效果)",
                    "效果: 所有三角形都被剔除 (物体消失!)"
                };
                ImGui::TextDisabled("%s", hints[cullMode]);
            }
            ImGui::Separator();

            // ---- 绕序 ----
            ImGui::Text("Front Face (R):");
            if (ImGui::BeginCombo("##frontFace", frontFaceNames[frontFace])) {
                for (int i = 0; i < 2; i++) {
                    bool sel = (frontFace == i);
                    if (ImGui::Selectable(frontFaceNames[i], sel)) {
                        frontFace = i;
                        glFrontFace(i == 0 ? GL_CCW : GL_CW);
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("CCW 是 OpenGL 默认，大多数模型使用 CCW 绕序");
            ImGui::Separator();

            // ---- 线框 ----
            ImGui::Checkbox("Wireframe (W)", &wireframe);
            if (wireframe) {
                ImGui::TextDisabled("线框模式: 可以看到所有三角形，\n便于观察哪些面被剔除了");
            }
            ImGui::Checkbox("Show Floor (G)", &showFloor);
            ImGui::Separator();

            // ---- 摄像机 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Camera");
            ImGui::SliderFloat("Move Speed", &moveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::Separator();

            ImGui::ColorEdit3("Clear Color", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            ImGui::TextDisabled(
                "F: Toggle culling   |  1/2/3: Mode\n"
                "Q: Wireframe        |  R: Reverse winding\n"
                "G: Toggle floor     |  Tab: Panel\n"
                "WASD/Arrows: Move   |  RClick+Drag: Look");

            ImGui::End();
        }

        // ImGui 渲染前恢复填充模式（线框在立方体绘制后已恢复）
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 15. 清理 ==========
    // 恢复默认状态
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteTextures(1, &floorTex);
    glDeleteTextures(1, &containerTex);
    glfwTerminate();
    return 0;
}


// ╔══════════════════════════════════════════════════════════════╗
// ║              帧缓冲（Framebuffers）Demo                      ║
// ╚══════════════════════════════════════════════════════════════╝

int runFramebuffersDemo()
{
    const unsigned int SCR_WIDTH  = 1000;
    const unsigned int SCR_HEIGHT = 700;

    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "LearnOpenGL - Framebuffers | 1-6:Effect F:Flip Tab:Panel",
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

    // ========== 3. 基础 OpenGL 设置 ==========
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);

    // ========== 4. 鼠标回调（FPS 视角） ==========
    static float camYaw   = -90.0f;
    static float camPitch = 0.0f;
    static bool  firstMouse = true;
    static double lastMX = 500.0, lastMY = 350.0;

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
            firstMouse = true;
            return;
        }
        if (firstMouse) { lastMX = xpos; lastMY = ypos; firstMouse = false; }
        double dx = xpos - lastMX, dy = lastMY - ypos;
        lastMX = xpos; lastMY = ypos;
        camYaw   += (float)dx * 0.1f;
        camPitch += (float)dy * 0.1f;
        camPitch = glm::clamp(camPitch, -89.0f, 89.0f);
    });

    // ========== 5. 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ========== 6. 编译着色器 ==========
    //
    // 场景着色器：用于渲染 3D 场景（地板 + 立方体）
    //
    Shader sceneShader("shaders/coordinates/coordinate_system.vert",
                       "shaders/textures/texture_combined.frag", true);

    // 屏幕着色器：用于将 FBO 纹理渲染到全屏四边形，应用后处理
    //
    Shader screenShader("shaders/framebuffers/screen.vert",
                        "shaders/framebuffers/screen.frag", true);

    // ========== 7. 地板平面数据 ==========
    float planeVertices[] = {
        // ---- 位置 ----------    -- uv ---
        -5.0f, -0.51f, -5.0f,     0.0f, 3.0f,
         5.0f, -0.51f,  5.0f,     3.0f, 0.0f,
         5.0f, -0.51f, -5.0f,     3.0f, 3.0f,
         5.0f, -0.51f,  5.0f,     3.0f, 0.0f,
        -5.0f, -0.51f, -5.0f,     0.0f, 3.0f,
        -5.0f, -0.51f,  5.0f,     0.0f, 0.0f
    };
    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 8. 立方体数据 ==========
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,     0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,     0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,     1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,     1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,     0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,     1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,     0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,     0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,     0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,     0.0f, 0.0f
    };
    unsigned int cubeIndices[] = {
         0,  2,  1,    2,  0,  3,    4,  5,  6,    6,  7,  4,
         8,  9, 10,   10, 11,  8,   12, 14, 13,   14, 12, 15,
        16, 17, 18,   18, 19, 16,   20, 22, 21,   22, 20, 23
    };
    unsigned int cubeVAO, cubeVBO, cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 9. 全屏四边形（后处理用） ==========
    //
    // NDC 坐标 + 纹理坐标，每顶点 4 个 float
    // 直接绘制到整个屏幕，然后通过屏幕着色器应用后处理效果
    //
    float quadVertices[] = {
        // ---- NDC 位置 --    -- 纹理坐标 --
        -1.0f, -1.0f,          0.0f, 0.0f,   // 左下
         1.0f, -1.0f,          1.0f, 0.0f,   // 右下
         1.0f,  1.0f,          1.0f, 1.0f,   // 右上

        -1.0f, -1.0f,          0.0f, 0.0f,   // 左下
         1.0f,  1.0f,          1.0f, 1.0f,   // 右上
        -1.0f,  1.0f,          0.0f, 1.0f    // 左上
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    // aPos (location = 0): vec2
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // aTexCoord (location = 1): vec2
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 10. 加载纹理 ==========
    unsigned int floorTex     = loadTexture("textures/marble.jpg", true);
    unsigned int containerTex = loadTexture("textures/container.jpg");

    // ========== 11. 纹理单元 ==========
    sceneShader.use();
    sceneShader.setInt("texture1", 0);
    sceneShader.setInt("texture2", 1);

    screenShader.use();
    screenShader.setInt("screenTexture", 0);

    // ========== 12. 创建帧缓冲（FBO） ==========
    //
    // ★ 帧缓冲 = 颜色附件（纹理） + 深度/模板附件（渲染缓冲对象 RBO）
    //
    // 步骤：
    //   1. 创建 FBO
    //   2. 创建颜色纹理附件
    //   3. 创建深度+模板 RBO
    //   4. 检查完整性
    //
    unsigned int fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // ---- 12a. 颜色附件：纹理 ----
    //
    // 将场景渲染到这个纹理上，后续采样它来做后处理
    //
    unsigned int texColorBuffer;
    glGenTextures(1, &texColorBuffer);
    glBindTexture(GL_TEXTURE_2D, texColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 附加纹理到帧缓冲的颜色附件 0
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

    // ---- 12b. 深度+模板附件：渲染缓冲对象（RBO） ----
    //
    // RBO 比纹理更适合深度/模板：GPU 可以直接写入，不需要采样时更高效
    //
    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // 附加 RBO 到帧缓冲
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // ---- 12c. 检查完整性 ----
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "✗ 帧缓冲不完整!" << std::endl;
    else
        std::cout << "✓ 帧缓冲创建成功 (" << SCR_WIDTH << "×" << SCR_HEIGHT << ")" << std::endl;

    // 恢复默认帧缓冲
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ========== 13. 场景物体位置 ==========
    struct { glm::vec3 pos; float rotSpeed; glm::vec3 rotAxis; } cubes[] = {
        { glm::vec3(-0.8f, 0.01f, -2.5f),  25.0f, glm::vec3(1.0f, 0.3f, 0.5f) },
        { glm::vec3( 0.8f, 0.01f, -2.5f), -30.0f, glm::vec3(0.2f, 1.0f, 0.4f) },
    };
    const int NUM_CUBES = 2;

    // ========== 14. 用户控制参数 ==========

    // ---- 后处理效果 ----
    // 0=Normal  1=Inversion  2=Grayscale  3=Sharpen  4=Blur  5=Edge
    //
    int   effect       = 0;       // 当前效果编号
    bool  flipVertical = false;   // 上下翻转纹理（用于对比 FBO 与默认帧缓冲的区别）

    const char* effectNames[] = {
        "0 - Normal (直通)",
        "1 - Inversion (反色)",
        "2 - Grayscale (灰度)",
        "3 - Sharpen (锐化)",
        "4 - Blur (高斯模糊)",
        "5 - Edge Detection (边缘检测)"
    };
    const char* effectHints[] = {
        "原始画面，不做处理",
        "所有颜色取反 (1 - color)，暗部变亮",
        "感知加权灰度: 0.2126R + 0.7152G + 0.0722B",
        "3×3 锐化卷积核，中心权重 9，周围 -1",
        "3×3 高斯模糊核，近似 7×7 效果",
        "3×3 拉普拉斯算子，中心 -8，检测亮度突变"
    };

    // ---- 摄像机 ----
    glm::vec3 camPos   = glm::vec3(0.0f, 1.5f, 5.0f);
    float     fov      = 50.0f;
    float     moveSpeed = 0.08f;

    bool  showDebugPanel = true;
    float clearColor[3]  = { 0.1f, 0.1f, 0.1f };

    // ========== 15. 清屏 ==========
    glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);

    std::cout << "\n============================================" << std::endl;
    std::cout << "  帧缓冲（Framebuffers）— 后处理效果" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  ESC           → 退出" << std::endl;
    std::cout << "  WASD / 箭头   → 前后左右移动" << std::endl;
    std::cout << "  右键拖动       → 旋转视角" << std::endl;
    std::cout << "  1 ~ 6         → 切换后处理效果" << std::endl;
    std::cout << "  F             → 翻转纹理（对比 FBO 效果）" << std::endl;
    std::cout << "  Tab           → 显示/隐藏调试面板" << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ========== 16. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        // ===== 16a. Delta time =====
        static float lastFrame = 0.0f;
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // ===== 16b. 输入处理 =====
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // ---- 1~6 键：切换后处理效果 ----
        static bool k1Pressed = false, k2Pressed = false, k3Pressed = false;
        static bool k4Pressed = false, k5Pressed = false, k6Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !k1Pressed) {
            k1Pressed = true; effect = 0;
            std::cout << "效果: " << effectNames[0] << std::endl;
        } else k1Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !k2Pressed) {
            k2Pressed = true; effect = 1;
            std::cout << "效果: " << effectNames[1] << std::endl;
        } else k2Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !k3Pressed) {
            k3Pressed = true; effect = 2;
            std::cout << "效果: " << effectNames[2] << std::endl;
        } else k3Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && !k4Pressed) {
            k4Pressed = true; effect = 3;
            std::cout << "效果: " << effectNames[3] << std::endl;
        } else k4Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS && !k5Pressed) {
            k5Pressed = true; effect = 4;
            std::cout << "效果: " << effectNames[4] << std::endl;
        } else k5Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS && !k6Pressed) {
            k6Pressed = true; effect = 5;
            std::cout << "效果: " << effectNames[5] << std::endl;
        } else k6Pressed = false;

        // ---- F 键：翻转纹理 ----
        static bool fPressed = false;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!fPressed) {
                flipVertical = !flipVertical;
                std::cout << (flipVertical ? "↕ 纹理翻转: 开启（模拟 FBO 上下颠倒）" : "↕ 纹理翻转: 关闭") << std::endl;
                fPressed = true;
            }
        } else fPressed = false;

        // ---- Tab 键：切换面板 ----
        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
            if (!tabPressed) { showDebugPanel = !showDebugPanel; tabPressed = true; }
        } else tabPressed = false;

        // ---- 摄像机移动 ----
        float velocity = moveSpeed * deltaTime * 60.0f;
        glm::vec3 front;
        front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front.y = sin(glm::radians(camPitch));
        front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += right * velocity;
        if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) camPos += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) camPos -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) camPos -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) camPos += right * velocity;

        // ===== 16c. MVP 矩阵 =====
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view       = glm::lookAt(camPos, camPos + front, glm::vec3(0.0f, 1.0f, 0.0f));

        // ============================================================
        // ★ 第一遍：渲染场景到自定义帧缓冲
        // ============================================================
        //
        // 绑定 FBO 后，所有绘制结果都写入 FBO 的颜色纹理
        //
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);  // 场景渲染需要深度测试

        sceneShader.use();
        sceneShader.setMat4("projection", projection);
        sceneShader.setMat4("view", view);

        // ---- 地板 ----
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, floorTex);

        glm::mat4 planeModel = glm::mat4(1.0f);
        sceneShader.setMat4("model", planeModel);
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ---- 旋转立方体 ----
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, containerTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, containerTex);

        glBindVertexArray(cubeVAO);
        for (int i = 0; i < NUM_CUBES; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubes[i].pos);
            model = glm::rotate(model, glm::radians(cubes[i].rotSpeed * currentFrame), cubes[i].rotAxis);
            sceneShader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // ============================================================
        // ★ 第二遍：渲染全屏四边形（后处理）
        // ============================================================
        //
        // 恢复到默认帧缓冲（屏幕），用屏幕着色器采样 FBO 纹理
        //
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);  // 全屏四边形不需要深度测试

        screenShader.use();
        screenShader.setInt("effect", effect);
        screenShader.setFloat("time", currentFrame);

        // 像素偏移量 = 1/纹理尺寸，用于 3×3 卷积核采样
        screenShader.setFloat("texOffsetX", 1.0f / SCR_WIDTH);
        screenShader.setFloat("texOffsetY", 1.0f / SCR_HEIGHT);

        // 翻转纹理：uv.y = 1 - uv.y
        // 通过修改 texOffsetY 的符号+偏移来模拟翻转
        // 更简洁的做法在 shader 中，这里通过传入标记由 C++ 控制
        if (flipVertical)
            screenShader.setFloat("texOffsetY", -1.0f / SCR_HEIGHT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texColorBuffer);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 恢复深度测试（下一帧场景渲染需要）
        glEnable(GL_DEPTH_TEST);

        // ============================================================
        // ImGui 面板
        // ============================================================
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (showDebugPanel) {
            ImGui::Begin("Debug Panel - Framebuffers");

            ImGui::Text("FPS: %.1f  (%.1f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();

            // ---- 后处理效果 ----
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "★ Post-Processing Effect");
            ImGui::Spacing();

            for (int i = 0; i < 6; i++) {
                char label[64];
                snprintf(label, sizeof(label), "%s##effect%d", effectNames[i], i);
                if (ImGui::RadioButton(label, effect == i)) {
                    effect = i;
                    std::cout << "效果: " << effectNames[i] << std::endl;
                }
            }
            ImGui::Separator();

            // 当前效果提示
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", effectHints[effect]);
            ImGui::Separator();

            // ---- 纹理翻转 ----
            ImGui::Checkbox("Flip Vertical (F)", &flipVertical);
            if (flipVertical) {
                ImGui::TextDisabled("FBO 纹理上下翻转，模拟 OpenGL 纹理坐标系与屏幕坐标系的差异");
            }
            ImGui::Separator();

            // ---- 摄像机 ----
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Camera");
            ImGui::SliderFloat("Move Speed", &moveSpeed, 0.01f, 0.5f, "%.2f");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.0f°");
            ImGui::Separator();

            ImGui::ColorEdit3("Clear Color", clearColor);
            glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
            ImGui::Separator();

            ImGui::TextDisabled(
                "1-6: Switch effect   |  F: Flip texture\n"
                "Tab: Toggle panel    |  ESC: Exit\n"
                "WASD/Arrows: Move    |  RClick+Drag: Look");

            ImGui::End();
        }

        // ImGui 渲染
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ========== 17. 清理 ==========
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteTextures(1, &floorTex);
    glDeleteTextures(1, &containerTex);
    glDeleteTextures(1, &texColorBuffer);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);
    glfwTerminate();
    return 0;
}


// ================================================================
// 主函数 — 章节选择器
// ================================================================

int main()
{
    std::cout << "▶ 运行最新章节：高级 OpenGL — 帧缓冲（Framebuffers）" << std::endl;
    return runFramebuffersDemo();
    return runBlendingDemo();
    return runStencilTestingDemo();
    return runDepthTestingDemo();
    return runModelLoadingDemo();
    return runMultipleLightsDemo();
    return runLightCastersDemo();
}

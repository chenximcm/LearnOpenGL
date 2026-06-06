#include <glad/glad.h>

// 告诉 GLFW 不要包含 OpenGL 头文件（由 GLAD 提供）
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include "shader.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// ========== 着色器源码 ==========

// 顶点着色器（GLSL）
const char* vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\n";

// 片段着色器（GLSL）
const char* fragmentShaderSource =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\n";

int main()
{
    // ========== 1. 初始化 GLFW ==========
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // ========== 2. 创建窗口 ==========
    GLFWwindow* window = glfwCreateWindow(800, 600, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // ========== 3. 初始化 GLAD ==========
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // ========== 4. 设置视口 ==========
    glViewport(0, 0, 800, 600);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // ========== 5. 编译着色器（使用 Shader 类）==========
    Shader shader(vertexShaderSource, fragmentShaderSource);

    // ========== 6. 顶点数据 ==========
    // 矩形的 4 个顶点（只需要 4 个，因为 EBO 复用顶点）
    float vertices[] = {
         0.5f,  0.5f, 0.0f,   // 右上角 (索引 0)
         0.5f, -0.5f, 0.0f,   // 右下角 (索引 1)
        -0.5f, -0.5f, 0.0f,   // 左下角 (索引 2)
        -0.5f,  0.5f, 0.0f    // 左上角 (索引 3)
    };

    // 索引数据 —— 定义两个三角形
    unsigned int indices[] = {
        0, 1, 3,   // 第一个三角形：右上 → 右下 → 左上
        1, 2, 3    // 第二个三角形：右下 → 左下 → 左上
    };

    // ========== 7. VAO & VBO & EBO ==========
    unsigned int VAO, VBO, EBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // 绑定 VAO → 绑定 VBO → 绑定 EBO → 设置顶点属性 → 解绑
    glBindVertexArray(VAO);

    // --- VBO：上传顶点数据 ---
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // --- EBO：上传索引数据 ---
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // --- 设置顶点属性 ---
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 解绑 VBO + VAO（EBO 由 VAO 记录，不手动解绑）
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ========== 8. 设置清屏颜色 ==========
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    // 线框模式（按需取消注释）
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // ========== 9. 渲染循环 ==========
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        // 使用 Shader 类 + EBO 索引绘制
        shader.use();
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    // ========== 10. 清理资源 ==========
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    glfwTerminate();
    return 0;
}

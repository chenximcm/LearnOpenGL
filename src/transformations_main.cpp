/**
 * ============================================================
 *  变换 + 双纹理（Transformations + Textures）
 * ============================================================
 *
 * 本章目标：同时展示双纹理混合 + 变换动画效果。
 *
 * 屏幕上绘制两个矩形，方便对比：
 *   左边：原始纹理（无变换，作为参考基准）
 *   右边：经过变换的纹理（缩放 + 旋转 + 平移动画）
 *   两者都使用双纹理混合（80% container + 20% awesomeface）
 *
 * 知识点覆盖：
 *   1. GLM 数学库 —— translate / rotate / scale
 *   2. 矩阵组合顺序 —— 先缩放 → 再旋转 → 最后平移（TRS）
 *   3. 双纹理混合 —— mix(sampler1, sampler2, 0.2)
 *   4. 纹理单元 —— glActiveTexture + glBindTexture
 *   5. uniform mat4 —— 顶点着色器中的变换矩阵
 *   6. glfwGetTime() —— 驱动连续动画
 *
 * 运行方式：在 VS 中设为启动文件（当前默认）
 */

// ============================== 头文件 ==============================
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

// GLM 数学库（仅头文件，无需链接）
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "../vendor/include/stb_image.h"


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


// ============================== 纹理加载工具 ==============================

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
    // 1~3：初始化 GLFW + GLAD + 视口
    // ====================================================================
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600,
        "LearnOpenGL - Transformations + Dual Textures", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "⨯ Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "⨯ Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, 800, 600);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // ====================================================================
    // 4：编译着色器
    // ====================================================================
    //
    // 着色器组合：
    //   transform.vert         —— 带 uniform mat4 transform
    //   texture_combined.frag  —— 双纹理混合（mix）
    Shader shader("shaders/transform.vert", "shaders/texture_combined.frag", true);

    // ====================================================================
    // 5：顶点数据（位置 + 纹理坐标）
    // ====================================================================
    float vertices[] = {
        // 位置                  // 纹理坐标
         0.5f,  0.5f, 0.0f,     1.0f, 1.0f,   // 右上
         0.5f, -0.5f, 0.0f,     1.0f, 0.0f,   // 右下
        -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,   // 左下
        -0.5f,  0.5f, 0.0f,     0.0f, 1.0f    // 左上
    };

    unsigned int indices[] = {
        0, 1, 3,   // 第一个三角形
        1, 2, 3    // 第二个三角形
    };

    // ====================================================================
    // 6：VAO / VBO / EBO
    // ====================================================================
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

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

    // ====================================================================
    // 7：加载两张纹理（双纹理混合）
    // ====================================================================
    unsigned int texture1 = loadTexture("textures/container.jpg");     // 砖墙
    unsigned int texture2 = loadTexture("textures/awesomeface.png");   // 笑脸

    // ====================================================================
    // 8：设置纹理单元
    // ====================================================================
    shader.use();
    shader.setInt("texture1", 0);   // container.jpg → 纹理单元 0
    shader.setInt("texture2", 1);   // awesomeface.png → 纹理单元 1

    // ====================================================================
    // 9：清屏颜色
    // ====================================================================
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    // ====================================================================
    // 10：渲染循环
    // ====================================================================
    //
    // ★ 每帧执行两个绘制调用 ★
    //
    //   第一个绘制（identity）：
    //     单位矩阵 → 矩形居中，显示双纹理混合效果
    //     作用：作为「变换前的基准参考」
    //
    //   第二个绘制（transformed）：
    //     缩放 + 旋转 + 平移 → 矩形动画
    //     作用：展示变换效果
    //
    //   这两个绘制共享同一个 VAO 和着色器，
    //   唯一的区别是 transform uniform 的值！
    //   这正是 uniform 的意义 —— 不改变顶点数据，
    //   通过矩阵变换来控制物体的位置/大小/朝向。
    //

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);
        glClear(GL_COLOR_BUFFER_BIT);

        // ----- 绑定两张纹理到各自的纹理单元 -----
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture2);

        // ----- 激活着色器 -----
        shader.use();

        // ============================================================
        // 绘制 1：基准参考（无变换，identity matrix）
        // ============================================================
        //
        // 把矩形缩小到 0.3 倍并放在左半边 (-0.35, 0)
        // 这样右半边可以放变换后的版本做对比
        //
        // 矩阵组合（从右到左读）：
        //   1. 缩小到 30%   → scale(0.3)
        //   2. 平移到左侧   → translate(-0.35, 0)
        //
        // 注意：这里只用到了平移 + 缩放，没有旋转，
        // 目的是保持「原始」形态供对比。
        //
        glm::mat4 identity = glm::mat4(1.0f);
        identity = glm::translate(identity, glm::vec3(-0.35f, 0.0f, 0.0f));
        identity = glm::scale(identity, glm::vec3(0.3f, 0.3f, 0.3f));

        glUniformMatrix4fv(
            glGetUniformLocation(shader.ID, "transform"),
            1, GL_FALSE, glm::value_ptr(identity));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // ============================================================
        // 绘制 2：变换效果（缩放 + 旋转 + 平移动画）
        // ============================================================
        //
        // 完整变换组合（从右到左读）：
        //   trans = T * R * S
        //
        //   1. 缩小到 40%        → scale(0.4)
        //   2. 随时间旋转         → rotate(time * 50°, Z)
        //   3. 平移到右侧 (0.35)  → translate(0.35, 0)
        //
        // 效果：矩形在右半边持续旋转，同时还有缩放
        //
        glm::mat4 trans = glm::mat4(1.0f);
        trans = glm::translate(trans, glm::vec3(0.35f, 0.0f, 0.0f));
        trans = glm::rotate(trans,
                            (float)glfwGetTime() * glm::radians(50.0f),
                            glm::vec3(0.0f, 0.0f, 1.0f));
        trans = glm::scale(trans, glm::vec3(0.4f, 0.4f, 0.4f));

        glUniformMatrix4fv(
            glGetUniformLocation(shader.ID, "transform"),
            1, GL_FALSE, glm::value_ptr(trans));

        // 同一个 VAO，同样的顶点，不同的变换矩阵
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // ----- 文字说明（用窗口标题实现）-----
        // 窗口标题已包含说明信息

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    // ====================================================================
    // 11：清理
    // ====================================================================
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture1);
    glDeleteTextures(1, &texture2);

    glfwTerminate();
    return 0;
}

#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

// GLM 用于 mat4 类型（setMat4 方法需要）
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

/**
 * Shader 类
 *
 * ============================================================
 *  1. 作用
 * ============================================================
 * 封装 OpenGL 着色器的整个生命周期：
 *   编译（Compile）→ 链接（Link）→ 使用（Use）→ 设置 Uniform
 *
 * 传统写法需要重复 9 步（CreateShader / ShaderSource / Compile /
 * GetShaderiv / CreateProgram / AttachShader / LinkProgram /
 * GetProgramiv / DeleteShader），封装后只需 1 行。
 *
 * ============================================================
 *  2. 用法
 * ============================================================
 *   // ===== 从源码字符串创建（推荐学习阶段） =====
 *   Shader shader(vertexShaderSource, fragmentShaderSource);
 *   shader.use();
 *   shader.setFloat("uOffset", 0.5f);
 *
 *   // ===== 从文件创建（推荐实际项目） =====
 *   Shader shader("shader.vs", "shader.fs", true);  // fromFile = true
 *
 * ============================================================
 *  3. 注意事项
 * ============================================================
 *  - 当前类没有析构函数，用完后需手动 glDeleteProgram(ID)
 *  - 暂时禁止了拷贝（看下面 Shader(const Shader&) = delete）
 *  - 可移动但不要求（忽略移动构造函数也没问题）
 */
class Shader
{
public:
    // ============================================================
    // 成员变量
    // ============================================================

    /// 着色器程序 ID —— 用于标识 OpenGL 中的一个着色器程序对象
    /// 创建成功后的返回值，后续所有操作（use、setUniform）都靠它
    unsigned int ID;

    // ============================================================
    // 构造函数
    // ============================================================

    /**
     * 构造一个 Shader 对象，自动完成编译 → 链接 → 错误检查
     *
     * @param vertexSource   顶点着色器源码，或文件路径（fromFile=true 时）
     * @param fragmentSource 片段着色器源码，或文件路径（fromFile=true 时）
     * @param fromFile       为 true 时，前两个参数作为文件路径读取
     *
     * 用法举例：
     *   Shader s1(src, frag);              // 从字符串编译
     *   Shader s2("a.vs", "b.fs", true);   // 从文件编译
     */
    Shader(const char* vertexSource, const char* fragmentSource, bool fromFile = false)
    {
        if (fromFile)
        {
            std::string vCode = loadFile(vertexSource);
            std::string fCode = loadFile(fragmentSource);
            compile(vCode.c_str(), fCode.c_str());
        }
        else
        {
            compile(vertexSource, fragmentSource);
        }
    }

    /**
     * 带几何着色器的构造函数
     *
     * @param vertexSource   顶点着色器源码或文件路径
     * @param geometrySource 几何着色器源码或文件路径
     * @param fragmentSource 片段着色器源码或文件路径
     * @param fromFile       为 true 时，前三参数作为文件路径读取
     */
    Shader(const char* vertexSource, const char* geometrySource,
           const char* fragmentSource, bool fromFile = false)
    {
        if (fromFile)
        {
            std::string vCode = loadFile(vertexSource);
            std::string gCode = loadFile(geometrySource);
            std::string fCode = loadFile(fragmentSource);
            compile(vCode.c_str(), gCode.c_str(), fCode.c_str());
        }
        else
        {
            compile(vertexSource, geometrySource, fragmentSource);
        }
    }

    /**
     * 禁止拷贝构造 —— 两个 Shader 对象不应共享同一个 OpenGL 程序 ID
     *
     * 如果允许拷贝，析构时一个对象释放程序，另一个就成空悬指针了。
     * 需要传递时请用引用或指针。
     */
    Shader(const Shader&) = delete;

    /**
     * 禁止拷贝赋值，原因同上
     */
    Shader& operator=(const Shader&) = delete;

    // ============================================================
    // 核心操作
    // ============================================================

    /**
     * 激活当前着色器程序
     *
     * 调用 glUseProgram(ID)，告诉 OpenGL 后续所有绘制操作
     * 都使用这个着色器程序来处理顶点和片段。
     *
     * 等价于旧写法：glUseProgram(shaderProgram);
     *
     * 渲染循环中的标准用法：
     *   shader.use();
     *   glBindVertexArray(VAO);
     *   glDrawElements(...);
     */
    void use() const
    {
        glUseProgram(ID);
    }

    // ============================================================
    // Uniform 设置 —— 从 CPU 向着色器传递数据
    // ============================================================

    /**
     * 设置 GLSL 中的 bool 类型 uniform
     *
     * 原理：GLSL 中没有真正的 bool 类型 uniform，
     * 底层用 int 表示（0 = false, 非 0 = true），
     * 所以这里调用 glUniform1i()。
     *
     * GLSL 对应：
     *   uniform bool uFlag;
     *
     * @param name  uniform 变量名（对着色器源码中的名字）
     * @param value true 或 false
     */
    void setBool(const std::string& name, bool value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }

    /**
     * 设置 GLSL 中的 int / sampler2D 类型 uniform
     *
     * 纹理采样器（sampler2D）也用这个设置纹理单元编号。
     *
     * GLSL 对应：
     *   uniform int uCount;
     *   uniform sampler2D uTexture;   // 纹理单元也用 setInt
     *
     * @param name  uniform 变量名
     * @param value 整数值
     */
    void setInt(const std::string& name, int value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }

    /**
     * 设置 GLSL 中的 float 类型 uniform
     *
     * 最常用的方法之一，用于传递时间、偏移量、透明度等标量值。
     *
     * GLSL 对应：
     *   uniform float uOffset;
     *   uniform float uTime;
     *
     * 使用示例 —— 在片段着色器中用 uniform 动画：
     *   // C++ 代码
     *   float time = glfwGetTime();
     *   shader.setFloat("uTime", time);
     *
     *   // GLSL 片段着色器
     *   uniform float uTime;
     *   void main() {
     *       FragColor = vec4(abs(sin(uTime)), 0.0, 0.0, 1.0);
     *   }
     *
     * @param name  uniform 变量名
     * @param value 浮点数值
     */
    void setFloat(const std::string& name, float value) const
    {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

    /**
     * 设置 GLSL 中的 vec3 类型 uniform
     *
     * 常用于传递 RGB 颜色、三维位置、方向向量等。
     *
     * GLSL 对应：
     *   uniform vec3 uColor;
     *   uniform vec3 uLightPos;
     *
     * @param name uniform 变量名
     * @param x    vec3 的第一个分量
     * @param y    vec3 的第二个分量
     * @param z    vec3 的第三个分量
     */
    void setVec3(const std::string& name, float x, float y, float z) const
    {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
    }

    /**
     * 设置 GLSL vec3（glm::vec3 重载）
     */
    void setVec3(const std::string& name, const glm::vec3& v) const
    {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), v.x, v.y, v.z);
    }

    /**
     * 设置 GLSL 中的 vec4 类型 uniform
     *
     * 常用于传递 RGBA 颜色（4 个分量）。
     *
     * GLSL 对应：
     *   uniform vec4 uColor;
     *
     * 使用示例 —— 让程序外部控制三角形颜色：
     *   shader.setVec4("uColor", 1.0f, 0.5f, 0.2f, 1.0f);   // 橙色
     *   shader.setVec4("uColor", 0.2f, 0.3f, 0.8f, 1.0f);   // 蓝色
     *
     * @param name uniform 变量名
     * @param x    R / x 分量
     * @param y    G / y 分量
     * @param z    B / z 分量
     * @param w    A / w 分量
     */
    void setVec4(const std::string& name, float x, float y, float z, float w) const
    {
        glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
    }

    /**
     * 设置 GLSL 中的 mat4 类型 uniform
     *
     * 这是坐标系统和后续章节中最常用的方法。
     * 用于传递 model / view / projection 变换矩阵。
     *
     * GLSL 对应：
     *   uniform mat4 model;
     *   uniform mat4 view;
     *   uniform mat4 projection;
     *
     * 使用示例 —— 在渲染循环中设置 MVP 矩阵：
     *   shader.setMat4("model", modelMatrix);
     *   shader.setMat4("view", viewMatrix);
     *   shader.setMat4("projection", projMatrix);
     *
     * 内部调用 glUniformMatrix4fv：
     *   - count = 1（一个矩阵）
     *   - transpose = GL_FALSE（GLM 使用列主序，OpenGL 也使用列主序，
     *     不需要转置。如果使用行主序库才需要转置）
     *   - value = glm::value_ptr(mat) 获取矩阵的裸指针（16 个 float）
     *
     * @param name uniform 变量名
     * @param mat  glm::mat4 矩阵（列主序）
     */
    void setMat4(const std::string& name, const glm::mat4& mat) const
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }

private:
    // ============================================================
    // 从文件读取着色器源码
    // ============================================================

    /**
     * 读取一个文本文件的全部内容
     *
     * 用 ifstream 打开文件，读到 stringstream 中再转成 string。
     * 如果文件不存在或打开失败，返回空字符串并输出错误。
     *
     * @param path 文件路径（相对于工作目录或绝对路径）
     * @return     文件内容的字符串
     */
    static std::string loadFile(const char* path)
    {
        std::string code;
        std::ifstream file;

        // 打开文件时如果失败会抛异常
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        try
        {
            file.open(path);

            // 把文件内容一次性读入 stringstream
            std::stringstream stream;
            stream << file.rdbuf();

            file.close();
            code = stream.str();
        }
        catch (std::ifstream::failure& e)
        {
            // 文件不存在 / 权限不足 / 读取中断 等情况会进到这里
            std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: "
                      << path << "\n  " << e.what() << std::endl;
        }

        return code;
    }

    // ============================================================
    // 编译和链接着色器
    // ============================================================

    /**
     * 核心编译逻辑
     *
     * 完整流程：
     *   1. 创建顶点着色器对象           glCreateShader(GL_VERTEX_SHADER)
     *   2. 附加源码到着色器对象          glShaderSource()
     *   3. 编译顶点着色器               glCompileShader()
     *   4. 检查编译是否成功             glGetShaderiv() + glGetShaderInfoLog()
     *   5. 重复 1-4 创建并编译片段着色器  GL_FRAGMENT_SHADER
     *   6. 创建着色器程序               glCreateProgram()
     *   7. 附着顶点和片段着色器          glAttachShader()
     *   8. 链接程序                    glLinkProgram()
     *   9. 检查链接是否成功             glGetProgramiv() + glGetProgramInfoLog()
     *  10. 删除着色器对象（不再需要）     glDeleteShader()
     *
     * @param vertexSource   顶点着色器源码（纯文本 GLSL）
     * @param fragmentSource 片段着色器源码（纯文本 GLSL）
     */
    void compile(const char* vertexSource, const char* fragmentSource)
    {
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);
        checkCompileErrors(vertexShader, "VERTEX");

        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);
        checkCompileErrors(fragmentShader, "FRAGMENT");

        ID = glCreateProgram();
        glAttachShader(ID, vertexShader);
        glAttachShader(ID, fragmentShader);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    /**
     * 带几何着色器的核心编译逻辑
     *
     * 相比两阶段（VS+FS）编译，多了几何着色器的创建和附着。
     * 几何着色器类型为 GL_GEOMETRY_SHADER。
     */
    void compile(const char* vertexSource, const char* geometrySource,
                 const char* fragmentSource)
    {
        // ===== 1. 编译顶点着色器 =====
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);
        checkCompileErrors(vertexShader, "VERTEX");

        // ===== 2. 编译几何着色器 =====
        unsigned int geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(geometryShader, 1, &geometrySource, NULL);
        glCompileShader(geometryShader);
        checkCompileErrors(geometryShader, "GEOMETRY");

        // ===== 3. 编译片段着色器 =====
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);
        checkCompileErrors(fragmentShader, "FRAGMENT");

        // ===== 4. 链接程序 =====
        ID = glCreateProgram();
        glAttachShader(ID, vertexShader);
        glAttachShader(ID, geometryShader);
        glAttachShader(ID, fragmentShader);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        // ===== 5. 清理着色器对象 =====
        glDeleteShader(vertexShader);
        glDeleteShader(geometryShader);
        glDeleteShader(fragmentShader);
    }

    // ============================================================
    // 检查编译/链接错误
    // ============================================================

    /**
     * 统一错误检查函数
     *
     * 既能检查着色器编译错误（type = "VERTEX" / "FRAGMENT"），
     * 也能检查程序链接错误（type = "PROGRAM"）。
     *
     * 编译错误常见原因：
     *   - #version 前面有空格或换行      → "invalid processing instruction"
     *   - 关键字拼写错误（void 写成 vo1d） → "syntax error"
     *   - 变量类型不匹配                 → "type mismatch"
     *   - 忘记写 main()                 → "undefined entry point"
     *
     * 链接错误常见原因：
     *   - 顶点着色器输出和片段着色器输入不匹配
     *   - 两个着色器的 #version 不一致
     *
     * @param shader 着色器对象 或 程序对象
     * @param type   类型标识（"VERTEX" / "FRAGMENT" / "PROGRAM"）
     */
    static void checkCompileErrors(unsigned int shader, const std::string& type)
    {
        int success;
        char infoLog[1024];

        if (type != "PROGRAM")
        {
            // ---------- 检查编译错误 ----------
            // glGetShaderiv: 查询着色器的编译状态
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

            if (!success)
            {
                // glGetShaderInfoLog: 获取编译错误详细信息
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::SHADER::" << type << "::COMPILATION_FAILED\n"
                          << infoLog << std::endl;
            }
        }
        else
        {
            // ---------- 检查链接错误 ----------
            glGetProgramiv(shader, GL_LINK_STATUS, &success);

            if (!success)
            {
                // glGetProgramInfoLog: 获取链接错误详细信息
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                          << infoLog << std::endl;
            }
        }
    }
};

#endif // SHADER_H

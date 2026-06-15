/**
 * mesh.h — 网格（Mesh）类
 *
 * 封装从 Assimp 转换来的单组网格数据，包含顶点、索引、纹理，
 * 并提供 VAO/VBO/EBO 的自动设置和绘制接口。
 *
 * 关联章节：模型加载 — Assimp
 */

#ifndef MESH_H
#define MESH_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>
#include <iostream>

#include "shader.h"


// ================================================================
// 顶点结构
// ================================================================
struct Vertex {
    glm::vec3 Position;     // 位置
    glm::vec3 Normal;       // 法线
    glm::vec2 TexCoords;    // 纹理坐标
    glm::vec3 Tangent;      // 切线（法线贴图用，预留）
    glm::vec3 Bitangent;    // 副切线（法线贴图用，预留）
};

// ================================================================
// 纹理结构
// ================================================================
struct Texture {
    unsigned int id;        // OpenGL 纹理 ID
    std::string type;       // 类型："texture_diffuse", "texture_specular", "texture_normal" 等
    std::string path;       // 文件路径（用于去重）
};

// ================================================================
// Mesh 类
// ================================================================
class Mesh {
public:
    // ---- 数据成员 ----
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    unsigned int VAO;

    // ---- 构造函数 ----
    Mesh(std::vector<Vertex> vertices,
         std::vector<unsigned int> indices,
         std::vector<Texture> textures)
        : vertices(vertices), indices(indices), textures(textures)
    {
        setupMesh();
    }

    // ---- 绘制接口 ----
    void Draw(Shader& shader) const
    {
        // 绑定纹理到对应的纹理单元
        unsigned int diffuseNr  = 1;
        unsigned int specularNr = 1;
        unsigned int normalNr   = 1;
        unsigned int heightNr   = 1;

        for (unsigned int i = 0; i < textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);   // 激活纹理单元

            // 构建 sampler 名称（如 "material.texture_diffuse1"）
            std::string number;
            std::string name = textures[i].type;
            if (name == "texture_diffuse")
                number = std::to_string(diffuseNr++);
            else if (name == "texture_specular")
                number = std::to_string(specularNr++);
            else if (name == "texture_normal")
                number = std::to_string(normalNr++);
            else if (name == "texture_height")
                number = std::to_string(heightNr++);

            shader.setInt(("material." + name + number).c_str(), (int)i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }
        glActiveTexture(GL_TEXTURE0);

        // 绘制网格
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

private:
    unsigned int VBO, EBO;

    // ---- 设置 VAO / VBO / EBO ----
    void setupMesh()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        // VBO：顶点数据
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                     &vertices[0], GL_STATIC_DRAW);

        // EBO：索引数据
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                     &indices[0], GL_STATIC_DRAW);

        // ---- 顶点属性指针 ----

        // 位置 (location = 0): 3 floats
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Position));

        // 法线 (location = 1): 3 floats
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Normal));

        // 纹理坐标 (location = 2): 2 floats
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, TexCoords));

        // 切线 (location = 3): 3 floats
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Tangent));

        // 副切线 (location = 4): 3 floats
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Bitangent));

        glBindVertexArray(0);
    }
};

#endif // MESH_H

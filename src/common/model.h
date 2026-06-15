/**
 * model.h — 模型（Model）类
 *
 * 使用 Assimp 库加载 3D 模型文件，递归遍历场景节点树，
 * 将其转换为 Mesh 对象数组，并自动加载关联的纹理贴图。
 *
 * 依赖：Assimp 库（v5.x）、mesh.h、shader.h
 *
 * 关联章节：模型加载 — Assimp
 */

#ifndef MODEL_H
#define MODEL_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <string>
#include <vector>
#include <iostream>
#include <map>

#include "mesh.h"
#include "../../vendor/include/stb_image.h"


// ================================================================
// Model 类
// ================================================================
class Model {
public:
    // ---- 构造函数 ----
    Model(const std::string& path)
    {
        loadModel(path);
    }

    // ---- 绘制所有网格 ----
    void Draw(Shader& shader) const
    {
        for (auto& mesh : meshes)
            mesh.Draw(shader);
    }

private:
    // ---- 数据成员 ----
    std::vector<Mesh> meshes;               // 模型包含的所有网格
    std::string directory;                  // 模型文件所在目录（纹理相对路径基准）
    std::vector<Texture> textures_loaded;   // 已加载纹理缓存（避免重复加载）

    // ---- 加载模型 ----
    void loadModel(const std::string& path)
    {
        Assimp::Importer importer;

        // 后处理选项：
        //   aiProcess_Triangulate   — 将所有图元转为三角形
        //   aiProcess_FlipUVs       — 翻转 Y 轴纹理坐标（OpenGL 坐标系）
        //   aiProcess_GenNormals    — 若无法线则自动生成
        //   aiProcess_OptimizeMeshes— 合并小网格，减少绘制调用
        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_GenNormals |
            aiProcess_OptimizeMeshes
        );

        // 检查加载是否有误
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            std::cerr << "✗ Assimp 加载失败: "
                      << importer.GetErrorString() << std::endl;
            return;
        }

        // 提取目录（用于后续纹理路径拼接）
        directory = path.substr(0, path.find_last_of('/'));
        if (directory.empty())
            directory = path.substr(0, path.find_last_of('\\'));

        std::cout << "✓ 模型加载成功: " << path << std::endl;
        std::cout << "  网格数量: " << scene->mNumMeshes << std::endl;

        // 递归处理根节点
        processNode(scene->mRootNode, scene);
    }

    // ---- 递归处理节点 ----
    void processNode(aiNode* node, const aiScene* scene)
    {
        // 处理当前节点的所有网格
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }

        // 递归处理子节点
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }
    }

    // ---- 将 aiMesh 转换为 Mesh 对象 ----
    Mesh processMesh(aiMesh* mesh, const aiScene* scene)
    {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        // ---- 1. 提取顶点数据 ----
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            // 位置
            vertex.Position = glm::vec3(
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            );
            // 法线
            if (mesh->HasNormals())
            {
                vertex.Normal = glm::vec3(
                    mesh->mNormals[i].x,
                    mesh->mNormals[i].y,
                    mesh->mNormals[i].z
                );
            }
            else
            {
                vertex.Normal = glm::vec3(0.0f);
            }

            // 纹理坐标（Assimp 支持最多 8 组，用第 0 组）
            if (mesh->mTextureCoords[0])
            {
                vertex.TexCoords = glm::vec2(
                    mesh->mTextureCoords[0][i].x,
                    mesh->mTextureCoords[0][i].y
                );
            }
            else
            {
                vertex.TexCoords = glm::vec2(0.0f);
            }

            // 切线 / 副切线（法线贴图用，预留）
            if (mesh->HasTangentsAndBitangents())
            {
                vertex.Tangent = glm::vec3(
                    mesh->mTangents[i].x,
                    mesh->mTangents[i].y,
                    mesh->mTangents[i].z
                );
                vertex.Bitangent = glm::vec3(
                    mesh->mBitangents[i].x,
                    mesh->mBitangents[i].y,
                    mesh->mBitangents[i].z
                );
            }
            else
            {
                vertex.Tangent   = glm::vec3(0.0f);
                vertex.Bitangent = glm::vec3(0.0f);
            }

            vertices.push_back(vertex);
        }

        // ---- 2. 提取索引数据 ----
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        // ---- 3. 提取材质 / 纹理 ----
        if (mesh->mMaterialIndex >= 0)
        {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            // 漫反射贴图
            std::vector<Texture> diffuseMaps = loadMaterialTextures(
                material, aiTextureType_DIFFUSE, "texture_diffuse"
            );
            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

            // 镜面光贴图
            std::vector<Texture> specularMaps = loadMaterialTextures(
                material, aiTextureType_SPECULAR, "texture_specular"
            );
            textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

            // 法线贴图（预留）
            std::vector<Texture> normalMaps = loadMaterialTextures(
                material, aiTextureType_NORMALS, "texture_normal"
            );
            textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

            // 高度贴图 / 视差贴图（预留）
            std::vector<Texture> heightMaps = loadMaterialTextures(
                material, aiTextureType_HEIGHT, "texture_height"
            );
            textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
        }

        return Mesh(vertices, indices, textures);
    }

    // ---- 加载指定类型的纹理 ----
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat,
                                               aiTextureType type,
                                               const std::string& typeName)
    {
        std::vector<Texture> textures;

        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);

            // 检查是否已加载（路径去重）
            bool skip = false;
            for (auto& loadedTex : textures_loaded)
            {
                if (std::strcmp(loadedTex.path.data(), str.C_Str()) == 0)
                {
                    textures.push_back(loadedTex);
                    skip = true;
                    break;
                }
            }

            if (!skip)
            {
                Texture texture;
                texture.id   = textureFromFile(str.C_Str(), directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);  // 加入缓存
            }
        }

        return textures;
    }

    // ---- 从文件加载纹理到 OpenGL ----
    unsigned int textureFromFile(const char* path, const std::string& directory)
    {
        std::string filename = directory + "/" + path;

        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(filename.c_str(), &width, &height,
                                        &nrChannels, 0);

        if (data)
        {
            GLenum format;
            if (nrChannels == 1)
                format = GL_RED;
            else if (nrChannels == 3)
                format = GL_RGB;
            else if (nrChannels == 4)
                format = GL_RGBA;
            else
                format = GL_RGB;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                         format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
        }
        else
        {
            std::cerr << "✗ 纹理加载失败: " << filename << std::endl;
            stbi_image_free(data);
        }

        return textureID;
    }
};

#endif // MODEL_H

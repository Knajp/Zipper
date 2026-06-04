#pragma once
#include "Nodes/Object.hpp"
#include "Utility/structs.hpp"
#include "Utility/RenderUtil.hpp"
#include "Graphics/Renderer.hpp"
#include <memory>
#include <variant>
#include <vector>
#include <vulkan/vulkan.h>
#include <tinyobjloader/tiny_obj_loader.h>
#include "Nodes/NodeInclude.hpp"

namespace ke
{
    namespace util
    {
    struct Mesh
    {
        util::Buffer indexBuffer;
        util::Buffer vertexBuffer;
        
        std::vector<ke::util::str::Vertex3P3C2T> mVertices;
        std::vector<uint32_t> mIndices;

        Mesh() = default;
        Mesh(const std::vector<util::str::Vertex3P3C2T>& vertices, const std::vector<uint32_t>& indices)
        {
            VkDeviceSize verticesSize = sizeof(vertices[0]) * vertices.size();
            VkDeviceSize indexSize = sizeof(indices[0]) * indices.size();

            ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();

            indexBuffer.setDevice(rend.getDevice());
            vertexBuffer.setDevice(rend.getDevice());

            rend.createVertexBuffer<util::str::Vertex3P3C2T>(vertices, vertexBuffer.buffer, vertexBuffer.bufferMemory);
            rend.createIndexBuffer(indices, indexBuffer.buffer, indexBuffer.bufferMemory);

            mVertices = std::move(vertices);
            mIndices = std::move(indices);
        }

        Mesh(const std::string objFilePath)
        {
            tinyobj::attrib_t attrib;
            std::vector<tinyobj::shape_t> shapes;
            std::vector<tinyobj::material_t> materials;
            std::string err, wrn;

            if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &wrn, &err, "src/Models/viking_room.obj"))
                throw std::runtime_error("Failed to load viking room model;");


            std::unordered_map<util::str::Vertex3P3C2T, uint32_t> uniqueVertices{};

            for(const auto& shape : shapes)
            {
                for(const auto& index : shape.mesh.indices)
                {
                    util::str::Vertex3P3C2T vertex{};

                    vertex.pos = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2],
                    };

                    vertex.uv = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
                    };

                    vertex.color = {1.0f, 1.0f, 1.0f};

                    if(uniqueVertices.count(vertex) == 0)
                    {
                        uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
                    }
                    mVertices.push_back(vertex);
                    mIndices.push_back(uniqueVertices[vertex]);
                }
            }

            VkDeviceSize verticesSize = sizeof(mVertices[0]) * mVertices.size();
            VkDeviceSize indexSize = sizeof(mIndices[0]) * mIndices.size();

            ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();

            indexBuffer.setDevice(rend.getDevice());
            vertexBuffer.setDevice(rend.getDevice());

            rend.createVertexBuffer<util::str::Vertex3P3C2T>(mVertices, vertexBuffer.buffer, vertexBuffer.bufferMemory);
            rend.createIndexBuffer(mIndices, indexBuffer.buffer, indexBuffer.bufferMemory);
        }
            
        Mesh(const Mesh& other) = delete;
        Mesh& operator=(const Mesh& other) = delete;

        Mesh(Mesh&& other) noexcept
            : indexBuffer(std::move(other.indexBuffer)),
              vertexBuffer(std::move(other.vertexBuffer)),
              mVertices(std::move(other.mVertices)),
              mIndices(std::move(other.mIndices))
        {
            other.mIndices.clear();
            other.mVertices.clear();
            other.indexBuffer.destroy();
            other.vertexBuffer.destroy();
        }
        Mesh& operator=(Mesh&& other) noexcept
        {
            if(this == &other) return *this;
            
            vertexBuffer = std::move(other.vertexBuffer);
            indexBuffer = std::move(other.indexBuffer);

            mVertices = std::move(other.mVertices);
            mIndices = std::move(other.mIndices);

            other.mIndices.clear();
            other.mVertices.clear();
            other.vertexBuffer.destroy();
            other.indexBuffer.destroy();

            return *this;
        }

    };
    }  
    class SceneManager
    {
    public:
        static SceneManager& getInstance()
        {
            static SceneManager instance;
            return instance;
        }


        void init(glm::ivec2 pos, glm::ivec2 extent, int windowHeight);
        void drawScene() const;

        float getSceneAspectRatio() const;
        void recreateViewport(glm::ivec2 pos, glm::ivec2 extent, int windowHeight);

        void terminate();

        const VkViewport& getViewport() const;
        const VkRect2D& getScissor() const;
        
        nodes::ISceneObject* const getSceneObject() const
        {
            if(!pSceneObject || pSceneObject == nullptr)
            {
                for(auto& child : mRootObject->getChildren())
                    if(child->getType() == nodes::ISceneObject::getStaticType())
                        pSceneObject = dynamic_cast<nodes::ISceneObject*>(child.get());
            }

            return pSceneObject;
        }
        nodes::RootObject* const getRootObject() const
        {
            nodes::RootObject* pRootObject = mRootObject.get();
            return pRootObject;
        }


    private:

        SceneManager() = default;

        VkViewport mSceneViewport;
        VkRect2D mSceneScissor;

        bool isFocused = true;
        
        std::unique_ptr<nodes::RootObject> mRootObject;
        mutable nodes::ISceneObject* pSceneObject = nullptr;
    };
}
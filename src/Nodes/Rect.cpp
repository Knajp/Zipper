#include "Rect.hpp"
#include "../SceneManager.hpp"

void ke::nodes::Rect2D::generateRectVertices()
{
    ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();

    mIndexBuffer.setDevice(rend.getDevice());
    mVertexBuffer.setDevice(rend.getDevice());

    mVertices = {
        {{position.x, position.y, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{position.x + extent.x, position.y, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{position.x, position.y + extent.y, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{position.x + extent.x, position.y + extent.y, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}  
    };
    mIndices = {
    0, 1, 2, 
    2, 1, 3  
    };

    rend.createVertexBuffer<ke::util::str::Vertex3P3C2T>(mVertices, mVertexBuffer.buffer, mVertexBuffer.bufferMemory);
    rend.createIndexBuffer(mIndices, mIndexBuffer.buffer, mIndexBuffer.bufferMemory);
}
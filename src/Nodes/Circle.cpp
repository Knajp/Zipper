#include "Circle.hpp"
#include <X11/Xdefs.h>
#include <cmath>

void ke::nodes::Circle::generateCircleVertices()
{
    mVertices.clear();
    mIndices.clear();

    ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();
    mVertexBuffer.setDevice(rend.getDevice());
    mIndexBuffer.setDevice(rend.getDevice());
    //center
    mVertices.push_back({{position.x, position.y, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});

    const int segments = 64;
    for(int i = 0; i <= segments; i++)
    {
        float alpha = 2.0f * M_PI * i / segments;

        float x = position.x + radius * cos(alpha);
        float y = position.y + radius * sin(alpha);

        mVertices.push_back({{x,y,0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});

        if(i == 0) continue;

        mIndices.push_back(0);
        mIndices.push_back(i);
        mIndices.push_back(i + 1);
    }

    rend.createVertexBuffer<ke::util::str::Vertex3P3C2T>(mVertices, mVertexBuffer.buffer, mVertexBuffer.bufferMemory);
    rend.createIndexBuffer(mIndices, mIndexBuffer.buffer, mIndexBuffer.bufferMemory);
}
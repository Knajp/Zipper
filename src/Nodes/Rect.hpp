#pragma once

#include "Object.hpp"

namespace ke::nodes
{
    class Rect2D : public Node2D
    {
    public:
        OBJECT_TYPE(RECT2D)
        Rect2D(glm::ivec2 pos, glm::ivec2 ext, std::string _name = "Rect2D")
            : Node2D(_name), position(pos), extent(ext) {}

        Rect2D(int _x, int _y, int _width, int _height, std::string _name = "Rect2D")
            : Node2D(_name), x(_x), y(_y), width(_width), height(_height) {}
        
        union
        {
            glm::ivec2 position;
            struct
            {
                int x;
                int y;
            };
        };

        union
        {
            glm::ivec2 extent;
            struct
            {
                int width;
                int height;
            };
        };
    };
}


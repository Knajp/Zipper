
#include "Object.hpp"
namespace ke::nodes
{
    class Circle : public Node2D
    {
    public:
        Circle(uint8_t depth, int rad, glm::ivec2 pos, std::string _name = "Circle")
            : Node2D(depth, _name), radius(rad), position(pos) {}
        
        Circle(uint8_t depth, int rad, int _x, int _y, std::string _name = "Circle")
            : Node2D(depth, _name), radius(rad), x(_x), y(_y){}

        int radius;
        union
        {
            glm::ivec2 position;

            struct
            {
                int x;
                int y;
            };
        };
        
    };
}
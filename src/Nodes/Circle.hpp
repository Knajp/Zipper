
#include "Object.hpp"
namespace ke::nodes
{
    /**
     * @brief Circle class, defined by a radius and center.
     * 
     */
    class Circle : public Node2D
    {
    public:
    /**
     * @brief Construct a new Circle object using vectors
     * 
     * @param rad Circle radius.
     * @param pos Circle center.
     * @param _name Object name.
     */
        Circle(int rad, glm::ivec2 pos, std::string _name = "Circle")
            : Node2D(_name), radius(rad), position(pos) {}
    /**
     * @brief Construct a new Circle object using scalars.
     * 
     * @param rad Cirlce radius.
     * @param _x Circle center-x.
     * @param _y Circle center-y.
     */
        Circle(int rad, int _x, int _y)
            : Node2D(), radius(rad), x(_x), y(_y) {}

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
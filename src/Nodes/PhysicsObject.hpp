#pragma once
#include "Object.hpp"

namespace ke
{
    namespace nodes
    {
        class PhysicsObject2D : public Node2D
        {
        public:
            PhysicsObject2D(uint8_t depth, std::string _name = "PhysicsObject2D")
                : Node2D(depth, _name) {}

        private:
            bool mHasGravity;
        };

        class PhysicsObject3D : public Node3D
        {
        public:
            PhysicsObject3D(uint8_t depth, std::string _name = "PhysicsObject3D")
                : Node3D(depth, _name) {}

        private:
            bool mHasGravity;
        };
    }
}
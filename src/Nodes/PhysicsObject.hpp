#pragma once
#include "Object.hpp"

namespace ke
{
    namespace nodes
    {
        class PhysicsObject2D : public Node2D
        {
        public:
            PhysicsObject2D(uint8_t depth)
                : Node2D(depth) {}

        private:
            bool mHasGravity;
        };

        class PhysicsObject3D : public Node3D
        {
        public:
            PhysicsObject3D(uint8_t depth)
                : Node3D(depth) {}

        private:
            bool mHasGravity;
        };
    }
}
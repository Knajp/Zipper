#pragma once
#include "Object.hpp"

namespace ke
{
    namespace nodes
    {
        class PhysicsObject2D : public Node2D
        {
        public:
            PhysicsObject2D() = default;

        private:
            bool mHasGravity;
        };

        class PhysicsObject3D : public Node3D
        {
        public:
            PhysicsObject3D() = default;

        private:
            bool mHasGravity;
        };
    }
}
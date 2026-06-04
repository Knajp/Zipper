#pragma once
#include "Object.hpp"

namespace ke
{
    namespace nodes
    {
        class PhysicsObject2D : public Node2D
        {
        public:
            PhysicsObject2D(std::string _name = "PhysicsObject2D")
                : Node2D(_name) {}

        private:
            bool mHasGravity;
        };

        class PhysicsObject3D : public Node3D
        {
        public:
            PhysicsObject3D(std::string _name = "PhysicsObject3D")
                : Node3D(_name) {}

        private:
            bool mHasGravity;
        };
    }
}
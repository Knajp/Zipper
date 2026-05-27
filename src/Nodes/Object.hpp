#pragma once 
#include <glm/glm.hpp>
#include <memory>
#include <span>

namespace ke
{
    namespace nodes
    {
        // DEFAULT SCENE OBJECT, USED FOR GLOBAL STORAGE
        class DefaultObject
        {
        public:
            uint64_t getObjectID() const {return mObjectID;}
            uint8_t getDepth() const {return mObjectDepth;}

            bool operator==(const DefaultObject& other) const
            {
                return mObjectID == other.mObjectID;
            }

            std::string name;
            
            template<typename T, typename... Args>
            T* createChild(Args&&... args)
            {
                auto child = std::make_unique<T>(std::forward<Args>(args)...);
                child->mParent = this;

                T* raw = child->get();
                mChildren.push_back(std::move(child));


                return raw;
            }    
            std::span<const std::unique_ptr<DefaultObject>> getChildren() const
            {
                return mChildren;
            }
            std::vector<DefaultObject*> gatherDescendants() const
            {
                std::vector<DefaultObject*> nodes;

                for(auto& child : mChildren)
                {
                    nodes.push_back(child.get()); 

                    if(child->mChildren.empty()) continue;

                    auto descendants = child->gatherDescendants();
                    nodes.insert(nodes.end(), descendants.begin(), descendants.end());
                }

                return nodes;
            }

        protected:
            DefaultObject(uint8_t depth)
            {
                static uint64_t id = 0;
                mObjectID = id++;

                mObjectDepth = depth;
            }
        private:
            uint64_t mObjectID;
            uint8_t mObjectDepth;

            std::vector<std::unique_ptr<DefaultObject>> mChildren;
            DefaultObject* mParent = nullptr;
        };


        enum class SysObjectType
        {
            ROOT, SCENE, MAX_ENUM
        };

        #define SYSTEMOBJECT_TYPE(type) static SysObjectType getStaticType() {return SysObjectType::type;} \
            SysObjectType getType() const override {return getStaticType();}

        // SYSTEM OBJECT, ONLY TO BE USED BY THE APP, NEVER BY THE USER
        class SystemObject : public DefaultObject
        {
        public:
            virtual ~SystemObject() = default;
            virtual SysObjectType getType() const = 0;
        protected:
            SystemObject(uint8_t depth)
                : DefaultObject(depth) {}

        private:
        };

        // ROOT OBJECT, THE ROOT OF THE SCENE TREE
        class RootObject : public SystemObject
        {
        public:
            static RootObject& getInstance() {static RootObject instance; return instance;}
            
            SYSTEMOBJECT_TYPE(ROOT)

        private:
            RootObject()
                : SystemObject(0) {}

            std::vector<std::unique_ptr<SystemObject>> mChildren;
        };

        template<typename T>
        class SceneObject : public SystemObject
        {
        public:
            static SceneObject& getInstance() {static SceneObject instance; return instance;}
            SYSTEMOBJECT_TYPE(SCENE)

        private:
            SceneObject(uint8_t depth)
                : SystemObject(depth) {}

            std::vector<std::unique_ptr<T>> mChildren;
        };

        class Node2D : public DefaultObject
        {
        public:
            Node2D(uint8_t depth)
                : DefaultObject(depth) {}

            void setPosition(glm::vec2 newPos) {mPosition = newPos;}
            glm::vec2 getPosition() const {return mPosition;}

            void setScale(float newScale) {mScale = newScale;}
            float getScale() const {return mScale;};


        private:

            
            glm::vec2 mPosition;
            float mScale;
        };

        class Node3D : public DefaultObject
        {
        public:
            Node3D(uint8_t depth)
                : DefaultObject(depth) {}
        
            void setPosition(glm::vec3 newPos) {mPosition = newPos;}
            glm::vec3 getPosition() const {return mPosition;}

            void setScale(float newScale) {mScale = newScale;}
            float getScale() const {return mScale;};
        private:
            glm::vec3 mPosition;
            float mScale;
        };
    }
}

namespace std
{
    template<>
    struct hash<ke::nodes::DefaultObject>
    {
        std::size_t operator()(const ke::nodes::DefaultObject& obj) const noexcept
        {
            return std::hash<uint64_t>{}(obj.getObjectID());
        }
    };
}
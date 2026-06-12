#pragma once 
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include "../Utility/RenderUtil.hpp"
#include "../Utility/structs.hpp"

namespace ke
{
    namespace nodes
    {
        /**
         * @brief Enum containing every Object Type.
         * 
         */
        enum class ObjectType
        {
            ROOT, SCENE, RECT2D, CIRCLE, PHYSICSOBJECT2D, PHYSICSOBJECT3D, MAX_ENUM
        };

        #define OBJECT_TYPE(type) static ObjectType getStaticType() {return ObjectType::type;} \
            ObjectType getType() const override {return getStaticType();}

        /**
         * @brief Default object, not to be instantiated by user.
         * @details Used for inheritance.
         * 
         */
        class DefaultObject
        {
        public:
        /**
         * @brief Get the ObjectID.
         * 
         * @return uint64_t 
         */
            uint64_t getObjectID() const {return mObjectID;}
        /**
         * @brief Get the Object Depth in the tree.
         * 
         * @return uint8_t 
         */
            uint8_t getDepth() const {return mObjectDepth;}

        /**
         * @brief Virtual destructor for DefaultObject.
         * @details Added to avoid undefined behaviors with std::unique_ptr<DefaultObject>
         * 
         */
            virtual ~DefaultObject() = default;
        /**
         * @brief Comparison operator between two objects.
         * @details Compares the ObjectIDs.
         * 
         * @param other The object to compare to.
         * @return true 
         * @return false 
         */
            bool operator==(const DefaultObject& other) const
            {
                return mObjectID == other.mObjectID;
            }

            std::string name;
            
        /**
         * @brief Create a Child object
         * 
         * @tparam T The type of the child object.
         * @tparam Args The types of child creation arguments.
         * @param args The child creation arguments
         * @return T* Pointer to the child
         */
            template<typename T, typename... Args>
            T* createChild(Args&&... args)
            {
                auto child = std::make_unique<T>(std::forward<Args>(args)...);
                child->mParent = this;
                child->setDepth(this->mObjectDepth + 1);

                T* raw = child.get();
                mChildren.push_back(std::move(child));


                return raw;
            }    
        /**
         * @brief Get the children (1st generation descendants) of the object.
         * 
         * @return std::span<const std::unique_ptr<DefaultObject>> The children.
         */
            std::span<const std::unique_ptr<DefaultObject>> getChildren() const
            {
                return mChildren;
            }
        /**
         * @brief Get all descendants of the object.
         * @details getChildren(), but recursive.
         * 
         * @return std::vector<DefaultObject*> The Descendants.
         */
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
        /**
         * @brief Get the object type, non-implemented.
         * @details To be implemented via macro in descendant classes.
         * 
         * @return ObjectType The object type.
         */
            virtual ObjectType getType() const = 0;

        protected:
        /**
         * @brief Construct a new DefaultObject.
         * @details Only accessible to descendants.
         * 
         * @param _name The name of the object, as it will appear in the Explorer.
         */
            DefaultObject(std::string _name = "Object")
                : name(_name)
            {
                static uint64_t id = 0;
                mObjectID = id++;

            }
        /**
         * @brief Set the object's depth.
         * @details Only to be used once, only accessible to descendants.
         * 
         * @param newDepth The target depth.
         */
            void setDepth(int newDepth) {mObjectDepth = newDepth;}
        private:
        /**
         * @brief The object ID.
         * @details Used for queries and comparisons.
         * 
         */
            uint64_t mObjectID;
        /**
         * @brief The object depth.
         * @details Used for explorer showcasing.
         */
            uint8_t mObjectDepth;
        
        /**
         * @brief The object's children.
         * 
         */
            std::vector<std::unique_ptr<DefaultObject>> mChildren;
        /**
         * @brief The pointer to the object's parent.
         * 
         */
            DefaultObject* mParent = nullptr;
        };

        /**
         * @brief System Object, root class for engine-operable objects.
         * @details Virtual class, descendants only to be used by engine.
         * 
         */
        class SystemObject : public DefaultObject
        {
        public:
        /**
         * @brief SystemObject's destructor, virtual.
         * 
         */
            virtual ~SystemObject() = default;
        protected:
        /**
         * @brief Create a new SystemObject, only to be used to instantiate derivative classes.
         * 
         * @param _name The object's name, used to instantiate DefaultObject
         */
            SystemObject(std::string _name = "SystemObject")
                :DefaultObject(_name) {}

        private:
        };

        /**
         * @brief Root Object, the base of the scene tree. Singleton.
         * 
         */
        class RootObject : public SystemObject
        {
        public:
        /**
         * @brief Get's the only instance of RootObject.
         * @details Part of the Singleton structure.
         * 
         * @return RootObject&  The reference to the object.
         */
            static RootObject& getInstance() {static RootObject instance; return instance;}
            
            OBJECT_TYPE(ROOT)

        private:
        /**
         * @brief Only to be called once, by the engine. Creates a new RootObject.
         * 
         * @param _name The Object's name, preset to "Root"
         */
            RootObject(std::string _name = "Root")
                : SystemObject(_name) {}
        
        /**
         * @brief The Object's children. Essentialy the entire scene.
         * 
         */
            std::vector<std::unique_ptr<SystemObject>> mChildren;
        };

        /**
         * @brief Base Scene Object.
         * @details To be split into 2D and 3D scene.
         * 
         */
        class ISceneObject : public SystemObject
        {
        public:
        /**
         * @brief Virtual IScene destructor.
         * 
         */
            virtual ~ISceneObject() = default;
            OBJECT_TYPE(SCENE)
        protected:
        /**
         * @brief Create a new IScene object.
         * 
         * @param _name The object's name, preset to "SceneObject"
         */
            ISceneObject(std::string _name = "SceneObject")
                : SystemObject(_name) {}
        };

        template<typename T>
        /**
         * @brief SceneObject, derivative of IScene, to be used by engine.
         * 
         */
        class SceneObject : public ISceneObject
        {
        public:
            /**
            * @brief Only to be called one by the engine. Not to be used by user. Creates a new SceneObject.
            * 
            * @param _name The object's name.
            */
            SceneObject(std::string _name = "Scene")
                : ISceneObject(_name) {}
            OBJECT_TYPE(SCENE)

        private:
        /**
         * @brief The object's children. Essentialy, what's visible on the screen.
         * 
         */
            std::vector<std::unique_ptr<T>> mChildren;
        };

        /**
         * @brief Basic 2D Node, to be used as base for 2D objects.
         * 
         */
        class Node2D : public DefaultObject
        {
        public:
        /**
         * @brief Create a new Node2D
         * 
         * @param _name The object's name, preset to "Node2D"
         */
            Node2D(std::string _name = "Node2D")
                : DefaultObject(_name) {}


        /**
         * @brief Sets the screen position of the object
         * 
         * @param newPos The target position
         */
            void setPosition(glm::vec2 newPos) {mPosition = newPos;}
        /**
         * @brief Gets the current screen position of the object.
         * 
         * @return glm::vec2 The current postion.
         */
            glm::vec2 getPosition() const {return mPosition;}

        /**
         * @brief Sets the scale of the object
         * 
         * @param newScale The target scale
         */
            void setScale(float newScale) {mScale = newScale;}
        /**
         * @brief Gets the current scale of the object
         * 
         * @return float The current scale
         */
            float getScale() const {return mScale;};


        private:

            /**
             * @brief The current screen position.
             * @details To be read and written to using getPosition and setPosition methods respectively.
             * 
             */
            glm::vec2 mPosition;
            /**
             * @brief The current scale.
             * @details To be read and written to using getScale and setScale methods respectively.
             * 
             */
            float mScale;

            /**
             * @brief The vertex buffer (Vulkan Resource)
             * 
             */
            ke::util::Buffer mVertexBuffer;
            /**
             * @brief The index buffer (Vulkan Resource)
             * 
             */
            ke::util::Buffer mIndexBuffer;

            /**
             * @brief The vertices
             * 
             */
            std::vector<ke::util::str::Vertex2P3C2T> mVertices;
            /**
             * @brief The indices
             * 
             */
            std::vector<uint16_t> mIndices;
        };

        /**
         * @brief Basic 3D Node, to be used as a base for other 3D objects.
         * 
         */
        class Node3D : public DefaultObject
        {
        public:
        /**
         * @brief Create a new Node3D, mostly used to instantiate derivatives.
         * 
         * @param _name The object's name, preset to "Node3D"
         */
            Node3D(std::string _name = "Node3D")
                : DefaultObject(_name) {}
        
        /**
         * @brief Set the object's screen position.
         * 
         * @param newPos The target position.
         */
            void setPosition(glm::vec3 newPos) {mPosition = newPos;}
        /**
         * @brief Get the object's screen position.
         * 
         * @return glm::vec3 The current position.
         */
            glm::vec3 getPosition() const {return mPosition;}

        /**
         * @brief Set the object's scale.
         * 
         * @param newScale The target scale.
         */
            void setScale(float newScale) {mScale = newScale;}
        /**
         * @brief Get the current scale of the object.
         * 
         * @return float The current scale.
         */
            float getScale() const {return mScale;};
        private:
        /**
         * @brief The current object position, to be read and written to by the getPosition and setPosition methods respectively.
         * 
         */
            glm::vec3 mPosition;
        /**
         * @brief The current object scale, to be read and written to by the getScale and setScale methods respectively.
         * 
         */
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
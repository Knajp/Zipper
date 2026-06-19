#include <string>
#include <pugixml/pugixml.hpp>
#include <iostream>
#include <variant>
#include <type_traits>
#include "Logger.hpp"
#include <glm/glm.hpp>
#include <functional>
#include "../Graphics/TextUtilities.hpp"
#include "../Nodes/Object.hpp"
#include "structs.hpp"
#include "../SceneManager.hpp"

namespace ke
{
    namespace gui
    {
        enum class VALTYPE {PERCENT, PIXEL, MAX_ENUM};
        struct XMLValue
        {
            XMLValue(std::string str)
            {
                if(str.ends_with("%"))
                {
                    type = VALTYPE::PERCENT;
                    value = std::stod(str.substr(0, str.size() - 1));
                }
                else if(str.ends_with("px"))
                {
                    type = VALTYPE::PIXEL;
                    value = std::stod(str.substr(0, str.size() - 2));
                }
                else std::cout << "Invalid attribute value type in XML file!";

                
            }

            int toPixels(double ref)
            {
                switch(type)
                {
                    case ke::gui::VALTYPE::PIXEL: return (int)value; break;
                    case ke::gui::VALTYPE::PERCENT: return (int)(value / 100.0 * ref); break;
                    case ke::gui::VALTYPE::MAX_ENUM: return 0; break;
                    default : 0;
                }
            }
            double value;
            VALTYPE type;
        };

        enum class UIType
        {
            TypeFrame, TypeButton, TypeInputField, TypeExplorer
        };
        #define GUI_TYPE(type) static UIType getStaticType() {return UIType::type;} \
            UIType getType() const override {return getStaticType();}

        class Element
        {
        public:
            Element() = default;
            Element(float _x, float _y, float _w, float _h, glm::vec3 _color)
                : x(_x), y(_y), w(_w), h(_h), color(_color) {}
            
            virtual ~Element() = default;

            float x, y;
            float w,h;

            glm::vec3 color;

            virtual UIType getType() const = 0;
        };


        class Frame : public Element
        {
        public:
            Frame() = default;
            Frame(float _x, float _y, float _w, float _h, glm::vec3 _color)
                : Element(_x,_y,_w,_h,_color){}

            GUI_TYPE(TypeFrame)
        };

        class Button : public Element
        {
        public:
            Button() = default;
            Button(float _x, float _y, float _w, float _h, glm::vec3 _color, std::string id)
                : Element(_x, _y, _w, _h, _color), buttonID(id){}

            std::function<void()> onClick;

            std::string buttonID;
            GUI_TYPE(TypeButton)
        };

        struct ExpEntry
        {
            ExpEntry(std::string _text, int _depth)
                : depth(_depth) {}
            Graphics::Text::TextInstance text;
            int depth;
            bool collapsed = false;
        };

        class Explorer : public Element
        {
        public:
            Explorer() = default;
            Explorer(float _x, float _y, float _w, float _h, glm::vec3 _color)
                : Element(_x, _y, _w, _h, _color)
            {
                Graphics::Renderer& rend = Graphics::Renderer::getInstance();
                mVertexBuffer.setDevice(rend.getDevice());
                mIndexBuffer.setDevice(rend.getDevice());
            }

            void Update()
            {
                updateExplorerEntries();
                reconstructExplorerVertices();
            }
            void DrawGeometry() const;
            //void DrawText() const;

            std::unordered_map<uint64_t, ExpEntry> mEntries;
            std::vector<uint64_t> mVisibleEntries;

            GUI_TYPE(TypeExplorer)

        private:
            void updateExplorerEntries();
            void reconstructExplorerVertices();

            std::vector<util::str::Vertex2P3C2T> mVertices;
            std::vector<uint32_t> mIndices;
            //std::vector<Graphics::Text::TextInstance> mTextInstances;

            util::Buffer mVertexBuffer;
            util::Buffer mIndexBuffer;

            VkViewport mViewport;
            VkRect2D mScissor;

            glm::mat4 mProjection;
        };

        enum InputType
        {
            TEXT, NUMBER, MAX_ENUM
        };
        struct InputValue
        {
            std::string val;
            glm::vec3 color;
        };
        class InputField : public Element
        {
        public: 
            InputField() = default;
            InputField(float _x, float _y, float _w, float _h, glm::vec3 _color, std::string _placeholder, InputType _type, std::string _name)
                : Element(_x, _y, _w, _h, _color), mPlaceholder(_placeholder), mType(_type), name(_name) {}

            void setValue(const std::string& newValue) {mValue = newValue;}
            InputValue getValue()
            {
                InputValue value{};
                if(mValue.empty() || mValue == "")
                    {value.val = mPlaceholder; value.color = glm::vec3(0.3f);}
                else
                    {value.val = mValue; value.color = glm::vec3(1.0f);}

                return value;
            }
            std::string getRawValue() const
            {
                return mValue;
            }

            void setTextInstance(ke::Graphics::Text::TextInstance& textInstance)
            {
                mTextInstance = std::move(textInstance);
            }
            GUI_TYPE(TypeInputField) 

            void DrawText() const;

            std::string name;

        private:
            std::string mPlaceholder;
            std::string mValue;   

            InputType mType;
            ke::Graphics::Text::TextInstance mTextInstance;
        };
    };
    namespace util
    {

        class XML
        {
        public:
            static XML& getInstance()
            {
                static XML instance;
                return instance;
            }

            void parseFile(std::string filepath, std::vector<std::unique_ptr<gui::Element>>& elements);
            void parseSceneFile(std::string filepath, glm::ivec2& offset, glm::ivec2& extent, int wx, int wy);
        private:
            XML() = default;

            util::Logger mLogger = util::Logger("XML parser logger");
        };
    }
}
#include "InterfaceManager.hpp"
#include "SceneManager.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>
#include <filesystem>

std::unordered_map<std::string, std::function<void()>> ke::gui::Component::mHandlers = 
{
    
};
ke::gui::Component::Component(std::string filepath)
    : mIndexBuffer(Graphics::Renderer::getInstance().getDevice()), mVertexBuffer(Graphics::Renderer::getInstance().getDevice())
{
    static util::XML parser = util::XML::getInstance();

    parser.parseFile(filepath, mFrames);
    
    int indexAdvance = 0;
    for(const auto& obj : mFrames)
    {
        float x = obj->x;
        x *= 2.0f;
        x -= 1.0f;
        float y = obj->y;
        y *= 2.0f;
        y -= 1.0f;
        float w = obj->w;
        w *= 2.0f;
        float h = obj->h;
        h *= 2.0f;

        mVertices.push_back({{x, y + h}, {obj->color.r, obj->color.g, obj->color.b}, {0.0f, 0.0f}});
        mVertices.push_back({{x, y}, {obj->color.r, obj->color.g, obj->color.b}, {0.0f, 0.0f}});
        mVertices.push_back({{x + w, y}, {obj->color.r, obj->color.g, obj->color.b}, {0.0f, 0.0f}});
        mVertices.push_back({{x + w, y + h}, {obj->color.r, obj->color.g, obj->color.b}, {0.0f, 0.0f}});

        mIndices.push_back(0 + indexAdvance);
        mIndices.push_back(1 + indexAdvance);
        mIndices.push_back(2 + indexAdvance);
        mIndices.push_back(0 + indexAdvance);
        mIndices.push_back(2 + indexAdvance);
        mIndices.push_back(3 + indexAdvance);

        indexAdvance += 4;

        if(obj->getType() == gui::InputField::getStaticType())
        {
            gui::InputField* field = dynamic_cast<gui::InputField*>(obj.get());

            int pixelX, pixelY, pixelH;
            
            ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();
            glm::ivec2 dimensions = rend.getSwapchainDimensions();

            pixelX = obj->x * dimensions.x + 5;
            pixelH = obj->h * dimensions.y;
            pixelY = obj->y * dimensions.y + pixelH * 0.3;


            InputValue value = field->getValue();
            ke::Graphics::Text::TextInstance textInstance = ke::Graphics::Text::TextInstance(value.val, "DejaVuSans", pixelX, pixelY, glm::vec4(value.color.r, value.color.g, value.color.b, 1.0f), pixelH);

            field->setTextInstance(textInstance);

            mInputFields.push_back(field);
        }

        if(obj->getType() == gui::Button::getStaticType())
        {
            gui::Button* button = dynamic_cast<gui::Button*>(obj.get());

            auto it = mHandlers.find(button->buttonID);

            if(it != mHandlers.end())
            button->onClick = it->second;

            mButtons.push_back(button);
        }
        if(obj->getType() == gui::Explorer::getStaticType())
            pExplorerElement = dynamic_cast<gui::Explorer*>(obj.get());
    }

    

    VkDeviceSize verticesSize = sizeof(mVertices[0]) * mVertices.size();
    VkDeviceSize indicesSize = sizeof(mIndices[0]) * mIndices.size();

    Graphics::Renderer& rend = Graphics::Renderer::getInstance();

    rend.createVertexBuffer(mVertices, mVertexBuffer.buffer, mVertexBuffer.bufferMemory);
    rend.createIndexBuffer(mIndices, mIndexBuffer.buffer, mIndexBuffer.bufferMemory);
}


ke::gui::Component::~Component()
{
}

bool isBetween(int point, int bLimit, int tLimit)
{
    return point > bLimit && point < tLimit;
}
bool ke::gui::Component::pollButtonClick(int mouseX, int mouseY, int windowX, int windowY) 
{
    for(auto& button : mButtons)
    {
        if(isBetween(mouseX, windowX * button->x, windowX * button->x + windowX * button->w) && isBetween(windowY - mouseY, windowY * button->y, windowY * button->y + windowY * button->h))
        {
            button->onClick();
            return true;
        }
    }
    return false;
}

ke::gui::Element *ke::gui::Component::pollInputFocus(int mouseX, int mouseY, int windowX, int windowY)
{
    for(auto& inputField : mInputFields)
    {
        if(isBetween(mouseX, windowX * inputField->x, windowX * inputField->x + windowX * inputField->w) && isBetween(windowY - mouseY, windowY * inputField->y, windowY * inputField->y + windowY * inputField->h))
        {
            return inputField;
        }
    }

    return nullptr;
}

ke::gui::Component::Component(Component&& other) noexcept
    : mFrames(std::move(other.mFrames)),
      mButtons(std::move(other.mButtons)),
      mIndexBuffer(std::move(other.mIndexBuffer)), 
      mVertexBuffer(std::move(other.mVertexBuffer)),
      mIndices(std::move(other.mIndices)),
      mVertices(std::move(other.mVertices)),
      pExplorerElement(other.pExplorerElement)
{
}

ke::gui::Component& ke::gui::Component::operator=(Component&& other) noexcept
{
    if (this != &other)
    {
        mFrames = std::move(other.mFrames);
        mButtons = std::move(other.mButtons);
        mIndexBuffer = std::move(other.mIndexBuffer);
        mVertexBuffer = std::move(other.mVertexBuffer);
        mIndices = std::move(other.mIndices);
        mVertices = std::move(other.mVertices);
        pExplorerElement = other.pExplorerElement;
    }
    return *this;
}
void ke::gui::Component::Draw(VkCommandBuffer commandBuffer)
{
    ke::Graphics::Renderer::getInstance().pickTextureIndex(-1);
    
    VkBuffer vertexBuffers[] = {mVertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mIndices.size()), 1, 0, 0, 0);

    if(pExplorerElement)
        pExplorerElement->DrawGeometry();
}

void ke::gui::Component::DrawText()
{
    for(auto field : mInputFields)
    {
        field->DrawText();
    }
}

bool ke::gui::Component::getInputFieldValue(const std::string &name, std::string &value)
{
    for(auto inputField : mInputFields)
    {
        if(inputField->name != name) continue;
        
        value = inputField->getRawValue();
        return true;
    }
    return false;
}

void ke::gui::UImanager::loadComponents(GLFWwindow* window)
{   
    const std::filesystem::path targetPath{"./src/UI/"};

    try
    {
        for(auto const& direntry : std::filesystem::directory_iterator{targetPath})
        {
            if(!std::filesystem::is_regular_file(direntry.path())) continue;

            if(direntry.path().filename().string() != "scene.xml")
                mComponents.push_back(std::make_unique<Component>(direntry.path().string()));
            else
            {
                sceneComponentFilepath = direntry.path().string();
                mSceneComponent = SceneComponent(sceneComponentFilepath, window);
            }
                
        }
    }catch(std::filesystem::filesystem_error const& err)
        {std::cout << "Error while reading directory: " << err.what() << std::endl;}
    
}

void ke::gui::UImanager::drawComponents(VkCommandBuffer commandBuffer)
{
    for(auto& comp : mComponents)
    {
        comp->Draw(commandBuffer);
    }
}

void ke::gui::UImanager::drawComponentTextLabels()
{
    for(auto& comp : mComponents)
    {
        comp->DrawText();
    }
}

void ke::gui::UImanager::unloadComponents()
{
    vkDeviceWaitIdle(Graphics::Renderer::getInstance().getDevice());
    mComponents.clear();
}

void ke::gui::UImanager::recreateSceneComponent(GLFWwindow *window)
{
    mSceneComponent = SceneComponent(sceneComponentFilepath, window);
}

glm::ivec2 ke::gui::UImanager::getSceneComponentPosition() const
{
    return mSceneComponent.pos;
}

glm::ivec2 ke::gui::UImanager::getSceneComponentExtent() const
{
    return mSceneComponent.extent;
}

void ke::gui::UImanager::processMouseClick(int mouseX, int mouseY, int windowX, int windowY)
{
    bool changedFocus = false;
    for(auto& component : mComponents)
    {
        component->pollButtonClick(mouseX, mouseY, windowX, windowY);
        if(!changedFocus)
        {
            pFocusedElement = component->pollInputFocus(mouseX, mouseY, windowX, windowY);
            if(pFocusedElement != nullptr) changedFocus = true;
        }
    }

    if(pFocusedElement == nullptr) mFocused = false;
    else mFocused = true;
}

void ke::gui::UImanager::processKeyboardInput(char codepoint)
{
    static ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();

    if(pFocusedElement != nullptr)
    {
        if(pFocusedElement->getType() == gui::InputField::getStaticType())
        {
            gui::InputField* field = dynamic_cast<gui::InputField*>(pFocusedElement);

            std::string currentFieldValue = field->getRawValue();

            std::string newValue = currentFieldValue + codepoint;
            field->setValue(newValue);

            glm::ivec2 screenDimensions = rend.getSwapchainDimensions();

            int pixelX, pixelY, pixelH;

            pixelX = field->x * screenDimensions.x + 5;
            pixelH = field->h * screenDimensions.y;
            pixelY = field->y * screenDimensions.y + pixelH * 0.3;

            gui::InputValue inputValue = field->getValue();
            ke::Graphics::Text::TextInstance instance(inputValue.val, "DejaVuSans", pixelX, pixelY, glm::vec4(inputValue.color.r, inputValue.color.g, inputValue.color.b, 1.0f), pixelH );

            field->setTextInstance(instance);
        }
    }
}

bool ke::gui::UImanager::processFunctionalKey(int key)
{
    ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();

    if(key == GLFW_KEY_BACKSPACE && pFocusedElement != nullptr)
    {
        if(pFocusedElement->getType() == gui::InputField::getStaticType())
        {
            gui::InputField* field = dynamic_cast<gui::InputField*>(pFocusedElement);

            std::string currentFieldValue = field->getRawValue();

            if(currentFieldValue.length() == 0) return true;
            
            currentFieldValue.pop_back();
            field->setValue(currentFieldValue);

            glm::ivec2 screenDimensions = rend.getSwapchainDimensions();

            int pixelX, pixelY, pixelH;

            pixelX = field->x * screenDimensions.x + 5;
            pixelH = field->h * screenDimensions.y;
            pixelY = field->y * screenDimensions.y + pixelH * 0.3;

            gui::InputValue inputValue = field->getValue();
            ke::Graphics::Text::TextInstance instance(inputValue.val, "DejaVuSans", pixelX, pixelY, glm::vec4(inputValue.color.r, inputValue.color.g, inputValue.color.b, 1.0f), pixelH );

            field->setTextInstance(instance);
            return true;
        }
    }

    return false;

}

std::string ke::gui::UImanager::getInputFieldValue(const std::string& name) const
{
    std::string value = "";
    for(const auto& comp : mComponents)
    {
        if(comp->getInputFieldValue(name, value)) break;
    }

    return value;
}

ke::gui::SceneComponent::SceneComponent(std::string filepath, GLFWwindow* window)
{
    static util::XML parser = util::XML::getInstance();

    int x, y;
    glfwGetFramebufferSize(window, &x, &y);

    parser.parseSceneFile(filepath, pos, extent, x, y);
}

ke::gui::SceneComponent::SceneComponent(SceneComponent &&other) noexcept
    :pos(other.pos), extent(other.extent)
{

}

ke::gui::SceneComponent &ke::gui::SceneComponent::operator=(SceneComponent &&other) noexcept
{
    if(this == &other) return *this;

    pos = other.pos;
    extent = other.extent;

    return *this;
}
    
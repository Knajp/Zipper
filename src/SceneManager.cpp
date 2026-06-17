#include "SceneManager.hpp"
#include "./Graphics/Texture.hpp"
#include "Nodes/Object.hpp"
#include <memory>

glm::ivec2 ke::SceneManager::normalizedToPixel(glm::vec2 norm)
{
    ke::SceneManager& sman = ke::SceneManager::getInstance();
    glm::ivec2 dims = {sman.getViewport().width, sman.getViewport().height};

    return {
    static_cast<int>((norm.x + 1.0f) * 0.5f * (dims.x - 1)),
    static_cast<int>((norm.y + 1.0f) * 0.5f * (dims.y - 1))
    };
}

glm::vec2 ke::SceneManager::pixelToNormalized(glm::ivec2 pix)
{
    ke::SceneManager& sman = ke::SceneManager::getInstance();
    glm::ivec2 dims = {sman.getViewport().width, sman.getViewport().height};

    return {
        2.0f * static_cast<float>(pix.x) / (dims.x - 1) - 1.0f,
        2.0f * static_cast<float>(pix.y) / (dims.y - 1) - 1.0f
    };
}
glm::vec2 ke::SceneManager::pixelSizeToNormalized(glm::ivec2 pixSize)
{
    ke::SceneManager& sman = ke::SceneManager::getInstance();
    glm::ivec2 dims = {sman.getViewport().width, sman.getViewport().height};

    return {
        2.0f * static_cast<float>(pixSize.x) / static_cast<float>(dims.x),
        2.0f * static_cast<float>(pixSize.y) / static_cast<float>(dims.y)
    };
}
glm::ivec2 ke::SceneManager::normalizedSizeToPixel(glm::vec2 normSize)
{
    ke::SceneManager& sman = ke::SceneManager::getInstance();
    glm::ivec2 dims = {sman.getViewport().width, sman.getViewport().height};

    return {
        static_cast<int>((normSize.x * static_cast<float>(dims.x)) / 2.0f),
        static_cast<int>((normSize.y * static_cast<float>(dims.y)) / 2.0f)
    };
}

void ke::SceneManager::init(glm::ivec2 pos, glm::ivec2 extent, int windowHeight)
{

    mSceneViewport.height = extent.y;
    mSceneViewport.width = extent.x;
    mSceneViewport.x = pos.x;
    mSceneViewport.y = windowHeight - (pos.y + extent.y);
    mSceneViewport.minDepth = 0.0f;
    mSceneViewport.maxDepth = 1.0f;

    mSceneScissor.extent = {static_cast<uint32_t>(extent.x), static_cast<uint32_t>(extent.y)};
    mSceneScissor.offset = {pos.x, windowHeight - (pos.y + extent.y)};

    pSceneObject = mRootObject.createChild<nodes::SceneObject<nodes::Node2D>>("Scene");
}

void ke::SceneManager::drawScene() const
{
    for(auto node : pSceneObject->gatherDescendants())
    {
        nodes::UserObject* userObject = dynamic_cast<nodes::UserObject*>(node);

        userObject->Draw();
    }
}

float ke::SceneManager::getSceneAspectRatio() const
{
    return mSceneViewport.width / mSceneViewport.height;
}

void ke::SceneManager::recreateViewport(glm::ivec2 pos, glm::ivec2 extent, int windowHeight)
{
    mSceneViewport.height = extent.y;
    mSceneViewport.width = extent.x;
    mSceneViewport.x = pos.x;
    mSceneViewport.y = windowHeight - (pos.y + extent.y);
    mSceneViewport.minDepth = 0.0f;
    mSceneViewport.maxDepth = 1.0f;

    mSceneScissor.extent = {static_cast<uint32_t>(extent.x), static_cast<uint32_t>(extent.y)};
    mSceneScissor.offset = {pos.x, windowHeight - (pos.y + extent.y)};   
}

void ke::SceneManager::terminate()
{
    //TODO
}

const VkViewport &ke::SceneManager::getViewport() const
{
    return mSceneViewport;
}

const VkRect2D &ke::SceneManager::getScissor() const
{
    return mSceneScissor;
}


#include "SceneManager.hpp"
#include "./Graphics/Texture.hpp"

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
}

void ke::SceneManager::drawScene() const
{
    //TODO
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


#include "XMLparser.hpp"
#include "RenderUtil.hpp"
#include "structs.hpp"
#include "../Graphics/Renderer.hpp"

#include <vector>

void ke::util::XML::parseFile(std::string filepath, std::vector<std::unique_ptr<gui::Element>>& elements)
{
    ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();
    glm::ivec2 dims = rend.getSwapchainDimensions();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());

    mLogger.info(result.description());

    pugi::xml_node root = doc.child("KEUIcomponent");
    
    ke::gui::XMLValue rootX(root.attribute("x").as_string());
    ke::gui::XMLValue rootY(root.attribute("y").as_string());
    ke::gui::XMLValue rootW(root.attribute("w").as_string());
    ke::gui::XMLValue rootH(root.attribute("h").as_string());

    int rootx = rootX.toPixels(dims.x);
    int rooty = rootY.toPixels(dims.y);
    int rootw = rootW.toPixels(dims.x);
    int rooth = rootH.toPixels(dims.y);

    for(pugi::xml_node frame : root.children("Frame"))
    {  
        ke::gui::XMLValue X(frame.attribute("x").as_string());
        ke::gui::XMLValue Y(frame.attribute("y").as_string());
        ke::gui::XMLValue W(frame.attribute("w").as_string());
        ke::gui::XMLValue H(frame.attribute("h").as_string());

        float x = rootx + X.toPixels(rootw);
        float y = rooty + Y.toPixels(rooth);
        float w = W.toPixels(rootw);
        float h = H.toPixels(rooth);

        const char* hexColor = frame.attribute("color").as_string();
        int r, g, b;
        std::sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);

        float rf = util::srgbToLinear(r / 255.0f);
        float gf = util::srgbToLinear(g / 255.0f);
        float bf = util::srgbToLinear(b / 255.0f);

        elements.push_back(std::make_unique<gui::Frame>(x,y,w,h, glm::vec3(rf,gf,bf)));
    }
    for(pugi::xml_node button : root.children("Button"))
    {

        ke::gui::XMLValue X(button.attribute("x").as_string());
        ke::gui::XMLValue Y(button.attribute("y").as_string());
        ke::gui::XMLValue W(button.attribute("w").as_string());
        ke::gui::XMLValue H(button.attribute("h").as_string());

        float x = rootx + X.toPixels(rootw);
        float y = rooty + Y.toPixels(rooth);
        float w = W.toPixels(rootw);
        float h = H.toPixels(rooth);

        const char* hexColor = button.attribute("color").as_string();
        int r, g, b;
        std::sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);

        float rf = util::srgbToLinear(r / 255.0f);
        float gf = util::srgbToLinear(g / 255.0f);
        float bf = util::srgbToLinear(b / 255.0f);

        const char* buttonID = button.attribute("id").as_string();
        elements.push_back(std::make_unique<gui::Button>(x,y,w,h, glm::vec3{rf,gf,bf}, buttonID));

    }
    for(pugi::xml_node inputField : root.children("input"))
    {
        ke::gui::XMLValue X(inputField.attribute("x").as_string());
        ke::gui::XMLValue Y(inputField.attribute("y").as_string());
        ke::gui::XMLValue W(inputField.attribute("w").as_string());
        ke::gui::XMLValue H(inputField.attribute("h").as_string());

        float x = rootx + X.toPixels(rootw);
        float y = rooty + Y.toPixels(rooth);
        float w = W.toPixels(rootw);
        float h = H.toPixels(rooth);

        const char* hexColor = inputField.attribute("color").as_string();
        int r, g, b;
        std::sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);

        float rf = util::srgbToLinear(r / 255.0f);
        float gf = util::srgbToLinear(g / 255.0f);
        float bf = util::srgbToLinear(b / 255.0f);
        
        const char* placeholder = inputField.attribute("placeholder").as_string();

        const char* inputName = inputField.attribute("name").as_string();

        const char* inputType = inputField.attribute("type").as_string();
        if(strcmp(inputType, "text") == 0)
            elements.push_back(std::make_unique<gui::InputField>(x,y,w,h, glm::vec3(rf, gf, bf), (std::string)placeholder, gui::InputType::TEXT, (std::string)inputName));
        else if(strcmp(inputType, "number") == 0)
            elements.push_back(std::make_unique<gui::InputField>(x,y,w,h, glm::vec3(rf, gf, bf), (std::string)placeholder, gui::InputType::NUMBER, (std::string)inputName));

            
    }
    for(pugi::xml_node explorer : root.children("Explorer"))
    {
        ke::gui::XMLValue X(explorer.attribute("x").as_string());
        ke::gui::XMLValue Y(explorer.attribute("y").as_string());
        ke::gui::XMLValue W(explorer.attribute("w").as_string());
        ke::gui::XMLValue H(explorer.attribute("h").as_string());

        float x = rootx + X.toPixels(rootw);
        float y = rooty + Y.toPixels(rooth);
        float w = W.toPixels(rootw);
        float h = H.toPixels(rooth);

        const char* hexColor = explorer.attribute("color").as_string();
        int r, g, b;
        std::sscanf(hexColor, "#%02x%02x%02x", &r, &g, &b);

        float rf = util::srgbToLinear(r / 255.0f);
        float gf = util::srgbToLinear(g / 255.0f);
        float bf = util::srgbToLinear(b / 255.0f);

        elements.push_back(std::make_unique<gui::Explorer>(x,y,w,h, glm::vec3(rf, gf, bf)));
    }
}

void ke::util::XML::parseSceneFile(std::string filepath, glm::ivec2 &offset, glm::ivec2 &extent, int wx, int wy)
{
    ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();
    glm::ivec2 dims = rend.getSwapchainDimensions();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());

    pugi::xml_node root = doc.child("KEUIcomponent");

    ke::gui::XMLValue rootX(root.attribute("x").as_string());
    ke::gui::XMLValue rootY(root.attribute("y").as_string());
    ke::gui::XMLValue rootW(root.attribute("w").as_string());
    ke::gui::XMLValue rootH(root.attribute("h").as_string());

    int rootx = rootX.toPixels(dims.x);
    int rooty = rootY.toPixels(dims.y);
    int rootw = rootW.toPixels(dims.x);
    int rooth = rootH.toPixels(dims.y);

    for(pugi::xml_node scene: root.children("SceneView"))
    {
        ke::gui::XMLValue X(scene.attribute("x").as_string());
        ke::gui::XMLValue Y(scene.attribute("y").as_string());
        ke::gui::XMLValue W(scene.attribute("w").as_string());
        ke::gui::XMLValue H(scene.attribute("h").as_string());

        float x = rootx + X.toPixels(rootw);
        float y = rooty + Y.toPixels(rooth);
        float w = W.toPixels(rootw);
        float h = H.toPixels(rooth); 
                
        offset = {x, y};
        extent = {w, h};
    }
}

void ke::gui::InputField::DrawText() const
{
    mTextInstance.Draw();
}

void ke::gui::Explorer::updateExplorerEntries()
{
    SceneManager& sceneManager = SceneManager::getInstance();
    const nodes::RootObject* rootObject = sceneManager.getRootObject();
    if(!rootObject) return;

    auto elements = rootObject->gatherDescendants();

    for(nodes::DefaultObject* el : elements)
    {
        std::cout << "Element found\n";
        mEntries.insert(std::make_pair<uint8_t, ExpEntry>(el->getObjectID(), ExpEntry(el->name, el->getDepth())));
        mVisibleEntries.push_back(el->getObjectID());
    }
}

void ke::gui::Explorer::reconstructExplorerVertices()
{
    mVertices.clear();
    mIndices.clear();
    mVertexBuffer.destroy();
    mIndexBuffer.destroy();

    mVertices.push_back({{0,y}, color, {0.0f, 0.0f}});
    mVertices.push_back({{0 + w, y}, color, {1.0f, 0.0f}});
    mVertices.push_back({{0, y + h}, color , {0.0f, 1.0f}});
    mVertices.push_back({{0 + w, y + h}, color, {1.0f, 1.0f}});

    mIndices = {
        0,1,2,
        2,3,1
    };

    uint8_t descend = 42;
    uint8_t inset = 20;

    uint64_t i = 0;
    uint32_t indexOffset = 4;
    for(uint64_t entryID : mVisibleEntries)
    {
        std::cout << "visible entry\n";
        ExpEntry& entry = mEntries.at(entryID);
        std::vector<util::str::Vertex2P3C2T> entryVertices{};

        float entryInset = entry.depth * inset;
        float entryDescend = i * descend;

        entryVertices.push_back(util::str::Vertex2P3C2T{{x + entryInset, h - (y + entryDescend)}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        entryVertices.push_back(util::str::Vertex2P3C2T{{x + entryInset + w, h - (y + entryDescend)}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        entryVertices.push_back(util::str::Vertex2P3C2T{{x + entryInset, h - (y + entryDescend + 40)}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        entryVertices.push_back(util::str::Vertex2P3C2T{{x + entryInset + w, h - (y + entryDescend + 40)}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});

        std::vector<uint32_t> entryIndices = 
        {
            0 + indexOffset,1 + indexOffset ,2 + indexOffset,
            2 + indexOffset,3 + indexOffset,1 + indexOffset
        };

        mVertices.insert(mVertices.end(), entryVertices.begin(), entryVertices.end());
        mIndices.insert(mIndices.end(), entryIndices.begin(), entryIndices.end());

        indexOffset += 4;
        i++;
    }

    mViewport.height = h;
    mViewport.width = w;
    mViewport.x = x;
    mViewport.y = y;
    mViewport.minDepth = 0.0f;
    mViewport.maxDepth = 1.0f;

    mScissor.extent = {static_cast<uint32_t>(w),static_cast<uint32_t>(h)};
    mScissor.offset = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
    
    Graphics::Renderer& rend = Graphics::Renderer::getInstance();
    rend.createVertexBuffer<util::str::Vertex2P3C2T>(mVertices, mVertexBuffer.buffer, mVertexBuffer.bufferMemory);
    rend.createIndexBuffer(mIndices, mIndexBuffer.buffer, mIndexBuffer.bufferMemory);
    
}

void ke::gui::Explorer::DrawGeometry() const
{
    ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();

    vkCmdSetViewport(rend.getCurrentCommandBuffer(), 0, 1, &mViewport);
    vkCmdSetScissor(rend.getCurrentCommandBuffer(), 0, 1, &mScissor);
    
    rend.drawBuffersIndexed(mVertexBuffer, mIndexBuffer, static_cast<uint32_t>(mIndices.size()));
}
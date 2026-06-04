#include "XMLparser.hpp"
#include "RenderUtil.hpp"
#include "structs.hpp"
#include <vector>

void ke::util::XML::parseFile(std::string filepath, std::vector<std::unique_ptr<gui::Element>>& elements)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());

    mLogger.info(result.description());

    pugi::xml_node root = doc.child("KEUIcomponent");
    
    float rootx = root.attribute("x").as_float() / 100.0f;
    float rooty = root.attribute("y").as_float() / 100.0f;
    float rootw = root.attribute("w").as_float() / 100.0f;
    float rooth = root.attribute("h").as_float() / 100.0f;

    for(pugi::xml_node frame : root.children("Frame"))
    {  
        float x = frame.attribute("x").as_float() / 100.0f;
        float y = frame.attribute("y").as_float() / 100.0f;
        float w = frame.attribute("w").as_float() / 100.0f;
        float h = frame.attribute("h").as_float() / 100.0f;

        x = rootx + rootw * x;
        y = rooty + rooth * y;
        w *= rootw;
        h *= rooth;


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

        float x = button.attribute("x").as_float() / 100.0f;
        float y = button.attribute("y").as_float() / 100.0f;
        float w = button.attribute("w").as_float() / 100.0f;
        float h = button.attribute("h").as_float() / 100.0f;

        x = rootx + rootw * x;
        y = rooty + rooth * y;
        w *= rootw;
        h *= rooth;

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
        float x = inputField.attribute("x").as_float() / 100.0f;
        float y = inputField.attribute("y").as_float() / 100.0f;
        float w = inputField.attribute("w").as_float() / 100.0f;
        float h = inputField.attribute("h").as_float() / 100.0f;

        x = rootx + rootw * x;
        y = rooty + rooth * y;
        w *= rootw;
        h *= rooth;

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
        float x = explorer.attribute(("x")).as_float() / 100.0f;
        float y = explorer.attribute(("y")).as_float() / 100.0f;
        float w = explorer.attribute(("w")).as_float() / 100.0f;
        float h = explorer.attribute(("h")).as_float() / 100.0f;

        x = rootx + rootw * x;
        y = rooty + rooth * y;
        w *= rootw;
        h *= rooth;

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
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());

    pugi::xml_node root = doc.child("KEUIcomponent");

    float rootx = root.attribute("x").as_float() / 100.0f;
    float rooty = root.attribute("y").as_float() / 100.0f;
    float rootw = root.attribute("w").as_float() / 100.0f;
    float rooth = root.attribute("h").as_float() / 100.0f;

    for(pugi::xml_node scene: root.children("SceneView"))
    {
        float x = scene.attribute("x").as_float() / 100.0f;
        float y = scene.attribute("y").as_float() / 100.0f;
        float w = scene.attribute("w").as_float() / 100.0f;
        float h = scene.attribute("h").as_float() / 100.0f;

        x = rootx + rootw * x;
        y = rooty + rooth * y;
        w *= rootw;
        h *= rooth;

        int rx = static_cast<int>(std::round(wx * x));
        int ry = static_cast<int>(std::round(wy * y));
        int rw = static_cast<int>(std::round(wx * w));
        int rh = static_cast<int>(std::round(wy * h));  
        
        offset = {rx, ry};
        extent = {rw, rh};
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
    if(rootObject == nullptr || !rootObject) return;

    auto elements = rootObject->gatherDescendants();

    for(nodes::DefaultObject* el : elements)
    {
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

    float descend = 0.1f;
    float inset = 0.2f;

    uint64_t i = 0;
    uint32_t indexOffset = 0;
    for(uint64_t entryID : mVisibleEntries)
    {
        ExpEntry& entry = mEntries.at(entryID);
        std::vector<util::str::Vertex2P3C2T> entryVertices{};

        float entryInset = entry.depth * inset;
        float entryDescend = i * descend;

        entryVertices.push_back(util::str::Vertex2P3C2T{{x, y}, color, {0.0f, 0.0f}});
        entryVertices.push_back(util::str::Vertex2P3C2T{{x + entryInset, y}, color, {0.0f, 0.0f}});
        entryVertices.push_back(util::str::Vertex2P3C2T{{x, y + entryDescend}, color, {0.0f, 0.0f}});
        entryVertices.push_back(util::str::Vertex2P3C2T{{x + entryInset, y + entryDescend}, color, {0.0f, 0.0f}});

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

    Graphics::Renderer& rend = Graphics::Renderer::getInstance();
    //rend.createVertexBuffer<util::str::Vertex2P3C2T>(mVertices, mVertexBuffer.buffer, mVertexBuffer.bufferMemory);
    //rend.createIndexBuffer(mIndices, mIndexBuffer.buffer, mIndexBuffer.bufferMemory);
    
}

void ke::gui::Explorer::DrawGeometry() const
{
    ke::Graphics::Renderer& rend = ke::Graphics::Renderer::getInstance();

    rend.drawBuffersIndexed(mVertexBuffer, mIndexBuffer, static_cast<uint32_t>(mIndices.size()));
}
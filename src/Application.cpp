#include "Application.hpp"
#include "Nodes/Object.hpp"
#include "Nodes/PhysicsObject.hpp"

ke::Core::Application& ke::Core::Application::getInstance()
{
    static ke::Core::Application instance;
    return instance;
}

void ke::Core::Application::Run()
{
    init();
    run();
    terminate();
}

void ke::Core::Application::init()
{
    mLogger.initLoggers();
    Graphics::Window::initGLFW();
    mLogger.info("Initialized GLFW.");
    mMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = glfwGetVideoMode(mMonitor);
    mWindow = std::make_unique<Graphics::Window>(videoMode->width, videoMode->height, "Knajp's Engine");
    mWindow->setApplicationEventCallback(onEvent);
    mLogger.info("Created window.");

    std::future<void> audioFuture = std::async(std::launch::async, [this]()
    {
        mAudioManager.init();
    });
    mLogger.trace("Requesting renderer init.");
    mRenderer.init(mWindow->getWindowHandle());
    mLogger.trace("Finished initializing renderer.");

    mLogger.trace("Requesting Text Utils init.");
    mTextUtils.init();
    mLogger.info("Finished loading text utils.");

    mLogger.trace("Requesting UI manager load");
    mUIManager.loadComponents(mWindow->getWindowHandle());
    mLogger.info("Finished loading UI manager.");

    mLogger.trace("Requesting Texture manager init.");
    mTextureManager.init();
    mLogger.info("Finished loading texture manager.");
    

    audioFuture.get();

    int width, height;
    glfwGetFramebufferSize(mWindow->getWindowHandle(), &width, &height);
    mSceneManager.init(mUIManager.getSceneComponentPosition(), mUIManager.getSceneComponentExtent(), height);

    mLogger.info("Finished application initialization.");
}

void ke::Core::Application::run()
{
    mLogger.trace("Proceeding to main loop.");
    
    uint16_t musicIndex = mAudioManager.createAudio("src/Sounds/music.mp3", AL_TRUE, 1.0f, 1.0f, "music");
    mAudioManager.PlayAudio(musicIndex);

    nodes::ISceneObject* sceneObject = mSceneManager.getSceneObject();
    sceneObject->createChild<nodes::Rect2D>(0, 0, 500, 500);

    while (!mWindow->shouldClose())
    {

        mWindow->calculateAspectRatio();
        mRenderer.readyCanvas(mWindow->getWindowHandle());
        VkCommandBuffer cb = mRenderer.getCurrentCommandBuffer();

        mRenderer.updateUIUniforms(mWindow->getAspectRatio());
        mRenderer.updateSceneUniforms(mSceneManager.getSceneAspectRatio());

        mRenderer.bindUIPipeline(cb);

        mUIManager.drawComponents(cb);

        mRenderer.endRenderPass();
        mRenderer.bindScenePipeline(cb, mSceneManager.getViewport(), mSceneManager.getScissor());

        mSceneManager.drawScene();

        mRenderer.endRenderPass();
        mRenderer.bindFontPipeline(cb);
        mRenderer.updateFontUniforms();

        mUIManager.drawComponentTextLabels();
        
        mRenderer.endRenderPass();
        mRenderer.finishDraw(mWindow->getWindowHandle());
        Graphics::Window::pollEvents();
    }
    
    mAudioManager.StopAudio(musicIndex);
    mLogger.info("Exit main loop.");
}

void ke::Core::Application::terminate()
{
    mLogger.trace("Proceeding to termination.");

    mLogger.info("Requesting scene termination.");
    mSceneManager.terminate();
    mLogger.info("Finished unloading scene.");
    
    mLogger.info("Requesting ui termination.");
    mUIManager.unloadComponents();
    mLogger.info("Finished unloading ui");

    mLogger.info("Requesting texture termination.");
    mTextureManager.terminate();
    mLogger.info("Finished unloading textures.");

    mLogger.info("Requesting audio termination.");
    mAudioManager.terminate();
    mLogger.info("Finished unloading audio.");
    
    mLogger.info("Requesting text termination.");
    mTextUtils.terminate();
    mLogger.info("Finished unloading text.");
    
    mLogger.trace("Requesting renderer termination.");
    mRenderer.terminate();
    mLogger.trace("Finished terminating renderer.");

    Graphics::Window::exitGLFW();
    mLogger.info("Exit GLFW.");
    
    mLogger.info("Goodbye.");
}


void ke::Core::Application::onEvent(Events::Event &ev)
{
    ke::Events::EventDispatcher dispatcher(ev);

    dispatcher.Dispatch<Events::KeyPressedEvent>([](Events::KeyPressedEvent& e)
    {
        Application& app = Application::getInstance();

        if(app.mUIManager.processFunctionalKey(e.getKeyCode()))
            return true;
        
        if(e.getKeyCode() == GLFW_KEY_Q && app.mWindow->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) // LCTRL + Q = QUIT
            {app.mWindow->quit(); return true;}
        
        return true;
    });
    dispatcher.Dispatch<Events::KeyReleasedEvent>([](Events::KeyReleasedEvent& e)
    {
        std::cout << "KEY RELEASED, keycode: " << e.getKeyCode() << "\n";
        return true;
    });
    dispatcher.Dispatch<Events::WindowResizedEvent>([](Events::WindowResizedEvent& e)
    {
        Application& app = Application::getInstance();

        std::cout << "WINDOW RESIZED, width: " << e.getWidth() << ", height: " << e.getHeight() << "\n";
        app.mRenderer.signalWindowResize();

        app.mUIManager.recreateSceneComponent(app.mWindow->getWindowHandle());

        int x,y;
        glfwGetFramebufferSize(app.mWindow->getWindowHandle(), &x, &y);
        app.mSceneManager.recreateViewport(app.mUIManager.getSceneComponentPosition(), app.mUIManager.getSceneComponentExtent(), y);

        return true;
    });
    dispatcher.Dispatch<Events::MouseButtonPressedEvent>([](Events::MouseButtonPressedEvent& e)
    {
        Application& app = Application::getInstance();

        GLFWwindow* window = app.mWindow->getWindowHandle();

        int wx, wy;
        glfwGetFramebufferSize(window, &wx, &wy);

        if(e.getButton() == GLFW_MOUSE_BUTTON_LEFT)
            app.mUIManager.processMouseClick(e.getMouseX(), e.getMouseY(), wx, wy);

        return true;
    });
    dispatcher.Dispatch<Events::TextInputEvent>([](Events::TextInputEvent& e)
    {
        
        Application& app = Application::getInstance();

        app.mUIManager.processKeyboardInput(e.getCodepoint());
        return true;
    });
}


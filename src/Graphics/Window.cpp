#include "Window.hpp"
#include "Renderer.hpp"
#include "../Events/event_pch.hpp"


static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto& self = *(ke::Graphics::Window*)glfwGetWindowUserPointer(window);
	
	if(action == GLFW_PRESS)
	{
		ke::Events::KeyPressedEvent event{key, false};
		self.event(event);
	}
	else if(action == GLFW_RELEASE)
	{
		ke::Events::KeyReleasedEvent event{key};
		self.event(event);
	}
	else std::cerr << "Invalid GLFW action value!\n";

}

static void charCallback(GLFWwindow* window, unsigned int codepoint)
{
	auto& self = *(ke::Graphics::Window*)glfwGetWindowUserPointer(window);

	ke::Events::TextInputEvent ev((char)codepoint);
	self.event(ev);
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto& self = *(ke::Graphics::Window*)glfwGetWindowUserPointer(window);

	ke::Events::WindowResizedEvent event{width, height};

	self.event(event);
}

static void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto& self = *(ke::Graphics::Window*)glfwGetWindowUserPointer(window);

	ke::Events::MouseMovedEvent event(xpos, ypos);

	self.event(event);
}
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	auto& self = *(ke::Graphics::Window*)glfwGetWindowUserPointer(window);
	
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	if(action == GLFW_PRESS)
	{
		ke::Events::MouseButtonPressedEvent event(button, xpos, ypos);
		self.event(event);
	}
	else if(action == GLFW_RELEASE)
	{
		ke::Events::MouseButtonReleasedEvent event(button, xpos, ypos);
		self.event(event);
	}
	else std::cerr << "Invalid GLFW action value!\n";
}

ke::Graphics::Window::Window(uint16_t width, uint16_t height, const char* title)
{
	pWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);
	glfwSetWindowUserPointer(pWindow, this);
	glfwSetFramebufferSizeCallback(pWindow, framebufferResizeCallback);
	glfwSetKeyCallback(pWindow, keyCallback);
	glfwSetCursorPosCallback(pWindow, mouseMoveCallback);
	glfwSetMouseButtonCallback(pWindow, mouseButtonCallback);
	glfwSetCharCallback(pWindow, charCallback);
}

ke::Graphics::Window::~Window()
{
	glfwDestroyWindow(pWindow);
}

void ke::Graphics::Window::initGLFW()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}
 
void ke::Graphics::Window::exitGLFW()
{
	glfwTerminate();
}

GLFWwindow* ke::Graphics::Window::getWindowHandle() const
{
	return pWindow;
}

void ke::Graphics::Window::pollEvents()
{
	glfwPollEvents();
}

bool ke::Graphics::Window::shouldClose() const
{
	return glfwWindowShouldClose(pWindow);
}

void ke::Graphics::Window::quit()
{
	glfwSetWindowShouldClose(pWindow, GLFW_TRUE);
}

void ke::Graphics::Window::setApplicationEventCallback(void (*func_ptr)(Events::Event &e))
{
	mEventCallback = func_ptr;
}

float ke::Graphics::Window::getAspectRatio() const
{
    return aspectRatio;
}

void ke::Graphics::Window::calculateAspectRatio()
{
	int width, height;
	glfwGetFramebufferSize(pWindow, &width, &height);

	aspectRatio = (float) width / (float) height; 
}

void ke::Graphics::Window::event(Events::Event &ev)
{
	mEventCallback(ev);
}

bool ke::Graphics::Window::isKeyPressed(int key) const
{
	return glfwGetKey(pWindow, key) == GLFW_PRESS;
}

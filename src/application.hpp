#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "serial.hpp"
#include "unique_resource.hpp"

struct Stateless
{
};

struct GLFW_deleter
{
    void operator()(Stateless);
};

struct Window_deleter
{
    void operator()(struct GLFWwindow *window);
};

struct ImGui_deleter
{
    void operator()(Stateless);
};

struct ImGui_glfw_deleter
{
    void operator()(Stateless);
};

struct ImGui_opengl_deleter
{
    void operator()(Stateless);
};

struct ImPlot_deleter
{
    void operator()(Stateless);
};

struct Application
{
    Unique_resource<Stateless, GLFW_deleter> glfw_context;
    Unique_resource<struct GLFWwindow *, Window_deleter> window;
    Unique_resource<Stateless, ImGui_deleter> imgui_context;
    Unique_resource<Stateless, ImGui_glfw_deleter> imgui_glfw_context;
    Unique_resource<Stateless, ImGui_opengl_deleter> imgui_opengl_context;
    Unique_resource<Stateless, ImPlot_deleter> implot_context;
    Serial_device serial_device;
};

[[nodiscard]] Application create_application();
void run_application(Application &application);

#endif
#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "unique_resource.hpp"

struct GLFW_deleter
{
    void operator()(bool);
};
struct Window_deleter
{
    void operator()(struct GLFWwindow *window);
};
struct ImGui_deleter
{
    void operator()(bool);
};
struct ImGui_glfw_deleter
{
    void operator()(bool);
};
struct ImGui_opengl_deleter
{
    void operator()(bool);
};
struct ImPlot_deleter
{
    void operator()(bool);
};

struct Application
{
    Unique_resource<bool, GLFW_deleter> glfw_context;
    Unique_resource<struct GLFWwindow *, Window_deleter> window;
    Unique_resource<bool, ImGui_deleter> imgui_context;
    Unique_resource<bool, ImGui_glfw_deleter> imgui_glfw_context;
    Unique_resource<bool, ImGui_opengl_deleter> imgui_opengl_context;
    Unique_resource<bool, ImPlot_deleter> implot_context;
};

[[nodiscard]] Application create_application();
void run_application(Application &application);

#endif
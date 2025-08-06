#include "application.hpp"
#include "unique_resource.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "implot.h"

#define GLFW_INCLUDE_GLCOREARB
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace
{

PFNGLENABLEPROC glEnable {};
PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback {};
PFNGLVIEWPORTPROC glViewport {};
PFNGLCLEARCOLORPROC glClearColor {};
PFNGLCLEARPROC glClear {};

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void load_gl_functions()
{
#define LOAD_GL_FUNCTION(name)                                                 \
    name = reinterpret_cast<decltype(name)>(glfwGetProcAddress(#name));        \
    assert(name != nullptr)

    LOAD_GL_FUNCTION(glEnable);
    LOAD_GL_FUNCTION(glDebugMessageCallback);
    LOAD_GL_FUNCTION(glViewport);
    LOAD_GL_FUNCTION(glClearColor);
    LOAD_GL_FUNCTION(glClear);

#undef LOAD_GL_FUNCTION
}

void APIENTRY gl_debug_callback([[maybe_unused]] GLenum source,
                                GLenum type,
                                [[maybe_unused]] GLuint id,
                                GLenum severity,
                                [[maybe_unused]] GLsizei length,
                                const GLchar *message,
                                [[maybe_unused]] const void *user_param)
{
    if (type == GL_DEBUG_TYPE_OTHER ||
        severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        return;
    }
    std::cerr << message << '\n';
}

} // namespace

void GLFW_deleter::operator()(bool)
{
    glfwTerminate();
}

void Window_deleter::operator()(GLFWwindow *window)
{
    glfwDestroyWindow(window);
}

void ImGui_deleter::operator()(bool)
{
    ImGui::DestroyContext();
}

void ImGui_glfw_deleter::operator()(bool)
{
    ImGui_ImplGlfw_Shutdown();
}

void ImGui_opengl_deleter::operator()(bool)
{
    ImGui_ImplOpenGL3_Shutdown();
}

void ImPlot_deleter::operator()(bool)
{
    ImPlot::DestroyContext();
}

Application create_application()
{
    Application app {};

    glfwSetErrorCallback(&glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    app.glfw_context.reset(true);

    constexpr auto glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);

    const auto main_scale =
        ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    auto *const window_ptr =
        glfwCreateWindow(static_cast<int>(1280 * main_scale),
                         static_cast<int>(800 * main_scale),
                         "Ground Control Station",
                         nullptr,
                         nullptr);
    if (window_ptr == nullptr)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }
    app.window.reset(window_ptr);

    glfwMakeContextCurrent(app.window.get());
    glfwSwapInterval(1);

    load_gl_functions();
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&gl_debug_callback, nullptr);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    app.imgui_context.reset(true);

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    ImGui::StyleColorsDark();

    auto &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    if (!ImGui_ImplGlfw_InitForOpenGL(app.window.get(), true))
    {
        throw std::runtime_error("ImGui: failed to initialize GLFW backend");
    }
    app.imgui_glfw_context.reset(true);

    if (!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        throw std::runtime_error("ImGui: failed to initialize OpenGL backend");
    }
    app.imgui_opengl_context.reset(true);

    ImPlot::CreateContext();
    app.implot_context.reset(true);

    return app;
}

void run_application(Application &application)
{
    while (!glfwWindowShouldClose(application.window.get()))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport();
        ImGui::ShowDemoWindow();
        ImPlot::ShowDemoWindow();

        ImGui::Render();
        int display_width {};
        int display_height {};
        glfwGetFramebufferSize(
            application.window.get(), &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);
        glClearColor(0.45f, 0.55f, 0.60f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(application.window.get());
    }
}
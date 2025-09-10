#include "application.hpp"
#include "serial.hpp"
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

void make_ui(Serial_port &serial_port)
{
    ImGui::DockSpaceOverViewport();
    ImPlot::ShowDemoWindow();

    if (ImGui::Begin("Serial"))
    {
        // FIXME: no default device, but save it to some config file
        static char device[512] {"/dev/ttyACM0"};
        // FIXME: if it's not too restrictive we should list potential devices
        ImGui::InputText("Device", device, sizeof(device));

        if (ImGui::Button("Open"))
        {
            serial_port.open(device, 115200);
        }
        ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            serial_port.close();
        }
    }
    ImGui::End();
}

struct Stateless
{
};

struct GLFW_deleter
{
    void operator()(Stateless)
    {
        glfwTerminate();
    }
};

struct Window_deleter
{
    void operator()(struct GLFWwindow *window)
    {
        glfwDestroyWindow(window);
    }
};

struct ImGui_deleter
{
    void operator()(Stateless)
    {
        ImGui::DestroyContext();
    }
};

struct ImGui_glfw_deleter
{
    void operator()(Stateless)
    {
        ImGui_ImplGlfw_Shutdown();
    }
};

struct ImGui_opengl_deleter
{
    void operator()(Stateless)
    {
        ImGui_ImplOpenGL3_Shutdown();
    }
};

struct ImPlot_deleter
{
    void operator()(Stateless)
    {
        ImPlot::DestroyContext();
    }
};

} // namespace

void run_application()
{
    glfwSetErrorCallback(&glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    const Unique_resource<Stateless, GLFW_deleter> glfw_context(Stateless {});

    constexpr auto glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
#endif

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
    const Unique_resource<GLFWwindow *, Window_deleter> window(window_ptr);

    glfwMakeContextCurrent(window.get());
    glfwSwapInterval(1);

    load_gl_functions();
#ifndef NDEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&gl_debug_callback, nullptr);
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    const Unique_resource<Stateless, ImGui_deleter> imgui_context(Stateless {});

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    ImGui::StyleColorsDark();

    auto &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    if (!ImGui_ImplGlfw_InitForOpenGL(window.get(), true))
    {
        throw std::runtime_error("ImGui: failed to initialize GLFW backend");
    }
    const Unique_resource<Stateless, ImGui_glfw_deleter> imgui_glfw_context(
        Stateless {});

    if (!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        throw std::runtime_error("ImGui: failed to initialize OpenGL backend");
    }
    const Unique_resource<Stateless, ImGui_opengl_deleter> imgui_opengl_context(
        Stateless {});

    ImPlot::CreateContext();
    const Unique_resource<Stateless, ImPlot_deleter> implot_context(
        Stateless {});

    Serial_port serial_port {};

    while (!glfwWindowShouldClose(window.get()))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        make_ui(serial_port);

        ImGui::Render();
        int display_width {};
        int display_height {};
        glfwGetFramebufferSize(window.get(), &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window.get());
    }
}

#include "utils.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GL_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_GLCOREARB
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void APIENTRY gl_debug_callback([[maybe_unused]] GLenum source,
                                [[maybe_unused]] GLenum type,
                                [[maybe_unused]] GLuint id,
                                [[maybe_unused]] GLenum severity,
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

void run()
{
    glfwSetErrorCallback(&glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    const Unique_resource glfw_context(true, [](bool) { glfwTerminate(); });

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
    const Unique_resource window(
        window_ptr, [](GLFWwindow *win) { glfwDestroyWindow(win); });

    glfwMakeContextCurrent(window.get());
    glfwSwapInterval(1);

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&gl_debug_callback, nullptr);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    const Unique_resource imgui_context(true,
                                        [](bool) { ImGui::DestroyContext(); });

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    auto &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    if (!ImGui_ImplGlfw_InitForOpenGL(window.get(), true))
    {
        throw std::runtime_error("ImGui: failed to initialize GLFW backend");
    }
    const Unique_resource imgui_glfw_context(
        true, [](bool) { ImGui_ImplGlfw_Shutdown(); });

    if (!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        throw std::runtime_error("ImGui: failed to initialize OpenGL backend");
    }
    const Unique_resource imgui_opengl_context(
        true, [](bool) { ImGui_ImplOpenGL3_Shutdown(); });

    while (!glfwWindowShouldClose(window.get()))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        ImGui::Render();
        int display_width {};
        int display_height {};
        glfwGetFramebufferSize(window.get(), &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);
        glClearColor(0.45f, 0.55f, 0.60f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window.get());
    }
}

} // namespace

int main()
{
    try
    {
        run();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        std::cout.flush();
        std::cerr << "Exception thrown: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cout.flush();
        std::cerr << "Unknown exception thrown\n";
        return EXIT_FAILURE;
    }
}

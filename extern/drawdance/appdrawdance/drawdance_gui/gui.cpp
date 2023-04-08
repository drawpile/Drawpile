/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "gui.h"
#include <SDL.h>
#include <gles2_inc.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>


extern "C" bool DP_imgui_init(void)
{
    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext()) {
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.ConfigWindowsResizeFromEdges = true;
        // FIXME: Hacked-up way to set the initial docking configuration.
        // Replace this with using the DockBuilder.
        ImGui::LoadIniSettingsFromMemory(
            "[Window][Chat##chat_dock]\n"
            "DockId=0x00000003,0\n"
            "[Docking][Data]\n"
            "DockSpace   ID=0x00000001 Window=0xDE2FB0B1 Pos=0,0 Size=1,1 "
            "Split=Y\n"
            "  DockNode  ID=0x00000002 Parent=0x00000001 SizeRef=1,1 "
            "CentralNode=1\n"
            "  DockNode  ID=0x00000003 Parent=0x00000001 SizeRef=1,300\n");
        return true;
    }
    else {
        return false;
    }
}
extern "C" void DP_imgui_quit(void)
{
    ImGui::DestroyContext();
}

extern "C" bool DP_imgui_init_sdl(SDL_Window *window, SDL_GLContext gl_context)
{
    return ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
}

extern "C" void DP_imgui_quit_sdl(void)
{
    ImGui_ImplSDL2_Shutdown();
}

extern "C" bool DP_imgui_init_gl(void)
{
    return ImGui_ImplOpenGL3_Init("#version 100");
}

extern "C" void DP_imgui_quit_gl(void)
{
    ImGui_ImplOpenGL3_Shutdown();
}

extern "C" void DP_imgui_handle_event(SDL_Event *event)
{
    ImGui_ImplSDL2_ProcessEvent(event);
}

extern "C" void DP_imgui_new_frame_gl(void)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
}

extern "C" void DP_imgui_new_frame(void)
{
    ImGui::NewFrame();
}

extern "C" void DP_imgui_render(SDL_Window *window, SDL_GLContext gl_context)
{
    ImGui::Render();
    ImGuiIO &io = ImGui::GetIO();
    glViewport(0, 0, static_cast<int>(io.DisplaySize.x),
               static_cast<int>(io.DisplaySize.y));
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(window, gl_context);
    }
}

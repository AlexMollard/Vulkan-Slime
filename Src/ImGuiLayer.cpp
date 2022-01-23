//
// Created by alexm on 19/01/2022.
//

#include "ImGuiLayer.h"

void ImguiLayer::init() {

    ImGui::StyleColorsDark();
    SetDarkThemeColors();

    ImGuiStyle &style = ImGui::GetStyle();
}

void ImguiLayer::draw(SDL_Window *window) {
    //imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    ImGui::Begin("Scene Hierarchy", nullptr);
    ImGui::TextColored(ImVec4{0.2f, 0.85f, 0.2f, 1.0f}, "This window will be the Hierarchy");
    ImGui::End();

}

void ImguiLayer::SetDarkThemeColors() {
    auto &colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg] = ImVec4{0.025f, 0.025f, 0.025f, 1.0f};

    // Headers
    colors[ImGuiCol_Header] = ImVec4{0.05f, 0.05f, 0.05f, 1.0f};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.075f, 0.075f, 0.075f, 1.0f};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.025f, 0.025f, 0.025f, 1.0f};

    // Buttons
    colors[ImGuiCol_Button] = ImVec4{0.05f, 0.05f, 0.05f, 1.0f};
    colors[ImGuiCol_ButtonHovered] = ImVec4{0.075f, 0.075f, 0.075f, 1.0f};
    colors[ImGuiCol_ButtonActive] = ImVec4{0.025f, 0.025f, 0.025f, 1.0f};

    // Frame BG
    colors[ImGuiCol_FrameBg] = ImVec4{0.05f, 0.05f, 0.05f, 1.0f};
    colors[ImGuiCol_FrameBgHovered] = ImVec4{0.075f, 0.075f, 0.075f, 1.0f};
    colors[ImGuiCol_FrameBgActive] = ImVec4{0.025f, 0.025f, 0.025f, 1.0f};

    // Main Menu
    colors[ImGuiCol_MenuBarBg] = ImVec4{0.03f, 0.03f, 0.03f, 1.0f};
    colors[ImGuiCol_Border] = ImVec4{0.0f, 0.0f, 0.0f, 1.0f};

    // Title
    colors[ImGuiCol_TitleBg] = ImVec4{0.015f, 0.015f, 0.015f, 1.0f};
    colors[ImGuiCol_TitleBgActive] = ImVec4{0.01f, 0.01f, 0.01f, 1.0f};
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.01f, 0.01f, 0.01f, 1.0f};

    colors[ImGuiCol_ResizeGrip] = ImVec4{0.05f, 0.05f, 0.05f, 1.0f};
    colors[ImGuiCol_ResizeGripHovered] = ImVec4{0.075f, 0.075f, 0.075f, 1.0f};
    colors[ImGuiCol_ResizeGripActive] = ImVec4{0.025f, 0.025f, 0.025f, 1.0f};
}


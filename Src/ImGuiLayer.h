//
// Created by alexm on 19/01/2022.
//
#pragma once

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_sdl.h"
#include "ImGuizmo.h"

struct ImguiLayer {
    void init();

    void draw(SDL_Window *window);

    static void SetDarkThemeColors();
};
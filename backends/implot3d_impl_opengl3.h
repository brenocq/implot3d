// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2024-2026 Breno Cunha Queiroz

#pragma once
#include "implot3d.h"
#include "implot3d_internal.h"
#ifndef IMGUI_DISABLE

IMPLOT3D_IMPL_API bool ImPlot3D_ImplOpenGL3_Init();
IMPLOT3D_IMPL_API void ImPlot3D_ImplOpenGL3_Shutdown();

IMPLOT3D_IMPL_API void ImPlot3D_ImplOpenGL3_RenderDrawData(ImDrawData3D* draw_data);

#endif // #ifndef IMGUI_DISABLE

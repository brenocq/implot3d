// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2024-2026 Breno Cunha Queiroz

#include "implot3d.h"
#ifndef IMGUI_DISABLE
#include "imgui.h"
#include "imgui_internal.h"
#include "implot3d_impl_opengl3.h"
#include <stdint.h>

// OpenGL loader
#include "imgui_impl_opengl3_loader.h"

// Track created textures for cleanup
static ImVector<GLuint> g_CreatedTextures;

bool ImPlot3D_ImplOpenGL3_Init() {
    // TODO
    return true;
}

void ImPlot3D_ImplOpenGL3_Shutdown() {
    // Clean up any remaining textures
    for (int i = 0; i < g_CreatedTextures.Size; i++) {
        glDeleteTextures(1, &g_CreatedTextures[i]);
    }
    g_CreatedTextures.clear();
}

void ImPlot3D_ImplOpenGL3_RenderPlots(ImPool<ImPlot3DPlot>* plots) {
    for (int i = 0; i < plots->GetBufSize(); i++) {
        ImPlot3DPlot* plot = plots->GetByIndex(i);
        // Create textures if they don't exist yet
        if (plot->ColorTextureID == ImTextureID_Invalid) {
            plot->ColorTextureID = ImPlot3D_ImplOpenGL3_CreateTexture(plot->PlotRect.GetSize());
        }
    }
}

ImTextureID ImPlot3D_ImplOpenGL3_CreateTexture(const ImVec2& size) {
    int width = (int)size.x;
    int height = (int)size.y;

    // Use ImGui's error handling for user-facing errors
    IM_ASSERT_USER_ERROR(width > 0 && height > 0, "ImPlot3D_ImplOpenGL3_CreateTexture: size must be positive!");
    if (width <= 0 || height <= 0)
        return ImTextureID_Invalid;

    // Create rainbow gradient pixel data (RGBA)
    size_t pixel_count = (size_t)width * (size_t)height;
    size_t data_size = pixel_count * 4; // 4 bytes per pixel (RGBA)

    // Allocate using ImGui's allocation
    unsigned char* pixels = (unsigned char*)IM_ALLOC(data_size);

    // Generate rainbow gradient
    // HSV to RGB conversion for smooth rainbow colors
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;

            // Create 2D rainbow: hue varies with x, saturation/brightness with y
            float hue = (float)x / (float)width;        // 0.0 to 1.0 across width
            float saturation = 1.0f;                     // Full saturation
            float value = (float)y / (float)height;      // 0.0 to 1.0 across height

            // HSV to RGB conversion
            float h = hue * 6.0f; // Hue in [0, 6)
            float c = value * saturation;
            float x_rgb = c * (1.0f - ImFabs(ImFmod(h, 2.0f) - 1.0f));
            float m = value - c;

            float r, g, b;
            if (h < 1.0f) {
                r = c; g = x_rgb; b = 0.0f;
            } else if (h < 2.0f) {
                r = x_rgb; g = c; b = 0.0f;
            } else if (h < 3.0f) {
                r = 0.0f; g = c; b = x_rgb;
            } else if (h < 4.0f) {
                r = 0.0f; g = x_rgb; b = c;
            } else if (h < 5.0f) {
                r = x_rgb; g = 0.0f; b = c;
            } else {
                r = c; g = 0.0f; b = x_rgb;
            }

            // Convert to 0-255 range and store
            pixels[idx + 0] = (unsigned char)((r + m) * 255.0f); // R
            pixels[idx + 1] = (unsigned char)((g + m) * 255.0f); // G
            pixels[idx + 2] = (unsigned char)((b + m) * 255.0f); // B
            pixels[idx + 3] = 255;                                // A (fully opaque)
        }
    }

    // Generate OpenGL texture
    GLuint texture_id = 0;
    GLint last_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload pixel data to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Free CPU memory
    IM_FREE(pixels);

    // Restore previous texture binding
    glBindTexture(GL_TEXTURE_2D, last_texture);

    // Track this texture for cleanup
    g_CreatedTextures.push_back(texture_id);

    // Return as ImTextureID (cast GLuint to ImTextureID)
    return (ImTextureID)(intptr_t)texture_id;
}

void ImPlot3D_ImplOpenGL3_DestroyTexture(ImTextureID tex_id) {
    GLuint texture_id = (GLuint)(intptr_t)tex_id;

    if (texture_id != 0) {
        glDeleteTextures(1, &texture_id);

        // Remove from tracking vector
        for (int i = 0; i < g_CreatedTextures.Size; i++) {
            if (g_CreatedTextures[i] == texture_id) {
                g_CreatedTextures.erase(&g_CreatedTextures[i]);
                break;
            }
        }
    }
}

#endif // #ifndef IMGUI_DISABLE

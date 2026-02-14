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

// Define depth texture constants if not present in ImGui's stripped loader
#ifndef GL_DEPTH_COMPONENT
#define GL_DEPTH_COMPONENT 0x1902
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

// Define framebuffer constants if not present in ImGui's stripped loader
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_DEPTH_BUFFER_BIT
#define GL_DEPTH_BUFFER_BIT 0x00000100
#endif

// Declare framebuffer functions if not in stripped loader
#ifndef IMGUI_IMPL_OPENGL_ES2
extern "C" {
typedef void (APIENTRYP PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef void (APIENTRYP PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint* framebuffers);
typedef void (APIENTRYP PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum (APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void (APIENTRYP PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);

static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
static PFNGLDRAWARRAYSPROC glDrawArrays;
}
#endif

// Shader sources
static const char* g_VertexShaderSource = R"(
#version 130

in vec3 Position;
in vec4 Color;

out vec4 Frag_Color;

void main() {
    // Simple orthographic projection: use X and Y, ignore Z for now
    gl_Position = vec4(Position.x, Position.y, 0.0, 1.0);
    Frag_Color = Color;
}
)";

static const char* g_FragmentShaderSource = R"(
#version 130

in vec4 Frag_Color;
out vec4 Out_Color;

void main() {
    Out_Color = Frag_Color;
}
)";

// Backend data stored in ImPlot3D context
struct ImPlot3D_ImplOpenGL3_Data {
    GLuint ShaderProgram;
    GLint AttribLocationPosition;
    GLint AttribLocationColor;
    GLuint VBO;
    GLuint VAO;
    GLuint FBO;
};
static ImPlot3D_ImplOpenGL3_Data g_Data;

// Track created textures for cleanup
static ImVector<GLuint> g_CreatedTextures;


IMPLOT3D_IMPL_API bool ImPlot3D_ImplOpenGL3_Init() {
    // Load framebuffer functions (not in stripped loader)
#ifndef IMGUI_IMPL_OPENGL_ES2
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)imgl3wGetProcAddress("glGenFramebuffers");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)imgl3wGetProcAddress("glDeleteFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)imgl3wGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)imgl3wGetProcAddress("glFramebufferTexture2D");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)imgl3wGetProcAddress("glCheckFramebufferStatus");
    glDrawArrays = (PFNGLDRAWARRAYSPROC)imgl3wGetProcAddress("glDrawArrays");
#endif

    // Compile vertex shader
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &g_VertexShaderSource, nullptr);
    glCompileShader(vertex_shader);

    // Check vertex shader compilation
    GLint success = 0;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(vertex_shader, 512, nullptr, info_log);
        IM_ASSERT_USER_ERROR(false, "ImPlot3D: Vertex shader compilation failed!");
        IMGUI_DEBUG_PRINTF("ImPlot3D: Vertex shader error: %s\n", info_log);
        return false;
    }

    // Compile fragment shader
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &g_FragmentShaderSource, nullptr);
    glCompileShader(fragment_shader);

    // Check fragment shader compilation
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(fragment_shader, 512, nullptr, info_log);
        IM_ASSERT_USER_ERROR(false, "ImPlot3D: Fragment shader compilation failed!");
        IMGUI_DEBUG_PRINTF("ImPlot3D: Fragment shader error: %s\n", info_log);
        glDeleteShader(vertex_shader);
        return false;
    }

    // Link shader program
    g_Data.ShaderProgram = glCreateProgram();
    glAttachShader(g_Data.ShaderProgram, vertex_shader);
    glAttachShader(g_Data.ShaderProgram, fragment_shader);
    glLinkProgram(g_Data.ShaderProgram);

    // Check linking
    glGetProgramiv(g_Data.ShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(g_Data.ShaderProgram, 512, nullptr, info_log);
        IM_ASSERT_USER_ERROR(false, "ImPlot3D: Shader program linking failed!");
        IMGUI_DEBUG_PRINTF("ImPlot3D: Shader linking error: %s\n", info_log);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }

    // Clean up shaders (no longer needed after linking)
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // Get attribute locations
    g_Data.AttribLocationPosition = glGetAttribLocation(g_Data.ShaderProgram, "Position");
    g_Data.AttribLocationColor = glGetAttribLocation(g_Data.ShaderProgram, "Color");

    // Create VAO and VBO
    glGenVertexArrays(1, &g_Data.VAO);
    glGenBuffers(1, &g_Data.VBO);

    // Create FBO for offscreen rendering
    glGenFramebuffers(1, &g_Data.FBO);

    return true;
}

IMPLOT3D_IMPL_API void ImPlot3D_ImplOpenGL3_Shutdown() {
    // Delete OpenGL resources
    if (g_Data.ShaderProgram)
        glDeleteProgram(g_Data.ShaderProgram);
    if (g_Data.VAO)
        glDeleteVertexArrays(1, &g_Data.VAO);
    if (g_Data.VBO)
        glDeleteBuffers(1, &g_Data.VBO);
    if (g_Data.FBO)
        glDeleteFramebuffers(1, &g_Data.FBO);

    // Clean up any remaining textures
    for (int i = 0; i < g_CreatedTextures.Size; i++) {
        glDeleteTextures(1, &g_CreatedTextures[i]);
    }
    g_CreatedTextures.clear();

    // Reset backend data
    g_Data = ImPlot3D_ImplOpenGL3_Data();
}

ImTextureID ImPlot3D_ImplOpenGL3_CreateRGBATexture(const ImVec2& size) {
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
            float hue = (float)x / (float)width;    // 0.0 to 1.0 across width
            float saturation = 1.0f;                // Full saturation
            float value = (float)y / (float)height; // 0.0 to 1.0 across height

            // HSV to RGB conversion
            float h = hue * 6.0f; // Hue in [0, 6)
            float c = value * saturation;
            float x_rgb = c * (1.0f - ImFabs(ImFmod(h, 2.0f) - 1.0f));
            float m = value - c;

            float r, g, b;
            if (h < 1.0f) {
                r = c;
                g = x_rgb;
                b = 0.0f;
            } else if (h < 2.0f) {
                r = x_rgb;
                g = c;
                b = 0.0f;
            } else if (h < 3.0f) {
                r = 0.0f;
                g = c;
                b = x_rgb;
            } else if (h < 4.0f) {
                r = 0.0f;
                g = x_rgb;
                b = c;
            } else if (h < 5.0f) {
                r = x_rgb;
                g = 0.0f;
                b = c;
            } else {
                r = c;
                g = 0.0f;
                b = x_rgb;
            }

            // Convert to 0-255 range and store
            pixels[idx + 0] = (unsigned char)((r + m) * 255.0f); // R
            pixels[idx + 1] = (unsigned char)((g + m) * 255.0f); // G
            pixels[idx + 2] = (unsigned char)((b + m) * 255.0f); // B
            pixels[idx + 3] = 255;                               // A (fully opaque)
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

IMPLOT3D_IMPL_API ImTextureID ImPlot3D_ImplOpenGL3_CreateDepthTexture(const ImVec2& size) {
    int width = (int)size.x;
    int height = (int)size.y;

    // Use ImGui's error handling for user-facing errors
    IM_ASSERT_USER_ERROR(width > 0 && height > 0, "ImPlot3D_ImplOpenGL3_CreateDepthTexture: size must be positive!");
    if (width <= 0 || height <= 0)
        return ImTextureID_Invalid;

    // Generate OpenGL depth texture
    GLuint texture_id = 0;
    GLint last_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Create depth texture
    // GL_DEPTH_COMPONENT24: 24-bit depth precision (good balance of precision and memory)
    // Note: ImPlot3D targets OpenGL 3.0+ where depth textures are core
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

    // Set texture parameters (recommended for depth textures)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // No interpolation for depth
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Clamp to avoid edge artifacts
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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

IMPLOT3D_IMPL_API void ImPlot3D_ImplOpenGL3_RenderPlots(ImPool<ImPlot3DPlot>* plots) {
    if (!plots)
        return;

    // Vertex format: vec3 position, vec4 color
    struct Vertex {
        float x, y, z;
        float r, g, b, a;
    };

    // Hardcoded rainbow triangle for testing
    Vertex triangle[3] = {
        {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, // Red (bottom-left)
        {0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},  // Green (bottom-right)
        {0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f},   // Blue (top)
    };

    // Iterate through all plots
    for (int i = 0; i < plots->GetBufSize(); i++) {
        ImPlot3DPlot* plot = plots->GetByIndex(i);

        // Create textures if they don't exist yet
        if (plot->ColorTextureID == ImTextureID_Invalid) {
            plot->ColorTextureID = ImPlot3D_ImplOpenGL3_CreateRGBATexture(plot->PlotRect.GetSize());
            plot->DepthTextureID = ImPlot3D_ImplOpenGL3_CreateDepthTexture(plot->PlotRect.GetSize());
        }

        if (!plot || !plot->Initialized)
            continue;

        // Get texture IDs
        GLuint color_texture = (GLuint)(intptr_t)plot->ColorTextureID;
        GLuint depth_texture = (GLuint)(intptr_t)plot->DepthTextureID;

        if (color_texture == 0)
            continue; // Skip if no color texture

        // Bind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, g_Data.FBO);

        // Attach textures to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);
        if (depth_texture != 0) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
        }

        // Check framebuffer status
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            IMGUI_DEBUG_PRINTF("ImPlot3D: Framebuffer not complete! Status: 0x%x\n", status);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            continue;
        }

        // Set viewport to texture size
        glViewport(0, 0, (int)plot->PlotRect.GetWidth(), (int)plot->PlotRect.GetHeight());

        // Clear color and depth
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | (depth_texture != 0 ? GL_DEPTH_BUFFER_BIT : 0));

        // Use shader program
        glUseProgram(g_Data.ShaderProgram);

        // Upload triangle data to VBO
        glBindBuffer(GL_ARRAY_BUFFER, g_Data.VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(triangle), triangle, GL_STREAM_DRAW);

        // Setup vertex attributes
        glBindVertexArray(g_Data.VAO);
        glEnableVertexAttribArray(g_Data.AttribLocationPosition);
        glEnableVertexAttribArray(g_Data.AttribLocationColor);

        glVertexAttribPointer(g_Data.AttribLocationPosition, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
        glVertexAttribPointer(g_Data.AttribLocationColor, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));

        // Draw triangle
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Cleanup
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
    }

    // Unbind framebuffer (return to default)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#endif // #ifndef IMGUI_DISABLE

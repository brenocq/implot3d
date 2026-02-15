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

// Define depth test constants
#ifndef GL_DEPTH_TEST
#define GL_DEPTH_TEST 0x0B71
#endif
#ifndef GL_LESS
#define GL_LESS 0x0201
#endif

// Declare framebuffer functions if not in stripped loader
#ifndef IMGUI_IMPL_OPENGL_ES2
typedef void(APIENTRYP PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef void(APIENTRYP PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint* framebuffers);
typedef void(APIENTRYP PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void(APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum(APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void(APIENTRYP PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void(APIENTRYP PFNGLCLEARDEPTHPROC)(GLdouble depth);
typedef void(APIENTRYP PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void(APIENTRYP PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void(APIENTRYP PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void(APIENTRYP PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);

static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
static PFNGLDEPTHFUNCPROC glDepthFunc;
static PFNGLCLEARDEPTHPROC glClearDepth;
static PFNGLDEPTHMASKPROC glDepthMask;
static PFNGLBLENDFUNCPROC glBlendFunc;
static PFNGLDRAWARRAYSPROC glDrawArrays;
static PFNGLUNIFORM2FPROC glUniform2f;
#endif

// Shader sources
static const char* g_VertexShaderSource = R"(
#version 130

in vec3 Position;  // 3D NDC position (before rotation)
in vec4 Color;     // RGBA color

out vec4 Frag_Color;

uniform mat4 u_Rotation;      // Rotation matrix from quaternion
uniform vec2 u_ViewportSize;  // Viewport size (width, height) in pixels

void main() {
    // The input Position is in NDC space [-1, 1] before rotation
    // NDCToPixels does: GetViewScale() * (Rotation * point)
    // So we need to: 1) Apply rotation, 2) Apply aspect ratio correction

    // Apply rotation to the 3D NDC position
    vec4 rotated_pos = u_Rotation * vec4(Position, 1.0);

    // Calculate aspect ratio correction
    // GetViewScale uses min(width, height), so we need to scale the longer axis
    float min_dim = min(u_ViewportSize.x, u_ViewportSize.y) * 1.11; // NOTE: No idea why 1.11 is needed
    vec2 scale = vec2(min_dim / u_ViewportSize.x, min_dim / u_ViewportSize.y);

    // Apply scale to maintain aspect ratio, flip Y, negate Z for depth
    gl_Position = vec4(rotated_pos.x * scale.x, -rotated_pos.y * scale.y, -rotated_pos.z, 1.0);
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
    GLint UniformLocationRotation;
    GLint UniformLocationViewportSize;
    GLuint VBO;
    GLuint EBO; // Element buffer for indices
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
    glClearDepth = (PFNGLCLEARDEPTHPROC)imgl3wGetProcAddress("glClearDepth");
    glDepthMask = (PFNGLDEPTHMASKPROC)imgl3wGetProcAddress("glDepthMask");
    glBlendFunc = (PFNGLBLENDFUNCPROC)imgl3wGetProcAddress("glBlendFunc");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)imgl3wGetProcAddress("glFramebufferTexture2D");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)imgl3wGetProcAddress("glCheckFramebufferStatus");
    glDepthFunc = (PFNGLDEPTHFUNCPROC)imgl3wGetProcAddress("glDepthFunc");
    glDrawArrays = (PFNGLDRAWARRAYSPROC)imgl3wGetProcAddress("glDrawArrays");
    glUniform2f = (PFNGLUNIFORM2FPROC)imgl3wGetProcAddress("glUniform2f");
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
    g_Data.UniformLocationRotation = glGetUniformLocation(g_Data.ShaderProgram, "u_Rotation");
    g_Data.UniformLocationViewportSize = glGetUniformLocation(g_Data.ShaderProgram, "u_ViewportSize");

    // Create buffers
    glGenVertexArrays(1, &g_Data.VAO);
    glGenBuffers(1, &g_Data.VBO);
    glGenBuffers(1, &g_Data.EBO);

    // Setup VAO with vertex attribute configuration
    // This only needs to be done once - the VAO stores this state
    glBindVertexArray(g_Data.VAO);

    // Bind buffers to VAO
    glBindBuffer(GL_ARRAY_BUFFER, g_Data.VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_Data.EBO);

    // Configure vertex attributes
    // Position: 3 floats at offset 0
    glEnableVertexAttribArray(g_Data.AttribLocationPosition);
    glVertexAttribPointer(g_Data.AttribLocationPosition, 3, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), // stride: 3 floats (xyz) + 1 float (packed color as 4 bytes)
                          (void*)0);

    // Color: 4 unsigned bytes at offset 12 (after 3 floats)
    glEnableVertexAttribArray(g_Data.AttribLocationColor);
    glVertexAttribPointer(g_Data.AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4 * sizeof(float), (void*)(3 * sizeof(float)));

    // Unbind VAO (stores all the state we just configured)
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

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
    if (g_Data.EBO)
        glDeleteBuffers(1, &g_Data.EBO);
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

    // Fill with zeros (transparent black)
    memset(pixels, 0, data_size);

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

IMPLOT3D_IMPL_API void ImPlot3D_ImplOpenGL3_RenderDrawData(ImDrawData3D* draw_data) {
    if (!draw_data)
        return;

    // First pass: Handle deletions and cleanup
    for (int i = draw_data->PlotData.Size - 1; i >= 0; i--) {
        ImDrawData3DPlot* plot_data = &draw_data->PlotData[i];
        if (plot_data->ShouldDelete) {
            // Clean up textures
            if (plot_data->ColorTextureID != ImTextureID_Invalid) {
                ImPlot3D_ImplOpenGL3_DestroyTexture(plot_data->ColorTextureID);
            }
            if (plot_data->DepthTextureID != ImTextureID_Invalid) {
                ImPlot3D_ImplOpenGL3_DestroyTexture(plot_data->DepthTextureID);
            }
            // Remove from array
            draw_data->PlotData.erase(draw_data->PlotData.Data + i);
        }
    }

    // Second pass: Render active plots
    for (int i = 0; i < draw_data->PlotData.Size; i++) {
        ImDrawData3DPlot* plot_data = &draw_data->PlotData[i];
        if (!plot_data->ShouldRender)
            continue;

        // Handle texture resizing
        if (plot_data->ShouldResize) {
            // Destroy old textures if they exist
            if (plot_data->ColorTextureID != ImTextureID_Invalid) {
                ImPlot3D_ImplOpenGL3_DestroyTexture(plot_data->ColorTextureID);
                plot_data->ColorTextureID = ImTextureID_Invalid;
            }
            if (plot_data->DepthTextureID != ImTextureID_Invalid) {
                ImPlot3D_ImplOpenGL3_DestroyTexture(plot_data->DepthTextureID);
                plot_data->DepthTextureID = ImTextureID_Invalid;
            }

            // Create new textures with current size
            plot_data->ColorTextureID = ImPlot3D_ImplOpenGL3_CreateRGBATexture(plot_data->TextureSize);
            plot_data->DepthTextureID = ImPlot3D_ImplOpenGL3_CreateDepthTexture(plot_data->TextureSize);
        }

        // Get texture IDs
        GLuint color_texture = (GLuint)(intptr_t)plot_data->ColorTextureID;
        GLuint depth_texture = (GLuint)(intptr_t)plot_data->DepthTextureID;
        if (color_texture == 0)
            continue;

        // Skip if no vertices to render
        if (plot_data->VtxBuffer.Size == 0 || plot_data->IdxBuffer.Size == 0)
            continue;

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
        glViewport(0, 0, (int)plot_data->GetPlotWidth(), (int)plot_data->GetPlotHeight());

        // Clear color and depth
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0); // Clear depth to far plane
        glClear(GL_COLOR_BUFFER_BIT | (depth_texture != 0 ? GL_DEPTH_BUFFER_BIT : 0));

        // Enable depth testing
        if (depth_texture != 0) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS); // Closer = smaller Z after negation
            glDepthMask(GL_TRUE); // Enable depth writes
        }

        // Enable alpha blending (same as ImGui's OpenGL3 backend)
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        // Use shader program
        glUseProgram(g_Data.ShaderProgram);

        // Convert quaternion to rotation matrix and upload to shader
        ImPlot3DQuat rot = plot_data->Rotation;
        float rot_matrix[16];
        // Quaternion to matrix conversion (column-major for OpenGL)
        float xx = (float)(rot.x * rot.x);
        float yy = (float)(rot.y * rot.y);
        float zz = (float)(rot.z * rot.z);
        float xy = (float)(rot.x * rot.y);
        float xz = (float)(rot.x * rot.z);
        float yz = (float)(rot.y * rot.z);
        float wx = (float)(rot.w * rot.x);
        float wy = (float)(rot.w * rot.y);
        float wz = (float)(rot.w * rot.z);

        rot_matrix[0] = 1.0f - 2.0f * (yy + zz);
        rot_matrix[1] = 2.0f * (xy + wz);
        rot_matrix[2] = 2.0f * (xz - wy);
        rot_matrix[3] = 0.0f;

        rot_matrix[4] = 2.0f * (xy - wz);
        rot_matrix[5] = 1.0f - 2.0f * (xx + zz);
        rot_matrix[6] = 2.0f * (yz + wx);
        rot_matrix[7] = 0.0f;

        rot_matrix[8] = 2.0f * (xz + wy);
        rot_matrix[9] = 2.0f * (yz - wx);
        rot_matrix[10] = 1.0f - 2.0f * (xx + yy);
        rot_matrix[11] = 0.0f;

        rot_matrix[12] = 0.0f;
        rot_matrix[13] = 0.0f;
        rot_matrix[14] = 0.0f;
        rot_matrix[15] = 1.0f;

        glUniformMatrix4fv(g_Data.UniformLocationRotation, 1, GL_FALSE, rot_matrix);

        // Upload viewport size uniform
        glUniform2f(g_Data.UniformLocationViewportSize, plot_data->GetPlotWidth(), plot_data->GetPlotHeight());

        // Convert vertices from double to float for OpenGL 3.x compatibility
        struct GLVertex {
            float x, y, z;
            ImU32 col;
        };

        ImVector<GLVertex> gl_vertices;
        gl_vertices.resize(plot_data->VtxBuffer.Size);
        for (int v = 0; v < plot_data->VtxBuffer.Size; v++) {
            const ImDrawVert3D& src = plot_data->VtxBuffer.Data[v];
            GLVertex& dst = gl_vertices.Data[v];
            dst.x = (float)src.pos.x;
            dst.y = (float)src.pos.y;
            dst.z = (float)src.pos.z;
            dst.col = src.col;
        }

        // Bind VAO (this restores all the vertex attribute configuration from Init)
        glBindVertexArray(g_Data.VAO);

        // Bind and upload vertex data to VBO
        glBindBuffer(GL_ARRAY_BUFFER, g_Data.VBO);
        glBufferData(GL_ARRAY_BUFFER, gl_vertices.Size * sizeof(GLVertex), gl_vertices.Data, GL_STREAM_DRAW);

        // Bind and upload index data to EBO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_Data.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, plot_data->IdxBuffer.Size * sizeof(ImDrawIdx3D), plot_data->IdxBuffer.Data, GL_STREAM_DRAW);

        // Draw triangles
        glDrawElements(GL_TRIANGLES, plot_data->IdxBuffer.Size, GL_UNSIGNED_INT, nullptr);

        // Unbind VAO
        glBindVertexArray(0);
        glUseProgram(0);

        // Disable states
        glDisable(GL_BLEND);
        if (depth_texture != 0) {
            glDisable(GL_DEPTH_TEST);
        }
    }

    // Third pass: Reset buffers
    for (int i = 0; i < draw_data->PlotData.Size; i++) {
        ImDrawData3DPlot* plot_data = &draw_data->PlotData[i];
        plot_data->ResetBuffers();
    }

    // Unbind framebuffer (return to default)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#endif // #ifndef IMGUI_DISABLE

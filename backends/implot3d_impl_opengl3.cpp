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
#ifndef GL_COLOR_ATTACHMENT1
#define GL_COLOR_ATTACHMENT1 0x8CE1
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_DEPTH_BUFFER_BIT
#define GL_DEPTH_BUFFER_BIT 0x00000100
#endif

// WBOIT texture format constants
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_R16F
#define GL_R16F 0x822D
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_COLOR
#define GL_COLOR 0x1800
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
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
typedef void(APIENTRYP PFNGLDRAWBUFFERSPROC)(GLsizei n, const GLenum* bufs);
typedef void(APIENTRYP PFNGLCLEARBUFFERFVPROC)(GLenum buffer, GLint drawbuffer, const GLfloat* value);

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
static PFNGLDRAWBUFFERSPROC glDrawBuffers;
static PFNGLCLEARBUFFERFVPROC glClearBufferfv;
#endif

// Shader sources
static const char* g_VertexShaderSource = R"(
#version 130

in vec3 Position;  // 3D NDC position (before rotation)
in vec4 Color;     // RGBA color

out vec4 Frag_Color;
out float Frag_Depth;

uniform mat4 u_Rotation;      // Rotation matrix from quaternion
uniform vec2 u_ViewportSize;  // Viewport size (width, height) in pixels

void main() {
    // Apply rotation to the 3D NDC position
    vec4 rotated_pos = u_Rotation * vec4(Position, 1.0);

    // Calculate aspect ratio correction
    float min_dim = min(u_ViewportSize.x, u_ViewportSize.y);
    vec2 scale = vec2(min_dim / u_ViewportSize.x, min_dim / u_ViewportSize.y);

    // Apply scale to maintain aspect ratio, flip Y, negate Z for depth
    gl_Position = vec4(rotated_pos.x * scale.x, -rotated_pos.y * scale.y, -rotated_pos.z, 1.0);
    Frag_Color = Color;
    Frag_Depth = gl_Position.z;
}
)";

static const char* g_FragmentShaderSource = R"(
#version 130

in vec4 Frag_Color;
in float Frag_Depth;

void main() {
    vec4 color = Frag_Color;

    // WBOIT weight function - simpler and more stable
    // Using depth-based weight to help with ordering
    float z = (Frag_Depth + 1.0) * 0.5; // Convert from [-1, 1] to [0, 1]
    float weight = color.a * clamp(0.03 / (1e-5 + pow(z / 200.0, 4.0)), 1e-2, 3e3);

    // Weighted color accumulation (to GL_COLOR_ATTACHMENT0)
    // Note: weight already includes alpha, so don't multiply by color.a again
    gl_FragData[0] = vec4(color.rgb * weight, weight);

    // Reveal: accumulate alpha (to GL_COLOR_ATTACHMENT1)
    gl_FragData[1] = vec4(color.a);
}
)";

// Composite shader for WBOIT final pass
static const char* g_CompositeVertexShaderSource = R"(
#version 130

in vec2 Position;
in vec2 UV;

out vec2 Frag_UV;

void main() {
    Frag_UV = UV;
    gl_Position = vec4(Position * 1.11, 0.0, 1.0);
}
)";

static const char* g_CompositeFragmentShaderSource = R"(
#version 130

in vec2 Frag_UV;

uniform sampler2D u_AccumTexture;
uniform sampler2D u_RevealTexture;

void main() {
    vec4 accum = texture2D(u_AccumTexture, Frag_UV);
    float reveal = texture2D(u_RevealTexture, Frag_UV).r;

    // Avoid division by zero
    if (accum.a < 0.00001) {
        discard;
    }

    // Average color from accumulated weighted colors
    vec3 average_color = accum.rgb / accum.a;

    // Use sqrt for more natural alpha response matching ImGui rendering
    float alpha = sqrt(clamp(reveal, 0.0, 1.0));

    gl_FragColor = vec4(average_color, alpha);
}
)";

// Backend data stored in ImPlot3D context
struct ImPlot3D_ImplOpenGL3_Data {
    GLuint ShaderProgram;
    GLuint CompositeShaderProgram;
    GLint AttribLocationPosition;
    GLint AttribLocationColor;
    GLint UniformLocationRotation;
    GLint UniformLocationViewportSize;
    GLint CompositeAttribLocationPosition;
    GLint CompositeAttribLocationUV;
    GLint CompositeUniformLocationAccum;
    GLint CompositeUniformLocationReveal;
    GLuint VBO;
    GLuint EBO; // Element buffer for indices
    GLuint VAO;
    GLuint CompositeVAO;
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
    glDrawBuffers = (PFNGLDRAWBUFFERSPROC)imgl3wGetProcAddress("glDrawBuffers");
    glClearBufferfv = (PFNGLCLEARBUFFERFVPROC)imgl3wGetProcAddress("glClearBufferfv");
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

    // Compile composite vertex shader for WBOIT
    GLuint comp_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(comp_vertex_shader, 1, &g_CompositeVertexShaderSource, nullptr);
    glCompileShader(comp_vertex_shader);

    glGetShaderiv(comp_vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(comp_vertex_shader, 512, nullptr, info_log);
        IM_ASSERT_USER_ERROR(false, "ImPlot3D: Composite vertex shader compilation failed!");
        IMGUI_DEBUG_PRINTF("ImPlot3D: Composite vertex shader error: %s\n", info_log);
        return false;
    }

    // Compile composite fragment shader for WBOIT
    GLuint comp_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(comp_fragment_shader, 1, &g_CompositeFragmentShaderSource, nullptr);
    glCompileShader(comp_fragment_shader);

    glGetShaderiv(comp_fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(comp_fragment_shader, 512, nullptr, info_log);
        IM_ASSERT_USER_ERROR(false, "ImPlot3D: Composite fragment shader compilation failed!");
        IMGUI_DEBUG_PRINTF("ImPlot3D: Composite fragment shader error: %s\n", info_log);
        glDeleteShader(comp_vertex_shader);
        return false;
    }

    // Link composite shader program
    g_Data.CompositeShaderProgram = glCreateProgram();
    glAttachShader(g_Data.CompositeShaderProgram, comp_vertex_shader);
    glAttachShader(g_Data.CompositeShaderProgram, comp_fragment_shader);
    glLinkProgram(g_Data.CompositeShaderProgram);

    glGetProgramiv(g_Data.CompositeShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(g_Data.CompositeShaderProgram, 512, nullptr, info_log);
        IM_ASSERT_USER_ERROR(false, "ImPlot3D: Composite shader linking failed!");
        IMGUI_DEBUG_PRINTF("ImPlot3D: Composite shader linking error: %s\n", info_log);
        glDeleteShader(comp_vertex_shader);
        glDeleteShader(comp_fragment_shader);
        return false;
    }

    glDeleteShader(comp_vertex_shader);
    glDeleteShader(comp_fragment_shader);

    // Get composite shader locations
    g_Data.CompositeAttribLocationPosition = glGetAttribLocation(g_Data.CompositeShaderProgram, "Position");
    g_Data.CompositeAttribLocationUV = glGetAttribLocation(g_Data.CompositeShaderProgram, "UV");
    g_Data.CompositeUniformLocationAccum = glGetUniformLocation(g_Data.CompositeShaderProgram, "u_AccumTexture");
    g_Data.CompositeUniformLocationReveal = glGetUniformLocation(g_Data.CompositeShaderProgram, "u_RevealTexture");

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

    // Setup composite VAO for full-screen quad
    glGenVertexArrays(1, &g_Data.CompositeVAO);
    glBindVertexArray(g_Data.CompositeVAO);

    // Note: Composite VAO doesn't need buffers bound yet - they'll be set during rendering
    // Just configure attribute locations for when we do bind them
    glEnableVertexAttribArray(g_Data.CompositeAttribLocationPosition);
    glEnableVertexAttribArray(g_Data.CompositeAttribLocationUV);

    glBindVertexArray(0);

    // Create FBO for offscreen rendering
    glGenFramebuffers(1, &g_Data.FBO);

    return true;
}

IMPLOT3D_IMPL_API void ImPlot3D_ImplOpenGL3_Shutdown() {
    // Delete OpenGL resources
    if (g_Data.ShaderProgram)
        glDeleteProgram(g_Data.ShaderProgram);
    if (g_Data.CompositeShaderProgram)
        glDeleteProgram(g_Data.CompositeShaderProgram);
    if (g_Data.VAO)
        glDeleteVertexArrays(1, &g_Data.VAO);
    if (g_Data.CompositeVAO)
        glDeleteVertexArrays(1, &g_Data.CompositeVAO);
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

// Generic texture creation function
static ImTextureID CreateTexture(const ImVec2& size, GLint internalFormat, GLenum format, GLenum type, GLint minFilter, GLint magFilter) {
    int width = (int)size.x;
    int height = (int)size.y;

    if (width <= 0 || height <= 0)
        return ImTextureID_Invalid;

    GLuint texture_id = 0;
    GLint last_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, last_texture);

    g_CreatedTextures.push_back(texture_id);
    return (ImTextureID)(intptr_t)texture_id;
}

ImTextureID ImPlot3D_ImplOpenGL3_CreateRGBATexture(const ImVec2& size) {
    IM_ASSERT_USER_ERROR(size.x > 0 && size.y > 0, "ImPlot3D_ImplOpenGL3_CreateTexture: size must be positive!");
    return CreateTexture(size, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR, GL_LINEAR);
}

IMPLOT3D_IMPL_API ImTextureID ImPlot3D_ImplOpenGL3_CreateDepthTexture(const ImVec2& size) {
    IM_ASSERT_USER_ERROR(size.x > 0 && size.y > 0, "ImPlot3D_ImplOpenGL3_CreateDepthTexture: size must be positive!");
    return CreateTexture(size, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_NEAREST, GL_NEAREST);
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

// Create WBOIT accumulation texture (RGBA16F)
ImTextureID ImPlot3D_ImplOpenGL3_CreateAccumTexture(const ImVec2& size) {
    return CreateTexture(size, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR, GL_LINEAR);
}

// Create WBOIT reveal texture (R16F)
ImTextureID ImPlot3D_ImplOpenGL3_CreateRevealTexture(const ImVec2& size) {
    return CreateTexture(size, GL_R16F, GL_RED, GL_FLOAT, GL_LINEAR, GL_LINEAR);
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
            if (plot_data->AccumTextureID != ImTextureID_Invalid) {
                ImPlot3D_ImplOpenGL3_DestroyTexture(plot_data->AccumTextureID);
            }
            if (plot_data->RevealTextureID != ImTextureID_Invalid) {
                ImPlot3D_ImplOpenGL3_DestroyTexture(plot_data->RevealTextureID);
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
            if (plot_data->AccumTextureID != ImTextureID_Invalid) {
                ImPlot3D_ImplOpenGL3_DestroyTexture(plot_data->AccumTextureID);
                plot_data->AccumTextureID = ImTextureID_Invalid;
            }
            if (plot_data->RevealTextureID != ImTextureID_Invalid) {
                ImPlot3D_ImplOpenGL3_DestroyTexture(plot_data->RevealTextureID);
                plot_data->RevealTextureID = ImTextureID_Invalid;
            }

            // Create new textures with current size
            plot_data->ColorTextureID = ImPlot3D_ImplOpenGL3_CreateRGBATexture(plot_data->TextureSize);
            plot_data->DepthTextureID = ImPlot3D_ImplOpenGL3_CreateDepthTexture(plot_data->TextureSize);
            plot_data->AccumTextureID = ImPlot3D_ImplOpenGL3_CreateAccumTexture(plot_data->TextureSize);
            plot_data->RevealTextureID = ImPlot3D_ImplOpenGL3_CreateRevealTexture(plot_data->TextureSize);
        }

        // Get texture IDs
        GLuint color_texture = (GLuint)(intptr_t)plot_data->ColorTextureID;
        GLuint depth_texture = (GLuint)(intptr_t)plot_data->DepthTextureID;
        GLuint accum_texture = (GLuint)(intptr_t)plot_data->AccumTextureID;
        GLuint reveal_texture = (GLuint)(intptr_t)plot_data->RevealTextureID;
        if (color_texture == 0 || accum_texture == 0 || reveal_texture == 0)
            continue;

        // Skip if no vertices to render
        if (plot_data->VtxBuffer.Size == 0 || plot_data->IdxBuffer.Size == 0)
            continue;

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

        // ====================
        // WBOIT Pass 1: Render geometry to accum/reveal targets
        // ====================

        glBindFramebuffer(GL_FRAMEBUFFER, g_Data.FBO);

        // Attach accum and reveal textures as color attachments
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accum_texture, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, reveal_texture, 0);
        if (depth_texture != 0) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
        }

        // Specify which color attachments to draw to
        GLenum draw_buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, draw_buffers);

        // Check framebuffer status
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            IMGUI_DEBUG_PRINTF("ImPlot3D: WBOIT framebuffer not complete! Status: 0x%x\n", status);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            continue;
        }

        // Set viewport to texture size
        glViewport(0, 0, (int)plot_data->GetPlotWidth(), (int)plot_data->GetPlotHeight());

        // Clear accum to (0,0,0,0) and reveal to 0.0
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | (depth_texture != 0 ? GL_DEPTH_BUFFER_BIT : 0));

        // For reveal texture (attachment 1), clear to 0.0 (we're accumulating alpha)
        GLfloat clear_reveal[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        glClearBufferfv(GL_COLOR, 1, clear_reveal);

        // Enable depth testing but disable depth writes (WBOIT requirement)
        if (depth_texture != 0) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_FALSE); // Disable depth writes for WBOIT
        }

        // Enable additive blending for WBOIT
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE); // Additive blending

        // Use WBOIT geometry shader
        glUseProgram(g_Data.ShaderProgram);

        // Convert quaternion to rotation matrix and upload to shader
        ImPlot3DQuat rot = plot_data->Rotation;
        float rot_matrix[16];
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
        glUniform2f(g_Data.UniformLocationViewportSize, plot_data->GetPlotWidth(), plot_data->GetPlotHeight());

        // Bind VAO and upload vertex/index data
        glBindVertexArray(g_Data.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, g_Data.VBO);
        glBufferData(GL_ARRAY_BUFFER, gl_vertices.Size * sizeof(GLVertex), gl_vertices.Data, GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_Data.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, plot_data->IdxBuffer.Size * sizeof(ImDrawIdx3D), plot_data->IdxBuffer.Data, GL_STREAM_DRAW);

        // Draw triangles to accum/reveal
        glDrawElements(GL_TRIANGLES, plot_data->IdxBuffer.Size, GL_UNSIGNED_INT, nullptr);

        glBindVertexArray(0);
        glUseProgram(0);

        // ====================
        // WBOIT Pass 2: Composite pass to final color texture
        // ====================

        // Attach final color texture as output
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);
        glDrawBuffers(1, draw_buffers); // Only draw to color attachment 0 now

        // Clear final color to transparent
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Disable depth test for composite pass (full-screen quad)
        glDisable(GL_DEPTH_TEST);

        // Use standard alpha blending for final composite
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        // Use composite shader
        glUseProgram(g_Data.CompositeShaderProgram);

        // Bind accum and reveal textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, accum_texture);
        glUniform1i(g_Data.CompositeUniformLocationAccum, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, reveal_texture);
        glUniform1i(g_Data.CompositeUniformLocationReveal, 1);

        // Draw full-screen quad
        // Define quad vertices: position (XY in NDC) + UV
        float quad_vertices[] = {
            // X     Y     U    V
            -1.0f, -1.0f, 0.0f, 0.0f, // Bottom-left
            1.0f,  -1.0f, 1.0f, 0.0f, // Bottom-right
            1.0f,  1.0f,  1.0f, 1.0f, // Top-right
            -1.0f, 1.0f,  0.0f, 1.0f  // Top-left
        };
        unsigned int quad_indices[] = {0, 1, 2, 0, 2, 3};

        glBindVertexArray(g_Data.CompositeVAO);

        // Upload quad vertices (position + UV)
        GLuint quad_vbo;
        glGenBuffers(1, &quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STREAM_DRAW);

        // Configure attributes
        glVertexAttribPointer(g_Data.CompositeAttribLocationPosition, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glVertexAttribPointer(g_Data.CompositeAttribLocationUV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        // Upload quad indices
        GLuint quad_ebo;
        glGenBuffers(1, &quad_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STREAM_DRAW);

        // Draw quad
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        // Cleanup temporary buffers
        glDeleteBuffers(1, &quad_vbo);
        glDeleteBuffers(1, &quad_ebo);

        glBindVertexArray(0);
        glUseProgram(0);

        // Disable states
        glDisable(GL_BLEND);
        if (depth_texture != 0) {
            glDepthMask(GL_TRUE); // Re-enable depth writes
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

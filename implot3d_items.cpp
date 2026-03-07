// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2024-2026 Breno Cunha Queiroz

// ImPlot3D v0.4 WIP

// Acknowledgments:
//  ImPlot3D is heavily inspired by ImPlot
//  (https://github.com/epezent/implot) by Evan Pezent,
//  and follows a similar code style and structure to
//  maintain consistency with ImPlot's API.

// Table of Contents:
// [SECTION] Includes
// [SECTION] Macros & Defines
// [SECTION] Template instantiation utility
// [SECTION] Item Utils
// [SECTION] Renderers
// [SECTION] Indexers
// [SECTION] Getters
// [SECTION] RenderPrimitives
// [SECTION] Markers
// [SECTION] PlotScatter
// [SECTION] PlotLine
// [SECTION] PlotTriangle
// [SECTION] PlotQuad
// [SECTION] PlotSurface
// [SECTION] PlotMesh
// [SECTION] PlotImage
// [SECTION] PlotText

//-----------------------------------------------------------------------------
// [SECTION] Includes
//-----------------------------------------------------------------------------

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "implot3d.h"
#include "implot3d_internal.h"

#ifndef IMGUI_DISABLE

//-----------------------------------------------------------------------------
// [SECTION] Macros & Defines
//-----------------------------------------------------------------------------

// clang-format off
#ifndef IMPLOT3D_NO_FORCE_INLINE
    #ifdef _MSC_VER
        #define IMPLOT3D_INLINE __forceinline
    #elif defined(__GNUC__)
        #define IMPLOT3D_INLINE inline __attribute__((__always_inline__))
    #elif defined(__CLANG__)
        #if __has_attribute(__always_inline__)
            #define IMPLOT3D_INLINE inline __attribute__((__always_inline__))
        #else
            #define IMPLOT3D_INLINE inline
        #endif
    #else
        #define IMPLOT3D_INLINE inline
    #endif
#else
    #define IMPLOT3D_INLINE inline
#endif
// clang-format on

#define IMPLOT3D_NORMALIZE2F(VX, VY)                                                                                                                 \
    do {                                                                                                                                             \
        float d2 = VX * VX + VY * VY;                                                                                                                \
        if (d2 > 0.0f) {                                                                                                                             \
            float inv_len = ImRsqrt(d2);                                                                                                             \
            VX *= inv_len;                                                                                                                           \
            VY *= inv_len;                                                                                                                           \
        }                                                                                                                                            \
    } while (0)

//-----------------------------------------------------------------------------
// [SECTION] Template instantiation utility
//-----------------------------------------------------------------------------

// By default, templates are instantiated for `float`, `double`, and for the following integer types, which are defined in imgui.h:
//     signed char         ImS8;   // 8-bit signed integer
//     unsigned char       ImU8;   // 8-bit unsigned integer
//     signed short        ImS16;  // 16-bit signed integer
//     unsigned short      ImU16;  // 16-bit unsigned integer
//     signed int          ImS32;  // 32-bit signed integer == int
//     unsigned int        ImU32;  // 32-bit unsigned integer
//     signed   long long  ImS64;  // 64-bit signed integer
//     unsigned long long  ImU64;  // 64-bit unsigned integer
// (note: this list does *not* include `long`, `unsigned long` and `long double`)
//
// You can customize the supported types by defining IMPLOT3D_CUSTOM_NUMERIC_TYPES at compile time to define your own type list.
//    As an example, you could use the compile time define given by the line below in order to support only float and double.
//        -DIMPLOT3D_CUSTOM_NUMERIC_TYPES="(float)(double)"
//    In order to support all known C++ types, use:
//        -DIMPLOT3D_CUSTOM_NUMERIC_TYPES="(signed char)(unsigned char)(signed short)(unsigned short)(signed int)(unsigned int)(signed long)(unsigned
//        long)(signed long long)(unsigned long long)(float)(double)(long double)"

#ifdef IMPLOT3D_CUSTOM_NUMERIC_TYPES
#define IMPLOT3D_NUMERIC_TYPES IMPLOT3D_CUSTOM_NUMERIC_TYPES
#else
#define IMPLOT3D_NUMERIC_TYPES (ImS8)(ImU8)(ImS16)(ImU16)(ImS32)(ImU32)(ImS64)(ImU64)(float)(double)
#endif

// CALL_INSTANTIATE_FOR_NUMERIC_TYPES will duplicate the template instantiation code `INSTANTIATE_MACRO(T)` on supported types.
#define _CAT(x, y) _CAT_(x, y)
#define _CAT_(x, y) x##y
#define _INSTANTIATE_FOR_NUMERIC_TYPES(chain) _CAT(_INSTANTIATE_FOR_NUMERIC_TYPES_1 chain, _END)
#define _INSTANTIATE_FOR_NUMERIC_TYPES_1(T) INSTANTIATE_MACRO(T) _INSTANTIATE_FOR_NUMERIC_TYPES_2
#define _INSTANTIATE_FOR_NUMERIC_TYPES_2(T) INSTANTIATE_MACRO(T) _INSTANTIATE_FOR_NUMERIC_TYPES_1
#define _INSTANTIATE_FOR_NUMERIC_TYPES_1_END
#define _INSTANTIATE_FOR_NUMERIC_TYPES_2_END
#define CALL_INSTANTIATE_FOR_NUMERIC_TYPES() _INSTANTIATE_FOR_NUMERIC_TYPES(IMPLOT3D_NUMERIC_TYPES)

//-----------------------------------------------------------------------------
// [SECTION] Item Utils
//-----------------------------------------------------------------------------
namespace ImPlot3D {

static const float ITEM_HIGHLIGHT_LINE_SCALE = 2.0f;
static const float ITEM_HIGHLIGHT_MARK_SCALE = 1.25f;

template <typename T> int Stride(const ImPlot3DSpec& spec) { return spec.Stride == IMPLOT3D_AUTO ? sizeof(T) : spec.Stride; }

bool BeginItem(const char* label_id, const ImPlot3DSpec& spec, const ImVec4& item_col, ImPlot3DMarker item_mkr) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PlotX() needs to be called between BeginPlot() and EndPlot()!");

    // Lock setup
    SetupLock();

    // Override next data with spec
    ImPlot3DStyle& style = gp.Style;
    ImPlot3DNextItemData& n = gp.NextItemData;
    ImPlot3DSpec& s = n.Spec;
    s = spec;

    // Register item
    bool just_created;
    ImPlot3DItem* item = RegisterOrGetItem(label_id, spec.Flags, &just_created);
    // Set current item
    gp.CurrentItem = item;

    // Set/override item color
    if (!IsColorAuto(item_col)) {
        item->Color = ImGui::ColorConvertFloat4ToU32(item_col);
    } else if (just_created) {
        item->Color = NextColormapColorU32();
    }

    // Set/override item marker
    if (item_mkr != ImPlot3DMarker_Invalid) {
        if (item_mkr != ImPlot3DMarker_Auto) {
            item->Marker = item_mkr;
        } else if (just_created && item_mkr == ImPlot3DMarker_Auto) {
            item->Marker = NextMarker();
        } else if (item_mkr == ImPlot3DMarker_Auto && item->Marker == ImPlot3DMarker_None) {
            item->Marker = NextMarker();
        }
    }

    // Set next item color
    ImVec4 item_color = ImGui::ColorConvertU32ToFloat4(item->Color);
    n.IsAutoLine = IsColorAuto(s.LineColor);
    n.IsAutoFill = IsColorAuto(s.FillColor);
    s.LineColor = IsColorAuto(s.LineColor) ? item_color : s.LineColor;
    s.FillColor = IsColorAuto(s.FillColor) ? item_color : s.FillColor;
    s.MarkerLineColor = IsColorAuto(s.MarkerLineColor) ? s.LineColor : s.MarkerLineColor;
    s.MarkerFillColor = IsColorAuto(s.MarkerFillColor) ? s.LineColor : s.MarkerFillColor;

    // Set size & weight
    s.LineWeight = s.LineWeight < 0.0f ? style.LineWeight : s.LineWeight;
    s.Marker = s.Marker < 0 ? style.Marker : s.Marker;
    s.MarkerSize = s.MarkerSize < 0.0f ? style.MarkerSize : s.MarkerSize;
    s.FillAlpha = s.FillAlpha < 0 ? gp.Style.FillAlpha : s.FillAlpha;

    // Apply alpha modifiers
    s.FillColor.w *= s.FillAlpha;
    s.MarkerFillColor.w *= s.FillAlpha;

    // Set render flags
    n.RenderLine = s.LineColor.w > 0 && s.LineWeight > 0;
    n.RenderFill = s.FillColor.w > 0;
    n.RenderMarkerLine = s.LineColor.w > 0 && s.LineWeight > 0;
    n.RenderMarkerFill = s.FillColor.w > 0;

    // Don't render if item is hidden
    if (!item->Show) {
        EndItem();
        return false;
    } else {
        // Legend hover highlight
        if (item->LegendHovered) {
            if (!ImHasFlag(gp.CurrentItems->Legend.Flags, ImPlot3DLegendFlags_NoHighlightItem)) {
                s.LineWeight *= ITEM_HIGHLIGHT_LINE_SCALE;
                s.MarkerSize *= ITEM_HIGHLIGHT_MARK_SCALE;
            }
        }
    }

    return true;
}

template <typename _Getter> bool BeginItemEx(const char* label_id, const _Getter& getter, const ImPlot3DSpec& spec,
                                             const ImVec4& item_col = IMPLOT3D_AUTO_COL, ImPlot3DMarker item_mkr = ImPlot3DMarker_Invalid) {
    if (BeginItem(label_id, spec, item_col, item_mkr)) {
        ImPlot3DContext& gp = *GImPlot3D;
        ImPlot3DPlot& plot = *gp.CurrentPlot;
        if (plot.FitThisFrame && !ImHasFlag(spec.Flags, ImPlot3DItemFlags_NoFit)) {
            for (int i = 0; i < getter.Count; i++)
                plot.ExtendFit(getter(i));
        }
        return true;
    }
    return false;
}

void EndItem() {
    ImPlot3DContext& gp = *GImPlot3D;
    gp.NextItemData.Reset();
    gp.CurrentItem = nullptr;
}

ImPlot3DItem* RegisterOrGetItem(const char* label_id, ImPlot3DItemFlags flags, bool* just_created) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DItemGroup& Items = *gp.CurrentItems;
    ImGuiID id = Items.GetItemID(label_id);
    if (just_created != nullptr)
        *just_created = Items.GetItem(id) == nullptr;
    ImPlot3DItem* item = Items.GetOrAddItem(id);

    // Avoid re-adding the same item to the legend (the legend is reset every frame)
    if (item->SeenThisFrame)
        return item;
    item->SeenThisFrame = true;

    // Add item to the legend
    int idx = Items.GetItemIndex(item);
    item->ID = id;
    if (!ImHasFlag(flags, ImPlot3DItemFlags_NoLegend) && ImGui::FindRenderedTextEnd(label_id, nullptr) != label_id) {
        Items.Legend.Indices.push_back(idx);
        item->NameOffset = Items.Legend.Labels.size();
        Items.Legend.Labels.append(label_id, label_id + strlen(label_id) + 1);
    }
    return item;
}

ImPlot3DItem* GetCurrentItem() {
    ImPlot3DContext& gp = *GImPlot3D;
    return gp.CurrentItem;
}

void BustItemCache() {
    ImPlot3DContext& gp = *GImPlot3D;
    for (int p = 0; p < gp.Plots.GetBufSize(); ++p) {
        ImPlot3DPlot& plot = *gp.Plots.GetByIndex(p);
        plot.Items.Reset();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Renderers
//-----------------------------------------------------------------------------

double GetPointDepth(ImPlot3DPoint p) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DPlot& plot = *gp.CurrentPlot;

    // Adjust for inverted axes before rotation
    if (ImHasFlag(plot.Axes[0].Flags, ImPlot3DAxisFlags_Invert))
        p.x = -p.x;
    if (ImHasFlag(plot.Axes[1].Flags, ImPlot3DAxisFlags_Invert))
        p.y = -p.y;
    if (ImHasFlag(plot.Axes[2].Flags, ImPlot3DAxisFlags_Invert))
        p.z = -p.z;

    ImPlot3DPoint p_rot = plot.Rotation * p;
    return p_rot.z;
}

struct RendererBase {
    RendererBase(int prims, int idx_consumed, int vtx_consumed, int z_consumed)
        : Prims(prims), IdxConsumed(idx_consumed), VtxConsumed(vtx_consumed), ZConsumed(z_consumed) {}
    const unsigned int Prims;       // Number of primitives to render
    const unsigned int IdxConsumed; // Number of indices consumed per primitive
    const unsigned int VtxConsumed; // Number of vertices consumed per primitive
    const unsigned int ZConsumed;   // Number of depth values consumed per primitive
};

template <class _Getter> struct RendererLineStrip : RendererBase {
    RendererLineStrip(const _Getter& getter, ImU32 col, float weight)
        : RendererBase(getter.Count - 1, 0, 0, 0), Getter(getter), Col(col), Weight(ImMax(1.0f, weight)) {
        // Initialize the first point in plot coordinates
        P1_plot = Getter(0);
    }

    void Init(ImDrawList3D&) const {}

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint P2_plot = Getter(prim + 1);

        // Clip the line segment to the culling box using Liang-Barsky algorithm
        ImPlot3DPoint P1_clipped, P2_clipped;
        bool visible = cull_box.ClipLineSegment(P1_plot, P2_plot, P1_clipped, P2_clipped);

        if (visible) {
            // Convert clipped points to NDC coordinates
            ImPlot3DPoint P1_ndc = PlotToNDC(P1_clipped);
            ImPlot3DPoint P2_ndc = PlotToNDC(P2_clipped);
            // Render the line segment
            draw_list_3d.AddLine(P1_ndc, P2_ndc, GetPointDepth((P1_plot + P2_plot) * 0.5), Col, Col, Weight);
        }

        // Update for next segment
        P1_plot = P2_plot;
        return visible;
    }

    const _Getter& Getter;
    const ImU32 Col;
    const float Weight;
    mutable ImPlot3DPoint P1_plot;
};

template <class _Getter> struct RendererLineStripSkip : RendererBase {
    RendererLineStripSkip(const _Getter& getter, ImU32 col, float weight)
        : RendererBase(getter.Count - 1, 0, 0, 0), Getter(getter), Col(col), Weight(ImMax(1.0f, weight)) {
        // Initialize the first point in plot coordinates
        P1_plot = Getter(0);
    }

    void Init(ImDrawList3D&) const {}

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        // Get the next point in plot coordinates
        ImPlot3DPoint P2_plot = Getter(prim + 1);
        bool visible = false;

        // Check for NaNs in P1_plot and P2_plot
        if (!ImNan(P1_plot.x) && !ImNan(P1_plot.y) && !ImNan(P1_plot.z) && !ImNan(P2_plot.x) && !ImNan(P2_plot.y) && !ImNan(P2_plot.z)) {
            // Clip the line segment to the culling box
            ImPlot3DPoint P1_clipped, P2_clipped;
            visible = cull_box.ClipLineSegment(P1_plot, P2_plot, P1_clipped, P2_clipped);

            if (visible) {
                // Convert clipped points to NDC coordinates
                ImPlot3DPoint P1_ndc = PlotToNDC(P1_clipped);
                ImPlot3DPoint P2_ndc = PlotToNDC(P2_clipped);
                // Render the line segment
                draw_list_3d.AddLine(P1_ndc, P2_ndc, GetPointDepth((P1_plot + P2_plot) * 0.5), Col, Col, Weight);
            }
        }

        // Update P1_plot if P2_plot is valid
        if (!ImNan(P2_plot.x) && !ImNan(P2_plot.y) && !ImNan(P2_plot.z))
            P1_plot = P2_plot;

        return visible;
    }

    const _Getter& Getter;
    const ImU32 Col;
    const float Weight;
    mutable ImPlot3DPoint P1_plot;
};

template <class _Getter> struct RendererLineSegments : RendererBase {
    RendererLineSegments(const _Getter& getter, ImU32 col, float weight)
        : RendererBase(getter.Count / 2, 0, 0, 0), Getter(getter), Col(col), Weight(ImMax(1.0f, weight)) {}

    void Init(ImDrawList3D&) const {}

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        // Get the segment's endpoints in plot coordinates
        ImPlot3DPoint P1_plot = Getter(prim * 2 + 0);
        ImPlot3DPoint P2_plot = Getter(prim * 2 + 1);

        // Check for NaNs in P1_plot and P2_plot
        if (!ImNan(P1_plot.x) && !ImNan(P1_plot.y) && !ImNan(P1_plot.z) && !ImNan(P2_plot.x) && !ImNan(P2_plot.y) && !ImNan(P2_plot.z)) {
            // Clip the line segment to the culling box
            ImPlot3DPoint P1_clipped, P2_clipped;
            bool visible = cull_box.ClipLineSegment(P1_plot, P2_plot, P1_clipped, P2_clipped);

            if (visible) {
                // Convert clipped points to NDC coordinates
                ImPlot3DPoint P1_ndc = PlotToNDC(P1_clipped);
                ImPlot3DPoint P2_ndc = PlotToNDC(P2_clipped);
                // Render the line segment
                draw_list_3d.AddLine(P1_ndc, P2_ndc, GetPointDepth((P1_plot + P2_plot) * 0.5), Col, Col, Weight);
            }
            return visible;
        }

        return false;
    }

    const _Getter& Getter;
    const ImU32 Col;
    const float Weight;
};

template <class _Getter> struct RendererTriangleFill : RendererBase {
    RendererTriangleFill(const _Getter& getter, ImU32 col) : RendererBase(getter.Count / 3, 3, 3, 1), Getter(getter), Col(col) {}

    void Init(ImDrawList3D& draw_list_3d) const { UV = draw_list_3d._SharedData->TexUvWhitePixel; }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint p_plot[3];
        p_plot[0] = Getter(3 * prim);
        p_plot[1] = Getter(3 * prim + 1);
        p_plot[2] = Getter(3 * prim + 2);

        // Check if the triangle is outside the culling box
        if (!cull_box.Contains(p_plot[0]) && !cull_box.Contains(p_plot[1]) && !cull_box.Contains(p_plot[2]))
            return false;

        // Project the triangle vertices to NDC space
        ImPlot3DPoint p[3];
        p[0] = PlotToNDC(p_plot[0]);
        p[1] = PlotToNDC(p_plot[1]);
        p[2] = PlotToNDC(p_plot[2]);

        // 3 vertices per triangle
        draw_list_3d._VtxWritePtr[0].pos = p[0];
        draw_list_3d._VtxWritePtr[0].uv = UV;
        draw_list_3d._VtxWritePtr[0].col = Col;
        draw_list_3d._VtxWritePtr[1].pos = p[1];
        draw_list_3d._VtxWritePtr[1].uv = UV;
        draw_list_3d._VtxWritePtr[1].col = Col;
        draw_list_3d._VtxWritePtr[2].pos = p[2];
        draw_list_3d._VtxWritePtr[2].uv = UV;
        draw_list_3d._VtxWritePtr[2].col = Col;
        draw_list_3d._VtxWritePtr += 3;

        // 3 indices per triangle
        draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 1);
        draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
        draw_list_3d._IdxWritePtr += 3;
        // 1 Z per triangle
        draw_list_3d._ZWritePtr[0] = GetPointDepth((p_plot[0] + p_plot[1] + p_plot[2]) / 3);
        draw_list_3d._ZWritePtr++;

        // Update vertex count
        draw_list_3d._VtxCurrentIdx += 3;

        // Add triangle command
        draw_list_3d.AddTriangleCmd(3);

        return true;
    }

    const _Getter& Getter;
    mutable ImVec2 UV;
    const ImU32 Col;
};

template <class _Getter> struct RendererQuadFill : RendererBase {
    RendererQuadFill(const _Getter& getter, ImU32 col) : RendererBase(getter.Count / 4, 6, 4, 2), Getter(getter), Col(col) {}

    void Init(ImDrawList3D& draw_list_3d) const { UV = draw_list_3d._SharedData->TexUvWhitePixel; }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint p_plot[4];
        p_plot[0] = Getter(4 * prim);
        p_plot[1] = Getter(4 * prim + 1);
        p_plot[2] = Getter(4 * prim + 2);
        p_plot[3] = Getter(4 * prim + 3);

        // Check if the quad is outside the culling box
        if (!cull_box.Contains(p_plot[0]) && !cull_box.Contains(p_plot[1]) && !cull_box.Contains(p_plot[2]) && !cull_box.Contains(p_plot[3]))
            return false;

        // Project the quad vertices to NDC space
        ImPlot3DPoint p[4];
        p[0] = PlotToNDC(p_plot[0]);
        p[1] = PlotToNDC(p_plot[1]);
        p[2] = PlotToNDC(p_plot[2]);
        p[3] = PlotToNDC(p_plot[3]);

        // Add vertices for two triangles
        draw_list_3d._VtxWritePtr[0].pos = p[0];
        draw_list_3d._VtxWritePtr[0].uv = UV;
        draw_list_3d._VtxWritePtr[0].col = Col;

        draw_list_3d._VtxWritePtr[1].pos = p[1];
        draw_list_3d._VtxWritePtr[1].uv = UV;
        draw_list_3d._VtxWritePtr[1].col = Col;

        draw_list_3d._VtxWritePtr[2].pos = p[2];
        draw_list_3d._VtxWritePtr[2].uv = UV;
        draw_list_3d._VtxWritePtr[2].col = Col;

        draw_list_3d._VtxWritePtr[3].pos = p[3];
        draw_list_3d._VtxWritePtr[3].uv = UV;
        draw_list_3d._VtxWritePtr[3].col = Col;

        draw_list_3d._VtxWritePtr += 4;

        // Add indices for two triangles
        draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 1);
        draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);

        draw_list_3d._IdxWritePtr[3] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[4] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
        draw_list_3d._IdxWritePtr[5] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 3);

        draw_list_3d._IdxWritePtr += 6;

        // Add depth value for the quad
        double z = GetPointDepth((p_plot[0] + p_plot[1] + p_plot[2] + p_plot[3]) / 4.0);
        draw_list_3d._ZWritePtr[0] = z;
        draw_list_3d._ZWritePtr[1] = z;
        draw_list_3d._ZWritePtr += 2;

        // Update vertex count
        draw_list_3d._VtxCurrentIdx += 4;

        // Add triangle command (6 indices = 2 triangles)
        draw_list_3d.AddTriangleCmd(6);

        return true;
    }

    const _Getter& Getter;
    mutable ImVec2 UV;
    const ImU32 Col;
};

template <class _Getter> struct RendererQuadImage : RendererBase {
    RendererQuadImage(const _Getter& getter, ImTextureRef tex_ref, const ImVec2& uv0, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3,
                      ImU32 col)
        : RendererBase(getter.Count / 4, 6, 4, 2), Getter(getter), TexRef(tex_ref), UV0(uv0), UV1(uv1), UV2(uv2), UV3(uv3), Col(col) {}

    void Init(ImDrawList3D& /*draw_list_3d*/) const {}

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint p_plot[4];
        p_plot[0] = Getter(4 * prim);
        p_plot[1] = Getter(4 * prim + 1);
        p_plot[2] = Getter(4 * prim + 2);
        p_plot[3] = Getter(4 * prim + 3);

        // Check if the quad is outside the culling box
        if (!cull_box.Contains(p_plot[0]) && !cull_box.Contains(p_plot[1]) && !cull_box.Contains(p_plot[2]) && !cull_box.Contains(p_plot[3]))
            return false;

        // Set texture ID to be used when rendering this quad
        draw_list_3d.SetTexture(TexRef);

        // Project the quad vertices to NDC space
        ImPlot3DPoint p[4];
        p[0] = PlotToNDC(p_plot[0]);
        p[1] = PlotToNDC(p_plot[1]);
        p[2] = PlotToNDC(p_plot[2]);
        p[3] = PlotToNDC(p_plot[3]);

        // Add vertices for two triangles
        draw_list_3d._VtxWritePtr[0].pos = p[0];
        draw_list_3d._VtxWritePtr[0].uv = UV0;
        draw_list_3d._VtxWritePtr[0].col = Col;

        draw_list_3d._VtxWritePtr[1].pos = p[1];
        draw_list_3d._VtxWritePtr[1].uv = UV1;
        draw_list_3d._VtxWritePtr[1].col = Col;

        draw_list_3d._VtxWritePtr[2].pos = p[2];
        draw_list_3d._VtxWritePtr[2].uv = UV2;
        draw_list_3d._VtxWritePtr[2].col = Col;

        draw_list_3d._VtxWritePtr[3].pos = p[3];
        draw_list_3d._VtxWritePtr[3].uv = UV3;
        draw_list_3d._VtxWritePtr[3].col = Col;

        draw_list_3d._VtxWritePtr += 4;

        // Add indices for two triangles
        draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 1);
        draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);

        draw_list_3d._IdxWritePtr[3] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[4] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
        draw_list_3d._IdxWritePtr[5] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 3);

        draw_list_3d._IdxWritePtr += 6;

        // Add depth value for the quad
        double z = GetPointDepth((p_plot[0] + p_plot[1] + p_plot[2] + p_plot[3]) / 4.0);
        draw_list_3d._ZWritePtr[0] = z;
        draw_list_3d._ZWritePtr[1] = z;
        draw_list_3d._ZWritePtr += 2;

        // Update vertex count
        draw_list_3d._VtxCurrentIdx += 4;

        // Add triangle command
        draw_list_3d.AddTriangleCmd(6);

        // Reset texture ID
        draw_list_3d.ResetTexture();

        return true;
    }

    const _Getter& Getter;
    const ImTextureRef TexRef;
    const ImVec2 UV0, UV1, UV2, UV3;
    const ImU32 Col;
};

template <class _Getter> struct RendererSurfaceFill : RendererBase {
    RendererSurfaceFill(const _Getter& getter, int x_count, int y_count, ImU32 col, double scale_min, double scale_max)
        : RendererBase((x_count - 1) * (y_count - 1), 6, 4, 2), Getter(getter), XCount(x_count), YCount(y_count), Col(col), ScaleMin(scale_min),
          ScaleMax(scale_max) {}

    void Init(ImDrawList3D& draw_list_3d) const {
        UV = draw_list_3d._SharedData->TexUvWhitePixel;

        // Compute min and max values for the colormap (if not solid fill)
        const ImPlot3DNextItemData& n = GetItemData();
        if (n.IsAutoFill) {
            Min = DBL_MAX;
            Max = -DBL_MAX;
            for (int i = 0; i < Getter.Count; i++) {
                double z = Getter(i).z;
                Min = ImMin(Min, z);
                Max = ImMax(Max, z);
            }
        }
    }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        int x = prim % (XCount - 1);
        int y = prim / (XCount - 1);

        ImPlot3DPoint p_plot[4];
        p_plot[0] = Getter(x + y * XCount);
        p_plot[1] = Getter(x + 1 + y * XCount);
        p_plot[2] = Getter(x + 1 + (y + 1) * XCount);
        p_plot[3] = Getter(x + (y + 1) * XCount);

        // Check if the quad is outside the culling box
        if (!cull_box.Contains(p_plot[0]) && !cull_box.Contains(p_plot[1]) && !cull_box.Contains(p_plot[2]) && !cull_box.Contains(p_plot[3]))
            return false;

        // Compute colors
        ImU32 cols[4] = {Col, Col, Col, Col};
        const ImPlot3DNextItemData& n = GetItemData();
        if (n.IsAutoFill) {
            float alpha = GImPlot3D->NextItemData.Spec.FillAlpha;
            double min = Min;
            double max = Max;
            if (ScaleMin != 0.0 || ScaleMax != 0.0) {
                min = ScaleMin;
                max = ScaleMax;
            }
            for (int i = 0; i < 4; i++) {
                ImVec4 col = SampleColormap((float)ImClamp(ImRemap01(p_plot[i].z, min, max), 0.0, 1.0));
                col.w *= alpha;
                cols[i] = ImGui::ColorConvertFloat4ToU32(col);
            }
        }

        // Project the quad vertices to NDC space
        ImPlot3DPoint p[4];
        p[0] = PlotToNDC(p_plot[0]);
        p[1] = PlotToNDC(p_plot[1]);
        p[2] = PlotToNDC(p_plot[2]);
        p[3] = PlotToNDC(p_plot[3]);

        // Add vertices for two triangles
        draw_list_3d._VtxWritePtr[0].pos = p[0];
        draw_list_3d._VtxWritePtr[0].uv = UV;
        draw_list_3d._VtxWritePtr[0].col = cols[0];

        draw_list_3d._VtxWritePtr[1].pos = p[1];
        draw_list_3d._VtxWritePtr[1].uv = UV;
        draw_list_3d._VtxWritePtr[1].col = cols[1];

        draw_list_3d._VtxWritePtr[2].pos = p[2];
        draw_list_3d._VtxWritePtr[2].uv = UV;
        draw_list_3d._VtxWritePtr[2].col = cols[2];

        draw_list_3d._VtxWritePtr[3].pos = p[3];
        draw_list_3d._VtxWritePtr[3].uv = UV;
        draw_list_3d._VtxWritePtr[3].col = cols[3];

        draw_list_3d._VtxWritePtr += 4;

        // Add indices for two triangles
        draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 1);
        draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);

        draw_list_3d._IdxWritePtr[3] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[4] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
        draw_list_3d._IdxWritePtr[5] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 3);

        draw_list_3d._IdxWritePtr += 6;

        // Add depth values for the two triangles
        draw_list_3d._ZWritePtr[0] = GetPointDepth((p_plot[0] + p_plot[1] + p_plot[2]) / 3.0);
        draw_list_3d._ZWritePtr[1] = GetPointDepth((p_plot[0] + p_plot[2] + p_plot[3]) / 3.0);
        draw_list_3d._ZWritePtr += 2;

        // Update vertex count
        draw_list_3d._VtxCurrentIdx += 4;

        // Add triangle command
        draw_list_3d.AddTriangleCmd(6);

        return true;
    }

    const _Getter& Getter;
    mutable ImVec2 UV;
    mutable double Min; // Minimum value for the colormap
    mutable double Max; // Maximum value for the colormap
    const int XCount;
    const int YCount;
    const ImU32 Col;
    const double ScaleMin;
    const double ScaleMax;
};

//-----------------------------------------------------------------------------
// [SECTION] Indexers
//-----------------------------------------------------------------------------

template <typename T> IMPLOT3D_INLINE T IndexData(const T* data, int idx, int count, int offset, int stride) {
    const int s = ((offset == 0) << 0) | ((stride == sizeof(T)) << 1);
    switch (s) {
        case 3: return data[idx];
        case 2: return data[(offset + idx) % count];
        case 1: return *(const T*)(const void*)((const unsigned char*)data + (size_t)((idx)) * stride);
        case 0: return *(const T*)(const void*)((const unsigned char*)data + (size_t)((offset + idx) % count) * stride);
        default: return T(0);
    }
}

template <typename T> struct IndexerIdx {
    IndexerIdx(const T* data, int count, int offset = 0, int stride = sizeof(T))
        : Data(data), Count(count), Offset(count ? ImPosMod(offset, count) : 0), Stride(stride) {}
    template <typename I> IMPLOT3D_INLINE double operator()(I idx) const { return (double)IndexData(Data, idx, Count, Offset, Stride); }
    const T* Data;
    int Count;
    int Offset;
    int Stride;
};

//-----------------------------------------------------------------------------
// [SECTION] Getters
//-----------------------------------------------------------------------------

template <typename _IndexerX, typename _IndexerY, typename _IndexerZ> struct GetterXYZ {
    GetterXYZ(_IndexerX x, _IndexerY y, _IndexerZ z, int count) : IndexerX(x), IndexerY(y), IndexerZ(z), Count(count) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const { return ImPlot3DPoint(IndexerX(idx), IndexerY(idx), IndexerZ(idx)); }
    const _IndexerX IndexerX;
    const _IndexerY IndexerY;
    const _IndexerZ IndexerZ;
    const int Count;
};

template <typename _Getter> struct GetterLoop {
    GetterLoop(_Getter getter) : Getter(getter), Count(getter.Count + 1) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const {
        idx = idx % (Count - 1);
        return Getter(idx);
    }
    const _Getter Getter;
    const int Count;
};

template <typename _Getter> struct GetterTriangleLines {
    GetterTriangleLines(_Getter getter) : Getter(getter), Count(getter.Count * 2) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const {
        idx = ((idx % 6 + 1) / 2) % 3 + idx / 6 * 3;
        return Getter(idx);
    }
    const _Getter Getter;
    const int Count;
};

template <typename _Getter> struct GetterQuadLines {
    GetterQuadLines(_Getter getter) : Getter(getter), Count(getter.Count * 2) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const {
        idx = ((idx % 8 + 1) / 2) % 4 + idx / 8 * 4;
        return Getter(idx);
    }
    const _Getter Getter;
    const int Count;
};

template <typename _Getter> struct GetterSurfaceLines {
    GetterSurfaceLines(_Getter getter, int x_count, int y_count) : Getter(getter), XCount(x_count), YCount(y_count) {
        int horizontal_segments = (XCount - 1) * YCount;
        int vertical_segments = (YCount - 1) * XCount;
        int segments = horizontal_segments + vertical_segments;
        Count = segments * 2; // Each segment has 2 endpoints
    }

    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const {
        // idx is an endpoint index
        int endpoint_i = (int)(idx % 2);
        int segment_i = (int)(idx / 2);

        int horizontal_segments = (XCount - 1) * YCount;

        int px, py;
        if (segment_i < horizontal_segments) {
            // Horizontal segment
            int row = segment_i / (XCount - 1);
            int col = segment_i % (XCount - 1);
            // Endpoint 0 is (col, row), endpoint 1 is (col+1, row)
            px = endpoint_i == 0 ? col : col + 1;
            py = row;
        } else {
            // Vertical segment
            int seg_v = segment_i - horizontal_segments;
            int col = seg_v / (YCount - 1);
            int row = seg_v % (YCount - 1);
            // Endpoint 0 is (col, row), endpoint 1 is (col, row+1)
            px = col;
            py = row + endpoint_i;
        }

        return Getter(py * XCount + px);
    }

    const _Getter Getter;
    int Count;
    const int XCount;
    const int YCount;
};

struct Getter3DPoints {
    Getter3DPoints(const ImPlot3DPoint* points, int count) : Points(points), Count(count) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const { return Points[idx]; }
    const ImPlot3DPoint* Points;
    const int Count;
};

struct GetterMeshTriangles {
    GetterMeshTriangles(const ImPlot3DPoint* vtx, const unsigned int* idx, int idx_count)
        : Vtx(vtx), Idx(idx), IdxCount(idx_count), TriCount(idx_count / 3), Count(idx_count) {}

    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I i) const {
        unsigned int vi = Idx[i];
        return Vtx[vi];
    }

    const ImPlot3DPoint* Vtx;
    const unsigned int* Idx;
    int IdxCount;
    int TriCount;
    int Count;
};

//-----------------------------------------------------------------------------
// [SECTION] RenderPrimitives
//-----------------------------------------------------------------------------

/// Renders triangle-based primitive shapes (surfaces, fills)
template <template <class> class _Renderer, class _Getter, typename... Args> void RenderPrimitives(const _Getter& getter, Args... args) {
    _Renderer<_Getter> renderer(getter, args...);
    ImPlot3DPlot& plot = *GetCurrentPlot();
    ImDrawList3D& draw_list_3d = plot.DrawList;
    ImPlot3DBox cull_box;
    if (ImHasFlag(plot.Flags, ImPlot3DFlags_NoClip)) {
        cull_box.Min = ImPlot3DPoint(-HUGE_VAL, -HUGE_VAL, -HUGE_VAL);
        cull_box.Max = ImPlot3DPoint(HUGE_VAL, HUGE_VAL, HUGE_VAL);
    } else {
        cull_box.Min = plot.RangeMin();
        cull_box.Max = plot.RangeMax();
    }

    // Find how many can be reserved up to end of current draw command's limit
    unsigned int prims_to_render = ImMin(renderer.Prims, (ImDrawList3D::MaxIdx() - draw_list_3d._VtxCurrentIdx) / renderer.VtxConsumed);

    // Reserve vertices, indices, and depth values to render the primitives
    draw_list_3d.PrimReserve(prims_to_render * renderer.IdxConsumed, prims_to_render * renderer.VtxConsumed, prims_to_render * renderer.ZConsumed);

    // Initialize renderer
    renderer.Init(draw_list_3d);

    // Render primitives
    int num_culled = 0;
    for (unsigned int i = 0; i < prims_to_render; i++)
        if (!renderer.Render(draw_list_3d, cull_box, i))
            num_culled++;
    // Unreserve unused vertices, indices, and depth values
    draw_list_3d.PrimUnreserve(num_culled * renderer.IdxConsumed, num_culled * renderer.VtxConsumed, num_culled * renderer.ZConsumed);
}

/// Renders line segment primitives (pushes to LineBuffer, no tessellation)
template <template <class> class _Renderer, class _Getter, typename... Args> void RenderLinePrimitives(const _Getter& getter, Args... args) {
    _Renderer<_Getter> renderer(getter, args...);
    ImPlot3DPlot& plot = *GetCurrentPlot();
    ImDrawList3D& draw_list_3d = plot.DrawList;
    ImPlot3DBox cull_box;
    if (ImHasFlag(plot.Flags, ImPlot3DFlags_NoClip)) {
        cull_box.Min = ImPlot3DPoint(-HUGE_VAL, -HUGE_VAL, -HUGE_VAL);
        cull_box.Max = ImPlot3DPoint(HUGE_VAL, HUGE_VAL, HUGE_VAL);
    } else {
        cull_box.Min = plot.RangeMin();
        cull_box.Max = plot.RangeMax();
    }

    renderer.Init(draw_list_3d);
    for (unsigned int i = 0; i < renderer.Prims; i++)
        renderer.Render(draw_list_3d, cull_box, i);
}

//-----------------------------------------------------------------------------
// [SECTION] Markers
//-----------------------------------------------------------------------------

template <typename _Getter> void RenderMarkers(const _Getter& getter, ImPlot3DMarker marker, float size, bool rend_fill, ImU32 col_fill,
                                               bool rend_line, ImU32 col_line, float weight) {
    ImPlot3DPlot& plot = *GetCurrentPlot();
    ImDrawList3D& draw_list_3d = plot.DrawList;
    ImPlot3DBox cull_box;
    if (ImHasFlag(plot.Flags, ImPlot3DFlags_NoClip)) {
        cull_box.Min = ImPlot3DPoint(-HUGE_VAL, -HUGE_VAL, -HUGE_VAL);
        cull_box.Max = ImPlot3DPoint(HUGE_VAL, HUGE_VAL, HUGE_VAL);
    } else {
        cull_box.Min = plot.RangeMin();
        cull_box.Max = plot.RangeMax();
    }

    const ImU32 no_col = IM_COL32(0, 0, 0, 0);
    for (int i = 0; i < getter.Count; i++) {
        ImPlot3DPoint p_plot = getter(i);
        if (!cull_box.Contains(p_plot))
            continue;
        ImPlot3DPoint p_ndc = PlotToNDC(p_plot);
        double z = GetPointDepth(p_plot);
        draw_list_3d.AddMarker(p_ndc, z, rend_fill ? col_fill : no_col, rend_line ? col_line : no_col, size, weight, marker);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] PlotScatter
//-----------------------------------------------------------------------------

template <typename Getter> void PlotScatterEx(const char* label_id, const Getter& getter, const ImPlot3DSpec& spec) {
    if (BeginItemEx(label_id, getter, spec, spec.MarkerLineColor, spec.Marker)) {
        const ImPlot3DNextItemData& n = GetItemData();
        const ImPlot3DSpec& s = n.Spec;
        ImPlot3DMarker marker = s.Marker == ImPlot3DMarker_None ? ImPlot3DMarker_Circle : s.Marker;
        const ImU32 col_line = ImGui::GetColorU32(s.MarkerLineColor);
        const ImU32 col_fill = ImGui::GetColorU32(s.MarkerFillColor);
        if (marker != ImPlot3DMarker_None)
            RenderMarkers<Getter>(getter, marker, s.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, s.LineWeight);
        EndItem();
    }
}

template <typename T> void PlotScatter(const char* label_id, const T* xs, const T* ys, const T* zs, int count, const ImPlot3DSpec& spec) {
    if (count < 1)
        return;
    int stride = Stride<T>(spec);
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(ys, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(zs, count, spec.Offset, stride), count);
    return PlotScatterEx(label_id, getter, spec);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotScatter<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int count, const ImPlot3DSpec& spec);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotLine
//-----------------------------------------------------------------------------

template <typename _Getter> void PlotLineEx(const char* label_id, const _Getter& getter, const ImPlot3DSpec& spec) {
    if (BeginItemEx(label_id, getter, spec, spec.LineColor, spec.Marker)) {
        const ImPlot3DNextItemData& n = GetItemData();
        const ImPlot3DSpec& s = n.Spec;

        if (getter.Count >= 2 && n.RenderLine) {
            const ImU32 col_line = ImGui::GetColorU32(s.LineColor);
            if (ImHasFlag(spec.Flags, ImPlot3DLineFlags_Segments)) {
                RenderLinePrimitives<RendererLineSegments>(getter, col_line, s.LineWeight);
            } else if (ImHasFlag(spec.Flags, ImPlot3DLineFlags_Loop)) {
                if (ImHasFlag(spec.Flags, ImPlot3DLineFlags_SkipNaN))
                    RenderLinePrimitives<RendererLineStripSkip>(GetterLoop<_Getter>(getter), col_line, s.LineWeight);
                else
                    RenderLinePrimitives<RendererLineStrip>(GetterLoop<_Getter>(getter), col_line, s.LineWeight);
            } else {
                if (ImHasFlag(spec.Flags, ImPlot3DLineFlags_SkipNaN))
                    RenderLinePrimitives<RendererLineStripSkip>(getter, col_line, s.LineWeight);
                else
                    RenderLinePrimitives<RendererLineStrip>(getter, col_line, s.LineWeight);
            }
        }

        // Render markers
        if (s.Marker != ImPlot3DMarker_None) {
            const ImU32 col_line = ImGui::GetColorU32(s.MarkerLineColor);
            const ImU32 col_fill = ImGui::GetColorU32(s.MarkerFillColor);
            RenderMarkers<_Getter>(getter, s.Marker, s.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, s.LineWeight);
        }
        EndItem();
    }
}

IMPLOT3D_TMP void PlotLine(const char* label_id, const T* xs, const T* ys, const T* zs, int count, const ImPlot3DSpec& spec) {
    if (count < 2)
        return;
    int stride = Stride<T>(spec);
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(ys, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(zs, count, spec.Offset, stride), count);
    return PlotLineEx(label_id, getter, spec);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotLine<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int count, const ImPlot3DSpec& spec);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotTriangle
//-----------------------------------------------------------------------------

template <typename _Getter> void PlotTriangleEx(const char* label_id, const _Getter& getter, const ImPlot3DSpec& spec) {
    if (BeginItemEx(label_id, getter, spec, spec.FillColor, spec.Marker)) {
        const ImPlot3DNextItemData& n = GetItemData();
        const ImPlot3DSpec& s = n.Spec;

        // Render fill
        if (getter.Count >= 3 && n.RenderFill && !ImHasFlag(spec.Flags, ImPlot3DTriangleFlags_NoFill)) {
            const ImU32 col_fill = ImGui::GetColorU32(s.FillColor);
            RenderPrimitives<RendererTriangleFill>(getter, col_fill);
        }

        // Render lines
        if (getter.Count >= 2 && n.RenderLine && !ImHasFlag(spec.Flags, ImPlot3DTriangleFlags_NoLines)) {
            const ImU32 col_line = ImGui::GetColorU32(s.LineColor);
            RenderLinePrimitives<RendererLineSegments>(GetterTriangleLines<_Getter>(getter), col_line, s.LineWeight);
        }

        // Render markers
        if (s.Marker != ImPlot3DMarker_None && !ImHasFlag(spec.Flags, ImPlot3DTriangleFlags_NoMarkers)) {
            const ImU32 col_line = ImGui::GetColorU32(s.MarkerLineColor);
            const ImU32 col_fill = ImGui::GetColorU32(s.MarkerFillColor);
            RenderMarkers<_Getter>(getter, s.Marker, s.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, s.LineWeight);
        }

        EndItem();
    }
}

IMPLOT3D_TMP void PlotTriangle(const char* label_id, const T* xs, const T* ys, const T* zs, int count, const ImPlot3DSpec& spec) {
    if (count < 3)
        return;
    int stride = Stride<T>(spec);
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(ys, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(zs, count, spec.Offset, stride), count);
    return PlotTriangleEx(label_id, getter, spec);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotTriangle<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int count, const ImPlot3DSpec& spec);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotQuad
//-----------------------------------------------------------------------------

template <typename _Getter> void PlotQuadEx(const char* label_id, const _Getter& getter, const ImPlot3DSpec& spec) {
    if (BeginItemEx(label_id, getter, spec, spec.FillColor, spec.Marker)) {
        const ImPlot3DNextItemData& n = GetItemData();
        const ImPlot3DSpec& s = n.Spec;

        // Render fill
        if (getter.Count >= 4 && n.RenderFill && !ImHasFlag(spec.Flags, ImPlot3DQuadFlags_NoFill)) {
            const ImU32 col_fill = ImGui::GetColorU32(s.FillColor);
            RenderPrimitives<RendererQuadFill>(getter, col_fill);
        }

        // Render lines
        if (getter.Count >= 2 && n.RenderLine && !ImHasFlag(spec.Flags, ImPlot3DQuadFlags_NoLines)) {
            const ImU32 col_line = ImGui::GetColorU32(s.LineColor);
            RenderLinePrimitives<RendererLineSegments>(GetterQuadLines<_Getter>(getter), col_line, s.LineWeight);
        }

        // Render markers
        if (s.Marker != ImPlot3DMarker_None && !ImHasFlag(spec.Flags, ImPlot3DQuadFlags_NoMarkers)) {
            const ImU32 col_line = ImGui::GetColorU32(s.MarkerLineColor);
            const ImU32 col_fill = ImGui::GetColorU32(s.MarkerFillColor);
            RenderMarkers<_Getter>(getter, s.Marker, s.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, s.LineWeight);
        }

        EndItem();
    }
}

IMPLOT3D_TMP void PlotQuad(const char* label_id, const T* xs, const T* ys, const T* zs, int count, const ImPlot3DSpec& spec) {
    if (count < 3)
        return;
    int stride = Stride<T>(spec);
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(ys, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(zs, count, spec.Offset, stride), count);
    return PlotQuadEx(label_id, getter, spec);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotQuad<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int count, const ImPlot3DSpec& spec);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotSurface
//-----------------------------------------------------------------------------

template <typename _Getter> void PlotSurfaceEx(const char* label_id, const _Getter& getter, int x_count, int y_count, double scale_min,
                                               double scale_max, const ImPlot3DSpec& spec) {
    if (BeginItemEx(label_id, getter, spec, spec.FillColor, spec.Marker)) {
        const ImPlot3DNextItemData& n = GetItemData();
        const ImPlot3DSpec& s = n.Spec;

        // Render fill
        if (getter.Count >= 4 && n.RenderFill && !ImHasFlag(spec.Flags, ImPlot3DSurfaceFlags_NoFill)) {
            const ImU32 col_fill = ImGui::GetColorU32(s.FillColor);
            RenderPrimitives<RendererSurfaceFill>(getter, x_count, y_count, col_fill, scale_min, scale_max);
        }

        // Render lines
        if (getter.Count >= 2 && n.RenderLine && !ImHasFlag(spec.Flags, ImPlot3DSurfaceFlags_NoLines)) {
            const ImU32 col_line = ImGui::GetColorU32(s.LineColor);
            RenderLinePrimitives<RendererLineSegments>(GetterSurfaceLines<_Getter>(getter, x_count, y_count), col_line, s.LineWeight);
        }

        // Render markers
        if (s.Marker != ImPlot3DMarker_None && !ImHasFlag(spec.Flags, ImPlot3DSurfaceFlags_NoMarkers)) {
            const ImU32 col_line = ImGui::GetColorU32(s.MarkerLineColor);
            const ImU32 col_fill = ImGui::GetColorU32(s.MarkerFillColor);
            RenderMarkers<_Getter>(getter, s.Marker, s.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, s.LineWeight);
        }

        EndItem();
    }
}

IMPLOT3D_TMP void PlotSurface(const char* label_id, const T* xs, const T* ys, const T* zs, int x_count, int y_count, double scale_min,
                              double scale_max, const ImPlot3DSpec& spec) {
    int count = x_count * y_count;
    if (count < 4)
        return;
    int stride = Stride<T>(spec);
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(ys, count, spec.Offset, stride),
                                                                  IndexerIdx<T>(zs, count, spec.Offset, stride), count);
    return PlotSurfaceEx(label_id, getter, x_count, y_count, scale_min, scale_max, spec);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotSurface<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int x_count, int y_count,                 \
                                              double scale_min, double scale_max, const ImPlot3DSpec& spec);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotMesh
//-----------------------------------------------------------------------------

void PlotMesh(const char* label_id, const ImPlot3DPoint* vtx, const unsigned int* idx, int vtx_count, int idx_count, const ImPlot3DSpec& spec) {
    Getter3DPoints getter(vtx, vtx_count);                     // Get vertices
    GetterMeshTriangles getter_triangles(vtx, idx, idx_count); // Get triangle vertices

    if (BeginItemEx(label_id, getter, spec, spec.FillColor, spec.Marker)) {
        const ImPlot3DNextItemData& n = GetItemData();
        const ImPlot3DSpec& s = n.Spec;

        // Render fill
        if (getter.Count >= 3 && n.RenderFill && !ImHasFlag(spec.Flags, ImPlot3DMeshFlags_NoFill)) {
            const ImU32 col_fill = ImGui::GetColorU32(s.FillColor);
            RenderPrimitives<RendererTriangleFill>(getter_triangles, col_fill);
        }

        // Render lines
        if (getter.Count >= 2 && n.RenderLine && !n.IsAutoLine && !ImHasFlag(spec.Flags, ImPlot3DMeshFlags_NoLines)) {
            const ImU32 col_line = ImGui::GetColorU32(s.LineColor);
            RenderLinePrimitives<RendererLineSegments>(GetterTriangleLines<GetterMeshTriangles>(getter_triangles), col_line, s.LineWeight);
        }

        // Render markers
        if (s.Marker != ImPlot3DMarker_None && !ImHasFlag(spec.Flags, ImPlot3DMeshFlags_NoMarkers)) {
            const ImU32 col_line = ImGui::GetColorU32(s.MarkerLineColor);
            const ImU32 col_fill = ImGui::GetColorU32(s.MarkerFillColor);
            RenderMarkers(getter, s.Marker, s.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, s.LineWeight);
        }

        EndItem();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] PlotImage
//-----------------------------------------------------------------------------

IMPLOT3D_API void PlotImage(const char* label_id, ImTextureRef tex_ref, const ImPlot3DPoint& center, const ImPlot3DPoint& axis_u,
                            const ImPlot3DPoint& axis_v, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImPlot3DSpec& spec) {
    // Compute corners from center and axes
    ImPlot3DPoint p0 = center - axis_u - axis_v; // Bottom-left
    ImPlot3DPoint p1 = center + axis_u - axis_v; // Bottom-right
    ImPlot3DPoint p2 = center + axis_u + axis_v; // Top-right
    ImPlot3DPoint p3 = center - axis_u + axis_v; // Top-left

    // Map ImPlot-style 2-point UVs into full 4-corner UVs
    ImVec2 uv_0 = uv0;
    ImVec2 uv_1 = ImVec2(uv1.x, uv0.y);
    ImVec2 uv_2 = uv1;
    ImVec2 uv_3 = ImVec2(uv0.x, uv1.y);

    // Delegate to full quad version
    PlotImage(label_id, tex_ref, p0, p1, p2, p3, uv_0, uv_1, uv_2, uv_3, tint_col, spec);
}

IMPLOT3D_API void PlotImage(const char* label_id, ImTextureRef tex_ref, const ImPlot3DPoint& p0, const ImPlot3DPoint& p1, const ImPlot3DPoint& p2,
                            const ImPlot3DPoint& p3, const ImVec2& uv0, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3,
                            const ImVec4& tint_col, const ImPlot3DSpec& spec) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PlotImage() needs to be called between BeginPlot() and EndPlot()!");
    SetupLock();

    ImPlot3DPoint corners[4] = {p0, p1, p2, p3};
    Getter3DPoints getter(corners, 4);

    // Invert Y from UVs
    ImVec2 uv_0 = ImVec2(uv0.x, 1 - uv0.y);
    ImVec2 uv_1 = ImVec2(uv1.x, 1 - uv1.y);
    ImVec2 uv_2 = ImVec2(uv2.x, 1 - uv2.y);
    ImVec2 uv_3 = ImVec2(uv3.x, 1 - uv3.y);

    if (BeginItemEx(label_id, getter, spec, tint_col, spec.Marker)) {
        ImU32 tint_col32 = ImGui::ColorConvertFloat4ToU32(tint_col);
        GetCurrentItem()->Color = tint_col32;

        // Render image
        bool is_transparent = (tint_col32 & IM_COL32_A_MASK) == 0;
        if (!is_transparent)
            RenderPrimitives<RendererQuadImage>(getter, tex_ref, uv_0, uv_1, uv_2, uv_3, tint_col32);

        EndItem();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] PlotText
//-----------------------------------------------------------------------------

void PlotText(const char* text, double x, double y, double z, double angle, const ImVec2& pix_offset) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PlotText() needs to be called between BeginPlot() and EndPlot()!");
    SetupLock();
    ImPlot3DPlot& plot = *gp.CurrentPlot;

    ImPlot3DBox cull_box;
    if (ImHasFlag(plot.Flags, ImPlot3DFlags_NoClip)) {
        cull_box.Min = ImPlot3DPoint(-HUGE_VAL, -HUGE_VAL, -HUGE_VAL);
        cull_box.Max = ImPlot3DPoint(HUGE_VAL, HUGE_VAL, HUGE_VAL);
    } else {
        cull_box.Min = plot.RangeMin();
        cull_box.Max = plot.RangeMax();
    }
    if (!cull_box.Contains(ImPlot3DPoint(x, y, z)))
        return;

    ImVec2 p = PlotToPixels(ImPlot3DPoint(x, y, z));
    p.x += pix_offset.x;
    p.y += pix_offset.y;
    AddTextRotated(GetPlotDrawList(), p, (float)angle, GetStyleColorU32(ImPlot3DCol_InlayText), text);
}

void PlotDummy(const char* label_id, const ImPlot3DSpec& spec) {
    if (BeginItem(label_id, spec, spec.LineColor, spec.Marker))
        EndItem();
}

} // namespace ImPlot3D

#endif // #ifndef IMGUI_DISABLE

//--------------------------------------------------
// ImPlot3D v0.1
// implot3d.h
// Date: 2024-11-16
// By brenocq
//
// Acknowledgments:
//  ImPlot3D is heavily inspired by ImPlot
//  (https://github.com/epezent/implot) by Evan Pezent,
//  and follows a similar code style and structure to
//  maintain consistency with ImPlot's API.
//--------------------------------------------------

// Table of Contents:
// [SECTION] Macros and Defines
// [SECTION] Forward declarations and basic types
// [SECTION] Context
// [SECTION] Begin/End Plot
// [SECTION] Setup
// [SECTION] Plot Items
// [SECTION] Plot Utils
// [SECTION] Miscellaneous
// [SECTION] Styles
// [SECTION] Demo
// [SECTION] Debugging
// [SECTION] Flags & Enumerations
// [SECTION] ImPlot3DPoint
// [SECTION] ImPlot3DRay
// [SECTION] ImPlot3DPlane
// [SECTION] ImPlot3DBox
// [SECTION] ImPlot3DQuat
// [SECTION] ImPlot3DStyle

#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#ifndef IMGUI_DISABLE

//-----------------------------------------------------------------------------
// [SECTION] Macros and Defines
//-----------------------------------------------------------------------------

#ifndef IMPLOT3D_API
#define IMPLOT3D_API
#endif

#define IMPLOT3D_VERSION "0.1"                // ImPlot3D version
#define IMPLOT3D_AUTO -1                      // Deduce variable automatically
#define IMPLOT3D_AUTO_COL ImVec4(0, 0, 0, -1) // Deduce color automatically
#define IMPLOT3D_TMP template <typename T> IMPLOT3D_API

//-----------------------------------------------------------------------------
// [SECTION] Forward declarations and basic types
//-----------------------------------------------------------------------------

// Forward declarations
struct ImPlot3DContext;
struct ImPlot3DStyle;
struct ImPlot3DPoint;
struct ImPlot3DRay;
struct ImPlot3DPlane;
struct ImPlot3DBox;
struct ImPlot3DRange;
struct ImPlot3DQuat;

// Enums
typedef int ImPlot3DCol;      // -> ImPlot3DCol_               // Enum: Styling colors
typedef int ImPlot3DMarker;   // -> ImPlot3DMarker_            // Enum: Marker styles
typedef int ImPlot3DLocation; // -> ImPlot3DLocation_          // Enum: Locations
typedef int ImAxis3D;         // -> ImAxis3D_                  // Enum: Axis indices

// Flags
typedef int ImPlot3DFlags;        // -> ImPlot3DFlags_         // Flags: for BeginPlot()
typedef int ImPlot3DItemFlags;    // -> ImPlot3DItemFlags_     // Flags: Item flags
typedef int ImPlot3DScatterFlags; // -> ImPlot3DScatterFlags_  // Flags: Scatter plot flags
typedef int ImPlot3DLineFlags;    // -> ImPlot3DLineFlags_     // Flags: Line plot flags
typedef int ImPlot3DLegendFlags;  // -> ImPlot3DLegendFlags_   // Flags: Legend flags
typedef int ImPlot3DAxisFlags;    // -> ImPlot3DAxisFlags_     // Flags: Axis flags

namespace ImPlot3D {

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------
IMPLOT3D_API ImPlot3DContext* CreateContext();
IMPLOT3D_API void DestroyContext(ImPlot3DContext* ctx = nullptr);
IMPLOT3D_API ImPlot3DContext* GetCurrentContext();
IMPLOT3D_API void SetCurrentContext(ImPlot3DContext* ctx);

//-----------------------------------------------------------------------------
// [SECTION] Begin/End Plot
//-----------------------------------------------------------------------------

// Starts a 3D plotting context. If this function returns true, EndPlot() MUST
// be called! You are encouraged to use the following convention:
//
// if (ImPlot3D::BeginPlot(...)) {
//     ImPlot3D::PlotLine(...);
//     ...
//     ImPlot3D::EndPlot();
// }
//
// Important notes:
// - #title_id must be unique to the current ImGui ID scope. If you need to avoid ID
//   collisions or don't want to display a title in the plot, use double hashes
//   (e.g. "MyPlot##HiddenIdText" or "##NoTitle").
// - #size is the **frame** size of the plot widget, not the plot area.
IMPLOT3D_API bool BeginPlot(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImPlot3DFlags flags = 0);
IMPLOT3D_API void EndPlot(); // Only call if BeginPlot() returns true!

//-----------------------------------------------------------------------------
// [SECTION] Setup
//-----------------------------------------------------------------------------

// The following API allows you to setup and customize various aspects of the
// current plot. The functions should be called immediately after BeginPlot()
// and before any other API calls. Typical usage is as follows:

// if (ImPlot3D::BeginPlot(...)) {                     1) Begin a new plot
//     ImPlot3D::SetupAxis(ImAxis3D_X, "My X-Axis");    2) Make Setup calls
//     ImPlot3D::SetupAxis(ImAxis3D_Y, "My Y-Axis");
//     ImPlot3D::SetupLegend(ImPlotLocation_North);
//     ...
//     ImPlot3D::SetupFinish();                        3) [Optional] Explicitly finish setup
//     ImPlot3D::PlotLine(...);                        4) Plot items
//     ...
//     ImPlot3D::EndPlot();                            5) End the plot
// }
//
// Important notes:
//
// - Always call Setup code at the top of your BeginPlot conditional statement.
// - Setup is locked once you start plotting or explicitly call SetupFinish.
//   Do NOT call Setup code after you begin plotting or after you make
//   any non-Setup API calls (e.g. utils like PlotToPixels also lock Setup).
// - Calling SetupFinish is OPTIONAL, but probably good practice. If you do not
//   call it yourself, then the first subsequent plotting or utility function will
//   call it for you.

// Enables an axis or sets the label and/or flags for an existing axis. Leave #label = nullptr for no label.
IMPLOT3D_API void SetupAxis(ImAxis3D axis, const char* label = nullptr, ImPlot3DAxisFlags flags = 0);

IMPLOT3D_API void SetupLegend(ImPlot3DLocation location, ImPlot3DLegendFlags flags = 0);

//-----------------------------------------------------------------------------
// [SECTION] Plot Items
//-----------------------------------------------------------------------------

IMPLOT3D_TMP void PlotScatter(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DScatterFlags flags = 0, int offset = 0, int stride = sizeof(T));

IMPLOT3D_TMP void PlotLine(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DLineFlags flags = 0, int offset = 0, int stride = sizeof(T));

//-----------------------------------------------------------------------------
// [SECTION] Plot Utils
//-----------------------------------------------------------------------------

// Convert a position in the current plot's coordinate system to pixels
IMPLOT3D_API ImVec2 PlotToPixels(const ImPlot3DPoint& point);
IMPLOT3D_API ImVec2 PlotToPixels(double x, double y, double z);
// Convert a pixel coordinate to a ray in the current plot's coordinate system
IMPLOT3D_API ImPlot3DRay PixelsToPlotRay(const ImVec2& pix);
IMPLOT3D_API ImPlot3DRay PixelsToPlotRay(double x, double y);

IMPLOT3D_API ImVec2 GetPlotPos();  // Get the current plot position (top-left) in pixels
IMPLOT3D_API ImVec2 GetPlotSize(); // Get the current plot size in pixels

//-----------------------------------------------------------------------------
// [SECTION] Miscellaneous
//-----------------------------------------------------------------------------

IMPLOT3D_API ImDrawList* GetPlotDrawList();

//-----------------------------------------------------------------------------
// [SECTION] Styles
//-----------------------------------------------------------------------------

// Get current style
IMPLOT3D_API ImPlot3DStyle& GetStyle();

// Set color styles
IMPLOT3D_API void StyleColorsAuto(ImPlot3DStyle* dst = nullptr);    // Set colors with ImGui style
IMPLOT3D_API void StyleColorsDark(ImPlot3DStyle* dst = nullptr);    // Set colors with dark style
IMPLOT3D_API void StyleColorsLight(ImPlot3DStyle* dst = nullptr);   // Set colors with light style
IMPLOT3D_API void StyleColorsClassic(ImPlot3DStyle* dst = nullptr); // Set colors with classic style

// Set the line color and weight for the next item only
IMPLOT3D_API void SetNextLineStyle(const ImVec4& col = IMPLOT3D_AUTO_COL, float weight = IMPLOT3D_AUTO);
// Set the marker style for the next item only
IMPLOT3D_API void SetNextMarkerStyle(ImPlot3DMarker marker = IMPLOT3D_AUTO, float size = IMPLOT3D_AUTO, const ImVec4& fill = IMPLOT3D_AUTO_COL, float weight = IMPLOT3D_AUTO, const ImVec4& outline = IMPLOT3D_AUTO_COL);

// Get color
IMPLOT3D_API ImVec4 GetStyleColorVec4(ImPlot3DCol idx);
IMPLOT3D_API ImU32 GetStyleColorU32(ImPlot3DCol idx);

//-----------------------------------------------------------------------------
// [SECTION] Demo
//-----------------------------------------------------------------------------
// Add implot3d_demo.cpp to your sources to use methods in this section

// Shows the ImPlot3D demo window
IMPLOT3D_API void ShowDemoWindow(bool* p_open = nullptr);

// Shows ImPlot3D style editor block (not a window)
IMPLOT3D_API void ShowStyleEditor(ImPlot3DStyle* ref = nullptr);

} // namespace ImPlot3D

//-----------------------------------------------------------------------------
// [SECTION] Flags & Enumerations
//-----------------------------------------------------------------------------

// Flags for ImPlot3D::BeginPlot()
enum ImPlot3DFlags_ {
    ImPlot3DFlags_None = 0,          // Default
    ImPlot3DFlags_NoTitle = 1 << 0,  // Hide plot title
    ImPlot3DFlags_NoLegend = 1 << 1, // Hide plot legend
    ImPlot3DFlags_NoClip = 1 << 2,   // Disable 3D box clipping
    ImPlot3DFlags_CanvasOnly = ImPlot3DFlags_NoTitle | ImPlot3DFlags_NoLegend,
};

enum ImPlot3DCol_ {
    // Item colors
    ImPlot3DCol_Line = 0,      // Line color
    ImPlot3DCol_MarkerOutline, // Marker outline color
    ImPlot3DCol_MarkerFill,    // Marker fill color
    // Plot colors
    ImPlot3DCol_TitleText,    // Title color
    ImPlot3DCol_FrameBg,      // Frame background color
    ImPlot3DCol_PlotBg,       // Plot area background color
    ImPlot3DCol_PlotBorder,   // Plot area border color
    ImPlot3DCol_LegendBg,     // Legend background color
    ImPlot3DCol_LegendBorder, // Legend border color
    ImPlot3DCol_LegendText,   // Legend text color
    ImPlot3DCol_COUNT,
};

enum ImPlot3DMarker_ {
    ImPlot3DMarker_None = -1, // No marker
    ImPlot3DMarker_Circle,    // Circle marker (default)
    ImPlot3DMarker_Square,    // Square maker
    ImPlot3DMarker_Diamond,   // Diamond marker
    ImPlot3DMarker_Up,        // Upward-pointing triangle marker
    ImPlot3DMarker_Down,      // Downward-pointing triangle marker
    ImPlot3DMarker_Left,      // Leftward-pointing triangle marker
    ImPlot3DMarker_Right,     // Rightward-pointing triangle marker
    ImPlot3DMarker_Cross,     // Cross marker (not fillable)
    ImPlot3DMarker_Plus,      // Plus marker (not fillable)
    ImPlot3DMarker_Asterisk,  // Asterisk marker (not fillable)
    ImPlot3DMarker_COUNT
};

// Flags for items
enum ImPlot3DItemFlags_ {
    ImPlot3DItemFlags_None = 0,          // Default
    ImPlot3DItemFlags_NoLegend = 1 << 0, // The item won't have a legend entry displayed
    ImPlot3DItemFlags_NoFit = 1 << 1,    // The item won't be considered for plot fits
};

// Flags for PlotScatter
enum ImPlot3DScatterFlags_ {
    ImPlot3DScatterFlags_None = 0, // Default
    ImPlot3DScatterFlags_NoLegend = ImPlot3DItemFlags_NoLegend,
    ImPlot3DScatterFlags_NoFit = ImPlot3DItemFlags_NoFit,
};

// Flags for PlotLine
enum ImPlot3DLineFlags_ {
    ImPlot3DLineFlags_None = 0, // Default
    ImPlot3DLineFlags_NoLegend = ImPlot3DItemFlags_NoLegend,
    ImPlot3DLineFlags_NoFit = ImPlot3DItemFlags_NoFit,
    ImPlot3DLineFlags_Segments = 1 << 10, // A line segment will be rendered from every two consecutive points
    ImPlot3DLineFlags_Loop = 1 << 11,     // The last and first point will be connected to form a closed loop
    ImPlot3DLineFlags_SkipNaN = 1 << 12,  // NaNs values will be skipped instead of rendered as missing data
};

// Flags for legends
enum ImPlot3DLegendFlags_ {
    ImPlot3DLegendFlags_None = 0,                 // Default
    ImPlot3DLegendFlags_NoButtons = 1 << 0,       // Legend icons will not function as hide/show buttons
    ImPlot3DLegendFlags_NoHighlightItem = 1 << 1, // Plot items will not be highlighted when their legend entry is hovered
    ImPlot3DLegendFlags_Horizontal = 1 << 2,      // Legend entries will be displayed horizontally
};

// Used to position legend on a plot
enum ImPlot3DLocation_ {
    ImPlot3DLocation_Center = 0,                                                 // Center-center
    ImPlot3DLocation_North = 1 << 0,                                             // Top-center
    ImPlot3DLocation_South = 1 << 1,                                             // Bottom-center
    ImPlot3DLocation_West = 1 << 2,                                              // Center-left
    ImPlot3DLocation_East = 1 << 3,                                              // Center-right
    ImPlot3DLocation_NorthWest = ImPlot3DLocation_North | ImPlot3DLocation_West, // Top-left
    ImPlot3DLocation_NorthEast = ImPlot3DLocation_North | ImPlot3DLocation_East, // Top-right
    ImPlot3DLocation_SouthWest = ImPlot3DLocation_South | ImPlot3DLocation_West, // Bottom-left
    ImPlot3DLocation_SouthEast = ImPlot3DLocation_South | ImPlot3DLocation_East  // Bottom-right
};

// Flags for axis
enum ImPlot3DAxisFlags_ {
    ImPlot3DAxisFlags_None = 0,             // Default
    ImPlot3DAxisFlags_NoGridLines = 1 << 0, // No grid lines will be displayed
};

// Axis indices
enum ImAxis3D_ {
    ImAxis3D_X = 0,
    ImAxis3D_Y,
    ImAxis3D_Z,
    ImAxis3D_COUNT,
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DPoint
//-----------------------------------------------------------------------------

// ImPlot3DPoint: 3D vector to store points in 3D
struct ImPlot3DPoint {
    float x, y, z;
    constexpr ImPlot3DPoint() : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr ImPlot3DPoint(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    // Accessors
    float& operator[](size_t idx) {
        IM_ASSERT(idx == 0 || idx == 1 || idx == 2);
        return ((float*)(void*)(char*)this)[idx];
    }
    float operator[](size_t idx) const {
        IM_ASSERT(idx == 0 || idx == 1 || idx == 2);
        return ((const float*)(const void*)(const char*)this)[idx];
    }

    // Binary operators
    ImPlot3DPoint operator*(float rhs) const;
    ImPlot3DPoint operator/(float rhs) const;
    ImPlot3DPoint operator+(const ImPlot3DPoint& rhs) const;
    ImPlot3DPoint operator-(const ImPlot3DPoint& rhs) const;
    ImPlot3DPoint operator*(const ImPlot3DPoint& rhs) const;
    ImPlot3DPoint operator/(const ImPlot3DPoint& rhs) const;

    // Unary operator
    ImPlot3DPoint operator-() const;

    // Compound assignment operators
    ImPlot3DPoint& operator*=(float rhs);
    ImPlot3DPoint& operator/=(float rhs);
    ImPlot3DPoint& operator+=(const ImPlot3DPoint& rhs);
    ImPlot3DPoint& operator-=(const ImPlot3DPoint& rhs);
    ImPlot3DPoint& operator*=(const ImPlot3DPoint& rhs);
    ImPlot3DPoint& operator/=(const ImPlot3DPoint& rhs);

    // Comparison operators
    bool operator==(const ImPlot3DPoint& rhs) const;
    bool operator!=(const ImPlot3DPoint& rhs) const;

    // Dot product
    float Dot(const ImPlot3DPoint& rhs) const;

    // Cross product
    ImPlot3DPoint Cross(const ImPlot3DPoint& rhs) const;

    // Get vector magnitude
    float Magnitude() const;

    // Normalize to unit length
    void Normalize();

    // Return vector normalized to unit length
    ImPlot3DPoint Normalized() const;

    // Friend binary operators to allow commutative behavior
    friend ImPlot3DPoint operator*(float lhs, const ImPlot3DPoint& rhs);

#ifdef IMPLOT3D_POINT_CLASS_EXTRA
    IMPLOT3D_POINT_CLASS_EXTRA // Define additional constructors and implicit cast operators in imconfig.h to convert back and forth between your math types and ImPlot3DPoint
#endif
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DRay
//-----------------------------------------------------------------------------

struct ImPlot3DRay {
    ImPlot3DPoint Origin;
    ImPlot3DPoint Direction;
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DPlane
//-----------------------------------------------------------------------------

struct ImPlot3DPlane {
    ImPlot3DPoint Point;
    ImPlot3DPoint Normal;
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DBox
//-----------------------------------------------------------------------------

struct ImPlot3DBox {
    ImPlot3DPoint Min;
    ImPlot3DPoint Max;

    // Default constructor
    constexpr ImPlot3DBox() : Min(ImPlot3DPoint()), Max(ImPlot3DPoint()) {}

    // Constructor with two points
    constexpr ImPlot3DBox(const ImPlot3DPoint& min, const ImPlot3DPoint& max) : Min(min), Max(max) {}

    // Method to expand the box to include a point
    void Expand(const ImPlot3DPoint& point);

    // Method to check if a point is inside the box
    bool Contains(const ImPlot3DPoint& point) const;

    // Method to clip a line segment against the box
    bool ClipLineSegment(const ImPlot3DPoint& p0, const ImPlot3DPoint& p1, ImPlot3DPoint& p0_clipped, ImPlot3DPoint& p1_clipped) const;
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DRange
//-----------------------------------------------------------------------------

struct ImPlot3DRange {
    float Min;
    float Max;

    constexpr ImPlot3DRange() : Min(0.0f), Max(0.0f) {}
    constexpr ImPlot3DRange(float min, float max) : Min(min), Max(max) {}

    void Expand(float value);
    bool Contains(float value) const;
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DQuat
//-----------------------------------------------------------------------------

struct ImPlot3DQuat {
    float x, y, z, w;

    // Constructors
    constexpr ImPlot3DQuat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    constexpr ImPlot3DQuat(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

    ImPlot3DQuat(float _angle, const ImPlot3DPoint& _axis);

    // Get quaternion magnitude
    float Magnitude() const;

    // Get normalized quaternion
    ImPlot3DQuat Normalized() const;

    // Conjugate of the quaternion
    ImPlot3DQuat Conjugate() const;

    // Inverse of the quaternion
    ImPlot3DQuat Inverse() const;

    // Binary operators
    ImPlot3DQuat operator*(const ImPlot3DQuat& rhs) const;

    // Normalize the quaternion in place
    ImPlot3DQuat& Normalize();

    // Rotate a 3D point using the quaternion
    ImPlot3DPoint operator*(const ImPlot3DPoint& point) const;

    // Comparison operators
    bool operator==(const ImPlot3DQuat& rhs) const;
    bool operator!=(const ImPlot3DQuat& rhs) const;

#ifdef IMPLOT3D_QUAT_CLASS_EXTRA
    IMPLOT3D_QUAT_CLASS_EXTRA // Define additional constructors and implicit cast operators in imconfig.h to convert back and forth between your math types and ImPlot3DQuat
#endif
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DStyle
//-----------------------------------------------------------------------------

struct ImPlot3DStyle {
    // Item style
    float LineWeight;   // Line weight in pixels
    int Marker;         // Default marker type (ImPlot3DMarker_None)
    float MarkerSize;   // Marker size in pixels (roughly the marker's "radius")
    float MarkerWeight; // Marker outline weight in pixels
    // Plot style
    ImVec2 PlotDefaultSize;
    ImVec2 PlotMinSize;
    ImVec2 PlotPadding;
    ImVec2 LabelPadding;
    // Legend style
    ImVec2 LegendPadding;      // Legend padding from plot edges
    ImVec2 LegendInnerPadding; // Legend inner padding from legend edges
    ImVec2 LegendSpacing;      // Spacing between legend entries
    // Colors
    ImVec4 Colors[ImPlot3DCol_COUNT];
    // Constructor
    IMPLOT3D_API ImPlot3DStyle();
};

#endif // #ifndef IMGUI_DISABLE

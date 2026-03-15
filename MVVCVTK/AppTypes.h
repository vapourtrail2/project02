#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class VizMode {
    Volume,
    IsoSurface,
    SliceAxial,
    SliceCoronal,
    SliceSagittal,
    CompositeVolume,
    CompositeIsoSurface
};

enum class ToolMode {
    Navigation,
    DistanceMeasure,
    AngleMeasure,
    ModelTransform
};

enum class LoadState {
    Idle,
    Loading,
    Succeeded,
    Failed
};

struct TFNode {
    double position;
    double opacity;
    double r;
    double g;
    double b;
};

struct MaterialParams {
    double ambient = 0.1;
    double diffuse = 0.7;
    double specular = 0.2;
    double specularPower = 10.0;
    double opacity = 1.0;
    bool shadeOn = false;
};

struct BackgroundColor {
    double r = 0.1;
    double g = 0.1;
    double b = 0.1;
};

struct WindowLevelParams {
    double windowWidth = 400.0;
    double windowCenter = 40.0;
};

enum class IsoRenderQuality {
    Fast,
    HighQuality
};

enum class UpdateFlags : int {
    None = 0,
    Cursor = 1 << 0,
    TF = 1 << 1,
    IsoValue = 1 << 2,
    Material = 1 << 3,
    Interaction = 1 << 4,
    Transform = 1 << 5,
    DataReady = 1 << 6,
    LoadFailed = 1 << 7,
    Background = 1 << 8,
    Visibility = 1 << 9,
    WindowLevel = 1 << 10,
    IsoQuality = 1 << 11,
    RenderMode = 1 << 12,
    All = Cursor | TF | IsoValue | Material | Interaction | Transform | WindowLevel | IsoQuality | RenderMode | Visibility
};

inline UpdateFlags operator|(UpdateFlags a, UpdateFlags b)
{
    return static_cast<UpdateFlags>(static_cast<int>(a) | static_cast<int>(b));
}

inline UpdateFlags operator&(UpdateFlags a, UpdateFlags b)
{
    return static_cast<UpdateFlags>(static_cast<int>(a) & static_cast<int>(b));
}

inline UpdateFlags& operator|=(UpdateFlags& a, UpdateFlags b)
{
    a = a | b;
    return a;
}

inline bool HasFlag(UpdateFlags flags, UpdateFlags bit)
{
    return (static_cast<int>(flags) & static_cast<int>(bit)) != 0;
}

namespace VisFlags {
constexpr std::uint32_t ClipPlanes = 1u << 0;
constexpr std::uint32_t Crosshair = 1u << 1;
constexpr std::uint32_t RulerAxes = 1u << 2;
}

struct RenderParams {
    std::array<int, 3> cursor = { 0, 0, 0 };
    std::vector<TFNode> tfNodes;
    double scalarRange[2] = { 0.0, 255.0 };
    MaterialParams material;
    double isoValue = 0.0;
    IsoRenderQuality isoRenderQuality = IsoRenderQuality::Fast;
    VizMode primary3DMode = VizMode::CompositeIsoSurface;
    WindowLevelParams windowLevel;
    std::array<double, 16> modelMatrix = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    BackgroundColor background;
    std::uint32_t visibilityMask =
        VisFlags::ClipPlanes |
        VisFlags::Crosshair |
        VisFlags::RulerAxes;
};

enum class Orientation { AXIAL = 2, CORONAL = 1, SAGITTAL = 0 };

struct PreInitConfig {
    VizMode vizMode = VizMode::IsoSurface;
    MaterialParams material;
    std::vector<TFNode> tfNodes;
    double isoThreshold = 0.0;
    IsoRenderQuality isoRenderQuality = IsoRenderQuality::Fast;
    VizMode primary3DMode = VizMode::CompositeIsoSurface;
    BackgroundColor bgColor;
    WindowLevelParams windowLevel;
    bool hasTF = false;
    bool hasIso = false;
    bool hasIsoQuality = false;
    bool hasPrimary3DMode = false;
    bool hasBgColor = false;
    bool hasWindowLevel = false;
};

struct WindowConfig {
    std::string title;
    int width = 600;
    int height = 600;
    int posX = 0;
    int posY = 0;
    VizMode vizMode = VizMode::SliceAxial;
    bool showAxes = false;
    PreInitConfig preInitCfg;
};



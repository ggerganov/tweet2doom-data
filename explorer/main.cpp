#include "build_timestamp.h"

#include "common.h"

#include "core/assets.h"

#include "imgui-extra/imgui_impl.h"
#include "imgui/imgui_internal.h"

#ifndef ICON_FA_COGS
#include "icons_font_awesome.h"
#endif

#ifdef USE_LINE_SHADER
#include "core/frame-buffer.h"
#include "core/shader-program.h"
#endif

#include <SDL.h>
#include <SDL_opengl.h>

#include <set>
#include <cmath>
#include <fstream>
#include <vector>
#include <functional>
#include <unordered_map>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "imgui_helpers.h"

// Constants

const float kFontScale = 2.0f;
const float kSizeX0 = 1000.0f;
const float kStepPos = 100.0f;
const float kAnimTime = 0.25f;
const float kWindowFadeTime = 0.25f;

const auto kColorBackground      = ImGui::ColorConvertFloat4ToU32({ float(0x0A)/256.0f, float(0x10)/256.0f, float(0x16)/256.0f, 0.50f });
const auto kColorEdge            = ImGui::ColorConvertFloat4ToU32({ float(0x1D)/256.0f, float(0xA1)/256.0f, float(0xF2)/256.0f, 0.40f });
const auto kColorEdgeSelected    = ImGui::ColorConvertFloat4ToU32({ float(0x1D)/256.0f, float(0xA1)/256.0f, float(0xF2)/256.0f, 0.80f });
const auto kColorNode            = ImGui::ColorConvertFloat4ToU32({ float(0x00)/256.0f, float(0xFF)/256.0f, float(0x7D)/256.0f, 0.80f });
const auto kColorNodeSelected    = ImGui::ColorConvertFloat4ToU32({ float(0x00)/256.0f, float(0xFF)/256.0f, float(0x7D)/256.0f, 1.00f });
const auto kColorCommand         = ImGui::ColorConvertFloat4ToU32({ float(0x1D)/256.0f, float(0xA1)/256.0f, float(0xA2)/256.0f, 0.40f });
const auto kColorCommandSelected = ImGui::ColorConvertFloat4ToU32({ float(0x1D)/256.0f, float(0xA1)/256.0f, float(0xA2)/256.0f, 0.80f });

namespace {

template <typename T>
int sgn(T x) { return x < 0 ? -1 : x > 0 ? 1 : 0; }

bool ScrollWhenDraggingOnVoid(ImVec2 delta, ImGuiMouseButton mouse_button) {
    if (ImGui::GetIO().MouseDownDuration[ImGuiMouseButton_Left] == 0.0f) {
        delta = { 0.0f, 0.0f, };
    }
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = g.CurrentWindow;
    bool hovered = false;
    bool held = false;
    bool dragging = false;
    ImGuiButtonFlags button_flags = (mouse_button == 0) ? ImGuiButtonFlags_MouseButtonLeft : (mouse_button == 1) ? ImGuiButtonFlags_MouseButtonRight : ImGuiButtonFlags_MouseButtonMiddle;
    if (g.HoveredId == 0) // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
        ImGui::ButtonBehavior(window->Rect(), window->GetID("##scrolldraggingoverlay"), &hovered, &held, button_flags);
    if (held && delta.x != 0.0f) {
        ImGui::SetScrollX(window, window->Scroll.x + delta.x);
    }
    if (held && delta.y != 0.0f) {
        auto dy = delta.y;
        // fix vertical scroll snapping:
        if (window->Scroll.y == 0.0f && delta.y > 0.0f && delta.y < 11.0f) dy = 11.0f;
        ImGui::SetScrollY(window, window->Scroll.y + dy);
        dragging = true;
    }
    return dragging;
}

}

using NodeId = int64_t;

static std::function<bool()> g_doInit;
static std::function<void(int, int)> g_setWindowSize;
static std::function<void(float, float, float, int)> g_setPinch;
static std::function<bool()> g_mainUpdate;
static std::function<void(const NodeId & , const std::string & , int, int, int, int)> g_addNode;
static std::function<void(const NodeId & , int, int)> g_updateNodePosition;
static std::function<void(const NodeId & , const NodeId & )> g_addEdge;
static std::function<void(const NodeId & )> g_focusNode;
static std::function<std::string()> g_getActionOpenUrl;
static std::function<void()> g_treeChanged;

void mainUpdate(void *) {
    g_mainUpdate();
}

// JS interface

extern "C" {
    EMSCRIPTEN_KEEPALIVE
        int do_init() {
            return g_doInit();
        }

    EMSCRIPTEN_KEEPALIVE
        void set_window_size(int sizeX, int sizeY) {
            g_setWindowSize(sizeX, sizeY);
        }

    EMSCRIPTEN_KEEPALIVE
        void set_pinch(float x, float y, float scale, int type) {
            g_setPinch(x, y, scale, type);
        }

    EMSCRIPTEN_KEEPALIVE
        void tree_changed() {
            g_treeChanged();
        }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(tweet2doom) {
    emscripten::function("add_node", emscripten::optional_override(
                    [](const std::string & id, const std::string & username, int level, int type, int x, int y) {
                        g_addNode(std::stoll(id), username, level, type, x, y);
                    }));

    emscripten::function("add_edge", emscripten::optional_override(
                    [](const std::string & src, const std::string & dst) {
                        g_addEdge(std::stoll(src), std::stoll(dst));
                    }));

    emscripten::function("update_node_position", emscripten::optional_override(
                    [](const std::string & id, int x, int y) {
                        g_updateNodePosition(std::stoll(id), x, y);
                    }));

    emscripten::function("focus_node", emscripten::optional_override(
                    [](const std::string & id) {
                        if (id.empty()) return;
                        g_focusNode(std::stoll(id));
                    }));

    emscripten::function("get_action_open_url", emscripten::optional_override(
                    []() {
                        return g_getActionOpenUrl();
                    }));
}
#endif

// Core

struct Node {
    NodeId id;
    NodeId parentId;
    std::string username;
    int level;
    int type;
    float x;
    float y;
};

struct Edge {
    NodeId src;
    NodeId dst;
};

const auto kZoomMin = 0.1f;
const auto kZoomMax = 1.0f;
const auto kZoomMinLog = std::log(kZoomMin);
const auto kZoomMaxLog = std::log(kZoomMax);

float zoomFromLog(float x) {
    return std::exp(kZoomMinLog + x*(kZoomMaxLog - kZoomMinLog));
}

enum class EWindowKind {
    None,
    Help,
    Statistics,
    Achievements,
};

enum class EAchievementType {
    LevelCompleted,
    Speedrun,
};

struct View {
    float x;
    float y;
    float z;
};

struct Animation {
    float t0;
    float t1;

    View v0;
    View v1;

    int type;
};

struct Rendering {
    float T;

    ImVec2 wSize;

    bool isAnimating;

    float scale;
    float iscale;

    float xmin;
    float ymin;
    float xmax;
    float ymax;

    float dx;
    float dy;
    float idx;
    float idy;

    float textHScaled;
};

struct Achievement {
    NodeId id;
    NodeId announcmentId;
    EAchievementType type;
    std::string desc;
};

std::unordered_map<int64_t, Node> g_nodes;
std::vector<Edge> g_edges;

std::map<NodeId, Achievement> g_achievementsMap;
std::vector<Achievement> g_achievements = {
    { 1451989230201315328, 1452303875990593539, EAchievementType::Speedrun,       "E1M2 Best time 0:31", },
    { 1451577865045254145, 1451579974234824708, EAchievementType::Speedrun,       "E1M1 Best time 0:10", },
    { 1451135258993250309, 1451216719566024705, EAchievementType::LevelCompleted, "E1M5 Completed", },
    { 1450067719299215360, 1450491942119243776, EAchievementType::LevelCompleted, "E1M4 Completed", },
    { 1449750989595152391, 1450129558255009792, EAchievementType::LevelCompleted, "E1M3 Completed", },
    { 1449678007149416449, 1449767161103323142, EAchievementType::LevelCompleted, "E1M2 Completed", },
    { 1449157956096991234, 1449404776286810113, EAchievementType::Speedrun,       "E1M1 Best time 0:11", },
    { 1448867734671040514, 1449042393391673351, EAchievementType::Speedrun,       "E1M1 Best time 0:16", },
    { 1447603566877757445, 1447940128735961108, EAchievementType::Speedrun,       "E1M1 Best time 0:19", },
    { 1445839902130769921, 1445845544748740612, EAchievementType::LevelCompleted, "E1M1 Completed", },
};

struct State {
    // bounding box of the nodes
    float bbxmin = 1e10;
    float bbxmax = -1e10;

    float bbymin = 1e10;
    float bbymax = -1e10;

    // viewport size at zoom = 1.0f
    float sizex0 = kSizeX0;
    float sizey0 = 0.0f;

    float sceneScale = 1.0;

    // pinch
    float pinchPosX1 = 0.0f;
    float pinchPosY1 = 0.0f;
    View viewPinch;
    float pinchScale = 1.0f;

    View viewCur;
    Animation anim;

    // used to render the line shader texture
    // stores infro on time of rendering
    float posEdgesX = 0.0f;
    float posEdgesY = 0.0f;
    float scaleEdges = 0.0f;

    float aspectRatio = 1.0f;

    NodeId rootId;
    NodeId focusId;
    NodeId selectedId;

    int nUpdates = 2;

    bool isMoving = false;
    bool wasMoving = true;
    bool isZooming = false;
    bool wasZooming = true;
    bool treeChanged = false;
    bool isFirstChange = true;
    bool isMouseDown = false;
    bool isPinching = false;
    bool isPanning = false;
    bool isPopupOpen = false;
    bool doSelect = false;

    float mouseDownX = 0.0f;
    float mouseDownY = 0.0f;
    float posDownX = 0.0f;
    float posDownY = 0.0f;

    float heightControls = 0.0f;

    // stats
    int statsNumNodesRendered = 0;
    int statsNumCommandsRendered = 0;
    int statsNumEdgesRendered = 0;
    int statsNumUniquePlayers = 0;

    // popup
    ImVec2 popupPos;
    ImVec2 popupSize;
    float popupShowT0 = 0.0f;

    // rendering
    bool forceRender = false;
    int nSkipUpdate = 0;
    Rendering rendering;

    // windows
    bool windowShow = false;
    float windowShowT0 = 0.0f;
    EWindowKind windowKind = EWindowKind::None;

    // rendering options
    float renderingEdgesMinZ = 0.0f;

    // open action
    std::string actionOpenUrl = "";

    ::ImVid::Assets assets;

#ifdef USE_LINE_SHADER
    ::ImVid::FrameBuffer fboEdges;
    ::ImVid::ShaderProgram shaderEdges;
#endif

    void initRendering() {
        rendering.T = ImGui::GetTime();
        rendering.wSize = ImGui::GetContentRegionAvail();

        rendering.isAnimating = rendering.T < anim.t1;

        rendering.scale = getScale(viewCur.z);
        rendering.iscale = (1.0/rendering.scale)/ImGui::GetIO().DisplayFramebufferScale.x;

        rendering.xmin = viewCur.x - 0.5*sizex0*rendering.scale;
        rendering.ymin = viewCur.y - 0.5*sizey0*rendering.scale;
        rendering.xmax = viewCur.x + 0.5*sizex0*rendering.scale;
        rendering.ymax = viewCur.y + 0.5*sizey0*rendering.scale;

        rendering.dx = (rendering.xmax - rendering.xmin);
        rendering.dy = (rendering.ymax - rendering.ymin);
        rendering.idx = 1.0f/rendering.dx;
        rendering.idy = 1.0f/rendering.dy;
    }

    inline float getScale(float z) const {
        return 0.5 + (1.0 - zoomFromLog(z))*sceneScale;
    }

    void onWindowResize() {
        sceneScale = std::max(
                1.1*(((bbxmax - bbxmin) / sizex0 - 1.0) / 0.9),
                1.1*(((bbymax - bbymin) / sizey0 - 1.0) / 0.9));
    }

    inline bool isMouseInMainCanvas() const {
        return (ImGui::GetIO().MousePos.y > 0.625f*heightControls &&
                ImGui::GetIO().MousePos.y < ImGui::GetIO().DisplaySize.y - 1.25f*heightControls);
    }

    inline ImVec2 getRenderPosition(const Node & node) const {
        return ImVec2{
            (node.x - rendering.xmin)*rendering.idx*rendering.wSize.x,
                (node.y - rendering.ymin)*rendering.idy*rendering.wSize.y,
        };
    }

    inline float getRenderRadius(const Node & node) const {
        return std::max(0.5f, (node.type == 0 ? 92.0f : 32.0f)*rendering.iscale);
    }

    inline auto getRenderCommand(const Node & node, const ImVec2 & pos) const {
        const ImVec2 tSize = { ImGui::CalcTextSize(node.username.c_str()).x, rendering.textHScaled };
        const ImVec2 tMargin = { 12.0f*rendering.iscale, 8.0f*rendering.iscale, };
        const ImVec2 pt = { pos.x - 0.5f*tSize.x, pos.y - 0.5f*tSize.y, };
        const ImVec2 p0 = { pos.x - 0.5f*tSize.x - tMargin.x, pos.y - 0.5f*tSize.y - tMargin.y, };
        const ImVec2 p1 = { pos.x + 0.5f*tSize.x + tMargin.x, pos.y + 0.5f*tSize.y + tMargin.y, };

        return std::tuple { pt, p0, p1 };
    }

    inline void focusNode(const NodeId & id, bool zoomOut) {
        focusId = id;
        selectedId = id;

        anim.v1.x = g_nodes[id].x;
        anim.v1.y = g_nodes[id].y;
        anim.v1.z = 0.999f;

        anim.t0 = rendering.T + 0.0f;
        anim.t1 = rendering.T + 3.0f;
        anim.v0 = viewCur;

        anim.type = zoomOut ? 4 : 3;
    }
};

State g_state;

void loadData() {
    const std::string kPath = "../data/";

    printf("Loading data from '%s'\n", kPath.c_str());

    {
        int n = 0;
        const auto fname = kPath + "nodes.dat";
        std::ifstream fin(fname);
        while (true) {
            Node cur;
            fin >> cur.id >> cur.username >> cur.level >> cur.type;

            if (fin.eof()) break;

            g_addNode(cur.id, cur.username.c_str(), cur.level, cur.type, 0, 0);
            ++n;
        }
        printf("Loaded %d entries from '%s'\n", n, fname.c_str());
        fin.close();
    }

    {
        int n = 0;
        const auto fname = kPath + "coordinates.dat";
        std::ifstream fin(fname);
        while (true) {
            Node cur;
            fin >> cur.id >> cur.x >> cur.y;

            if (fin.eof()) break;

            g_updateNodePosition(cur.id, cur.x, cur.y);
            ++n;
        }
        printf("Loaded %d entries from '%s'\n", n, fname.c_str());
        fin.close();
    }

    {
        int n = 0;
        const auto fname = kPath + "edges.dat";
        std::ifstream fin(fname);
        while (true) {
            Edge cur;
            fin >> cur.src >> cur.dst;

            if (fin.eof()) break;

            g_addEdge(cur.src, cur.dst);
            ++n;
        }
        printf("Loaded %d entries from '%s'\n", n, fname.c_str());
        fin.close();
    }

    g_treeChanged();
}

void initMain() {
}

void renderMain() {
    ImGui::NewFrame();

    static bool isFirstFrame = true;
    if (isFirstFrame) {
        ImGui_SetStyle();

        ImGuiStyle & style = ImGui::GetStyle();
        style.Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(kColorNode);

        isFirstFrame = false;
    }

    auto & style = ImGui::GetStyle();

    const auto saveWindowPadding = style.WindowPadding;
    const auto saveWindowBorderSize = style.WindowBorderSize;

    style.WindowPadding.x = 0.0f;
    style.WindowPadding.y = 0.0f;
    style.WindowBorderSize = 0.0f;

    ImGui::SetNextWindowPos({ 0.0f, 0.0f });
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("main", NULL,
                 ImGuiWindowFlags_NoNav |
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_NoBackground);

    style.WindowPadding = saveWindowPadding;
    style.WindowBorderSize = saveWindowBorderSize;

    auto drawList = ImGui::GetWindowDrawList();

    g_state.initRendering();

    const auto & T = g_state.rendering.T;
    const auto & wSize = g_state.rendering.wSize;

    const auto & isAnimating = g_state.rendering.isAnimating;

    const auto & scale  = g_state.rendering.scale;
    const auto & iscale = g_state.rendering.iscale;

    g_state.statsNumEdgesRendered = 0;
    g_state.statsNumNodesRendered = 0;
    g_state.statsNumCommandsRendered = 0;

    // background
    drawList->AddRectFilled({ 0.0f, 0.0f }, wSize, kColorBackground);

    if (g_state.viewCur.z >= g_state.renderingEdgesMinZ) {
#ifdef USE_LINE_SHADER
        // shader-based line rendering
        const float f = scale/g_state.scaleEdges;
        const float ds = 0.5*(1.0 - f);
        const float dx = f*(g_state.viewCur.x - g_state.posEdgesX)*g_state.rendering.idx;
        const float dy = f*(g_state.viewCur.y - g_state.posEdgesY)*g_state.rendering.idy;

        const float x0 = ds + dx;
        const float x1 = 1.0f - ds + dx;

        const float y0 = ds + dy;
        const float y1 = 1.0f - ds + dy;

        const float px0 = 0.25 + 0.25*(x0 + x1) - 0.25*(x1 - x0);
        const float px1 = 0.25 + 0.25*(x0 + x1) + 0.25*(x1 - x0);
        const float py0 = 0.25 + 0.25*(y0 + y1) - 0.25*(y1 - y0);
        const float py1 = 0.25 + 0.25*(y0 + y1) + 0.25*(y1 - y0);

        ImGui::SetCursorScreenPos({ 0.0f, 0.0f, });
        ImGui::Image((void *)(intptr_t) g_state.fboEdges.getIdTex(), ImGui::GetContentRegionAvail(), { px0, py0, }, { px1, py1, });
#else
        // imgui line rendering
        const auto thickness = std::max(0.1, 2.0*iscale);

        for (const auto & edge : g_edges) {
            const auto col = (edge.src == g_state.selectedId || edge.dst == g_state.selectedId) ?  kColorEdgeSelected : kColorEdge;

            const auto p0 = g_state.getRenderPosition(g_nodes[edge.src]);
            const auto p1 = g_state.getRenderPosition(g_nodes[edge.dst]);

            const float cull = 1000.0f;

            const float pmaxx = std::max(p0.x, p1.x);
            const float pminx = std::min(p0.x, p1.x);
            const float pmaxy = std::max(p0.y, p1.y);
            const float pminy = std::min(p0.y, p1.y);

            if (pmaxx < -cull || pminx > wSize.x + cull ||
                pmaxy < -cull || pminy > wSize.y + cull) {
                continue;
            }

            drawList->AddLine(p0, p1, col, thickness);
            g_state.statsNumEdgesRendered++;
        }
#endif
    }

    ImGui::SetWindowFontScale(1.0f*iscale/kFontScale);
    g_state.rendering.textHScaled = ImGui::CalcTextSize("X").y;

    // render nodes
    {
        if (g_state.viewCur.z > 0.900f) {
            drawList->PushTextureID((void *)(intptr_t) g_state.assets.getTexId(::ImVid::Assets::ICON_T2D_SMALL_BLUR));
        }

        for (const auto & [id, node] : g_nodes) {
            if (node.type == 2) continue;

            const auto pos = g_state.getRenderPosition(node);
            const auto radius = g_state.getRenderRadius(node);

            if (pos.x < -2.0*radius || pos.x > wSize.x + 2.0*radius) continue;
            if (pos.y < -2.0*radius || pos.y > wSize.y + 2.0*radius) continue;

            const auto col = kColorNode;

            const ImVec2 h0 = { pos.x - 2.0f*radius, pos.y - 2.0f*radius };
            const ImVec2 h1 = { pos.x + 2.0f*radius, pos.y + 2.0f*radius };

            if (node.type == 0) {
                const float w = (1.8f*radius);
                const float h = (3.2f*radius);

                ImGui::SetCursorScreenPos({ pos.x - w, pos.y - h, });
                ImGui::Image((void *)(intptr_t) g_state.assets.getTexId(::ImVid::Assets::ICON_T2D_BIG), { 2.0f*w, 2.0f*h });
            } else {
                if (g_state.viewCur.z > 0.900f) {
                    const float w = (1.0f*radius);
                    const float h = (1.0f*radius);

                    ImGui::SetCursorScreenPos({ pos.x - w, pos.y - h, });
                    if (id == g_state.selectedId) {
                        ImGui::Image((void *)(intptr_t) g_state.assets.getTexId(::ImVid::Assets::ICON_T2D_SMALL_BLUR), { 2.0f*w, 2.0f*h }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, ImGui::ColorConvertU32ToFloat4(kColorNodeSelected));
                    } else {
                        ImGui::Image((void *)(intptr_t) g_state.assets.getTexId(::ImVid::Assets::ICON_T2D_SMALL_BLUR), { 2.0f*w, 2.0f*h }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, ImGui::ColorConvertU32ToFloat4(kColorNode));
                    }
                } else if (g_state.viewCur.z > 0.500f) {
                    drawList->AddCircleFilled(pos, radius, col);
                } else {
                    drawList->AddRectFilled({ float(pos.x - radius), float(pos.y - radius) }, { float(pos.x + radius), float(pos.y + radius) }, col);
                }
            }
            g_state.statsNumNodesRendered++;

            if (g_state.viewCur.z > 0.90 && g_state.isPopupOpen == false) {
                if (g_state.isMouseInMainCanvas()) {
                    if (ImGui::IsMouseHoveringRect(h0, h1, true)) {
                        if (ImGui::IsMouseReleased(0) && g_state.isPanning == false && isAnimating == false && g_state.windowShow == false) {
                            ImGui::SetNextWindowPos({ pos.x + 0.05f*wSize.x, pos.y - std::max(200.0f, 0.25f*wSize.y) });

                            ImGui::OpenPopup("Node");
                            g_state.selectedId = id;
                            g_state.popupShowT0 = T;
                            g_state.nUpdates = kWindowFadeTime/0.016f + 1.0f;
                        }
                    }
                }
            }

            if (g_state.doSelect && g_state.focusId == id && isAnimating == false) {
                ImGui::SetNextWindowPos({ pos.x + 0.05f*wSize.x, pos.y - std::max(200.0f, 0.25f*wSize.y) });

                ImGui::OpenPopup("Node");
                g_state.selectedId = id;
                g_state.popupShowT0 = T;
                g_state.nUpdates = kWindowFadeTime/0.016f + 1.0f;
                g_state.doSelect = false;
            }
        }

        if (g_state.viewCur.z > 0.900f) {
            drawList->PopTextureID();
        }
    }

    // render commands
    for (const auto & [id, node] : g_nodes) {
        if (node.type != 2) continue;

        const auto pos    = g_state.getRenderPosition(node);
        const auto radius = g_state.getRenderRadius(node);

        if (pos.x < -2.0*radius || pos.x > wSize.x + 2.0*radius) continue;
        if (pos.y < -2.0*radius || pos.y > wSize.y + 2.0*radius) continue;

        const auto col = (id == g_state.selectedId) ? kColorCommandSelected : kColorCommand;

        const auto [ pt, p0, p1 ] = g_state.getRenderCommand(node, pos);

        if (g_state.viewCur.z > 0.98) {
            drawList->AddRectFilled(p0, p1, col, 8.0);
            drawList->AddRect(p0, p1, g_state.selectedId == id ? kColorNodeSelected : kColorNode, 8.0);
            if (g_state.viewCur.z > 0.980) {
                ImGui::SetCursorScreenPos(pt);
                ImGui::Text("%s", node.username.c_str());
            }
        } else {
            drawList->AddRectFilled(p0, p1, col);
        }
        g_state.statsNumCommandsRendered++;

        if (g_state.viewCur.z > 0.90 && g_state.isPopupOpen == false) {
            if (g_state.isMouseInMainCanvas()) {
                if (ImGui::IsMouseHoveringRect(p0, p1, true)) {
                    if (ImGui::IsMouseReleased(0) && g_state.isPanning == false && isAnimating == false) {
                        ImGui::SetNextWindowPos({ pos.x + 0.05f*wSize.x, pos.y - std::max(200.0f, 0.25f*wSize.y) });

                        ImGui::OpenPopup("Node");
                        g_state.selectedId = id;
                        g_state.popupShowT0 = T;
                        g_state.nUpdates = kWindowFadeTime/0.016f + 1.0f;
                    }
                }
            }
        }
    }

    if (g_nodes.find(g_state.selectedId) != g_nodes.end()) {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.back());
        ImGui::SetWindowFontScale(1.0f/kFontScale);

        const float t = (T - g_state.popupShowT0)/kWindowFadeTime;
        ImGui::GetStyle().Alpha = std::min(1.0f, t);

        if (ImGui::BeginPopup("Node")) {
            g_state.popupPos = ImGui::GetWindowPos();
            g_state.popupSize = ImGui::GetWindowSize();

            if (g_state.isPopupOpen) {
                const auto & node = g_nodes[g_state.selectedId];

                const auto pos = g_state.getRenderPosition(node);

                const auto col4 = ImGui::ColorConvertU32ToFloat4(kColorNode);
                const auto col = ImGui::ColorConvertFloat4ToU32(ImVec4(col4.x, col4.y, col4.z, t));

                const ImVec2 pp0 = { g_state.popupPos.x                      , g_state.popupPos.y                       };
                const ImVec2 pp1 = { g_state.popupPos.x + g_state.popupSize.x, g_state.popupPos.y                       };
                const ImVec2 pp2 = { g_state.popupPos.x + g_state.popupSize.x, g_state.popupPos.y + g_state.popupSize.y };
                const ImVec2 pp3 = { g_state.popupPos.x                      , g_state.popupPos.y + g_state.popupSize.y };

                //ImVec2 pn0;
                //ImVec2 pn1;
                //ImVec2 pn2;
                //ImVec2 pn3;

                //if (node.type == 2) {
                //    const auto [ pt, p0, p1 ] = g_state.getRenderCommand(node, pos);

                //    pn0 = { pos.x, p0.y };
                //    pn1 = { pos.x, p1.y };
                //} else {
                //    const auto radius = g_state.getRenderRadius(node);
                //    pn0 = { pos.x, pos.y - radius };
                //    pn1 = { pos.x, pos.y + radius };
                //}

                drawList->AddLine(pos, pp0, col);
                drawList->AddLine(pos, pp1, col);
                drawList->AddLine(pos, pp2, col);
                drawList->AddLine(pos, pp3, col);
            }

            g_state.isPopupOpen = true;

            const auto & node = g_nodes[g_state.selectedId];
            ImGui::Text("Node:   %" PRIu64 "", g_state.selectedId);
            if (node.type == 1) {
                ImGui::Text("Author: %s", g_nodes[node.parentId].username.c_str());
            } else if (node.type == 2) {
                ImGui::Text("Author: %s", node.username.c_str());
            }
            //ImGui::Text("Pos:    %.0f %.0f", node.x, node.y);
            ImGui::Text("Type:   %s", node.type == 0 ? "ROOT" : node.type == 1 ? "Node" : "Command");
            ImGui::Text("Depth:  %d", node.level);
            if (g_achievementsMap.find(node.id) != g_achievementsMap.end()) {
                const auto & achievement = g_achievementsMap[node.id];
                const auto col = ImGui::ColorConvertU32ToFloat4(kColorNodeSelected);
                ImGui::Text("%s\n", "");
                auto icon = ICON_FA_TROPHY;
                switch (achievement.type) {
                    case EAchievementType::LevelCompleted: { icon = ICON_FA_TROPHY;     } break;
                    case EAchievementType::Speedrun:       { icon = ICON_FA_HOURGLASS_HALF;  } break;
                };
                const int nFonts = ImGui::GetIO().Fonts->Fonts.size();
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[nFonts - 2]);
                ImGui::TextColored(col, "%s", icon);
                ImGui::PopFont();
                ImGui::SameLine();
                ImGui::TextColored(col, "%s", achievement.desc.c_str());
            }

            ImGui::Separator();

            if (ImGui::Button("Twitter")) {
                g_state.actionOpenUrl = "https://twitter.com/tweet2doom/status/" + std::to_string(g_state.selectedId);
            }
            if (g_achievementsMap.find(node.id) != g_achievementsMap.end()) {
                ImGui::SameLine();
                if (ImGui::Button("Announcement")) {
                    const auto & achievement = g_achievementsMap[node.id];
                    g_state.actionOpenUrl = "https://twitter.com/tweet2doom/status/" + std::to_string(achievement.announcmentId);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Focus")) {
                g_state.focusNode(g_state.selectedId, false);
            }

            ImGui::EndPopup();
        } else {
            if (g_state.isPopupOpen) {
                g_state.isPopupOpen = false;
                g_state.selectedId = 0;
            }
        }

        ImGui::GetStyle().Alpha = 1.0f;

        ImGui::PopFont();
    }

    // render controls
    {
        ImGui::SetWindowFontScale(1.0f/kFontScale);

        const float kGridSize = 1.5f*ImGui::GetTextLineHeightWithSpacing();
        const ImVec2 kGridOffset = { 2.0f*style.ItemInnerSpacing.x, 2.0f*style.ItemInnerSpacing.y, };

        // help
        {
            ImGui::SetCursorScreenPos({ kGridOffset.x + 0.0f*(kGridSize + kGridOffset.x), 1.0f*(kGridSize + kGridOffset.y) - kGridSize, });

            if (ImGui::Button(ICON_FA_QUESTION, ImVec2 { kGridSize, kGridSize })) {
                ImGui::SetNextWindowPos({ kGridOffset.x + 0.0f*(kGridSize + kGridOffset.x), 2.0f*(kGridSize + kGridOffset.y) - kGridSize, });
                ImGui::SetNextWindowFocus();
                g_state.windowShow = true;
                g_state.windowShowT0 = T;
                g_state.windowKind = EWindowKind::Help;
                g_state.nUpdates = kWindowFadeTime/0.016f + 1.0f;
            }
        }

        // statistics
        {
            ImGui::SetCursorScreenPos({ kGridOffset.x + 1.0f*(kGridSize + kGridOffset.x), 1.0f*(kGridSize + kGridOffset.y) - kGridSize, });

            if (ImGui::Button(ICON_FA_CHART_PIE, ImVec2 { kGridSize, kGridSize })) {
                ImGui::SetNextWindowPos({ kGridOffset.x + 0.0f*(kGridSize + kGridOffset.x), 2.0f*(kGridSize + kGridOffset.y) - kGridSize, });
                ImGui::SetNextWindowFocus();
                g_state.windowShow = true;
                g_state.windowShowT0 = T;
                g_state.windowKind = EWindowKind::Statistics;
                g_state.nUpdates = kWindowFadeTime/0.016f + 1.0f;
            }
        }

        // achievements
        {
            ImGui::SetCursorScreenPos({ kGridOffset.x + 2.0f*(kGridSize + kGridOffset.x), 1.0f*(kGridSize + kGridOffset.y) - kGridSize, });

            if (ImGui::Button(ICON_FA_TROPHY, ImVec2 { kGridSize, kGridSize })) {
                ImGui::SetNextWindowPos({ kGridOffset.x + 0.0f*(kGridSize + kGridOffset.x), 2.0f*(kGridSize + kGridOffset.y) - kGridSize, });
                ImGui::SetNextWindowFocus();
                g_state.windowShow = true;
                g_state.windowShowT0 = T;
                g_state.windowKind = EWindowKind::Achievements;
                g_state.nUpdates = kWindowFadeTime/0.016f + 1.0f;
            }
        }

        // fit scene
        {
            ImGui::SetCursorScreenPos({ wSize.x - 1.0f*(kGridSize + kGridOffset.x), wSize.y - 2.0f*(kGridSize + kGridOffset.y), });

            if (ImGui::Button(ICON_FA_EXPAND, ImVec2 { kGridSize, kGridSize })) {
                g_state.anim.t0 = T;
                g_state.anim.t1 = T + 3.0f;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1.z = 0.1f;
                g_state.anim.v1.x = 0.0f;
                g_state.anim.v1.y = g_nodes[g_state.rootId].y + 0.45f*g_state.sizex0*(1.0f + (1.0f - g_state.anim.v1.z)*g_state.sceneScale)*g_state.aspectRatio;
                g_state.anim.type = 1;
            }
        }

        // zoom out
        {
            ImGui::SetCursorScreenPos({ wSize.x - 2.0f*(kGridSize + kGridOffset.x), wSize.y - 1.0f*(kGridSize + kGridOffset.y), });

            if (ImGui::Button(ICON_FA_SEARCH_MINUS, ImVec2 { kGridSize, kGridSize })) {
            }
            if (ImGui::IsMouseDown(0) && ImGui::IsItemHovered()) {
                g_state.anim.t0 = T - 0.016f;
                g_state.anim.t1 = T + kAnimTime;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1.z -= 0.05f*(1.0001f - g_state.viewCur.z);
                g_state.anim.type = 2;
            }
        }

        // zoom in
        {
            ImGui::SetCursorScreenPos({ wSize.x - 1.0f*(kGridSize + kGridOffset.x), wSize.y - 1.0f*(kGridSize + kGridOffset.y), });

            if (ImGui::Button(ICON_FA_SEARCH_PLUS, ImVec2 { kGridSize, kGridSize })) {
            }
            if (ImGui::IsMouseDown(0) && ImGui::IsItemHovered()) {
                g_state.anim.t0 = T - 0.016f;
                g_state.anim.t1 = T + kAnimTime;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1.z += 0.05f*(1.0001f - g_state.viewCur.z);
                g_state.anim.type = 2;
            }
        }

        // left
        {
            ImGui::SetCursorScreenPos({ kGridOffset.x + 0.0f*(kGridSize + kGridOffset.x), wSize.y - 1.0f*(kGridSize + kGridOffset.y), });

            if (ImGui::Button(ICON_FA_ARROW_LEFT, ImVec2 { kGridSize, kGridSize })) {
            }
            if (ImGui::IsMouseDown(0) && ImGui::IsItemHovered()) {
                g_state.anim.t0 = T - 0.016f;
                g_state.anim.t1 = T + kAnimTime;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1.x -= 0.2f*kStepPos*scale;
                g_state.anim.type = 2;
            }
        }

        // right
        {
            ImGui::SetCursorScreenPos({ kGridOffset.x + 2.0f*(kGridSize + kGridOffset.x), wSize.y - 1.0f*(kGridSize + kGridOffset.y), });

            if (ImGui::Button(ICON_FA_ARROW_RIGHT, ImVec2 { kGridSize, kGridSize })) {
            }
            if (ImGui::IsMouseDown(0) && ImGui::IsItemHovered()) {
                g_state.anim.t0 = T - 0.016f;
                g_state.anim.t1 = T + kAnimTime;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1.x += 0.2f*kStepPos*scale;
                g_state.anim.type = 2;
            }
        }

        // up
        {
            ImGui::SetCursorScreenPos({ kGridOffset.x + 1.0f*(kGridSize + kGridOffset.x), wSize.y - 2.0f*(kGridSize + kGridOffset.y), });

            if (ImGui::Button(ICON_FA_ARROW_UP, ImVec2 { kGridSize, kGridSize })) {
            }
            if (ImGui::IsMouseDown(0) && ImGui::IsItemHovered()) {
                g_state.anim.t0 = T - 0.016f;
                g_state.anim.t1 = T + kAnimTime;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1.y -= 0.2f*kStepPos*scale;
                g_state.anim.type = 2;
            }
        }

        // down
        {
            ImGui::SetCursorScreenPos({ kGridOffset.x + 1.0f*(kGridSize + kGridOffset.x), wSize.y - 1.0f*(kGridSize + kGridOffset.y), });

            if (ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2 { kGridSize, kGridSize })) {
            }
            if (ImGui::IsMouseDown(0) && ImGui::IsItemHovered()) {
                g_state.anim.t0 = T - 0.016f;
                g_state.anim.t1 = T + kAnimTime;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1.y += 0.2f*kStepPos*scale;
                g_state.anim.type = 2;
            }
        }

        g_state.heightControls = 2.0f*(kGridSize + kGridOffset.y);
    }

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.back());

    // window : Help
    if (g_state.windowShow && g_state.windowKind == EWindowKind::Help) {
        const float kIconSize = ImGui::GetTextLineHeightWithSpacing();

        ImGui::Begin("What is this?", nullptr,
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::GetStyle().Alpha = std::min(1.0f, (T - g_state.windowShowT0)/kWindowFadeTime);
        ImGui::SetWindowFontScale(1.0f/kFontScale);
        ImGui::PushTextWrapPos(std::min(0.65f*ImGui::GetIO().DisplaySize.x, 400.0f));

        ImGui::Text("This is the State Tree Explorer of the @tweet2doom Twitter bot. "
                    "The tree contains all commands ever tweeted to the bot and the resulting game states (i.e. nodes). "
                    "\n\n");

        {
            ImGui::Image((void *)(intptr_t) g_state.assets.getTexId(::ImVid::Assets::ICON_T2D_SMALL_BLUR), { kIconSize, kIconSize, }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, ImGui::ColorConvertU32ToFloat4(kColorNode));
            ImGui::SameLine();
            ImGui::Text("- Node");
        }

        {
            const auto pos = ImGui::GetCursorScreenPos();

            ImGui::GetWindowDrawList()->AddRectFilled(pos, { pos.x + kIconSize, pos.y + kIconSize }, kColorCommand, 4.0f);
            ImGui::GetWindowDrawList()->AddRect      (pos, { pos.x + kIconSize, pos.y + kIconSize }, kColorNode,    4.0f);
            ImGui::Image((void *)(intptr_t) g_state.assets.getTexId(::ImVid::Assets::ICON_T2D_SMALL_BLUR), { kIconSize, kIconSize, }, {}, {}, {}, {});
            ImGui::SameLine();
            ImGui::Text("- Command");
        }

        ImGui::Text("\n");
        ImGui::Separator();
        ImGui::Text("For more information:\n\n");
        if (ImGui::Button("https://twitter.com/tweet2doom")) {
            g_state.actionOpenUrl = "https://twitter.com/tweet2doom/status/1444355917160534024";
        }

        ImGui::PopTextWrapPos();
        ImGui::End();
    }

    // window : Statistics
    if (g_state.windowShow && g_state.windowKind == EWindowKind::Statistics) {
        static ImVec2 wSize;
        const float t = (T - g_state.windowShowT0)/kWindowFadeTime;
        if (t > 0.1f) {
            wSize.x = std::min(400.0f, wSize.x);
            wSize.y = std::min(0.75f*g_state.rendering.wSize.y, wSize.y);
            ImGui::SetNextWindowSize(wSize);
        }
        ImGui::Begin("Statistics", nullptr,
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse |
                     (t < 0.1f ? ImGuiWindowFlags_AlwaysAutoResize : 0));
        if (t < 0.1f) wSize = ImGui::GetWindowSize();

        ImGui::GetStyle().Alpha = std::min(1.0f, t);
        ImGui::SetWindowFontScale(1.0f/kFontScale);

        ImGui::PushTextWrapPos(std::min(0.65f*ImGui::GetIO().DisplaySize.x, 400.0f));

        ImGui::Text("Total nodes:    %d\n", (int) g_nodes.size());
        ImGui::Text("Unique players: %d\n", g_state.statsNumUniquePlayers);

        ImGui::Separator();
        ImGui::TextDisabled("Rendering info");
        ImGui::Separator();
        ImGui::Text("Zoom:      %.5f", g_state.viewCur.z);
        ImGui::Text("Mouse:     %.0f %.0f", std::max(0.0f, ImGui::GetIO().MousePos.x), std::max(0.0f, ImGui::GetIO().MousePos.y));
        ImGui::Text("Framerate: %.2f (%d, %.3f)", ImGui::GetIO().Framerate, g_state.nUpdates, T);
        ImGui::Text("Display:   %.0f %.0f", ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
        ImGui::Text("Nodes:     %d", g_state.statsNumNodesRendered);
        ImGui::Text("Commands:  %d", g_state.statsNumCommandsRendered);
        ImGui::Text("Edges:     %d", g_state.statsNumEdgesRendered);

        ImGui::Separator();
        ImGui::TextDisabled("Build info");
        ImGui::Separator();
        ImGui::Text("%s", BUILD_TIMESTAMP);

        ImGui::PopTextWrapPos();

        ScrollWhenDraggingOnVoid(ImVec2(-ImGui::GetIO().MouseDelta.x, -ImGui::GetIO().MouseDelta.y), ImGuiMouseButton_Left);
        ImGui::End();
    }

    // window : Achievements
    if (g_state.windowShow && g_state.windowKind == EWindowKind::Achievements) {
        static ImVec2 wSize;
        const float t = (T - g_state.windowShowT0)/kWindowFadeTime;
        if (t > 0.1f) {
            wSize.x = std::min(400.0f, wSize.x);
            wSize.y = std::min(0.75f*g_state.rendering.wSize.y, wSize.y);
            ImGui::SetNextWindowSize(wSize);
        }
        ImGui::Begin("Achievements", nullptr,
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar |
                     (t < 0.1f ? ImGuiWindowFlags_AlwaysAutoResize : 0));
        if (t < 0.1f) wSize = ImGui::GetWindowSize();

        ImGui::GetStyle().Alpha = std::min(1.0f, (T - g_state.windowShowT0)/kWindowFadeTime);
        ImGui::SetWindowFontScale(1.0f/kFontScale);

        ImGui::PushTextWrapPos(std::min(0.65f*ImGui::GetIO().DisplaySize.x, 400.0f));

        ImVec2 p0;
        for (int i = 0; i < (int) g_achievements.size(); ++i) {
            ImGui::PushID(i);
            auto icon = ICON_FA_TROPHY;
            switch (g_achievements[i].type) {
                case EAchievementType::LevelCompleted: { icon = ICON_FA_TROPHY;     } break;
                case EAchievementType::Speedrun:       { icon = ICON_FA_HOURGLASS_HALF;  } break;
            };

            const int nFonts = ImGui::GetIO().Fonts->Fonts.size();
            const auto col = ImGui::ColorConvertU32ToFloat4(kColorNodeSelected);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[nFonts - 2]);
            if (ImGui::Button(ICON_FA_CROSSHAIRS)) {
                g_state.focusNode(g_achievements[i].id, true);
                g_state.doSelect = true;
                g_state.windowShow = false;
                g_state.windowKind = EWindowKind::None;
            }
            ImGui::SameLine();
            ImGui::TextColored(col, "%s", icon);
            ImGui::PopFont();
            ImGui::SameLine();
            if (i == 0) {
                p0 = ImGui::GetCursorScreenPos();
            } else {
                ImGui::SetCursorScreenPos({ p0.x, ImGui::GetCursorScreenPos().y });
            }
            ImGui::TextColored(col, "%s", g_achievements[i].desc.c_str());
            ImGui::PopID();
        }

        ImGui::PopTextWrapPos();

        ScrollWhenDraggingOnVoid(ImVec2(-ImGui::GetIO().MouseDelta.x, -ImGui::GetIO().MouseDelta.y), ImGuiMouseButton_Left);
        ImGui::End();
    }

    ImGui::PopFont();
    ImGui::GetStyle().Alpha = 1.0f;

    if (ImGui::IsWindowFocused()) {
        g_state.windowShow = false;
        g_state.windowKind = EWindowKind::None;
    }

    ImGui::End();

    ImGui::EndFrame();
}

bool interp(float & x, float x0, float x1, float t0, float t1, float t, int type, bool isZoom) {
    if (x == x1 && type != 4) return false;
    if (t < t0) {
        x = x0;
        return true;
    }
    if (t >= t1) {
        x = x1;
        return true;
    }

    float f = (t - t0)/(t1 - t0);
    if (type == 1) {
        x = x0 + (x1 - x0)*std::pow(f, 4);
    } else if (type == 2) {
        x = x0 + (x1 - x0)*std::pow(f, 0.5);
    } else if (type == 3) {
        x = x0 + (x1 - x0)*(f*f*(3.0f - 2.0f*f));
    } else if (type == 4) {
        if (isZoom) {
            if (f < 0.5f) {
                f = 2.0f*f;
                x = x0 + (0.1f - x0)*(f*f*(3.0f - 2.0f*f));
            } else {
                f = 2.0f*(f - 0.5f);
                x = 0.1f + (x1 - 0.1f)*(f*f*(3.0f - 2.0f*f));
            }
        } else {
            x = x0 + (x1 - x0)*(f*f*(3.0f - 2.0f*f));
        }
    } else {
        // linear
        x = x0 + ((x1 - x0)/(t1 - t0))*(t - t0);
    }

    return true;
}

void updatePre() {
    const float T = ImGui::GetTime();

    if (g_state.treeChanged) {
        for (auto & [src, dst] : g_edges) {
            g_nodes[src].parentId = dst;
        }

        for (auto & [id, node] : g_nodes) {
            if (node.type == 0) {
                g_state.rootId = id;
                printf("Root node: %" PRIu64 " %g %g\n", id, node.x, node.y);
            }

            if (node.level < 1) {
                node.y -= 200;
            }

            //if (node.level < 100) {
            //    node.y -= std::pow((100.0 - node.level)/100.0, 4)*10000;
            //}
        }

        for (const auto & [id, node] : g_nodes) {
            if (node.x < g_state.bbxmin) g_state.bbxmin = node.x;
            if (node.x > g_state.bbxmax) g_state.bbxmax = node.x;
            if (node.y < g_state.bbymin) g_state.bbymin = node.y;
            if (node.y > g_state.bbymax) g_state.bbymax = node.y;
        }

        g_state.onWindowResize();

        if (g_state.sceneScale < 1.0) g_state.sceneScale = 1.0;

        if (g_state.isFirstChange) {
            if (g_state.focusId == 0 || g_nodes.find(g_state.focusId) == g_nodes.end()) {
                g_state.viewCur.x = g_nodes[g_state.rootId].x;
                g_state.viewCur.y = g_nodes[g_state.rootId].y + 0.1f*g_state.sizey0;
                g_state.viewCur.z = 0.999f;

                g_state.anim.t0 = T + 1.0f;
                g_state.anim.t1 = T + 4.0f;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1.z = 0.1f;
                g_state.anim.v1.x = 0.0f;
                g_state.anim.v1.y = g_nodes[g_state.rootId].y + 0.45f*g_state.sizex0*(1.0f + (1.0f - g_state.anim.v1.z)*g_state.sceneScale)*g_state.aspectRatio;
                g_state.anim.type = 1;
            } else {
                g_state.anim.v1.x = g_nodes[g_state.focusId].x;
                g_state.anim.v1.y = g_nodes[g_state.focusId].y;
                g_state.anim.v1.z = 0.999f;

                g_state.anim.t0 = T + 0.5f;
                g_state.anim.t1 = T + 3.5f;
                g_state.anim.v0.z = 0.1f;
                g_state.anim.v0.x = 0.0f;
                g_state.anim.v0.y = g_nodes[g_state.rootId].y + 0.45f*g_state.sizex0*(1.0f + (1.0f - g_state.anim.v0.z)*g_state.sceneScale)*g_state.aspectRatio;
                g_state.viewCur = g_state.anim.v0;
                g_state.anim.type = 3;
            }

            for (const auto & achievement : g_achievements) {
                g_achievementsMap[achievement.id] = achievement;
            }

            {
                std::set<std::string> seen;
                for (const auto & node : g_nodes) {
                    if (seen.find(node.second.username) == seen.end()) {
                        seen.emplace(node.second.username);
                        g_state.statsNumUniquePlayers++;
                    }
                }
            }

            g_state.isFirstChange = false;
        }

        printf("Bounding box: [%g %g -> %g %g]\n", g_state.bbxmin, g_state.bbymin, g_state.bbxmax, g_state.bbymax);
        printf("Scene scale:  %g\n", g_state.sceneScale);

        g_state.treeChanged = false;
    }

#ifdef USE_LINE_SHADER
    if (g_state.shaderEdges.isValid() == false) {
        if (g_state.shaderEdges.createLineRender() == false) {
            fprintf(stderr, "Error: Failed to create line shader!\n");
            throw 1;
        }
    }
#endif

    g_state.isMoving = false;
    g_state.isZooming = false;

    {
        const float scale = g_state.getScale(g_state.viewCur.z);

        bool newAnim = false;
        View vt = g_state.viewCur;

        // input handling
        {
            const auto & keyMap = ImGui::GetIO().KeyMap;

            if (ImGui::IsKeyDown(keyMap[ImGuiKey_LeftArrow]))  { vt.x -= kStepPos*scale; newAnim = true; }
            if (ImGui::IsKeyDown(keyMap[ImGuiKey_RightArrow])) { vt.x += kStepPos*scale; newAnim = true; }
            if (ImGui::IsKeyDown(keyMap[ImGuiKey_UpArrow]))    { vt.y -= kStepPos*scale; newAnim = true; }
            if (ImGui::IsKeyDown(keyMap[ImGuiKey_DownArrow]))  { vt.y += kStepPos*scale; newAnim = true; }

            if (g_state.isMouseInMainCanvas()) {
                if (ImGui::IsMouseDown(0) && g_state.isPinching == false && g_state.isPopupOpen == false && g_state.windowShow == false) {
                    // panning
                    if (g_state.isMouseDown == false) {
                        g_state.mouseDownX = ImGui::GetIO().MousePos.x;
                        g_state.mouseDownY = ImGui::GetIO().MousePos.y;
                        g_state.posDownX = g_state.viewCur.x;
                        g_state.posDownY = g_state.viewCur.y;
                        g_state.isMouseDown = true;
                    }

                    if (std::fabs(ImGui::GetIO().MousePos.x - g_state.mouseDownX) > 5.0f ||
                        std::fabs(ImGui::GetIO().MousePos.y - g_state.mouseDownY) > 5.0f) {
                        g_state.isPanning = true;

                        vt.x = g_state.posDownX - (ImGui::GetIO().MousePos.x - g_state.mouseDownX)*scale*(g_state.sizex0/ImGui::GetIO().DisplaySize.x);
                        vt.y = g_state.posDownY - (ImGui::GetIO().MousePos.y - g_state.mouseDownY)*scale*(g_state.sizey0/ImGui::GetIO().DisplaySize.y);

                        newAnim = true;
                    }
                } else {
                    g_state.isMouseDown = false;
                    g_state.isPanning = false;
                }

                if (ImGui::IsMouseDoubleClicked(0) && g_state.windowShow == false) {
                    float dx = (ImGui::GetIO().MousePos.x - 0.5f*ImGui::GetIO().DisplaySize.x)*scale*(g_state.sizex0/ImGui::GetIO().DisplaySize.x);
                    float dy = (ImGui::GetIO().MousePos.y - 0.5f*ImGui::GetIO().DisplaySize.y)*scale*(g_state.sizey0/ImGui::GetIO().DisplaySize.y);

                    vt.x += dx;
                    vt.y += dy;
                    vt.z += 0.5f*(1.0f - g_state.viewCur.z);

                    newAnim = true;
                }
            }

            {
                if (ImGui::IsKeyDown(SDL_SCANCODE_Q)) { vt.z += 0.3f*(1.001f - g_state.viewCur.z); newAnim = true; }
                if (ImGui::IsKeyDown(SDL_SCANCODE_A)) { vt.z -= 0.3f*(1.001f - g_state.viewCur.z); newAnim = true; }

                const float mwheel = std::max(-5.0f, std::min(5.0f, ImGui::GetIO().MouseWheel));

                if (mwheel != 0 && std::fabs(ImGui::GetIO().MousePos.x) < 1e6 && std::fabs(ImGui::GetIO().MousePos.y) < 1e6) {
                    vt.z += 0.3*mwheel*(1.001f - g_state.viewCur.z); newAnim = true;

                    const float s0 = scale;
                    const float s1 = g_state.getScale(vt.z);

                    vt.x += ((ImGui::GetIO().MousePos.x - 0.5f*ImGui::GetIO().DisplaySize.x)/ImGui::GetIO().DisplaySize.x)*(s0 - s1)*g_state.sizex0;
                    vt.y += ((ImGui::GetIO().MousePos.y - 0.5f*ImGui::GetIO().DisplaySize.y)/ImGui::GetIO().DisplaySize.y)*(s0 - s1)*g_state.sizey0;
                }
            }

            if (g_state.isPinching) {
                vt.z = g_state.viewPinch.z - 1.0f*(1.0f - g_state.pinchScale)*(1.001f - g_state.viewCur.z);

                const float s0 = scale;
                const float s1 = g_state.getScale(vt.z);

                vt.x += ((g_state.pinchPosX1 - 0.5f*ImGui::GetIO().DisplaySize.x)/ImGui::GetIO().DisplaySize.x)*(s0 - s1)*g_state.sizex0;
                vt.y += ((g_state.pinchPosY1 - 0.5f*ImGui::GetIO().DisplaySize.y)/ImGui::GetIO().DisplaySize.y)*(s0 - s1)*g_state.sizey0;

                newAnim = true;
            }

            vt.z = std::max(kZoomMin, std::min(kZoomMax, vt.z));

            if (newAnim) {
                g_state.anim.t0 = T - 0.016f;
                g_state.anim.t1 = T + kAnimTime;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1 = vt;
                g_state.anim.type = 2;

                g_state.nUpdates = std::max(1, g_state.nUpdates);
            }

            //printf("%g %g\n", g_state.viewCur.z, g_state.zoomTgt);
        }

        g_state.anim.v1.z = std::max(kZoomMin, std::min(kZoomMax, g_state.anim.v1.z));

        g_state.isMoving  |= interp(g_state.viewCur.x, g_state.anim.v0.x, g_state.anim.v1.x, g_state.anim.t0, g_state.anim.t1, T, g_state.anim.type, false);
        g_state.isMoving  |= interp(g_state.viewCur.y, g_state.anim.v0.y, g_state.anim.v1.y, g_state.anim.t0, g_state.anim.t1, T, g_state.anim.type, false);
        g_state.isZooming |= interp(g_state.viewCur.z, g_state.anim.v0.z, g_state.anim.v1.z, g_state.anim.t0, g_state.anim.t1, T, g_state.anim.type, true);
    }

    const float scale = g_state.getScale(g_state.viewCur.z);
    [[maybe_unused]] const float iscale = (1.0/scale)/ImGui::GetIO().DisplayFramebufferScale.x;

#ifdef USE_LINE_SHADER
    const float xmin = g_state.viewCur.x - 0.5*g_state.sizex0*scale;
    const float ymin = g_state.viewCur.y - 0.5*g_state.sizey0*scale;
    const float xmax = g_state.viewCur.x + 0.5*g_state.sizex0*scale;
    const float ymax = g_state.viewCur.y + 0.5*g_state.sizey0*scale;

    const float idx = 1.0f/(xmax - xmin);
    const float idy = 1.0f/(ymax - ymin);

    if (
            g_state.forceRender ||
            (
                    (g_state.viewCur.z > 0.9f) &&
                    (
                            ((g_state.isZooming || g_state.isMoving) && g_state.nSkipUpdate-- <= 0) ||
                            (g_state.isZooming == false && g_state.isMoving == false && (g_state.wasMoving || g_state.wasZooming))
                    )
            )
       ) {

        //printf("%d %d %d %d\n", g_state.isMoving, g_state.isZooming, g_state.wasMoving, g_state.wasZooming);

        auto & fbo = g_state.fboEdges;
        auto & shader = g_state.shaderEdges;

        if (fbo.isAllocated() == false) {
            fbo.create(4*1024, 4*1024);
        }

        if (fbo.isAllocated()) {
            fbo.bind();
            fbo.clear();
            fbo.unbind();

            const std::array<float, 4> col = { float(0x1D)/256.0f, float(0xA1)/256.0f, float(0xF2)/256.0f, float(0.5f*(g_state.viewCur.z - 0.9f)/0.1f) };
            const auto thickness = std::max(0.25, 2.0*iscale);

            std::vector<std::array<float, 2>> points;

            for (const auto & edge : g_edges) {
                const ImVec2 p0 = {
                    1.0f*(g_nodes[edge.src].x - xmin)*idx - 0.5f,
                    1.0f*(g_nodes[edge.src].y - ymin)*idy - 0.5f,
                };

                const ImVec2 p1 = {
                    1.0f*(g_nodes[edge.dst].x - xmin)*idx - 0.5f,
                    1.0f*(g_nodes[edge.dst].y - ymin)*idy - 0.5f,
                };

                points.push_back({ p0.x, p0.y });
                points.push_back({ p1.x, p1.y });
            }

            shader.renderLinesAsQuads(fbo, col, points, thickness);

            g_state.posEdgesX = g_state.viewCur.x;
            g_state.posEdgesY = g_state.viewCur.y;
            g_state.scaleEdges = scale;
            g_state.nSkipUpdate = 10;
        }

        g_state.forceRender = false;
    }
#endif

    if (g_state.isMoving || g_state.isZooming) {
        g_state.nUpdates = std::max(1, g_state.nUpdates);
    }

    if (T >= g_state.anim.t1) {
        g_state.anim.v1 = g_state.viewCur;
    }
}

void updatePost() {
    g_state.wasMoving = g_state.isMoving;
    g_state.wasZooming = g_state.isZooming;
}

void deinitMain() {
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    printf("Build time: %s\n", BUILD_TIMESTAMP);
#ifdef __EMSCRIPTEN__
    printf("Press the Init button to start\n");
#endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error: %s\n", SDL_GetError());
        return -1;
    }

    ImGui_PreInit();

    int windowX = 1200;
    int windowY = 800;

#ifdef __EMSCRIPTEN__
    SDL_Renderer * renderer;
    SDL_Window * window;
    SDL_CreateWindowAndRenderer(windowX, windowY, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_RENDERER_PRESENTVSYNC, &window, &renderer);
#else
    const char * windowTitle = "Tweet2Doom - State Tree Explorer";
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window * window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowX, windowY, window_flags);
#endif

    void * gl_context = SDL_GL_CreateContext(window);

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    ImGui_Init(window, gl_context);
    ImGui::GetIO().IniFilename = nullptr;

    // loading assets and fonts

#ifdef __EMSCRIPTEN__
        g_state.assets.load(".");
#else
        g_state.assets.load("../images");
#endif

    {
        bool isNotLoaded = true;
        const bool merge = false;
        const float fontSize = 14.0f*kFontScale;
        const char * fontName = "DroidSans.ttf";
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + fontName, fontSize, merge);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../fonts/" + fontName, fontSize, merge);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../bin/" + fontName, fontSize, merge);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../examples/assets/fonts/" + fontName, fontSize, merge);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../examples/assets/fonts/" + fontName, fontSize, merge);
    }

    {
        bool isNotLoaded = true;
        const bool merge = true;
        const float fontSize = 14.0f*kFontScale;
        const char * fontName = "fontawesome-webfont.ttf";
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + fontName, fontSize, merge);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../fonts/" + fontName, fontSize, merge);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../bin/" + fontName, fontSize, merge);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../examples/assets/fonts/" + fontName, fontSize, merge);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../examples/assets/fonts/" + fontName, fontSize, merge);
    }

    ImFontConfig cfg;
    cfg.SizePixels = 13.0f*kFontScale;
    //cfg.OversampleH = 3;
    ImGui::GetIO().Fonts->AddFontDefault(&cfg);

    ImGui_BeginFrame(window);
    ImGui::NewFrame();
    ImGui::EndFrame();
    ImGui_EndFrame(window);

    bool isInitialized = false;

    g_doInit = [&]() {
        initMain();

        isInitialized = true;

        return true;
    };

    g_setWindowSize = [&](int sizeX, int sizeY) {
        static int lastX = -1;
        static int lastY = -1;

        if (lastX == sizeX && lastY == sizeY) {
            return;
        }

        lastX = sizeX;
        lastY = sizeY;

        SDL_SetWindowSize(window, sizeX, sizeY);

        g_state.aspectRatio = sizeX > 0 ? float(sizeY)/sizeX : 1.0f;
        //g_state.sizex0 = kSizeX0*ImGui::GetIO().DisplayFramebufferScale.x;
        g_state.sizex0 = sizeX*ImGui::GetIO().DisplayFramebufferScale.x;
        g_state.sizey0 = g_state.sizex0*g_state.aspectRatio;
        g_state.onWindowResize();
        g_state.forceRender = true;
        g_state.nUpdates = 1;
    };

    g_setPinch = [&](float x, float y, float scale, int type) {
        if (type == 0) {
            g_state.isPinching = true;
            g_state.viewPinch = g_state.viewCur;
        }
        if (type == 2) g_state.isPinching = false;

        g_state.pinchPosX1 = x;
        g_state.pinchPosY1 = y;
        g_state.pinchScale = scale;
    };

    g_mainUpdate = [&]() {
        // framerate throtling when idle
        --g_state.nUpdates;
        if (g_state.nUpdates < -30) g_state.nUpdates = 0;

        if (isInitialized == false) {
            return true;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            g_state.nUpdates = std::max(5, g_state.nUpdates);
            ImGui_ProcessEvent(&event);
            if (event.type == SDL_QUIT) return false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) return false;
        }

        updatePre();

        if (g_state.nUpdates >= 0) {
            if (ImGui_BeginFrame(window) == false) {
                return false;
            }

            renderMain();
            ImGui_EndFrame(window);
        }

        updatePost();

        return true;
    };

    g_addNode = [&](const NodeId & id, const std::string & username, int level, int type, int x, int y) {
        g_nodes[id] = {
            id, id, username, level, type, float(x), float(y),
        };
    };

    g_updateNodePosition = [&](const NodeId & id, int x, int y) {
        if (g_nodes.find(id) == g_nodes.end()) {
            fprintf(stderr, "Error: update position - unknown id %" PRIu64 "\n", id);
            return;
        }

        g_nodes[id].x = x;
        g_nodes[id].y = y;
    };

    g_addEdge = [&](const NodeId & src, const NodeId & dst) {
        if (g_nodes.find(src) == g_nodes.end() ||
            g_nodes.find(dst) == g_nodes.end()) {
            // most likely a hidden node
            //fprintf(stderr, "Error: adding edge - unknown id '%s' or '%s'\n", src, dst);
            return;
        }

        g_edges.push_back({ src, dst, });
    };

    g_focusNode = [&](const NodeId & id) {
        if (id == 0) return;
        g_state.focusId = id;
        g_state.selectedId = g_state.focusId;
        g_state.doSelect = true;
    };

    g_getActionOpenUrl = [&]() {
        auto res = g_state.actionOpenUrl;
        g_state.actionOpenUrl.clear();
        return res;
    };

    g_treeChanged = [&]() {
        g_state.treeChanged = true;
    };

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(mainUpdate, NULL, 0, true);
#else
    loadData();
    g_setWindowSize(windowX, windowY);

    if (g_doInit() == false) {
        printf("Error: failed to initialize\n");
        return -2;
    }

    while (true) {
        if (g_mainUpdate() == false) break;

        {
            int sizeX = -1;
            int sizeY = -1;

            SDL_GetWindowSize(window, &sizeX, &sizeY);
            g_setWindowSize(sizeX, sizeY);
        }
    }

    deinitMain();

    // Cleanup
    ImGui_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_CloseAudio();
    SDL_Quit();
#endif

    return 0;
}

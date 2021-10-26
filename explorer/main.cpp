#include "build_timestamp.h"

#include "common.h"

#include "core/assets.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "imgui-extra/imgui_impl.h"

#ifndef ICON_FA_COGS
#include "icons_font_awesome.h"
#endif

#include <SDL.h>
#include <SDL_opengl.h>

#include <cmath>
#include <fstream>
#include <vector>
#include <functional>

#ifdef USE_LINE_SHADER
#include "core/frame-buffer.h"
#include "core/shader-program.h"
#endif

const float kFontScale = 2.0f;

namespace {
template <typename T>
int sgn(T x) { return x < 0 ? -1 : x > 0 ? 1 : 0; }
}

// ImGui helpers

bool ImGui_tryLoadFont(const std::string & filename, float size = 14.0f, bool merge = false) {
    printf("Trying to load font from '%s' ..\n", filename.c_str());
    std::ifstream f(filename);
    if (f.good() == false) {
        printf(" - failed\n");
        return false;
    }
    printf(" - success\n");
    if (merge) {
        // todo : ugly static !!!
        static ImWchar ranges[] = { 0xf000, 0xf3ff, 0 };
        static ImFontConfig config;

        config.MergeMode = true;
        config.GlyphOffset = { 0.0f, 0.0f };

        ImGui::GetIO().Fonts->AddFontFromFileTTF(filename.c_str(), size, &config, ranges);
    } else {
        ImGui::GetIO().Fonts->AddFontFromFileTTF(filename.c_str(), size);
    }
    return true;
}

bool ImGui_BeginFrame(SDL_Window * window) {
    ImGui_NewFrame(window);

    return true;
}

bool ImGui_EndFrame(SDL_Window * window) {
    // Rendering
    int display_w, display_h;
    SDL_GetWindowSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Render();
    ImGui_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);

    return true;
}

bool ImGui_SetStyle() {
    ImGuiStyle & style = ImGui::GetStyle();

    style.AntiAliasedFill = true;
    style.AntiAliasedLines = true;
    style.WindowRounding = 0.0f;

    style.WindowPadding = ImVec2(8, 8);
    style.WindowRounding = 0.0f;
    style.FramePadding = ImVec2(4, 3);
    style.FrameRounding = 0.0f;
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.IndentSpacing = 21.0f;
    style.ScrollbarSize = 16.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 3.0f;

    style.Colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.24f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.15f, 0.20f, 0.60f);
    //style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 0.31f, 0.71f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.00f, 0.39f, 0.39f, 0.39f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.00f, 0.50f, 0.50f, 0.70f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.27f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    //style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.00f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.80f, 0.80f, 0.83f, 0.39f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.80f, 0.80f, 0.83f, 0.39f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.13f, 0.55f, 0.55f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.61f, 1.00f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.79f, 0.51f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    //style.Colors[ImGuiCol_Column]                = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    //style.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    //style.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    //style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.40f, 0.39f, 0.38f, 0.16f);
    //style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    //style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(1.00f, 0.65f, 0.38f, 0.67f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(1.00f, 0.65f, 0.38f, 0.67f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
    //style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(1.00f, 0.98f, 0.95f, 0.78f);

    style.ScaleAllSizes(1.0f);

    return true;
}

static std::function<bool()> g_doInit;
static std::function<void(int, int)> g_setWindowSize;
static std::function<void(float, float, float, int)> g_setPinch;
static std::function<bool()> g_mainUpdate;
static std::function<void(const std::string & , const std::string & , int, int, int, int)> g_addNode;
static std::function<void(const std::string & , int, int)> g_updateNodePosition;
static std::function<void(const std::string & , const std::string & )> g_addEdge;
static std::function<void(const std::string & )> g_focusNode;
static std::function<std::string()> g_getActionOpenId;
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
                        g_addNode(id, username, level, type, x, y);
                    }));

    emscripten::function("add_edge", emscripten::optional_override(
                    [](const std::string & src, const std::string & dst) {
                        g_addEdge(src, dst);
                    }));

    emscripten::function("update_node_position", emscripten::optional_override(
                    [](const std::string & id, int x, int y) {
                        g_updateNodePosition(id, x, y);
                    }));

    emscripten::function("focus_node", emscripten::optional_override(
                    [](const std::string & id) {
                        g_focusNode(id);
                    }));

    emscripten::function("get_action_open_id", emscripten::optional_override(
                    []() {
                        return g_getActionOpenId();
                    }));
}
#endif

// Core

const float kSizeX0 = 1000.0f;
const float kStepPos = 50.0f;
const float kAnimTime = 0.25f;

struct Node {
    std::string id;
    std::string username;
    int level;
    int type;
    float x;
    float y;
};

struct Edge {
    std::string src;
    std::string dst;
};

const auto kZoomMin = 0.1f;
const auto kZoomMax = 1.0f;
const auto kZoomMinLog = std::log(kZoomMin);
const auto kZoomMaxLog = std::log(kZoomMax);

float zoomFromLog(float x) {
    return std::exp(kZoomMinLog + x*(kZoomMaxLog - kZoomMinLog));
}

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

struct State {
    // bounding box of the nodes
    float xmin = 1e10;
    float xmax = -1e10;

    float ymin = 1e10;
    float ymax = -1e10;

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

    std::string rootId;
    std::string focusId;
    std::string selectedId;

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
    bool forceRender = false;

    float mouseDownX = 0.0f;
    float mouseDownY = 0.0f;
    float posDownX = 0.0f;
    float posDownY = 0.0f;

    float heightControls = 0.0f;

    int nSkipUpdate = 0;

    // rendering options
    float renderingEdgesMinZ = 0.0f;

    // open action
    std::string actionOpenId = "";

    ::ImVid::Assets assets;

#ifdef USE_LINE_SHADER
    ::ImVid::FrameBuffer fboEdges;
    ::ImVid::ShaderProgram shaderEdges;
#endif

    float scale(float z) const { return 0.5 + (1.0 - zoomFromLog(z))*sceneScale; }
    void onWindowResize() {
        sceneScale = std::max(
                1.1*(((xmax - xmin) / sizex0 - 1.0) / 0.9),
                1.1*(((ymax - ymin) / sizey0 - 1.0) / 0.9));
    }
};

std::map<std::string, Node> g_nodes;
std::vector<Edge> g_edges;

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

            g_addNode(cur.id.c_str(), cur.username.c_str(), cur.level, cur.type, 0, 0);
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

            g_updateNodePosition(cur.id.c_str(), cur.x, cur.y);
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

            g_addEdge(cur.src.c_str(), cur.dst.c_str());
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

    const float T = ImGui::GetTime();

    static bool isFirstFrame = true;
    if (isFirstFrame) {
        ImGui_SetStyle();
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

    const auto wSize = ImGui::GetContentRegionAvail();

    auto drawList = ImGui::GetWindowDrawList();

    // background
    {
        const auto col = ImGui::ColorConvertFloat4ToU32({ float(0x0A)/256.0f, float(0x10)/256.0f, float(0x16)/256.0f, 0.5f });
        drawList->AddRectFilled({ 0.0f, 0.0f }, wSize, col);
    }

    const bool isAnimating = T < g_state.anim.t1;

    const float scale = g_state.scale(g_state.viewCur.z);
    const float iscale = (1.0/scale)/ImGui::GetIO().DisplayFramebufferScale.x;

    const float xmin = g_state.viewCur.x - 0.5*g_state.sizex0*scale;
    const float ymin = g_state.viewCur.y - 0.5*g_state.sizey0*scale;
    const float xmax = g_state.viewCur.x + 0.5*g_state.sizex0*scale;
    const float ymax = g_state.viewCur.y + 0.5*g_state.sizey0*scale;

    const float dx = (xmax - xmin);
    const float dy = (ymax - ymin);
    const float idx = 1.0f/dx;
    const float idy = 1.0f/dy;

    if (g_state.viewCur.z >= g_state.renderingEdgesMinZ) {
#ifdef USE_LINE_SHADER
        // shader-based line rendering
        const float f = scale/g_state.scaleEdges;
        const float ds = 0.5*(1.0 - f);
        const float dx = f*(g_state.viewCur.x - g_state.posEdgesX)*idx;
        const float dy = f*(g_state.viewCur.y - g_state.posEdgesY)*idy;

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
        const auto col = ImGui::ColorConvertFloat4ToU32({ float(0x1D)/256.0f, float(0xA1)/256.0f, float(0xF2)/256.0f, 0.40f });
        const auto thickness = std::max(0.1, 2.0*iscale);

        for (const auto & edge : g_edges) {
            const ImVec2 p0 = {
                (g_nodes[edge.src].x - xmin)*idx*wSize.x,
                (g_nodes[edge.src].y - ymin)*idy*wSize.y,
            };

            const ImVec2 p1 = {
                (g_nodes[edge.dst].x - xmin)*idx*wSize.x,
                (g_nodes[edge.dst].y - ymin)*idy*wSize.y,
            };

            const float cull = 1000.0f;

            if (
                    (p0.x < -cull || p0.x > wSize.x + cull || p0.y < -cull || p0.y > wSize.y + cull) &&
                    (p1.x < -cull || p1.x > wSize.x + cull || p1.y < -cull || p1.y > wSize.y + cull)
               ) continue;

            drawList->AddLine(p0, p1, col, thickness);
        }
#endif
    }

    ImGui::SetWindowFontScale(1.0f*iscale/kFontScale);
    drawList->PushTextureID((void *)(intptr_t) g_state.assets.getTexId(::ImVid::Assets::ICON_T2D_SMALL_BLUR));
    // render nodes
    for (const auto & [id, node] : g_nodes) {
        if (node.type == 2) continue;

        const ImVec2 pos = {
            (node.x - xmin)*idx*wSize.x,
            (node.y - ymin)*idy*wSize.y,
        };

        const float radius = std::max(0.5f, (node.type == 0 ? 92.0f : 32.0f)*iscale);

        if (pos.x < -2.0*radius || pos.x > wSize.x + 2.0*radius) continue;
        if (pos.y < -2.0*radius || pos.y > wSize.y + 2.0*radius) continue;

        const auto col = node.type == 2 ?
            ImGui::ColorConvertFloat4ToU32({ float(0x1D)/256.0f, float(0xA1)/256.0f, float(0xA2)/256.0f, 1.0f }) :
            ImGui::ColorConvertFloat4ToU32({ float(0x00)/256.0f, float(0xFF)/256.0f, float(0x7D)/256.0f, 1.0f });

        const ImVec2 h0 = { pos.x - 2.0f*radius, pos.y - 2.0f*radius };
        const ImVec2 h1 = { pos.x + 2.0f*radius, pos.y + 2.0f*radius };

        if (node.type == 0) {
            const float w = (1.8f*radius);
            const float h = (3.2f*radius);

            ImGui::SetCursorScreenPos({ pos.x - w, pos.y - h, });
            ImGui::Image((void *)(intptr_t) g_state.assets.getTexId(::ImVid::Assets::ICON_T2D_BIG), { 2.0f*w, 2.0f*h });
        } else {
            if (g_state.viewCur.z > 0.900) {
                const float w = (1.0f*radius);
                const float h = (1.0f*radius);

                ImGui::SetCursorScreenPos({ pos.x - w, pos.y - h, });
                ImGui::Image((void *)(intptr_t) g_state.assets.getTexId(::ImVid::Assets::ICON_T2D_SMALL_BLUR), { 2.0f*w, 2.0f*h });
            } else if (g_state.viewCur.z > 0.500) {
                drawList->AddCircleFilled(pos, radius, col);
            } else {
                drawList->AddRectFilled({ float(pos.x - radius), float(pos.y - radius) }, { float(pos.x + radius), float(pos.y + radius) }, col);
            }
        }

        // TODO: deduplicate
        if (g_state.viewCur.z > 0.90 && g_state.selectedId.empty()) {
            if (ImGui::GetIO().MousePos.y < ImGui::GetIO().DisplaySize.y - 1.25f*g_state.heightControls) {
                if (ImGui::IsMouseHoveringRect(h0, h1, true)) {
                    if (ImGui::IsMouseReleased(0) && g_state.isPanning == false && isAnimating == false) {
                        auto pm = ImGui::GetMousePos();
                        pm.x = h1.x;
                        pm.y = h0.y;
                        ImGui::SetNextWindowPos(pm);

                        ImGui::OpenPopup("Node");
                        g_state.selectedId = id;
                    }
                }
            }
        }
    }
    drawList->PopTextureID();

    // render commands
    for (const auto & [id, node] : g_nodes) {
        if (node.type != 2) continue;

        const ImVec2 pos = {
            (node.x - xmin)*idx*wSize.x,
            (node.y - ymin)*idy*wSize.y,
        };

        const float radius = std::max(0.5f, (node.type == 0 ? 92.0f : 32.0f)*iscale);

        if (pos.x < -2.0*radius || pos.x > wSize.x + 2.0*radius) continue;
        if (pos.y < -2.0*radius || pos.y > wSize.y + 2.0*radius) continue;

        const auto col = ImGui::ColorConvertFloat4ToU32({ float(0x1D)/256.0f, float(0xA1)/256.0f, float(0xA2)/256.0f, 1.0f });

        const ImVec2 tSize = ImGui::CalcTextSize(node.username.c_str());
        const ImVec2 tMargin = { 12.0f*iscale, 8.0f*iscale, };
        const ImVec2 pt = {pos.x - 0.5f*tSize.x, pos.y - 0.5f*tSize.y, };
        const ImVec2 p0 = {pos.x - 0.5f*tSize.x - tMargin.x, pos.y - 0.5f*tSize.y - tMargin.y, };
        const ImVec2 p1 = {pos.x + 0.5f*tSize.x + tMargin.x, pos.y + 0.5f*tSize.y + tMargin.y, };

        if (g_state.viewCur.z > 0.98) {
            drawList->AddRectFilled(p0, p1, col, 8.0);
            if (g_state.viewCur.z > 0.980) {
                ImGui::SetCursorScreenPos(pt);
                ImGui::Text("%s", node.username.c_str());
            }
        } else {
            drawList->AddRectFilled(p0, p1, col);
        }

        const ImVec2 h0 = p0;
        const ImVec2 h1 = p1;

        // TODO: deduplicate
        if (g_state.viewCur.z > 0.90 && g_state.selectedId.empty()) {
            if (ImGui::GetIO().MousePos.y < ImGui::GetIO().DisplaySize.y - 1.25f*g_state.heightControls) {
                if (ImGui::IsMouseHoveringRect(h0, h1, true)) {
                    if (ImGui::IsMouseReleased(0) && g_state.isPanning == false && isAnimating == false) {
                        auto pm = ImGui::GetMousePos();
                        pm.x = h1.x;
                        pm.y = h0.y;
                        ImGui::SetNextWindowPos(pm);

                        ImGui::OpenPopup("Node");
                        g_state.selectedId = id;
                    }
                }
            }
        }
    }

    {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.back());
        ImGui::SetWindowFontScale(1.0f/kFontScale);
        if (ImGui::BeginPopup("Node")) {

            const auto & node = g_nodes[g_state.selectedId];
            ImGui::Text("Node:  %s", g_state.selectedId.c_str());
            ImGui::Text("User:  %s", node.username.c_str());
            ImGui::Text("Pos:   %.0f %.0f", node.x, node.y);
            ImGui::Text("Type:  %s", node.type == 0 ? "ROOT" : node.type == 1 ? "Node" : "Command");
            ImGui::Text("Depth: %d", node.level);

            ImGui::Separator();

            if (ImGui::Button("Twitter")) {
                g_state.actionOpenId = g_state.selectedId;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        } else {
            g_state.selectedId.clear();
        }
        ImGui::PopFont();
    }

    {
        ImGui::SetWindowFontScale(1.0f/kFontScale);

        const float kButtonSize = 1.5f*ImGui::GetTextLineHeightWithSpacing();
        const ImVec2 offset = { 2.0f*style.ItemInnerSpacing.x, 2.0f*style.ItemInnerSpacing.y, };

        // fit scene
        {
            ImGui::SetCursorScreenPos({ wSize.x - 1.0f*(kButtonSize + offset.x), wSize.y - 2.0f*(kButtonSize + offset.y), });

            if (ImGui::Button(ICON_FA_EXPAND, ImVec2 { kButtonSize, kButtonSize })) {
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
            ImGui::SetCursorScreenPos({ wSize.x - 2.0f*(kButtonSize + offset.x), wSize.y - 1.0f*(kButtonSize + offset.y), });

            if (ImGui::Button(ICON_FA_SEARCH_MINUS, ImVec2 { kButtonSize, kButtonSize })) {
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
            ImGui::SetCursorScreenPos({ wSize.x - 1.0f*(kButtonSize + offset.x), wSize.y - 1.0f*(kButtonSize + offset.y), });

            if (ImGui::Button(ICON_FA_SEARCH_PLUS, ImVec2 { kButtonSize, kButtonSize })) {
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
            ImGui::SetCursorScreenPos({ offset.x + 0.0f*(kButtonSize + offset.x), wSize.y - 1.0f*(kButtonSize + offset.y), });

            if (ImGui::Button(ICON_FA_ARROW_LEFT, ImVec2 { kButtonSize, kButtonSize })) {
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
            ImGui::SetCursorScreenPos({ offset.x + 2.0f*(kButtonSize + offset.x), wSize.y - 1.0f*(kButtonSize + offset.y), });

            if (ImGui::Button(ICON_FA_ARROW_RIGHT, ImVec2 { kButtonSize, kButtonSize })) {
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
            ImGui::SetCursorScreenPos({ offset.x + 1.0f*(kButtonSize + offset.x), wSize.y - 2.0f*(kButtonSize + offset.y), });

            if (ImGui::Button(ICON_FA_ARROW_UP, ImVec2 { kButtonSize, kButtonSize })) {
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
            ImGui::SetCursorScreenPos({ offset.x + 1.0f*(kButtonSize + offset.x), wSize.y - 1.0f*(kButtonSize + offset.y), });

            if (ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2 { kButtonSize, kButtonSize })) {
            }
            if (ImGui::IsMouseDown(0) && ImGui::IsItemHovered()) {
                g_state.anim.t0 = T - 0.016f;
                g_state.anim.t1 = T + kAnimTime;
                g_state.anim.v0 = g_state.viewCur;
                g_state.anim.v1.y += 0.2f*kStepPos*scale;
                g_state.anim.type = 2;
            }
        }

        g_state.heightControls = 2.0f*(kButtonSize + offset.y);
    }

    ImGui::SetWindowFontScale(1.0f/kFontScale);

    ImGui::SetCursorScreenPos({ 0.0f, 0.0f, });
    ImGui::Text("%g", g_state.viewCur.z);
    //ImGui::Text("%g", ImGui::GetIO().Framerate);
    //ImGui::Text("%g %g", ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
    //ImGui::Text("%g %g", ImGui::GetIO().DisplayFramebufferScale.x, ImGui::GetIO().DisplayFramebufferScale.y);
    //ImGui::Text("%g, %g", g_state.pinchScale, g_state.viewCur.z);
    //ImGui::Text("v0:  %g, %g, %g, %g", g_state.anim.v0.x, g_state.anim.v0.y, g_state.anim.v0.z, g_state.anim.t0);
    //ImGui::Text("v1:  %g, %g, %g, %g", g_state.anim.v1.x, g_state.anim.v1.y, g_state.anim.v1.z, g_state.anim.t1);
    //ImGui::Text("cur: %g, %g, %g, %g", g_state.viewCur.x, g_state.viewCur.y, g_state.viewCur.z, ImGui::GetTime());

    //if (ImGui::IsMouseHoveringRect({ 0.0f, 0.0f }, wSize)) {
    //    m_state.mousePos.from(ImGui::GetIO().MousePos);
    //}

    ImGui::End();

    ImGui::EndFrame();
}

bool interp(float & x, float x0, float x1, float t0, float t1, float t, int type) {
    if (x == x1) return false;
    if (t < t0) {
        x = x0;
        return true;
    }
    if (t >= t1) {
        x = x1;
        return true;
    }
    if (x0 == x1) return false;

    const float f = (t - t0)/(t1 - t0);
    if (type == 1) {
        x = x0 + (x1 - x0)*std::pow(f, 4);
    } else if (type == 2) {
        x = x0 + (x1 - x0)*std::pow(f, 0.5);
    } else if (type == 3) {
        x = x0 + (x1 - x0)*(f*f*(3.0f - 2.0f*f));
    } else {
        // linear
        x = x0 + ((x1 - x0)/(t1 - t0))*(t - t0);
    }

    return true;
}

void updatePre() {
    const float T = ImGui::GetTime();

    if (g_state.treeChanged) {
        for (auto & [id, node] : g_nodes) {
            if (node.type == 0) {
                g_state.rootId = id;
                printf("Root node: %s %g %g\n", id.c_str(), node.x, node.y);
            }

            if (node.level < 1) {
                node.y -= 200;
            }

            //if (node.level < 100) {
            //    node.y -= std::pow((100.0 - node.level)/100.0, 4)*10000;
            //}
        }

        for (const auto & [id, node] : g_nodes) {
            if (node.x < g_state.xmin) g_state.xmin = node.x;
            if (node.x > g_state.xmax) g_state.xmax = node.x;
            if (node.y < g_state.ymin) g_state.ymin = node.y;
            if (node.y > g_state.ymax) g_state.ymax = node.y;
        }

        g_state.onWindowResize();

        if (g_state.sceneScale < 1.0) g_state.sceneScale = 1.0;

        if (g_state.isFirstChange) {
            if (g_state.focusId.empty() || g_nodes.find(g_state.focusId) == g_nodes.end()) {
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
                g_state.anim.v1.z = 1.0f;

                g_state.anim.t0 = T + 0.5f;
                g_state.anim.t1 = T + 3.5f;
                g_state.anim.v0.z = 0.1f;
                g_state.anim.v0.x = 0.0f;
                g_state.anim.v0.y = g_nodes[g_state.rootId].y + 0.45f*g_state.sizex0*(1.0f + (1.0f - g_state.anim.v0.z)*g_state.sceneScale)*g_state.aspectRatio;
                g_state.viewCur = g_state.anim.v0;
                g_state.anim.type = 3;
            }

            g_state.isFirstChange = false;
        }

        printf("Bounding box: [%g %g -> %g %g]\n", g_state.xmin, g_state.ymin, g_state.xmax, g_state.ymax);
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
        const float scale = g_state.scale(g_state.viewCur.z);

        bool newAnim = false;
        View vt = g_state.viewCur;

        // input handling
        {
            const auto & keyMap = ImGui::GetIO().KeyMap;

            if (ImGui::IsKeyDown(keyMap[ImGuiKey_LeftArrow]))  { vt.x -= kStepPos*scale; newAnim = true; }
            if (ImGui::IsKeyDown(keyMap[ImGuiKey_RightArrow])) { vt.x += kStepPos*scale; newAnim = true; }
            if (ImGui::IsKeyDown(keyMap[ImGuiKey_UpArrow]))    { vt.y -= kStepPos*scale; newAnim = true; }
            if (ImGui::IsKeyDown(keyMap[ImGuiKey_DownArrow]))  { vt.y += kStepPos*scale; newAnim = true; }

            if (ImGui::GetIO().MousePos.y < ImGui::GetIO().DisplaySize.y - 1.25f*g_state.heightControls) {
                if (ImGui::IsMouseDown(0) && g_state.isPinching == false) {
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

                if (ImGui::IsMouseDoubleClicked(0)) {
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

                if (mwheel != 0) {
                    vt.z += 0.3*mwheel*(1.001f - g_state.viewCur.z); newAnim = true;

                    const float s0 = scale;
                    const float s1 = g_state.scale(vt.z);

                    vt.x += ((ImGui::GetIO().MousePos.x - 0.5f*ImGui::GetIO().DisplaySize.x)/ImGui::GetIO().DisplaySize.x)*(s0 - s1)*g_state.sizex0;
                    vt.y += ((ImGui::GetIO().MousePos.y - 0.5f*ImGui::GetIO().DisplaySize.y)/ImGui::GetIO().DisplaySize.y)*(s0 - s1)*g_state.sizey0;
                }
            }

            if (g_state.isPinching) {
                vt.z = g_state.viewPinch.z - 1.0f*(1.0f - g_state.pinchScale)*(1.001f - g_state.viewCur.z);

                const float s0 = scale;
                const float s1 = g_state.scale(vt.z);

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

                g_state.nUpdates = 1;
            }

            //printf("%g %g\n", g_state.viewCur.z, g_state.zoomTgt);
        }

        g_state.anim.v1.z = std::max(kZoomMin, std::min(kZoomMax, g_state.anim.v1.z));

        g_state.isMoving  |= interp(g_state.viewCur.x, g_state.anim.v0.x, g_state.anim.v1.x, g_state.anim.t0, g_state.anim.t1, T, g_state.anim.type);
        g_state.isMoving  |= interp(g_state.viewCur.y, g_state.anim.v0.y, g_state.anim.v1.y, g_state.anim.t0, g_state.anim.t1, T, g_state.anim.type);
        g_state.isZooming |= interp(g_state.viewCur.z, g_state.anim.v0.z, g_state.anim.v1.z, g_state.anim.t0, g_state.anim.t1, T, g_state.anim.type);
    }

    const float scale = g_state.scale(g_state.viewCur.z);
    const float iscale = (1.0/scale)/ImGui::GetIO().DisplayFramebufferScale.x;

    const float xmin = g_state.viewCur.x - 0.5*g_state.sizex0*scale;
    const float ymin = g_state.viewCur.y - 0.5*g_state.sizey0*scale;
    const float xmax = g_state.viewCur.x + 0.5*g_state.sizex0*scale;
    const float ymax = g_state.viewCur.y + 0.5*g_state.sizey0*scale;

    const float idx = 1.0f/(xmax - xmin);
    const float idy = 1.0f/(ymax - ymin);

#ifdef USE_LINE_SHADER
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
        g_state.nUpdates = 1;
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

int main(int argc, char** argv) {
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

    const char * windowTitle = "Tweet2Doom - State Tree Explorer";

#ifdef __EMSCRIPTEN__
    SDL_Renderer * renderer;
    SDL_Window * window;
    SDL_CreateWindowAndRenderer(windowX, windowY, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_RENDERER_PRESENTVSYNC, &window, &renderer);
#else
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
        const float fontSize = 14.0f*kFontScale;
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "DroidSans.ttf", fontSize, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../fonts/DroidSans.ttf", fontSize, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../bin/DroidSans.ttf", fontSize, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../examples/assets/fonts/DroidSans.ttf", fontSize, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../examples/assets/fonts/DroidSans.ttf", fontSize, false);
    }

    {
        bool isNotLoaded = true;
        const float fontSize = 14.0f*kFontScale;
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "fontawesome-webfont.ttf", fontSize, true);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../fonts/fontawesome-webfont.ttf", fontSize, true);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../bin/fontawesome-webfont.ttf", fontSize, true);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../examples/assets/fonts/fontawesome-webfont.ttf", fontSize, true);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../examples/assets/fonts/fontawesome-webfont.ttf", fontSize, true);
    }

    {
        bool isNotLoaded = true;
        const float fontSize = 9.0f*kFontScale;
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "ProggyTiny.ttf", fontSize, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../fonts/ProggyTiny.ttf", fontSize, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../bin/ProggyTiny.ttf", fontSize, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../examples/assets/fonts/ProggyTiny.ttf", fontSize, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../examples/assets/fonts/ProggyTiny.ttf", fontSize, false);
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
        while (SDL_PollEvent(&event))
        {
            g_state.nUpdates = 5;
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

    g_addNode = [&](const std::string & id, const std::string & username, int level, int type, int x, int y) {
        g_nodes[id] = {
            id, username, level, type, float(x), float(y),
        };
    };

    g_updateNodePosition = [&](const std::string & id, int x, int y) {
        if (g_nodes.find(id) == g_nodes.end()) {
            fprintf(stderr, "Error: update position - unknown id '%s'\n", id.c_str());
            return;
        }

        g_nodes[id].x = x;
        g_nodes[id].y = y;
    };

    g_addEdge = [&](const std::string & src, const std::string & dst) {
        if (g_nodes.find(src) == g_nodes.end() ||
            g_nodes.find(dst) == g_nodes.end()) {
            // most likely a hidden node
            //fprintf(stderr, "Error: adding edge - unknown id '%s' or '%s'\n", src, dst);
            return;
        }

        g_edges.push_back({ src, dst, });
    };

    g_focusNode = [&](const std::string & id) {
        if (id.empty()) return;
        g_state.focusId = id;
    };

    g_getActionOpenId = [&]() {
        auto res = g_state.actionOpenId;
        g_state.actionOpenId.clear();
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

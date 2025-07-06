#include "stdafx.h"
#include "UIBezierController.h"
#include "UIAnimationTool.h"
#include "UISequencer.h"
#include "UIManager.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
using namespace ImGui;

const char* Bezier_Preset_Name[EASE_PRESET_MAX]
{
    "easeLinear", "easeInSine", "easeOutSine", "easeInOutSine", "easeInQuad", "easeOutQuad", "easeInOutQuad", "easeInCubic", "easeOutCubic", "easeInOutCubic",
    "easeInQuart", "easeOutQuart", "easeInOutQuart", "easeInQuint", "easeOutQuint", "easeInOutQuint", "easeInExpo", "easeOutExpo", "easeInOutExpo",
    "easeInCirc", "easeOutCirc", "easeInOutCirc", "easeInBack", "easeOutBack", "easeInOutBack"
};

float Bezier_Preset_Vec[EASE_PRESET_MAX][4]
{
    { 0.000f, 0.000f, 1.000f, 1.000f },
    { 0.470f, 0.000f, 0.745f, 0.715f },
    { 0.390f, 0.575f, 0.565f, 1.000f },
    { 0.445f, 0.050f, 0.550f, 0.950f },
    { 0.550f, 0.085f, 0.680f, 0.530f },
    { 0.250f, 0.460f, 0.450f, 0.940f },
    { 0.455f, 0.030f, 0.515f, 0.955f },
    { 0.550f, 0.055f, 0.675f, 0.190f },
    { 0.215f, 0.610f, 0.355f, 1.000f },
    { 0.645f, 0.045f, 0.355f, 1.000f },
    { 0.895f, 0.030f, 0.685f, 0.220f },
    { 0.165f, 0.840f, 0.440f, 1.000f },
    { 0.770f, 0.000f, 0.175f, 1.000f },
    { 0.755f, 0.050f, 0.855f, 0.060f },
    { 0.230f, 1.000f, 0.320f, 1.000f },
    { 0.860f, 0.000f, 0.070f, 1.000f },
    { 0.950f, 0.050f, 0.795f, 0.035f },
    { 0.190f, 1.000f, 0.220f, 1.000f },
    { 1.000f, 0.000f, 0.000f, 1.000f },
    { 0.600f, 0.040f, 0.980f, 0.335f },
    { 0.075f, 0.820f, 0.165f, 1.000f },
    { 0.785f, 0.135f, 0.150f, 0.860f },
    { 0.600f, -0.28f, 0.735f, 0.045f },
    { 0.175f, 0.885f, 0.320f, 1.275f },
    { 0.680f, -0.55f, 0.265f, 1.550f },
};


UIBezierController::UIBezierController(UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix) :UIBase(manager, name, pos, size, posFix)
{
    m_SelectItem = 0;
    memcpy(Bezier_Vec, Bezier_Preset_Vec[m_SelectItem], sizeof(float) * 4);
    m_type = UI_TYPE::BEZIERCONTROLLER;
}

UIBezierController::~UIBezierController()
{

}

void UIBezierController::Render()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove;
    bool open = true;
    ImGui::Begin("Bezier", &open, window_flags);
    UpdateWindowInfo();

    ShowBezierWindow();

    ImGui::End();
}
void UIBezierController::Update()
{

}
void UIBezierController::Input()
{

}



template<int steps>
void UIBezierController::bezier_table(ImVec2 P[4], ImVec2 results[steps + 1]) {
    static float C[(steps + 1) * 4], * K = 0;
    if (!K) {
        K = C;
        for (unsigned step = 0; step <= steps; ++step) {
            float t = (float)step / (float)steps;
            C[step * 4 + 0] = (1 - t) * (1 - t) * (1 - t);   // * P0
            C[step * 4 + 1] = 3 * (1 - t) * (1 - t) * t; // * P1
            C[step * 4 + 2] = 3 * (1 - t) * t * t;     // * P2
            C[step * 4 + 3] = t * t * t;               // * P3
        }
    }
    for (unsigned step = 0; step <= steps; ++step) {
        ImVec2 point = {
            K[step * 4 + 0] * P[0].x + K[step * 4 + 1] * P[1].x + K[step * 4 + 2] * P[2].x + K[step * 4 + 3] * P[3].x,
            K[step * 4 + 0] * P[0].y + K[step * 4 + 1] * P[1].y + K[step * 4 + 2] * P[2].y + K[step * 4 + 3] * P[3].y
        };
        results[step] = point;
    }
}

float UIBezierController::BezierValue(float dt01, float P[4]) {
    enum { STEPS = 256 };
    ImVec2 Q[4] = { { 0, 0 }, { P[0], P[1] }, { P[2], P[3] }, { 1, 1 } };
    ImVec2 results[STEPS + 1];
    bezier_table<STEPS>(Q, results);
    return results[(int)((dt01 < 0 ? 0 : dt01 > 1 ? 1 : dt01) * STEPS)].y;
}

int UIBezierController::Bezier(const char* label, float P[4]) {
    // visuals
    enum { SMOOTHNESS = 64 }; // curve smoothness: the higher number of segments, the smoother curve
    enum { CURVE_WIDTH = 4 }; // main curved line width
    enum { LINE_WIDTH = 1 }; // handlers: small lines width
    enum { GRAB_RADIUS = 6 }; // handlers: circle radius
    enum { GRAB_BORDER = 2 }; // handlers: circle border width

    const ImGuiStyle& Style = GetStyle();
    const ImGuiIO& IO = GetIO();
    ImDrawList* DrawList = GetWindowDrawList();
    ImGuiWindow* Window = GetCurrentWindow();
    if (Window->SkipItems)
        return false;

    // header and spacing
    int changed = SliderFloat4(label, P, 0, 1, "%.3f", 1.0f);
    int hovered = IsItemActive() || IsItemHovered(); // IsItemDragged() ?
    Dummy(ImVec2(0, 3));

    // prepare canvas
    const float avail = GetContentRegionAvail().x;
    const float dim = ImMin(avail, 128.f);
    ImVec2 Canvas(dim + 48, dim + 48);

    ImRect bb(Window->DC.CursorPos, ImVec2(Window->DC.CursorPos.x + Canvas.x, Window->DC.CursorPos.y + Canvas.y) + ImVec2(48, 48));
    ItemSize(bb);
    if (!ItemAdd(bb, NULL))
        return changed;

    const ImGuiID id = Window->GetID(label);
    hovered |= 0 != IsItemHovered(/*ImRect(bb.Min, ImVec2(bb.Min.x + avail, bb.Min.y + dim)), id*/);

    RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg, 1), true, Style.FrameRounding);

    // background grid
    for (int i = 0; i <= Canvas.x; i += (Canvas.x / 4)) {
        DrawList->AddLine(
            ImVec2(bb.Min.x + i, bb.Min.y),
            ImVec2(bb.Min.x + i, bb.Max.y),
            GetColorU32(ImGuiCol_TextDisabled));
    }
    for (int i = 0; i <= Canvas.y; i += (Canvas.y / 4)) {
        DrawList->AddLine(
            ImVec2(bb.Min.x, bb.Min.y + i),
            ImVec2(bb.Max.x, bb.Min.y + i),
            GetColorU32(ImGuiCol_TextDisabled));
    }

    // eval curve
    ImVec2 Q[4] = { { 0, 0 }, { P[0], P[1] }, { P[2], P[3] }, { 1, 1 } };
    ImVec2 results[SMOOTHNESS + 1];
    bezier_table<SMOOTHNESS>(Q, results);

    // control points: 2 lines and 2 circles
    {
        char buf[128];
        sprintf(buf, "0##%s", label);

        // handle grabbers
        for (int i = 0; i < 2; ++i)
        {
            ImVec2 pos = ImVec2(P[i * 2 + 0], 1 - P[i * 2 + 1]) * ImVec2(bb.Max.x - bb.Min.x, bb.Max.y - bb.Min.y) + bb.Min;
            SetCursorScreenPos(pos - ImVec2(GRAB_RADIUS, GRAB_RADIUS));
            InvisibleButton((buf[0]++, buf), ImVec2(2 * GRAB_RADIUS, 2 * GRAB_RADIUS));
            if (IsItemActive() || IsItemHovered())
            {
                SetTooltip("(%4.3f, %4.3f)", P[i * 2 + 0], P[i * 2 + 1]);
            }
            if (IsItemActive() && IsMouseDragging(0))
            {
                P[i * 2 + 0] += GetIO().MouseDelta.x / Canvas.x;
                P[i * 2 + 1] -= GetIO().MouseDelta.y / Canvas.y;
                changed = true;
            }
        }

        if (hovered || changed) DrawList->PushClipRectFullScreen();

        // draw curve
        {
            ImColor color(GetStyle().Colors[ImGuiCol_PlotLines]);
            for (int i = 0; i < SMOOTHNESS; ++i) {
                ImVec2 p = { results[i + 0].x, 1 - results[i + 0].y };
                ImVec2 q = { results[i + 1].x, 1 - results[i + 1].y };
                ImVec2 r(p.x * (bb.Max.x - bb.Min.x) + bb.Min.x, p.y * (bb.Max.y - bb.Min.y) + bb.Min.y);
                ImVec2 s(q.x * (bb.Max.x - bb.Min.x) + bb.Min.x, q.y * (bb.Max.y - bb.Min.y) + bb.Min.y);
                DrawList->AddLine(r, s, color, CURVE_WIDTH);
            }
        }

        // draw lines and grabbers
        float luma = IsItemActive() || IsItemHovered() ? 0.5f : 1.0f;
        ImVec4 pink(1.00f, 0.00f, 0.75f, luma), cyan(0.00f, 0.75f, 1.00f, luma);
        ImVec4 white(GetStyle().Colors[ImGuiCol_Text]);
        ImVec2 p1 = ImVec2(P[0], 1 - P[1]) * (bb.Max - bb.Min) + bb.Min;
        ImVec2 p2 = ImVec2(P[2], 1 - P[3]) * (bb.Max - bb.Min) + bb.Min;
        DrawList->AddLine(ImVec2(bb.Min.x, bb.Max.y), p1, ImColor(white), LINE_WIDTH);
        DrawList->AddLine(ImVec2(bb.Max.x, bb.Min.y), p2, ImColor(white), LINE_WIDTH);
        DrawList->AddCircleFilled(p1, GRAB_RADIUS, ImColor(white));
        DrawList->AddCircleFilled(p1, GRAB_RADIUS - GRAB_BORDER, ImColor(pink));
        DrawList->AddCircleFilled(p2, GRAB_RADIUS, ImColor(white));
        DrawList->AddCircleFilled(p2, GRAB_RADIUS - GRAB_BORDER, ImColor(cyan));

        if (hovered || changed) DrawList->PopClipRect();

        // restore cursor pos
        SetCursorScreenPos(ImVec2(bb.Min.x, bb.Max.y + GRAB_RADIUS)); // :P
    }

    return changed;
}

void UIBezierController::SetBezierValue(int ItemIndex, float bezier[4])
{
    m_SelectItem = ItemIndex;
    memcpy(Bezier_Vec, bezier, sizeof(float) * 4);
}

void UIBezierController::ShowBezierWindow()
{
    bool isChange = false;

    if (ImGui::Combo("Bezier", &m_SelectItem, Bezier_Preset_Name, EASE_PRESET_MAX))
    {
        if (!ImGui::IsWindowFocused())
            ImGui::SetWindowFocus();

        memcpy(Bezier_Vec, Bezier_Preset_Vec[m_SelectItem], sizeof(float) * 4);

        isChange |= true;

    }
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        if (!ImGui::IsWindowFocused())
            ImGui::SetWindowFocus();

        memcpy(Bezier_Vec, Bezier_Preset_Vec[m_SelectItem], sizeof(float) * 4);
        isChange |= true;
    }

    if (Bezier(Bezier_Preset_Name[m_SelectItem], Bezier_Vec))
    {
        if (!ImGui::IsWindowFocused())
            ImGui::SetWindowFocus();

        isChange |= true;
    }

    if (isChange)
    {
        SQ_Key* key = ((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetCurrentKey();
        if (key != nullptr)
        {
            key->aniValue.bezierIndex = m_SelectItem;
            key->aniValue.B1 = ((UIBezierController*)g_UIManager->GetUI(UI_TYPE::BEZIERCONTROLLER))->GetB1();
            key->aniValue.B2 = ((UIBezierController*)g_UIManager->GetUI(UI_TYPE::BEZIERCONTROLLER))->GetB2();
        }
    }

    // easeInElastic: not a bezier
    // easeOutElastic: not a bezier
    // easeInOutElastic: not a bezier
    // easeInBounce: not a bezier
    // easeOutBounce: not a bezier
    // easeInOutBounce: not a bezier
}
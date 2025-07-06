#include "stdafx.h"
#include "UIAnimation.h"
#include "Engine.h"
//#include "UIManager.h"
//#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_neo_internal.h"
#include "imgui/imgui_neo_sequencer.h"

#if _DF_GIZMO_SEQUENCER_
#include "imgui/ImSequencer.h"
#endif

#if _DF_SEQUENTITY_
#include "entt/entt.hpp"
#include "entt/entity/registry.hpp"
#include "imgui/Sequentity.h"
#endif


#define MAX_LOADSTRING 100

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.
//LPDIRECT3D9         g_pD3D = NULL; // Used to create the D3DDevice
//LPDIRECT3DDEVICE9   g_pd3dDevice = NULL; // Our rendering device

static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
D3DXVECTOR3 tempPos;
D3DXVECTOR2 mousePos;
D3DXVECTOR2 mouseDownPos;
D3DXVECTOR2 mousePosWorld;
bool mousePress = false;
bool mouseDown = false;
bool mouseUp = false;
bool spaceKey = false;
bool ctrlKey = false;
bool shiftKey = false;
bool deleteKey = false;

//static D3DPRESENT_PARAMETERS    g_d3dpp = {};


ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
//void Render();
//void ResetDevice();

#if _DF_SEQUENTITY_
entt::registry registry;
#endif

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);


    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_UIANIMATIONTOOL, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);


    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }


    MSG msg;

    // 기본 메시지 루프입니다:
    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT)
            return FALSE;

        gEngine->Update();

        gEngine->Render();

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            gEngine->GetD3Dpp().BackBufferWidth = g_ResizeWidth;
            gEngine->GetD3Dpp().BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;

            gEngine->ReleaseAllTexture();
            gEngine->ResetDevice();
        }
    }

    delete gEngine;

    return (int)msg.wParam;
}

#if _DF_GIZMO_SEQUENCER_
static const char* SequencerItemTypeNames[] = { "Camera","Music", "ScreenEffect", "FadeIn", "Animation" };

struct MySequence : public ImSequencer::SequenceInterface
{
    // interface with sequencer

    virtual int GetFrameMin() const {
        return mFrameMin;
    }
    virtual int GetFrameMax() const {
        return mFrameMax;
    }
    virtual int GetItemCount() const { return (int)myItems.size(); }

    virtual int GetItemTypeCount() const { return sizeof(SequencerItemTypeNames) / sizeof(char*); }
    virtual const char* GetItemTypeName(int typeIndex) const { return SequencerItemTypeNames[typeIndex]; }
    virtual const char* GetItemLabel(int index) const
    {
        static char tmps[512];
        snprintf(tmps, 512, "[%02d] %s", index, SequencerItemTypeNames[myItems[index].mType]);
        return tmps;
    }

    virtual void Get(int index, int** start, int** end, int* type, unsigned int* color)
    {
        MySequenceItem& item = myItems[index];
        if (color)
            *color = 0xFFAA8080; // same color for everyone, return color based on type
        if (start)
            *start = &item.mFrameStart;
        if (end)
            *end = &item.mFrameEnd;
        if (type)
            *type = item.mType;
    }
    virtual void Add(int type) { myItems.push_back(MySequenceItem{ type, 0, 10, false }); };
    virtual void Del(int index) { myItems.erase(myItems.begin() + index); }
    virtual void Duplicate(int index) { myItems.push_back(myItems[index]); }

    virtual size_t GetCustomHeight(int index) { return myItems[index].mExpanded ? 300 : 0; }

    // my datas
    MySequence() : mFrameMin(0), mFrameMax(0) {}
    int mFrameMin, mFrameMax;
    struct MySequenceItem
    {
        int mType;
        int mFrameStart, mFrameEnd;
        bool mExpanded;
    };
    std::vector<MySequenceItem> myItems;
    //RampEdit rampEdit;

    virtual void DoubleClick(int index) {
        if (myItems[index].mExpanded)
        {
            myItems[index].mExpanded = false;
            return;
        }
        for (auto& item : myItems)
            item.mExpanded = false;
        myItems[index].mExpanded = !myItems[index].mExpanded;
    }

    virtual void CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect)
    {
        static const char* labels[] = { "Translation", "Rotation" , "Scale" };

        //rampEdit.mMax = ImVec2(float(mFrameMax), 1.f);
        //rampEdit.mMin = ImVec2(float(mFrameMin), 0.f);
        draw_list->PushClipRect(legendClippingRect.Min, legendClippingRect.Max, true);
        for (int i = 0; i < 3; i++)
        {
            ImVec2 pta(legendRect.Min.x + 30, legendRect.Min.y + i * 14.f);
            ImVec2 ptb(legendRect.Max.x, legendRect.Min.y + (i + 1) * 14.f);
            //draw_list->AddText(pta, rampEdit.mbVisible[i] ? 0xFFFFFFFF : 0x80FFFFFF, labels[i]);
            //if (ImRect(pta, ptb).Contains(ImGui::GetMousePos()) && ImGui::IsMouseClicked(0))
            //    rampEdit.mbVisible[i] = !rampEdit.mbVisible[i];
        }
        draw_list->PopClipRect();

        ImGui::SetCursorScreenPos(rc.Min);
        //ImCurveEdit::Edit(rampEdit, rc.Max - rc.Min, 137 + index, &clippingRect);
    }

    virtual void CustomDrawCompact(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& clippingRect)
    {
        //rampEdit.mMax = ImVec2(float(mFrameMax), 1.f);
        //rampEdit.mMin = ImVec2(float(mFrameMin), 0.f);
        draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);
        for (int i = 0; i < 3; i++)
        {
            /*for (unsigned int j = 0; j < rampEdit.mPointCount[i]; j++)
            {
                float p = rampEdit.mPts[i][j].x;
                if (p < myItems[index].mFrameStart || p > myItems[index].mFrameEnd)
                    continue;
                float r = (p - mFrameMin) / float(mFrameMax - mFrameMin);
                float x = ImLerp(rc.Min.x, rc.Max.x, r);
                draw_list->AddLine(ImVec2(x, rc.Min.y + 6), ImVec2(x, rc.Max.y - 4), 0xAA000000, 4.f);
            }
            */
        }
        draw_list->PopClipRect();
    }
};
MySequence mySequence;
#endif

//namespace ImGui
//{
//
//    template<int steps>
//    void bezier_table(ImVec2 P[4], ImVec2 results[steps + 1]) {
//        static float C[(steps + 1) * 4], * K = 0;
//        if (!K) {
//            K = C;
//            for (unsigned step = 0; step <= steps; ++step) {
//                float t = (float)step / (float)steps;
//                C[step * 4 + 0] = (1 - t) * (1 - t) * (1 - t);   // * P0
//                C[step * 4 + 1] = 3 * (1 - t) * (1 - t) * t; // * P1
//                C[step * 4 + 2] = 3 * (1 - t) * t * t;     // * P2
//                C[step * 4 + 3] = t * t * t;               // * P3
//            }
//        }
//        for (unsigned step = 0; step <= steps; ++step) {
//            ImVec2 point = {
//                K[step * 4 + 0] * P[0].x + K[step * 4 + 1] * P[1].x + K[step * 4 + 2] * P[2].x + K[step * 4 + 3] * P[3].x,
//                K[step * 4 + 0] * P[0].y + K[step * 4 + 1] * P[1].y + K[step * 4 + 2] * P[2].y + K[step * 4 + 3] * P[3].y
//            };
//            results[step] = point;
//        }
//    }
//
//    float BezierValue(float dt01, float P[4]) {
//        enum { STEPS = 256 };
//        ImVec2 Q[4] = { { 0, 0 }, { P[0], P[1] }, { P[2], P[3] }, { 1, 1 } };
//        ImVec2 results[STEPS + 1];
//        bezier_table<STEPS>(Q, results);
//        return results[(int)((dt01 < 0 ? 0 : dt01 > 1 ? 1 : dt01) * STEPS)].y;
//    }
//
//    int Bezier(const char* label, float P[4]) {
//        // visuals
//        enum { SMOOTHNESS = 64 }; // curve smoothness: the higher number of segments, the smoother curve
//        enum { CURVE_WIDTH = 4 }; // main curved line width
//        enum { LINE_WIDTH = 1 }; // handlers: small lines width
//        enum { GRAB_RADIUS = 6 }; // handlers: circle radius
//        enum { GRAB_BORDER = 2 }; // handlers: circle border width
//
//        const ImGuiStyle& Style = GetStyle();
//        const ImGuiIO& IO = GetIO();
//        ImDrawList* DrawList = GetWindowDrawList();
//        ImGuiWindow* Window = GetCurrentWindow();
//        if (Window->SkipItems)
//            return false;
//
//        // header and spacing
//        int changed = SliderFloat4(label, P, 0, 1, "%.3f", 1.0f);
//        int hovered = IsItemActive() || IsItemHovered(); // IsItemDragged() ?
//        Dummy(ImVec2(0, 3));
//
//        // prepare canvas
//        const float avail = GetContentRegionAvail().x;
//        const float dim = ImMin(avail, 128.f);
//        ImVec2 Canvas(dim, dim);
//
//        ImRect bb(Window->DC.CursorPos, ImVec2(Window->DC.CursorPos.x + Canvas.x, Window->DC.CursorPos.y + Canvas.y));
//        ItemSize(bb);
//        if (!ItemAdd(bb, NULL))
//            return changed;
//
//        const ImGuiID id = Window->GetID(label);
//        hovered |= 0 != IsItemHovered(/*ImRect(bb.Min, ImVec2(bb.Min.x + avail, bb.Min.y + dim)), id*/);
//
//        RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg, 1), true, Style.FrameRounding);
//
//        // background grid
//        for (int i = 0; i <= Canvas.x; i += (Canvas.x / 4)) {
//            DrawList->AddLine(
//                ImVec2(bb.Min.x + i, bb.Min.y),
//                ImVec2(bb.Min.x + i, bb.Max.y),
//                GetColorU32(ImGuiCol_TextDisabled));
//        }
//        for (int i = 0; i <= Canvas.y; i += (Canvas.y / 4)) {
//            DrawList->AddLine(
//                ImVec2(bb.Min.x, bb.Min.y + i),
//                ImVec2(bb.Max.x, bb.Min.y + i),
//                GetColorU32(ImGuiCol_TextDisabled));
//        }
//
//        // eval curve
//        ImVec2 Q[4] = { { 0, 0 }, { P[0], P[1] }, { P[2], P[3] }, { 1, 1 } };
//        ImVec2 results[SMOOTHNESS + 1];
//        bezier_table<SMOOTHNESS>(Q, results);
//
//        // control points: 2 lines and 2 circles
//        {
//            char buf[128];
//            sprintf(buf, "0##%s", label);
//
//            // handle grabbers
//            for (int i = 0; i < 2; ++i)
//            {
//                ImVec2 pos = ImVec2(P[i * 2 + 0], 1 - P[i * 2 + 1]) * ImVec2(bb.Max.x - bb.Min.x, bb.Max.y - bb.Min.y) + bb.Min;
//                SetCursorScreenPos(pos - ImVec2(GRAB_RADIUS, GRAB_RADIUS));
//                InvisibleButton((buf[0]++, buf), ImVec2(2 * GRAB_RADIUS, 2 * GRAB_RADIUS));
//                if (IsItemActive() || IsItemHovered())
//                {
//                    SetTooltip("(%4.3f, %4.3f)", P[i * 2 + 0], P[i * 2 + 1]);
//                }
//                if (IsItemActive() && IsMouseDragging(0))
//                {
//                    P[i * 2 + 0] += GetIO().MouseDelta.x / Canvas.x;
//                    P[i * 2 + 1] -= GetIO().MouseDelta.y / Canvas.y;
//                    changed = true;
//                }
//            }
//
//            if (hovered || changed) DrawList->PushClipRectFullScreen();
//
//            // draw curve
//            {
//                ImColor color(GetStyle().Colors[ImGuiCol_PlotLines]);
//                for (int i = 0; i < SMOOTHNESS; ++i) {
//                    ImVec2 p = { results[i + 0].x, 1 - results[i + 0].y };
//                    ImVec2 q = { results[i + 1].x, 1 - results[i + 1].y };
//                    ImVec2 r(p.x * (bb.Max.x - bb.Min.x) + bb.Min.x, p.y * (bb.Max.y - bb.Min.y) + bb.Min.y);
//                    ImVec2 s(q.x * (bb.Max.x - bb.Min.x) + bb.Min.x, q.y * (bb.Max.y - bb.Min.y) + bb.Min.y);
//                    DrawList->AddLine(r, s, color, CURVE_WIDTH);
//                }
//            }
//
//            // draw lines and grabbers
//            float luma = IsItemActive() || IsItemHovered() ? 0.5f : 1.0f;
//            ImVec4 pink(1.00f, 0.00f, 0.75f, luma), cyan(0.00f, 0.75f, 1.00f, luma);
//            ImVec4 white(GetStyle().Colors[ImGuiCol_Text]);
//            ImVec2 p1 = ImVec2(P[0], 1 - P[1]) * (bb.Max - bb.Min) + bb.Min;
//            ImVec2 p2 = ImVec2(P[2], 1 - P[3]) * (bb.Max - bb.Min) + bb.Min;
//            DrawList->AddLine(ImVec2(bb.Min.x, bb.Max.y), p1, ImColor(white), LINE_WIDTH);
//            DrawList->AddLine(ImVec2(bb.Max.x, bb.Min.y), p2, ImColor(white), LINE_WIDTH);
//            DrawList->AddCircleFilled(p1, GRAB_RADIUS, ImColor(white));
//            DrawList->AddCircleFilled(p1, GRAB_RADIUS - GRAB_BORDER, ImColor(pink));
//            DrawList->AddCircleFilled(p2, GRAB_RADIUS, ImColor(white));
//            DrawList->AddCircleFilled(p2, GRAB_RADIUS - GRAB_BORDER, ImColor(cyan));
//
//            if (hovered || changed) DrawList->PopClipRect();
//
//            // restore cursor pos
//            SetCursorScreenPos(ImVec2(bb.Min.x, bb.Max.y + GRAB_RADIUS)); // :P
//        }
//
//        return changed;
//    }
//
//    void ShowBezierDemo()
//    {
//        { static float v[] = { 0.000f, 0.000f, 1.000f, 1.000f }; Bezier("easeLinear", v); }
//        { static float v[] = { 0.470f, 0.000f, 0.745f, 0.715f }; Bezier("easeInSine", v); }
//        { static float v[] = { 0.390f, 0.575f, 0.565f, 1.000f }; Bezier("easeOutSine", v); }
//        { static float v[] = { 0.445f, 0.050f, 0.550f, 0.950f }; Bezier("easeInOutSine", v); }
//        { static float v[] = { 0.550f, 0.085f, 0.680f, 0.530f }; Bezier("easeInQuad", v); }
//        { static float v[] = { 0.250f, 0.460f, 0.450f, 0.940f }; Bezier("easeOutQuad", v); }
//        { static float v[] = { 0.455f, 0.030f, 0.515f, 0.955f }; Bezier("easeInOutQuad", v); }
//        { static float v[] = { 0.550f, 0.055f, 0.675f, 0.190f }; Bezier("easeInCubic", v); }
//        { static float v[] = { 0.215f, 0.610f, 0.355f, 1.000f }; Bezier("easeOutCubic", v); }
//        { static float v[] = { 0.645f, 0.045f, 0.355f, 1.000f }; Bezier("easeInOutCubic", v); }
//        { static float v[] = { 0.895f, 0.030f, 0.685f, 0.220f }; Bezier("easeInQuart", v); }
//        { static float v[] = { 0.165f, 0.840f, 0.440f, 1.000f }; Bezier("easeOutQuart", v); }
//        { static float v[] = { 0.770f, 0.000f, 0.175f, 1.000f }; Bezier("easeInOutQuart", v); }
//        { static float v[] = { 0.755f, 0.050f, 0.855f, 0.060f }; Bezier("easeInQuint", v); }
//        { static float v[] = { 0.230f, 1.000f, 0.320f, 1.000f }; Bezier("easeOutQuint", v); }
//        { static float v[] = { 0.860f, 0.000f, 0.070f, 1.000f }; Bezier("easeInOutQuint", v); }
//        { static float v[] = { 0.950f, 0.050f, 0.795f, 0.035f }; Bezier("easeInExpo", v); }
//        { static float v[] = { 0.190f, 1.000f, 0.220f, 1.000f }; Bezier("easeOutExpo", v); }
//        { static float v[] = { 1.000f, 0.000f, 0.000f, 1.000f }; Bezier("easeInOutExpo", v); }
//        { static float v[] = { 0.600f, 0.040f, 0.980f, 0.335f }; Bezier("easeInCirc", v); }
//        { static float v[] = { 0.075f, 0.820f, 0.165f, 1.000f }; Bezier("easeOutCirc", v); }
//        { static float v[] = { 0.785f, 0.135f, 0.150f, 0.860f }; Bezier("easeInOutCirc", v); }
//        { static float v[] = { 0.600f, -0.28f, 0.735f, 0.045f }; Bezier("easeInBack", v); }
//        { static float v[] = { 0.175f, 0.885f, 0.320f, 1.275f }; Bezier("easeOutBack", v); }
//        { static float v[] = { 0.680f, -0.55f, 0.265f, 1.550f }; Bezier("easeInOutBack", v); }
//        // easeInElastic: not a bezier
//        // easeOutElastic: not a bezier
//        // easeInOutElastic: not a bezier
//        // easeInBounce: not a bezier
//        // easeOutBounce: not a bezier
//        // easeInOutBounce: not a bezier
//    }
//}



//bool Init_Imgui()
//{
//    // #_IMGUI: IMGUI 초기화
//    // Setup Dear ImGui context
//    IMGUI_CHECKVERSION();
//    ImGui::CreateContext();
//
//    ImGuiIO& io = ImGui::GetIO(); (void)io;
//    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
//    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
////	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
//    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
////	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;       // Enable Multi-Viewport / Platform Windows
//
//    // io.ConfigViewportsNoAutoMerge = true;
//    // io.ConfigViewportsNoTaskBarIcon = true;
//
//    // 폰트 추가 
//    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgunbd.ttf", 17.0f, NULL, io.Fonts->GetGlyphRangesKorean());
//
//    ImGuiStyle& style = ImGui::GetStyle();
//    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
//    {
//        style.WindowRounding = 0.0f;
//        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
//    }
//
//    // ImGui::Spectrum::StyleColorsSpectrum();
//
//    ImGui_ImplWin32_Init(WinApp.GetHandle());
//    ImGui_ImplDX9_Init(g_pDevice);
//
//    g_PostProcess->bPostProcess = false;
//    ShowCursor(true);
//
//    m_flag_InputText = 0;
//    m_flag_Slider = 0;
//    m_DragSpeed = 0.01f;
//
//#if _DF_IMGUITOOL_INGAME_
//    Create_Tool_INGAME();
//#elif _DF_IMGUITOOL_PARTICLE_
//    Create_Tool_Partilce();
//#endif
//    return true;
//}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_UIANIMATIONTOOL));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = nullptr;

    return RegisterClassExW(&wcex);
}

//HRESULT InitD3D(HWND hWnd)
//{
//    if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
//        return E_FAIL;
//
//    D3DPRESENT_PARAMETERS d3dpp;
//    ZeroMemory(&d3dpp, sizeof(d3dpp));
//    d3dpp.Windowed = TRUE;
//    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
//    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
//
//    if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
//        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
//        &d3dpp, &g_pd3dDevice)))
//    {
//        return E_FAIL;
//    }
//
//    return S_OK;
//}

//bool CreateDeviceD3D(HWND hWnd)
//{
//    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
//        return false;
//
//    // Create the D3DDevice
//    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
//    g_d3dpp.Windowed = TRUE;
//    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
//    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
//    g_d3dpp.EnableAutoDepthStencil = TRUE;
//    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
//    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
//    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
//    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
//        return false;
//
//    return true;
//}
//
//
//void ResetDevice()
//{
//    ImGui_ImplDX9_InvalidateDeviceObjects();
//    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
//    if (hr == D3DERR_INVALIDCALL)
//        IM_ASSERT(0);
//    ImGui_ImplDX9_CreateDeviceObjects();
//}


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    gEngine = new Engine();
    gEngine->Init(hWnd, hInstance);

    //IMGUI_CHECKVERSION();
    //ImGui::CreateContext();
    //ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    //ImGui::StyleColorsDark();
    ////ImGui::StyleColorsLight();

    //// Setup Platform/Renderer backends
    //ImGui_ImplWin32_Init(hWnd);
    //ImGui_ImplDX9_Init(gEngine->GetDevice());

#if _DF_GIZMO_SEQUENCER_
    mySequence.mFrameMin = -100;
    mySequence.mFrameMax = 1000;
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 0, 10, 30, false });
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 1, 20, 30, true });
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 3, 12, 60, false });
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 2, 61, 90, false });
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 4, 90, 99, false });
#endif

#if _DF_SEQUENTITY_
    //registry.
    auto entity = registry.create();
    auto& track = registry.emplace<Sequentity::Track>(entity); {
        track.label = "My first track";
        track.color = ImColor::HSV(0.0f, 0.5f, 0.75f);
    }

    auto& channel = Sequentity::PushChannel(track, 0); {
        channel.label = "My first channel";
        channel.color = ImColor::HSV(0.33f, 0.5f, 0.75f);
    }

    auto& event = Sequentity::PushEvent(channel); {
        event.time = 1;
        event.length = 50;
        event.color = ImColor::HSV(0.66f, 0.5f, 0.75f);
    }
#endif
    return TRUE;
}

//void Cleanup()
//{
//    if (g_pd3dDevice != NULL)
//        g_pd3dDevice->Release();
//
//    if (g_pD3D != NULL)
//        g_pD3D->Release();
//}

////NeoSequencer
//int32_t currentFrame = 0;
//int32_t startFrame = 0;
//int32_t endFrame = 64;
//static bool transformOpen = false;
//std::vector<ImGui::FrameIndexType> keys = { 0, 10, 24 };
//bool doDelete = false;
////NeoSequencer

//void UIRender()
//{
//    // Start the Dear ImGui frame
//    ImGui_ImplDX9_NewFrame();
//    ImGui_ImplWin32_NewFrame();
//    ImGui::NewFrame();
//
//
//
//
//    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
//    {
//        //ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
//        static float f = 0.0f;
//        static int counter = 0;
//        bool temp = 0;
//
//        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
//
//        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
//        ImGui::Checkbox("Demo Window", &temp);      // Edit bools storing our window open/close state
//        ImGui::Checkbox("Another Window", &temp);
//
//        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
//
//        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
//            counter++;
//        ImGui::SameLine();
//        ImGui::Text("counter = %d", counter);
//
//        //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
//
//#if _DF_GIZMO_SEQUENCER_
//    //Gezmo Sequencer
//        static int selectedEntry = -1;
//        static int firstFrame = 0;
//        static bool expanded = true;
//        static int currentFrame = 100;
//        Sequencer(&mySequence, &currentFrame, &expanded, &selectedEntry, &firstFrame, ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_COPYPASTE | ImSequencer::SEQUENCER_CHANGE_FRAME);
//        //Gezmo Sequencer
//#endif
//    //ImGui::ShowBezierDemo();
//
//        ImGui::End();
//
//#if _DF_SEQUENTITY_
//        ImGui::Begin("Sequencer");
//        Sequentity::EventEditor(registry);
//        ImGui::End();
//#endif
//
//#if _DF_NEO_SEQUENCER_
//        //내가 만든거
//        if (ImGui::BeginNeoSequencer("Sequencer", &currentFrame, &startFrame, &endFrame, { 0, 0 },
//            ImGuiNeoSequencerFlags_AllowLengthChanging |
//            ImGuiNeoSequencerFlags_EnableSelection |
//            ImGuiNeoSequencerFlags_Selection_EnableDragging |
//            ImGuiNeoSequencerFlags_Selection_EnableDeletion))
//        {
//            if (ImGui::BeginNeoGroup("Transform", &transformOpen))
//            {
//
//                if (ImGui::BeginNeoTimelineEx("Position"))
//                {
//                    for (auto&& v : keys)
//                    {
//                        ImGui::NeoKeyframe(&v);
//                        // Per keyframe code here
//                    }
//
//
//                    if (doDelete)
//                    {
//                        uint32_t count = ImGui::GetNeoKeyframeSelectionSize();
//
//                        ImGui::FrameIndexType* toRemove = new ImGui::FrameIndexType[count];
//
//                        ImGui::GetNeoKeyframeSelection(toRemove);
//
//                        //Delete keyframes from your structure
//                    }
//                    ImGui::EndNeoTimeLine();
//                }
//                ImGui::EndNeoGroup();
//            }
//
//            ImGui::EndNeoSequencer();
//        }
//#endif
//
//
//    }
//
//    // Rendering
//    ImGui::EndFrame();
//    ImGui::Render();
//    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
//
//    ImGuiIO& io = ImGui::GetIO();
//    // Update and Render additional Platform Windows
//    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
//    {
//        ImGui::UpdatePlatformWindows();
//        ImGui::RenderPlatformWindowsDefault();
//    }
//
//}


//void Render()
//{
//    if (NULL == g_pd3dDevice)
//        return;
//
//    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
//
//    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
//    {
//        UIRender();
//        g_pd3dDevice->EndScene();
//    }
//
//    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
//}


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        break;

        //case WM_COMMAND:
        //    {
        //        int wmId = LOWORD(wParam);
        //        // 메뉴 선택을 구문 분석합니다:
        //        switch (wmId)
        //        {
        //        case IDM_ABOUT:
        //            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
        //            break;
        //        case IDM_EXIT:
        //            DestroyWindow(hWnd);
        //            break;
        //        default:
        //            return DefWindowProc(hWnd, message, wParam, lParam);
        //        }
        //    }
        //    break;

    case WM_MOVE:
        //Render();
        break;

    case WM_MOUSEMOVE:
        mousePos.x = LOWORD(lParam);
        mousePos.y = HIWORD(lParam);
        break;

    case WM_LBUTTONDOWN:
        mousePress = true;
        mouseDownPos.x = LOWORD(lParam);
        mouseDownPos.y = HIWORD(lParam);
        break;

    case WM_LBUTTONUP:
        mousePress = false;
        break;

    case WM_CHAR:
        if (wParam == 'w')
        {
            tempPos.y += 1;
        }
        else if (wParam == 's')
        {
            tempPos.y -= 1;
        }
        else if (wParam == 'a')
        {
            tempPos.x -= 1;
        }
        else if (wParam == 'd')
        {
            tempPos.x += 1;
        }
        else if (wParam == 'z')
        {
            tempPos.z -= 1;
        }
        else if (wParam == 'x')
        {
            tempPos.z += 1;
        }
        else if (wParam == ' ')
        {
            spaceKey = true;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_LCONTROL)
        {
            ctrlKey = true;
        }
        else if (wParam == VK_LSHIFT)
        {
            shiftKey = true;
        }
        else if (wParam == VK_DELETE)
        {
            deleteKey = true;
        }
        break;

    case WM_KEYUP:
        if (wParam == VK_LCONTROL)
        {
            ctrlKey = false;
        }
        else if (wParam == VK_LSHIFT)
        {
            shiftKey = false;
        }
        else if (wParam == VK_DELETE)
        {
            deleteKey = false;
        }
        break;

    case WM_DESTROY:
    {
        PostQuitMessage(0);
    }
    break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 정보 대화 상자의 메시지 처리기입니다.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

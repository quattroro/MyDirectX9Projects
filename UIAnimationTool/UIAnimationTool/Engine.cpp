#include "stdafx.h"
#include "Engine.h"
#include "UIManager.h"
#include "UISequencer.h"
#include "UIHierarkhy.h"
#include "UITransform.h"
#include "UIScreen.h"
#include "UIBezierController.h"
#include "GameObject.h"
#include "InputManager.h"
#include "UIAnimationTool.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_neo_internal.h"
#include "imgui/imgui_neo_sequencer.h"

Engine* gEngine = nullptr;
D3DXMATRIX matWorld;
D3DXMATRIX matView;
D3DXMATRIX matProj;

Engine::~Engine()
{
    if (m_pd3dDevice != NULL)
    {
        m_pd3dDevice->Release();
        m_pd3dDevice = NULL;
    }

    if (m_pD3D != NULL)
    {
        m_pD3D->Release();
        m_pD3D = NULL;
    }

    if (m_UIManager != nullptr)
    {
        delete m_UIManager;
        m_UIManager = NULL;
    }
}

void Engine::Init(HWND hWnd, HINSTANCE hInst)
{
    m_hInst = hInst;
    m_hWnd = hWnd;
    InitD3D(hWnd);
    InitImGui(hWnd);
    InitShader();

    g_InputManager->Init();

    g_UIManager->Add(UI_TYPE::SEQUENCER, new UISequencer(g_UIManager, "Sequencer", D3DXVECTOR2(0, 0), D3DXVECTOR2(0, 0), true));

    g_UIManager->Add(UI_TYPE::HIERARKHY, new UIHierarkhy(g_UIManager, "Hierarkhy", D3DXVECTOR2(0, 0), D3DXVECTOR2(0, 0), true));

    g_UIManager->Add(UI_TYPE::TRANSFORM, new UITransform(g_UIManager, "Transform", D3DXVECTOR2(0, 0), D3DXVECTOR2(0, 0), true));

    g_UIManager->Add(UI_TYPE::SCREEN, new UIScreen(g_UIManager, "Screen", D3DXVECTOR2(0, 0), D3DXVECTOR2(0, 0), true));

    g_UIManager->Add(UI_TYPE::BEZIERCONTROLLER, new UIBezierController(g_UIManager, "BezierController", D3DXVECTOR2(0, 0), D3DXVECTOR2(0, 0), true));

}



void Engine::Render()
{
    if (NULL == m_pd3dDevice)
        return;

    m_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    if (SUCCEEDED(m_pd3dDevice->BeginScene()))
    {
        g_UIManager->Render();

        vector<TreeNode<class GameObject*>*> treenodes = ((UIHierarkhy*)g_UIManager->GetUI(UI_TYPE::HIERARKHY))->GetAllNodes();

        D3DVIEWPORT9  oldViewport;
        gEngine->GetDevice()->GetViewport(&oldViewport);
        D3DVIEWPORT9  curViewport = ((UIScreen*)g_UIManager->GetUI(UI_TYPE::SCREEN))->GetViewport();
        gEngine->GetDevice()->SetViewport(&curViewport);
        gEngine->GetDevice()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        gEngine->GetDevice()->SetViewport(&oldViewport);

        for (int i = treenodes.size() - 1; i > 0; i--)
        {
            treenodes[i]->GetData()->Render();
        }

        m_pd3dDevice->EndScene();
    }

    m_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}

void Engine::Update()
{
    if (NULL == m_pd3dDevice)
        return;

    g_InputManager->Update();
    g_UIManager->Update();
}

bool Engine::InitShader()
{
    LPD3DXBUFFER		lpErrors = 0;

    char fileName[1024];
    sprintf(fileName, "Shader\\UIShader.fx");

    HRESULT hr = D3DXCreateEffectFromFile(gEngine->GetDevice(), fileName, NULL, NULL, D3DXSHADER_USE_LEGACY_D3DX9_31_DLL, NULL, &m_ShaderEffect, &lpErrors);
    if (hr != S_OK)
    {
        char strLog[1024];
        sprintf(strLog, "Error : %s", (char*)lpErrors->GetBufferPointer());
        return false;
    }
    return true;
}

LPD3DXEFFECT Engine::GetShader()
{
    LPD3DXBUFFER		lpErrors = 0;
    if (m_ShaderEffect == NULL)
    {
        char fileName[1024];
        sprintf(fileName, "Shader\\UIShader.fx");
        HRESULT hr = D3DXCreateEffectFromFile(gEngine->GetDevice(), fileName, NULL, NULL, D3DXSHADER_USE_LEGACY_D3DX9_31_DLL, NULL, &m_ShaderEffect, &lpErrors);
        if (hr != S_OK)
        {
            char strLog[1024];
            sprintf(strLog, "Error : %s", (char*)lpErrors->GetBufferPointer());
            return NULL;
        }
    }

    return m_ShaderEffect;
}

HRESULT Engine::InitD3D(HWND hWnd)
{
    if ((m_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return E_FAIL;

    ZeroMemory(&m_d3dpp, sizeof(m_d3dpp));
    m_d3dpp.Windowed = TRUE;
    m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    m_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    m_d3dpp.EnableAutoDepthStencil = TRUE;
    m_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate

    if (m_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING/*D3DCREATE_SOFTWARE_VERTEXPROCESSING*/, &m_d3dpp, &m_pd3dDevice) < 0)
        return E_FAIL;

    m_pd3dDevice->GetDeviceCaps(&m_Caps);

    return S_OK;
}

bool Engine::InitImGui(HWND hWnd)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX9_Init(m_pd3dDevice);

    return true;
}


void Engine::ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = m_pd3dDevice->Reset(&m_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

void Engine::ReleaseAllTexture()
{
    if (m_ShaderEffect != NULL)
    {
        m_ShaderEffect->Release();
        m_ShaderEffect = NULL;
    }
    /*vector<TreeNode<class GameObject*>*> treenodes = ((UIHierarkhy*)g_UIManager->GetUI(UI_TYPE::HIERARKHY))->GetAllNodes();
    for (int i = treenodes.size() - 1; i > 0; i--)
    {
        treenodes[i]->GetData()->ReleaseTexture();
    }*/
}

void Engine::GetCurClientRect(RECT& rt)
{
    //GetClientRect(m_hWnd, &rt);
    GetWindowRect(m_hWnd, &rt);
}


void Engine::RenderUI()
{
    // Start the Dear ImGui frame
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();


    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        //ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        static float f = 0.0f;
        static int counter = 0;
        bool temp = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &temp);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &temp);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

#if _DF_GIZMO_SEQUENCER_
    //Gezmo Sequencer
        static int selectedEntry = -1;
        static int firstFrame = 0;
        static bool expanded = true;
        static int currentFrame = 100;
        Sequencer(&mySequence, &currentFrame, &expanded, &selectedEntry, &firstFrame, ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_COPYPASTE | ImSequencer::SEQUENCER_CHANGE_FRAME);
        //Gezmo Sequencer
#endif
    //ImGui::ShowBezierDemo();

        ImGui::End();

#if _DF_SEQUENTITY_
        ImGui::Begin("Sequencer");
        Sequentity::EventEditor(registry);
        ImGui::End();
#endif

        ////내가 만든거
        //if (ImGui::BeginNeoSequencer("Sequencer", &currentFrame, &startFrame, &endFrame, { 0, 0 },
        //    ImGuiNeoSequencerFlags_AllowLengthChanging |
        //    ImGuiNeoSequencerFlags_EnableSelection |
        //    ImGuiNeoSequencerFlags_Selection_EnableDragging |
        //    ImGuiNeoSequencerFlags_Selection_EnableDeletion))
        //{
        //    if (ImGui::BeginNeoGroup("Transform", &transformOpen))
        //    {

        //        if (ImGui::BeginNeoTimelineEx("Position"))
        //        {
        //            for (auto&& v : keys)
        //            {
        //                ImGui::NeoKeyframe(&v);
        //                // Per keyframe code here
        //            }


        //            if (doDelete)
        //            {
        //                uint32_t count = ImGui::GetNeoKeyframeSelectionSize();

        //                ImGui::FrameIndexType* toRemove = new ImGui::FrameIndexType[count];

        //                ImGui::GetNeoKeyframeSelection(toRemove);

        //                //Delete keyframes from your structure
        //            }
        //            ImGui::EndNeoTimeLine();
        //        }
        //        ImGui::EndNeoGroup();
        //    }

        //    ImGui::EndNeoSequencer();
        //}


    }

    // Rendering
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    // Update and Render additional Platform Windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

}


LPDIRECT3DTEXTURE9 Engine::CreateTextureFromDDS(const char* fileName, DWORD* pWidth, DWORD* pHeight)
{
    LPDIRECT3DTEXTURE9 pTexture = NULL;

    DWORD s_maxTextureWidth = m_Caps.MaxTextureWidth;
    DWORD s_maxTextureHeight = m_Caps.MaxTextureHeight;
    FILE* fp = fopen(fileName, "rb");

    if (fp == NULL)
    {
        //errorMsg << fileName << " Not Found! ";
        //Assert(0);
        return NULL;
    }

    BYTE DDSHeader[0x20];
    fread(&DDSHeader, sizeof(DDSHeader), 1, fp);
    DWORD dwFOURCC;
    memcpy(&dwFOURCC, DDSHeader, sizeof(DWORD));
    if (dwFOURCC != MAKEFOURCC('D', 'D', 'S', ' '))
    {
        //errorMsg << fileName << " DDS INVALID FORMAT!! ";
    }

    DWORD Width, Height, numSubLevel;
    memcpy(&Height, DDSHeader + 12, sizeof(DWORD));
    memcpy(&Width, DDSHeader + 16, sizeof(DWORD));

    DWORD scaledWidth = min(Width, s_maxTextureWidth);
    DWORD scaledHeight = min(Height, s_maxTextureHeight);
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    BYTE* buffer = new BYTE[fsize];

    if (buffer == NULL)
    {
        fclose(fp);
        return NULL;
    }

    fread(buffer, 1, fsize, fp);
    D3DXIMAGE_INFO ImgInfo;
    D3DXCreateTextureFromFileInMemoryEx(m_pd3dDevice, buffer, fsize, scaledWidth, scaledHeight, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0, &ImgInfo, NULL, &pTexture);

    delete[] buffer;
    buffer = NULL;

    if (pWidth)*pWidth = ImgInfo.Width;
    if (pHeight)*pHeight = ImgInfo.Height;
    fclose(fp);

    return pTexture;
}


LPDIRECT3DTEXTURE9 Engine::CreateTextureFromPNG(char* fileName, DWORD* pWidth, DWORD* pHeight)
{
    LPDIRECT3DTEXTURE9 pTexture = NULL;
    static DWORD s_maxTextureWidth = m_Caps.MaxTextureWidth;
    static DWORD s_maxTextureHeight = m_Caps.MaxTextureHeight;

    FILE* fp = fopen(fileName, "rb");
    if (fp == NULL)
    {
        //errorMsg << fileName << " Not Found! ";
        //Assert(0);
        return NULL;
    }

    //8byte 버리고 4byte 뜯어냄 -> HeaderSize
    int HeaderSize = 0;
    fseek(fp, 8, SEEK_SET);
    fread((void*)&HeaderSize, 4, 1, fp);

    //PNG는 정수값이 빅엔디안 값으로 들어있기 때문에 리틀엔디안으로 변환해준다.
    HeaderSize = ((unsigned int)((((HeaderSize) & 0xff000000) >> 24) | (((HeaderSize) & 0x00ff0000) >> 8) | (((HeaderSize) & 0x0000ff00) << 8) | (((HeaderSize) & 0x000000ff) << 24)));


    //17byte 위치부터 HeaderSize 만큼 가져온다
    //해당 데이터의 앞 4byte ,4byte 가 width, height 값이다.
    char PNGHeader[125] = { 0 };
    fseek(fp, 16, SEEK_SET);
    fread(&PNGHeader, HeaderSize, 1, fp);

    int Width, Height, numSubLevel;
    memcpy(&Width, PNGHeader, sizeof(int));
    Width = ((unsigned int)((((Width) & 0xff000000) >> 24) | (((Width) & 0x00ff0000) >> 8) | (((Width) & 0x0000ff00) << 8) | (((Width) & 0x000000ff) << 24)));
    memcpy(&Height, PNGHeader + 4, sizeof(int));
    Height = ((unsigned int)((((Height) & 0xff000000) >> 24) | (((Height) & 0x00ff0000) >> 8) | (((Height) & 0x0000ff00) << 8) | (((Height) & 0x000000ff) << 24)));
    DWORD scaledWidth = min(Width, s_maxTextureWidth);
    DWORD scaledHeight = min(Height, s_maxTextureHeight);

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    //(E) 08.09.05
    BYTE* bbuffer = new BYTE[fsize];
    ZeroMemory(bbuffer, sizeof(BYTE) * (fsize));
    fread(bbuffer, 1, fsize, fp);

    D3DXIMAGE_INFO ImgInfo;

    HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(m_pd3dDevice, bbuffer, fsize, scaledWidth, scaledHeight, 0, 0, D3DFMT_UNKNOWN/*D3DFMT_A8R8G8B8*/, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0, &ImgInfo, NULL, &pTexture);

    delete[] bbuffer;
    bbuffer = NULL;

    if (pWidth)*pWidth = ImgInfo.Width;
    if (pHeight)*pHeight = ImgInfo.Height;
    fclose(fp);
    return pTexture;
}
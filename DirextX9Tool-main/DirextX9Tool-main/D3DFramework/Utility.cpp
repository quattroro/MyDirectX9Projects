#include "pch.h"
#include "Utility.h"
#include "InputManager.h"

float DeltaTime;
HWND hWndMain;
LPDIRECT3D9         g_pD3D = NULL;
LPDIRECT3DDEVICE9   g_pd3dDevice = NULL;
float WindowWidth;
float WindowHeight;

Matrix WorldViewMat;



HRESULT InitD3D(HINSTANCE hInstance, int nCmdShow , float width, float height, LPDIRECT3D9& g_pD3D, LPDIRECT3DDEVICE9&   g_pd3dDevice)
{
    //UNREFERENCED_PARAMETER(hInstance);

    WNDCLASS wcex;

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(0,IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "D3DFramework";
    
    RegisterClass(&wcex);

    HWND hWnd = CreateWindow("D3DFramework", "D3DFramework", WS_OVERLAPPEDWINDOW, 100, 100, width, height, nullptr, nullptr, wcex.hInstance, nullptr);

    hWndMain = hWnd;
    WindowHeight = height;
    WindowWidth = width;

    if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
        return E_FAIL;

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    // Create the D3DDevice
    if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice)))
    {
        return E_FAIL;
    }

    // Turn on the zbuffer
    g_pd3dDevice->SetRenderState(D3DRS_LIGHTING, false);

    // Turn on ambient lighting 
    g_pd3dDevice->SetRenderState(D3DRS_AMBIENT, 0xffffffff);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    return S_OK;

}


bool EnterMsgLoop(bool(*render)(float time))
{
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    lasttime = (float)timeGetTime();
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            float curtime = (float)timeGetTime();
            DeltaTime = (curtime - lasttime)*0.001f;//이전 프레임에서 현재 프레임까지의 간격

            InputManager::instance()->Update();
            InputManager::instance()->InputUpdate();
            

            render(DeltaTime);

            lasttime = curtime;
        }
    }
    return true;
}


bool GetMeshInfoToXFile(LPDIRECT3DDEVICE9 g_pd3dDevice, LPCSTR FilePath, LPD3DXMESH& pMesh, D3DMATERIAL9*& pMeshMaterials, LPDIRECT3DTEXTURE9*& pMeshTextures, DWORD& dwNumMaterials)
{
    LPD3DXBUFFER pD3DXMtrlBuffer;

    // Load the mesh from the specified file
    if (FAILED(D3DXLoadMeshFromX(FilePath, D3DXMESH_SYSTEMMEM, g_pd3dDevice, NULL, &pD3DXMtrlBuffer, NULL, &dwNumMaterials, &pMesh)))
    {
        // If model is not in current folder, try parent folder
        if (FAILED(D3DXLoadMeshFromX(FilePath, D3DXMESH_SYSTEMMEM,
            g_pd3dDevice, NULL,
            &pD3DXMtrlBuffer, NULL, &dwNumMaterials,
            &pMesh)))
        {
            MessageBox(NULL, "Could not find tiger.x", "Meshes.exe", MB_OK);
            return E_FAIL;
        }
    }

    // We need to extract the material properties and texture names from the 
    // pD3DXMtrlBuffer
    D3DXMATERIAL* d3dxMaterials = (D3DXMATERIAL*)pD3DXMtrlBuffer->GetBufferPointer();

    pMeshMaterials = new D3DMATERIAL9[dwNumMaterials];
    if (pMeshMaterials == NULL)
        return E_OUTOFMEMORY;
    pMeshTextures = new LPDIRECT3DTEXTURE9[dwNumMaterials];
    if (pMeshTextures == NULL)
        return E_OUTOFMEMORY;

    for (DWORD j = 0; j < dwNumMaterials; j++)
    {
        // Copy the material
        pMeshMaterials[j] = d3dxMaterials[j].MatD3D;

        // Set the ambient color for the material (D3DX does not do this)
        pMeshMaterials[j].Ambient = pMeshMaterials[j].Diffuse;

        pMeshTextures[j] = NULL;
        if (d3dxMaterials[j].pTextureFilename != NULL && lstrlenA(d3dxMaterials[j].pTextureFilename) > 0)
        {
            // Create the texture
            if (FAILED(D3DXCreateTextureFromFileA(g_pd3dDevice,
                d3dxMaterials[j].pTextureFilename,
                &pMeshTextures[j])))
            {
                // If texture is not in current folder, try parent folder
                const CHAR* strPrefix = "..\\";
                CHAR strTexture[MAX_PATH];
                strcpy_s(strTexture, MAX_PATH, strPrefix);
                strcat_s(strTexture, MAX_PATH, d3dxMaterials[j].pTextureFilename);
                // If texture is not in current folder, try parent folder
                if (FAILED(D3DXCreateTextureFromFileA(g_pd3dDevice, strTexture, &pMeshTextures[j])))
                {
                    MessageBox(NULL, "Could not find texture map", "Meshes.exe", MB_OK);
                }
            }
        }
    }



    // Done with the material buffer
    pD3DXMtrlBuffer->Release();
    return true;
}
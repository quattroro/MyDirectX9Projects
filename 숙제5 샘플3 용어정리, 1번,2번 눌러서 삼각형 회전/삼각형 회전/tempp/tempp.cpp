
#pragma comment(lib,"d3dx9.lib")
#include <d3dx9.h>
#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#pragma warning( default : 4996 )

#include <Windows.h>
#include <mmsystem.h>



LPDIRECT3D9             g_pD3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL; // Our rendering device
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices


bool Button1 = false;
bool Button2 = false;
float LastX, LastY;
float CurX, CurY;
float xRotVal;
float yRotVal;


// A structure for our custom vertex type
struct CUSTOMVERTEX
{
    FLOAT x, y, z;      // The untransformed, 3D position for the vertex
    DWORD color;        // The vertex color
};

// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)



HRESULT InitD3D(HWND hWnd)
{
    // Create the D3D object.
    if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
        return E_FAIL;

    // Set up the structure used to create the D3DDevice
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

    // Create the D3DDevice
    if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp, &g_pd3dDevice)))
    {
        return E_FAIL;
    }

    // Turn off culling, so we see the front and back of the triangle
    g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);//
    //g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_FORCE_DWORD);//

    // Turn off D3D lighting, since we are providing our own vertex colors
    g_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    return S_OK;
}


HRESULT InitGeometry()
{
    // Initialize three vertices for rendering a triangle
    CUSTOMVERTEX g_Vertices[] =
    {
        { -1.0f,-1.0f, 0.0f, 0xffff0000, },
        {  1.0f,-1.0f, 0.0f, 0xff0000ff, },
        {  0.0f, 1.0f, 0.0f, 0xffffffff, },

        //{ -1.0f,-1.0f, 0.0f, 0xffff0000, },
        //{  1.0f,-1.0f, 0.0f, 0xff0000ff, },
        //{  0.0f, 1.0f, 0.0f, 0xffffffff, },

        //{ 4.0f,-1.0f, 0.0f, 0xffff0000, },
        //{  6.0f,-1.0f, 0.0f, 0xff0000ff, },
        //{  5.0f, 1.0f, 0.0f, 0xffffffff, },
    };


    // Create the vertex buffer.
    if (FAILED(g_pd3dDevice->CreateVertexBuffer(3 * sizeof(CUSTOMVERTEX), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL)))
    {
        return E_FAIL;
    }


    // Fill the vertex buffer.
    VOID* pVertices;
    if (FAILED(g_pVB->Lock(0, sizeof(g_Vertices), (void**)&pVertices, 0)))
        return E_FAIL;
    memcpy(pVertices, g_Vertices, sizeof(g_Vertices));
    g_pVB->Unlock();


    return S_OK;
}



VOID Cleanup()
{
    if (g_pVB != NULL)
        g_pVB->Release();

    if (g_pd3dDevice != NULL)
        g_pd3dDevice->Release();

    if (g_pD3D != NULL)
        g_pD3D->Release();
}


VOID SetupMatrices()
{
    // For our world matrix, we will just rotate the object about the y-axis.
    D3DXMATRIXA16 matWorld;
    UINT iTime;
    FLOAT fAngle = 0;

    if (!Button2)
    {
        iTime = timeGetTime() % 1000;
        fAngle = iTime * (2.0f * D3DX_PI) / 1000.0f;
        D3DXMatrixRotationY(&matWorld, fAngle);
    }
    else
    {
        D3DXMATRIXA16 temp;
        D3DXMatrixRotationY(&matWorld, xRotVal);
        D3DXMatrixRotationX(&temp, yRotVal);
        D3DXMatrixMultiply(&matWorld, &matWorld, &temp);
    }


    if (Button1)
    {
        //D3DXVECTOR3 Axis(0.0f, 10.0f, 0.0f);
        //D3DXMatrixRotationAxis(&matWorld,&Axis,fAngle);

        g_pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
    }

    // Set up our view matrix. A view matrix can be defined given an eye point,
    // a point to lookat, and a direction for which way is up. Here, we set the
    // eye five units back along the z-axis and up three units, look at the
    // origin, and define "up" to be in the y-direction.

    D3DXVECTOR3 vEyePt(0.0f, 3.0f, -20.0f);
    D3DXVECTOR3 vLookatPt(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 vUpVec(0.0f, 1.0f, 0.0f);
    D3DXMATRIXA16 matView;

    D3DXMatrixLookAtLH(&matView, &vEyePt/*시점*/, &vLookatPt/*카메라의 주시대상*/, &vUpVec/*위쪽*/);//카메라 설정
    g_pd3dDevice->SetTransform(D3DTS_VIEW, &matView);

    // For the projection matrix, we set up a perspective transform (which
    // transforms geometry from 3D view space to 2D viewport space, with
    // a perspective divide making objects smaller in the distance). To build
    // a perpsective transform, we need the field of view (1/4 pi is common),
    // the aspect ratio, and the near and far clipping planes (which define at
    // what distances geometry should be no longer be rendered).

    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f);//투영해주는 함수
    g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);

}

// 회전행렬을 이동시킨다음에 회전을 시킨다.
VOID SetupMatrices2()
{
    // For our world matrix, we will just rotate the object about the y-axis.
    D3DXMATRIXA16 matWorld;
    D3DXMATRIXA16 matTrans;
    D3DXMATRIXA16 matRotation;
    UINT iTime;
    FLOAT fAngle = 0;


    D3DXMatrixIdentity(&matWorld);
    D3DXMatrixTranslation(&matTrans, 5, 0, 0);

    if (!Button2)
    {
        iTime = timeGetTime() % 1000;
        fAngle = iTime * (2.0f * D3DX_PI) / 1000.0f;
        D3DXMatrixRotationZ(&matRotation, fAngle);
    }
    else
    {
        D3DXMATRIXA16 temp;
        D3DXMatrixRotationY(&matRotation, xRotVal);
        D3DXMatrixRotationX(&temp, yRotVal);
        D3DXMatrixMultiply(&matRotation, &matRotation, &temp);
    }



    if (Button1)
    {
        D3DXMatrixMultiply(&matWorld, &matWorld, &matRotation);

        D3DXMatrixMultiply(&matWorld, &matWorld, &matTrans);


        g_pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
    }


}

VOID SetupMatrices3()
{
    // For our world matrix, we will just rotate the object about the y-axis.
    D3DXMATRIXA16 matWorld;
    D3DXMATRIXA16 matTrans;
    D3DXMATRIXA16 matRotation;
    UINT iTime;
    FLOAT fAngle = 0;

    D3DXMatrixIdentity(&matWorld);
    D3DXMatrixTranslation(&matTrans, -5, 0, 0);

    if (!Button2)
    {
        iTime = timeGetTime() % 1000;
        fAngle = iTime * (2.0f * D3DX_PI) / 1000.0f;


        D3DXMatrixRotationX(&matRotation, fAngle);

    }
    else
    {
        D3DXMATRIXA16 temp;
        D3DXMatrixRotationY(&matRotation, xRotVal);
        D3DXMatrixRotationX(&temp, yRotVal);
        D3DXMatrixMultiply(&matRotation, &matRotation, &temp);
    }



    if (Button1)
    {


        D3DXMatrixMultiply(&matWorld, &matWorld, &matRotation);

        D3DXMatrixMultiply(&matWorld, &matWorld, &matTrans);
        g_pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
    }


}


VOID Render()
{
    // Clear the backbuffer to a black color
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    // Begin the scene
    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
    {
        // Setup the world, view, and projection matrices


        // Render the vertex buffer contents
        g_pd3dDevice->SetStreamSource(0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
        SetupMatrices();
        g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 1);
        SetupMatrices2();
        g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 1);
        SetupMatrices3();
        g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 1);
        // End the scene
        g_pd3dDevice->EndScene();
    }



    // Present the backbuffer contents to the display
    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}


LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        Cleanup();
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == '1')
        {
            Button1 = true;
            Button2 = false;
        }
        else if (wParam == '2')
        {
            Button2 = true;
        }
        break;

    case WM_MOUSEMOVE:
        if (Button2)
        {
            LastX = CurX;
            LastY = CurY;
            CurX = LOWORD(lParam);//x
            CurY = HIWORD(lParam);//y
            xRotVal += (CurX - LastX) * (2.0f * D3DX_PI) / 500.0f;
            yRotVal += (CurY - LastY) * (2.0f * D3DX_PI) / 100.0f;
        }
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}


INT WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, INT)
{
    UNREFERENCED_PARAMETER(hInst);

    // Register the window class
    WNDCLASSEX wc =
    {
        sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        "D3D Tutorial", NULL
    };
    RegisterClassEx(&wc);

    // Create the application's window
    HWND hWnd = CreateWindow("D3D Tutorial", "D3D Tutorial 03: Matrices",
        WS_OVERLAPPEDWINDOW, 100, 100, 1000, 700,
        NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (SUCCEEDED(InitD3D(hWnd)))
    {
        // Create the scene geometry
        if (SUCCEEDED(InitGeometry()))
        {
            // Show the window
            ShowWindow(hWnd, SW_SHOWDEFAULT);
            UpdateWindow(hWnd);

            // Enter the message loop
            MSG msg;
            ZeroMemory(&msg, sizeof(msg));
            while (msg.message != WM_QUIT)
            {
                if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                else
                    Render();
            }
        }
    }

    UnregisterClass("D3D Tutorial", wc.hInstance);
    return 0;
}




#include "Utility.h"
#include "TigerObj.h"
#include "Plane.h"
#include "Camera.h"

LPDIRECT3D9         g_pD3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9   g_pd3dDevice = NULL; // Our rendering device

LPD3DXMESH          g_pMesh = NULL; // Our mesh object in sysmem
D3DMATERIAL9* g_pMeshMaterials = NULL; // Materials for our mesh
LPDIRECT3DTEXTURE9* g_pMeshTextures = NULL; // Textures for our mesh
DWORD               g_dwNumMaterials = 0L;   // Number of mesh materials


TigerObj* tiger;
TigerObj* tiger2;
Plane* plane;
Camera* camera;

bool keybuf[256];
bool OnClicked;
bool Render(float time);

D3DXVECTOR2 ChangeRotVal;
//D3DXVECTOR2 CurRotVal;
//D3DXVECTOR2 StartRotVal;

D3DXVECTOR2 MoveMousePos;
D3DXVECTOR2 StartMousePos;

void Init()
{
    GetMeshInfoToXFile(g_pd3dDevice, L"Tiger.x", g_pMesh, g_pMeshMaterials, g_pMeshTextures, g_dwNumMaterials);
    tiger = new TigerObj(g_pd3dDevice, D3DXVECTOR3(0, 0, 0), g_dwNumMaterials, g_pMeshTextures, g_pMesh, g_pMeshMaterials);
    tiger2 = new TigerObj(g_pd3dDevice, D3DXVECTOR3(0, 0, 0), g_dwNumMaterials, g_pMeshTextures, g_pMesh, g_pMeshMaterials);
    tiger2->SetParent(tiger, D3DXVECTOR3(0, 0, 2));

    GetMeshInfoToXFile(g_pd3dDevice, L"ChessBoard.x", g_pMesh, g_pMeshMaterials, g_pMeshTextures, g_dwNumMaterials);
    plane = new Plane(g_pd3dDevice, D3DXVECTOR3(0, 0, 0), g_dwNumMaterials, g_pMeshTextures, g_pMesh, g_pMeshMaterials);

    camera = new Camera(g_pd3dDevice, D3DXVECTOR3(0.0f, 30.0f, 0.0f), D3DXVECTOR3(0, 0, 0), D3DXVECTOR3(1.0f, 0.0f, 0.0f));
}

INT WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int  nCmdShow)
{

	InitD3D(hInstance, nCmdShow, 1500, 1000, g_pD3D, g_pd3dDevice);

    Init();

	EnterMsgLoop(Render);
}

VOID SetupMatrices(int index)
{
    // Set up world matrix
    D3DXMATRIXA16 matWorld;
    D3DXMATRIXA16 scalemat;


    D3DXVECTOR3 vEyePt(0.0f, 30.0f, 0.0f);
    D3DXVECTOR3 vLookatPt(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 vUpVec(-1.0f, 0.0f, 0.0f);

    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);
    g_pd3dDevice->SetTransform(D3DTS_VIEW, &matView);


    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f);
    g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
}

void KeyInput()
{
    int x=0, z=0;

    if (keybuf['W'])
    {
        if (OnClicked)
        {
            camera->FowardMove();
        }
        else
        {
            tiger->DirectionMove(D3DXVECTOR3(1, 0, 0));
        }
        //x = 1;
        
        
    }
    else if (keybuf['A'])
    {
        //z = 1;
        
        if (OnClicked)
        {
            camera->LeftMove();
        }
        else
        {
            tiger->DirectionMove(D3DXVECTOR3(0, 0, 1));
        }
    }

    else if (keybuf['S'])
    {
        //x = -1;
        
        if (OnClicked)
        {
            camera->BackMove();
        }
        else
        {
            tiger->DirectionMove(D3DXVECTOR3(-1, 0, 0));
        }
    }

    else if (keybuf['D'])
    {
        //z = -1;
        
        if (OnClicked)
        {
            camera->RightMove();
        }
        else
        {
            tiger->DirectionMove(D3DXVECTOR3(0, 0, -1));
        }
    }
    
}


bool Render(float time)
{
    //DeltaTime = time;
    KeyInput();

    if (g_pd3dDevice != NULL)
    {
        // Clear the backbuffer and the zbuffer
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);

        // Begin the scene
        if (SUCCEEDED(g_pd3dDevice->BeginScene()))
        {
            //SetupMatrices(0);

            tiger->Render();
            tiger2->Render();
            plane->Render();
            camera->Render();

            // End the scene
            g_pd3dDevice->EndScene();
        }

        // Present the backbuffer contents to the display
        g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    }
    
	return true;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_RBUTTONDOWN:
        OnClicked = true;
        StartMousePos = D3DXVECTOR2(LOWORD(lParam), HIWORD(lParam));
        MoveMousePos = StartMousePos;
        break;

    case WM_RBUTTONUP:
        OnClicked = false;
        ChangeRotVal = D3DXVECTOR2(0.0f, 0.0f);
        StartMousePos = D3DXVECTOR2(0.0f, 0.0f);
        MoveMousePos = D3DXVECTOR2(0.0f, 0.0f);
        break;

        //마우스 10만큼의 이동을 1도로 정한다.
    case WM_MOUSEMOVE:
        if (OnClicked)
        {
            MoveMousePos = D3DXVECTOR2(LOWORD(lParam), HIWORD(lParam));

            ChangeRotVal.x = (MoveMousePos.y - StartMousePos.y) * -1.0f;

            ChangeRotVal.y = (MoveMousePos.x - StartMousePos.x) * -1.0f;

            StartMousePos = MoveMousePos;

            camera->YawRotation(ChangeRotVal.y);
            camera->PitchRotation(ChangeRotVal.x);
        }
        break;

    case WM_KEYDOWN:
        keybuf[wParam] = true;
        break;

    case WM_KEYUP:
        keybuf[wParam] = false;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
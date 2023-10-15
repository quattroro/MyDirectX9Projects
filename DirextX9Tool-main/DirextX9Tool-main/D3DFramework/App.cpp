#include "pch.h"
#include "Utility.h"
#include "TigerObj.h"
#include "Plane.h"
#include "Camera.h"
#include "InputManager.h"
#include "Transform.h"
#include "Mesh.h"
#include "MeshRenderer.h"
#include "SceneManager.h"
#include "Scene.h"
#include "Shader.h"

extern float DeltaTime;
extern HWND hWndMain;
extern LPDIRECT3D9         g_pD3D ;
extern LPDIRECT3DDEVICE9   g_pd3dDevice ;
extern Matrix WorldViewMat;

LPD3DXMESH          g_pMesh = NULL; // Our mesh object in sysmem
D3DMATERIAL9* g_pMeshMaterials = NULL; // Materials for our mesh
LPDIRECT3DTEXTURE9* g_pMeshTextures = NULL; // Textures for our mesh
DWORD               g_dwNumMaterials = 0L;   // Number of mesh materials


//TigerObj* tiger;
//TigerObj* tiger2;
//Plane* plane;
//Camera* camera;

bool keybuf[256];
bool OnClicked;
bool Render(float time);

D3DXVECTOR2 ChangeRotVal;
//D3DXVECTOR2 CurRotVal;
//D3DXVECTOR2 StartRotVal;

D3DXVECTOR2 MoveMousePos;
D3DXVECTOR2 StartMousePos;

GameObject* sphere;
SceneManager* scenemanager = new SceneManager();

Shader* shader;




Mesh* LoadCubeMesh()
{
    /*Mesh* findMesh = Get<Mesh>(L"Cube");
    if (findMesh)
        return findMesh;*/

    float w2 = 0.5f;
    float h2 = 0.5f;
    float d2 = 0.5f;

    vector<Vertex> vec(24);

    // 앞면
    vec[0] = Vertex(Vector3(-w2, -h2, -d2), Vector2(0.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f), Vector3(1.0f, 0.0f, 0.0f));
    vec[1] = Vertex(Vector3(-w2, +h2, -d2), Vector2(0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f), Vector3(1.0f, 0.0f, 0.0f));
    vec[2] = Vertex(Vector3(+w2, +h2, -d2), Vector2(1.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f), Vector3(1.0f, 0.0f, 0.0f));
    vec[3] = Vertex(Vector3(+w2, -h2, -d2), Vector2(1.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f), Vector3(1.0f, 0.0f, 0.0f));
    // 뒷면
    vec[4] = Vertex(Vector3(-w2, -h2, +d2), Vector2(1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f));
    vec[5] = Vertex(Vector3(+w2, -h2, +d2), Vector2(0.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f));
    vec[6] = Vertex(Vector3(+w2, +h2, +d2), Vector2(0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f));
    vec[7] = Vertex(Vector3(-w2, +h2, +d2), Vector2(1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f));
    // 윗면
    vec[8] = Vertex(Vector3(-w2, +h2, -d2), Vector2(0.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f));
    vec[9] = Vertex(Vector3(-w2, +h2, +d2), Vector2(0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f));
    vec[10] = Vertex(Vector3(+w2, +h2, +d2), Vector2(1.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f));
    vec[11] = Vertex(Vector3(+w2, +h2, -d2), Vector2(1.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f));
    // 아랫면
    vec[12] = Vertex(Vector3(-w2, -h2, -d2), Vector2(1.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f));
    vec[13] = Vertex(Vector3(+w2, -h2, -d2), Vector2(0.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f));
    vec[14] = Vertex(Vector3(+w2, -h2, +d2), Vector2(0.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f));
    vec[15] = Vertex(Vector3(-w2, -h2, +d2), Vector2(1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f));
    // 왼쪽면
    vec[16] = Vertex(Vector3(-w2, -h2, +d2), Vector2(0.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f));
    vec[17] = Vertex(Vector3(-w2, +h2, +d2), Vector2(0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f));
    vec[18] = Vertex(Vector3(-w2, +h2, -d2), Vector2(1.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f));
    vec[19] = Vertex(Vector3(-w2, -h2, -d2), Vector2(1.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f));
    // 오른쪽면
    vec[20] = Vertex(Vector3(+w2, -h2, -d2), Vector2(0.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));
    vec[21] = Vertex(Vector3(+w2, +h2, -d2), Vector2(0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));
    vec[22] = Vertex(Vector3(+w2, +h2, +d2), Vector2(1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));
    vec[23] = Vertex(Vector3(+w2, -h2, +d2), Vector2(1.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));

    vector<WORD> idx(36);

    // 앞면
    idx[0] = 0; idx[1] = 1; idx[2] = 2;
    idx[3] = 0; idx[4] = 2; idx[5] = 3;
    // 뒷면
    idx[6] = 4; idx[7] = 5; idx[8] = 6;
    idx[9] = 4; idx[10] = 6; idx[11] = 7;
    // 윗면
    idx[12] = 8; idx[13] = 9; idx[14] = 10;
    idx[15] = 8; idx[16] = 10; idx[17] = 11;
    // 아랫면
    idx[18] = 12; idx[19] = 13; idx[20] = 14;
    idx[21] = 12; idx[22] = 14; idx[23] = 15;
    // 왼쪽면
    idx[24] = 16; idx[25] = 17; idx[26] = 18;
    idx[27] = 16; idx[28] = 18; idx[29] = 19;
    // 오른쪽면
    idx[30] = 20; idx[31] = 21; idx[32] = 22;
    idx[33] = 20; idx[34] = 22; idx[35] = 23;

    //shared_ptr<Mesh> mesh = make_shared<Mesh>();
    Mesh* mesh = new Mesh();
    mesh->Init(vec, idx);
    //Add(L"Cube", mesh);

    return mesh;
}

Mesh* LoadSphereMesh()
{
    /*shared_ptr<Mesh> findMesh = Get<Mesh>(L"Sphere");
    if (findMesh)
        return findMesh;*/

    float radius = 0.5f; // 구의 반지름
    int stackCount = 20; // 가로 분할
    int sliceCount = 20; // 세로 분할

    vector<Vertex> vec;

    Vertex v;

    // 북극
    v.pos = Vector3(0.0f, radius, 0.0f);
    v.uv = Vector2(0.5f, 0.0f);
    v.normal = v.pos;
    D3DXVec3Normalize(&v.normal, &v.normal);
    //v.normal.Normalize();
    v.tangent = Vector3(1.0f, 0.0f, 1.0f);
    vec.push_back(v);

    float stackAngle = /*XM_PI*/3.141592f / stackCount;
    float sliceAngle = /*XM_2PI*/6.28318f / sliceCount;

    float deltaU = 1.f / static_cast<float>(sliceCount);
    float deltaV = 1.f / static_cast<float>(stackCount);

    // 고리마다 돌면서 정점을 계산한다 (북극/남극 단일점은 고리가 X)
    for (int y = 1; y <= stackCount - 1; ++y)
    {
        float phi = y * stackAngle;

        // 고리에 위치한 정점
        for (int x = 0; x <= sliceCount; ++x)
        {
            float theta = x * sliceAngle;

            v.pos.x = radius * sinf(phi) * cosf(theta);
            v.pos.y = radius * cosf(phi);
            v.pos.z = radius * sinf(phi) * sinf(theta);

            v.uv = Vector2(deltaU * x, deltaV * y);

            v.normal = v.pos;
            D3DXVec3Normalize(&v.normal, &v.normal);
            //v.normal.Normalize();

            v.tangent.x = -radius * sinf(phi) * sinf(theta);
            v.tangent.y = 0.0f;
            v.tangent.z = radius * sinf(phi) * cosf(theta);

            D3DXVec3Normalize(&v.tangent, &v.tangent);
            //v.tangent.Normalize();

            vec.push_back(v);
        }
    }

    // 남극
    v.pos = Vector3(0.0f, -radius, 0.0f);
    v.uv = Vector2(0.5f, 1.0f);
    v.normal = v.pos;
    D3DXVec3Normalize(&v.normal, &v.normal);
    //v.normal.Normalize();
    v.tangent = Vector3(1.0f, 0.0f, 0.0f);
    vec.push_back(v);

    vector<WORD> idx(36);

    // 북극 인덱스
    for (int i = 0; i <= sliceCount; ++i)
    {
        //  [0]
        //   |  \
		//  [i+1]-[i+2]
        idx.push_back(0);
        idx.push_back(i + 2);
        idx.push_back(i + 1);
    }

    // 몸통 인덱스
    int ringVertexCount = sliceCount + 1;
    for (int y = 0; y < stackCount - 2; ++y)
    {
        for (int x = 0; x < sliceCount; ++x)
        {
            //  [y, x]-[y, x+1]
            //  |		/
            //  [y+1, x]
            idx.push_back(1 + (y)*ringVertexCount + (x));
            idx.push_back(1 + (y)*ringVertexCount + (x + 1));
            idx.push_back(1 + (y + 1) * ringVertexCount + (x));
            //		 [y, x+1]
            //		 /	  |
            //  [y+1, x]-[y+1, x+1]
            idx.push_back(1 + (y + 1) * ringVertexCount + (x));
            idx.push_back(1 + (y)*ringVertexCount + (x + 1));
            idx.push_back(1 + (y + 1) * ringVertexCount + (x + 1));
        }
    }

    // 남극 인덱스
    int bottomIndex = static_cast<int>(vec.size()) - 1;
    int lastRingStartIndex = bottomIndex - ringVertexCount;
    for (int i = 0; i < sliceCount; ++i)
    {
        //  [last+i]-[last+i+1]
        //  |      /
        //  [bottom]
        idx.push_back(bottomIndex);
        idx.push_back(lastRingStartIndex + i);
        idx.push_back(lastRingStartIndex + i + 1);
    }
    Mesh* mesh = new Mesh();
    //shared_ptr<Mesh> mesh = make_shared<Mesh>();
    mesh->Init(vec, idx);
    //Add(L"Sphere", mesh);

    return mesh;
}

void Init()
{
    //GetMeshInfoToXFile(g_pd3dDevice, L"Tiger.x", g_pMesh, g_pMeshMaterials, g_pMeshTextures, g_dwNumMaterials);
    //tiger = new TigerObj(g_pd3dDevice, D3DXVECTOR3(0, 0, 0), g_dwNumMaterials, g_pMeshTextures, g_pMesh, g_pMeshMaterials);
    //tiger2 = new TigerObj(g_pd3dDevice, D3DXVECTOR3(0, 0, 0), g_dwNumMaterials, g_pMeshTextures, g_pMesh, g_pMeshMaterials);
    ////tiger2->SetParent(tiger, D3DXVECTOR3(0, 0, 2));

    //GetMeshInfoToXFile(g_pd3dDevice, L"ChessBoard.x", g_pMesh, g_pMeshMaterials, g_pMeshTextures, g_dwNumMaterials);
    //plane = new Plane(g_pd3dDevice, D3DXVECTOR3(0, 0, 0), g_dwNumMaterials, g_pMeshTextures, g_pMesh, g_pMeshMaterials);
    //camera = new Camera(g_pd3dDevice, D3DXVECTOR3(0.0f, 30.0f, 0.0f), D3DXVECTOR3(0, 0, 0), D3DXVECTOR3(1.0f, 0.0f, 0.0f));
   
    Scene* scene = new Scene();

    sphere = new GameObject();
    sphere->AddComponent(new Transform());
    MeshRenderer* meshrenderer = new MeshRenderer();
    Mesh* mesh = LoadCubeMesh();
    meshrenderer->setMesh(mesh);
    sphere->AddComponent(meshrenderer);

    scene->AddGameObject(sphere);
    scenemanager->TempAddScene(scene);

    shader = new Shader();
    shader->Init(L"../Resources/Shader/Test.hlsli");

    //obj1->AddComponent()
}


//void KeyInput()
//{
//    int x=0, z=0;
//
//    if (InputManager::instance()->KeyIsPressed(KEY_TYPE::W))
//    {
//        if (OnClicked)
//        {
//            camera->FowardMove();
//        }
//        else
//        {
//            tiger->DirectionMove(D3DXVECTOR3(1, 0, 0));
//        }
//    }
//    else if (InputManager::instance()->KeyIsPressed(KEY_TYPE::A))
//    {
//        //z = 1;
//        
//        if (OnClicked)
//        {
//            camera->LeftMove();
//        }
//        else
//        {
//            tiger->DirectionMove(D3DXVECTOR3(0, 0, 1));
//        }
//    }
//
//    else if (InputManager::instance()->KeyIsPressed(KEY_TYPE::S))
//    {
//        //x = -1;
//        
//        if (OnClicked)
//        {
//            camera->BackMove();
//        }
//        else
//        {
//            tiger->DirectionMove(D3DXVECTOR3(-1, 0, 0));
//        }
//    }
//
//    else if (InputManager::instance()->KeyIsPressed(KEY_TYPE::D))
//    {
//        //z = -1;
//        
//        if (OnClicked)
//        {
//            camera->RightMove();
//        }
//        else
//        {
//            tiger->DirectionMove(D3DXVECTOR3(0, 0, -1));
//        }
//    }
//    
//}

VOID SetupMatrices(int index)
{
    // Set up world matrix
    D3DXMATRIXA16 matWorld;
    D3DXMATRIXA16 scalemat;


    D3DXVECTOR3 vEyePt(5.0f, 5.0f, 0.0f);
    D3DXVECTOR3 vLookatPt(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 vUpVec(-1.0f, 0.0f, 0.0f);

    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);
    g_pd3dDevice->SetTransform(D3DTS_VIEW, &matView);
    //d3dxmatrix

    //D3DXMatrixTranspose()
    //D3DXMatrixTranslation(&matView, vEyePt.x, vEyePt.y, vEyePt.z);
    //D3DXMatrixInverse(&matView, NULL, &matView);

    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f);
    g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);

    WorldViewMat = matView * matProj;
}

bool Render(float time)
{
    //DeltaTime = time;
    //KeyInput();

    if (g_pd3dDevice != NULL)
    {
        // Clear the backbuffer and the zbuffer
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

        // Begin the scene
        if (SUCCEEDED(g_pd3dDevice->BeginScene()))
        {
            SetupMatrices(0);

            ////tiger->Translation(InputManager::instance()->GetVertivalAxis()*DeltaTime * 10.f, 0, 0);
            ////tiger->Translation(0, 0, -InputManager::instance()->GetHorizentalAxis() * DeltaTime*10.f);

            ///*tiger->Render();
            //tiger2->Render();
            //plane->Render();
            //camera->Render();*/
            ////dynamic_cast<MeshRenderer*>(sphere->GetComponent(COMPONENT_TYPE::MESH_RENDERER))->Render();
            ////Scene->



            shader->pTbv->SetMatrix(g_pd3dDevice, shader->g_Val_Handle, &WorldViewMat);
            scenemanager->Render(shader);

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
        InputManager::instance()->MouseButtonDown(1);
        OnClicked = true;
        StartMousePos = D3DXVECTOR2(LOWORD(lParam), HIWORD(lParam));
        MoveMousePos = StartMousePos;
        break;

    case WM_RBUTTONUP:
        InputManager::instance()->MouseButtonUp(1);
        OnClicked = false;
        ChangeRotVal = D3DXVECTOR2(0.0f, 0.0f);
        StartMousePos = D3DXVECTOR2(0.0f, 0.0f);
        MoveMousePos = D3DXVECTOR2(0.0f, 0.0f);
        break;

    case WM_LBUTTONDOWN:
        InputManager::instance()->MouseButtonDown(0);
        break;

    case WM_LBUTTONUP:
        InputManager::instance()->MouseButtonUp(0);
        break;

        //마우스 10만큼의 이동을 1도로 정한다.
    case WM_MOUSEMOVE:
        InputManager::instance()->SetMousePosition(D3DXVECTOR2(LOWORD(lParam), HIWORD(lParam)));

        if (OnClicked)
        {
            MoveMousePos = D3DXVECTOR2(LOWORD(lParam), HIWORD(lParam));

            ChangeRotVal.x = (MoveMousePos.y - StartMousePos.y) * -1.0f;

            ChangeRotVal.y = (MoveMousePos.x - StartMousePos.x) * -1.0f;

            StartMousePos = MoveMousePos;

            //camera->YawRotation(ChangeRotVal.y);
            //camera->PitchRotation(ChangeRotVal.x);
        }
        break;

    case WM_KEYDOWN:
        //InputManager::instance()->KeyDown(wParam);
        //keybuf[wParam] = true;
        break;

    case WM_KEYUP:
        //InputManager::instance()->KeyUp(wParam);
        //keybuf[wParam] = false;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}


LPDIRECT3DDEVICE9 GetDevice()
{
    return g_pd3dDevice;
}

INT WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int  nCmdShow)
{

    InitD3D(hInstance, nCmdShow, 1500, 1000, g_pD3D, g_pd3dDevice);

    Init();

    EnterMsgLoop(Render);
}


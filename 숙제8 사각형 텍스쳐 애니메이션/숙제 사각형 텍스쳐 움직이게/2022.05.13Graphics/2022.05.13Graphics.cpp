#pragma comment(lib, "d3d9.lib")
#pragma comment(lib,"d3dx9.lib")
#pragma comment(lib, "winmm.lib")
#include <Windows.h>
#include <mmsystem.h>
#include <d3dx9.h>
#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#include <iostream>
#include <algorithm>
#include <vector>

#pragma warning( default : 4996 )

#include "framework.h"



//#define SHOW_HOW_TO_USE_TCI

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D9             g_pD3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL; // Our rendering device
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices
LPDIRECT3DTEXTURE9      g_pTexture = NULL; // Our texture
LPDIRECT3DTEXTURE9      g_pTexture2 = NULL; // Our texture
LPDIRECT3DTEXTURE9      g_FontTexture = NULL; // Our texture
LPD3DXMESH m_pCubeMesh;

LPDIRECT3DINDEXBUFFER9 g_pIB = NULL;


// A structure for our custom vertex type. We added texture coordinates
struct CUSTOMVERTEX
{
    D3DXVECTOR3 position; // The position
    D3DCOLOR color;    // The color
#ifndef SHOW_HOW_TO_USE_TCI
    FLOAT tu, tv;   // The texture coordinates
#endif
};

// Our custom FVF, which describes our custom vertex structure
#ifdef SHOW_HOW_TO_USE_TCI
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)
#else
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)
#endif



struct _Triangle
{
    LPDIRECT3DTEXTURE9 texture;// 텍스처정보
    LPDIRECT3DVERTEXBUFFER9 pVB;//정점버퍼
    CUSTOMVERTEX Vertices[3];//정점 정보
    CUSTOMVERTEX FirstVertices[3];


    D3DXVECTOR3 pos;
    D3DXVECTOR2 size;
    bool isactive;

};


struct _Rectangle
{
    LPDIRECT3DTEXTURE9 texture;
    LPDIRECT3DVERTEXBUFFER9 pVB;
    LPDIRECT3DINDEXBUFFER9 pIB;
    CUSTOMVERTEX Vertices[4];
    CUSTOMVERTEX FirstVertices[4];
    WORD Indices[6];

    D3DXVECTOR3 pos;
    D3DXVECTOR2 size;

    bool isactive;
};

_Triangle* triangles[100];
int tricount = 0;
_Rectangle* rectangles[100];
int recvcount = 0;


_Triangle* CreateTriangle(CUSTOMVERTEX* vertices, LPDIRECT3DTEXTURE9 texture,bool isAnimation)
{
    triangles[tricount] = new _Triangle;
    ZeroMemory(triangles[tricount], sizeof(_Triangle));

    g_pd3dDevice->CreateVertexBuffer(3 * sizeof(CUSTOMVERTEX), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &triangles[tricount]->pVB, NULL);

    //triangles[tricount]->Vertices = vertices;
    memcpy(triangles[tricount]->Vertices, vertices, 3 * sizeof(CUSTOMVERTEX));
    memcpy(triangles[tricount]->FirstVertices, vertices, 3 * sizeof(CUSTOMVERTEX));

    CUSTOMVERTEX* pVertices;
    triangles[tricount]->pVB->Lock(0, 0, (void**)&pVertices, 0);

    memcpy(pVertices, triangles[tricount]->Vertices, 3 * sizeof(CUSTOMVERTEX));

    triangles[tricount]->pVB->Unlock();

    triangles[tricount]->texture = texture;
    triangles[tricount]->isactive = isAnimation;

    tricount++;
    return triangles[tricount - 1];
}


_Rectangle* CreateRectangle(CUSTOMVERTEX* vertices, WORD* Indices, LPDIRECT3DTEXTURE9 texture, bool isAnimation)
{
    rectangles[recvcount] = new _Rectangle;
    ZeroMemory(rectangles[recvcount], sizeof(_Rectangle));
    g_pd3dDevice->CreateVertexBuffer(4 * sizeof(CUSTOMVERTEX), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &rectangles[recvcount]->pVB, NULL);

    memcpy(rectangles[recvcount]->Vertices, vertices, 4 * sizeof(CUSTOMVERTEX));
    memcpy(rectangles[recvcount]->FirstVertices, vertices, 4 * sizeof(CUSTOMVERTEX));
    

    CUSTOMVERTEX* pVertices;
    rectangles[recvcount]->pVB->Lock(0, 4 * sizeof(CUSTOMVERTEX), (void**)&pVertices, 0);

    memcpy(pVertices, rectangles[recvcount]->Vertices, 4 * sizeof(CUSTOMVERTEX));

    rectangles[recvcount]->pVB->Unlock();


    g_pd3dDevice->CreateIndexBuffer(sizeof(WORD)*6, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &rectangles[recvcount]->pIB, NULL);

    memcpy(rectangles[recvcount]->Indices, Indices, sizeof(WORD) * 6);

    VOID* pIndices;
    rectangles[recvcount]->pIB->Lock(0, sizeof(WORD) * 6, (void**)&pIndices, 0);
    memcpy(pIndices, rectangles[recvcount]->Indices, sizeof(WORD) * 6);
    rectangles[recvcount]->pIB->Unlock();

    rectangles[recvcount]->texture = texture;
    rectangles[recvcount]->isactive = isAnimation;

    recvcount++;

    return rectangles[recvcount - 1];
}


void SetAnimation(_Triangle* obj)
{
    for (int i = 0; i < 3; i++)
    {
        obj->Vertices[i].tv += 0.01;
    }
    if (obj->Vertices[0].tv >= 1 || obj->Vertices[2].tv >= 1)
    {

        memcpy(obj->Vertices, obj->FirstVertices,3 * sizeof(CUSTOMVERTEX));
        
    }

    CUSTOMVERTEX* pVertices;
    
    obj->pVB->Lock(0, 0, (void**)&pVertices, 0);
    //int size = sizeof(obj->Vertices);
    memcpy(pVertices, obj->Vertices, sizeof(CUSTOMVERTEX)*3);

    obj->pVB->Unlock();
}

void SetAnimation(_Rectangle* obj)
{
    for (int i = 0; i < 4; i++)
    {
        obj->Vertices[i].tv += 0.01;
    }
    if (obj->Vertices[0].tv >= 1 || obj->Vertices[2].tv >= 1)
    {
        memcpy(obj->Vertices, obj->FirstVertices, 4 * sizeof(CUSTOMVERTEX));
        
    }

    CUSTOMVERTEX* pVertices;
    obj->pVB->Lock(0, 0, (void**)&pVertices, 0);

    memcpy(pVertices, obj->Vertices, sizeof(CUSTOMVERTEX) * 4);

    obj->pVB->Unlock();
}


void TriangleRender(_Triangle* obj)
{
    /*if (obj->isactive)
    {
        SetAnimation(obj);
    }*/

    g_pd3dDevice->SetTexture(0, obj->texture);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    g_pd3dDevice->SetStreamSource(0, obj->pVB, 0, sizeof(CUSTOMVERTEX));
    g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);

    g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 1);


}


void RectangleRender(_Rectangle* obj)
{
    /*if (obj->isactive)
    {
        SetAnimation(obj);
    }*/

    g_pd3dDevice->SetTexture(0, obj->texture);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    g_pd3dDevice->SetStreamSource(0, obj->pVB, 0, sizeof(CUSTOMVERTEX));
    g_pd3dDevice->SetIndices(obj->pIB);
    g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
   
    g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);

}

void SetTexTure(LPDIRECT3DTEXTURE9 texture)
{
    for (int i = 0; i < tricount; i++)
    {
        triangles[i]->texture = texture;
    }
}


HRESULT InitD3D(HWND hWnd)
{
    // Create the D3D object.
    if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
        return E_FAIL;

    // Set up the structure used to create the D3DDevice. Since we are now
    // using more complex geometry, we will create a device with a zbuffer.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    // Create the D3DDevice
    if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp, &g_pd3dDevice)))
    {
        return E_FAIL;
    }

    // Turn off culling
    g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    // Turn off D3D lighting
    g_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    // Turn on the zbuffer
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);


    D3DXCreateBox(g_pd3dDevice, 1, 1, 1, &m_pCubeMesh, NULL);

    return S_OK;
}

void NormalizeSDF(float* sdf, int size) 
{
    float minVal = *std::min_element(sdf, sdf + size);
    float maxVal = *std::max_element(sdf, sdf + size);
    for (int i = 0; i < size; ++i) {
        sdf[i] = 1.0 - (sdf[i] - minVal) / (maxVal - minVal);
    }
}

void QuantizeSDF(const float* sdf, int size, unsigned char* out) {
    for (int i = 0; i < size; ++i) {
        out[i] = static_cast<unsigned char>(round(sdf[i] * 255.0f));
    }
}

//float euclidean_distance(int x1, int y1, int x2, int y2) {
//    return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
//}
//
//void GenerateSDF(const unsigned char* input, int width, int height, float* sdf) {
//    for (int y = 0; y < height; ++y) {
//        for (int x = 0; x < width; ++x) {
//            bool inside = input[y * width + x] > 127;
//            float minDist = width + height;
//            for (int yy = 0; yy < height; ++yy) {
//                for (int xx = 0; xx < width; ++xx) {
//                    if ((input[yy * width + xx] > 127) != inside) {
//                        float dist = euclidean_distance(x, y, xx, yy);
//                        if (dist < minDist) minDist = dist;
//                    }
//                }
//            }
//            sdf[y * width + x] = inside ? -minDist : minDist;
//        }
//    }
//}


// 유클리드 거리 계산
inline float dist(int x0, int y0, int x1, int y1) 
{
    return std::sqrt((x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1));
}

// 외곽선 픽셀 판별
bool isEdge(const unsigned char* bmp, int x, int y, int w, int h) 
{
    int idx = y * w + x;
    if (bmp[idx] < 128) return false;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) 
        {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            if (bmp[ny * w + nx] < 128) return true;
        }
    return false;
}

// SDF 생성 (브루트포스 방식, 정확도 우선)
void GenerateSDF(const unsigned char* bmp, int w, int h, float* sdf) 
{
    std::vector<std::pair<int, int>> edges;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (isEdge(bmp, x, y, w, h))
                edges.emplace_back(x, y);

    for (int y = 0; y < h; ++y) 
    {
        for (int x = 0; x < w; ++x) 
        {
            bool inside = bmp[y * w + x] > 128;
            float minDist = (std::numeric_limits<float>::max)();
            for (auto& e : edges) 
            {
                float d = dist(x, y, e.first, e.second);
                if (d < minDist) minDist = d;
            }
            sdf[y * w + x] = inside ? -minDist : minDist;
        }
    }
}


//gpt 알고리즘
// 간단한 2D Distance Transform: 각 픽셀에서 최근접 경계 픽셀까지 거리
void compute_distance_map(/*const std::vector<uint8_t>&*/const unsigned char* bitmap, int width, int height, std::vector<float>& distMap, bool inside) {
    const float INF = 1e10f;
    distMap.resize(width * height, INF);

    // 경계 픽셀 찾기
    auto isInside = [&](int x, int y) {
        return bitmap[y * width + x] > 127;
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool curr = isInside(x, y);
            if (curr != inside) continue;

            // 현재 픽셀에서 경계인지 검사
            bool boundary = false;
            for (int dy = -1; dy <= 1 && !boundary; ++dy) {
                for (int dx = -1; dx <= 1 && !boundary; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                    if (isInside(nx, ny) != curr)
                        boundary = true;
                }
            }

            if (!boundary) continue;

            // 경계면 거리 0
            distMap[y * width + x] = 0.0f;
        }
    }

    // Distance propagation
    // forward pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float d = distMap[y * width + x];
            if (d == 0) continue;
            for (int dy = -1; dy <= 0; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                    float nd = distMap[ny * width + nx] + std::hypot(dx, dy);
                    if (nd < d) d = nd;
                }
            }
            distMap[y * width + x] = d;
        }
    }
    // backward pass
    for (int y = height - 1; y >= 0; --y) {
        for (int x = width - 1; x >= 0; --x) {
            float d = distMap[y * width + x];
            if (d == 0) continue;
            for (int dy = 0; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                    float nd = distMap[ny * width + nx] + std::hypot(dx, dy);
                    if (nd < d) d = nd;
                }
            }
            distMap[y * width + x] = d;
        }
    }
}

// SDF 생성
void generate_sdf(/*const std::vector<uint8_t>&*/const unsigned char* bitmap, int width, int height, std::vector<uint8_t>& sdf, float spread) {
    std::vector<float> distIn, distOut;
    compute_distance_map(bitmap, width, height, distIn, true);
    compute_distance_map(bitmap, width, height, distOut, false);

    sdf.resize(width * height);
    for (int i = 0; i < width * height; ++i) {
        float d = distOut[i] - distIn[i];
        float v = 128 + (d / spread) * 128;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        sdf[i] = (uint8_t)(v);
    }
}


void InitFont()
{
    FT_Library ft;
    FT_Face face;
    FT_Init_FreeType(&ft);
    FT_New_Face(ft, "C:\\Windows\\Fonts\\malgun.ttf", 0, &face);
    FT_Set_Pixel_Sizes(face, 0, 300);

    FT_Load_Char(face, 'A', FT_LOAD_RENDER);
    FT_Bitmap& bitmap = face->glyph->bitmap;
    int width = bitmap.width;
    int height = bitmap.rows;
    unsigned char* buffer = bitmap.buffer;

    std::vector<float> sdf(width * height);

    GenerateSDF(bitmap.buffer, width, height, sdf.data());
    NormalizeSDF(sdf.data(), width * height);

    std::vector<unsigned char> sdf8(width * height);
    QuantizeSDF(sdf.data(), width * height, sdf8.data());

    
    IDirect3DTexture9* texture = nullptr;

    /*std::vector<unsigned char> sdf8(width * height);
    generate_sdf(bitmap.buffer, width, height, sdf8, 30);*/
    // 8비트 그레이스케일 SDF를 A8R8G8B8로 변환하여 저장
    HRESULT hr = g_pd3dDevice->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_FontTexture, nullptr);
    if (FAILED(hr)) return;

    D3DLOCKED_RECT lockedRect;
    if (SUCCEEDED(g_FontTexture->LockRect(0, &lockedRect, nullptr, 0)))
    {
        unsigned char* dst = (unsigned char*)lockedRect.pBits;
        int dstPitch = lockedRect.Pitch;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                unsigned char v = sdf8[y * width + x]; // 0~255
                // A8R8G8B8 포맷: BGRA 순서로 저장
                dst[x * 4 + 0] = v; // B
                dst[x * 4 + 1] = v; // G
                dst[x * 4 + 2] = v; // R
                dst[x * 4 + 3] = 255; // A
            }
            dst += dstPitch;
        }
        g_FontTexture->UnlockRect(0);
    }


}



HRESULT InitGeometry()
{
    if (FAILED(D3DXCreateTextureFromFile(g_pd3dDevice, L"Water.bmp", &g_pTexture)))
    {
        if (FAILED(D3DXCreateTextureFromFile(g_pd3dDevice, L"..\\Water.bmp", &g_pTexture)))
        {
            MessageBox(NULL, L"Could not find banana.bmp", L"Textures.exe", MB_OK);
            return E_FAIL;
        }
    }


    if (FAILED(D3DXCreateTextureFromFile(g_pd3dDevice, L"Fire.bmp", &g_pTexture2)))
    {
        if (FAILED(D3DXCreateTextureFromFile(g_pd3dDevice, L"..\\Fire.bmp", &g_pTexture2)))
        {
            MessageBox(NULL, L"Could not find skull.bmp", L"Textures.exe", MB_OK);
            return E_FAIL;
        }
    }

    CUSTOMVERTEX g_Vertices[] = 
    {
        { D3DXVECTOR3(-2.5f , -1.0f,  0.0f) ,  D3DCOLOR_RGBA(255, 255, 255, 255),0,1 },
        { D3DXVECTOR3(-2.5f , 1.0f,  0.0f)  , D3DCOLOR_RGBA(255, 255, 255, 255), 0,0},
        { D3DXVECTOR3(-0.5f , 1.0f, 0.0f) , D3DCOLOR_RGBA(255,255, 255, 255),1,0 }
    };

    CUSTOMVERTEX g_Vertices2[] = 
    {
        { D3DXVECTOR3(-2.5f , -1.0f,  0.0f) ,  D3DCOLOR_RGBA(255, 255, 255, 255) ,0,1 },
        { D3DXVECTOR3(-0.5f , 1.0f,  0.0f)  , D3DCOLOR_RGBA(255, 255, 255, 255) , 1,0},
        { D3DXVECTOR3(-0.5f , -1.0f, 0.0f) , D3DCOLOR_RGBA(255, 255, 255, 255) ,1,1}
    };

    CUSTOMVERTEX g_Vertices3[] = 
    {
        { D3DXVECTOR3(0.5f , -1.0f,  0.0f) ,  D3DCOLOR_RGBA(255, 255, 255, 255),0,1 },
        { D3DXVECTOR3(0.5f , 2.0f,  0.0f)  , D3DCOLOR_RGBA(255, 255, 255, 255), 0,0},
        { D3DXVECTOR3(3.5f , 2.0f, 0.0f) , D3DCOLOR_RGBA(255,255, 255, 255),1,0},
        { D3DXVECTOR3(3.5f , -1.0f,  0.0f) ,  D3DCOLOR_RGBA(255, 255, 255, 255), 1,1 }
    };

    WORD Indices[] =
    {
            0, 1, 2,
            0, 2, 3
    };

    CreateTriangle(g_Vertices, g_pTexture, true);

    CreateTriangle(g_Vertices2, g_pTexture, true);

    CreateRectangle(g_Vertices3, Indices, g_FontTexture, true);


    return S_OK;
}


VOID Cleanup()
{
    if (g_pTexture != NULL)
        g_pTexture->Release();

    if (g_pTexture2 != NULL)
        g_pTexture2->Release();


    if (g_pVB != NULL)
        g_pVB->Release();

    if (g_pd3dDevice != NULL)
        g_pd3dDevice->Release();

    if (g_pD3D != NULL)
        g_pD3D->Release();
}



//-----------------------------------------------------------------------------
// Name: SetupMatrices()
// Desc: Sets up the world, view, and projection transform matrices.
//-----------------------------------------------------------------------------
VOID SetupMatrices()
{
   
    D3DXVECTOR3 vEyePt(0.0f, 0.0f, -10.0f);
    D3DXVECTOR3 vLookatPt(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 vUpVec(0.0f, 1.0f, 0.0f);
    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);
    g_pd3dDevice->SetTransform(D3DTS_VIEW, &matView);

    
    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f);
    g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
VOID Render()
{
    // Clear the backbuffer and the zbuffer
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);

    // Begin the scene
    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
    {
        SetupMatrices();

        for (int i = 0; i < tricount; i++)
        {
            TriangleRender(triangles[i]);
        }

        for (int i = 0; i < recvcount; i++)
        {
            RectangleRender(rectangles[i]);
        }

        // End the scene
        g_pd3dDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}




//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------
LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        SetTexTure(g_pTexture2);
        break;

    case WM_DESTROY:
        Cleanup();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, INT)
{
    UNREFERENCED_PARAMETER(hInst);

    // Register the window class
    WNDCLASSEX wc =
    {
        sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        L"D3D Tutorial", NULL
    };
    RegisterClassEx(&wc);

    // Create the application's window
    HWND hWnd = CreateWindow(L"D3D Tutorial", L"D3D Tutorial 05: Textures",
        WS_OVERLAPPEDWINDOW, 100, 100, 1200, 1000,
        NULL, NULL, wc.hInstance, NULL);
    

    // Initialize Direct3D
    if (SUCCEEDED(InitD3D(hWnd)))
    {
        InitFont();
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

    UnregisterClass(L"D3D Tutorial", wc.hInstance);
    return 0;
}




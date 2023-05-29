#pragma once
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "winmm.lib")
#include <d3dx9.h>
#include <d3d9.h>


static float lasttime;
//전역변수로 저장
extern float DeltaTime;

//Init,  render, release, delete
HRESULT InitD3D(HINSTANCE hInstance, int nCmdShow, float width, float height, LPDIRECT3D9& g_pD3D, LPDIRECT3DDEVICE9&   g_pd3dDevice);

void Release();

void Delete();

bool EnterMsgLoop(bool(*render)(float time));

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool GetMeshInfoToXFile(LPDIRECT3DDEVICE9 g_pd3dDevice, LPCWSTR FilePath, LPD3DXMESH& pMesh, D3DMATERIAL9*& pMeshMaterials, LPDIRECT3DTEXTURE9*& pMeshTextures, DWORD& dwNumMaterials);

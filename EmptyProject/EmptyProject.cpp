#include "DXUT.h"
#include "resource.h"
#include "DirectXMath.h"

LPDIRECT3DVERTEXBUFFER9 g_pVB;
LPDIRECT3DINDEXBUFFER9 g_pIB;
IDirect3DVertexDeclaration9* g_pVertexDecl = NULL;
ID3DXEffect* g_pEffect9 = NULL;

struct XMPosVertex
{
    DirectX::XMFLOAT3 p;
};

D3DXMATRIX g_matWorld;
D3DXMATRIX g_matView;
D3DXMATRIX g_matProj;

void DrawRing(IDirect3DDevice9* pd3dDevice, const DirectX::XMFLOAT3& Origin, const DirectX::XMFLOAT3& MajorAxis, const DirectX::XMFLOAT3& MinorAxis, D3DCOLOR Color)
{
    static const DWORD dwRingSegments = 32;

    DirectX::XMFLOAT3 verts[dwRingSegments + 1];

    DirectX::XMVECTOR vOrigin = XMLoadFloat3(&Origin);
    DirectX::XMVECTOR vMajor = XMLoadFloat3(&MajorAxis);
    DirectX::XMVECTOR vMinor = XMLoadFloat3(&MinorAxis);

    FLOAT fAngleDelta = DirectX::XM_2PI / (float)dwRingSegments;
    // Instead of calling cos/sin for each segment we calculate
    // the sign of the angle delta and then incrementally calculate sin
    // and cosine from then on.
    DirectX::XMVECTOR cosDelta = DirectX::XMVectorReplicate(cosf(fAngleDelta));
    DirectX::XMVECTOR sinDelta = DirectX::XMVectorReplicate(sinf(fAngleDelta));
    DirectX::XMVECTOR incrementalSin = DirectX::XMVectorZero();
    static const DirectX::XMVECTOR initialCos =
    {
        1.0f, 1.0f, 1.0f, 1.0f
    };
    DirectX::XMVECTOR incrementalCos = initialCos;
    for (DWORD i = 0; i < dwRingSegments; i++)
    {
        DirectX::XMVECTOR Pos;
        Pos = DirectX::XMVectorMultiplyAdd(vMajor, incrementalCos, vOrigin);
        Pos = DirectX::XMVectorMultiplyAdd(vMinor, incrementalSin, Pos);
        DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&verts[i], Pos);
        // Standard formula to rotate a vector.

        DirectX::XMVECTOR newCos = DirectX::XMVectorSubtract(DirectX::XMVectorMultiply(incrementalCos, cosDelta), DirectX::XMVectorMultiply(incrementalSin, sinDelta));
        DirectX::XMVECTOR newSin = DirectX::XMVectorAdd(DirectX::XMVectorMultiply(incrementalCos, sinDelta), DirectX::XMVectorMultiply(incrementalSin, cosDelta));
        incrementalCos = newCos;
        incrementalSin = newSin;
    }
    verts[dwRingSegments] = verts[0];

    // Copy to vertex buffer
    assert((dwRingSegments + 1) <= 100);

    DirectX::XMFLOAT3* pVerts = NULL;
    HRESULT hr;
    V(g_pVB->Lock(0, 0, (void**)&pVerts, D3DLOCK_DISCARD))
        memcpy(pVerts, verts, sizeof(verts));
    V(g_pVB->Unlock())

    /*D3DXCOLOR clr = Color;
    g_pEffect9->SetFloatArray(g_Color, clr, 4);
    g_pEffect9->CommitChanges();*/

    pd3dDevice->DrawPrimitive(D3DPT_LINESTRIP, 0, dwRingSegments);
}

void DrawSphere(IDirect3DDevice9* pd3dDevice, const XNA::Sphere& sphere, D3DCOLOR Color)
{
    const DirectX::XMFLOAT3 Origin = sphere.Center;
    const float fRadius = sphere.Radius;

    DrawRing(pd3dDevice, Origin, DirectX::XMFLOAT3(fRadius, 0, 0), DirectX::XMFLOAT3(0, 0, fRadius), Color);
    DrawRing(pd3dDevice, Origin, DirectX::XMFLOAT3(fRadius, 0, 0), DirectX::XMFLOAT3(0, fRadius, 0), Color);
    DrawRing(pd3dDevice, Origin, DirectX::XMFLOAT3(0, fRadius, 0), DirectX::XMFLOAT3(0, 0, fRadius), Color);
}


bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                      bool bWindowed, void* pUserContext )
{
    // Typically want to skip back buffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    return true;
}


bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;
    V_RETURN(pd3dDevice->CreateVertexBuffer(100 * sizeof(XMPosVertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
        0, D3DPOOL_DEFAULT, &g_pVB, NULL));

    V_RETURN(pd3dDevice->CreateIndexBuffer(100 * sizeof(WORD), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
        D3DPOOL_DEFAULT, &g_pIB, NULL));

    // Create drawing resources
    static const D3DVERTEXELEMENT9 elements[2] =
    {
        { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
        D3DDECL_END()
    };

    V_RETURN(pd3dDevice->CreateVertexDeclaration(elements, &g_pVertexDecl));

    //V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, L"Collision.fx"));
    //V_RETURN(D3DXCreateEffectFromFile(pd3dDevice, str, NULL, NULL, dwShaderFlags | D3DXFX_LARGEADDRESSAWARE, NULL, &g_pEffect9, NULL));

    return S_OK;
}


HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext )
{
    return S_OK;
}


void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
}


void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        UINT cPasses;
        V(g_pEffect9->Begin(&cPasses, 0));

        V(pd3dDevice->SetStreamSource(0, g_pVB, 0, sizeof(XMPosVertex)));
        V(pd3dDevice->SetVertexDeclaration(g_pVertexDecl));

        for (UINT iPass = 0; iPass < cPasses; ++iPass)
        {
            V(g_pEffect9->BeginPass(iPass));
            //RenderObjects(pd3dDevice);
            V(g_pEffect9->EndPass());
        }

        V(g_pEffect9->End());


        V( pd3dDevice->EndScene() );
    }
}


LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
    return 0;
}


void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
}


void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    SAFE_RELEASE(g_pVertexDecl);
}


INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the default hotkeys
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"EmptyProject" );
    DXUTCreateDevice( true, 640, 480 );

    DXUTMainLoop();

    return DXUTGetExitCode();
}



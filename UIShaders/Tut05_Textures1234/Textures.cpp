#include "stdafx.h"

//#include <Windows.h>
//#include <mmsystem.h>
//#include <d3dx9.h>
//#include <strsafe.h>
//#pragma warning( default : 4996 )

#include "glyph.h"
#include "metrics_parser.h"
#include "SDFGenerator.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D9             g_pD3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL; // Our rendering device
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices
LPDIRECT3DTEXTURE9      g_pTexture = NULL; // Our texture
LPDIRECT3DINDEXBUFFER9  g_pIB = NULL;
LPDIRECT3DTEXTURE9      g_SDFTexture = NULL; // Our texture

LPD3DXEFFECTCOMPILER    g_lpEffectCompiler = NULL;
LPD3DXEFFECT            g_lpEffect = NULL;
map<string, LPDIRECT3DTEXTURE9> g_FontTextureMap;

SDFGenerator* g_SDFGenerator;


struct FPOINT
{
    float x;
    float y;
};

FPOINT g_FontSize;
FPOINT testPoint = {0,0};
FPOINT testScale = {0,0};

// A structure for our custom vertex type. We added texture coordinates
struct CUSTOMVERTEX
{
    CUSTOMVERTEX()
    {
        position = D3DXVECTOR3(0, 0, 0);
        color = 0xffffffff;
        tu = 0;
        tv = 0;
    }
    CUSTOMVERTEX(D3DXVECTOR3 _pos, D3DCOLOR _color, FLOAT _tu, FLOAT _tv)
    {
        position = _pos;
        color = _color;
        tu = _tu;
        tv = _tv;
    }
    D3DXVECTOR3 position; // The position
    D3DCOLOR color;    // The color
#ifndef SHOW_HOW_TO_USE_TCI
    FLOAT tu, tv;   // The texture coordinates
#endif
};


struct SUQARE
{
    D3DXVECTOR3 m_pos;
    D3DXVECTOR3 m_scale;
    D3DXQUATERNION m_rotate;

    LPDIRECT3DVERTEXBUFFER9 m_VB;
    LPDIRECT3DINDEXBUFFER9 m_IB;

    LPDIRECT3DTEXTURE9 m_Tex;
};

//struct POINT
//{
//    float x;
//    float y;
//};

// Our custom FVF, which describes our custom vertex structure
#ifdef SHOW_HOW_TO_USE_TCI
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)
#else
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)
#endif

map< long, Glyph>  g_glyphs;
float g_margin;

//-----------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-----------------------------------------------------------------------------
HRESULT InitD3D( HWND hWnd )
{
    // Create the D3D object.
    if( NULL == ( g_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice. Since we are now
    // using more complex geometry, we will create a device with a zbuffer.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    
    
    // Create the D3DDevice
    if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                      &d3dpp, &g_pd3dDevice ) ) )
    {
        return E_FAIL;
    }

    // Turn off culling
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Turn off D3D lighting
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

    // Turn on the zbuffer
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );


    //DLPDIRECT3DTEXTURE tex;
    
        //g_testTexMap["toonSeamlessNoise"] = tex;

    return S_OK;
}


HRESULT InitTexTure()
{
    // Use D3DX to create a texture from a file based image
    if (FAILED(D3DXCreateTextureFromFile(g_pd3dDevice, L"banana.bmp", &g_pTexture)))
    {
        // If texture is not in current folder, try parent folder
        if (FAILED(D3DXCreateTextureFromFile(g_pd3dDevice, L"..\\banana.bmp", &g_pTexture)))
        {
            MessageBox(NULL, L"Could not find banana.bmp", L"Textures.exe", MB_OK);
            return E_FAIL;
        }
    }

    if (FAILED(D3DXCreateTextureFromFile(g_pd3dDevice, L"SDF_PNG.png", &g_SDFTexture)))
    {
        MessageBox(NULL, L"Could not find signed_dist_font.png", L"SDF_PNG.png", MB_OK);
        return E_FAIL;
    }
    
    SDFont::MetricsParser parser(g_glyphs, g_margin);
    parser.parseSpec("SDF_PNG.txt");

    int a = 0;
}

// Initialize three vertices for rendering a triangle
CUSTOMVERTEX g_Vertices[] =
{
    { D3DXVECTOR3(-5.0f,-5.0f, 0.0f), 0xffffffff,0,1 }, // 왼쪽 아래
    { D3DXVECTOR3(5.0f,-5.0f, 0.0f), 0xffffffff,1,1 },// 오른쪽 아래
    { D3DXVECTOR3(5.0f, 5.0f, 0.0f), 0xffffffff,1,0 },//오른쪽 위
    { D3DXVECTOR3(-5.0f, 5.0f, 0.0f), 0xffffffff,0,0 },//왼쪽 위
};


DWORD g_Indexes[] =
{
    3,2,0,
    0,2,1,
};

HRESULT CreateBuffer()
{   
    // Create the vertex buffer.
    if (FAILED(g_pd3dDevice->CreateVertexBuffer(4 * sizeof(CUSTOMVERTEX) * 2048, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL)))
    {
        return E_FAIL;
    }


    if (FAILED(g_pd3dDevice->CreateIndexBuffer(sizeof(DWORD) * 6 * 2048, 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &g_pIB, NULL)))
    {
        return E_FAIL;
    }
}

long StartGlyphsCode = 0xAC00;//12593;
float BaseLine = 0.4;
int Curindex = 0;

//정점의 왼쪽 위 위치 
HRESULT SetGeometry(float x, float y, float z, float width, float height)
{
    CUSTOMVERTEX* pVertices;
    if( FAILED( g_pVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
        return E_FAIL;

    float myx = width / 2.0f;
    float myy = height / 2.0f;
    g_Vertices[0].position = D3DXVECTOR3(-myx, -myy, 0.0f);
    g_Vertices[1].position = D3DXVECTOR3(myx, -myy, 0.0f);
    g_Vertices[2].position = D3DXVECTOR3(myx, myy, 0.0f);
    g_Vertices[3].position = D3DXVECTOR3(-myx, myy, 0.0f);

    memcpy(pVertices, g_Vertices, sizeof(CUSTOMVERTEX)*4);

    g_pVB->Unlock();


    DWORD* pIndexes;

    if (FAILED(g_pIB->Lock(0, 0, (void**)&pIndexes, 0)))
        return E_FAIL;

    memcpy(pIndexes, g_Indexes, sizeof(DWORD) * 6);

    g_pIB->Unlock();

    return S_OK;
}


HRESULT InitShader(LPCWSTR  FileName)
{
    LPD3DXBUFFER lpErrors = 0;
    LPD3DXBUFFER lpBufferEffect = 0;

    HRESULT hr = D3DXCreateEffectCompilerFromFile(FileName, NULL, NULL, NULL, &g_lpEffectCompiler,&lpErrors);
    if (hr == S_OK)
    {
        hr = g_lpEffectCompiler->CompileEffect(0, &lpBufferEffect, &lpErrors);
    }


    HRESULT hr2 = D3DXCreateEffectFromFile(g_pd3dDevice, FileName, NULL, NULL, D3DXSHADER_USE_LEGACY_D3DX9_31_DLL/*NULL*/, NULL, &g_lpEffect, &lpErrors);
    if (hr2 != S_OK) {
        char strLog[1024];
        sprintf(strLog, "Error : %s",  (char*)lpErrors->GetBufferPointer());
        return NULL;
    }

    return S_OK;
}


VOID Cleanup()
{
    if( g_pTexture != NULL )
        g_pTexture->Release();

    if( g_pVB != NULL )
        g_pVB->Release();

    if( g_pd3dDevice != NULL )
        g_pd3dDevice->Release();

    if( g_pD3D != NULL )
        g_pD3D->Release();

    /*if (tex != NULL)
        tex->Release();*/
}


VOID SetupMatrices(float x = 0, float y = 0, float z = 0, float sx = 0, float sy = 0, float sz = 0)
{
    D3DXMATRIXA16 matWorld;
    D3DXMatrixIdentity(&matWorld);

    if (x != 0 || y != 0 || z != 0)
    {
        D3DXMATRIXA16 tempWorld;
        D3DXMatrixTranslation(&tempWorld, x, y, z);
        D3DXMatrixMultiply(&matWorld, &matWorld, &tempWorld);
    }

    if (sx != 0 || sy != 0 || sz != 0)
    {
        D3DXMATRIXA16 tempWorld;
        D3DXMatrixScaling(&tempWorld, sx, sy, sz);
        D3DXMatrixMultiply(&matWorld, &matWorld, &tempWorld);
    }

    g_pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);

	D3DXVECTOR3 vEyePt(/*g_FontSize.x/ 2.0f*/0, /*g_FontSize.y / 2.0f*/0, -10.0f);
	D3DXVECTOR3 vLookatPt(/*g_FontSize.x / 2.0f*/0, /*g_FontSize.y / 2.0f*/0, 0.0f);
    D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
    
    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    D3DXMATRIXA16 matProj;
    //D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f );
    //D3DXMatrixOrthoLH(&matProj, 10, /*12.0f*/g_FontSize.y, 0, 30);
    D3DXMatrixOrthoOffCenterLH(&matProj, 0, 10, 0, 10, -500, 500);
    g_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    D3DXMATRIXA16 temp;
    D3DXMatrixMultiply(&temp, &matWorld, &matView);
    D3DXMatrixMultiply(&temp, &temp, &matProj);

    D3DXHANDLE handle;
    handle = g_lpEffect->GetParameterByName(0, "_matWorldViewProjection");
    g_lpEffect->SetMatrix(handle, &temp);
}

VOID SetupMatrices2(float x = 0, float y = 0, float z = 0, float sx = 0, float sy = 0, float sz = 0)
{
    D3DXMATRIXA16 matWorld;
    D3DXMatrixIdentity(&matWorld);

    if (x != 0 || y != 0 || z != 0)
    {
        D3DXMATRIXA16 tempWorld;
        D3DXMatrixTranslation(&tempWorld, x, y, z);
        D3DXMatrixMultiply(&matWorld, &matWorld, &tempWorld);
    }

    if (sx != 0 || sy != 0 || sz != 0)
    {
        D3DXMATRIXA16 tempWorld;
        D3DXMatrixScaling(&tempWorld, sx, sy, sz);
        D3DXMatrixMultiply(&matWorld, &matWorld, &tempWorld);
    }

    g_pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);

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

void SetShaderParameter(char* technique = "SoftEdgeDraw", D3DXVECTOR4 baseColor = D3DXVECTOR4(0,0,0,0))
{
    //텍스쳐 설정
    g_pd3dDevice->SetTexture(0, g_SDFTexture);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    //RenderState 설정
    g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    //Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

    //파이프라인 설정
    g_pd3dDevice->SetStreamSource(0, g_pVB, 0, sizeof(CUSTOMVERTEX));
    g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
    g_pd3dDevice->SetIndices(g_pIB);

    //셰이더 설정
    g_lpEffect->SetTechnique(technique);

    D3DXHANDLE handle;
    handle = g_lpEffect->GetParameterByName(0, "_baseColor");
    g_lpEffect->SetVector(handle, &baseColor);

    if (!strcmp(technique, "SoftEdgeDraw"))
    {
        handle = g_lpEffect->GetParameterByName(0, "_smoothing");
        g_lpEffect->SetFloat(handle, 0.175);
    }
    else if (!strcmp(technique, "SharpEdgeDraw"))
    {
        handle = g_lpEffect->GetParameterByName(0, "_lowThreshold");
        g_lpEffect->SetFloat(handle, 0.5);
    }
    else if (!strcmp(technique, "SharpEdgeWithGlowDraw"))
    {
        handle = g_lpEffect->GetParameterByName(0, "_lowThreshold");
        g_lpEffect->SetFloat(handle, 0.495);

        handle = g_lpEffect->GetParameterByName(0, "_borderColor");
        g_lpEffect->SetVector(handle, &D3DXVECTOR4(0.5, 0, 1, 1));

        handle = g_lpEffect->GetParameterByName(0, "_borderAlpha");
        g_lpEffect->SetFloat(handle, 1);
    }
    else if (!strcmp(technique, "BorderDraw"))
    {

        handle = g_lpEffect->GetParameterByName(0, "_smoothing");
        g_lpEffect->SetFloat(handle, 0.49);

        handle = g_lpEffect->GetParameterByName(0, "_highThreshold");
        g_lpEffect->SetFloat(handle, 0.55 + testPoint.x);

        handle = g_lpEffect->GetParameterByName(0, "_lowThreshold");
        g_lpEffect->SetFloat(handle, 0.001 + testPoint.y);

        handle = g_lpEffect->GetParameterByName(0, "_borderColor");
        g_lpEffect->SetVector(handle, &D3DXVECTOR4(1, 0, 0, 1));
    }
    else if (!strcmp(technique, "SoftEdge2Draw"))
    {
        handle = g_lpEffect->GetParameterByName(0, "_smoothing");
        g_lpEffect->SetFloat(handle, 0.175);
    }
    else if (!strcmp(technique, "NewBorderDraw"))
    {
        handle = g_lpEffect->GetParameterByName(0, "_TestFontWidth");
        g_lpEffect->SetFloat(handle, 0.359 + testPoint.x);

        handle = g_lpEffect->GetParameterByName(0, "_TestOutlineWidth");
        g_lpEffect->SetFloat(handle, 0.079 + testPoint.y);

        handle = g_lpEffect->GetParameterByName(0, "_borderColor");
        g_lpEffect->SetVector(handle, &D3DXVECTOR4(1, 0, 0, 1));
    } 
    else if (!strcmp(technique, "NewSoftDraw"))
    {
        handle = g_lpEffect->GetParameterByName(0, "_TestFontWidth");
        g_lpEffect->SetFloat(handle, 0.14 + testPoint.x);
    }
    else if (!strcmp(technique, "NewNeonDraw"))
    {
        handle = g_lpEffect->GetParameterByName(0, "_TestFontWidth");
        g_lpEffect->SetFloat(handle, 0.359 + testPoint.y);

        handle = g_lpEffect->GetParameterByName(0, "_BorderWidth");
        g_lpEffect->SetFloat(handle, 0.14 + testPoint.x);

        handle = g_lpEffect->GetParameterByName(0, "_NeonPower");
        g_lpEffect->SetFloat(handle, 2.48 + testPoint.y);

        handle = g_lpEffect->GetParameterByName(0, "_NeonBrightness");
        g_lpEffect->SetFloat(handle, 0.71  + testScale.x);

        handle = g_lpEffect->GetParameterByName(0, "_borderColor");
        g_lpEffect->SetVector(handle, &D3DXVECTOR4(0.92, 0.156, 0.901, 1));
    }
    else if (!strcmp(technique, "NewDropShadow"))
    {
        handle = g_lpEffect->GetParameterByName(0, "_TestFontWidth");
        g_lpEffect->SetFloat(handle, 0.359 + testPoint.x);

        handle = g_lpEffect->GetParameterByName(0, "_ShadowDist");
        g_lpEffect->SetFloat(handle, 0.14 + testPoint.y);

        handle = g_lpEffect->GetParameterByName(0, "_borderColor");
        g_lpEffect->SetVector(handle, &D3DXVECTOR4(1, 0.32, 0.32, 0.32));
    }
}

void CreateFontGeometry(Glyph glyph, CUSTOMVERTEX* Vertices, D3DXVECTOR2& penpos)
{

    //CUSTOMVERTEX Vertices[4];

    //float tu = (glyph.mTextureCoordX - g_margin);
    //float tv = 1 - (glyph.mTextureCoordY - g_margin) - (glyph.mTextureHeight + g_margin/* * 2*/);

    D3DXVECTOR2 pos = D3DXVECTOR2(penpos.x + glyph.mHorizontalBearingX, penpos.y + glyph.mHorizontalBearingY);

    float tu;
    float tv;

    float WidthRatio = glyph.mWidth / glyph.mTextureWidth;
    float HeightRatio = glyph.mHeight / glyph.mTextureHeight;

    float WidthMargin = g_margin * WidthRatio;
    float HeightMargin = g_margin * HeightRatio;


    /*if (testbool)
    {
        pos.x -= WidthMargin;
        pos.y += HeightMargin;

        glyph.mWidth += WidthMargin;
        glyph.mHeight += HeightMargin;
    }*/
    pos.x -= WidthMargin;
    pos.y += HeightMargin;

    glyph.mWidth += WidthMargin;
    glyph.mHeight += HeightMargin;


    tu = glyph.mTextureCoordX - g_margin;
    tv = glyph.mTextureCoordY - g_margin;
    Vertices[0] = CUSTOMVERTEX(D3DXVECTOR3(pos.x, pos.y - glyph.mHeight, 0.0f), 0xffffffff, tu, tv);//왼쪽 위

    tu = glyph.mTextureCoordX + glyph.mTextureWidth + g_margin;
    tv = glyph.mTextureCoordY - g_margin;
    Vertices[1] = CUSTOMVERTEX(D3DXVECTOR3(pos.x + glyph.mWidth, pos.y - glyph.mHeight, 0.0f), 0xffffffff, tu, tv);//오른쪽 위


    tu = glyph.mTextureCoordX + glyph.mTextureWidth + g_margin;
    tv = glyph.mTextureCoordY + glyph.mTextureHeight + g_margin;
    Vertices[2] = CUSTOMVERTEX(D3DXVECTOR3(pos.x + glyph.mWidth, pos.y, 0.0f), 0xffffffff, tu, tv);//오른쪽 아래


    tu = glyph.mTextureCoordX - g_margin;
    tv = glyph.mTextureCoordY + glyph.mTextureHeight + g_margin;
    Vertices[3] = CUSTOMVERTEX(D3DXVECTOR3(pos.x, pos.y, 0.0f), 0xffffffff, tu, tv);//왼쪽 아래


    penpos.x += (glyph.mHorizontalAdvance /*+ m_FontPitch*/);
}

Glyph GetGlyph(int fontindex, long unicode)
{
    Glyph glyph;
    memcpy(&glyph, g_SDFGenerator->GetCashedGlyph(fontindex, 'A'), sizeof(Glyph));
    return glyph;
}

void CreateFontGeometry(WCHAR* str, D3DXVECTOR2 pos/*시작 좌표*/)
{
    int strsize = wcslen(str);
    //CUSTOMVERTEX* Vertices = (CUSTOMVERTEX*)malloc(sizeof(CUSTOMVERTEX) * 4 * strsize);
    CUSTOMVERTEX Vertices[500];
    ZeroMemory(Vertices, sizeof(CUSTOMVERTEX) * 4 * strsize);
    //DWORD* Indexices = (DWORD*)malloc(sizeof(DWORD) * 6 * strsize);
    DWORD Indexices[500] = { 0 };

    Glyph curGlyph;

    for (int count = 0; count < strsize; count++)
    {
        /*long temp = str[count];
        curGlyph = g_glyphs[temp];*/
        curGlyph = GetGlyph(0, str[count]);
        CreateFontGeometry(curGlyph, &Vertices[count * 4], pos);

        for (int i = 0; i < 6; i++)
        {
            Indexices[(count * 6) + i] = g_Indexes[i] + count * 4;
        }
    }


    CUSTOMVERTEX* pVerticesTemp;
    if (FAILED(g_pVB->Lock(0, 0, (void**)&pVerticesTemp, 0)))
        return;

    memcpy(pVerticesTemp, Vertices, sizeof(CUSTOMVERTEX) * 4 * strsize);

    g_pVB->Unlock();


    //인덱스 정보
    DWORD* pIndexes;

    if (FAILED(g_pIB->Lock(0, 0, (void**)&pIndexes, 0)))
        return;

    memcpy(pIndexes, Indexices, sizeof(DWORD) * 6 * strsize);

    g_pIB->Unlock();
}



void DrawFont2(WCHAR* str, D3DXVECTOR2 startPos, D3DXVECTOR4 baseColor, char* technique = "SoftEdgeDraw")
{

    //글자 하나당 Vertex의 크기는 Metrics 파일에 있는 크기 정보로 만든다.
    int strsize = wcslen(str);
    
    SetupMatrices(startPos.x, startPos.y, 0, /*0.64*/0.03, /*0.64*/0.03, 1);

    CreateFontGeometry(str, startPos);

    SetShaderParameter(technique, baseColor);

    //출력
    UINT passCount;
    g_lpEffect->Begin(&passCount, 0);
    for (int pass = 0; pass < passCount; pass++)
    {
        g_lpEffect->BeginPass(pass);
        g_pd3dDevice->DrawIndexedPrimitive(/*D3DPT_TRIANGLESTRIP*/D3DPT_TRIANGLELIST, 0, 0, 4 * strsize, 0, 2 * strsize);
        g_lpEffect->EndPass();

    }
    g_lpEffect->End();

}

D3DXVECTOR3 WorldToScreen(D3DXVECTOR3 vec)
{
    D3DXMATRIXA16 matWorld;
    D3DXMatrixIdentity(&matWorld);

    D3DXVECTOR3 vEyePt(0.0f, 0.0f, -10.0f);
    D3DXVECTOR3 vLookatPt(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 vUpVec(0.0f, 1.0f, 0.0f);

    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);

    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f);

    D3DXMATRIXA16 matWVP;
    D3DXMatrixMultiply(&matWVP, &matWorld, &matView);
    D3DXMatrixMultiply(&matWVP, &matWVP, &matProj);



    D3DXVECTOR3 screen;

    D3DXVec3TransformNormal(&screen, &vec, &matWVP);

    return screen;
}



int GetCharsetFromLang(LANGID langid)
{
    switch (langid)
    {
    case LANG_JAPANESE:
        return SHIFTJIS_CHARSET;
    case LANG_KOREAN:
        return HANGEUL_CHARSET;

    case LANG_ENGLISH:
#if ENGLAND || EUROPE || SOUTH_AMERICA
        return DEFAULT_CHARSET;
        //EASTEUROPE_CHARSET
#else
        return DEFAULT_CHARSET;
#endif	


    case LANG_CHINESE:
        return GB2312_CHARSET;

    //case LANG_TAIWAN:
    //    // 디버그 버전에서는 한글로...
    //    return CHINESEBIG5_CHARSET;

        // 태국
    case LANG_THAI:
        return THAI_CHARSET;

    case LANG_VIETNAMESE:
        return VIETNAMESE_CHARSET;

        // 브라질 
    case LANG_PORTUGUESE:
        return ANSI_CHARSET;

        // 남미
// 	   case LANG_ESTONIAN:
// 		   return EASTEUROPE_CHARSET;

    default:
        return ANSI_CHARSET;


    }
    return 0;
}

int	GetSystemCharSet()
{
    return GetCharsetFromLang(LANG_KOREAN);
}

SIZE GetFontSize(WCHAR* str)
{
    HDC m_hDC = ::CreateCompatibleDC(0);

#if _SHOUT_PANNEL_LINK_
    HFONT hFont = GetAgentFontHandle(fattr.fontName, fattr.fontSize, fattr.bBold, fattr.bItalic, fattr.bUnderLine, fattr.aniFontSize);
#else
    HFONT hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, 0, FALSE, FALSE, GetSystemCharSet(), OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, L"굴림체");
#endif

    HFONT hOldFont = (HFONT)::SelectObject(m_hDC, hFont);

    SIZE size;
    if (::GetTextExtentPoint(m_hDC, str, 12, (LPSIZE)&size) == 0)
    {
        ::SelectObject(m_hDC, hOldFont);
    }
    ::SelectObject(m_hDC, hOldFont);
    return size;
}

void CalculateFontSize(WCHAR* str, float& width, float& height)
{
    float minY = 0;
    float maxY = 0;

    width = 0;
    height = 0;

    int len = wcslen(str);
    if (len <= 0)
        return;

    D3DXVECTOR3 startpos = D3DXVECTOR3(0, 0, 0);// 원점에서 시작

    for (int i = 0; i < len; i++)
    {
        //해당 좌표는 월드 좌표이기 때문에 screen 좌표로 바꿔주어야 한다.
        startpos = D3DXVECTOR3(g_glyphs[str[i]].mHorizontalBearingX, g_glyphs[str[i]].mHorizontalBearingY, 0);

        //d3dxcoord
        maxY = max(startpos.y, maxY);
        minY = min(startpos.y - g_glyphs[str[i]].mHeight, minY);

        width += g_glyphs[str[i]].mHorizontalAdvance;
    }

    minY = WorldToScreen(D3DXVECTOR3(0, minY, 0)).y;
    maxY = WorldToScreen(D3DXVECTOR3(0, maxY, 0)).y;

    width = WorldToScreen(D3DXVECTOR3(width, 0, 0)).x;

    height = abs(minY - maxY);

    width *= 700 / 512;
    height *= 700 / 512;
}

LPDIRECT3DTEXTURE9 CreateFontTexture(LPDIRECT3DTEXTURE9& tex,WCHAR* str, float& width, float& height, float texWidth, float texHeight)
{
    LPDIRECT3DSURFACE9 pSurface;
    LPDIRECT3DSURFACE9 pOriSurface;
    LPDIRECT3DTEXTURE9 pTexture = NULL;
    LPD3DXRENDERTOSURFACE pRentoSur;

    float tempWidth;
    float tempHeight;
    CalculateFontSize(str, width, height);
    SIZE fontsize = GetFontSize(str);
    width = fontsize.cx;
    height = fontsize.cy;
    g_FontSize.x = fontsize.cx/2.0f;
    g_FontSize.y = fontsize.cy/2.0f;

    //D3DXCreateRenderToSurface(g_pd3dDevice, texWidth, texHeight, D3DFMT_A4R4G4B4, FALSE, D3DFMT_UNKNOWN, &pRentoSur);
    if (tex == NULL)
        D3DXCreateTexture(g_pd3dDevice, fontsize.cx * 10/*512*/, fontsize.cy * 10/*512*/, 0, D3DUSAGE_RENDERTARGET, D3DFMT_A4R4G4B4, D3DPOOL_DEFAULT, &tex);

    tex->GetSurfaceLevel(0, &pSurface);

    g_pd3dDevice->GetRenderTarget(0, &pOriSurface);
    g_pd3dDevice->SetRenderTarget(0, pSurface);


    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DXCOLOR(255,255,255,0), 1.0f, 0);
    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
    {
        //DrawFont(L"부드럽게", D3DXVECTOR2(0, 1), D3DXVECTOR4(1, 1, 1, 1), /*"DebugDraw"*/"SoftEdgeDraw");
        g_pd3dDevice->EndScene();
    }

    g_pd3dDevice->SetRenderTarget(0, pOriSurface);
    return pTexture;
}

LPDIRECT3DTEXTURE9 tex = NULL;



VOID Render()
{
    float width = 0;
    float height = 0;

    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    // Begin the scene
    if( SUCCEEDED( g_pd3dDevice->BeginScene() ) )
    {
        //알파 블렌드 사용
        //g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        //g_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        ////g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        //g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        //g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

        ////알파 테스트 사용
        //g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        //g_pd3dDevice->SetRenderState(D3DRS_ALPHAREF, /*0x00000001*//*0x80*//*0x1*/0x80);
        //g_pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL/*D3DCMP_LESSEQUAL*/); // D3DCMP_GREATEREQUAL => 픽셀의 알파 값이 ALPHAREF에 설정된 값보다 크거나 같으면 Alpha를 1로해서 출력 한다.

        DrawFont2(L"가나다라마바사", D3DXVECTOR2(0, 100), D3DXVECTOR4(1, 1, 1, 1), "NewDropShadow");
        DrawFont2(L"안녕하세요", D3DXVECTOR2(0, 120), D3DXVECTOR4(1, 1, 1, 1), "NewDropShadow");
        DrawFont2(L"Draw DropShadow", D3DXVECTOR2(0, 140), D3DXVECTOR4(1, 1, 1, 1), "NewDropShadow");
       
        //SetGeometry
        //SetupMatrices


        g_pd3dDevice->EndScene();
    }

    g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}

LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_DESTROY:
            Cleanup();
            PostQuitMessage( 0 );
            return 0;

        case WM_KEYUP:
            /*switch (wParam)
            {
            case VK_LEFT:
                Curindex = (((g_glyphs.size() + Curindex) - 1) % g_glyphs.size());
                break;

            case VK_RIGHT:
                Curindex = ((Curindex + 1) % g_glyphs.size());
                break;
            }*/
            break;

        case WM_KEYDOWN:
            {
                switch (wParam)
                {
                case VK_LEFT:
                {
                    testPoint.x += 0.01f;
                    int a = 0;
                }
                    break;

                case VK_RIGHT:
                {
                    testPoint.x -= 0.01f;
                    int a = 0;
                }
                    
                    break;


                case VK_UP:
                {
                    testPoint.y -= 0.01f;
                    int a = 0;
                }
                    break;

                case VK_DOWN:
                {
                    testPoint.y += 0.01f;
                    int a = 0;
                }
                    break;

                case 'Z':
                    testScale.x += 0.01f;
                    testScale.y += 0.01f;
                    break;

                case 'X':
                    testScale.x -= 0.01f;
                    testScale.y -= 0.01f;
                    break;
                
                }
            }
            break;
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE hInst, HINSTANCE, LPWSTR, INT )
{
    UNREFERENCED_PARAMETER( hInst );

    // Register the window class
    WNDCLASSEX wc =
    {
        sizeof( WNDCLASSEX ), CS_CLASSDC, MsgProc, 0L, 0L,
        GetModuleHandle( NULL ), NULL, NULL, NULL, NULL,
        L"D3D Tutorial", NULL
    };
    RegisterClassEx( &wc );

    // Create the application's window
    HWND hWnd = CreateWindow( L"D3D Tutorial", L"D3D Tutorial 05: Textures",
                              WS_OVERLAPPEDWINDOW, 100, 100, 1100, 1100,
                              NULL, NULL, wc.hInstance, NULL );

    // Initialize Direct3D
    if( SUCCEEDED( InitD3D( hWnd ) ) )
    {
        g_SDFGenerator = new SDFGenerator(g_pd3dDevice);
        g_SDFGenerator->Init(7);
        //g_SDFGenerator->GetCashedGlyph(0, 'A');

        if (SUCCEEDED(InitTexTure()))
        {
            // Create the scene geometry
            if (SUCCEEDED(CreateBuffer()))
            {

                InitShader(L"Shader\\SDFShader.hlsl");

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
        
    }

    UnregisterClass( L"D3D Tutorial", wc.hInstance );
    return 0;
}




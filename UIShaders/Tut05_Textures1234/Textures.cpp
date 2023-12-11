
#include <Windows.h>
#include <mmsystem.h>
#include <d3dx9.h>
#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#pragma warning( default : 4996 )

#include "glyph.h"
#include "metrics_parser.h"


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


struct FPOINT
{
    float x;
    float y;
};

FPOINT g_FontSize;
FPOINT testPoint = {0,0};
FPOINT testScale = { 8,8 };

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
    { D3DXVECTOR3(-5.0f,-5.0f, 0.0f), 0xffffffff,0,1 }, // ���� �Ʒ�
    { D3DXVECTOR3(5.0f,-5.0f, 0.0f), 0xffffffff,1,1 },// ������ �Ʒ�
    { D3DXVECTOR3(5.0f, 5.0f, 0.0f), 0xffffffff,1,0 },//������ ��
    { D3DXVECTOR3(-5.0f, 5.0f, 0.0f), 0xffffffff,0,0 },//���� ��
};


DWORD g_Indexes[] =
{
    3,2,0,
    0,2,1,
};

HRESULT CreateBuffer()
{   
    // Create the vertex buffer.
    if (FAILED(g_pd3dDevice->CreateVertexBuffer(4 * sizeof(CUSTOMVERTEX),
        0, D3DFVF_CUSTOMVERTEX,
        D3DPOOL_DEFAULT, &g_pVB, NULL)))
    {
        return E_FAIL;
    }


    if (FAILED(g_pd3dDevice->CreateIndexBuffer(sizeof(DWORD) * 6, 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &g_pIB, NULL)))
    {
        return E_FAIL;
    }
}

long StartGlyphsCode = 0xAC00;//12593;
float BaseLine = 0.4;
int Curindex = 0;

//������ ���� �� ��ġ 
HRESULT SetGeometry(float x, float y, float z, float width, float height)
{
    CUSTOMVERTEX* pVertices;
    if( FAILED( g_pVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
        return E_FAIL;

    //���� �Ʒ�
    //g_Vertices[0].position = D3DXVECTOR3(x, y - height, 0);
    //������ �Ʒ�
   // g_Vertices[1].position = D3DXVECTOR3(x + width, y - height, 0);

    //������ ��
    //g_Vertices[2].position = D3DXVECTOR3(x + width, y, 0);

    //���� ��
    //g_Vertices[3].position = D3DXVECTOR3(x,y,0);


    //float uvY = 1 - (g_glyphs[StartGlyphsCode + Curindex].mTextureCoordY - g_margin);


    //g_Vertices[0].tu = (g_glyphs[StartGlyphsCode + Curindex].mTextureCoordX - g_margin);
    ////g_Vertices[0].tv = 1- (g_glyphs[StartGlyphsCode + Curindex].mTextureCoordY - g_margin);
    //g_Vertices[0].tv = uvY - (g_glyphs[StartGlyphsCode + Curindex].mTextureHeight + g_margin * 2);

    //g_Vertices[1].tu = (g_Vertices[0].tu + (g_glyphs[StartGlyphsCode + Curindex].mTextureWidth + g_margin*2));
    //g_Vertices[1].tv = uvY - (g_glyphs[StartGlyphsCode + Curindex].mTextureHeight + g_margin * 2);

    //g_Vertices[2].tu = (g_Vertices[0].tu + (g_glyphs[StartGlyphsCode + Curindex].mTextureWidth + g_margin*2));
    //g_Vertices[2].tv = uvY;

    //g_Vertices[3].tu = (g_Vertices[0].tu);
    //g_Vertices[3].tv = uvY;

    //{ D3DXVECTOR3(-5.0f, -5.0f, 0.0f), 0xffffffff, 0, 1 }, // ���� �Ʒ�
    //{ D3DXVECTOR3(5.0f,-5.0f, 0.0f), 0xffffffff,1,1 },// ������ �Ʒ�
    //{ D3DXVECTOR3(5.0f, 5.0f, 0.0f), 0xffffffff,1,0 },//������ ��
    //{ D3DXVECTOR3(-5.0f, 5.0f, 0.0f), 0xffffffff,0,0 },//���� ��

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

	D3DXVECTOR3 vEyePt(g_FontSize.x/ 2.0f, g_FontSize.y / 2.0f, -10.0f);
	D3DXVECTOR3 vLookatPt(g_FontSize.x / 2.0f, g_FontSize.y / 2.0f, 0.0f);
    D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
    
    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f );
    //D3DXMatrixOrthoLH(&matProj, /*108.0f*/g_FontSize.x, /*12.0f*/g_FontSize.y, 0, 30);
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

void CreateFontGeometry(Glyph glyph, D3DXVECTOR2 pos/*���� ���� ��ǥ*/)
{

    CUSTOMVERTEX Vertices[4];
    /*float tu = (glyph.mTextureCoordX - g_margin);
    float tv = 1 - (glyph.mTextureCoordY - g_margin) - (glyph.mTextureHeight + g_margin * 2);*/
    float tu = glyph.mTextureCoordX/* - g_margin*/;
    float tv = glyph.mTextureCoordY - g_margin;
    Vertices[0] = CUSTOMVERTEX(D3DXVECTOR3(pos.x, pos.y - glyph.mHeight, 0.0f), 0xffffffff, tu, tv);

    /*tu = (Vertices[0].tu + (glyph.mTextureWidth + g_margin * 2));
    tv = Vertices[0].tv;*/
    tu = glyph.mTextureCoordX + glyph.mTextureWidth + g_margin;
    tv = glyph.mTextureCoordY - g_margin;
    Vertices[1] = CUSTOMVERTEX(D3DXVECTOR3(pos.x + glyph.mWidth, pos.y - glyph.mHeight, 0.0f), 0xffffffff, tu, tv);

    /*tu = Vertices[1].tu;
    tv = 1 - (glyph.mTextureCoordY - g_margin);*/
    tu = glyph.mTextureCoordX + glyph.mTextureWidth + g_margin;
    tv = glyph.mTextureCoordY + glyph.mTextureHeight + g_margin;
    Vertices[2] = CUSTOMVERTEX(D3DXVECTOR3(pos.x + glyph.mWidth, pos.y, 0.0f), 0xffffffff, tu, tv);

    /*tu = Vertices[0].tu;
    tv = 1 - (glyph.mTextureCoordY - g_margin);*/
    tu = glyph.mTextureCoordX/* - g_margin*/;
    tv = glyph.mTextureCoordY + glyph.mTextureHeight + g_margin;
    Vertices[3] = CUSTOMVERTEX(D3DXVECTOR3(pos.x, pos.y, 0.0f), 0xffffffff, tu, tv);

    CUSTOMVERTEX* pVertices;
    if (FAILED(g_pVB->Lock(0, 0, (void**)&pVertices, 0)))
        return;

    memcpy(pVertices, Vertices, sizeof(CUSTOMVERTEX) * 4);

    g_pVB->Unlock();


    //�ε��� ����
    DWORD* pIndexes;

    if (FAILED(g_pIB->Lock(0, 0, (void**)&pIndexes, 0)))
        return;

    memcpy(pIndexes, g_Indexes, sizeof(DWORD) * 6);

    g_pIB->Unlock();
}



//�ش� ��ġ�� ���ڸ� �׷��ְ� ��ġ�� ���� ��ġ�� �Ű��ش�.
void DrawFont(Glyph glyph, D3DXVECTOR2& penPos, D3DXVECTOR2 baseLine, D3DXVECTOR4 baseColor , char* technique = "SoftEdgeDraw")
{
    //���� ��ġ�� �������� ���ڸ� �׷��� �������� �����ش�.
    D3DXVECTOR2 pos = D3DXVECTOR2(penPos.x + glyph.mHorizontalBearingX, penPos.y + glyph.mHorizontalBearingY);
    CreateFontGeometry(glyph, pos);
    

    //�ؽ��� ����
    g_pd3dDevice->SetTexture(0, g_SDFTexture);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    //RenderState ����
    g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    //Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

    //���������� ����
    g_pd3dDevice->SetStreamSource(0, g_pVB, 0, sizeof(CUSTOMVERTEX));
    g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
    g_pd3dDevice->SetIndices(g_pIB);

    //���̴� ����
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
        g_lpEffect->SetFloat(handle, 0.55);

        handle = g_lpEffect->GetParameterByName(0, "_lowThreshold");
        g_lpEffect->SetFloat(handle, 0.001);

        handle = g_lpEffect->GetParameterByName(0, "_borderColor");
        g_lpEffect->SetVector(handle, &D3DXVECTOR4(1, 0, 0, 1));
    }
    else if (!strcmp(technique, "SoftEdge2Draw"))
    {
        handle = g_lpEffect->GetParameterByName(0, "_smoothing");
        g_lpEffect->SetFloat(handle, 0.175);
    }

    
    //g_lpEffect->SetFloat(handle, 0.495);

    //handle = g_lpEffect->GetParameterByName(0, "_highThreshold");
    ////g_lpEffect->SetFloat(handle, 0.24);
    //g_lpEffect->SetFloat(handle, 0.44);
    //g_lpEffect->SetFloat(handle, 0.84);

    //handle = g_lpEffect->GetParameterByName(0, "_borderColor");
    //g_lpEffect->SetVector(handle, &D3DXVECTOR4(1, 0, 1, 1));

    //handle = g_lpEffect->GetParameterByName(0, "_baseColor");
    //g_lpEffect->SetVector(handle, &D3DXVECTOR4(1, 1, 1, 1));

    //handle = g_lpEffect->GetParameterByName(0, "_smoothing");
    //g_lpEffect->SetFloat(handle, 0.175);
    ////g_lpEffect->SetFloat(handle, 0.035);

    //handle = g_lpEffect->GetParameterByName(0, "_borderAlpha");
    //g_lpEffect->SetFloat(handle, 1);

    //���
    UINT passCount;
    g_lpEffect->Begin(&passCount, 0);
    for (int pass = 0; pass < passCount; pass++)
    {
        g_lpEffect->BeginPass(pass);
        g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        g_lpEffect->EndPass();

    }
    g_lpEffect->End();




    //���� �ϳ��� ����� ������ ���� ���� ��ġ�� ���� ���� ���� ��ġ�� �Ű��ش�.
    penPos.x += glyph.mHorizontalAdvance;
    //penPos.y += glyph.mVerticalAdvance;
}

void DrawFont(WCHAR* str, D3DXVECTOR2 startPos , D3DXVECTOR4 baseColor , char* technique = "SoftEdgeDraw")
{

    //���� �ϳ��� Vertex�� ũ��� Metrics ���Ͽ� �ִ� ũ�� ������ �����.
    int strsize = wcslen(str);
    //D3DXCreateFont(g_pd3dDevice, 25,12,500,0,false, DEFAULT_CHARSET,
    SetupMatrices(startPos.x, startPos.y, -10, 0.02, 0.02, 0.02);
    Glyph curGlyph;

    //���� ���ڰ� �׷��� ��ġ
    //D3DXVECTOR2 curCharPos = startPos;
    D3DXVECTOR2 curPenPos = startPos;
    //char teststr[1024];
    //strcpy(teststr, str);
    //WCHAR strunicode[1024] = { 0 };
    //MultiByteToWideChar(CP_ACP, 0, teststr, strsize, strunicode, strsize);
    // 
    // 
    //WideCharToMultiByte(CP_ACP, 0, strunicode, -1, teststr, strsize, NULL, NULL);
    for (int count = 0; count < strsize; count++)
    {
        long test = str[count];
        curGlyph = g_glyphs[test];

        DrawFont(curGlyph, curPenPos, startPos, baseColor, technique);
    }
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
    //    // ����� ���������� �ѱ۷�...
    //    return CHINESEBIG5_CHARSET;

        // �±�
    case LANG_THAI:
        return THAI_CHARSET;

    case LANG_VIETNAMESE:
        return VIETNAMESE_CHARSET;

        // ����� 
    case LANG_PORTUGUESE:
        return ANSI_CHARSET;

        // ����
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
    HFONT hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, 0, FALSE, FALSE, GetSystemCharSet(), OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, L"����ü");
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

    D3DXVECTOR3 startpos = D3DXVECTOR3(0, 0, 0);// �������� ����

    for (int i = 0; i < len; i++)
    {
        //�ش� ��ǥ�� ���� ��ǥ�̱� ������ screen ��ǥ�� �ٲ��־�� �Ѵ�.
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


    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DXCOLOR(255,255,255,0)/*D3DCOLOR_XRGB(255, 255, 255)*/, 1.0f, 0);
    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
    {

        //DrawFont(L"�ε巴��", D3DXVECTOR2(0, 1), D3DXVECTOR4(1, 1, 1, 1), /*"DebugDraw"*/"SoftEdgeDraw");

        //DrawFont(L"��ī�Ӱ�", D3DXVECTOR2(0, 0), D3DXVECTOR4(0, 1, 1, 1), "SharpEdgeDraw");

        //DrawFont(L"�ܰ���", D3DXVECTOR2(0, -1), D3DXVECTOR4(0, 0, 1, 1), "BorderDraw");

        //DrawFont(L"�ܰ����ι�°", D3DXVECTOR2(0, -2), D3DXVECTOR4(1, 0, 1, 1), "SharpEdgeWithGlowDraw");

        DrawFont(str, D3DXVECTOR2(0, 0), D3DXVECTOR4(0, 0, 0, 1), "SoftEdgeDraw");

        g_pd3dDevice->EndScene();
    }


    g_pd3dDevice->SetRenderTarget(0, pOriSurface);
    //DrawFont(str, D3DXVECTOR2(0, 0), D3DXVECTOR4(1, 1, 1, 1), "SoftEdgeDraw");
    //tex = pTexture;
    return pTexture;
}

LPDIRECT3DTEXTURE9 tex = NULL;

VOID Render()
{
    
    
    float width = 0;
    float height = 0;
    //if(g_FontTextureMap.find("Sharp Boarder") == g_FontTextureMap.end())
    //CreateFontTexture(tex, L"����ǿ����", width, height, 512, 512);

    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    // Begin the scene
    if( SUCCEEDED( g_pd3dDevice->BeginScene() ) )
    {
       
        //DrawFont(L"�����ٶ󸶹ٻ�", D3DXVECTOR2(0, -1), D3DXVECTOR4(0, 1, 1, 1), "SoftEdgeDraw");
        //DrawFont(L"�����ٶ󸶹ٻ�", D3DXVECTOR2(0, 1), D3DXVECTOR4(0, 1, 1, 1), "SharpEdgeDraw");

        //DrawFont(L"�ܰ����ι�°", D3DXVECTOR2(0, -2), D3DXVECTOR4(1, 0, 1, 1), "SharpEdgeWithGlowDraw");
        /*DrawFont(L"�����ٶ󸶹ٻ�", D3DXVECTOR2(0, 1), D3DXVECTOR4(1,1,1,1));

        DrawFont(L"�����ٶ󸶹ٻ�", D3DXVECTOR2(0, 0), D3DXVECTOR4(0, 1, 1, 1), "SharpEdgeDraw");

        DrawFont(L"�����ٶ󸶹ٻ�", D3DXVECTOR2(0, -1), D3DXVECTOR4(0, 0, 1, 1), "BorderDraw");

        DrawFont(L"�����ٶ󸶹ٻ�", D3DXVECTOR2(0, -2), D3DXVECTOR4(0, 0, 1, 1), "SharpEdgeWithGlowDraw");*/


        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //CreateFontTexture(tex, L"������", width, height, 512, 512);
        //SetGeometry(-7,0,-10, width, height);
        //SetupMatrices(-2, 0, 10);

        //

        
        SetGeometry(0, 0, 0, width / 3.0f, height / 3.0f);
        SetupMatrices2(0, 0, 50);

        g_pd3dDevice->SetTexture( 0, tex);
        g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE); 
        g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, /*D3DTOP_DISABLE*/D3DTOP_BLENDTEXTUREALPHA);


        //���� ���� ���
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        g_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        //g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

        ////���� �׽�Ʈ ���
        //g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        //g_pd3dDevice->SetRenderState(D3DRS_ALPHAREF, /*0x00000001*//*0x80*//*0x1*/0x80);
        //g_pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL/*D3DCMP_LESSEQUAL*/); // D3DCMP_GREATEREQUAL => �ȼ��� ���� ���� ALPHAREF�� ������ ������ ũ�ų� ������ Alpha�� 1���ؼ� ��� �Ѵ�.

        g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOMVERTEX ) );
        g_pd3dDevice->SetFVF( D3DFVF_CUSTOMVERTEX );
        g_pd3dDevice->SetIndices(g_pIB);

        //g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        //BorderDraw SoftEdgeDraw
        DrawFont(L"!ilililj1", D3DXVECTOR2(0, 0), D3DXVECTOR4(1, 1, 1, 1), "SoftEdgeDraw");
        DrawFont(L"!ilililj1", D3DXVECTOR2(0, -20), D3DXVECTOR4(1, 1, 1, 1), "SoftEdgeDraw");
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //g_pd3dDevice->SetTexture( 0, g_SDFTexture);
        //g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        //g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE); 
        //g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        ////g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

        ///*g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
        //g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        //g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        //g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_BLENDDIFFUSEALPHA );*/


        ////���� �׽�Ʈ ���
        ////g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        ////g_pd3dDevice->SetRenderState(D3DRS_ALPHAREF, /*0x00000001*/0x80/*0x1*/);
        ////g_pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL/*D3DCMP_LESSEQUAL*/); // D3DCMP_GREATEREQUAL => �ȼ��� ���� ���� ALPHAREF�� ������ ������ ũ�ų� ������ Alpha�� 1���ؼ� ��� �Ѵ�.

        ////���� ���� ���
        //g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        //g_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        //g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        //g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        //

        //// Render the vertex buffer contents
        //g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOMVERTEX ) );
        //g_pd3dDevice->SetFVF( D3DFVF_CUSTOMVERTEX );
        //g_pd3dDevice->SetIndices(g_pIB);
        ////g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 * 50 - 2 );


        ////BasicDraw
        ////SoftEdgeDraw
        ////SharpEdgeDraw
        ////SharpEdgeWithGlowDraw
        ////BorderDraw
        ////SoftEdge2Draw
        //g_lpEffect->SetTechnique("BorderDraw");

        //D3DXHANDLE handle;
        //handle = g_lpEffect->GetParameterByName(0, "_lowThreshold");
        //g_lpEffect->SetFloat(handle, 0.011);
        ////g_lpEffect->SetFloat(handle, 0.495);

        //handle = g_lpEffect->GetParameterByName(0, "_highThreshold");
        ////g_lpEffect->SetFloat(handle, 0.24);
        //g_lpEffect->SetFloat(handle, 0.44);
        //g_lpEffect->SetFloat(handle, 0.84);

        //handle = g_lpEffect->GetParameterByName(0, "_borderColor");
        //g_lpEffect->SetVector(handle, &D3DXVECTOR4(1, 0, 1, 1));

        //handle = g_lpEffect->GetParameterByName(0, "_baseColor");
        //g_lpEffect->SetVector(handle, &D3DXVECTOR4(1, 1, 1, 1));

        //handle = g_lpEffect->GetParameterByName(0, "_smoothing");
        //g_lpEffect->SetFloat(handle, 0.175);
        ////g_lpEffect->SetFloat(handle, 0.035);

        //handle = g_lpEffect->GetParameterByName(0, "_borderAlpha");
        //g_lpEffect->SetFloat(handle, 1);

        //UINT passCount;
        //g_lpEffect->Begin(&passCount, 0);
        //for (int pass = 0; pass < passCount; pass++)
        //{
        //    g_lpEffect->BeginPass(pass);
        //    g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
        //    g_lpEffect->EndPass();

        //}
        //g_lpEffect->End();
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        // End the scene
        g_pd3dDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}




//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------
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
                    testPoint.x += .1f;
                    int a = 0;
                }
                    break;

                case VK_RIGHT:
                {
                    testPoint.x -= .1f;
                    int a = 0;
                }
                    
                    break;


                case VK_UP:
                {
                    testPoint.y -= .1f;
                    int a = 0;
                }
                    break;

                case VK_DOWN:
                {
                    testPoint.y += .1f;
                    int a = 0;
                }
                    break;

                case 'Z':
                    testScale.x += 1;
                    testScale.y += 1;
                    break;

                case 'X':
                    testScale.x -= 1;
                    testScale.y -= 1;
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
                              WS_OVERLAPPEDWINDOW, 100, 100, 1500, 1500,
                              NULL, NULL, wc.hInstance, NULL );

    // Initialize Direct3D
    if( SUCCEEDED( InitD3D( hWnd ) ) )
    {
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




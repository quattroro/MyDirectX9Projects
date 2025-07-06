#pragma once
class Engine
{
public:
	~Engine();
	void Init(HWND hWnd, HINSTANCE hInst);
	HRESULT InitD3D(HWND hWnd);
	bool InitImGui(HWND hWnd);
	bool InitShader();

	void Render();
	void RenderUI();

	void Update();

	LPDIRECT3DTEXTURE9 CreateTextureFromDDS(const char* fileName, DWORD* pWidth, DWORD* pHeight);
	LPDIRECT3DTEXTURE9 CreateTextureFromPNG(char* fileName, DWORD* pWidth, DWORD* pHeight);
	void ResetDevice();
	LPDIRECT3DDEVICE9 GetDevice() { return m_pd3dDevice; }
	D3DPRESENT_PARAMETERS& GetD3Dpp() { return m_d3dpp; }
	D3DCAPS9& GetCaps() { return m_Caps; }
	HWND GethWnd() { return m_hWnd; }
	void GetCurClientRect(RECT& rt);
	LPD3DXEFFECT GetShader();
	HINSTANCE GethInst() { return m_hInst; }

	void ReleaseAllTexture();

private:

	LPDIRECT3D9				m_pD3D;
	LPDIRECT3DDEVICE9		m_pd3dDevice;
	D3DPRESENT_PARAMETERS	m_d3dpp;
	D3DCAPS9				m_Caps;
	HINSTANCE				m_hInst;
	class UIManager* m_UIManager;
	HWND					m_hWnd;
	LPD3DXEFFECT			m_ShaderEffect;
};

extern D3DXMATRIX matWorld;
extern D3DXMATRIX matView;
extern D3DXMATRIX matProj;
extern Engine* gEngine;

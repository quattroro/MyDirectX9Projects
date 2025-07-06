#pragma once


#define DF_PROTECT_KEYFUNC				10
#define DF_GUITAR_CONTROLER_PRODUCT_ID	0xe10404b4

class InputManager
{
public:
	bool Init();
	void Release();
	bool Update();

	bool UpdateMouse();
	bool UpdateKeyboard();

	bool KeyDown(BYTE key);
	bool KeyUp(BYTE key);
	bool KeyPress(BYTE key, int time = 0);

	bool MouseLDown();
	bool MouseLUp();
	bool MouseLPress();

	bool MouseRDown();
	bool MouseRUp();
	bool MouseRPress();
	int MouseWheel();
	void ClearMouseWheel();

	D3DXVECTOR2 GetMouseDownPos() { return m_MouseDownPos; }
	D3DXVECTOR2 GetMouseUpPos() { return m_MouseUpPos; }
	D3DXVECTOR2 GetMousePos() { return m_MousePos; }

private:
	LPDIRECTINPUT8 m_pDI;
	LPDIRECTINPUTDEVICE8 m_pkeyDevice;
	LPDIRECTINPUTDEVICE8 m_pMouseDevice;

	DIMOUSESTATE m_MouseState;
	DIMOUSESTATE m_OldMouseState;
	/*BYTE m_MouseState[3];
	BYTE m_OldMouseState[3];*/
	BYTE m_KeyState[256];
	BYTE m_OldKeyState[256];
	int m_KeyDownTime[256];

	D3DXVECTOR2 m_MouseDownPos;
	D3DXVECTOR2 m_MouseUpPos;
	D3DXVECTOR2 m_MousePos;
	int m_MouseWheel;
};

extern InputManager* g_InputManager;

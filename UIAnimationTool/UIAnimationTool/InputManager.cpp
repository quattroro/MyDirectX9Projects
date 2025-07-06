#include "stdafx.h"
#include "InputManager.h"
#include "Engine.h"
#include "UIAnimationTool.h"

InputManager* g_InputManager = new InputManager();

bool InputManager::Init()
{
	HRESULT hr = S_OK;
	if (FAILED(hr = DirectInput8Create(gEngine->GethInst(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&m_pDI, NULL)))
	{
		return false;
	}

	if (FAILED(hr = m_pDI->CreateDevice(GUID_SysKeyboard, &m_pkeyDevice, NULL)))
	{
		return false;
	}

	m_pkeyDevice->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(hr = m_pkeyDevice->SetCooperativeLevel(gEngine->GethWnd(), DISCL_NONEXCLUSIVE | DISCL_FOREGROUND | DISCL_NOWINKEY)))
	{
		return false;
	}

	while (m_pkeyDevice->Acquire() == DIERR_INPUTLOST);

	if (FAILED(hr = m_pDI->CreateDevice(GUID_SysMouse, &m_pMouseDevice, NULL)))
	{
		return false;
	}

	m_pMouseDevice->SetDataFormat(&c_dfDIMouse);

	if (FAILED(hr = m_pMouseDevice->SetCooperativeLevel(gEngine->GethWnd(), DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)))
	{
		return false;
	}

	while (m_pMouseDevice->Acquire() == DIERR_INPUTLOST);

	m_MouseWheel = 0;

	return true;
}

void InputManager::Release()
{
	if (m_pMouseDevice)
	{
		m_pMouseDevice->Unacquire();
		m_pMouseDevice->Release();
		m_pMouseDevice = NULL;
	}

	if (m_pkeyDevice)
	{
		m_pkeyDevice->Unacquire();
		m_pkeyDevice->Release();
		m_pkeyDevice = NULL;
	}

	if (m_pDI)
	{
		m_pDI->Release();
		m_pDI = NULL;
	}
}

bool InputManager::Update()
{
	if (!UpdateMouse())
		return false;

	if (!UpdateKeyboard())
		return false;



}

bool InputManager::UpdateMouse()
{
	HRESULT hr;

	if (m_pMouseDevice == NULL) return false;

	m_MousePos = mousePos;

	m_OldMouseState = m_MouseState;

	if (FAILED(hr = m_pMouseDevice->GetDeviceState(sizeof(DIMOUSESTATE), &m_MouseState)))
	{
		while (m_pMouseDevice->Acquire() == DIERR_INPUTLOST);
	}

	if ((m_MouseState.rgbButtons[0] != 0) && (m_OldMouseState.rgbButtons[0] == 0))
	{
		m_MouseDownPos = mousePos;
	}

	if ((m_MouseState.rgbButtons[0] == 0) && (m_OldMouseState.rgbButtons[0] != 0))
	{
		m_MouseUpPos = mousePos;
	}

	return true;
}

bool InputManager::UpdateKeyboard()
{
	HRESULT hr;
	if (m_pkeyDevice == NULL) return false;

	memcpy(m_OldKeyState, m_KeyState, 256 * sizeof(unsigned char));

	if (FAILED(hr = m_pkeyDevice->GetDeviceState(256, m_KeyState)))
	{
		while (m_pkeyDevice->Acquire() == DIERR_INPUTLOST);
	}

	return true;
}


bool InputManager::KeyDown(BYTE key)
{
	if (m_pkeyDevice == NULL)
		return FALSE;

	if ((m_KeyState[key] & 0x80) && !(m_OldKeyState[key] & 0x80))
		return TRUE;
	else
		return FALSE;
}

bool InputManager::KeyUp(BYTE key)
{
	if (!(m_KeyState[key] & 0x80) && (m_OldKeyState[key] & 0x80))
		return TRUE;
	else
		return FALSE;
}
bool InputManager::KeyPress(BYTE key, int time)
{
	int curTime = clock();
	if ((m_KeyState[key] & 0x80) && !(m_OldKeyState[key] & 0x80))
	{
		m_KeyDownTime[key] = curTime;
		return TRUE;
	}

	if (m_KeyState[key] & 0x80 && curTime >= m_KeyDownTime[key] + time)
		return TRUE;
	else
		return FALSE;
}

bool InputManager::MouseLDown()
{
	if (GetFocus() == NULL)
		return FALSE;

	return (m_MouseState.rgbButtons[0] != 0) && (m_OldMouseState.rgbButtons[0] == 0);
}
bool InputManager::MouseLUp()
{
	if (GetFocus() == NULL)
		return FALSE;

	return (m_MouseState.rgbButtons[0] == 0) && (m_OldMouseState.rgbButtons[0] != 0);
}
bool InputManager::MouseLPress()
{
	if (GetFocus() == NULL)
		return FALSE;

	return (m_MouseState.rgbButtons[0] != 0);
}

bool InputManager::MouseRDown()
{
	if (GetFocus() == NULL)
		return FALSE;

	return (m_MouseState.rgbButtons[1] != 0) && (m_OldMouseState.rgbButtons[1] == 0);
}
bool InputManager::MouseRUp()
{
	if (GetFocus() == NULL)
		return FALSE;

	return (m_MouseState.rgbButtons[1] == 0) && (m_OldMouseState.rgbButtons[1] != 0);
}
bool InputManager::MouseRPress()
{
	if (GetFocus() == NULL)
		return FALSE;

	return (m_MouseState.rgbButtons[1] != 0);
}


// Mouse 마우스휠 변위
int InputManager::MouseWheel()
{
	m_MouseWheel += m_MouseState.lZ * 0.05;
	return m_MouseWheel;
}

void InputManager::ClearMouseWheel()
{
	m_MouseWheel = 0;
}
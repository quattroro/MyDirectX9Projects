#include "pch.h"
#include "InputManager.h"

void InputManager::Init(HWND _hwnd)
{
	this->_hwnd = _hwnd;
}

void InputManager::SetMousePosition(D3DXVECTOR2 pos)
{
	CurMousePos = pos;
}

void InputManager::Update()
{
	/*HWND hwnd = ::GetActiveWindow();
	if (_hwnd != hwnd)
	{
		for (int key = 0; key < KEY_TYPE_COUNT; key++)
			keybuf[key] = KEY_STATE::NONE;

		return;
	}*/

	BYTE asciiKeys[KEY_TYPE_COUNT] = {};
	if (::GetKeyboardState(asciiKeys) == false)
		return;

	for (int key = 0; key < KEY_TYPE_COUNT; key++)
	{
		// 키가 눌려 있으면 true
		//if (::GetAsyncKeyState(key) & 0x8000)
		if (asciiKeys[key] & 0x80)
		{
			KEY_STATE& state = keybuf[key];

			// 이전 프레임에 키를 누른 상태라면 PRESS
			if (state == KEY_STATE::PRESS || state == KEY_STATE::DOWN)
				state = KEY_STATE::PRESS;
			else
				state = KEY_STATE::DOWN;
		}
		else
		{
			KEY_STATE& state = keybuf[key];

			// 이전 프레임에 키를 누른 상태라면 UP
			if (state == KEY_STATE::PRESS || state == KEY_STATE::DOWN)
				state = KEY_STATE::UP;
			else
				state = KEY_STATE::NONE;
		}
	}
}

//마우스 움직임 포착
void InputManager::InputUpdate()
{




	MouseAxis.x = CurMousePos.x - PreMousePos.x;
	MouseAxis.y = CurMousePos.y - PreMousePos.y;

	if (keybuf[(int)KEY_TYPE::W]==KEY_STATE::PRESS)
	{
		if (VerticalAxis < 1)
		{
			VerAxisVal += AxisAccel;
			VerticalAxis += VerAxisVal;
			if (VerticalAxis > 1)
			{
				VerticalAxis = 1;
				VerAxisVal = 0;
			}
		}
		
	}
	else if (keybuf[(int)KEY_TYPE::S] == KEY_STATE::PRESS)
	{
		if (VerticalAxis > -1)
		{
			VerAxisVal -= AxisAccel;
			VerticalAxis += VerAxisVal;
			if (VerticalAxis < -1)
			{
				VerticalAxis = -1;
				VerAxisVal = 0;
			}
		}
	}
	

	if (keybuf[(int)KEY_TYPE::D] == KEY_STATE::PRESS)
	{
		if (HorizentalAxis < 1)
		{
			HorAxisVal += AxisAccel;
			HorizentalAxis += HorAxisVal;
			if (HorizentalAxis > 1)
			{
				HorizentalAxis = 1;
				HorAxisVal = 0;
			}
		}
	}
	else if(keybuf[(int)KEY_TYPE::A] == KEY_STATE::PRESS)
	{
		if (HorizentalAxis > -1)
		{
			HorAxisVal -= AxisAccel;
			HorizentalAxis += HorAxisVal;
			if (HorizentalAxis < -1)
			{
				HorizentalAxis = -1;
				HorAxisVal = 0;
			}
		}
	}
	
	if (keybuf[(int)KEY_TYPE::W] != KEY_STATE::PRESS && keybuf[(int)KEY_TYPE::S] != KEY_STATE::PRESS)
	{
		if (VerticalAxis != 0.0f)
		{
			if (VerticalAxis >= 0)
			{
				VerAxisVal -= AxisAccel;
				VerticalAxis += VerAxisVal;
				if (VerticalAxis <= 0)
				{
					VerticalAxis = 0;
					VerAxisVal = 0;
				}
			}
			else
			{
				VerAxisVal += AxisAccel;
				VerticalAxis += VerAxisVal;
				if (VerticalAxis >= 0)
				{
					VerticalAxis = 0;
					VerAxisVal = 0;
				}
			}

			/*if (fabs(VerticalAxis) <= 0.01f)
			{
				VerAxisVal = 0;
				VerticalAxis = 0;
			}*/
		}
		
	}

	if (keybuf[(int)KEY_TYPE::D] != KEY_STATE::PRESS && keybuf[(int)KEY_TYPE::A] != KEY_STATE::PRESS)
	{
		if (HorizentalAxis != 0.0f)
		{
			if (HorizentalAxis >= 0)
			{
				HorAxisVal -= AxisAccel;
				HorizentalAxis += HorAxisVal;
				if (HorizentalAxis <= 0)
				{
					HorizentalAxis = 0;
					HorAxisVal = 0;
				}
			}
			else
			{
				HorAxisVal += AxisAccel;
				HorizentalAxis += HorAxisVal;
				if (HorizentalAxis >= 0)
				{
					HorizentalAxis = 0;
					HorAxisVal = 0;
				}
			}

		
		}
		
	}
}

void InputManager::KeyDown(char input)
{
	//keybuf[input] = true;
}

void InputManager::KeyUp(char input)
{
	//keybuf[input] = false;
	/*if (!keybuf['W'])
	{
		VerticalAxis = 0;
		VerAxisVal = 0;
	}
	if (!keybuf['S'])
	{
		VerticalAxis = 0;
		VerAxisVal = 0;
	}
	if (!keybuf['D'])
	{
		HorizentalAxis = 0;
		HorAxisVal = 0;
	}
	if (!keybuf['A'])
	{
		HorizentalAxis = 0;
		HorAxisVal = 0;
	}*/
}

//bool InputManager::GetKey(char code)
//{
//	return keybuf[code];
//}


bool InputManager::KeyIsDown(KEY_TYPE code)
{
	return (keybuf[(int)code] == KEY_STATE::DOWN);
}
bool InputManager::KeyIsUp(KEY_TYPE code)
{
	return (keybuf[(int)code] == KEY_STATE::UP);
}
bool InputManager::KeyIsPressed(KEY_TYPE code)
{
	return (keybuf[(int)code] == KEY_STATE::PRESS);
}

D3DXVECTOR2 InputManager::GetMouseAxis()
{
	return MouseAxis;
}
void InputManager::MouseButtonDown(char input)
{
	mousebuf[input] = true;
}
void InputManager::MouseButtonUp(char input)
{
	mousebuf[input] = false;
}

bool InputManager::GetMouseButton(char code)
{
	return mousebuf[code];
}
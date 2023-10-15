#pragma once
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "winmm.lib")
#include <d3dx9.h>
#include <d3d9.h>
#include <math.h>

#include "Singleton.h"


enum class KEY_TYPE
{
	UP = VK_UP,
	DOWN = VK_DOWN,
	LEFT = VK_LEFT,
	RIGHT = VK_RIGHT,

	W = 'W',
	A = 'A',
	S = 'S',
	D = 'D',

	Q = 'Q',
	E = 'E',

};

enum class KEY_STATE
{
	NONE,
	PRESS,
	DOWN,
	UP,
	END
};

enum
{
	KEY_TYPE_COUNT = 256,
	KEY_STATE_COUNT = KEY_STATE::END,
};


class InputManager : public Singleton<InputManager>
{
public:
	InputManager()
	{
		for (int i = 0; i < 256; i++)
		{
			keybuf[i] = KEY_STATE::NONE;
		}
		for (int i = 0;i<2;i++)
		{
			bool mousebuf[2];
		}
		D3DXVECTOR2 MouseAxis = D3DXVECTOR2(0.f, 0.f);
		float VerticalAxis = 0.f;
		float HorizentalAxis = 0.f;


		D3DXVECTOR2 CurMousePos = D3DXVECTOR2(0.f, 0.f);
		D3DXVECTOR2 PreMousePos = D3DXVECTOR2(0.f, 0.f);

	}
	void Init(HWND _hwnd);

	void SetMousePosition(D3DXVECTOR2 pos);

	void Update();

	void InputUpdate();
	void KeyDown(char input);
	void KeyUp(char input);

	void MouseButtonDown(char input);
	void MouseButtonUp(char input);

	bool GetMouseButton(char code);

	bool GetKey(char code);//키값을 입력하면 현재 해당 키가 눌려있는지를 확인해서 리턴 해준다.

	bool KeyIsDown(KEY_TYPE code);
	bool KeyIsUp(KEY_TYPE code);
	bool KeyIsPressed(KEY_TYPE code);

	D3DXVECTOR2 GetMouseAxis();//한번 업데이트가 호출 될때마다 현재 마우스 위치값을 받아와서 이전 프레임의 마우스 위치값과 비교해서 마우스의 움직임 값을 리턴 해준다.

	float GetHorizentalAxis()
	{
		return HorizentalAxis;
	}
	float GetVertivalAxis()
	{
		return VerticalAxis;
	}

public:
	//MoveAxis값이 최대값(-1,1)이 될때까지 걸릴 시간
	float AxisAccel = 0.01f;
	
private:

	float HorAxisVal = 0.f;
	float VerAxisVal = 0.f;

	D3DXVECTOR2 MouseAxis;
	float VerticalAxis = 0.f;
	float HorizentalAxis = 0.f;


	D3DXVECTOR2 CurMousePos;
	D3DXVECTOR2 PreMousePos;

	KEY_STATE keybuf[256];
	bool mousebuf[2];

	HWND _hwnd;
};


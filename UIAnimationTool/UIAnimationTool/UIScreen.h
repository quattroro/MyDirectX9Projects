#pragma once
#include "UIBase.h"
class GameObject;

class UIScreen : public UIBase
{
public:
	UIScreen(class UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix);
	~UIScreen();

	virtual void Render();
	virtual void Update();
	virtual void Input();

	D3DVIEWPORT9 GetViewport();

private:
	bool m_isSelect;
	D3DXVECTOR2 clickpos;
	GameObject* canvasObject;
	//bool m_PickedObj;
	//GameObject* m_Picked
};

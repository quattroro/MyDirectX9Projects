#pragma once
#include "UIBase.h"

enum TransformLock_Flags
{
	TransformLock_None = 0,
	TransformLock_Size = (1 << 0),
	TransformLock_Scale = (1 << 1),
	TransformLock_MAX,
};

class UITransform : public UIBase
{
public:
	UITransform(class UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix);
	~UITransform();

	virtual void Render();
	virtual void Update();
	virtual void Input();

	void SetTransform(GameObject* obj, Transform value);

private:
	bool m_ChangeName;
	class Transform* m_Transform;
	/*D3DXCOLOR*/D3DXVECTOR4 m_Color;
	int LockFlag;
	D3DXVECTOR2 SizeRatio;
	D3DXVECTOR2 ScaleRatio;
};

#pragma once

class GameObject;

class UIBase
{
public:
	UIBase(class UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix);
	~UIBase();

	virtual void Render() = 0;
	virtual void Update() = 0;
	virtual void Input() = 0;
	void UpdateWindowInfo();

	D3DXVECTOR2 GetPos() { return m_Pos; }
	D3DXVECTOR2 GetSize() { return m_Size; }
	void SetPos(D3DXVECTOR2 pos) { m_Pos = pos; }
	void SetSize(D3DXVECTOR2 size) { m_Size = size; }
	void SetFix(bool fix) { m_PosFix = fix; }
	bool GetFix() { return m_PosFix; }
	D3DXVECTOR2 GetUISize();
	D3DXVECTOR2 GetUIPos();
	D3DXVECTOR2 GetUIPosWorld();

	bool IsFocus() { return m_isFocus; }

	void UIBegin();
	void UIEnd();

	int GetType() { return m_type; }

protected:
	bool m_PosFix;
	bool m_isFocus;

	D3DXVECTOR2 m_Pos;
	D3DXVECTOR2 m_Size;

	int m_type;

	class UIManager* m_UIManager;
};

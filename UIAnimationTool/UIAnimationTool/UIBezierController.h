#pragma once
#include "UIBase.h"

struct ImVec2;

enum BezierPreset
{
	EASE_LINEAR,
	EASE_INSINE,
	EASE_OUTSINE,
	EASE_INOUTSINE,
	EASE_INQUAD,
	EASE_OUTQUAD,
	EASE_INOUTQUAD,
	EASE_INCUBIC,
	EASE_OUTCUBIC,
	EASE_INOUTCIBIC,
	EASE_INQUART,
	EASE_OUTQUART,
	EASE_INOUTQUART,
	EASE_INQUINT,
	EASE_OUTQUINT,
	EASE_INOUTQUINT,
	EASE_INEXPO,
	EASE_OUTEXPO,
	EASE_INOUTEXPO,
	EASE_INCIRC,
	EASE_OUTCIRC,
	EASE_INOUTCIRC,
	EASE_INBACK,
	EASE_OUTBACK,
	EASE_INOUTBACK,
	EASE_PRESET_MAX
};

class UIBezierController : public UIBase
{
public:
	UIBezierController(class UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix);
	~UIBezierController();

	virtual void Render();
	virtual void Update();
	virtual void Input();

	void ShowBezierWindow();
	int Bezier(const char* label, float P[4]);
	float BezierValue(float dt01, float P[4]);
	D3DXVECTOR2 GetB1() { return D3DXVECTOR2(Bezier_Vec[0], Bezier_Vec[1]); }
	D3DXVECTOR2 GetB2() { return D3DXVECTOR2(Bezier_Vec[2], Bezier_Vec[3]); }
	int GetBezierIndex() { return m_SelectItem; }

	void SetBezierValue(int ItemIndex, float bezier[4]);

public:
	template<int steps>
	void bezier_table(ImVec2 P[4], ImVec2 results[steps + 1]);

private:
	int m_SelectItem;
	float Bezier_Vec[4];
};

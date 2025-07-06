#pragma once

class UIBase;
class GameObject;
class Transform;

enum UI_TYPE
{
	SEQUENCER,
	TRANSFORM,
	HIERARKHY,
	SCREEN,
	BEZIERCONTROLLER,
	UI_TYPE_MAX
};


class UIManager
{
public:
	UIManager();
	~UIManager();

	void Add(UI_TYPE type, UIBase* ui);
	void Delete(UI_TYPE type);
	UIBase* GetUI(UI_TYPE type);
	UIBase* GetFocusedUI();

	void Render();
	void RenderMenuBar();
	void SaveData();
	void CreateRectangle();
	void CreateCircle();
	void Update();
	void Input();

	void AddObject(GameObject* obj, bool addHistory = true);

	void RenderStart();
	void RenderEnd();
	//const char* CheckObjectName(const char* name);
	void ClearObjectSelect();
	void SelectObject(GameObject* obj, bool clear = false);
	bool IsSelected(int instanceID);
	map<int, GameObject*> GetSelectedObject();

	void ClearAllObjects();
	void DeleteObject(GameObject* obj, bool addHistory = true);
	vector<GameObject*> GetObjectList() { return m_ObjectList; }

	GameObject* GetCopiedObj();
	SQ_Key* GetCopiedKey();

	int* GetCanvasSize();

public:
	vector<GameObject*> m_ObjectList;
	map<int, GameObject*> m_SelectObjectMap;
	GameObject* m_CopiedObj;
	SQ_Key* m_CopiedKey;

	int m_CanvasRatioType;
	int m_CanvasSize[2];
	float m_CanvasRatio;

private:
	map<UI_TYPE, UIBase*> m_UIMap;
};

extern UIManager* g_UIManager;

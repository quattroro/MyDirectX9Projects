#pragma once
#include "UIBase.h"


struct SQ_Group;
struct SQ_TimeLine;
struct AnimationInfo;
class GameObject;
//class TreeNode;

struct AnimationInfo
{
	AnimationInfo();
	AnimationInfo(float t, Transform value, int bIndex, D3DXVECTOR4 bezier);
	//AnimationInfo(AnimationInfo* value);
	AnimationInfo(const AnimationInfo& value);
	~AnimationInfo();

	float Time;
	Transform TransformValue;
	int bezierIndex;
	D3DXVECTOR2 B1;
	D3DXVECTOR2 B2;
};

struct SQ_Key
{
	SQ_Key();
	SQ_Key(int time, Transform value, int bIndex, D3DXVECTOR4 bezier);
	SQ_Key(int time, Transform value, int bIndex, D3DXVECTOR2 b1, D3DXVECTOR2 b2);
	SQ_Key(const SQ_Key& value);
	~SQ_Key();

	bool operator==(const SQ_Key& other);
	bool operator!=(const SQ_Key& other);

	int time;
	AnimationInfo aniValue;
	bool isSelected;
};

struct SQ_TimeLine
{
	SQ_TimeLine(GameObject* _obejct);
	~SQ_TimeLine();

	void Render();
	void DeleteKey();
	void DeleteKey(int index);
	//void DeleteKey(SQ_Key key);
	void AddKey(SQ_Key* key, int count);

	vector<SQ_Key> GetKeys() { return keys; }
	SQ_Key GetPreKey(float time);
	SQ_Key GetNextKey(float time);
	SQ_Key* GetKey(int time);
	SQ_Key* GetKey(int time, int* out_index);
	SQ_Key* GetKey_Index(int index);

	vector<SQ_Key> GetSelectedKey();


	GameObject* object;
	vector<SQ_Key> keys;
};

enum TIMELINE_TYPE
{
	TL_GROUP,
	TL_POSITION,
	TL_SCALE,
	TL_ROTATION,
	TL_OPACITY,
};

struct SQ_Group
{
	SQ_Group(GameObject* _obejct);
	~SQ_Group();

	void Render();

	void AddKey(int tl_type, SQ_Key* key, int count);
	void AddTimeLine(int tl_type);
	SQ_TimeLine* GetTimeLine(int tl_type);

	bool Open;
	GameObject* object; // 데이터 원본

	//map<string, SQ_TimeLine*> m_TimelineMap;
	map<int, SQ_TimeLine*> m_TimelineMap;
};

struct KeyActionData
{
	int ObjectID;
	int TimelineType;
	int KeyIndex;
	SQ_Key key;
};



//struct ObjectActionData
//{
//	GameObject* object;
//	TreeNode<GameObject*>* node;
//	SQ_Group* Group;
//};


class UISequencer : public UIBase
{
public:
	UISequencer(class UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix);
	~UISequencer();

	void Init(int startFrame, int endFrame);
	virtual void Render();
	virtual void Update();
	virtual void Input();

	void AddKey(GameObject* object, int tl_type, SQ_Key* key, int count);
	void AddKey(int instanceID, int tl_type, SQ_Key* key, int count);
	void AddTimeLine(GameObject* object, int tl_type);
	void AddObject(GameObject* object);
	SQ_Key* GetKey(GameObject* object, int tl_type, int index);
	SQ_Key* GetKey(int instanceID, int tl_type, int index);
	SQ_Group* GetGroup(GameObject* object);
	SQ_Group* GetGroup(int instanceID);
	Transform CalcTransform(float time, SQ_Key* preKey, SQ_Key* nextKey);
	D3DXVECTOR2 CalcBezier(D3DXVECTOR2 B1, D3DXVECTOR2 B2, float t);

	SQ_Group* GetSelectedGroup();
	SQ_Key* GetSelectedKey();

	float GetEndFrame() { return m_endFrame; }
	void SetEndFrame(float frame);

	void DeleteGroup(GameObject* object);
	//void DeleteKey(SQ_Key* key);

	SQ_Key* GetCurrentKey();
	SQ_Key* GetCurrentKey(int* index); // None Pointer
	SQ_Key* SetCurrentKey(GameObject* obj);
	void SetCurrentKey(GameObject* obj, SQ_Key keydata);

	void ClearAllObjects();

private:
	bool m_Play;
	bool doDelete;

	int m_PlayTime;
	int m_LastFrame;

	int32_t m_currentFrame;
	int32_t m_startFrame;
	int32_t m_endFrame;
	map<int, SQ_Group*> m_SqMap;
};

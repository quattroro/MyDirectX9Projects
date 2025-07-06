#pragma once

enum ACTION_TYPE
{
	ACTION_CREATE_OBJECT,
	ACTION_DELETE_OBJECT,
	ACTION_CREATE_KEY,
	ACTION_DELETE_KEY,
	ACTION_CHANGE_TRANSFORM,
	ACTION_CHANGE_TREENODE,
	ACTION_DELETE_GROUP,
};

enum COPY_TYPE
{
	COPY_OBJECT,
	COPY_KEY,
};

struct ObjectActionData
{

};

struct PayloadData
{
	PayloadData()
	{
		_type = 0;
		_data = nullptr;
	}

	~PayloadData()
	{
	}

	int _type;
	int _size;
	void* _data;
};

class ObjectManager
{
public:
	ObjectManager();
	~ObjectManager();

	void PushAction(void* data, int size, ACTION_TYPE type);
	PayloadData* PopAction();
	PayloadData* TopAction();
	void ClearAction();

	void Copy(void* data, int size, COPY_TYPE type);
	PayloadData* Paste();

	vector<int> m_HistoryList;

private:
	stack<PayloadData*> m_History;
	PayloadData* m_CopiedData;
};

extern ObjectManager* g_ObjectManager;

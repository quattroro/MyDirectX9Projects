#include "stdafx.h"
#include "ObjectManager.h"

ObjectManager* g_ObjectManager = new ObjectManager;

ObjectManager::ObjectManager()
{
	m_CopiedData = nullptr;
}

ObjectManager::~ObjectManager()
{
	while (!m_History.empty())
	{
		delete m_History.top()->_data;
		delete m_History.top();
		m_History.pop();
	}

	if (m_CopiedData != nullptr)
		delete m_CopiedData;

	m_HistoryList.clear();
}

void ObjectManager::ClearAction()
{
	while (!m_History.empty())
	{
		delete m_History.top()->_data;
		delete m_History.top();
		m_History.pop();
	}
	m_HistoryList.clear();
}

void ObjectManager::PushAction(void* data, int size, ACTION_TYPE type)
{
	if (m_History.size() >= 50)
	{
		/*if (m_History.top()->_type == ACTION_DELETE_OBJECT)
		{
			delete m_History.top();
			m_History.pop();
			m_HistoryList.erase(m_HistoryList.end() - 1);
		}

		delete m_History.top();
		m_History.pop();
		m_HistoryList.erase(m_HistoryList.end() - 1);*/
		ClearAction();
	}

	PayloadData* action = new PayloadData();
	action->_data = new char[size];
	memcpy(action->_data, data, size);
	action->_type = type;
	action->_size = size;

	m_HistoryList.push_back(type);

	m_History.push(action);
}

PayloadData* ObjectManager::PopAction()
{
	PayloadData* data = nullptr;

	if (!m_History.empty())
	{
		data = m_History.top();
		m_History.pop();
		m_HistoryList.erase(m_HistoryList.end() - 1);
	}

	return data;
}

PayloadData* ObjectManager::TopAction()
{
	PayloadData* data = nullptr;

	if (!m_History.empty())
	{
		data = m_History.top();
	}

	return data;
}

void ObjectManager::Copy(void* data, int size, COPY_TYPE type)
{
	PayloadData* action = new PayloadData();
	action->_data = new char[size];
	memcpy(action->_data, data, size);
	action->_type = type;
	action->_size = size;

	if (m_CopiedData != nullptr)
		delete m_CopiedData;

	m_CopiedData = action;
}

PayloadData* ObjectManager::Paste()
{
	return m_CopiedData;
}
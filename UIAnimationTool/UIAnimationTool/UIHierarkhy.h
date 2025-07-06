#pragma once
#include "UIBase.h"
#include "Tree.h"

//typedef TreeNode<class GameObject*>* LPTREENODE;
//typedef Tree<class GameObject*>* LPTREE;
class GameObject;

class UIHierarkhy : public UIBase
{
public:
	UIHierarkhy(class UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix);
	~UIHierarkhy();

	virtual void Render();
	virtual void Update();
	virtual void Input();

	void AddNode(GameObject* object);
	void RenderTree(TreeNode<GameObject*>* node);
	void ClearTreeNodeClick();

	void ClearAllNodes();

	void DeleteNode(GameObject* object);

	//vector<TreeNode<class GameObject*>*> GetClickedNode();
	vector<TreeNode<class GameObject*>*> GetAllNodes();

	Tree<GameObject*>* GetTreeRoot();

private:
	int m_Clicked;
	bool m_Dragging;

	TreeNode<GameObject*>* m_DraggingNode;

	TreeNode<GameObject*>* m_ClickedNode;
	//vector<TreeNode<class GameObject*>*> m_ClickedNodes;
	Tree<GameObject*>* m_Tree;
};

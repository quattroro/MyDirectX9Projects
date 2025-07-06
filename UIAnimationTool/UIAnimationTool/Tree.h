#pragma once

template <typename T>
class TreeNode
{
public:
	TreeNode(T data)
	{
		_data = data;
		_parent = nullptr;
		_isClicked = false;
		_depth = 0;
	}

	TreeNode(TreeNode<T>* parent, T data)
	{
		_data = data;
		_parent = parent;
		_isClicked = false;
		_depth = 0;
	}

	void AddChild(T data)
	{
		TreeNode<T>* child = new TreeNode<T>(data);
		_child.push_back(child);
		child->SetParent(this);
		child->SetDepth(_depth + 1);
	}

	void AddChild(TreeNode<T>* child)
	{
		_child.push_back(child);
		child->SetParent(this);
		child->SetDepth(_depth + 1);
	}

	TreeNode<T>* FindNode(T data)
	{
		TreeNode<T>* node = nullptr;

		if (_data == data)
			return this;

		for (TreeNode<T>* a : _child)
		{
			node = a->FindNode(data);
		}

		return node;
	}

	//자식 모드들 중에서 해당하는 노드를 찾아서 삭제한다.
	bool DeleteNode(T data)
	{
		bool result = false;
		for (int i = 0; i < _child.size(); i++)
		{
			if (_child[i]->GetData() == data)
			{
				_child[i]->Clear();
				_child.erase(_child.begin() + i);
				result = true;
				break;
			}
		}

		return result;
	}

	bool DeleteNode(TreeNode<T>* node)
	{

	}

	void Clear()
	{
		for (int i = _child.size() - 1; i >= 0; i--)
		{
			_child[i]->Clear();
			delete _child[i];
		}

		/*for (TreeNode<T>* a : _child)
		{
			a->Clear();
			delete a
		}*/
		_child.clear();

		//delete _data;
	}

	vector<TreeNode<T>*>& GetChild()
	{
		return _child;
	}

	bool operator == (TreeNode<T>* t)
	{
		return _data == t->GetData();
	}

	T GetData()
	{
		return _data;
	}

	bool IsClicked()
	{
		return _isClicked;
	}

	void SetClicked(bool value)
	{
		_isClicked = value;
	}

	void SetParent(TreeNode<T>* parent)
	{
		_parent = parent;
	}

	TreeNode<T>* GetParent()
	{
		return _parent;
	}

	void SetDepth(int depth)
	{
		_depth = depth;
	}

	TreeNode<T>* SearchNode(int InstanceID)
	{
		if (*_data == InstanceID)
			return this;

		for (TreeNode<T>* a : _child)
		{
			TreeNode<T>* temp = a->SearchNode(InstanceID);

			if (temp != nullptr)
				return temp;
		}
		return nullptr;
	}

private:
	TreeNode<T>* _parent;
	vector<TreeNode<T>*> _child;
	T _data;
	bool _isClicked;
	int _depth;
};


template <typename T>
class Tree
{
public:
	Tree(T value)
	{
		_root = new TreeNode<T>(value);
	}

	TreeNode<T>* GetRoot()
	{
		return _root;
	}

	bool DeleteNode(T data)
	{
		return _root->DeleteNode(data);
	}

	bool DeleteNode(TreeNode<T>* node)
	{
		//return _root->DeleteNode(data);
	}

	vector<TreeNode<T>*> GetAllNode()
	{
		vector<TreeNode<T>*> result;
		queue<TreeNode<T>*> queue;
		queue.push(_root);
		while (queue.size())
		{
			TreeNode<T>* temp = queue.front(); queue.pop();
			result.push_back(temp);

			if (temp->GetChild().size())
			{
				for (TreeNode<T>* c : temp->GetChild())
					queue.push(c);
			}
		}
		return result;
	}

	TreeNode<T>* SearchNode(int InstanceID)
	{
		return _root->SearchNode(InstanceID);
	}

	void MoveNode(TreeNode<T>* destNode, TreeNode<T>* targetNode)
	{
		//같은 부모를 가진 노드들 끼리만 이동 가능
		//destNode 의 앞으로 targetNode를 이동시켜준다.
		if (destNode->GetParent() == targetNode->GetParent())
		{
			TreeNode<T>* parent = destNode->GetParent();
			TreeNode<T>* leaf = new TreeNode<T>(targetNode->GetData());
			leaf->SetParent(parent);

			vector<TreeNode<T>*>& nodes = parent->GetChild();
			nodes.erase(remove(nodes.begin(), nodes.end(), targetNode), nodes.end());

			nodes.insert(find(nodes.begin(), nodes.end(), destNode), leaf);
		}
	}

	void Clear()
	{
		_root->Clear();
	}

private:
	TreeNode<T>* _root;
};

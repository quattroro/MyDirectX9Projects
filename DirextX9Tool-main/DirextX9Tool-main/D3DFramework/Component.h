#pragma once
#include "Object.h"

class GameObject;
class Transform;



enum class COMPONENT_TYPE
{
	TRANSFORM,
	MESH_RENDERER,
	CAMERA,
	// ...
	MONO_BEHAVIOUR,
	END,
};


enum
{
	FIXED_COMPONENT_COUNT = (int)(COMPONENT_TYPE::END) - 1
};


class Component:public Object
{
public:
	Component(COMPONENT_TYPE);
	virtual ~Component();

public:
	virtual void Awake() {}
	virtual void Start() {}
	virtual void Update() {}
	virtual void LateUpdate() {}
	virtual void FinalUpdate() {}

	GameObject* GetGameObject();
	Transform* GetTransform();

public:
	COMPONENT_TYPE GetType() { return _type; }
	void SetGameObject(GameObject* gameObject) { _gameObject = gameObject; }


private:
	COMPONENT_TYPE _type;
	GameObject* _gameObject;//게임 오브젝트와 서로서로 가지고 있는다 필요하면 사용
};


#include "pch.h"
#include "Component.h"
#include "GameObject.h"
#include "Transform.h"

Component::Component(COMPONENT_TYPE type) :Object(OBJECT_TYPE::COMPONENT), _type(type)
{

}

Component::~Component()
{
}

GameObject* Component::GetGameObject()
{
	return _gameObject;
}

Transform* Component::GetTransform()
{
	return dynamic_cast<Transform*>(_gameObject->GetComponent(COMPONENT_TYPE::TRANSFORM));
}

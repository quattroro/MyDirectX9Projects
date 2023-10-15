#include "pch.h"
#include "GameObject.h"
#include "MonoBehaviour.h"
#include "Transform.h"
#include "MeshRenderer.h"
#include "Camera.h"


GameObject::GameObject() :Object(OBJECT_TYPE::GAMEOBJECT)
{
}

GameObject::~GameObject()
{
}

void GameObject::Awake()
{
	for (Component* component : _components)
	{
		if (component)
			component->Awake();
	}

	for (MonoBehaviour* mono : _scripts)
	{
		if (mono)
			mono->Awake();
	}

}

void GameObject::Start()
{
	for (Component* component : _components)
	{
		if (component)
			component->Start();
	}

	for (MonoBehaviour* mono : _scripts)
	{
		if (mono)
			mono->Start();
	}
}

void GameObject::Update()
{
	for (Component* component : _components)
	{
		if (component)
			component->Update();
	}

	for (MonoBehaviour* mono : _scripts)
	{
		if (mono)
			mono->Update();
	}

}

void GameObject::LateUpdate()
{
	for (Component* component : _components)
	{
		if (component)
			component->LateUpdate();
	}

	for (MonoBehaviour* mono : _scripts)
	{
		if (mono)
			mono->LateUpdate();
	}
}

void GameObject::FinalUpdate()
{
	for (Component* component : _components)
	{
		if (component)
			component->FinalUpdate();
	}
}

Component* GameObject::GetComponent(COMPONENT_TYPE type)
{
	
	return _components[(int)type];
}

void GameObject::AddComponent(Component* component)
{
	component->SetGameObject(this);

	COMPONENT_TYPE type = component->GetType();
	if (type != COMPONENT_TYPE::MONO_BEHAVIOUR)
	{
		_components.insert(_components.begin() + (int)type, component);
		//_components[type] = component;
		//_components. [type] = component;
	}
	else
	{
		//�ٿ� ĳ��Ʈ dymanic_cast �� üũ ���� MonoBehaciour�� �ƴϸ� null
		_scripts.push_back(dynamic_cast<MonoBehaviour*>(component));
	}

}

Transform* GameObject::GetTransform()
{
	Component* component = GetComponent(COMPONENT_TYPE::TRANSFORM);
	return dynamic_cast<Transform*>(component);
}

MeshRenderer* GameObject::GetMeshRenderer()
{
	Component* component = GetComponent(COMPONENT_TYPE::MESH_RENDERER);
	return dynamic_cast<MeshRenderer*>(component);
}

Camera* GameObject::GetCamera()
{
	Component* component = GetComponent(COMPONENT_TYPE::CAMERA);
	return dynamic_cast<Camera*>(component);
}

//Light* GameObject::GetLight()
//{
//	Component* component = GetComponent(COMPONENT_TYPE::TRANSFORM);
//	return dynamic_cast<Light*>(component);
//}

#pragma region  Transform���� ����
////�̵��� �Ҷ��� ������ ���� ���ӿ�����Ʈ�� ������ ���� �̵��� ��Ų��.
//void GameObject::Translation(float x, float y, float z)
//{
//	D3DXMATRIXA16 transmat;
//
//	//D3DXMatrixTranslation(&transmat, x, y, z);
//
//	//D3DXVec3TransformCoord(&pos, &pos, &transmat);
//
//	this->pos.x += x;
//	this->pos.y += y;
//	this->pos.z += z;
//
//	//��ȭ�� ���� ����� ������ �ִ´�.
//	D3DXMatrixTranslation(&TransMat, pos.x, pos.y, pos.z);
//
//	if (child != nullptr)
//	{
//		child->Translation(x, y, z);
//	}
//
//}
//
////void GameObject::Rotation(float x, float y, float z)
////{
////	D3DXMATRIXA16 tempmat;
////	
////
////
////}
//
////�� ȸ���� �Ҷ��� ������ ��ü�� �������� ���� ȸ���� �����ְ� �ڽ��� ������ ��ü�� �������� ���� ��ü�� ȸ������ �ڽ��� ������ġ�� �̿��ؼ� �ڽ��� ��ġ�� ���� �Ѵ�.
////�������� 
////��, ��
//void GameObject::PitchRotation(float fAngle)
//{
//	D3DXMATRIXA16 Xrotmat;
//
//	rot.x += fAngle;
//
//	D3DXMatrixRotationAxis(&Xrotmat, &Axis[E_Axis_Right], fAngle * 3.14 / 180);
//
//	D3DXVec3TransformNormal(&Axis[E_Axis_Forward], &Axis[E_Axis_Forward], &Xrotmat);
//	D3DXVec3TransformNormal(&Axis[E_Axis_Up], &Axis[E_Axis_Up], &Xrotmat);
//
//	//��ȭ�� ���� ����� ������ �ִ´�.
//	D3DXMatrixRotationAxis(&XRotMat, &Axis[E_Axis_Right], rot.x * 3.14 / 180);
//
//	/*if (child != nullptr)
//	{
//		child->PitchRotation(fAngle);
//	}*/
//}
//
////��������
////��, ����
//void GameObject::YawRotation(float fAngle)
//{
//	D3DXMATRIXA16 Yrotmat;
//
//	rot.y += fAngle;
//
//	D3DXMatrixRotationAxis(&Yrotmat, &Axis[E_Axis_Up], fAngle * 3.14 / 180);
//
//	D3DXVec3TransformNormal(&Axis[E_Axis_Forward], &Axis[E_Axis_Forward], &Yrotmat);
//	D3DXVec3TransformNormal(&Axis[E_Axis_Right], &Axis[E_Axis_Right], &Yrotmat);
//
//	D3DXMatrixRotationAxis(&YRotMat, &Axis[E_Axis_Up], rot.y * 3.14 / 180);
//
//	/*if (child != nullptr)
//	{
//		child->YawRotation(fAngle);
//	}*/
//}
//
////�����
////��, ����
//void GameObject::RollRotation(float fAngle)
//{
//	D3DXMATRIXA16 Zrotmat;
//
//	rot.z += fAngle;
//
//	D3DXMatrixRotationAxis(&Zrotmat, &Axis[E_Axis_Forward], fAngle * 3.14 / 180);
//
//	D3DXVec3TransformNormal(&Axis[E_Axis_Up], &Axis[E_Axis_Up], &Zrotmat);
//	D3DXVec3TransformNormal(&Axis[E_Axis_Right], &Axis[E_Axis_Right], &Zrotmat);
//
//	D3DXMatrixRotationAxis(&ZRotMat, &Axis[E_Axis_Forward], rot.z * 3.14 / 180);
//
//	/*if (child != nullptr)
//	{
//		child->RollRotation(fAngle);
//	}*/
//}
//
////��������
//void GameObject::QPitchRotation(float fAngle)
//{
//	//D3DXQUATERNION qRot;
//	//D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Right], fAngle);
//	//rotX = rotX * qRot;//���ʹϾ� �� ����
//
//	//D3DXMATRIXA16 matRot;
//	//D3DXMatrixRotationQuaternion(&matRot, &qRot);
//
//	//D3DXVec3TransformNormal(&Axis[E_Axis_Forward], &Axis[E_Axis_Forward], &matRot);
//	//D3DXVec3TransformNormal(&Axis[E_Axis_Up], &Axis[E_Axis_Up], &matRot);
//
//	////��ȭ�� ���� ����� ������ �ִ´�.
//	//D3DXMatrixRotationAxis(&XRotMat, &Axis[E_Axis_Right], rot.x * 3.14 / 180);
//
//	D3DXQUATERNION qRot;
//	D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Right], fAngle * 3.14 / 180);
//	//rotX = rotX * qRot;//���ʹϾ� �� ����
//	rot = rot * qRot;//���ʹϾ� �� ����
//
//	D3DXMATRIXA16 matRot;
//	D3DXMatrixRotationQuaternion(&matRot, &qRot);
//
//	D3DXVec3TransformNormal(&Axis[E_Axis_Forward], &Axis[E_Axis_Forward], &matRot);
//	D3DXVec3TransformNormal(&Axis[E_Axis_Up], &Axis[E_Axis_Up], &matRot);
//
//	//��ȭ�� ���� ����� ������ �ִ´�.
//	//D3DXMatrixRotationQuaternion(&XRotMat, &rotX);
//}
////��������
//void GameObject::QYawRotation(float fAngle)
//{
//	D3DXQUATERNION qRot;
//	D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Up], fAngle * 3.14 / 180);
//	//rotY = rotY * qRot;//���ʹϾ� �� ����
//	rot = rot * qRot;
//
//	D3DXMATRIXA16 matRot;
//	D3DXMatrixRotationQuaternion(&matRot, &qRot);
//
//	D3DXVec3TransformNormal(&Axis[E_Axis_Forward], &Axis[E_Axis_Forward], &matRot);
//	D3DXVec3TransformNormal(&Axis[E_Axis_Right], &Axis[E_Axis_Right], &matRot);
//
//	//��ȭ�� ���� ����� ������ �ִ´�.
//	//D3DXMatrixRotationQuaternion(&YRotMat, &rotY);
//}
////�����
//void GameObject::QRollRotation(float fAngle)
//{
//	D3DXQUATERNION qRot;
//	D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Forward], fAngle * 3.14 / 180);
//	//rotZ = rotZ * qRot;//���ʹϾ� �� ����
//	rot = rot * qRot;
//
//	D3DXMATRIXA16 matRot;
//	D3DXMatrixRotationQuaternion(&matRot, &qRot);
//
//	D3DXVec3TransformNormal(&Axis[E_Axis_Up], &Axis[E_Axis_Up], &matRot);
//	D3DXVec3TransformNormal(&Axis[E_Axis_Right], &Axis[E_Axis_Right], &matRot);
//
//	//��ȭ�� ���� ����� ������ �ִ´�.
//	//D3DXMatrixRotationQuaternion(&ZRotMat, &rotZ);
//}
//
////void GameObject::Render()
////{
////	/*D3DXMATRIXA16 RotMat;
////	D3DXMATRIXA16 TransMat;
////	D3DXMATRIXA16 ScaleMat;
////	D3DXMATRIXA16 TempMat;
////	D3DXMATRIXA16 Xrotmat;
////	D3DXMATRIXA16 Yrotmat;
////	D3DXMATRIXA16 Zrotmat;
////
////	D3DXMatrixIdentity(&TempMat);
////
////	D3DXMatrixTranslation(&TransMat, pos.x, pos.y, pos.z);
////	D3DXMatrixMultiply(&TempMat, &TempMat, &TransMat);
////
////	D3DXMatrixRotationAxis(&Xrotmat, &Axis[E_Axis_Right], rot.x);
////	D3DXMatrixRotationAxis(&Yrotmat, &Axis[E_Axis_Up], rot.y);
////	D3DXMatrixRotationAxis(&Zrotmat, &Axis[E_Axis_Forward], rot.z);
////	D3DXMatrixMultiply(&TempMat, &TempMat, &Xrotmat);
////	D3DXMatrixMultiply(&TempMat, &TempMat, &Yrotmat);
////	D3DXMatrixMultiply(&TempMat, &TempMat, &Zrotmat);
////
////	D3DXMatrixScaling(&ScaleMat, scale.x, scale.y, scale.z);*/
////
////
////}
//
//
//void GameObject::Scaling(float x, float y, float z)
//{
//	this->scale.x = x;
//	this->scale.y = y;
//	this->scale.z = z;
//
//	D3DXMatrixScaling(&ScaleMat, scale.x, scale.y, scale.z);
//
//	/*if (child != nullptr)
//	{
//		child->Scaling(x, y, z);
//	}*/
//
//}
//
//float GameObject::getDegree(D3DXVECTOR3 p1, D3DXVECTOR3 p2)
//{
//	float angle = acos(D3DXVec3Dot(&p1, &p2));
//	return angle * 180.0f / 3.14f;
//}
//
////D3DXMATRIXA16 GameObject::SetMatrix()
////{
////	D3DXMATRIXA16 TempMat;
////
////	D3DXMATRIXA16 RotMat;
////	D3DXMATRIXA16 TransMat;
////	D3DXMATRIXA16 ScaleMat;
////	
////	D3DXMATRIXA16 Xrotmat;
////	D3DXMATRIXA16 Yrotmat;
////	D3DXMATRIXA16 Zrotmat;
////
////	D3DXMatrixIdentity(&TempMat);
////	//���� rotation���� ���� ����ȸ���� �ϰ�
////	D3DXMatrixRotationAxis(&Xrotmat, &Axis[E_Axis_Right], rot.x * 3.14 / 180);
////	D3DXMatrixRotationAxis(&Yrotmat, &Axis[E_Axis_Up], rot.y * 3.14 / 180);
////	D3DXMatrixRotationAxis(&Zrotmat, &Axis[E_Axis_Forward], rot.z * 3.14 / 180);
////
////	D3DXMatrixMultiply(&TempMat, &TempMat, &Xrotmat);
////	D3DXMatrixMultiply(&TempMat, &TempMat, &Yrotmat);
////	D3DXMatrixMultiply(&TempMat, &TempMat, &Zrotmat);
////	
////	
////	D3DXMatrixScaling(&ScaleMat, scale.x, scale.y, scale.z);
////	D3DXMatrixMultiply(&TempMat, &TempMat, &ScaleMat);
////
////	D3DXMatrixTranslation(&TransMat, pos.x, pos.y, pos.z);
////	D3DXMatrixMultiply(&TempMat, &TempMat, &TransMat);
////	
////
////	return TempMat;
////
////	//m_pd3dDevice->SetTransform(D3DTS_WORLD, &matworld);
////
////}
//
//D3DXMATRIXA16 GameObject::GetFinalMat()
//{
//	D3DXMATRIXA16 TempMat;
//	D3DXMatrixIdentity(&TempMat);
//
//	//���� rotation���� ���� ����ȸ���� �ϰ�
//	D3DXMatrixMultiply(&TempMat, &TempMat, &XRotMat);
//	D3DXMatrixMultiply(&TempMat, &TempMat, &YRotMat);
//	D3DXMatrixMultiply(&TempMat, &TempMat, &ZRotMat);
//
//	//�����ϸ�
//	D3DXMatrixMultiply(&TempMat, &TempMat, &ScaleMat);
//
//	D3DXMatrixMultiply(&TempMat, &TempMat, &TransMat);
//
//	return TempMat;
//}
//
//D3DXMATRIXA16 GameObject::SetMatrix()
//{
//	D3DXMATRIXA16 TempMat;
//
//	//D3DXMATRIXA16 RotMat;
//	D3DXMATRIXA16 localTransMat;
//	//D3DXMATRIXA16 ScaleMat;
//
//	D3DXMATRIXA16 rotateMat;
//	D3DXMATRIXA16 parentmat;
//	/*D3DXMATRIXA16 Xrotmat;
//	D3DXMATRIXA16 Yrotmat;
//	D3DXMATRIXA16 Zrotmat;*/
//
//	D3DXMatrixIdentity(&TempMat);
//	
//	/*D3DXMatrixRotationAxis(&Xrotmat, &Axis[E_Axis_Right], rot.x * 3.14 / 180);
//	D3DXMatrixRotationAxis(&Yrotmat, &Axis[E_Axis_Up], rot.y * 3.14 / 180);
//	D3DXMatrixRotationAxis(&Zrotmat, &Axis[E_Axis_Forward], rot.z * 3.14 / 180);*/
//
//	D3DXMatrixRotationQuaternion(&rotateMat, &rot);
//
//	D3DXMatrixMultiply(&TempMat, &TempMat, &rotateMat);
//	//���� rotation���� ���� ����ȸ���� �ϰ�
//	//D3DXMatrixMultiply(&TempMat, &TempMat, &XRotMat);
//	//D3DXMatrixMultiply(&TempMat, &TempMat, &YRotMat);
//	//D3DXMatrixMultiply(&TempMat, &TempMat, &ZRotMat);
//	
//	//�����ϸ�
//	D3DXMatrixMultiply(&TempMat, &TempMat, &ScaleMat);
//
//	
//
//	//�ڽ��� ������ ��ü�� �������� ���� ��ü�� ȸ������ �ڽ��� ������ġ�� �̿��ؼ� �ڽ��� ��ġ�� ���� �Ѵ�.
//	if (parent != nullptr)
//	{
//		//���� ������ǥ�� ���� �̵��� ���ش�.
//		D3DXMatrixTranslation(&localTransMat, localpos.x, localpos.y, localpos.z);
//		D3DXMatrixMultiply(&TempMat, &TempMat, &localTransMat);
//
//		/*if (this->rot.y != 0)
//		{
//			int a = 0;
//		}*/
//
//		//�ڽ��� ȸ���� ������ġ��ŭ ��ȯ�� ���� �� ������ 
//		//���� ��ü�� ���� ����� �޾ƿͼ�
//		parentmat = parent->SetMatrix();
//		//�����ش�.
//		D3DXMatrixMultiply(&TempMat, &TempMat, &parentmat);
//	}
//	else//������ ��ü�� ��������
//	{
//		D3DXMatrixMultiply(&TempMat, &TempMat, &TransMat);
//	}
//
//	/*D3DXMatrixScaling(&ScaleMat, scale.x, scale.y, scale.z);
//	D3DXMatrixMultiply(&TempMat, &TempMat, &ScaleMat);*/
//
//	//D3DXMatrixTranslation(&TransMat, pos.x, pos.y, pos.z);
//	//D3DXMatrixMultiply(&TempMat, &TempMat, &TransMat);
//
//
//	return TempMat;
//
//	//m_pd3dDevice->SetTransform(D3DTS_WORLD, &matworld);
//
//}
//
//
//////���� ��ü���� ȸ���� �Ͼ������
//////���� ��ü�� ȸ������ �ڽ��� ������ġ�� �̿��ؼ� �ڽ��� ��ġ�� ���� �Ѵ�.
////void GameObject::SetChildWorldPos()
////{
////	D3DXVECTOR3 parentrot = parent->rot;
////	D3DXVECTOR3 temp; 
////
////
////	D3DXMATRIXA16 Xrotmat;
////	D3DXMATRIXA16 Yrotmat;
////	D3DXMATRIXA16 Zrotmat;
////
////
////
////
////}
//
////�� ����� ������ ������ ���� ���ؼ� ȸ������ �ش�.
//void GameObject::LookAt(D3DXVECTOR3 point)
//{
//	D3DXVECTOR3 temp;
//	D3DXVec3Normalize(&temp, &point);
//
//	float angle = acos(D3DXVec3Dot(&Axis[E_Axis_Forward], &temp));
//
//	QYawRotation(angle * 180.0f / 3.14f);
//}
//
////
//void GameObject::LookAt3D(D3DXVECTOR3 point)
//{
//	D3DXVECTOR3 curpos = Axis[E_Axis_Forward];
//	D3DXVECTOR3 dest = point - this->pos;
//
//	curpos.y = 0;
//	dest.y = 0;
//
//	D3DXVec3Normalize(&dest, &dest);
//	D3DXVec3Normalize(&curpos, &curpos);
//
//
//	float xangle = acos(D3DXVec3Dot(&curpos, &dest));
//	YawRotation(xangle * 180.0f / 3.14f);
//
//
//	curpos = Axis[E_Axis_Forward];
//	dest = point - this->pos;
//
//	curpos.x = 0;
//	dest.x = 0;
//
//	D3DXVec3Normalize(&dest, &dest);
//	D3DXVec3Normalize(&curpos, &curpos);
//
//
//	float yangle = acos(D3DXVec3Dot(&curpos, &dest));
//	PitchRotation(yangle * 180.0f / 3.14f);
//}

#pragma endregion






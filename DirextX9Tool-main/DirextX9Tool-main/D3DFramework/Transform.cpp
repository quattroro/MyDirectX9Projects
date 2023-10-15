#include "pch.h"
#include "Transform.h"

Transform::Transform():Component(COMPONENT_TYPE::TRANSFORM)
{


}

Transform::~Transform()
{
}

void Transform::FinalUpdate()
{
	Matrix matScale;
	Matrix matTranslation;
	Matrix matRotation;
	
	D3DXMatrixScaling(&matScale, _localScale.x, _localScale.y, _localScale.z);
	D3DXMatrixTranslation(&matTranslation, _localPotision.x, _localPotision.y, _localPotision.z);
	D3DXMatrixRotationQuaternion(&matRotation, &_localRotation);

	_matLocal = matScale * matRotation * matTranslation;
	_matWorld = _matLocal;

	Transform* parent = GetParent();
	if (parent != nullptr)
	{
		//���� ��ȯ�� ���� �ϰ� �θ� ���� ��쿡�� ���Ŀ� �θ��� ��ȯ�� ���� ���ش�.
		_matWorld *= parent->GetLocalToWorldMatrix();
	}
}

void Transform::PushData()
{
	////�����͸� ���������� GPU�� ������
	//Matrix matWVP = _matWorld * Camera::S_MatView * Camera::S_MatProjection;
	//CONST_BUFFER(CONSTANT_BUFFER_TYPE::TRANSFORM)->PushData(&matWVP, sizeof(matWVP));
}


void Transform::PitchRotation(float fAngle)
{
	D3DXQUATERNION qRot;
	D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Right], fAngle * 3.14 / 180);
	_localRotation = _localRotation * qRot;//���ʹϾ� �� ����

	D3DXMATRIXA16 matRot;
	D3DXMatrixRotationQuaternion(&matRot, &qRot);

	D3DXVec3TransformNormal(&Axis[E_Axis_Forward], &Axis[E_Axis_Forward], &matRot);
	D3DXVec3TransformNormal(&Axis[E_Axis_Up], &Axis[E_Axis_Up], &matRot);

	//��ȭ�� ���� ����� ������ �ִ´�.
	//D3DXMatrixRotationQuaternion(&XRotMat, &_rotX);
}

void Transform::YawRotation(float fAngle)
{
	D3DXQUATERNION qRot;
	D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Up], fAngle * 3.14 / 180);
	_localRotation = _localRotation * qRot;//���ʹϾ� �� ����

	D3DXMATRIXA16 matRot;
	D3DXMatrixRotationQuaternion(&matRot, &qRot);

	D3DXVec3TransformNormal(&Axis[E_Axis_Forward], &Axis[E_Axis_Forward], &matRot);
	D3DXVec3TransformNormal(&Axis[E_Axis_Right], &Axis[E_Axis_Right], &matRot);

	//��ȭ�� ���� ����� ������ �ִ´�.
	//D3DXMatrixRotationQuaternion(&YRotMat, &_rotY);
}

void Transform::RollRotation(float fAngle)
{
	D3DXQUATERNION qRot;
	D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Forward], fAngle * 3.14 / 180);
	_localRotation = _localRotation * qRot;//���ʹϾ� �� ����

	D3DXMATRIXA16 matRot;
	D3DXMatrixRotationQuaternion(&matRot, &qRot);

	D3DXVec3TransformNormal(&Axis[E_Axis_Up], &Axis[E_Axis_Up], &matRot);
	D3DXVec3TransformNormal(&Axis[E_Axis_Right], &Axis[E_Axis_Right], &matRot);

	//��ȭ�� ���� ����� ������ �ִ´�.
	//D3DXMatrixRotationQuaternion(&ZRotMat, &_rotZ);
}

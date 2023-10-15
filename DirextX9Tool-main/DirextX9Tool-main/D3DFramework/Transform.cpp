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
		//개인 변환을 먼저 하고 부모가 있을 경우에는 이후에 부모의 변환을 따라서 해준다.
		_matWorld *= parent->GetLocalToWorldMatrix();
	}
}

void Transform::PushData()
{
	////데이터를 최종적으로 GPU에 보내줌
	//Matrix matWVP = _matWorld * Camera::S_MatView * Camera::S_MatProjection;
	//CONST_BUFFER(CONSTANT_BUFFER_TYPE::TRANSFORM)->PushData(&matWVP, sizeof(matWVP));
}


void Transform::PitchRotation(float fAngle)
{
	D3DXQUATERNION qRot;
	D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Right], fAngle * 3.14 / 180);
	_localRotation = _localRotation * qRot;//쿼터니언 값 적용

	D3DXMATRIXA16 matRot;
	D3DXMatrixRotationQuaternion(&matRot, &qRot);

	D3DXVec3TransformNormal(&Axis[E_Axis_Forward], &Axis[E_Axis_Forward], &matRot);
	D3DXVec3TransformNormal(&Axis[E_Axis_Up], &Axis[E_Axis_Up], &matRot);

	//변화된 최종 행렬을 가지고 있는다.
	//D3DXMatrixRotationQuaternion(&XRotMat, &_rotX);
}

void Transform::YawRotation(float fAngle)
{
	D3DXQUATERNION qRot;
	D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Up], fAngle * 3.14 / 180);
	_localRotation = _localRotation * qRot;//쿼터니언 값 적용

	D3DXMATRIXA16 matRot;
	D3DXMatrixRotationQuaternion(&matRot, &qRot);

	D3DXVec3TransformNormal(&Axis[E_Axis_Forward], &Axis[E_Axis_Forward], &matRot);
	D3DXVec3TransformNormal(&Axis[E_Axis_Right], &Axis[E_Axis_Right], &matRot);

	//변화된 최종 행렬을 가지고 있는다.
	//D3DXMatrixRotationQuaternion(&YRotMat, &_rotY);
}

void Transform::RollRotation(float fAngle)
{
	D3DXQUATERNION qRot;
	D3DXQuaternionRotationAxis(&qRot, &Axis[E_Axis_Forward], fAngle * 3.14 / 180);
	_localRotation = _localRotation * qRot;//쿼터니언 값 적용

	D3DXMATRIXA16 matRot;
	D3DXMatrixRotationQuaternion(&matRot, &qRot);

	D3DXVec3TransformNormal(&Axis[E_Axis_Up], &Axis[E_Axis_Up], &matRot);
	D3DXVec3TransformNormal(&Axis[E_Axis_Right], &Axis[E_Axis_Right], &matRot);

	//변화된 최종 행렬을 가지고 있는다.
	//D3DXMatrixRotationQuaternion(&ZRotMat, &_rotZ);
}

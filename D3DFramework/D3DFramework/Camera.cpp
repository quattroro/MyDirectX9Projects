#include "Camera.h"
#include "Utility.h"

Camera::Camera(LPDIRECT3DDEVICE9 m_pd3dDevice)
{
	this->m_pd3dDevice = m_pd3dDevice;
	this->vEyePt = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	this->vLookatPt = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	this->vUpVec = D3DXVECTOR3(0.0f, 0.0f, 0.0f);

	this->First_vEyePt = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	this->First_vLookatPt = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	this->First_vUpVec = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
}

Camera::Camera(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 vEyePt, D3DXVECTOR3 vLookatPt, D3DXVECTOR3 vUpVec)
{

	this->m_pd3dDevice = m_pd3dDevice;
	/*this->vEyePt = vEyePt;
	this->vLookatPt = vLookatPt;
	this->vUpVec = vUpVec;

	this->First_vEyePt = vEyePt;
	this->First_vLookatPt = vLookatPt;
	this->First_vUpVec = vUpVec;*/

	this->Axis[E_Axis_Up] = vUpVec;
	this->pos = vEyePt;
	D3DXVECTOR3 temp = vLookatPt - this->pos;
	D3DXVec3Normalize(&temp, &temp);
	this->Axis[E_Axis_Forward] = temp;

	D3DXVec3Cross(&this->Axis[E_Axis_Right], &this->Axis[E_Axis_Up], &this->Axis[E_Axis_Forward]);
	/*vUpVec = this->Axis[E_Axis_Up];
	 = ;*/
	//vLookatPt = (this->pos + this->Axis[E_Axis_Forward]) * LookDistance;


	/*this->pos = vEyePt;
	LookAt3D(vLookatPt);*/

}


void Camera::Init(D3DXVECTOR3 vEyePt, D3DXVECTOR3 vLookatPt, D3DXVECTOR3 vUpVec)
{
	this->vEyePt = vEyePt;
	this->vLookatPt = vLookatPt;
	this->vUpVec = vUpVec;

	this->First_vEyePt = vEyePt;
	this->First_vLookatPt = vLookatPt;
	this->First_vUpVec = vUpVec;
}

////위치와 lookat이 같이 움직여야 한다.
//void Camera::Translation(float x, float y, float z)
//{
//
//	direction = D3DXVECTOR3(x, y, z);
//	vEyePt += direction;
//	vLookatPt += direction;
//}

////끄덕끄덕
//void Camera::PitchRotation(float fAngle)
//{
//	D3DXMATRIXA16 tempmat;
//	D3DXMATRIXA16 transmat;
//	D3DXMATRIXA16 rotmat;
//	D3DXMatrixIdentity(&tempmat);
//
//	if (movemode == 0)
//	{
//		direction = vEyePt - vLookatPt;//카메라를 원점으로 이동시킨 다음에 회전값을 구한다.
//
//		D3DXMatrixTranslation(&transmat, direction.x, direction.y, direction.z);
//
//		D3DXMatrixRotationZ(&rotmat, (fAngle * 0.1) * 3.14 / 180);
//
//		D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);
//		D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);
//		D3DXMatrixTranslation(&transmat, -direction.x, -direction.y, -direction.z);
//
//		D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);
//
//
//		D3DXVec3TransformCoord(&vLookatPt, &vLookatPt, &tempmat);
//	}
//	else if (movemode == 1)
//	{
//		D3DXMatrixRotationY(&rotmat, (fAngle * 0.1) * 3.14 / 180);
//		D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);
//
//		D3DXVec3TransformCoord(&vEyePt, &vEyePt, &tempmat);
//		D3DXVec3TransformCoord(&vUpVec, &vUpVec, &tempmat);
//	}
//	else if (movemode == 2)//카메라가 객체에 붙어있을때
//	{
//
//		//부모의 위치를 기준으로 회전
//		if (parent != nullptr)
//		{
//
//		}
//	}
//}
////도리도리
//void Camera::YawRotation(float fAngle)
//{
//	D3DXMATRIXA16 tempmat;
//	D3DXMATRIXA16 transmat;
//	D3DXMATRIXA16 rotmat;
//	D3DXMatrixIdentity(&tempmat);
//
//	if (movemode == 0)
//	{
//		direction = vEyePt - vLookatPt;//카메라를 원점으로 이동시킨 다음에 회전값을 구한다.
//
//		D3DXMatrixTranslation(&transmat, direction.x, direction.y, direction.z);
//		D3DXMatrixRotationY(&rotmat, (fAngle * 0.1) * 3.14 / 180);
//
//		D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);
//		D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);
//		D3DXMatrixTranslation(&transmat, -direction.x, -direction.y, -direction.z);
//		D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);
//
//
//		D3DXVec3TransformCoord(&vLookatPt, &vLookatPt, &tempmat);
//	}
//	else if (movemode == 1)
//	{
//		direction = vLookatPt - vEyePt;
//		D3DXMatrixRotationX(&rotmat, (fAngle * 0.1) * 3.14 / 180);
//		D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);
//
//		D3DXVec3TransformCoord(&vEyePt, &vEyePt, &tempmat);
//
//		//D3DXVec3Cross()
//	}
//	else if (movemode == 2)
//	{
//		if (parent != nullptr)
//		{
//			D3DXVECTOR3 temp = parent->GetPosition() * -1;
//			D3DXMatrixTranslation(&transmat, temp.x, temp.y, temp.z);
//			D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);
//			D3DXMatrixRotationY(&rotmat, (fAngle * 0.1) * 3.14 / 180 * -1);
//			D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);
//			temp = parent->GetPosition();
//			D3DXMatrixTranslation(&transmat, temp.x, temp.y, temp.z);
//			D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);
//
//			D3DXMatrixRotationY(&rotmat, (fAngle * 0.1) * 3.14 / 180);
//			D3DXVec3TransformCoord(&vEyePt, &vEyePt, &tempmat);
//			D3DXVec3TransformCoord(&vLookatPt, &vLookatPt, &rotmat);
//
//
//		}
//	}
//}



//끄덕끄덕
void Camera::PitchRotation(float fAngle)
{
	GameObject::PitchRotation(fAngle);

	//D3DXMATRIXA16 tempmat;
	//D3DXMATRIXA16 transmat;
	//D3DXMATRIXA16 rotmat;
	//D3DXMatrixIdentity(&tempmat);

	//if (movemode == 0)
	//{
	//	direction = vEyePt - vLookatPt;//카메라를 원점으로 이동시킨 다음에 회전값을 구한다.

	//	D3DXMatrixTranslation(&transmat, direction.x, direction.y, direction.z);

	//	D3DXMatrixRotationZ(&rotmat, (fAngle * 0.1) * 3.14 / 180);

	//	D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);
	//	D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);
	//	D3DXMatrixTranslation(&transmat, -direction.x, -direction.y, -direction.z);

	//	D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);


	//	D3DXVec3TransformCoord(&vLookatPt, &vLookatPt, &tempmat);
	//}
	//else if (movemode == 1)
	//{
	//	D3DXMatrixRotationY(&rotmat, (fAngle * 0.1) * 3.14 / 180);
	//	D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);

	//	D3DXVec3TransformCoord(&vEyePt, &vEyePt, &tempmat);
	//	D3DXVec3TransformCoord(&vUpVec, &vUpVec, &tempmat);
	//}
	//else if (movemode == 2)//카메라가 객체에 붙어있을때
	//{

	//}
}

//도리도리
void Camera::YawRotation(float fAngle)
{
	GameObject::YawRotation(fAngle);

	//D3DXMATRIXA16 tempmat;
	//D3DXMATRIXA16 transmat;
	//D3DXMATRIXA16 rotmat;
	//D3DXMatrixIdentity(&tempmat);

	//if (movemode == 0)
	//{
	//	direction = vEyePt - vLookatPt;//카메라를 원점으로 이동시킨 다음에 회전값을 구한다.

	//	D3DXMatrixTranslation(&transmat, direction.x, direction.y, direction.z);
	//	D3DXMatrixRotationY(&rotmat, (fAngle * 0.1) * 3.14 / 180);

	//	D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);
	//	D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);
	//	D3DXMatrixTranslation(&transmat, -direction.x, -direction.y, -direction.z);
	//	D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);


	//	D3DXVec3TransformCoord(&vLookatPt, &vLookatPt, &tempmat);
	//}
	//else if (movemode == 1)
	//{
	//	direction = vLookatPt - vEyePt;
	//	D3DXMatrixRotationX(&rotmat, (fAngle * 0.1) * 3.14 / 180);
	//	D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);

	//	D3DXVec3TransformCoord(&vEyePt, &vEyePt, &tempmat);

	//	//D3DXVec3Cross()
	//}
	//else if (movemode == 2)
	//{

	//}
}

//갸우뚱
void Camera::RollRotation(float fAngle)
{
	GameObject::RollRotation(fAngle);

	//if (movemode == 0)
	//{
	//	direction = vEyePt - vLookatPt;//카메라를 원점으로 이동시킨 다음에 회전값을 구한다.

	//	D3DXMatrixTranslation(&transmat, direction.x, direction.y, direction.z);
	//	D3DXMatrixRotationX(&rotmat, (fAngle * 0.1) * 3.14 / 180);

	//	D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);
	//	D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);
	//	D3DXMatrixTranslation(&transmat, -direction.x, -direction.y, -direction.z);
	//	D3DXMatrixMultiply(&tempmat, &tempmat, &transmat);


	//	D3DXVec3TransformCoord(&vLookatPt, &vLookatPt, &tempmat);
	//}
	//else if (movemode == 1)
	//{
	//	direction = vLookatPt - vEyePt;
	//	D3DXMatrixRotationZ(&rotmat, (fAngle * 0.1) * 3.14 / 180);
	//	D3DXMatrixMultiply(&tempmat, &tempmat, &rotmat);

	//	D3DXVec3TransformCoord(&vEyePt, &vEyePt, &tempmat);

	//	//D3DXVec3Cross()
	//}
	//else if (movemode == 2)
	//{

	//}

}

//카메라 뿐만 아니라 시점도 같이 움직인다.
void Camera::RightMove()
{
	D3DXVECTOR3 temp = Axis[E_Axis_Right] * movespeed * DeltaTime;
	Translation(temp.x, temp.y, temp.z);

	/*direction = vLookatPt - vEyePt;
	D3DXVECTOR3 cross;
	D3DXVECTOR3 upvec = D3DXVECTOR3(0.0f, 0.1f, 0.0f);
	D3DXVec3Cross(&cross, &direction, &upvec);
	D3DXVec3Normalize(&cross, &cross);
	vEyePt += cross * movespeed;
	vLookatPt += cross * movespeed;*/

}

//카메라 뿐만 아니라 시점도 같이 움직인다.
void Camera::LeftMove()
{
	D3DXVECTOR3 temp = Axis[E_Axis_Right] * movespeed * DeltaTime * -1;
	Translation(temp.x, temp.y, temp.z);

	/*direction = vLookatPt - vEyePt;
	D3DXVECTOR3 cross;
	D3DXVECTOR3 upvec = D3DXVECTOR3(0.0f, 0.1f, 0.0f);
	D3DXVec3Cross(&cross, &direction, &upvec);
	D3DXVec3Normalize(&cross, &cross);
	vEyePt -= cross * movespeed;
	vLookatPt -= cross * movespeed;*/

}

//카메라 뿐만 아니라 시점도 같이 움직인다.
void Camera::FowardMove()
{
	D3DXVECTOR3 temp = Axis[E_Axis_Forward] * movespeed * DeltaTime;
	Translation(temp.x, temp.y, temp.z);

	/*direction = vLookatPt - vEyePt;
	D3DXVec3Normalize(&direction, &direction);
	vEyePt += direction * movespeed;
	vLookatPt += direction * movespeed;*/

}

//카메라 뿐만 아니라 시점도 같이 움직인다.
void Camera::BackMove()
{
	D3DXVECTOR3 temp = Axis[E_Axis_Forward] * movespeed * DeltaTime * -1;
	Translation(temp.x, temp.y, temp.z);

	/*direction = vLookatPt - vEyePt;
	D3DXVec3Normalize(&direction, &direction);
	vEyePt -= direction * movespeed;
	vLookatPt -= direction * movespeed;*/

}


void Camera::Render()
{
	vUpVec = this->Axis[E_Axis_Up];
	vEyePt = this->pos;
	vLookatPt = this->pos + this->Axis[E_Axis_Forward] * LookDistance;

	D3DXMATRIXA16 matView;
	D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);
	m_pd3dDevice->SetTransform(D3DTS_VIEW, &matView);

	D3DXMATRIXA16 matProj;
	D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f);
	m_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
}

//해당 방향을 바라보도록 
void Camera::LookAt(D3DXVECTOR3 pos)
{
	//현재 바라보고있는 방향과의 각도차이를 구해서 돌려준다.
	//두 벡터를 정규화하고 두 벡터의 내적을 구하면 해당 값은 cos@ 값이 된다. 해당값을 
	vLookatPt = pos;

}

void Camera::SetMoveMode(int mode)
{
	this->movemode = mode;
	vEyePt = First_vEyePt;
	vLookatPt = First_vLookatPt;
	vUpVec = First_vUpVec;
}
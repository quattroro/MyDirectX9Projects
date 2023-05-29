#pragma once
#include <Windows.h>
#include <mmsystem.h>
#include <d3dx9.h>
#include <strsafe.h>


class GameObject
{
public:
	enum E_Axis
	{
		E_Axis_Up,
		E_Axis_Right,
		E_Axis_Forward,
		E_Axis_Max
	};

	GameObject(D3DXVECTOR3 pos)
	{
		parent = nullptr;
		child = nullptr;

		Axis[E_Axis_Up] = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
		Axis[E_Axis_Right] = D3DXVECTOR3(1.0f, 0.0f, 0.0f);
		Axis[E_Axis_Forward] = D3DXVECTOR3(0.0f, 0.0f, 1.0f);
		localpos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		//월드상의 위치
		this->pos = pos;
		//현재 회전값
		this->rot = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		//현재 스케일값
		this->scale = D3DXVECTOR3(1.0f, 1.0f, 1.0f);

		D3DXMatrixIdentity(&FinalMat);
		D3DXMatrixIdentity(&XRotMat);
		D3DXMatrixIdentity(&YRotMat);
		D3DXMatrixIdentity(&ZRotMat);
		D3DXMatrixIdentity(&TransMat);
		D3DXMatrixIdentity(&ScaleMat);
		
	}

	GameObject()
	{
		parent = nullptr;
		child = nullptr;

		Axis[E_Axis_Up] = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
		Axis[E_Axis_Right] = D3DXVECTOR3(1.0f, 0.0f, 0.0f);
		Axis[E_Axis_Forward] = D3DXVECTOR3(0.0f, 0.0f, 1.0f);
		localpos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		//월드상의 위치
		this->pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		//현재 회전값
		this->rot = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		//현재 스케일값
		this->scale = D3DXVECTOR3(1.0f, 1.0f, 1.0f);

		D3DXMatrixIdentity(&FinalMat);
		D3DXMatrixIdentity(&XRotMat);
		D3DXMatrixIdentity(&YRotMat);
		D3DXMatrixIdentity(&ZRotMat);
		D3DXMatrixIdentity(&TransMat);
		D3DXMatrixIdentity(&ScaleMat);

	}

	void SetParent(GameObject* parent, D3DXVECTOR3 localpos)
	{
		this->parent = parent;
		this->localpos = localpos;
		parent->child = this;

	}

	/*void SetChild(GameObject* child)
	{
		this->child = child;
		child->parent = this;
	}*/

	GameObject* GetParent()
	{
		return parent;
	}

	GameObject* GetChild()
	{
		return child;
	}

	D3DXVECTOR3 GetPosition()
	{

		return pos;
	}

	D3DXVECTOR3 GetRotation()
	{
		return rot;
	}

	D3DXVECTOR3 GetScale()
	{
		return scale;
	}

	virtual D3DXMATRIXA16 SetMatrix();

	virtual D3DXMATRIXA16 GetFinalMat();

	void LookAt(D3DXVECTOR3 point);

	void LookAt3D(D3DXVECTOR3 point);

	float getDegree(D3DXVECTOR3 p1, D3DXVECTOR3 p2);

	virtual void Translation(float x, float y, float z);

	virtual void Scaling(float x, float y, float z);

	//끄덕끄덕
	virtual void PitchRotation(float fAngle);
	//도리도리
	virtual void YawRotation(float fAngle);
	//갸우뚱
	virtual void RollRotation(float fAngle);

	

	//위치값과 회전값에 따라 그려준다.
	virtual void Render() = 0;

private:


protected:
	//월드상의 위치
	D3DXVECTOR3 pos;
	//현재 회전값 degree
	D3DXVECTOR3 rot;
	//현재 스케일값
	D3DXVECTOR3 scale;
	//바로 상위에 있는 객체
	GameObject* parent;
	//바로 하위에 있는 객체
	GameObject* child;
	//로컬위치
	D3DXVECTOR3 localpos;
	//3방향 축
	D3DXVECTOR3 Axis[E_Axis_Max];

	//test
	D3DXMATRIXA16 FinalMat;
	D3DXMATRIXA16 XRotMat;
	D3DXMATRIXA16 YRotMat;
	D3DXMATRIXA16 ZRotMat;
	D3DXMATRIXA16 TransMat;
	D3DXMATRIXA16 ScaleMat;
};


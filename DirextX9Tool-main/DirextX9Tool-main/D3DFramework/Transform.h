#pragma once
#include "pch.h"
#include "Component.h"

enum E_Axis
{
	E_Axis_Up,
	E_Axis_Right,
	E_Axis_Forward,
	E_Axis_Max
};

class Transform:public Component
{
public:
	Transform();
	virtual ~Transform();

	virtual void FinalUpdate() override;
	void PushData();

public:

	/*virtual Matrix SetMatrix();

	virtual Matrix GetFinalMat();

	void LookAt(Vector3 point);

	void LookAt3D(Vector3 point);

	virtual void Translation(float x, float y, float z);

	virtual void Scaling(float x, float y, float z);*/


	const Matrix& GetLocalToWorldMatrix() { return _matWorld; }

	Transform* GetParent() { return _parent; }

	//²ô´ö²ô´ö
	virtual void PitchRotation(float fAngle);
	//µµ¸®µµ¸®
	virtual void YawRotation(float fAngle);
	//°¼¿ì¶×
	virtual void RollRotation(float fAngle);

	//SetLocalPosition()


private:
	Vector3 _localPotision = { 0.0f, 0.0f, 0.0f };
	Quaternion _localRotation = { 0.0f, 0.0f, 0.0f ,0.0f };
	Vector3 _localScale = { 1.f,1.f,1.f };

	/*Quaternion _rotX = {};
	Quaternion _rotY = {};
	Quaternion _rotZ = {};*/

	Matrix _matLocal = {};
	Matrix _matWorld = {};

	Transform* _parent;

	Vector3 Axis[E_Axis_Max] = { Vector3(0.0f, 1.0f, 0.0f),Vector3(1.0f, 0.0f, 0.0f),Vector3(0.0f, 0.0f, 1.0f) };
};


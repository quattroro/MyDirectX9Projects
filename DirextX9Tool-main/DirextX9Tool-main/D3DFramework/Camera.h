#pragma once
#include "GameObject.h"

enum class PROJECTION_TYPE
{
	PERSPECTIVE,
	ORTHOGRAPHIC,
};


class Camera: public Component
{
public:
	Camera();

	virtual ~Camera();

	virtual void FinalUpdate() override;

	void Render();

	//void Init(D3DXVECTOR3 vEyePt, D3DXVECTOR3 vLookatPt, D3DXVECTOR3 vUpVec);

	//위치와 lookat이 같이 움직여야 한다.
	//virtual void Translation(float x, float y, float z);

	////끄덕끄덕
	//virtual void PitchRotation(float fAngle);
	////도리도리
	//virtual void YawRotation(float fAngle);
	

	//도리도리
	//void RotHor(float ydegree);

	//끄덕끄덕
	//void RotVir(float xdegree);


	////끄덕끄덕
	//virtual void PitchRotation(float fAngle);
	////도리도리
	//virtual void YawRotation(float fAngle);
	////갸우뚱
	//virtual void RollRotation(float fAngle);

	//위치와 lookat이 같이 움직여야 한다.
	/*void Translation(float x, float y, float z)
	{
		direction = D3DXVECTOR3(x, y, z);
		vEyePt += direction;
		vLookatPt += direction;
	}*/

	//void RightMove();

	//void LeftMove();

	//void FowardMove();

	//void BackMove();


	//void Render();

	////해당 방향을 바라보도록 
	//void LookAt(D3DXVECTOR3 pos);

	//void SetMoveMode(int mode);

	Matrix& GetViewMatrix() { return _matView; }
	Matrix& GetProjectionMatrix() { return _matProjection; }

private:
	//기본은 원근투영
	PROJECTION_TYPE _type = PROJECTION_TYPE::PERSPECTIVE;

	float _near = 1.0f;
	float _far = 1000.0f;
	float _fov = PI / 4.0f;
	float _scale = 1.0f;

	Matrix _matView = {};
	Matrix _matProjection = {};

	D3DXVECTOR3 vEyePt;
	D3DXVECTOR3 vLookatPt;
	D3DXVECTOR3 vUpVec;
//public:
//	// TEMP
//	static Matrix S_MatView;
//	static Matrix S_MatProjection;
};


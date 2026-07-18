#pragma once
#include "Utility.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>

#if !_USE_DirectXMath_
enum class ColliderType
{
	Sphere, // 구체
	AABB,   // 축 정렬 경계 상자(Axis-Aligned Bounding Box)
	OBB,    // 객체 정렬 경계 상자(Object-Oriented Bounding Box)
	CAPSULE,// 캡슐
	COLLIDER_MAX,
};

// 콜리전 채널 각 채널에 따라 필터를 설정해줄때 사용 모든 Collider들의 기본 채널은 GENERAL이다.
enum class Collision_Channel
{
	GENERAL,
	STATIC,
	PLAYER,
	CAMERA,
	Collision_Channel_Max,
};

// 모든 Collision Channel들의 기본 Filter는 Block이다.
enum class Collision_Filter
{
	C_BLOCK,
	C_OVERLAP,
	C_IGNORE,
	C_REVERSE,
};

class Ray
{
public:
	D3DXVECTOR3 dir;
	D3DXVECTOR3 pos;
};

struct Interval3D
{
	float min;
	float max;
};

struct CrashPoint
{
	CrashPoint()
	{
		Point = D3DXVECTOR3(0, 0, 0);
		Normal = D3DXVECTOR3(0, 0, 0);
	}
	D3DXVECTOR3 Point;
	D3DXVECTOR3 Normal;
};

class BoundingBox;
class BoundingOrientedBox;
class BoundingSphere;
class BoundingCapsule;
#endif



class BaseCollider
{
public:
	BaseCollider();
	BaseCollider(ColliderType type);
	virtual ~BaseCollider();

	virtual void Update() = 0;
	virtual void Render() = 0;
	virtual void TransForm(D3DXMATRIX& transMat); // 스랜스폼이 변경될때 한번만 실행
	virtual bool Intersects(Ray& ray, OUT float& distance) = 0; //레이와의 교차 여부
	virtual bool Intersects(BaseCollider* other) = 0; //다른 Collider와의 교차 여부
	//Ray
	virtual bool DXIntersects(Ray& ray, OUT float& distance, CrashPoint* crashPoint = NULL) = 0; //레이와의 교차 여부
	virtual bool DXIntersects(BaseCollider* other, CrashPoint* crashPoint = NULL) = 0; //다른 Collider와의 교차 여부

	virtual void RenderLine(D3DXVECTOR3* pos1, D3DXVECTOR3* pos2, D3DXCOLOR col);

	ColliderType GetColliderType() { return m_CollType; }

	void SetCollisionChannel(Collision_Channel channel) { m_CollChannel = channel; }
	void SetCollisionFilter(Collision_Channel channel, Collision_Filter filter) { m_CollFilter[(int)channel] = (int)filter; }

	Collision_Channel GetCollisionChannel() { return m_CollChannel; }
	Collision_Filter GetCollisionFilter(Collision_Channel channel) { return (Collision_Filter)m_CollFilter[(int)channel]; }

	//Debug 출력용
	void DrawRing(IDirect3DDevice9* pd3dDevice, const DirectX::XMFLOAT3& Origin, const DirectX::XMFLOAT3& MajorAxis, const DirectX::XMFLOAT3& MinorAxis, D3DCOLOR Color);
	void DrawSphere(IDirect3DDevice9* pd3dDevice, BoundingSphere& sphere, D3DCOLOR Color);
	LPD3DXLINE Line;
	LPDIRECT3DVERTEXBUFFER9         pVB = NULL;
	LPDIRECT3DINDEXBUFFER9          pIB = NULL;
	IDirect3DVertexDeclaration9* pVertexDecl = NULL;
	//DirectX::BoundingOrientedBox
protected:
	ColliderType m_CollType;

	Collision_Channel m_CollChannel;
	int m_CollFilter[(int)Collision_Channel::Collision_Channel_Max];
};

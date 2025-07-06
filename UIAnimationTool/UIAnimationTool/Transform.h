#pragma once
class Transform
{
public:
	Transform();
	Transform(Transform* t);


	D3DXVECTOR2 m_scale;
	D3DXVECTOR2 m_size;
	D3DXVECTOR2 m_position;
	float m_rotation;
	float m_opacity;
	D3DXVECTOR4 m_color;
};

#include "stdafx.h"
#include "Transform.h"

Transform::Transform()
{
	m_size = D3DXVECTOR2(0, 0);
	m_scale = D3DXVECTOR2(1, 1);
	m_position = D3DXVECTOR2(0, 0);
	m_rotation = 0;
	m_opacity = 1;
	m_color = D3DXVECTOR4(1, 1, 1, 1);
}

Transform::Transform(Transform* t)
{
	m_size = t->m_size;
	m_scale = t->m_scale;
	m_position = t->m_position;
	m_rotation = t->m_rotation;
	m_opacity = t->m_opacity;
	m_color = t->m_color;
}
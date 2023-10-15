#include "pch.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "Material.h"

MeshRenderer::MeshRenderer():Component(COMPONENT_TYPE::MESH_RENDERER)
{

}

MeshRenderer::~MeshRenderer()
{
}

void MeshRenderer::Render(Shader* shader)
{
	//Material->PushData();
	_mesh->Render(shader);
}


//void MeshRenderer::Render()
//{
//	GetTransform()->PushData();
//	_material->PushData();
//
//	_mesh->Render();
//}
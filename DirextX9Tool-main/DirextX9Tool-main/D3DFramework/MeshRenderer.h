#pragma once
#include "Component.h"

class Mesh;
class Material;
class Shader;

//해당 게임 오브젝트의 mesh와 material 을 관리한다.
class MeshRenderer:public Component
{
public:
	MeshRenderer();
	virtual ~MeshRenderer();

	void setMesh(Mesh* mesh) { _mesh = mesh; }
	void setMaterial(Material* mat) { _material = mat; }

	//렌더만 담당한다.
	void Render(Shader* shader);

private:
	Mesh* _mesh = nullptr;
	Material* _material = nullptr;

};


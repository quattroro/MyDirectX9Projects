#pragma once
#include "Object.h"

class Shader;

//�޽�
class Mesh:public Object
{
public:
	Mesh();
	virtual ~Mesh();

	void Init(const vector<Vertex>& vertexBuffer, const vector<WORD>& indexBuffer);

	void Render(Shader* shader);

private:
	void CreateVertecBuffer(const vector<Vertex>& buffer);
	void CreateIndexBuffer(const vector<WORD>& buffer);


	
private:
	//�������ۿ� �ε��� ���۸� ������ �ִ´�.
	LPDIRECT3DVERTEXBUFFER9 _vertexBuffer;

	LPDIRECT3DINDEXBUFFER9 _indexBuffer;

	int vertexCount;
	int triCount;

};


#pragma once
#include "Object.h"

class Shader;

//메쉬
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
	//정점버퍼와 인덱스 버퍼를 가지고 있는다.
	LPDIRECT3DVERTEXBUFFER9 _vertexBuffer;

	LPDIRECT3DINDEXBUFFER9 _indexBuffer;

	int vertexCount;
	int triCount;

};


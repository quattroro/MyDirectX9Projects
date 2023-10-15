#include "pch.h"
#include "Mesh.h"
#include "Utility.h"
#include "Shader.h"

extern float DeltaTime;
extern HWND hWndMain;
extern LPDIRECT3D9         g_pD3D;
extern LPDIRECT3DDEVICE9   g_pd3dDevice;

Mesh::Mesh():Object(OBJECT_TYPE::MESH)
{
}

Mesh::~Mesh()
{
}

void Mesh::Init(const vector<Vertex>& vertexBuffer, const vector<WORD>& indexBuffer)
{
	CreateVertecBuffer(vertexBuffer);
	CreateIndexBuffer(indexBuffer);
}

void Mesh::Render(Shader* shader)
{
	g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
	//g_pd3dDevice->SetVertexDeclaration(shader->g_pDecl);
	if (shader != nullptr)
	{
		g_pd3dDevice->SetVertexShader(shader->_verShader);
		g_pd3dDevice->SetPixelShader(shader->_pixShader);
	}
		

	//shader
	g_pd3dDevice->SetStreamSource(0, _vertexBuffer, 0, sizeof(Vertex));
	g_pd3dDevice->SetIndices(_indexBuffer);
	

	g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, vertexCount, 0, triCount);

}

void Mesh::CreateVertecBuffer(const vector<Vertex>& buffer)
{
	int bufferSize = buffer.size() * sizeof(Vertex);
	vertexCount = buffer.size();
	triCount = vertexCount / 2;

	g_pd3dDevice->CreateVertexBuffer(bufferSize, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &_vertexBuffer, NULL);

	Vertex* pVertices;
	_vertexBuffer->Lock(0, bufferSize, (void**)&pVertices, 0);
	memcpy(pVertices, &buffer[0], bufferSize);
	_vertexBuffer->Unlock();

}

void Mesh::CreateIndexBuffer(const vector<WORD>& buffer)
{
	int bufferSize = buffer.size() * sizeof(WORD);

	g_pd3dDevice->CreateIndexBuffer(bufferSize, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &_indexBuffer, NULL);

	VOID* pIndices;
	_indexBuffer->Lock(0, bufferSize, (void**)&pIndices, 0);

	memcpy(pIndices, &buffer[0], bufferSize);

	_indexBuffer->Unlock();
}


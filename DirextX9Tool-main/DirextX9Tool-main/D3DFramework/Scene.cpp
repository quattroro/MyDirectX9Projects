#include "pch.h"
#include "Scene.h"
#include "GameObject.h"
#include "MeshRenderer.h"

void Scene::Awake()
{
	for (GameObject* gameObject : _gameObjects)
	{
		gameObject->Awake();
	}
}

void Scene::Start()
{
	for (GameObject* gameObject : _gameObjects)
	{
		gameObject->Start();
	}
}

void Scene::LateUpdate()
{
	for (GameObject* gameObject : _gameObjects)
	{
		gameObject->LateUpdate();
	}
}

void Scene::FinalUpdate()
{
	for (GameObject* gameObject : _gameObjects)
	{
		gameObject->FinalUpdate();
	}
}

void Scene::Update()
{
	for (GameObject* gameObject : _gameObjects)
	{
		gameObject->Update();
	}
}

//조명에 대한 정보들을 여기서 만들어 준다.
void Scene::Render(Shader* shader)
{
	for (GameObject* gameObject : _gameObjects)
	{
		gameObject->GetMeshRenderer()->Render(shader);
	}

	////조명에 대한 정보들을 여기서 만들어 준다.
	//PushLightData();

	//for (auto& gameObject : _gameObjects)
	//{
	//	if (gameObject->GetCamera() == nullptr)
	//		continue;

	//	gameObject->GetCamera()->Render();
	
	//D3DXCompileShader()

}

//typedef struct hlsl_shader_data
//{
//	struct hlsl_program prg[RARCH_HLSL_MAX_SHADERS];
//	unsigned active_idx;
//	struct video_shader* cg_shader;
//} hlsl_shader_data_t;
//
//static bool load_program(hlsl_shader_data* hlsl,void* data, unsigned idx, const char* prog, bool path_is_file)
//{
//	d3d_video_t* d3d = (d3d_video_t*)data;
//	LPDIRECT3DDEVICE9 d3d_device_ptr = (LPDIRECT3DDEVICE9)d3d->dev;
//	HRESULT ret, ret_fp, ret_vp;
//	ID3DXBuffer* listing_f = NULL;
//	ID3DXBuffer* listing_v = NULL;
//	ID3DXBuffer* code_f = NULL;
//	ID3DXBuffer* code_v = NULL;
//
//	if (path_is_file)
//	{
//		ret_fp = D3DXCompileShaderFromFile(prog, NULL, NULL,
//			"main_fragment", "ps_3_0", 0, &code_f, &listing_f, &hlsl->prg[idx].f_ctable);
//		ret_vp = D3DXCompileShaderFromFile(prog, NULL, NULL,
//			"main_vertex", "vs_3_0", 0, &code_v, &listing_v, &hlsl->prg[idx].v_ctable);
//	}
//	else
//	{
//		/* TODO - crashes currently - to do with 'end of line' of stock shader */
//		ret_fp = D3DXCompileShader(prog, strlen(prog), NULL, NULL,
//			"main_fragment", "ps_3_0", 0, &code_f, &listing_f, &hlsl->prg[idx].f_ctable);
//		ret_vp = D3DXCompileShader(prog, strlen(prog), NULL, NULL,
//			"main_vertex", "vs_3_0", 0, &code_v, &listing_v, &hlsl->prg[idx].v_ctable);
//	}
//
//	if (ret_fp < 0 || ret_vp < 0 || listing_v || listing_f)
//	{
//		//RARCH_ERR("Cg/HLSL error:\n");
//		/*if (listing_f)
//			RARCH_ERR("Fragment:\n%s\n", (char*)listing_f->GetBufferPointer());
//		if (listing_v)
//			RARCH_ERR("Vertex:\n%s\n", (char*)listing_v->GetBufferPointer());*/
//
//		ret = false;
//		goto end;
//	}
//
//	d3d_device_ptr->CreatePixelShader((const DWORD*)code_f->GetBufferPointer(), &hlsl->prg[idx].fprg);
//	d3d_device_ptr->CreateVertexShader((const DWORD*)code_v->GetBufferPointer(), &hlsl->prg[idx].vprg);
//	code_f->Release();
//	code_v->Release();
//
//end:
//	if (listing_f)
//		listing_f->Release();
//	if (listing_v)
//		listing_v->Release();
//	return ret;
//}

void Scene::PushLightData()
{
	/*LightParams lightParams = {};

	for (auto& gameObject : _gameObjects)
	{
		if (gameObject->GetLight() == nullptr)
			continue;

		const LightInfo& lightInfo = gameObject->GetLight()->GetLightInfo();

		lightParams.lights[lightParams.lightCount] = lightInfo;
		lightParams.lightCount++;
	}

	CONST_BUFFER(CONSTANT_BUFFER_TYPE::GLOBAL)->SetGlobalData(&lightParams, sizeof(lightParams));*/
}

void Scene::AddGameObject(GameObject* gameObject)
{
	_gameObjects.push_back(gameObject);
}

void Scene::RemoveGameObject(GameObject* gameObject)
{
	auto findIt = std::find(_gameObjects.begin(), _gameObjects.end(), gameObject);
	if (findIt != _gameObjects.end())
	{
		_gameObjects.erase(findIt);
	}
}

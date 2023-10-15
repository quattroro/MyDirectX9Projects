#include "pch.h"
#include "SceneManager.h"
#include "Scene.h"
#include "Shader.h"

void SceneManager::Update()
{
	if (_activeScene == nullptr)
		return;

	_activeScene->Update();
	_activeScene->LateUpdate();
	_activeScene->FinalUpdate();
}

void SceneManager::Render(Shader* shader)
{
	if (_activeScene != nullptr)
		_activeScene->Render(shader);
}


void SceneManager::LoadScene(wstring sceneName)
{
	// TODO : 기존 Scene 정리
	// TODO : 파일에서 Scene 정보 로드

	_activeScene = LoadTestScene();

	_activeScene->Awake();
	_activeScene->Start();
}


Scene* SceneManager::LoadTestScene()
{

	return nullptr;
}


//// TEMP
//void SceneManager::Render()
//{
//	if (_activeScene == nullptr)
//		return;
//
//	const vector<shared_ptr<GameObject>>& gameObjects = _activeScene->GetGameObjects();
//	for (auto& gameObject : gameObjects)
//	{
//		if (gameObject->GetCamera() == nullptr)
//			continue;
//
//		gameObject->GetCamera()->Render();
//	}
//}

//void Engine::Update()
//{
//	GET_SINGLE(Input)->Update();
//	GET_SINGLE(Timer)->Update();
//	GET_SINGLE(SceneManager)->Update();
//
//	Render();
//
//	ShowFps();
//}

SceneManager::SceneManager()
{
}

SceneManager::~SceneManager()
{
}

void SceneManager::CreateScene()
{
}


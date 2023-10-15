#pragma once
#include "pch.h"
#include "Scene.h"
#include "Singleton.h"


class SceneManager :public Singleton<SceneManager>
{
public:
	SceneManager();
	virtual ~SceneManager();

	void Update();
	void Render(Shader* shader);
	void LoadScene(wstring sceneName);
	
	Scene* GetActiveScene() { return _activeScene; }

	void CreateScene();
	Scene* LoadTestScene();

	void TempAddScene(Scene* scene) { _activeScene = scene; }

private:
	vector<Scene*> _cenes;
	Scene* _activeScene;
};


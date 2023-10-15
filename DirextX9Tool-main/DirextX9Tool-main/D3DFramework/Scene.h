#pragma once
class GameObject;
class Shader;
class Scene
{
public:
	void Awake();
	void Start();
	void LateUpdate();
	void FinalUpdate();
	void Update();

	void Render(Shader* shader);

private:
	void PushLightData();

public:
	void AddGameObject(GameObject* gameObject);
	void RemoveGameObject(GameObject* gameObject);

	const vector<GameObject*>& GetGameObjects() { return _gameObjects; }
private:
	vector<GameObject*> _gameObjects;

};


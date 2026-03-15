#pragma once

struct SceneSaveData;

class USceneSerializer
{
public:
	static bool SaveScene(const std::string& sceneName, const SceneSaveData& sceneData);
	static bool LoadScene(const std::string& sceneName, SceneSaveData& outSceneData);

private:
	static std::string Serialize(const SceneSaveData& sceneInfo);
	static bool Desrialize(const std::string& jsonString, SceneSaveData& outSceneInfo);

	static const std::string GetSaveDirectory() { return "Content/Scenes/"; }
};
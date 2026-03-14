#pragma once

struct SceneSaveData;

namespace USceneSerializer
{
	std::string Serialize(const SceneSaveData& sceneInfo);
	bool Desrialize(const std::string& jsonString, SceneSaveData& outSceneInfo);
};
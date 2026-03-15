#pragma once

struct SceneSaveData
{
	std::string Name;
	int Version;
	int NextUUID;
};

namespace USceneSerializer
{
	std::string Serialize(const SceneSaveData& sceneInfo);
	bool Desrialize(const std::string& jsonString, SceneSaveData& outSceneInfo);
};
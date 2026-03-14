#include "pch.h"
#include "SceneSerializer.h"
#include "Scene.h"
#include "Object.h"
#include "json.hpp"

using json = nlohmann::json;

static json VectorToJson(const FVector& v)
{
	return json::array({ v.X, v.Y, v.Z });
}

std::string USceneSerializer::Serialize(const SceneSaveData& sceneInfo)
{
	json root;

	root["Version"] = sceneInfo.Version;
	root["NextUUID"] = sceneInfo.NextUUID;

	json objectArray = json::array();

	for (int i = 0; i < GUObjectArray.Size(); i++)
	{
		json obj;
		json objData = json::array();

		//objData["Location"] = 

		obj[i] = objData;
	}

	return std::string();
}

bool USceneSerializer::Desrialize(const std::string& jsonString, SceneSaveData& outSceneInfo)
{
	return false;
}

#include "pch.h"
#include "SceneSerializer.h"
#include "json.hpp"
#include <fstream>

#include "PrimitiveComponent.h"

using ordered_json = nlohmann::ordered_json;
using json = nlohmann::json;

//test용
TArray<UPrimitiveComponent*> objectTestArray;

static json VectorToJson(const FVector& v)
{
	return json::array({ v.X, v.Y, v.Z });
}

std::string USceneSerializer::Serialize(const SceneSaveData& sceneInfo)
{
	ordered_json root;

	root["Version"] = sceneInfo.Version;
	root["NextUUID"] = sceneInfo.NextUUID;

	/*
	json primitive = json::object();

	for (int i = 0; i < objectTestArray.Size(); i++)
	{
	    // 오브젝트 구조체 읽기
		json objData = json::object();

		objData["Location"] = VectorToJson(objectTestArray[i]->GetPosition());
		objData["Rotation"] = VectorToJson(objectTestArray[i]->GetRotation());
		objData["Scale"] = VectorToJson(objectTestArray[i]->GetScale());
		objData["Type"] = "Cube";
		
		primitive[std::to_string(i)] = objData;
	}

	root["Primitives"] = primitive;
	*/

	return root.dump(4);
}

bool USceneSerializer::Desrialize(const std::string& jsonString, SceneSaveData& outSceneInfo)
{
	json root = json::parse(jsonString);

	outSceneInfo.NextUUID = root["NextUUID"];
	outSceneInfo.Version = root["Version"];

	/*
	* if (root.contains("Primitives"))
	{
		for (auto& item : root["Primitives"].items())
		{
			json objData = item.value();

			auto locArr = objData["Location"];
			auto rotArr = objData["Rotation"];
			auto sclArr = objData["Scale"];

			std::string type = objData["Type"];

			// 오브젝트 구조체에 다 담아서 스폰하는 함수에 다 보내기
		}
	}
	*/

	return true;
}

bool USceneSerializer::SaveScene(const std::string& sceneName, const SceneSaveData& sceneData)
{
	std::string fullPath = GetSaveDirectory() + sceneName + ".Scene";
	std::filesystem::create_directories(GetSaveDirectory());

	std::string root = Serialize(sceneData);

	std::ofstream file(fullPath);
	if (!file.is_open()) return false;
	
	file << root;
	file.close();
	return true;
}

bool USceneSerializer::LoadScene(const std::string& sceneName, SceneSaveData& outSceneData)
{
	std::string fullPath = GetSaveDirectory() + sceneName + ".Scene";

	std::ifstream file(fullPath);
	if (!file.is_open()) return false;

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string jsonContent = buffer.str();

	return Desrialize(jsonContent, outSceneData);
}

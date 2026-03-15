#include "pch.h"
#include "SceneSerializer.h"
#include "SceneComponent.h"
#include "Object.h"
#include "json.hpp"
#include "PrimitiveComponent.h"

using json = nlohmann::json;
// Test용
TArray<UPrimitiveComponent*> objectTestArray;


static json VectorToJson(const FVector& v)
{
	return json::array({ v.X, v.Y, v.Z });
}

std::string USceneSerializer::Serialize(const SceneSaveData& sceneInfo)
{
	json root;

	root["Version"] = sceneInfo.Version;
	root["NextUUID"] = sceneInfo.NextUUID;

	json primitive = json::object();

	for (int i = 0; i < objectTestArray.Size(); i++)
	{
		json objData = json::object();

		objData["Location"] = VectorToJson(objectTestArray[i]->GetPosition());
		objData["Rotation"] = VectorToJson(objectTestArray[i]->GetRotation());
		objData["Scale"] = VectorToJson(objectTestArray[i]->GetScale());
		objData["Type"] = "Cube";
		
		primitive[std::to_string(i)] = objData;
	}

	root["Primitives"] = primitive;

	return root.dump(4);
}

bool USceneSerializer::Desrialize(const std::string& jsonString, SceneSaveData& outSceneInfo)
{
	json root = json::parse(jsonString);

	outSceneInfo.Version = root["Version"];
	outSceneInfo.NextUUID = root["NextUUID"];

	if (root.contains("Primitives"))
	{
		for (auto& item : root["Primitives"].items())
		{
			json objData = item.value();

			auto locArr = objData["Location"];
			auto rotArr = objData["Rotation"];
			auto sclArr = objData["Scale"];

			std::string type = objData["Type"];

			// 오브젝트 스폰 로직 연결
		}
	}

	return true;
}

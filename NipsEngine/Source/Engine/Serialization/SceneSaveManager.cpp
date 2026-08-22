#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>

#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Object/Object.h"
#include "Object/ActorIterator.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Object/FName.h"
#include "Math/Matrix.h"

namespace SceneKeys
{
	static constexpr const char* Version            = "Version";
	static constexpr const char* Name               = "Name";
	static constexpr const char* WorldType          = "WorldType";
	static constexpr const char* Actors             = "Actors";
	static constexpr const char* Class              = "Class";
	static constexpr const char* RootComponent      = "RootComponent";
	static constexpr const char* NonSceneComponents = "NonSceneComponents";
	static constexpr const char* Props              = "Props";
	static constexpr const char* Children           = "Children";
	static constexpr const char* IsEditorOnly       = "IsEditorOnly";

	// PerspectiveCamera 섹션
	static constexpr const char* PerspectiveCamera  = "PerspectiveCamera";
	static constexpr const char* Location           = "Location";
	static constexpr const char* Rotation           = "Rotation";
	static constexpr const char* FOV                = "FOV";
	static constexpr const char* NearClip           = "NearClip";
	static constexpr const char* FarClip            = "FarClip";
}

static const char* WorldTypeToString(EWorldType Type)
{
	switch (Type) {
	case EWorldType::Game: return "Game";
	case EWorldType::PIE:  return "PIE";
	default:               return "Editor";
	}
}

static EWorldType StringToWorldType(const string& Str)
{
	if (Str == "Game") return EWorldType::Game;
	if (Str == "PIE")  return EWorldType::PIE;
	return EWorldType::Editor;
}

// ============================================================
// Save
// ============================================================

void FSceneSaveManager::SaveSceneAsJSON(const string& InSceneName, FWorldContext& WorldContext,
                                        const FEditorCameraState* CameraState)
{
	using namespace json;
	if (!WorldContext.World) return;

	string FinalName = InSceneName.empty() ? "Save_" + GetCurrentTimeStamp() : InSceneName;
	std::wstring SceneDir = GetSceneDirectory();
	std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
	std::filesystem::create_directories(SceneDir);

	JSON Root = json::Object();
	Root[SceneKeys::Version]           = CurrentVersion;
	Root[SceneKeys::Name]              = FinalName;
	Root[SceneKeys::WorldType]         = WorldTypeToString(WorldContext.WorldType);
	Root[SceneKeys::PerspectiveCamera] = SerializeCameraState(CameraState);
	Root[SceneKeys::Actors]            = SerializeActors(WorldContext.World);

	std::ofstream File(FileDestination);
	if (File.is_open()) {
		File << Root.dump();
		File.flush();
		File.close();
	}
}

json::JSON FSceneSaveManager::SerializeActors(UWorld* World)
{
	using namespace json;
	JSON Actors = json::Array();

	ULevel* PersistentLevel = World->GetPersistentLevel();
	if (!PersistentLevel) return Actors;

	for (AActor* Actor : PersistentLevel->GetActors())
	{
		if (!Actor) continue;
		Actors.append(SerializeActor(Actor));
	}
	return Actors;
}

json::JSON FSceneSaveManager::SerializeActor(AActor* Actor)
{
	using namespace json;
	JSON a = json::Object();
	a[SceneKeys::Class] = Actor->GetTypeInfo()->name;

	if (USceneComponent* Root = Actor->GetRootComponent())
	{
		a[SceneKeys::RootComponent] = SerializeSceneComponentTree(Root);
	}

	// NonSceneComponent 직렬화
	JSON NonScene = json::Array();
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp) continue;
		if (Comp->IsA<USceneComponent>()) continue;

		JSON nc = json::Object();
		nc[SceneKeys::Class] = Comp->GetTypeInfo()->name;
		nc[SceneKeys::Props] = SerializeProperties(Comp);
		NonScene.append(nc);
	}
	if (NonScene.size() > 0)
	{
		a[SceneKeys::NonSceneComponents] = NonScene;
	}

	return a;
}

json::JSON FSceneSaveManager::SerializeSceneComponentTree(USceneComponent* Comp)
{
	using namespace json;
	JSON c = json::Object();
	c[SceneKeys::Class]        = Comp->GetTypeInfo()->name;
	c[SceneKeys::IsEditorOnly] = Comp->IsEditorOnly();
	c[SceneKeys::Props]        = SerializeProperties(Comp);

	JSON Children = json::Array();
	for (USceneComponent* Child : Comp->GetChildren())
	{
		if (!Child) continue;
		Children.append(SerializeSceneComponentTree(Child));
	}
	if (Children.size() > 0)
	{
		c[SceneKeys::Children] = Children;
	}

	return c;
}

json::JSON FSceneSaveManager::SerializeProperties(UActorComponent* Comp)
{
	using namespace json;
	JSON props = json::Object();

	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (const auto& Prop : Descriptors) {
		props[Prop.Name] = SerializePropertyValue(Prop);
	}
	return props;
}

json::JSON FSceneSaveManager::SerializePropertyValue(const FPropertyDescriptor& Prop)
{
	using namespace json;

	switch (Prop.Type) {
	case EPropertyType::Bool:
		return JSON(*static_cast<bool*>(Prop.ValuePtr));

	case EPropertyType::Int:
		return JSON(*static_cast<int32*>(Prop.ValuePtr));

	case EPropertyType::Float:
		return JSON(static_cast<double>(*static_cast<float*>(Prop.ValuePtr)));

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		return arr;
	}
	case EPropertyType::Vec4: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		arr.append(static_cast<double>(v[3]));
		return arr;
	}
	case EPropertyType::String:
		return JSON(*static_cast<FString*>(Prop.ValuePtr));

	case EPropertyType::Name:
		return JSON(static_cast<FName*>(Prop.ValuePtr)->ToString());

	default:
		return JSON();
	}
}

json::JSON FSceneSaveManager::SerializeCameraState(const FEditorCameraState* CameraState)
{
	using namespace json;

	if (CameraState && CameraState->bValid)
	{
		JSON Cam = Object();
		Cam[SceneKeys::Location] = Array(
			static_cast<double>(CameraState->Location.X),
			static_cast<double>(CameraState->Location.Y),
			static_cast<double>(CameraState->Location.Z));
		Cam[SceneKeys::Rotation] = Array(
			static_cast<double>(CameraState->Rotation.Pitch),
			static_cast<double>(CameraState->Rotation.Yaw),
			static_cast<double>(CameraState->Rotation.Roll));
		Cam[SceneKeys::FOV]      = Array(static_cast<double>(CameraState->FOV));
		Cam[SceneKeys::NearClip] = Array(static_cast<double>(CameraState->NearClip));
		Cam[SceneKeys::FarClip]  = Array(static_cast<double>(CameraState->FarClip));
		return Cam;
	}
	return nullptr;
}

// ============================================================
// Load
// ============================================================

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext,
                                          FEditorCameraState* OutCameraState)
{
	using json::JSON;
	std::ifstream File(std::filesystem::path(FPaths::ToWide(filepath)));
	if (!File.is_open()) return;

	string FileContent((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(FileContent);

	UObject* WorldObj = FObjectFactory::Get().Create("UWorld");
	if (!WorldObj || !WorldObj->IsA<UWorld>()) return;

	UWorld* World = static_cast<UWorld*>(WorldObj);
	EWorldType WorldType = Root.hasKey(SceneKeys::WorldType)
		? StringToWorldType(Root[SceneKeys::WorldType].ToString())
		: EWorldType::Editor;

	DeserializeCameraState(Root, OutCameraState);

	if (Root.hasKey(SceneKeys::Actors))
	{
		DeserializeActors(Root[SceneKeys::Actors], World);
	}

	World->SyncSpatialIndex();

	OutWorldContext.WorldType = WorldType;
	OutWorldContext.World     = World;
}

void FSceneSaveManager::DeserializeActors(json::JSON& ActorsNode, UWorld* World)
{
	for (auto& ActorJSON : ActorsNode.ArrayRange())
	{
		DeserializeActor(ActorJSON, World);
	}
}

void FSceneSaveManager::DeserializeActor(json::JSON& ActorNode, UWorld* World)
{
	if (!ActorNode.hasKey(SceneKeys::Class)) return;

	string ActorClassName = ActorNode[SceneKeys::Class].ToString();
	UObject* Obj = FObjectFactory::Get().Create(ActorClassName);
	if (!Obj) return;

	AActor* Actor = Cast<AActor>(Obj);
	if (!Actor) return;

	// SpatialIndex 등록을 위해 SetWorld를 컴포넌트 등록보다 먼저 호출
	Actor->SetWorld(World);

	if (ULevel* PersistentLevel = World->GetPersistentLevel())
	{
		PersistentLevel->AddActor(Actor);
	}

	// RootComponent 트리 복원
	if (ActorNode.hasKey(SceneKeys::RootComponent))
	{
		USceneComponent* Root = DeserializeSceneComponentTree(ActorNode[SceneKeys::RootComponent], Actor);
		if (Root)
		{
			Actor->SetRootComponent(Root);
		}
	}

	// NonSceneComponent 복원 (RootComponent 복원 후)
	if (ActorNode.hasKey(SceneKeys::NonSceneComponents))
	{
		DeserializeNonSceneComponents(ActorNode[SceneKeys::NonSceneComponents], Actor);
	}
}

USceneComponent* FSceneSaveManager::DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner)
{
	if (!Node.hasKey(SceneKeys::Class)) return nullptr;

	string CompClassName = Node[SceneKeys::Class].ToString();
	UObject* Obj = FObjectFactory::Get().Create(CompClassName);
	if (!Obj) return nullptr;

	USceneComponent* Comp = Cast<USceneComponent>(Obj);
	if (!Comp) return nullptr;

	// Owner 등록 (SpatialIndex 등록 포함)
	Owner->RegisterComponent(Comp);

	if (Node.hasKey(SceneKeys::IsEditorOnly))
	{
		Comp->SetEditorOnly(Node[SceneKeys::IsEditorOnly].ToBool());
	}

	// 프로퍼티 복원
	if (Node.hasKey(SceneKeys::Props))
	{
		ApplyProperties(Comp, Node[SceneKeys::Props]);
	}

	Comp->MarkTransformDirty();

	// 자식 트리 재귀 복원
	if (Node.hasKey(SceneKeys::Children))
	{
		for (auto& ChildNode : Node[SceneKeys::Children].ArrayRange())
		{
			USceneComponent* Child = DeserializeSceneComponentTree(ChildNode, Owner);
			if (Child)
			{
				Child->AttachToComponent(Comp);
			}
		}
	}

	return Comp;
}

void FSceneSaveManager::DeserializeNonSceneComponents(json::JSON& Node, AActor* Owner)
{
	for (auto& CompNode : Node.ArrayRange())
	{
		if (!CompNode.hasKey(SceneKeys::Class)) continue;

		string CompClassName = CompNode[SceneKeys::Class].ToString();
		UObject* Obj = FObjectFactory::Get().Create(CompClassName);
		if (!Obj) continue;

		UActorComponent* Comp = Cast<UActorComponent>(Obj);
		if (!Comp) continue;

		Owner->RegisterComponent(Comp);

		if (CompNode.hasKey(SceneKeys::Props))
		{
			ApplyProperties(Comp, CompNode[SceneKeys::Props]);
		}
	}
}

void FSceneSaveManager::ApplyProperties(UActorComponent* Comp, json::JSON& PropsJSON)
{
	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (auto& Prop : Descriptors) {
		if (!PropsJSON.hasKey(Prop.Name)) continue;

		ApplyPropertyValue(Prop, PropsJSON[Prop.Name]);
		Comp->PostEditProperty(Prop.Name);
	}
}

void FSceneSaveManager::ApplyPropertyValue(FPropertyDescriptor& Prop, json::JSON& Value)
{
	switch (Prop.Type) {
	case EPropertyType::Bool:
		*static_cast<bool*>(Prop.ValuePtr) = Value.ToBool();
		break;

	case EPropertyType::Int:
		*static_cast<int32*>(Prop.ValuePtr) = Value.ToInt();
		break;

	case EPropertyType::Float:
		*static_cast<float*>(Prop.ValuePtr) = static_cast<float>(Value.ToFloat());
		break;

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 3) v[i] = static_cast<float>(elem.ToFloat());
			++i;
		}
		break;
	}
	case EPropertyType::Vec4: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 4) v[i] = static_cast<float>(elem.ToFloat());
			++i;
		}
		break;
	}
	case EPropertyType::String:
		*static_cast<FString*>(Prop.ValuePtr) = Value.ToString();
		break;

	case EPropertyType::Name:
		*static_cast<FName*>(Prop.ValuePtr) = FName(Value.ToString());
		break;

	default:
		break;
	}
}

void FSceneSaveManager::DeserializeCameraState(json::JSON& Root, FEditorCameraState* OutCameraState)
{
	using namespace json;
	if (!OutCameraState || !Root.hasKey(SceneKeys::PerspectiveCamera)) return;

	JSON Cam = Root[SceneKeys::PerspectiveCamera];

	if (Cam.hasKey(SceneKeys::Location))
	{
		JSON Loc = Cam[SceneKeys::Location];
		OutCameraState->Location = FVector(
			static_cast<float>(Loc[0].ToFloat()),
			static_cast<float>(Loc[1].ToFloat()),
			static_cast<float>(Loc[2].ToFloat()));
	}
	if (Cam.hasKey(SceneKeys::Rotation))
	{
		JSON Rot = Cam[SceneKeys::Rotation];
		OutCameraState->Rotation = FRotator(
			static_cast<float>(Rot[0].ToFloat()),
			static_cast<float>(Rot[1].ToFloat()),
			static_cast<float>(Rot[2].ToFloat()));
	}
	if (Cam.hasKey(SceneKeys::FOV))
		OutCameraState->FOV = static_cast<float>(Cam[SceneKeys::FOV][0].ToFloat());
	if (Cam.hasKey(SceneKeys::NearClip))
		OutCameraState->NearClip = static_cast<float>(Cam[SceneKeys::NearClip][0].ToFloat());
	if (Cam.hasKey(SceneKeys::FarClip))
		OutCameraState->FarClip = static_cast<float>(Cam[SceneKeys::FarClip][0].ToFloat());

	OutCameraState->bValid = true;
}

// ============================================================
// Utility
// ============================================================

string FSceneSaveManager::GetCurrentTimeStamp()
{
	std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_s(&tm, &t);

	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
	return buf;
}

TArray<FString> FSceneSaveManager::GetSceneFileList()
{
	TArray<FString> Result;
	std::wstring SceneDir = GetSceneDirectory();
	if (!std::filesystem::exists(SceneDir))
	{
		return Result;
	}

	for (auto& Entry : std::filesystem::directory_iterator(SceneDir))
	{
		if (Entry.is_regular_file() && Entry.path().extension() == SceneExtension)
		{
			Result.push_back(FPaths::ToUtf8(Entry.path().stem().wstring()));
		}
	}
	return Result;
}

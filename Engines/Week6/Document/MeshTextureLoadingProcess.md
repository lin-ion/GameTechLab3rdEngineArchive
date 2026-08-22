# Mesh & Texture Loading Process in NipsEngine

이 문서는 NipsEngine에서 `.obj` 모델 파일과 `.mtl` 머티리얼 설정, 그리고 이미지 텍스처(`.jpg`, `.png` 등)를 결합하여 최종적으로 화면에 출력하는 기술적 과정을 설명합니다.

---

## 1. 개요 (The Pipeline)

엔진은 정적 메시(Static Mesh)를 로드할 때 다음의 3단계 흐름을 거칩니다:
1.  **OBJ 파싱**: 기하학적 데이터(Vertices, Indices)와 머티리얼 참조 식별.
2.  **MTL 로딩**: 색상 속성 및 텍스처 파일 경로 파싱.
3.  **Texture 생성**: 이미지 파일을 GPU 텍스처 리소스(SRV)로 변환.

---

## 2. 세부 단계별 로직

### 단계 1: OBJ 파일의 머티리얼 참조 (`mtllib`, `usemtl`)
`FResourceManager::LoadStaticMesh`가 실행되면, 가장 먼저 OBJ 파일의 텍스트를 스캔합니다.
*   **`mtllib [파일명].mtl`**: OBJ 파일 최상단에서 해당 메시가 사용할 머티리얼 정의 파일을 찾습니다.
*   **`usemtl [머티리얼명]`**: 특정 폴리곤 그룹이 MTL 파일 내의 어떤 머티리얼 설정을 사용할지 지정합니다. 
    *   *주의*: `usemtl`에 적힌 이름은 반드시 `.mtl` 파일의 `newmtl` 이름과 일치해야 합니다.

### 단계 2: MTL 파일 파싱 (`FObjMtlLoader`)
`FResourceManager::LoadMaterialFromPath` 함수는 식별된 `.mtl` 파일을 읽어 `FMaterial` 구조체를 채웁니다.
*   **주요 토큰 매핑**:
    *   `map_Kd`: Diffuse Texture (기본 색상)
    *   `map_bump` / `bump`: Normal Map (표면 굴곡)
    *   `map_Ks`: Specular/Roughness Map (반사광)
    *   `map_Ka`: Ambient/AO Map (환경광/차폐)
*   **경로 해결 (`ResolveTexPath`)**: MTL 파일에 적힌 텍스처 경로가 실제 파일 시스템과 일치하지 않을 수 있으므로, 엔진은 `.mtl` 파일 주변 폴더를 재귀적으로 검색(`FindFileRecursively`)하여 실제 파일을 찾아냅니다.

### 단계 3: GPU 텍스처 로드 (`LoadTexture`)
식별된 이미지 파일 경로는 `FResourceManager::LoadTexture`로 전달됩니다.
*   **DDS 로더**: `.dds` 확장자인 경우 `DirectX::CreateDDSTextureFromFileEx`를 사용하여 고성능으로 로드합니다.
*   **WIC 로더**: `.jpg`, `.png`, `.jpeg` 등 일반 이미지인 경우 Windows Imaging Component(WIC)를 통해 로드하며, 내부적으로 D3D11 Shader Resource View(SRV)를 생성합니다.

---

## 3. 최적화: 바이너리 직렬화 (Binary Serialization)

OBJ와 MTL은 텍스트 형식이어서 로딩 속도가 느립니다. NipsEngine은 이를 개선하기 위해 **캐싱 시스템**을 운용합니다.
1.  **최초 로드**: OBJ/MTL을 파싱하여 `FStaticMesh` 데이터를 구축합니다.
2.  **구우기 (Baking)**: 구축된 데이터를 `Asset/Mesh/Bin/[파일명].bin` 경로에 바이너리 형태로 저장합니다.
3.  **이후 로드**: 원본 OBJ 파일의 수정 시간보다 `.bin` 파일이 최신이라면, 복잡한 텍스트 파싱을 건너뛰고 바이너리 데이터를 메모리로 직접 읽어들여 로딩 속도를 수십 배 향상시킵니다.

---

## 4. 트러블슈팅 (Troubleshooting Guide)

### 텍스처가 나오지 않는 경우
*   **mtllib 누락**: OBJ 파일 최상단에 `.mtl` 파일 참조가 있는지 확인하세요.
*   **이름 불일치**: OBJ의 `usemtl` 이름과 MTL의 `newmtl` 이름이 정확히 일치해야 합니다.
*   **캐시 간섭**: `.mtl` 파일을 수정한 후에는 반드시 `Asset/Mesh/Bin/` 폴더 내의 해당 `.bin` 파일을 삭제해야 변경 사항이 반영됩니다.
*   **확장자 오타**: 실제 파일은 `.jpg`인데 `.mtl`에는 `.jpeg`로 적혀 있는 경우 로드에 실패할 수 있습니다.

### 색상이 이상하게 나오는 경우
*   **Linear vs SRGB**: 엔진 셰이더는 선형 공간(Linear Space) 연산을 기대하므로, 텍스처 로드 시 SRV 생성 옵션에서 감마 보정 여부를 체크해야 합니다.
*   **Normal Map 채널**: 엔진이 사용하는 노말 맵 형식이 OpenGL 방식인지 DirectX 방식(Y채널 반전)인지 확인이 필요합니다.

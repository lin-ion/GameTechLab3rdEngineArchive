# Special Effects Implementation: FireBall & HeightFog

이 문서는 NipsEngine에서 구현된 `FireBall`과 `HeightFog` 효과의 컴포넌트 구조, 데이터 흐름, 단계별 처리 로직 및 수학적 유도 과정을 상세히 설명합니다.

---

## 1. World Position Reconstruction (공통 로직)

### A. 처리 단계
두 효과 모두 화면 공간 후처리이므로, 픽셀의 깊이(Depth)로부터 월드 좌표를 복원하는 과정이 선행됩니다.
1.  **NDC 생성**: UV와 Depth를 사용하여 NDC 공간의 좌표 $(x, y, z, 1.0)$을 생성합니다.
2.  **역투영**: `InvViewProj` 행렬을 곱해 동차 월드 좌표를 얻습니다.
3.  **원근 분할**: $w$ 성분으로 나누어 최종 월드 좌표를 산출합니다.

### B. 유도 과정 (The Logic of Unprojection)
표준 그래픽스 파이프라인에서 정점 $P_{world}$가 화면의 $P_{ndc}$로 변환되는 과정은 다음과 같습니다:
1.  $P_{clip} = P_{world} \times M_{view} \times M_{proj}$
2.  $P_{ndc}.xyz = \frac{P_{clip}.xyz}{P_{clip}.w}$ (Perspective Divide)

이를 역으로 추적하기 위해 **역행렬($M_{invViewProj}$)**을 사용합니다:
1.  **Homogeneous Coordinate 복원**: $P_{ndc} = (x, y, z, 1.0)$ 정의.
2.  **역변환**: $P_{world\_unnormalized} = P_{ndc} \times M_{invViewProj}$
3.  **원근 보정**: $P_{world}.xyz = \frac{P_{world\_unnormalized}.xyz}{P_{world\_unnormalized}.w}$

---

## 2. FireBall Effect (Screen-Space Volume Approximation)

### A. Component (UFireBallComponent)
*   **데이터 정의**: `WorldLocation`, `Intensity`, `Radius`, `RadiusFallOff`, `Color`를 관리합니다.
*   **AABB 계산**: 가시성 컬링을 위해 `RadiusFallOff`를 기준으로 `WorldAABB`를 생성합니다. (AABB가 시각적 범위보다 작으면 가장자리가 잘리는 현상 발생)
*   **수집**: `RenderCollector`가 매 프레임 `FFireBallInfo`를 생성하여 `RenderBus`에 적재합니다.

### B. Shader Logic (FireBallShader.hlsl)
1.  **거리 계산**: 복원된 월드 좌표와 파이어볼 중심 사이의 거리($dist$)를 구합니다.
2.  **범위 판정**: $dist > RadiusFallOff$인 픽셀은 스킵합니다.
3.  **강도 및 블렌딩**: `smoothstep` 기반 weight에 `Intensity`를 곱해 `SceneColor`와 합성합니다.

### C. 수학적 유도 (Cubic Hermite Interpolation)
선형 보간의 급격한 변화를 막기 위해 경계($t=0, t=1$)에서 기울기가 0이 되는 3차 다항식 $f(t) = at^3 + bt^2 + ct + d$를 유도합니다.

**제한 조건**:
1. $f(0)=0, f(1)=1, f'(0)=0, f'(1)=0$
2. 위 조건을 연립방정식으로 풀면 $a=-2, b=3, c=0, d=0$ 도출.
3. **$f(t) = 3t^2 - 2t^3$** (표준 smoothstep)
4. 최종 가중치: $weight = 1.0 - f(\frac{dist}{RadiusFallOff})$

---

## 3. HeightFog Effect (Exponential Height Fog)

### A. Component (UHeightFogComponent)
*   **데이터 정의**: `FogDensity`, `FogHeightFalloff`, `StartDistance`, `FogMaxOpacity`, `FogInscatteringColor` 등을 관리합니다.
*   **값 변환**: UI(Property Window) 연동 시 임시 객체 주소 오류를 방지하기 위해 멤버 변수의 직접 주소(`&Color.r`)를 사용합니다.

### B. Shader Logic (HeightFogShader.hlsl)
1.  **Height Density**: 높이에 따른 밀도 감쇄를 계산합니다.
2.  **Distance Extinction**: 카메라 거리에 따른 소광 현상을 계산합니다.
3.  **최종 합성**: `lerp(SceneColor, InscatteringColor, FogFactor)` 적용.

### C. 수학적 유도 (Physical Principles)
1.  **Beer-Lambert Law (거리 감쇄)**:
    *   $\frac{dI}{ds} = -\rho I$ 미분 방정식을 풀면 $I(s) = I_0 e^{-\rho s}$ 도출.
    *   안개 가림 정도: **$1 - e^{-\rho s}$**
2.  **Atmospheric Density (높이 감쇄)**:
    *   기압 공식을 응용하여 고도 $z$에 따른 밀도를 지수 함수로 모델링: $\rho(z) = \rho_0 \times e^{-k(z - z_0)}$

---

## 4. 구현 시 주요 고려사항 (Technical Notes)

*   **상수 버퍼 정렬**: HLSL의 16바이트 정렬 규칙 준수 (`FVector`+`float` 조합).
*   **성능 최적화**: `exp` 함수의 비용을 고려하여 `FogCutoffDistance` 활용.
*   **정밀도 문제**: 카메라가 원점에서 멀어질 때 발생하는 `InvViewProj` 오차(Jittering) 주의.
*   **깊이 상호작용**: 빌보드 등 반투명 객체의 `DepthWrite` 설정이 후처리 결과에 끼치는 영향 고려.

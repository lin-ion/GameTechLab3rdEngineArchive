작업 단위별 계획
[1] FRenderResources 추가

FireBallShader (FShader) — FireBallShader.hlsl, VS_Main / PS_Main, InputLayout nullptr
FireBallConstantBuffer (FConstantBuffer) — sizeof(FFireBallCBuffer)
FFireBallCBuffer는 hlsl의 b10 레이아웃과 1:1 매칭되는 C++ 구조체로 별도 정의

[2] Renderer::Create 추가

FireBallShader.Create(...) 호출
FireBallConstantBuffer.Create(...) 호출
InitializePostProcesses()에서 FireBallPostProcess를 Outline 앞에 push_back

순서 근거: SceneColor에 색 합성하는 효과이므로 Outline 이전, FXAA 이전이 적절



[3] FPostProcessViewDesc 빌드 시점 (Renderer 또는 상위 호출자)

RenderBus에서 FireBallInfoArray를 읽어서 ViewDesc에 snapshot
이 부분은 이미 FPostProcessViewDesc에 FireBallInfoArray가 선언되어 있으니 빌드 로직만 추가

[4] FireBallPostProcess::IsEnabled

ViewDesc.FireBallInfoArray가 비어있으면 false
ViewMode 조건이 필요하다면 추가 (예: DepthScene 모드일 때 스킵 등)

[5] FireBallPostProcess::Execute 내부 순서
① InvViewProj 계산
   (View * Proj).Inverse() → FFireBallCBuffer.InvViewProj

② FFireBallCBuffer 채우기
   FireBallInfoArray → FireBalls[] 배열 매핑 (MAX 32 클램프)
   FireBallCount 설정

③ FireBallConstantBuffer.Update() → PSSetConstantBuffers(10)

④ Shader.Bind(Context)

⑤ SRV 바인딩
   t0 → SceneColorSRV (Execute 인자)
   t1 → RenderTargets.DepthSRV

⑥ Sampler 바인딩
   s0 → PointSampler (별도 생성 필요 — 아래 참고)

⑦ Context->Draw(3, 0)   ← fullscreen triangle, no VB

⑧ SRV null 클린업
   PSSetShaderResources(0, 2, nullptrs)
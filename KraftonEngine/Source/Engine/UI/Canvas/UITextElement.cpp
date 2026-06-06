#include "UI/Canvas/UITextElement.h"

// 사이클 ①: 데이터 골격만 — 텍스트 5멤버/접근자는 헤더에 인라인이라 이 TU 는 현재 비어 있다.
// 사이클 ②에서 OnLayoutUpdated(RmlUi 마운트+동기화)·BeginDestroy 를 UILabel.cpp 에서 이리로 이전하고,
//   - 비빈-텍스트 마운트 가드(R2): Text 가 비어있지 않을 때만 마운트, 빈 동안 latch 금지
//   - 런타임 한정 외부동기화 게이트(R1): 에디터 LayoutCanvas 에선 마운트/동기화 스킵
// 를 적용한다. RmlUi include 는 그때 이 .cpp 에만 추가한다(헤더 격리 유지).

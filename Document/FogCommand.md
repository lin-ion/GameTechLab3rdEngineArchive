## HeightFogComponent를 구현하세요.

---

1. Resource 준비 
    1. Depth 기록용 Texture2D
    2. Color 기록용 Texture2D
    3. 안개  value당 값 처리할 constantbuffer
    4. Camera Info
2. depth buffer , scene color 확보
    1. 오브젝트 렌더링(Opaque Pass)이 완료된 후, 결과물이 담긴 Render Target과 Depth Buffer를 후처리 셰이더의 입력(SRV)으로 바인딩합니다.
3. Full-screen Quad 
    1. 화면 전체를 덮는 사각형을 그린다. 이 단계의 pixel shader에서 depth buffer와 scene color를 이용해서 shading 처리를 함
4. Pixel Logic
    1. 임의의 pixel UV를 이용해 depth 값을 읽고 해당 값과 카메라 역행렬을 이용해서 world space position 계산
    2. 카메라와 월드의 거리를 계산 , 픽셀의 높이와 안개의 기준 높이를 비교
    3. 거리 + 높이의 로직을 통해서 안개를 어떻게 그려낼지 계산. 안개의 특성을 반영해서 높이가 높아질 수록 안개가 옅어짐
    4. 색상 블렌딩 보간으로 마무리
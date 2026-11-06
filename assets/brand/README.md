# VEIL 필터 브랜드 자산

CVA 아래 VEIL의 공통 V와 오른쪽 아래 보정 빛 표식입니다. 제품 이름을 함께 표시하며 색만으로 구분하지 않습니다.

- 정본: CVA 저장소의 `docs/design/cva-brand-v1/build_veil_identity.py`와 `ICON_GUIDE.md`. 이 사본의 원본/출력 해시는 `source-manifest.json`에 기록합니다.
- `color.svg`는 24px 이상, `micro.svg`는 16·20px, `mono.svg`는 밝은 표면, `reverse.svg`는 어두운 표면에 사용합니다.
- PNG는 같은 정본의 투명 512px 렌더이며, ICO는 16·20·24·32·40·48·64·128·256px 프레임을 포함합니다. PNG·ICO를 따로 크롭하거나 손으로 수정하지 않습니다.
- 설치 EXE의 Windows 아이콘, 설치창 제목 표시줄과 64px 헤더에 이 자산을 사용합니다. 이름과 기능 설명은 텍스트로 남기며 장식 이미지는 키보드 초점이나 중복 낭독 대상에 넣지 않습니다.
- 제품은 OBS 내부 필터입니다. OBS의 기본 속성·테마·필터 목록 아이콘을 임의로 덮어쓰거나 별도 실행 앱 아이콘이 있는 것처럼 표시하지 않습니다.

브랜드 자산의 권리·사용 정책은 CVA 브랜드 가이드 정본을 따릅니다. 설치·실제 OBS 채택·공개 릴리스 상태는 제품 README와 실제 산출물 검증을 별도로 확인합니다.

구현 참고: [Microsoft C# Win32 icon/resource 옵션](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/compiler-options/win32icon-compiler-option), [Windows Forms Form.Icon](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.form.icon). 기존 C# 설치기의 공식 지원 경로를 사용하고 런타임 외부 이미지 파일 의존성은 추가하지 않습니다.

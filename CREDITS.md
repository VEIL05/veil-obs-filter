# VEIL · Credits

VEIL은 아래 프로젝트의 코드·모델·런타임을 사용합니다. 얼굴 보정과 추적의
기반을 제공한 **Face Beautifier**와 **Lumina Beauty**, 그리고 각 프로젝트의
개발자와 기여자에게 감사드립니다. 실제로 가져온 구성요소와 수학적 참고·조사
대상을 구분했습니다. 구성요소의 원래 저작권과 라이선스는 그대로 유지됩니다.

## 얼굴 보정과 추적의 기반

| 프로젝트 | 고정 기준 | VEIL에서 사용하는 부분 | 라이선스·고지 |
| --- | --- | --- | --- |
| [ctbot000/face-beautifier](https://github.com/ctbot000/face-beautifier) | [`ca5ad40b58e2d0133de6b22282b29000d3667046`](https://github.com/ctbot000/face-beautifier/tree/ca5ad40b58e2d0133de6b22282b29000d3667046) | 얼굴 변형, 얼굴 영역, 피부·빛·색감 계산과 One Euro 좌표 보정을 C++/OBS 효과로 이식했습니다. 마스크 생성과 OBS 처리 흐름은 VEIL에 맞게 바꿨습니다. | MIT · Copyright © 2026 CTBot000. [전문](third_party/face-beautifier/LICENSE) |
| [dinjufen/lumina-beauty](https://github.com/dinjufen/lumina-beauty) | [`f1472d581dab9706c8536e0decd46592f9045d44`](https://github.com/dinjufen/lumina-beauty/tree/f1472d581dab9706c8536e0decd46592f9045d44) | CPU 얼굴 검출, 회전 ROI와 478점 랜드마크 해석·추적을 적용했습니다. OpenGL 렌더러와 별도 보정·분할 엔진은 포함하지 않습니다. | MIT · Copyright © 2026 Lumina Beauty contributors. [전문](third_party/face-tracking/LICENSE-LUMINA.txt) · [적용 범위](third_party/face-tracking/NOTICE.md) |

## 사용·포함한 구성요소

| 프로젝트·구성요소 | 버전·기준 | VEIL에서 사용하는 부분 | 라이선스·고지 |
| --- | --- | --- | --- |
| [Google MediaPipe](https://github.com/google-ai-edge/mediapipe) 얼굴 모델 | 위 Lumina 고정 커밋에서 가져온 ONNX 자산. [파일별 해시](models/PROVENANCE.json) | `face_detector.onnx`와 `face_landmarks.onnx` 두 모델을 사용합니다. | Apache-2.0 · [전문](models/LICENSE-Apache-2.0.txt) · [모델 출처](models/NOTICE.md) |
| [PeterL1n/RobustVideoMatting](https://github.com/PeterL1n/RobustVideoMatting) | [v1.0.0](https://github.com/PeterL1n/RobustVideoMatting/tree/v1.0.0), MobileNetV3 FP32 ONNX | 현재 자동 인물 배경 제거 모델입니다. 공식 모델의 입출력과 네 재귀 상태를 연결하는 네이티브 어댑터를 구현했습니다. | GPL-3.0 · [전문](third_party/video-matting/data/licenses/RVM-GPL-3.0.txt) · [모델·런타임 해시](third_party/video-matting/data/PROVENANCE.json) |
| [PeterL1n/BackgroundMattingV2](https://github.com/PeterL1n/BackgroundMattingV2) | [v1.0.0](https://github.com/PeterL1n/BackgroundMattingV2/tree/v1.0.0), MobileNetV2 HD ONNX | 과거 배경 등록 방식의 어댑터와 모델을 보관합니다. 현재 기본 제품에서는 이 방식을 사용하지 않지만 모델 자산은 포함되어 있습니다. | MIT · [전문](third_party/video-matting/data/licenses/BMv2-MIT.txt) · [보관 기능](README.md) |
| [kaito-tokyo/live-backgroundremoval-lite](https://github.com/kaito-tokyo/live-backgroundremoval-lite) | [4.0.1](https://github.com/kaito-tokyo/live-backgroundremoval-lite/tree/4.0.1) | 이전 장면 호환을 위해 공식 DLL을 포함합니다. DLL은 바꾸지 않았으며, OBS 필터 연결용 premultiplied-alpha 보정은 [별도 패치](third_party/background-engines/lite-premultiplied-alpha.patch)로 기록했습니다. 현재 자동 인물 제거의 기본 모델은 RVM입니다. | 플러그인 GPL-3.0-or-later · [전문](third_party/background-engines/lite/UPSTREAM-LICENSE.txt). 효과·번역 자산의 Apache-2.0 구분은 [REUSE](third_party/background-engines/lite/data/REUSE.toml), 포함 의존성 고지는 [licenses](third_party/background-engines/lite/data/licenses)에 있습니다. |
| [FiniteSingularity/obs-advanced-masks](https://github.com/FiniteSingularity/obs-advanced-masks) | [v1.6.1](https://github.com/FiniteSingularity/obs-advanced-masks/tree/v1.6.1) | 크로마키·가장자리 처리에 세 C 구현 파일과 셰이더를 사용합니다. 해당 C 파일은 수정하지 않았고 VEIL 연결 코드는 별도입니다. 독립 필터 호환용 공식 DLL도 포함합니다. | GPL-2.0 · [전문](third_party/background-engines/advanced-components/LICENSE.txt) · [사용 파일 해시](third_party/background-engines/advanced-components/PROVENANCE.json) · [공식 DLL 고지](third_party/background-engines/advanced/UPSTREAM-LICENSE.txt) |
| [Microsoft ONNX Runtime](https://github.com/microsoft/onnxruntime) | CPU [1.31.1](https://github.com/microsoft/onnxruntime/releases/tag/v1.31.1), DirectML 패키지 **1.24.4** | 얼굴 CPU 추론과 인물 GPU 추론 런타임입니다. 두 런타임은 별도 모듈/API 테이블을 사용하며, 인물용 DLL 파일명만 충돌 방지를 위해 변경했습니다. | MIT 및 포함 의존성 고지 · [CPU 전문](third_party/onnxruntime/LICENSE) · [CPU 추가 고지](third_party/onnxruntime/ThirdPartyNotices.txt) · [인물용 전문](third_party/video-matting/data/licenses/ONNX-Runtime-LICENSE.txt) · [인물용 추가 고지](third_party/video-matting/data/licenses/ONNX-Runtime-ThirdPartyNotices.txt) |
| [Microsoft DirectML](https://github.com/microsoft/DirectML) | Microsoft.AI.DirectML **1.15.4** | Windows x64 GPU 추론용 공식 재배포 DLL입니다. | **오픈소스 라이선스가 아닌 Microsoft DirectML 사용·재배포 조건**이 적용됩니다. [전문](third_party/video-matting/data/licenses/DirectML-LICENSE.txt) · [추가 고지](third_party/video-matting/data/licenses/DirectML-ThirdPartyNotices.txt) |
| [OBS Studio/libobs](https://github.com/obsproject/obs-studio) | [33.0.1](https://github.com/obsproject/obs-studio/tree/33.0.1) SDK 기준 | OBS 플러그인 API, 헤더와 import library를 사용합니다. OBS 애플리케이션 자체는 포함하지 않습니다. | GPL-2.0-or-later · [전문](third_party/obs-sdk/obs-studio-33.0.1/COPYING) |
| [mstorsjo/llvm-mingw](https://github.com/mstorsjo/llvm-mingw) | **20261020**, UCRT x86_64 | 빌드 도구 체인입니다. 생성한 VEIL 바이너리에 C++·unwind·MinGW·winpthreads 런타임이 정적으로 연결되며, 빌드 시 해당 고지문을 설치 패키지에 복사합니다. | 구성요소별 라이선스·예외 조항 · [고정 다운로드와 해시](scripts/build/bootstrap_tools.py) · [빌드 안내](docs/BUILD.md) |
| NASA / [scikit-image](https://github.com/scikit-image/scikit-image) 테스트 사진 | [scikit-image v0.21.0](https://github.com/scikit-image/scikit-image/tree/v0.21.0) `astronaut` | 공개 NASA 사진과 320×320 BGRA 변환본을 개발 테스트에만 사용합니다. 설치되는 필터 자산에는 포함하지 않습니다. | upstream이 public domain으로 안내한 NASA 이미지 · [사진 출처·해시](third_party/face-tracking/NOTICE.md#public-test-fixture) |

## 수학적 참고와 조사

| 대상 | 관계 | 고지 |
| --- | --- | --- |
| [PyMatting / Shared Matting](https://github.com/pymatting/pymatting/blob/master/pymatting/alpha/estimate_alpha_sm.py) | 과거 배경 사진 기반 윤곽 보정에서 색상선 alpha 투영식을 참고했습니다. 작은 투영·제한 계산은 네이티브 C++로 독립 구현했으며, 전체 Shared Matting 알고리즘이나 Python·Numba 런타임을 포함하지 않습니다. | MIT · Copyright © 2020 PyMatting. [보존한 전문](third_party/background-engines/PyMatting-MIT.txt) |
| BGOBS 0.3.26 | 과거 비교 조사와 BMv2 공식 모델 파일의 해시 대조에만 사용했습니다. 래퍼, 전화기·USB·ADB 구성요소와 외부 서비스는 포함하지 않습니다. | [기록된 모델 대조 범위](third_party/video-matting/data/PROVENANCE.json) |

## 전체 고지와 재배포 소스

이 문서는 기여와 사용 범위를 빠르게 찾기 위한 목록입니다. 전체 라이선스 전문,
변경 범위와 재배포 소스 위치는 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)를
함께 확인해 주세요. 각 프로젝트의 코드·모델·런타임은 위에 연결한 원래 조건을
따르며, 이 목록은 서로 다른 구성요소를 하나의 라이선스로 다시 허가하지 않습니다.

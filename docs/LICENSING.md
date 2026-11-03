# VEIL 라이선스 안내 / Licensing

VEIL은 여러 모듈과 외부 자산으로 구성됩니다. 이 저장소 전체를 MIT나 하나의
GPL 버전으로 일괄 재허가하지 않습니다. 원래 저작권·라이선스 고지는 유지됩니다.

| 범위 | 적용 조건 | 원문·근거 |
| --- | --- | --- |
| VEIL 얼굴 보정 OBS 통합, 설치·빌드·테스트 코드 | GPL-2.0-or-later, 개별 파일의 별도 고지가 있으면 해당 고지 우선 | [LICENSE](../LICENSE), [GPL 2](../LICENSES/GPL-2.0.txt) |
| VEIL 인물 처리 모듈 `veil-video-matting` | VEIL 작성 통합은 GPL-3.0-or-later | [모듈 고지](../src/video_matting_filter.cpp), [GPL 3](../LICENSES/GPL-3.0.txt) |
| RVM 모델·upstream 소스 | upstream GPL version 3 조건 유지 | [원문](../third_party/video-matting/data/licenses/RVM-GPL-3.0.txt) |
| Advanced Masks 코드·별도 finishing 모듈 | upstream GPL version 2 조건 유지. VEIL wrapper의 later 조항으로 upstream의 권리를 확대하지 않음 | [원문](../third_party/background-engines/advanced/UPSTREAM-LICENSE.txt), [재사용 파일 해시](../third_party/background-engines/advanced-components/PROVENANCE.json) |
| Face Beautifier·Lumina Beauty에서 사용한 부분 | MIT 저작권·허가문 유지 | [Face Beautifier](../third_party/face-beautifier/LICENSE), [Lumina](../third_party/face-tracking/LICENSE-LUMINA.txt) |
| MediaPipe 얼굴 모델 2개 | Apache-2.0 및 모델 고지 | [원문](../models/LICENSE-Apache-2.0.txt), [고지](../models/NOTICE.md) |
| BMv2 모델·보관 소스 | MIT. 현재 기능은 비활성이지만 포함된 자산의 고지는 유지 | [원문](../third_party/video-matting/data/licenses/BMv2-MIT.txt) |
| Lite 호환 모듈 | GPL-3.0-or-later. 효과·번역 파일 등은 upstream REUSE의 개별 조건 적용 | [원문](../third_party/background-engines/lite/UPSTREAM-LICENSE.txt), [REUSE](../third_party/background-engines/lite/data/REUSE.toml) |
| ONNX Runtime CPU·DirectML 패키지 | MIT 및 포함된 third-party notices | [CPU](../third_party/onnxruntime/LICENSE), [matting runtime](../third_party/video-matting/data/licenses/ONNX-Runtime-LICENSE.txt) |
| Microsoft DirectML DLL | Microsoft 전용 소프트웨어 사용·재배포 조건. 오픈소스 라이선스가 아님 | [전체 원문](../third_party/video-matting/data/licenses/DirectML-LICENSE.txt), [고지](../third_party/video-matting/data/licenses/DirectML-ThirdPartyNotices.txt) |
| OBS SDK·컴파일러 런타임·테스트 이미지 | 각 upstream 라이선스·예외·자산 고지 | [외부 구성요소 고지](../THIRD-PARTY-NOTICES.md) |

## 배포할 때

원래의 저작권·LICENSE·NOTICE·수정 내역을 함께 유지하세요. GPL 적용 바이너리를
재배포할 때에는 그 버전에 해당하는 소스와 빌드·설치에 필요한 자료를 해당 GPL의
조건에 맞게 제공해야 합니다. 이 저장소의 원본 소스 ZIP, 수정된 VEIL 소스,
패치 및 외부 의존성 출처는 [DEPENDENCY-SOURCES.md](DEPENDENCY-SOURCES.md)에 연결합니다.
외부 바이너리·모델을 다른 버전으로 교체하면 해당 자료도 함께 갱신해야 합니다.

Microsoft DirectML의 별도 조건은 그대로 적용됩니다. Windows/Xbox 용도와
재배포 범위·고지 유지 조건을 포함하므로, 이 DLL을 GPL이나 MIT 자산으로 표시하지
마세요. 모듈을 별도 DLL로 둔 구현 사실만으로 모든 형태의 재배포에 대한 라이선스
호환성이 입증되는 것은 아닙니다.

이번 공개는 소스 저장소와 명시된 upstream 의존성의 출처 공개입니다. 별도의
공개 설치 프로그램 Release는 만들지 않습니다. upstream 전체 DLL을 새로 배포할
때에는 아래 의존성 소스 경로까지 해당 바이너리의 정확한 버전과 맞춰야 합니다.
고지 파일만으로 모든 재배포 의무를 대신하지 않습니다.

## 기준 문서

- [GNU GPL FAQ · 라이선스 호환성과 소스 제공](https://www.gnu.org/licenses/gpl-faq.en.html)
- [GNU GPL version 2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
- [GNU GPL version 3](https://www.gnu.org/licenses/gpl-3.0.html)

이 문서는 사용한 자산과 소스 위치를 정확하게 설명하기 위한 프로젝트 고지이며,
upstream이 부여한 권리를 변경하거나 새로운 예외를 만드는 문서가 아닙니다.

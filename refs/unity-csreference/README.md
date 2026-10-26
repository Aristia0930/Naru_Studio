# Unity 공개 참고 소스 재현

원본: https://github.com/Unity-Technologies/UnityCsReference

조사 리비전: `bf6028387a4fc2ea6d41aa7d8998e774655aedf6`. 이 리비전은 7000.0.0a12 참고 소스이며, 설치 6000.6.5f1과 동일 빌드라고 검증된 것이 아니다.

`acquisition.json`에 선택한 7개 C# 파일과 LICENSE/README의 URL·크기·SHA-256을 보관한다. 원문 `source/`는 로컬 참고 자료로만 두며 Git에 올리지 않는다. 원문의 README가 수정/재배포를 허용하지 않는다고 명시한다.

다른 PC에서는 저장소 루트에서 실행한다:

```powershell
python tools/acquire_unity_reference.py
```

Python 표준 라이브러리와 네트워크만 사용하며 기본값은 조사 당시 고정 커밋이다. `source/` 아래에 같은 선택 파일들을 다시 받는다. 전체 Unity C++ 엔진 소스나 설치 DLL을 받는 도구는 아니다. 설치 DLL의 관측을 재현하려면 해당 Unity 설치를 별도로 준비한다.

분석 결과와 REA 근거는 [조사 문서](../../analysis/unity/02_REA_및_공개소스_조사.md)에 있다. Naru 자체 엔진 개발을 위해 이 코드나 Unity 설치가 필수인 것은 아니다.

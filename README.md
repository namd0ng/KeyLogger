# Hook-Based DLL Injection Learning Project

## 프로젝트 개요
이 프로젝트는 **교육 목적**으로 `SetWindowsHookEx()`를 이용한 Hook 기반 DLL Injection 기법을 학습하기 위한 예제입니다.
Red Team/모의해킹 교육 및 Windows 내부 동작 원리 학습을 위해 제작되었습니다.

⚠️ **경고**: 이 코드는 교육 목적으로만 사용되어야 하며, 승인받지 않은 시스템에 대한 무단 침투 행위는 불법입니다.

## 프로젝트 구성

```
project-root/
├── Test/
│   ├── HookLoader/         # Hook 기반 DLL 주입 (SetWindowsHookEx)
│   │   └── main.cpp
│   └── HookDLL/            # Hook Procedure를 포함한 DLL
│       └── dllmain.cpp
├── LICENSE                 # MIT License
└── README.md
```

### HookLoader (Hook Injector)
- `SetWindowsHookEx()`를 사용한 전역 훅 설치
- DLL에서 export된 hook procedure를 시스템 전체에 설치
- 메시지 루프를 통해 훅 유지
- CTRL+C로 훅 해제 및 종료

### HookDLL (Hook Procedure DLL)
- Keyboard hook procedure (`WH_KEYBOARD_LL`) export
- 키 이벤트를 `C:\hook_log.txt`에 기록 (교육 목적)
- `CallNextHookEx()`로 다음 훅 체인 호출
- 전역 훅이므로 시스템 전체 키보드 이벤트 모니터링

## 기술 특징

### Hook 기반 DLL Injection의 장점
✅ **정상적인 Windows 메커니즘 활용**: OS가 자동으로 DLL을 각 프로세스에 주입
✅ **CreateRemoteThread 불필요**: 수동 메모리 조작 없이 주입 가능
✅ **합법적 사용 사례 존재**: IME, 접근성 도구, 디버거 등에서 사용

### Hook 기반 DLL Injection의 단점
❌ **쉬운 탐지**: EDR/AV가 의심스러운 훅 활동을 모니터링
❌ **메시지 루프 필수**: Hook을 유지하려면 프로세스가 계속 실행되어야 함
❌ **권한 요구**: 시스템 전체 훅은 관리자 권한이 필요할 수 있음

## 빌드 방법

### Visual Studio 사용

#### 1. HookDLL 빌드 (Hook DLL 프로젝트)
1. Visual Studio 실행
2. "새 프로젝트 만들기" → "빈 프로젝트" 선택
3. 프로젝트 이름: `HookDLL`
4. 프로젝트 속성 설정:
   - 구성: `모든 구성`
   - `구성 속성` → `일반` → `구성 형식`: **동적 라이브러리(.dll)**
5. `Test/HookDLL/dllmain.cpp` 파일을 프로젝트에 추가
6. 빌드: `Ctrl+Shift+B`
7. 출력: `x64/Debug/HookDLL.dll` 또는 `x64/Release/HookDLL.dll`

#### 2. HookLoader 빌드 (Hook Loader 프로젝트)
1. Visual Studio 실행
2. "새 프로젝트 만들기" → "빈 프로젝트" 선택
3. 프로젝트 이름: `HookLoader`
4. 프로젝트 속성 설정:
   - 구성: `모든 구성`
   - `구성 속성` → `일반` → `구성 형식`: **응용 프로그램(.exe)**
   - `구성 속성` → `고급` → `문자 집합`: **멀티바이트 문자 집합 사용**
5. `Test/HookLoader/main.cpp` 파일을 프로젝트에 추가
6. 빌드: `Ctrl+Shift+B`
7. 출력: `x64/Debug/HookLoader.exe` 또는 `x64/Release/HookLoader.exe`

### 빌드 순서
1. 먼저 **HookDLL** 프로젝트를 빌드하여 `HookDLL.dll` 생성
2. 그 다음 **HookLoader** 프로젝트를 빌드하여 `HookLoader.exe` 생성

## 실행 방법

**실행 (명령줄 인자 불필요):**
```cmd
HookLoader.exe
```

HookLoader는 Visual Studio 빌드 구조를 기반으로 자동으로 HookDLL.dll을 찾습니다:
- **Debug 빌드**: `../../HookDLL/x64/Debug/HookDLL.dll`
- **Release 빌드**: `../../HookDLL/x64/Release/HookDLL.dll`

**예상 동작:**
1. HookLoader.exe 실행 (인자 없이)
2. Visual Studio 빌드 구조에서 자동으로 HookDLL.dll 탐색
3. DLL이 로드되면 MessageBox 표시: "Hook DLL Loaded!"
4. 시스템 전체 키보드 훅이 설치됨
5. 키 이벤트가 `C:\hook_log.txt`에 기록됨
6. CTRL+C 또는 창 종료 시 훅 해제 및 DLL 언로드
7. 언로드 시 MessageBox 표시: "Hook DLL is being unloaded."

⚠️ **주의**: HookLoader는 시스템 전체 키보드 훅을 설치하므로 **관리자 권한**이 필요할 수 있습니다.

### 안전한 테스트 방법
1. **가상 머신 사용**: Windows VM에서 테스트 (VMware, VirtualBox, Hyper-V)
2. **스냅샷 생성**: 테스트 전 VM 스냅샷 생성
3. **로그 파일 확인**: `C:\hook_log.txt` 파일에서 이벤트 확인
4. **종료 방법**: CTRL+C 또는 프로세스 종료로 훅 해제

## 실행 예제
```
> HookLoader.exe
======================================
  Hook-Based DLL Injection Test
  Educational Purpose Only
======================================

[*] Build Configuration: Debug
[*] Executable directory: C:\Users\User\KeyLogger\Test\HookLoader\x64\Debug

[*] Loading HookDLL.dll from Visual Studio build structure...
[*] Trying Debug build: ..\..\HookDLL\x64\Debug\HookDLL.dll
[+] DLL loaded from Debug build!
[+] Full path: C:\Users\User\KeyLogger\Test\HookDLL\x64\Debug\HookDLL.dll
[+] DLL loaded successfully! Handle: 0x00007FF8XXXXXXXX

[*] Getting hook procedure address...
[+] Hook procedure found at: 0x00007FF8XXXXXXXX
[*] Installing keyboard hook (WH_KEYBOARD_LL)...
[+] Hook installed successfully! Handle: 0x000000000XXXXXXX
[+] Hook handle passed to DLL

[*] Hook is now active (system-wide)
[*] Events will be logged to C:\hook_log.txt
[*] Press CTRL+C or close this window to unhook and exit

[키보드 입력 시 자동으로 로그 파일에 기록됨]
```

## 학습 포인트

### 1. DLL 로딩 메커니즘
- `LoadLibraryA()`: DLL을 프로세스 주소 공간에 로드
- `FreeLibrary()`: 로드된 DLL을 언로드
- `HMODULE`: 로드된 모듈의 핸들
- `GetProcAddress()`: DLL에서 export된 함수의 주소 가져오기

### 2. DllMain 함수
- DLL의 진입점 함수
- `DLL_PROCESS_ATTACH`: 프로세스가 DLL을 로드할 때
- `DLL_PROCESS_DETACH`: 프로세스가 DLL을 언로드할 때
- `DLL_THREAD_ATTACH/DETACH`: 스레드 생성/종료 시

### 3. Windows Hook 메커니즘 (SetWindowsHookEx)
- `SetWindowsHookEx()`: 시스템 이벤트를 가로채는 훅 설치
- `WH_KEYBOARD_LL`: Low-level 키보드 훅 (시스템 전체)
- `CallNextHookEx()`: 다음 훅 프로시저로 이벤트 전달
- `UnhookWindowsHookEx()`: 훅 해제
- **메시지 루프 필수**: 훅이 작동하려면 `GetMessage()` 루프 필요
- **전역 훅**: DLL 핸들을 지정하면 시스템 전체에 DLL 주입됨

### 4. Hook 기반 DLL Injection의 원리
- **정상 메커니즘**: Windows가 전역 훅을 위해 DLL을 각 프로세스에 자동 주입
- **차이점**: CreateRemoteThread 방식과 달리 Windows가 자동으로 처리
- **활용**: 키보드/마우스 모니터링, 접근성 도구, IME(Input Method Editor)
- **탐지**: EDR/안티바이러스가 의심스러운 훅 활동을 모니터링

### 5. 응용 가능한 기법
- **DLL Injection**: 다른 프로세스에 DLL을 주입하는 기법
- **Code Injection**: 악성코드 분석 및 방어 기법 이해
- **Hooking**: API 후킹 및 동작 변경
- **Process Monitoring**: 시스템 활동 모니터링

## 다음 단계

이 기본 예제를 이해한 후, 다음 주제를 학습할 수 있습니다:

1. **SetWindowsHookEx를 이용한 Hook 기반 주입** ✅ (이미 구현됨 - HookLoader/HookDLL)
   - 전역 훅을 통한 시스템 전체 DLL 주입
   - 키보드/마우스 이벤트 모니터링
   - 메시지 루프와 훅 체인 이해

2. **CreateRemoteThread를 이용한 DLL Injection**
   - 다른 프로세스의 메모리에 DLL 경로 작성
   - 원격 스레드 생성으로 LoadLibrary 호출
   - WriteProcessMemory와 VirtualAllocEx 활용

3. **Process Hollowing**
   - 정상 프로세스를 생성하고 메모리를 교체
   - Process Injection의 고급 기법

4. **Reflective DLL Injection**
   - 파일 시스템을 거치지 않고 메모리에서 직접 DLL 로드
   - 디스크 기반 탐지 우회

5. **API Hooking**
   - IAT/EAT 후킹
   - Inline 후킹 (Detours, MinHook 등)

6. **Hook 탐지 및 방어**
   - 의심스러운 훅 탐지 도구 개발
   - EDR 우회 기법 이해 (방어 관점)

## 문제 해결 (Troubleshooting)

### DLL 로드 실패
- **원인**: HookDLL.dll을 찾을 수 없음
- **해결**:
  - Visual Studio에서 HookDLL 프로젝트를 먼저 빌드했는지 확인
  - 빌드 구성이 일치하는지 확인 (Debug/Release)
  - 예상 경로 확인:
    - `Test/HookDLL/x64/Debug/HookDLL.dll` (Debug 빌드)
    - `Test/HookDLL/x64/Release/HookDLL.dll` (Release 빌드)

### 훅이 설치되지 않음 (SetWindowsHookEx 실패)
- **원인**: 권한 부족 또는 Hook procedure export 실패
- **해결**:
  - 관리자 권한으로 실행
  - DLL이 제대로 빌드되었는지 확인
  - `GetLastError()`로 에러 코드 확인

### 훅이 이벤트를 캡처하지 않음
- **원인**: 메시지 루프가 실행되지 않음
- **해결**:
  - `GetMessage()` 루프가 정상 실행되는지 확인
  - DLL의 hook procedure가 올바르게 export되었는지 확인 (Dependency Walker 사용)

### 안티바이러스가 차단함
- **원인**: 키로깅 동작으로 인식
- **해결**:
  - Windows Defender 실시간 보호 일시 비활성화 (VM 환경에서만)
  - 빌드 출력 폴더를 제외 목록에 추가
  - 코드 서명 (실제 애플리케이션 배포 시)

### C:\hook_log.txt에 아무것도 기록되지 않음
- **원인**: 파일 접근 권한 또는 hook procedure 미실행
- **해결**:
  - C:\ 드라이브에 쓰기 권한 확인
  - 로그 경로를 사용자 폴더로 변경 (`C:\Users\<username>\hook_log.txt`)
  - `fflush()` 호출로 버퍼 즉시 플러시

## 주의사항

- 이 프로젝트는 **오직 교육 목적**으로만 사용해야 합니다.
- 승인받지 않은 시스템에서 사용 시 법적 책임이 따를 수 있습니다.
- 모의해킹 실습은 반드시 **격리된 환경**에서 진행하세요.
- 안티바이러스 소프트웨어가 이 코드를 탐지할 수 있습니다.
- **타인의 시스템에서 절대 실행하지 마세요** - 법적 처벌 대상입니다.

## 라이선스

이 프로젝트는 MIT License 하에 배포됩니다. 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

**교육 목적 고지**: 이 소프트웨어는 교육 및 연구 목적으로만 제공됩니다. 사용자는 모든 관련 법률 및 규정을 준수할 책임이 있습니다.

## 참고 자료

### 공식 문서
- [Microsoft Docs - SetWindowsHookEx](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw)
- [Microsoft Docs - Hooks Overview](https://docs.microsoft.com/en-us/windows/win32/winmsg/hooks)
- [Microsoft Docs - DllMain](https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain)
- [Microsoft Docs - Low-Level Keyboard Hook](https://docs.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)

### 보안 프레임워크
- [MITRE ATT&CK - T1055 (Process Injection)](https://attack.mitre.org/techniques/T1055/)
- [MITRE ATT&CK - T1056.001 (Input Capture: Keylogging)](https://attack.mitre.org/techniques/T1056/001/)

### 도서
- **Windows Internals** by Mark Russinovich
- **Practical Malware Analysis** by Michael Sikorski
- **The Rootkit Arsenal** by Bill Blunden

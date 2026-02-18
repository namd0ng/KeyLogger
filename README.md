# CreateRemoteThread DLL Injection Learning Project

## 프로젝트 개요
이 프로젝트는 **교육 목적**으로 `CreateRemoteThread()`를 이용한 DLL Injection 기법을 학습하기 위한 예제입니다.
Red Team/모의해킹 교육 및 Windows 내부 동작 원리 학습을 위해 제작되었습니다.

⚠️ **경고**: 이 코드는 교육 목적으로만 사용되어야 하며, 승인받지 않은 시스템에 대한 무단 침투 행위는 불법입니다.

## 프로젝트 구성

```
Main/
├── Common/
│   └── constants.h         # 공유 상수 (대상 프로세스, DLL 이름, C2 설정 등)
├── Injector/               # injector.exe - DLL을 대상 프로세스에 주입
│   ├── main.cpp            # 메인 진입점 (4단계 플로우)
│   ├── injection.cpp       # CreateRemoteThread 주입 로직
│   ├── persistence.cpp     # 레지스트리 지속성 설정
│   └── process_utils.cpp   # 프로세스 탐색, 아키텍처 검증
└── Payload/                # payload_*.dll - 주입될 키로거 DLL
    ├── dllmain.cpp         # DLL 진입점 (키로거 스레드 & 네트워크 초기화)
    ├── keylogger.cpp/h     # 키보드 폴링 기반 키로거
    ├── logger.cpp/h        # 로그 파일 기록
    └── network.cpp/h       # C2 서버 통신 (TCP)
```

### Injector (injector.exe)
대상 프로세스(`explorer.exe`)에 payload DLL을 주입하는 실행 파일입니다. 4단계로 동작합니다:

1. **[Phase 1/4] 지속성 설정**: 레지스트리 Run 키에 injector 경로를 등록 (`HKCU\...\Run` → `WindowsUpdate`)
2. **[Phase 2/4] 대상 프로세스 탐색**: `CreateToolhelp32Snapshot()`으로 `explorer.exe`의 PID 탐색
3. **[Phase 3/4] 중복 주입 방지**: `TH32CS_SNAPMODULE`로 DLL이 이미 로드되었는지 확인
4. **[Phase 4/4] DLL 주입**: `injection.cpp`의 `CreateRemoteThread` 기법으로 주입 수행

`--uninstall` 플래그로 레지스트리 지속성을 제거할 수 있습니다.

### Payload (payload_*.dll)
주입된 후 `explorer.exe` 내부에서 실행되는 DLL입니다. `DllMain`의 `DLL_PROCESS_ATTACH`에서:
- `InitNetwork()`: `config.ini`를 읽어 C2 서버에 TCP 연결, 송신 스레드 시작
- `StartKeylogger()`: 키보드 폴링 스레드 시작 (`GetAsyncKeyState()`, 10ms 간격)

키 입력 데이터는 C2 서버로 전송되며, 연결 실패 시 `%TEMP%\keylog_fallback.txt`에 기록됩니다.

## 기술 특징

### CreateRemoteThread DLL Injection의 주입 흐름
`injection.cpp`에서 5단계로 구현됩니다:

1. `OpenProcess(PROCESS_ALL_ACCESS, ...)` — 대상 프로세스 핸들 획득
2. `VirtualAllocEx(...)` — 대상 프로세스 메모리에 DLL 경로 크기만큼 할당
3. `WriteProcessMemory(...)` — 할당된 메모리에 DLL 절대 경로 문자열 기록
4. `GetProcAddress(kernel32, "LoadLibraryA")` — Injector 자신의 주소 공간에서 `LoadLibraryA` 주소 획득
5. `CreateRemoteThread(..., loadLibraryAddr, remoteMem, ...)` — 대상 프로세스에서 `LoadLibraryA(dllPath)` 실행

### CreateRemoteThread 방식의 특징
✅ **명시적 제어**: 주입 대상 프로세스와 DLL을 직접 지정
✅ **Hook 불필요**: `SetWindowsHookEx`와 달리 메시지 루프 없이 동작
✅ **널리 연구됨**: 가장 대표적인 DLL Injection 기법, 방어 기법 이해에 필수
❌ **쉬운 탐지**: EDR/AV가 `CreateRemoteThread` + `LoadLibrary` 패턴을 적극 모니터링
❌ **아키텍처 일치 필수**: Injector, Payload DLL, 대상 프로세스가 모두 동일 아키텍처여야 함
❌ **관리자 권한 필요**: `OpenProcess(PROCESS_ALL_ACCESS)`는 높은 권한을 요구

## 실행 방법

### 사전 요구사항
- **Windows** (x64, x86, ARM64 지원)
- **관리자 권한** 으로 실행
- Injector와 Payload DLL이 **같은 디렉터리**에 있어야 함

### 파일 배치
```
your_directory/
├── injector.exe        # Injector 실행 파일
├── payload_x64.dll     # x64 아키텍처용 Payload (또는 payload_arm64.dll / payload_x86.dll)
└── config.ini          # (선택) C2 서버 설정
```

> **아키텍처 주의**: `explorer.exe`가 x64이면 `injector.exe`와 `payload_x64.dll`을 사용해야 합니다.
> 아키텍처 불일치 시 `CreateRemoteThread`가 실패합니다.

### config.ini (선택)
C2 서버 설정이 없으면 기본값(`127.0.0.1:5555`)을 사용합니다.
```ini
[C2]
ip=192.168.1.100
port=5555
```

### 실행
```cmd
# 관리자 권한 명령 프롬프트에서:
injector.exe

# 제거 (레지스트리 지속성 삭제):
injector.exe --uninstall
```

### 안전한 테스트 방법
1. **가상 머신 사용**: Windows VM에서 테스트 (VMware, VirtualBox, Hyper-V)
2. **스냅샷 생성**: 테스트 전 VM 스냅샷 생성
3. **로그 확인**: C2 서버 수신 데이터 또는 `%TEMP%\keylog_fallback.txt` 확인
4. **제거**: `injector.exe --uninstall` 실행 후 `explorer.exe` 재시작

## 실행 예제
```
> injector.exe
======================================
  DLL Injection - Injector
======================================

[Phase 1/4] Setting up persistence

[*] Adding to startup registry...
[*] Executable path: C:\Users\User\test\injector.exe
[+] Successfully added to startup!
[+] Registry Key: HKCU\Software\Microsoft\Windows\CurrentVersion\Run
[+] Value Name: WindowsUpdate

[Phase 2/4] Finding target process
[+] Found explorer.exe (PID: 5432)

[Phase 3/4] Checking for existing injection
[*] DLL not found in target process. Proceeding with injection.

[Phase 4/4] Preparing DLL injection
[*] Expected DLL location: C:\Users\User\test\payload_x64.dll

[*] Starting DLL injection into PID: 5432
[*] DLL Path: C:\Users\User\test\payload_x64.dll
[*] Step 1: Opening target process...
[+] Process opened successfully. Handle: 0x00000000000000AC
[*] Step 2: Allocating memory in target process...
[+] Memory allocated at: 0x000001F3XXXXXXXX (Size: 41 bytes)
[*] Step 3: Writing DLL path to target process memory...
[+] DLL path written successfully (41 bytes)
[*] Step 4: Getting LoadLibraryA address...
[+] LoadLibraryA address: 0x00007FF8XXXXXXXX
[*] Step 5: Creating remote thread to load DLL...
[+] Remote thread created successfully. Handle: 0x00000000000000B0
[*] Waiting for DLL to load...
[+] DLL load thread completed
[+] DLL loaded successfully! Module handle: 0x7FF8XXXXXXXX
[*] Cleaning up...
[+] DLL injection completed successfully!

======================================
[+] All tasks completed successfully!
======================================

[*] The payload is now running inside explorer.exe
[*] Keystrokes will be logged to: C:\Users\User\AppData\Local\Temp\syslog.txt
[*] This injector will now exit.
[*] The payload will continue running in the background.

[*] To uninstall: Run with --uninstall flag
[*] To verify: Use Process Explorer to check DLLs in explorer.exe
```

## 학습 포인트

### 1. CreateRemoteThread DLL Injection 원리
- **왜 `LoadLibraryA` 주소를 Injector에서 구하는가?**
  `kernel32.dll`은 모든 프로세스에서 동일한 주소에 매핑된다는 Windows의 ASLR 예외 덕분에, Injector 자신의 주소 공간에서 구한 `LoadLibraryA` 주소를 그대로 대상 프로세스에서 사용할 수 있습니다.
- **왜 절대 경로가 필요한가?**
  `CreateRemoteThread`로 호출되는 `LoadLibraryA`는 대상 프로세스의 컨텍스트에서 실행되므로, 상대 경로는 Injector 기준이 아닌 대상 프로세스 기준으로 해석됩니다.

### 2. VirtualAllocEx / WriteProcessMemory
- `VirtualAllocEx()`: 다른 프로세스의 가상 주소 공간에 메모리 할당
- `WriteProcessMemory()`: 다른 프로세스의 메모리에 데이터 기록
- 이 두 API는 프로세스 간 메모리 조작의 핵심으로, 다양한 인젝션 기법에서 활용됩니다.

### 3. DllMain 함수
- DLL의 진입점으로, 로드/언로드 시점에 호출됩니다.
- `DLL_PROCESS_ATTACH`: 주입 직후 실행 — 키로거 스레드와 네트워크 모듈을 초기화합니다.
- `DLL_PROCESS_DETACH`: 언로드 시 — 스레드 종료 및 소켓 정리를 수행합니다.
- `DllMain` 내부에서 수행 가능한 작업은 [Loader Lock](https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices) 제약으로 제한됩니다.

### 4. 아키텍처 호환성
- `CreateRemoteThread`는 Injector와 대상 프로세스의 아키텍처가 일치해야 합니다.
- `process_utils.cpp`의 `ValidateArchitectureMatch()`는 `IsWow64Process2()`(Win10 1709+)로 정확한 아키텍처를 확인하며, 구형 Windows에서는 `IsWow64Process()`로 폴백합니다.

### 5. 지속성 (Registry Run Key)
- `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`에 등록하면 로그인 시 자동 실행됩니다.
- HKCU(현재 사용자)는 관리자 권한 없이도 쓰기 가능하다는 점이 특징입니다.
- `--uninstall` 플래그로 `RegDeleteValueA()`를 호출해 제거합니다.

### 6. 이중 주입 방지
- `TH32CS_SNAPMODULE`로 대상 프로세스의 로드된 모듈 목록을 확인합니다.
- 동일 DLL이 이미 로드되어 있으면 주입을 건너뜁니다.
- 이는 실제 악성코드에서도 자주 사용하는 방어적 설계 패턴입니다.

## 주의사항

- 이 프로젝트는 **오직 교육 목적**으로만 사용해야 합니다.
- 승인받지 않은 시스템에서 사용 시 법적 책임이 따를 수 있습니다.
- 모의해킹 실습은 반드시 **격리된 환경**에서 진행하세요.
- 안티바이러스 소프트웨어가 이 코드를 탐지할 수 있습니다.
- **타인의 시스템에서 절대 실행하지 마세요** — 법적 처벌 대상입니다.

## 라이선스

이 프로젝트는 MIT License 하에 배포됩니다. 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

**교육 목적 고지**: 이 소프트웨어는 교육 및 연구 목적으로만 제공됩니다. 사용자는 모든 관련 법률 및 규정을 준수할 책임이 있습니다.

## 참고 자료

### 공식 문서
- [Microsoft Docs - CreateRemoteThread](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createremotethread)
- [Microsoft Docs - VirtualAllocEx](https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualallocex)
- [Microsoft Docs - WriteProcessMemory](https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-writeprocessmemory)
- [Microsoft Docs - DllMain](https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain)
- [Microsoft Docs - DLL Best Practices (Loader Lock)](https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices)

### 보안 프레임워크
- [MITRE ATT&CK - T1055.001 (Process Injection: DLL Injection)](https://attack.mitre.org/techniques/T1055/001/)
- [MITRE ATT&CK - T1547.001 (Registry Run Keys / Startup Folder)](https://attack.mitre.org/techniques/T1547/001/)
- [MITRE ATT&CK - T1056.001 (Input Capture: Keylogging)](https://attack.mitre.org/techniques/T1056/001/)

### 도서
- **Windows Internals** by Mark Russinovich
- **Practical Malware Analysis** by Michael Sikorski
- **The Rootkit Arsenal** by Bill Blunden

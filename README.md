# WinHvShellcodeEmulator

WinHvShellcodeEmulator (WHSE) is a shellcode emulator leveraging the Windows Hypervisor Platform API [\[1\]](https://docs.microsoft.com/en-us/virtualization/api/hypervisor-platform/hypervisor-platform).

The project is based on three components :
* WinHvEmulator : The emulation library taking charge of partition management, virtual processor management, memory allocation and so on.
* WinHvShellcodeEmulator : The actual emulator taking charge of properly setting up the virtual CPU registers, managing guest exits and so on.
* WinHvShellcodeContainer : An AppContainer isolation [\[2\]](https://docs.microsoft.com/en-us/windows/win32/secauthz/appcontainer-for-legacy-applications-)


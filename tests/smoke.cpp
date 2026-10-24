#include <windows.h>
#include <stdio.h>
int wmain(int argc,wchar_t** argv) {
 if(argc!=2)return 1;
 HMODULE mod=LoadLibraryW(argv[1]);
 if(!mod){printf("LoadLibrary failed: %lu\n",GetLastError());return 2;}
 wchar_t self[MAX_PATH];GetModuleFileNameW(NULL,self,MAX_PATH);
 HANDLE f=CreateFileW(self,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
 char b[8];DWORD got;
 if(!ReadFile(f,b,8,&got,NULL)||got!=8)return 3;
 if(!ReadFile(f,b,8,&got,NULL)||got!=8)return 4;
 CloseHandle(f);
 auto tick=(DWORD(WINAPI*)())GetProcAddress(mod,"timeGetTime");
 auto devices=(UINT(WINAPI*)())GetProcAddress(mod,"waveOutGetNumDevs");
 if(!tick||!devices)return 5;
 DWORD a=tick();Sleep(30);DWORD delta=tick()-a;
 if(delta<10||delta>2000)return 6;
 printf("PASS: x86 DLL load, bootstrap ReadFile calls, timeGetTime forwarding (%lu ms), waveOutGetNumDevs (%u).\n",delta,devices());
 return 0;
}

#include <windows.h>
#include <string.h>
extern "C" BOOL WINAPI _DllMainCRTStartup(HINSTANCE,DWORD,LPVOID);
extern "C" void __cdecl ModInitialize();
extern "C" BOOL WINAPI ModReadFile(HANDLE,void*,DWORD,DWORD*,OVERLAPPED*);
static FARPROC targets[256];
static const char* exportNames[256];
static HMODULE realWinmm;
static INIT_ONCE attachOnce=INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK Attach(PINIT_ONCE,void*,void**) { ModInitialize(); return TRUE; }
static BOOL WINAPI FirstRead(HANDLE h,void* b,DWORD n,DWORD* got,OVERLAPPED* ov) {
 DWORD error=GetLastError();
 InitOnceExecuteOnce(&attachOnce,Attach,NULL,NULL);
 SetLastError(error);
 return ModReadFile(h,b,n,got,ov);
}
static void InstallBootstrap() {
 BYTE* b=(BYTE*)GetModuleHandleW(NULL);
 auto nt=(IMAGE_NT_HEADERS*)(b+((IMAGE_DOS_HEADER*)b)->e_lfanew);
 DWORD rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
 if(!rva)return;
 for(auto d=(IMAGE_IMPORT_DESCRIPTOR*)(b+rva);d->Name;++d) {
  if(!d->OriginalFirstThunk)continue;
  auto names=(IMAGE_THUNK_DATA*)(b+d->OriginalFirstThunk);
  auto entries=(IMAGE_THUNK_DATA*)(b+d->FirstThunk);
  for(;names->u1.AddressOfData;++names,++entries) {
   if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
   auto name=(IMAGE_IMPORT_BY_NAME*)(b+names->u1.AddressOfData);
   if(strcmp((char*)name->Name,"ReadFile"))continue;
   DWORD old;
   if(VirtualProtect(&entries->u1.Function,sizeof(ULONG_PTR),PAGE_READWRITE,&old)) {
    entries->u1.Function=(ULONG_PTR)FirstRead;
    DWORD ignored;VirtualProtect(&entries->u1.Function,sizeof(ULONG_PTR),old,&ignored);
   }
   return;
  }
 }
}
static void __cdecl Resolve(int index) {
 DWORD error=GetLastError();
 if(!realWinmm) {
  wchar_t path[MAX_PATH];
  if(!GetSystemDirectoryW(path,MAX_PATH))TerminateProcess(GetCurrentProcess(),127);
  wcscat_s(path,L"\\winmm.dll");realWinmm=LoadLibraryW(path);
 }
 targets[index]=GetProcAddress(realWinmm,exportNames[index]);
 if(!targets[index])TerminateProcess(GetCurrentProcess(),127);
 SetLastError(error);
}
#include "proxy-exports.h"
extern "C" BOOL WINAPI ModDllEntry(HINSTANCE h,DWORD reason,LPVOID reserved) {
 if(!_DllMainCRTStartup(h,reason,reserved))return FALSE;
 if(reason==DLL_PROCESS_ATTACH) { InitExports();InstallBootstrap(); }
 return TRUE;
}

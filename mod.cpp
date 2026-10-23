#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#pragma comment(lib,"bcrypt.lib")
#pragma comment(lib,"user32.lib")
struct PatchSet { uint64_t fileSize; unsigned char sha256[32]; };
static const PatchSet patchSets[] = {
{518464345ULL,{0xc3,0x54,0x8a,0x61,0x28,0x70,0xc9,0xa4,0x50,0x8b,0xa8,0x42,0x0d,0xc2,0x10,0x8f,0x46,0xb8,0x96,0xc7,0x47,0x86,0x6c,0x49,0xd7,0xe8,0xdf,0x3a,0x04,0x99,0xac,0xc1}},
{522913104ULL,{0xbe,0xfc,0x40,0x8f,0x7e,0x15,0x57,0x6a,0xaa,0x3a,0xa6,0x6b,0xa6,0x5e,0x8d,0x4d,0xaf,0x3c,0x2f,0x37,0x34,0x77,0x41,0x55,0x95,0xe8,0xc0,0xa8,0x4c,0xc5,0x1a,0xef}},
};
static const size_t PATCH_SET_COUNT=sizeof(patchSets)/sizeof(patchSets[0]);
// Hooks apply only to the validated game and its extracted runtime modules.
static HMODULE selfModule,gameModule;
static HWND gameWindow;
static BOOL (WINAPI *readOriginal)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED)=ReadFile;
static HWND (WINAPI *createOriginal)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID)=CreateWindowExW;
static BOOL (WINAPI *showOriginal)(HWND,int)=ShowWindow;
static BOOL (WINAPI *posOriginal)(HWND,HWND,int,int,int,int,UINT)=SetWindowPos;
static INIT_ONCE validationOnce=INIT_ONCE_STATIC_INIT;
static const PatchSet* activePatch;
static void InstallNativeHooks();
static bool unlockFPS=true;
static wchar_t exePath[MAX_PATH];
static void Log(const char* text){wchar_t path[MAX_PATH];GetModuleFileNameW(selfModule,path,MAX_PATH);wchar_t* slash=wcsrchr(path,L'\\');if(slash)wcscpy_s(slash+1,MAX_PATH-(slash+1-path),L"fnaf4-mod.log");HANDLE f=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,0,NULL);if(f!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(f,text,(DWORD)strlen(text),&n,NULL);WriteFile(f,"\r\n",2,&n,NULL);CloseHandle(f);}}
static BOOL WINAPI Validate(PINIT_ONCE once,PVOID arg,PVOID* context){
 HANDLE file=CreateFileW(exePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
 if(file==INVALID_HANDLE_VALUE){Log("Validation open failed");return TRUE;}
 LARGE_INTEGER size;GetFileSizeEx(file,&size);
 BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;BYTE digest[32],buffer[65536];DWORD got;BOOL okay=FALSE;
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0 && BCryptCreateHash(alg,&hash,NULL,0,NULL,0,0)>=0){
  okay=TRUE;while(ReadFile(file,buffer,sizeof(buffer),&got,NULL)&&got){if(BCryptHashData(hash,buffer,got,0)<0){okay=FALSE;break;}}
  if(okay && BCryptFinishHash(hash,digest,32,0)>=0){for(size_t i=0;i<PATCH_SET_COUNT;i++)if(patchSets[i].fileSize==(uint64_t)size.QuadPart&&!memcmp(digest,patchSets[i].sha256,32)){activePatch=&patchSets[i];break;}}
 }
 Log(activePatch?"Validated supported game":"Game hash not recognized or hash failed");if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);CloseHandle(file);return TRUE;
}
static BOOL IsGameFile(HANDLE file){
 wchar_t path[MAX_PATH+8];DWORD n=GetFinalPathNameByHandleW(file,path,MAX_PATH+8,FILE_NAME_NORMALIZED);
 if(!n||n>=MAX_PATH+8)return FALSE;const wchar_t* p=path;if(!wcsncmp(p,L"\\\\?\\",4))p+=4;return !_wcsicmp(p,exePath);
}
static void ApplyRuntimeSettings(){
 BYTE* app=*(BYTE**)((BYTE*)gameModule+0xac9ac);
 if(!app || *(DWORD*)(app+0x10)!=112 || *(WORD*)(app+0x14)!=0xbc82)return;
 *(WORD*)(app+0x14)=(*(WORD*)(app+0x14)&~0x800)|0x10; // Preserve other flags to keep gameplay audio working.
 app[0x390]=1;
 wchar_t ini[MAX_PATH];GetModuleFileNameW(selfModule,ini,MAX_PATH);wchar_t* slash=wcsrchr(ini,L'\\');if(slash)wcscpy_s(slash+1,MAX_PATH-(slash+1-ini),L"fnaf4-mod.ini");
 unlockFPS=GetPrivateProfileIntW(L"Mod",L"UnlockFPS",1,ini)!=0;
 if(unlockFPS)*(WORD*)(app+0x16)&=~0x800;
 InstallNativeHooks();
 Log("Applied runtime borderless settings after header checksum");
}
static void __cdecl EndApplicationPatched(){
 BYTE* app=*(BYTE**)((BYTE*)gameModule+0xac9ac);
 BYTE* run=*(BYTE**)((BYTE*)gameModule+0xac9b4);
 if(!run)return;
 char msg[100];sprintf_s(msg,"End application action: frame=%d",app?*(int*)(app+0x1f0):-1);Log(msg);
 if(app&&*(int*)(app+0x1f0)==3){*(DWORD*)(run+0x38)=4;*(WORD*)(run+0x30)=3;Log("Escape routed to title frame");}
 else *(WORD*)(run+0x30)=0xfffe;
}
static int __cdecl TickAndRender(){
 BYTE* run=*(BYTE**)((BYTE*)gameModule+0xac9b4);
 DWORD before=run?*(DWORD*)(run+0xd0):0;
 int result=((int(__cdecl*)())((BYTE*)gameModule+0x29f40))();
 run=*(BYTE**)((BYTE*)gameModule+0xac9b4);
 if(run&&result==0&&*(DWORD*)(run+0xd0)==before&&*(DWORD*)(run+0x178)==0)
  ((void(__cdecl*)())((BYTE*)gameModule+0x2b970))();
 return result;
}
static void InstallNativeHooks(){
 BYTE* entry=(BYTE*)gameModule+0xbfe0;
 static const BYTE signature[]={0xb9,0xfe,0xff,0xff,0xff,0x66,0x89,0x48,0x30,0xc3};
 if(entry[0]!=0xa1||memcmp(entry+5,signature,sizeof(signature))){Log("Native action signature mismatch");return;}
 DWORD old;if(VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&old)){
 entry[0]=0xe9;*(DWORD*)(entry+1)=(DWORD)((BYTE*)EndApplicationPatched-entry-5);
 DWORD ignored;VirtualProtect(entry,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),entry,5);Log("Native EndApplication action hooked");}
 if(unlockFPS){
  BYTE* tick=(BYTE*)gameModule+0x45444;const BYTE tickExpected[]={0xe8,0xf7,0x4a,0xfe,0xff};
  if(!memcmp(tick,tickExpected,5)&&VirtualProtect(tick,5,PAGE_EXECUTE_READWRITE,&old)){
   *(DWORD*)(tick+1)=(DWORD)((BYTE*)TickAndRender-tick-5);DWORD ignored;VirtualProtect(tick,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),tick,5);
  }else {Log("FPS tick signature mismatch; FPS unlock skipped");return;}
  BYTE* wait=(BYTE*)gameModule+0x2f21;
  const BYTE expected[]={0x83,0xbe,0xc0,0,0,0,0};
  if(!memcmp(wait,expected,sizeof(expected))&&VirtualProtect(wait,7,PAGE_EXECUTE_READWRITE,&old)){
   // Retain the original message pump, but return before its frame-deadline wait.
   wait[0]=0xe9;*(DWORD*)(wait+1)=0x2f5d-0x2f21-5;wait[5]=wait[6]=0x90;
   DWORD ignored;VirtualProtect(wait,7,old,&ignored);FlushInstructionCache(GetCurrentProcess(),wait,7);
   Log("Rendering wait removed; simulation clock remains 60 Hz");
  }
 }
}
static BOOL WINAPI ReadPatched(HANDLE file,LPVOID buffer,DWORD count,LPDWORD read,LPOVERLAPPED overlap){
 static bool first=true;if(first){Log("ReadFile hook reached");first=false;} LARGE_INTEGER zero={0},position={0};BOOL target=IsGameFile(file);
 if(target){InitOnceExecuteOnce(&validationOnce,Validate,NULL,NULL);if(overlap){position.LowPart=overlap->Offset;position.HighPart=overlap->OffsetHigh;}else if(!SetFilePointerEx(file,zero,&position,FILE_CURRENT))target=FALSE;}
 if(target&&activePatch&&position.QuadPart==1554554)ApplyRuntimeSettings();
 return readOriginal(file,buffer,count,read,overlap);
}

static void Bounds(HWND h,int* x,int* y,int* w,int* ht){
 MONITORINFO m={sizeof(m)};GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST),&m);
 int mw=m.rcMonitor.right-m.rcMonitor.left,mh=m.rcMonitor.bottom-m.rcMonitor.top;
 *w=mw;*ht=mw*9/16;if(*ht>mh){*ht=mh;*w=mh*16/9;}*x=m.rcMonitor.left+(mw-*w)/2;*y=m.rcMonitor.top+(mh-*ht)/2;
}
static BOOL WINAPI PositionPatched(HWND h,HWND after,int x,int y,int w,int ht,UINT flags){
 if(h==gameWindow&&activePatch&&!IsIconic(h)){Bounds(h,&x,&y,&w,&ht);flags&=~(SWP_NOSIZE|SWP_NOMOVE);}
 return posOriginal(h,after,x,y,w,ht,flags);
}
static HWND WINAPI CreatePatched(DWORD ex,LPCWSTR cls,LPCWSTR title,DWORD style,int x,int y,int w,int ht,HWND parent,HMENU menu,HINSTANCE instance,LPVOID param){
 Log("Create window reached");BOOL main=activePatch && !parent && !(style&WS_CHILD) && w>=500 && ht>=300;
 if(main){style&=~(WS_CAPTION|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU|WS_MAXIMIZE);ex&=~(WS_EX_CLIENTEDGE|WS_EX_WINDOWEDGE|WS_EX_DLGMODALFRAME);}
 HWND h=createOriginal(ex,cls,title,style,x,y,w,ht,parent,menu,instance,param);if(main){gameWindow=h;Log("Game window found");}return h;
}
static BOOL WINAPI ShowPatched(HWND h,int mode){
 BOOL result=showOriginal(h,mode);
 if(h==gameWindow&&activePatch&&mode!=SW_HIDE&&mode!=SW_MINIMIZE&&mode!=SW_SHOWMINIMIZED){
  int x,y,w,ht;Bounds(h,&x,&y,&w,&ht);SetWindowLongW(h,GWL_STYLE,GetWindowLongW(h,GWL_STYLE)&~(WS_CAPTION|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU|WS_MAXIMIZE));posOriginal(h,NULL,x,y,w,ht,SWP_NOZORDER|SWP_FRAMECHANGED|SWP_NOACTIVATE);
 }return result;
}
static void Hook(HMODULE module,const char* name,void* replacement){
 BYTE* base=(BYTE*)module;IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)base;IMAGE_NT_HEADERS* nt=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);
 DWORD rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;if(!rva)return;
 IMAGE_IMPORT_DESCRIPTOR* d=(IMAGE_IMPORT_DESCRIPTOR*)(base+rva);
 for(;d->Name;d++){if(!d->OriginalFirstThunk)continue;IMAGE_THUNK_DATA* names=(IMAGE_THUNK_DATA*)(base+d->OriginalFirstThunk);IMAGE_THUNK_DATA* table=(IMAGE_THUNK_DATA*)(base+d->FirstThunk);
  for(;names->u1.AddressOfData;names++,table++){if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;IMAGE_IMPORT_BY_NAME* n=(IMAGE_IMPORT_BY_NAME*)(base+names->u1.AddressOfData);if(strcmp((char*)n->Name,name))continue;
   DWORD old;if(VirtualProtect(&table->u1.Function,sizeof(ULONG_PTR),PAGE_READWRITE,&old)){table->u1.Function=(ULONG_PTR)replacement;DWORD ignored;VirtualProtect(&table->u1.Function,sizeof(ULONG_PTR),old,&ignored);}return;
  }
 }
}
static void HookModule(HMODULE module);
static bool InNight(){
 if(!activePatch)return false;BYTE* app=*(BYTE**)((BYTE*)gameModule+0xac9ac);
 return app&&*(int*)(app+0x1f0)==3;
}
static SHORT WINAPI AsyncKeyPatched(int key){
 SHORT state=GetAsyncKeyState(key);
 if(InNight()){
  if(key==VK_CONTROL||key==VK_LCONTROL)state|=GetAsyncKeyState(VK_LBUTTON)&0x8000;
  if(key==VK_SHIFT||key==VK_LSHIFT)state|=GetAsyncKeyState(VK_RBUTTON)&0x8000;
 }return state;
}
static SHORT WINAPI KeyPatched(int key){
 SHORT state=GetKeyState(key);
 if(InNight()){
  if(key==VK_CONTROL||key==VK_LCONTROL)state|=GetKeyState(VK_LBUTTON)&0x8000;
  if(key==VK_SHIFT||key==VK_LSHIFT)state|=GetKeyState(VK_RBUTTON)&0x8000;
 }return state;
}
static BOOL WINAPI KeyboardPatched(PBYTE keys){
 BOOL ok=GetKeyboardState(keys);if(ok&&InNight()){
  keys[VK_CONTROL]|=keys[VK_LBUTTON]&0x80;keys[VK_LCONTROL]|=keys[VK_LBUTTON]&0x80;
  keys[VK_SHIFT]|=keys[VK_RBUTTON]&0x80;keys[VK_LSHIFT]|=keys[VK_RBUTTON]&0x80;
 }return ok;
}
static HMODULE WINAPI LoadPatchedW(LPCWSTR path){HMODULE m=LoadLibraryW(path);if(m)HookModule(m);return m;}
static HMODULE WINAPI LoadPatchedA(LPCSTR path){HMODULE m=LoadLibraryA(path);if(m)HookModule(m);return m;}
static HMODULE WINAPI LoadExPatchedW(LPCWSTR path,HANDLE file,DWORD flags){HMODULE m=LoadLibraryExW(path,file,flags);if(m&&!(flags&(LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE)))HookModule(m);return m;}
static HMODULE WINAPI LoadExPatchedA(LPCSTR path,HANDLE file,DWORD flags){HMODULE m=LoadLibraryExA(path,file,flags);if(m&&!(flags&(LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE)))HookModule(m);return m;}
static void HookModule(HMODULE module){
 if(module==selfModule)return;wchar_t path[MAX_PATH];GetModuleFileNameW(module,path,MAX_PATH);
 if(module!=gameModule&&!wcsstr(path,L"mrt"))return;
 Log("Hooking game/runtime module");
 Hook(module,"ReadFile",(void*)ReadPatched);
 Hook(module,"GetAsyncKeyState",(void*)AsyncKeyPatched);Hook(module,"GetKeyState",(void*)KeyPatched);Hook(module,"GetKeyboardState",(void*)KeyboardPatched);
 Hook(module,"CreateWindowExW",(void*)CreatePatched);Hook(module,"ShowWindow",(void*)ShowPatched);Hook(module,"SetWindowPos",(void*)PositionPatched);
 Hook(module,"LoadLibraryW",(void*)LoadPatchedW);Hook(module,"LoadLibraryA",(void*)LoadPatchedA);Hook(module,"LoadLibraryExW",(void*)LoadExPatchedW);Hook(module,"LoadLibraryExA",(void*)LoadExPatchedA);
}
static FARPROC targets[256];static HMODULE realWinmm;
static const char* exportNames[256];
static void __cdecl Resolve(int index){
 if(!realWinmm){wchar_t path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);wcscat_s(path,L"\\winmm.dll");realWinmm=LoadLibraryW(path);}
 targets[index]=GetProcAddress(realWinmm,exportNames[index]);if(!targets[index])TerminateProcess(GetCurrentProcess(),127);
}
#include "proxy-exports.h"
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved){
 if(reason==DLL_PROCESS_ATTACH){selfModule=module;DisableThreadLibraryCalls(module);gameModule=GetModuleHandleW(NULL);GetModuleFileNameW(gameModule,exePath,MAX_PATH);Log("DLL attached");InitExports();HookModule(gameModule);}
 return TRUE;
}

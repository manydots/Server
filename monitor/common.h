#pragma once

//#define DEBUG_INFO
#ifdef DEBUG_INFO
#define DPRINTF(...) printf(__VA_ARGS__)  //宏打印函数定义
#else
#define DPRINTF(...)
#endif

#define ROUND_UP(x, align) (((int) (x) + (align - 1)) & ~(align - 1))

#define ENUM_ITEMSPACE_INVENTORY		0 //物品栏
#define ENUM_ITEMSPACE_AVATAR			1 //时装栏
#define ENUM_ITEMSPACE_CARGO			2 //仓库
#define ENUM_ITEMSPACE_CREATURE			7 //宠物栏
#define ENUM_ITEMSPACE_ACCOUNT_CARGO	12 //账号仓库

#define BYTE unsigned char
#define WORD unsigned short
#define DWORD unsigned int

#define RBYTE(ptr, pos)			(*(BYTE*)((int)ptr + (pos)))
#define RWORD(ptr, pos)			(*(WORD*)((int)ptr + (pos)))
#define RDWORD(ptr, pos)		(*(DWORD*)((int)ptr + (pos)))
#define RINT8(ptr, pos)			(*(char*)((int)ptr + (pos)))
#define RINT16(ptr, pos)		(*(short*)((int)ptr + (pos)))
#define RINT32(ptr, pos)		(*(int*)((int)ptr + (pos)))

#define WBYTE(ptr, pos, v)		(*(BYTE*)((int)ptr + (pos)) = (BYTE)v)
#define WWORD(ptr, pos, v)		(*(WORD*)((int)ptr + (pos)) = (WORD)v)
#define WDWORD(ptr, pos, v)		(*(DWORD*)((int)ptr + (pos)) = (DWORD)v)



#define CurrentSecTime			(time(0))

//__attribute__((visibility("hidden")))
//
#define __hide		__attribute__((visibility("hidden")))
#define __noinline  __attribute__((noinline))
#define __inline __attribute__((always_inline))
#define __cdecl		__attribute__((__cdecl__))
#define __stdcall  __attribute__((__stdcall__))
#define __fastcall  __attribute__((__fastcall__))


typedef int(__cdecl* cdeclCall)(void);
typedef int(__cdecl* cdeclCall1)(int);
typedef int(__cdecl* cdeclCall1P)(int*);
typedef BYTE(__cdecl* cdeclBCall1P)(int*);
typedef int(__cdecl* cdeclCall2)(int, int);
typedef int(__cdecl* cdeclCall2P)(int*, int);
typedef float(__cdecl* cdeclFCall2P)(int*, int);
typedef bool(__cdecl* cdeclBCall2P)(int*, int);
typedef int(__cdecl* cdeclCall3)(int, int, int);
typedef int(__cdecl* cdeclCall3P)(int*, int, int);
typedef int(__cdecl* cdeclCall4)(int, int, int, int);
typedef int(__cdecl* cdeclCall4P)(int*, int, int, int);
typedef int(__cdecl* cdeclCall5P)(int*, int, int, int, int);
typedef int(__cdecl* cdeclCall6)(int, int, int, int, int, int);
typedef int(__cdecl* cdeclCall6P)(int*, int, int, int, int, int);
typedef int(__cdecl* cdeclCall7P)(int*, int, int, int, int, int, int);
typedef int(__cdecl* cdeclCall10P)(int *, int, int, int, int, int, int, int, int, int);
typedef int(__cdecl* cdeclCall11)(int, int, int, int, int, int, int, int, int, int, int);


typedef int(__cdecl* cdeclCallType)(int*, const char*, ...);
typedef int(__cdecl* cdeclCallType2)(int*, int , ...);

typedef int(__stdcall* stdCall3P)(int*, int, int);
typedef int(__stdcall* stdCall5P)(int*, int, int, int, int);

extern cdeclCall3P DBMgr_GetDBHandle ;
extern cdeclCall1P MySQL_MySQL ;
extern cdeclCall1P MySQL_init ;
extern cdeclCall6P MySQL_open ;
extern cdeclCall1P MySQL_close ;
extern cdeclCallType MySQL_set_query ;
extern cdeclCall2P MySQL_exec ;
extern cdeclCall1P MySQL_exec_query ;
extern cdeclCall1P MySQL_get_n_rows ;
extern cdeclCall1P MySQL_fetch ;
extern cdeclCall3P MySQL_get_int ;
extern cdeclCall3P MySQL_get_uint ;
extern cdeclCall3P MySQL_get_ulonglong ;
extern cdeclCall3P MySQL_get_float ;
extern cdeclCall1P MySQL_get_ushort ;
extern cdeclCall4P MySQL_get_binary ;
extern cdeclCall2P MySQL_get_binary_length ;
extern cdeclCall4P MySQL_get_str ;
extern cdeclCall4P MySQL_blob_to_str ;
extern cdeclCall4P compress_zip ;
extern cdeclCall4P uncompress_zip;

int* MYSQL_open(const char* db_name, const char* db_ip, int db_port, const char* db_account, const char* db_password);
int MYSQL_exec(int* mysql, const char* sql);
int MYSQL_get_binary(int* mysql, int field_index, BYTE* buf, int maxLen);

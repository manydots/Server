// server.cpp: 定义应用程序的入口点。
//
#include <iostream>
#include <vector>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include "frida-gum.h"
#include "unistd.h"
#include <fcntl.h>
#include "common.h"
#include <unordered_map>

using namespace std;

static inline void  __inline writeJmpCode(DWORD p, void* fn) {
	gum_mprotect((gpointer)p, 1, GUM_PAGE_RWX); *(BYTE*)p = 0xe9;
	gum_mprotect((gpointer)((BYTE*)p + 1), 4, GUM_PAGE_RWX); *(DWORD*)((BYTE*)p + 1) = (DWORD)fn - (DWORD)p - 5;
}

static inline void  __inline writeCallCode(DWORD p, void* fn) {
	gum_mprotect((gpointer)p, 1, GUM_PAGE_RWX); *(BYTE*)p = 0xe8;
	gum_mprotect((gpointer)((BYTE*)p + 1), 4, GUM_PAGE_RWX); *(DWORD*)((BYTE*)p + 1) = (DWORD)fn - (DWORD)p - 5;
}

static inline void  __inline writeByteCode(DWORD p, BYTE code) {
	gum_mprotect((gpointer)p, 1, GUM_PAGE_RWX); *(BYTE*)p = code;
}

static inline void  __inline writeWordCode(DWORD p, WORD code) {
	gum_mprotect((gpointer)p, 2, GUM_PAGE_RWX); *(WORD*)p = code;
}

static inline void  __inline writeDWordCode(DWORD p, DWORD code) {
	gum_mprotect((gpointer)p, 4, GUM_PAGE_RWX); *(DWORD*)p = code;
}

static inline void  __inline writeArrayCode(DWORD p, BYTE* code, DWORD len) {
	gum_mprotect((gpointer)p, len, GUM_PAGE_RWX); 
	for(DWORD i = 0; i < len; ++i)
		*((BYTE*)p + i) = code[i];
}

static inline void  __inline writeNopCode(DWORD p,  DWORD len) {
	gum_mprotect((gpointer)p, len, GUM_PAGE_RWX);
	for (DWORD i = 0; i < len; ++i)
		*((BYTE*)p + i) = 0x90;
}


static cdeclCall2P CVillageAttackedManager__CVillageAttackedManager = (cdeclCall2P)0x080A8406;
static cdeclCall2P CVillageAttackedManager__RequestEventStart = (cdeclCall2P)0x080A9B16;
static cdeclCall2P CVillageAttackedManager__RequestEventEnd = (cdeclCall2P)0x080A9B4C;
static cdeclCall1P CVillageAttackedManager__RequestEventPenaltyEnd = (cdeclCall1P)0x080A9B98;
static cdeclCall3P CVillageAttackedManager__InsertTimer = (cdeclCall3P)0x080A88C6;
static cdeclCall5P CVillageAttackedManager__UpdateHuntingPoint = (cdeclCall5P)0x080A9904;
static cdeclCall2P CVillageAttackedManager__OnCharacLogin = (cdeclCall2P)0x080A963A;
static cdeclCall1P CVillageAttackedManager__ProcessByMinute = (cdeclCall1P)0x080A8A84;
static cdeclCall2P CUserManager__FindUser_CharNo = (cdeclCall2P)0x080713A0;
static cdeclCall1P CUserManager__Size = (cdeclCall1P)0x08093088;
static cdeclCall1P CApplication__Get_ServerGroup = (cdeclCall1P)0x08064F44;
static cdeclCall GetNowTime = (cdeclCall)0x080A857E;

//CApplication *__cdecl CApplication::Load(CApplication *this, int a2, char **a3)
int __cdecl hookCApplication__Load(int* thisP, int a2, int a3) {
	int ret = ((cdeclCall3P)0x08062F4E)(thisP, a2, a3);
	int * CVillageAttackedManager = (int *)malloc(128);
	memset(CVillageAttackedManager, 0, 128);
	CVillageAttackedManager__CVillageAttackedManager(CVillageAttackedManager, (int)thisP);
	WDWORD(thisP, 206 * 4, CVillageAttackedManager);
	return ret;
}

void requestEventStart(int *mgr, DWORD time) {
	
#ifdef TW
	DWORD now = GetNowTime() + 600;
#else
	DWORD now = GetNowTime() + 15;
	*(DWORD*)0x081BAE78 = 15;//village_attacked::COUNTDOWN_FIRST_TIME
	*(DWORD*)0x081BAE7C = 10;//village_attacked::COUNTDOWN_SECOND_TIME
	*(DWORD*)0x081BAE80 = 5;//village_attacked::COUNTDOWN_THIRD_TIME
#endif
	CVillageAttackedManager__InsertTimer(mgr, now, now + time);
}

//int __cdecl CPacketTranslater::OnVillageAttackedGMCommand(CPacketTranslater *this)
int __cdecl CPacketTranslater__OnVillageAttackedGMCommand(int * thisP) {
	int* app = *(int**)0x081BAAB0;
	int *CVillageAttackedManager = (int *)RDWORD(app, 206 * 4);
	int cmd = RBYTE(thisP, 22);

	//CPacketTranslater::OnVillageAttackedGMCommand 0, 0xf5c18b78, 0x8192de0
	//CApplication::Load 0xf5c18b78, 0xf5c18b78, 0x8192de0
	//townatk start     0
	//townatk success   1
	//townatk fail      2
	//townatk end       3
	switch (cmd) {
	case 0: requestEventStart(CVillageAttackedManager, 1800); break;
	case 1: CVillageAttackedManager__RequestEventEnd(CVillageAttackedManager, 1); break;
	case 2: CVillageAttackedManager__RequestEventEnd(CVillageAttackedManager, 0); break;
	case 3: CVillageAttackedManager__RequestEventPenaltyEnd(CVillageAttackedManager); break;
	}
	return 0;
}

//int __cdecl CPacketTranslater::OnVillageMonsterFightResult(CPacketTranslater *this)
int __cdecl CPacketTranslater__OnVillageMonsterFightResult(int* thisP) {
	int param[4] = { 0 }, users[4] = { 0 };
	int* app = *(int**)0x081BAAB0;
	int* CVillageAttackedManager = (int*)RDWORD(app, 206 * 4);
	int* cusermgr = app + 4;

	BYTE isGridClear = RBYTE(thisP, 42);
	for (int i = 0, user, charNo; i < 4; ++i) {
		charNo = RDWORD(thisP, 26 + i * 4);
		if (!charNo) continue;
		users[i] = CUserManager__FindUser_CharNo(cusermgr, charNo);
	}
	// console.log(bin2hex(users, 16));
	// console.log("app:", CVillageAttackedManager.add(7 * 4).readU32(), CVillageAttackedManager.add(8 * 4).readU32());
	CVillageAttackedManager__UpdateHuntingPoint(CVillageAttackedManager, (int)users, isGridClear, (int)param, (int)thisP + 26);
	return 0;
}

//CApplication *__cdecl CPacketTranslater::OnCharLogin(CPacketTranslater *this)
int __cdecl CPacketTranslater__OnCharLogin(int* thisP) {
	int ret = ((cdeclCall1P)0x0807F5A2)(thisP);
	int charaNo = RDWORD(thisP, 15);
	int* app = *(int**)0x081BAAB0;
	int* CVillageAttackedManager = (int*)RDWORD(app, 206 * 4);
	int* cusermgr = app + 4;
	int user = CUserManager__FindUser_CharNo(cusermgr, charaNo);
	CVillageAttackedManager__OnCharacLogin(CVillageAttackedManager, user);
	return ret;
}

//CServerHandler **__cdecl CApplication::ProcessTimeSync(CServerHandler **this)
int __cdecl CApplication__ProcessTimeSync(int* thisP) {
	int* app = *(int**)0x081BAAB0;
	int* CVillageAttackedManager = (int*)RDWORD(app, 206 * 4);
	CVillageAttackedManager__ProcessByMinute(CVillageAttackedManager);
	return ((cdeclCall1P)0x08065CEC)(thisP);
}

static inline void  __inline  villageAttacked(void) {
	writeDWordCode(0x0813A9CC, 4);
	writeDWordCode(0x08121E58, (int)hookCApplication__Load);
	writeDWordCode(0x0807C888, (int)CPacketTranslater__OnVillageAttackedGMCommand);
	writeDWordCode(0x0807C87B, (int)CPacketTranslater__OnVillageMonsterFightResult);
	writeDWordCode(0x0807C4E0, (int)CPacketTranslater__OnCharLogin);
	writeCallCode(0x08064639, (void*)CApplication__ProcessTimeSync);
	writeWordCode(0x080A92A1, 0x0CEB);
}

void __hide on_load(void) {
	gum_init_embedded();
	villageAttacked();
}

void __hide on_unload(void) {
	gum_shutdown();
	gum_deinit_embedded();
}

__attribute__((constructor)) static void
fd_monitor_on_load(void){
//void on_load(void) {
	printf("--------------------------------------------------------------------------fd_monitor_on_load begin\n");
	on_load();
	printf("--------------------------------------------------------------------------fd_monitor_on_load end\n");

}

__attribute__((destructor)) static void
fd_monitor_on_unload(void){
//void on_unload(void) {
	printf("--------------------------------------------------------------------------fd_monitor_on_unload\n");
	on_unload();
}

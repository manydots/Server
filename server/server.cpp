// server.cpp: 定义应用程序的入口点。
//
#include <iostream>
#include <vector>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include "frida-gum.h"
#include "unistd.h"
#include <fcntl.h>
#include "common.h"
#include <unordered_map>

using namespace std;


struct UserExData
{
	BYTE isExpandMode;
};

static cdeclCall G_CEnvironment = (cdeclCall)0x080CC181;

static unordered_map<int, UserExData*> GameMap;

static cdeclCall10P WongWork_CMailBoxHelper_ReqDBSendNewSystemMultiMail = (cdeclCall10P)0x8556b68;
static cdeclCall3P WongWork_CMailBoxHelper_MakeSystemMultiMailPostal = (cdeclCall3P)0x8556a14;

//已打开的数据库句柄
static int* mysql_fd = NULL;
static int* mysql_taiwan_cain = NULL;
static int* mysql_d_guild = NULL;
static int* mysql_taiwan_cain_2nd = NULL;
static int* mysql_taiwan_billing = NULL;
static int* mysql_frida = NULL;


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
	for (DWORD i = 0; i < len; ++i)
		*((BYTE*)p + i) = code[i];
}

static inline void  __inline writeNopCode(DWORD p, DWORD len) {
	gum_mprotect((gpointer)p, len, GUM_PAGE_RWX);
	for (DWORD i = 0; i < len; ++i)
		*((BYTE*)p + i) = 0x90;
}

static cdeclCall2P SkillSlot__get_skillslot_buf = (cdeclCall2P)0x086067DE;
static cdeclCall2P SkillSlot__gcheckComboSkillInsertQuickSlot = (cdeclCall2P)0x08608D58;

int __hide __cdecl hookCSkill__reform_ui_group_no(int* thisP, int* skillClassP, bool isTpSkill, int a4) {
	int p = (int)skillClassP;
	if (RDWORD(skillClassP, -0x60) == 197) {
		//cout << " fix skill197 class to 4!" << endl;
		*skillClassP = 4;
	}
	return 0;
}

int __hide __cdecl hookSkillSlot__get_skillslot_group(int* thisP, int slot) {
	//DWORD retAddr = (DWORD)gum_invocation_context_get_return_address(NULL);
	//cout<< retAddr << " get_skillslot_group:" << slot << endl;
	if (slot < 8 || slot >= 198) return -1;
	else if (slot >= 160) return 4;
	else if (slot >= 122) return 3;
	else if (slot >= 84) return 2;
	else if (slot >= 46) return 1;
	else return 0;
}

int __hide __cdecl hookSkillSlot__get_skillslot_no(int* thisP, int skillId, int group, int slot, char is_active_skill) {
	if (!thisP || !*thisP) return -1;
	BYTE* buf = (BYTE*)SkillSlot__get_skillslot_buf(thisP, slot);
	if (!buf) return -1;
	if (is_active_skill && SkillSlot__gcheckComboSkillInsertQuickSlot(thisP, skillId)) {
		int end = skillId ? 7 : 5;
		for (int i = 0; i <= end; ++i) {
			if (buf[2 * i] == skillId) {
				return i;
			}
		}
		for (int i = 198; i <= 203; ++i) {
			if (buf[2 * i] == skillId) {
				return i;
			}
		}
	}
	int pos = 8 + group * 38;
	for (int i = 0; i < 38; ++i) {
		if (buf[2 * (i + pos)] == skillId) {
			return i + pos;
		}
	}
	return -1;
}

int __hide __cdecl hookSkillSlot__get_skillslot_no2(int* thisP, BYTE* buf, int skillId, int group, char is_active_skill) {
	if (!thisP || !*thisP || !buf) return -1;
	if (is_active_skill) {
		int end = skillId ? 7 : 5;
		for (int i = 0; i <= end; ++i) {
			if (buf[2 * i] == skillId) {
				return i;
			}
		}
		for (int i = 198; i <= 203; ++i) {
			if (buf[2 * i] == skillId) {
				return i;
			}
		}
	}
	int pos = 8 + group * 38;
	for (int i = 0; i < 38; ++i) {
		if (buf[2 * (i + pos)] == skillId) {
			return i + pos;
		}
	}
	return -1;
}

static cdeclCall5P SkillSlot__growtype_skill;
int __cdecl hookSkillSlot__growtype_skill(int* thisP, int job, int skillId, int expertJobLevel, int slot) {
	return SkillSlot__growtype_skill(thisP, job, skillId, expertJobLevel, slot);
}

static inline void  __inline FixOldSkillSlotInfo() {
	//printf("--------------------------------------------------------------------------FixOldSkillSlotInfo begin\n");
	//GumAddress addr = gum_module_find_base_address("df_game_r");
	GumInterceptor* v = gum_interceptor_obtain();
	gum_interceptor_begin_transaction(v);
	gum_interceptor_replace_fast(v, (gpointer)0x083507E8, (gpointer)hookCSkill__reform_ui_group_no, NULL);
	gum_interceptor_replace_fast(v, (gpointer)0x086049FC, (gpointer)hookSkillSlot__get_skillslot_group, NULL);
	gum_interceptor_replace_fast(v, (gpointer)0x08604A86, (gpointer)hookSkillSlot__get_skillslot_no, NULL);
	gum_interceptor_replace_fast(v, (gpointer)0x08607DBA, (gpointer)hookSkillSlot__get_skillslot_no2, NULL);
	//gum_interceptor_replace_fast(v, (gpointer)0x086040BC, (gpointer)hookSkillSlot__growtype_skill, (gpointer*)&SkillSlot__growtype_skill);
	gum_interceptor_end_transaction(v);

	writeByteCode(0x08604596, 5);
	writeByteCode(0x0860459C, 5);
	writeByteCode(0x08606A27, 5);
	writeByteCode(0x0860795E, 5);
	writeDWordCode(0x08605090, 8 + 38 * 4);
	writeDWordCode(0x08605097, 8 + 38 * 5);
	writeDWordCode(0x0860514F, 8 + 38 * 3);
	writeDWordCode(0x08605156, 8 + 38 * 4);
	writeDWordCode(0x0860513F, 8 + 38 * 2);
	writeDWordCode(0x08605146, 8 + 38 * 3);
	writeDWordCode(0x0860512F, 8 + 38 * 1);
	writeDWordCode(0x08605136, 8 + 38 * 2);
	writeDWordCode(0x0860511F, 8 + 38 * 0);
	writeDWordCode(0x08605126, 8 + 38 * 1);
	writeByteCode(0x08608CD5, 8);
	writeByteCode(0x08608CCF, 8);
	writeByteCode(0x08608969, 8);
	writeByteCode(0x08609001, 8);

	writeDWordCode(0x0866C6C4, 196);//工会技能
	writeDWordCode(0x0866C96E, 196);//工会技能
}



static cdeclCall1P PacketGuard_PacketGuard = (cdeclCall1P)0x858DD4C;
static cdeclCall3P InterfacePacketBuf_put_header = (cdeclCall3P)0x80CB8FC;
static cdeclCall2P InterfacePacketBuf_put_byte = (cdeclCall2P)0x80CB920;
static cdeclCall2P InterfacePacketBuf_put_short = (cdeclCall2P)0x80D9EA4;
static cdeclCall2P InterfacePacketBuf_put_int = (cdeclCall2P)0x80CB93C;
static cdeclCall3P InterfacePacketBuf_put_binary = (cdeclCall3P)0x811DF08;
static cdeclCall3P InterfacePacketBuf_put_str = (cdeclCall3P)0x81B73E4;
static cdeclCall2P InterfacePacketBuf_finalize = (cdeclCall2P)0x80CB958;
static cdeclCall1P Destroy_PacketGuard_PacketGuard = (cdeclCall1P)0x858DE80;

static cdeclBCall1P Inven_Item_isEmpty = (cdeclBCall1P)0x811ED66;
static cdeclCall1P Inven_Item_getKey = (cdeclCall1P)0x850D14E;

static cdeclCall1P CItem__IsImpossibleGift = (cdeclCall1P)0x085143AE;

static cdeclCall3P AddSocketToAvatar = (cdeclCall3P)0x0821a412;
static cdeclCall1P PacketBuf_get_index = (cdeclCall1P)0x8110b1c;
static cdeclCall2P PacketBuf_set_index = (cdeclCall2P)0x81252ba;
static cdeclCall1P PacketBuf_get_len = (cdeclCall1P)0x858da52;
static cdeclCall2P PacketBuf_get_byte = (cdeclCall2P)0x858CF22;
static cdeclCall2P PacketBuf_get_short = (cdeclCall2P)0x858CFC0;
static cdeclCall2P PacketBuf_get_int = (cdeclCall2P)0x858D27E;
static cdeclCall3P PacketBuf_get_binary = (cdeclCall3P)0x858D3B2;
static cdeclCall4P PacketBuf_get_str = (cdeclCall4P)0x0858D2BC;
static cdeclCall2P PacketBuf_get_packet = (cdeclCall2P)0x0822B702;
// const CEquipItem_GetItemType = ()0x08514d26;
static cdeclCall3P CEquipItem_getAvatarSocket = (cdeclCall3P)0x08150f36;
static cdeclCall1P InterfacePacketBuf_get_index = (cdeclCall1P)0x08110b4c;
static cdeclCall2P InterfacePacketBuf_set_index = (cdeclCall2P)0x0822b7b0;
static cdeclCall2P InterfacePacketBuf_get_short = (cdeclCall2P)0x0848f3c0;
static cdeclCall2P InterfacePacketBuf_get_int = (cdeclCall2P)0x0848f3dc;
static cdeclCall3P CUser_SendCmdErrorPacket = (cdeclCall3P)0x0867bf42;
static cdeclCall2P CInventory_get_inven_slot_no = (cdeclCall2P)0x0850cd62;
static cdeclBCall1P isEquipableItemType = (cdeclBCall1P)0x0850D17C;

static cdeclCall2P PacketBuf__put_packet;
int __hide procPutPacket(int* packet_guard, int slotObj, int retAddr) {
	DWORD dataP = *(DWORD*)(*packet_guard + 20);
	WORD cmdType = *(WORD*)(dataP + 1);

	//289回购装备不处理，如果回购以镶嵌的装备，需要重新选角色才可以刷新镶嵌信息
	//!Inven_Item_isEquipableItemType(slotObj) || Inven_Item_isAvatarItemType(slotObj)
	if (Inven_Item_isEmpty((int*)slotObj) || *(BYTE*)(slotObj + 1) != 0x01) {
		//printf("putPacket:0x%08x %d\n", retAddr, cmdType);
		return PacketBuf__put_packet(packet_guard, slotObj);
	}

	//DPRINTF("1 cmdType:%d\n", cmdType);
	BYTE slotType = RBYTE(slotObj, 21);
	if (!slotType) {
		//printf("no slot data putPacket:0x%08x %d\n", retAddr, cmdType);
		return PacketBuf__put_packet(packet_guard, slotObj);
	}
	//printf("putPacket slot:0x%08x %d %d\n", retAddr, slotType, cmdType);
	//DPRINTF("2 slot:[%d, %d, %d]\n", slot1, slot2, slot3);
	DWORD oIdx = InterfacePacketBuf_get_index(packet_guard);

	char myData[25];

	WORD slot1 = 0, slot2 = 0, slot3 = 0;
	switch (slotType) {
	case 1:slot1 = 0x01; slot2 = 0x01; slot3 = 0x00; break;
	case 2:slot1 = 0x02; slot2 = 0x02; slot3 = 0x00; break;
	case 3:slot1 = 0x04; slot2 = 0x04; slot3 = 0x00; break;
	case 4:slot1 = 0x08; slot2 = 0x08; slot3 = 0x00; break;
	case 5:slot1 = 0x10; slot2 = 0x00; slot3 = 0x00; break;
	}

	int p = (int)myData;
	*(WORD*)p = slot1;
	*(DWORD*)(p + 2) = *(DWORD*)(slotObj + 22);
	*(BYTE*)(p + 5) = 0;
	*(WORD*)(p + 6) = slot2;
	*(DWORD*)(p + 8) = *(DWORD*)(slotObj + 25);
	*(BYTE*)(p + 11) = 0;
	*(WORD*)(p + 12) = slot3;
	*(DWORD*)(p + 14) = *(DWORD*)(slotObj + 28);
	*(BYTE*)(p + 17) = 0;
	//cout << "data:" << cmdType << "," << hex << bannerType << "," << id << ", " << slot << "," << type << endl;
	InterfacePacketBuf_put_byte(packet_guard, 0xef);
	InterfacePacketBuf_put_int(packet_guard, sizeof(myData));
	InterfacePacketBuf_put_binary(packet_guard, (int)&myData, sizeof(myData));
	return PacketBuf__put_packet(packet_guard, slotObj);
}

int __hide __cdecl hookPacketBuf__put_packet(int* packet_guard, int  slotObj) {
	//DWORD retAddr = (DWORD)gum_invocation_context_get_return_address(NULL);
	DWORD retAddr = (DWORD)__builtin_return_address(0);
	return procPutPacket(packet_guard, slotObj, retAddr);
}

#define INVENTORY_TYPE_BODY		0            //身上穿的装备
#define INVENTORY_TYPE_ITEM		1            //物品栏
#define INVENTORY_TYPE_AVARTAR	2         //时装栏

static cdeclCall2P CDataManager_find_item = (cdeclCall2P)0x835FA32;
static cdeclCall1P CEquipItem_GetItemType = (cdeclCall1P)0x08514D26;
static cdeclCall G_CDataManager = (cdeclCall)0x80CC19B;
static cdeclCall1P CUserCharacInfo_getCurCharacInvenW = (cdeclCall1P)0x80DA28E;
static cdeclCall1P CUserCharacInfo_getCurCharacInvenR = (cdeclCall1P)0x80DA27E;
static cdeclCall3P CInventory_GetInvenRef = (cdeclCall3P)0x84FC1DE;
static cdeclCall2P CUser_Send = (cdeclCall2P)0x86485BA;
static cdeclBCall1P Inven_Item_isEquipableItemType = (cdeclBCall1P)0x08150812;
//通知客户端道具更新(客户端指针, 通知方式[仅客户端=1, 世界广播=0, 小队=2, war room=3], itemSpace[装备=0, 时装=1], 道具所在的背包槽)
static cdeclCall4P CUser_SendUpdateItemList = (cdeclCall4P)0x867C65A;
//背包中删除道具(背包指针, 背包类型, 槽, 数量, 删除原因, 记录删除日志)
static cdeclCall6P CInventory_delete_item = (cdeclCall6P)0x850400C;

void __hide SendMyErrorPacket(int* user, int cmdType, BYTE errorNo) {
	BYTE packet_guard[64];
	int* p = (int*)packet_guard;
	PacketGuard_PacketGuard(p);
	InterfacePacketBuf_put_header(p, 1, cmdType);
	InterfacePacketBuf_put_byte(p, 0);
	InterfacePacketBuf_put_byte(p, errorNo);
	InterfacePacketBuf_put_byte(p, 0xff);
	InterfacePacketBuf_finalize(p, 1);
	CUser_Send(user, (int)p);
	Destroy_PacketGuard_PacketGuard(p);
}



static cdeclCall3P Dispatcher_AddSocketToAvatar__dispatch_sig = (cdeclCall3P)0x0821a412;
int __hide __cdecl hookDispatcher_AddSocketToAvatar__dispatch_sig(int* thisP, int* user, int* dataBuff) {
	BYTE m[64] = { 0 };
	DWORD oIdx = PacketBuf_get_index(dataBuff);
	WORD eSlot = -1;
	DWORD eId = -1;
	PacketBuf_get_short(dataBuff, (int)&eSlot);
	PacketBuf_get_int(dataBuff, (int)&eId);
	if (eId == -1)
		return Dispatcher_AddSocketToAvatar__dispatch_sig(thisP, (int)user, PacketBuf_set_index(dataBuff, oIdx));
	int eItem = CDataManager_find_item((int*)G_CDataManager(), eId);
	int itemType = CEquipItem_GetItemType((int*)eItem);
	int invenType = itemType < 0x0a ? INVENTORY_TYPE_AVARTAR : INVENTORY_TYPE_ITEM;
	if (invenType == INVENTORY_TYPE_AVARTAR)
		return Dispatcher_AddSocketToAvatar__dispatch_sig(thisP, (int)user, PacketBuf_set_index(dataBuff, oIdx));
	int* inven = (int*)CUserCharacInfo_getCurCharacInvenW(user);
	int eSlotObj = CInventory_GetInvenRef(inven, INVENTORY_TYPE_ITEM, eSlot);
	if (Inven_Item_isEmpty((int*)eSlotObj) || Inven_Item_getKey((int*)eSlotObj) != eId) {
		SendMyErrorPacket(user, 209, 0x04);
		return 0;
	}
	BYTE slotType = RBYTE(eSlotObj, 21);
	if (slotType) {
		CUser_SendUpdateItemList(user, 1, 0, eSlot);
		SendMyErrorPacket(user, 209, 19);
		return 0;
	}
	slotType = 0;
	WORD sSlot = -1;
	PacketBuf_get_short(dataBuff, (int)&sSlot);
	//int ret = CEquipItem_getAvatarSocket((int *)eItem, -1, (int)m);
	//if (ret != 0) {
		//S 铂金槽 0x10
		//A 红色槽 0x01
		//B 黄色槽 0x02
		//C 绿色槽 0x04
		//D 蓝色槽 0x08
		//M 彩色槽 0xef
	if (itemType == 10 || itemType == 11) {
		SendMyErrorPacket(user, 209, 19);
		return 0;//武器，称号默认不支持镶嵌
	}
	else if (itemType == 19 || itemType == 16) {
		WWORD(m, 0, 0x01); WWORD(m, 6, 0x01);//腰带 //戒指
		slotType = 1;
	}
	else if (itemType == 17 || itemType == 13) {
		WWORD(m, 0, 0x02); WWORD(m, 6, 0x02);//项链 头肩
		slotType = 2;
	}
	else if (itemType == 14 || itemType == 12) {
		WWORD(m, 0, 0x04); WWORD(m, 6, 0x04);//上衣 下衣
		slotType = 3;
	}
	else if (itemType == 18 || itemType == 15) {
		WWORD(m, 0, 0x08); WWORD(m, 6, 0x08);//手镯 鞋子
		slotType = 4;
	}
	else {
		WWORD(m, 0, 0x10); // 辅助装备，魔法石默认开白金孔
		slotType = 5;
	}
	//switch (itemType) {
	//case 10:
	//case 11: SendMyErrorPacket(user, 209, 19); return 0;//武器，称号默认不支持镶嵌
	//case 19: //WWORD(m, 0, 0x01); WWORD(m, 6, 0x01); break;//戒指
	//case 16: WWORD(m, 0, 0x01); WWORD(m, 6, 0x01); break;//腰带
	//case 13: //WWORD(m, 0, 0x02); WWORD(m, 6, 0x02); break;//头肩
	//case 17: WWORD(m, 0, 0x02); WWORD(m, 6, 0x02); break;//项链
	//case 14: //WWORD(m, 0, 0x04); WWORD(m, 6, 0x04); break;//下衣
	//case 12: WWORD(m, 0, 0x04); WWORD(m, 6, 0x04); break;//上衣
	//case 15:// WWORD(m, 0, 0x08); WWORD(m, 6, 0x08); break;//鞋子
	//case 18: WWORD(m, 0, 0x08); WWORD(m, 6, 0x08); break;//手镯
	//case 20:
	//case 21:WWORD(m, 0, 0x10); break;//辅助装备，魔法石默认开白金孔
	//}
//}
	WDWORD(m, 2, 0);
	WDWORD(m, 8, 0);
	WDWORD(m, 14, 0);
	if (CInventory_delete_item(inven, 1, sSlot, 1, 27, 1) != 1)
		return 17;
	WBYTE(eSlotObj, 21, slotType);
	CUser_SendUpdateItemList(user, 1, 0, eSlot);
	int* packet_guard = (int*)m;
	PacketGuard_PacketGuard(packet_guard);
	InterfacePacketBuf_put_header(packet_guard, 1, 209);
	InterfacePacketBuf_put_byte(packet_guard, 1);
	InterfacePacketBuf_put_byte(packet_guard, 0xff);
	InterfacePacketBuf_put_short(packet_guard, eSlot);
	InterfacePacketBuf_put_short(packet_guard, sSlot);
	InterfacePacketBuf_finalize(packet_guard, 1);
	CUser_Send(user, (int)packet_guard);
	return Destroy_PacketGuard_PacketGuard(packet_guard);
}

static cdeclCall3P CUser_CheckItemLock = (cdeclCall3P)0x8646942;
static cdeclCall1P CUser_get_state = (cdeclCall1P)0x80DA38C;
//获取时装插槽数据
static cdeclCall2P WongWork_CAvatarItemMgr_getJewelSocketData = (cdeclCall2P)0x82F98F8;
//获取时装管理器
static cdeclCall1P CInventory_GetAvatarItemMgrR = (cdeclCall1P)0x80DD576;
//获取道具附加信息
static cdeclCall1P Inven_Item_get_add_info = (cdeclCall1P)0x80F783A;
//道具是否为消耗品
static cdeclCall1P CItem_is_stackable = (cdeclCall1P)0x80F12FA;
//获取消耗品类型
static cdeclCall1P CStackableItem_GetItemType = (cdeclCall1P)0x8514A84;
//获取徽章支持的镶嵌槽类型
static cdeclCall1P CStackableItem_getJewelTargetSocket = (cdeclCall1P)0x0822CA28;
//时装镶嵌数据存盘
static cdeclCall3P DB_UpdateAvatarJewelSlot_makeRequest = (cdeclCall3P)0x843081C;
//获取当前角色id
static cdeclCall1P CUserCharacInfo_getCurCharacNo = (cdeclCall1P)0x80CBC4E;

static cdeclCall2P Dispatcher_UseJewel__dispatch_sig;
int __cdecl hookDispatcher_UseJewel__dispatch_sig(int* thisP, int* user, int* dataBuff) {
	//校验角色状态是否允许镶嵌
	if (CUser_get_state(user) != 3)
		return 0;

	WORD eSlot = -1;
	PacketBuf_get_short(dataBuff, (int)&eSlot);
	DWORD eId = -1;
	PacketBuf_get_int(dataBuff, (int)&eId);
	BYTE badgeCount = -1;
	PacketBuf_get_byte(dataBuff, (int)&badgeCount);

	DPRINTF("cmd:%d,%d\n", eSlot, eId);
	//cout << "cmd:"<< eSlot << ", " << eId << "," << badgeCount << endl;
	if (eId == -1 || eSlot == -1) {
		CUser_SendCmdErrorPacket(user, 204, 0x04);
		return 0;
	}

	int* inven = (int*)CUserCharacInfo_getCurCharacInvenW(user);
	int* eItem = (int*)CDataManager_find_item((int*)G_CDataManager(), eId);
	int invenType = CEquipItem_GetItemType(eItem) < 0x0a ? INVENTORY_TYPE_AVARTAR : INVENTORY_TYPE_ITEM;
	//获取格子上的物品对象
	int eSlotObj = CInventory_GetInvenRef(inven, invenType, eSlot);
	if (Inven_Item_isEmpty((int*)eSlotObj) || Inven_Item_getKey((int*)eSlotObj) != eId) {
		CUser_SendCmdErrorPacket(user, 204, 0x04);
		return 0;
	}
	if (CUser_CheckItemLock(user, invenType, eSlot)) {
		CUser_SendCmdErrorPacket(user, 204, 0xd5);
		return 0;
	}

	BYTE jewelData[30];
	int dP = (int)jewelData;
	if (invenType == INVENTORY_TYPE_AVARTAR) {
		//获取时装插槽数据
		dP = WongWork_CAvatarItemMgr_getJewelSocketData((int*)CInventory_GetAvatarItemMgrR(inven), Inven_Item_get_add_info((int*)eSlotObj));
	}
	else {
		BYTE slotType = RBYTE(eSlotObj, 21);
		WORD slot1 = 0, slot2 = 0, slot3 = 0;

		switch (slotType) {
		case 1:slot1 = 0x01; slot2 = 0x01; slot3 = 0x00; break;
		case 2:slot1 = 0x02; slot2 = 0x02; slot3 = 0x00; break;
		case 3:slot1 = 0x04; slot2 = 0x04; slot3 = 0x00; break;
		case 4:slot1 = 0x08; slot2 = 0x08; slot3 = 0x00; break;
		case 5:slot1 = 0x10; slot2 = 0x00; slot3 = 0x00; break;
		}
		*(WORD*)dP = slot1;
		*(DWORD*)(dP + 2) = *(DWORD*)(eSlotObj + 22);
		*(BYTE*)(dP + 5) = 0;
		*(WORD*)(dP + 6) = slot2;
		*(DWORD*)(dP + 8) = *(DWORD*)(eSlotObj + 25);
		*(BYTE*)(dP + 11) = 0;
		*(WORD*)(dP + 12) = slot3;
		*(DWORD*)(dP + 14) = *(DWORD*)(eSlotObj + 28);
		*(BYTE*)(dP + 17) = 0;
	}
	if (!dP || badgeCount > 3) {
		CUser_SendCmdErrorPacket(user, 204, 0x04);
		return 0;
	}

	int badges[3][2] = { -1 }, badge, bItem;
	WORD bSlot;
	DWORD bId;
	BYTE slotId;
	for (int i = 0; i < badgeCount; ++i) {
		PacketBuf_get_short(dataBuff, (int)&bSlot);
		PacketBuf_get_int(dataBuff, (int)&bId);
		PacketBuf_get_byte(dataBuff, (int)&slotId);
		badge = CInventory_GetInvenRef(inven, INVENTORY_TYPE_ITEM, bSlot);
		//校验徽章及插槽数据是否合法
		if (Inven_Item_isEmpty((int*)badge) || Inven_Item_getKey((int*)badge) != bId || slotId >= 3) {
			CUser_SendCmdErrorPacket(user, 204, 0x11);
			return 0;
		}

		//校验徽章是否满足时装插槽颜色要求
		//获取徽章pvf数据
		bItem = CDataManager_find_item((int*)G_CDataManager(), bId);
		if (!bItem) return 0;
		//校验徽章类型
		if (!CItem_is_stackable((int*)bItem) || CStackableItem_GetItemType((int*)bItem) != 20) {
			CUser_SendCmdErrorPacket(user, 204, 0x11);
			return 0;
		}
		//检测徽章与插槽类型是否一致
		if (!(CStackableItem_getJewelTargetSocket((int*)bItem) & *(WORD*)(dP + slotId * 6))) {
			CUser_SendCmdErrorPacket(user, 204, 0x04);
			return 0;
		}
		//开始镶嵌
		CInventory_delete_item(inven, 1, bSlot, 1, 8, 1);//删除徽章
		*(DWORD*)(dP + slotId * 6 + 2) = bId;//设置时装插槽数据
	}

	if (invenType == INVENTORY_TYPE_AVARTAR) {
		//时装插槽数据存档
		DWORD avartar_ui_id = *(DWORD*)(eSlotObj + 7);
		DB_UpdateAvatarJewelSlot_makeRequest((int*)CUserCharacInfo_getCurCharacNo(user), avartar_ui_id, dP);
		//通知客户端时装数据已更新
		CUser_SendUpdateItemList(user, 1, 1, eSlot);
	}
	else {
		BYTE tyep = *(BYTE*)(eSlotObj + 22 + 3);
		*(DWORD*)(eSlotObj + 22) = *(DWORD*)(dP + 2);
		*(BYTE*)(eSlotObj + 22 + 3) = tyep;

		tyep = *(BYTE*)(eSlotObj + 25 + 3);
		*(DWORD*)(eSlotObj + 25) = *(DWORD*)(dP + 8);
		*(BYTE*)(eSlotObj + 25 + 3) = tyep;

		tyep = *(BYTE*)(eSlotObj + 28 + 3);
		*(DWORD*)(eSlotObj + 28) = *(DWORD*)(dP + 14);
		*(BYTE*)(eSlotObj + 28 + 3) = tyep;
		CUser_SendUpdateItemList(user, 1, 0, eSlot);
	}
	//回包给客户端
	int packet_guard[64];
	PacketGuard_PacketGuard(packet_guard);
	InterfacePacketBuf_put_header(packet_guard, 1, 204);
	InterfacePacketBuf_put_int(packet_guard, 1);
	InterfacePacketBuf_finalize(packet_guard, 1);
	CUser_Send(user, (int)packet_guard);
	Destroy_PacketGuard_PacketGuard(packet_guard);
	return 0;
}

static BYTE EmptySlot[30];
static cdeclCall2P WongWork__CAvatarItemMgr__getJewelSocketData = (cdeclCall2P)0x082F98F8;
int __cdecl hookWongWork__CAvatarItemMgr__getJewelSocketData(int* thisP, int* user) {
	return (int)EmptySlot;
}


int __cdecl fixDropItmeSlot(BYTE* mapItem, BYTE* slotItem) {
	mapItem[0x10] = slotItem[0];
	*(WORD*)&mapItem[0x1B] = *(WORD*)&slotItem[0xB];
	*(DWORD*)&mapItem[0x1D] = *(DWORD*)&slotItem[0xD];
	*(DWORD*)&mapItem[0x25] = *(DWORD*)&slotItem[0x15];
	*(DWORD*)&mapItem[0x29] = *(DWORD*)&slotItem[0x19];
	*(WORD*)&mapItem[0x2D] = *(WORD*)&slotItem[0x1D];
	*(DWORD*)&mapItem[0x21] = *(DWORD*)&slotItem[0x11];
	*(DWORD*)&mapItem[0x35] = *(DWORD*)&slotItem[0x25];
	*(DWORD*)&mapItem[0x39] = *(DWORD*)&slotItem[0x29];
	*(DWORD*)&mapItem[0x3D] = *(DWORD*)&slotItem[0x2D];
	*(WORD*)&mapItem[0x41] = *(WORD*)&slotItem[0x31];
	memcpy(mapItem + 21, slotItem + 21, 10);
	//*(BYTE*)(mapItem + 36) = *(BYTE*)(slotItem + 36);
	//*(BYTE*)(mapItem + 56) = *(BYTE*)(slotItem + 56);
	//*(BYTE*)(mapItem + 60) = *(BYTE*)(slotItem + 60);
	//*(DWORD*)(mapItem + 33) = *(DWORD*)(slotItem + 33);
	//*(DWORD*)(mapItem + 53) = *(DWORD*)(slotItem + 53);
	//*(DWORD*)(mapItem + 57) = *(DWORD*)(slotItem + 57);
}

cdeclCall1P Inven_Item__reset = (cdeclCall1P)0x080CB7D8;
int __cdecl hookInven_Item__reset(int* thisP) {
	if (((cdeclBCall1P)0x08150812)(thisP))
		memset((char*)thisP + 21, 0, 10);
	return Inven_Item__reset(thisP);
}

cdeclCall2P Inven_Item__setCopy = (cdeclCall2P)0x0814A62E;
int __cdecl hookInven_Item__setCopy(int* thisP, int slot) {
	int ret = Inven_Item__setCopy(thisP, slot);
	if (((cdeclBCall1P)0x08150812)(thisP))
		memcpy((char*)thisP + 21, (char*)slot + 21, 10);
	return ret;
}


cdeclCall3P CItemGloballyUniqueIdentifierGenerator__generate = (cdeclCall3P)0x0889246C;
int hookCItemGloballyUniqueIdentifierGenerator__generate(int* thisP, int a2, int) {
	return a2;
}

int __cdecl CUser__Add_RedeemInfo(int* user, int* item, int a3, bool a4) {
	return 0;
}

int __cdecl CUser__reqSendMailCertify(int* user) {
	return 0;
}


int __cdecl fixWongWork__CMailBoxHelper__ReqDBSendNewSystemMultiMail(
	int* title,
	int a2,
	int a3,
	int a4,
	int a5,
	int text,
	int textLen,
	int a8,
	int a9,
	int a10) {
	if (a3 > 10) {
		WongWork_CMailBoxHelper_ReqDBSendNewSystemMultiMail(title, a2 + 61 * 10, a3 - 10, a4, a5, text, textLen, a8, a9, a10);
		a3 = 10;
	}
	return WongWork_CMailBoxHelper_ReqDBSendNewSystemMultiMail(title, a2, a3, a4, a5, text, textLen, a8, a9, a10);
}

cdeclCall10P CInventory__AddAvatarItem = (cdeclCall10P)0x08509B9E;
int __cdecl hookCInventory__AddAvatarItem(
	int* a1,
	int a2,
	int a3,
	int a4,
	int a5,
	int a6,
	int src,
	int a8,
	int a9,
	int a10) {
	return CInventory__AddAvatarItem(a1, a2, a3, a4, a5, 3, src, a8, a9, a10);
}

static inline void  __inline EquipmentEmblembSuppust() {
	//DPRINTF("--------------------------------------------------------------------------EquipmentEmblembSuppust begin\n");
	GumInterceptor* v = gum_interceptor_obtain();
	gum_interceptor_begin_transaction(v);
	gum_interceptor_replace_fast(v, (gpointer)0x0815098e, (gpointer)hookPacketBuf__put_packet, (gpointer*)&PacketBuf__put_packet);
	gum_interceptor_replace_fast(v, (gpointer)0x0821a412, (gpointer)hookDispatcher_AddSocketToAvatar__dispatch_sig, (gpointer*)&Dispatcher_AddSocketToAvatar__dispatch_sig);
	gum_interceptor_replace_fast(v, (gpointer)0x08217bd6, (gpointer)hookDispatcher_UseJewel__dispatch_sig, (gpointer*)&Dispatcher_UseJewel__dispatch_sig);
	////屏蔽时装镶嵌信息
	//gum_interceptor_replace_fast(v, (gpointer)0x082F98F8, (gpointer)hookWongWork__CAvatarItemMgr__getJewelSocketData, (gpointer*)&WongWork__CAvatarItemMgr__getJewelSocketData);
	writeByteCode(0x0821a8e3, 1);
	writeByteCode(0x0821a863, 0xEB);
	writeByteCode(0x0821a798, 0xEB);
	writeByteCode(0x0821a821, 0xEB);
	writeWordCode(0x0821a54d, 0x57eb);
	//writeDWordCode(0x083337D8, 3);
	//writeCallCode(0x08326EC8, (void*)hookCInventory__AddAvatarItem);

	gum_interceptor_replace_fast(v, (gpointer)0x080CB7D8, (gpointer)hookInven_Item__reset, (gpointer*)&Inven_Item__reset);
	gum_interceptor_replace_fast(v, (gpointer)0x0814A62E, (gpointer)hookInven_Item__setCopy, (gpointer*)&Inven_Item__setCopy);
	gum_interceptor_replace_fast(v, (gpointer)0x0889246C, (gpointer)hookCItemGloballyUniqueIdentifierGenerator__generate, (gpointer*)&CItemGloballyUniqueIdentifierGenerator__generate);//屏蔽id 系统无用，替换成镶嵌

	gum_interceptor_replace_fast(v, (gpointer)0x086472C0, (gpointer)CUser__Add_RedeemInfo, NULL);//屏蔽装备回购
	gum_interceptor_replace_fast(v, (gpointer)0x0868A51A, (gpointer)CUser__reqSendMailCertify, NULL);//修复卡邮件bug
	gum_interceptor_end_transaction(v);

	writeCallCode(0x085C5F3B, (void*)fixWongWork__CMailBoxHelper__ReqDBSendNewSystemMultiMail);

	//BYTE hookCode[] = { 0x8D, 0x85, 0xD4, 0xFE, 0xFF, 0xFF, 0x83, 0xC0, 0x10, 0x89, 0x04, 0x24, 0x8D, 0x85, 0x2B, 0xFF, 0xFF, 0xFF, 0x89, 0x44, 0x24, 0x04, 0xE8, 0xFB, 0x9C, 0xBA, 0xFF, 0xEB, 0x67 };
	//writeArrayCode(0x085A6AD2, hookCode, sizeof(hookCode));
	//writeCallCode(0x085A6AE8, (void*)fixDropItmeSlot);

	//最大摆摊格子数量
	writeByteCode(0x085C6EBB, 13);

	//时装摆摊
	writeByteCode(0x085C70F1, 0xEB);

	//时装分解
	writeByteCode(0x08217F97, 0xEB);

	////单邮件
	//BYTE hookCode2[] = { 0x8B, 0x85, 0x14, 0xFF, 0xFF, 0xFF, 0x89, 0x44, 0x24, 0x20, 0x8B, 0x85, 0x18, 0xFF, 0xFF, 0xFF, 0x89, 0x44, 0x24, 0x1C, 0x8B, 0x85, 0x1C, 0xFF, 0xFF, 0xFF, 0x89, 0x44, 0x24, 0x18, 0x8B, 0x85, 0x20, 0xFF, 0xFF, 0xFF, 0x89, 0x44, 0x24, 0x14, 0x8B, 0x45, 0xD8, 0x89, 0x44, 0x24, 0x10, 0x8D, 0x85, 0x55, 0xFF, 0xFF, 0xFF, 0x89, 0x44, 0x24, 0x0C, 0x8B, 0x45, 0xE4, 0x89, 0x44, 0x24, 0x08, 0x8B, 0x45, 0x0C, 0x89, 0x44, 0x24, 0x04, 0x8B, 0x45, 0x08, 0x89, 0x04, 0x24, 0xE8, 0x8F, 0x4B, 0xFD, 0xFF, 0xE9, 0x06, 0x01, 0x00, 0x00 };
	//writeArrayCode(0x0841F5DF, hookCode2, sizeof(hookCode2));
	//writeCallCode(0x0841F62C, (void*)fixSendMailItmeSlot);
	//
	////多邮件
	//writeCallCode(0x08443594, (void*)DB_MailBox_Req_System_Multi_Mail___InsertPostal);
	//
	////获取邮件
	//writeDWordCode(0x0841DF92, (int)&MailSlotQuery);
	//BYTE hookCode3[] = { 0x8D, 0x95, 0xFE, 0xFE, 0xFF, 0xFF, 0x89, 0x54, 0x24, 0x08, 0x89, 0x44, 0x24, 0x04, 0x8B, 0x55, 0xC8, 0x89, 0x14, 0x24, 0xE8, 0xF9, 0x37, 0xD2, 0xFF, 0xEB, 0x0B };
	//writeArrayCode(0x0841E80A, hookCode3, sizeof(hookCode3));
	//writeCallCode(0x0841E81E, (void*)fixGetMailItmeSlot);

	//DPRINTF("--------------------------------------------------------------------------EquipmentEmblembSuppust end\n");
}


static cdeclCall2P CDataManager__find_dungeon = (cdeclCall2P)0x0835F9F8;
static cdeclCall3P CUser__AddDungeonClear = (cdeclCall3P)0x086780FA;
static cdeclBCall2P CParty__checkValidUser = (cdeclBCall2P)0x085B4D12;
static cdeclCall1P CUserCharacInfo__get_charac_level = (cdeclCall1P)0x080DA2B8;
static cdeclCall1P CBattle_Field__get_dungeon_diff = (cdeclCall1P)0x080F981C;
static cdeclCall1P CDungeon__get_min_level = (cdeclCall1P)0x0814559A;
char __hide __cdecl hookCParty__DungeonPermission(int* thisP, int score) {
	//普通图 评分达到D以上 开除冒险 
	// 冒险评分B以上 开出勇士 
	// 勇士 评分S以上开出王者 
	// 王者 特殊任务开出英雄
	// 0x512c//sss
	// 0x512e//ss
	// 0x5130//s
	// 0x5132//a
	// 0x5134//b
	// 0x5136//c
	// 0x5138//d
	// 0x513a//f
	int buf[64];
	int dungeonId = *((DWORD*)thisP + 0x32E);
	int mgr = G_CDataManager();
	int v = CDataManager__find_dungeon((int*)mgr, dungeonId);
	if (!v) return 0;
	int miniLevel = CDungeon__get_min_level((int*)v);
	for (int i = 0; i <= 3; ++i) {
		if (CParty__checkValidUser(thisP, i) != 1) continue;
		int* user = (int*)*(thisP + 6 * i + 30);
		if (miniLevel > CUserCharacInfo__get_charac_level(user))continue;
		int isAdd = 0;
		WORD currentDiff = CBattle_Field__get_dungeon_diff((int*)((BYTE*)thisP + 0xB24));
		if (currentDiff == 0) {
			if (score >= RWORD(mgr, 0x513a))// 大于F评分
				isAdd = CUser__AddDungeonClear(user, dungeonId, 1);
		}
		else if (currentDiff == 1) {
			if (score >= RWORD(mgr, 0x5134))// 大于B评分
				isAdd = CUser__AddDungeonClear(user, dungeonId, 2);
		}
		else if (currentDiff == 2) {
			if (score >= RWORD(mgr, 0x5130))// 大于S评分
				isAdd = CUser__AddDungeonClear(user, dungeonId, 3);
		}
		//switch (currentDiff) {
		//case 0://普通级
		//	if (score >= *(WORD*)(mgr + 0x5138))// 大于d评分
		//		isAdd = CUser__AddDungeonClear(user, dungeonId, 1);
		//	break;
		//case 1://冒险级
		//	if (score >= *(WORD*)(mgr + 0x5134))// 大于B评分
		//		isAdd = CUser__AddDungeonClear(user, dungeonId, 2);
		//	break;
		//case 2://勇士级
		//	if (score >= *(WORD*)(mgr + 0x5130))// 大于B评分
		//		isAdd = CUser__AddDungeonClear(user, dungeonId, 3);
		//	break;
		////case 3://王者级
		//}
		if (isAdd) {
			PacketGuard_PacketGuard(buf);
			InterfacePacketBuf_put_header(buf, 0, 5);
			InterfacePacketBuf_put_short(buf, 1);
			InterfacePacketBuf_put_short(buf, dungeonId);
			InterfacePacketBuf_put_short(buf, currentDiff + 1);
			InterfacePacketBuf_finalize(buf, 1);
			CUser_Send(user, (int)buf);
			Destroy_PacketGuard_PacketGuard(buf);
		}
	}
	return 1;
}

static cdeclBCall2P  DB_CreateCharac___createCharacDungeon = (cdeclBCall2P)0x084010FC;
bool __hide __cdecl hookDB_CreateCharac___createCharacDungeon(int* thisP, unsigned int a2) {
	int ebp = *(int*)((int)&thisP - 8);
	int charac_no = *(DWORD*)(*(DWORD*)(ebp - 0x0c) + 0x5348);
	return DB_CreateCharac___createCharacDungeon(thisP, charac_no);
}

static BYTE selectCharacDungeon[48 + 1];
static BYTE insertCharacDungeon[64 + 1];
static BYTE selectDungeon[56 + 1];
static BYTE insertDungeon[64 + 1];
static BYTE updateDungeon[64 + 1];
static inline void  __inline OldderDungeonPermission() {
	GumInterceptor* v = gum_interceptor_obtain();
	gum_interceptor_replace_fast(v, (gpointer)0x085B12F8, (gpointer)hookCParty__DungeonPermission, NULL);

	writeDWordCode(0x0867079e, 4);//英雄级任务修复
	writeDWordCode(0x084841DA, 4);//英雄级任务修复
	writeByteCode(0x08484163, 0xEB);

	strcpy((char*)selectCharacDungeon, "seLect * from charac_dungeon where charac_no=%s");
	writeDWordCode(0x0840113D, (int)selectCharacDungeon);
	strcpy((char*)insertCharacDungeon, "inSert into charac_dungeon (charac_no,dungeon) values (%s,'')");
	writeDWordCode(0x0840119C, (int)insertCharacDungeon);

	int hookCharacNoAddr[] = { 0x08419DEA, 0x0841A14A, 0x0841A0D2 };
	int hookQueryAddr[] = { 0x08419E03, 0x0841A167, 0x0841A0F5 };
	strcpy((char*)selectDungeon, "seLect dungeon from charac_dungeon where charac_no=%s");
	strcpy((char*)insertDungeon, "inSert into charac_dungeon (charac_no,dungeon) values (%s,'%s')");
	strcpy((char*)updateDungeon, "upDate charac_dungeon set dungeon='%s' where charac_no=%s");
	BYTE* queryStr[] = { selectDungeon, insertDungeon, updateDungeon };
	for (int i = 0, p; i < 3; ++i) {
		writeByteCode(hookCharacNoAddr[i], 0);
		writeDWordCode(hookQueryAddr[i], (int)queryStr[i]);
	}

	writeCallCode(0x08400F4C, (void*)hookDB_CreateCharac___createCharacDungeon);
}


static cdeclCall1P CUserCharacInfo_get_charac_level = (cdeclCall1P)0x80da2b8;
static cdeclCall6P CUser_AddItem = (cdeclCall6P)0x867b6d4;
static cdeclCall2P CNPCDynamicInfoManager__getNPCInfo = (cdeclCall2P)0x08581948;
static cdeclCall2P CNPCDynamicInfoManager__makeNotiPacketNPCMood = (cdeclCall2P)0x085808A6;
static cdeclCall1P CNPCDynamicInfoManager__onTimer = (cdeclCall1P)0x08580722;
static cdeclCall2P CDataManager__find_npc = (cdeclCall2P)0x08363818;
static cdeclCall2P stNPCCommonData_t__getIllustIndex = (cdeclCall2P)0x089FAEEE;
static cdeclCall2P CNPCScriptList__getFavorLevel = (cdeclCall2P)0x085816E4;
static cdeclCall2P CNPCScriptList__getFavorLevelValue = (cdeclCall2P)0x0858174E;
static cdeclCall1P CNPCScriptList__getMaxFavorValue = (cdeclCall1P)0x08582364;
static cdeclCall2P CNPCScript__isRewardLevel = (cdeclCall2P)0x08580FD0;
static cdeclCall3P CNPCScript__isKeyItem = (cdeclCall3P)0x08580E6A;
static cdeclCall1P CNPCScript__isFavorableNPC = (cdeclCall1P)0x085819B6;
static cdeclCall5P CNPCScript__giveGiftItem = (cdeclCall5P)0x085809E4;
static cdeclFCall2P CNPCScript__getFavorRatePerMood = (cdeclFCall2P)0x085819C6;
static cdeclCall1P CNPCDynamicInfo__getMood = (cdeclCall1P)0x08581910;
static cdeclCall6P CNPCDynamicInfo__giveGiftItem = (cdeclCall6P)0x085805A8;
static cdeclCall1P Inven_Item_Inven_Item = (cdeclCall1P)0x80cb854;
static cdeclCall4P RDARScriptStringManager__findString = (cdeclCall4P)0x08AA57FE;
static cdeclCall1P vector_pair_int_int_vector = (cdeclCall1P)0x81349d6;
static cdeclCall1P vector_pair_int_int_clear = (cdeclCall1P)0x817a342;
static stdCall3P make_pair_int_int = (stdCall3P)0x81b8d41;
static cdeclCall2P vector_pair_int_int_push_back = (cdeclCall2P)0x80dd606;

int __hide sendItemMaill(int characNo, const char* title, const char* text, DWORD gold, int list[][2], int listLen) {
	int vector[4];
	int pair[4];

	vector_pair_int_int_vector(vector);
	vector_pair_int_int_clear(vector);

	for (int i = 0, item_id, item_cnt; i < listLen; ++i) {
		make_pair_int_int(pair, (int)&list[i][0], (int)&list[i][1]);
		vector_pair_int_int_push_back(vector, (int)pair);
	}

	BYTE addition_slots[61 * 10];
	for (int i = 0; i < 10; ++i) {
		Inven_Item_Inven_Item((int*)(addition_slots + i * 61));
	}

	WongWork_CMailBoxHelper_MakeSystemMultiMailPostal(vector, (int)addition_slots, 10);

	WongWork_CMailBoxHelper_ReqDBSendNewSystemMultiMail(
		(int*)title,
		(int)addition_slots,
		listLen,
		gold,
		characNo,
		(int)text,
		strlen(text),
		0,
		99,
		1
	);
	return 0;
}

int __hide sendItemMaillSigle(int characNo, const char* title, const char* text, DWORD gold, int itemId, int itemCount) {
	int vector[4];
	int pair[4];
	vector_pair_int_int_vector(vector);
	make_pair_int_int(pair, (int)&itemId, (int)&itemCount);
	vector_pair_int_int_push_back(vector, (int)pair);
	BYTE addition_slots[61];
	Inven_Item_Inven_Item((int*)addition_slots);
	WongWork_CMailBoxHelper_MakeSystemMultiMailPostal(vector, (int)addition_slots, 1);
	WongWork_CMailBoxHelper_ReqDBSendNewSystemMultiMail(
		(int*)title,
		(int)addition_slots,
		1,
		gold,
		characNo,
		(int)text,
		strlen(text),
		0,
		99,
		1);
	return 0;
}

int __hide getNpcFavorData(int charac_no, BYTE* buf, int maxLen) {
	//从数据库中查询角色名
	DPRINTF("getNpcFavorData %d\n", charac_no);
	MySQL_set_query(mysql_taiwan_cain, "select npc_data from charac_npc where charac_no=%d", charac_no);
	if (MySQL_exec(mysql_taiwan_cain, 1)) {
		if (MySQL_get_n_rows(mysql_taiwan_cain) == 1) {
			if (MySQL_fetch(mysql_taiwan_cain)) {
				return MYSQL_get_binary(mysql_taiwan_cain, 0, buf, maxLen);
			}
		}
	}
	return 0;
}

int __hide saveNpcFavorData(int charac_no, BYTE* data, int dataLen) {
	DPRINTF("saveNpcFavorData %d\n", charac_no);
	char* blobStr = (char*)MySQL_blob_to_str(mysql_taiwan_cain, 0, (int)data, dataLen);
	if (!blobStr) {
		printf("%s:%d blob to str error!\n", __func__, __LINE__);
		return 0;
	}
	MySQL_set_query(mysql_taiwan_cain, "upDate charac_npc set npc_data='%s' where charac_no=%d", blobStr, charac_no);
	return MySQL_exec(mysql_taiwan_cain, 1) == 1 ? 1 : 0;
}

static cdeclCall1P vectorIntSize = (cdeclCall1P)0x0808E1C0;
static cdeclCall2P vectorIntAt = (cdeclCall2P)0x08096C72;

int __hide getFavorableNpcIdx(int* stNPCCommonData, int npcId) {
	int size = vectorIntSize((int*)((int)stNPCCommonData + 0x18));
	for (int i = 0, id; i < size; ++i) {
		id = *(int*)vectorIntAt((int*)((int)stNPCCommonData + 0x18), i);
		if (id == npcId) return i;
	}
	return -1;
}

int __hide getFavorableNpcIdByIdx(int* stNPCCommonData, int idx) {
	int size = vectorIntSize((int*)((int)stNPCCommonData + 0x18));
	if (idx >= size) return -1;
	return *(int*)vectorIntAt((int*)((int)stNPCCommonData + 0x18), idx);
}

WORD inline getMaxLevelFavorNpcCount(int* CNPCScriptList, BYTE* npcData) {
	WORD count = 0;
	int cFavor;
	BYTE* data;
	for (int i = 0; i < 50; ++i) {
		data = npcData + i * 12;
		cFavor = RWORD(data, 0);
		if (cFavor == 0) continue;
		if (4 == CNPCScriptList__getFavorLevel(CNPCScriptList, cFavor)/* && RDWORD(data, 8)*/)
			++count;
	}
	return count;
}

int __hide checkNoGiftDays(int* CNPCDynamicInfo, int noGiftDayCount, int cFavor) {
	if (noGiftDayCount > RWORD(CNPCDynamicInfo, 0x17E)) {
		int v = RWORD(CNPCDynamicInfo, 0x180);
		int defaultFavor = RDWORD(CNPCDynamicInfo, 0x178);
		if (cFavor >= v + defaultFavor)
			cFavor -= v;
		else cFavor = defaultFavor;
	}
	return cFavor;
}

int __hide checkMaxFavorStartT(int* CNPCScriptList, int maxFavorStartT, int cFavor) {
	// console.log("checkMaxFavorStartT", CNPCScriptList, npcData, cFavor);
	// var maxFavorStartT = npcData.add(0x8).readU32();
	if ((maxFavorStartT > 0) && (CNPCScriptList__getFavorLevel(CNPCScriptList, cFavor) != 4))//不是信任等级
		maxFavorStartT = 0;//清空信任开始时间计数
	return maxFavorStartT;
}



static stdCall5P CNPCScript__getGiftRewardItem = (stdCall5P)0x08580D30;
int __hide __cdecl Dispatcher_GiveGiftToNPC__dispatch_sig_821E4AC(int* thisP, int* user, int* dataBuff) {
	BYTE npcData[0x258];
	WORD npcId = 0, slot = 0;
	BYTE itemSpace = 0;
	DWORD itemId = 0, count = 0;

	PacketBuf_get_short(dataBuff, (int)&npcId);
	PacketBuf_get_byte(dataBuff, (int)&itemSpace);
	PacketBuf_get_short(dataBuff, (int)&slot);
	PacketBuf_get_int(dataBuff, (int)&itemId);
	PacketBuf_get_int(dataBuff, (int)&count);
	DPRINTF("npcId:%d itemSpace:%d slot:%d itemId:%d count:%d\n", npcId, itemSpace, slot, itemId, count);

	if (itemSpace) {
		DPRINTF("itemSpace error [%d]!\n", itemSpace);
		CUser_SendCmdErrorPacket(user, 225, 19);
		return 0;
	}
	int* mgr = (int*)G_CDataManager();
	int* inven = (int*)CUserCharacInfo_getCurCharacInvenW(user);
	int* eSlotObj = (int*)CInventory_GetInvenRef(inven, INVENTORY_TYPE_ITEM, slot);
	if (Inven_Item_isEmpty(eSlotObj) || Inven_Item_getKey(eSlotObj) != itemId) {
		DPRINTF("empty item slot [%d]\n", eSlotObj);
		// SendMyErrorPacket(user, 209, 0x04);
		CUser_SendCmdErrorPacket(user, 225, 17);
		return 0;
	}

	int* citem = (int*)CDataManager_find_item(mgr, itemId);
	if (CItem__IsImpossibleGift(citem)) {
		DPRINTF("Impossible Gift item [%d]\n", itemId);
		CUser_SendCmdErrorPacket(user, 225, 19);
		return 0;
	}
	int cCount;
	if (Inven_Item_isEquipableItemType(eSlotObj)) {
		cCount = 1;
	}
	else {
		cCount = RDWORD(eSlotObj, 7);
		if (cCount < count) {
			DPRINTF("item count no enough [%d, %d]\n", cCount, count);
			CUser_SendCmdErrorPacket(user, 225, 22);
			return 0;
		}
	}

	int* CNPCDynamicInfoAddr = (int*)CNPCDynamicInfoManager__getNPCInfo((int*)mgr[0x2A35], npcId);
	int* CNPCDynamicInfo = *(int**)CNPCDynamicInfoAddr;

	// var CNPCScript = CDataManager__find_npc(mgr, npcId);
	if (!RBYTE(CNPCDynamicInfo, 0x174)) {
		DPRINTF("not favorable npc %d\n", npcId);
		CUser_SendCmdErrorPacket(user, 276, 7);
		return 0; //不是好感度npc
	}

	int* CNPCScriptList = (int*)mgr[0x2A34];
	int idx = getFavorableNpcIdx(CNPCScriptList + 5, npcId);
	int characNo = CUserCharacInfo_getCurCharacNo(user);
	BYTE needSave = 1;
	BYTE* cNpcData = npcData + idx * 12;
	if (0x258 != getNpcFavorData(characNo, npcData, 0x258)) {
		memset(npcData, 0, 0x258);
		DWORD defaultFavor = RDWORD(CNPCDynamicInfo, 0x178);
		DPRINTF("empty load default favor:%d\n", defaultFavor);
		WWORD(cNpcData, 0, defaultFavor);//当前好感度
		//WWORD(cNpcData, 2, 0);//当日已送物品数
		//WBYTE(cNpcData, 4, 0);//当前已经送过duration礼物
		//WBYTE(cNpcData, 5, 0);//当前等级已经送过level礼物
		//WWORD(cNpcData, 6, 0); //不送礼物天数
		//WDWORD(cNpcData, 8, 0);//信任开始时间
	}

	struct {
		DWORD key;
		BYTE isFavorEvent;
		BYTE favorType;
		BYTE favorExcp;
		BYTE favorValueSta;
		BYTE favorM;
		BYTE bannerType;
		BYTE favorLevelSta;
		BYTE eventFavorDialogPos;
		DWORD v36;
		DWORD npcId;
		DWORD itemSlot;
		DWORD favorV;
		DWORD itemCount;
		DWORD npcRetGiftItemId;
		DWORD npcRetGiftItemCount;
		DWORD itemId;
	}binaryData = { 0 };

	BYTE cmood = CNPCDynamicInfo__getMood(CNPCDynamicInfoAddr);

	binaryData.isFavorEvent = 0;
	binaryData.npcId = npcId;
	binaryData.v36 = 0;
	binaryData.favorM = cmood;
	binaryData.bannerType = itemSpace;
	binaryData.itemSlot = slot;
	binaryData.itemId = itemId;
	binaryData.itemCount = cCount - count;

	DWORD excp = 0;
	BYTE cFavorLv;
	short opFavor;
	WORD cFavor = RWORD(cNpcData, 0);
	if (!cFavor) {
		DWORD defaultFavor = RDWORD(CNPCDynamicInfo, 0x178);
		DPRINTF("error favor value, load default favor:%d\n", defaultFavor);
		cFavor = defaultFavor;
		WWORD(cNpcData, 0, cFavor);//当前好感度
	}
	WORD giftCount = RWORD(cNpcData, 2) + 1;
	WORD maxGiftPerDay = RWORD(CNPCDynamicInfo, 0x17C);
	if (giftCount >= maxGiftPerDay) {
		DPRINTF("over maxGiftPerDay!\n");
		binaryData.favorType = 1;
		binaryData.favorLevelSta = 0;
		binaryData.favorValueSta = 0;
		binaryData.favorExcp = 2;
		binaryData.favorV = cFavor;
		goto exit;
	}

	if (CInventory_delete_item(inven, INVENTORY_TYPE_ITEM, slot, count, 10, 1) != 1) {
		printf("delete item error [%d, %d]\n", slot, count);
		CUser_SendCmdErrorPacket(user, 276, 1);
		goto exit2;
	}

	excp = 0;
	cFavorLv = CNPCScriptList__getFavorLevel(CNPCScriptList, cFavor);
	opFavor = CNPCScript__giveGiftItem(CNPCDynamicInfo, cFavorLv, CDataManager_find_item(mgr, itemId), count, (int)&excp);
	//int opFavor = CNPCDynamicInfo__giveGiftItem(CNPCDynamicInfoAddr, cFavorLv, 0, count, CDataManager_find_item(mgr, itemId), (int) & excp);
	DPRINTF("giveGift:[%d, %d, %d, %d, %d, %d, %d]\n", cFavor, cFavorLv, count, excp, opFavor, cmood, giftCount);
	if (opFavor) {
		int maxFavor = CNPCScriptList__getMaxFavorValue(CNPCScriptList);
		float rate = CNPCScript__getFavorRatePerMood(CNPCDynamicInfo, cmood);
		opFavor = opFavor * rate;
		if (opFavor < 0 && cFavor <= -opFavor) cFavor = 1;
		else cFavor += opFavor;
		if (cFavor > maxFavor) cFavor = maxFavor;
	}

	if (excp) {
		binaryData.favorType = 1;
		binaryData.favorLevelSta = 0;
		binaryData.favorValueSta = 0;
		binaryData.favorExcp = excp;
		binaryData.favorV = cFavor;
	}
	else {
		BYTE favorValueSta = 0, favorLevelSta = 0;
		if (opFavor > 0) favorValueSta = 1;
		else if (opFavor < 0) favorValueSta = 2;
		BYTE newFavorLv = CNPCScriptList__getFavorLevel(CNPCScriptList, cFavor);
		DPRINTF("ChangeLv %d->%d %d\n", cFavorLv, newFavorLv, cFavor);
		if (newFavorLv > cFavorLv) {
			if (newFavorLv == 4) {//进入信任等级
				//开始计时
				WORD maxFavorLevelnpc = RWORD(CNPCScriptList, 0x14 + 0x34);
				WORD cMaxFavorLevelnpc = getMaxLevelFavorNpcCount(CNPCScriptList, npcData);
				if (cMaxFavorLevelnpc < maxFavorLevelnpc) {
					DWORD now = CurrentSecTime;
					WDWORD(cNpcData, 8, now);
					favorLevelSta = 1;
					DPRINTF("enterToMaxLv %u [%d, %d]\n", now, maxFavorLevelnpc, cMaxFavorLevelnpc);
				}
				else {
					newFavorLv -= 1;
					cFavor = CNPCScriptList__getFavorLevelValue(CNPCScriptList, 3) - 1;
					favorLevelSta = 0;
					DPRINTF("over MaxLvNpcCount [%d, %d][%d, %d]\n", maxFavorLevelnpc, cMaxFavorLevelnpc, newFavorLv, cFavor);
				}
			}
		}
		else if (newFavorLv < cFavorLv) {
			favorLevelSta = 2;
			if (cFavorLv == 4) {//退出信任等级
				WDWORD(cNpcData, 8, 0);//取消计时
			}
		}

		BYTE retGiftItem[61] = { 0 };
		CNPCScript__getGiftRewardItem((int*)retGiftItem, (int)CNPCDynamicInfo, cmood, favorValueSta, newFavorLv);
		DPRINTF("retGift:[%d, %d, %d, %d]\n", favorValueSta, favorLevelSta, newFavorLv, cmood);

		bool needRetGift = Inven_Item_isEmpty((int*)retGiftItem) ? 1 : 0;
		binaryData.favorType = needRetGift;
		binaryData.favorLevelSta = favorLevelSta;
		binaryData.favorValueSta = favorValueSta;
		binaryData.favorExcp = 0;
		binaryData.favorV = cFavor;
		if (!needRetGift) {
			DPRINTF("make ret gift\n");
			DWORD retGiftItemId = Inven_Item_getKey((int*)retGiftItem);
			DWORD retGiftItemCount = RDWORD(retGiftItem, 7);
			DWORD item_space = ENUM_ITEMSPACE_INVENTORY;
			int slot = CUser_AddItem(user, retGiftItemId, retGiftItemCount, 6, (int)&item_space, 0);
			//printf("CUser_AddItem %d[%d,%d]\n", slot, retGiftItemId, retGiftItemCount);
			if (slot >= 0) {
				//通知客户端有游戏道具更新
				CUser_SendUpdateItemList(user, 1, item_space, slot);
			}
			else {
				sendItemMaillSigle(characNo, "回赠礼物", "Npc回赠礼物时，背包空间不足，使用邮箱发送！", 0, retGiftItemId, retGiftItemCount);
			}
			//CInventory::get_empty_slot(inven, INVENTORY_TYPE_ITEM, CInventory::GetItemType);
			binaryData.npcRetGiftItemId = retGiftItemId;
			binaryData.npcRetGiftItemCount = retGiftItemCount;
		}
	}
exit:
	int packet_guard[48];
	PacketGuard_PacketGuard(packet_guard);
	InterfacePacketBuf_put_header(packet_guard, 1, 225);
	InterfacePacketBuf_put_byte(packet_guard, 1);//resultCode
	InterfacePacketBuf_put_int(packet_guard, sizeof(binaryData));
	InterfacePacketBuf_put_binary(packet_guard, (int)&binaryData, sizeof(binaryData));
	InterfacePacketBuf_finalize(packet_guard, 1);
	CUser_Send(user, (int)packet_guard);
	Destroy_PacketGuard_PacketGuard(packet_guard);
exit2:
	WWORD(cNpcData, 0, cFavor);
	WWORD(cNpcData, 2, giftCount);
	if (needSave) saveNpcFavorData(characNo, npcData, 0x258);
	return 0;
}

int __hide __cdecl CUser__resetNPCRelationShipDailyData(int* user) {
	BYTE npcData[0x258], * data;
	int characNo = CUserCharacInfo_getCurCharacNo(user);
	//printf("-------------------------------------------------------------------------resetNPCRelationShipDailyData %d\n", characNo);
	if (0x258 != getNpcFavorData(characNo, npcData, 0x258)) return 0;

	int* mgr = (int*)G_CDataManager();
	int* CNPCScriptList = (int*)mgr[0x2A34];
	int* CNPCDynamicInfoManager = (int*)mgr[0x2A35];
	int* CNPCDynamicInfo;
	WORD maxFavorLevelnpc = RWORD(CNPCScriptList, 0x14 + 0x34);
	WORD cMaxFavorLevelnpc = 0;
	for (int i = 0, cFavor, giftCount, noGiftDayCount, durationGift, npcId, maxFavorStartT; i < 50; ++i) {
		data = npcData + i * 12;
		cFavor = RWORD(data, 0);
		if (cFavor == 0) continue;
		npcId = getFavorableNpcIdByIdx(CNPCScriptList + 5, i);
		giftCount = RWORD(data, 2);
		noGiftDayCount = RWORD(data, 6);
		maxFavorStartT = RDWORD(data, 8);
		DPRINTF("npc:[%d, %d, %d, %d, %d]\n", npcId, cFavor, maxFavorStartT, giftCount, noGiftDayCount);

		if (giftCount == 0) ++noGiftDayCount;
		else noGiftDayCount = 0;
		giftCount = 0;

		CNPCDynamicInfo = *((int**)CNPCDynamicInfoManager__getNPCInfo(CNPCDynamicInfoManager, npcId));
		cFavor = checkNoGiftDays(CNPCDynamicInfo, noGiftDayCount, cFavor);
		maxFavorStartT = checkMaxFavorStartT(CNPCScriptList, maxFavorStartT, cFavor);
		DPRINTF("checkNpc:[%d, %d, %d]\n", npcId, cFavor, maxFavorStartT);

		if (4 == CNPCScriptList__getFavorLevel(CNPCScriptList, cFavor)) {
			if (++cMaxFavorLevelnpc > maxFavorLevelnpc) {
				cFavor = CNPCScriptList__getFavorLevelValue(CNPCScriptList, 3) - 1;
			}
		}

		durationGift = RBYTE(data, 4);//当前已经送过duration礼物
		if (maxFavorStartT && !durationGift) {
			//[favor_duration_reward]
			DWORD day = RWORD(CNPCDynamicInfo, 0x670);
			DWORD passDay = (CurrentSecTime - maxFavorStartT) / (60 * 60 * 24);
			DPRINTF("checkdurationGift:[%d, %d, %d]", npcId, day, passDay);
			if (passDay >= day) {
				DWORD itemId = RDWORD(CNPCDynamicInfo, 0x678);
				DWORD itemCount = RDWORD(CNPCDynamicInfo, 0x67C);
				int mailTitle = RDWORD(CNPCDynamicInfo, 0x680);
				int mailCtx = RDWORD(CNPCDynamicInfo, 0x684);
				DPRINTF("mailTitle1:[%s, %s]\n", mailTitle, mailCtx);
				mailTitle = RDARScriptStringManager__findString((int*)0x0949B140, 4, mailTitle, 0);
				mailCtx = RDARScriptStringManager__findString((int*)0x0949B140, 4, mailCtx, 0);
				DPRINTF("mailTitle2:[%s, %s]\n", mailTitle, mailCtx);
				sendItemMaillSigle(characNo, (char*)mailTitle, (char*)mailCtx, 0, itemId, itemCount);
				durationGift = 1;
			}
		}
		WWORD(data, 0, cFavor);
		WWORD(data, 2, giftCount);
		WBYTE(data, 4, durationGift);
		WWORD(data, 6, noGiftDayCount);
		WDWORD(data, 8, maxFavorStartT);
	}
	saveNpcFavorData(characNo, npcData, 0x258);
	return 0;
}

int __hide __cdecl CUser__sendNPCRelationShipFavor(int* user) {
	BYTE npcData[0x258] = { 0 }, buf[50 * 5], * data, save = 0;
	int characNo = CUserCharacInfo_getCurCharacNo(user);
	DPRINTF("sendNPCRelationShipFavor %d\n", characNo);
	if (0x258 != getNpcFavorData(characNo, npcData, 0x258)) {
		DPRINTF("no NpcFavorData %d\n", characNo);
		save = 1;
	}

	int* mgr = (int*)G_CDataManager();
	int* CNPCScriptList = (int*)mgr[0x2A34];
	int* CNPCDynamicInfoManager = (int*)mgr[0x2A35];
	int* CNPCDynamicInfo, * CNPCDynamicInfoAddr;

	int count = 0;
	for (int i = 0, pos, npcId, mood, cFavor; i < 50; ++i) {
		npcId = getFavorableNpcIdByIdx(CNPCScriptList + 5, i);
		if (npcId == -1) break;
		pos = i * 5;
		data = npcData + i * 12;
		CNPCDynamicInfoAddr = (int*)CNPCDynamicInfoManager__getNPCInfo(CNPCDynamicInfoManager, npcId);
		if (!CNPCDynamicInfoAddr) continue;
		mood = CNPCDynamicInfo__getMood(CNPCDynamicInfoAddr);
		CNPCDynamicInfo = *(int**)CNPCDynamicInfoAddr;
		cFavor = RWORD(data, 0);
		if (!cFavor) {
			DWORD defaultFavor = RDWORD(CNPCDynamicInfo, 0x178);
			WWORD(data, 0, defaultFavor);//当前好感度
			cFavor = defaultFavor;
			//WWORD(cNpcData, 2, 0);//当日已送物品数
			//WBYTE(cNpcData, 4, 0);//当前已经送过duration礼物
			//WBYTE(cNpcData, 5, 0);//当前等级已经送过level礼物
			//WWORD(cNpcData, 6, 0); //不送礼物天数
			//WDWORD(cNpcData, 8, 0);//信任开始时间
		}
		WWORD(buf, pos, npcId);
		WWORD(buf, pos + 2, cFavor);
		WBYTE(buf, pos + 4, mood);
		DPRINTF("%s npc[%d, %d, %d]\n", __func__, npcId, cFavor, mood);
		count += 5;
	}
	if (save) saveNpcFavorData(characNo, npcData, 0x258);
	// console.log("count", count);
	int* packet_guard = (int*)npcData;
	PacketGuard_PacketGuard(packet_guard);
	InterfacePacketBuf_put_header(packet_guard, 0, 194);//Noti194NpcFavor
	InterfacePacketBuf_put_int(packet_guard, count);
	InterfacePacketBuf_put_binary(packet_guard, (int)buf, count);
	InterfacePacketBuf_finalize(packet_guard, 1);
	CUser_Send(user, (int)packet_guard);
	Destroy_PacketGuard_PacketGuard(packet_guard);
	return 0;
}

static stdCall3P CNPCScript__getLevelRewardInfo = (stdCall3P)0x08581076;
int __hide __cdecl CUser__processNPCGiftOnLevelUp(int* user) {
	BYTE npcData[0x258], * data;
	int characNo = CUserCharacInfo_getCurCharacNo(user);
	DPRINTF("CUser__processNPCGiftOnLevelUp %d\n", characNo);
	if (0x258 != getNpcFavorData(characNo, npcData, 0x258)) return 0;

	int* mgr = (int*)G_CDataManager();
	int* CNPCScriptList = (int*)mgr[0x2A34];
	int* CNPCDynamicInfoManager = (int*)mgr[0x2A35];
	int* CNPCDynamicInfo, ** CNPCDynamicInfoAddr;

	int* favorLevelRewardNpc = CNPCScriptList + 0x13;
	int levelRewardInfo[5];
	BYTE needSave = 0;

	int size = vectorIntSize(favorLevelRewardNpc);
	for (int i = 0, pos, cFavor, giftLevel, npcId, maxFavorStartT; i < size; ++i) {
		npcId = *(int*)vectorIntAt(favorLevelRewardNpc, i);
		pos = getFavorableNpcIdx(CNPCScriptList + 5, npcId);
		if (pos == -1) continue;
		data = npcData + pos * 12;
		cFavor = RWORD(data, 0);
		//DPRINTF("LevelGiveBy:[%d, %d, %d]\n", pos, npcId, cFavor);
		if (cFavor == 0) continue;
		CNPCDynamicInfoAddr = (int**)CNPCDynamicInfoManager__getNPCInfo(CNPCDynamicInfoManager, npcId);
		if (!CNPCDynamicInfoAddr) continue;
		CNPCDynamicInfo = *CNPCDynamicInfoAddr;

		cFavor = checkNoGiftDays(CNPCDynamicInfo, RWORD(data, 6), cFavor);
		maxFavorStartT = checkMaxFavorStartT(CNPCScriptList, RDWORD(data, 8), cFavor);
		if (maxFavorStartT) {//好感度为信任
			// stLevelRewardInfo
			// WORD level
			// DWORD itemId
			// DWORD itemCount
			// String mailTitle
			// String mailCtx
			giftLevel = RBYTE(data, 5);
			WWORD(levelRewardInfo, 0, giftLevel);
			WORD cLv = CUserCharacInfo_get_charac_level(user);
			CNPCScript__getLevelRewardInfo(levelRewardInfo, (int)CNPCDynamicInfo, cLv);
			DPRINTF("LevelGive:[%d, %d, %d]\n", npcId, cLv, giftLevel);
			WORD levelRewardInfo_lv = RWORD(levelRewardInfo, 0);
			if (!levelRewardInfo_lv || levelRewardInfo_lv == giftLevel) continue;
			DWORD levelRewardInfo_id = RDWORD(levelRewardInfo, 4);
			DWORD levelRewardInfo_cnt = RDWORD(levelRewardInfo, 8);
			DPRINTF("LevelGiveInf:[%d, %d, %d]\n", levelRewardInfo_lv, levelRewardInfo_id, levelRewardInfo_cnt);
			int mailTitle = RDWORD(levelRewardInfo, 0x0c);
			int mailCtx = RDWORD(levelRewardInfo, 0x10);
			DPRINTF("mailTitle1:[%s, %s]\n", mailTitle, mailCtx);
			mailTitle = RDARScriptStringManager__findString((int*)0x0949B140, 4, mailTitle, 0);
			mailCtx = RDARScriptStringManager__findString((int*)0x0949B140, 4, mailCtx, 0);
			DPRINTF("mailTitle2:[%s, %s]\n", mailTitle, mailCtx);
			sendItemMaillSigle(characNo, (char*)mailTitle, (char*)mailCtx, 0, levelRewardInfo_id, levelRewardInfo_cnt);
		}
		WWORD(data, 0, cFavor);
		WBYTE(data, 5, giftLevel);
		WDWORD(data, 8, maxFavorStartT);
		needSave = 1;
	}
	if (needSave) saveNpcFavorData(characNo, npcData, 0x258);
	return 0;
}

static stdCall3P CNPCScript__getBuffRewardInfo = (stdCall3P)0x085811F8;
int __hide __cdecl Dispatcher_DungeonNPCBuffInfo__dispatch_sig(int* thisP, int* user, int* dataBuff) {
	DPRINTF("Dispatcher_DungeonNPCBuffInfo__dispatch_sig\n");
	BYTE npcData[0x258], * data;
	int characNo = CUserCharacInfo_getCurCharacNo(user);
	if (0x258 != getNpcFavorData(characNo, npcData, 0x258)) return 0;

	int* mgr = (int*)G_CDataManager();
	int* CNPCScriptList = (int*)mgr[0x2A34];
	int* CNPCDynamicInfoManager = (int*)mgr[0x2A35];
	int* CNPCDynamicInfo;

	for (int i = 0, cFavor, maxFavorStartT, npcId; i < 50; ++i) {
		data = npcData + i * 12;
		cFavor = RWORD(data, 0);
		if (!cFavor) continue;
		// cFavor = checkNoGiftDays(CNPCDynamicInfo, data.add(0x6).readU16(), cFavor);
		maxFavorStartT = checkMaxFavorStartT(CNPCScriptList, RDWORD(data, 8), cFavor);
		if (!maxFavorStartT) continue;
		DPRINTF("cFavor:%d %d\n", cFavor, maxFavorStartT);
		//好感度为信任
		npcId = getFavorableNpcIdByIdx(CNPCScriptList + 5, i);
		if (npcId == -1) continue;
		CNPCDynamicInfo = (int*)CNPCDynamicInfoManager__getNPCInfo(CNPCDynamicInfoManager, npcId);
		if (!CNPCDynamicInfo) continue;
		CNPCDynamicInfo = *(int**)CNPCDynamicInfo;

		DWORD passDay = (CurrentSecTime - maxFavorStartT) / (60 * 60 * 24);
		//DWORD passDay = 30;
		int buffInfo[4] = { 0 };
		CNPCScript__getBuffRewardInfo(buffInfo, (int)CNPCDynamicInfo, passDay);
		WORD day = RWORD(buffInfo, 0);
		WORD buffId = RWORD(buffInfo, 2);
		int probability = RDWORD(buffInfo, 4);
		DPRINTF("[%d, %d] getBuffRewardInfo:[%d, %d, %d]\n", maxFavorStartT, passDay, day, buffId, probability);

		if (!buffId) continue;
		int* packet_guard = (int*)npcData;
		PacketGuard_PacketGuard(packet_guard);
		InterfacePacketBuf_put_header(packet_guard, 1, 276);
		InterfacePacketBuf_put_byte(packet_guard, 1);//retCode
		InterfacePacketBuf_put_short(packet_guard, npcId);
		InterfacePacketBuf_put_byte(packet_guard, buffId);
		InterfacePacketBuf_finalize(packet_guard, 1);
		CUser_Send(user, (int)packet_guard);
		Destroy_PacketGuard_PacketGuard(packet_guard);
		return 0;
	}
	return 0;
}

static cdeclCall G_TimerQueue = (cdeclCall)0x80f647c;
static cdeclCall7P TimerQueue__InsertTimer = (cdeclCall7P)0x08630E16;
int __hide __cdecl TimerNPCMoodChange__RegistNextTimer(int* thisP) {
	return TimerQueue__InsertTimer((int*)G_TimerQueue(), 2, 0, 116, 100, 0, 0);
}

bool __hide __cdecl TimerNPCMoodChange__dispatch_sig(int* thisP, int a2, int a3) {
	DPRINTF("TimerNPCMoodChange__dispatch_sig %d\n", CurrentSecTime);
	int* mgr = (int*)G_CDataManager();
	int* CNPCDynamicInfoManager = (int*)mgr[0x2A35];
	CNPCDynamicInfoManager__onTimer(CNPCDynamicInfoManager);
	TimerNPCMoodChange__RegistNextTimer(NULL);
	return 1;
}

static cdeclCall1P CUser__onSelectCharacter = (cdeclCall1P)0x086800C6;
int __hide __cdecl hookCUser__onSelectCharacter(int* user) {
	DPRINTF("CUser::onSelectCharacter\n");
	int ret = CUser__onSelectCharacter(user);
	//CUser__resetNPCRelationShipDailyData(user);
	CUser__sendNPCRelationShipFavor(user);
	//TimerNPCMoodChange__RegistNextTimer(NULL);
	extern int characExReset(int* user, BYTE isForce);
	characExReset(user, false);
	return ret;
}

static cdeclCall1P CUserCharacInfo__getCurCharacR = (cdeclCall1P)0x08120432;
static cdeclBCall2P CUser__RecoverFatigue = (cdeclBCall2P)0x08657ADA;
char __cdecl resetNPCRelationShipDailyData(int* user, int a2) {
	bool ret = CUser__RecoverFatigue(user, a2);
	if (CUserCharacInfo__getCurCharacR(user))
		CUser__resetNPCRelationShipDailyData(user);
	return ret;
}


//static gboolean
//_gum_found_module_func(GumModuleDetails* details, gpointer self)
//{
//	DPRINTF("name:%s path:%s range:[0x%p, %d]\n", details->name, details->path, GSIZE_TO_POINTER(details->range->base_address), details->range->size);
//	return TRUE;
//}

static inline void  __inline npcFavorInit() {


	writeCallCode(0x084C1416, (void*)hookCUser__onSelectCharacter);
	writeDWordCode(0x08CE8A60, (int)TimerNPCMoodChange__dispatch_sig);
	//gum_mprotect((gpointer)0x08CE8A60, 4, GUM_PAGE_RWX); *(DWORD*)0x08CE8A60 = (int)TimerNPCMoodChange__dispatch_sig;
	writeCallCode(0x0858166B, (void*)TimerNPCMoodChange__RegistNextTimer);
	writeDWordCode(0x08BD6DD4, (int)Dispatcher_DungeonNPCBuffInfo__dispatch_sig);
	//gum_mprotect((gpointer)0x08BD6DD4, 4, GUM_PAGE_RWX); *(DWORD*)0x08BD6DD4 = (int)Dispatcher_DungeonNPCBuffInfo__dispatch_sig;
	GumInterceptor* v = gum_interceptor_obtain();
	gum_interceptor_replace_fast(v, (gpointer)0x0868121E, (gpointer)CUser__sendNPCRelationShipFavor, NULL);
	gum_interceptor_replace_fast(v, (gpointer)0x08681218, (gpointer)CUser__resetNPCRelationShipDailyData, NULL);
	writeCallCode(0x084C3365, (void*)resetNPCRelationShipDailyData);

	writeCallCode(0x086638C6, (void*)CUser__processNPCGiftOnLevelUp);
	//gum_interceptor_replace_fast(v, (gpointer)0x0821E4AC, (gpointer)Dispatcher_GiveGiftToNPC__dispatch_sig_821E4AC, NULL);
	writeDWordCode(0x08BD73D4, (int)Dispatcher_GiveGiftToNPC__dispatch_sig_821E4AC);
	writeDWordCode(0x0858192D, 2);
	//gum_mprotect((gpointer)0x08BD73D4, 4, GUM_PAGE_RWX); *(DWORD*)0x08BD73D4 = (int)Dispatcher_GiveGiftToNPC__dispatch_sig_821E4AC;
	//gum_mprotect((gpointer)0x0858192D, 4, GUM_PAGE_RWX); *(DWORD*)0x0858192D = 2;
}


//初始化数据库(打开数据库/建库建表/数据库字段扩展)
static inline int  __inline MYSQL_init(const char* user, const char* pwd, const char* ip) {
	if (!mysql_taiwan_cain) {
		mysql_taiwan_cain = MYSQL_open("taiwan_cain", ip, 3306, user, pwd);
	}

	if (!mysql_d_guild) {
		mysql_d_guild = MYSQL_open("d_guild", ip, 3306, user, pwd);
	}

	MySQL_set_query(mysql_taiwan_cain, "create database if not exists fd default charset utf8;");
	MySQL_exec(mysql_taiwan_cain, 1);

	if (!mysql_fd) {
		mysql_fd = MYSQL_open("fd", ip, 3306, user, pwd);
	}
	MySQL_set_query(mysql_fd, " CREATE TABLE `fd`.`charac_ex`(`charac_no` int(11) NOT NULL,`ex_data` blob NOT NULL,`flag` tinyint(4) NOT NULL DEFAULT '0' ,PRIMARY KEY (`charac_no`)) ENGINE=InnoDB DEFAULT CHARSET=utf8;");
	MySQL_exec(mysql_fd, 1);
	return 0;
}

//关闭数据库（卸载插件前调用）
static inline int  __inline MYSQL_deinit() {
	//关闭数据库连接
	if (mysql_taiwan_cain) {
		MySQL_close(mysql_taiwan_cain);
		mysql_taiwan_cain = NULL;
	}

	if (mysql_d_guild) {
		MySQL_close(mysql_d_guild);
		mysql_d_guild = NULL;
	}

	if (mysql_fd) {
		MySQL_close(mysql_fd);
		mysql_fd = NULL;
	}
	return 0;
}

//int spTable[] = { 15, 17, 18, 20, 21, 23, 25, 26, 28, 30, 32, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 60, 62, 64, 66, 69, 71, 74, 77, 79, 81, 83, 85, 87, 91, 94, 96, 99, 102, 104, 107, 110, 113, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 146, 149, 152, 156, 159, 162, 166, 169, 172, 176, 179, 183, 186 };
std::vector<int> spTable;
cdeclCall1P ScanInt = (cdeclCall1P)0x088BC37B;
int __cdecl loadSpTable(int* ret) {
	int value = ScanInt(ret);
	//printf("push:%d\n", value);
	spTable.push_back(value);
	return 0;
}

cdeclCall2P CUserCharacInfo__setCurCharacExp = (cdeclCall2P)0x0819A87C;
cdeclCall2P CDataManager__get_level_exp = (cdeclCall2P)0x08360442;
cdeclCall1P CUserCharacInfo__get_charac_exp = (cdeclCall1P)0x084EC05C;

int __hide getCharacExData(int charac_no, BYTE* buf, int maxLen) {
	//从数据库中查询角色名
	DPRINTF("getCharacExData %d\n", charac_no);
	MySQL_set_query(mysql_fd, "select ex_data from charac_ex where charac_no=%d;", charac_no);
	if (MySQL_exec(mysql_fd, 1)) {
		if (MySQL_get_n_rows(mysql_fd) == 1) {
			if (MySQL_fetch(mysql_fd)) {
				return MYSQL_get_binary(mysql_fd, 0, buf, maxLen);
			}
		}
	}
	MySQL_set_query(mysql_fd, "insert into charac_ex (charac_no,ex_data) values(%d,'');", charac_no);
	MySQL_exec(mysql_fd, 1);
	return 0;
}

int __hide saveCharacExData(int charac_no, BYTE* data, int dataLen) {
	DPRINTF("saveCharacExData %d\n", charac_no);
	char* blobStr = (char*)MySQL_blob_to_str(mysql_fd, 0, (int)data, dataLen);
	if (!blobStr) {
		printf("%s:%d blob to str error!\n", __func__, __LINE__);
		return 0;
	}
	MySQL_set_query(mysql_fd, "upDate charac_ex set ex_data='%s' where charac_no=%d", blobStr, charac_no);
	if (MySQL_exec(mysql_fd, 1) != 1) {
		printf("%s:%d save data error!\n", __func__, __LINE__);
		return 0;
	}
	return 1;
}

cdeclCall2P CUser__increase_status = (cdeclCall2P)0x086657FC;
void __cdecl hookCUser__increase_status(int* user, WORD slot) {
	int slotObj = CInventory_GetInvenRef((int*)CUserCharacInfo_getCurCharacInvenR(user), INVENTORY_TYPE_ITEM, slot);
	int itemId = RDWORD(slotObj, 2);
	CUser__increase_status(user, slot);
	DPRINTF("use %d %d\n", slot, itemId);
	if ((itemId < 1039 && itemId != 1031 && itemId != 1038) || (itemId > 1046 && itemId != 1204 && itemId != 1205))
		return;
	int characNo = CUserCharacInfo_getCurCharacNo(user);
	BYTE exData[48] = { 0 };
	getCharacExData(characNo, exData, 48);

	switch (itemId) {
	case 1031://5sp
		WDWORD(exData, 0, RDWORD(exData, 0) + 5);//sp0
		WDWORD(exData, 4, RDWORD(exData, 4) + 5);//sp1
		break;
	case 1038:
		WDWORD(exData, 0, RDWORD(exData, 0) + 20);//sp0
		WDWORD(exData, 4, RDWORD(exData, 4) + 20);//sp1
		break;
	case 1204:
		WDWORD(exData, 8, RDWORD(exData, 8) + 1);//tp0
		WDWORD(exData, 12, RDWORD(exData, 12) + 1);//tp1
		break;
	case 1205:
		WDWORD(exData, 8, RDWORD(exData, 8) + 5);//tp0
		WDWORD(exData, 12, RDWORD(exData, 12) + 5);//tp1
		break;
	case 1039://4 50	pos	8 WORD
	case 1040://6 50	pos 0xC WORD
	case 1041://5 50	pos 0xA WORD
	case 1042://7 50	pos 0xE WORD
	case 1043://2 250 	pos 0 DWORD
	case 1044://3 250 	pos 4 DWORD
	case 1045://8 10	pos 0x42 DWORD
	case 1046://9 10	WORD for ( i = 0; i <= 3; ++i ) *(_WORD *)(base + 2 * (i + 8)) += (_WORD)GuildExpBook;
		WDWORD(exData, 16 + (itemId - 1039) * 4, RDWORD(exData, 16 + (itemId - 1039) * 4) + 1);//stone
		break;
	}
	saveCharacExData(characNo, exData, 48);
}

cdeclCall2P CUser_gain_sp = (cdeclCall2P)0x0866A9A0;
cdeclCall4P CUser_history_log_sp = (cdeclCall4P)0x0866AC0E;
cdeclCall2P CUser_gain_sfp = (cdeclCall2P)0x0866AAD2;
cdeclCall4P CUser_history_log_sfp = (cdeclCall4P)0x0866ACD0;
void rechangeEX(int* user, int type, int num) {//sp tp充值，格式在下面
	BYTE opType;
	if (type == 0) {
		CUser_gain_sp(user, num);
		CUser_history_log_sp(user, -1, num, 1);
		opType = 0;
	}
	else if (type == 1) {
		CUser_gain_sfp(user, num);
		CUser_history_log_sfp(user, -1, num, 1);
		opType = 17;
	}
	else {
		return;
	}
	int packet_guard[16];
	PacketGuard_PacketGuard(packet_guard);
	InterfacePacketBuf_put_header(packet_guard, 1, 32);
	InterfacePacketBuf_put_byte(packet_guard, 1);
	InterfacePacketBuf_put_short(packet_guard, -1);
	InterfacePacketBuf_put_byte(packet_guard, opType);
	InterfacePacketBuf_put_int(packet_guard, num);
	InterfacePacketBuf_put_short(packet_guard, 0);
	InterfacePacketBuf_put_short(packet_guard, 0);
	InterfacePacketBuf_finalize(packet_guard, 1);
	CUser_Send(user, (int)packet_guard);
	Destroy_PacketGuard_PacketGuard(packet_guard);
}

cdeclCall1P CUser__GetUserMaxLevel = (cdeclCall1P)0x0868FE1E;
DWORD getCurExpSp(int* user) {
	int* mgr = (int*)G_CDataManager();
	BYTE lv = CUserCharacInfo_get_charac_level(user);
	DWORD lv_exp = CDataManager__get_level_exp(mgr, lv + 1);
	DWORD last_lv_exp = CDataManager__get_level_exp(mgr, lv);
	DWORD cur_exp = CUserCharacInfo__get_charac_exp(user);
	DWORD totalSp = spTable[lv - 1];
	if (totalSp == 0 || cur_exp == last_lv_exp || lv_exp == last_lv_exp) return 0;
	DWORD new_sp = (cur_exp - last_lv_exp) / (float)(lv_exp - last_lv_exp) * totalSp;//同步下面算法 保持一致
	//printf("getCurExpSp %d[%d, %d][%d][%d, %d]\n", lv, lv_exp, last_lv_exp, cur_exp, totalSp, new_sp);
	return new_sp;
}

void checkExpSpByLevel(int* user, int newExp, BYTE lv, int cur_exp) {
	if (cur_exp >= newExp)return;
	int* mgr = (int*)G_CDataManager();
	DWORD last_lv_exp = CDataManager__get_level_exp(mgr, lv);
	if (cur_exp < last_lv_exp)return;
	DWORD lv_exp = CDataManager__get_level_exp(mgr, lv + 1);
	DWORD totalSp = spTable[lv - 1];

	DWORD currentSp;
	if (totalSp == 0 || cur_exp == last_lv_exp || lv_exp == last_lv_exp)
		currentSp = 0;
	else
		currentSp = (cur_exp - last_lv_exp) / (float)(lv_exp - last_lv_exp) * totalSp;

	DWORD newSp = 0;
	if (newExp == lv_exp) {
		newSp = totalSp - currentSp;
	}
	else {
		if (totalSp == 0 || newExp == last_lv_exp || lv_exp == last_lv_exp)
			newSp = 0;
		else {
			newSp = (newExp - last_lv_exp) / (float)(lv_exp - last_lv_exp) * totalSp;
			newSp -= currentSp;
		}
	}
	//printf("%d[%d, %d][%d, %d][%d, %d]\n", lv, lv_exp, last_lv_exp, newExp, cur_exp, totalSp, newSp);
	if (newSp) rechangeEX(user, 0, newSp);
}

int __cdecl hookCUserCharacInfo__setCurCharacExp(int* user, int newExp) {
	BYTE lv = CUserCharacInfo_get_charac_level(user);
	DWORD cExp = CUserCharacInfo__get_charac_exp(user);
	checkExpSpByLevel(user, newExp, lv, cExp);
	int ret = CUserCharacInfo__setCurCharacExp(user, newExp);
	return ret;
}


cdeclCall5P CUser___check_level_up = (cdeclCall5P)0x08662AEA;
int __cdecl hookCUser___check_level_up(int* user, int exp, int a3, int a4, int a5) {
	int* mgr = (int*)G_CDataManager();
	BYTE lv = CUserCharacInfo_get_charac_level(user);
	DWORD lv_exp = CDataManager__get_level_exp(mgr, lv + 1), lv_last_exp;
	DWORD cExp = CUserCharacInfo__get_charac_exp(user);
	DWORD newExp = cExp + exp;
	if (lv_exp < newExp) {
		checkExpSpByLevel(user, lv_exp, lv, cExp);
		lv_last_exp = lv_exp;
		lv_exp = CDataManager__get_level_exp(mgr, ++lv + 1);
		while (lv_exp < newExp) {
			checkExpSpByLevel(user, lv_exp, lv, lv_last_exp);
			lv_last_exp = lv_exp;
			lv_exp = CDataManager__get_level_exp(mgr, ++lv + 1);
		}
		checkExpSpByLevel(user, newExp, lv, lv_last_exp);
	}
	else {
		checkExpSpByLevel(user, newExp, lv, cExp);
	}

	return CUser___check_level_up(user, exp, a3, a4, a5);
}

cdeclCall1P CUserCharacInfo_getCurCharacSkillW = (cdeclCall1P)0x0822F140;
cdeclCall3P SkillSlot__set_remain_sfp_at_index = (cdeclCall3P)0x08603590;
cdeclCall3P SkillSlot__set_remain_sp_at_index = (cdeclCall3P)0x086034F8;
//cdeclCall3P WongWork__CSkillChanger___ResetSkillPoint = (cdeclCall3P)0x0860A558;
int __cdecl hookWongWork__CSkillChanger___ResetSkillPoint(int* changer, int* user, int slot) {
	//int ret = WongWork__CSkillChanger___ResetSkillPoint(changer, user, slot);
	BYTE lv = CUserCharacInfo_get_charac_level(user);
	if (lv == 0) return 0;
	DWORD exSp = 0;
	DWORD characNo = CUserCharacInfo_getCurCharacNo(user);
	BYTE exData[48] = { 0 };
	if (48 == getCharacExData(characNo, exData, 48)) {
		exSp = RDWORD(exData, 4 * slot);
	}
	for (int i = 0; i < lv - 1; ++i)
		exSp += spTable[i];
	exSp += getCurExpSp(user);
	//printf("_ResetSkillPoint:%d %d %d\n", lv, slot, exSp);
	int* wSkill = (int*)CUserCharacInfo_getCurCharacSkillW(user);
	SkillSlot__set_remain_sp_at_index(wSkill, exSp, slot);
	return 0;
}

//cdeclCall3P WongWork__CSkillChanger___ResetSFPoint = (cdeclCall3P)0x0860A5D8;
int __cdecl hookWongWork__CSkillChanger___ResetSFPoint(int* changer, int* user, int slot) {
	//int ret = WongWork__CSkillChanger___ResetSFPoint(changer, user, slot);
	BYTE lv = CUserCharacInfo_get_charac_level(user);
	if (lv <= 47) return 0;
	DWORD exTp = 0;
	DWORD characNo = CUserCharacInfo_getCurCharacNo(user);
	BYTE exData[48] = { 0 };
	if (48 == getCharacExData(characNo, exData, 48)) {
		exTp = RDWORD(exData, 8 + 4 * (slot - 2));
	}
	//printf("_ResetSFPoint:%d %d %d\n", lv, slot, exTp);
	int* wSkill = (int*)CUserCharacInfo_getCurCharacSkillW(user);
	SkillSlot__set_remain_sfp_at_index(wSkill, lv - 49 + exTp, slot);
	return 0;
}

cdeclCall1P CUserCharacInfo__getCurCharacAddInfoRefW = (cdeclCall1P)0x086960D8;
cdeclCall1P CUser__adjust_charac_stat = (cdeclCall1P)0x08664766;
int __cdecl hookCUser__adjust_charac_stat(int* user) {
	int ret = CUser__adjust_charac_stat(user);
	DWORD characNo = CUserCharacInfo_getCurCharacNo(user);
	BYTE exData[48] = { 0 };
	if (48 != getCharacExData(characNo, exData, 48))
		return ret;
	int* info = (int*)CUserCharacInfo__getCurCharacAddInfoRefW(user);
	WWORD(info, 0x8, RINT16(info, 0x8) + RDWORD(exData, 16 + 0 * 4) * 50);//1039
	WWORD(info, 0xC, RINT16(info, 0xC) + RDWORD(exData, 16 + 1 * 4) * 50);//1040
	WWORD(info, 0xA, RINT16(info, 0xA) + RDWORD(exData, 16 + 2 * 4) * 50);//1041
	WWORD(info, 0xE, RINT16(info, 0xE) + RDWORD(exData, 16 + 3 * 4) * 50);//1042

	WDWORD(info, 0, RINT32(info, 0) + RDWORD(exData, 16 + 4 * 4) * 250);//1043
	WDWORD(info, 4, RINT32(info, 4) + RDWORD(exData, 16 + 5 * 4) * 250);//1044
	WDWORD(info, 0x42, RINT32(info, 0x42) + RDWORD(exData, 16 + 6 * 4) * 10);//1045

	int addVal = RDWORD(exData, 16 + 7 * 4) * 10;//1046
	for (int i = 0; i < 4; ++i)
		WWORD(info, 2 * (i + 8), RINT16(info, 2 * (i + 8)) + addVal);
	return ret;
}

int __cdecl hookCUser__calc_lev_stat(int* user, int addInfo) {
	int ret = ((cdeclCall2P)(0x08664AE8))(user, addInfo);//CUser::calc_lev_stat
	((cdeclCall2P)(0x08664C50))(user, addInfo);//CUser::calc_quest_stat
	return ret;
}

cdeclCall1P CUser__getCurCharacQuestR = (cdeclCall1P)0x0819A8A6;
int __cdecl hookCDataManager__find_quest(int* user, int id) {
	if (((cdeclBCall2P)(0x086AB920))((int*)CUser__getCurCharacQuestR(user), id))  //UserQuest::isClearQuest
		return ((cdeclCall2P)(0x0835FDC6))((int*)G_CDataManager(), id);
	return 0;
}

int characExReset(int* user, BYTE isForce) {
	//fd_ext reset
	//printf("characExReset %d\n", isForce);
	int characNo = CUserCharacInfo_getCurCharacNo(user);
	if (!isForce) {
		MySQL_set_query(mysql_fd, "seLect flag from charac_ex where charac_no=%d;", characNo);
		int flag = 0;
		if (MySQL_exec(mysql_fd, 1)) {
			if (MySQL_get_n_rows(mysql_fd) == 1) {
				if (MySQL_fetch(mysql_fd)) {
					MySQL_get_int(mysql_fd, 0, (int)&flag);
				}
			}
		}
		//printf("characEx flag:%d\n", flag);
		if (!flag) return 0;
		MySQL_set_query(mysql_fd, (const char*)"upDate charac_ex set flag='%d' where charac_no=%d", 0, characNo);
		MySQL_exec(mysql_fd, 1);
		//printf("need to reset characEx\n");
	}

	int itemIds[3], notUsedItems[3], questRewardItems[3];
	((cdeclCall1P)(0x0808E1AC))(itemIds);//std::vector<int>::vector

	//item 1031
	//fd_ext reset
	int ids[] = { 1031, 1038, 1204, 1205 , };
	for (int i = 0; i < 4; ++i)
		((cdeclCall2P)(0x08111126))(itemIds, (int)&ids[i]);//std::vector<int>::push_back

	for (int i = 1039; i <= 1046; ++i)
		((cdeclCall2P)(0x08111126))(itemIds, (int)&i);//std::vector<int>::push_back

	((cdeclCall1P)(0x081349D6))(notUsedItems);//std::vector<std::pair<int,int>>::vector
	((cdeclCall3P)(0x0866514A))(user, (int)itemIds, (int)notUsedItems); //CUser::count_specific_items
	int size = ((cdeclCall1P)(0x080DD814))(notUsedItems);//std::vector<std::pair<int,int>>::size
	for (int i = 0; i < size; ++i) {
		int* itemPair = (int*)((cdeclCall2P)(0x080EA8A4))(notUsedItems, i);//std::vector<std::pair<int,int>>::operator[]
		if (!itemPair) continue;
		//printf("notUsedItems:[%d, %d]\n", itemPair[0], itemPair[1]);
	}

	((cdeclCall1P)(0x081349D6))(questRewardItems);//std::vector<std::pair<int,int>>::vector
	int* questR = (int*)CUser__getCurCharacQuestR(user);
	((cdeclCallType2)(0x0808BB88))(questR + 1, 0x08664E8E, user, itemIds, questRewardItems); //WongWork::CQuestClear::enumQuestClear
	size = ((cdeclCall1P)(0x080DD814))(questRewardItems);//std::vector<std::pair<int,int>>::size
	for (int i = 0; i < size; ++i) {
		int* itemPair = (int*)((cdeclCall2P)(0x080EA8A4))(questRewardItems, i);//std::vector<std::pair<int,int>>: :operator[]
		if (!itemPair) continue;
		//printf("questRewardItems:[%d, %d]\n", itemPair[0], itemPair[1]);
	}

	((cdeclCall3P)(0x08664DCE))(user, (int)questRewardItems, (int)notUsedItems); //CUser::complete_reward_list
	size = ((cdeclCall1P)(0x080DD814))(questRewardItems);//std::vector<std::pair<int,int>>::size
	BYTE exData[48] = { 0 };
	for (int i = 0; i < size; ++i) {
		int* itemPair = (int*)((cdeclCall2P)(0x080EA8A4))(questRewardItems, i);//std::vector<std::pair<int,int>>: :operator[]
		if (!itemPair) continue;
		switch (itemPair[0]) {
		case 1031://5sp
			WDWORD(exData, 0, RDWORD(exData, 0) + itemPair[1] * 5);//sp0
			WDWORD(exData, 4, RDWORD(exData, 4) + itemPair[1] * 5);//sp1
			break;
		case 1038:
			WDWORD(exData, 0, RDWORD(exData, 0) + itemPair[1] * 20);//sp0
			WDWORD(exData, 4, RDWORD(exData, 4) + itemPair[1] * 20);//sp1
			break;
		case 1204:
			WDWORD(exData, 8, RDWORD(exData, 8) + itemPair[1]);//tp0
			WDWORD(exData, 12, RDWORD(exData, 12) + itemPair[1]);//tp1
			break;
		case 1205:
			WDWORD(exData, 8, RDWORD(exData, 8) + itemPair[1] * 5);//tp0
			WDWORD(exData, 12, RDWORD(exData, 12) + itemPair[1] * 5);//tp1
			break;
		case 1039://4 50	pos	8 WORD
		case 1040://6 50	pos 0xC WORD
		case 1041://5 50	pos 0xA WORD
		case 1042://7 50	pos 0xE WORD
		case 1043://2 250 	pos 0 DWORD
		case 1044://3 250 	pos 4 DWORD
		case 1045://8 10	pos 0x42 DWORD
		case 1046://9 10	WORD for ( i = 0; i <= 3; ++i ) *(_WORD *)(base + 2 * (i + 8)) += (_WORD)GuildExpBook;
			WDWORD(exData, 16 + (itemPair[0] - 1039) * 4, itemPair[1]);//stone
			break;
		}
		//printf("completeItems:[%d, %d]\n", itemPair[0], itemPair[1]);
	}

	saveCharacExData(characNo, exData, 48);

	((cdeclCall1P)(0x081349EA))(questRewardItems);//std::vector<std::pair<int,int>>::~vector
	((cdeclCall1P)(0x081349EA))(notUsedItems);//std::vector<std::pair<int,int>>::~vector
	((cdeclCall1P)(0x08083DDA))(itemIds);//std::vector<int>::~vector

	hookCUser__adjust_charac_stat(user);

	BYTE slot[16];
	slot[8] = 0;
	((cdeclCall3P)(0x081E5BDC))(NULL, (int)user, (int)slot);////Dispatcher_SkillInit::process_skill_init
	slot[8] = 1;
	((cdeclCall3P)(0x081E5BDC))(NULL, (int)user, (int)slot);

	((cdeclCall4P)(0x0867BA5C))(user, 0, 2, 0); //CUser::SendNotiPacket
	((cdeclCall1P)(0x0866C46A))(user); //CUser::send_skill_info(this)
	((cdeclCall4P)(0x0867BA5C))(user, 1, 2, 1); //CUser::SendNotiPacket
}

static inline void  __inline  characExInit(void) {
	//人物属性计算修正
	writeByteCode(0x08664BEB, 18 - 1);
	writeByteCode(0x08664C05, 48 - 1);

	writeCallCode(0x089105D0, (void*)loadSpTable);

	GumInterceptor* v = gum_interceptor_obtain();
	gum_interceptor_replace_fast(v, (gpointer)0x086657FC, (gpointer)hookCUser__increase_status, (gpointer*)&CUser__increase_status);
	gum_interceptor_replace_fast(v, (gpointer)0x0819A87C, (gpointer)hookCUserCharacInfo__setCurCharacExp, (gpointer*)&CUserCharacInfo__setCurCharacExp);
	gum_interceptor_replace_fast(v, (gpointer)0x08662AEA, (gpointer)hookCUser___check_level_up, (gpointer*)&CUser___check_level_up);
	gum_interceptor_replace_fast(v, (gpointer)0x0860A558, (gpointer)hookWongWork__CSkillChanger___ResetSkillPoint, NULL);
	gum_interceptor_replace_fast(v, (gpointer)0x0860A5D8, (gpointer)hookWongWork__CSkillChanger___ResetSFPoint, NULL);
	gum_interceptor_replace_fast(v, (gpointer)0x08664766, (gpointer)hookCUser__adjust_charac_stat, (gpointer*)&CUser__adjust_charac_stat);
	//writeCallCode(0x0866479E, (void*)hookCUser__calc_lev_stat);
	writeNopCode(0x0860A738, 5); writeCallCode(0x0860A744, (void*)hookCDataManager__find_quest);
	writeDWordCode(0x08664EC2, 0x90E4458B); writeByteCode(0x08664EC6, 0x90); writeCallCode(0x08664ECE, (void*)hookCDataManager__find_quest);



}


cdeclCall1P CParty__setStandardDimensionLevel = (cdeclCall1P)0x0859F612;
// int __cdecl DisPatcher_SelectDungeon::process(DisPatcher_SelectDungeon *this, CUser *user, MSG_BASE *msg, ParamBase *param)
cdeclCall1P CUser__GetParty = (cdeclCall1P)0x0865514C;
cdeclCall1P CUser__getDungIndex = (cdeclCall1P)0x082A5A14;
cdeclCall4P DisPatcher_SelectDungeon__process = (cdeclCall4P)0x081C8102;
int __cdecl hookDisPatcher_SelectDungeon__process(int* thisP, int user, int msg, int param) {
	int party = CUser__GetParty((int*)user), dungeonId;
	if (!party) goto exit;
	dungeonId = RWORD(msg, 0xD);
	if (dungeonId >= 62 && dungeonId <= 67) {
		if (dungeonId <= 64) {
			BYTE expandMode = RBYTE(msg, 0x10);
			//printf("ExpandMode:%d, %d\n", dungeonId, expandMode);
			CParty__setStandardDimensionLevel((int*)party);
			int lv = RDWORD(party, 0x0D5C);
			if (expandMode) {
				if (lv < 65) lv = 65;
			}
			else {
				if (lv > 64) lv = 64;
				else if (lv < 60) lv = 60;
			}
			WDWORD(party, 0x0D5C, lv);
			WBYTE(msg, 0x10, 0);
		}
		else {
			WDWORD(party, 0x0D5C, 0);
		}
	}
exit:
	return DisPatcher_SelectDungeon__process(thisP, user, msg, param);
}

//CDungeon::get_dimension_min_partymem

//int __cdecl CParty::setStandardDimensionLevel(CUser **this)

int __cdecl hookCParty__setStandardDimensionLevel(int* thisP) {
	if (RDWORD(thisP, 0x0D5C) == 0) {
		return CParty__setStandardDimensionLevel(thisP);
	}
	//printf("expand lv:%d\n", RDWORD(thisP, 0x0D5C));
	return 1;
}


cdeclCall1P CParty__get_member_count = (cdeclCall1P)0x0859A16A;
cdeclCall4P CParty__CheckEnterDimensionDungeon = (cdeclCall4P)0x0859F1CE;
int __cdecl hookCParty__CheckEnterDimensionDungeon(int* party, int dungeon, int a3, int a4) {
	if (RDWORD(party, 0x0D5C) <= 64) {//异界常规模式
		if (CParty__get_member_count(party) > 1)//两个人就能进
			return CParty__CheckEnterDimensionDungeon(party, dungeon, a3, 0);
	}
	return CParty__CheckEnterDimensionDungeon(party, dungeon, a3, a4);
}

static inline void  __inline  highLevelEvilInit(void) {
	//GumInterceptor* v = gum_interceptor_obtain();
	//gum_interceptor_replace_fast(v, (gpointer)0x081C8102, (gpointer)hookDisPatcher_SelectDungeon__process, (gpointer*)&DisPatcher_SelectDungeon__process);
	//gum_interceptor_replace_fast(v, (gpointer)0x0859F612, (gpointer)hookCParty__setStandardDimensionLevel, (gpointer*)&CParty__setStandardDimensionLevel);
	writeDWordCode(0x08BDBDE0, (int)hookDisPatcher_SelectDungeon__process);
	writeCallCode(0x085AC1F4, (void*)hookCParty__setStandardDimensionLevel);
	writeCallCode(0x085AC021, (void*)hookCParty__CheckEnterDimensionDungeon);
}

cdeclCall3P StreamPool__Acquire = (cdeclCall3P)0x0828FA86;
cdeclCall3P CStreamGuard__Constructor = (cdeclCall3P)0x080C8C26;
cdeclCall2P CStreamGuard__operatorLL = (cdeclCall2P)0x080C8C56;
cdeclCall3P MsgQueueMgr__put = (cdeclCall3P)0x08570FDE;
cdeclCall1P CStreamGuard__Destructor = (cdeclCall1P)0x0861C8D2;
static void towerOfDespairReload(void) {
	int streamMutex = StreamPool__Acquire(*(int**)0x0940BD6C, (int)"App.cpp", 7904);
	int guard[4];
	CStreamGuard__Constructor(guard, streamMutex, 1);
	CStreamGuard__operatorLL(guard, 631);
	CStreamGuard__operatorLL(guard, 0xFFFFFFFF);
	MsgQueueMgr__put(*(int**)0x0940BD68, 2, (int)guard);
	CStreamGuard__Destructor(guard);
}

int __hide getRankerData(int* ranker) {
	ranker[0] = 0;
	ranker[1] = 0;
	ranker[2] = 0;
	MySQL_set_query(mysql_d_guild, "SELECT * FROM `power_war_statue_ranker`");
	if (MySQL_exec(mysql_d_guild, 1) != 1) goto exit;
	if (MySQL_get_n_rows(mysql_d_guild) != 1) goto exit;
	if (MySQL_fetch(mysql_d_guild) != 1) goto exit;
	if (MySQL_get_int(mysql_d_guild, 1, (int)&ranker[0]) != 1) goto exit;
	if (MySQL_get_int(mysql_d_guild, 2, (int)&ranker[1]) != 1) goto exit;
	if (MySQL_get_int(mysql_d_guild, 3, (int)&ranker[2]) != 1) goto exit;
	return 1;
exit:
	MySQL_set_query(mysql_d_guild, "insert into power_war_statue_ranker (server_id,first_ranker,second_ranker,third_ranker) values (0,0,0,0);");
	return MySQL_exec(mysql_d_guild, 1);
}

int __hide saveRankerData(int* ranker) {
	MySQL_set_query(mysql_d_guild, "upDate power_war_statue_ranker set first_ranker=%d, second_ranker=%d, third_ranker=%d where server_id=0;", ranker[0], ranker[1], ranker[2]);
	return MySQL_exec(mysql_d_guild, 1);
}

int __cdecl DB_LoadPowerWarStatueRanker__dispatch(int* thisP, int src, int a3, int* a4) {
	int ranker[3];
	if (getRankerData(ranker) != 1) {
		//printf("empty ranker info\n");
		return 0;
	}
	//printf("load ranker info:[%d, %d, %d]\n", ranker[0], ranker[1], ranker[2]);
	int streamMutex = StreamPool__Acquire(*(int**)0x0940BD6C, (int)"DBThread.cpp", 7904);
	int guard[4];
	CStreamGuard__Constructor(guard, streamMutex, 1);
	CStreamGuard__operatorLL(guard, src);
	CStreamGuard__operatorLL(guard, a3);
	CStreamGuard__operatorLL(guard, ranker[0]);
	CStreamGuard__operatorLL(guard, ranker[1]);
	CStreamGuard__operatorLL(guard, ranker[2]);
	MsgQueueMgr__put(*(int**)0x0940BD68, 1, (int)guard);
	CStreamGuard__Destructor(guard);
}

static inline void  __inline  towerOfDespairInit(void) {
	//BYTE key[16] = StrKey;
	////EncryptData:72 seLect charac_no from charac_stat ORDER BY last_play_time DESC LIMIT 10
	//BYTE selecApcData[72 + 1] = Query13;
	//tea_decrypt((BYTE*)selecApcData, sizeof(selecApcData) - 1, key);
	//writeDWordCode(0x084400F4, (int)strdup((char*)selecApcData));

	writeDWordCode(0x08C5ED48, (int)DB_LoadPowerWarStatueRanker__dispatch);
}

static cdeclCall1P CUser__isGMUser = (cdeclCall1P)0x0814589C;
static cdeclCall1P CUser__get_acc_id = (cdeclCall1P)0x080DA36E;
static cdeclBCall2P WongWork__CGMAccounts__isGM = (cdeclBCall2P)0x08109346;
static cdeclCall1P CPowerManager__ClearMVPInfo = (cdeclCall1P)0x0847F5DE;
static cdeclCall1P CPowerManager__LoadRankerInfo = (cdeclCall1P)0x0847F4FE;
static cdeclCall3P Dispatcher_New_Gmdebug_Command__dispatch_sig = (cdeclCall3P)0x0820BBDE;
int __cdecl hookDispatcher_New_Gmdebug_Command__dispatch_sig(int* thisP, int* user, int* dataBuff) {
	if (1 != WongWork__CGMAccounts__isGM(*(int**)0x0941F710, CUser__get_acc_id(user))) return 0;
	if (!CUser__isGMUser(user)) return 0;
	DWORD oIdx = PacketBuf_get_index(dataBuff);
	int cmdLen = 0;
	char cmd[64] = { 0 };
	PacketBuf_get_int(dataBuff, (int)&cmdLen);
	PacketBuf_get_str(dataBuff, (int)cmd, 64, cmdLen);
	//printf("gmCmd:[%d, %s]\n", cmdLen, cmd);
	char cmdStr[16] = { 0 };
	if (sscanf(cmd, "//%s ", cmdStr) == 1) {
		//printf("parseCmd:[%s]\n", cmdStr);
		if (!strcmp(cmdStr, "fd_tod")) {
			char op[8] = { 0 };
			int v1 = 0, v2 = 0;
			int ret = sscanf(cmd, "//fd_tod %s %d %d", op, &v1, &v2);
			if (ret && !strcmp(op, "reload")) {
				towerOfDespairReload();
				return 0;
			}
		}
		else if (!strcmp(cmdStr, "fd_ranker")) {
			char op[8] = { 0 };
			int v1 = 0, v2 = 0;
			int ret = sscanf(cmd, "//fd_ranker %s %d %d", op, &v1, &v2);
			if (ret == 1 && !strcmp(op, "reload")) {
				CPowerManager__ClearMVPInfo(*(int**)0x0940BE50);
				CPowerManager__LoadRankerInfo(*(int**)0x0940BE50);
				return 0;
			}
			else if (ret == 3 && !strcmp(op, "set") && v1 > 0 && v1 < 4) {
				int ranker[3] = { 0 };
				if (getRankerData(ranker) != 1)return 0;
				ranker[v1 - 1] = v2;
				if (saveRankerData(ranker) != 1)return 0;
				CPowerManager__ClearMVPInfo(*(int**)0x0940BE50);
				CPowerManager__LoadRankerInfo(*(int**)0x0940BE50);
				return 0;
			}
		}
		else if (!strcmp(cmdStr, "fd_ext")) {
			char op[8] = { 0 };
			int ret = sscanf(cmd, "//fd_ext %s", op);
			if (ret == 1 && !strcmp(op, "reset")) {
				characExReset(user, true);
				return 0;
			}
		}
		else if (!strcmp(cmdStr, "fd_q")) {
			char op[8] = { 0 };
			int ret = sscanf(cmd, "//fd_q %s", op);
			if (ret == 1 && !strcmp(op, "reset")) {
				int* questW = (int*)((cdeclCall1P)0x0814AA5E)(user);// CUser::getCurCharacQuestW(this)
				for (int i = 0; i < 29999; ++i)
					((cdeclCall2P)0x086AB93E)(questW, i);//UserQuest::resetClearQuest
				return 0;
			}
		}
		else if (!strcmp(cmdStr, "fd_mail")) {
			int id = 0, count = 0, type = 0;
			int ret = sscanf(cmd, "//fd_mail %d %d %d", &id, &count, &type);
			if (ret >= 2) {
				int characNo = CUserCharacInfo_getCurCharacNo(user);
				if (type == 0) {
					DWORD item_space = ENUM_ITEMSPACE_INVENTORY;
					DWORD slot = CUser_AddItem(user, id, count, 6, (int)&item_space, 0);
					if (slot >= 0) {
						//通知客户端有游戏道具更新
						CUser_SendUpdateItemList(user, 1, item_space, slot);
						return 0;
					}
				}
				sendItemMaillSigle(characNo, "回赠礼物", "Npc回赠礼物时，背包空间不足，使用邮箱发送！", 0, id, count);
				return 0;
			}
		}
	}
	return Dispatcher_New_Gmdebug_Command__dispatch_sig(thisP, (int)user, (int)PacketBuf_set_index(dataBuff, oIdx));
}

static inline void  __inline  gmCmd(void) {
	towerOfDespairInit();
	writeDWordCode(0x08BD7CD4, (int)hookDispatcher_New_Gmdebug_Command__dispatch_sig);
}


//[clear count for revenge ticket] // 마을침공 역습의 공간 보상 주기  
//// 클리어 횟수 
//5 9 1  //(조건1)  
//10 14 2  //(조건2)  
//15 999999 3 //(조건3)
//[/ clear count for revenge ticket]
static cdeclCall1P CUserCharacInfo__VillageAttack_DBUpdate = (cdeclCall1P)0x084EC1DE;
static cdeclCall2P CUserCharacInfo__SetCurRevengeDungeonCount = (cdeclCall2P)0x0822F762;
int __cdecl fixRevengeDungeonEnterCondition(int* user, BYTE attackCount) {
	//printf("attackCount:%d\n", attackCount);
	int condi = 0;
	if (attackCount >= 5 && attackCount <= 9) condi = 1;
	else if (attackCount >= 10 && attackCount <= 14) condi = 2;
	else if (attackCount >= 15 && attackCount <= 999999) condi = 3;
	if (condi) {
		CUserCharacInfo__SetCurRevengeDungeonCount(user, condi);
		CUserCharacInfo__VillageAttack_DBUpdate(user);
	}
}
static cdeclCall3P CInventory__GetInvenData = (cdeclCall3P)0x084FBF2C;
static cdeclCallType3 CInventory__update_item = (cdeclCallType3)0x085000AE;
static cdeclCallType4 CInventory__insertItemIntoInventory = (cdeclCallType4)0x08502D86;
static cdeclCall1P Inven_Item__Inven_Item = (cdeclCall1P)0x080CB854;

int addItem(int* user, int id, int addCount, int maxCount) {
	if (addCount > maxCount) addCount = maxCount;
	int* dmgr = (int*)G_CDataManager();
	int* item = (int*)CDataManager_find_item(dmgr, id);
	if (!item) {
		//printf("find item error: %d\n", id);
		return -1;
	}
	char invenItem[61] = { 0 };
	Inven_Item__Inven_Item((int*)invenItem);
	int* invenR = (int*)CUserCharacInfo_getCurCharacInvenR(user);
	int slot = CInventory__GetInvenData(invenR, id, (int)invenItem);
	if (slot >= 0) {//已有物品update
		DWORD cCount = RDWORD(invenItem, 7);
		if (cCount >= maxCount) {
			//printf("max item error: [%d, %d][%d, %d]\n", slot, id, cCount, maxCount);
			return 0;
		}
		else if (cCount + addCount > maxCount) addCount = maxCount - cCount;
		//cUserHistoryLog::ItemAdd
		((cdeclCall6P)0x08682E84)(user + 0x1E5C0, 1, addCount, (int)invenItem + 7, (int)invenItem, 0xD);
		WDWORD(invenItem, 7, cCount + addCount);
		int* invenW = (int*)CUserCharacInfo_getCurCharacInvenW(user);
		CInventory__update_item(invenW, 1, slot,
			((int*)invenItem)[0],
			((int*)invenItem)[1],
			((int*)invenItem)[2],
			((int*)invenItem)[3],
			((int*)invenItem)[4],
			((int*)invenItem)[5],
			((int*)invenItem)[6],
			((int*)invenItem)[7],
			((int*)invenItem)[8],
			((int*)invenItem)[9],
			((int*)invenItem)[10],
			((int*)invenItem)[11],
			((int*)invenItem)[12],
			((int*)invenItem)[13],
			((int*)invenItem)[14],
			invenItem[60]
		);
		//printf("update item %d[%d, %d]\n", slot, id, addCount);
	}
	else {
		DWORD item_space = ENUM_ITEMSPACE_INVENTORY;
		slot = CUser_AddItem(user, id, addCount, 6, (int)&item_space, 0);
		/*((cdeclCall2P)Vtable(item, 0x08))(item, (int)invenItem);
		WDWORD(invenItem, 7, addCount);
		WDWORD(invenItem, 2, id);
		int* invenW = (int*)CUserCharacInfo_getCurCharacInvenW(user);
		slot = CInventory__insertItemIntoInventory(invenW,
			((int*)invenItem)[0],
			((int*)invenItem)[1],
			((int*)invenItem)[2],
			((int*)invenItem)[3],
			((int*)invenItem)[4],
			((int*)invenItem)[5],
			((int*)invenItem)[6],
			((int*)invenItem)[7],
			((int*)invenItem)[8],
			((int*)invenItem)[9],
			((int*)invenItem)[10],
			((int*)invenItem)[11],
			((int*)invenItem)[12],
			((int*)invenItem)[13],
			((int*)invenItem)[14],
			invenItem[60],
			0x0D, 1, 0
		);*/
		if (slot == -1) {
			printf("add item fail %d[%d, %d]\n", slot, id, addCount);
		}

	}
	if (slot >= 0) CUser_SendUpdateItemList(user, 1, 0, slot);
	return 0;

}

cdeclCall1P CUserCharacInfo__GetProperDungeonClearCount = (cdeclCall1P)0x08335C80;
//void __cdecl CConditionEventManager::ProcessCheckStepUp(CConditionEventManager *this, CUser *a2, __int16 a3)
void __cdecl hookCConditionEventManager__ProcessCheckStepUp(int* mgr, int user, WORD stop) {
	((cdeclCall3P)0x08335566)(mgr, user, stop);
	int clearCount = CUserCharacInfo__GetProperDungeonClearCount((int*)user);
	//printf("clearCount:%d\n", clearCount);
	switch (clearCount) {
	case 2:
	case 4:
	case 6:
	case 8:
	case 10:
		addItem((int*)user, 2749235, 1, 5);
	}
}


int __cdecl hookCUser__Send(int* user, int a2) {
	int ret = ((cdeclCall2P)0x086485BA)(user, a2);
	int usedCera = user[0x23438];//CUser::getUsedCera
	//printf("UsedCera:%d\n", usedCera);
	int count = 0;
	if (usedCera >= 38800) count = 98;
	else if (usedCera >= 33800) count = 68;
	else if (usedCera >= 19800) count = 48;
	else if (usedCera >= 4200) count = 18;
	else if (usedCera >= 3800) count = 13;
	else if (usedCera >= 3300) count = 11;
	else if (usedCera >= 2800) count = 9;
	else if (usedCera >= 2200) count = 7;
	else if (usedCera >= 1800) count = 5;
	else if (usedCera >= 1200) count = 3;
	else if (usedCera >= 900) count = 2;
	else if (usedCera >= 600) count = 1;
	if (count) {
		int item_space;
		int slot = CUser_AddItem(user, 2749236, count, 6, (int)&item_space, 0);
		if (slot >= 0) CUser_SendUpdateItemList(user, 1, 0, slot);
	}
}


static inline void  __inline  eventInit(void) {
	//修复标签[clear reward item] 直接有效
	writeWordCode(0x085AE833, 0x48EB);


	//怪物攻城相关
	//修复
	//[revenge dungeon reward card item]
	//2680669
	//[/ revenge dungeon reward card item]
	int mgr = G_CDataManager();
	WDWORD(mgr, 0x50C, 2680669);

	//修复
	//[clear count for revenge ticket] // 마을침공 역습의 공간 보상 주기  
	//// 클리어 횟수 
	//5 9 1  //(조건1)  
	//10 14 2  //(조건2)  
	//15 999999 3 //(조건3)
	//[/ clear count for revenge ticket]
	writeArrayCode(0x085B9CF8, (BYTE*)"\x8B\x45\xE4\x89\x44\x24\x04\x8B\x45\xDC\x8B\x44\x85\xB0\x89\x04\x24\xE8\x42\x2B\xF3\xFF\xE9\x9F\x00\x00\x00", 28);
	writeCallCode(0x085B9D09, (void*)fixRevengeDungeonEnterCondition);
	writeArrayCode(0x084DF8E4, (BYTE*)"\x8B\x45\xDF\x89\x44\x24\x04\x8B\x45\xD8\x89\x04\x24\xE8\x6C\xFE\xD4\xFF\xE9\xF3\x00\x00\x00", 24);
	writeCallCode(0x084DF8F1, (void*)fixRevengeDungeonEnterCondition);

	//抉择之沼
	// 修复
	//[proper dungeon event param]
	////적정던전클리어횟수	아이템아이디	아이템개수 (2012.05.22 선택의늪이벤트던전 <비밀열쇠>아이템 지급)
	//2	2749235	1
	//4	2749235	1
	//6	2749235	1
	//8	2749235	1
	//10	2749235	1
	//[/ proper dungeon event param]
	//writeCallCode(0x085B0F04, (void*)hookCConditionEventManager__ProcessCheckStepUp);
	//使用点券送 闪耀的希望之邀请函 2749236
	writeCallCode(0x081794B1, (void*)hookCUser__Send);
	writeCallCode(0x0832361E, (void*)hookCUser__Send);

}


//CUser::isAffectedPremium((int)this[6 * i + 0x1E], 12)  1 1 slot
int __hide sendClientMsgBox(int* user, const char* str) {
	BYTE packet_guard[256];
	int* p = (int*)packet_guard;
	PacketGuard_PacketGuard(p);
	InterfacePacketBuf_put_header(p, 0, 233);
	InterfacePacketBuf_put_byte(p, 1);
	InterfacePacketBuf_put_byte(p, 0);
	int len = strlen(str);
	InterfacePacketBuf_put_int(p, len);
	InterfacePacketBuf_put_str(p, (int)str, len);
	InterfacePacketBuf_put_byte(p, 1);
	InterfacePacketBuf_finalize(p, 1);
	CUser_Send(user, (int)p);
	Destroy_PacketGuard_PacketGuard(p);
}

static cdeclCall3P CUser__SendNotiPacketMessage = (cdeclCall3P)0x86886ce;
static cdeclBCall2P CUser__isAffectedPremium = (cdeclBCall2P)0x080E600E;
static cdeclCall3P Dispatcher_UseVendingMachine__dispatch_sig = (cdeclCall3P)0x0821C2E6;
int __cdecl hookDispatcher_UseVendingMachine__dispatch_sig(int* thisP, int* user, int* dataBuff) {
	DWORD oIdx = PacketBuf_get_index(dataBuff);
	int id, info;
	PacketBuf_get_int(dataBuff, (int)&id);
	PacketBuf_get_int(dataBuff, (int)&info);
	//printf("[%d, %d, %d]\n", id, info, CUser__isAffectedPremium(user, 0x0C));
	if (id == 1 && info == 1) {
		if (!CUser__isAffectedPremium(user, 0x0C)) {
			sendClientMsgBox(user, "非黑钻会员无法使用此设备！");
			SendMyErrorPacket(user, 218, 0x10);
			return 0;
		}
	}
	return Dispatcher_UseVendingMachine__dispatch_sig(thisP, (int)user, (int)PacketBuf_set_index(dataBuff, oIdx));
}


void setMaxLv(BYTE lv) {
	writeByteCode(0x08360C3B, lv);
	writeByteCode(0x08360C79, lv);
	writeByteCode(0x08360CC4, lv);
	writeByteCode(0x08662F55, lv);
	writeByteCode(0x086630F3, lv);
	writeByteCode(0x086638F6, lv);
	writeByteCode(0x08665D28, lv - 1);
	writeByteCode(0x08666E9C, lv - 1);
	writeByteCode(0x0866A4A8, lv - 1);
	writeByteCode(0x0866A659, lv);
	writeByteCode(0x0866A929, lv);
	writeByteCode(0x0866A941, lv);
	writeByteCode(0x08689D4B, lv - 1);
	writeByteCode(0x0868FECE, lv);
	writeByteCode(0x0868FEDA, lv);
	writeByteCode(0x085BB6F0, lv);
	writeByteCode(0x085BB7DE, lv);
}




int __cdecl CItem__get_gen_rate(int* thisP) {
	printf("[%d, %d, %d]\n", thisP[1], thisP[0x14], thisP[0xE]);
	return 9000;
}

static cdeclCall3P WongWork__CGenerateRandomNumber__generateNumber = (cdeclCall3P)0x085334A4;
int hookWongWork__CGenerateRandomNumber__generateNumber(int* thisP, int a2, int a3) {
	int ret = WongWork__CGenerateRandomNumber__generateNumber(thisP, 0, 10000);
	printf("rate[%d, %d]\n", ret, a3);
	return ret;
}

void testRate(void) {
	//writeCallCode(0x08534E48, (void*)CItem__get_gen_rate);
	BYTE hookCode[] = { 0x8B, 0x55, 0xC8, 0x89, 0x54, 0x24, 0x08, 0x90 };
	writeArrayCode(0x8536ADB, hookCode, sizeof(hookCode));
	writeCallCode(0x08536AEE, (void*)hookWongWork__CGenerateRandomNumber__generateNumber);
}


int __cdecl CParty__UseAncientDungeonItems(int* party, int* dungeon, int* item, int* a4) {
	if (party[0x336] == 1) return 1;
	if (!RBYTE(dungeon, 0x7FC)) return 1;
	for (int i = 0, needCount; i <= 3; ++i) {
		if (((cdeclCall2P)0x085B4D12)(party, i) != 1) continue;
		int* CurCharacInvenW = (int*)CUserCharacInfo_getCurCharacInvenW((int*)party[6 * i + 0x1E]);
		if (CInventory_delete_item(CurCharacInvenW, 1, a4[i], dungeon[0x1FE], 14, 1) != 1) {
			printf("CInventory_delete_item error\n");
			return 0;
		}
		((cdeclCall1P)0x0864FE52)((int*)party[6 * i + 0x1E]);//CUser::SaveInventory
	}
	return 1;
}

int __cdecl CBattle_Field__check_random_appear_hell_dungeon(int* party, int dungeon, int a3, int a4, int* enHellParty) {
	int ret = ((cdeclCall5P)0x0830A862)(party, dungeon, a3, a4, (int)enHellParty);
	if (!RBYTE(dungeon, 0x89D)) {
		*enHellParty = 0;
	}
	return ret;
}


void __hide on_load(void) {

	MYSQL_init((const char*)"game", (const char*)"uu5!^%jg", "127.0.0.1");
	gum_init_embedded();
	OldderDungeonPermission();
	FixOldSkillSlotInfo();
	EquipmentEmblembSuppust();
	npcFavorInit();
	characExInit();
	highLevelEvilInit();
	//towerOfDespairInit();
	gmCmd();
	writeDWordCode(0x08BD7614, (int)hookDispatcher_UseVendingMachine__dispatch_sig);
	setMaxLv(70);
	eventInit();
	//writeCallCode(0x085ABF3B, (void*)CParty__UseAncientDungeonItems);//修复南部溪谷门票错误
	//testRate();
	//writeCallCode(0x085A0D3F, (void *)CBattle_Field__check_random_appear_hell_dungeon);//修复随机深渊可以开启不支持深渊的地图
}

void __hide on_unload(void) {
	gum_shutdown();
	gum_deinit_embedded();
	MYSQL_deinit();
}

__attribute__((constructor)) static void
fd_on_load(void) {
	//void on_load(void) {
	printf("--------------------------------------------------------------------------fd_on_load begin\n");
	on_load();
	printf("--------------------------------------------------------------------------fd_on_load end\n");

}

__attribute__((destructor)) static void
fd_on_unload(void) {
	//void on_unload(void) {
	printf("--------------------------------------------------------------------------fd_on_unload\n");
	on_unload();
}

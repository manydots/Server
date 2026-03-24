#include <stdio.h>
#include "server.h"
#include "frida-gum.h"

#include "unistd.h"
#include <fcntl.h>
#include "common.h"

//MYSQL操作
//游戏中已打开的数据库索引(游戏数据库非线程安全 谨慎操作)
#define TAIWAN_CAIN = 2;

cdeclCall3P DBMgr_GetDBHandle = (cdeclCall3P)0x83f523e;
cdeclCall1P MySQL_MySQL = (cdeclCall1P)0x83f3ac8;
cdeclCall1P MySQL_init = (cdeclCall1P)0x83f3ce4;
cdeclCall6P MySQL_open = (cdeclCall6P)0x83f4024;
cdeclCall1P MySQL_close = (cdeclCall1P)0x83f3e74;

cdeclCallType MySQL_set_query = (cdeclCallType)0x83f41c0;

cdeclCall2P MySQL_exec = (cdeclCall2P)0x83f4326;
cdeclCall1P MySQL_exec_query = (cdeclCall1P)0x083f5348;
cdeclCall1P MySQL_get_n_rows = (cdeclCall1P)0x80e236c;
cdeclCall1P MySQL_fetch = (cdeclCall1P)0x83f44bc;
cdeclCall3P MySQL_get_int = (cdeclCall3P)0x811692c;
cdeclCall3P MySQL_get_uint = (cdeclCall3P)0x80e22f2;
cdeclCall3P MySQL_get_ulonglong = (cdeclCall3P)0x81754c8;

cdeclCall3P MySQL_get_float = (cdeclCall3P)0x844d6d0;
cdeclCall1P MySQL_get_ushort = (cdeclCall1P)0x8116990;
cdeclCall4P MySQL_get_binary = (cdeclCall4P)0x812531a;
cdeclCall2P MySQL_get_binary_length = (cdeclCall2P)0x81253de;
cdeclCall4P MySQL_get_str = (cdeclCall4P)0x80ecdea;
cdeclCall4P MySQL_blob_to_str = (cdeclCall4P)0x83f452a;
cdeclCall4P compress_zip = (cdeclCall4P)0x86b201f;
cdeclCall4P uncompress_zip = (cdeclCall4P)0x86b2102;



//打开数据库
int __hide *MYSQL_open(const char *db_name, const char* db_ip, int db_port, const char*  db_account, const char*  db_password) {
	//mysql初始化
	int * mysql = (int *)malloc(0x80000);
	if (!mysql) {
		printf("malloc mysql buf error!\n");
		return NULL;
	}
	MySQL_MySQL(mysql);
	MySQL_init(mysql);
	//连接数据库
	//printf("MySQL_open start [%s, %s, %s, %s]!\n", db_name, db_ip, db_account, db_password);
	int ret = MySQL_open(mysql, (int)db_ip, db_port, (int)db_name, (int)db_account, (int)db_password);
	if (ret) {
		//printf("MySQL_open ok!\n");
		return mysql;
	}
	printf("MySQL connect error!\n");
	return NULL;
}

//mysql查询(返回mysql句柄)(注意线程安全)
int __hide MYSQL_exec(int *mysql, const char *sql) {
	MySQL_set_query(mysql, sql);
	return MySQL_exec(mysql, 1);
}

int __hide MYSQL_get_binary(int *mysql, int field_index, BYTE *buf, int maxLen) {
	int binary_length = MySQL_get_binary_length(mysql, field_index);
	if (binary_length > 0) {
		if (binary_length > maxLen)binary_length = maxLen;
		if (1 == MySQL_get_binary(mysql, field_index, (int)buf, binary_length))
			return binary_length;//v.readByteArray(binary_length);
	}
	return 0;
}




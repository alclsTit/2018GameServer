#pragma once
#include "../ShareHeader.h"

#include <windows.h>
#include <stdio.h>

#include <sqlext.h>
#include <locale.h>
#include <tchar.h>
#include <iostream>
#include <cstring>
#include <algorithm>

void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);

//#define TRYODBC(h, ht, x)   {   RETCODE rc = x;\
//                                if (rc != SQL_SUCCESS)	{ HandleDiagnosticRecord (h, ht, rc); } \
//                                if (rc == SQL_ERROR)	{ fwprintf(stderr, L"Error in " L#x L"\n"); }} 
       
#define NAME_LEN 20

class CDB_Test
{
public:
	CDB_Test() { Init(); };
	~CDB_Test() { Release(); };

	void Init();
	bool DB_Login(wchar_t *id, wchar_t *pw, Player_Data& pd);
	bool DB_Update(Player_Data& pd);

private:
	void Release();

	SQLHENV hEnv = NULL;
	SQLHDBC hDbc = NULL;
	SQLHSTMT hStmt = 0;
	SQLRETURN retcode;

	SQLWCHAR szID[NAME_LEN], szPassword[NAME_LEN];
	SQLINTEGER nPosX, nPosY, nDB_Reg_Num, nHP, nCatched;
	SQLLEN  cbID = 20, cbPW = 20, cbPosX = 0, cbPosY = 0;

};

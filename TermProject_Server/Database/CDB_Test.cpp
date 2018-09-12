#include "CDB_Test.h"

void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER  iError;
	WCHAR       wszMessage[1000];
	WCHAR       wszState[SQL_SQLSTATE_SIZE + 1];

	if (RetCode == SQL_INVALID_HANDLE) {
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT *)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5)) {
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}


void CDB_Test::Init()
{
	
	// Allocate environment handle  
	setlocale(LC_ALL, "korean");
	std::wcout.imbue(std::locale("korean"));

	/*
	if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) == SQL_ERROR)
	{
		fwprintf(stderr, L"Init Func : SQLAllocHandle error\n");
		exit(-1);
	}
	
	TRYODBC(hEnv, SQL_HANDLE_ENV, SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0));
	TRYODBC(hEnv, SQL_HANDLE_ENV, SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc));
	TRYODBC(hDbc, SQL_HANDLE_DBC, SQLSetConnectAttr(hDbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)2, 0));
	TRYODBC(hDbc, SQL_HANDLE_DBC, SQLConnect(hDbc, (SQLWCHAR*)L"2018Server_DB", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0));
	TRYODBC(hDbc, SQL_HANDLE_DBC, SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt));
	*/
}

void CDB_Test::Release()
{
	if (hStmt)
	{
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	}

	if (hDbc)
	{
		SQLDisconnect(hDbc);
		SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
	}

	if (hEnv)
	{
		SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
	}
}


bool CDB_Test::DB_Login(wchar_t * pid, wchar_t * ppw, Player_Data& pd)
{
	wchar_t input_id[MAX_LOGIN_ID_PW_SIZE]{ 0 };
	wchar_t input_pw[MAX_LOGIN_ID_PW_SIZE]{ 0 };

	SQLLEN pIndex[7];

	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	{
		retcode = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
			{
				SQLSetConnectAttr(hDbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)2, 0);

				// Connect to data source  
				retcode = SQLConnect(hDbc, (SQLWCHAR*)L"2018Server_DB", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

				// Allocate statement handle  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				{
					wchar_t query[1000] = { 0 };
					//_swprintf(query, L"EXEC DB_QUERY_PRAC01 %s, %d, %d", 2, 145, 120);
					_swprintf(query, L"EXEC DB_QUERY_PRAC02");

					retcode = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

					//retcode = SQLExecDirect(hStmt, (SQLWCHAR *)L"EXEC DB_QUERY_PRAC01 , 160, 150", SQL_NTS); // SQL Query 문을 직접 날렸음 -> 성능저하. 이걸 미리넣어줘야한다
					retcode = SQLExecDirect(hStmt, (SQLWCHAR *)query, SQL_NTS);

					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
					{
						// Bind columns 1, 2, and 3  
						retcode = SQLBindCol(hStmt, 1, SQL_C_WCHAR, (SQLPOINTER)&input_id, MAX_LOGIN_ID_PW_SIZE, &pIndex[0]);								//여기에 넣어줄 데이터에 신경을 많이 써야한다 // 유니 VS 멀티 // 타입
						retcode = SQLBindCol(hStmt, 2, SQL_C_WCHAR, (SQLPOINTER)&input_pw, MAX_LOGIN_ID_PW_SIZE, &pIndex[1]);
						retcode = SQLBindCol(hStmt, 3, SQL_INTEGER, &nPosX, sizeof(int), &pIndex[2]);
						retcode = SQLBindCol(hStmt, 4, SQL_INTEGER, &nPosY, sizeof(int), &pIndex[3]);
						retcode = SQLBindCol(hStmt, 5, SQL_INTEGER, &nDB_Reg_Num, sizeof(int), &pIndex[4]);
						retcode = SQLBindCol(hStmt, 6, SQL_INTEGER, &nHP, sizeof(int), &pIndex[5]);
						retcode = SQLBindCol(hStmt, 7, SQL_INTEGER, &nCatched, sizeof(int), &pIndex[6]);

						// Fetch and print each row of data. On an error, display a message and exit. 
						/*
						for (int i = 0; ; i++)
						{
							retcode = SQLFetch(hStmt);
							if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
								HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
							if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
								wprintf(L"%d: %S and %S --- %d %d \n", i + 1, szID, szPassword, nPosX, nPosY);
							else
								break;
						}
						*/

						for (int i = 0; ; i++)
						{
							retcode = SQLFetch(hStmt);
							if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
							{
								std::wstring lstr(input_id);
								std::wstring rstr(input_pw);

								lstr.erase(std::remove(lstr.begin(), lstr.end(),' '), lstr.end());
								rstr.erase(std::remove(rstr.begin(), rstr.end(), ' '), rstr.end());
								
								if ((wcscmp(lstr.c_str(), pid) == 0) && (wcscmp(rstr.c_str(), ppw) == 0))
								{
									if (pd.pos_x == NULL) pd.pos_x = 4;
									else pd.pos_x = nPosX;
									
									if (pd.pos_y == NULL) pd.pos_y = 4;
									else pd.pos_y = nPosY;

									pd.db_reg_num = nDB_Reg_Num;

									if (pd.hp != nHP)
										pd.hp = nHP;

									if (nHP == 0)
										pd.hp = -1;

									if (pd.catch_cnt != nCatched)
										pd.catch_cnt = nCatched;

									return true;
								}
							}
							else return false;
						}
					}

					// Process data  
					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
					{
						SQLCancel(hStmt);
						SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
					}
					else
					{
						SQLCloseCursor(hStmt);
						HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
					}

					//SQLCloseCursor(hStmt);
					SQLDisconnect(hDbc);
				}

				SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
			}
		}
		SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
	}

	//while (SQLFetch(hStmt) == SQL_SUCCESS)
	//{
	//	if ((wcscmp(input_id, pid) == 0) && (wcscmp(input_pw, ppw) == 0))
	//	{
	//		if (pd.pos_x == NULL) { pd.pos_x = 160; }
	//		if (pd.pos_y == NULL) { pd.pos_y = 160; }
	//
	//		fwprintf(stderr, L"Player Data successfully loaded ");
	//		return true;
	//	}
	//}

	/*
	if (SQLExecDirect(hStmt, (SQLWCHAR*)L"EXEX DB_QUERY_PRAC02", SQL_NTS))
	{
		SQLBindCol(hStmt, 1, SQL_CHAR, szID, NAME_LEN, &cbID);
		SQLBindCol(hStmt, 2, SQL_CHAR, szPassword, NAME_LEN, &cbPW);
		SQLBindCol(hStmt, 3, SQL_INTEGER, &nPosX, sizeof(int), &cbPosX);
		SQLBindCol(hStmt, 4, SQL_INTEGER, &nPosY, sizeof(int), &cbPosY);

		for (int i = 0; ; i++)
		{
			retcode = SQLFetch(hStmt);
			if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
				HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				wprintf(L"%d: %S and %S --- %d %d \n", i + 1, szID, szPassword, nPosX, nPosY);
			else
				break;
		}

		while (SQLFetch(hStmt) == SQL_SUCCESS)
		{
			if ((wcscmp(input_id, pid)== 0) && (wcscmp(input_pw, ppw) == 0))
			{
				if (pd.pos_x == NULL) { pd.pos_x = 160; }
				if (pd.pos_y == NULL) { pd.pos_y = 160; }

				fwprintf(stderr, L"Player Data successfully loaded ");
				return true;
			}
		}
	
	}
	*/

	return false;
}

bool CDB_Test::DB_Update(Player_Data& pd)
{
	
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	{
		retcode = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
			{
				SQLSetConnectAttr(hDbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)2, 0);

				// Connect to data source  
				retcode = SQLConnect(hDbc, (SQLWCHAR*)L"2018Server_DB", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

				// Allocate statement handle  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				{
					wchar_t query[1000] = { 0 };
					_swprintf(query, L"EXEC DB_QUERY_PRAC01 %d, %d, %d, %d, %d", pd.db_reg_num, pd.pos_x, pd.pos_y, pd.hp, pd.catch_cnt);

					retcode = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
					retcode = SQLExecDirect(hStmt, (SQLWCHAR *)query, SQL_NTS);

					if ((retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
					{
						retcode = SQLFetch(hStmt);
						if ((retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
						{
							fwprintf(stderr, L"Player Data successfully saved\n");					
							fwprintf(stderr, L"Player Data successfully loaded ");
						
							return true;
						}
						else return false;

					}
					else
					{
						HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
					}

					// Process data  
					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
					{
						SQLCancel(hStmt);
						SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
					}
					else
					{
						HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
					}

					SQLDisconnect(hDbc);
				}

				SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
			}
		}
		SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
	}
	
	/*
	wchar_t query[1000] = { 0 };
	_swprintf(query, L"EXEC DB_QUERY_PRAC01 %d, %d, %d", pd.db_reg_num, pd.pos_x, pd.pos_y);

	retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query, SQL_NTS);
	if ((retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
	{
	retcode = SQLFetch(hStmt);
	if ((retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
	{
	fwprintf(stderr, L"Player Data successfully saved\n");
	return;
	}
	}
	else
	{
	HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
	}

	*/


}

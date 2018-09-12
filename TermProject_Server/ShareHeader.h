#pragma once

#define MAX_LOGIN_ID_PW_SIZE 20

struct Player_Data
{
	wchar_t ID[MAX_LOGIN_ID_PW_SIZE]{ 0 };
	wchar_t PW[MAX_LOGIN_ID_PW_SIZE]{ 0 };
	int		pos_x;
	int		pos_y;
	int     db_reg_num;
	int     catch_cnt;
	int		hp;
};

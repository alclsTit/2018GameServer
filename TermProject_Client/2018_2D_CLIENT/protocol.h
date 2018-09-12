

#define MAX_BUFF_SIZE   4000
#define MAX_PACKET_SIZE  255

#define BOARD_WIDTH   200
#define BOARD_HEIGHT  200

#define VIEW_RADIUS   7 // ---≥™--- => --- = 3
#define VIEW_RADIUS_MAPOBJECT 1

#define MAX_USER 500
#define NUM_OF_ITEM 1000
#define NUM_OF_MAPOBJECT 3000

#define NPC_START  500
#define NUM_OF_NPC 2500
#define MAX_ANGRYNPC_NUM 2000

#define MY_SERVER_PORT  4000

#define MAX_STR_SIZE  100

#define MAX_LOGIN_ID_PW_SIZE 20

#define CS_UP		1
#define CS_DOWN		2
#define CS_LEFT		3
#define CS_RIGHT    4
#define CS_CHAT		5
#define CS_DB		6
#define CS_MODE     7

#define SC_POS           1
#define SC_PUT_PLAYER    2
#define SC_REMOVE_PLAYER 3
#define SC_CHAT			 4
#define SC_DB			 5
#define SC_MODE         6
#define SC_SET_MAPOBJECT 7
#define SC_SET_ITEM 8
#define SC_SET_INVISIBLE_NPC 9
#define SC_SET_NPC_TYPE 10
#define SC_SET_PLAYER_STATE 11

//∏ ¿∫ 100 X 100
enum MAP_OBJECT { ROCK, CACTUS, BUSH };
enum NPC_TYPE { PEACE, ANGRY };


#pragma pack (push, 1)

using PACKET = unsigned char;
using WORD = unsigned short;

struct cs_packet_up {
	PACKET size;
	PACKET type;
};

struct cs_packet_down {
	PACKET size;
	PACKET type;
};

struct cs_packet_left {
	PACKET size;
	PACKET type;
};

struct cs_packet_right {
	PACKET size;
	PACKET type;
};

struct cs_packet_chat {
	PACKET size;
	PACKET type;
	wchar_t message[MAX_STR_SIZE];
};

struct cs_packet_db
{
	PACKET size;
	PACKET type;
	wchar_t login_id[MAX_LOGIN_ID_PW_SIZE]{ 0 };
	wchar_t login_pw[MAX_LOGIN_ID_PW_SIZE]{ 0 };
};

struct cs_packet_mode {
	BYTE size;
	BYTE type;
	BYTE mode;
};


struct sc_packet_pos {
	PACKET size;
	PACKET type;
	short id;
	char x;
	char y;
};

struct sc_packet_put_player {
	BYTE size;
	BYTE type;
	WORD id;
	BYTE x;
	BYTE y;
};

struct sc_packet_remove_player {
	PACKET size;
	PACKET type;
	short id;
};

struct sc_packet_chat {
	PACKET size;
	PACKET type;
	short id;
	wchar_t message[MAX_STR_SIZE];
};

struct sc_packet_db
{
	PACKET size;
	PACKET type;
	wchar_t login_id[MAX_LOGIN_ID_PW_SIZE]{ 0 };
	wchar_t login_pw[MAX_LOGIN_ID_PW_SIZE]{ 0 };
};

struct sc_packet_mode {
	BYTE size;
	BYTE type;
	BYTE mode;
};

struct sc_packet_mobj
{
	BYTE size;
	BYTE type;
	WORD id;
	BYTE x;
	BYTE y;
	BYTE mobj_type;
};

struct sc_packet_item
{
	BYTE size;
	BYTE type;
	WORD id;
	BYTE x;
	BYTE y;
	BYTE exist;
};

struct sc_packet_do_npc_invisible
{
	BYTE size;
	BYTE type;
	WORD id;
};

struct sc_packet_NPC_type
{
	BYTE size;
	BYTE type;
	WORD id;
	BYTE NPC_Type;
};

struct sc_packet_Player_state
{
	BYTE size;
	BYTE type;
	WORD id;
	WORD catched;
	BYTE hp;
};
#pragma pack (pop)
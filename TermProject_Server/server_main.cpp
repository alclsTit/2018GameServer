#define WIN32_LEAN_AND_MEAN  
#define INITGUID

#include <WinSock2.h>
#include <windows.h>   // include important windows stuff

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "lua53.lib")

#include "protocol.h"
#include "Database/CDB_Test.h"
#include <thread>
#include <vector>
#include <array>
#include <iostream>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <queue>
#include <random>

#define MAX_ROOM_CNT 100
#define MAX_ROOM_RANGE 12
#define NO_CREATE_OBJECT_BOUNDARY 10		// 10 x 10 이후부터 오브젝트들 생성
#define MAX_PLAYER_HP 100

using namespace std;
using namespace chrono;

extern "C"
{
#include "include/lua.h"
#include "include/lauxlib.h"
#include "include/lualib.h"
}

int CAPI_send_chat_packet(lua_State *L);
int CAPI_get_x_position(lua_State *L);
int CAPI_get_y_position(lua_State *L);

HANDLE gh_iocp;
int g_room_up_cnt = 1;
int g_room_chg_cnt = 0;
int g_angryNPC_num = 0;


enum OPTYPE { OP_SEND, OP_RECV, OP_DOAI, OP_PLAYER_MOVE_NOTIFY, OP_PLAYER_MOVE };

//static const int EVT_PLAYER_MOVE = 3;

struct EXOVER {
	WSAOVERLAPPED m_over;
	char m_iobuf[MAX_BUFF_SIZE];
	WSABUF m_wsabuf;
	OPTYPE event_type;
	int target_object;
};

struct EVENT
{
	int obj_id;
	high_resolution_clock::time_point wakeup_t;
	OPTYPE event_type;
	int target_id;
};

class mycomp
{
	bool comp;
public:
	mycomp() {};
	bool operator() (const EVENT lhs, const EVENT rhs)  const
	{
		return (lhs.wakeup_t > rhs.wakeup_t);
	}
};

//wake_up 타임 순으로 sorting 되어있는 애여야함 - priority_queue 를 사용(시간따라 정렬)
priority_queue<EVENT, vector<EVENT>, mycomp> g_timer_queue;
mutex g_timer_lock;

void Add_Event(int obj_id, OPTYPE event_type, high_resolution_clock::time_point wakeup_t)
{
	//g_timer_lock.lock();
	g_timer_queue.push(EVENT{ obj_id, wakeup_t, event_type, 0 });
	//g_timer_lock.unlock();
}

//여기 -----------------------------------------------------------------------------------------------------------------------
void ProcessEvent(EVENT &ev)
{
	//EXOVER는 지역X, GetQueuedCompletion이 실행될 때까지 살아있어야 하므로
	EXOVER *over = new EXOVER();
	over->event_type = ev.event_type;		//worker_thread에게 지금 뭘해야될지 알려주는 것 worker_thread에서 처리하는 event_type

	//1. iocp가 연결된 handle 값
	//2. 데이터 받는것 아니기 때문에 numberbytransferred 는 1로 0아님
	//3. 깨울 오브젝트 id
	//4. Overlapped 구조체정보
	PostQueuedCompletionStatus(gh_iocp, 1, ev.obj_id, &over->m_over);
}

void Timer_Thread()
{
	while (true)
	{	
		this_thread::sleep_for(10ms);
		while (false == g_timer_queue.empty())
		{
			if (g_timer_queue.top().wakeup_t > high_resolution_clock::now())
				break;		//현재 큐에 들어있는 이벤트보다 시간이 더 지난경우 - 빠져나간다

			EVENT et = g_timer_queue.top();
			g_timer_queue.pop();
			ProcessEvent(et);
		}
	}
}

class Client {
public:
	SOCKET m_s;
	bool m_isconnected;
	unordered_set <int> m_viewlist;
	unordered_set <int> m_mobj_viewlist;
	unordered_set <int> m_item_viewlist;
	int m_hp{ 100 };
	mutex m_mvl;
	int m_x; 
	int m_y;
	bool m_is_active;
	wchar_t m_login_id[MAX_LOGIN_ID_PW_SIZE];
	wchar_t m_login_pw[MAX_LOGIN_ID_PW_SIZE];
	int m_db_reg_num;
	lua_State *L;
	bool m_NPC_alive{ true };
	bool m_isNPC;
	int m_damage{ 10 };
	int m_NPC_Type{ -1 };
	int m_catch_cnt{ 0 };

	EXOVER m_rxover;
	int m_packet_size;  // 지금 조립하고 있는 패킷의 크기
	int	m_prev_packet_size; // 지난번 recv에서 완성되지 않아서 저장해 놓은 패킷의 앞부분의 크기
	char m_packet[MAX_PACKET_SIZE];

	Client()
	{
		m_isconnected = false;
		m_x = 4;
		m_y = 4;

		ZeroMemory(&m_rxover.m_over, sizeof(WSAOVERLAPPED));
		m_rxover.m_wsabuf.buf = m_rxover.m_iobuf;
		m_rxover.m_wsabuf.len = sizeof(m_rxover.m_wsabuf.buf);
		m_rxover.event_type = OPTYPE::OP_RECV;
		m_prev_packet_size = 0;

	
	}
};


//어떤 객체 데이터를 한 곳에 몰아두는게 좋다
//array <NPC *, NUM_OF_NPC> g_NPCs;

array <Client, NUM_OF_NPC > g_clients;

//----------------------------------------------------Map

class MapObject
{
public:
	int m_x;
	int m_y;
	int m_type;
	char pass;	// -1 : 통과 불가능 1 : 통과 가능
};

array <MapObject, NUM_OF_MAPOBJECT> g_mapobjs;
bool existed_array[BOARD_HEIGHT][BOARD_WIDTH] = { false };

void InitMapObjects()
{
	default_random_engine generator(time(0));
	for (int i = 0; i < NUM_OF_MAPOBJECT; ++i)
	{
		switch (rand() % 3)
		{
		case MAP_OBJECT::ROCK:
			g_mapobjs[i].m_type = ROCK; 
			g_mapobjs[i].pass = -1;
			break;
		case MAP_OBJECT::CACTUS:
			g_mapobjs[i].m_type = CACTUS;
			g_mapobjs[i].pass = -1;
			break;
		case MAP_OBJECT::BUSH:
			g_mapobjs[i].m_type = BUSH;
			g_mapobjs[i].pass = 1;
			break;
		}

		uniform_int_distribution<int> rnd_posx(NO_CREATE_OBJECT_BOUNDARY, BOARD_WIDTH);
		uniform_int_distribution<int> rnd_posy(NO_CREATE_OBJECT_BOUNDARY, BOARD_HEIGHT);
		//uniform_int_distribution<int> rnd_posx(0, 15);
		//uniform_int_distribution<int> rnd_posy(0, 15);

		g_mapobjs[i].m_x = rnd_posx(generator);
		g_mapobjs[i].m_y = rnd_posy(generator);

		if (existed_array[g_mapobjs[i].m_y][g_mapobjs[i].m_x])
		{
			while (1)
			{
				g_mapobjs[i].m_x = rnd_posx(generator);
				g_mapobjs[i].m_y = rnd_posy(generator);

				if (!existed_array[g_mapobjs[i].m_y][g_mapobjs[i].m_x])
					break;
			}
		}
		existed_array[g_mapobjs[i].m_y][g_mapobjs[i].m_x] = true;	
	}
}

//----------------------------------------------------Map End

//----------------------------------------------------Item
class Item
{
public :
	int m_id;
	int m_x;
	int m_y;
	int hp_recovery{ 10 };
	bool m_existed{ true };
	mutex item_mtx;
};

array <Item, NUM_OF_ITEM> g_items;

void InitItems()
{
	default_random_engine generator(time(0));
	for (int i = 0; i < NUM_OF_ITEM; ++i)
	{
		uniform_int_distribution<int> rnd_posx(NO_CREATE_OBJECT_BOUNDARY, BOARD_WIDTH);
		uniform_int_distribution<int> rnd_posy(NO_CREATE_OBJECT_BOUNDARY, BOARD_HEIGHT);

		//uniform_int_distribution<int> rnd_posx(0,5);
		//uniform_int_distribution<int> rnd_posy(0,5);

		g_items[i].m_x = rnd_posx(generator);
		g_items[i].m_y = rnd_posy(generator);
		g_items[i].m_id = i;

		if (existed_array[g_items[i].m_y][g_items[i].m_x])
		{
			while (1)
			{
				g_items[i].m_x = rnd_posx(generator);
				g_items[i].m_y = rnd_posy(generator);

				if (!existed_array[g_items[i].m_y][g_items[i].m_x])
					break;
			}
		}

		existed_array[g_items[i].m_y][g_items[i].m_x] = true;

	}
}



//----------------------------------------------------Item End
void DisconnectPlayer(int id);

void error_display(const char *msg, int err_no)
{
	WCHAR *lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
		std::cout << msg;
		std::wcout << L"  에러" << lpMsgBuf << std::endl;
		LocalFree(lpMsgBuf);
		while (true);
}

void ErrorDisplay(const char * location)
{
	error_display(location, WSAGetLastError());
}

bool CanSee(int a, int b)
{
	int dist_x = g_clients[a].m_x - g_clients[b].m_x;
	int dist_y = g_clients[a].m_y - g_clients[b].m_y;
	int dist = dist_x * dist_x + dist_y * dist_y;
	return (VIEW_RADIUS * VIEW_RADIUS >= dist);
}

bool ExploreMapObjects(int a, int b)
{
	int dist_x = g_mapobjs[a].m_x - g_clients[b].m_x;
	int dist_y = g_mapobjs[a].m_y - g_clients[b].m_y;
	int dist = dist_x * dist_x + dist_y * dist_y;
	return (VIEW_RADIUS_MAPOBJECT * VIEW_RADIUS_MAPOBJECT >= dist);
}

bool ExploreItems(int a, int b)
{
	int dist_x = g_items[a].m_x - g_clients[b].m_x;
	int dist_y = g_items[a].m_y - g_clients[b].m_y;
	int dist = dist_x * dist_x + dist_y * dist_y;
	return (VIEW_RADIUS_MAPOBJECT * VIEW_RADIUS_MAPOBJECT >= dist);
}

bool ExploreMonster(int a, int b)
{
	int dist_x = g_clients[a].m_x - g_clients[b].m_x;
	int dist_y = g_clients[a].m_y - g_clients[b].m_y;
	int dist = dist_x * dist_x + dist_y * dist_y;
	return (VIEW_RADIUS_MAPOBJECT * VIEW_RADIUS_MAPOBJECT >= dist);
}

bool ExplorePlayer(int a, int b)
{
	int dist_x = g_clients[a].m_x - g_clients[b].m_x;
	int dist_y = g_clients[a].m_y - g_clients[b].m_y;
	int dist = dist_x * dist_x + dist_y * dist_y;
	return (2 >= dist);
}

bool IsNPC(int id)
{
	return ((id >= NPC_START) && (id < NUM_OF_NPC));
		
}

/*
void Add_NPC_MoveEvent(int id)
{
	if (g_clients[id].is_active) return;
	g_clients[id].is_active = true;

	Timer_Event event = { id, high_resolution_clock::now() + 1s, NPC_MOVE };

	g_t_lock.lock();
	g_timer_queue.push(event);
	g_t_lock.unlock();
}
*/

void initialize()
{
	//맵 오브젝트 초기화
	InitMapObjects();
	InitItems();

	for (int i = NPC_START; i < NUM_OF_NPC; ++i)
	{
		g_clients[i].m_x = rand() % BOARD_WIDTH;
		g_clients[i].m_y = rand() % BOARD_HEIGHT;
		g_clients[i].m_is_active = false;

		lua_State *L = luaL_newstate();			//VM 객체생성
		luaL_openlibs(L);
		luaL_loadfile(L, "BlueDragon.lua");
		lua_pcall(L, 0, 0, 0);

		lua_getglobal(L, "set_myid");           //Lua에 정의한 함수 호출
		lua_pushnumber(L, i);					//위 함수에 인자를 넣어줌
		lua_pcall(L, 1, 0, 0);

		lua_register(L, "API_send_chat_packet", CAPI_send_chat_packet);			//LUA에서 호출하는 CAPI를 등록한다.
		lua_register(L, "API_get_x_position", CAPI_get_x_position);
		lua_register(L, "API_get_y_position", CAPI_get_y_position);

		g_clients[i].L = L;
	}

	cout << "LUA VM loading CompleteD!!\n";

	gh_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0); // 의미없는 파라메터, 마지막은 알아서 쓰레드를 만들어준다.
	std::wcout.imbue(std::locale("korean"));

	WSADATA	wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);
}

bool CheckLogin(int id)
{
	CDB_Test db;

	Player_Data pdata;
	wcscpy(pdata.ID, g_clients[id].m_login_id);
	wcscpy(pdata.PW, g_clients[id].m_login_pw);
	pdata.pos_x = g_clients[id].m_x;
	pdata.pos_y = g_clients[id].m_y;
	pdata.catch_cnt = 0;
	pdata.hp = 200;

	if (db.DB_Login(g_clients[id].m_login_id, g_clients[id].m_login_pw, pdata))
	{
		g_clients[id].m_x = pdata.pos_x;
		g_clients[id].m_y = pdata.pos_y;
		g_clients[id].m_db_reg_num = pdata.db_reg_num;
		g_clients[id].m_catch_cnt = pdata.catch_cnt;
		g_clients[id].m_hp = pdata.hp;

		wstring login_id(g_clients[id].m_login_id);
		wstring login_pw(g_clients[id].m_login_pw);

		if (pdata.hp == -1)
		{
			wprintf(L"ID: %s and PW: %s 플레이어는 HP가 %d 여서 접속을 차단했습니다.\n",
				g_clients[id].m_login_id, g_clients[id].m_login_pw, 0);
			
			
			g_clients[id].m_hp = 0;
			DisconnectPlayer(id);
			return false;
		}

		//cout << "ID: " << *login_id.c_str() << " --- " << "PW: " << *login_pw.c_str() << "을 가진 플레이어가 접속했습니다." << endl;
		wprintf(L"ID: %s and PW: %s 을 가진 플레이어가 접속하였습니다. ( X, Y )위치 : ( %d , %d )\n",
			g_clients[id].m_login_id, g_clients[id].m_login_pw,
			g_clients[id].m_x, g_clients[id].m_y);

		return true;
	}
	else
	{
		wprintf(L"ID: %s -------- PW: %s 을 가진 플레이어는 데이터베이스에 존재하지 않습니다.\n", g_clients[id].m_login_id, g_clients[id].m_login_pw);

		closesocket(g_clients[id].m_s);
		g_clients[id].m_isconnected = false;

		wprintf(L"소켓과의 연결이 끊겼습니다. \n");

		return false;
	}

}
void StartRecv(int id)
{
	unsigned long r_flag = 0;
	ZeroMemory(&g_clients[id].m_rxover.m_over, sizeof(WSAOVERLAPPED));
	int ret = WSARecv(g_clients[id].m_s, &g_clients[id].m_rxover.m_wsabuf, 1, 
		NULL, &r_flag, &g_clients[id].m_rxover.m_over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) error_display("Recv Error", err_no);
	}
}

void SendPacket(int id, void *ptr)
{
	unsigned char *packet = reinterpret_cast<unsigned char *>(ptr);
	EXOVER *s_over = new EXOVER;
	s_over->event_type = OP_SEND;
	memcpy(s_over->m_iobuf, packet, packet[0]);		//unsigned char -> char 로 그냥 집어넣었음 = s_over->m_iobuf[0] = unsigned char
	s_over->m_wsabuf.buf = s_over->m_iobuf;
	s_over->m_wsabuf.len = packet[0];
	ZeroMemory(&s_over->m_over, sizeof(WSAOVERLAPPED));
	int res = WSASend(g_clients[id].m_s, &s_over->m_wsabuf, 1, NULL, 0,
		&s_over->m_over, NULL);
	if (0 != res) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) error_display("Send Error! ", err_no);
	}
}

void SendPutObjectPacket(int client, int object)
{
	sc_packet_put_player p;
	p.id = object;
	p.size = sizeof(p);
	p.type = SC_PUT_PLAYER;
	p.x = g_clients[object].m_x;
	p.y = g_clients[object].m_y;


	SendPacket(client, &p);
}

void SendRemoveObjectPacket(int client, int object)
{
	sc_packet_remove_player p;
	p.id = object;
	p.size = sizeof(p);
	p.type = SC_REMOVE_PLAYER;

	
	SendPacket(client, &p);
}

//speaker 가client에 mess를 말했다고 표시를해라
void SendChatPacket(int client, int speaker, WCHAR *mess)
{
	sc_packet_chat p;
	p.id = speaker;
	p.size = sizeof(p);
	p.type = SC_CHAT;
	wcscpy_s(p.message, mess);

	SendPacket(client, &p);
}

int CAPI_send_chat_packet(lua_State *L)
{
	int client = lua_tonumber(L, -3);	//-3은 제일 바닥에 있는거
	int speaker = lua_tonumber(L, -2);
	char *mess = (char*)lua_tostring(L, -1);	//LUA는 유니코드를 지원X -> LUA인터프리터를 개조해야됨. 그럴시간없으니 컨버터 
	lua_pop(L, 4);						//함수객체까지 같이뽑아서 4

	//1. 길이를 알아야한다 - 멀티바이트 스트링을 오ㅏ이드 바이트 스트링으로 변환
	size_t len = strlen(mess);
	if (len > MAX_STR_SIZE - 1)								//루아에서 받은 메시지가 서버에서 설정한 메시지 크기보다 클경우
		len = MAX_STR_SIZE - 1;							//모든 c string의 끝은 0이 들어간다. 즉, 0의 여유분을 남겨둬야함
		
	size_t wlen = 0;
	WCHAR wmess[MAX_STR_SIZE + MAX_STR_SIZE];							//*로 받으면 죽음. 버퍼로 받아야함
	mbstowcs_s(&wlen, wmess, len, mess,_TRUNCATE);		//스트링 길이를 맞춰서 변환해줌
	wmess[MAX_STR_SIZE - 1] = 0;

	SendChatPacket(client, speaker, wmess);

	return 0;
}
int CAPI_get_x_position(lua_State *L)
{
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L, 2);
	int x = g_clients[obj_id].m_x;
	lua_pushnumber(L, x);

	return 1;
}
int CAPI_get_y_position(lua_State *L)
{
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L, 2);
	int y = g_clients[obj_id].m_y;
	lua_pushnumber(L, y);

	return 1;
}

void WakeUpNPC(int npc_id)
{
	//플레이어 둘이 동시에 이동을 하고 npc가 2배로 움직일 수 있다 ---> 낮은 확률이지만 처리해야한다
	// if (CAS(&m_is_active, false, true) == true) add_event();
	if (false == g_clients[npc_id].m_is_active)
	{
		g_clients[npc_id].m_is_active = true;

		//플레이어가 로그인을 하거나 지나다니며 npc를 발견했을 때 npc를 깨운다
		Add_Event(npc_id, OP_DOAI, high_resolution_clock::now() + 1s);
	}

}

void ProcessPacket(int id, char *packet)
{
	int x = g_clients[id].m_x;
	int y = g_clients[id].m_y;

	bool update_mobj_viewlist = false;
	bool remove_items = false;
	vector<int> invisible_NPC;
	switch (packet[1])
	{
	case CS_UP:
	{
		if (y > 0)	y--;

		g_clients[id].m_x = x;
		g_clients[id].m_y = y;

		for (auto mobj : g_clients[id].m_mobj_viewlist)
		{
			if (g_mapobjs[mobj].pass == -1 &&
				g_mapobjs[mobj].m_x == x && g_mapobjs[mobj].m_y == y)
			{
				//통과할 수 없는 장애물로 이동했음
				g_clients[id].m_y++;
				update_mobj_viewlist = true;
				break;
			}
		}

		for (auto item : g_clients[id].m_item_viewlist)
		{
			if (g_items[item].m_x == x && g_items[item].m_y == y)
			{ 
				if (g_clients[id].m_hp < MAX_PLAYER_HP)
					g_clients[id].m_hp += g_items[item].hp_recovery;
				g_items[item].m_existed = false;
				//cout << "체력회복실행" << "\t" << id << " 의 현재체력: " << g_clients[id].m_hp << "아이템 넘버: " << item << "\n";
				remove_items = true;
				break;
			}
		}

		for (auto mob : g_clients[id].m_viewlist)
		{
			if (g_clients[mob].m_x == x && g_clients[mob].m_y == y && g_clients[mob].m_isNPC == true && g_clients[mob].m_NPC_Type == PEACE)
			{
				g_clients[id].m_catch_cnt++;
				g_clients[mob].m_NPC_alive = false;
				invisible_NPC.push_back(mob);
				//cout << "몹" << mob << " 과 충돌 " << "PosX: " << g_clients[mob].m_x << " , " << "PosY: " << g_clients[mob].m_y << "타입: " << static_cast<int>(g_clients[mob].m_NPC_Type) << endl;
				break;
			}
		}


		break;
	}
	case CS_DOWN:
	{
		if (y < BOARD_HEIGHT - 1) y++;

		g_clients[id].m_x = x;
		g_clients[id].m_y = y;

		for (auto mobj : g_clients[id].m_mobj_viewlist)
		{
			if (g_mapobjs[mobj].pass == -1 &&
				g_mapobjs[mobj].m_x == x && g_mapobjs[mobj].m_y == y)
			{
				//통과할 수 없는 장애물로 이동했음
				g_clients[id].m_y--;
				update_mobj_viewlist = true;
				break;
			}
		}

		for (auto item : g_clients[id].m_item_viewlist)
		{
			if (g_items[item].m_x == x && g_items[item].m_y == y)
			{
				if (g_clients[id].m_hp < MAX_PLAYER_HP)
					g_clients[id].m_hp += g_items[item].hp_recovery;
				g_items[item].m_existed = false;
				//cout << "체력회복실행" << "\t" << id << " 의 현재체력: " << g_clients[id].m_hp << "아이템 넘버: " << item << "\n";
				remove_items = true;
				break;
			}
		}

		for (auto mob : g_clients[id].m_viewlist)
		{
			if (g_clients[mob].m_x == x && g_clients[mob].m_y == y && g_clients[mob].m_isNPC == true && g_clients[mob].m_NPC_Type == PEACE)
			{
				g_clients[id].m_catch_cnt++;
				g_clients[mob].m_NPC_alive = false;
				invisible_NPC.push_back(mob);
				//cout << "몹" << mob << " 과 충돌 " << "PosX: " << g_clients[mob].m_x << " , " << "PosY: " << g_clients[mob].m_y << "타입: " << static_cast<int>(g_clients[mob].m_NPC_Type) << endl;
				break;
			}
		}
		

		break;
	}
	case CS_LEFT:
	{
		if (x > 0) x--;

		g_clients[id].m_x = x;
		g_clients[id].m_y = y;

		for (auto mobj : g_clients[id].m_mobj_viewlist)
		{
			if (g_mapobjs[mobj].pass == -1 &&
				g_mapobjs[mobj].m_x == x && g_mapobjs[mobj].m_y == y)
			{
				//통과할 수 없는 장애물로 이동했음
				g_clients[id].m_x++;
				update_mobj_viewlist = true;
				break;
			}
		}

		for (auto item : g_clients[id].m_item_viewlist)
		{
			if (g_items[item].m_x == x && g_items[item].m_y == y)
			{
				if (g_clients[id].m_hp < MAX_PLAYER_HP)
					g_clients[id].m_hp += g_items[item].hp_recovery;
				g_items[item].m_existed = false;
				//cout << "체력회복실행" << "\t" << id << " 의 현재체력: " << g_clients[id].m_hp << "아이템 넘버: " << item << "\n";
				remove_items = true;
				break;
			}
		}

		for (auto mob : g_clients[id].m_viewlist)
		{
			if (g_clients[mob].m_x == x && g_clients[mob].m_y == y && g_clients[mob].m_isNPC == true && g_clients[mob].m_NPC_Type == PEACE)
			{
				g_clients[id].m_catch_cnt++;
				g_clients[mob].m_NPC_alive = false;
				invisible_NPC.push_back(mob);
				//cout << "몹" << mob << " 과 충돌 " << "PosX: " << g_clients[mob].m_x << " , " << "PosY: " << g_clients[mob].m_y << "타입: " << static_cast<int>(g_clients[mob].m_NPC_Type) << endl;
				break;
			}
		}


		break;
	}
	case CS_RIGHT:
	{
		if (x < BOARD_WIDTH - 1) x++;

		g_clients[id].m_x = x;
		g_clients[id].m_y = y;

		for (auto mobj : g_clients[id].m_mobj_viewlist)
		{
			if (g_mapobjs[mobj].pass == -1 &&
				g_mapobjs[mobj].m_x == x && g_mapobjs[mobj].m_y == y)
			{
				//통과할 수 없는 장애물로 이동했음
				g_clients[id].m_x--;
				update_mobj_viewlist = true;
				break;
			}
		}

		for (auto item : g_clients[id].m_item_viewlist)
		{
			if (g_items[item].m_x == x && g_items[item].m_y == y)
			{
				if (g_clients[id].m_hp < MAX_PLAYER_HP)
					g_clients[id].m_hp += g_items[item].hp_recovery;
				g_items[item].m_existed = false;
				//cout << "체력회복실행" << "\t" << id << " 의 현재체력: " << g_clients[id].m_hp << "아이템 넘버: " << item << "\n";
				remove_items = true;
				break;
			}
		}

		for (auto mob : g_clients[id].m_viewlist)
		{
			if (g_clients[mob].m_x == x && g_clients[mob].m_y == y && g_clients[mob].m_isNPC == true && g_clients[mob].m_NPC_Type == PEACE)
			{
				g_clients[id].m_catch_cnt++;
				g_clients[mob].m_NPC_alive = false;
				invisible_NPC.push_back(mob);
				//cout << "몹" << mob << " 과 충돌 " << "PosX: " << g_clients[mob].m_x << " , " << "PosY: " << g_clients[mob].m_y << "타입: " << static_cast<int>(g_clients[mob].m_NPC_Type) << endl;
				
				break;
			}
		}


		break;
	}
	case CS_DB:
	{
		sc_packet_db* db = reinterpret_cast<sc_packet_db*>(packet);
		wcscpy(g_clients[id].m_login_id, db->login_id);
		wcscpy(g_clients[id].m_login_pw, db->login_pw);
		if (CheckLogin(id) == false) return;

		break;
	}
	case CS_MODE:
	{
		auto recieved = reinterpret_cast<sc_packet_mode*>(packet);
		if (recieved->mode == 0) // 핫스팟 
		{
			x = 4; y = 4;
		}
		else if (recieved->mode == 1) // 동접
		{
		retry:
			if (MAX_ROOM_CNT * (g_room_up_cnt - 1) <= id && id < MAX_ROOM_CNT * g_room_up_cnt)
			{
				x = rand() % MAX_ROOM_RANGE + (MAX_ROOM_RANGE * g_room_chg_cnt);
				y = rand() % MAX_ROOM_RANGE + (MAX_ROOM_RANGE * g_room_chg_cnt);

				g_clients[id].m_x = x;
				g_clients[id].m_y = y;


			}
			else
			{
				g_room_up_cnt++;
				g_room_chg_cnt++;
				goto retry;
			}
		}

		break;
	}
	default:
		cout << "Unkown Packet Type from Client [" << id << "]\n";
		return;
	}
	//g_clients[id].m_x = x;
	//g_clients[id].m_y = y;

	sc_packet_pos pos_packet;
	pos_packet.id = id;
	pos_packet.size = sizeof(sc_packet_pos);
	pos_packet.type = SC_POS;
	pos_packet.x = g_clients[id].m_x;
	pos_packet.y = g_clients[id].m_y;
	SendPacket(id, &pos_packet);

	unordered_set<int> new_vl;
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (i == id) continue;
		if (false == g_clients[i].m_isconnected) continue;
		if (true == CanSee(id, i)) 
			new_vl.insert(i);
		
	}

	//------------------------------------------------------------- 캐릭터 이동 시 주변 맵 오브젝트, 아이템 탐색. 업데이트
	unordered_set<int> new_mobj_vl;
	for (int i = 0; i < NUM_OF_MAPOBJECT; ++i)
	{
		if (ExploreMapObjects(i, id) == true)
			new_mobj_vl.insert(i);
	}

	g_clients[id].m_mvl.lock();
	g_clients[id].m_mobj_viewlist.clear();
	g_clients[id].m_mvl.unlock();

	for (auto new_mobj : new_mobj_vl)
		g_clients[id].m_mobj_viewlist.insert(new_mobj);


	unordered_set<int> new_item_vl;
	vector<int> remove_item_vector;
	for (int i = 0; i < NUM_OF_ITEM; ++i)
	{
		if (ExploreItems(i, id) == true)
			new_item_vl.insert(i);
	}

	g_clients[id].m_mvl.lock();
	g_clients[id].m_item_viewlist.clear();
	g_clients[id].m_mvl.unlock();

	for (auto new_item : new_item_vl)
	{
		if (!g_items[new_item].m_existed) continue;
		g_clients[id].m_item_viewlist.insert(new_item);
	}

	if (remove_items)
	{
		for (auto new_item : new_item_vl)
		{
			g_items[new_item].item_mtx.lock();
			if (g_items[new_item].m_existed == false)
			{
				g_items[new_item].item_mtx.unlock();
				remove_item_vector.push_back(new_item);
				continue;
			}
			g_items[new_item].item_mtx.unlock();

			g_clients[id].m_item_viewlist.insert(new_item);
		}

		//------------------------------------------------------------- 아이텥 삭제 플래그 서버 ->  클라이언트
		sc_packet_item item_packet;
		item_packet.size = sizeof(item_packet);
		item_packet.type = SC_SET_ITEM;
		item_packet.exist = false;

		for (int i = 0; i < remove_item_vector.size(); ++i)
		{
			item_packet.id = remove_item_vector[i];
			item_packet.x = g_items[remove_item_vector[i]].m_x;
			item_packet.y = g_items[remove_item_vector[i]].m_y;

			for (int i = 0; i < MAX_USER; ++i)
			{
				if (g_clients[i].m_isconnected == false) continue;

				SendPacket(i, &item_packet);
			}
		}
	}
	
	//------------------------------------------------------------- 캐릭터 이동 시 주변 맵 오브젝트 탐색.아이템 업데이트
	sc_packet_do_npc_invisible npc_invi;
	npc_invi.size = sizeof(npc_invi);
	npc_invi.type = SC_SET_INVISIBLE_NPC;

	for (int i = 0; i < invisible_NPC.size(); ++i)
	{
		npc_invi.id = invisible_NPC[i];

		for (int i = 0; i < MAX_USER; ++i)
		{
			if (g_clients[i].m_isconnected == false) continue;
			SendPacket(i, &npc_invi);
		}
	}

	for (int i = NPC_START; i < NUM_OF_NPC; ++i)
	{
		//플레이어가 움직일 때마다 시야에 있는 npc에게 이벤트를 날림
		if (true == CanSee(id, i) && g_clients[i].m_NPC_alive == true)
		{
			new_vl.insert(i);
			EXOVER *exover = new EXOVER;
			exover->event_type = OP_PLAYER_MOVE;
			exover->target_object = id;			//내가 움직임
			//플레이어가 이동할 때 npc 이벤트 발생
			//key = 어떤 객체에 이벤트를 줄것인가
			PostQueuedCompletionStatus(gh_iocp, 1, i, &exover->m_over);
		}
	}

	//SendPacket(id, &pos_packet);

	
	for (auto ob : new_vl) 
	{
		g_clients[id].m_mvl.lock();

		// 1.new_vl에는 있는데 old_vl에 없는 경우
		// *이전 시야리스트(m_viewlist)에 새로운대상(ob)가 없는 경우
		// 이 때 움직일 npc를 추가시킨다 npc만 해주면 된다
		// 깨어나는 것만 생각 - 유일하게 깨어나는 경우가 이동을 했는데 이전에 없는경우임
		if (0 == g_clients[id].m_viewlist.count(ob))
		{
			g_clients[id].m_viewlist.insert(ob);
			g_clients[id].m_mvl.unlock();
			SendPutObjectPacket(id, ob);

			if (true == IsNPC(ob))
			{
				WakeUpNPC(ob);
				continue;
			}
		

			g_clients[ob].m_mvl.lock();
			if (0 == g_clients[ob].m_viewlist.count(id))
			{
				g_clients[ob].m_viewlist.insert(id);
				g_clients[ob].m_mvl.unlock();
				SendPutObjectPacket(ob, id);
			}
			else
			{
				g_clients[ob].m_mvl.unlock();
				SendPacket(ob, &pos_packet);
			}
		}
		else 
		{
			// 2.new_vl에도 있고 old_vl(m_view_list)에도 있는 경우
			// *이전 시야리스트(m_viewlist)에 새로운대상(ob)가 있는 경우
			g_clients[id].m_mvl.unlock();
			if (true == IsNPC(ob)) continue;

			g_clients[ob].m_mvl.lock();
			if (0 != g_clients[ob].m_viewlist.count(id))
			{
				g_clients[ob].m_mvl.unlock();
				SendPacket(ob, &pos_packet);
			}
			else 
			{
				g_clients[ob].m_viewlist.insert(id);
				g_clients[ob].m_mvl.unlock();
				SendPutObjectPacket(ob, id);
			}
		}

	}

	// new_vl에는 없는데 old_vl에 있는 경우 - 사라진거
	// 사라지는 경우엔 npc 추가 필요 x // 깨어나는 것만 생각 - 유일하게 깨어나는 경우가 이동을 했는데 이전에 없는경우임
	vector <int> to_remove;

	g_clients[id].m_mvl.lock();
	unordered_set<int> vl_copy = g_clients[id].m_viewlist;
	g_clients[id].m_mvl.unlock();

	for (auto ob : vl_copy) 
	{
		if (0 == new_vl.count(ob)) 
		{
			to_remove.push_back(ob);
			if (true == IsNPC(ob)) continue;

			g_clients[ob].m_mvl.lock();
			if (0 != g_clients[ob].m_viewlist.count(id))
			{
				g_clients[ob].m_viewlist.erase(id);
				g_clients[ob].m_mvl.unlock();
				SendRemoveObjectPacket(ob, id);
			}
			else 
			{
				g_clients[ob].m_mvl.unlock();
			}
		}
	}

	g_clients[id].m_mvl.lock();
	for (auto ob : to_remove) g_clients[id].m_viewlist.erase(ob);
	g_clients[id].m_mvl.unlock();
	for (auto ob : to_remove)
	{
		SendRemoveObjectPacket(id, ob);
	}

	
	sc_packet_Player_state pstate;
	pstate.size = sizeof(pstate);
	pstate.type = SC_SET_PLAYER_STATE;
	pstate.hp = g_clients[id].m_hp;
	pstate.catched = g_clients[id].m_catch_cnt;

	SendPacket(id, &pstate);

	//cout << "hp: " << static_cast<int>(pstate.hp) << " , " << "catched: " << pstate.catched << endl;
	

}

void MoveNPC(int i)
{
	//자신의 캐릭터 주변시야에 있는 npc들만 주기적으로 업데이트해준다. 
	//1. 주변 시야처리
	//2. 타이머
	
	bool isInvisibled = false;
	if (g_clients[i].m_NPC_alive == false)
		return;


	//if (g_clients[i].m_awaken == false) continue; //깨어난 애들만 움직이고 그렇지 않은 애들은 잠재움 // 누가 깨우고? 모든 npc를 돌아다니며 검사해야됨
	//npc 랜덤이동 전 나의 시야에 보이는 npc들을 old_vl에 집어넣는다
	unordered_set<int> old_vl;
	for (int id = 0; id < MAX_USER; ++id) //i -> id
	{
		if (false == g_clients[id].m_isconnected) continue;
		if (CanSee(id, i)) old_vl.insert(id);
	}

	//npc 랜덤 이동
	switch (rand() % 4)
	{
	case 0: 	if (g_clients[i].m_x > 0) g_clients[i].m_x--; break;
	case 1:		if (g_clients[i].m_x < BOARD_WIDTH - 1) g_clients[i].m_x++; break;
	case 2:		if (g_clients[i].m_y < BOARD_HEIGHT - 1) g_clients[i].m_y++; break;
	case 3:		if (g_clients[i].m_y > 0) g_clients[i].m_y--; break;
	default:
		break;
	}

	//npc 랜덤이동 후 나의 시야에서 보이는 npc들을 new_vl에 집어넣는다
	unordered_set<int> new_vl;
	for (int id = 0; id < MAX_USER; ++id) //i -> id
	{
		if (false == g_clients[id].m_isconnected) continue;
		if (CanSee(id, i))
			new_vl.insert(id);
	}

	//npc 랜덤이동 후 주변에 탐색하여 캐릭터가 있으면 공격
	//new_vl에 있는건 주변 캐릭터
	vector<int> invisible_to_player;
	if (g_clients[i].m_NPC_Type == ANGRY)
	{
		for (int id = 0; id < new_vl.size(); ++id)
		{
			if (ExplorePlayer(id, i) == true && g_clients[i].m_NPC_alive == true)
			{
				if (g_clients[id].m_hp > 0)
					g_clients[id].m_hp -= g_clients[i].m_damage;
			
				g_clients[i].m_NPC_alive = false;
				isInvisibled = true;
				invisible_to_player.push_back(id);
				cout << "NPC ID: " << i << "가 " << "Player: " << id << "에게 데미지" << g_clients[i].m_damage << "을 입혀 체력이" << g_clients[id].m_hp << "가 되었다." << endl;
				cout << "NPC ID: " << i << "의 위치는" << "PosX: " << g_clients[i].m_x << ", " << "PosY: " << g_clients[i].m_y << endl;
			
				if (g_clients[id].m_hp <= 0)
					DisconnectPlayer(id);
			}
		}
	}


	// 1 2 - 2가 삭제
	for (int id = 0; id < invisible_to_player.size(); ++id)
		new_vl.erase(invisible_to_player[id]);
	
	if (isInvisibled)
	{
		sc_packet_do_npc_invisible npc_invi;
		npc_invi.size = sizeof(npc_invi);
		npc_invi.type = SC_SET_INVISIBLE_NPC;
		npc_invi.id = i;

		for (int id = 0; id < MAX_USER; ++id)
		{
			if (g_clients[id].m_isconnected == false) continue;
			SendPacket(id, &npc_invi);
		}
	}
	else
	{
		//랜덤 이동한 npc의 정보
		sc_packet_pos pos_p;
		pos_p.id = i;
		pos_p.size = sizeof(pos_p);
		pos_p.type = SC_POS;
		pos_p.x = g_clients[i].m_x;
		pos_p.y = g_clients[i].m_y;

		//멀어진 플레이어에서 시야 삭제
		for (auto id : old_vl)
		{
			//여기서 i는 npc 넘버
			//나의 주변에서 랜덤이동한 npc를 집어넣은 리스트에 현재 npc넘버가 없다 = 나의 시야에 해당 npc가 없다
			//현재 시야리스트에서 안보이는 이전 오브젝트들 제거
			if (0 == new_vl.count(id))
			{
				g_clients[id].m_mvl.lock();
				if (g_clients[id].m_viewlist.count(i))	//나한테 npc가 있으면 지운다 없으면 no no 
				{
					g_clients[id].m_viewlist.erase(i);
					g_clients[id].m_mvl.unlock();
					SendRemoveObjectPacket(id, i);
				}
				else
				{
					g_clients[id].m_mvl.unlock();
				}
			}
			else
			{
				// 계속 보고 있다 
				g_clients[id].m_mvl.lock();
				if (g_clients[id].m_viewlist.count(i) != 0)
				{
					g_clients[id].m_mvl.unlock();
					SendPacket(id, &pos_p);
				}
				else
				{
					//상대(npc) view_list에 내가없을 경우
					g_clients[id].m_viewlist.insert(i);
					g_clients[id].m_mvl.unlock();
					SendRemoveObjectPacket(id, i);
				}
			}
		}

		//새로 만난 플레이어에게 시야 추가
		for (auto id : new_vl)
		{
			if (0 == old_vl.count(id))
			{
				g_clients[id].m_mvl.lock();
				if (0 == g_clients[id].m_viewlist.count(i))
				{
					g_clients[id].m_viewlist.insert(i);
					g_clients[id].m_mvl.unlock();
					SendPutObjectPacket(id, i);
				}
				else
				{
					g_clients[id].m_mvl.unlock();
					SendPacket(id, &pos_p);
				}

			}
		}

		if (false == new_vl.empty())	//이동 후 주위에 npc가 있을 경우에만 이벤트 추가
			Add_Event(i, OPTYPE::OP_DOAI, high_resolution_clock::now() + 1s); // 현재시간 + 1초 후에 1s == 1초후에
		else
			g_clients[i].m_is_active = false;

	}
	
	for (int id = 0; id < invisible_to_player.size(); ++id)
	{
		g_clients[invisible_to_player[id]].m_mvl.lock();
		g_clients[invisible_to_player[id]].m_viewlist.erase(i);
		g_clients[invisible_to_player[id]].m_mvl.unlock();
	}

}

void DisconnectPlayer(int id)
{
	sc_packet_remove_player p;
	p.id = id;
	p.size = sizeof(p);
	p.type = SC_REMOVE_PLAYER;

	CDB_Test db;
	Player_Data pdata;

	wcscpy(pdata.ID, g_clients[id].m_login_id);
	wcscpy(pdata.PW, g_clients[id].m_login_pw);
	pdata.pos_x = g_clients[id].m_x;
	pdata.pos_y = g_clients[id].m_y;
	pdata.db_reg_num = g_clients[id].m_db_reg_num;
	pdata.hp = g_clients[id].m_hp;
	pdata.catch_cnt = g_clients[id].m_catch_cnt;

	db.DB_Update(pdata);

	wprintf(L"ID: %s and PW: %s 을 가진 플레이어가 접속을 끊었습니다. ( X, Y )위치 : ( %d , %d ) , HP: (%d) , 잡은먼지수: (%d)\n",
		pdata.ID, pdata.PW, pdata.pos_x, pdata.pos_y, pdata.hp, pdata.catch_cnt);

	for (int i = 0; i < MAX_USER; ++i) {
		if (false == g_clients[i].m_isconnected) continue;
		if (i == id) continue;

		if (true == IsNPC(i)) break;

		g_clients[i].m_mvl.lock();
		if (0 != g_clients[i].m_viewlist.count(id)) {
			g_clients[i].m_viewlist.erase(id);
			g_clients[i].m_mvl.unlock();
			SendPacket(i, &p);
		}
		else {
			g_clients[i].m_mvl.unlock();
		}
	}
	closesocket(g_clients[id].m_s);
	g_clients[id].m_mvl.lock();
	g_clients[id].m_viewlist.clear();
	g_clients[id].m_mvl.unlock();
	g_clients[id].m_isconnected = false;
}

void worker_thread()
{
	while (true)
	{
		unsigned long io_size;
		unsigned long long iocp_key; // 64 비트 integer , 우리가 64비트로 컴파일해서 64비트
		WSAOVERLAPPED *over;
		BOOL ret = GetQueuedCompletionStatus(gh_iocp, &io_size, &iocp_key, &over, INFINITE);
		int key = static_cast<int>(iocp_key);
		//cout << "WT::Network I/O with Client [" << key << "]\n";
		if (FALSE == ret) {
			cout << "Error in GQCS\n";
			DisconnectPlayer(key);
			continue;
		}
		if (0 == io_size) {
			DisconnectPlayer(key);
			continue;
		}

		EXOVER *p_over = reinterpret_cast<EXOVER *>(over);
		if (p_over->event_type == OPTYPE::OP_RECV)
		{
			//cout << "WT:Packet From Client [" << key << "]\n";
			int work_size = io_size;
			char *wptr = p_over->m_iobuf;
			while (0 < work_size)
			{
				int p_size;
				if (0 != g_clients[key].m_packet_size)
					p_size = g_clients[key].m_packet_size;
				else
				{
					p_size = wptr[0];
					g_clients[key].m_packet_size = p_size;
				}
				int need_size = p_size - g_clients[key].m_prev_packet_size;
				if (need_size <= work_size)
				{
					memcpy(g_clients[key].m_packet + g_clients[key].m_prev_packet_size, wptr, need_size);
					ProcessPacket(key, g_clients[key].m_packet);
					g_clients[key].m_prev_packet_size = 0;
					g_clients[key].m_packet_size = 0;
					work_size -= need_size;
					wptr += need_size;
				}
				else
				{
					memcpy(g_clients[key].m_packet + g_clients[key].m_prev_packet_size, wptr, work_size);
					g_clients[key].m_prev_packet_size += work_size;
					work_size = -work_size;
					wptr += work_size;
				}
			}
			StartRecv(key);
		}
		else if (p_over->event_type == OPTYPE::OP_SEND) 
		{
			delete p_over;
		}
		else if (p_over->event_type == OPTYPE::OP_DOAI) 
		{
			MoveNPC(static_cast<int>(iocp_key));
		}
		else if (p_over->event_type == OPTYPE::OP_PLAYER_MOVE)
		{
			//이벤트 처리
			int player_id = p_over->target_object;
													//주위에서 플레이어가 이동했다
			lua_State *L = g_clients[key].L;		// 해당 몬스터 아이디가 가지고있는 가상함수를 쓴다
			lua_getglobal(L, "player_move");		// LUA의 function player_move를 호출
			lua_pushnumber(L, player_id);			//player_id를 EXOVER로 부터가져와야한다
			lua_pushnumber(L, g_clients[player_id].m_x);
			lua_pushnumber(L, g_clients[player_id].m_y);
			lua_pcall(L, 3, 0, 0);					//int error 하고 에러메시지를 여기서 실행해봐도 된다
		
			delete p_over;				//여기서 player_move 객체를 지워줘야함
		
		}
		else 
		{  // Send 후처리
			cout << "Unknown Event Type detected in worker thread!!\n";
		}
	}
}

void accept_thread()	//새로 접속해 오는 클라이언트를 IOCP로 넘기는 역할
{
	SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN bind_addr;
	ZeroMemory(&bind_addr, sizeof(SOCKADDR_IN));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(MY_SERVER_PORT);
	bind_addr.sin_addr.s_addr = INADDR_ANY;	// 0.0.0.0  아무대서나 오는 것을 다 받겠다.

	::bind(s, reinterpret_cast<sockaddr *>(&bind_addr), sizeof(bind_addr));
	listen(s, 1000);

	while (true)
	{
		SOCKADDR_IN c_addr;
		ZeroMemory(&c_addr, sizeof(SOCKADDR_IN));
		c_addr.sin_family = AF_INET;
		c_addr.sin_port = htons(MY_SERVER_PORT);
		c_addr.sin_addr.s_addr = INADDR_ANY;	// 0.0.0.0  아무대서나 오는 것을 다 받겠다.
		int addr_size = sizeof(sockaddr);

		SOCKET cs = WSAAccept(s, reinterpret_cast<sockaddr *>(&c_addr), &addr_size, NULL, NULL);
		if (INVALID_SOCKET == cs) {
			ErrorDisplay("In Accept Thread:WSAAccept()");
			continue;
		}
		//cout << "New Client Connected!\n";
		int id = -1;
		for (int i = 0; i < MAX_USER; ++i) 
			if (false == g_clients[i].m_isconnected) {
				id = i;
				break;
			}
		if (-1 == id) {
			cout << "MAX USER Exceeded\n";
			continue;
		}
		//cout << "ID of new Client is [" << id << "]";
		g_clients[id].m_s = cs;
		g_clients[id].m_packet_size = 0;
		g_clients[id].m_prev_packet_size = 0;
		g_clients[id].m_viewlist.clear();

		//g_clients[id].m_mvl.lock();
		//if (g_clients[id].m_viewlist.size() != 0)
		//	g_clients[id].m_viewlist.empty();
		//g_clients[id].m_mvl.unlock();

		//g_clients[id].m_x = 4;
		//g_clients[id].m_y = 4;
		g_clients[id].m_isNPC = false;

		CreateIoCompletionPort(reinterpret_cast<HANDLE>(cs), gh_iocp, id, 0);
		g_clients[id].m_isconnected = true;
		StartRecv(id);

		sc_packet_put_player p;
		p.id = id;
		p.size = sizeof(p);
		p.type = SC_PUT_PLAYER;
		p.x = g_clients[id].m_x;
		p.y = g_clients[id].m_y;
	
		SendPacket(id, &p);

		sc_packet_mobj mobj;
		for (int i = 0; i < NUM_OF_MAPOBJECT; ++i)
		{
			mobj.id = i;
			mobj.size = sizeof(mobj);
			mobj.type = SC_SET_MAPOBJECT;
			mobj.x = g_mapobjs[i].m_x;
			mobj.y = g_mapobjs[i].m_y;
			mobj.mobj_type = g_mapobjs[i].m_type;

			SendPacket(id, &mobj);
			//cout << "MAP OBJECT ID:" << mobj.id << "PosX: " << static_cast<int>(mobj.x) << "," << "PosY: " << static_cast<int>(mobj.y) << "Type: " << static_cast<int>(mobj.mobj_type) << endl;
		}

		sc_packet_item item_packet;
		for (int i = 0; i < NUM_OF_ITEM; ++i)
		{
			item_packet.size = sizeof(item_packet);
			item_packet.type = SC_SET_ITEM;
			item_packet.x = g_items[i].m_x;
			item_packet.y = g_items[i].m_y;
			item_packet.exist = g_items[i].m_existed;
			item_packet.id = g_items[i].m_id;

			SendPacket(id, &item_packet);
			//cout << "ITEM OBJECT ID:" << item_packet.id << "PosX: " << static_cast<int>(item_packet.x) << "," << "PosY: " << static_cast<int>(item_packet.y) << endl;
		}


		// 나의 접속을 기존 플레이어들에게 알려준다.
		for (int i = 0; i < MAX_USER; ++i)
			if (true == g_clients[i].m_isconnected) {
				if (false == CanSee(i, id)) continue;
				if (i == id) continue;
				g_clients[i].m_mvl.lock();
				g_clients[i].m_viewlist.insert(id);
				g_clients[i].m_mvl.unlock();
				SendPacket(i, &p);
			}

		//주변 장애물 탐색 
		//장애물들은 없어지지 않는다
		for (int i = 0; i < NUM_OF_MAPOBJECT; ++i)
		{
			if (ExploreMapObjects(i, id) == false) continue;
			g_clients[id].m_mvl.lock();
			g_clients[id].m_mobj_viewlist.insert(i);
			g_clients[id].m_mvl.unlock();
		}

		//주변 아이템 검색
		for (int i = 0; i < NUM_OF_ITEM; ++i)
		{
			if (ExploreItems(i, id) == false) continue;
			g_clients[id].m_mvl.lock();
			g_clients[id].m_item_viewlist.insert(i);
			g_clients[id].m_mvl.unlock();
		}

		// 나에게 이미 접속해 있는 플레이어들의 정보를 알려준다.
		for (int i = 0; i < MAX_USER; ++i)
		{
			if (false == g_clients[i].m_isconnected) continue;
			if (i == id) continue;
			if (false == CanSee(i, id)) continue;
			p.id = i;
			p.x = g_clients[i].m_x;
			p.y = g_clients[i].m_y;
			g_clients[id].m_mvl.lock();
			g_clients[id].m_viewlist.insert(i);
			g_clients[id].m_mvl.unlock();
			SendPacket(id, &p);
		}

		sc_packet_NPC_type npc_type;
		npc_type.size = sizeof(npc_type);
		npc_type.type = SC_SET_NPC_TYPE;
		// 주위에 있는 NPC 정보를 알려 준다.
		for (int i = NPC_START; i < NUM_OF_NPC; ++i)
		{
			g_clients[i].m_isNPC = true;

			if (g_angryNPC_num > MAX_ANGRYNPC_NUM)
				g_clients[i].m_NPC_Type = PEACE;
			else
			{
				switch (rand() % 2)
				{
				case NPC_TYPE::PEACE:
					g_clients[i].m_NPC_Type = PEACE;
					break;
				case NPC_TYPE::ANGRY:
					g_clients[i].m_NPC_Type = ANGRY;
					g_angryNPC_num++;
					break;
				}
			}

			npc_type.id = i;
			npc_type.NPC_Type = g_clients[i].m_NPC_Type;
			SendPacket(id, &npc_type);

			if (false == CanSee(i, id)) continue;
			p.id = i;
			p.x = g_clients[i].m_x;
			p.y = g_clients[i].m_y;
			g_clients[id].m_mvl.lock();
			g_clients[id].m_viewlist.insert(i);
			g_clients[id].m_mvl.unlock();

			WakeUpNPC(i);								//내 view_list에 있는 npc들만 깨운다 
			SendPacket(id, &p);
		}


	}
}


int main()
{
	vector<thread> w_threads;

	initialize();

	for (int i = 0; i < 6; ++i)
		w_threads.push_back(thread{ worker_thread }); 

	thread a_thread{ accept_thread };

	thread t_thread{ Timer_Thread };

	for (auto& th : w_threads)
		th.join();

	t_thread.join();
	a_thread.join();

}


int main()
{
	vector<thread> w_threads;
	
	initialize();

	//CreateWorkerThreads();	// 쓰레드 조인까지 이 안에서 해주어야 한다. 전역변수 해서 관리를 해야 함. 전역변수 만드는 것은
							// 좋은 방법이 아님.
	for (int i = 0; i < 6; ++i)
		w_threads.push_back(thread{ worker_thread }); // 4인 이유는 쿼드코어 CPU 라서

	thread a_thread{ accept_thread };

	thread t_thread{ Timer_Thread };

	for (auto& th : w_threads) 
		th.join();

	t_thread.join();
	a_thread.join();

}


// PROG14_1_16b.CPP - DirectInput keyboard demo

// INCLUDES ///////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN  
#define INITGUID

#include <WinSock2.h>
#include <windows.h>   // include important windows stuff
#include <windowsx.h>
#include <stdio.h>
#include <d3d9.h>     // directX includes
#include <array>
#include <iostream>
#include "d3dx9tex.h"     // directX includes
#include "gpdumb1.h"
#include "resource1.h"
//#include "..\..\2018Server\2018Server\protocol.h"
#include "..\2018_2D_CLIENT\protocol.h"
//#pragma comment(linker,"/entry:WinMainCRTStartup /subsystem:console")


#pragma comment (lib, "ws2_32.lib")

// DEFINES ////////////////////////////////////////////////

#define MAX(a,b)	((a)>(b))?(a):(b)
#define	MIN(a,b)	((a)<(b))?(a):(b)

// defines for windows 
#define WINDOW_CLASS_NAME L"WINXCLASS"  // class name

#define WINDOW_WIDTH    680 //1000 //748 //680   // size of window
#define WINDOW_HEIGHT   730 //1025 //803 //730

#define	BUF_SIZE				1024
#define	WM_SOCKET				WM_USER + 1

// PROTOTYPES /////////////////////////////////////////////

// game console
int Game_Init(void *parms=NULL);
int Game_Shutdown(void *parms=NULL);
int Game_Main(void *parms=NULL);

// GLOBALS ////////////////////////////////////////////////

HWND main_window_handle = NULL; // save the window handle
HINSTANCE main_instance = NULL; // save the instance
char buffer[80];                // used to print text

// demo globals
BOB			player;				// 플레이어 Unit
BOB			npc[NUM_OF_NPC];      // NPC Unit
BOB         skelaton[MAX_USER];     // the other player skelaton

BITMAP_IMAGE reactor;      // the background   

BITMAP_IMAGE black_tile;
BITMAP_IMAGE white_tile;
BITMAP_IMAGE tree_tile;


int g_ani_frame{ 0 };

//-------------------------------- item object
BITMAP_IMAGE potion_hp;


//-------------------------------- map object
BITMAP_IMAGE dessert_tile;
BITMAP_IMAGE rock_tile;
BITMAP_IMAGE cactus_tile;
BITMAP_IMAGE bush_tile;
//--------------------------------

#define TILE_WIDTH 49 //65
#define PLAYER_SIZE 49
#define NPC_SIZE 49

#define UNIT_TEXTURE  0
#define UNIT_TEXTURE2 1
#define UNIT_TEXTURE3 2
#define UNIT_TEXTURE4 3

using namespace std;

SOCKET g_mysocket;
WSABUF	send_wsabuf;
char 	send_buffer[BUF_SIZE];
WSABUF	recv_wsabuf;
char	recv_buffer[BUF_SIZE];
char	packet_buffer[BUF_SIZE];
DWORD		in_packet_size = 0;
int		saved_packet_size = 0;
int		g_myid;

int		g_left_x = 0;
int     g_top_y = 0;


wchar_t g_login_id[MAX_LOGIN_ID_PW_SIZE]{ L"First" };
wchar_t g_login_pw[MAX_LOGIN_ID_PW_SIZE]{ L"1234" };

class MapObject
{
public:
	int m_x;
	int m_y;
	int m_type;
	int m_id;
};

class Item
{
public:
	int m_id;
	int m_x;
	int m_y;
	bool m_existed{ true };

};

array <Item, NUM_OF_ITEM> g_items;
array <MapObject, NUM_OF_MAPOBJECT> g_mapobjs;

// FUNCTIONS //////////////////////////////////////////////

void Login_To_Server()
{
	//cs_packet_db *my_packet = reinterpret_cast<cs_packet_db *>(send_buffer);
	//my_packet->size = sizeof(my_packet);
	//send_wsabuf.len = sizeof(my_packet);
	DWORD iobyte;

	cs_packet_db my_packet;
	my_packet.type = CS_DB;
	my_packet.size = sizeof(my_packet);
	wcscpy(my_packet.login_id, g_login_id);
	wcscpy(my_packet.login_pw, g_login_pw);

	PACKET *p = reinterpret_cast<PACKET*>(&my_packet);
	memcpy(send_buffer, p, p[0]);

	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = my_packet.size;

	int ret = WSASend(g_mysocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	if (ret)
	{
		int error_code = WSAGetLastError();
		printf("Error while DB_Sending Packet [%d]", error_code);
	}

	//char temp_buf[MAX_PACKET_SIZE]{ 0 };
	//temp_buf[0] = wcslen(g_login_id) * 2;
	//wcscpy(reinterpret_cast<wchar_t*>(&temp_buf[1]), g_login_id);
	//temp_buf[temp_buf[0] + 3] = wcslen(g_login_pw) * 2;
	//wcscpy(reinterpret_cast<wchar_t*>(&temp_buf[temp_buf[0] + 4]), g_login_pw);

	//send(g_mysocket, reinterpret_cast<char*>(&temp_buf), MAX_PACKET_SIZE, 0);
	//recv(g_mysocket, reinterpret_cast<char*>(&temp_buf), MAX_PACKET_SIZE, 0);

	//if (1 == temp_buf[0]) { return; }
	//else 
	//{
	//	system("cls");
	//	printf("Login Failed\n");
	//}
}

void ProcessPacket(char *ptr)
{
	static bool first_time = true;
	switch(ptr[1])
	{
	case SC_SET_MAPOBJECT:
	{
		sc_packet_mobj *my_packet = reinterpret_cast<sc_packet_mobj*>(ptr);
		int id = my_packet->id;
		g_mapobjs[id].m_id = id;
		g_mapobjs[id].m_x = my_packet->x;
		g_mapobjs[id].m_y = my_packet->y;
		g_mapobjs[id].m_type = my_packet->mobj_type;

		break;
	}

	case SC_SET_ITEM:
	{
		sc_packet_item *my_packet = reinterpret_cast<sc_packet_item*>(ptr);
		int id = my_packet->id;
		g_items[id].m_id = id;
		g_items[id].m_x = my_packet->x;
		g_items[id].m_y = my_packet->y;
		g_items[id].m_existed = my_packet->exist;

		break;
	}

	case SC_SET_INVISIBLE_NPC:
	{
		sc_packet_do_npc_invisible *my_packet = reinterpret_cast<sc_packet_do_npc_invisible*>(ptr);
		int id = my_packet->id;
		npc[id - NPC_START].m_isInvisible = false;
	
		break;
	}

	case SC_SET_NPC_TYPE:
	{
		sc_packet_NPC_type *my_packet = reinterpret_cast<sc_packet_NPC_type*>(ptr);
		int id = my_packet->id;
		npc[id - NPC_START].m_NPC_Type = my_packet->NPC_Type;

		if (my_packet->NPC_Type == PEACE)
		{
			Load_Frame_BOB32(&npc[id - NPC_START], UNIT_TEXTURE2, 0, 2, 0, BITMAP_EXTRACT_MODE_CELL);
			Set_Anim_Speed_BOB32(&npc[id - NPC_START], 8);
		}

		if (my_packet->NPC_Type == ANGRY)
		{
			Load_Frame_BOB32(&npc[id - NPC_START], UNIT_TEXTURE4, 0, 0, 0, BITMAP_EXTRACT_MODE_CELL);
			Set_Anim_Speed_BOB32(&npc[id - NPC_START], 1);
		}

		break;
	}

	case SC_SET_PLAYER_STATE:
	{
		sc_packet_Player_state *my_packet = reinterpret_cast<sc_packet_Player_state*>(ptr);
		int id = my_packet->id;
		player.m_hp = my_packet->hp;
		player.m_catched = my_packet->catched;

		break;
	}

	case SC_PUT_PLAYER: 
		{
		sc_packet_put_player *my_packet = reinterpret_cast<sc_packet_put_player *>(ptr);
		int id = my_packet->id;
		if (first_time) { 
			first_time = false;
			g_myid = id;
			}
		if (id == g_myid) {
			player.x = my_packet->x;
			player.y = my_packet->y;
			player.attr |= BOB_ATTR_VISIBLE;
		} else if (id < NPC_START) {
			skelaton[id].x = my_packet->x;
			skelaton[id].y = my_packet->y;
			skelaton[id].attr |= BOB_ATTR_VISIBLE;
		} else {
			npc[id - NPC_START].x = my_packet->x;
			npc[id - NPC_START].y = my_packet->y;
			npc[id - NPC_START].attr |= BOB_ATTR_VISIBLE;
		}
		break;
		}
	case SC_POS:
		{
		sc_packet_pos *my_packet = reinterpret_cast<sc_packet_pos *>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			g_left_x = my_packet->x - 4;
			g_top_y = my_packet->y - 4;
			player.x = my_packet->x;
			player.y = my_packet->y;
		} else if (other_id < NPC_START) {
			skelaton[other_id].x = my_packet->x;
			skelaton[other_id].y = my_packet->y;
		} else {
			npc[other_id - NPC_START].x = my_packet->x;
			npc[other_id - NPC_START].y = my_packet->y;
		}
		break;
		}

	case SC_REMOVE_PLAYER:
		{
		sc_packet_remove_player *my_packet = reinterpret_cast<sc_packet_remove_player *>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			player.attr &= ~BOB_ATTR_VISIBLE;
		} else if (other_id < NPC_START) {
			skelaton[other_id].attr &= ~BOB_ATTR_VISIBLE;
		} else {
			npc[other_id - NPC_START].attr &= ~BOB_ATTR_VISIBLE;
		}
		break;
		}
	case SC_CHAT:
		{
			sc_packet_chat *my_packet = reinterpret_cast<sc_packet_chat *>(ptr);
			int other_id = my_packet->id;
			if (other_id == g_myid) {
				wcsncpy_s(player.message, my_packet->message, 256);
				player.message_time = GetTickCount();
		} else if (other_id < NPC_START) {
			wcsncpy_s(skelaton[other_id].message, my_packet->message, 256);
			skelaton[other_id].message_time = GetTickCount();
		} else {
			wcsncpy_s(npc[other_id - NPC_START].message, my_packet->message, 256);
			npc[other_id - NPC_START].message_time = GetTickCount();
		}
		break;

		}
	default :
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void ReadPacket(SOCKET sock)
{
	DWORD iobyte, ioflag = 0;

	int ret = WSARecv(sock, &recv_wsabuf, 1, &iobyte, &ioflag, NULL, NULL);
	if (ret) {
		int err_code = WSAGetLastError();
		printf("Recv Error [%d]\n", err_code);
	}

	BYTE *ptr = reinterpret_cast<BYTE *>(recv_buffer);

	while(0 != iobyte) 
	{
		if (0 == in_packet_size)
			in_packet_size = ptr[0];
		if (iobyte + saved_packet_size >= in_packet_size) 
		{
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			iobyte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		} else 
		{
			memcpy(packet_buffer + saved_packet_size, ptr, iobyte);
			saved_packet_size += iobyte;
			iobyte = 0;
		}
	}
}

void clienterror()
{
	exit(-1);
}

// -------------DlgProc-----------------------------------------------------------

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam) {

	switch (iMessage)
	{
	case WM_INITDIALOG: {

		SetDlgItemText(hDlg, IDC_ID, L"First");
		SetDlgItemText(hDlg, IDC_PW, L"1111");

		return TRUE;
	}
	case WM_COMMAND: {

		switch (LOWORD(wParam))
		{
		case IDOK:
			//wchar_t ip[32];

			//GetDlgItemText(hDlg, IDC_IPADDR, ip, 32);
			//WideCharToMultiByte(CP_ACP, 0, ip, -1, g_client.get_server_ip_array(), 32, 0, 0);

			GetDlgItemText(hDlg, IDC_ID, g_login_id, MAX_PACKET_SIZE / 4);
			GetDlgItemText(hDlg, IDC_PW, g_login_pw, MAX_PACKET_SIZE / 4);

			EndDialog(hDlg, IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			PostQuitMessage(0);
			return TRUE;
		default:
			break;
		}

		break;
	}
	default:
		break;
	}
	return FALSE;
}


LRESULT CALLBACK WindowProc(HWND hwnd, 
						    UINT msg, 
                            WPARAM wparam, 
                            LPARAM lparam)
{
// this is the main message handler of the system
PAINTSTRUCT	ps;		   // used in WM_PAINT
HDC			hdc;	   // handle to a device context

// what is the message 
switch(msg)
	{	
	case WM_KEYDOWN:{
   	int x = 0, y = 0;
	if (wparam == VK_RIGHT)	x += 1;
	if (wparam == VK_LEFT)	x -= 1;
	if (wparam == VK_UP)	y -= 1; 
	if (wparam == VK_DOWN)	y += 1;
	cs_packet_up *my_packet = reinterpret_cast<cs_packet_up *>(send_buffer);
	my_packet->size = sizeof(my_packet);
	send_wsabuf.len = sizeof(my_packet);
	DWORD iobyte;
    if (0 != x)
	{
		if (1 == x) my_packet->type = CS_RIGHT;
		else my_packet->type = CS_LEFT;
		int ret = WSASend(g_mysocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
		if (ret) {
			int error_code = WSAGetLastError();
			printf ("Error while sending packet [%d]", error_code);
		}
	}
    if (0 != y) {
		if (1 == y) my_packet->type = CS_DOWN;
		else my_packet->type = CS_UP;
		WSASend(g_mysocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	}


		}
		break;
	case WM_CREATE: 
        {
		// do initialization stuff here
		// login to server 2018-05-27 추가 - db연동 - id와 pw을 입력받기위한 dialog 창 생성
	
		return(0);
		} break;

    case WM_PAINT:
         {
         // start painting
         hdc = BeginPaint(hwnd,&ps);

         // end painting
         EndPaint(hwnd,&ps);
         return(0);
        } break;

	case WM_DESTROY: 
		{
		// kill the application			
		PostQuitMessage(0);
		return(0);
		} break;
	case WM_SOCKET:
		{
		 if (WSAGETSELECTERROR(lparam)) {
			 closesocket((SOCKET) wparam);
			 clienterror();
			 break;
		 }
		 switch(WSAGETSELECTEVENT(lparam)) {
		 case FD_READ:
			 ReadPacket((SOCKET) wparam);
			 break;
		 case FD_CLOSE:
			 closesocket((SOCKET) wparam);
			 clienterror();
			 break;
		 }
		}

	default:break;

    } // end switch

// process any messages that we didn't take care of 
return (DefWindowProc(hwnd, msg, wparam, lparam));

} // end WinProc



// WINMAIN ////////////////////////////////////////////////

int WINAPI WinMain(	HINSTANCE hinstance,
					HINSTANCE hprevinstance,
					LPSTR lpcmdline,
					int ncmdshow)
{
// this is the winmain function

WNDCLASS winclass;	// this will hold the class we create
HWND	 hwnd;		// generic window handle
MSG		 msg;		// generic message


// first fill in the window class stucture
winclass.style			= CS_DBLCLKS | CS_OWNDC | 
                          CS_HREDRAW | CS_VREDRAW;
winclass.lpfnWndProc	= WindowProc;
winclass.cbClsExtra		= 0;
winclass.cbWndExtra		= 0;
winclass.hInstance		= hinstance;
winclass.hIcon			= LoadIcon(NULL, IDI_APPLICATION);
winclass.hCursor		= LoadCursor(NULL, IDC_ARROW);
winclass.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
winclass.lpszMenuName	= NULL; 
winclass.lpszClassName	= WINDOW_CLASS_NAME;

// register the window class
if (!RegisterClass(&winclass))
	return(0);

// create the window, note the use of WS_POPUP
if (!(hwnd = CreateWindow(WINDOW_CLASS_NAME, // class
						  L"Chess Client",	 // title
						  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
					 	  0,0,	   // x,y
						  WINDOW_WIDTH,  // width
                          WINDOW_HEIGHT, // height
						  NULL,	   // handle to parent 
						  NULL,	   // handle to menu
						  hinstance,// instance
						  NULL)))	// creation parms
return(0);

// save the window handle and instance in a global
main_window_handle = hwnd;
main_instance      = hinstance;

// perform all game console specific initialization
Game_Init();

// enter main event loop
while(1)
	{
	if (PeekMessage(&msg,NULL,0,0,PM_REMOVE))
		{ 
		// test if this is a quit
        if (msg.message == WM_QUIT)
           break;
	
		// translate any accelerator keys
		TranslateMessage(&msg);

		// send the message to the window proc
		DispatchMessage(&msg);
		} // end if
    
    // main game processing goes here
    Game_Main();

	} // end while

// shutdown game and release all resources
Game_Shutdown();

// return to Windows like this
return(msg.wParam);

} // end WinMain

///////////////////////////////////////////////////////////

// WINX GAME PROGRAMMING CONSOLE FUNCTIONS ////////////////

int Game_Init(void *parms)
{
// this function is where you do all the initialization 
// for your game

// set up screen dimensions
screen_width = WINDOW_WIDTH;
screen_height = WINDOW_HEIGHT;
screen_bpp   = 32;

// initialize directdraw
DD_Init(screen_width, screen_height, screen_bpp);


// create and load the reactor bitmap image
Create_Bitmap32(&tree_tile, 0, 0, 65, 65);
Create_Bitmap32(&reactor, 0,0, 531, 532);
Create_Bitmap32(&black_tile, 0, 0, 531, 532);
Create_Bitmap32(&white_tile, 0, 0, 531, 532);

Load_Image_Bitmap32(&reactor,L"CHESSMAP.BMP",0,0,BITMAP_EXTRACT_MODE_ABS);

Load_Image_Bitmap32(&dessert_tile, L"Resources//dessert.png", 0, 0, BITMAP_EXTRACT_MODE_ABS);
dessert_tile.x = 5;
dessert_tile.y = 5;
dessert_tile.width = TILE_WIDTH;
dessert_tile.height = TILE_WIDTH;

Load_Image_Bitmap32(&rock_tile, L"Resources//rock.png", 0, 0, BITMAP_EXTRACT_MODE_CELL);
rock_tile.x = 0;
rock_tile.y = 0;
rock_tile.width = TILE_WIDTH;
rock_tile.height = TILE_WIDTH;

Load_Image_Bitmap32(&cactus_tile, L"Resources//cactus.png", 0, 0, BITMAP_EXTRACT_MODE_ABS);
cactus_tile.x = 0;
cactus_tile.y = 0;
cactus_tile.width = TILE_WIDTH;
cactus_tile.height = TILE_WIDTH;

Load_Image_Bitmap32(&bush_tile, L"Resources//bush.png", 0, 0, BITMAP_EXTRACT_MODE_ABS);
bush_tile.x = 0;
bush_tile.y = 0;
bush_tile.width = TILE_WIDTH;
bush_tile.height = TILE_WIDTH;

Load_Image_Bitmap32(&potion_hp, L"Resources//Potion1.PNG", 0, 0, BITMAP_EXTRACT_MODE_ABS);
potion_hp.x = 5;
potion_hp.y = 5;
potion_hp.height = TILE_WIDTH;
potion_hp.width = TILE_WIDTH;

Load_Image_Bitmap32(&tree_tile, L"Tree.BMP", 0, 0, BITMAP_EXTRACT_MODE_ABS);
tree_tile.x = 5;
tree_tile.y = 5;
tree_tile.height = TILE_WIDTH;
tree_tile.width = TILE_WIDTH;

//Load_Image_Bitmap32(&black_tile,L"CHESSMAP.BMP",0,0,BITMAP_EXTRACT_MODE_ABS);
//black_tile.x = 69;
//black_tile.y = 5;
//black_tile.height = TILE_WIDTH;
//black_tile.width = TILE_WIDTH;
//
//Load_Image_Bitmap32(&white_tile,L"CHESSMAP.BMP",0,0,BITMAP_EXTRACT_MODE_ABS);
//white_tile.x = 5;
//white_tile.y = 5;
//white_tile.height = TILE_WIDTH;
//white_tile.width = TILE_WIDTH;

// now let's load in all the frames for the skelaton!!!

	//Load_Texture(L"CHESS2.PNG", UNIT_TEXTURE, 384, 64);
	Load_Texture(L"Resources/sunggae.png", UNIT_TEXTURE, 49, 49);	//300 49
	Load_Texture(L"Resources/frog.png", UNIT_TEXTURE3, 49, 49);
	Load_Texture(L"Resources/dust.png", UNIT_TEXTURE2, 392, 49);
	Load_Texture(L"Resources/dust_boom.png", UNIT_TEXTURE4, 49, 49);

	//if (!Create_BOB32(&player,0,0,64,64,1,BOB_ATTR_SINGLE_FRAME)) return(0);
	//Load_Frame_BOB32(&player,UNIT_TEXTURE,0,2,0,BITMAP_EXTRACT_MODE_CELL);
	if (!Create_BOB32(&player, 0, 0, PLAYER_SIZE, PLAYER_SIZE, 1, BOB_ATTR_SINGLE_FRAME)) return(0);
	Load_Frame_BOB32(&player, UNIT_TEXTURE, 0, 0, 0, BITMAP_EXTRACT_MODE_CELL);

	// set up stating state of skelaton
	Set_Animation_BOB32(&player, 0);
	Set_Anim_Speed_BOB32(&player, 1);
	Set_Vel_BOB32(&player, 0,0);
	Set_Pos_BOB32(&player, 0, 0);


	// create skelaton bob
	for (int i=0; i < MAX_USER; ++i) {
		if (!Create_BOB32(&skelaton[i], 0, 0, PLAYER_SIZE, PLAYER_SIZE, 1, BOB_ATTR_SINGLE_FRAME))
			return(0);
		Load_Frame_BOB32(&skelaton[i], UNIT_TEXTURE3, 0, 0, 0, BITMAP_EXTRACT_MODE_CELL);

		// set up stating state of skelaton
		Set_Animation_BOB32(&skelaton[i], 0);
		Set_Anim_Speed_BOB32(&skelaton[i], 1);
		Set_Vel_BOB32(&skelaton[i], 0,0);
		Set_Pos_BOB32(&skelaton[i], 0, 0);
	}

	// create skelaton bob
	for (int i=0; i < NUM_OF_NPC; ++i)
	{
		if (!Create_BOB32(&npc[i], 0, 0, NPC_SIZE, NPC_SIZE, 8, BOB_ATTR_SINGLE_FRAME))
			return(0);
		Load_Frame_BOB32(&npc[i], UNIT_TEXTURE2, 0, 2, 0, BITMAP_EXTRACT_MODE_CELL);

		// set up stating state of skelaton
		Set_Anim_Speed_BOB32(&npc[i], 8);
		Set_Animation_BOB32(&npc[i], 0);
		Set_Vel_BOB32(&npc[i], 0, 0);
		Set_Pos_BOB32(&npc[i], 0, 0);

		/*
		if (npc[i].m_NPC_Type == PEACE)
		{
			if (!Create_BOB32(&npc[i], 0, 0, NPC_SIZE, NPC_SIZE, 8, BOB_ATTR_SINGLE_FRAME))
				return(0);
			Load_Frame_BOB32(&npc[i], UNIT_TEXTURE2, 0, 2, 0, BITMAP_EXTRACT_MODE_CELL);

			// set up stating state of skelaton
			Set_Anim_Speed_BOB32(&npc[i], 8);
		}

		if (npc[i].m_NPC_Type == ANGRY)
		{
			if (!Create_BOB32(&npc[i], 0, 0, NPC_SIZE, NPC_SIZE, 1, BOB_ATTR_SINGLE_FRAME))
				return(0);
			Load_Frame_BOB32(&npc[i], UNIT_TEXTURE4, 0, 0, 0, BITMAP_EXTRACT_MODE_CELL);

			// set up stating state of skelaton
			Set_Anim_Speed_BOB32(&npc[i], 1);
		}

		// set up stating state of skelaton
		Set_Animation_BOB32(&npc[i], 0);
		Set_Vel_BOB32(&npc[i], 0, 0);
		Set_Pos_BOB32(&npc[i], 0, 0);
		*/
	}


// set clipping rectangle to screen extents so mouse cursor
// doens't mess up at edges
//RECT screen_rect = {0,0,screen_width,screen_height};
//lpddclipper = DD_Attach_Clipper(lpddsback,1,&screen_rect);

// hide the mouse
//ShowCursor(FALSE);

	//DialogBox(main_instance, MAKEINTRESOURCE(IDD_DIALOG1), main_window_handle, AboutDlgProc);

	WSADATA	wsadata;
	WSAStartup(MAKEWORD(2,2), &wsadata);

	g_mysocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);

	SOCKADDR_IN ServerAddr;
	ZeroMemory(&ServerAddr, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family		= AF_INET;
	ServerAddr.sin_port			= htons(MY_SERVER_PORT);
	ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int Result = WSAConnect(g_mysocket, (sockaddr *) &ServerAddr, sizeof(ServerAddr), NULL, NULL, NULL, NULL);

	//login to server 2018-05-27 추가 - db연동
	//Login_To_Server();

	WSAAsyncSelect(g_mysocket, main_window_handle, WM_SOCKET, FD_CLOSE | FD_READ);

	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = BUF_SIZE;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = BUF_SIZE;

	DialogBox(main_instance, MAKEINTRESOURCE(IDD_DIALOG1), main_window_handle, AboutDlgProc);
	Login_To_Server();
// return success
return(1);

} // end Game_Init

///////////////////////////////////////////////////////////

int Game_Shutdown(void *parms)
{
// this function is where you shutdown your game and
// release all resources that you allocated

// kill the reactor
Destroy_Bitmap32(&black_tile);
Destroy_Bitmap32(&white_tile);
Destroy_Bitmap32(&reactor);

	// kill skelaton
	for (int i = 0; i < MAX_USER; ++i) Destroy_BOB32(&skelaton[i]);
	for (int i = 0; i < NUM_OF_NPC; ++i) 
		Destroy_BOB32(&npc[i]);

// shutdonw directdraw
DD_Shutdown();

WSACleanup();

// return success
return(1);
} // end Game_Shutdown

///////////////////////////////////////////////////////////

int Game_Main(void *parms)
{
// this is the workhorse of your game it will be called
// continuously in real-time this is like main() in C
// all the calls for you game go here!
// check of user is trying to exit
if (KEY_DOWN(VK_ESCAPE) || KEY_DOWN(VK_SPACE))
    PostMessage(main_window_handle, WM_DESTROY,0,0);

// start the timing clock
Start_Clock();

// clear the drawing surface
DD_Fill_Surface(D3DCOLOR_ARGB(255,0,0,0));

// get player input

	g_pd3dDevice->BeginScene();
	g_pSprite->Begin(D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE );

	// draw the background reactor image
	// 화면에 보이는 타일의 갯수를 증가시키는 곳 현재 11x11이 하나의 창에 보이도록 설정함 
	/*
	for (int i=0; i<10; ++i)
		for (int j=0; j<10; ++j)
		{
			int tile_x = i + g_left_x;		//g_left_x = 내위치 -4
			int tile_y = j + g_top_y;
			if ((tile_x <0) || (tile_y<0)) continue;

			//Draw_Bitmap32(&dessert_tile, TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);



			int tile_x = i + g_left_x;		//g_left_x = 내위치 -4
			int tile_y = j + g_top_y;
			if ((tile_x <0) || (tile_y<0)) continue;
			if (((tile_x >> 2) % 2) == ((tile_y >> 2) % 2))
			{
				//Draw_Bitmap32(&white_tile, TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				Draw_Bitmap32(&dessert_tile, TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
			
			
				//if (tile_x % 8 == 0 && tile_y % 8 == 0)
				//	Draw_Bitmap32(&tree_tile, i * TILE_WIDTH + 7, j * TILE_WIDTH + 7);
			}
			else
			{
				//Draw_Bitmap32(&black_tile, TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				Draw_Bitmap32(&dessert_tile, TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
			}
				
		}
    	//Draw_Bitmap32(&reactor);
	*/


	for (int i = 0; i < 13; ++i)
	{
		for (int j = 0; j < 13; ++j)
		{
			int tile_x = i + g_left_x;		//g_left_x = 내위치 -4
			int tile_y = j + g_top_y;
			if ((tile_x <0) || (tile_y<0)) continue;
	
			Draw_Bitmap32(&dessert_tile, TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
		}
	}
	g_pSprite->End();
	g_pd3dDevice->EndScene();

	//g_pSprite->End();

	g_pd3dDevice->BeginScene();
	g_pSprite->Begin(D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE);

	for (int i = 0; i < 13; ++i)
	{
		for (int j = 0; j < 13; ++j)
		{
			int tile_x = i + g_left_x;		//g_left_x = 내위치 -4
			int tile_y = j + g_top_y;
			if ((tile_x <0) || (tile_y<0)) continue;

			for (int k = 0; k < g_mapobjs.size(); ++k)
			{
				if ((tile_x == g_mapobjs[k].m_x) && (tile_y == g_mapobjs[k].m_y))
				{
					if (g_mapobjs[k].m_type == ROCK)
					{
						Draw_Bitmap32(&rock_tile, (g_mapobjs[k].m_x - g_left_x) *TILE_WIDTH + 7, (g_mapobjs[k].m_y - g_top_y) * TILE_WIDTH + 7);
					}
					else if (g_mapobjs[k].m_type == CACTUS)
					{
						Draw_Bitmap32(&cactus_tile, (g_mapobjs[k].m_x - g_left_x) * TILE_WIDTH + 7, (g_mapobjs[k].m_y - g_top_y) * TILE_WIDTH + 7);
					}
					else if (g_mapobjs[k].m_type == BUSH)
					{
						Draw_Bitmap32(&bush_tile, (g_mapobjs[k].m_x - g_left_x) * TILE_WIDTH + 7, (g_mapobjs[k].m_y - g_top_y)  * TILE_WIDTH + 7);
					}
				}
			}
			

			for (int w = 0; w < g_items.size(); ++w)
			{
				if (!g_items[w].m_existed)
					continue;

				if ((tile_x == g_items[w].m_x) && (tile_y == g_items[w].m_y))
				{
					Draw_Bitmap32(&potion_hp, (g_items[w].m_x - g_left_x) * TILE_WIDTH + 7, (g_items[w].m_y - g_top_y) * TILE_WIDTH + 7);
				}
			}
		}
	}

	g_pSprite->End();
	g_pd3dDevice->EndScene();

	//g_pd3dDevice->EndScene();
	
	g_pd3dDevice->BeginScene();
	g_pSprite->Begin(D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE );
	// draw the skelaton
	Draw_BOB32(&player);
	for (int i=0;i<MAX_USER;++i) Draw_BOB32(&skelaton[i]);
	for (int i = NPC_START; i < NUM_OF_NPC; ++i)
	{
		if (npc[i].m_isInvisible)
			Draw_BOB32(&npc[i]);
	}

	// draw some text
	wchar_t text[300], text_hp[50], text_catch_cnt[100];
	wsprintf(text, L"MY POSITION (%3d, %3d)", player.x, player.y);
	wsprintf(text_hp, L"HP (%3d)", player.m_hp);
	wsprintf(text_catch_cnt, L"Catched Dust (%3d)", player.m_catched);

	Draw_Text_D3D(text,10,screen_height-64,D3DCOLOR_ARGB(255,255,255,255));
	Draw_Text_D3D(text_hp, 250, screen_height - 64, D3DCOLOR_ARGB(255, 255, 255, 255));
	Draw_Text_D3D(text_catch_cnt, 355, screen_height - 64, D3DCOLOR_ARGB(255, 255, 255, 255));
	
	g_pSprite->End();
	g_pd3dDevice->EndScene();

// flip the surfaces
DD_Flip();

// sync to 3o fps
//Wait_Clock(30);


// return success
return(1);

} // end Game_Main

//////////////////////////////////////////////////////////
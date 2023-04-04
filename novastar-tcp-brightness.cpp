/**

Author   Yuriy Zemlyakov
Email    LodestaRgr@yandex.ru
Source   https://github.com/LodestaRgr/novastar-tcp-brightness
Version  1.0

**/

#include <iostream>

// для работы сокета
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <inaddr.h>
#include <stdio.h>
#include <vector>

// для работы с ini файлом
#include "SimpleIni.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using std::string;

//Выясняем имя программы без .exe
string getFileNameWithoutExtension(const string& s) {
	char sep = '/';
#ifdef _WIN32
	sep = '\\';
#endif
	size_t i = s.rfind(sep, s.length());
	if (i != string::npos)
	{
		string filename = s.substr(i + 1, s.length() - i);
		size_t lastindex = filename.find_last_of(".");
		string rawname = filename.substr(0, lastindex);
		return(rawname);
	}
	return("");
}

int main(int argc, char** argv)
{
	int brightness;
	//если не задан аргумент % яркости экрана, завершить программу
	if (argc < 2) {
		cout << "BRIGHTNESS CONTROL on LED Processing Novastar over TCP" << endl;
		cout << "Models: MCTRL4K, VX4S, VX6S, NovaPro HD, NVP UHD Jr, VX1000, VX600, VX16S, J6" << endl;
		cout << endl;
		cout << "Usage: " << argv[0] << " <brightness>" << endl;
		return 0;
	}
	else {

		try {
			brightness = stoi(argv[1]);
		}
		catch (exception& err) {
			brightness = -1;
		}

		if (brightness < 0 or brightness > 100) {
			cout << "brightness must be in 0..100" << endl;
			return 0;
		}
	}

	//Стандартные параметры для ini файла
	const char SERVER_IP[] = "192.168.0.10";		// Enter IPv4 address of Server
	const char SERVER_PORT[] = "5200";				// Enter Listening port on Server side

	//Выясняем имя файла проекта без типа и добавляем ".ini"
	string file_name = getFileNameWithoutExtension(argv[0]) + ".ini";
	const char* ini_file = file_name.c_str();

	// --- Работа с INI файлом -------------------------------------------------------------
		CSimpleIniA ini;
		ini.SetUnicode();

		// -- читаем ini файл
		SI_Error rc = ini.LoadFile(ini_file);
		if (rc < 0) {
			// если файл не существует или не читается 
			// создаем новый ini файл со стандартными параметрами
			cout << "Not found configuration file '" << ini_file << "' !!!" << endl;

			//вносим стандартные значения
			ini.SetValue("config", "ip", SERVER_IP);
			ini.SetValue("config", "port", SERVER_PORT);

			// сохраняем параметры в файл
			ini.SaveFile(ini_file);

			cout << "Create default configuration file." << endl;
		};

		// - загружаем параметры из ini файла в программу
		const char* cfg_ip;
		const char* cfg_port;
		cfg_ip = ini.GetValue("config", "ip", SERVER_IP);
		cfg_port = ini.GetValue("config", "port", SERVER_PORT);

	// --- Работа с TCP клиентом -----------------------------------------------------------
		cout << "Connecting to: " << cfg_ip << ":" << cfg_port << endl;

		//Key constants
		const short BUFF_SIZE = 1024;					// Maximum size of buffer for exchange info between server and client

		// Key variables for all program
		int erStat;										// For checking errors in sockets functions

		//IP in string format to numeric format for socket functions. Data is in "ip_to_num"
		in_addr ip_to_num;
		inet_pton(AF_INET, cfg_ip, &ip_to_num);
	
		// WinSock initialization
		WSADATA wsData;
		erStat = WSAStartup(MAKEWORD(2, 2), &wsData);

		if (erStat != 0) {
			cout << "Error WinSock version initializaion #";
			cout << WSAGetLastError();
			return 1;
		}
		//else
		//	cout << "WinSock initialization is OK" << endl;

		// Socket initialization
		SOCKET ClientSock = socket(AF_INET, SOCK_STREAM, 0);

		if (ClientSock == INVALID_SOCKET) {
			cout << "Error initialization socket # " << WSAGetLastError() << endl;
			closesocket(ClientSock);
			WSACleanup();
		}
		//else
		//	cout << "Client socket initialization is OK" << endl;

		// Establishing a connection to Server
		sockaddr_in servInfo;

		ZeroMemory(&servInfo, sizeof(servInfo));

		servInfo.sin_family = AF_INET;

		servInfo.sin_addr = ip_to_num;
		servInfo.sin_port = htons((unsigned short)strtoul(cfg_port, NULL, 0));
	
		erStat = connect(ClientSock, (sockaddr*)&servInfo, sizeof(servInfo));

		if (erStat != 0) {
			cout << "Connection to LED Processing NO FOUND. Error # " << WSAGetLastError() << endl;
			closesocket(ClientSock);
			WSACleanup();
			return 1;
		}
		//else
		//	cout << "Connection established SUCCESSFULLY. Ready to send a message to Server" << endl;


		//Exchange text data between Server and Client. Disconnection if a Client send "xxx"

		vector <char> servBuff(BUFF_SIZE), clientBuff(BUFF_SIZE);							// Buffers for sending and receiving data
		short packet_size = 0;												// The size of sending / receiving packet in bytes


	// Работа с видеопроцессором (MCTRL4K, VX4S, VX6S, NovaPro HD, NovaPro HD Jr, VX1000, VX600, VX16S, J6 -----------------------------------

			char connect[]	= {0x55, 0xAA, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x57, 0x56};
			char brightXX[]	= {0x55, 0xAA, 0x00, 0x00, 0xFE, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x5A};

			//hex1 = Преобразование % (процента) в hex (0..255)
			int brightness_hex1 = round(255 * (float)brightness / 100);

			//hex2 = Второе значение (85 + hex1) и если hex1 >= 256 тогда (85 + hex1 - 256)
			int brightness_hex2 = (85 + brightness_hex1) < 256 ? (85 + brightness_hex1) : (85 + brightness_hex1 - 256);		

			// подмена 18 и 19 байта в шаблоне команды яркости
			brightXX[18] = brightness_hex1;
			brightXX[19] = brightness_hex2;

			//cout << brightness_hex1 << " : " << brightness_hex2 << endl;

			// Отправка комманды подключения к видеопроцессору
			packet_size = send(ClientSock, connect, sizeof(connect), 0);

				// если ошибка подключения к видеопроцессору - отменяем отправку
				if (packet_size == SOCKET_ERROR) {
					cout << "Can't connect to LED Processing. Error # " << WSAGetLastError() << endl;
					closesocket(ClientSock);
					WSACleanup();
					return 1;
				}

				// получение ответа
				packet_size = recv(ClientSock, servBuff.data(), servBuff.size(), 0);
				
				// если ошибка получения ответа от видеопроцессора - отменяем отправку
				if (packet_size == SOCKET_ERROR) {
					cout << "Can't receive answer from LED Processing. Error # " << WSAGetLastError() << endl;
					closesocket(ClientSock);
					WSACleanup();
					return 1;
				}
				else {
					//cout << "Server message: " << servBuff.data() << endl;

					// если ответ получен
					// отправка команды изменения яркости 
					packet_size = send(ClientSock, brightXX, sizeof(brightXX), 0);

					// если ошибка подключения к видеопроцессору - отменяем отправку
					if (packet_size == SOCKET_ERROR) {
						cout << "Can't set brightness to LED Processing. Error # " << WSAGetLastError() << endl;
						closesocket(ClientSock);
						WSACleanup();
						return 1;
					}

					// получение ответа
					packet_size = recv(ClientSock, servBuff.data(), servBuff.size(), 0);

					// если ошибка получения ответа от видеопроцессора - отменяем отправку
					if (packet_size == SOCKET_ERROR) {
						cout << "Can't change brightness from LED Processing. Error # " << WSAGetLastError() << endl;
						closesocket(ClientSock);
						WSACleanup();
						return 1;
					}
					else {
						// если всё успешно вывести сообщение о смене яркости
						cout << "Set brightness: " << brightness << endl;
					}
					
				}

		closesocket(ClientSock);
		WSACleanup();

		return 0;
}
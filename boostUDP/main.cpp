#include "udpServer.h"
#include <thread>
#include <atomic>

std::atomic<bool> quit{ false };

void process_console_commands() {
	std::string command;
	while (!quit) {
		std::getline(std::cin, command);
		if (command == "/quit") {
			quit = true;
		}
	}
}

int main() {
	unsigned short port = 50000;

	try {
		boost::asio::io_context io_context;
		UdpServer server(io_context, port);

		// 콘솔 명령을 처리하는 쓰레드 생성
		std::thread console_thread(process_console_commands);

		while (!quit) {
			io_context.run_one();
		}

		// 종료 전 쓰레드 조인
		console_thread.join();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}

	return 0;
}
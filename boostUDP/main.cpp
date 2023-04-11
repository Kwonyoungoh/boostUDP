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

		// �ܼ� ����� ó���ϴ� ������ ����
		std::thread console_thread(process_console_commands);

		while (!quit) {
			io_context.run_one();
		}

		// ���� �� ������ ����
		console_thread.join();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}

	return 0;
}
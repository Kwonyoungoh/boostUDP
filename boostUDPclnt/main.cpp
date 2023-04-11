#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

using boost::asio::ip::udp;

enum class conn_flags : uint8_t {
	CONNECT_FLAG = 0x01,
	DISCONNECT_FLAG = 0x02,
	CONNECTION_ACK_FLAG = 0x03,
	DISCONNECTION_ACK_FLAG = 0x04,
	DATA_FLAG = 0x05
};

std::atomic<bool> connected{ false };
std::atomic<bool> quit{ false };

void send_connect_request(udp::socket& socket, const udp::endpoint& server_endpoint);
void send_disconnect_request(udp::socket& socket, const udp::endpoint& server_endpoint);
void process_client_actions(udp::socket& socket, const udp::endpoint& server_endpoint);
void process_server_response(udp::socket& socket, const udp::endpoint& server_endpoint);

void send_connect_request(udp::socket& socket, const udp::endpoint& server_endpoint) {
	std::array<uint8_t, 2> request = { static_cast<uint8_t>(conn_flags::CONNECT_FLAG), 0x00 };

	while (connected == false) {
		socket.send_to(boost::asio::buffer(request, 2), server_endpoint);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	std::cout<< "Connected" << std::endl;
}

void send_disconnect_request(udp::socket& socket, const udp::endpoint& server_endpoint) {
	std::array<uint8_t, 2> request = { static_cast<uint8_t>(conn_flags::DISCONNECT_FLAG), 0x00 };

	while (connected == true) {
		socket.send_to(boost::asio::buffer(request, 2), server_endpoint);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	std::cout << "Disconnected" << std::endl;
}

void process_client_actions(udp::socket& socket, const udp::endpoint& server_endpoint) {
	// Connect request
	send_connect_request(socket, server_endpoint);

	// Send dummy data every 0.1 seconds
	std::array<uint8_t, 1024> dummy_data;
	dummy_data.fill(0);
	dummy_data[0] = static_cast<uint8_t>(conn_flags::DATA_FLAG);
	while (quit == false) {
		socket.send_to(boost::asio::buffer(dummy_data), server_endpoint);
		//std::cout << "Send data" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	std::cout << "Quit process_client_actions()" << std::endl;
	//// Disconnect request
	//send_disconnect_request(socket, server_endpoint);
}

void process_server_response(udp::socket& socket, const udp::endpoint& server_endpoint) {
	std::array<uint8_t, 1024> recv_buffer;
	udp::endpoint sender_endpoint;

	socket.non_blocking(true); // 소켓을 non-blocking 모드로 설정

	while (quit == false) {
		boost::system::error_code ec;
		std::size_t bytes_received = socket.receive_from(boost::asio::buffer(recv_buffer), sender_endpoint, 0, ec);
		
		if (ec == boost::asio::error::would_block) {
			// non-blocking 모드에서 데이터를 수신할 수 없을 때 발생하는 에러를 처리합니다.
			std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 짧은 지연을 추가하여 CPU를 덜 사용합니다.
			continue;
		}
		
		if (!ec && bytes_received > 0) {
			conn_flags flag = static_cast<conn_flags>(recv_buffer[0]);

			if (flag == conn_flags::CONNECTION_ACK_FLAG) {
				
				connected = true;
			}

			if (flag == conn_flags::DATA_FLAG) {
				std::string message(recv_buffer.begin() + 1, recv_buffer.begin() + bytes_received);
				std::cout << "Received from " << sender_endpoint << std::endl;
			}

			if (flag == conn_flags::DISCONNECTION_ACK_FLAG) {
				
				connected = false;
				quit = true;
			}
		}
		else {
			std::cerr << "Error receiving data: " << ec.message() << std::endl;
		}
	}

	std::cout << "Quit process_server_response()" << std::endl;
}

int main() {
	std::string server_ip = "222.107.137.167";
	unsigned short port = 50000;

	boost::asio::io_context io_context;
	udp::socket socket(io_context);
	socket.open(udp::v4());

	udp::resolver resolver(io_context);
	udp::resolver::query query(udp::v4(), server_ip, std::to_string(port));
	udp::endpoint server_endpoint = *resolver.resolve(query);

	std::thread client_actions_thread(process_client_actions, std::ref(socket), server_endpoint);
	std::thread receive_thread(process_server_response, std::ref(socket), server_endpoint);

	std::string input;
	while (std::getline(std::cin, input)) {
		if (input == "/quit") {
			break;
		}
	}

	send_disconnect_request(socket, server_endpoint);
	quit = true;
	receive_thread.join();
	client_actions_thread.join();
}
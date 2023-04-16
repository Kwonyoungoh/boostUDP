#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>
#include <mysqlx/xdevapi.h>
#include <set>

using json = nlohmann::json;
// 플래그 
enum class conn_flags : uint8_t {
	CONNECT_FLAG = 0x01,
	DISCONNECT_FLAG = 0x02,
	CONNECTION_ACK_FLAG = 0x03,
	DISCONNECTION_ACK_FLAG = 0x04,
	DATA_FLAG =0x05
};

using boost::asio::ip::udp;

class UdpServer {
public:
	UdpServer(boost::asio::io_context& io_context, unsigned short port);

private:
	void receive();
	void handle_client_management(std::size_t bytes_recvd);
	void broadcast(const std::array<uint8_t, 1024>& data, std::size_t size, const udp::endpoint& sender_endpoint);
	mysqlx::Session connect_to_database();
	void update_user_location(const std::string& steamid, float x, float y, float z);

	udp::socket socket_;
	udp::endpoint remote_endpoint_;
	std::array<uint8_t, 1024> recv_buffer_;
	std::set<udp::endpoint> connected_clients_;
};

UdpServer::UdpServer(boost::asio::io_context& io_context, unsigned short port)
	: socket_(io_context, udp::endpoint(udp::v4(), port)) {
	std::cout << "Server is listening on port :" << port << std::endl;

	receive();
}

void UdpServer::receive() {
	socket_.async_receive_from(
		boost::asio::buffer(recv_buffer_), remote_endpoint_,
		[this](boost::system::error_code ec, std::size_t bytes_recvd) {
			if (!ec && bytes_recvd > 0) {
				// 연결 요청 및 종료 메시지 처리
				handle_client_management(bytes_recvd);

				receive();
			}
			else {
				std::cerr << "Error receiving data: " << ec.message() << std::endl;
				connected_clients_.erase(remote_endpoint_);
				// 계속 수신
				receive();
			}
		});
}

void UdpServer::handle_client_management(std::size_t bytes_recvd) {
	
	// 플래그
	conn_flags flag = static_cast<conn_flags>(recv_buffer_[0]);

	// 연결 요청 메시지 처리
	if (flag == conn_flags::CONNECT_FLAG) {
		
		// 클라이언트에 연결 완료 메시지 송신
		std::array<uint8_t, 2> response = { static_cast<uint8_t>(conn_flags::CONNECTION_ACK_FLAG), 0x00 };
		socket_.async_send_to(
			boost::asio::buffer(response, 2), remote_endpoint_,
			[this](boost::system::error_code ec, std::size_t bytes_sent) {
				if (ec) {
					std::cerr << "Error [CONNECT_ACK_FLAG] to " << remote_endpoint_ << " : " << ec.message() << std::endl;
				}
				else {
					std::cout << "Successfully sent [CONNECT_ACK_FLAG] to " << remote_endpoint_ << std::endl;
				}
			});
		
		// 중복 연결 요청 확인
		if (connected_clients_.find(remote_endpoint_) == connected_clients_.end()) {
		// 해당 클라이언트 추가
			connected_clients_.insert(remote_endpoint_);
		}
	}
	// 연결 종료 메시지 대한 처리
	else if (flag == conn_flags::DISCONNECT_FLAG) {
		connected_clients_.erase(remote_endpoint_);

		// 클라이언트에게 연결 종료 메시지 송신
		std::array<uint8_t, 2> response = { static_cast<uint8_t>(conn_flags::DISCONNECTION_ACK_FLAG), 0x00 };
		socket_.async_send_to(
			boost::asio::buffer(response, 2), remote_endpoint_,
			[this](boost::system::error_code ec, std::size_t bytes_sent) {
				if (ec) {
					std::cerr << "Error [DISCONNECT_ACK_FLAG] to " << remote_endpoint_ << " : " << ec.message() << std::endl;
				}
				else {

					std::cout << "Successfully sent [DISCONNECT_ACK_FLAG] to " << remote_endpoint_ << std::endl;
				}
			});
		
		// JSON 데이터 추출 및 데이터베이스에 저장
		std::string json_string(recv_buffer_.begin() + 1, recv_buffer_.begin() + bytes_recvd);
		json received_data = json::parse(json_string);

		std::string steamid = received_data["_steamid"].get<std::string>();
		float x = received_data["x"].get<float>();
		float y = received_data["y"].get<float>();
		float z = received_data["z"].get<float>();

		std::cout << "Received data: " << json_string << std::endl;
		update_user_location(steamid, x, y, z);
	}
	else if (flag == conn_flags::DATA_FLAG) {

		// 브로드캐스트 메시지를 처리
		broadcast(recv_buffer_, bytes_recvd, remote_endpoint_);
	}
}

void UdpServer::broadcast(const std::array<uint8_t, 1024>& data, std::size_t size, const udp::endpoint& sender_endpoint) {
	
	std::cout << "Broadcasting data to " << connected_clients_.size() << " clients" << std::endl;
	
	for (const auto& endpoint : connected_clients_) {

		// 발신 클라이언트 스킵
		if (endpoint == sender_endpoint) {
			continue;
		}

		socket_.async_send_to(
			boost::asio::buffer(data, size), endpoint,
			[endpoint](boost::system::error_code ec, std::size_t bytes_sent) {
				if (ec) {
					std::cerr << "Error sending data to " << endpoint << ": " << ec.message() << std::endl;
				}
				else {
				/*	std::cout << "Successfully sent " << bytes_sent << " bytes to " << endpoint << std::endl;*/
				}
			});
	}
}

mysqlx::Session UdpServer::connect_to_database() {
	try {
		mysqlx::Session sess("localhost", 3306, "root", "password", "user_info_db");
		return sess;
	}
	catch (const std::exception& e) {
		std::cerr << "Database connection error: " << e.what() << std::endl;
	}
}


void UdpServer::update_user_location(const std::string& steamid, float x, float y, float z) {
	try {
		mysqlx::Session sess = connect_to_database();
		mysqlx::Schema db = sess.getSchema("user_info_db");
		mysqlx::Table user_location = db.getTable("user_location");

		mysqlx::Result result = user_location.update()
			.set("x", x)
			.set("y", y)
			.set("z", z)
			.where("_steamid = :steamid")
			.bind("steamid", steamid)
			.execute();

		if (result.getAffectedItemsCount() == 0) {
			std::cerr << "Error: SteamID not found" << std::endl;
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error executing query: " << e.what() << std::endl;
	}
}
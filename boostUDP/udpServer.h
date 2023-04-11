#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <set>

// �÷��� 
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
				// ���� ��û �� ���� �޽��� ó��
				handle_client_management(bytes_recvd);

				receive();
			}
			else {
				std::cerr << "Error receiving data: " << ec.message() << std::endl;
				connected_clients_.erase(remote_endpoint_);
				// ��� ����
				receive();
			}
		});
}

void UdpServer::handle_client_management(std::size_t bytes_recvd) {
	
	// �÷���
	conn_flags flag = static_cast<conn_flags>(recv_buffer_[0]);

	// ���� ��û �޽��� ó��
	if (flag == conn_flags::CONNECT_FLAG) {
		
		// Ŭ���̾�Ʈ�� ���� �Ϸ� �޽��� �۽�
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
		
		// �ߺ� ���� ��û Ȯ��
		if (connected_clients_.find(remote_endpoint_) == connected_clients_.end()) {
		// �ش� Ŭ���̾�Ʈ �߰�
			connected_clients_.insert(remote_endpoint_);
		}
	}
	// ���� ���� �޽��� ���� ó��
	else if (flag == conn_flags::DISCONNECT_FLAG) {
		connected_clients_.erase(remote_endpoint_);

		// Ŭ���̾�Ʈ���� ���� ���� �޽��� �۽�
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

	}
	else if (flag == conn_flags::DATA_FLAG) {
		// ��ε�ĳ��Ʈ �޽����� ó��
		broadcast(recv_buffer_, bytes_recvd, remote_endpoint_);
	}
}

void UdpServer::broadcast(const std::array<uint8_t, 1024>& data, std::size_t size, const udp::endpoint& sender_endpoint) {
	
	std::cout << "Broadcasting data to " << connected_clients_.size() << " clients" << std::endl;
	
	for (const auto& endpoint : connected_clients_) {

		// �߽� Ŭ���̾�Ʈ ��ŵ
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
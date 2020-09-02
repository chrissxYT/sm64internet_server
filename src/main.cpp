#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

class vec3d {
        public:
                double x, y, z;
                vec3d(double _X, double _Y, double _Z) : x(_X), y(_Y), z(_Z) {}
};

class player {
        public:
                int m_id;
                boost::asio::ip::address m_addr;
                vec3d m_pos;
                std::string m_name;
                player(const int id,
                       const boost::asio::ip::address &addr,
                       const vec3d &pos,
                       const char *name)
                        : m_id(id),
                          m_addr(addr),
                          m_pos(pos),
                          m_name(name) {}
};

class server {
        public:
                server(boost::asio::io_service &io_service, short port)
                        : m_io_service(io_service),
                          m_socket(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)) {
                        m_socket.async_receive_from(
                                boost::asio::buffer(m_data, max_length), m_sender,
                                boost::bind(&server::handle_receive_from, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
                }

                // this control flow is quite lost and goto-heavy, TODO: improve
                void handle_receive_from(const boost::system::error_code &error,
                                         size_t bytes_recvd) {
                        if (!error && bytes_recvd > 0) {
                                if(m_data[0] == PACKET_HANDSHAKE) {
                                                if(bytes_recvd < PACKET_SIZE_HANDSHAKE) goto fail;
                                                if(m_data[1] != 0) goto fail;
                                                int id(0);
                                                for(player &p : players) {
                                                        id = std::max(id, p.m_id + 1);
                                                }
                                                player p(id,
                                                         m_sender.address(),
                                                         vec3d(0, 0, 0),
                                                         &m_data[2]);
                                                players.push_back(p);
                                } else if(m_data[0] == PACKET_POSITION) {
                                                if(bytes_recvd < PACKET_SIZE_POSITION) goto fail;
                                                if(players.empty()) goto fail;
                                                player &p = players.front();
                                                bool found(false);
                                                for(player &pl : players) {
                                                        if(pl.m_id == m_data[1]) {
                                                                p = pl;
                                                                found = true;
                                                        }
                                                }
                                                if(!found || p.m_addr != m_sender.address()) goto fail;
                                                memcpy(&p.m_pos.x, &m_data[2+sizeof(double)*0], sizeof(double));
                                                memcpy(&p.m_pos.y, &m_data[2+sizeof(double)*1], sizeof(double));
                                                memcpy(&p.m_pos.z, &m_data[2+sizeof(double)*2], sizeof(double));
                                } else if(m_data[0] == PACKET_ACTION) {
                                                //TODO: implement
                                } else goto fail;
                                m_socket.async_send_to(
                                        boost::asio::buffer(m_data, bytes_recvd), m_sender,
                                        boost::bind(&server::handle_send_to, this,
                                                boost::asio::placeholders::error,
                                                boost::asio::placeholders::bytes_transferred));
                        } else goto fail_silently;
                        return;
fail:
                        m_socket.async_send_to(
                                boost::asio::buffer({0}, 1), m_sender,
                                boost::bind(&server::handle_send_to, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
                        return;
fail_silently:
                        m_socket.async_receive_from(
                                boost::asio::buffer(m_data, max_length), m_sender,
                                boost::bind(&server::handle_receive_from, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
                }

                void handle_send_to(const boost::system::error_code &, size_t) {
                        m_socket.async_receive_from(
                                boost::asio::buffer(m_data, max_length), m_sender,
                                boost::bind(&server::handle_receive_from, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
                }

        private:
                boost::asio::io_service &m_io_service;
                boost::asio::ip::udp::socket m_socket;
                boost::asio::ip::udp::endpoint m_sender;
                enum { max_length = 1024 };
                char m_data[max_length];
                std::vector<player> players;

                enum {
                        PACKET_HANDSHAKE = 0,
                        PACKET_POSITION = 1,
                        PACKET_ACTION = 2,
                };

                enum {
                        PACKET_SIZE_HANDSHAKE = 1 + 1 + sizeof(char),
                        PACKET_SIZE_POSITION = 1 + 3 * sizeof(double),
                        PACKET_SIZE_ACTION = 1 + 1 + 1,
                };
};

int main() {
        try {
                boost::asio::io_service io_service;
                server s(io_service, 13370);
                io_service.run();
        }
        catch (std::exception &e) {
                std::cerr << "Exception: " << e.what() << "\n";
        }

        return 0;
}

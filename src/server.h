#ifndef FUZZYDB_SERVER
#define FUZZYDB_SERVER

#include <vector>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "./ISession.h"
#include "./session.h"

using boost::asio::ip::tcp;

class TCPServer
{
    friend class TCPSession;
public:
    TCPServer(boost::asio::io_service& io_service, unsigned short port);
    ~TCPServer();

    void handle_accept(std::shared_ptr<ISession> session, const boost::system::error_code& error);

private:
    void listenForNewConnection();

    unsigned short _port;
    boost::asio::io_service& _io_service;
    tcp::acceptor _acceptor;

    boost::uuids::random_generator _uuidGenerator;
};

#endif
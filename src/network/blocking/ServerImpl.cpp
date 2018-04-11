#include "ServerImpl.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <pthread.h>
#include <signal.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <afina/Storage.h>
#include <protocol/Parser.h>
#include <sstream>

namespace Afina {
namespace Network {
namespace Blocking {

void *ServerImpl::RunAcceptorProxy(void *p) {
    ServerImpl *srv = reinterpret_cast<ServerImpl *>(p);
    try {
        srv->RunAcceptor();
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Server.h
ServerImpl::ServerImpl(std::shared_ptr<Afina::Storage> ps) : Server(ps) {}

// See Server.h
ServerImpl::~ServerImpl() {}

// See Server.h
void ServerImpl::Start(uint32_t port, uint16_t n_workers) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // If a client closes a connection, this will generally produce a SIGPIPE
    // signal that will kill the process. We want to ignore this signal, so send()
    // just returns -1 when this happens.
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &sig_mask, NULL) != 0) {
        throw std::runtime_error("Unable to mask SIGPIPE");
    }

    // Setup server parameters BEFORE thread created, that will guarantee
    // variable value visibility
    max_workers = n_workers;
    listen_port = port;

    // The pthread_create function creates a new thread.
    //
    // The first parameter is a pointer to a pthread_t variable, which we can use
    // in the remainder of the program to manage this thread.
    //
    // The second parameter is used to specify the attributes of this new thread
    // (e.g., its stack size). We can leave it NULL here.
    //
    // The third parameter is the function this thread will run. This function *must*
    // have the following prototype:
    //    void *f(void *args);
    //
    // Note how the function expects a single parameter of type void*. We are using it to
    // pass this pointer in order to proxy call to the class member function. The fourth
    // parameter to pthread_create is used to specify this parameter value.
    //
    // The thread we are creating here is the "server thread", which will be
    // responsible for listening on port 23300 for incoming connections. This thread,
    // in turn, will spawn threads to service each incoming connection, allowing
    // multiple clients to connect simultaneously.
    // Note that, in this particular example, creating a "server thread" is redundant,
    // since there will only be one server thread, and the program's main thread (the
    // one running main()) could fulfill this purpose.
    running.store(true);
    if (pthread_create(&accept_thread, NULL, ServerImpl::RunAcceptorProxy, this) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
}

// See Server.h
void ServerImpl::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(false);
    Join();

}

// See Server.h
void ServerImpl::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    std::unique_lock<std::mutex> lock(connections_mutex);

    pthread_join(accept_thread, 0);

    for (auto&& thread : connections)
    {
        pthread_join(thread, 0);
    }

    connections.clear();

}


// See Server.h
void ServerImpl::RunAcceptor() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // For IPv4 we use struct sockaddr_in:
    // struct sockaddr_in {
    //     short int          sin_family;  // Address family, AF_INET
    //     unsigned short int sin_port;    // Port number
    //     struct in_addr     sin_addr;    // Internet address
    //     unsigned char      sin_zero[8]; // Same size as struct sockaddr
    // };
    //
    // Note we need to convert the port to network order

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_port = htons(listen_port); // TCP port number
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any address

    // Arguments are:
    // - Family: IPv4
    // - Type: Full-duplex stream (reliable)
    // - Protocol: TCP
    int server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) {
        throw std::runtime_error("Failed to open socket");
    }

    // when the server closes the socket,the connection must stay in the TIME_WAIT state to
    // make sure the client received the acknowledgement that the connection has been terminated.
    // During this time, this port is unavailable to other processes, unless we specify this option
    //
    // This option let kernel knows that we are OK that multiple threads/processes are listen on the
    // same port. In a such case kernel will balance input traffic between all listeners (except those who
    // are closed already)
    int opts = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket setsockopt() failed");
    }

    // Bind the socket to the address. In other words let kernel know data for what address we'd
    // like to see in the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket bind() failed");
    }

    // Start listening. The second parameter is the "backlog", or the maximum number of
    // connections that we'll allow to queue up. Note that listen() doesn't block until
    // incoming connections arrive. It just makesthe OS aware that this process is willing
    // to accept connections on this socket (which is bound to a specific IP and port)
    if (listen(server_socket, 5) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket listen() failed");
    }

    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t sinSize = sizeof(struct sockaddr_in);
    while (running.load()) {
        std::cout << "network debug: waiting for connection..." << std::endl;

        // When an incoming connection arrives, accept it. The call to accept() blocks until
        // the incoming connection arrives
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sinSize)) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket accept() failed");
        }

        // TODO: Start new thread and process data from/to connection

        std::unique_lock<std::mutex> lock(connections_mutex);

        if(connections.size() >= max_workers)
        {
            close(client_socket);
        }

        pthread_t new_thread;
        ServerImpl::ProxyArgs args = {this, client_socket};

        if(pthread_create(&new_thread, nullptr, RunConnectionProxy, &args) != 0)
        {
            close(client_socket);
            throw std::runtime_error("Socket send() failed");
        }

        pthread_detach(new_thread);

        connections.insert(new_thread);

        lock.unlock();
    }

    // Cleanup on exit...

    close(server_socket);
}


    void* ServerImpl::RunConnectionProxy(void *proxy_args) {
        std::cout<< "RunConnectionProxy"<<std::endl;
        auto args = reinterpret_cast<ServerImpl::ProxyArgs*>(proxy_args);
        try{
            args->server->RunConnection(args->con_socket);
        } catch (std::runtime_error &ex) {
            std::cerr << "Connection fails: " << ex.what() << std::endl;
        }
        return nullptr;
    }


    bool ServerImpl::parse_commands(int client_socket, std::string& current_data, Afina::Protocol::Parser& parser)
    {
        auto new_data = new char[buffer_read_size];
        while (!current_data.empty())
        {
            size_t parsed = 0;
            bool command_flag = false;
            try
            {
                command_flag = parser.Parse(current_data, parsed);
            }
            catch (std::exception &e)
            {
                std::string error_msg = "Parsing error: ";
                error_msg += e.what();
                error_msg += end_of_msg;
                send(client_socket, error_msg.c_str(), error_msg.size(), 0);
                current_data = "";
                parser.Reset();

                // не получается распарсить команду, необходимо прочитать больше данных
                return false;
            }

            current_data = current_data.substr(parsed);
            if (!command_flag) {
                //не смогл прочитать команду до конца, читаем дальше
                return false;
            }


            uint32_t arguments_data_size = 0;
            auto command = parser.Build(arguments_data_size);

            if (arguments_data_size != 0) {
                arguments_data_size += end_of_msg.size();
            }

            if (arguments_data_size > current_data.size()) {
                if (recv(client_socket, new_data, (arguments_data_size) * sizeof(char), MSG_WAITALL) <= 0) {
                    // не получается прочитать аргументы для команды
                    throw std::exception();
                }
                current_data.append(new_data);
            }

            std::string argument;
            if (arguments_data_size > end_of_msg.size()) {
                argument = current_data.substr(0, arguments_data_size - end_of_msg.size()); // \r\n not needed
                current_data = current_data.substr(arguments_data_size); //remove argument from received data
            }

            std::string out;
            try
            {
                command->Execute(*pStorage, argument, out);
            }
            catch (std::exception &e) {
                out = "SERVER ERROR ";
                out += e.what();
            }

            out += end_of_msg;

            if (send(client_socket, out.c_str(), out.size(), 0) < out.size())
            {
                // не получается писать в сокет, закрываем соединение
                throw std::exception();
            }

            parser.Reset();

        }
        return true;
    }

    // See Server.h
    void ServerImpl::RunConnection(int client_socket)
    {
        std::cout<< "RunConnection"<<std::endl;
        Afina::Protocol::Parser parser;

        std::string current_data;
        auto new_data = new char[buffer_read_size];
        bool parse_result = false;

        while (running.load())
        {
            // обработка последовательности комманд реализовать в цикле
            if (recv(client_socket, new_data, buffer_read_size * sizeof(char), 0) <= 0)
            {
                break;
            }

            current_data.append(new_data);
            memset(new_data, 0, buffer_read_size *sizeof(char));

            try
            {
                parse_result = parse_commands(client_socket, current_data, parser);
            }
            catch (std::exception& e )
            {
                break;
            }

        }

        close(client_socket);
    }

} // namespace Blocking
} // namespace Network
} // namespace Afina

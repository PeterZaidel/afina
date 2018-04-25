#include "Worker.h"

#include <iostream>
#include <afina/Storage.h>

#include "afina/execute/Command.h"

//#include <sys/epoll.h>
//#include <sys/socket.h>
//#include <sys/types.h>
//#include <protocol/Parser.h>

//#include "Utils.h"


#define EPOLLEXCLUSIVE 1 << 28
namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps):
 pStorage(ps)
{}

// See Worker.h
Worker::~Worker() {
    Stop();
    Join();
}


void* Worker::OnRunProxy(void* _args) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
//    auto  args = reinterpret_cast<OnRunArgs*>(_args);

    Worker* worker = (Worker*)_args;
    int server_socket = worker->server_socket.load();
    try {
        worker->OnRun(server_socket);
    }catch (std::exception &ex)
    {
        std::cerr << "Connection fails: " << ex.what() << std::endl;
    }

    return nullptr;
}


//static void* TestFn(void* _args)
//{
//    return NULL;
//}

// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    this->server_socket.store(server_socket);

//    Worker::OnRunArgs args = {this, 0};
    running.store(true);

//    int res_p = pthread_create(&this->thread, nullptr, OnRunProxy, &args);
    if( pthread_create(&this->thread, nullptr, OnRunProxy, this) != 0)
    {
        throw std::runtime_error("Creating worker thread failed");
    }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(false);
    shutdown(server_socket, SHUT_RDWR);
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(thread, nullptr);
}



bool Worker::parse_commands(int client_socket, std::string& current_data, Afina::Protocol::Parser& parser)
{
    auto new_data = new char[BUFFER_READ_SIZE];
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
            error_msg += END_OF_MSG;
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
            arguments_data_size += END_OF_MSG.size();
        }

        if (arguments_data_size > current_data.size())
        {
            while(true)
            {
                ssize_t n = recv(client_socket, new_data, (arguments_data_size) * sizeof(char), MSG_WAITALL);
                if(n < 0 && errno == EAGAIN)
                {
                    continue;
                }
                if(n > 0 )
                {
                    break;
                }

                if (n <= 0) {
                    // не получается прочитать аргументы для команды
                    throw std::exception();
                }
            }
            current_data.append(new_data);
        }

        std::string argument;
        if (arguments_data_size > END_OF_MSG.size()) {
            argument = current_data.substr(0, arguments_data_size - END_OF_MSG.size()); // \r\n not needed
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

        out += END_OF_MSG;

        if (send(client_socket, out.c_str(), out.size(), 0) < out.size())
        {
            // не получается писать в сокет, закрываем соединение
            throw std::exception();
        }

        parser.Reset();

    }
    return true;
}

void Worker::process_client(int client_socket)
{
    std::cout<< "RunConnection"<<std::endl;
    Afina::Protocol::Parser parser;

    std::string current_data;
    auto new_data = new char[BUFFER_READ_SIZE];
    bool parse_result = false;

    while (running.load())
    {
        // обработка последовательности комманд реализовать в цикле
        ssize_t n = recv(client_socket, new_data, BUFFER_READ_SIZE * sizeof(char), 0);

        if(n < 0 && errno == EAGAIN)
        {
            continue;
        }

        if (n <= 0)
        {
            std::cout << "network debug: " << __PRETTY_FUNCTION__ <<"recv()=0 close connection: "<<client_socket << std::endl;
            close(client_socket);
            return;
        }

        current_data.append(new_data);
        memset(new_data, 0, BUFFER_READ_SIZE *sizeof(char));

        try
        {
            std::cout <<"CLIENT: "<< client_socket  <<" PARSE COMMAND: "<< current_data << std::endl;
            parse_result = parse_commands(client_socket, current_data, parser);
        }
        catch (std::exception& e )
        {
            std::cout << "network debug: " << __PRETTY_FUNCTION__ <<" close connection: "<<client_socket << std::endl;
            close(client_socket);
            return;
        }
    }



}

void Worker::process_event(epoll_event event, int server_socket, int efd)
{
    if ((event.events & EPOLLERR) ||
        (event.events & EPOLLHUP) ||
        (!(event.events & EPOLLIN) &&
            !(event.events & EPOLLOUT)) )
    {
        /* An error has occured on this fd, or the socket is not
           ready for reading (why were we notified then?) */
        std::cout << "network debug: " << __PRETTY_FUNCTION__ <<" events :"<< event.events<<" close connection: "<<event.data.fd << std::endl;
        close(event.data.fd);
        return;
    }
    else if (server_socket == event.data.fd)
    {
        /* We have a notification on the listening socket, which
           means one or more incoming connections. */
        while (true)
        {
            sockaddr in_addr;
            socklen_t in_len;
            int infd;

            in_len = sizeof in_addr;

            std::cout<<"EPOLL "<<efd<< " accept client"<<std::endl;

            infd = accept(server_socket, &in_addr, &in_len);
            if (infd == -1)
            {
                if ((errno == EAGAIN) ||
                    (errno == EWOULDBLOCK)) {
                    /* We have processed all incoming
                       connections. */
                    break;
                } else {
//                    perror("accept");
                    break;
                }
            }

            /* Make the incoming socket non-blocking and add it to the
               list of fds to monitor. */
            try
            {
                make_socket_non_blocking(infd);
            }
            catch (std::exception& e)
            {
                return;
            }

            event.data.fd = infd;
            event.events =  EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR;
            if (epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event) < 0)
            {
                return;
            }
        }
        return;
    }
    else
    {
        /* We have data on the fd waiting to be read. Read and
           display it. We must read whatever data is available
           completely, as we are running in edge-triggered mode
           and won't get a notification again for the same
           data. */
        // работаем с клиентом читаем, пишем и тд

        int client_sock = event.data.fd;

        std::cout<<"EPOLL "<<efd<< " process client "<<client_sock<<std::endl;

        process_client(client_sock);
    }
}

// See Worker.h
void* Worker::OnRun(int server_socket) {

    pthread_t cur_thread = pthread_self() % 100;
    std::cout << "network debug: " << __PRETTY_FUNCTION__ <<"Start "<<" cur thread: " <<cur_thread << std::endl;



//    int s;

    struct epoll_event event{};
    epoll_event events_arr [MAX_EPOLL_EVENTS];
//    epoll_event * events_arr = (epoll_event*)malloc(MAX_EPOLL_EVENTS * sizeof(epoll_event));
//    int* arrInt = new int[MAX_EPOLL_EVENTS];
//    arrInt[0] = 10;
//
//    std::cout<< arrInt[0]<<std::endl;


    std::cout << "network debug: " << __PRETTY_FUNCTION__ << " 2" <<" cur thread: " <<cur_thread << std::endl;


//    int listen_sock;
//    int conn_sock;
//    int nfds;
    int efd = epoll_create(MAX_EPOLL_EVENTS);

    if(efd  < 0)
    {
        throw std::runtime_error("Failed to create epoll");
    }

    std::cout << "network debug: " << __PRETTY_FUNCTION__ << " 3" <<" cur thread: " <<cur_thread << std::endl;

    event.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLHUP | EPOLLERR;
    event.data.fd = server_socket;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        throw std::runtime_error("epollexclusive");
    }

    std::cout << "network debug: " << __PRETTY_FUNCTION__ << " 4 "<< running.load() <<" cur thread: " <<cur_thread << std::endl;

    while (running.load())
    {
        int n;

//        std::cout << "network debug: " << __PRETTY_FUNCTION__ << " EPOLL WAIT" <<" cur thread: " <<cur_thread << std::endl;

        n = epoll_wait(efd, events_arr, MAX_EPOLL_EVENTS, -1);
        if(errno == EINTR)
        {
            continue;
        }

        std::cout<<"EPOLL "<<efd<<"  got "<<n<<" events"<<std::endl;

        if (n == -1 )
        {
            std::cout<< "epoll wait falied: "<< errno<< std::endl;
            std::cout << "network debug: " << __PRETTY_FUNCTION__ << " EXCEPTION EPOLL WAIT FAILED" <<" cur thread: " <<cur_thread << std::endl;
            throw std::runtime_error("epoll_wait() failed");
        }

        for (int i = 0; i < n; i++)
        {

            auto cur_event = events_arr[i];
            std::cout << " EPOLL Process event: " << cur_event.events <<" cur thread: " <<cur_thread << std::endl;
            process_event(cur_event, server_socket, efd);
        }
    }


    std::cout << "network debug: " << __PRETTY_FUNCTION__ << " CLOSED" <<" cur thread: " <<cur_thread << std::endl;
    //When no longer required, the file descriptor
    //returned by epoll_create() should be closed by using close(2).
    close(efd);
//    delete[] events_arr;



    // TODO: implementation here
    // 1. Create epoll_context here
    // 2. Add server_socket to context
    // 3. Accept new connections, don't forget to call make_socket_nonblocking on
    //    the client socket descriptor
    // 4. Add connections to the local context
    // 5. Process connection events
    //
    // Do not forget to use EPOLLEXCLUSIVE flag when register socket
    // for events to avoid thundering herd type behavior.
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina

#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

#include <memory>
#include <pthread.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <protocol/Parser.h>
#include <atomic>
#include "Utils.h"

namespace Afina {

// Forward declaration, see afina/Storage.h
class Storage;

namespace Network {
namespace NonBlocking {

/**
 * # Thread running epoll
 * On Start spaws background thread that is doing epoll on the given server
 * socket and process incoming connections and its data
 */
class Worker {
public:

    Worker()
    {}

    Worker(std::shared_ptr<Afina::Storage> ps);
    ~Worker();

    /**
     * Spaws new background thread that is doing epoll on the given server
     * socket. Once connection accepted it must be registered and being processed
     * on this thread
     */
    void Start(int server_socket);

    /**
     * Signal background thread to stop. After that signal thread must stop to
     * accept new connections and must stop read new commands from existing. Once
     * all readed commands are executed and results are send back to client, thread
     * must stop
     */
    void Stop();

    /**
     * Blocks calling thread until background one for this worker is actually
     * been destoryed
     */
    void Join();

protected:

    class OnRunArgs{
    public:
        Worker* worker;
        int server_sock;

    };

     static void* OnRunProxy(void* args);

    /**
     * Method executing by background thread
     */
    void* OnRun(int server_socket);

private:


    bool parse_commands(int client_socket, std::string& current_data, Afina::Protocol::Parser& parser);

    void process_event(epoll_event event, int server_socket, int efd);

    void process_client(int client_socket);

    const int MAX_EPOLL_EVENTS = 50;

    const int buffer_read_size= 1024;
    const std::string end_of_msg = "\r\n";

    std::atomic<bool> running;
    std::atomic<int> server_socket ;

    pthread_t thread;
    std::shared_ptr<Afina::Storage> pStorage;

};

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_WORKER_H

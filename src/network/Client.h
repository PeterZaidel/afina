//
// Created by peter on 22.04.18.
//

#include <bits/shared_ptr.h>
#include <afina/Storage.h>
#include <atomic>

#ifndef AFINA_CONNECTION_H
#define AFINA_CONNECTION_H

#endif //AFINA_CONNECTION_H

namespace Afina {
namespace Network {

class Client
{
public:
    Client(int _client_socket, std::shared_ptr<Afina::Storage> _ps )
    :
        client_socket(_client_socket),
        ps(_ps)
        {}

    int process_client()
    {

    }

private:
    int client_socket;
    std::shared_ptr<Afina::Storage> ps;
    std::atomic<bool> running;
};


}
}
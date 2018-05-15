//
// Created by peter on 22.04.18.
//

#include <bits/shared_ptr.h>
#include <afina/Storage.h>
#include <atomic>
#include <iostream>
#include <protocol/Parser.h>
#include <network/nonblocking/Worker.h>
#include "afina/execute/Command.h"

#define  BUFFER_READ_SIZE 1024

#ifndef AFINA_CONNECTION_H
#define AFINA_CONNECTION_H

namespace Afina {
namespace Network {

class Client
{
public:

    enum CLientState
    {
        Open,
        Closed,
        WaitingArguments
    };

    Client(int _client_socket, std::shared_ptr<Afina::Storage> _ps )
    :
        socket(_client_socket),
        pStorage(_ps),
        state(CLientState::Open),
        parser(Afina::Protocol::Parser())
        {

            new_data = new char[BUFFER_READ_SIZE];
        }

    Client():
        state(CLientState::Closed),
        parser(Afina::Protocol::Parser())
    {
        new_data = new char[BUFFER_READ_SIZE];
    }

    CLientState  getState()
    {
        return state;
    }

    void close_connection()
    {
        close(socket);
        state = CLientState ::Closed;
    }

    void process_client() {
        //std::cout << __PRETTY_FUNCTION__ << std::endl;

        memset(new_data, 0, BUFFER_READ_SIZE * sizeof(char));
        bool parse_result = false;

        // обработка последовательности комманд реализовать в цикле
        ssize_t n = -1;
        if (state == CLientState::WaitingArguments) {
            n = recv(socket, new_data, (arguments_data_size) * sizeof(char), MSG_WAITALL);
        } else {
            n = recv(socket, new_data, BUFFER_READ_SIZE * sizeof(char), 0);
        }

        if (n < 0 && errno == EAGAIN) {
           // state = CLientState::Open;
            return;
        }

        if (n <= 0) {
            std::cout << "network debug: " << __PRETTY_FUNCTION__ << "recv()=0 close connection: " << socket << std::endl;
            close_connection();
            return;
        }

        current_data.append(new_data);


        std::cout << "CLIENT: " << socket << " PARSE COMMAND: " << current_data << std::endl;
        parse_commands();


    }

private:
    const std::string END_OF_MSG  = "\r\n";

    void parse_commands()
    {

        if(state == CLientState::Open)
        {
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


                    if (send(socket, error_msg.c_str(), error_msg.size(), 0) < error_msg.size())
                    {
                        // не получается писать в сокет, закрываем соединение
                        close_connection();
                        return;
                    }

                    current_data = "";
                    parser.Reset();

                    // не получается распарсить команду, необходимо читать дальше
                    state = CLientState ::Open;
                    return;
                }


                if (!command_flag)
                {
                    //не смогли прочитать команду до конца, читаем дальше
                    state = CLientState ::Open;
                    return;
                }
                current_data = current_data.substr(parsed);


                arguments_data_size = 0;
                command = parser.Build(arguments_data_size);

                if (arguments_data_size != 0) {
                    arguments_data_size += END_OF_MSG.size();
                }
                if(arguments_data_size > current_data.size())
                {
                    state = CLientState ::WaitingArguments;
                    return;
                }

                if(arguments_data_size <= current_data.size())
                {
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

                    if (send(socket, out.c_str(), out.size(), 0) < out.size())
                    {
                        // не получается писать в сокет, закрываем соединение
                        close_connection();
                        return;
                    }

                    parser.Reset();
                    state = CLientState ::Open;
                }
            }
        }


        if(state == CLientState::WaitingArguments)
        {
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

            if (send(socket, out.c_str(), out.size(), 0) < out.size())
            {
                // не получается писать в сокет, закрываем соединение
                close_connection();
                return;
            }

            parser.Reset();
            state = CLientState ::Open;
            return;
        }

        state = CLientState ::Open;
        return;

    }



    CLientState  state = CLientState::Closed;
    Afina::Protocol::Parser parser;
    int socket;
    std::shared_ptr<Afina::Storage> pStorage;

    std::string current_data;
    std::unique_ptr<Afina::Execute::Command> command;

    uint32_t arguments_data_size;

    char* new_data;

};


}
}

#endif //AFINA_CONNECTION_H
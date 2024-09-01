
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "common.h"

using namespace std;

int main()
{
    // 1. 创建socket
    // 2. bind 端口
    // 3. listen；把socket在内核中进行注册
    // 4. 创建epoll文件描述符
    // 5. 把socket注册到epoll
    // 6. 把epoll注册进内核
    // 7. 处理epoll事件

    auto server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        cout << "create socket failed" << endl;
        return -1;
    }

    cout << "create socket, fd:" << server_fd << endl;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    auto bind_ret =
        bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    if (bind_ret < 0)
    {
        close(server_fd);
        cout << "bind failed" << endl;
        return -1;
    }

    cout << "bind port, fd-port:" << server_fd << "-" << PORT << endl;

    auto listen_ret = listen(server_fd, 10);

    cout << "listen fd:" << server_fd << endl;

    int opt = 1;
    auto ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                          &opt, sizeof(opt));
    if (ret != 0)
    {
        cout << "set opt failed:" << ret << endl;
        return -1;
    }

    if (set_non_blocking(server_fd) < 0)
    {
        close(server_fd);
        return -1;
    }

    auto epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        cout << "create epoll_fd failded" << endl;
        return -1;
    }

    cout << "create epoll fd:" << epoll_fd << endl;

    struct epoll_event event;
    event.data.fd = server_fd;
    event.events = EPOLLIN;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
    {
        std::cerr << "epoll_ctl failed" << std::endl;
        close(server_fd);
        close(epoll_fd);
        return -1;
    }

    cout << "epoll_ctl, server_fd:" << server_fd << ", epoll_fd:" << epoll_fd
         << endl;

    cout << "epoll begin, server_fd:" << server_fd << ", epoll_fd:" << epoll_fd
         << endl;
    struct epoll_event events[1024];
    while (true)
    {
        int num_fds = epoll_wait(epoll_fd, events, 1024, -1);
        if (num_fds == -1)
        {
            cout << "epoll wait failed" << endl;
            return -1;
        }

        for (int i = 0; i < num_fds; ++i)
        {
            if (events[i].data.fd == server_fd)
            {
                auto addrlen = sizeof(address);
                auto conn = accept(server_fd, (struct sockaddr *)&address,
                                   (socklen_t *)&addrlen);

                if (set_non_blocking(conn) < 0)
                {
                    cout << "set socket noblocking failed, fd:" << conn << endl;
                    close(conn);
                    continue;
                }

                event.data.fd = conn;
                event.events = EPOLLIN | EPOLLET;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn, &event) == -1)
                {
                    std::cerr << "epoll_ctl failed" << std::endl;
                    close(conn);
                }
                else
                {
                    cout << "add connect, fd:" << conn << endl;
                }
            }
            else
            {
                char buffer[1024];
                ssize_t count;

                while ((count = read(events[i].data.fd, buffer,
                                     sizeof(buffer))) > 0)
                {
                    std::cout << "fd:" << events[i].data.fd
                              << ", received:" << std::string(buffer, count)
                              << std::endl;
                }

                if (count == 0)
                {
                    std::cout << "Client close, df:" << events[i].data.fd
                              << std::endl;
                    close(events[i].data.fd);
                }
                else if (count == -1 && errno != EAGAIN)
                {
                    std::cerr << "read failed" << std::endl;
                    close(events[i].data.fd);
                }
            }
        }
    }

    return 0;
}
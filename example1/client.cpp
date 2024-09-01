#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <linux/errqueue.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "common.h"

#define BUFFER_SIZE 4096

int main()
{
    int sock_fd;
    struct sockaddr_in server_addr
    {
    };

    // 创建套接字
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        perror("socket failed");
        return 1;
    }

    // 设置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);

    // 连接服务器
    if (connect(sock_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1)
    {
        perror("connect failed");
        close(sock_fd);
        return 1;
    }

    std::cout << "Connected to the server!" << std::endl;

    // 创建用户态缓冲区
    char *user_buffer = new char[BUFFER_SIZE];
    strcpy(user_buffer, "Hello, Zero-Copy!");

    // 构建 iovec 结构，用于描述用户态缓冲区
    struct iovec iov;
    iov.iov_base = user_buffer;
    iov.iov_len = strlen(user_buffer);

    // 构建 msghdr 结构
    struct msghdr msg
    {
    };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = nullptr;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    // 使用 sendmsg 进行零拷贝发送
    ssize_t sent_bytes = sendmsg(sock_fd, &msg, MSG_ZEROCOPY);
    if (sent_bytes == -1)
    {
        perror("sendmsg failed");
        delete[] user_buffer;
        close(sock_fd);
        return 1;
    }

    std::cout << "Sent " << sent_bytes << " bytes using zero-copy."
              << std::endl;

    // 等待零拷贝完成的通知（可选）
    struct msghdr err_msg
    {
    };
    char ctrl_buffer[CMSG_SPACE(sizeof(uint64_t))];
    struct cmsghdr *cmsg;
    struct sock_extended_err *serr;
    ssize_t recv_err;

    err_msg.msg_control = ctrl_buffer;
    err_msg.msg_controllen = sizeof(ctrl_buffer);

    recv_err = recvmsg(sock_fd, &err_msg, MSG_ERRQUEUE);
    if (recv_err == -1)
    {
        perror("recvmsg (error queue) failed");
    }
    else
    {
        // 解析控制消息
        cmsg = CMSG_FIRSTHDR(&err_msg);
        if (cmsg && cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR)
        {
            serr = (struct sock_extended_err *)CMSG_DATA(cmsg);
            if (serr->ee_origin == SO_EE_ORIGIN_ZEROCOPY)
            {
                std::cout << "Zero-copy send completed!" << std::endl;
            }
        }
    }

    // 清理资源
    delete[] user_buffer;
    close(sock_fd);
    return 0;
}

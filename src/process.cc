#include <unistd.h>
#include "process.h"

extern char **environ;

LocalProcess::LocalProcess(const char *path, const char *argv[])
    : Process(path, argv)
{
    pipe(pipe_stdin);
    pipe(pipe_stdout);
    pipe(pipe_stderr);

    pid = fork();

    if (!pid) {

        close(pipe_stdin[1]);
        close(pipe_stdout[0]);
        close(pipe_stderr[0]);

        dup2(pipe_stdin[0], STDIN_FILENO);
        dup2(pipe_stdout[1], STDOUT_FILENO);
        dup2(pipe_stderr[1], STDERR_FILENO);

        execve(
            path,
            const_cast<char **>(argv),
            environ
        );
    }

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);
    close(pipe_stderr[1]);
}

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <cstring>
#include <iostream>

RemoteProcess::RemoteProcess(const char* host, int port, const char *path, const char *argv[])
    : Process(path, argv)
{
    memset(pipe_stdin, -1, sizeof(pipe_stdin));
    memset(pipe_stdout, -1, sizeof(pipe_stdout));
    memset(pipe_stderr, -1, sizeof(pipe_stderr));

    // consider supporting ipv6?
    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        std::cerr << "cannot create socket (socket)\n";
        return;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    in_addr_t inaddr = inet_addr(host);
    memcpy(&addr.sin_addr, &inaddr, sizeof(addr.sin_addr));

    int result = connect(sock_fd, (sockaddr*)&addr, sizeof(addr));
    if (result < 0) {
      std::cerr << "socket connect error: " << result << "\n";
      std::cerr << "errno: " << errno << "\n";
      return;
    }

    pipe_stdin[1] = sock_fd;
    pipe_stdout[0] = sock_fd;
    pipe_stderr[0] = -1;
}

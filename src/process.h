#pragma once

#include <unistd.h>

class Process
{
    public:
        Process(const char *path, const char *argv[]) { }

        virtual int get_stdin() const { return pipe_stdin[1]; }
        virtual int get_stdout() const { return pipe_stdout[0]; }
        virtual int get_stderr() const { return pipe_stderr[0]; }

    protected:
        int pipe_stdin[2];
        int pipe_stdout[2];
        int pipe_stderr[2];
};

class LocalProcess : public Process
{
    public:
        LocalProcess(const char *path, const char *argv[]);

    private:
        pid_t pid;
};

class RemoteProcess : public Process
{
    public:
        RemoteProcess(const RemoteProcess&) = delete;
        const RemoteProcess& operator = (const RemoteProcess&) = delete;

        RemoteProcess(const char* host, int port, const char *path, const char *argv[]);

    private:
        int sock_fd;

};

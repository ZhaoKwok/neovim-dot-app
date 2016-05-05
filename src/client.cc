#include <sstream>
#include <tuple>
#include <iostream>
#include <cassert>
#include <msgpack.hpp>

#include "client.h"
#include "process.h"


static ssize_t safe_write(int fildes, const void* buf, size_t nbyte)
{
    const uint8_t* p_buf = static_cast<const uint8_t*>(buf);
    while (nbyte) {
        ssize_t r = write(fildes, p_buf, nbyte);
        if (r < 0) return r;
        nbyte -= r;
        p_buf += nbyte;
    }
    return nbyte;
}

Client::Client(Process &process):
    process(process),
    next_id(0)
{
    if (process.get_stdin() < 0 || process.get_stdout() < 0) {
        std::cerr << "bad process! exiting...\n";
        exit(-1);
    }
    pthread_mutex_init(&mutex, 0);
}

Client::~Client()
{
    pthread_mutex_destroy(&mutex);
}

/* Send an arbitrary string to the other process's stdin */
void Client::send(const std::string &message)
{
    safe_write(process.get_stdin(), &message[0], message.size());
}

/* Block until something happens */
Event Client::wait()
{
    typedef msgpack::type::tuple<
        int,
        std::string,
        msgpack::object
    > note_t;

    int pstdout = process.get_stdout();
    int pstderr = process.get_stderr();

again:

    pthread_mutex_lock(&mutex);

    /* Reset the unpacked object, dropping all the objects
       we parsed last time from memory. */
    unpacked = msgpack::unpacked();

    while(unpacker.next(&unpacked)) {

        msgpack::object item = unpacked.get();

        if (item.type != msgpack::type::ARRAY)
            continue;

        std::vector<msgpack::object> array = item.convert();

        int type = array[0].convert();

        /* A response to one of our requests */
        if (type == 1) {
            response_t response = item.as<response_t>();
            int id = std::get<1>(response);

            rpc_map_t::iterator iter = rpc_map.find(id);
            if (iter != rpc_map.end()) {
                RPC *rpc = iter->second;
                rpc->error = std::get<2>(response);
                rpc->value = std::get<3>(response);
                rpc->resolved = true;
                rpc_map.erase(iter);
                pthread_mutex_unlock(&mutex);
                Event r = {rpc};
                return r;
            }
        }

        /* A notification */
        if (type == 2) {
            note_t note_data = item.as<note_t>();
            std::string method = std::get<1>(note_data);
            msgpack::object args = std::get<2>(note_data);
            pthread_mutex_unlock(&mutex);
            Event r = {0, method, args};
            return r;

        }
    }
    pthread_mutex_unlock(&mutex);

    /* Unpacker is empty or has only partially received a message;
       wait until there is data to read from the process's stdout. */
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(pstdout, &readset);
    int result = select(pstdout + 1, &readset, 0, 0, 0);

    if (result < 0) {
        std::cerr << "Dunno what this means\n";
        exit(-1);
    }

    if (result > 0) {

        unpacker.reserve_buffer(1024);

        int sz = read(
            pstdout,
            unpacker.buffer(),
            unpacker.buffer_capacity()
        );

        if (sz == 0) {
            Event r = {0, std::string("neovim.app.nodata")};
            char errmsg[2048];
            if (pstderr >= 0) {
                if (read(pstderr, &errmsg, sizeof(errmsg)) != -1)
                    std::cerr << errmsg << "\n";
            }
            else {
                std::cerr << "Connection closed but there's no stderr to output." << "\n";
            }
            return r;
        }

        if (sz < 0) {
            std::cerr << "Read error\n";
            exit(-1);
        }

        unpacker.buffer_consumed(sz);
    }

    goto again;
}

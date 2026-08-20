#include "LocalSocketServer.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>

namespace duet::collab
{
namespace
{
    /** The listen backlog. One sidecar connects at a time, and the second connection
    exists only to be refused, so the queue never needs to be deep.
*/
    constexpr int connectionBacklog = 4;

    std::string describe (const char* what, int errorNumber)
    {
        return std::string { what } + ": " + std::generic_category().message (errorNumber);
    }

    /** Fills the address a Unix domain socket is named by.

    `sun_path` is a fixed 108-byte array, so a path that does not fit is a
    failure to report rather than a truncation to live with.
*/
    sockaddr_un addressFor (const std::string& path)
    {
        sockaddr_un address {};
        address.sun_family = AF_UNIX;

        if (path.size() + 1 > sizeof (address.sun_path))
            throw std::runtime_error ("Collaborator socket path is too long: " + path);

        std::memcpy (std::data (address.sun_path), path.data(), path.size());

        return address;
    }
} // namespace

void UniqueFd::close()
{
    if (descriptor >= 0)
        ::close (descriptor);

    descriptor = -1;
}

LocalSocketServer::~LocalSocketServer() { close(); }

void LocalSocketServer::listenAt (const std::filesystem::path& path)
{
    close();

    const auto text = path.string();
    const auto address = addressFor (text);

    UniqueFd bound { ::socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0) };

    if (! bound.valid())
        throw std::runtime_error (describe ("Collaborator socket could not be made", errno));

    // Whatever the path held is a socket no process is listening on any more —
    // a DAW that was killed rather than exited. bind() would fail on it.
    std::error_code ignored;
    std::filesystem::remove (path, ignored);

    // The sockets API is typed on sockaddr, and the cast to it is that API's
    // calling convention rather than a reinterpretation of anything.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* generic = reinterpret_cast<const sockaddr*> (&address);

    if (::bind (bound.get(), generic, sizeof (address)) != 0)
        throw std::runtime_error (describe ("Collaborator socket could not be bound", errno));

    if (::listen (bound.get(), connectionBacklog) != 0)
        throw std::runtime_error (describe ("Collaborator socket could not listen", errno));

    socket = std::move (bound);
    socketPath = path;
}

void LocalSocketServer::close()
{
    socket.close();

    if (! socketPath.empty())
    {
        std::error_code ignored;
        std::filesystem::remove (socketPath, ignored);
        socketPath.clear();
    }
}

UniqueFd LocalSocketServer::accept() const
{
    if (! socket.valid())
        return {};

    return UniqueFd { ::accept4 (socket.get(), nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK) };
}
} // namespace duet::collab

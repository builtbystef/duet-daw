#pragma once

#include <filesystem>
#include <utility>

namespace duet::collab
{
/** A file descriptor that closes itself. */
class UniqueFd
{
public:
    UniqueFd() = default;
    explicit UniqueFd (int descriptorToOwn) : descriptor (descriptorToOwn) {}
    ~UniqueFd() { close(); }

    UniqueFd (const UniqueFd&) = delete;
    UniqueFd& operator= (const UniqueFd&) = delete;

    UniqueFd (UniqueFd&& other) noexcept : descriptor (std::exchange (other.descriptor, -1)) {}

    UniqueFd& operator= (UniqueFd&& other) noexcept
    {
        if (this != &other)
        {
            close();
            descriptor = std::exchange (other.descriptor, -1);
        }

        return *this;
    }

    [[nodiscard]] bool valid() const { return descriptor >= 0; }
    [[nodiscard]] int get() const { return descriptor; }

    void close();

private:
    int descriptor = -1;
};

/** The listening end of the seam: a Unix domain stream socket at a filesystem
    path.

    A path and not a port, because the path is what the sidecar is spawned with
    and what proves at exit that nothing was left behind. JUCE has no Unix domain
    socket — its `StreamingSocket` is TCP and its `NamedPipe` is a pair of FIFOs
    — so this is the POSIX call sequence directly, which is also no dependency at
    all.
*/
class LocalSocketServer
{
public:
    LocalSocketServer() = default;
    ~LocalSocketServer();

    LocalSocketServer (const LocalSocketServer&) = delete;
    LocalSocketServer& operator= (const LocalSocketServer&) = delete;

    /** Binds and listens, removing whatever the path held before. Throws
        `std::runtime_error` when the socket cannot be made.
    */
    void listenAt (const std::filesystem::path& path);

    /** Stops listening and removes the socket file. */
    void close();

    [[nodiscard]] bool listening() const { return socket.valid(); }
    [[nodiscard]] int descriptor() const { return socket.get(); }

    /** The next connection waiting to be accepted, or an invalid descriptor
        when there is none.
    */
    [[nodiscard]] UniqueFd accept() const;

private:
    UniqueFd socket;
    std::filesystem::path socketPath;
};
} // namespace duet::collab

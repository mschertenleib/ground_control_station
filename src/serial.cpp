#include "serial.hpp"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <cstdint>
#include <system_error>

namespace
{

void set_raw_8n1(termios &tio) noexcept
{
    // Raw mode baseline
    ::cfmakeraw(&tio);
    // 8 data bits
    tio.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    tio.c_cflag |= CS8;
    // Ignore modem ctrl, enable receiver
    tio.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    // No parity, 1 stop
    tio.c_cflag &= static_cast<tcflag_t>(~(PARENB | PARODD | CSTOPB));
#ifdef CRTSCTS
    // No HW flow control
    tio.c_cflag &= ~CRTSCTS;
#endif
    // No SW flow control
    tio.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF | IXANY));

    // Blocking read: return as soon as 1 byte is available.
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
}

[[nodiscard]] constexpr speed_t baudrate_to_speed(int baud) noexcept
{
    switch (baud)
    {
    case 0: return B0;
    case 50: return B50;
    case 75: return B75;
    case 110: return B110;
    case 134: return B134;
    case 150: return B150;
    case 200: return B200;
    case 300: return B300;
    case 600: return B600;
    case 1200: return B1200;
    case 1800: return B1800;
    case 2400: return B2400;
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
#ifdef B57600
    case 57600: return B57600;
#endif
#ifdef B115200
    case 115200: return B115200;
#endif
#ifdef B230400
    case 230400: return B230400;
#endif
#ifdef B460800
    case 460800: return B460800;
#endif
#ifdef B921600
    case 921600: return B921600;
#endif
#ifdef B1000000
    case 1000000: return B1000000;
#endif
#ifdef B1152000
    case 1152000: return B1152000;
#endif
#ifdef B1500000
    case 1500000: return B1500000;
#endif
#ifdef B2000000
    case 2000000: return B2000000;
#endif
#ifdef B2500000
    case 2500000: return B2500000;
#endif
#ifdef B3000000
    case 3000000: return B3000000;
#endif
#ifdef B3500000
    case 3500000: return B3500000;
#endif
#ifdef B4000000
    case 4000000: return B4000000;
#endif
    default: return 0;
    }
}

} // namespace

void File_descriptor_deleter::operator()(int fd) const
{
    ::close(fd);
}

// FIXME: many of these error cases should be handled with std::expected
void Serial_device::open(std::string_view name, int baudrate)
{
    // FIXME
    if (handle)
    {
        close();
    }

    // FIXME: why the conversion to string ?
    // Open file descriptor
    const auto fd =
        ::open(std::string(name).c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
    {
        throw std::system_error(errno, std::generic_category(), "open");
    }
    handle.reset(fd);

    // Make it synchronous
    const auto flags = ::fcntl(handle.get(), F_GETFL, 0);
    if (flags == -1 ||
        ::fcntl(handle.get(), F_SETFL, flags & ~O_NONBLOCK) == -1)
    {
        throw std::system_error(
            errno, std::generic_category(), "fcntl(F_SETFL)");
    }

    // Close-on-exec (portable via fcntl).
    const auto clo = ::fcntl(handle.get(), F_GETFD, 0);
    if (clo == -1 || ::fcntl(handle.get(), F_SETFD, clo | FD_CLOEXEC) == -1)
    {
        throw std::system_error(
            errno, std::generic_category(), "fcntl(FD_CLOEXEC)");
    }

    // Configure termios
    termios tio {};
    if (::tcgetattr(handle.get(), &tio) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "tcgetattr");
    }

    set_raw_8n1(tio);

    const auto sp = baudrate_to_speed(baudrate);
    if (sp == 0)
    {
        throw std::system_error(EINVAL,
                                std::generic_category(),
                                "Unsupported baud for this platform");
    }

    if (::cfsetispeed(&tio, sp) == -1 || ::cfsetospeed(&tio, sp) == -1)
    {
        throw std::system_error(
            errno, std::generic_category(), "cfset[io]speed");
    }

    // Flush both input and output queues, then apply now
    if (::tcflush(handle.get(), TCIOFLUSH) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "tcflush");
    }
    if (::tcsetattr(handle.get(), TCSANOW, &tio) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "tcsetattr");
    }
}

void Serial_device::close()
{
    handle.reset();
}

bool Serial_device::is_open() const
{
    return static_cast<bool>(handle);
}

std::size_t Serial_device::write_all(const void *data, std::size_t len)
{
    if (!handle)
    {
        throw std::system_error(
            EBADF, std::generic_category(), "SerialPort not open");
    }

    const auto p = static_cast<const std::uint8_t *>(data);
    std::size_t total {0};
    while (total < len)
    {
        const auto n = ::write(handle.get(), p + total, len - total);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "write");
        }
        total += static_cast<std::size_t>(n);
    }
    if (::tcdrain(handle.get()) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "tcdrain");
    }

    return total;
}

std::size_t Serial_device::read_some(void *buf,
                                     std::size_t maxlen,
                                     std::chrono::milliseconds timeout)
{
    if (!handle)
    {
        throw std::system_error(
            EBADF, std::generic_category(), "SerialPort not open");
    }

    if (timeout.count() >= 0)
    {
        pollfd pfd {.fd = handle.get(), .events = POLLIN, .revents = 0};
        for (;;)
        {
            const auto r = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
            if (r > 0)
            {
                break;
            }
            if (r == 0)
            {
                return 0; // timeout
            }
            if (errno == EINTR)
            {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "poll");
        }
    }
    for (;;)
    {
        const auto n = ::read(handle.get(), buf, maxlen);
        if (n >= 0)
        {
            return static_cast<std::size_t>(n);
        }
        if (errno == EINTR)
        {
            continue;
        }
        throw std::system_error(errno, std::generic_category(), "read");
    }
}

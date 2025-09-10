#include "serial.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <system_error>

namespace
{

[[nodiscard]] constexpr speed_t baudrate_to_speed(int baudrate) noexcept
{
    switch (baudrate)
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

void File_descriptor_flushing_deleter::operator()(int fd) const
{
    ::tcflush(fd, TCIOFLUSH);
    ::close(fd);
}

Serial_port::~Serial_port()
{
    // FIXME: proper rule of 5 probably necessary?
    close();
}

void Serial_port::open(const std::string &path, int baudrate)
{
    // FIXME ?
    if (m_serial_fd)
    {
        close();
    }

    const auto serial_fd =
        ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (serial_fd < 0)
    {
        throw std::system_error(errno, std::generic_category(), "open");
    }
    m_serial_fd.reset(serial_fd);

    termios tio {};
    if (::tcgetattr(m_serial_fd.get(), &tio) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "tcgetattr");
    }

    ::cfmakeraw(&tio);
    // Ignore modem control, enable receiver
    tio.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    // 8 data bits
    tio.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    tio.c_cflag |= static_cast<tcflag_t>(CS8);
    // No parity, 1 stop
    tio.c_cflag &= static_cast<tcflag_t>(~(PARENB | PARODD | CSTOPB));
#ifdef CRTSCTS
    // No hardware flow control
    tio.c_cflag &= ~CRTSCTS;
#endif
    // No software flow control
    tio.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF | IXANY));

    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    const auto speed = baudrate_to_speed(baudrate);
    if (speed == 0)
    {
        throw std::system_error(EINVAL,
                                std::generic_category(),
                                "Unsupported baud for this platform");
    }
    if (::cfsetispeed(&tio, speed) < 0 || ::cfsetospeed(&tio, speed) < 0)
    {
        throw std::system_error(
            errno, std::generic_category(), "cfset[io]speed");
    }

    if (::tcflush(m_serial_fd.get(), TCIOFLUSH) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "tcflush");
    }
    if (::tcsetattr(m_serial_fd.get(), TCSANOW, &tio) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "tcsetattr");
    }

    const auto wake_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wake_fd < 0)
    {
        throw std::system_error(errno, std::generic_category(), "eventfd");
    }
    m_wake_fd.reset(wake_fd);

    m_rx_thread = std::jthread([this](std::stop_token st) { rx_loop(st); });
}

void Serial_port::close()
{
    if (!m_serial_fd)
    {
        return;
    }

    m_rx_thread.request_stop();

    constexpr std::uint64_t one {1};
    ::write(m_wake_fd.get(), &one, sizeof(one));

    m_rx_thread.join();

    m_serial_fd.reset();
    m_wake_fd.reset();
}

bool Serial_port::is_open() const noexcept
{
    return static_cast<bool>(m_serial_fd);
}

void Serial_port::rx_loop(std::stop_token stop_token)
{
    std::array<std::byte, 4096> buffer {};

    while (!stop_token.stop_requested())
    {
        pollfd fds[] {{.fd = m_serial_fd.get(), .events = POLLIN, .revents = 0},
                      {.fd = m_wake_fd.get(), .events = POLLIN, .revents = 0}};

        const auto poll_ret = ::poll(fds, std::size(fds), -1);
        if (poll_ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            // FIXME: notify that we failed
            std::cerr
                << "::poll failed: "
                << std::error_code(errno, std::generic_category()).message()
                << "\n";
            return;
        }

        // We were woken by eventfd
        if (fds[1].revents & POLLIN)
        {
            // Drain the eventfd counter
            std::uint64_t data {};
            while (::read(m_wake_fd.get(), &data, sizeof(data)) == sizeof(data))
            {
            }
            if (stop_token.stop_requested())
            {
                return;
            }
        }

        if (fds[0].revents & POLLNVAL)
        {
            // FIXME: notify that we failed
            std::cerr << "POLLNVAL: Serial FD not open\n";
            return;
        }
        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP))
        {
            while (true)
            {
                const auto num_read =
                    ::read(m_serial_fd.get(), buffer.data(), buffer.size());
                if (num_read > 0)
                {
                    // TODO: Parse packet framing, push complete packet to the
                    // RX queue. We will drain the RX queue from the main thread
                    const std::string message(
                        reinterpret_cast<char *>(buffer.data()),
                        reinterpret_cast<char *>(buffer.data() + num_read));
                    std::cout << message;
                }
                else if (num_read == 0)
                {
                    // No bytes available
                    break;
                }
                else if (num_read < 0 && errno == EAGAIN)
                {
                    // No bytes available
                    break;
                }
                else if (num_read < 0 && errno == EINTR)
                {
                    continue;
                }
                else
                {
                    // FIXME: notify that we failed
                    std::cerr << "::read failed: "
                              << std::error_code(errno, std::generic_category())
                                     .message()
                              << "\n";
                    return;
                }
            }
        }

        if (fds[0].revents & (POLLERR | POLLHUP))
        {
            // FIXME: notify that we failed
            // After draining any remaining bytes:
            std::cerr << "Serial port error/hangup\n";
            return;
        }
    }
}

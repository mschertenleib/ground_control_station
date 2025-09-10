#ifndef SERIAL_HPP
#define SERIAL_HPP

#include "unique_resource.hpp"

#include <string>
#include <thread>

struct File_descriptor_deleter
{
    void operator()(int fd) const;
};

struct File_descriptor_flushing_deleter
{
    void operator()(int fd) const;
};

class Serial_port
{
public:
    ~Serial_port();

    void open(const std::string &name, int baudrate);
    void close();
    [[nodiscard]] bool is_open() const noexcept;

private:
    void rx_loop(std::stop_token stop_token);

    Unique_resource<int, File_descriptor_flushing_deleter> m_serial_fd;
    Unique_resource<int, File_descriptor_deleter> m_wake_fd;
    std::jthread m_rx_thread;
};

#endif

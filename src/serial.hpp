#ifndef SERIAL_HPP
#define SERIAL_HPP

#include "unique_resource.hpp"

#include <chrono>
#include <string_view>

struct File_descriptor_deleter
{
    void operator()(int fd) const;
};

struct Serial_device
{
    void open(std::string_view path, int baudrate);
    void close();

    std::size_t write_all(const void *data, std::size_t len);
    std::size_t read_some(
        void *buf,
        std::size_t maxlen,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

    Unique_resource<int, File_descriptor_deleter> handle;
};

#endif
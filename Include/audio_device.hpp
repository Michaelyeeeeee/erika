#pragma once

#include <string>

class AudioDevice
{
public:
    AudioDevice();

    const std::string &input() const;
    const std::string &output() const;

    const std::string &input_name() const;
    const std::string &output_name() const;

private:
    struct DeviceResult
    {
        std::string device;
        std::string name;
        int score;
    };

    static DeviceResult find_input_device();
    static DeviceResult find_output_device();

    static DeviceResult find_device(
        bool capture);

    std::string input_device_;
    std::string output_device_;

    std::string input_name_;
    std::string output_name_;
};
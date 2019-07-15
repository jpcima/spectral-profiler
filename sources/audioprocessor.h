//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <memory>
struct Basic_Message;

class Audio_Processor {
public:
    Audio_Processor();
    ~Audio_Processor();
    void start();

    unsigned fft_size() const;

    float input_level() const;
    float output_level() const;

    void send_message(const Basic_Message &hmsg);
    Basic_Message *receive_message();

private:
    struct Impl;
    std::unique_ptr<Impl> P;
};

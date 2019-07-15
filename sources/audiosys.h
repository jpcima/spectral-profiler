//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <jack/jack.h>
#include <memory>

class Audio_Sys {
public:
    static Audio_Sys &instance();

private:
    Audio_Sys();

public:
    ~Audio_Sys();
    explicit operator bool() const;

    float sample_rate() const;

    void start(void (*fn)(const float *, float *, unsigned, void *), void *data);
    void stop();

private:
    struct Jack_Deleter {
        void operator()(jack_client_t *x) { jack_client_close(x); }
    };

    std::unique_ptr<jack_client_t, Jack_Deleter> client_;
    jack_port_t *in_ = nullptr;
    jack_port_t *out_ = nullptr;
    void (*cb_fn_)(const float *, float *, unsigned, void *) = nullptr;
    void *cb_data_ = nullptr;

    static int process(jack_nframes_t nframes, void *userdata);
};

//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "audiosys.h"
#include <QCoreApplication>

Audio_Sys &Audio_Sys::instance()
{
    static Audio_Sys sys;
    return sys;
}

Audio_Sys::Audio_Sys()
{
    QCoreApplication *app = QCoreApplication::instance();

    jack_client_t *client = jack_client_open(
        app->applicationName().toUtf8().data(),
        JackNoStartServer, nullptr);
    if (!client)
        return;

    client_.reset(client);

    jack_port_t *in = jack_port_register(client, app->tr("Measurement input").toUtf8().data(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    jack_port_t *out = jack_port_register(client, app->tr("Generator output").toUtf8().data(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (!in || !out) {
        client_.reset();
        return;
    }

    in_ = in;
    out_ = out;

    jack_set_process_callback(client, &process, this);
}

Audio_Sys::~Audio_Sys()
{
}

Audio_Sys::operator bool() const
{
    return client_ != nullptr;
}

float Audio_Sys::sample_rate() const
{
    return jack_get_sample_rate(client_.get());
}

void Audio_Sys::start(void (*fn)(const float *, float *, unsigned, void *), void *data)
{
    jack_client_t *client = client_.get();
    jack_deactivate(client);
    cb_fn_ = fn;
    cb_data_ = data;
    jack_activate(client);
}

void Audio_Sys::stop()
{
    jack_client_t *client = client_.get();
    jack_deactivate(client);
}

int Audio_Sys::process(jack_nframes_t nframes, void *userdata)
{
    Audio_Sys *self = (Audio_Sys *)userdata;

    const float *in = (float *)jack_port_get_buffer(self->in_, nframes);
    float *out = (float *)jack_port_get_buffer(self->out_, nframes);

    if (self->cb_fn_)
        self->cb_fn_(in, out, nframes, self->cb_data_);

    return 0;
}

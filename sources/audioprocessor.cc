//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "audioprocessor.h"
#include "audiosys.h"
#include "analyzerdefs.h"
#include "messages.h"
#include "dsp/amp_follower.h"
#include "utility/nextpow2.h"
#include "utility/ring_buffer.h"
#include <fftw3.h>
#include <algorithm>
#include <thread>
#include <complex>
#include <cassert>
typedef std::complex<float> cfloat;
typedef std::complex<double> cdouble;

struct Audio_Processor::Impl {
    static void process(const float *in, float *out, unsigned n, void *userdata);
    void handle_messages();
    void process_message(const Basic_Message &hmsg);
    void generate(float *out, unsigned n);
    void collect(const float *in, unsigned n);
    void compute_response(cfloat *response);
    void update_levels(const float *in, float *out, unsigned n);

/*
    static cdouble interpolate(const cfloat *in, double pos, unsigned size);
    static double interpolate4(const double *y, double mu);
*/

    Amp_Follower<float> in_amp_follower_;
    Amp_Follower<float> out_amp_follower_;
    float in_amp_ = 0;
    float out_amp_ = 0;

    std::unique_ptr<Ring_Buffer> rb_in_;
    std::unique_ptr<Ring_Buffer> rb_out_;
    std::unique_ptr<uint8_t[]> rb_in_buf_;
    std::unique_ptr<uint8_t[]> rb_out_buf_;

    bool active_ = false;

    bool gen_can_start_ = false;
    bool gen_has_finished_ = false;
    int gen_spl_ = Analysis::Signal_Lo;

    unsigned gen_num_bins_ = 0;
    float gen_freq_[Analysis::max_bins_at_once] = {};
    float gen_phase_[Analysis::max_bins_at_once] = {};
    float gen_starting_phase_[Analysis::max_bins_at_once] = {};
    float gen_gain_compensate_ = 0;

    std::unique_ptr<float[]> out_buf_;
    unsigned out_buf_len_ = 0;
    unsigned out_buf_fill_ = 0;

    struct Fftwf_Deleter {
        void operator()(void *x) { fftwf_free(x); }
    };
    struct Fftwf_Plan_Deleter {
        void operator()(fftwf_plan x) { fftwf_destroy_plan(x); }
    };

    std::unique_ptr<float[], Fftwf_Deleter> fft_real_;
    std::unique_ptr<cfloat[], Fftwf_Deleter> fft_cplx_;
    std::unique_ptr<fftwf_plan_s, Fftwf_Plan_Deleter> fft_plan_;
};

Audio_Processor::Audio_Processor()
    : P(new Impl)
{
    const float sr = Analysis::sample_rate;

    P->in_amp_follower_.release(50e-3f * sr);
    P->out_amp_follower_.release(50e-3f * sr);

    P->rb_in_.reset(new Ring_Buffer(8192));
    P->rb_out_.reset(new Ring_Buffer(8192));
    P->rb_in_buf_.reset(Messages::allocate_buffer());
    P->rb_out_buf_.reset(Messages::allocate_buffer());

    const unsigned fft_size = nextpow2(std::ceil(0.5f * sr));

    P->out_buf_len_ = fft_size;
    P->out_buf_.reset(new float[fft_size]);

    P->fft_real_.reset(fftwf_alloc_real(fft_size));
    P->fft_cplx_.reset((cfloat *)fftwf_alloc_complex(fft_size / 2 + 1));
    if (!P->fft_real_ || !P->fft_cplx_)
        throw std::bad_alloc();

    P->fft_plan_.reset(fftwf_plan_dft_r2c_1d(fft_size, P->fft_real_.get(), (fftwf_complex *)P->fft_cplx_.get(), FFTW_MEASURE));
    if (!P->fft_plan_)
        throw std::bad_alloc();
}

Audio_Processor::~Audio_Processor()
{
}

void Audio_Processor::start()
{
    Audio_Sys &sys = Audio_Sys::instance();
    sys.start(&Impl::process, this);
}

unsigned Audio_Processor::fft_size() const
{
    return P->out_buf_len_;
}

float Audio_Processor::input_level() const
{
    return P->in_amp_;
}

float Audio_Processor::output_level() const
{
    return P->out_amp_;
}

void Audio_Processor::send_message(const Basic_Message &hmsg)
{
    Ring_Buffer &rb = *P->rb_in_;
    while (!rb.put((uint8_t *)&hmsg, Messages::size_of(hmsg.tag)))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

Basic_Message *Audio_Processor::receive_message()
{
    Ring_Buffer &rb = *P->rb_out_;
    Basic_Message *msg = (Basic_Message *)P->rb_out_buf_.get();

    if (!rb.peek(*msg))
        return nullptr;

    size_t size = Messages::size_of(msg->tag);
    if (rb.size_used() < size)
        return nullptr;

    rb.get((uint8_t *)msg, size);
    return msg;
}

void Audio_Processor::Impl::process(const float *in, float *out, unsigned n, void *userdata)
{
    Audio_Processor *self = (Audio_Processor *)userdata;
    Impl *P = self->P.get();

    std::fill_n(out, n, 0);

    P->handle_messages();

    if (P->active_) {
        if (P->gen_can_start_) {
            P->collect(in, n);
            if (!P->gen_has_finished_ && P->out_buf_fill_ == P->out_buf_len_) {
                Ring_Buffer &rb_out = *P->rb_out_;
                Messages::NotifyFrequencyAnalysis msg;
                if (sizeof(msg) < rb_out.size_free()) {
                    msg.spl = P->gen_spl_;
                    msg.num_bins = P->gen_num_bins_;
                    P->compute_response(msg.response);
                    for (unsigned a = 0; a < msg.num_bins; ++a)
                        msg.frequency[a] = P->gen_freq_[a] * Analysis::sample_rate;
                    rb_out.put(msg);
                    P->gen_has_finished_ = true;
                }
            }
        }

        if (!P->gen_can_start_ && P->out_amp_ < Analysis::silence_threshold) {
            P->gen_can_start_ = true;
            for (unsigned a = 0, num_bins = P->gen_num_bins_; a < num_bins; ++a)
                P->gen_starting_phase_[a] = P->gen_phase_[a];
        }

        if (P->gen_can_start_)
            P->generate(out, n);
    }

    P->update_levels(in, out, n);
}

void Audio_Processor::Impl::handle_messages()
{
    Ring_Buffer &rb_in = *rb_in_;
    Basic_Message *hmsg = (Basic_Message *)rb_in_buf_.get();
    while (rb_in.peek(*hmsg)) {
        size_t size = Messages::size_of(hmsg->tag);
        if (rb_in.size_used() < size)
            break;
        rb_in.get((uint8_t *)hmsg, size);
        process_message(*hmsg);
    }
}

void Audio_Processor::Impl::process_message(const Basic_Message &hmsg)
{
    float sr = Analysis::sample_rate;
    unsigned fft_size = out_buf_len_;

    switch (hmsg.tag) {
    case Message_Tag::RequestAnalyzeFrequency: {
        auto *msg = (Messages::RequestAnalyzeFrequency *)&hmsg;
        active_ = true;
        gen_can_start_ = false;
        gen_has_finished_ = false;
        gen_spl_ = msg->spl;
        unsigned num_bins = gen_num_bins_ = msg->num_bins;
        for (unsigned a = 0; a < num_bins; ++a) {
            unsigned bin = std::lround(fft_size * msg->frequency[a] / sr);
            bin = std::min(bin, fft_size / 2);
            gen_freq_[a] = (float)bin / fft_size;
            gen_phase_[a] = 0;
            gen_starting_phase_[a] = 0;
        }
        out_buf_fill_ = 0;

        // compensate for level increase caused by sum of sines
        float rms_single = M_SQRT1_2;
        float rms_sum = sqrt((M_SQRT1_2 * M_SQRT1_2) * num_bins);
        gen_gain_compensate_ = rms_single / rms_sum;

        break;
    }
    case Message_Tag::RequestStop:
        active_ = false;
        break;
    default:
        assert(false);
        break;
    }
}

void Audio_Processor::Impl::generate(float *out, unsigned n)
{
    const float amp = Analysis::global_amplitude(gen_spl_);

    for (unsigned i = 0; i < n; ++i)
        out[i] = 0;

    unsigned num_bins = gen_num_bins_;
    for (unsigned a = 0; a < num_bins; ++a) {
        const float f = gen_freq_[a];
        float p = gen_phase_[a];
        for (unsigned i = 0; i < n; ++i) {
            out[i] += amp * std::cos(2 * (float)M_PI * p);
            p += f;
            p -= (int)p;
        }
        gen_phase_[a] = p;
    }

    //
    const float comp = gen_gain_compensate_;
    for (unsigned i = 0; i < n; ++i)
        out[i] *= comp;
}

void Audio_Processor::Impl::collect(const float *in, unsigned n)
{
    float *buf = out_buf_.get();
    const unsigned len = out_buf_len_;
    unsigned fill = out_buf_fill_;

    n = std::min(n, len - fill);
    for (unsigned i = 0; i < n; ++i)
        buf[fill++] = in[i];

    out_buf_fill_ = fill;
}

void Audio_Processor::Impl::compute_response(cfloat *response)
{
    const unsigned n = out_buf_len_;

    const float *raw = out_buf_.get();
    float *real = fft_real_.get();
    cfloat *cplx = fft_cplx_.get();

    for (unsigned i = 0; i < n; ++i) {
        float w = 0.5f * (1 - std::cos((2 * (float)M_PI * i) / (n - 1)));
        real[i] = raw[i] * w;
    }

    fftwf_execute(fft_plan_.get());

    unsigned num_bins = gen_num_bins_;
    for (unsigned a = 0; a < num_bins; ++a) {
        const float f = gen_freq_[a];
        unsigned bin = std::lround(n * f);
        cfloat h_out = cplx[bin] * 4.0f / (float)n;
        cfloat h_in = std::polar(
            (float)Analysis::global_amplitude(gen_spl_) * gen_gain_compensate_,
            2 * (float)M_PI * gen_starting_phase_[a]);
        response[a] = h_out / h_in;
    }
}

void Audio_Processor::Impl::update_levels(const float *in, float *out, unsigned n)
{
    float in_amp = in_amp_;
    float out_amp = out_amp_;

    for (unsigned i = 0; i < n; ++i) {
        in_amp = in_amp_follower_.process(in[i]);
        out_amp = out_amp_follower_.process(out[i]);
    };

    in_amp_ = in_amp;
    out_amp_ = out_amp;
}

/*
cdouble Audio_Processor::Impl::interpolate(const cfloat *in, double pos, unsigned size)
{
    unsigned i0 = (unsigned)pos;
    double mu = pos - i0;

    double x[4], y[4];
    for (unsigned i = 0; i < 4; ++i) {
        cdouble v = (i0 + i < size) ? in[i0 + i] : 0.0;
        x[i] = v.real();
        y[i] = v.imag();
    }

    return cdouble(interpolate4(x, mu), interpolate4(y, mu));
}

double Audio_Processor::Impl::interpolate4(const double *y, double mu)
{
    double a0, a1, a2, a3;

    if (0) {
        a0 = y[3] - y[2] - y[0] + y[1];
        a1 = y[0] - y[1] - a0;
        a2 = y[2] - y[0];
        a3 = y[1];
    }
    else {
        a0 = -0.5 * y[0] + 1.5 * y[1] - 1.5 * y[2] + 0.5 * y[3];
        a1 = y[0] - 2.5 * y[1] + 2.0 * y[2] - 0.5 * y[3];
        a2 = -0.5 * y[0] + 0.5 * y[2];
        a3 = y[1];
    }

    double mu2 = mu * mu;
    return a0 * mu2 * mu + a1 * mu2 + a2 * mu + a3;
}
*/

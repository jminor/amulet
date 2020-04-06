#include "amulet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <math.h>
#define SUNVOX_MAIN
#include "../third_party/sunvox_dll/headers/sunvox.h"
static float *sunvox_buffer = NULL;
static int sunvox_buffer_size = 0;

#define AM_AUDIO_NODE_FLAG_MARK             ((uint32_t)1)
#define AM_AUDIO_NODE_FLAG_CHILDREN_DIRTY   ((uint32_t)2)
#define AM_AUDIO_NODE_FLAG_PENDING_PAUSE    ((uint32_t)4)
#define AM_AUDIO_NODE_MASK_LIVE_PAUSE_STATE ((uint32_t)(8+16))

#define node_marked(node)           (node->flags & AM_AUDIO_NODE_FLAG_MARK)
#define mark_node(node)             node->flags |= AM_AUDIO_NODE_FLAG_MARK
#define unmark_node(node)           node->flags &= ~AM_AUDIO_NODE_FLAG_MARK

#define children_dirty(node)        (node->flags & AM_AUDIO_NODE_FLAG_CHILDREN_DIRTY)
#define set_children_dirty(node)    node->flags |= AM_AUDIO_NODE_FLAG_CHILDREN_DIRTY
#define clear_children_dirty(node)  node->flags &= ~AM_AUDIO_NODE_FLAG_CHILDREN_DIRTY

#define LIVE_PAUSE_STATE_UNPAUSED   (0 << 3)
#define LIVE_PAUSE_STATE_BEGIN      (1 << 3)
#define LIVE_PAUSE_STATE_PAUSED     (2 << 3) 
#define LIVE_PAUSE_STATE_END        (3 << 3)

#define live_pause_state(node)      (node->flags & AM_AUDIO_NODE_MASK_LIVE_PAUSE_STATE)
#define set_live_pause_state(node, state) {node->flags &= ~AM_AUDIO_NODE_MASK_LIVE_PAUSE_STATE; node->flags |= state;}

#define pending_pause(node)         (node->flags & AM_AUDIO_NODE_FLAG_PENDING_PAUSE)
#define set_pending_pause(node)     node->flags |= AM_AUDIO_NODE_FLAG_PENDING_PAUSE
#define clear_pending_pause(node)   node->flags &= ~AM_AUDIO_NODE_FLAG_PENDING_PAUSE

static am_audio_context audio_context;

// Audio Bus

// all buffers in the pool have the same size
// current_pool_bufsize is the size of each buffer in the
// pool, in bytes.
static std::vector<void*> buffer_pool;
static int current_pool_bufsize = 0;
static unsigned int bufpool_top = 0;

static double audio_time_accum = 0.0;

static void clear_buffer_pool() {
    for (unsigned int i = 0; i < buffer_pool.size(); i++) {
        free(buffer_pool[i]);
    }
    buffer_pool.clear();
    bufpool_top = 0;
    current_pool_bufsize = 0;
}

static void* push_buffer(int size) {
    if (size != current_pool_bufsize) {
        // size of audio buffer has changed, clear pool
        clear_buffer_pool();
        current_pool_bufsize = size;
    }
    am_always_assert(bufpool_top <= buffer_pool.size());
    if (bufpool_top == buffer_pool.size()) {
        buffer_pool.push_back(malloc(size));
    }
    void *buf = buffer_pool[bufpool_top++];
    memset(buf, 0, size);
    return buf;
}

static void pop_buffer(void *buf) {
    bufpool_top--;
    assert(bufpool_top >= 0);
    assert(buffer_pool.size() > bufpool_top);
    assert(buffer_pool[bufpool_top] == buf);
}

static void setup_channels(am_audio_bus *bus) {
    for (int i = 0; i < bus->num_channels; i++) {
        bus->channel_data[i] = bus->buffer + i * bus->num_samples;
    }
}

am_audio_bus::am_audio_bus(int nchannels, int nsamples, float* buf) {
    num_channels = nchannels;
    num_samples = nsamples;
    buffer = buf;
    owns_buffer = false;
    setup_channels(this);
}

am_audio_bus::am_audio_bus(am_audio_bus *bus) {
    num_channels = bus->num_channels;
    num_samples = bus->num_samples;
    buffer = (float*)push_buffer(num_channels * num_samples * sizeof(float));
    owns_buffer = true;
    setup_channels(this);
}

am_audio_bus::~am_audio_bus() {
    if (owns_buffer) {
        pop_buffer(buffer);
    }
}

// Audio Param

static float linear_incf(am_audio_param<float> param, int samples) {
    return (param.target_value - param.current_value) / (float)samples;
}

// Audio Node

am_audio_node::am_audio_node() {
    pending_children.owner = this;
    live_children.owner = this;
    last_sync = 0;
    last_render = 0;
    flags = 0;
    recursion_limit = 0;
}

void am_audio_node::sync_params() {
}

void am_audio_node::post_render(am_audio_context *context, int num_samples) {
}

void am_audio_node::render_audio(am_audio_context *context, am_audio_bus *bus) {
    render_children(context, bus);
}

bool am_audio_node::finished() {
    for (int i = 0; i < pending_children.size; i++) {
        am_audio_node_child *child = &pending_children.arr[i];
        if (!child->child->finished()) return false;
    }
    return true;
}

static void mix_bus(am_audio_bus * AM_RESTRICT dest, am_audio_bus * AM_RESTRICT src) {
    for (int c = 0; c < am_min(dest->num_channels, src->num_channels); c++) {
        int n = am_min(dest->num_samples, src->num_samples);
        float * AM_RESTRICT dest_data = dest->channel_data[c];
        float * AM_RESTRICT src_data = src->channel_data[c];
        for (int s = 0; s < n; s++) {
            dest_data[s] += src_data[s];
        }
    }
}

static void apply_fadein(am_audio_bus *bus) {
    int n = am_conf_audio_interpolate_samples;
    float inc = 1.0f / (float)n;
    for (int c = 0; c < bus->num_channels; c++) {
        float *data = bus->channel_data[c];
        float scale = 0.0f;
        for (int s = 0; s < n; s++) {
            data[s] *= scale;
            scale += inc;
        }
    }
}

static void apply_fadeout(am_audio_bus *bus) {
    int n = am_conf_audio_interpolate_samples;
    int start = bus->num_samples - n;
    int end = bus->num_samples;
    float inc = -1.0f / (float)n;
    for (int c = 0; c < bus->num_channels; c++) {
        float *data = bus->channel_data[c];
        float scale = 1.0f;
        for (int s = start; s < end; s++) {
            data[s] *= scale;
            scale += inc;
        }
    }
}

void am_audio_node::render_children(am_audio_context *context, am_audio_bus *bus) {
    if (recursion_limit < 0) return;
    recursion_limit--;
    for (int i = 0; i < live_children.size; i++) {
        am_audio_node_child *child = &live_children.arr[i];
        int pause_state = live_pause_state(child->child);
        am_audio_node_child_state child_state = child->state;
        if (child_state == AM_AUDIO_NODE_CHILD_STATE_OLD
            && pause_state == LIVE_PAUSE_STATE_UNPAUSED)
        {
            child->child->render_audio(context, bus);
        } else if (child_state == AM_AUDIO_NODE_CHILD_STATE_DONE
            || pause_state == LIVE_PAUSE_STATE_PAUSED
            // also ignore if paused and added at same time...
            || (pause_state == LIVE_PAUSE_STATE_BEGIN && child->state == AM_AUDIO_NODE_CHILD_STATE_NEW)
            // ...or if unpaused and removed at same time
            || (pause_state == LIVE_PAUSE_STATE_END && child->state == AM_AUDIO_NODE_CHILD_STATE_REMOVED))
        {
            // ignore
        } else {
            // a fadein or fadeout is required, because
            // the child was recently added/removed or
            // paused/unpaused.
            am_audio_bus tmp(bus);
            child->child->render_audio(context, &tmp);
            if (child_state == AM_AUDIO_NODE_CHILD_STATE_NEW || pause_state == LIVE_PAUSE_STATE_END) {
                apply_fadein(&tmp);
            } else if (child_state == AM_AUDIO_NODE_CHILD_STATE_REMOVED || pause_state == LIVE_PAUSE_STATE_BEGIN) {
                apply_fadeout(&tmp);
            } else {
                assert(false);
            }
            mix_bus(bus, &tmp);
        }
    }
    recursion_limit++;
}

// Gain Node

am_gain_node::am_gain_node() : gain(0) {
    gain.pending_value = 1.0f;
}

void am_gain_node::sync_params() {
    gain.update_target();
}

void am_gain_node::post_render(am_audio_context *context, int num_samples) {
    gain.update_current();
}

void am_gain_node::render_audio(am_audio_context *context, am_audio_bus *bus) {
    am_audio_bus tmp(bus);
    render_children(context, &tmp);
    int num_channels = bus->num_channels;
    int num_samples = bus->num_samples;
    for (int s = 0; s < num_samples; s++) {
        for (int c = 0; c < num_channels; c++) {
            bus->channel_data[c][s] += tmp.channel_data[c][s] * gain.interpolate_linear(s);
        }
    }
}

// Biquad filters. Most code here adapted from http://www.chromium.org/blink

am_biquad_filter_node::am_biquad_filter_node() {
    for (int c = 0; c < AM_MAX_CHANNELS; c++) {
        memset(&current_state[c], 0, sizeof(am_biquad_filter_state));
        next_state[c] = current_state[c];
    }
}

void am_biquad_filter_node::post_render(am_audio_context *context, int num_samples) {
    for (int i = 0; i < AM_MAX_CHANNELS; i++) {
        current_state[i] = next_state[i];
    }
}

void am_biquad_filter_node::render_audio(am_audio_context *context, am_audio_bus *bus) {
    am_audio_bus tmp(bus);
    render_children(context, &tmp);
    int num_channels = bus->num_channels;
    int num_samples = bus->num_samples;
    for (int c = 0; c < num_channels; c++) {
        float *source = &tmp.channel_data[c][0];
        float *dest = &bus->channel_data[c][0];
        int n = num_samples;

        double x1 = current_state[c].x1;
        double x2 = current_state[c].x2;
        double y1 = current_state[c].y1;
        double y2 = current_state[c].y2;

        double b0 = coeffs.b0;
        double b1 = coeffs.b1;
        double b2 = coeffs.b2;
        double a1 = coeffs.a1;
        double a2 = coeffs.a2;

        while (n--) {
            float x = *source++;
            float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;

            *dest++ += y;

            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
        }

        next_state[c].x1 = x1;
        next_state[c].x2 = x2;
        next_state[c].y1 = y1;
        next_state[c].y2 = y2;
    }
}

void am_biquad_filter_node::set_lowpass_params(double cutoff, double resonance) {
    // normalize cutoff 0->1
    double nyquist = (double)am_conf_audio_sample_rate * 0.5;
    cutoff = cutoff / nyquist;
    cutoff = am_clamp(cutoff, 0.0, 1.0);
    if (cutoff == 1) {
        // When cutoff is 1, the z-transform is 1.
        set_normalized_coeffs(1, 0, 0,
                              1, 0, 0);
    } else if (cutoff > 0) {
        // Compute biquad coefficients for lowpass filter
        resonance = am_max(0.0, resonance); // can't go negative
        double g = pow(10.0, 0.05 * resonance);
        double d = sqrt((4 - sqrt(16 - 16 / (g * g))) / 2);

        double theta = AM_PI * cutoff;
        double sn = 0.5 * d * sin(theta);
        double beta = 0.5 * (1 - sn) / (1 + sn);
        double gamma = (0.5 + beta) * cos(theta);
        double alpha = 0.25 * (0.5 + beta - gamma);

        double b0 = 2 * alpha;
        double b1 = 2 * 2 * alpha;
        double b2 = 2 * alpha;
        double a1 = 2 * -gamma;
        double a2 = 2 * beta;

        set_normalized_coeffs(b0, b1, b2, 1, a1, a2);
    } else {
        // When cutoff is zero, nothing gets through the filter, so set
        // coefficients up correctly.
        set_normalized_coeffs(0, 0, 0,
                              1, 0, 0);
    }
}

void am_biquad_filter_node::set_highpass_params(double cutoff, double resonance) {
    // normalize cutoff 0->1
    double nyquist = (double)am_conf_audio_sample_rate * 0.5;
    cutoff = cutoff / nyquist;
    cutoff = am_clamp(cutoff, 0.0, 1.0);
    if (cutoff == 1) {
        // The z-transform is 0.
        set_normalized_coeffs(0, 0, 0,
                              1, 0, 0);
    } else if (cutoff > 0) {
        // Compute biquad coefficients for highpass filter
        resonance = am_max(0.0, resonance); // can't go negative
        double g = pow(10.0, 0.05 * resonance);
        double d = sqrt((4 - sqrt(16 - 16 / (g * g))) / 2);

        double theta = AM_PI * cutoff;
        double sn = 0.5 * d * sin(theta);
        double beta = 0.5 * (1 - sn) / (1 + sn);
        double gamma = (0.5 + beta) * cos(theta);
        double alpha = 0.25 * (0.5 + beta + gamma);

        double b0 = 2 * alpha;
        double b1 = 2 * -2 * alpha;
        double b2 = 2 * alpha;
        double a1 = 2 * -gamma;
        double a2 = 2 * beta;

        set_normalized_coeffs(b0, b1, b2, 1, a1, a2);
    } else {
        // When cutoff is zero, we need to be careful because the above
        // gives a quadratic divided by the same quadratic, with poles
        // and zeros on the unit circle in the same place. When cutoff
        // is zero, the z-transform is 1.
        set_normalized_coeffs(1, 0, 0,
                              1, 0, 0);
    }
}

void am_biquad_filter_node::set_normalized_coeffs(double b0, double b1, double b2, double a0, double a1, double a2) {
    double a0_inv = 1.0 / a0;
    coeffs.b0 = b0 * a0_inv;
    coeffs.b1 = b1 * a0_inv;
    coeffs.b2 = b2 * a0_inv;
    coeffs.a1 = a1 * a0_inv;
    coeffs.a2 = a2 * a0_inv;
}

am_lowpass_filter_node::am_lowpass_filter_node() : cutoff(0), resonance(0) {
}

void am_lowpass_filter_node::sync_params() {
    cutoff.update_target();
    resonance.update_target();
    if (cutoff.current_value != cutoff.target_value
        || resonance.current_value != resonance.target_value)
    {
        cutoff.update_current();
        resonance.update_current();
        set_lowpass_params(cutoff.current_value, resonance.current_value);
    }
}

am_highpass_filter_node::am_highpass_filter_node() : cutoff(0), resonance(0) {
}

void am_highpass_filter_node::sync_params() {
    cutoff.update_target();
    resonance.update_target();
    if (cutoff.current_value != cutoff.target_value
        || resonance.current_value != resonance.target_value)
    {
        cutoff.update_current();
        resonance.update_current();
        set_highpass_params(cutoff.current_value, resonance.current_value);
    }
}

// Audio track node

am_audio_track_node::am_audio_track_node() 
    : playback_speed(1.0f), gain(1.0f)
{
    audio_buffer = NULL;
    audio_buffer_ref = LUA_NOREF;
    current_position = 0.0;
    next_position = 0.0;
    reset_position = 0.0;
    loop = false;
    needs_reset = false;
    done_server = false;
    done_client = false;
}

void am_audio_track_node::sync_params() {
    playback_speed.update_target();
    gain.update_target();
    if (needs_reset) {
        current_position = reset_position;
        next_position = reset_position;
        needs_reset = false;
        done_server = false;
    }
    done_client = done_server;
}

static bool track_resample_required(am_audio_track_node *node) {
    return (node->audio_buffer->sample_rate != am_conf_audio_sample_rate)
        || (node->playback_speed.current_value != node->playback_speed.target_value)
        || (fabs(node->playback_speed.current_value - 1.0f) > 0.00001f);
}

static bool is_too_slow(float playback_speed) {
    return playback_speed < 0.00001f;
}

void am_audio_track_node::render_audio(am_audio_context *context, am_audio_bus *bus) {
    if (done_server) return;
    if (is_too_slow(playback_speed.current_value)) return;
    if (audio_buffer->buffer->data == NULL) return;
    am_audio_bus tmp(bus);
    int buf_num_channels = audio_buffer->num_channels;
    int buf_num_samples = audio_buffer->buffer->size / (buf_num_channels * sizeof(float));
    int bus_num_samples = tmp.num_samples;
    int bus_num_channels = tmp.num_channels;
    if (!track_resample_required(this)) {
        // optimise common case where no resampling is required
        for (int c = 0; c < bus_num_channels; c++) {
            float *bus_data = tmp.channel_data[c];
            float *buf_data = ((float*)audio_buffer->buffer->data) + c * buf_num_samples;
            if (c < buf_num_channels) {
                int buf_pos = (int)floor(current_position);
                assert(buf_pos < buf_num_samples);
                for (int bus_pos = 0; bus_pos < bus_num_samples; bus_pos++) {
                    bus_data[bus_pos] += buf_data[buf_pos++] * gain.interpolate_linear(bus_pos);
                    if (buf_pos >= buf_num_samples) {
                        if (loop) {
                            buf_pos = 0;
                        } else {
                            done_server = true;
                            break;
                        }
                    }
                }
            } else {
                // less channels in buffer than bus, so duplicate previous channels
                assert(c > 0);
                memcpy(bus_data, tmp.channel_data[c-1], bus_num_samples * sizeof(float));
            }
        }
        next_position = current_position + (double)bus_num_samples;
        if (next_position >= (double)buf_num_samples) {
            next_position = fmod(next_position, (double)buf_num_samples);
        }
    } else {
        // resample
        for (int c = 0; c < bus_num_channels; c++) {
            float *bus_data = tmp.channel_data[c];
            float *buf_data = ((float*)audio_buffer->buffer->data) + c * buf_num_samples;
            if (c < buf_num_channels) {
                double pos = current_position;
                for (int write_index = 0; write_index < bus_num_samples; write_index++) {
                    int read_index1 = (int)floor(pos);
                    int read_index2 = read_index1 + 1;
                    if (read_index2 >= buf_num_samples) {
                        if (loop) {
                            read_index2 = 0;
                        } else {
                            done_server = true;
                            break;
                        }
                    }
                    double interpolation_factor = pos - (double)read_index1;
                    float sample1 = buf_data[read_index1];
                    float sample2 = buf_data[read_index2];
                    float interpolated_sample = (1.0 - interpolation_factor) * sample1 + interpolation_factor * sample2;
                    bus_data[write_index] += interpolated_sample * gain.interpolate_linear(write_index);
                    pos += playback_speed.interpolate_linear(write_index) * sample_rate_ratio;
                    if (pos >= (double)buf_num_samples) {
                        if (loop) {
                            pos = fmod(pos, (double)buf_num_samples);
                        } else {
                            done_server = true;
                            break;
                        }
                    }
                }
                next_position = pos;
            } else {
                // less channels in buffer than bus, so duplicate previous channels
                assert(c > 0);
                memcpy(bus_data, tmp.channel_data[c-1], bus_num_samples * sizeof(float));
            }
        }
    }
    mix_bus(bus, &tmp);
}

void am_audio_track_node::post_render(am_audio_context *context, int num_samples) {
    playback_speed.update_current();
    gain.update_current();
    current_position = next_position;
}

bool am_audio_track_node::finished() {
    return done_client;
}

// Audio stream node

am_audio_stream_node::am_audio_stream_node()
    : playback_speed(1.0f)
{
    buffer = NULL;
    buffer_ref = LUA_NOREF;
    handle = NULL;
    num_channels = 2;
    sample_rate = 44100;
    sample_rate_ratio = 1.0f;
    loop = false;
    done_server = false;
    done_client = false;
    current_position = 0.0;
    next_position = 0.0;
}

void am_audio_stream_node::sync_params() {
    playback_speed.update_target();
    done_client = done_server;
}

void am_audio_stream_node::render_audio(am_audio_context *context, am_audio_bus *bus) {
    if (done_server) return;
    int bus_num_samples = bus->num_samples;
    int bus_num_channels = bus->num_channels;
    stb_vorbis *f = (stb_vorbis*)handle;
    int n = 0;
    float *channel_data[AM_MAX_CHANNELS];
    am_audio_bus tmp(bus);
    for (int i = 0; i < bus_num_channels; i++) {
        channel_data[i] = tmp.channel_data[i];
    }
    while (n < bus_num_samples) {
        int m = stb_vorbis_get_samples_float(f, bus_num_channels, channel_data, bus_num_samples - n);
        if (m == 0) {
            if (loop) {
                stb_vorbis_seek_start(f);
            } else {
                done_server = true;
                break;
            }
        } else {
            n += m;
            for (int i = 0; i < bus_num_channels; i++) {
                channel_data[i] += m;
            }
        }
    }
    // fill in missing channels
    for (int c = num_channels; c < bus_num_channels; c++) {
        assert(c > 0);
        memcpy(tmp.channel_data[c], tmp.channel_data[c-1], bus_num_samples * sizeof(float));
    }
    mix_bus(bus, &tmp);
}

void am_audio_stream_node::post_render(am_audio_context *context, int num_samples) {
    playback_speed.update_current();
    current_position = next_position;
}

bool am_audio_stream_node::finished() {
    return done_client;
}

// Oscillator Node

am_oscillator_node::am_oscillator_node()
    : phase(0)
    , freq(440)
    , waveform(AM_WAVEFORM_SINE)
{
    offset = 0;
}

void am_oscillator_node::sync_params() {
    phase.update_target();
    freq.update_target();
}

void am_oscillator_node::post_render(am_audio_context *context, int num_samples) {
    phase.update_current();
    freq.update_current();
    offset += num_samples;
}

void am_oscillator_node::render_audio(am_audio_context *context, am_audio_bus *bus) {
    double t = (double)offset / (double)context->sample_rate;
    double dt = 1.0/(double)context->sample_rate;
    int num_channels = bus->num_channels;
    int num_samples = bus->num_samples;
    float phase_inc = linear_incf(phase, num_samples);
    float freq_inc = linear_incf(freq, num_samples);
    float phase_val = phase.current_value;
    float freq_val = freq.current_value;
    switch (waveform.current_value) {
        case AM_WAVEFORM_SINE: {
            for (int i = 0; i < num_samples; i++) {
                float val = sinf(AM_PI*2.0f*freq_val*(float)t+phase_val);
                for (int c = 0; c < num_channels; c++) {
                    bus->channel_data[c][i] += val;
                }
                phase_val += phase_inc;
                freq_val += freq_inc;
                t += dt;
            }
        }
        default: {}
    }
}

bool am_oscillator_node::finished() {
    return false;
}

// Spectrum node

am_spectrum_node::am_spectrum_node() : smoothing(0.9f) {
    for (int i = 0; i < AM_MAX_FFT_BINS; i++) {
        bin_data[i] = 0.0f;
    }
    fftsize = 1024;
    num_bins = fftsize / 2 + 1;
    cfg = NULL;
    arr = NULL;
    arr_ref = LUA_NOREF;
}

void am_spectrum_node::sync_params() {
    if (arr->size < num_bins - 1) return;
    if (arr->type != AM_VIEW_TYPE_F32) return;
    if (arr->components != 1) return;
    if (arr->buffer->data == NULL) return;
    float *farr = (float*)&arr->buffer->data[arr->offset];
    for (int i = 1; i < num_bins; i++) {
        float data = bin_data[i];
        float decibels = data == 0.0f ? -1000.0f : 20.0f * log10f(data);
        *farr = decibels;
        farr = (float*)(((uint8_t*)farr) + arr->stride);
    }
    smoothing.update_target();
    smoothing.update_current();
    done = false;
}

void am_spectrum_node::post_render(am_audio_context *context, int num_samples) {
}

static void apply_fft_window(float *p, size_t n) {
    // Blackman window, copied from http://www.chromium.org/blink
    double alpha = 0.16;
    double a0 = 0.5 * (1 - alpha);
    double a1 = 0.5;
    double a2 = 0.5 * alpha;

    for (unsigned i = 0; i < n; ++i) {
        double x = static_cast<double>(i) / static_cast<double>(n);
        double window = a0 - a1 * cos(AM_2PI * x) + a2 * cos(AM_2PI * 2.0 * x);
        p[i] *= float(window);
    }
}

void am_spectrum_node::render_audio(am_audio_context *context, am_audio_bus *bus) {
    am_audio_bus tmp(bus);
    render_children(context, &tmp);
    mix_bus(bus, &tmp);
    if (done) return;

    int num_channels = tmp.num_channels;
    int num_samples = tmp.num_samples;

    if (fftsize > num_samples) {
        am_log1("%s", "WARNING: fft size > audio buffer size, ignoring");
        return;
    }
    if (num_samples % fftsize != 0) {
        am_log1("%s", "WARNING: audio buffer size not divisible by fft size, ignoring");
        return;
    }
    if (!am_is_power_of_two(fftsize)) {
        am_log1("%s", "WARNING: fft size is not a power of 2, ignoring");
        return;
    }

    float *input = tmp.channel_data[0];

    // combine channels
    for (int c = 1; c < num_channels; c++) {
        float *channeln = tmp.channel_data[c];
        for (int s = 0; s < num_samples; s++) {
            input[s] += channeln[s];
        }
    }
    
    for (int i = 0; i < num_samples / fftsize; i++) {
        apply_fft_window(input, fftsize);
        kiss_fft_cpx output[AM_MAX_FFT_BINS];
        kiss_fftr(cfg, input, output);

        // Adapted from http://www.chromium.org/blink (I don't understand this)
        // Normalize so that an input sine wave at 0dBfs registers as 0dBfs (undo FFT scaling factor).
        const float magnitudeScale = 1.0f / (float)fftsize;

        // A value of 0 does no averaging with the previous result.  Larger values produce slower, but smoother changes.
        float k = smoothing.current_value;

        // Convert the analysis data from complex to magnitude and average with the previous result.
        for (int j = 0; j < num_bins; j++) {
            float im = output[j].i;
            float re = output[j].r;
            double scalarMagnitude = sqrtf(re * re + im * im) * magnitudeScale;
            bin_data[j] = k * bin_data[j] + (1.0f - k) * scalarMagnitude;
        }

        input += fftsize;
    }

    done = true;
}

// Capture Node

am_capture_node::am_capture_node() {
}

void am_capture_node::sync_params() {
}

void am_capture_node::post_render(am_audio_context *context, int num_samples) {
}

void am_capture_node::render_audio(am_audio_context *context, am_audio_bus *bus) {
    am_audio_bus tmp(bus);
    am_capture_audio(&tmp);
    mix_bus(bus, &tmp);
}

bool am_capture_node::finished() {
    return false;
}

// Sunvox Node

am_sunvox_node::am_sunvox_node()
    : filename(""),
    offset(0)
{
}

void am_sunvox_node::sync_params() {
    // int level = sv_get_current_signal_level( slot, channel ); // 0-255
    // int bpm = sv_get_song_bpm( slot );
    // int num = sv_get_number_of_modules( slot );
    // const char* sv_get_module_name( int slot, int mod_num ) SUNVOX_FN_ATTR;
    // int mod = sv_find_module( slot, name );
    // uint32_t sv_get_module_flags( int slot, int mod_num ) SUNVOX_FN_ATTR; /* SV_MODULE_FLAG_xxx */
    // uint32_t sv_get_module_xy( int slot, int mod_num ) SUNVOX_FN_ATTR;
    // int sv_get_module_color( int slot, int mod_num ) SUNVOX_FN_ATTR;
    // 
    /*
   sv_get_module_scope2() return value = received number of samples (may be less or equal to samples_to_read).
   Example:
     int16_t buf[ 1024 ];
     int received = sv_get_module_scope2( slot, mod_num, 0, buf, 1024 );
     //buf[ 0 ] = value of the first sample (-32768...32767);
     //buf[ 1 ] = value of the second sample;
     //...
     //buf[ received - 1 ] = value of the last received sample;
*/
// uint32_t sv_get_module_scope2( int slot, int mod_num, int channel, int16_t* dest_buf, uint32_t samples_to_read ) SUNVOX_FN_ATTR;

/*
int sv_get_number_of_module_ctls( int slot, int mod_num ) SUNVOX_FN_ATTR;
const char* sv_get_module_ctl_name( int slot, int mod_num, int ctl_num ) SUNVOX_FN_ATTR;
int sv_get_module_ctl_value( int slot, int mod_num, int ctl_num, int scaled ) SUNVOX_FN_ATTR;
*/

// const char* sv_get_log( int size ) SUNVOX_FN_ATTR;

}

void am_sunvox_node::post_render(am_audio_context *context, int num_samples) {
    offset += num_samples;
}

void am_sunvox_node::render_audio(am_audio_context *context, am_audio_bus *bus) {
    int num_channels = bus->num_channels;
    int num_samples = bus->num_samples;

    if (num_samples > sunvox_buffer_size) {
        num_samples = sunvox_buffer_size;
        fprintf(stderr, "amulet audio buffer is bigger than sunvox audio buffer!\n");
    }

    // Note: we're using 'offset' to keep track of how many samples into the song
    // we have progressed, but we need to convert from samples into sunvox's ticks
    // via sv_get_ticks_per_second()
    // Note: We could use Sunvox's internal timer sv_get_ticks() instead.
    // Actually, it doesn't seem to matter what we pass in as 'ticks' even 0?!
    uint32_t ticks = offset;
    if (am_conf_audio_sample_rate != sv_get_ticks_per_second()) {
        ticks = (double)offset / (double)am_conf_audio_sample_rate * sv_get_ticks_per_second();
    }
    // uint32_t ticks = sv_get_ticks();
	sv_audio_callback( sunvox_buffer, num_samples, 0, ticks );

    for (int i = 0; i < num_samples; i++) {
        for (int c = 0; c < num_channels; c++) {
            bus->channel_data[c][i] += sunvox_buffer[i*num_channels+c];
        }
    }
}

bool am_sunvox_node::finished() {
    // are *any* of the slots still playing?
    for (int slot=0; slot<4; slot++) {
        if (!sv_end_of_song(slot)) {
            return false;
        }
    }
    return true;
}

am_sunvox_node::~am_sunvox_node() {
    // sv_close_slot( slot );
}

int am_sunvox_node::play_file(const char *filename, int slot, bool loop) {
    this->filename = filename;

    sv_open_slot( slot );
    int res = sv_load( slot, filename );
    if( res != 0 ) {
        return res;
    }

    fprintf(stderr, "sunvox loaded file into slot %d: %s\n", slot, filename);
    fprintf(stderr, "sunvox song name: %s\n", sv_get_song_name(0));

    sv_set_autostop( slot, !loop );
    sv_volume( slot, 256 );
    sv_play_from_beginning( slot );

    return 0;
}

//-------------------------------------------------------------------------
// Lua bindings.

static int search_uservalues(lua_State *L, am_audio_node *node) {
    if (node_marked(node)) return 0; // cycle
    node->pushuservalue(L); // push uservalue table of node
    lua_pushvalue(L, 2); // push field
    lua_rawget(L, -2); // lookup field in uservalue table
    if (!lua_isnil(L, -1)) {
        // found it
        lua_remove(L, -2); // remove uservalue table
        return 1;
    }
    lua_pop(L, 2); // pop nil, uservalue table
    if (node->pending_children.size != 1) return 0;
    mark_node(node);
    am_audio_node *child = node->pending_children.arr[0].child;
    child->push(L);
    lua_replace(L, 1); // child is now at index 1
    int r = search_uservalues(L, child);
    unmark_node(node);
    return r;
}

int am_audio_node_index(lua_State *L) {
    am_audio_node *node = (am_audio_node*)lua_touserdata(L, 1);
    am_default_index_func(L); // check metatable
    if (!lua_isnil(L, -1)) return 1;
    lua_pop(L, 1); // pop nil
    return search_uservalues(L, node);
}

static int add_child(lua_State *L) {
    am_check_nargs(L, 2);
    am_audio_node *parent = am_get_userdata(L, am_audio_node, 1);
    am_audio_node *child = am_get_userdata(L, am_audio_node, 2);
    am_audio_node_child child_slot;
    child_slot.child = child;
    child_slot.ref = parent->ref(L, 2); // ref from parent to child

    // keep list sorted (required for sync_children_list below)
    int n = parent->pending_children.size;
    for (int i = 0; i <= n; i++) {
        if (i == n || child < parent->pending_children.arr[i].child) {
            parent->pending_children.insert(L, i, child_slot);
            break;
        }
    }

    set_children_dirty(parent);
    lua_pushvalue(L, 1); // for chaining
    return 1;
}

static int remove_child(lua_State *L) {
    am_check_nargs(L, 2);
    am_audio_node *parent = am_get_userdata(L, am_audio_node, 1);
    am_audio_node *child = am_get_userdata(L, am_audio_node, 2);
    for (int i = 0; i < parent->pending_children.size; i++) {
        if (parent->pending_children.arr[i].child == child) {
            parent->unref(L, parent->pending_children.arr[i].ref);
            parent->pending_children.remove(i);
            set_children_dirty(parent);
            break;
        }
    }
    lua_pushvalue(L, 1); // for chaining
    return 1;
}

static int remove_all_children(lua_State *L) {
    am_check_nargs(L, 1);
    am_audio_node *parent = am_get_userdata(L, am_audio_node, 1);
    for (int i = parent->pending_children.size-1; i >= 0; i--) {
        parent->unref(L, parent->pending_children.arr[i].ref);
    }
    parent->pending_children.clear();
    set_children_dirty(parent);
    lua_pushvalue(L, 1); // for chaining
    return 1;
}

void am_set_audio_node_child(lua_State *L, am_audio_node *parent) {
    if (lua_isnil(L, 1)) {
        return;
    }
    am_audio_node *child = am_get_userdata(L, am_audio_node, 1);
    am_audio_node_child child_slot;
    child_slot.child = child;
    child_slot.ref = parent->ref(L, 1); // ref from parent to child
    parent->pending_children.push_back(L, child_slot);
    set_children_dirty(parent);
}

static int child_pair_next(lua_State *L) {
    am_check_nargs(L, 2);
    am_audio_node *node = am_get_userdata(L, am_audio_node, 1);
    int i = luaL_checkinteger(L, 2);
    if (i >= 0 && i < node->pending_children.size) {
        lua_pushinteger(L, i+1);
        node->pending_children.arr[i].child->push(L);
        return 2;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

static int child_pairs(lua_State *L) {
    lua_pushcclosure(L, child_pair_next, 0);
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 0);
    return 3;
}

static int get_child(lua_State *L) {
    am_check_nargs(L, 2);
    am_audio_node *node = am_get_userdata(L, am_audio_node, 1);
    int i = luaL_checkinteger(L, 2);
    if (i >= 1 && i <= node->pending_children.size) {
        node->pending_children.arr[i-1].child->push(L);
        return 1;
    } else {
        return 0;
    }
}

static void check_alias(lua_State *L) {
    am_default_index_func(L);
    if (!lua_isnil(L, -1)) goto error;
    lua_pop(L, 1);
    return;
    error:
    luaL_error(L,
        "alias '%s' is already used for something else",
        lua_tostring(L, 2));
}

static int alias(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_audio_node *node = am_get_userdata(L, am_audio_node, 1);
    node->pushuservalue(L);
    int userval_idx = am_absindex(L, -1);
    if (lua_istable(L, 2)) {
        // create multiple aliases - one for each key/value pair
        lua_pushvalue(L, 2); // push table, as we need position 2 for check_alias
        int tbl_idx = am_absindex(L, -1);
        lua_pushnil(L);
        while (lua_next(L, tbl_idx)) {
            lua_pushvalue(L, -2); // key
            lua_replace(L, 2); // check_alias expects key in position 2
            check_alias(L);
            lua_pushvalue(L, -2); // key
            lua_pushvalue(L, -2); // value
            lua_rawset(L, userval_idx); // uservalue[key] = value
            lua_pop(L, 1); // value
        }
        lua_pop(L, 1); // table
    } else if (lua_isstring(L, 2)) {
        check_alias(L);
        lua_pushvalue(L, 2);
        if (nargs > 2) {
            lua_pushvalue(L, 3);
        } else {
            lua_pushvalue(L, 1);
        }
        lua_rawset(L, userval_idx);
    } else {
        return luaL_error(L, "expecting a string or table at position 2");
    }
    lua_pop(L, 1); // uservalue
    lua_pushvalue(L, 1);
    return 1;
}

// Gain node lua bindings

static int create_gain_node(lua_State *L) {
    am_check_nargs(L, 2);
    am_gain_node *node = am_new_userdata(L, am_gain_node);
    am_set_audio_node_child(L, node);
    node->gain.set_immediate(luaL_checknumber(L, 2));
    return 1;
}

static void get_gain(lua_State *L, void *obj) {
    am_gain_node *node = (am_gain_node*)obj;
    lua_pushnumber(L, node->gain.pending_value);
}

static void set_gain(lua_State *L, void *obj) {
    am_gain_node *node = (am_gain_node*)obj;
    node->gain.pending_value = luaL_checknumber(L, 3);
}

static am_property gain_property = {get_gain, set_gain};

static void register_gain_node_mt(lua_State *L) {
    lua_newtable(L);
    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);

    am_register_property(L, "value", &gain_property);

    am_register_metatable(L, "gain", MT_am_gain_node, MT_am_audio_node);
}

// Lowpass biquad filter node lua bindings

static int create_lowpass_filter_node(lua_State *L) {
    am_check_nargs(L, 3);
    am_lowpass_filter_node *node = am_new_userdata(L, am_lowpass_filter_node);
    am_set_audio_node_child(L, node);
    node->cutoff.set_immediate(luaL_checknumber(L, 2));
    node->resonance.set_immediate(luaL_checknumber(L, 3));
    node->set_lowpass_params(node->cutoff.current_value, node->resonance.current_value);
    return 1;
}

static void get_lowpass_cutoff(lua_State *L, void *obj) {
    am_lowpass_filter_node *node = (am_lowpass_filter_node*)obj;
    lua_pushnumber(L, node->cutoff.pending_value);
}

static void set_lowpass_cutoff(lua_State *L, void *obj) {
    am_lowpass_filter_node *node = (am_lowpass_filter_node*)obj;
    node->cutoff.pending_value = am_clamp(luaL_checknumber(L, 3), 1.0, 22050.0);
}

static void get_lowpass_resonance(lua_State *L, void *obj) {
    am_lowpass_filter_node *node = (am_lowpass_filter_node*)obj;
    lua_pushnumber(L, node->resonance.pending_value);
}

static void set_lowpass_resonance(lua_State *L, void *obj) {
    am_lowpass_filter_node *node = (am_lowpass_filter_node*)obj;
    node->resonance.pending_value = am_clamp(luaL_checknumber(L, 3), 0.0, 1000.0);
}

static am_property lowpass_cutoff_property = {get_lowpass_cutoff, set_lowpass_cutoff};
static am_property lowpass_resonance_property = {get_lowpass_resonance, set_lowpass_resonance};

static void register_lowpass_filter_node_mt(lua_State *L) {
    lua_newtable(L);
    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);

    am_register_property(L, "cutoff", &lowpass_cutoff_property);
    am_register_property(L, "resonance", &lowpass_resonance_property);

    am_register_metatable(L, "lowpass_filter", MT_am_lowpass_filter_node, MT_am_audio_node);
}

// Highpass biquad filter node lua bindings

static int create_highpass_filter_node(lua_State *L) {
    am_check_nargs(L, 3);
    am_highpass_filter_node *node = am_new_userdata(L, am_highpass_filter_node);
    am_set_audio_node_child(L, node);
    node->cutoff.set_immediate(luaL_checknumber(L, 2));
    node->resonance.set_immediate(luaL_checknumber(L, 3));
    node->set_highpass_params(node->cutoff.current_value, node->resonance.current_value);
    return 1;
}

static void get_highpass_cutoff(lua_State *L, void *obj) {
    am_highpass_filter_node *node = (am_highpass_filter_node*)obj;
    lua_pushnumber(L, node->cutoff.pending_value);
}

static void set_highpass_cutoff(lua_State *L, void *obj) {
    am_highpass_filter_node *node = (am_highpass_filter_node*)obj;
    node->cutoff.pending_value = am_clamp(luaL_checknumber(L, 3), 1.0, 22050.0);
}

static void get_highpass_resonance(lua_State *L, void *obj) {
    am_highpass_filter_node *node = (am_highpass_filter_node*)obj;
    lua_pushnumber(L, node->resonance.pending_value);
}

static void set_highpass_resonance(lua_State *L, void *obj) {
    am_highpass_filter_node *node = (am_highpass_filter_node*)obj;
    node->resonance.pending_value = am_clamp(luaL_checknumber(L, 3), 0.0, 1000.0);
}

static am_property highpass_cutoff_property = {get_highpass_cutoff, set_highpass_cutoff};
static am_property highpass_resonance_property = {get_highpass_resonance, set_highpass_resonance};

static void register_highpass_filter_node_mt(lua_State *L) {
    lua_newtable(L);
    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);

    am_register_property(L, "cutoff", &highpass_cutoff_property);
    am_register_property(L, "resonance", &highpass_resonance_property);

    am_register_metatable(L, "highpass_filter", MT_am_highpass_filter_node, MT_am_audio_node);
}

// Audio track node lua bindings

static int create_audio_track_node(lua_State *L) {
    int nargs = am_check_nargs(L, 1);
    am_audio_track_node *node = am_new_userdata(L, am_audio_track_node);
    node->audio_buffer = am_get_userdata(L, am_audio_buffer, 1);
    node->audio_buffer_ref = node->ref(L, 1);
    if (nargs > 1) {
        node->loop = lua_toboolean(L, 2);
    }
    if (nargs > 2) {
        node->playback_speed.set_immediate(luaL_checknumber(L, 3));
    }
    if (nargs > 3) {
        node->gain.set_immediate(luaL_checknumber(L, 4));
    }
    node->sample_rate_ratio = (float)node->audio_buffer->sample_rate / (float)am_conf_audio_sample_rate;
    return 1;
}

static int reset_track(lua_State *L) {
    int nargs = am_check_nargs(L, 1);
    am_audio_track_node *node = am_get_userdata(L, am_audio_track_node, 1);
    node->needs_reset = true;
    if (nargs > 1) {
        int buf_num_channels = node->audio_buffer->num_channels;
        int buf_num_samples = node->audio_buffer->buffer->size / (buf_num_channels * sizeof(float));
        node->reset_position = am_min(luaL_checknumber(L, 2) * node->audio_buffer->sample_rate, (double)(buf_num_samples-1));
    } else {
        node->reset_position = 0.0;
    }
    return 0;
}

static void get_track_playback_speed(lua_State *L, void *obj) {
    am_audio_track_node *node = (am_audio_track_node*)obj;
    lua_pushnumber(L, node->playback_speed.pending_value);
}

static void set_track_playback_speed(lua_State *L, void *obj) {
    am_audio_track_node *node = (am_audio_track_node*)obj;
    node->playback_speed.pending_value = luaL_checknumber(L, 3);
}

static am_property track_playback_speed_property = {get_track_playback_speed, set_track_playback_speed};

static void get_track_volume(lua_State *L, void *obj) {
    am_audio_track_node *node = (am_audio_track_node*)obj;
    lua_pushnumber(L, node->gain.pending_value);
}

static void set_track_volume(lua_State *L, void *obj) {
    am_audio_track_node *node = (am_audio_track_node*)obj;
    node->gain.pending_value = luaL_checknumber(L, 3);
}

static am_property track_volume_property = {get_track_volume, set_track_volume};

static void register_audio_track_node_mt(lua_State *L) {
    lua_newtable(L);
    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);

    lua_pushcclosure(L, reset_track, 0);
    lua_setfield(L, -2, "reset");

    am_register_property(L, "playback_speed", &track_playback_speed_property);
    am_register_property(L, "volume", &track_volume_property);

    am_register_metatable(L, "track", MT_am_audio_track_node, MT_am_audio_node);
}

// Audio stream node lua bindings

static int create_audio_stream_node(lua_State *L) {
    int nargs = am_check_nargs(L, 1);
    am_audio_stream_node *node = am_new_userdata(L, am_audio_stream_node);
    node->buffer = am_check_buffer(L, 1);
    node->buffer_ref = node->ref(L, 1);
    if (nargs > 1) {
        node->loop = lua_toboolean(L, 2);
    }
    if (nargs > 2) {
        node->playback_speed.set_immediate(luaL_checknumber(L, 3));
    }
    int err = 0;
    node->handle = stb_vorbis_open_memory(
        (const unsigned char *)node->buffer->data, node->buffer->size, &err, NULL);
    if (node->handle == NULL) {
        return luaL_error(L, "buffer '%s' is not valid ogg vorbis data", node->buffer->origin);
    }
    stb_vorbis_info info = stb_vorbis_get_info((stb_vorbis*)node->handle);
    node->sample_rate = info.sample_rate;
    if (node->sample_rate != am_conf_audio_sample_rate) {
        am_log0("WARNING: buffer '%s' has sample rate of %dHz, but will play at %dHz",
            node->buffer->origin, node->sample_rate, am_conf_audio_sample_rate);
    }
    node->num_channels = info.channels;
    node->sample_rate_ratio = (float)node->sample_rate / (float)am_conf_audio_sample_rate;
    return 1;
}

static int audio_stream_gc(lua_State *L) {
    am_audio_stream_node *node = (am_audio_stream_node*)lua_touserdata(L, 1);
    if (node->handle != NULL) {
        stb_vorbis_close((stb_vorbis*)node->handle);
        node->handle = NULL;
    }
    return 0;
}

static void get_stream_playback_speed(lua_State *L, void *obj) {
    am_audio_stream_node *node = (am_audio_stream_node*)obj;
    lua_pushnumber(L, node->playback_speed.pending_value);
}

static void set_stream_playback_speed(lua_State *L, void *obj) {
    am_audio_stream_node *node = (am_audio_stream_node*)obj;
    node->playback_speed.pending_value = luaL_checknumber(L, 3);
}

static am_property stream_playback_speed_property = {get_stream_playback_speed, set_stream_playback_speed};

static void register_audio_stream_node_mt(lua_State *L) {
    lua_newtable(L);
    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);
    lua_pushcclosure(L, audio_stream_gc, 0);
    lua_setfield(L, -2, "__gc");

    am_register_property(L, "playback_speed", &stream_playback_speed_property);

    am_register_metatable(L, "audio_stream", MT_am_audio_stream_node, MT_am_audio_node);
}

// Oscillator node lua bindings

static int create_oscillator_node(lua_State *L) {
    int nargs = am_check_nargs(L, 1);
    am_oscillator_node *node = am_new_userdata(L, am_oscillator_node);
    node->freq.set_immediate(luaL_checknumber(L, 1));
    if (nargs > 1) {
        node->phase.set_immediate(luaL_checknumber(L, 2));
    }
    return 1;
}

static void get_phase(lua_State *L, void *obj) {
    am_oscillator_node *node = (am_oscillator_node*)obj;
    lua_pushnumber(L, node->phase.pending_value);
}

static void set_phase(lua_State *L, void *obj) {
    am_oscillator_node *node = (am_oscillator_node*)obj;
    node->phase.pending_value = luaL_checknumber(L, 3);
}

static am_property phase_property = {get_phase, set_phase};

static void get_freq(lua_State *L, void *obj) {
    am_oscillator_node *node = (am_oscillator_node*)obj;
    lua_pushnumber(L, node->freq.pending_value);
}

static void set_freq(lua_State *L, void *obj) {
    am_oscillator_node *node = (am_oscillator_node*)obj;
    node->freq.pending_value = luaL_checknumber(L, 3);
}

static am_property freq_property = {get_freq, set_freq};

static void register_oscillator_node_mt(lua_State *L) {
    lua_newtable(L);
    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);

    am_register_property(L, "phase", &phase_property);
    am_register_property(L, "freq", &freq_property);

    am_register_metatable(L, "oscillator", MT_am_oscillator_node, MT_am_audio_node);
}

// Spectrum node lua bindings

static am_spectrum_node *new_spectrum_node(lua_State *L, int fftsize) {
    size_t cfg_sz = 0;
    kiss_fftr_alloc(fftsize, 0, NULL, &cfg_sz);
    // allocate extra space for kissfft cfg
    am_spectrum_node *node = (am_spectrum_node*)am_set_metatable(L,
        new (lua_newuserdata(L, 
            sizeof(am_spectrum_node) + cfg_sz))
        am_spectrum_node(), MT_am_spectrum_node);
    node->cfg = kiss_fftr_alloc(fftsize, 0, (void*)(node + 1), &cfg_sz);
    assert(node->cfg);
    node->fftsize = fftsize;
    node->num_bins = fftsize / 2 + 1;
    return node;
}

static int create_spectrum_node(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    int freq_bins = luaL_checkinteger(L, 2);
    int fftsize = freq_bins * 2;
    if (fftsize < AM_MIN_FFT_SIZE) {
        return luaL_error(L, "number of frequency bins must be at least %d", AM_MIN_FFT_SIZE / 2);
    }
    if (fftsize > am_min(am_conf_audio_buffer_size, AM_MAX_FFT_SIZE)) {
        return luaL_error(L, "too many frequency bins (max %d)", am_min(am_conf_audio_buffer_size, AM_MAX_FFT_SIZE) / 2);
    }
    if (!am_is_power_of_two(fftsize)) {
        return luaL_error(L, "frequency bins must be a power of 2", freq_bins);
    }
    am_spectrum_node *node = new_spectrum_node(L, fftsize);
    am_set_audio_node_child(L, node);
    node->arr = am_get_userdata(L, am_buffer_view, 3);
    node->arr_ref = node->ref(L, 3);
    if (node->arr->size < freq_bins) {
        return luaL_error(L, "array must have at least %d elements", freq_bins);
    }
    if (node->arr->type != AM_VIEW_TYPE_F32) {
        return luaL_error(L, "array must have type float");
    }
    if (node->arr->components != 1) {
        return luaL_error(L, "array must have 1 component of type float");
    }
    if (nargs > 3) {
        node->smoothing.set_immediate(luaL_checknumber(L, 4));
    }
    return 1;
}

static void get_smoothing(lua_State *L, void *obj) {
    am_spectrum_node *node = (am_spectrum_node*)obj;
    lua_pushnumber(L, node->smoothing.pending_value);
}

static void set_smoothing(lua_State *L, void *obj) {
    am_spectrum_node *node = (am_spectrum_node*)obj;
    node->smoothing.pending_value = am_clamp(luaL_checknumber(L, 3), 0.0, 1.0);
}

static am_property smoothing_property = {get_smoothing, set_smoothing};

static void register_spectrum_node_mt(lua_State *L) {
    lua_newtable(L);
    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);

    am_register_property(L, "smoothing", &smoothing_property);

    am_register_metatable(L, "spectrum", MT_am_spectrum_node, MT_am_audio_node);
}

// Capture node lua bindings

static int create_capture_node(lua_State *L) {
    am_new_userdata(L, am_capture_node);
    return 1;
}

static void register_capture_node_mt(lua_State *L) {
    lua_newtable(L);
    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);

    am_register_metatable(L, "capture", MT_am_capture_node, MT_am_audio_node);
}

// Sunvox node lua bindings

static int create_sunvox_node(lua_State *L) {
    int nargs = am_check_nargs(L, 0);
    am_sunvox_node *node = am_new_userdata(L, am_sunvox_node);

    if (nargs > 0) {
        const char *filename = luaL_checkstring(L, 1);
        int slot = nargs > 1 ? luaL_checknumber(L, 2) : 0;
        bool loop = nargs > 2 ? luaL_checknumber(L, 3) : 0;

        int res = node->play_file(filename, slot, loop);
        if (res) {
            return luaL_error(L, "sunvox load error %d: %s\n", res, filename);
        }
    }

    return 1;
}

// Sunvox API bindings
// Most of these just pass through to the underlying API.
// These are in the same order as sunvox.h

static int sunvox_load(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    const char *filename = luaL_checkstring(L, 3);

    sv_open_slot( slot );
    int res = sv_load( slot, filename );
    if (res) {
        return luaL_error(L, "sunvox load error %d: %s\n", res, filename);
    }

    fprintf(stderr, "sunvox loaded file into slot %d: %s\n", slot, filename);
    fprintf(stderr, "sunvox song name: %s\n", sv_get_song_name(0));

    // sv_volume( slot, 256 );
    // sv_play_from_beginning( slot );

    return 1;
}

static int sunvox_play(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int res = sv_play( slot );
    if (res) {
        return luaL_error(L, "sunvox play error %d\n", res);
    }

    return 1;
}

static int sunvox_play_from_beginning(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int res = sv_play_from_beginning( slot );
    if (res) {
        return luaL_error(L, "sunvox play_from_beginning error %d\n", res);
    }

    return 1;
}

static int sunvox_stop(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int res = sv_stop( slot );
    if (res) {
        return luaL_error(L, "sunvox stop error %d\n", res);
    }

    return 1;
}

static int sunvox_set_autostop(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int autostop = luaL_checknumber(L, 3);

    int res = sv_set_autostop( slot, autostop );
    if (res) {
        return luaL_error(L, "sunvox set_autostop error %d\n", res);
    }

    return 1;
}

static int sunvox_get_autostop(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int autostop = sv_get_autostop( slot );
    lua_pushinteger(L, autostop);

    return 1;
}

static int sunvox_end_of_song(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int result = sv_end_of_song(slot);
    lua_pushboolean(L, result);

    return 1;
}

static int sunvox_rewind(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int line_num = luaL_checknumber(L, 3);

    int res = sv_rewind(slot, line_num);
    if (res) {
        return luaL_error(L, "sunvox rewind error %d\n", res);
    }

    return 1;
}

static int sunvox_volume(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int volume = luaL_checknumber(L, 3);

    int old_volume = sv_volume(slot, volume);
    lua_pushinteger(L, old_volume);

    return 1;
}

static int sunvox_set_event_t(lua_State *L) {
    int nargs = am_check_nargs(L, 8);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int set = luaL_checknumber(L, 3);
    int t = luaL_checknumber(L, 4);

    int res = sv_set_event_t(slot, set, t);
    if (res) {
        return luaL_error(L, "sunvox set_event_t error %d\n", res);
    }

    return 1;
}

static int sunvox_send_event(lua_State *L) {
    int nargs = am_check_nargs(L, 8);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int track_num = luaL_checknumber(L, 3);
    int note = luaL_checknumber(L, 4);
    int vel = luaL_checknumber(L, 5);
    int module = luaL_checknumber(L, 6);
    int ctl = luaL_checknumber(L, 7);
    int ctl_val = luaL_checknumber(L, 8);

    int res = sv_send_event(slot, track_num, note, vel, module, ctl, ctl_val);
    if (res) {
        return luaL_error(L, "sunvox send_event error %d\n", res);
    }

    return 1;
}

static int sunvox_get_current_line(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int line_num = sv_get_current_line(slot);
    lua_pushinteger(L, line_num);

    return 1;
}

static int sunvox_get_current_signal_level(lua_State *L) {
    int result = 0;
    int nargs = am_check_nargs(L, 1);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);
    int slot = nargs >= 2 ? luaL_checknumber(L, 2) : 0;
    if (nargs >= 3) {
        int channel = nargs >= 3 ? luaL_checknumber(L, 3) : 0;
        result = sv_get_current_signal_level(slot, channel);
    }else{
        // no channel specified - average L/R
        result = (
            sv_get_current_signal_level(slot, 0) +
            sv_get_current_signal_level(slot, 1)
            ) / 2;
    }
    lua_pushinteger(L, result);

    return 1;
}

static int sunvox_get_song_name(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    const char* song_name = sv_get_song_name(slot);
    lua_pushstring(L, song_name);

    return 1;
}

static int sunvox_get_song_bpm(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int bpm = sv_get_song_bpm(slot);
    lua_pushinteger(L, bpm);

    return 1;
}

static int sunvox_get_song_tpl(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int tpl = sv_get_song_tpl(slot);
    lua_pushinteger(L, tpl);

    return 1;
}

static int sunvox_get_song_length_frames(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int frames = sv_get_song_length_frames(slot);
    lua_pushinteger(L, frames);

    return 1;
}

static int sunvox_get_song_length_lines(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int lines = sv_get_song_length_lines(slot);
    lua_pushinteger(L, lines);

    return 1;
}

static int sunvox_new_module(lua_State *L) {
    int nargs = am_check_nargs(L, 7);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    const char *type = luaL_checkstring(L, 3);
    const char *name = luaL_checkstring(L, 4);
    int x = luaL_checknumber(L, 5);
    int y = luaL_checknumber(L, 6);
    int z = luaL_checknumber(L, 7);

    sv_lock_slot(slot);
    int mod_num = sv_new_module(slot, type, name, x, y, z);
    sv_unlock_slot(slot);
    lua_pushinteger(L, mod_num);

    return 1;
}

static int sunvox_remove_module(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int mod_num = luaL_checknumber(L, 3);

    sv_lock_slot(slot);
    int res = sv_remove_module(slot, mod_num);
    sv_unlock_slot(slot);
    if (res) {
        return luaL_error(L, "sunvox remove_module error %d\n", res);
    }

    return 1;
}

static int sunvox_connect_module(lua_State *L) {
    int nargs = am_check_nargs(L, 4);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int source = luaL_checknumber(L, 3);
    int destination = luaL_checknumber(L, 4);

    sv_lock_slot(slot);
    int res = sv_connect_module(slot, source, destination);
    sv_unlock_slot(slot);
    if (res) {
        return luaL_error(L, "sunvox connect_module error %d\n", res);
    }

    return 1;
}

static int sunvox_disconnect_module(lua_State *L) {
    int nargs = am_check_nargs(L, 4);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int source = luaL_checknumber(L, 3);
    int destination = luaL_checknumber(L, 4);

    sv_lock_slot(slot);
    int res = sv_disconnect_module(slot, source, destination);
    sv_unlock_slot(slot);
    if (res) {
        return luaL_error(L, "sunvox disconnect_module error %d\n", res);
    }

    return 1;
}

static int sunvox_load_module(lua_State *L) {
    int nargs = am_check_nargs(L, 6);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    const char *filename = luaL_checkstring(L, 3);
    int x = luaL_checknumber(L, 4);
    int y = luaL_checknumber(L, 5);
    int z = luaL_checknumber(L, 6);

    // sv_lock_slot(slot);
    int mod_num = sv_load_module(slot, filename, x, y, z);
    // sv_unlock_slot(slot);
    lua_pushinteger(L, mod_num);

    return 1;
}

static int sunvox_sampler_load(lua_State *L) {
    int nargs = am_check_nargs(L, 5);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int sampler_module = luaL_checknumber(L, 3);
    const char *filename = luaL_checkstring(L, 4);
    int sample_slot = luaL_checknumber(L, 5);

    // sv_lock_slot(slot);
    int result = sv_sampler_load(slot, sampler_module, filename, sample_slot);
    // sv_unlock_slot(slot);
    lua_pushinteger(L, result);

    return 1;
}

static int sunvox_get_number_of_modules(lua_State *L) {
    int nargs = am_check_nargs(L, 2);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);

    int result = sv_get_number_of_modules(slot);
    lua_pushinteger(L, result);

    return 1;
}

static int sunvox_find_module(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    const char *name = luaL_checkstring(L, 3);

    int result = sv_find_module(slot, name);
    lua_pushinteger(L, result);

    return 1;
}

static int sunvox_get_module_flags(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int mod_num = luaL_checknumber(L, 3);

    uint32_t flags = sv_get_module_flags(slot, mod_num);
    lua_pushinteger(L, flags);

    return 1;
}

/*
   sv_get_module_inputs(), sv_get_module_outputs() - 
   get pointers to the int[] arrays with the input/output links.
   Number of inputs = ( module_flags & SV_MODULE_INPUTS_MASK ) >> SV_MODULE_INPUTS_OFF.
   Number of outputs = ( module_flags & SV_MODULE_OUTPUTS_MASK ) >> SV_MODULE_OUTPUTS_OFF.
*/
// int* sv_get_module_inputs( int slot, int mod_num ) SUNVOX_FN_ATTR;
// int* sv_get_module_outputs( int slot, int mod_num ) SUNVOX_FN_ATTR;

static int sunvox_get_module_name(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int mod_num = luaL_checknumber(L, 3);

    const char* name = sv_get_module_name(slot, mod_num);
    lua_pushstring(L, name);

    return 1;
}

static int sunvox_get_module_xy(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int mod_num = luaL_checknumber(L, 3);

    uint32_t xy = sv_get_module_xy(slot, mod_num);
    int x, y;
    SV_GET_MODULE_XY(xy, x, y);

    // put the result into a 2-element table
    lua_createtable(L, 2, 0);
    lua_pushinteger(L, x);
    lua_rawseti (L, -2, 1);
    lua_pushinteger(L, y);
    lua_rawseti (L, -2, 2);

    return 1;
}

static int sunvox_get_module_color(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int mod_num = luaL_checknumber(L, 3);

    int rgb = sv_get_module_color(slot, mod_num);

    // put the result into a 3-element table
    lua_createtable(L, 3, 0);
    lua_pushinteger(L, rgb & 0xFF);
    lua_rawseti (L, -2, 1);
    lua_pushinteger(L, (rgb >> 8) & 0xFF);
    lua_rawseti (L, -2, 1);
    lua_pushinteger(L, (rgb >> 16) & 0xFF);
    lua_rawseti (L, -2, 1);

    return 1;
}

static int sunvox_get_module_finetune(lua_State *L) {
    int nargs = am_check_nargs(L, 3);
    am_sunvox_node *node = am_get_userdata(L, am_sunvox_node, 1);

    int slot = luaL_checknumber(L, 2);
    int mod_num = luaL_checknumber(L, 3);

    uint32_t packed = sv_get_module_finetune(slot, mod_num);
    int finetune, relative_note;
    SV_GET_MODULE_FINETUNE(packed, finetune, relative_note);

    // put the result into a 2-element table
    lua_createtable(L, 2, 0);
    lua_pushinteger(L, finetune);
    lua_rawseti (L, -2, 1);
    lua_pushinteger(L, relative_note);
    lua_rawseti (L, -2, 2);

    return 1;
}

/*
   sv_get_module_scope2() return value = received number of samples (may be less or equal to samples_to_read).
   Example:
     int16_t buf[ 1024 ];
     int received = sv_get_module_scope2( slot, mod_num, 0, buf, 1024 );
     //buf[ 0 ] = value of the first sample (-32768...32767);
     //buf[ 1 ] = value of the second sample;
     //...
     //buf[ received - 1 ] = value of the last received sample;
*/
// uint32_t sv_get_module_scope2( int slot, int mod_num, int channel, int16_t* dest_buf, uint32_t samples_to_read ) SUNVOX_FN_ATTR;

/*
   sv_module_curve() - access to the curve values of the specified module
   Parameters:
     slot;
     mod_num - module number;
     curve_num - curve number;
     data - destination or source buffer;
     len - number of items to read/write;
     w - read (0) or write (1).
   return value: number of items processed successfully.

   Available curves (Y=CURVE[X]):
     MultiSynth:
       0 - X = note (0..127); Y = velocity (0..1); 128 items;
       1 - X = velocity (0..256); Y = velocity (0..1); 257 items;
     WaveShaper:
       0 - X = input (0..255); Y = output (0..1); 256 items;
     MultiCtl:
       0 - X = input (0..256); Y = output (0..1); 257 items;
     Analog Generator, Generator:
       0 - X = drawn waveform sample number (0..31); Y = volume (-1..1); 32 items;
*/
// int sv_module_curve( int slot, int mod_num, int curve_num, float* data, int len, int w ) SUNVOX_FN_ATTR;

// int sv_get_number_of_module_ctls( int slot, int mod_num ) SUNVOX_FN_ATTR;
// const char* sv_get_module_ctl_name( int slot, int mod_num, int ctl_num ) SUNVOX_FN_ATTR;
// int sv_get_module_ctl_value( int slot, int mod_num, int ctl_num, int scaled ) SUNVOX_FN_ATTR;
// int sv_get_number_of_patterns( int slot ) SUNVOX_FN_ATTR;
// int sv_find_pattern( int slot, const char* name ) SUNVOX_FN_ATTR;
// int sv_get_pattern_x( int slot, int pat_num ) SUNVOX_FN_ATTR;
// int sv_get_pattern_y( int slot, int pat_num ) SUNVOX_FN_ATTR;
// int sv_get_pattern_tracks( int slot, int pat_num ) SUNVOX_FN_ATTR;
// int sv_get_pattern_lines( int slot, int pat_num ) SUNVOX_FN_ATTR;
// const char* sv_get_pattern_name( int slot, int pat_num ) SUNVOX_FN_ATTR;

/*
   sv_get_pattern_data() - get the pattern buffer (for reading and writing)
   containing notes (events) in the following order:
     line 0: note for track 0, note for track 1, ... note for track X;
     line 1: note for track 0, note for track 1, ... note for track X;
     ...
     line X: ...
   Example:
     int pat_tracks = sv_get_pattern_tracks( slot, pat_num ); //number of tracks
     sunvox_note* data = sv_get_pattern_data( slot, pat_num ); //get the buffer with all the pattern events (notes)
     sunvox_note* n = &data[ line_number * pat_tracks + track_number ];
     ... and then do someting with note n ...
*/
// sunvox_note* sv_get_pattern_data( int slot, int pat_num ) SUNVOX_FN_ATTR;

/*
   sv_pattern_mute() - mute (1) / unmute (0) specified pattern;
   negative values are ignored;
   return value: previous state (1 - muted; 0 - unmuted) or -1 (error);
*/
// int sv_pattern_mute( int slot, int pat_num, int mute ) SUNVOX_FN_ATTR; /* USE LOCK/UNLOCK! */

/*
   SunVox engine uses its own time space, measured in system ticks (don't confuse it with the project ticks);
   required when calculating the out_time parameter in the sv_audio_callback().
   Use sv_get_ticks() to get current tick counter (from 0 to 0xFFFFFFFF).
   Use sv_get_ticks_per_second() to get the number of SunVox ticks per second.
*/
// uint32_t sv_get_ticks( void ) SUNVOX_FN_ATTR;
// uint32_t sv_get_ticks_per_second( void ) SUNVOX_FN_ATTR;

/*
   sv_get_log() - get the latest messages from the log
   Parameters:
     size - max number of bytes to read.
   Return value: pointer to the null-terminated string with the latest log messages.
*/
// const char* sv_get_log( int size ) SUNVOX_FN_ATTR;



// End of Sunvox API

static void register_sunvox_node_mt(lua_State *L) {
    lua_newtable(L);
    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);

    lua_pushcclosure(L, sunvox_load, 0);
    lua_setfield(L, -2, "load");
    lua_pushcclosure(L, sunvox_play, 0);
    lua_setfield(L, -2, "play");
    lua_pushcclosure(L, sunvox_play_from_beginning, 0);
    lua_setfield(L, -2, "play_from_beginning");
    lua_pushcclosure(L, sunvox_stop, 0);
    lua_setfield(L, -2, "stop");
    lua_pushcclosure(L, sunvox_set_autostop, 0);
    lua_setfield(L, -2, "set_autostop");
    lua_pushcclosure(L, sunvox_get_autostop, 0);
    lua_setfield(L, -2, "get_autostop");
    lua_pushcclosure(L, sunvox_end_of_song, 0);
    lua_setfield(L, -2, "end_of_song");
    lua_pushcclosure(L, sunvox_rewind, 0);
    lua_setfield(L, -2, "rewind");
    lua_pushcclosure(L, sunvox_volume, 0);
    lua_setfield(L, -2, "volume");
    lua_pushcclosure(L, sunvox_set_event_t, 0);
    lua_setfield(L, -2, "set_event_t");
    lua_pushcclosure(L, sunvox_send_event, 0);
    lua_setfield(L, -2, "send_event");
    lua_pushcclosure(L, sunvox_get_current_line, 0);
    lua_setfield(L, -2, "get_current_line");
    lua_pushcclosure(L, sunvox_get_current_signal_level, 0);
    lua_setfield(L, -2, "get_current_signal_level");
    lua_pushcclosure(L, sunvox_get_song_name, 0);
    lua_setfield(L, -2, "get_song_name");
    lua_pushcclosure(L, sunvox_get_song_bpm, 0);
    lua_setfield(L, -2, "get_song_bpm");
    lua_pushcclosure(L, sunvox_get_song_tpl, 0);
    lua_setfield(L, -2, "get_song_tpl");
    lua_pushcclosure(L, sunvox_get_song_length_frames, 0);
    lua_setfield(L, -2, "get_song_length_frames");
    lua_pushcclosure(L, sunvox_get_song_length_lines, 0);
    lua_setfield(L, -2, "get_song_length_lines");
    lua_pushcclosure(L, sunvox_new_module, 0);
    lua_setfield(L, -2, "new_module");
    lua_pushcclosure(L, sunvox_remove_module, 0);
    lua_setfield(L, -2, "remove_module");
    lua_pushcclosure(L, sunvox_connect_module, 0);
    lua_setfield(L, -2, "connect_module");
    lua_pushcclosure(L, sunvox_disconnect_module, 0);
    lua_setfield(L, -2, "disconnect_module");
    lua_pushcclosure(L, sunvox_load_module, 0);
    lua_setfield(L, -2, "load_module");
    lua_pushcclosure(L, sunvox_sampler_load, 0);
    lua_setfield(L, -2, "sampler_load");
    lua_pushcclosure(L, sunvox_get_number_of_modules, 0);
    lua_setfield(L, -2, "get_number_of_modules");
    lua_pushcclosure(L, sunvox_find_module, 0);
    lua_setfield(L, -2, "find_module");
    lua_pushcclosure(L, sunvox_get_module_flags, 0);
    lua_setfield(L, -2, "get_module_flags");
    lua_pushcclosure(L, sunvox_get_module_name, 0);
    lua_setfield(L, -2, "get_module_name");
    lua_pushcclosure(L, sunvox_get_module_xy, 0);
    lua_setfield(L, -2, "get_module_xy");
    lua_pushcclosure(L, sunvox_get_module_color, 0);
    lua_setfield(L, -2, "get_module_color");
    lua_pushcclosure(L, sunvox_get_module_finetune, 0);
    lua_setfield(L, -2, "get_module_finetune");

    am_register_metatable(L, "sunvox", MT_am_sunvox_node, MT_am_audio_node);
}

// Audio node lua bindings

static int create_audio_node(lua_State *L) {
    am_new_userdata(L, am_audio_node);
    return 1;
}

static int get_root_audio_node(lua_State *L) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, AM_ROOT_AUDIO_NODE);
    return 1;
}

static void get_finished(lua_State *L, void *obj) {
    am_audio_node *node = (am_audio_node*)obj;
    lua_pushboolean(L, node->finished());
}

static am_property finished_property = {get_finished, NULL};

static void get_num_children(lua_State *L, void *obj) {
    am_audio_node *node = (am_audio_node*)obj;
    lua_pushinteger(L, node->pending_children.size);
}

static am_property num_children_property = {get_num_children, NULL};

static void get_paused(lua_State *L, void *obj) {
    am_audio_node *node = (am_audio_node*)obj;
    lua_pushboolean(L, pending_pause(node));
}

static void set_paused(lua_State *L, void *obj) {
    am_audio_node *node = (am_audio_node*)obj;
    if (lua_toboolean(L, 3)) {
        set_pending_pause(node);
    } else {
        clear_pending_pause(node);
    }
}

static am_property paused_property = {get_paused, set_paused};

static void register_audio_node_mt(lua_State *L) {
    lua_newtable(L);

    lua_pushcclosure(L, am_audio_node_index, 0);
    lua_setfield(L, -2, "__index");
    am_set_default_newindex_func(L);

    lua_pushcclosure(L, child_pairs, 0);
    lua_setfield(L, -2, "children");
    lua_pushcclosure(L, get_child, 0);
    lua_setfield(L, -2, "child");

    lua_pushcclosure(L, alias, 0);
    lua_setfield(L, -2, "alias");

    lua_pushcclosure(L, add_child, 0);
    lua_setfield(L, -2, "add");
    lua_pushcclosure(L, remove_child, 0);
    lua_setfield(L, -2, "remove");
    lua_pushcclosure(L, remove_all_children, 0);
    lua_setfield(L, -2, "remove_all");
    
    lua_pushcclosure(L, create_gain_node, 0);
    lua_setfield(L, -2, "gain");
    lua_pushcclosure(L, create_lowpass_filter_node, 0);
    lua_setfield(L, -2, "lowpass_filter");
    lua_pushcclosure(L, create_highpass_filter_node, 0);
    lua_setfield(L, -2, "highpass_filter");

    lua_pushcclosure(L, create_spectrum_node, 0);
    lua_setfield(L, -2, "spectrum");

    am_register_property(L, "finished", &finished_property);
    am_register_property(L, "num_children", &num_children_property);
    am_register_property(L, "paused", &paused_property);

    am_register_metatable(L, "audio_node", MT_am_audio_node, 0);
}

//-------------------------------------------------------------------------

static int create_audio_buffer(lua_State *L) {
    am_check_nargs(L, 3);
    am_buffer *buf = am_check_buffer(L, 1);
    int channels = lua_tointeger(L, 2);
    int sample_rate = lua_tointeger(L, 3);
    luaL_argcheck(L, channels >= 1, 2, "channels must be a positive integer");
    luaL_argcheck(L, buf->size / sizeof(float) / channels >= 1, 2, "not enough data for that many channels");
    luaL_argcheck(L, (buf->size / sizeof(float)) % channels == 0, 2, "buffer has invalid size for that many channels");
    luaL_argcheck(L, sample_rate >= 1, 3, "sample rate must be a positive integer");
    am_audio_buffer *audio_buffer = am_new_userdata(L, am_audio_buffer);
    audio_buffer->buffer = buf;
    audio_buffer->buffer_ref = audio_buffer->ref(L, 1);
    audio_buffer->num_channels = channels;
    audio_buffer->sample_rate = sample_rate;
    return 1;
}

static void get_channels(lua_State *L, void *obj) {
    am_audio_buffer *buf = (am_audio_buffer*)obj;
    lua_pushinteger(L, buf->num_channels);
}

static am_property channels_property = {get_channels, NULL};

static void get_sample_rate(lua_State *L, void *obj) {
    am_audio_buffer *buf = (am_audio_buffer*)obj;
    lua_pushinteger(L, buf->sample_rate);
}

static am_property sample_rate_property = {get_sample_rate, NULL};

static void get_samples_per_channel(lua_State *L, void *obj) {
    am_audio_buffer *buf = (am_audio_buffer*)obj;
    lua_pushinteger(L, buf->buffer->size / sizeof(float) / buf->num_channels);
}

static am_property samples_per_channel_property = {get_samples_per_channel, NULL};

static void get_audio_buf_length(lua_State *L, void *obj) {
    am_audio_buffer *buf = (am_audio_buffer*)obj;
    double samples = (double)(buf->buffer->size / sizeof(float) / buf->num_channels);
    double len = samples / (double)buf->sample_rate;
    lua_pushnumber(L, len);
}

static am_property audio_buf_length_property = {get_audio_buf_length, NULL};

static void get_audio_buf_buffer(lua_State *L, void *obj) {
    am_audio_buffer *buf = (am_audio_buffer*)obj;
    buf->buffer->push(L);
}

static am_property audio_buf_buffer_property = {get_audio_buf_buffer, NULL};

static void register_audio_buffer_mt(lua_State *L) {
    lua_newtable(L);

    lua_pushcclosure(L, am_default_index_func, 0);
    lua_setfield(L, -2, "__index");

    am_register_property(L, "channels", &channels_property);
    am_register_property(L, "sample_rate", &sample_rate_property);
    am_register_property(L, "samples_per_channel", &samples_per_channel_property);
    am_register_property(L, "length", &audio_buf_length_property);
    am_register_property(L, "buffer", &audio_buf_buffer_property);

    am_register_metatable(L, "audio_buffer", MT_am_audio_buffer, 0);
}

//-------------------------------------------------------------------------

static int load_audio(lua_State *L) {
    char *errmsg;
    int len;
    const char *filename = luaL_checkstring(L, 1);
    void *data = am_read_resource(filename, &len, &errmsg);
    if (data == NULL) {
        free(errmsg);
        lua_pushnil(L);
        return 1;
    }
    int num_channels;
    int sample_rate;
    short *tmp_data;
    int num_samples = stb_vorbis_decode_memory((unsigned char*)data,
        len, &num_channels, &sample_rate, &tmp_data);
    free(data);
    if (num_samples <= 0) {
        return luaL_error(L, "error loading audio '%s'", filename);
    }
    am_buffer *dest_buf;
    float *dest_data;
    num_channels = am_min(num_channels, am_conf_audio_channels);
    int dest_samples;
    if (sample_rate != am_conf_audio_sample_rate) {
        // resample required
        am_log0("WARNING: resampling buffer '%s' from %dHz to %dHz",
            filename, sample_rate, am_conf_audio_sample_rate);
        double sample_rate_ratio = (double)sample_rate / (double)am_conf_audio_sample_rate;
        dest_samples = floor((double)num_samples / sample_rate_ratio);
        dest_buf = am_push_new_buffer_and_init(L, dest_samples * num_channels * 4);
        dest_data = (float*)dest_buf->data;
        for (int c = 0; c < num_channels; c++) {
            double pos = 0.0f;
            for (int write_index = 0; write_index < dest_samples; write_index++) {
                int read_index1 = (int)floor(pos);
                int read_index2 = read_index1 + 1;
                if (read_index2 >= num_samples) {
                    break;
                }
                float interpolation_factor = (float)(pos - (float)read_index1);
                float sample1 = (float)tmp_data[read_index1 * num_channels + c] / (float)INT16_MAX;
                float sample2 = (float)tmp_data[read_index2 * num_channels + c] / (float)INT16_MAX;
                float interpolated_sample = (1.0f - interpolation_factor) * sample1 + interpolation_factor * sample2;
                dest_data[c * dest_samples + write_index] = interpolated_sample;
                pos += sample_rate_ratio;
                if (pos >= (double)num_samples) {
                    break;
                }
            }
        }
    } else {
        // no resample required
        dest_buf = am_push_new_buffer_and_init(L, num_samples * num_channels * 4);
        dest_data = (float*)dest_buf->data;
        dest_samples = num_samples;
        for (int c = 0; c < num_channels; c++) {
            for (int s = 0; s < num_samples; s++) {
                dest_data[c * num_samples + s] = (float)tmp_data[s * num_channels + c] / (float)INT16_MAX;
            }
        }
    }
    free(tmp_data);
    am_audio_buffer *audio_buffer = am_new_userdata(L, am_audio_buffer);
    audio_buffer->num_channels = num_channels;
    audio_buffer->sample_rate = am_conf_audio_sample_rate;
    audio_buffer->buffer = dest_buf;
    audio_buffer->buffer_ref = audio_buffer->ref(L, -2);
    lua_remove(L, -2); // remove dest buf
    return 1;
}

//-------------------------------------------------------------------------

void am_destroy_audio() {
    audio_context.root = NULL;
    clear_buffer_pool();
}

static void update_live_pause_state(am_audio_node *node) {
    switch (live_pause_state(node)) {
        case LIVE_PAUSE_STATE_BEGIN: set_live_pause_state(node, LIVE_PAUSE_STATE_PAUSED); break;
        case LIVE_PAUSE_STATE_END: set_live_pause_state(node, LIVE_PAUSE_STATE_UNPAUSED); break;
    }
}

static void do_post_render(am_audio_context *context, int num_samples, am_audio_node *node) {
    if (node->last_render >= context->render_id) return; // already processed
    node->last_render = context->render_id;
    update_live_pause_state(node);
    node->post_render(context, num_samples);
    for (int i = 0; i < node->live_children.size; i++) {
        am_audio_node_child *child = &node->live_children.arr[i];
        if (child->state == AM_AUDIO_NODE_CHILD_STATE_REMOVED) {
            child->state = AM_AUDIO_NODE_CHILD_STATE_DONE;
        } else if (child->state == AM_AUDIO_NODE_CHILD_STATE_NEW) {
            child->state = AM_AUDIO_NODE_CHILD_STATE_OLD;
        }
        do_post_render(context, num_samples, child->child);
    }
}

void am_fill_audio_bus(am_audio_bus *bus) {
    if (audio_context.root == NULL) return;
    double t0 = 0.0;
    if (am_record_perf_timings) {
        t0 = am_get_current_time();
    }
    if (!am_conf_audio_mute) {
#if AM_STEAMWORKS
        // mute audio if steam overlay shown
        if (!am_steam_overlay_enabled) {
#endif
            audio_context.root->render_audio(&audio_context, bus);
#if AM_STEAMWORKS
        }
#endif
    }
    audio_context.render_id++;
    do_post_render(&audio_context, bus->num_samples, audio_context.root);
    if (am_record_perf_timings) {
        audio_time_accum += am_get_current_time() - t0;
    }
}

static void sync_children_list(lua_State *L, am_audio_node *node) {
    int p = 0;
    int l = 0;

    am_lua_array<am_audio_node_child> *parr = &node->pending_children;
    am_lua_array<am_audio_node_child> *larr = &node->live_children;

    // remove live children that need to be removed
    for (l = larr->size-1; l >= 0; l--) {
        if (larr->arr[l].state == AM_AUDIO_NODE_CHILD_STATE_DONE) {
            node->unref(L, larr->arr[l].ref);
            larr->remove(l);
        }
    }

    if (children_dirty(node)) {
        // insert NEW children and mark REMOVED children
        l = 0;
        p = 0;
        while (p < parr->size && l < larr->size) {
            while (p < parr->size && parr->arr[p].child < larr->arr[l].child) {
                parr->arr[p].state = AM_AUDIO_NODE_CHILD_STATE_NEW;
                larr->insert(L, l, parr->arr[p]);
                larr->arr[l].child->push(L);
                larr->arr[l].ref = node->ref(L, -1);
                lua_pop(L, 1);
                p++;
                l++;
            }
            while (p < parr->size && l < larr->size && parr->arr[p].child == larr->arr[l].child) {
                p++;
                l++;
            }
            while (p < parr->size && l < larr->size && parr->arr[p].child > larr->arr[l].child) {
                if (larr->arr[l].state != AM_AUDIO_NODE_CHILD_STATE_DONE) {
                    larr->arr[l].state = AM_AUDIO_NODE_CHILD_STATE_REMOVED;
                }
                l++;
            }
        }
        while (p < parr->size) {
            parr->arr[p].state = AM_AUDIO_NODE_CHILD_STATE_NEW;
            larr->insert(L, l, parr->arr[p]);
            larr->arr[l].child->push(L);
            larr->arr[l].ref = node->ref(L, -1);
            lua_pop(L, 1);
            p++;
            l++;
        }
        while (l < larr->size) {
            if (larr->arr[l].state != AM_AUDIO_NODE_CHILD_STATE_DONE) {
                larr->arr[l].state = AM_AUDIO_NODE_CHILD_STATE_REMOVED;
            }
            l++;
        }

        clear_children_dirty(node);
    }
}

static void sync_paused(am_audio_node *node) {
    int live_state = live_pause_state(node);
    if (pending_pause(node) && live_state != LIVE_PAUSE_STATE_PAUSED) {
        set_live_pause_state(node, LIVE_PAUSE_STATE_BEGIN);
    } else if (!pending_pause(node) && live_state != LIVE_PAUSE_STATE_UNPAUSED) {
        set_live_pause_state(node, LIVE_PAUSE_STATE_END);
    }
}

static void sync_audio_graph(lua_State *L, am_audio_context *context, am_audio_node *node) {
    if (node->last_sync >= context->sync_id) return; // already synced
    node->last_sync = context->sync_id;
    node->sync_params();
    sync_children_list(L, node);
    sync_paused(node);
    for (int i = 0; i < node->live_children.size; i++) {
        sync_audio_graph(L, context, node->live_children.arr[i].child);
    }
    am_last_frame_audio_time = audio_time_accum;
    audio_time_accum = 0.0;
}

void am_sync_audio_graph(lua_State *L) {
    if (audio_context.root == NULL) return;
    audio_context.sync_id++;
    sync_audio_graph(L, &audio_context, audio_context.root);
}

//-------------------------------------------------------------------------
// Backend utility functions

void am_interleave_audio(float* AM_RESTRICT dest, float* AM_RESTRICT src,
    int num_channels, int num_samples, int sample_offset, int count)
{
    int i, j;
    int k = sample_offset + count;
    assert(k <= num_samples);
    for (int c = 0; c < num_channels; c++) {
        i = k - count;
        j = c;
        while (i != k) {
            dest[j] = src[i];
            i++;
            j += num_channels;
        }
        k += num_samples;
    }
}

void am_interleave_audio16(int16_t* AM_RESTRICT dest, float* AM_RESTRICT src,
    int num_channels, int num_samples, int sample_offset, int count)
{
    int i, j;
    int k = sample_offset + count;
    assert(k <= num_samples);
    for (int c = 0; c < num_channels; c++) {
        i = k - count;
        j = c;
        while (i != k) {
            dest[j] = (int16_t)(src[i] * 32767.0);
            i++;
            j += num_channels;
        }
        k += num_samples;
    }
}

void am_uninterleave_audio(float* AM_RESTRICT dest, float* AM_RESTRICT src,
    int num_channels, int num_samples)
{
    int i = 0;
    int j;
    for (int c = 0; c < num_channels; c++) {
        j = c;
        for (int k = 0; k < num_samples; k++) {
            dest[i] = src[j];
            i++;
            j += num_channels;
        }
    }
}

//-------------------------------------------------------------------------
// Module registration

int am_setup_sunvox(int buffer_size, int num_channels, int sample_rate) {
    if( sv_load_dll() ) {
        return -1;
    }
    int flags = SV_INIT_FLAG_USER_AUDIO_CALLBACK | SV_INIT_FLAG_ONE_THREAD | SV_INIT_FLAG_AUDIO_FLOAT32;
    int ver = sv_init( NULL, sample_rate, num_channels, flags );
    sunvox_buffer_size = buffer_size;
    sunvox_buffer = (float*)malloc( buffer_size * num_channels * sizeof(float) );
    return 0;
}

void am_open_audio_module(lua_State *L) {

    int err = am_setup_sunvox(
        am_conf_audio_buffer_size,
        am_conf_audio_channels,
        am_conf_audio_sample_rate = 44100
    );
    if (err) {
        // TODO: Better way to return error message up the call stack?
        fprintf(stderr, "failed to load sunvox module.");
    }

    luaL_Reg funcs[] = {
        {"audio_buffer", create_audio_buffer},
        {"audio_node", create_audio_node},
        {"oscillator", create_oscillator_node},
        {"capture_audio", create_capture_node},
        {"sunvox", create_sunvox_node},
        {"track", create_audio_track_node},
        {"stream", create_audio_stream_node},
        {"load_audio", load_audio},
        {"root_audio_node", get_root_audio_node},
        {NULL, NULL}
    };
    am_open_module(L, AMULET_LUA_MODULE_NAME, funcs);
    register_audio_buffer_mt(L);
    register_audio_node_mt(L);
    register_gain_node_mt(L);
    register_lowpass_filter_node_mt(L);
    register_highpass_filter_node_mt(L);
    register_audio_track_node_mt(L);
    register_audio_stream_node_mt(L);
    register_oscillator_node_mt(L);
    register_capture_node_mt(L);
    register_spectrum_node_mt(L);
    register_sunvox_node_mt(L);

    audio_context.sample_rate = am_conf_audio_sample_rate;
    audio_context.sync_id = 0;
    audio_context.render_id = 0;

    // Create root audio node
    create_audio_node(L);
    audio_context.root = am_get_userdata(L, am_audio_node, -1);
    lua_rawseti(L, LUA_REGISTRYINDEX, AM_ROOT_AUDIO_NODE);
}

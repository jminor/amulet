// Bindings for librsynth

#include "amulet.h"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>

static float *buffer = NULL;
static int buffer_size = 0;

extern "C"
{
    #include "config.h"
    #include "rsynth/useconfig.h"
    #include "rsynth.h"

    char *default_dev_filename = "/dev/null";

    int audio_init(rsynth_synth *synth, int argc, char **argv)
    {
        // nothing to do here, this is just a vestige of rsynth's audio system
        return (0);
    }

    void audio_term(rsynth_synth *synth)
    {
        // nothing to do here, this is just a vestige of rsynth's audio system
    }

    void audio_play(rsynth_synth *synth, int n, short *data)
    {
        // Let's talk about memory allocation for a moment.
        // We're expecting just one call to audio_play for the whole
        // call to say_string. Right here, we allocate a float
        // buffer for the audio samples, and copy from rsynth's
        // 16-bit buffer into that.
        // Later, we're going to give ownership of this buffer
        // to an am_audio_buffer. That buffer will take care of
        // freeing this memory when it is deallocated.
        buffer = (float*)malloc(n * sizeof(float));
        buffer_size = n;

        // Copy from rsynth's signed short int buffer
        // into amulet's float buffer
        short *sp = data;
        float *fp = buffer;
        for (int i=0; i<n; i++) {
            *fp++ = *sp++ / (float)SHRT_MAX;
        }
    }

    int rsynth_start_audio(rsynth_synth *synth)
    {
        // nothing to do here, this is just a vestige of rsynth's audio system
        return 1;
    }

    int rsynth_sync_audio(rsynth_synth *synth)
    {
        // nothing to do here, this is just a vestige of rsynth's audio system
        return 1;
    }

    int rsynth_stop_audio(rsynth_synth *synth)
    {
        // nothing to do here, this is just a vestige of rsynth's audio system
        return 1;
    }

}

static int gen_rsynth_buffer(lua_State *L) {
    am_check_nargs(L, 1);
    const char* text = lua_tostring(L, 1);

    // TODO: Should we cache this? We should measure the cost
    // and maybe weigh against the ability to configure it?
    rsynth_synth *synth = rsynth_new();
    if (synth == NULL) {
        return luaL_error(L, "failed to create rsynth instance");
    }

    synth->verbose = false;

    char *prog = "rsynth";
    char *argv[] = {prog};
    int argc = rsynth_configure(synth, 1, argv);

    say_string(synth, (char *)text);

    am_buffer *buf = am_push_new_buffer_with_data(L, buffer_size * sizeof(float), buffer);
    am_audio_buffer *audio_buffer = am_new_userdata(L, am_audio_buffer);
    audio_buffer->sample_rate = synth->samp_rate;
    audio_buffer->num_channels = 1;
    audio_buffer->buffer = buf;
    audio_buffer->buffer_ref = audio_buffer->ref(L, -2);
    lua_remove(L, -2); // buf

    rsynth_destroy(synth);

    return 1;
}

static int text_to_phonemes(lua_State *L) {
    am_check_nargs(L, 1);
    const char* text = lua_tostring(L, 1);
 
    rsynth_synth *synth = rsynth_new();
    if (synth == NULL) {
        return luaL_error(L, "failed to create rsynth instance");
    }

    darray_t phone_array;
    darray_init(&phone_array, sizeof(char), 128);
    xlate_string(synth, (char*)text, &phone_array);

    char* result = (char *)darray_find(&phone_array, 0);
    lua_pushstring(L, result);

    darray_free(&phone_array);
    rsynth_destroy(synth);

    return 1;
}

void am_open_rsynth_module(lua_State *L) {
    luaL_Reg funcs[] = {
        {"speech_synth", gen_rsynth_buffer},
        {"text_to_phonemes", text_to_phonemes},
        {NULL, NULL}
    };
    am_open_module(L, AMULET_LUA_MODULE_NAME, funcs);
}

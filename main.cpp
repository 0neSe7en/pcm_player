#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>

#include <SDL2/SDL.h>
#include <array>

#include "includes/SPSCQueue.h"
#include "ebur128/ebur128.h"

using std::cerr;
using std::cout;
using std::endl;

enum FIXED_BY_MODE { INTEGRATED, SHORT_TERM };

static const auto SAMPLES = 1024;
static const auto PCM_BUFFER_SIZE = SAMPLES * 2 * 4;
static const auto TARGET_LOUDNESS = -16;
static const auto fixed_by = INTEGRATED;
static const auto START_CHUNK = 100;

// 存1000个长度为1024的sample，不是最好的办法，但别的办法有点麻烦
static rigtorp::SPSCQueue<std::array<float, SAMPLES * 2>> buffer_queue(1000);

static std::string file_path = "/Users/wsy/Downloads/diashengdiaqi.pcm";

struct Loudness {
    double integrated;
    double shortterm;
};


double calc_ratio(double target, double current) {
    if (current < -70) {
        return 1;
    }
    // 把两个lufs转为一个系数
    return std::pow(10, (target - std::max(-70.0, current)) / 20);
}

/* Audio Callback
 * The audio function callback takes the following parameters:
 * stream: A pointer to the audio buffer to be filled
 * len: The length (in bytes) of the audio buffer
 *
*/
void fill_audio(void *udata,Uint8 *stream,int len){
    //SDL 2.0
    SDL_memset(stream, 0, len);
    if (!buffer_queue.front()) {
        return;
    }
    auto *pcm_buf = buffer_queue.front();
    SDL_MixAudio(stream, (Uint8 *)pcm_buf -> data(), len, SDL_MIX_MAXVOLUME);
    buffer_queue.pop();
}


int main(int argc, char * argv[])
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        cerr << "can't open audio" << endl;
        return -1;
    }

    auto loudness = Loudness{0, 0};

    ebur128_state *state = ebur128_init(2, 44100, EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_S);

    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = 44100;
    wanted_spec.format = AUDIO_F32SYS;
    wanted_spec.channels = 2;
    wanted_spec.silence = 0;
    wanted_spec.samples = 1024;
    wanted_spec.callback = fill_audio;

    if (SDL_OpenAudio(&wanted_spec, nullptr) < 0) {
        cerr << "can't open audio" << endl;
        return -1;
    }

    std::ifstream input_file(file_path, std::ios::binary);

    if (!input_file.is_open()) {
        cerr << "can't open file: " << file_path << endl;
        return -1;
    }

    long current_chunk = 0;

    SDL_PauseAudio(0);

    auto pcm_buffer = std::array<char, PCM_BUFFER_SIZE>();

    while (!input_file.eof()) {
        input_file.read(pcm_buffer.data(), pcm_buffer.size());
        current_chunk += 1;
        auto float_buffer = std::array<float, SAMPLES * 2>();
        memcpy(float_buffer.data(), pcm_buffer.data(), PCM_BUFFER_SIZE);

        ebur128_add_frames_float(state, (float *)pcm_buffer.data(), SAMPLES);
        ebur128_loudness_global(state, &loudness.integrated);
        ebur128_loudness_shortterm(state, &loudness.shortterm);

        auto ratio = calc_ratio(TARGET_LOUDNESS, fixed_by == INTEGRATED ? loudness.integrated : loudness.shortterm);

        if (current_chunk >= START_CHUNK && fixed_by == INTEGRATED) {
            // 为了intergrated稳定，先算START_CHUNK帧
            // 避免音量忽大忽小
            for (float & i : float_buffer) {
                // TODO: true peak and limiter
                i = ratio * i;
            }
        }

        cout << "\t integrated:" << loudness.integrated << "\t shortterm:" << loudness.shortterm << "\t ratio:" << ratio << endl;

        buffer_queue.push(float_buffer);
        if (buffer_queue.size() > 100) {
            SDL_Delay(1);
        }
    }

    SDL_Quit();

    return 0;
}
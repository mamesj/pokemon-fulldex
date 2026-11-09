#include "../frontend/audio_fast_forward.h"
#include <cassert>
#include <vector>

int main()
{
    for (int speed : {1, 2, 3, 4, 8})
    {
        std::vector<int16_t> samples(800 * speed * 2);
        for (int i = 0; i < 800 * speed; ++i)
        {
            samples[i * 2] = static_cast<int16_t>(i);
            samples[i * 2 + 1] = static_cast<int16_t>(-i);
        }
        assert(AudioFastForward(samples.data(), 800 * speed, speed) == 800);
        for (int i = 0; i < 800; ++i)
        {
            const int expected = i * speed + (speed - 1) / 2;
            assert(samples[i * 2] == expected);
            assert(samples[i * 2 + 1] == -expected);
        }
        // A new sound is heard in the next batch, with no stored old music.
        std::fill(samples.begin(), samples.end(), -9000);
        assert(AudioFastForward(samples.data(), 800 * speed, speed) == 800);
        for (int i = 0; i < 1600; ++i) assert(samples[i] == -9000);
    }
    int16_t tiny[] = {32767, -32768, 32767, -32768, 32767, -32768};
    assert(AudioFastForward(tiny, 3, 2) == 1);
    assert(tiny[0] == 32767 && tiny[1] == -32768);
    assert(AudioFastForward(tiny, 0, 8) == 0);
}

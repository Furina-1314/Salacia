#include "vertical_mixer.h"

#include <assert.h>
#include <stdio.h>

static void expect(const int16_t actual[4], int a, int b, int c, int d)
{
    assert(actual[0] == a && actual[1] == b &&
           actual[2] == c && actual[3] == d);
}

int main(void)
{
    int16_t commands[4];

    assert(VerticalMixer_Mix(0, 0.0f, 5.0f, commands) ==
           VERTICAL_MIXER_STATUS_OK);
    expect(commands, -5, 5, -5, 5);
    assert(VerticalMixer_Mix(0, -4.0f, 0.0f, commands) ==
           VERTICAL_MIXER_STATUS_OK);
    expect(commands, -4, -4, 4, 4);
    assert(VerticalMixer_Mix(20, 8.0f, 7.0f, commands) ==
           VERTICAL_MIXER_STATUS_OK);
    expect(commands, 21, 30, 10, 19);
    assert(VerticalMixer_Mix(95, 10.0f, 10.0f, commands) ==
           VERTICAL_MIXER_STATUS_OK);
    expect(commands, 95, 100, 85, 95);
    assert(VerticalMixer_Mix(-95, -10.0f, -10.0f, commands) ==
           VERTICAL_MIXER_STATUS_OK);
    expect(commands, -95, -100, -85, -95);
    puts("vertical_mixer_test: PASS");
    return 0;
}

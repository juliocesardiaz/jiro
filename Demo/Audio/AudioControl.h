#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

// One owner and one render thread per strip. Stop/destroy its IOProc before
// destroying the strip. SetControls and meter reads may run concurrently.
typedef struct SPStrip SPStrip;
typedef struct SPBuffer {
    float *data;
    uint32_t channels;
    uint32_t byteSize;
} SPBuffer;

SPStrip *SPStripCreate(double sampleRate, uint32_t inputChannelOffset);
void SPStripDestroy(SPStrip *strip);
// Gain is 0...1; balance is -1...1. Center preserves unity on both channels.
void SPStripSetControls(SPStrip *strip, float gain, float balance, bool muted);
float SPStripTakePeak(SPStrip *strip);
uint32_t SPStripCallbackCount(SPStrip *strip);
bool SPStripHasFormatFault(SPStrip *strip);
// Float32 packed PCM; separate input/output storage; interleaved or planar.
// Missing inputs, extra outputs, short buffers, and null data are silenced.
void SPStripProcess(SPStrip *strip, const SPBuffer *inputs, uint32_t inputCount,
                    const SPBuffer *outputs, uint32_t outputCount);
#ifdef __APPLE__
OSStatus SPStripInstallIOProc(AudioObjectID device, SPStrip *strip,
                             AudioDeviceIOProcID *proc);
#endif

#ifdef __cplusplus
}
#endif

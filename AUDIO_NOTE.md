# Temp Audio Notes

Currently only one audio channel is operational, but it works approximately the way I envision the other three channels
eventually working.  The registers will be remapped also (no room for other channels, and borrowing bits from
video).

There will be a separate `AUDIO_CTRL` register, but for now I am using bit 4 of `VID_CTRL` for audio channel 0 control/status.

To enable AUDIO 0 channel output, set bit 4 of `XR_VID_CTRL` to a `1` (e.g., 0x0010)

When read, bit 4 of `XR_VID_CTRL` will be a `1` if a new audio start/length is pending when current sample length
exhausted (i.e., `XR_AUD0_START` and/or `XR_AUD0_START` set, but channel has not loaded them yet).  You can _force_ a
channel to restart with bit 15 of `XR_AUD0_PERIOD` [untested]).

There are four channel parameter registers (very similar to Amiga):

- `XR_AUD0_VOL` - channel volume/pan
    left volume `[15:8]`, right volume `[7:0]`
    (e.g., 0x8040 = 1.0 left, 0.5 right)  
    NOTE: I plan to add clipping, but currently audio "wraps" if exceeds 8 bits.
- `XR_AUD0_PERIOD` `[14:0]` channel period -1 (sample rate), `[15]` force channel restart  
    Number of pixel clocks between sample output (1-32768)
    but cannot consistently output new sample data faster than DMA (one word per scanline, ~1088 cycles)
    Example sample rate calculation:
- `XR_AUD0_START` start address for sample data  
    Samples are 16-bit with (two 8 bit signed samples, high byte first)
    TILE memory if `XR_AUD0_LENGTH[15]` is set, otherwise VRAM [untested]
- `XR_AUD0_LENGTH` sample length in words -1, `[15]` `0`=VRAM/`1`=TILEMEM for sample data  
    1-32768 words with two 8-bit samples

Example play sample function (from xosera_test_m68k, NOTE: this assumes audio channel was already started and is looping
silence):

```c
static void play_sample(uint16_t vaddr, uint16_t len, uint16_t rate)
{
    // set sample start address and lengthß
    xreg_setw(AUD0_START, vaddr);
    xreg_setw(AUD0_LENGTH, (len / 2) - 1);

    // set sample period
    uint32_t clk_hz = xreg_getw(VID_HSIZE) > 640 ? 33750000 : 25125000;
    uint16_t period = (clk_hz + rate - 1) / rate;   // rate is samples per second
    xreg_setw(AUD0_PERIOD, period);

    // hack to make sure above sample was started
    // NOTE: should be able to poll VID_CTRL bit 4 now
    wait_scanline();

    // queue silence after sample
    xreg_setw(AUD0_START, SILENCE_VADDR);   // address of 0x0000 silence sample
    xreg_setw(AUD0_LENGTH, 0);              // length 1, to reload next sample ASAP
}
```

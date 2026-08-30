#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <vector>
#pragma comment(lib, "winmm.lib")

void PlayChordWave(const std::vector<double>& frequencies, DWORD durationMs)
{
    const int sampleRate = 44100;
    int totalSamples = (sampleRate * durationMs) / 1000;

    // 生成复合波形（各频率叠加后归一化）
    std::vector<short> buffer(totalSamples);
    for (int i = 0; i < totalSamples; i++)
    {
        double t = (double)i / sampleRate;
        double sample = 0.0;

        for (double freq : frequencies)
        {
            sample += sin(2 * M_PI * freq * t);
        }

        // 归一化到 [-1, 1]，然后转为16位整数
        sample /= frequencies.size();
        buffer[i] = (short)(sample * 32767);
    }

    // 设置波形格式
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = sampleRate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = sampleRate * 2;

    // 打开音频设备
    HWAVEOUT hwo;
    if (waveOutOpen(&hwo, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        return;

    // 准备并播放
    WAVEHDR wh = {};
    wh.lpData = (LPSTR)buffer.data();
    wh.dwBufferLength = totalSamples * 2;
    wh.dwFlags = 0;

    waveOutPrepareHeader(hwo, &wh, sizeof(WAVEHDR));
    waveOutWrite(hwo, &wh, sizeof(WAVEHDR));

    // 等待播放完成（也可以用回调实现非阻塞）
    Sleep(durationMs);

    // 清理
    waveOutUnprepareHeader(hwo, &wh, sizeof(WAVEHDR));
    waveOutClose(hwo);
}

int main()
{
    // C大调和弦：C4(261.63), E4(329.63), G4(392.00)
    PlayChordWave({261.63, 329.63, 392.00}, 2000);

    // 七和弦：C4(261.63), E4(329.63), G4(392.00), Bb4(466.16)
    PlayChordWave({261.63, 329.63, 392.00, 466.16}, 1500);

    return 0;
}

#ifndef __SDLAUDIOJNI_H__
#define __SDLAUDIOJNI_H__

namespace ff{
	extern int Android_JNI_OpenAudioDevice(int sampleRate, int is16Bit, int channelCount, int desiredBufferFrames);
	extern void* Android_JNI_GetAudioBuffer();
	extern void Android_JNI_WriteAudioBuffer();
	extern void Android_JNI_CloseAudioDevice();
}
#endif
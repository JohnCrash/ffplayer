#include "SDLAudioJNI.h"
#include <jni.h>
#include "jni/JniHelper.h"
#include "../ljcommon/AppDelegateBase.h"

namespace ff
{
	/* Main activity */
	static jclass mActivityClass;
	
	/* method signatures */
	
	static jboolean audioBuffer16Bit = JNI_FALSE;
	static jboolean audioBufferStereo = JNI_FALSE;
	static jobject audioBuffer = nullptr;
	static void *audioBufferPinned = nullptr;

	static JniMethodInfo jim_audioInit;
	static JniMethodInfo jim_audioWriteShortBuffer;
	static JniMethodInfo jim_audioWriteByteBuffer;
	static JniMethodInfo jim_audioQuit;
	static bool bMethodInfoExist = false;

	static bool bInited = false;

	int Android_JNI_OpenAudioDevice(int sampleRate, int is16Bit, int channelCount, int desiredBufferFrames)
	{
		int audioBufferFrames;
		
		__android_log_print(ANDROID_LOG_VERBOSE, "SDL", "SDL audio: opening device, rate: %d, 16Bit: %d, cnChannel: %d, cnFrame: %d", sampleRate, is16Bit, channelCount, desiredBufferFrames);

		char szFullClassName[256];
		sprintf(szFullClassName, "com/lj/%s/%s", g_pTheApp->GetAppName().c_str(), g_pTheApp->GetAppName().c_str());

		bInited = false;

		JNIEnv *env = JniHelper::getEnv();

		if (!bMethodInfoExist)
		{
			//只获取一次
			if (!JniHelper::getStaticMethodInfo(jim_audioInit, szFullClassName, "audioInit", "(IZZI)I") ||
				!JniHelper::getStaticMethodInfo(jim_audioWriteShortBuffer, szFullClassName, "audioWriteShortBuffer", "([S)V") ||
				!JniHelper::getStaticMethodInfo(jim_audioWriteByteBuffer, szFullClassName, "audioWriteByteBuffer", "([B)V") ||
				!JniHelper::getStaticMethodInfo(jim_audioQuit, szFullClassName, "audioQuit", "()V"))
			{
				__android_log_print(ANDROID_LOG_WARN, "SDL", "SDL audio: error on JniHelper::getStaticMethodInfo!");
				return 0;
			}
			jim_audioInit.classID = (jclass)env->NewGlobalRef(jim_audioInit.classID);
			jim_audioWriteShortBuffer.classID = (jclass)env->NewGlobalRef(jim_audioWriteShortBuffer.classID);
			jim_audioWriteByteBuffer.classID = (jclass)env->NewGlobalRef(jim_audioWriteByteBuffer.classID);
			jim_audioQuit.classID = (jclass)env->NewGlobalRef(jim_audioQuit.classID);

			bMethodInfoExist = true;
		}

		audioBuffer16Bit = is16Bit;
		audioBufferStereo = channelCount > 1;
		if (env->CallStaticIntMethod(jim_audioInit.classID,jim_audioInit.methodID,sampleRate, audioBuffer16Bit, audioBufferStereo, desiredBufferFrames)!=0)
		{
			__android_log_print(ANDROID_LOG_WARN, "SDL", "SDL audio: error on AudioTrack initialization!");
			env->DeleteLocalRef(jim_audioInit.classID);
			return 0;
		}

		if (is16Bit)
		{
			jshortArray audioBufferLocal = env->NewShortArray(desiredBufferFrames * (audioBufferStereo ? 2 : 1));
			if (audioBufferLocal)
			{
				audioBuffer = env->NewGlobalRef(audioBufferLocal);
				env->DeleteLocalRef(audioBufferLocal);
			}
		}
		else
		{
			jbyteArray audioBufferLocal = env->NewByteArray(desiredBufferFrames * (audioBufferStereo ? 2 : 1));
			if (audioBufferLocal)
			{
				audioBuffer = env->NewGlobalRef(audioBufferLocal);
				env->DeleteLocalRef(audioBufferLocal);
			}
		}		
		if (audioBuffer == nullptr)
		{
			__android_log_print(ANDROID_LOG_WARN, "SDL", "SDL audio: could not allocate an audio buffer!");
			return 0;
		}		
		jboolean isCopy = JNI_FALSE;
		if (audioBuffer16Bit)
		{
			audioBufferPinned = env->GetShortArrayElements((jshortArray)audioBuffer, &isCopy);
			audioBufferFrames = env->GetArrayLength((jshortArray)audioBuffer);
		}
		else
		{
			audioBufferPinned = env->GetByteArrayElements((jbyteArray)audioBuffer, &isCopy);
			audioBufferFrames = env->GetArrayLength((jbyteArray)audioBuffer);
		}
		if (audioBufferStereo)
		{
			audioBufferFrames /= 2;
		}		

		if (audioBuffer == NULL || audioBufferPinned == NULL)
		{
			__android_log_print(ANDROID_LOG_WARN, "SDL", "OpenAudioDevice audioBuffer failed !");
			return 0;
		}
		__android_log_print(ANDROID_LOG_WARN, "SDL", "OpenAudioDevice done !");

		bInited = true;
		return audioBufferFrames;
	}

	void *Android_JNI_GetAudioBuffer()
	{
		if (!bInited) return nullptr;
		return audioBufferPinned;
	}

	void Android_JNI_WriteAudioBuffer()
	{
		if (!bInited) return;
		JNIEnv *env = JniHelper::getEnv();

		if (audioBuffer16Bit)
		{
			env->ReleaseShortArrayElements((jshortArray)audioBuffer, (jshort *)audioBufferPinned, JNI_COMMIT);
			env->CallStaticVoidMethod(jim_audioWriteShortBuffer.classID,jim_audioWriteShortBuffer.methodID, (jshortArray)audioBuffer);
		}
		else
		{
			env->ReleaseByteArrayElements((jbyteArray)audioBuffer, (jbyte *)audioBufferPinned, JNI_COMMIT);
			env->CallStaticVoidMethod(jim_audioWriteByteBuffer.classID,jim_audioWriteByteBuffer.methodID, (jbyteArray)audioBuffer);
		}	
	}

	void Android_JNI_CloseAudioDevice()
	{
		if (!bInited) return;

		JNIEnv *env = JniHelper::getEnv();

		__android_log_print(ANDROID_LOG_WARN, "SDL", "SDL audio: close audio device!");
		env->CallStaticVoidMethod(jim_audioQuit.classID, jim_audioQuit.methodID);
		
		/*env->DeleteLocalRef(jim_audioInit.classID);
		env->DeleteLocalRef(jim_audioWriteShortBuffer.classID);
		env->DeleteLocalRef(jim_audioWriteByteBuffer.classID);
		env->DeleteLocalRef(jim_audioQuit.classID);*/
		
		if( audioBuffer )
		{
			env->DeleteGlobalRef(audioBuffer);
			audioBuffer = nullptr;
			audioBufferPinned = nullptr;
		}
		__android_log_print(ANDROID_LOG_WARN, "SDL", "SDL audio: close audio device done!");
	}
}

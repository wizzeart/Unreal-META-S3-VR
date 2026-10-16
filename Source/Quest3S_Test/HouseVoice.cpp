#include "HouseVoice.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace HouseVoice
{
bool IsValidName(const FString& Name)
{
    if (Name.Len() != 36 || !Name.EndsWith(TEXT(".wav"))) return false;
    for (int32 Index = 0; Index < 32; ++Index)
        if (!FChar::IsHexDigit(Name[Index])) return false;
    return true;
}

FString FilePath(const FString& HousesDirectory, const FString& Name)
{
    return IsValidName(Name) ? FPaths::Combine(HousesDirectory, TEXT("Voice"), Name) : FString();
}

bool LoadPcm(const FString& HousesDirectory, const FString& Name, TArray<uint8>& Pcm)
{
    Pcm.Empty();
    const FString Path = FilePath(HousesDirectory, Name);
    if (Path.IsEmpty()) return false;
    TArray<uint8> Wav;
    if (!FFileHelper::LoadFileToArray(Wav, *Path) || Wav.Num() < 44
        || Wav.Num() > 44 + SampleRate * 2 * MaxSeconds) return false;
    auto U16 = [&](int32 Offset) { return uint32(Wav[Offset]) | (uint32(Wav[Offset + 1]) << 8); };
    auto U32 = [&](int32 Offset) { return U16(Offset) | (U16(Offset + 2) << 16); };
    if (FMemory::Memcmp(Wav.GetData(), "RIFF", 4) || FMemory::Memcmp(Wav.GetData() + 8, "WAVEfmt ", 8)
        || FMemory::Memcmp(Wav.GetData() + 36, "data", 4) || U32(16) != 16
        || U16(20) != 1 || U16(22) != 1 || U32(24) != SampleRate || U16(34) != 16
        || U32(40) == 0 || U32(40) > uint32(Wav.Num() - 44) || U32(40) % 2 != 0) return false;
    Pcm.Append(Wav.GetData() + 44, U32(40));
    return true;
}

int32 StartRecording(const FString& Name)
{
#if PLATFORM_ANDROID
    if (!IsValidName(Name)) return -1;
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
            "AndroidThunkJava_StartHouseVoice", "(Ljava/lang/String;)I", false);
        if (Method)
        {
            jstring JavaName = Env->NewStringUTF(TCHAR_TO_UTF8(*Name));
            const int32 State = FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, Method, JavaName);
            Env->DeleteLocalRef(JavaName);
            return State;
        }
    }
#endif
    return -1;
}

void StopRecording()
{
#if PLATFORM_ANDROID
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
            "AndroidThunkJava_StopHouseVoice", "()V", false);
        if (Method) FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
    }
#endif
}

int32 RecordingState()
{
#if PLATFORM_ANDROID
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
            "AndroidThunkJava_GetHouseVoiceState", "()I", false);
        if (Method) return FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, Method);
    }
#endif
    return -1;
}
}

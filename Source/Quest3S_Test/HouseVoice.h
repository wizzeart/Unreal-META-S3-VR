#pragma once

#include "CoreMinimal.h"

namespace HouseVoice
{
    constexpr int32 SampleRate = 16000;
    constexpr float MaxSeconds = 30.f;
    bool IsValidName(const FString& Name);
    FString FilePath(const FString& HousesDirectory, const FString& Name);
    bool LoadPcm(const FString& HousesDirectory, const FString& Name, TArray<uint8>& Pcm);
    // 1: recording, 0: waiting for microphone permission, negative: error.
    int32 StartRecording(const FString& Name);
    void StopRecording();
    // 0: waiting, 1: recording, 2: saved, -2: denied, -1: failed.
    int32 RecordingState();
}

#pragma once
#include "CoreMinimal.h"

class UProceduralMeshComponent;

namespace HouseGlb
{
    bool BuildMesh(const TArray<uint8>& Bytes, UProceduralMeshComponent* Mesh, FVector& Min, FVector& Max, float& FloorZ, int64& TriangleCount, FString& Error);
}

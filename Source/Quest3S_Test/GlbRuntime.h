#pragma once
#include "CoreMinimal.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

struct FHouseMaterialFeatures
{
    int32 NormalMaps = 0;
    int32 OcclusionMaps = 0;
    int32 MetalRoughMaps = 0;
    int32 EmissiveMaps = 0;
    TArray<float> NormalScales;
    TArray<float> OcclusionStrengths;
};

namespace HouseGlb
{
    bool BuildMesh(const TArray<uint8>& Bytes, UProceduralMeshComponent* Mesh, FVector& Min, FVector& Max,
        float& FloorZ, int64& TriangleCount, TArray<UMaterialInstanceDynamic*>& LitMaterials,
        TArray<UMaterialInstanceDynamic*>& ReferenceMaterials, FHouseMaterialFeatures& Features, FString& Error);
}

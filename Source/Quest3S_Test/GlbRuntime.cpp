#include "GlbRuntime.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
uint32 U32(const uint8* P) { return uint32(P[0]) | uint32(P[1]) << 8 | uint32(P[2]) << 16 | uint32(P[3]) << 24; }
const TArray<TSharedPtr<FJsonValue>>* Array(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
{
    const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
    return O.IsValid() && O->TryGetArrayField(Key, A) ? A : nullptr;
}
TSharedPtr<FJsonObject> ObjectAt(const TArray<TSharedPtr<FJsonValue>>* A, int32 I)
{
    return A && A->IsValidIndex(I) ? (*A)[I]->AsObject() : nullptr;
}
int32 Number(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, int32 Default = INDEX_NONE)
{
    double Value;
    return O.IsValid() && O->TryGetNumberField(Key, Value) ? static_cast<int32>(Value) : Default;
}
FString String(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
{
    FString Value;
    if (O.IsValid()) O->TryGetStringField(Key, Value);
    return Value;
}
struct FAccessorView
{
    const uint8* Data = nullptr;
    int32 Count = 0, Components = 0, ComponentBytes = 0, Stride = 0, ComponentType = 0;
    bool Valid() const { return Data && Count > 0 && Components > 0; }
    double Value(int32 Index, int32 Component) const
    {
        const uint8* P = Data + Index * Stride + Component * ComponentBytes;
        switch (ComponentType)
        {
        case 5121: return P[0];
        case 5123: return uint16(P[0] | uint16(P[1]) << 8);
        case 5125: return U32(P);
        case 5126: { float F; FMemory::Memcpy(&F, P, 4); return F; }
        default: return 0;
        }
    }
};
FVector Convert(const FVector& V) { return FVector(-V.Z, V.X, V.Y) * 100.; }
FVector ConvertNormal(const FVector& V) { return FVector(-V.Z, V.X, V.Y).GetSafeNormal(); }
FVector ReadVec(const FAccessorView& A, int32 I)
{
    return FVector(A.Value(I, 0), A.Value(I, 1), A.Value(I, 2));
}
FVector JsonVec3(const TArray<TSharedPtr<FJsonValue>>* A, const FVector& Default)
{
    return A && A->Num() == 3 ? FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber()) : Default;
}
}

bool HouseGlb::BuildMesh(const TArray<uint8>& Bytes, UProceduralMeshComponent* Mesh, FVector& Min, FVector& Max, float& FloorZ, int64& TriangleCount, FString& Error)
{
    if (Bytes.Num() < 20 || U32(Bytes.GetData()) != 0x46546c67 || U32(Bytes.GetData()+4) != 2 || U32(Bytes.GetData()+8) != static_cast<uint32>(Bytes.Num()))
    {
        Error = TEXT("Cabecera GLB invalida"); return false;
    }
    const uint32 JsonSize = U32(Bytes.GetData()+12);
    if (U32(Bytes.GetData()+16) != 0x4e4f534a || JsonSize > static_cast<uint32>(Bytes.Num()-20))
    {
        Error = TEXT("Bloque JSON invalido"); return false;
    }
    const int64 BinHeader = 20LL + JsonSize;
    if (BinHeader + 8 > Bytes.Num() || U32(Bytes.GetData()+BinHeader+4) != 0x004e4942 || U32(Bytes.GetData()+BinHeader) > Bytes.Num()-BinHeader-8)
    {
        Error = TEXT("Bloque binario invalido"); return false;
    }
    const uint8* Bin = Bytes.GetData()+BinHeader+8;
    const int64 BinLength = U32(Bytes.GetData()+BinHeader);
    FUTF8ToTCHAR Decoded(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()+20), JsonSize);
    FString Json(Decoded.Length(), Decoded.Get());
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
    {
        Error = TEXT("JSON GLB invalido"); return false;
    }
    const auto* Accessors = Array(Root, TEXT("accessors"));
    const auto* Views = Array(Root, TEXT("bufferViews"));
    const auto* Meshes = Array(Root, TEXT("meshes"));
    const auto* Nodes = Array(Root, TEXT("nodes"));
    const auto* Materials = Array(Root, TEXT("materials"));
    const auto* Textures = Array(Root, TEXT("textures"));
    const auto* Images = Array(Root, TEXT("images"));
    UMaterialInterface* OpaqueMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided.Widget3DPassThrough_Opaque_OneSided"));
    UMaterialInterface* TranslucentMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided.Widget3DPassThrough_Translucent_OneSided"));
    UMaterialInterface* MaskedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked_OneSided.Widget3DPassThrough_Masked_OneSided"));
    UTexture2D* WhiteTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: materials opaque=%d translucent=%d masked=%d"),
        OpaqueMaterial != nullptr, TranslucentMaterial != nullptr, MaskedMaterial != nullptr);
    TMap<int32, UTexture2D*> LoadedImages;
    int64 DecodedTextureBytes = 0;
    int32 SuccessfulImageCount = 0;
    int32 AppliedMaterialCount = 0;
    auto ImageForTexture = [&](int32 TextureIndex) -> UTexture2D*
    {
        const int32 ImageIndex = Number(ObjectAt(Textures, TextureIndex), TEXT("source"));
        if (ImageIndex < 0) return nullptr;
        if (UTexture2D** Existing = LoadedImages.Find(ImageIndex)) return *Existing;
        UTexture2D* Result = nullptr;
        const auto Image = ObjectAt(Images, ImageIndex);
        const auto BufferView = ObjectAt(Views, Number(Image, TEXT("bufferView")));
        const FString Mime = String(Image, TEXT("mimeType"));
        const int64 Start = Number(BufferView, TEXT("byteOffset"), 0);
        const int64 Length = Number(BufferView, TEXT("byteLength"), 0);
        if (BufferView.IsValid() && Number(BufferView, TEXT("buffer"), 0) == 0
            && Start >= 0 && Length > 0 && Start + Length <= BinLength
            && (Mime == TEXT("image/png") || Mime == TEXT("image/jpeg")))
        {
            IImageWrapperModule& ImageModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
            const EImageFormat Format = Mime == TEXT("image/png") ? EImageFormat::PNG : EImageFormat::JPEG;
            const TSharedPtr<IImageWrapper> Wrapper = ImageModule.CreateImageWrapper(Format);
            if (Wrapper.IsValid() && Wrapper->SetCompressed(Bin + Start, Length)
                && Wrapper->GetWidth() > 0 && Wrapper->GetHeight() > 0
                && Wrapper->GetWidth() <= 4096 && Wrapper->GetHeight() <= 4096)
            {
                const int32 SourceWidth = static_cast<int32>(Wrapper->GetWidth());
                const int32 SourceHeight = static_cast<int32>(Wrapper->GetHeight());
                const int32 Scale = FMath::Max(1, FMath::Max((SourceWidth + 1023) / 1024, (SourceHeight + 1023) / 1024));
                const int32 Width = (SourceWidth + Scale - 1) / Scale;
                const int32 Height = (SourceHeight + Scale - 1) / Scale;
                const int64 TextureBytes = int64(Width) * Height * 4;
                if (TextureBytes <= 128LL * 1024 * 1024 - DecodedTextureBytes)
                {
                    TArray64<uint8> Pixels;
                    if (Wrapper->GetRaw(ERGBFormat::BGRA, 8, Pixels)
                        && Pixels.Num() == int64(SourceWidth) * SourceHeight * 4)
                    {
                        Result = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
                        if (Result && Result->GetPlatformData() && Result->GetPlatformData()->Mips.Num() > 0)
                        {
                            FTexture2DMipMap& Mip = Result->GetPlatformData()->Mips[0];
                            uint8* Target = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
                            if (Scale == 1) FMemory::Memcpy(Target, Pixels.GetData(), TextureBytes);
                            else for (int32 Y = 0; Y < Height; ++Y) for (int32 X = 0; X < Width; ++X)
                            {
                                const uint8* Source = Pixels.GetData() + (int64(Y * Scale) * SourceWidth + X * Scale) * 4;
                                FMemory::Memcpy(Target + (int64(Y) * Width + X) * 4, Source, 4);
                            }
                            Mip.BulkData.Unlock();
                            Result->SRGB = true;
                            Result->NeverStream = true;
                            Result->UpdateResource();
                            DecodedTextureBytes += TextureBytes;
                            ++SuccessfulImageCount;
                        }
                        else Result = nullptr;
                    }
                }
            }
        }
        LoadedImages.Add(ImageIndex, Result);
        return Result;
    };
    auto View = [&](int32 AccessorIndex) -> FAccessorView
    {
        FAccessorView Out;
        auto A = ObjectAt(Accessors, AccessorIndex);
        auto V = ObjectAt(Views, Number(A, TEXT("bufferView")));
        if (!A.IsValid() || !V.IsValid() || Number(V, TEXT("buffer"), 0) != 0) return Out;
        Out.Count = Number(A, TEXT("count"), 0);
        Out.ComponentType = Number(A, TEXT("componentType"), 0);
        Out.ComponentBytes = Out.ComponentType == 5121 ? 1 : Out.ComponentType == 5123 ? 2 : 4;
        const FString Type = String(A, TEXT("type"));
        Out.Components = Type == TEXT("SCALAR") ? 1 : Type == TEXT("VEC2") ? 2 : Type == TEXT("VEC3") ? 3 : Type == TEXT("VEC4") ? 4 : 0;
        Out.Stride = Number(V, TEXT("byteStride"), Out.Components * Out.ComponentBytes);
        const int64 Offset = int64(Number(V, TEXT("byteOffset"), 0)) + Number(A, TEXT("byteOffset"), 0);
        const int64 ViewLength = Number(V, TEXT("byteLength"), 0);
        if (Out.Count <= 0 || Out.Count > 2000000 || Out.Components == 0 || Out.Stride < Out.Components * Out.ComponentBytes || Offset < 0 || ViewLength <= 0 || Offset + int64(Out.Count-1)*Out.Stride + Out.Components*Out.ComponentBytes > BinLength || Offset + int64(Out.Count-1)*Out.Stride + Out.Components*Out.ComponentBytes > int64(Number(V, TEXT("byteOffset"), 0)) + ViewLength) return FAccessorView();
        Out.Data = Bin + Offset;
        return Out;
    };
    Min = FVector(UE_BIG_NUMBER);
    Max = FVector(-UE_BIG_NUMBER);
    TriangleCount = 0;
    TMap<int32, double> HorizontalAreaByHeight;
    int32 Section = 0;
    TFunction<void(int32, const FTransform&, int32)> Visit;
    Visit = [&](int32 Index, const FTransform& Parent, int32 Depth)
    {
        if (Depth > 128) return;
        auto N = ObjectAt(Nodes, Index);
        if (!N.IsValid()) return;
        FTransform Local;
        Local.SetTranslation(JsonVec3(Array(N, TEXT("translation")), FVector::ZeroVector));
        Local.SetScale3D(JsonVec3(Array(N, TEXT("scale")), FVector::OneVector));
        if (const auto* R = Array(N, TEXT("rotation")); R && R->Num() == 4)
            Local.SetRotation(FQuat((*R)[0]->AsNumber(), (*R)[1]->AsNumber(), (*R)[2]->AsNumber(), (*R)[3]->AsNumber()));
        if (const auto* M = Array(N, TEXT("matrix")); M && M->Num() == 16)
        {
            FMatrix Matrix = FMatrix::Identity;
            for (int32 Row=0; Row<4; ++Row) for (int32 Col=0; Col<4; ++Col) Matrix.M[Row][Col] = (*M)[Row*4+Col]->AsNumber();
            Local = FTransform(Matrix);
        }
        const FTransform World = Local * Parent;
        auto M = ObjectAt(Meshes, Number(N, TEXT("mesh")));
        if (const auto* Prims = Array(M, TEXT("primitives"))) for (const auto& Value : *Prims)
        {
            auto P = Value->AsObject();
            const auto Material = ObjectAt(Materials, Number(P, TEXT("material")));
            if (Number(P, TEXT("mode"), 4) != 4) continue;
            const TSharedPtr<FJsonObject>* AttrPtr = nullptr;
            if (P.IsValid()) P->TryGetObjectField(TEXT("attributes"), AttrPtr);
            TSharedPtr<FJsonObject> Attr = AttrPtr ? *AttrPtr : nullptr;
            FAccessorView Pos = View(Number(Attr, TEXT("POSITION")));
            FAccessorView Normal = View(Number(Attr, TEXT("NORMAL")));
            FAccessorView Tex = View(Number(Attr, TEXT("TEXCOORD_0")));
            FAccessorView Indices = View(Number(P, TEXT("indices")));
            if (!Pos.Valid() || Pos.Components != 3 || Pos.ComponentType != 5126 || TriangleCount + (Indices.Valid() ? Indices.Count : Pos.Count)/3 > 350000) continue;
            TArray<FVector> Vertices, Normals;
            TArray<FVector2D> UV;
            TArray<int32> Triangles;
            Vertices.Reserve(Pos.Count); Normals.Reserve(Pos.Count); UV.Reserve(Pos.Count);
            for (int32 I=0; I<Pos.Count; ++I)
            {
                const FVector Point = Convert(World.TransformPosition(ReadVec(Pos, I)));
                Vertices.Add(Point); Min = Min.ComponentMin(Point); Max = Max.ComponentMax(Point);
                Normals.Add(Normal.Valid() && Normal.Count == Pos.Count && Normal.Components == 3 ? ConvertNormal(World.TransformVectorNoScale(ReadVec(Normal, I))) : FVector::UpVector);
                UV.Add(Tex.Valid() && Tex.Count == Pos.Count && Tex.Components == 2 ? FVector2D(Tex.Value(I,0), Tex.Value(I,1)) : FVector2D::ZeroVector);
            }
            const int32 Count = Indices.Valid() ? Indices.Count : Pos.Count;
            for (int32 I=0; I+2<Count; I+=3)
            {
                const int32 A = Indices.Valid() ? Indices.Value(I,0) : I;
                const int32 B = Indices.Valid() ? Indices.Value(I+1,0) : I+1;
                const int32 C = Indices.Valid() ? Indices.Value(I+2,0) : I+2;
                if (Vertices.IsValidIndex(A) && Vertices.IsValidIndex(B) && Vertices.IsValidIndex(C))
                {
                    // glTF -> Unreal swaps handedness (Convert has a negative determinant).
                    // Reverse each triangle so the visible surface remains front-facing.
                    Triangles.Add(A); Triangles.Add(C); Triangles.Add(B);
                    const FVector Cross = FVector::CrossProduct(Vertices[B] - Vertices[A], Vertices[C] - Vertices[A]);
                    const double Area = Cross.Size() * 0.5;
                    if (Area > 1. && FMath::Abs(Cross.Z) > 0.9 * Cross.Size())
                    {
                        const float Height = (Vertices[A].Z + Vertices[B].Z + Vertices[C].Z) / 3.f;
                        HorizontalAreaByHeight.FindOrAdd(FMath::RoundToInt(Height / 10.f)) += Area;
                    }
                }
            }
            if (!Triangles.IsEmpty())
            {
                // Every source face needs a front-facing copy on each side: the
                // Widget3D two-sided parent paints its back face solid black.
                if (TriangleCount + 2 * Triangles.Num() / 3 <= 350000)
                {
                    const int32 OriginalVertexCount = Vertices.Num();
                    const int32 OriginalIndexCount = Triangles.Num();
                    for (int32 I = 0; I < OriginalVertexCount; ++I)
                    {
                        const FVector VertexCopy = Vertices[I];
                        const FVector NormalCopy = -Normals[I];
                        const FVector2D UvCopy = UV[I];
                        Vertices.Add(VertexCopy);
                        Normals.Add(NormalCopy);
                        UV.Add(UvCopy);
                    }
                    for (int32 I = 0; I < OriginalIndexCount; I += 3)
                    {
                        const int32 A = Triangles[I] + OriginalVertexCount;
                        const int32 B = Triangles[I + 1] + OriginalVertexCount;
                        const int32 C = Triangles[I + 2] + OriginalVertexCount;
                        Triangles.Add(A);
                        Triangles.Add(C);
                        Triangles.Add(B);
                    }
                }
                TArray<FLinearColor> Colors;
                TArray<FProcMeshTangent> Tangents;
                const int32 ThisSection = Section++;
                Mesh->CreateMeshSection_LinearColor(ThisSection, Vertices, Triangles, Normals, UV, Colors, Tangents, true);
                if (Material.IsValid() && OpaqueMaterial)
                {
                    FString AlphaMode = String(Material, TEXT("alphaMode"));
                    UMaterialInterface* ParentMaterial = AlphaMode == TEXT("BLEND") && TranslucentMaterial ? TranslucentMaterial
                        : AlphaMode == TEXT("MASK") && MaskedMaterial ? MaskedMaterial : OpaqueMaterial;
                    UTexture2D* BaseColor = WhiteTexture;
                    FLinearColor Tint = FLinearColor::White;
                    const TSharedPtr<FJsonObject>* PbrPtr = nullptr;
                    if (Material->TryGetObjectField(TEXT("pbrMetallicRoughness"), PbrPtr))
                    {
                        const TArray<TSharedPtr<FJsonValue>>* Factor = Array(*PbrPtr, TEXT("baseColorFactor"));
                        if (Factor && Factor->Num() == 4)
                        {
                            Tint = FLinearColor((*Factor)[0]->AsNumber(), (*Factor)[1]->AsNumber(), (*Factor)[2]->AsNumber(), (*Factor)[3]->AsNumber());
                        }
                        const TSharedPtr<FJsonObject>* BaseColorPtr = nullptr;
                        if ((*PbrPtr)->TryGetObjectField(TEXT("baseColorTexture"), BaseColorPtr))
                            BaseColor = ImageForTexture(Number(*BaseColorPtr, TEXT("index")));
                    }
                    if (BaseColor)
                    {
                        UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(ParentMaterial, Mesh);
                        Dynamic->SetTextureParameterValue(TEXT("SlateUI"), BaseColor);
                        Dynamic->SetVectorParameterValue(TEXT("TintColorAndOpacity"), Tint);
                        if (AlphaMode == TEXT("BLEND") || AlphaMode == TEXT("MASK"))
                            Dynamic->SetScalarParameterValue(TEXT("OpacityFromTexture"), 1.0f);
                        Mesh->SetMaterial(ThisSection, Dynamic);
                        ++AppliedMaterialCount;
                    }
                }
                TriangleCount += Triangles.Num()/3;
            }
        }
        if (const auto* Children = Array(N, TEXT("children"))) for (const auto& Child : *Children) Visit(Child->AsNumber(), World, Depth+1);
    };
    const auto* Scenes = Array(Root, TEXT("scenes"));
    auto Scene = ObjectAt(Scenes, Number(Root, TEXT("scene"), 0));
    if (const auto* SceneNodes = Array(Scene, TEXT("nodes"))) for (const auto& Node : *SceneNodes) Visit(Node->AsNumber(), FTransform::Identity, 0);
    if (Section == 0) { Error = TEXT("Sin triangulos compatibles; exporta mallas estaticas sin compresion Draco"); return false; }
    FloorZ = Min.Z;
    double BestArea = 0.;
    for (const TPair<int32, double>& Candidate : HorizontalAreaByHeight)
    {
        const float Height = Candidate.Key * 10.f;
        if (Height <= Min.Z + (Max.Z - Min.Z) * 0.5f && Candidate.Value > BestArea)
        {
            BestArea = Candidate.Value;
            FloorZ = Height;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: decoded %d/%d base images, %.1f MB; assigned %d/%d materials; floor %.1f cm"),
        SuccessfulImageCount, LoadedImages.Num(), DecodedTextureBytes / 1048576.f, AppliedMaterialCount, Section, FloorZ);
    return true;
}

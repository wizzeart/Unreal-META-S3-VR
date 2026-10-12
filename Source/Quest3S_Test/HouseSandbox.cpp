#include "HouseSandbox.h"

#include "GlbRuntime.h"
#include "ProceduralMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "MotionControllerComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Camera/CameraComponent.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Components/PrimitiveComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Components/HorizontalBox.h"
#include "Components/Spacer.h"
#include "Serialization/JsonWriter.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "Engine/World.h"
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace
{
constexpr int64 MaxGlbBytes = 100 * 1024 * 1024;
bool IsGlb(const TArray<uint8>& Bytes)
{
    return Bytes.Num() >= 20 && Bytes[0] == 'g' && Bytes[1] == 'l' && Bytes[2] == 'T' && Bytes[3] == 'F'
        && *reinterpret_cast<const uint32*>(Bytes.GetData() + 4) == 2
        && *reinterpret_cast<const uint32*>(Bytes.GetData() + 8) == static_cast<uint32>(Bytes.Num());
}
}

AHouseSandboxGameMode::AHouseSandboxGameMode()
{
    static ConstructorHelpers::FClassFinder<APawn> XRPawn(TEXT("/Game/XRFramework/Blueprints/BP_XRPawn"));
    if (XRPawn.Succeeded()) DefaultPawnClass = XRPawn.Class;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Opaque(TEXT("/Game/HouseMaterials/M_HouseLitOpaque.M_HouseLitOpaque"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Masked(TEXT("/Game/HouseMaterials/M_HouseLitMasked.M_HouseLitMasked"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Blend(TEXT("/Game/HouseMaterials/M_HouseLitBlend.M_HouseLitBlend"));
    CookedLitOpaque = Opaque.Object;
    CookedLitMasked = Masked.Object;
    CookedLitBlend = Blend.Object;
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> Buttons(TEXT("/Game/HouseInput/IMC_HouseButtons.IMC_HouseButtons"));
    HouseButtonContext = Buttons.Object;
}

AHouseMenuInputActor::AHouseMenuInputActor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AHouseMenuInputActor::Initialize(APawn* Pawn)
{
    if (!Pawn) return;
    UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
    if (!Camera) return;
    ControlledPawn = Pawn;
    PlayerCamera = Camera;
    USceneComponent* PointerOrigin = Camera;
    TArray<UMotionControllerComponent*> Controllers;
    Pawn->GetComponents<UMotionControllerComponent>(Controllers);
    for (UMotionControllerComponent* Controller : Controllers)
    {
        if (Controller && Controller->GetName().StartsWith(TEXT("MotionControllerRightAim")))
        {
            PointerOrigin = Controller;
            break;
        }
    }
    Pointer = NewObject<UWidgetInteractionComponent>(Pawn, TEXT("HouseMenuGazePointer"));
    Pointer->AttachToComponent(PointerOrigin, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    Pointer->InteractionSource = EWidgetInteractionSource::World;
    Pointer->InteractionDistance = 500.f;
    Pointer->VirtualUserIndex = 2;
    Pointer->PointerIndex = 2;
    Pointer->RegisterComponent();
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    if (Controller && Controller->GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Controller->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (UInputMappingContext* TemplateControls = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/XRFramework/Input/IMC_Default.IMC_Default")))
                Subsystem->RemoveMappingContext(TemplateControls);
        }
    }
    EnableInput(Controller);
    if (InputComponent)
    {
        InputComponent->BindKey(EKeys::OculusTouch_Right_Trigger_Click, IE_Pressed, this, &AHouseMenuInputActor::TriggerDown).bConsumeInput = false;
        InputComponent->BindKey(EKeys::OculusTouch_Right_Trigger_Click, IE_Released, this, &AHouseMenuInputActor::TriggerUp).bConsumeInput = false;
        InputComponent->BindAxisKey(EKeys::OculusTouch_Right_Trigger_Axis, this, &AHouseMenuInputActor::TriggerAxis).bConsumeInput = false;
        InputComponent->BindAxisKey(EKeys::OculusTouch_Left_Thumbstick_Y, this, &AHouseMenuInputActor::MoveForwardAxis).bConsumeInput = false;
        InputComponent->BindAxisKey(EKeys::OculusTouch_Left_Thumbstick_X, this, &AHouseMenuInputActor::MoveRightAxis).bConsumeInput = false;
        InputComponent->BindAxisKey(EKeys::OculusTouch_Right_Thumbstick_X, this, &AHouseMenuInputActor::TurnAxis).bConsumeInput = false;
        InputComponent->BindAxisKey(EKeys::OculusTouch_Right_Thumbstick_Y, this, &AHouseMenuInputActor::ScrollAxis).bConsumeInput = false;
        if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
        {
            const UInputAction* X = LoadObject<UInputAction>(nullptr, TEXT("/Game/HouseInput/IA_HouseX.IA_HouseX"));
            const UInputAction* Y = LoadObject<UInputAction>(nullptr, TEXT("/Game/HouseInput/IA_HouseY.IA_HouseY"));
            const UInputAction* A = LoadObject<UInputAction>(nullptr, TEXT("/Game/HouseInput/IA_HouseA.IA_HouseA"));
            const UInputAction* B = LoadObject<UInputAction>(nullptr, TEXT("/Game/HouseInput/IA_HouseB.IA_HouseB"));
            if (X && Y && A && B)
            {
                Enhanced->BindAction(X, ETriggerEvent::Started, this, &AHouseMenuInputActor::XDown);
                Enhanced->BindAction(X, ETriggerEvent::Completed, this, &AHouseMenuInputActor::XUp);
                Enhanced->BindAction(X, ETriggerEvent::Canceled, this, &AHouseMenuInputActor::XUp);
                Enhanced->BindAction(Y, ETriggerEvent::Started, this, &AHouseMenuInputActor::YDown);
                Enhanced->BindAction(Y, ETriggerEvent::Completed, this, &AHouseMenuInputActor::YUp);
                Enhanced->BindAction(Y, ETriggerEvent::Canceled, this, &AHouseMenuInputActor::YUp);
                Enhanced->BindAction(A, ETriggerEvent::Started, this, &AHouseMenuInputActor::ADown);
                Enhanced->BindAction(A, ETriggerEvent::Completed, this, &AHouseMenuInputActor::AUp);
                Enhanced->BindAction(A, ETriggerEvent::Canceled, this, &AHouseMenuInputActor::AUp);
                Enhanced->BindAction(B, ETriggerEvent::Started, this, &AHouseMenuInputActor::BDown);
                Enhanced->BindAction(B, ETriggerEvent::Completed, this, &AHouseMenuInputActor::BUp);
                Enhanced->BindAction(B, ETriggerEvent::Canceled, this, &AHouseMenuInputActor::BUp);
                UE_LOG(LogTemp, Display, TEXT("HouseSandbox: four XR button actions bound"));
            }
            else UE_LOG(LogTemp, Error, TEXT("HouseSandbox: missing HouseInput action assets"));
        }
        else UE_LOG(LogTemp, Error, TEXT("HouseSandbox: EnhancedInputComponent unavailable for XR buttons"));
    }
}

void AHouseMenuInputActor::MoveForwardAxis(float Value) { ForwardInput = Value; }
void AHouseMenuInputActor::MoveRightAxis(float Value) { RightInput = Value; }
void AHouseMenuInputActor::TurnAxis(float Value) { TurnInput = Value; }
void AHouseMenuInputActor::ScrollAxis(float Value) { ScrollInput = Value; }

void AHouseMenuInputActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!ControlledPawn || !PlayerCamera) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (bXHeld && bYHeld && !bVisualChordFired && Now - FMath::Max(XDownTime, YDownTime) >= 0.6f)
    {
        bVisualChordFired = true;
        bXUsedForChord = true;
        UE_LOG(LogTemp, Display, TEXT("HouseSandbox: X+Y visual chord fired"));
        if (AHouseSandboxGameMode* Sandbox = Cast<AHouseSandboxGameMode>(UGameplayStatics::GetGameMode(this)))
            Sandbox->ShowPanel(Sandbox->GetActivePanel() == EHousePanel::Visual ? EHousePanel::Closed : EHousePanel::Visual);
    }
    if (bAHeld && bBHeld && !bReturnChordFired && Now - FMath::Max(ADownTime, BDownTime) >= 1.f)
    {
        bReturnChordFired = true;
        UE_LOG(LogTemp, Display, TEXT("HouseSandbox: A+B return chord fired"));
        if (AHouseSandboxGameMode* Sandbox = Cast<AHouseSandboxGameMode>(UGameplayStatics::GetGameMode(this))) Sandbox->ReturnToLibrary();
    }
    const float Step = FMath::Min(DeltaSeconds, 0.05f);
    auto DeadZone = [](float Value) { return FMath::Abs(Value) < 0.15f ? 0.f : Value; };
    if (AHouseSandboxGameMode* Sandbox = Cast<AHouseSandboxGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (Sandbox->IsMenuVisible())
        {
            const float Scroll = DeadZone(ScrollInput);
            if (!FMath::IsNearlyZero(Scroll)) Sandbox->ScrollPanel(-Scroll * Step * 650.f);
            return;
        }
    }
    const FVector2D Stick(DeadZone(RightInput), DeadZone(ForwardInput));
    if (!Stick.IsNearlyZero())
    {
        const FRotator Yaw(0.f, PlayerCamera->GetComponentRotation().Yaw, 0.f);
        const FVector Direction = (Yaw.Vector() * Stick.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * Stick.X).GetClampedToMaxSize(1.f);
        const FVector Delta = Direction * 420.f * Step;
        const FVector PawnLocation = ControlledPawn->GetActorLocation();
        const FVector HeadLocation = PlayerCamera->GetComponentLocation();
        const FVector Start(HeadLocation.X, HeadLocation.Y, PawnLocation.Z + 92.f);
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(HouseWalk), false, ControlledPawn);
        const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, Start, Start + Delta, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(22.f, 85.f), Params);
        ControlledPawn->SetActorLocation(PawnLocation + Delta * (bBlocked ? FMath::Max(0.f, Hit.Time - 0.02f) : 1.f));
    }
    const float Turn = DeadZone(TurnInput);
    if (!FMath::IsNearlyZero(Turn))
    {
        const FVector HeadBefore = PlayerCamera->GetComponentLocation();
        ControlledPawn->AddActorWorldRotation(FRotator(0.f, Turn * 150.f * Step, 0.f));
        const FVector HeadAfter = PlayerCamera->GetComponentLocation();
        ControlledPawn->AddActorWorldOffset(FVector(HeadBefore.X - HeadAfter.X, HeadBefore.Y - HeadAfter.Y, 0.f));
    }
}

void AHouseMenuInputActor::TriggerDown()
{
    if (Pointer && !bTriggerHeld && !bSuppressTriggerUntilRelease)
    {
        bTriggerHeld = true;
        UE_LOG(LogTemp, Display, TEXT("HouseSandbox: right trigger down, widget hit=%d"), Pointer->GetHoveredWidgetComponent() != nullptr);
        Pointer->PressPointerKey(EKeys::LeftMouseButton);
    }
}
void AHouseMenuInputActor::TriggerUp()
{
    if (Pointer && bTriggerHeld)
    {
        bTriggerHeld = false;
        Pointer->ReleasePointerKey(EKeys::LeftMouseButton);
    }
    bSuppressTriggerUntilRelease = false;
}
void AHouseMenuInputActor::CancelPointerPress()
{
    if (Pointer && bTriggerHeld) Pointer->ReleasePointerKey(EKeys::LeftMouseButton);
    bSuppressTriggerUntilRelease = bTriggerHeld;
    bTriggerHeld = false;
}
void AHouseMenuInputActor::TriggerAxis(float Value)
{
    if (Value > 0.55f && !bSuppressTriggerUntilRelease) TriggerDown();
    else if (Value < 0.25f) TriggerUp();
}
void AHouseMenuInputActor::ToggleMenu()
{
    if (AHouseSandboxGameMode* Sandbox = Cast<AHouseSandboxGameMode>(UGameplayStatics::GetGameMode(this)))
        Sandbox->ShowPanel(Sandbox->IsMenuVisible() ? EHousePanel::Closed : EHousePanel::Library);
}

void AHouseMenuInputActor::XDown() { bXHeld = true; XDownTime = GetWorld()->GetTimeSeconds(); if (bYHeld) bXUsedForChord = true; UE_LOG(LogTemp, Display, TEXT("HouseSandbox: XR X pressed")); }
void AHouseMenuInputActor::XUp()
{
    if (!bXUsedForChord && !bYHeld) ToggleMenu();
    bXHeld = false;
    bXUsedForChord = false;
    if (!bYHeld) bVisualChordFired = false;
}
void AHouseMenuInputActor::YDown() { bYHeld = true; YDownTime = GetWorld()->GetTimeSeconds(); if (bXHeld) bXUsedForChord = true; UE_LOG(LogTemp, Display, TEXT("HouseSandbox: XR Y pressed")); }
void AHouseMenuInputActor::YUp() { bYHeld = false; if (!bXHeld) bVisualChordFired = false; }
void AHouseMenuInputActor::ADown() { bAHeld = true; ADownTime = GetWorld()->GetTimeSeconds(); UE_LOG(LogTemp, Display, TEXT("HouseSandbox: XR A pressed")); }
void AHouseMenuInputActor::AUp() { bAHeld = false; if (!bBHeld) bReturnChordFired = false; }
void AHouseMenuInputActor::BDown() { bBHeld = true; BDownTime = GetWorld()->GetTimeSeconds(); UE_LOG(LogTemp, Display, TEXT("HouseSandbox: XR B pressed")); }
void AHouseMenuInputActor::BUp() { bBHeld = false; if (!bAHeld) bReturnChordFired = false; }

void AHouseSandboxGameMode::BeginPlay()
{
    Super::BeginPlay();
    IFileManager::Get().MakeDirectory(*GetModelDirectory(), true);
    LoadVisualSettings();
    MenuActor = GetWorld()->SpawnActor<AActor>();
    MenuComponent = NewObject<UWidgetComponent>(MenuActor, TEXT("HouseLibrary"));
    MenuComponent->RegisterComponent();
    MenuActor->SetRootComponent(MenuComponent);
    MenuComponent->SetWidgetSpace(EWidgetSpace::World);
    MenuComponent->SetDrawSize(FVector2D(1200, 800));
    MenuComponent->SetWorldScale3D(FVector(0.075));
    MenuComponent->SetTwoSided(true);
    MenuComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MenuComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MenuComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    MenuWidget = CreateWidget<UHouseSandboxWidget>(GetWorld(), UHouseSandboxWidget::StaticClass());
    MenuComponent->SetWidget(MenuWidget);
    GetWorldTimerManager().SetTimerForNextTick(this, &AHouseSandboxGameMode::PlaceMenuAtPlayer);
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: menu created, local directory %s"), *GetModelDirectory());
    MenuWidget->Populate(this);
}

void AHouseSandboxGameMode::PlaceMenuAtPlayer()
{
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
    if (!Camera)
    {
        FTimerHandle Retry;
        GetWorldTimerManager().SetTimer(Retry, this, &AHouseSandboxGameMode::PlaceMenuAtPlayer, 0.25f, false);
        return;
    }
    ShowPanel(EHousePanel::Library);
    MenuInput = GetWorld()->SpawnActor<AHouseMenuInputActor>();
    MenuInput->Initialize(Pawn);
    PreviewLight = NewObject<UPointLightComponent>(Pawn, TEXT("HousePreviewLight"));
    PreviewLight->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    PreviewLight->SetIntensityUnits(ELightUnits::Lumens);
    PreviewLight->SetIntensity(1500.f);
    PreviewLight->SetAttenuationRadius(1400.f);
    PreviewLight->SetCastShadows(false);
    PreviewLight->RegisterComponent();
    PreviewLight->SetVisibility(false);
    SceneSun = GetWorld()->SpawnActor<ADirectionalLight>();
    if (SceneSun)
    {
        SceneSun->SetActorRotation(FRotator(-55.f, 25.f, 0.f));
        ULightComponent* Sun = SceneSun->GetLightComponent();
        Sun->SetMobility(EComponentMobility::Movable);
        Sun->SetIntensity(3.f);
        Sun->SetCastShadows(false);
        Sun->SetVisibility(false);
    }
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: world menu placed in front of XR camera"));
}

FString AHouseSandboxGameMode::GetModelDirectory() const
{
    return FPaths::Combine(FPaths::ProjectPersistentDownloadDir(), TEXT("Houses"));
}

TArray<FString> AHouseSandboxGameMode::GetLocalHouses() const
{
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *FPaths::Combine(GetModelDirectory(), TEXT("*.glb")), true, false);
    Files.Sort();
    return Files;
}

void AHouseSandboxGameMode::SetError(const FString& Message)
{
    LastError = Message;
    UE_LOG(LogTemp, Warning, TEXT("HouseSandbox: %s"), *Message);
    RefreshMenu();
}

void AHouseSandboxGameMode::RefreshMenu()
{
    if (MenuWidget) MenuWidget->Populate(this);
}

void AHouseSandboxGameMode::ToggleMenuVisibility()
{
    ShowPanel(IsMenuVisible() ? EHousePanel::Closed : EHousePanel::Library);
}

void AHouseSandboxGameMode::ShowPanel(EHousePanel Panel)
{
    if (Panel != ActivePanel && MenuInput) MenuInput->CancelPointerPress();
    ActivePanel = Panel;
    if (!MenuComponent || !MenuActor) return;
    MenuComponent->SetVisibility(Panel != EHousePanel::Closed);
    if (Panel == EHousePanel::Closed) return;
    if (MenuWidget) MenuWidget->SetVisualMode(Panel == EHousePanel::Visual);
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
    if (!Camera) return;
    const FRotator Yaw(0.f, Camera->GetComponentRotation().Yaw, 0.f);
    MenuActor->SetActorLocation(Camera->GetComponentLocation() + Yaw.Vector() * 135.f);
    MenuActor->SetActorRotation(FRotator(0.f, Yaw.Yaw + 180.f, 0.f));
}

void AHouseSandboxGameMode::ReturnToLibrary()
{
    ++RequestGeneration;
    if (HouseActor) HouseActor->Destroy();
    HouseActor = nullptr;
    HouseMesh = nullptr;
    LitMaterials.Empty();
    ReferenceMaterials.Empty();
    MaterialFeatures = FHouseMaterialFeatures();
    if (PreviewLight) PreviewLight->SetVisibility(false);
    if (SceneSun) SceneSun->GetLightComponent()->SetVisibility(false);
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0)) Pawn->SetActorLocation(FVector::ZeroVector);
    LastError = TEXT("Elige un modelo de la laptop o del visor");
    RefreshMenu();
    ShowPanel(EHousePanel::Library);
    FetchHubModels(GetHubAddress());
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: returned to model library"));
}

void AHouseSandboxGameMode::LoadVisualSettings()
{
    FString Contents;
    const FString Path = FPaths::Combine(GetModelDirectory(), TEXT("visual_settings.json"));
    if (!FFileHelper::LoadFileToString(Contents, *Path)) return;
    TSharedPtr<FJsonObject> Json;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Contents), Json) || !Json.IsValid()) return;
    Json->TryGetBoolField(TEXT("lit"), VisualSettings.bLit);
    Json->TryGetBoolField(TEXT("normal"), VisualSettings.bNormal);
    Json->TryGetBoolField(TEXT("occlusion"), VisualSettings.bOcclusion);
    Json->TryGetBoolField(TEXT("metalRough"), VisualSettings.bMetalRough);
    Json->TryGetBoolField(TEXT("emissive"), VisualSettings.bEmissive);
    Json->TryGetBoolField(TEXT("fillLight"), VisualSettings.bFillLight);
    Json->TryGetBoolField(TEXT("shadows"), VisualSettings.bShadows);
}

void AHouseSandboxGameMode::SaveVisualSettings() const
{
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetBoolField(TEXT("lit"), VisualSettings.bLit);
    Json->SetBoolField(TEXT("normal"), VisualSettings.bNormal);
    Json->SetBoolField(TEXT("occlusion"), VisualSettings.bOcclusion);
    Json->SetBoolField(TEXT("metalRough"), VisualSettings.bMetalRough);
    Json->SetBoolField(TEXT("emissive"), VisualSettings.bEmissive);
    Json->SetBoolField(TEXT("fillLight"), VisualSettings.bFillLight);
    Json->SetBoolField(TEXT("shadows"), VisualSettings.bShadows);
    FString Contents;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Contents);
    if (FJsonSerializer::Serialize(Json, Writer))
        FFileHelper::SaveStringToFile(Contents, *FPaths::Combine(GetModelDirectory(), TEXT("visual_settings.json")));
}

void AHouseSandboxGameMode::ApplyVisualSettings(bool bSave)
{
    if (HouseMesh)
    {
        for (int32 Index = 0; Index < LitMaterials.Num(); ++Index)
        {
            UMaterialInstanceDynamic* Lit = LitMaterials[Index];
            UMaterialInstanceDynamic* Reference = ReferenceMaterials.IsValidIndex(Index) ? ReferenceMaterials[Index].Get() : nullptr;
            if (Lit)
            {
                Lit->SetScalarParameterValue(TEXT("NormalStrength"), VisualSettings.bNormal && MaterialFeatures.NormalScales.IsValidIndex(Index) ? MaterialFeatures.NormalScales[Index] : 0.f);
                Lit->SetScalarParameterValue(TEXT("OcclusionStrength"), VisualSettings.bOcclusion && MaterialFeatures.OcclusionStrengths.IsValidIndex(Index) ? MaterialFeatures.OcclusionStrengths[Index] : 0.f);
                Lit->SetScalarParameterValue(TEXT("MetalRoughStrength"), VisualSettings.bMetalRough ? 1.f : 0.f);
                Lit->SetScalarParameterValue(TEXT("EmissiveStrength"), VisualSettings.bEmissive ? 1.f : 0.f);
            }
            HouseMesh->SetMaterial(Index, VisualSettings.bLit && Lit ? static_cast<UMaterialInterface*>(Lit) : Reference);
        }
    }
    const bool bLightScene = HouseActor && VisualSettings.bLit;
    if (PreviewLight) PreviewLight->SetVisibility(bLightScene && VisualSettings.bFillLight);
    if (SceneSun)
    {
        ULightComponent* Sun = SceneSun->GetLightComponent();
        Sun->SetCastShadows(VisualSettings.bShadows);
        Sun->SetVisibility(bLightScene);
    }
    if (MenuWidget) MenuWidget->UpdateVisualLabels();
    if (bSave) SaveVisualSettings();
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: visual lit=%d normal=%d ao=%d metalrough=%d emissive=%d fill=%d shadows=%d"),
        VisualSettings.bLit, VisualSettings.bNormal, VisualSettings.bOcclusion, VisualSettings.bMetalRough,
        VisualSettings.bEmissive, VisualSettings.bFillLight, VisualSettings.bShadows);
}

void AHouseSandboxGameMode::ChangeVisualControl(int32 Feature)
{
    switch (Feature)
    {
    case 0: VisualSettings.bLit = !VisualSettings.bLit; break;
    case 1: VisualSettings.bNormal = !VisualSettings.bNormal; break;
    case 2: VisualSettings.bOcclusion = !VisualSettings.bOcclusion; break;
    case 3: VisualSettings.bMetalRough = !VisualSettings.bMetalRough; break;
    case 4: VisualSettings.bEmissive = !VisualSettings.bEmissive; break;
    case 5: VisualSettings.bFillLight = !VisualSettings.bFillLight; break;
    case 6: VisualSettings.bShadows = !VisualSettings.bShadows; break;
    case 100:
        VisualSettings = FHouseVisualSettings();
        VisualSettings.bLit = false; VisualSettings.bFillLight = false; break;
    case 101:
        VisualSettings = FHouseVisualSettings(); break;
    case 102:
        VisualSettings = FHouseVisualSettings(); VisualSettings.bShadows = true; break;
    case 200: ReturnToLibrary(); return;
    case 201: ShowPanel(EHousePanel::Library); return;
    case 202: ShowPanel(EHousePanel::Closed); return;
    default: return;
    }
    ApplyVisualSettings();
}

void AHouseSandboxGameMode::ScrollPanel(float Amount)
{
    if (MenuWidget) MenuWidget->ScrollBy(Amount);
}

bool AHouseSandboxGameMode::IsMenuVisible() const
{
    return MenuComponent && MenuComponent->IsVisible();
}

void AHouseSandboxGameMode::HideTemplateGeometry()
{
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor == Pawn || Actor == HouseActor || Actor == MenuActor || Actor == MenuInput || Actor == this) continue;
        if (Actor->GetName().Contains(TEXT("Sky"), ESearchCase::IgnoreCase)
            || Actor->GetClass()->GetName().Contains(TEXT("Sky"), ESearchCase::IgnoreCase)) continue;
        bool bAttachedToPlayer = false;
        for (AActor* Parent = Actor->GetAttachParentActor(); Parent; Parent = Parent->GetAttachParentActor())
        {
            if (Parent == Pawn) { bAttachedToPlayer = true; break; }
        }
        if (bAttachedToPlayer) continue;
        TInlineComponentArray<UPrimitiveComponent*> Primitives;
        Actor->GetComponents(Primitives);
        for (UPrimitiveComponent* Primitive : Primitives)
        {
            Primitive->SetVisibility(false, true);
            Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
}

FString AHouseSandboxGameMode::GetHubAddress() const
{
    FString SavedAddress;
    if (FFileHelper::LoadFileToString(SavedAddress, *FPaths::Combine(GetModelDirectory(), TEXT("hub_address.txt"))))
    {
        SavedAddress.TrimStartAndEndInline();
        if (SavedAddress.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase)
            || SavedAddress.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase)) return SavedAddress;
    }
    return TEXT("http://127.0.0.1:8765");
}

void AHouseSandboxGameMode::FetchHubModels(const FString& HubAddress)
{
    FString Base = HubAddress.TrimStartAndEnd();
    while (Base.EndsWith(TEXT("/"))) Base.LeftChopInline(1);
    if ((!Base.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase)
            && !Base.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
        || Base.Len() > 200 || Base.Contains(TEXT(" ")))
    {
        SetError(TEXT("Escribe la direccion del hub: http://IP:8765"));
        return;
    }
    FFileHelper::SaveStringToFile(Base, *FPaths::Combine(GetModelDirectory(), TEXT("hub_address.txt")));
    SetError(TEXT("Buscando casas en la laptop..."));
    TWeakObjectPtr<AHouseSandboxGameMode> WeakThis(this);
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Base + TEXT("/api/models"));
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(12.f);
    const int32 Generation = RequestGeneration;
    Request->OnProcessRequestComplete().BindLambda([WeakThis, Base, Generation](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
    {
        if (!WeakThis.IsValid() || WeakThis->RequestGeneration != Generation) return;
        if (!bSuccess || !Response.IsValid() || Response->GetResponseCode() != 200
            || Response->GetContent().Num() > 512 * 1024)
        {
            WeakThis->SetError(TEXT("No se pudo conectar al hub. Revisa Wi-Fi o puente USB."));
            return;
        }
        TArray<TSharedPtr<FJsonValue>> Values;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Values))
        {
            WeakThis->SetError(TEXT("Respuesta del hub invalida"));
            return;
        }
        TArray<TPair<FString, FString>> Models;
        for (const TSharedPtr<FJsonValue>& Value : Values)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object) continue;
            const TSharedPtr<FJsonObject> Item = Value->AsObject();
            FString Name, Url;
            if (Item->TryGetStringField(TEXT("name"), Name) && Item->TryGetStringField(TEXT("url"), Url)
                && Name.EndsWith(TEXT(".glb"), ESearchCase::IgnoreCase)
                && Url.StartsWith(Base + TEXT("/models/"), ESearchCase::IgnoreCase))
            {
                Models.Emplace(Name, Url);
                if (Models.Num() >= 200) break;
            }
        }
        if (WeakThis->MenuWidget) WeakThis->MenuWidget->SetRemoteModels(Models);
        WeakThis->SetError(FString::Printf(TEXT("Hub conectado: %d modelos disponibles"), Models.Num()));
        UE_LOG(LogTemp, Display, TEXT("HouseSandbox: hub %s, %d models"), *Base, Models.Num());
    });
    Request->ProcessRequest();
}

void AHouseSandboxGameMode::DownloadHouse(const FString& Url)
{
    if (!Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) && !Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
    {
        SetError(TEXT("Introduce una URL http(s) de un archivo .glb"));
        return;
    }
    FString Path = Url;
    Path.Split(TEXT("?"), &Path, nullptr);
    const FString Name = FPaths::GetCleanFilename(Path);
    if (Name.IsEmpty() || Name == TEXT(".") || Name == TEXT("..")
        || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("/"))
        || !Name.EndsWith(TEXT(".glb"), ESearchCase::IgnoreCase))
    {
        SetError(TEXT("La URL debe terminar en .glb"));
        return;
    }
    SetError(TEXT("Descargando modelo..."));
    const int32 Generation = ++RequestGeneration;
    TWeakObjectPtr<AHouseSandboxGameMode> WeakThis(this);
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("GET"));
    Request->OnProcessRequestComplete().BindLambda([WeakThis, Name, Generation](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
    {
        if (!WeakThis.IsValid() || WeakThis->RequestGeneration != Generation) return;
        if (!bSuccess || !Response.IsValid() || Response->GetResponseCode() != 200)
        {
            WeakThis->SetError(TEXT("No se pudo descargar el archivo"));
            return;
        }
        const TArray<uint8>& Bytes = Response->GetContent();
        if (Bytes.Num() > MaxGlbBytes || !IsGlb(Bytes))
        {
            WeakThis->SetError(TEXT("Archivo GLB invalido o mayor de 100 MB"));
            return;
        }
        const FString Target = FPaths::Combine(WeakThis->GetModelDirectory(), Name);
        if (!FFileHelper::SaveArrayToFile(Bytes, *Target))
        {
            WeakThis->SetError(TEXT("No se pudo guardar el archivo"));
            return;
        }
        WeakThis->LoadHouse(Name);
    });
    Request->ProcessRequest();
}

void AHouseSandboxGameMode::OpenAndroidFilePicker()
{
#if PLATFORM_ANDROID
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_OpenHousePicker", "()V", false);
        if (Method) FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
    }
#else
    SetError(TEXT("Selector de archivos disponible en Quest"));
#endif
}

bool AHouseSandboxGameMode::LoadHouse(const FString& FileName)
{
    if (FileName != FPaths::GetCleanFilename(FileName) || !FileName.EndsWith(TEXT(".glb"), ESearchCase::IgnoreCase))
    {
        SetError(TEXT("Nombre de archivo invalido"));
        return false;
    }
    const FString Path = FPaths::Combine(GetModelDirectory(), FileName);
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() > MaxGlbBytes || !IsGlb(Bytes))
    {
        SetError(TEXT("No se puede abrir este GLB"));
        return false;
    }
    AActor* NewActor = GetWorld()->SpawnActor<AActor>();
    UProceduralMeshComponent* NewMesh = NewObject<UProceduralMeshComponent>(NewActor, TEXT("ImportedHouse"));
    NewMesh->RegisterComponent();
    NewActor->SetRootComponent(NewMesh);
    NewMesh->SetMobility(EComponentMobility::Movable);
    NewMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    NewMesh->SetCollisionObjectType(ECC_WorldStatic);
    NewMesh->SetCollisionResponseToAllChannels(ECR_Block);
    NewMesh->bUseComplexAsSimpleCollision = true;

    int64 TotalTriangles = 0;
    FVector Min, Max;
    float FloorZ = 0.f;
    FString ImportError;
    TArray<UMaterialInstanceDynamic*> NewLitMaterials;
    TArray<UMaterialInstanceDynamic*> NewReferenceMaterials;
    FHouseMaterialFeatures NewFeatures;
    if (!HouseGlb::BuildMesh(Bytes, NewMesh, Min, Max, FloorZ, TotalTriangles,
        NewLitMaterials, NewReferenceMaterials, NewFeatures, ImportError))
    {
        NewActor->Destroy();
        SetError(ImportError);
        return false;
    }
    NewActor->SetActorLocation(FVector(-(Min.X + Max.X) / 2, -(Min.Y + Max.Y) / 2, -FloorZ));
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: bounds %s to %s, walkable floor %.1f cm"), *Min.ToString(), *Max.ToString(), FloorZ);
    // The bounding-box centre can lie inside a wall, furniture or a low roof.
    // Look for a nearby clear floor position before placing the headset.
    FVector SpawnPosition = FVector::ZeroVector;
    bool bFoundClearFloor = false;
    float BestSpawnScore = -TNumericLimits<float>::Max();
    const FCollisionQueryParams SpawnTraceParams(SCENE_QUERY_STAT(HouseSpawn), true);
    const FVector2D Directions[4] = {FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1)};
    for (int32 Ring = 0; Ring <= 4; ++Ring)
    {
        for (int32 GridY = -Ring; GridY <= Ring; ++GridY)
        for (int32 GridX = -Ring; GridX <= Ring; ++GridX)
        {
            if (FMath::Max(FMath::Abs(GridX), FMath::Abs(GridY)) != Ring) continue;
            const FVector Candidate(GridX * 150.f, GridY * 150.f, 0.f);
            FHitResult FloorHit;
            if (!NewMesh->LineTraceComponent(FloorHit, Candidate + FVector(0, 0, 70), Candidate - FVector(0, 0, 60), SpawnTraceParams)
                || FloorHit.ImpactNormal.Z < 0.6f || FMath::Abs(FloorHit.ImpactPoint.Z) > 35.f) continue;
            FHitResult CeilingHit;
            if (NewMesh->LineTraceComponent(CeilingHit, Candidate + FVector(0, 0, 15), Candidate + FVector(0, 0, 205), SpawnTraceParams)) continue;
            bool bBodyClear = true;
            for (const FVector2D& Direction : Directions)
            {
                const FVector Offset(Direction.X * 45.f, Direction.Y * 45.f, 0.f);
                FHitResult BodyHit;
                if (NewMesh->LineTraceComponent(BodyHit, Candidate + Offset + FVector(0, 0, 35), Candidate + Offset + FVector(0, 0, 180), SpawnTraceParams))
                {
                    bBodyClear = false;
                    break;
                }
            }
            if (!bBodyClear) continue;
            float Score = -0.25f * Candidate.Size2D();
            for (const FVector2D& Direction : Directions)
            {
                const FVector Eye = Candidate + FVector(0, 0, 155);
                FHitResult ViewHit;
                const FVector End = Eye + FVector(Direction.X * 260.f, Direction.Y * 260.f, 0.f);
                Score += NewMesh->LineTraceComponent(ViewHit, Eye, End, SpawnTraceParams) ? ViewHit.Distance : 260.f;
            }
            if (Score > BestSpawnScore)
            {
                BestSpawnScore = Score;
                SpawnPosition = Candidate;
                bFoundClearFloor = true;
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: spawn X=%.0f Y=%.0f clear_floor=%d openness=%.0f"),
        SpawnPosition.X, SpawnPosition.Y, bFoundClearFloor, bFoundClearFloor ? BestSpawnScore : 0.f);
    if (HouseActor) HouseActor->Destroy();
    HouseActor = NewActor;
    HouseMesh = NewMesh;
    LitMaterials.Reset();
    ReferenceMaterials.Reset();
    for (UMaterialInstanceDynamic* Item : NewLitMaterials) LitMaterials.Add(Item);
    for (UMaterialInstanceDynamic* Item : NewReferenceMaterials) ReferenceMaterials.Add(Item);
    MaterialFeatures = NewFeatures;
    HideTemplateGeometry();
    ApplyVisualSettings(false);
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        Pawn->SetActorLocation(SpawnPosition);
    }
    SetError(FString::Printf(TEXT("%s cargado: %lld triangulos"), *FileName, TotalTriangles));
    ShowPanel(EHousePanel::Closed);
    return true;
}

void UHouseFileButton::ActivateFile()
{
    if (ParentMenu) ParentMenu->OpenFile(FileName);
}

void UHouseRemoteButton::ActivateRemote()
{
    if (ParentMenu) ParentMenu->OpenRemote(ModelUrl);
}

void UHouseVisualButton::ActivateVisual()
{
    if (ParentMenu) ParentMenu->ActivateVisualControl(Feature);
}

bool UHouseSandboxWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Backdrop->SetBrushColor(FLinearColor(0.025f, 0.055f, 0.095f, 0.98f));
    Backdrop->SetPadding(FMargin(26.f));
    WidgetTree->RootWidget = Backdrop;
    UVerticalBox* Frame = WidgetTree->ConstructWidget<UVerticalBox>();
    Backdrop->SetContent(Frame);
    auto AddText = [&](UVerticalBox* Parent, const FString& Value, int32 Size = 28) -> UTextBlock*
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Value));
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = Size;
        Text->SetFont(Font);
        Text->SetAutoWrapText(true);
        Parent->AddChildToVerticalBox(Text);
        return Text;
    };
    auto AddButton = [&](UVerticalBox* Parent, const FString& Label, FName Name) -> UButton*
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        Button->SetBackgroundColor(FLinearColor(0.12f, 0.37f, 0.43f, 1.f));
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label));
        FSlateFontInfo Font = Text->GetFont(); Font.Size = 27; Text->SetFont(Font);
        Text->SetAutoWrapText(true);
        Button->AddChild(Text);
        if (UVerticalBoxSlot* MenuSlot = Parent->AddChildToVerticalBox(Button)) MenuSlot->SetPadding(FMargin(0.f, 7.f));
        return Button;
    };
    AddText(Frame, TEXT("CASAS VR    ·    BIBLIOTECA Y VISUALES"), 38);
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
    Frame->AddChildToVerticalBox(Tabs);
    UButton* LibraryTab = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* LibraryTabText = WidgetTree->ConstructWidget<UTextBlock>();
    LibraryTabText->SetText(FText::FromString(TEXT("MODELOS")));
    LibraryTab->AddChild(LibraryTabText);
    LibraryTab->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnShowLibrary);
    Tabs->AddChildToHorizontalBox(LibraryTab);
    UButton* VisualTab = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* VisualTabText = WidgetTree->ConstructWidget<UTextBlock>();
    VisualTabText->SetText(FText::FromString(TEXT("OPCIONES VISUALES")));
    VisualTab->AddChild(VisualTabText);
    VisualTab->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnShowVisual);
    Tabs->AddChildToHorizontalBox(VisualTab);
    ContentScroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* MenuSlot = Frame->AddChildToVerticalBox(ContentScroll))
    {
        FSlateChildSize Size; Size.SizeRule = ESlateSizeRule::Fill; MenuSlot->SetSize(Size);
    }
    LibraryPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ContentScroll->AddChild(LibraryPanel);
    VisualPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ContentScroll->AddChild(VisualPanel);
    VisualPanel->SetVisibility(ESlateVisibility::Collapsed);
    AddText(LibraryPanel, TEXT("HUB DE LA LAPTOP"), 30);
    HubBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    HubBox->SetText(FText::FromString(TEXT("http://127.0.0.1:8765")));
    HubBox->SetHintText(FText::FromString(TEXT("http://IP-LAPTOP:8765")));
    LibraryPanel->AddChildToVerticalBox(HubBox);
    UButton* Sync = AddButton(LibraryPanel, TEXT("VER MODELOS DE LA LAPTOP"), TEXT("SyncHub"));
    Sync->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnSyncHub);
    RemoteList = WidgetTree->ConstructWidget<UVerticalBox>();
    LibraryPanel->AddChildToVerticalBox(RemoteList);
    AddText(LibraryPanel, TEXT("ARCHIVOS DEL VISOR"), 30);
    FileList = WidgetTree->ConstructWidget<UVerticalBox>();
    LibraryPanel->AddChildToVerticalBox(FileList);
    UButton* Pick = AddButton(LibraryPanel, TEXT("ABRIR ARCHIVO DEL DISPOSITIVO"), TEXT("PickLocal"));
    Pick->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnPickLocal);
    UButton* Refresh = AddButton(LibraryPanel, TEXT("ACTUALIZAR ARCHIVOS LOCALES"), TEXT("RefreshLocal"));
    Refresh->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnRefresh);
    AddText(LibraryPanel, TEXT("ENLACE DIRECTO GLB  ·  OPCIONAL"), 27);
    UrlBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    UrlBox->SetHintText(FText::FromString(TEXT("http://IP-LAPTOP:8765/models/casa.glb")));
    LibraryPanel->AddChildToVerticalBox(UrlBox);
    UButton* Download = AddButton(LibraryPanel, TEXT("DESCARGAR Y ABRIR"), TEXT("DownloadDirect"));
    Download->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnDownload);
    AddText(VisualPanel, TEXT("PERFILES"), 32);
    auto VisualButton = [&](int32 Feature, const FString& Label) -> UTextBlock*
    {
        UHouseVisualButton* Button = WidgetTree->ConstructWidget<UHouseVisualButton>();
        Button->Feature = Feature;
        Button->ParentMenu = this;
        Button->SetBackgroundColor(FLinearColor(0.12f, 0.37f, 0.43f, 1.f));
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label));
        FSlateFontInfo Font = Text->GetFont(); Font.Size = 27; Text->SetFont(Font);
        Button->AddChild(Text);
        Button->OnClicked.AddDynamic(Button, &UHouseVisualButton::ActivateVisual);
        if (UVerticalBoxSlot* MenuSlot = VisualPanel->AddChildToVerticalBox(Button)) MenuSlot->SetPadding(FMargin(0.f, 7.f));
        if (Feature < 100) { VisualButtons.Add(Button); VisualLabels.Add(Text); }
        return Text;
    };
    VisualButton(100, TEXT("REFERENCIA  ·  COLOR ORIGINAL"));
    VisualButton(101, TEXT("EQUILIBRADO  ·  MATERIALES Y LUZ"));
    VisualButton(102, TEXT("DETALLE  ·  SOMBRAS EXPERIMENTALES"));
    AddText(VisualPanel, TEXT("AJUSTES INDIVIDUALES"), 32);
    VisualButton(0, TEXT("Materiales realistas"));
    VisualButton(1, TEXT("Relieve de superficies"));
    VisualButton(2, TEXT("Oclusión del modelo"));
    VisualButton(3, TEXT("Mapa metal / rugosidad"));
    VisualButton(4, TEXT("Emisión"));
    VisualButton(5, TEXT("Luz de relleno"));
    VisualButton(6, TEXT("Sombras direccionales (experimental)"));
    VisualButton(200, TEXT("VOLVER A MODELOS Y LIBERAR CASA"));
    VisualButton(201, TEXT("ABRIR BIBLIOTECA"));
    UButton* Recenter = AddButton(Frame, TEXT("RECOLOCAR PANEL DELANTE DE MÍ"), TEXT("RecenterPanel"));
    Recenter->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnRecenter);
    UButton* Close = AddButton(Frame, TEXT("CERRAR  ·  X PARA ABRIR  ·  X+Y VISUALES  ·  A+B MODELOS"), TEXT("ClosePanel"));
    Close->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnClose);
    StatusText = AddText(Frame, TEXT("Listo"), 24);
    return true;
}

void UHouseSandboxWidget::Populate(AHouseSandboxGameMode* Owner)
{
    Sandbox = Owner;
    if (!FileList || !Sandbox) return;
    const float PreviousScroll = ContentScroll ? ContentScroll->GetScrollOffset() : 0.f;
    if (HubBox && !bHubAddressLoaded)
    {
        HubBox->SetText(FText::FromString(Sandbox->GetHubAddress()));
        bHubAddressLoaded = true;
    }
    StatusText->SetText(FText::FromString(Sandbox->GetLastError()));
    FileList->ClearChildren();
    for (const FString& File : Sandbox->GetLocalHouses())
    {
        UHouseFileButton* Button = WidgetTree->ConstructWidget<UHouseFileButton>();
        Button->FileName = File;
        Button->ParentMenu = this;
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(File));
        FSlateFontInfo Font = Label->GetFont(); Font.Size = 27; Label->SetFont(Font);
        Label->SetAutoWrapText(true);
        Button->SetBackgroundColor(FLinearColor(0.12f, 0.37f, 0.43f, 1.f));
        Button->AddChild(Label);
        Button->OnClicked.AddDynamic(Button, &UHouseFileButton::ActivateFile);
        if (UVerticalBoxSlot* MenuSlot = FileList->AddChildToVerticalBox(Button)) MenuSlot->SetPadding(FMargin(0.f, 7.f));
    }
    if (ContentScroll) ContentScroll->SetScrollOffset(PreviousScroll);
    UpdateVisualLabels();
}

void UHouseSandboxWidget::SetVisualMode(bool bVisual)
{
    if (ContentScroll)
    {
        if (bShowingVisual) VisualScrollOffset = ContentScroll->GetScrollOffset();
        else LibraryScrollOffset = ContentScroll->GetScrollOffset();
    }
    bShowingVisual = bVisual;
    if (LibraryPanel) LibraryPanel->SetVisibility(bVisual ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (VisualPanel) VisualPanel->SetVisibility(bVisual ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (ContentScroll) ContentScroll->SetScrollOffset(bVisual ? VisualScrollOffset : LibraryScrollOffset);
    UpdateVisualLabels();
}

void UHouseSandboxWidget::ScrollBy(float Amount)
{
    if (ContentScroll) ContentScroll->SetScrollOffset(FMath::Max(0.f, ContentScroll->GetScrollOffset() + Amount));
}

void UHouseSandboxWidget::UpdateVisualLabels()
{
    if (!Sandbox || VisualLabels.Num() < 7 || VisualButtons.Num() < 7) return;
    const FHouseVisualSettings& V = Sandbox->GetVisualSettings();
    const FHouseMaterialFeatures& F = Sandbox->GetMaterialFeatures();
    const bool States[7] = { V.bLit, V.bNormal, V.bOcclusion, V.bMetalRough, V.bEmissive, V.bFillLight, V.bShadows };
    const TCHAR* Names[7] = { TEXT("Materiales realistas"), TEXT("Relieve de superficies"), TEXT("Oclusión del modelo"),
        TEXT("Mapa metal / rugosidad"), TEXT("Emisión"), TEXT("Luz de relleno"), TEXT("Sombras direccionales · experimental") };
    const bool Available[7] = { true, F.NormalMaps > 0, F.OcclusionMaps > 0, F.MetalRoughMaps > 0,
        F.EmissiveMaps > 0, true, true };
    for (int32 Index = 0; Index < 7; ++Index)
    {
        VisualButtons[Index]->SetIsEnabled(Available[Index]);
        VisualLabels[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s   ·   %s"), Names[Index],
            Available[Index] ? (States[Index] ? TEXT("ACTIVO") : TEXT("APAGADO")) : TEXT("SIN MAPA EN ESTE GLB"))));
    }
}

void UHouseSandboxWidget::ActivateVisualControl(int32 Feature)
{
    if (Sandbox) Sandbox->ChangeVisualControl(Feature);
}

void UHouseSandboxWidget::OpenFile(const FString& FileName)
{
    if (Sandbox) Sandbox->LoadHouse(FileName);
}
void UHouseSandboxWidget::OpenRemote(const FString& Url)
{
    if (Sandbox) Sandbox->DownloadHouse(Url);
}
void UHouseSandboxWidget::SetRemoteModels(const TArray<TPair<FString, FString>>& Models)
{
    if (!RemoteList) return;
    const float PreviousScroll = ContentScroll ? ContentScroll->GetScrollOffset() : 0.f;
    RemoteList->ClearChildren();
    for (const TPair<FString, FString>& Model : Models)
    {
        UHouseRemoteButton* Button = WidgetTree->ConstructWidget<UHouseRemoteButton>();
        Button->ModelUrl = Model.Value;
        Button->ParentMenu = this;
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(TEXT("DESCARGAR  |  ") + Model.Key));
        FSlateFontInfo Font = Label->GetFont(); Font.Size = 27; Label->SetFont(Font);
        Label->SetAutoWrapText(true);
        Button->SetBackgroundColor(FLinearColor(0.12f, 0.37f, 0.43f, 1.f));
        Button->AddChild(Label);
        Button->OnClicked.AddDynamic(Button, &UHouseRemoteButton::ActivateRemote);
        if (UVerticalBoxSlot* MenuSlot = RemoteList->AddChildToVerticalBox(Button)) MenuSlot->SetPadding(FMargin(0.f, 7.f));
    }
    if (ContentScroll) ContentScroll->SetScrollOffset(PreviousScroll);
}
void UHouseSandboxWidget::OnShowLibrary() { if (Sandbox) Sandbox->ShowPanel(EHousePanel::Library); }
void UHouseSandboxWidget::OnShowVisual() { if (Sandbox) Sandbox->ShowPanel(EHousePanel::Visual); }
void UHouseSandboxWidget::OnRecenter() { if (Sandbox) Sandbox->ShowPanel(Sandbox->GetActivePanel()); }
void UHouseSandboxWidget::OnSyncHub()
{
    if (Sandbox && HubBox) Sandbox->FetchHubModels(HubBox->GetText().ToString());
}
void UHouseSandboxWidget::OnDownload()
{
    if (Sandbox && UrlBox) Sandbox->DownloadHouse(UrlBox->GetText().ToString().TrimStartAndEnd());
}
void UHouseSandboxWidget::OnPickLocal()
{
    if (Sandbox) Sandbox->OpenAndroidFilePicker();
}
void UHouseSandboxWidget::OnRefresh()
{
    if (Sandbox) Sandbox->RefreshMenu();
}
void UHouseSandboxWidget::OnClose()
{
    if (Sandbox) Sandbox->ToggleMenuVisibility();
}

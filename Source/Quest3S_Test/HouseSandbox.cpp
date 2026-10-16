#include "HouseSandbox.h"
#include "HouseVoice.h"

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
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureCube.h"
#include "Components/HorizontalBox.h"
#include "Components/Spacer.h"
#include "Components/CanvasPanel.h"
#include "Components/AudioComponent.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundWaveProcedural.h"
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
const FLinearColor CyberBackground(0.012f, 0.025f, 0.06f, 0.98f);
const FLinearColor CyberAction(0.035f, 0.18f, 0.28f, 1.f);
const FLinearColor CyberText(0.91f, 0.97f, 1.f, 1.f);
const FLinearColor CyberCyan(0.31f, 0.9f, 0.98f, 1.f);
const FLinearColor CyberMagenta(0.96f, 0.35f, 0.81f, 1.f);
bool IsGlb(const TArray<uint8>& Bytes)
{
    return Bytes.Num() >= 20 && Bytes[0] == 'g' && Bytes[1] == 'l' && Bytes[2] == 'T' && Bytes[3] == 'F'
        && *reinterpret_cast<const uint32*>(Bytes.GetData() + 4) == 2
        && *reinterpret_cast<const uint32*>(Bytes.GetData() + 8) == static_cast<uint32>(Bytes.Num());
}
}

AHouseSandboxGameMode::AHouseSandboxGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
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
    static ConstructorHelpers::FObjectFinder<UTextureCube> DaylightCubemap(TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
    CookedSkyCubemap = DaylightCubemap.Object;
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
    AHouseSandboxGameMode* Sandbox = Cast<AHouseSandboxGameMode>(UGameplayStatics::GetGameMode(this));
    const bool bGhost = Sandbox && Sandbox->IsGhost();
    const FVector2D Stick(DeadZone(RightInput), DeadZone(ForwardInput));
    if (!Stick.IsNearlyZero())
    {
        const FRotator Yaw(0.f, PlayerCamera->GetComponentRotation().Yaw, 0.f);
        const FVector Direction = (Yaw.Vector() * Stick.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * Stick.X).GetClampedToMaxSize(1.f);
        const FVector Delta = (Direction + FVector::UpVector * (bGhost ? DeadZone(ScrollInput) : 0.f)).GetClampedToMaxSize(1.f) * 420.f * Step;
        const FVector PawnLocation = ControlledPawn->GetActorLocation();
        const FVector HeadLocation = PlayerCamera->GetComponentLocation();
        const FVector Start(HeadLocation.X, HeadLocation.Y, PawnLocation.Z + 92.f);
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(HouseWalk), false, ControlledPawn);
        const bool bBlocked = !bGhost && GetWorld()->SweepSingleByChannel(Hit, Start, Start + Delta, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(22.f, 85.f), Params);
        ControlledPawn->SetActorLocation(PawnLocation + Delta * (bBlocked ? FMath::Max(0.f, Hit.Time - 0.02f) : 1.f));
    }
    else if (bGhost && !FMath::IsNearlyZero(DeadZone(ScrollInput)))
        ControlledPawn->AddActorWorldOffset(FVector::UpVector * DeadZone(ScrollInput) * 420.f * Step);
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
void AHouseMenuInputActor::YUp()
{
    if (!bXHeld && !bVisualChordFired && GetWorld()->GetTimeSeconds() - YDownTime < 0.6f)
        if (AHouseSandboxGameMode* Sandbox = Cast<AHouseSandboxGameMode>(UGameplayStatics::GetGameMode(this))) Sandbox->ToggleSun();
    bYHeld = false;
    if (!bXHeld) bVisualChordFired = false;
}
void AHouseMenuInputActor::ADown() { bAHeld = true; ADownTime = GetWorld()->GetTimeSeconds(); if (bBHeld) bBUsedForChord = true; UE_LOG(LogTemp, Display, TEXT("HouseSandbox: XR A pressed")); }
void AHouseMenuInputActor::AUp() { bAHeld = false; if (!bBHeld) { bReturnChordFired = false; bBUsedForChord = false; } }
void AHouseMenuInputActor::BDown() { bBHeld = true; BDownTime = GetWorld()->GetTimeSeconds(); if (bAHeld) bBUsedForChord = true; UE_LOG(LogTemp, Display, TEXT("HouseSandbox: XR B pressed")); }
void AHouseMenuInputActor::BUp()
{
    if (!bAHeld && !bReturnChordFired && !bBUsedForChord && GetWorld()->GetTimeSeconds() - BDownTime < 0.6f)
        if (AHouseSandboxGameMode* Sandbox = Cast<AHouseSandboxGameMode>(UGameplayStatics::GetGameMode(this))) Sandbox->ToggleGhost();
    bBHeld = false;
    if (!bAHeld) { bReturnChordFired = false; bBUsedForChord = false; }
}

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

void AHouseSandboxGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Notes.IsEmpty()) return;
    if (RecordingNote != INDEX_NONE) return;
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
    if (!Camera) return;
    const FVector Listener = Camera->GetComponentLocation();
    for (int32 Index = 0; Index < Notes.Num(); ++Index)
    {
        if (!Notes[Index].bVoice || !NoteAudio.IsValidIndex(Index) || !NoteAudio[Index]) continue;
        const float Distance = FVector::Dist(Listener, Notes[Index].Location);
        if (VoiceCooldown.IsValidIndex(Index)) VoiceCooldown[Index] = FMath::Max(0.f, VoiceCooldown[Index] - DeltaSeconds);
        if (VoiceRemaining.IsValidIndex(Index) && VoiceRemaining[Index] > 0.f)
        {
            if (Distance > 1200.f || Notes[Index].bMuted)
            {
                StopVoicePlayback(Index);
                continue;
            }
            if (!Notes[Index].bPaused)
            {
                VoiceRemaining[Index] -= DeltaSeconds;
                if (VoiceRemaining[Index] <= 0.f)
                {
                    StopVoicePlayback(Index);
                    VoiceCooldown[Index] = 2.f;
                }
            }
        }
        else if (!Notes[Index].bMuted && !Notes[Index].bPaused && Distance < 950.f
            && VoiceCooldown.IsValidIndex(Index) && VoiceCooldown[Index] <= 0.f)
            StartVoicePlayback(Index);
    }
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
    HideTemplateGeometry();
    ShowPanel(EHousePanel::Library);
    MenuInput = GetWorld()->SpawnActor<AHouseMenuInputActor>();
    MenuInput->Initialize(Pawn);
    PreviewLight = NewObject<UPointLightComponent>(Pawn, TEXT("HousePreviewLight"));
    PreviewLight->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    PreviewLight->SetIntensityUnits(ELightUnits::Lumens);
    PreviewLight->SetIntensity(1800.f);
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
        Sun->SetIntensity(0.5f);
        Sun->SetCastShadows(false);
        Sun->SetVisibility(false);
    }
    SceneSky = GetWorld()->SpawnActor<ASkyLight>();
    if (SceneSky)
    {
        USkyLightComponent* Sky = SceneSky->GetLightComponent();
        Sky->SetMobility(EComponentMobility::Movable);
        Sky->SourceType = SLS_SpecifiedCubemap;
        Sky->SetCubemap(CookedSkyCubemap);
        Sky->SetIntensity(VisualSettings.AmbientIntensity);
        Sky->SetCastShadows(false);
        Sky->SetVisibility(false);
        UE_LOG(LogTemp, Display, TEXT("HouseSandbox: ambient cubemap available=%d"), CookedSkyCubemap != nullptr);
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
    if (MenuWidget)
    {
        MenuWidget->SetVisualMode(Panel == EHousePanel::Visual);
        MenuWidget->SetNotesMode(Panel == EHousePanel::Notes);
        MenuWidget->SetVoiceMode(Panel == EHousePanel::Voice);
    }
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
    ClearNotes();
    CurrentHouseFile.Empty();
    if (HouseActor) HouseActor->Destroy();
    HouseActor = nullptr;
    HouseMesh = nullptr;
    LitMaterials.Empty();
    ReferenceMaterials.Empty();
    MaterialFeatures = FHouseMaterialFeatures();
    if (PreviewLight) PreviewLight->SetVisibility(false);
    if (SceneSun) SceneSun->GetLightComponent()->SetVisibility(false);
    if (SceneSky) SceneSky->GetLightComponent()->SetVisibility(false);
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
    double Ambient = VisualSettings.AmbientIntensity;
    if (Json->TryGetNumberField(TEXT("ambientIntensity"), Ambient))
    {
        double LightingVersion = 0.0;
        Json->TryGetNumberField(TEXT("lightingVersion"), LightingVersion);
        if (LightingVersion < 2.0 && Ambient > 0.0)
            Ambient = Ambient < 1.1 ? 1.4 : 2.2;
        VisualSettings.AmbientIntensity = FMath::Clamp(static_cast<float>(Ambient), 0.f, 2.2f);
    }
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
    Json->SetNumberField(TEXT("ambientIntensity"), VisualSettings.AmbientIntensity);
    Json->SetNumberField(TEXT("lightingVersion"), 2);
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
        Sun->SetVisibility(bLightScene && bSunOn);
    }
    if (SceneSky)
    {
        USkyLightComponent* Sky = SceneSky->GetLightComponent();
        Sky->SetIntensity(VisualSettings.AmbientIntensity);
        Sky->SetVisibility(bLightScene && CookedSkyCubemap && VisualSettings.AmbientIntensity > 0.f);
    }
    if (MenuWidget) MenuWidget->UpdateVisualLabels();
    if (bSave) SaveVisualSettings();
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: visual lit=%d normal=%d ao=%d metalrough=%d emissive=%d fill=%d shadows=%d ambient=%.2f sun=%d"),
        VisualSettings.bLit, VisualSettings.bNormal, VisualSettings.bOcclusion, VisualSettings.bMetalRough,
        VisualSettings.bEmissive, VisualSettings.bFillLight, VisualSettings.bShadows, VisualSettings.AmbientIntensity, bSunOn);
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
    case 7:
        VisualSettings.AmbientIntensity = VisualSettings.AmbientIntensity < 0.4f ? 1.4f
            : VisualSettings.AmbientIntensity < 1.8f ? 2.2f : 0.f;
        break;
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
    case 203: ToggleSun(); return;
    case 204: ToggleGhost(); return;
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
    ClearNotes();
    HouseActor = NewActor;
    CurrentHouseFile = FileName;
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

void AHouseSandboxGameMode::ToggleSun()
{
    bSunOn = !bSunOn;
    ApplyVisualSettings(false);
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: sun %d"), bSunOn);
}

void AHouseSandboxGameMode::ToggleGhost()
{
    if (bGhost)
    {
        APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
        UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
        if (Pawn && Camera)
        {
            const FVector P = Pawn->GetActorLocation();
            const FVector H = Camera->GetComponentLocation();
            FCollisionQueryParams Params(SCENE_QUERY_STAT(HouseGhostExit), false, Pawn);
            if (GetWorld()->OverlapBlockingTestByChannel(FVector(H.X, H.Y, P.Z + 92.f), FQuat::Identity,
                ECC_Pawn, FCollisionShape::MakeCapsule(22.f, 85.f), Params))
            {
                SetError(TEXT("Sal de la pared antes de desactivar vuelo"));
                return;
            }
        }
    }
    bGhost = !bGhost;
    SetError(bGhost ? TEXT("Vuelo fantasma: palanca derecha arriba/abajo") : TEXT("Colisiones activas"));
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: ghost %d"), bGhost);
}

void AHouseSandboxGameMode::ClearNotes()
{
    if (RecordingNote != INDEX_NONE) HouseVoice::StopRecording();
    RecordingNote = INDEX_NONE;
    bStoppingCapture = false;
    LastCaptureState = -1;
    PendingVoiceFile.Empty();
    GetWorldTimerManager().ClearTimer(VoiceCaptureTimer);
    for (AActor* Actor : NoteActors) if (Actor) Actor->Destroy();
    NoteActors.Empty();
    NoteAudio.Empty();
    NoteWaves.Empty();
    VoiceRemaining.Empty();
    VoiceCooldown.Empty();
    Notes.Empty();
    EditingNote = INDEX_NONE;
}

void AHouseSandboxGameMode::RebuildNotes()
{
    for (AActor* Actor : NoteActors) if (Actor) Actor->Destroy();
    NoteActors.SetNumZeroed(Notes.Num());
    NoteAudio.SetNumZeroed(Notes.Num());
    NoteWaves.SetNumZeroed(Notes.Num());
    VoiceRemaining.Init(0.f, Notes.Num());
    VoiceCooldown.Init(0.f, Notes.Num());
    for (int32 Index = 0; Index < Notes.Num(); ++Index)
    {
        AActor* Actor = GetWorld()->SpawnActor<AActor>(Notes[Index].Location, Notes[Index].Rotation);
        if (!Actor) continue;
        UWidgetComponent* Widget = NewObject<UWidgetComponent>(Actor);
        Actor->SetRootComponent(Widget);
        Widget->SetWidgetSpace(EWidgetSpace::World);
        Widget->SetDrawSize(FVector2D(500, 240));
        Widget->SetWorldScale3D(FVector(0.08f));
        Widget->SetTwoSided(true);
        Widget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Widget->SetCollisionResponseToAllChannels(ECR_Ignore);
        Widget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Widget->RegisterComponent();
        Actor->SetActorLocationAndRotation(Notes[Index].Location, Notes[Index].Rotation);
        UHouseNoteWidget* View = CreateWidget<UHouseNoteWidget>(GetWorld(), UHouseNoteWidget::StaticClass());
        if (View)
        {
            View->ParentMenu = MenuWidget;
            View->NoteIndex = Index;
            View->NoteContent = Notes[Index].Text;
            View->Refresh();
            View->SetVoiceState(Notes[Index].bVoice, false, Notes[Index].bPaused, Notes[Index].bMuted);
            Widget->SetWidget(View);
        }
        NoteActors[Index] = Actor;
        if (Notes[Index].bVoice)
        {
            UAudioComponent* Audio = NewObject<UAudioComponent>(Actor);
            Audio->SetupAttachment(Widget);
            Audio->bAutoActivate = false;
            Audio->bAllowSpatialization = true;
            Audio->bOverrideAttenuation = true;
            Audio->AttenuationOverrides.bAttenuate = true;
            Audio->AttenuationOverrides.bSpatialize = true;
            Audio->AttenuationOverrides.SpatializationAlgorithm = SPATIALIZATION_HRTF;
            Audio->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
            Audio->AttenuationOverrides.AttenuationShapeExtents = FVector(70.f);
            Audio->AttenuationOverrides.FalloffDistance = 930.f;
            Audio->RegisterComponent();
            Audio->SetVolumeMultiplier(1.25f);
            NoteAudio[Index] = Audio;
        }
    }
}

void AHouseSandboxGameMode::StartNewNote()
{
    CreateAnchoredNote(false);
}

void AHouseSandboxGameMode::StartNewVoiceNote()
{
    CreateAnchoredNote(true);
}

void AHouseSandboxGameMode::CreateAnchoredNote(bool bVoice)
{
    if (!HouseMesh) { SetError(TEXT("Carga una casa antes de crear notas")); return; }
    if (Notes.Num() >= 100) { SetError(TEXT("Limite de 100 notas por partida")); return; }
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
    if (!Camera) return;
    const FVector Start = Camera->GetComponentLocation();
    const FVector End = Start + Camera->GetForwardVector() * 600.f;
    FHitResult Hit;
    const FCollisionQueryParams Params(SCENE_QUERY_STAT(HouseNote), true);
    if (!HouseMesh->LineTraceComponent(Hit, Start, End, Params))
    {
        SetError(TEXT("Mira una superficie de la casa a menos de 6 m"));
        return;
    }
    FHouseNote Note;
    Note.Text = TEXT("");
    Note.bVoice = bVoice;
    Note.Location = Hit.ImpactPoint + Hit.ImpactNormal * 8.f;
    Note.Rotation = (Start - Note.Location).Rotation();
    Notes.Add(Note);
    EditingNote = Notes.Num() - 1;
    RebuildNotes();
    ShowPanel(bVoice ? EHousePanel::Voice : EHousePanel::Notes);
    if (bVoice) RecordVoiceForEditing();
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: %s note added index=%d"), bVoice ? TEXT("voice") : TEXT("text"), EditingNote);
}

void AHouseSandboxGameMode::EditNote(int32 Index)
{
    if (!Notes.IsValidIndex(Index)) return;
    EditingNote = Index;
    ShowPanel(Notes[Index].bVoice ? EHousePanel::Voice : EHousePanel::Notes);
}

FString AHouseSandboxGameMode::GetEditingText() const
{
    return Notes.IsValidIndex(EditingNote) ? Notes[EditingNote].Text : FString();
}

void AHouseSandboxGameMode::SetNoteText(const FString& Text)
{
    if (!Notes.IsValidIndex(EditingNote) || Notes[EditingNote].bVoice) return;
    Notes[EditingNote].Text = Text.Left(300);
    if (NoteActors.IsValidIndex(EditingNote) && NoteActors[EditingNote])
        if (UWidgetComponent* Widget = NoteActors[EditingNote]->FindComponentByClass<UWidgetComponent>())
            if (UHouseNoteWidget* View = Cast<UHouseNoteWidget>(Widget->GetWidget()))
            { View->NoteContent = Notes[EditingNote].Text; View->Refresh(); }
}

void AHouseSandboxGameMode::DeleteNote()
{
    if (!Notes.IsValidIndex(EditingNote)) return;
    if (RecordingNote == EditingNote) { SetError(TEXT("Espera a que termine de guardarse el audio")); return; }
    Notes.RemoveAt(EditingNote);
    EditingNote = INDEX_NONE;
    RebuildNotes();
    ShowPanel(EHousePanel::Closed);
}

void AHouseSandboxGameMode::RecordVoiceForEditing()
{
    if (!Notes.IsValidIndex(EditingNote) || !Notes[EditingNote].bVoice || RecordingNote != INDEX_NONE) return;
    for (int32 Index = 0; Index < Notes.Num(); ++Index) StopVoicePlayback(Index);
    PendingVoiceFile = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower() + TEXT(".wav");
    const int32 State = HouseVoice::StartRecording(PendingVoiceFile);
    if (State < 0)
    {
        PendingVoiceFile.Empty();
        SetError(TEXT("No se pudo iniciar el microfono"));
        return;
    }
    RecordingNote = EditingNote;
    bStoppingCapture = false;
    LastCaptureState = State;
    GetWorldTimerManager().SetTimer(VoiceCaptureTimer, this, &AHouseSandboxGameMode::PollVoiceCapture, 0.2f, true);
    SetError(State == 0 ? TEXT("Acepta el permiso de microfono para grabar") : TEXT("Grabando voz; pulsa DETENER"));
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: voice capture requested index=%d state=%d"), RecordingNote, State);
}

void AHouseSandboxGameMode::StopVoiceRecording()
{
    if (RecordingNote == INDEX_NONE) return;
    bStoppingCapture = true;
    HouseVoice::StopRecording();
    SetError(TEXT("Guardando nota de voz..."));
}

void AHouseSandboxGameMode::PollVoiceCapture()
{
    if (RecordingNote == INDEX_NONE) { GetWorldTimerManager().ClearTimer(VoiceCaptureTimer); return; }
    const int32 State = HouseVoice::RecordingState();
    if (State != LastCaptureState)
    {
        LastCaptureState = State;
        if (State == 1) SetError(TEXT("Grabando voz; pulsa DETENER"));
    }
    if (State == 0 || State == 1) return;
    const int32 FinishedNote = RecordingNote;
    RecordingNote = INDEX_NONE;
    bStoppingCapture = false;
    LastCaptureState = -1;
    GetWorldTimerManager().ClearTimer(VoiceCaptureTimer);
    if (State == 2 && Notes.IsValidIndex(FinishedNote))
    {
        TArray<uint8> Pcm;
        if (HouseVoice::LoadPcm(GetModelDirectory(), PendingVoiceFile, Pcm))
        {
            Notes[FinishedNote].VoiceFile = PendingVoiceFile;
            Notes[FinishedNote].bPaused = false;
            Notes[FinishedNote].bMuted = false;
            RebuildNotes();
            SetError(TEXT("Nota de voz guardada en la superficie"));
            UE_LOG(LogTemp, Display, TEXT("HouseSandbox: voice note saved index=%d bytes=%d"), FinishedNote, Pcm.Num());
        }
        else SetError(TEXT("La grabacion de voz no se pudo leer"));
    }
    else SetError(State == -2 ? TEXT("Permiso de microfono denegado") : TEXT("No se pudo guardar la nota de voz"));
    PendingVoiceFile.Empty();
    if (MenuWidget) MenuWidget->Populate(this);
}

FString AHouseSandboxGameMode::GetVoiceStatus() const
{
    if (!Notes.IsValidIndex(EditingNote) || !Notes[EditingNote].bVoice) return TEXT("");
    if (RecordingNote == EditingNote)
        return bStoppingCapture ? TEXT("Guardando audio...") : HouseVoice::RecordingState() == 0
            ? TEXT("Esperando permiso de microfono...") : TEXT("GRABANDO  |  maximo 30 segundos");
    const FHouseNote& Note = Notes[EditingNote];
    if (Note.VoiceFile.IsEmpty()) return TEXT("Pulsa GRABAR para dejar un audio aqui");
    if (Note.bMuted) return TEXT("SILENCIADA");
    if (Note.bPaused) return TEXT("EN PAUSA");
    return TEXT("Audio espacial activo cerca de esta nota");
}

void AHouseSandboxGameMode::ToggleVoicePause()
{
    if (!Notes.IsValidIndex(EditingNote) || !Notes[EditingNote].bVoice || RecordingNote != INDEX_NONE) return;
    FHouseNote& Note = Notes[EditingNote];
    Note.bPaused = !Note.bPaused;
    if (NoteAudio.IsValidIndex(EditingNote) && NoteAudio[EditingNote] && VoiceRemaining.IsValidIndex(EditingNote)
        && VoiceRemaining[EditingNote] > 0.f) NoteAudio[EditingNote]->SetPaused(Note.bPaused);
    UpdateVoiceMarker(EditingNote);
    if (MenuWidget) MenuWidget->Populate(this);
}

void AHouseSandboxGameMode::ToggleVoiceMute()
{
    if (!Notes.IsValidIndex(EditingNote) || !Notes[EditingNote].bVoice || RecordingNote != INDEX_NONE) return;
    FHouseNote& Note = Notes[EditingNote];
    Note.bMuted = !Note.bMuted;
    if (Note.bMuted) StopVoicePlayback(EditingNote);
    UpdateVoiceMarker(EditingNote);
    if (MenuWidget) MenuWidget->Populate(this);
}

void AHouseSandboxGameMode::StartVoicePlayback(int32 Index)
{
    if (!Notes.IsValidIndex(Index) || !NoteAudio.IsValidIndex(Index) || !NoteAudio[Index]
        || Notes[Index].VoiceFile.IsEmpty()) return;
    TArray<uint8> Pcm;
    if (!HouseVoice::LoadPcm(GetModelDirectory(), Notes[Index].VoiceFile, Pcm))
    {
        VoiceCooldown[Index] = 30.f;
        UE_LOG(LogTemp, Warning, TEXT("HouseSandbox: missing or invalid voice file %s"), *Notes[Index].VoiceFile);
        return;
    }
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(NoteAudio[Index]);
    Wave->SetSampleRate(HouseVoice::SampleRate);
    Wave->SetNumFrames(Pcm.Num() / 2);
    Wave->NumChannels = 1;
    Wave->Duration = float(Pcm.Num()) / (HouseVoice::SampleRate * 2.f);
    Wave->SoundGroup = SOUNDGROUP_Voice;
    Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
    NoteWaves[Index] = Wave;
    NoteAudio[Index]->SetSound(Wave);
    NoteAudio[Index]->Play();
    VoiceRemaining[Index] = Wave->Duration;
    UpdateVoiceMarker(Index);
}

void AHouseSandboxGameMode::StopVoicePlayback(int32 Index)
{
    if (!VoiceRemaining.IsValidIndex(Index)) return;
    if (NoteAudio.IsValidIndex(Index) && NoteAudio[Index]) NoteAudio[Index]->Stop();
    VoiceRemaining[Index] = 0.f;
    if (NoteWaves.IsValidIndex(Index)) NoteWaves[Index] = nullptr;
    UpdateVoiceMarker(Index);
}

void AHouseSandboxGameMode::UpdateVoiceMarker(int32 Index)
{
    if (!Notes.IsValidIndex(Index) || !NoteActors.IsValidIndex(Index) || !NoteActors[Index]) return;
    if (UWidgetComponent* Widget = NoteActors[Index]->FindComponentByClass<UWidgetComponent>())
        if (UHouseNoteWidget* View = Cast<UHouseNoteWidget>(Widget->GetWidget()))
            View->SetVoiceState(Notes[Index].bVoice, VoiceRemaining.IsValidIndex(Index) && VoiceRemaining[Index] > 0.f
                && !Notes[Index].bPaused && !Notes[Index].bMuted, Notes[Index].bPaused, Notes[Index].bMuted);
}

TArray<FString> AHouseSandboxGameMode::GetSaveSlots() const
{
    TArray<FString> Slots;
    IFileManager::Get().FindFiles(Slots, *FPaths::Combine(GetModelDirectory(), TEXT("Saves"), TEXT("*.json")), true, false);
    for (FString& Slot : Slots) Slot = FPaths::GetBaseFilename(Slot);
    Slots.Sort();
    return Slots;
}

void AHouseSandboxGameMode::SaveGame(const FString& Slot)
{
    if (CurrentHouseFile.IsEmpty()) { SetError(TEXT("Carga una casa antes de guardar")); return; }
    if (RecordingNote != INDEX_NONE) { SetError(TEXT("Deten la grabacion antes de guardar")); return; }
    for (const FHouseNote& Note : Notes)
        if (Note.bVoice && Note.VoiceFile.IsEmpty())
        { SetError(TEXT("Termina de grabar cada nota de voz antes de guardar")); return; }
    bool bValidSlot = !Slot.IsEmpty() && Slot.Len() <= 40;
    for (TCHAR C : Slot) bValidSlot &= FChar::IsAlnum(C) || C == TEXT('_');
    if (!bValidSlot)
    { SetError(TEXT("Nombre de partida: letras, numeros o _ (maximo 40)")); return; }
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Pawn) return;
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("version"), 2);
    Json->SetStringField(TEXT("model"), CurrentHouseFile);
    const FVector P = Pawn->GetActorLocation();
    Json->SetNumberField(TEXT("x"), P.X); Json->SetNumberField(TEXT("y"), P.Y); Json->SetNumberField(TEXT("z"), P.Z);
    Json->SetNumberField(TEXT("yaw"), Pawn->GetActorRotation().Yaw);
    TArray<TSharedPtr<FJsonValue>> SavedNotes;
    for (const FHouseNote& Note : Notes)
    {
        TSharedRef<FJsonObject> N = MakeShared<FJsonObject>();
        N->SetStringField(TEXT("text"), Note.Text);
        if (Note.bVoice)
        {
            N->SetStringField(TEXT("kind"), TEXT("voice"));
            N->SetStringField(TEXT("voice"), Note.VoiceFile);
            N->SetBoolField(TEXT("paused"), Note.bPaused);
            N->SetBoolField(TEXT("muted"), Note.bMuted);
        }
        N->SetNumberField(TEXT("x"), Note.Location.X); N->SetNumberField(TEXT("y"), Note.Location.Y); N->SetNumberField(TEXT("z"), Note.Location.Z);
        N->SetNumberField(TEXT("pitch"), Note.Rotation.Pitch); N->SetNumberField(TEXT("yaw"), Note.Rotation.Yaw);
        SavedNotes.Add(MakeShared<FJsonValueObject>(N));
    }
    Json->SetArrayField(TEXT("notes"), SavedNotes);
    FString Contents;
    if (!FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Contents))) return;
    const FString Dir = FPaths::Combine(GetModelDirectory(), TEXT("Saves"));
    IFileManager::Get().MakeDirectory(*Dir, true);
    const FString Target = FPaths::Combine(Dir, Slot + TEXT(".json"));
    const FString Temporary = Target + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(Contents, *Temporary) || !IFileManager::Get().Move(*Target, *Temporary, true))
    { SetError(TEXT("No se pudo guardar la partida")); return; }
    SetError(TEXT("Partida guardada: ") + Slot);
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: game saved slot=%s notes=%d"), *Slot, Notes.Num());
}

void AHouseSandboxGameMode::LoadGame(const FString& Slot)
{
    if (!GetSaveSlots().Contains(Slot)) { SetError(TEXT("Partida no encontrada")); return; }
    FString Contents;
    if (!FFileHelper::LoadFileToString(Contents, *FPaths::Combine(GetModelDirectory(), TEXT("Saves"), Slot + TEXT(".json")))) return;
    TSharedPtr<FJsonObject> Json;
    double Version = 0;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Contents), Json) || !Json.IsValid()
        || !Json->TryGetNumberField(TEXT("version"), Version) || (Version != 1 && Version != 2))
    { SetError(TEXT("Partida incompatible")); return; }
    FString Model;
    if (!Json->TryGetStringField(TEXT("model"), Model) || !LoadHouse(Model)) return;
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (Pawn)
    {
        double X = 0, Y = 0, Z = 0, Yaw = 0;
        Json->TryGetNumberField(TEXT("x"), X); Json->TryGetNumberField(TEXT("y"), Y);
        Json->TryGetNumberField(TEXT("z"), Z); Json->TryGetNumberField(TEXT("yaw"), Yaw);
        if (FMath::IsFinite(X) && FMath::IsFinite(Y) && FMath::IsFinite(Z) && FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)) < 100000.f)
        { Pawn->SetActorLocation(FVector(X, Y, Z)); Pawn->SetActorRotation(FRotator(0, Yaw, 0)); }
    }
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Json->TryGetArrayField(TEXT("notes"), Values))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const TSharedPtr<FJsonObject> N = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!N.IsValid() || Notes.Num() >= 100) continue;
            FHouseNote Note;
            double X = 0, Y = 0, Z = 0, Pitch = 0, Yaw = 0;
            N->TryGetStringField(TEXT("text"), Note.Text);
            FString Kind;
            N->TryGetStringField(TEXT("kind"), Kind);
            if (Kind == TEXT("voice"))
            {
                Note.bVoice = true;
                N->TryGetStringField(TEXT("voice"), Note.VoiceFile);
                if (!HouseVoice::IsValidName(Note.VoiceFile)
                    || !IFileManager::Get().FileExists(*HouseVoice::FilePath(GetModelDirectory(), Note.VoiceFile))) continue;
                N->TryGetBoolField(TEXT("paused"), Note.bPaused);
                N->TryGetBoolField(TEXT("muted"), Note.bMuted);
            }
            N->TryGetNumberField(TEXT("x"), X); N->TryGetNumberField(TEXT("y"), Y); N->TryGetNumberField(TEXT("z"), Z);
            N->TryGetNumberField(TEXT("pitch"), Pitch); N->TryGetNumberField(TEXT("yaw"), Yaw);
            if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)) continue;
            Note.Location = FVector(X, Y, Z); Note.Rotation = FRotator(Pitch, Yaw, 0);
            Note.Text = Note.Text.Left(300);
            Notes.Add(Note);
        }
    }
    RebuildNotes();
    SetError(TEXT("Partida cargada: ") + Slot);
    ShowPanel(EHousePanel::Closed);
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: game loaded slot=%s notes=%d"), *Slot, Notes.Num());
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

void UHouseActionButton::Activate()
{
    if (!ParentMenu) return;
    if (Value == TEXT("save")) ParentMenu->OpenSave(TEXT("@save"));
    else if (Value == TEXT("delete")) ParentMenu->TypeNoteCharacter(TEXT("@delete"));
    else if (Value == TEXT("note")) ParentMenu->OpenNote(Index);
    else if (Value.StartsWith(TEXT("slot:"))) ParentMenu->OpenSave(Value.Mid(5));
    else ParentMenu->TypeNoteCharacter(Value);
}

bool UHouseNoteWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    Outline = WidgetTree->ConstructWidget<UBorder>();
    Outline->SetPadding(FMargin(5.f));
    Outline->SetBrushColor(CyberCyan);
    NoteButton = WidgetTree->ConstructWidget<UHouseActionButton>();
    NoteButton->Value = TEXT("note");
    NoteButton->SetBackgroundColor(CyberAction);
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    SpeakerIcon = WidgetTree->ConstructWidget<UImage>();
    SpeakerIcon->SetDesiredSizeOverride(FVector2D(60.f, 60.f));
    SpeakerIcon->SetVisibility(ESlateVisibility::Collapsed);
    SpeakerTexture = UTexture2D::CreateTransient(64, 64, PF_B8G8R8A8);
    if (SpeakerTexture && SpeakerTexture->GetPlatformData() && SpeakerTexture->GetPlatformData()->Mips.Num())
    {
        FTexture2DMipMap& Mip = SpeakerTexture->GetPlatformData()->Mips[0];
        FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
        for (int32 Y = 0; Y < 64; ++Y) for (int32 X = 0; X < 64; ++X)
        {
            const bool Body = X >= 8 && X <= 20 && Y >= 25 && Y <= 39;
            const bool Cone = X > 20 && X <= 35 && FMath::Abs(Y - 32) <= 7 + (X - 20) * 0.8f;
            const float Radius = FMath::Sqrt(FMath::Square(float(X - 28)) + FMath::Square(float(Y - 32)));
            const bool SoundArc = X > 38 && (FMath::Abs(Radius - 20.f) < 2.f || FMath::Abs(Radius - 30.f) < 2.f);
            Pixels[Y * 64 + X] = Body || Cone || SoundArc ? FColor(79, 230, 250, 255) : FColor(0, 0, 0, 0);
        }
        Mip.BulkData.Unlock();
        SpeakerTexture->UpdateResource();
        SpeakerIcon->SetBrushFromTexture(SpeakerTexture);
    }
    Row->AddChildToHorizontalBox(SpeakerIcon);
    Label = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo Font = Label->GetFont(); Font.Size = 30; Label->SetFont(Font);
    Label->SetColorAndOpacity(FSlateColor(CyberText));
    Label->SetAutoWrapText(true);
    Row->AddChildToHorizontalBox(Label);
    NoteButton->AddChild(Row);
    NoteButton->OnClicked.AddDynamic(NoteButton, &UHouseActionButton::Activate);
    Outline->SetContent(NoteButton);
    WidgetTree->RootWidget = Outline;
    return true;
}

void UHouseNoteWidget::Refresh()
{
    if (NoteButton) { NoteButton->ParentMenu = ParentMenu; NoteButton->Index = NoteIndex; }
    if (Label && !bVoice) Label->SetText(FText::FromString(NoteContent.IsEmpty() ? TEXT("(nota vacia)") : NoteContent));
}

void UHouseNoteWidget::SetVoiceState(bool bInVoice, bool bInPlaying, bool bInPaused, bool bInMuted)
{
    bVoice = bInVoice;
    bPlaying = bInPlaying;
    bPaused = bInPaused;
    bMuted = bInMuted;
    if (SpeakerIcon) SpeakerIcon->SetVisibility(bVoice ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Label && bVoice) Label->SetText(FText::FromString(bPlaying ? TEXT("REPRODUCIENDO")
        : bMuted ? TEXT("AUDIO SILENCIADO") : bPaused ? TEXT("AUDIO EN PAUSA") : TEXT("NOTA DE VOZ")));
    if (Outline) Outline->SetBrushColor(bPlaying ? CyberMagenta : bMuted ? CyberBackground : CyberCyan);
}

void UHouseNoteWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bPlaying && Outline)
    {
        const float Pulse = 0.7f + 0.3f * FMath::Sin(GetWorld()->GetTimeSeconds() * 7.f);
        Outline->SetBrushColor(FLinearColor(CyberMagenta.R * Pulse, CyberMagenta.G * Pulse,
            CyberMagenta.B * Pulse, 1.f));
    }
}

bool UHouseSandboxWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Backdrop->SetBrushColor(CyberBackground);
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
        Text->SetColorAndOpacity(FSlateColor(Size >= 30 ? CyberCyan : CyberText));
        Text->SetAutoWrapText(true);
        Parent->AddChildToVerticalBox(Text);
        return Text;
    };
    auto AddButton = [&](UVerticalBox* Parent, const FString& Label, FName Name) -> UButton*
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        Button->SetBackgroundColor(CyberAction);
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label));
        FSlateFontInfo Font = Text->GetFont(); Font.Size = 27; Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(CyberText));
        Text->SetAutoWrapText(true);
        Button->AddChild(Text);
        if (UVerticalBoxSlot* MenuSlot = Parent->AddChildToVerticalBox(Button)) MenuSlot->SetPadding(FMargin(0.f, 7.f));
        return Button;
    };
    AddText(Frame, TEXT("MATRIX VR SHOW"), 43);
    UTextBlock* BrandLine = AddText(Frame, TEXT("EXPLORA  /  ANOTA  /  GUARDA"), 22);
    BrandLine->SetColorAndOpacity(FSlateColor(CyberMagenta));
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
    Frame->AddChildToVerticalBox(Tabs);
    UButton* LibraryTab = WidgetTree->ConstructWidget<UButton>();
    LibraryTab->SetBackgroundColor(CyberCyan);
    UTextBlock* LibraryTabText = WidgetTree->ConstructWidget<UTextBlock>();
    LibraryTabText->SetText(FText::FromString(TEXT("MODELOS")));
    LibraryTabText->SetColorAndOpacity(FSlateColor(CyberBackground));
    LibraryTab->AddChild(LibraryTabText);
    LibraryTab->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnShowLibrary);
    Tabs->AddChildToHorizontalBox(LibraryTab);
    UButton* VisualTab = WidgetTree->ConstructWidget<UButton>();
    VisualTab->SetBackgroundColor(CyberMagenta);
    UTextBlock* VisualTabText = WidgetTree->ConstructWidget<UTextBlock>();
    VisualTabText->SetText(FText::FromString(TEXT("OPCIONES VISUALES")));
    VisualTabText->SetColorAndOpacity(FSlateColor(CyberBackground));
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
    NotesPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ContentScroll->AddChild(NotesPanel);
    NotesPanel->SetVisibility(ESlateVisibility::Collapsed);
    VoicePanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ContentScroll->AddChild(VoicePanel);
    VoicePanel->SetVisibility(ESlateVisibility::Collapsed);
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
    AddText(LibraryPanel, TEXT("PARTIDAS GUARDADAS"), 30);
    SlotBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    SlotBox->SetText(FText::FromString(TEXT("partida_1")));
    SlotBox->SetHintText(FText::FromString(TEXT("Nombre de partida")));
    LibraryPanel->AddChildToVerticalBox(SlotBox);
    UButton* Save = AddButton(LibraryPanel, TEXT("GUARDAR PARTIDA ACTUAL"), TEXT("SaveGame"));
    Save->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnSave);
    SaveList = WidgetTree->ConstructWidget<UVerticalBox>();
    LibraryPanel->AddChildToVerticalBox(SaveList);
    UButton* NewNote = AddButton(LibraryPanel, TEXT("CREAR NOTA EN LA SUPERFICIE QUE MIRO"), TEXT("NewNote"));
    NewNote->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnNewNote);
    UButton* NewVoice = AddButton(LibraryPanel, TEXT("GRABAR NOTA DE VOZ EN LA SUPERFICIE QUE MIRO"), TEXT("NewVoiceNote"));
    NewVoice->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnNewVoiceNote);
    UButton* Ghost = AddButton(LibraryPanel, TEXT("VUELO FANTASMA"), TEXT("Ghost"));
    Ghost->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnToggleGhost);
    GhostLabel = Cast<UTextBlock>(Ghost->GetContent());
    UButton* Sun = AddButton(LibraryPanel, TEXT("LUZ SOL"), TEXT("Sun"));
    Sun->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnToggleSun);
    SunLabel = Cast<UTextBlock>(Sun->GetContent());
    AddText(VisualPanel, TEXT("PERFILES"), 32);
    auto VisualButton = [&](int32 Feature, const FString& Label) -> UTextBlock*
    {
        UHouseVisualButton* Button = WidgetTree->ConstructWidget<UHouseVisualButton>();
        Button->Feature = Feature;
        Button->ParentMenu = this;
        Button->SetBackgroundColor(CyberAction);
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label));
        FSlateFontInfo Font = Text->GetFont(); Font.Size = 27; Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(CyberText));
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
    VisualButton(7, TEXT("Luz ambiental: media"));
    VisualButton(203, TEXT("ENCENDER / APAGAR SOL  ·  Y CORTO"));
    VisualButton(204, TEXT("VUELO FANTASMA  ·  B CORTO  ·  PALANCA DERECHA VERTICAL"));
    VisualButton(200, TEXT("VOLVER A MODELOS Y LIBERAR CASA"));
    VisualButton(201, TEXT("ABRIR BIBLIOTECA"));
    AddText(NotesPanel, TEXT("EDITAR NOTA"), 34);
    NoteText = AddText(NotesPanel, TEXT(""), 30);
    const TCHAR* Rows[] = { TEXT("ABCDEFGHIJ"), TEXT("KLMNOPQRST"), TEXT("UVWXYZ0123"), TEXT("456789.,!?"), TEXT("-_:;/()") };
    for (const TCHAR* Row : Rows)
    {
        UHorizontalBox* Keys = WidgetTree->ConstructWidget<UHorizontalBox>();
        NotesPanel->AddChildToVerticalBox(Keys);
        for (const TCHAR* Key = Row; *Key; ++Key)
        {
            UHouseActionButton* Button = WidgetTree->ConstructWidget<UHouseActionButton>();
            Button->SetBackgroundColor(CyberAction);
            Button->Value = FString::Chr(*Key);
            Button->ParentMenu = this;
            UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
            Label->SetText(FText::FromString(Button->Value));
            FSlateFontInfo Font = Label->GetFont(); Font.Size = 31; Label->SetFont(Font);
            Label->SetColorAndOpacity(FSlateColor(CyberText));
            Button->AddChild(Label);
            Button->OnClicked.AddDynamic(Button, &UHouseActionButton::Activate);
            Keys->AddChildToHorizontalBox(Button);
        }
    }
    auto NoteKey = [&](const FString& Label, const FString& Value)
    {
        UHouseActionButton* Button = WidgetTree->ConstructWidget<UHouseActionButton>();
        Button->SetBackgroundColor(CyberAction);
        Button->Value = Value; Button->ParentMenu = this;
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label));
        Text->SetColorAndOpacity(FSlateColor(CyberText));
        Button->AddChild(Text);
        Button->OnClicked.AddDynamic(Button, &UHouseActionButton::Activate);
        NotesPanel->AddChildToVerticalBox(Button);
    };
    NoteKey(TEXT("ESPACIO"), TEXT(" "));
    NoteKey(TEXT("BORRAR ULTIMO CARACTER"), TEXT("@backspace"));
    UButton* Delete = AddButton(NotesPanel, TEXT("ELIMINAR NOTA"), TEXT("DeleteNote"));
    Delete->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnDeleteNote);
    UButton* Done = AddButton(NotesPanel, TEXT("LISTO"), TEXT("DoneNote"));
    Done->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnClose);
    AddText(VoicePanel, TEXT("NOTA DE VOZ ESPACIAL"), 34);
    VoiceStatus = AddText(VoicePanel, TEXT("Pulsa GRABAR"), 27);
    AddText(VoicePanel, TEXT("Se oye desde esta superficie al acercarte. Maximo 30 segundos."), 23);
    UButton* Record = AddButton(VoicePanel, TEXT("GRABAR / VOLVER A GRABAR"), TEXT("RecordVoice"));
    Record->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnRecordVoice);
    UButton* Stop = AddButton(VoicePanel, TEXT("DETENER Y GUARDAR AUDIO"), TEXT("StopVoice"));
    Stop->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnStopVoice);
    UButton* Pause = AddButton(VoicePanel, TEXT("PAUSAR AUDIO"), TEXT("PauseVoice"));
    Pause->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnPauseVoice);
    VoicePauseLabel = Cast<UTextBlock>(Pause->GetContent());
    UButton* Mute = AddButton(VoicePanel, TEXT("SILENCIAR AUDIO"), TEXT("MuteVoice"));
    Mute->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnMuteVoice);
    VoiceMuteLabel = Cast<UTextBlock>(Mute->GetContent());
    UButton* DeleteVoice = AddButton(VoicePanel, TEXT("ELIMINAR NOTA DE VOZ"), TEXT("DeleteVoice"));
    DeleteVoice->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnDeleteNote);
    UButton* DoneVoice = AddButton(VoicePanel, TEXT("LISTO"), TEXT("DoneVoice"));
    DoneVoice->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnClose);
    UButton* Recenter = AddButton(Frame, TEXT("RECOLOCAR PANEL DELANTE DE MÍ"), TEXT("RecenterPanel"));
    Recenter->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnRecenter);
    UButton* Close = AddButton(Frame, TEXT("CERRAR  ·  X PARA ABRIR  ·  X+Y VISUALES  ·  A+B MODELOS"), TEXT("ClosePanel"));
    Close->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnClose);
    StatusText = AddText(Frame, TEXT("Listo"), 24);
    StatusText->SetColorAndOpacity(FSlateColor(CyberMagenta));
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
        Label->SetColorAndOpacity(FSlateColor(CyberText));
        Label->SetAutoWrapText(true);
        Button->SetBackgroundColor(CyberAction);
        Button->AddChild(Label);
        Button->OnClicked.AddDynamic(Button, &UHouseFileButton::ActivateFile);
        if (UVerticalBoxSlot* MenuSlot = FileList->AddChildToVerticalBox(Button)) MenuSlot->SetPadding(FMargin(0.f, 7.f));
    }
    if (SaveList)
    {
        SaveList->ClearChildren();
        for (const FString& SaveName : Sandbox->GetSaveSlots())
        {
            UHouseActionButton* Button = WidgetTree->ConstructWidget<UHouseActionButton>();
            Button->ParentMenu = this; Button->Value = TEXT("slot:") + SaveName;
            UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
            Label->SetText(FText::FromString(TEXT("CARGAR: ") + SaveName));
            FSlateFontInfo Font = Label->GetFont(); Font.Size = 27; Label->SetFont(Font);
            Label->SetColorAndOpacity(FSlateColor(CyberText));
            Button->SetBackgroundColor(CyberAction);
            Button->AddChild(Label);
            Button->OnClicked.AddDynamic(Button, &UHouseActionButton::Activate);
            SaveList->AddChildToVerticalBox(Button);
        }
    }
    if (GhostLabel) GhostLabel->SetText(FText::FromString(Sandbox->IsGhost() ? TEXT("VUELO FANTASMA: ACTIVO") : TEXT("VUELO FANTASMA: APAGADO")));
    if (SunLabel) SunLabel->SetText(FText::FromString(Sandbox->IsSunOn() ? TEXT("LUZ SOL: ACTIVA") : TEXT("LUZ SOL: APAGADA")));
    if (ContentScroll) ContentScroll->SetScrollOffset(PreviousScroll);
    UpdateVisualLabels();
    RefreshVoiceEditor();
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

void UHouseSandboxWidget::SetNotesMode(bool bNotes)
{
    if (bNotes && LibraryPanel) LibraryPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (bNotes && VisualPanel) VisualPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (NotesPanel) NotesPanel->SetVisibility(bNotes ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bNotes) RefreshNoteEditor();
}

void UHouseSandboxWidget::SetVoiceMode(bool bVoice)
{
    if (bVoice && LibraryPanel) LibraryPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (bVoice && VisualPanel) VisualPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (bVoice && NotesPanel) NotesPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (VoicePanel) VoicePanel->SetVisibility(bVoice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bVoice && ContentScroll) ContentScroll->SetScrollOffset(0.f);
    if (bVoice) RefreshVoiceEditor();
}

void UHouseSandboxWidget::ScrollBy(float Amount)
{
    if (ContentScroll) ContentScroll->SetScrollOffset(FMath::Max(0.f, ContentScroll->GetScrollOffset() + Amount));
}

void UHouseSandboxWidget::UpdateVisualLabels()
{
    if (!Sandbox || VisualLabels.Num() < 8 || VisualButtons.Num() < 8) return;
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
    const float Ambient = V.AmbientIntensity;
    const TCHAR* AmbientName = Ambient < 0.1f ? TEXT("APAGADA") : Ambient < 1.8f ? TEXT("MEDIA") : TEXT("ALTA");
    VisualLabels[7]->SetText(FText::FromString(FString::Printf(TEXT("Luz ambiental   ·   %s"), AmbientName)));
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
void UHouseSandboxWidget::OpenSave(const FString& SaveName)
{
    if (Sandbox && SaveName != TEXT("@save")) Sandbox->LoadGame(SaveName);
}
void UHouseSandboxWidget::OpenNote(int32 Index)
{
    if (Sandbox) Sandbox->EditNote(Index);
}
void UHouseSandboxWidget::RefreshNoteEditor()
{
    if (NoteText && Sandbox) NoteText->SetText(FText::FromString(Sandbox->GetEditingText()));
}
void UHouseSandboxWidget::RefreshVoiceEditor()
{
    if (!Sandbox) return;
    if (VoiceStatus) VoiceStatus->SetText(FText::FromString(Sandbox->GetVoiceStatus()));
    if (VoicePauseLabel) VoicePauseLabel->SetText(FText::FromString(TEXT("PAUSAR / REANUDAR AUDIO")));
    if (VoiceMuteLabel) VoiceMuteLabel->SetText(FText::FromString(TEXT("SILENCIAR / ACTIVAR AUDIO")));
}
void UHouseSandboxWidget::TypeNoteCharacter(const FString& Character)
{
    if (!Sandbox) return;
    FString Text = Sandbox->GetEditingText();
    if (Character == TEXT("@backspace")) { if (!Text.IsEmpty()) Text.LeftChopInline(1); }
    else Text += Character;
    Sandbox->SetNoteText(Text);
    RefreshNoteEditor();
}
void UHouseSandboxWidget::OnNewNote() { if (Sandbox) Sandbox->StartNewNote(); }
void UHouseSandboxWidget::OnNewVoiceNote() { if (Sandbox) Sandbox->StartNewVoiceNote(); }
void UHouseSandboxWidget::OnRecordVoice() { if (Sandbox) Sandbox->RecordVoiceForEditing(); }
void UHouseSandboxWidget::OnStopVoice() { if (Sandbox) Sandbox->StopVoiceRecording(); }
void UHouseSandboxWidget::OnPauseVoice() { if (Sandbox) Sandbox->ToggleVoicePause(); }
void UHouseSandboxWidget::OnMuteVoice() { if (Sandbox) Sandbox->ToggleVoiceMute(); }
void UHouseSandboxWidget::OnSave() { if (Sandbox && SlotBox) Sandbox->SaveGame(SlotBox->GetText().ToString().TrimStartAndEnd()); }
void UHouseSandboxWidget::OnDeleteNote() { if (Sandbox) Sandbox->DeleteNote(); }
void UHouseSandboxWidget::OnToggleGhost() { if (Sandbox) Sandbox->ToggleGhost(); }
void UHouseSandboxWidget::OnToggleSun() { if (Sandbox) { Sandbox->ToggleSun(); Sandbox->RefreshMenu(); } }
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
        Label->SetColorAndOpacity(FSlateColor(CyberText));
        Label->SetAutoWrapText(true);
        Button->SetBackgroundColor(CyberAction);
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

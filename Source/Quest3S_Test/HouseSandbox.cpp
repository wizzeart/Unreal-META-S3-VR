#include "HouseSandbox.h"

#include "GlbRuntime.h"
#include "ProceduralMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "MotionControllerComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Camera/CameraComponent.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
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
        InputComponent->BindKey(EKeys::OculusTouch_Left_X_Click, IE_Pressed, this, &AHouseMenuInputActor::ToggleMenu).bConsumeInput = false;
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
    const float Step = FMath::Min(DeltaSeconds, 0.05f);
    auto DeadZone = [](float Value) { return FMath::Abs(Value) < 0.15f ? 0.f : Value; };
    if (AHouseSandboxGameMode* Sandbox = Cast<AHouseSandboxGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (Sandbox->IsMenuVisible())
        {
            const float Scroll = DeadZone(ScrollInput);
            if (Pointer && !FMath::IsNearlyZero(Scroll)) Pointer->ScrollWheel(-Scroll * Step * 8.f);
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
    if (Pointer && !bTriggerHeld)
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
}
void AHouseMenuInputActor::TriggerAxis(float Value)
{
    if (Value > 0.55f) TriggerDown();
    else if (Value < 0.25f) TriggerUp();
}
void AHouseMenuInputActor::ToggleMenu()
{
    if (AHouseSandboxGameMode* Sandbox = Cast<AHouseSandboxGameMode>(UGameplayStatics::GetGameMode(this)))
        Sandbox->ToggleMenuVisibility();
}

void AHouseSandboxGameMode::BeginPlay()
{
    Super::BeginPlay();
    IFileManager::Get().MakeDirectory(*GetModelDirectory(), true);
    MenuActor = GetWorld()->SpawnActor<AActor>();
    MenuComponent = NewObject<UWidgetComponent>(MenuActor, TEXT("HouseLibrary"));
    MenuComponent->RegisterComponent();
    MenuActor->SetRootComponent(MenuComponent);
    MenuComponent->SetWidgetSpace(EWidgetSpace::World);
    MenuComponent->SetDrawSize(FVector2D(700, 850));
    MenuComponent->SetWorldScale3D(FVector(0.22));
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
    MenuActor->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    MenuComponent->SetRelativeLocation(FVector(120, 0, 0));
    MenuComponent->SetRelativeRotation(FRotator(0, 180, 0));
    MenuInput = GetWorld()->SpawnActor<AHouseMenuInputActor>();
    MenuInput->Initialize(Pawn);
    PreviewLight = NewObject<UPointLightComponent>(Pawn, TEXT("HousePreviewLight"));
    PreviewLight->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    PreviewLight->SetIntensityUnits(ELightUnits::Lumens);
    PreviewLight->SetIntensity(1200.f);
    PreviewLight->SetAttenuationRadius(1800.f);
    PreviewLight->SetCastShadows(false);
    PreviewLight->RegisterComponent();
    PreviewLight->SetVisibility(false);
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: menu attached to XR camera"));
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
    if (MenuComponent) MenuComponent->SetVisibility(!MenuComponent->IsVisible());
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
    Request->OnProcessRequestComplete().BindLambda([WeakThis, Base](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
    {
        if (!WeakThis.IsValid()) return;
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
    TWeakObjectPtr<AHouseSandboxGameMode> WeakThis(this);
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("GET"));
    Request->OnProcessRequestComplete().BindLambda([WeakThis, Name](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
    {
        if (!WeakThis.IsValid()) return;
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
    if (!HouseGlb::BuildMesh(Bytes, NewMesh, Min, Max, FloorZ, TotalTriangles, ImportError))
    {
        NewActor->Destroy();
        SetError(ImportError);
        return false;
    }
    NewActor->SetActorLocation(FVector(-(Min.X + Max.X) / 2, -(Min.Y + Max.Y) / 2, -FloorZ));
    UE_LOG(LogTemp, Display, TEXT("HouseSandbox: bounds %s to %s, walkable floor %.1f cm"), *Min.ToString(), *Max.ToString(), FloorZ);
    if (HouseActor) HouseActor->Destroy();
    HouseActor = NewActor;
    HouseMesh = NewMesh;
    HideTemplateGeometry();
    if (PreviewLight) PreviewLight->SetVisibility(true);
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        Pawn->SetActorLocation(FVector(0, 0, 0));
    }
    SetError(FString::Printf(TEXT("%s cargado: %lld triangulos"), *FileName, TotalTriangles));
    if (MenuComponent) MenuComponent->SetVisibility(false);
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

bool UHouseSandboxWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Backdrop->SetBrushColor(FLinearColor(0.025f, 0.05f, 0.09f, 0.98f));
    Backdrop->SetPadding(FMargin(22.f));
    WidgetTree->RootWidget = Backdrop;
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Backdrop->SetContent(Scroll);
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(Root);
    auto AddText = [&](const FString& Value) -> UTextBlock*
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Value));
        Root->AddChildToVerticalBox(Text);
        return Text;
    };
    AddText(TEXT("CASAS VR  |  BIBLIOTECA"));
    AddText(TEXT("HUB DE LA LAPTOP  |  SUBE DESDE EL NAVEGADOR"));
    HubBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    HubBox->SetText(FText::FromString(TEXT("http://127.0.0.1:8765")));
    HubBox->SetHintText(FText::FromString(TEXT("http://IP-LAPTOP:8765")));
    Root->AddChildToVerticalBox(HubBox);
    UButton* Sync = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* SyncLabel = WidgetTree->ConstructWidget<UTextBlock>();
    SyncLabel->SetText(FText::FromString(TEXT("VER MODELOS DE LA LAPTOP")));
    Sync->AddChild(SyncLabel);
    Sync->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnSyncHub);
    Root->AddChildToVerticalBox(Sync);
    RemoteList = WidgetTree->ConstructWidget<UVerticalBox>();
    Root->AddChildToVerticalBox(RemoteList);
    AddText(TEXT("ARCHIVO DEL VISOR"));
    UButton* Pick = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* PickLabel = WidgetTree->ConstructWidget<UTextBlock>();
    PickLabel->SetText(FText::FromString(TEXT("ABRIR ARCHIVO DEL DISPOSITIVO")));
    Pick->AddChild(PickLabel);
    Pick->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnPickLocal);
    Root->AddChildToVerticalBox(Pick);
    UButton* Refresh = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* RefreshLabel = WidgetTree->ConstructWidget<UTextBlock>();
    RefreshLabel->SetText(FText::FromString(TEXT("ACTUALIZAR ARCHIVOS LOCALES")));
    Refresh->AddChild(RefreshLabel);
    Refresh->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnRefresh);
    Root->AddChildToVerticalBox(Refresh);
    AddText(TEXT("ENLACE DIRECTO GLB  |  OPCIONAL"));
    UrlBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    UrlBox->SetHintText(FText::FromString(TEXT("http://IP-LAPTOP:8765/models/casa.glb")));
    Root->AddChildToVerticalBox(UrlBox);
    UButton* Download = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* DownloadLabel = WidgetTree->ConstructWidget<UTextBlock>();
    DownloadLabel->SetText(FText::FromString(TEXT("DESCARGAR Y ABRIR")));
    Download->AddChild(DownloadLabel);
    Download->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnDownload);
    Root->AddChildToVerticalBox(Download);
    UButton* Close = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>();
    CloseLabel->SetText(FText::FromString(TEXT("CERRAR PANEL  |  BOTON X PARA ABRIR")));
    Close->AddChild(CloseLabel);
    Close->OnClicked.AddDynamic(this, &UHouseSandboxWidget::OnClose);
    Root->AddChildToVerticalBox(Close);
    StatusText = AddText(TEXT("Listo"));
    AddText(TEXT("CASAS GUARDADAS EN EL VISOR"));
    FileList = WidgetTree->ConstructWidget<UVerticalBox>();
    Root->AddChildToVerticalBox(FileList);
    return true;
}

void UHouseSandboxWidget::Populate(AHouseSandboxGameMode* Owner)
{
    Sandbox = Owner;
    if (!FileList || !Sandbox) return;
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
        Button->AddChild(Label);
        Button->OnClicked.AddDynamic(Button, &UHouseFileButton::ActivateFile);
        FileList->AddChildToVerticalBox(Button);
    }
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
    RemoteList->ClearChildren();
    for (const TPair<FString, FString>& Model : Models)
    {
        UHouseRemoteButton* Button = WidgetTree->ConstructWidget<UHouseRemoteButton>();
        Button->ModelUrl = Model.Value;
        Button->ParentMenu = this;
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(TEXT("DESCARGAR  |  ") + Model.Key));
        Button->AddChild(Label);
        Button->OnClicked.AddDynamic(Button, &UHouseRemoteButton::ActivateRemote);
        RemoteList->AddChildToVerticalBox(Button);
    }
}
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

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"
#include "Components/Button.h"
#include "Blueprint/UserWidget.h"
#include "GlbRuntime.h"
#include "HouseSandbox.generated.h"

class UProceduralMeshComponent;
class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UScrollBox;
class UWidgetInteractionComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UInputMappingContext;
class ADirectionalLight;

enum class EHousePanel : uint8 { Closed, Library, Visual };

struct FHouseVisualSettings
{
    bool bLit = true;
    bool bNormal = true;
    bool bOcclusion = true;
    bool bMetalRough = true;
    bool bEmissive = true;
    bool bFillLight = true;
    bool bShadows = false;
};

UCLASS()
class QUEST3S_TEST_API AHouseMenuInputActor : public AActor
{
    GENERATED_BODY()
public:
    AHouseMenuInputActor();
    void Initialize(APawn* Pawn);
    void CancelPointerPress();
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<UWidgetInteractionComponent> Pointer;
    UPROPERTY() TObjectPtr<APawn> ControlledPawn;
    UPROPERTY() TObjectPtr<class UCameraComponent> PlayerCamera;
    bool bTriggerHeld = false;
    bool bSuppressTriggerUntilRelease = false;
    float ForwardInput = 0.f;
    float RightInput = 0.f;
    float TurnInput = 0.f;
    float ScrollInput = 0.f;
    bool bXHeld = false, bYHeld = false, bAHeld = false, bBHeld = false;
    bool bVisualChordFired = false, bReturnChordFired = false, bXUsedForChord = false;
    float XDownTime = 0.f, YDownTime = 0.f, ADownTime = 0.f, BDownTime = 0.f;
    void TriggerDown();
    void TriggerUp();
    void TriggerAxis(float Value);
    void MoveForwardAxis(float Value);
    void MoveRightAxis(float Value);
    void TurnAxis(float Value);
    void ScrollAxis(float Value);
    void ToggleMenu();
    void XDown();
    void XUp();
    void YDown();
    void YUp();
    void ADown();
    void AUp();
    void BDown();
    void BUp();
};

UCLASS()
class QUEST3S_TEST_API UHouseVisualButton : public UButton
{
    GENERATED_BODY()
public:
    int32 Feature = 0;
    UPROPERTY() TObjectPtr<class UHouseSandboxWidget> ParentMenu;
    UFUNCTION() void ActivateVisual();
};

UCLASS()
class QUEST3S_TEST_API UHouseFileButton : public UButton
{
    GENERATED_BODY()
public:
    FString FileName;
    UPROPERTY() TObjectPtr<class UHouseSandboxWidget> ParentMenu;
    UFUNCTION() void ActivateFile();
};

UCLASS()
class QUEST3S_TEST_API UHouseRemoteButton : public UButton
{
    GENERATED_BODY()
public:
    FString ModelUrl;
    UPROPERTY() TObjectPtr<class UHouseSandboxWidget> ParentMenu;
    UFUNCTION() void ActivateRemote();
};

UCLASS()
class QUEST3S_TEST_API AHouseSandboxGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AHouseSandboxGameMode();
    virtual void BeginPlay() override;
    UFUNCTION(BlueprintCallable, Category="House Sandbox") bool LoadHouse(const FString& FileName);
    UFUNCTION(BlueprintCallable, Category="House Sandbox") void DownloadHouse(const FString& Url);
    void FetchHubModels(const FString& HubAddress);
    FString GetHubAddress() const;
    UFUNCTION(BlueprintCallable, Category="House Sandbox") void OpenAndroidFilePicker();
    UFUNCTION(BlueprintCallable, Category="House Sandbox") TArray<FString> GetLocalHouses() const;
    UFUNCTION(BlueprintCallable, Category="House Sandbox") FString GetModelDirectory() const;
    UFUNCTION(BlueprintCallable, Category="House Sandbox") FString GetLastError() const { return LastError; }
    UFUNCTION(BlueprintCallable, Category="House Sandbox") void RefreshMenu();
    void ToggleMenuVisibility();
    bool IsMenuVisible() const;
    EHousePanel GetActivePanel() const { return ActivePanel; }
    void ShowPanel(EHousePanel Panel);
    void ReturnToLibrary();
    void ChangeVisualControl(int32 Feature);
    void ScrollPanel(float Amount);
    const FHouseVisualSettings& GetVisualSettings() const { return VisualSettings; }
    const FHouseMaterialFeatures& GetMaterialFeatures() const { return MaterialFeatures; }
private:
    UPROPERTY() TObjectPtr<AActor> HouseActor;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> HouseMesh;
    UPROPERTY() TObjectPtr<UWidgetComponent> MenuComponent;
    UPROPERTY() TObjectPtr<AActor> MenuActor;
    UPROPERTY() TObjectPtr<class UHouseSandboxWidget> MenuWidget;
    UPROPERTY() TObjectPtr<AHouseMenuInputActor> MenuInput;
    UPROPERTY() TObjectPtr<class UPointLightComponent> PreviewLight;
    UPROPERTY() TObjectPtr<ADirectionalLight> SceneSun;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> LitMaterials;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ReferenceMaterials;
    // Hard references keep the runtime-loaded parents in the Android cook.
    UPROPERTY() TObjectPtr<UMaterialInterface> CookedLitOpaque;
    UPROPERTY() TObjectPtr<UMaterialInterface> CookedLitMasked;
    UPROPERTY() TObjectPtr<UMaterialInterface> CookedLitBlend;
    UPROPERTY() TObjectPtr<UInputMappingContext> HouseButtonContext;
    FHouseMaterialFeatures MaterialFeatures;
    FHouseVisualSettings VisualSettings;
    EHousePanel ActivePanel = EHousePanel::Library;
    int32 RequestGeneration = 0;
    FString LastError;
    void SetError(const FString& Message);
    void PlaceMenuAtPlayer();
    void HideTemplateGeometry();
    void ApplyVisualSettings(bool bSave = true);
    void LoadVisualSettings();
    void SaveVisualSettings() const;
};

UCLASS()
class QUEST3S_TEST_API UHouseSandboxWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual bool Initialize() override;
    void Populate(AHouseSandboxGameMode* Owner);
    void OpenFile(const FString& FileName);
    void OpenRemote(const FString& Url);
    void SetRemoteModels(const TArray<TPair<FString, FString>>& Models);
    void SetVisualMode(bool bVisual);
    void UpdateVisualLabels();
    void ActivateVisualControl(int32 Feature);
    void ScrollBy(float Amount);
private:
    UPROPERTY() TObjectPtr<UEditableTextBox> UrlBox;
    UPROPERTY() TObjectPtr<UEditableTextBox> HubBox;
    UPROPERTY() TObjectPtr<UTextBlock> StatusText;
    UPROPERTY() TObjectPtr<UVerticalBox> FileList;
    UPROPERTY() TObjectPtr<UVerticalBox> RemoteList;
    UPROPERTY() TObjectPtr<UVerticalBox> LibraryPanel;
    UPROPERTY() TObjectPtr<UVerticalBox> VisualPanel;
    UPROPERTY() TObjectPtr<UScrollBox> ContentScroll;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> VisualLabels;
    UPROPERTY() TArray<TObjectPtr<UHouseVisualButton>> VisualButtons;
    float LibraryScrollOffset = 0.f;
    float VisualScrollOffset = 0.f;
    bool bShowingVisual = false;
    UPROPERTY() TObjectPtr<AHouseSandboxGameMode> Sandbox;
    bool bHubAddressLoaded = false;
    UFUNCTION() void OnDownload();
    UFUNCTION() void OnSyncHub();
    UFUNCTION() void OnPickLocal();
    UFUNCTION() void OnRefresh();
    UFUNCTION() void OnClose();
    UFUNCTION() void OnShowLibrary();
    UFUNCTION() void OnShowVisual();
    UFUNCTION() void OnRecenter();
};

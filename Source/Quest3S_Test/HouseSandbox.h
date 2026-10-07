#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"
#include "Components/Button.h"
#include "Blueprint/UserWidget.h"
#include "HouseSandbox.generated.h"

class UProceduralMeshComponent;
class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UWidgetInteractionComponent;

UCLASS()
class QUEST3S_TEST_API AHouseMenuInputActor : public AActor
{
    GENERATED_BODY()
public:
    AHouseMenuInputActor();
    void Initialize(APawn* Pawn);
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<UWidgetInteractionComponent> Pointer;
    UPROPERTY() TObjectPtr<APawn> ControlledPawn;
    UPROPERTY() TObjectPtr<class UCameraComponent> PlayerCamera;
    bool bTriggerHeld = false;
    float ForwardInput = 0.f;
    float RightInput = 0.f;
    float TurnInput = 0.f;
    float ScrollInput = 0.f;
    void TriggerDown();
    void TriggerUp();
    void TriggerAxis(float Value);
    void MoveForwardAxis(float Value);
    void MoveRightAxis(float Value);
    void TurnAxis(float Value);
    void ScrollAxis(float Value);
    void ToggleMenu();
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
private:
    UPROPERTY() TObjectPtr<AActor> HouseActor;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> HouseMesh;
    UPROPERTY() TObjectPtr<UWidgetComponent> MenuComponent;
    UPROPERTY() TObjectPtr<AActor> MenuActor;
    UPROPERTY() TObjectPtr<class UHouseSandboxWidget> MenuWidget;
    UPROPERTY() TObjectPtr<AHouseMenuInputActor> MenuInput;
    UPROPERTY() TObjectPtr<class UPointLightComponent> PreviewLight;
    FString LastError;
    void SetError(const FString& Message);
    void PlaceMenuAtPlayer();
    void HideTemplateGeometry();
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
private:
    UPROPERTY() TObjectPtr<UEditableTextBox> UrlBox;
    UPROPERTY() TObjectPtr<UEditableTextBox> HubBox;
    UPROPERTY() TObjectPtr<UTextBlock> StatusText;
    UPROPERTY() TObjectPtr<UVerticalBox> FileList;
    UPROPERTY() TObjectPtr<UVerticalBox> RemoteList;
    UPROPERTY() TObjectPtr<AHouseSandboxGameMode> Sandbox;
    bool bHubAddressLoaded = false;
    UFUNCTION() void OnDownload();
    UFUNCTION() void OnSyncHub();
    UFUNCTION() void OnPickLocal();
    UFUNCTION() void OnRefresh();
    UFUNCTION() void OnClose();
};

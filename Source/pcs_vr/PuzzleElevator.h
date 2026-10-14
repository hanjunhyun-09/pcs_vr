#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleElevator.generated.h"

class UPointLightComponent;
class UPuzzleTextWidget;
class UStaticMesh;
class UStaticMeshComponent;
class UWidgetComponent;

/**
 * 스테이지 사이를 잇는 엘리베이터 (노션: 내부 2m x 2m, 모든 엘리베이터 같은 크기).
 * 액터 위치 = 엘리베이터 바닥 가운데, 앞(+X) = 문 쪽.
 * - 처음엔 문이 닫혀 있고 호출 버튼을 누르면 "대기 중…"
 * - Arrive() → 문이 열림
 * - 플레이어가 안에 들어가 있으면 문이 닫히고, 화면이 어두워진 뒤 NextLevel 을 연다
 */
UCLASS()
class PCS_VR_API APuzzleElevator : public AActor
{
	GENERATED_BODY()

public:
	APuzzleElevator();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator")
	FName NextLevel = TEXT("/Game/VRPuzzle/Maps/L_02_Perspective");

	/** 안에 들어간 뒤 문이 닫히기까지 기다리는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator")
	float DepartDelay = 1.5f;

	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FPuzzleSimpleEvent OnArrived;

	UFUNCTION(BlueprintCallable, Category = "Elevator")
	void Arrive();

	UFUNCTION(BlueprintPure, Category = "Elevator")
	bool HasArrived() const { return State != EState::Waiting; }

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	enum class EState : uint8 { Waiting, Opening, Open, Closing, Leaving };

	UFUNCTION()
	void HandleCall(AActor* InteractInstigator);

	void SetDisplay(const FString& Message, const FLinearColor& Color);
	bool IsPlayerInside() const;
	void OpenNextLevel();
	UStaticMeshComponent* AddBox(const TCHAR* Name, const FVector& Center, const FVector& SizeCm, UStaticMesh* Mesh);

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TArray<TObjectPtr<UStaticMeshComponent>> Shell;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UStaticMeshComponent> DoorLeft;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UStaticMeshComponent> DoorRight;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UStaticMeshComponent> CallButton;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UPuzzleInteractComponent> CallInteract;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UWidgetComponent> Display;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UPointLightComponent> CabinLight;

	EState State = EState::Waiting;
	float DoorAlpha = 0.f;
	float InsideTime = 0.f;
	FTimerHandle LevelHandle;
};

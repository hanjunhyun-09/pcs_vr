#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleDispenser.generated.h"

class APerspectiveCube;
class UMaterialInstanceDynamic;
class UPuzzleInteractComponent;
class UStaticMeshComponent;

/**
 * 큐브 디스펜서 (노션 원근법 규칙: 리셋). 받침대 + 앞면 버튼.
 * 버튼을 누르면 전에 나온 물체가 사라지고 받침대 위에 새로 나온다.
 * 액터 위치 = 받침대 바닥 가운데, 앞(+X) = 버튼 쪽.
 */
UCLASS()
class PCS_VR_API APuzzleDispenser : public AActor
{
	GENERATED_BODY()

public:
	APuzzleDispenser();

	/** 내보낼 원근 물체 (보통 BP_PerspectiveCube) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispenser")
	TSubclassOf<APerspectiveCube> SpawnClass;

	/** 나오는 물체의 한 변 (cm). 이 크기가 기준 크기(배율 1.0)가 된다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispenser", meta = (ClampMin = "5.0"))
	float SpawnSize = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispenser")
	FLinearColor SpawnColor = FLinearColor(0.1f, 0.35f, 0.9f);

	/** 0보다 크면 나오는 물체의 MaxScale을 이 값으로 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispenser")
	float SpawnMaxScale = 0.f;

	/** 나오는 물체에 붙일 Tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispenser")
	TArray<FName> SpawnTags;

	/** 게임 시작 때 하나 내보낼지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispenser")
	bool bSpawnOnStart = true;

	/** 이 물체가 있으면 버튼을 눌렀을 때 그것도 사라지고 새로 나온다 (2-3: 줄여 온 상자를 잃어버린 경우) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispenser")
	TObjectPtr<APerspectiveCube> ReplaceActor;

	UFUNCTION(BlueprintCallable, Category = "Dispenser")
	APerspectiveCube* Dispense();

	UFUNCTION(BlueprintPure, Category = "Dispenser")
	APerspectiveCube* GetLastSpawned() const { return LastSpawned; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleInteract(AActor* InteractInstigator);

	UPROPERTY(VisibleAnywhere, Category = "Dispenser")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Dispenser")
	TObjectPtr<UStaticMeshComponent> Pedestal;

	UPROPERTY(VisibleAnywhere, Category = "Dispenser")
	TObjectPtr<UStaticMeshComponent> Tray;

	UPROPERTY(VisibleAnywhere, Category = "Dispenser")
	TObjectPtr<UStaticMeshComponent> Button;

	UPROPERTY(VisibleAnywhere, Category = "Dispenser")
	TObjectPtr<UPuzzleInteractComponent> Interact;

	UPROPERTY(Transient)
	TObjectPtr<APerspectiveCube> LastSpawned;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ButtonMID;
};

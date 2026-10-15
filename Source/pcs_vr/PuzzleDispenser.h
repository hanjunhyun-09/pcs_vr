#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "PuzzleDispenser.generated.h"

class UArrowComponent;
class UStaticMeshComponent;

/**
 * 큐브 디스펜서 (노션 BP_Dispenser, "막힘 방지").
 * 상호작용하면 이전에 나온 물체를 없애고 SpawnPoint 위치에 새 물체를 하나 만든다.
 * 퍼즐 물체를 잃어버리거나 너무 크게/작게 만들어 버려도 다시 받을 수 있다.
 *
 * 사용법: 블루프린트로 상속(BP_Dispenser) → Body에 메시 지정 → SpawnClass에 BP_PerspectiveObject 지정
 */
UCLASS()
class PCS_VR_API APuzzleDispenser : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APuzzleDispenser();

	/** 만들어 낼 물체 (예: BP_PerspectiveObject) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispenser")
	TSubclassOf<AActor> SpawnClass;

	/** 게임 시작 때 하나 만들어 두기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispenser")
	bool bSpawnOnBeginPlay = true;

	/** 이전에 만든 물체를 없앤 뒤 새로 하나 만든다 */
	UFUNCTION(BlueprintCallable, Category = "Dispenser")
	AActor* Dispense();

	UFUNCTION(BlueprintPure, Category = "Dispenser")
	AActor* GetCurrentItem() const { return CurrentItem; }

	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dispenser")
	TObjectPtr<USceneComponent> Root;

	/** 디스펜서 몸체 (버튼 역할도 함. 메시는 에디터에서 지정) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dispenser")
	TObjectPtr<UStaticMeshComponent> Body;

	/** 물체가 나오는 위치 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dispenser")
	TObjectPtr<UArrowComponent> SpawnPoint;

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> CurrentItem;
};

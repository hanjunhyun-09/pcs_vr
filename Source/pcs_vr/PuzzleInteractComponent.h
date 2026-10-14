#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PuzzleInteractComponent.generated.h"

/** 퍼즐 부품들이 같이 쓰는 "무언가 일어났다" 이벤트 (레버 내려감, 카드 승인, 발판 도착 등) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPuzzleSimpleEvent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPuzzleInteractEvent, AActor*, InteractInstigator);

/**
 * 트리거로 누를 수 있는 지점 (노션 조작 약속: 상호작용 = 트리거, 손이 닿은 상태에서).
 * 이 컴포넌트의 위치에서 InteractRadius 안에 손이 있을 때 트리거를 당기면 OnInteract가 불린다.
 * 데스크톱 테스트에서는 E 키로 바라보는 대상을 누른다. (UVRInteractionSubsystem 참고)
 */
UCLASS(ClassGroup = (Puzzle), meta = (BlueprintSpawnableComponent))
class PCS_VR_API UPuzzleInteractComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	/** 손이 이 거리(cm) 안에 있어야 누를 수 있다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = "1.0"))
	float InteractRadius = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	bool bInteractEnabled = true;

	UPROPERTY(BlueprintAssignable, Category = "Interact")
	FPuzzleInteractEvent OnInteract;

	UFUNCTION(BlueprintCallable, Category = "Interact")
	void Interact(AActor* InteractInstigator);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};

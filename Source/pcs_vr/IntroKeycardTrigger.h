#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interactable.h"
#include "IntroKeycardTrigger.generated.h"

/**
 * 인트로 출입카드에 붙이는 컴포넌트.
 * 같은 액터의 VR 템플릿 GrabComponent가 "잡힘(bIsHeld)" 상태가 되면
 * 화면을 페이드 아웃한 뒤 다음 레벨을 연다.
 * 데스크톱 테스트 모드에서는 카드를 클릭하거나 E 키로 상호작용하면 된다 (Interactable).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PCS_VR_API UIntroKeycardTrigger : public UActorComponent, public IInteractable
{
	GENERATED_BODY()

public:
	UIntroKeycardTrigger();

	/** 다음에 열 레벨 (예: /Game/VRPuzzle/Maps/L_01_Isolation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro")
	FName NextLevelName = TEXT("/Game/VRPuzzle/Maps/L_01_Isolation");

	/** 화면이 검게 변하는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (ClampMin = "0.0"))
	float FadeTime = 1.0f;

	/** 잡힘 상태를 확인하는 간격(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (ClampMin = "0.01"))
	float PollInterval = 0.05f;

	/** 잡지 않고도 바로 다음 레벨로 보내기 (데스크톱 테스트용) */
	UFUNCTION(BlueprintCallable, Category = "Intro")
	void TriggerNow();

	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CheckGrabbed();
	void OpenNextLevel();
	bool IsGrabComponentHeld() const;

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> GrabComp;

	FTimerHandle PollHandle;
	FTimerHandle LevelHandle;
	bool bTriggered = false;
};

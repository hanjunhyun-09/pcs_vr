#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleCardReader.generated.h"

class UMaterialInstanceDynamic;
class UPuzzleTextWidget;
class UStaticMeshComponent;
class UWidgetComponent;

/**
 * 카드 리더기. 카드(액터 Tag가 Keycard로 시작하는 PuzzleGrabbable)를 앞에 대면
 * AcceptTag와 같으면 승인(초록불 + OnAccepted), 다르면 거부 메시지.
 * 전원이 꺼져 있으면(bPowered = false) 아무 반응이 없다.
 * 액터의 앞(+X)이 벽 바깥(플레이어 쪽)을 보게 놓는다.
 */
UCLASS()
class PCS_VR_API APuzzleCardReader : public AActor
{
	GENERATED_BODY()

public:
	APuzzleCardReader();

	UPROPERTY(BlueprintAssignable, Category = "CardReader")
	FPuzzleSimpleEvent OnAccepted;

	/** 다른 카드를 대서 거부됐을 때 */
	UPROPERTY(BlueprintAssignable, Category = "CardReader")
	FPuzzleSimpleEvent OnDenied;

	/** 이 Tag를 가진 카드만 승인 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardReader")
	FName AcceptTag = TEXT("KeycardPlayer");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardReader")
	bool bPowered = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardReader")
	FText AcceptedMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardReader")
	FText DeniedMessage;

	/** 카드를 인식하는 거리 (리더기 앞면에서 cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardReader")
	float DetectRadius = 20.f;

	UFUNCTION(BlueprintCallable, Category = "CardReader")
	void SetPowered(bool bNewPowered);

	UFUNCTION(BlueprintPure, Category = "CardReader")
	bool IsAccepted() const { return bAccepted; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CheckCards();
	void ShowMessage(const FText& Message, const FLinearColor& Color, float Duration);
	void HideMessage();
	void UpdateLed();

	UPROPERTY(VisibleAnywhere, Category = "CardReader")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "CardReader")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "CardReader")
	TObjectPtr<UStaticMeshComponent> Led;

	UPROPERTY(VisibleAnywhere, Category = "CardReader")
	TObjectPtr<UWidgetComponent> Screen;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LedMID;

	FTimerHandle CheckHandle;
	FTimerHandle MessageHandle;
	TSet<TWeakObjectPtr<AActor>> CardsInRange;
	bool bAccepted = false;
	double LastNearLogTime = 0.0;
};

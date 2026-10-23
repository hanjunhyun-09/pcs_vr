#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleButton.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * 벽 버튼. 손을 대고 트리거 → 눌리고 OnPressed.
 * bHiddenAtStart면 처음엔 벽 안에 숨어 있다가 PopOut()으로 튀어나온다 (봉쇄 해제 버튼).
 * 액터의 앞(+X)이 벽 바깥(플레이어 쪽)을 보게 놓는다.
 */
UCLASS()
class PCS_VR_API APuzzleButton : public AActor
{
	GENERATED_BODY()

public:
	APuzzleButton();

	UPROPERTY(BlueprintAssignable, Category = "Button")
	FPuzzleSimpleEvent OnPressed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bHiddenAtStart = false;

	/** true면 한 번 누른 뒤에는 더 이상 눌리지 않는다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bOneShot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FLinearColor CapColor = FLinearColor(0.9f, 0.6f, 0.05f);

	UFUNCTION(BlueprintCallable, Category = "Button")
	void PopOut();

	UFUNCTION(BlueprintCallable, Category = "Button")
	void Press();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleInteract(AActor* InteractInstigator);

	void StartMove(float ToX, float Time);

	UPROPERTY(VisibleAnywhere, Category = "Button")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Button")
	TObjectPtr<UStaticMeshComponent> Plate;

	UPROPERTY(VisibleAnywhere, Category = "Button")
	TObjectPtr<UStaticMeshComponent> Cap;

	UPROPERTY(VisibleAnywhere, Category = "Button")
	TObjectPtr<UPuzzleInteractComponent> Interact;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CapMID;

	float MoveFromX = 0.f;
	float MoveToX = 0.f;
	float MoveTime = 0.f;
	float MoveAlpha = 1.f;
	bool bPressedOnce = false;
	bool bReturnAfterPress = false;

	static constexpr float HiddenX = -3.f;
	static constexpr float OutX = 4.f;
	static constexpr float PressedX = 1.5f;
};

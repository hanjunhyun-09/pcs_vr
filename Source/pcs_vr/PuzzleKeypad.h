#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleKeypad.generated.h"

class APuzzleKeypad;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWidgetComponent;

/** 키패드 버튼 하나: 데스크톱 E 키로 바라보고 누르면 자기 글자를 키패드에 전한다 (VR 손은 APuzzleKeypad가 직접 본다) */
UCLASS(ClassGroup = (Puzzle))
class PCS_VR_API UPuzzleKeypadKey : public UPuzzleInteractComponent
{
	GENERATED_BODY()

public:
	/** "0" ~ "9", "C"(지우기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad")
	FString Symbol;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleSelf(AActor* InteractInstigator);
};

/**
 * 숫자 4자리 키패드 (노션 3-1: 셔터 옆, 비밀번호 1114).
 * VR: 손끝이 올라간 버튼이 밝게 표시되고, 손끝으로 밀어 넣거나 그 상태에서 트리거를 당기면 눌린다.
 *     (손 위치는 컨트롤러 가운데라서, 앞쪽 TipOffset만큼 나간 손끝으로 버튼을 고른다)
 * 숫자를 Code 길이만큼 누르면 바로 확인: 맞으면 "승인" + OnSolved, 틀리면 "오류"가 잠깐 뜨고 지워진다 (벌 없음).
 * 액터의 앞(+X)이 벽 바깥(플레이어 쪽)을 보게 놓는다. 액터 위치 = 판 가운데.
 */
UCLASS()
class PCS_VR_API APuzzleKeypad : public AActor
{
	GENERATED_BODY()

public:
	APuzzleKeypad();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad")
	FString Code = TEXT("1114");

	/** 손(그립) 위치에서 컨트롤러 앞쪽으로 손끝까지 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad")
	float TipOffset = 7.f;

	UPROPERTY(BlueprintAssignable, Category = "Keypad")
	FPuzzleSimpleEvent OnSolved;

	/** 틀린 번호를 넣을 때마다 */
	UPROPERTY(BlueprintAssignable, Category = "Keypad")
	FPuzzleSimpleEvent OnWrongCode;

	UFUNCTION(BlueprintPure, Category = "Keypad")
	bool IsSolved() const { return bSolved; }

	UFUNCTION(BlueprintCallable, Category = "Keypad")
	void PressKey(const FString& Symbol);

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	/** 한 손의 상태: 지금 손끝이 올라간 버튼, 밀어 넣었는지, 트리거를 당기고 있었는지 */
	struct FHandState
	{
		int32 Hovered = INDEX_NONE;
		bool bPushed = false;
		bool bTriggerWasDown = false;
	};

	void UpdateHand(FHandState& State, const TCHAR* GripName, bool bLeft);
	/** 키패드 로컬 좌표의 손끝 → 버튼 번호 (없으면 INDEX_NONE) */
	int32 FindKeyAt(const FVector& LocalTip) const;
	void UpdateDisplay();
	void ClearInput();
	void RefreshKeyColors();

	UPROPERTY(VisibleAnywhere, Category = "Keypad")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Keypad")
	TObjectPtr<UStaticMeshComponent> Panel;

	UPROPERTY(VisibleAnywhere, Category = "Keypad")
	TObjectPtr<UWidgetComponent> Display;

	UPROPERTY(VisibleAnywhere, Category = "Keypad")
	TArray<TObjectPtr<UStaticMeshComponent>> KeyMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Keypad")
	TArray<TObjectPtr<UTextRenderComponent>> KeyLabels;

	UPROPERTY(VisibleAnywhere, Category = "Keypad")
	TArray<TObjectPtr<UPuzzleKeypadKey>> Keys;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> KeyMIDs;

	FHandState LeftHand;
	FHandState RightHand;
	int32 FlashIndex = INDEX_NONE;
	double FlashUntil = 0.0;

	FString Input;
	bool bSolved = false;
	bool bShowingError = false;
	FTimerHandle ErrorHandle;
};

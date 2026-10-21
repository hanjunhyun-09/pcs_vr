#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleHandDevice.generated.h"

class APuzzleToggleTarget;
class UMaterialInstanceDynamic;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHandDeviceSymbolEvent, int32, SymbolIndex);

/**
 * 양손을 넣고 들고 다니는 피험 장치 상자 + 책상 위 받침대 (노션 "4스테이지 퍼즐 설계", Statik 참고).
 * Statik처럼 두 손이 상자 하나에 갇혀 손이 보이지 않고, 눈앞의 상자에 달린 다이얼·버튼·창만 보면서 규칙을 알아낸다.
 * 액터 위치 = 받침대 바닥 가운데, 앞(+X) = 플레이어가 서는 쪽.
 *
 * 장치 상자 (로컬 +X = 플레이어에게서 먼 쪽, +Z = 위, +Y = 오른손 쪽):
 *  - 뒷면(플레이어 쪽)에 손을 넣는 고무 소매 두 개
 *  - 윗면 왼쪽: 다이얼 (돌아가는 원판 + 둘레의 기호 ● ▲ ■ ◆), 가운데: 기호 창, 오른쪽: 큰 빨간 버튼
 *  - 윗면 뒤쪽(플레이어 쪽) 가장자리: 진행 램프 4개
 *
 * - 장착: VR은 두 손을 받침대 위 상자의 소매에 넣으면, 데스크톱은 받침대를 보고 E 키
 *   → 상자가 두 손 사이에 떠서 손을 따라다니고, 손 모델은 숨는다 (상자 안에 있으니까)
 * - 왼손 트리거(데스크톱 Q) = 다이얼 한 칸, 오른손 트리거(데스크톱 E) = 버튼 → Targets[다이얼]을 켜고 끔
 * - 마지막 입력 4번이 Solution과 같으면 장치가 풀려 받침대로 돌아가고 각인이 보인다
 * 장착 중에는 트리거가 이 장치로만 가고, 원근 큐브 레이 잡기도 막힌다 (UVRInteractionSubsystem).
 * 5스테이지 장치 구역에서는 Targets를 제어판 램프로, Solution만 바꿔서 그대로 쓴다.
 */
UCLASS()
class PCS_VR_API APuzzleHandDevice : public AActor
{
	GENERATED_BODY()

public:
	APuzzleHandDevice();

	/** 다이얼 순서대로 (0 ● / 1 ▲ / 2 ■ / 3 ◆) 버튼이 켜고 끄는 장비 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	TArray<TObjectPtr<APuzzleToggleTarget>> Targets;

	/** 풀리는 순서 (다이얼 번호). 노션 4스테이지: ▲ ● ◆ ■ = [1, 0, 3, 2] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	TArray<int32> Solution = { 1, 0, 3, 2 };

	/** 맞는 순서가 이어지면 진행 램프가 하나씩 켜진다 (노션: 선택, 난이도 낮추기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	bool bShowProgress = true;

	/** 상자 윗면 이름판 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	FText Nameplate;

	/** 풀린 뒤 받침대에 보이는 각인 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice", meta = (MultiLine = "true"))
	FText Engraving;

	/** 손(Grip)이 소매 입구에서 이 거리(cm) 안에 들어오면 손을 넣은 것으로 본다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	float SleeveRadius = 16.f;

	/** VR: 두 손 가운데 기준 상자 위치 (상자 좌표). 손이 상자 안쪽에 오게 앞으로 조금 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	FVector VRCenterOffset = FVector(4.f, 0.f, 2.f);

	/** 데스크톱: 카메라 기준 상자 위치와 기울기 (윗면이 보이게 앞쪽을 들어 올림) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	FVector DesktopOffset = FVector(42.f, 0.f, -24.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	FRotator DesktopRotation = FRotator(40.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice|Sound")
	TSoftObjectPtr<USoundBase> LockSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice|Sound")
	TSoftObjectPtr<USoundBase> DialSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice|Sound")
	TSoftObjectPtr<USoundBase> ButtonSound;

	UPROPERTY(BlueprintAssignable, Category = "HandDevice")
	FPuzzleSimpleEvent OnEquipped;

	UPROPERTY(BlueprintAssignable, Category = "HandDevice")
	FHandDeviceSymbolEvent OnDialChanged;

	/** 버튼을 눌렀을 때 (다이얼이 가리키던 기호) */
	UPROPERTY(BlueprintAssignable, Category = "HandDevice")
	FHandDeviceSymbolEvent OnPressed;

	UPROPERTY(BlueprintAssignable, Category = "HandDevice")
	FPuzzleSimpleEvent OnUnlocked;

	UFUNCTION(BlueprintCallable, Category = "HandDevice")
	void Equip();

	/** 왼손: 다이얼 한 칸 */
	UFUNCTION(BlueprintCallable, Category = "HandDevice")
	void PressLeft();

	/** 오른손: 버튼 → 지금 다이얼의 장비를 켜고 끔 + 입력 기록 */
	UFUNCTION(BlueprintCallable, Category = "HandDevice")
	void PressRight();

	UFUNCTION(BlueprintPure, Category = "HandDevice")
	bool IsEquipped() const { return bEquipped; }

	UFUNCTION(BlueprintPure, Category = "HandDevice")
	bool IsUnlocked() const { return bUnlocked; }

	UFUNCTION(BlueprintPure, Category = "HandDevice")
	int32 GetDialIndex() const { return DialIndex; }

	/** 0 ● / 1 ▲ / 2 ■ / 3 ◆ */
	static FString SymbolOf(int32 Index);

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleRackInteract(AActor* InteractInstigator);

	void Unlock();
	void FollowHands(float DeltaTime);
	void ReturnToRack();
	void SetHandsHidden(bool bHide);
	void Buzz(bool bLeft, float Amplitude);
	void PlaySound(const TSoftObjectPtr<USoundBase>& Sound, float Volume = 1.f);
	bool AreHandsInSleeves() const;
	int32 CountProgress() const;
	void UpdateDisplays();

	UStaticMeshComponent* AddPart(const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Center, const FVector& SizeCm,
		const FRotator& Rotation = FRotator::ZeroRotator);
	UWidgetComponent* AddDisplay(const TCHAR* Name, USceneComponent* Parent, const FVector& Location, const FVector2D& DrawSize, float Scale);

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UPuzzleInteractComponent> RackInteract;

	/** 들고 다니는 상자 전체 (장착하면 월드 좌표로 손을 따라감) */
	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<USceneComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<USceneComponent> DialPivot;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UStaticMeshComponent> ButtonCap;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TArray<TObjectPtr<UStaticMeshComponent>> ProgressLamps;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UWidgetComponent> SymbolWindow;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UWidgetComponent> NameplateDisplay;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TArray<TObjectPtr<UWidgetComponent>> DialLabels;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UWidgetComponent> EngravingDisplay;

	/** 색을 칠할 부품들 (BeginPlay에서 이름별로 색을 정함) */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LampMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ButtonMID;

	TArray<TWeakObjectPtr<USceneComponent>> HiddenHands;
	FTransform BoxRestTM;
	FTransform SmoothedTM;
	TArray<int32> History;
	int32 DialIndex = 0;
	float DialAngle = 0.f;
	float ButtonPress = 0.f;
	float LampFlash = 0.f;
	bool bHasSmoothed = false;
	bool bEquipped = false;
	bool bUnlocked = false;
};

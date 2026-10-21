#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleHandDevice.generated.h"

class APuzzleToggleTarget;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHandDeviceSymbolEvent, int32, SymbolIndex);

/**
 * 손에 붙는 장치 + 책상 위 거치대 (노션 "4스테이지 퍼즐 설계", Statik 참고).
 * 액터 위치 = 거치대 바닥 가운데, 앞(+X) = 플레이어가 서는 쪽. 거치대 두 칸 중 플레이어 왼쪽(+Y)이 왼손 칸.
 * - 장착: VR은 두 손을 거치대 칸에 넣으면, 데스크톱은 거치대를 보고 E 키 → 장치가 손(데스크톱은 카메라)에 붙는다
 * - 왼손 트리거(데스크톱 Q) = 다이얼 한 칸 (● ▲ ■ ◆), 오른손 트리거(데스크톱 E) = Targets[다이얼]을 켜고 끔
 * - 마지막 입력 4번이 Solution과 같으면 장치가 풀려 거치대로 돌아가고 안쪽 각인이 보인다
 * 장착 중에는 트리거가 이 장치로만 가고, 원근 큐브 레이 잡기도 막힌다 (UVRInteractionSubsystem).
 * 5스테이지 장치 구역에서는 Targets를 제어판 램프로, Solution만 바꿔서 그대로 쓴다.
 */
UCLASS()
class PCS_VR_API APuzzleHandDevice : public AActor
{
	GENERATED_BODY()

public:
	APuzzleHandDevice();

	/** 다이얼 순서대로 (0 ● / 1 ▲ / 2 ■ / 3 ◆) 오른손 버튼이 켜고 끄는 장비 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	TArray<TObjectPtr<APuzzleToggleTarget>> Targets;

	/** 풀리는 순서 (다이얼 번호). 노션 4스테이지: ▲ ● ◆ ■ = [1, 0, 3, 2] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	TArray<int32> Solution = { 1, 0, 3, 2 };

	/** 맞는 순서가 이어지면 오른손 표시창에 점이 하나씩 찬다 (노션: 선택, 난이도 낮추기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	bool bShowProgress = true;

	/** 풀린 뒤 보이는 장치 안쪽 각인 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice", meta = (MultiLine = "true"))
	FText Engraving;

	/** 손(Grip)이 거치대 칸 가운데에서 이 거리(cm) 안에 들어오면 칸에 넣은 것으로 본다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	float CradleRadius = 20.f;

	/** VR: 손(Grip) 기준 장치 위치. 손목 위에 오게 뒤로 조금 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	FVector VRHandOffset = FVector(-9.f, 0.f, 3.f);

	/** 데스크톱: 카메라 기준 장치 위치 (왼손. 오른손은 Y만 반대) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandDevice")
	FVector DesktopOffset = FVector(40.f, -15.f, -14.f);

	UPROPERTY(BlueprintAssignable, Category = "HandDevice")
	FPuzzleSimpleEvent OnEquipped;

	UPROPERTY(BlueprintAssignable, Category = "HandDevice")
	FHandDeviceSymbolEvent OnDialChanged;

	/** 오른손 버튼을 눌렀을 때 (다이얼이 가리키던 기호) */
	UPROPERTY(BlueprintAssignable, Category = "HandDevice")
	FHandDeviceSymbolEvent OnPressed;

	UPROPERTY(BlueprintAssignable, Category = "HandDevice")
	FPuzzleSimpleEvent OnUnlocked;

	UFUNCTION(BlueprintCallable, Category = "HandDevice")
	void Equip();

	/** 왼손: 다이얼 한 칸 */
	UFUNCTION(BlueprintCallable, Category = "HandDevice")
	void PressLeft();

	/** 오른손: 지금 다이얼의 장비를 켜고 끔 + 입력 기록 */
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
	void AttachDevices();
	void ReturnToRack();
	bool AreHandsInCradles() const;
	int32 CountProgress() const;
	void UpdateDisplays();
	UStaticMeshComponent* AddBox(const TCHAR* Name, USceneComponent* Parent, const FVector& Center, const FVector& SizeCm);
	UWidgetComponent* AddDisplay(const TCHAR* Name, USceneComponent* Parent, const FVector& Location, const FVector2D& DrawSize, float Scale);

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TArray<TObjectPtr<UStaticMeshComponent>> RackParts;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UPuzzleInteractComponent> RackInteract;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<USceneComponent> LeftDevice;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<USceneComponent> RightDevice;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TArray<TObjectPtr<UStaticMeshComponent>> DeviceParts;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UStaticMeshComponent> RightButton;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UWidgetComponent> DialDisplay;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UWidgetComponent> ProgressDisplay;

	UPROPERTY(VisibleAnywhere, Category = "HandDevice")
	TObjectPtr<UWidgetComponent> EngravingDisplay;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ButtonMID;

	FTransform LeftRestTM;
	FTransform RightRestTM;
	TArray<int32> History;
	int32 DialIndex = 0;
	float ButtonFlash = 0.f;
	bool bEquipped = false;
	bool bUnlocked = false;
};

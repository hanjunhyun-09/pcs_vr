#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "PerspectiveObjectComponent.generated.h"

class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPerspectiveHoldStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerspectiveReleased, float, ScaleRatio);

/**
 * 원근법 물체 (노션 BP_PerspectiveObject의 핵심 로직).
 * 들고 있는 동안 "보이는 크기"를 그대로 유지하면서, 바라보는 곳의 벽/바닥 바로 앞까지 물체를 보내고
 * 크기 = 잡았을 때 크기 × (지금 거리 ÷ 잡았을 때 거리) 로 바꾼다.
 *
 * 사용법
 * - 액터의 루트 컴포넌트가 Static Mesh 같은 프리미티브여야 한다 (물리 시뮬레이션 켜 두기).
 * - 데스크톱: DesktopPawn이 클릭하면 자동으로 BeginHold / EndHold를 부른다.
 * - VR: 같은 액터에 VR 템플릿 GrabComponent가 있으면, 손으로 잡는 순간 자동으로 원근법 모드로 바뀐다
 *       (bAutoHookVRGrab). 직접 연결하려면 BeginHold(카메라, 손의 Aim 컴포넌트) / EndHold 를 부르면 된다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PCS_VR_API UPerspectiveObjectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPerspectiveObjectComponent();

	/** 처음 크기 대비 최소 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "0.01"))
	float MinScale = 0.25f;

	/** 처음 크기 대비 최대 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "0.01"))
	float MaxScale = 8.f;

	/** 바라보는 곳을 찾는 최대 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "100.0"))
	float MaxTraceDistance = 5000.f;

	/** 눈에서 물체 중심까지 최소 거리 (cm). 물체가 얼굴 안으로 들어오지 않게 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "1.0"))
	float MinHoldDistance = 30.f;

	/** 벽/바닥 겹침 검사 때 물체를 이만큼(cm) 작게 본다. 표면에 딱 붙어도 겹침으로 치지 않게 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "0.0"))
	float SkinWidth = 0.5f;

	/** VR 템플릿 GrabComponent로 잡으면 자동으로 원근법 모드로 바꾼다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective")
	bool bAutoHookVRGrab = true;

	UPROPERTY(BlueprintAssignable, Category = "Perspective")
	FOnPerspectiveHoldStarted OnHoldStarted;

	/** 놓았을 때. ScaleRatio = 처음 크기 대비 배율 (압력판 "3배 이상 = 무거움" 판정 등에 사용) */
	UPROPERTY(BlueprintAssignable, Category = "Perspective")
	FOnPerspectiveReleased OnReleased;

	/**
	 * 들기 시작.
	 * @param Eye  거리와 크기를 재는 기준 (플레이어 카메라)
	 * @param Aim  바라보는 방향 (VR: 손의 Aim 컴포넌트). 비우면 Eye 방향을 쓴다
	 */
	UFUNCTION(BlueprintCallable, Category = "Perspective")
	bool BeginHold(USceneComponent* Eye, USceneComponent* Aim = nullptr);

	/** 놓기. 물리와 충돌을 원래대로 되돌린다 */
	UFUNCTION(BlueprintCallable, Category = "Perspective")
	void EndHold();

	UFUNCTION(BlueprintPure, Category = "Perspective")
	bool IsHeld() const { return bHeld; }

	/** 처음 크기 대비 지금 배율 */
	UFUNCTION(BlueprintPure, Category = "Perspective")
	float GetScaleRatio() const;

	/** 처음 크기로 되돌리기 */
	UFUNCTION(BlueprintCallable, Category = "Perspective")
	void ResetScale();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateHeld();
	bool IsBlockedAt(const FVector& Center, const FVector& Extent) const;
	void CheckVRGrab();
	bool IsVRGrabHeld() const;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> Prim;

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> VRGrabComp;

	TWeakObjectPtr<USceneComponent> EyeComp;
	TWeakObjectPtr<USceneComponent> AimComp;

	FVector OriginalScale = FVector::OneVector;
	FVector GrabScale = FVector::OneVector;
	FQuat GrabRotation = FQuat::Identity;
	float GrabEyeYaw = 0.f;
	float GrabDistance = 100.f;

	bool bHeld = false;
	bool bWasSimulating = false;
	bool bHeldByVRGrab = false;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;

	FTimerHandle VRGrabPollHandle;
};

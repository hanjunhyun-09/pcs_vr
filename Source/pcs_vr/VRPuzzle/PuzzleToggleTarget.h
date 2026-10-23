#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleToggleTarget.generated.h"

class APuzzleToggleTarget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPuzzleToggleEvent, APuzzleToggleTarget*, Target, bool, bOn);

/**
 * 손 장치(APuzzleHandDevice)의 오른손 버튼으로 켜고 끄는 방 장비 하나 (노션 4스테이지: 조명, 경고등, 모니터, 환풍기).
 * 레벨에 이미 놓인 액터들을 묶어서 함께 켜고 끈다:
 * - Lights: 조명 액터 (Movable). 켜면 보이고, 끄면 안 보인다
 * - ShowWhenOn: 켜져 있을 때만 보이는 액터 (모니터 화면 글자, 빛나는 램프 등). APuzzleSign이면 글자판으로 다룬다
 * - SpinWhenOn: 켜져 있는 동안 돌아가는 액터 (환풍기 날개 축). 이 액터의 앞(+X) 방향을 축으로 돈다
 * 5스테이지에서는 코어 제어판 램프 4개도 같은 클래스로 만든다.
 */
UCLASS()
class PCS_VR_API APuzzleToggleTarget : public AActor
{
	GENERATED_BODY()

public:
	APuzzleToggleTarget();

	/** 로그와 디버그용 이름 (예: 천장 조명) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	bool bStartOn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	TArray<TObjectPtr<AActor>> Lights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	TArray<TObjectPtr<AActor>> ShowWhenOn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	TArray<TObjectPtr<AActor>> SpinWhenOn;

	/** 다 돌았을 때 회전 속도 (도/초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	float SpinSpeed = 720.f;

	/** 멈춤 ↔ 최고 속도까지 걸리는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle", meta = (ClampMin = "0.01"))
	float SpinUpTime = 0.8f;

	UPROPERTY(BlueprintAssignable, Category = "Toggle")
	FPuzzleToggleEvent OnToggled;

	UFUNCTION(BlueprintCallable, Category = "Toggle")
	void Toggle();

	/** bNotify가 false면 OnToggled를 부르지 않는다 (진행 담당이 한꺼번에 끌 때) */
	UFUNCTION(BlueprintCallable, Category = "Toggle")
	void SetOn(bool bNewOn, bool bNotify = true);

	UFUNCTION(BlueprintPure, Category = "Toggle")
	bool IsOn() const { return bOn; }

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	void Apply();

	UPROPERTY(VisibleAnywhere, Category = "Toggle")
	TObjectPtr<USceneComponent> Root;

	bool bOn = false;
	float SpinRate = 0.f;
};

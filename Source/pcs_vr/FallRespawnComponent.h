#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FallRespawnComponent.generated.h"

/**
 * 퍼즐 물체에 붙이는 안전장치.
 * 물체가 처음 위치보다 FallDistance 이상 아래로 떨어지면(월드 밖으로 추락 등)
 * 처음 위치와 회전으로 되돌리고 속도를 0으로 만든다.
 * (노션 "막힘 방지: 물체를 잃어버려도 다시 받을 수 있게" 항목)
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PCS_VR_API UFallRespawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 처음 높이보다 이만큼(cm) 아래로 내려가면 되돌린다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (ClampMin = "10.0"))
	float FallDistance = 200.f;

	/** 확인 간격(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (ClampMin = "0.1"))
	float CheckInterval = 0.5f;

	/** 지금 바로 처음 위치로 되돌리기 */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void RespawnNow();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CheckFallen();

	FTransform HomeTransform;
	FTimerHandle CheckHandle;
};

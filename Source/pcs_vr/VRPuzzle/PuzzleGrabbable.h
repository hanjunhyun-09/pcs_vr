#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleGrabbable.generated.h"

class UActorComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * 손으로 잡는 퍼즐 물건의 공통 부모 (카드, 큐브 등).
 * 잡기 자체는 블루프린트 자식에 붙인 VR 템플릿 GrabComponent가 처리하고,
 * 이 클래스는 잡힘 상태를 지켜보며 다음을 한다.
 * - 색칠 (Color, Emissive, 반짝임)
 * - bHolsterToPlayer: 손에서 놓으면 플레이어 허리로 돌아간다 (노션: 카드는 허리에 자동으로 걸림, 잃어버릴 수 없게)
 * 카드 종류는 액터 Tag로 구분한다 (KeycardPlayer / KeycardAdmin).
 */
UCLASS()
class PCS_VR_API APuzzleGrabbable : public AActor
{
	GENERATED_BODY()

public:
	APuzzleGrabbable();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FLinearColor Color = FLinearColor(0.8f, 0.8f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable", meta = (ClampMin = "0.0"))
	float Emissive = 0.f;

	/** true면 은은하게 깜빡이며 빛난다 (바닥에 떨어진 관리자 카드) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	bool bGlowPulse = false;

	/** true면 손에서 놓은 뒤 HolsterDelay초 후 플레이어 허리로 돌아간다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Holster")
	bool bHolsterToPlayer = false;

	/** 카메라 기준 허리 위치 (앞, 오른쪽, 위) cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Holster")
	FVector HolsterOffset = FVector(18.f, 22.f, -60.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Holster", meta = (ClampMin = "0.0"))
	float HolsterDelay = 0.6f;

	/**
	 * 0보다 크면 이 크기(반지름 cm, 실제 크기)의 보이지 않는 잡기 영역을 만든다.
	 * 템플릿 VRPawn은 손 주변 6cm 구 안에 물체의 충돌(PhysicsBody)이 있어야 잡을 수 있어서,
	 * 바닥에 납작하게 놓인 카드처럼 손이 닿기 힘든 물건에 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FVector GrabAssistExtent = FVector::ZeroVector;

	/** 템플릿 GrabComponent로 잡혀 있는지 (자식 클래스가 다른 잡기 방식을 더할 수 있음) */
	UFUNCTION(BlueprintPure, Category = "Grabbable")
	virtual bool IsHeld() const;

	UFUNCTION(BlueprintPure, Category = "Grabbable")
	bool IsHolstered() const { return bHolstered; }

	UStaticMeshComponent* GetMesh() const { return Mesh; }
	UActorComponent* GetGrabComponent() const { return GrabComp; }

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	/** 잡힌 순간 / 놓은 순간 (자식 클래스에서 사용) */
	virtual void OnGrabbed() {}
	virtual void OnReleased() {}

	void SetGlow(float NewEmissive);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grabbable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	UPROPERTY(VisibleAnywhere, Category = "Grabbable")
	TObjectPtr<UBoxComponent> GrabAssist;

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void UpdateHolster();

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> GrabComp;

	bool bWasHeld = false;
	bool bHolstered = false;
	double ReleasedTime = -1000.0;
};

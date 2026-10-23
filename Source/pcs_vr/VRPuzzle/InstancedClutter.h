#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstancedClutter.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** 같은 메시 + 같은 머티리얼인 잡동사니 묶음 */
USTRUCT(BlueprintType)
struct FClutterGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clutter")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clutter")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clutter")
	TArray<FTransform> Transforms;
};

/**
 * 바닥 잡동사니(먼지 더미, 콘크리트 조각, 바랜 종이)를 인스턴스 메시로 한꺼번에 그리는 액터.
 * 액터 수백 개 대신 묶음마다 컴포넌트 하나 → 그리기 호출(드로우콜)이 크게 줄어든다 (VR 프레임 절약).
 * dress_grime.py가 Groups를 채운다. 충돌·내비게이션·그림자 없음 (퍼즐과 길을 막지 않음).
 * Transforms는 월드 좌표 (액터는 원점에 둔다).
 */
UCLASS()
class PCS_VR_API AInstancedClutter : public AActor
{
	GENERATED_BODY()

public:
	AInstancedClutter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clutter")
	TArray<FClutterGroup> Groups;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void Build();

	UPROPERTY(VisibleAnywhere, Category = "Clutter")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Instances;
};

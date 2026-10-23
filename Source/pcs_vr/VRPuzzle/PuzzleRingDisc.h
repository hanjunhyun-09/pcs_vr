#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleRingDisc.generated.h"

class APuzzleButton;
class UMaterialInstanceDynamic;

/**
 * 벽의 회전 원판 퍼즐 (5스테이지 시점 구역: 관측 원판 ① ②).
 * 원판은 안쪽 → 바깥 고리 여러 개(Rings)로 나뉘어 있고, 고리마다 기호 그림의 조각이 그려져 있다.
 * 고리마다 버튼이 하나씩 있어서 누르면 그 고리가 한 칸(360 / StepsPerTurn 도) 돈다.
 * 모든 고리의 칸이 같아지면 그림이 이어진 것 → 성공: 원판 전체가 바로 선 방향(0칸)으로 돌아가 기호가 똑바로 읽히고 OnSolved.
 * (모든 고리를 같이 돌린 상태도 그림은 이어져 보이므로 같이 정답으로 친다)
 * 액터 위치 = 원판 가운데, 앞(+X) = 플레이어 쪽. 고리 액터는 원판 가운데에 두고, 그림 조각은 고리 액터에 붙여 둔다.
 */
UCLASS()
class PCS_VR_API APuzzleRingDisc : public AActor
{
	GENERATED_BODY()

public:
	APuzzleRingDisc();

	/** 안쪽부터 고리 액터 (Movable). 그림 조각은 여기에 붙어 같이 돈다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RingDisc")
	TArray<TObjectPtr<AActor>> Rings;

	/** 고리마다 하나씩, 누르면 그 고리가 한 칸 돈다 (Rings와 같은 순서, bOneShot = false) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RingDisc")
	TArray<TObjectPtr<APuzzleButton>> Buttons;

	/** 시작할 때 고리마다 돌아가 있는 칸 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RingDisc")
	TArray<int32> StartSteps;

	/** 한 바퀴의 칸 수. 8 = 한 번 누를 때 45도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RingDisc", meta = (ClampMin = "2"))
	int32 StepsPerTurn = 8;

	/** 한 칸 도는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RingDisc", meta = (ClampMin = "0.05"))
	float TurnTime = 0.3f;

	/** 그림 조각 (M_VRP_Solid). 맞추면 밝게 빛난다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RingDisc")
	TArray<TObjectPtr<AActor>> Pieces;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RingDisc")
	FLinearColor PieceColor = FLinearColor(0.95f, 0.9f, 0.8f);

	UPROPERTY(BlueprintAssignable, Category = "RingDisc")
	FPuzzleSimpleEvent OnSolved;

	UFUNCTION(BlueprintPure, Category = "RingDisc")
	bool IsSolved() const { return bSolved; }

	/** 고리 하나를 한 칸 돌린다 */
	UFUNCTION(BlueprintCallable, Category = "RingDisc")
	void TurnRing(int32 Index);

	/** 판정 없이 바로 성공 처리 (테스트용) */
	UFUNCTION(BlueprintCallable, Category = "RingDisc")
	void Solve();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleButton0();

	UFUNCTION()
	void HandleButton1();

	UFUNCTION()
	void HandleButton2();

	UFUNCTION()
	void HandleButton3();

	void ApplyAngles();
	void CheckSolved();
	void SetPieceGlow(float Emissive);

	UPROPERTY(VisibleAnywhere, Category = "RingDisc")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PieceMIDs;

	/** 원판 기준 고리의 처음 회전 (0칸일 때) */
	TArray<FQuat> RingRest;
	TArray<int32> Current;
	/** 지금 보이는 각도 (도). 목표 Current * 칸 각도 쪽으로 움직인다 */
	TArray<float> Shown;
	TArray<float> Target;
	float SolvedTime = -1.f;
	bool bSolved = false;
};

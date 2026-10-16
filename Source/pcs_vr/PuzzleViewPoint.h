#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleViewPoint.generated.h"

class UMaterialInstanceDynamic;

/**
 * 시점 맞추기 판정 (노션 "3스테이지 퍼즐 설계" 공통 규칙, The Witness 참고).
 * 흩어진 조각(Pieces: 글자판 APuzzleSign 또는 메시 액터)이 이 위치에서 LookTarget 쪽을 볼 때만 하나의 모양으로 보인다.
 * - 정답 위치에 가까워질수록 조각이 은은하게 빛난다
 * - 머리(카메라)가 판정 영역(ZoneSize x ZoneHeight) 안에 있고, 바라보는 방향과 LookTarget 방향의 Dot ≥ MinDot이면 성공
 *   → 조각이 한 번 번쩍이고 OnSolved (한 번만)
 * 액터 위치 = 정답 위치의 바닥. 판정 높이를 넓게 둬서 키가 달라도(앉아서도) 성공한다.
 */
UCLASS()
class PCS_VR_API APuzzleViewPoint : public AActor
{
	GENERATED_BODY()

public:
	APuzzleViewPoint();

	/** 조각들이 하나로 모여 보이는 곳 (월드 좌표, 보통 조각 모양의 가운데) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint")
	FVector LookTarget = FVector::ZeroVector;

	/** 판정 영역 가로·세로 (cm). 노션: 60cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint")
	FVector2D ZoneSize = FVector2D(60.f, 60.f);

	/** 판정 영역 높이 (cm, 바닥부터). 노션: 2m */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint")
	float ZoneHeight = 200.f;

	/** 바라보는 방향 허용치. 0.97 ≈ 14도 이내 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float MinDot = 0.97f;

	/** 이 거리(cm) 안으로 들어오면 조각이 빛나기 시작한다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint")
	float GlowRadius = 300.f;

	/** 시점 맞추기 조각들 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint")
	TArray<TObjectPtr<AActor>> Pieces;

	/** 조각 색 (글자판은 글자 색, 메시는 M_VRP_Solid 색) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint")
	FLinearColor PieceColor = FLinearColor(0.9f, 0.9f, 0.88f);

	/** 메시 조각의 발광: 멀 때 / 가장 가까울 때 / 맞춘 뒤 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint")
	float IdleEmissive = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint")
	float NearEmissive = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewPoint")
	float SolvedEmissive = 2.f;

	UPROPERTY(BlueprintAssignable, Category = "ViewPoint")
	FPuzzleSimpleEvent OnSolved;

	UFUNCTION(BlueprintPure, Category = "ViewPoint")
	bool IsSolved() const { return bSolved; }

	/** 판정 없이 바로 성공 처리 (테스트용) */
	UFUNCTION(BlueprintCallable, Category = "ViewPoint")
	void Solve();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	/** 0 = 멀다, 1 = 정답 근처 (조각 밝기) */
	void ApplyGlow(float Alpha);

	UPROPERTY(VisibleAnywhere, Category = "ViewPoint")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PieceMIDs;

	bool bSolved = false;
	float SolvedTime = -1.f;
	float LastAlpha = -1.f;
};

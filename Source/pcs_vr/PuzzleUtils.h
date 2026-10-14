#pragma once

#include "CoreMinimal.h"

class AActor;
class APawn;
class UActorComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class USceneComponent;

/**
 * 퍼즐 액터들이 같이 쓰는 작은 도우미 함수 모음.
 * - 단색 머티리얼(M_VRP_Solid) 칠하기
 * - 플레이어 폰 / 카메라 / 손 컴포넌트 찾기
 * - VR 템플릿 GrabComponent의 잡힘 상태 읽기
 */
namespace PuzzleUtils
{
	/** /Game/VRPuzzle/Materials/M_VRP_Solid 로 칠한다 (파라미터 Color, Emissive) */
	PCS_VR_API UMaterialInstanceDynamic* ApplySolidColor(UPrimitiveComponent* Comp, const FLinearColor& Color, float Emissive = 0.f);
	PCS_VR_API void SetSolidColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Emissive);

	PCS_VR_API APawn* GetPlayerPawn(const UObject* WorldContext);

	/** 플레이어 눈 위치와 방향 (VR이면 헤드셋 카메라) */
	PCS_VR_API bool GetPlayerView(const UObject* WorldContext, FVector& OutLocation, FRotator& OutRotation);

	/** 플레이어 카메라 컴포넌트 (메모를 눈앞에 붙일 때 사용) */
	PCS_VR_API USceneComponent* GetPlayerCamera(const UObject* WorldContext);

	/** 이름으로 폰의 컴포넌트 찾기 (예: MotionControllerLeftGrip) */
	PCS_VR_API USceneComponent* FindPawnComponent(const APawn* Pawn, const TCHAR* Name);

	/** VR 템플릿의 GrabComponent (블루프린트 클래스 GrabComponent_C) */
	PCS_VR_API UActorComponent* FindGrabComponent(const AActor* Actor);
	PCS_VR_API bool IsGrabComponentHeld(const UActorComponent* GrabComp);
}

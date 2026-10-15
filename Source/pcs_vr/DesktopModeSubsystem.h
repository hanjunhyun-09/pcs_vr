#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Subsystems/WorldSubsystem.h"
#include "DesktopModeSubsystem.generated.h"

/**
 * 데스크톱 테스트 모드 (노션: "헤드셋이 연결 안 돼 있으면 자동으로 마우스 + 키보드용 캐릭터로 시작").
 * 게임이 시작되고 플레이어 폰이 생기면 한 번 확인해서, VR(스테레오)이 꺼져 있으면
 * VRPawn을 없애고 같은 자리에 DesktopPawnClass를 스폰해서 조종한다.
 *
 * - 에디터의 일반 Play(뷰포트/새 창) → 데스크톱 모드, VR Preview → VR 그대로
 * - 실행 인자 -desktop 또는 콘솔 변수 vrpuzzle.ForceDesktop 1 → 헤드셋이 있어도 데스크톱 모드
 * - 블루프린트 데스크톱 폰을 쓰려면 DefaultGame.ini에:
 *     [/Script/pcs_vr.DesktopModeSubsystem]
 *     DesktopPawnClass=/Game/VRPuzzle/Blueprints/Core/BP_DesktopPawn.BP_DesktopPawn_C
 */
UCLASS(Config = Game)
class PCS_VR_API UDesktopModeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UDesktopModeSubsystem, STATGROUP_Tickables);
	}

	/** 지금 데스크톱 모드로 실행 중인지 (VR이 꺼져 있거나 강제) */
	UFUNCTION(BlueprintPure, Category = "Desktop")
	static bool IsDesktopMode();

	/** 스폰할 데스크톱 폰. 비우면 C++ ADesktopPawn */
	UPROPERTY(Config)
	TSoftClassPtr<APawn> DesktopPawnClass;

private:
	void SwapToDesktopPawn(class APlayerController* PC, APawn* OldPawn);

	bool bChecked = false;
};

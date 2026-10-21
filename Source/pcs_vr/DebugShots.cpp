#include "DebugShots.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "PuzzleUtils.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDebugShots, Log, All);

void DebugShots::WhenReady(UWorld* World, TFunction<void()> Action)
{
	if (!World)
	{
		return;
	}
	int32 Remaining = 0;
#if WITH_EDITOR
	Remaining = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
	FTimerHandle Handle;
	if (Remaining > 0)
	{
		UE_LOG(LogDebugShots, Log, TEXT("셰이더 컴파일 기다리는 중 (%d개 남음)"), Remaining);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [World, Action]() { WhenReady(World, Action); }), 1.f, false);
		return;
	}
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [Action]() { Action(); }), 2.f, false);
}

/**
 * 헤드셋 없이 맵 모습 확인용: 카메라를 정해진 자리로 옮겨 가며 스크린샷을 남긴다 (Saved/Screenshots).
 * 콘솔: pcs.Shots 이름:x:y:z:yaw:pitch 이름2:x:y:z:yaw:pitch ...   (월드 좌표 cm, 1.5초 간격)
 * 예) -ExecCmds="pcs.Shots L02_A:20500:200:160:60:-5"
 */
static FAutoConsoleCommandWithWorldAndArgs GShotsCommand(
	TEXT("pcs.Shots"),
	TEXT("카메라를 이름:x:y:z:yaw:pitch 자리들로 옮겨 가며 스크린샷을 남긴다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		DebugShots::WhenReady(World, [World, Args]()
		{
		for (int32 i = 0; i < Args.Num(); ++i)
		{
			TArray<FString> P;
			Args[i].ParseIntoArray(P, TEXT(":"));
			if (P.Num() != 6)
			{
				UE_LOG(LogDebugShots, Warning, TEXT("pcs.Shots: 형식이 틀림 (%s)"), *Args[i]);
				continue;
			}
			const FString Name = P[0];
			const FVector Loc(FCString::Atof(*P[1]), FCString::Atof(*P[2]), FCString::Atof(*P[3]));
			const FRotator Rot(FCString::Atof(*P[5]), FCString::Atof(*P[4]), 0.f);
			FTimerHandle Handle;
			// 자리를 옮기고 0.7초 뒤에 찍는다
			const float Start = 0.01f + 1.5f * i;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [World, Loc, Rot]()
			{
				if (USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(World))
				{
					Cam->SetWorldLocationAndRotation(Loc, Rot);
				}
			}), Start, false);
			FTimerHandle ShotHandle;
			World->GetTimerManager().SetTimer(ShotHandle, FTimerDelegate::CreateWeakLambda(World, [Name]()
			{
				FScreenshotRequest::RequestScreenshot(Name, false, false);
				UE_LOG(LogDebugShots, Log, TEXT("pcs.Shots: %s"), *Name);
			}), Start + 0.7f, false);
		}
		});
	}));

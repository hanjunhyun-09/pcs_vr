#include "BGMSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogBGM, Log, All);

namespace
{
	const TCHAR* BGMFolder = TEXT("/Game/VRPuzzle/Audio/BGM/");

	FSoftObjectPath MakeTrackPath(const TCHAR* AssetName)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), BGMFolder, AssetName, AssetName));
	}
}

FSoftObjectPath UBGMSubsystem::GetTrackForMap(const FString& MapName)
{
	static const TMap<FString, const TCHAR*> Tracks = {
		{ TEXT("L_MainMenu"),       TEXT("BGM_00_MainMenu") },
		{ TEXT("L_00_Intro"),       TEXT("BGM_01_Isolation") },
		{ TEXT("L_01_Isolation"),   TEXT("BGM_01_Isolation") },
		{ TEXT("L_02_Perspective"), TEXT("BGM_02_Perspective") },
		{ TEXT("L_03_Observation"), TEXT("BGM_03_Observation") },
		{ TEXT("L_04_Device"),      TEXT("BGM_04_Device") },
		{ TEXT("L_05_Core"),        TEXT("BGM_05_Core") },
		{ TEXT("L_06_Ending"),      TEXT("BGM_06_Ending") },
	};

	if (const TCHAR* const* Found = Tracks.Find(MapName))
	{
		return MakeTrackPath(*Found);
	}
	return FSoftObjectPath();
}

void UBGMSubsystem::Deinitialize()
{
	// 레벨 전환을 넘어 살아남는 컴포넌트라서, PIE를 끝낼 때 직접 멈추지 않으면 에디터에서 계속 울린다
	if (Current)
	{
		Current->Stop();
		Current = nullptr;
	}
	CurrentTrack.Reset();
	Super::Deinitialize();
}

void UBGMSubsystem::HandleLevelStarted(UWorld& World)
{
	CurrentMap = UGameplayStatics::GetCurrentLevelName(&World, true);
	PlayTrack(World, GetTrackForMap(CurrentMap));
}

void UBGMSubsystem::PlayTrack(UWorld& World, const FSoftObjectPath& Track)
{
	// 같은 곡이 이미 나오고 있으면 그대로 이어 간다 (인트로 → 격리실)
	if (Track.IsValid() && Track == CurrentTrack && Current && Current->IsPlaying())
	{
		return;
	}

	if (Current)
	{
		if (FadeTime > 0.f)
		{
			Current->FadeOut(FadeTime, 0.f);   // bAutoDestroy라서 다 줄어들면 스스로 정리됨
		}
		else
		{
			Current->Stop();
		}
		Current = nullptr;
	}
	CurrentTrack.Reset();

	if (!Track.IsValid())
	{
		UE_LOG(LogBGM, Log, TEXT("%s: 배경음 없음"), *CurrentMap);
		return;
	}

	USoundBase* Sound = Cast<USoundBase>(Track.TryLoad());
	if (!Sound)
	{
		UE_LOG(LogBGM, Warning, TEXT("%s: 배경음 %s 을 찾지 못함 (Scripts/import_bgm.py 로 임포트했는지 확인)"), *CurrentMap, *Track.ToString());
		return;
	}

	// bPersistAcrossLevelTransition = true: OpenLevel 뒤에도 끊기지 않는다
	Current = UGameplayStatics::CreateSound2D(&World, Sound, Volume, 1.f, 0.f, nullptr, true, true);
	if (!Current)
	{
		return;   // 오디오가 꺼진 환경 (-nosound, 데디케이티드 서버 등)
	}

	CurrentTrack = Track;
	if (FadeTime > 0.f)
	{
		Current->FadeIn(FadeTime, 1.f);
	}
	else
	{
		Current->Play();
	}
	UE_LOG(LogBGM, Log, TEXT("%s: 배경음 %s 재생 (음량 %.2f)"), *CurrentMap, *Sound->GetName(), Volume);
}

void UBGMSubsystem::StopBGM(float FadeOutTime)
{
	if (Current)
	{
		if (FadeOutTime > 0.f)
		{
			Current->FadeOut(FadeOutTime, 0.f);
		}
		else
		{
			Current->Stop();
		}
		Current = nullptr;
	}
	CurrentTrack.Reset();
}

void UBGMSubsystem::ResumeBGM()
{
	if (UWorld* World = GetWorld())
	{
		PlayTrack(*World, GetTrackForMap(CurrentMap));
	}
}

void UBGMSubsystem::SetBGMVolume(float NewVolume)
{
	Volume = FMath::Clamp(NewVolume, 0.f, 1.f);
	if (Current)
	{
		Current->SetVolumeMultiplier(Volume);
	}
}

void UBGMWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		if (UBGMSubsystem* BGM = GI->GetSubsystem<UBGMSubsystem>())
		{
			BGM->HandleLevelStarted(InWorld);
		}
	}
}

bool UBGMWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

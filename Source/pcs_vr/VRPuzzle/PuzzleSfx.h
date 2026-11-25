#pragma once

#include "CoreMinimal.h"

/**
 * 퍼즐 장치 효과음 (/Game/VRPuzzle/Audio/SFX, Scripts/make_sfx.py 로 합성 → import_sfx.py 로 임포트).
 * 효과음 에셋에는 거리 감쇠(ATT_SFX)가 붙어 있어서 Play로 틀면 VR에서 소리 나는 방향이 들린다.
 */
namespace PuzzleSfx
{
	enum class ESfx : uint8
	{
		Click,          // 버튼 누름, 엘리베이터 호출
		Beep,           // 키패드 숫자
		Accept,         // 카드 승인
		Deny,           // 카드 거부, 키패드 틀림
		Success,        // 키패드 정답, 회전 원판 완성
		LeverClunk,     // 배전함 레버 내려감 (전원 들어옴)
		DoorOpen,       // 문 열림/닫힘
		ElevatorDing,   // 엘리베이터 도착
		PlateThud,      // 압력판 눌림
		Dispense,       // 디스펜서 큐브 나옴
		Snap,           // 코어 슬롯·도면 칸에 끼워짐
		RingTurn,       // 회전 원판 고리 한 칸
		PadChime,       // 빛나는 발판 도착
		CoreShutdown,   // 중앙 코어 정지
	};

	/** Location에서 3D로 재생한다 */
	PCS_VR_API void Play(const UObject* WorldContext, ESfx Sfx, const FVector& Location, float Volume = 1.f);

	/** 위치 없이 귀 옆에서 재생한다 (스테이지 클리어 같은 큰 순간) */
	PCS_VR_API void Play2D(const UObject* WorldContext, ESfx Sfx, float Volume = 1.f);
}

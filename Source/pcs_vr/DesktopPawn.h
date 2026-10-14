#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DesktopPawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UPerspectiveObjectComponent;
class UPhysicsHandleComponent;
struct FInputActionValue;

/**
 * 데스크톱 테스트 캐릭터 (노션 BP_DesktopPawn). 집에서 VR 없이 퍼즐을 테스트할 때 쓴다.
 * 헤드셋이 없으면 DesktopModeSubsystem이 VRPawn 대신 자동으로 이 캐릭터를 스폰한다.
 *
 * 조작
 * - 마우스: 둘러보기          - WASD: 이동
 * - 왼쪽 클릭: 잡기 / 놓기 (원근법 물체, 물리 물체. 잡을 수 없는 물체는 상호작용)
 * - E: 상호작용 (Interactable)  - 오른쪽 클릭: 바라보는 바닥으로 텔레포트 (VR 텔레포트 대신, 높은 곳 올라가기)
 */
UCLASS()
class PCS_VR_API ADesktopPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ADesktopPawn();

	/** 서 있을 때 눈높이 (cm, 바닥 기준). 노션 기준 160cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Desktop")
	float EyeHeight = 160.f;

	/** 잡기 최대 거리 (cm). 원근법 물체는 멀리서도 잡을 수 있어야 함 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desktop")
	float GrabRange = 1500.f;

	/** E 상호작용 최대 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desktop")
	float InteractRange = 300.f;

	/** 오른쪽 클릭 텔레포트 최대 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desktop")
	float TeleportRange = 2000.f;

	/** 물리 물체를 들고 있을 때 눈앞 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desktop")
	float PhysicsHoldDistance = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desktop")
	float MouseSensitivity = 1.f;

	/** 화면 가운데 조준점 표시 (개발 빌드에서만) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desktop")
	bool bShowCrosshair = true;

	/** 잡기 / 놓기 (왼쪽 클릭과 같음) */
	UFUNCTION(BlueprintCallable, Category = "Desktop")
	void ToggleGrab();

	/** 바라보는 물체와 상호작용 (E와 같음) */
	UFUNCTION(BlueprintCallable, Category = "Desktop")
	void InteractInFront();

	/** 바라보는 바닥으로 텔레포트 (오른쪽 클릭과 같음) */
	UFUNCTION(BlueprintCallable, Category = "Desktop")
	void TeleportToAim();

	UFUNCTION(BlueprintPure, Category = "Desktop")
	UCameraComponent* GetCamera() const { return Camera; }

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Desktop")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Desktop")
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

private:
	void CreateInputObjects();
	bool TraceFromCamera(float Range, FHitResult& OutHit) const;
	void ReleaseHeld();

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnGrab(const FInputActionValue& Value);
	void OnInteract(const FInputActionValue& Value);
	void OnTeleport(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DesktopContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GrabAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TeleportAction;

	UPROPERTY(Transient)
	TObjectPtr<UPerspectiveObjectComponent> HeldPerspective;
};

// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/PartyCharacter.h"
#include "Controllers/CompanionAIController.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "AI/Components/PlayerCrowdAgentComponent.h"
#include "AI/CombatRoleTags.h"
#include "TacticalAI.h"   // LogTacticalAI

APartyCharacter::APartyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = ACompanionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCharacterMovement()->bUseRVOAvoidance = false;

	// ───── 빙의용 카메라 (동료 전용) ─────
	// 카메라 붐: 캐릭터 뒤. 빙의 시 컨트롤러 회전을 따른다.
	// カメラブーム：憑依時はコントローラー回転に追従。
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 카메라가 캡슐·메시를 관통 (동료 너머로 카메라가 막히는 것 방지).
	// カメラがカプセル・メッシュを貫通（仲間越しのカメラブロック防止）。
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	PlayerAgentComp = CreateDefaultSubobject<UPlayerCrowdAgentComponent>(TEXT("PlayerAgentComp"));

	// 역할 기본값. 헤더 초기화 불가(Native Tag는 모듈 로드 후 유효)라 생성자에서 대입.
	// 役割の既定値。Native Tagはモジュールロード後に有効なため、ここで代入。
	CombatRole = CombatRoleTags::Melee;
}

void APartyCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentTargetLocation = GetActorLocation();
}

void APartyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APartyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 빙의 시 플레이어 입력 바인딩. 동료가 리더가 됐을 때만 의미를 가진다.
	// 憑依時のプレイヤー入力バインド。仲間がリーダーになった時のみ有効。
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APartyCharacter::Move);
		EnhancedInput->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &APartyCharacter::Look);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &APartyCharacter::Look);
	}
	else
	{
		UE_LOG(LogTacticalAI, Error,
			TEXT("'%s' Failed to find an Enhanced Input component!"), *GetNameSafe(this));
	}
}

void APartyCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void APartyCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void APartyCharacter::DoMove(float Right, float Forward)
{
	if (GetController() == nullptr) return;

	// 컨트롤러 yaw 기준으로 전/우 방향을 산출해 이동 입력.
	// コントローラーyaw基準で前/右方向を算出し移動入力。
	const FRotator Rotation = GetController()->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

void APartyCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() == nullptr) return;

	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void APartyCharacter::DoJumpStart()
{
	Jump();
}

void APartyCharacter::DoJumpEnd()
{
	StopJumping();
}

void APartyCharacter::InvalidateMoveCache()
{
	// 캐시를 도달 불가능한 값으로 → 다음 명령이 반드시 threshold 초과 → 강제 재발행.
	// キャッシュを到達不能値にし、次の命令で必ずthreshold超過＝強制再発行。
	CurrentTargetLocation = FVector(FLT_MAX);
}

void APartyCharacter::UpdateTargetSlotLocation(const FVector& NewTarget, bool bForceRefresh)
{
	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC) return;

	// 목표가 거의 안 움직였으면 재발행 스킵(저렴한 MoveTo 중복 억제).
	// Yielding 중엔 force로 캐시를 우회해 매 틱 확실히 MoveTo를 갱신한다.
	// 目標がほぼ動いていなければ再発行スキップ。Yielding中はforceで毎Tick更新。
	if (!bForceRefresh)
	{
		const float DistSq = FVector::DistSquared(NewTarget, CurrentTargetLocation);
		if (DistSq < FMath::Square(UpdateThreshold)) return;
	}

	CurrentTargetLocation = NewTarget;
	AIC->MoveToLocation(NewTarget, AcceptanceRadius);
}
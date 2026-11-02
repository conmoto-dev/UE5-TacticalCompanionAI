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
#include "TacticalAI.h"
#include "AI/Components/CombatMicroMovementComponent.h"
#include "AI/Components/FormationBattleComponent.h"
#include "AI/Targeting/PartyTargetSelectorComponent.h"
#include "Party/PartyManager.h"
#include "AbilitySystem/TacticalAbilitySystemComponent.h"
#include "AbilitySystem/Tags/TacticalGameplayTags.h"

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
	
	TargetSelector = CreateDefaultSubobject<UPartyTargetSelectorComponent>(TEXT("TargetSelector"));
	
	MicroMovement = CreateDefaultSubobject<UCombatMicroMovementComponent>(TEXT("MicroMovement"));
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
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &APartyCharacter::MoveReleased);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Canceled, this, &APartyCharacter::MoveReleased);
		
		EnhancedInput->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &APartyCharacter::Look);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &APartyCharacter::Look);

		// =======================================================
		// 평타 입력 바인딩
		//
		// 누름 시 한 번만 요청하고, 입력 유지 중 타수 반복은 Ability가 처리한다.
		// 입력 완료와 취소는 모두 해제 요청으로 전달한다.
		//
		// 通常攻撃の入力バインド。
		// =======================================================
		if (BasicAttackAction)
		{
			EnhancedInput->BindAction(
				BasicAttackAction, ETriggerEvent::Started,
				this, &APartyCharacter::BasicAttackStarted);

			EnhancedInput->BindAction(
				BasicAttackAction, ETriggerEvent::Completed,
				this, &APartyCharacter::BasicAttackReleased);

			EnhancedInput->BindAction(
				BasicAttackAction, ETriggerEvent::Canceled,
				this, &APartyCharacter::BasicAttackReleased);
		}
	}
	else
	{
		UE_LOG(LogTacticalAI, Error,
			TEXT("'%s' Enhanced Input Componentが見つかりません。"), *GetNameSafe(this));
	}
}

void APartyCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void APartyCharacter::MoveReleased()
{
	MovementInputAxis = FVector2D::ZeroVector;
}

void APartyCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

// =======================================================
// 플레이어 평타 시작
//
// ASC에 입력 유지 상태를 기록하고 InputBound 방식으로 평타 발동을 요청한다.
// 발동 가능 여부와 발동 후 타수 진행은 GAS와 평타 Ability가 결정한다.
//
// プレイヤーの通常攻撃開始。
// ASCに入力維持状態を記録し、InputBound方式で通常攻撃の発動を要求する。
// 発動可否と発動後の各段の進行はGASと通常攻撃Abilityが決定する。
// =======================================================
void APartyCharacter::BasicAttackStarted()
{
	if (!IsLocallyControlled() || !IsPlayerControlled()) return;

	UTacticalAbilitySystemComponent* ASC = GetTacticalAbilitySystemComponent();
	if (!ASC) return;

	ASC->RequestActivateInputBoundAbility(TacticalGameplayTags::Ability_Attack_Basic);
}

// =======================================================
// 플레이어 평타 입력 해제
//
// 입력 유지 상태만 해제한다.
// 진행 중인 타수까지 마무리한 뒤 종료하는 처리는 평타 Ability가 담당한다.
//
// プレイヤーの通常攻撃入力の解放。
// =======================================================
void APartyCharacter::BasicAttackReleased()
{
	UTacticalAbilitySystemComponent* ASC = GetTacticalAbilitySystemComponent();
	if (!ASC) return;

	ASC->RequestReleaseAbilityInput(TacticalGameplayTags::Ability_Attack_Basic);
}

// =======================================================
// 조작 주체 변경 시 평타 중단
//
// 플레이어·AI 중 누가 시작했는지와 관계없이,
// 이전 조작 주체의 평타 입력을 해제하고 실행 중인 평타를 취소한다.
//
// 操作主体の変更時に通常攻撃を中断する。
// プレイヤー・AIのどちらが開始したかに関係なく、
// 以前の操作主体の通常攻撃入力を解除し、実行中の通常攻撃をキャンセルする。
// =======================================================
void APartyCharacter::NotifyControllerChanged()
{
	// 이전 Controller의 방향 입력이 남지 않도록 먼저 초기화한다.
	// 以前のControllerの方向入力が残らないよう、先に初期化する。
	MovementInputAxis = FVector2D::ZeroVector;
	
	// [1] 게임 시작 전 최초 Controller 배정에서는 정리할 평타가 없으므로 건너뛴다.
	// [1] ゲーム開始前の初回Controller割り当てでは、解除・キャンセル対象の通常攻撃がないためスキップする。
	if (HasActorBegunPlay())
	{
		if (UTacticalAbilitySystemComponent* ASC = GetTacticalAbilitySystemComponent())
		{
			// [2] 이전 Controller 종류와 관계없이 평타 입력 유지 상태를 해제한다.
			// [2] 以前のControllerの種類に関係なく、通常攻撃の入力維持状態を解除する。
			ASC->RequestReleaseAbilityInput(TacticalGameplayTags::Ability_Attack_Basic);

			// [3] 입력 해제만으로는 현재 타수가 계속되므로 평타 Ability도 취소한다.
			// 평타 태그로 대상을 제한하여 다른 스킬은 취소하지 않는다.
			//
			// [3] 入力解除だけでは現在段が継続するため、通常攻撃Abilityもキャンセルする。
			// 通常攻撃タグで対象を限定し、他のスキルはキャンセルしない。
			FGameplayTagContainer BasicAttackTags;
			BasicAttackTags.AddTag(TacticalGameplayTags::Ability_Attack_Basic);
			ASC->CancelAbilities(&BasicAttackTags);
		}
	}

	// [4] 이전 평타를 정리한 뒤 Controller 변경 알림과 ActorInfo 갱신을 수행한다.
	// 부모 호출 중 변경 알림을 받은 처리가 새로 시작한 공격을 취소하지 않도록 한다.
	//
	// [4] 以前の通常攻撃を整理した後、Controller変更通知とActorInfo更新を実行する。
	// 親の呼び出し中に変更通知を受けた処理が開始した攻撃をキャンセルしないようにする。
	Super::NotifyControllerChanged();
}

// =======================================================
// 이동 입력 보관 및 이동 요청
// 컨트롤 입력과 UI 입력을 같은 경로에서 처리한다.
//
// 移動入力の保持と移動要求。
// コントロール入力とUI入力を同じ経路で処理する。
// =======================================================
void APartyCharacter::DoMove(float Right, float Forward)
{
	// [1] 현재 방향 입력을 보관한다. 실제 이동 여부와 별도로 관리한다.
	// [1] 現在の方向入力を保持する。実際の移動有無とは別に管理する。
	MovementInputAxis = FVector2D(Right, Forward);

	if (GetController() == nullptr) return;

	// [2] Controller Yaw 기준으로 전방·우측 방향을 구해 이동을 요청한다.
	// [2] Controller Yawから前方・右方向を求め、移動を要求する。
	const FRotator Rotation = GetController()->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

// =======================================================
// 현재 이동 입력의 월드 방향 조회
// 입력 방향만 제공하며, 실제 이동 속도나 이전 프레임의 이동 입력은 사용하지 않는다.
//
// 現在の移動入力のワールド方向を取得する。
// 入力方向のみ提供し、実際の移動速度や前フレームの移動入力は使用しない。
// =======================================================
FVector APartyCharacter::GetMovementInputDirection2D() const
{
	// [1] 로컬 플레이어가 조작하지 않거나 방향 입력이 없으면 무입력으로 처리한다.
	// [1] ローカルプレイヤーの操作でない場合、または方向入力がなければ入力なしとする。
	if (!IsLocallyControlled() || !IsPlayerControlled() || MovementInputAxis.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const AController* CurrentController = GetController();
	if (!CurrentController) return FVector::ZeroVector;

	// [2] 조회 시점의 Controller Yaw를 사용하여 카메라 기준 방향을 구한다.
	// [2] 取得時のController Yawを使い、カメラ基準の方向を求める。
	const FRotator YawRotation(0.f, CurrentController->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);

	const FVector ForwardDirection = YawMatrix.GetUnitAxis(EAxis::X);
	const FVector RightDirection = YawMatrix.GetUnitAxis(EAxis::Y);

	// [3] 입력 크기와 무관하게 수평 단위 방향을 반환한다.
	// [3] 入力の大きさにかかわらず、水平方向の単位ベクトルを返す。
	return (ForwardDirection * MovementInputAxis.Y
		+ RightDirection * MovementInputAxis.X).GetSafeNormal2D();
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

void APartyCharacter::SetPartyManager(APartyManager* InManager)
{
	OwningPartyManager = InManager;
}

APartyManager* APartyCharacter::GetPartyManager() const
{
	return OwningPartyManager.Get();
}

UTargetSelectorComponent* APartyCharacter::GetTargetSelector() const
{
	return TargetSelector;
}

bool APartyCharacter::TryGetHomeSlot(FVector& OutHomeSlot) const
{
	// [1] 소속 확인 + 전투 모드 검사. 경로는 캐릭터 → Manager 읽기 단방향 (back-ref, 등록 시 주입).
	// [1] 所属確認＋戦闘モード検査。キャラ→Manager読み取りの単方向。
	const APartyManager* Manager = OwningPartyManager.Get();
	if (!Manager || Manager->GetFormationMode() != EPartyFormationMode::Battle) return false;

	// [2] 전투 진형 컴포넌트 확보.
	// [2] 戦闘隊形コンポーネント確保。
	const UFormationBattleComponent* Battle = Manager->GetFormationBattleComponent();
	if (!Battle) return false;

	// [3] 커밋 슬롯 조회. 미커밋(전투 진입 직후 첫 배치 전 등)이면 홈 슬롯 없음.
	// [3] コミットスロット照会。未コミットならホームスロット無し。
	return Battle->TryGetCommittedSlot(this, OutHomeSlot);
}
#include "Characters/TacticalCharacterBase.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/TacticalAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TacticalCombatAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATacticalCharacterBase::ATacticalCharacterBase()
{
	// =======================================================
	// Ability System
	//
	// 동료·적·플레이어 빙의 캐릭터가 공통으로 사용하는 전투 실행 기반.
	// 플레이어와 AI가 바뀌어도 Ability와 전투 상태는 캐릭터에 유지되므로,
	// ASC와 AttributeSet을 Controller가 아닌 캐릭터에 생성한다.
	//
	// 仲間・敵・プレイヤー憑依キャラが共有する戦闘実行基盤。
	// プレイヤーとAIが切り替わってもAbilityと戦闘状態はキャラクターに保持するため、
	// ASCとAttributeSetはControllerではなくキャラクターに生成する。
	// =======================================================
	AbilitySystemComponent =
		CreateDefaultSubobject<UTacticalAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	CombatAttributeSet =
		CreateDefaultSubobject<UTacticalCombatAttributeSet>(TEXT("CombatAttributeSet"));

	// 공통 캡슐 기본값. 
	// 共通カプセル既定値。体格の違う種類は子/BPで上書き。
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// 컨트롤러 회전이 캐릭터를 직접 돌리지 않게 — 캐릭터는 "이동 방향"으로 돈다.
	// コントローラー回転でキャラを直接回さず移動方向に向ける。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	// 공통 이동 특성 기본값. 종류별 조정(속도 등)은 자식/BP에서.
	// 共通の移動特性。種類別の調整は子/BPで。
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement  = true;
	MoveComp->RotationRate               = FRotator(0.f, 500.f, 0.f);
	MoveComp->JumpZVelocity              = 500.f;
	MoveComp->AirControl                 = 0.35f;
	MoveComp->MaxWalkSpeed               = 500.f;
	MoveComp->MinAnalogWalkSpeed         = 20.f;
	MoveComp->BrakingDecelerationWalking = 2000.f;
	MoveComp->BrakingDecelerationFalling = 1500.f;
}

// =======================================================
// 전투 기반 초기화
//
// Controller의 존재 여부와 관계없이 캐릭터의 ASC를 초기화한다.
// Ability를 소유하는 주체와 실제 수행하는 몸이 모두 이 캐릭터이므로,
// OwnerActor와 AvatarActor에 동일한 Actor를 지정한다.
//
// 戦闘基盤の初期化。
// Controllerの有無に関係なく、キャラクターのASCを初期化する。
// Abilityの所有主体と実行する身体が同じキャラクターのため、
// OwnerActorとAvatarActorに同じActorを指定する。
// =======================================================
void ATacticalCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// [1] Ability가 참조할 소유자·메시·이동 컴포넌트 등의 정보를 준비한다.
	// Ability 부여 시점에서도 ActorInfo(소유자와 Avatar 정보)를 사용할 수 있도록 먼저 초기화한다.
	//
	// [1] Abilityが参照する所有者・Mesh・移動Componentなどの情報を準備する。
	// Abilityの付与処理でもActorInfoを利用できるよう、先に初期化する。
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// [2] 캐릭터 BP에서 지정한 시작 Ability를 ASC에 부여한다.
	// 중복 부여 방지는 Tactical ASC가 담당한다.
	//
	// [2] キャラクターBPで指定した初期AbilityをASCへ付与する。
	// 重複付与の防止はTactical ASCが担当する。
	if (HasAuthority())
	{
		AbilitySystemComponent->GiveStartupAbilities(StartupAbilities);
	}
}

// =======================================================
// 조작 주체 변경
//
// 동료 스왑으로 Controller가 바뀌어도 ASC와 Ability는 캐릭터에 남는다.
// ActorInfo가 이전 PlayerController 등의 참조를 유지하지 않도록 갱신한다.
//
// 操作主体の変更。
// 仲間の切替でControllerが変わっても、ASCとAbilityはキャラクターに残る。
// ActorInfoが以前のPlayerControllerなどを参照し続けないよう更新する。
// =======================================================
void ATacticalCharacterBase::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// [1] 최초 빙의 알림은 BeginPlay 이전에도 들어올 수 있다.
	// 그 시점의 ASC 초기화는 BeginPlay에 맡기고,
	// 여기서는 게임 시작 이후 Controller가 변경된 경우만 갱신한다.
	//
	// [1] 最初の憑依通知はBeginPlayより前に届く場合がある。
	// その時点のASC初期化はBeginPlayに任せ、
	// ここではゲーム開始後のController変更のみ更新する。
	if (!HasActorBegunPlay())
	{
		return;
	}
	
	// [2] 같은 캐릭터라도 스왑 후에는 연결된 PlayerController가 달라질 수 있다.
	// ASC가 현재 연결 상태를 사용하도록 ActorInfo의 참조를 다시 조회한다.
	//
	// [2] 同じキャラクターでも、切替後は関連するPlayerControllerが変わる場合がある。
	// ASCが現在の接続状態を参照できるよう、ActorInfo内の参照を再取得する。
	AbilitySystemComponent->RefreshAbilityActorInfo();
}

bool ATacticalCharacterBase::IsTargetable_Implementation() const
{
	// 사망 생명주기 도입 전의 임시 구현으로, 현재 존재하는 캐릭터는 타겟 가능하다.
	// Health가 0이 되어 State.Dead가 부여되면 해당 태그를 기준으로 판정한다.
	// 死亡ライフサイクル導入前の暫定実装として、現在存在するキャラクターはターゲット可能。
	// Healthが0になりState.Deadが付与された後は、そのタグを基準に判定する。
	return true;
}

float ATacticalCharacterBase::GetEncircleRadius_Implementation() const
{
	return EncircleRadius;
}

UAbilitySystemComponent* ATacticalCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

UTacticalAbilitySystemComponent* ATacticalCharacterBase::GetTacticalAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}
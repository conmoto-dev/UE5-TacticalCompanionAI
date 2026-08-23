#include "Characters/TacticalCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TacticalCombatAttributeSet.h"

ATacticalCharacterBase::ATacticalCharacterBase()
{
	// =======================================================
	// Ability System
	// 동료·적·플레이어 빙의 캐릭터가 공통으로 사용하는 전투 실행 기반.
	// 仲間・敵・プレイヤー憑依キャラが共有する戦闘実行基盤。
	// 
	// ASC를 먼저 생성한 뒤 AttributeSet을 생성한다.
	// AttributeSet 생성 시 유효한 ASC를 조회할 수 있어 자동 등록된다.
	// ASCを先に生成してからAttributeSetを生成する。
	// AttributeSet生成時に有効なASCを取得でき、自動登録される。
	// =======================================================
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	CombatAttributeSet = CreateDefaultSubobject<UTacticalCombatAttributeSet>(FName("CombatAttributeSet"));
	
	// 공통 캡슐 기본값. 인간형 표준 — 슬라임·보스 등 체격이 다른 종류는 자식/BP에서 덮어쓴다.
	// 共通カプセル既定値。体格の違う種類は子/BPで上書き。
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// 컨트롤러 회전이 캐릭터를 직접 돌리지 않게 — 캐릭터는 "이동 방향"으로 돈다.
	// AI 이동(동료·적)과 third-person 빙의 양쪽에 맞는 공통 설정이라 베이스에 둔다.
	// コントローラー回転でキャラを直接回さず移動方向に向ける。AI移動と憑依の両方に適合。
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

void ATacticalCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// ASC의 소유자와 실제 전투 수행자를 현재 캐릭터로 초기화한다.
	// ASCのOwnerとAvatarを現在のキャラクターで初期化する。
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
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
	return AbilitySystemComponent;
}
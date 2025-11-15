#pragma once
#include "SkinnedMeshComponent.h"
#include "USkeletalMeshComponent.generated.h"

UCLASS(DisplayName="스켈레탈 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()
    
    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override = default;

    void TickComponent(float DeltaTime) override;
    void SetSkeletalMesh(const FString& PathFileName) override;

// Editor Section
public:
    /**
     * @brief 특정 뼈의 부모 기준 로컬 트랜스폼을 설정
     * @param BoneIndex 수정할 뼈의 인덱스
     * @param NewLocalTransform 새로운 부모 기준 로컬 FTransform
     */
    void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform);

    void SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform);
    
    /**
     * @brief 특정 뼈의 현재 로컬 트랜스폼을 반환
     */
    FTransform GetBoneLocalTransform(int32 BoneIndex) const;
    
    /**
     * @brief 기즈모를 렌더링하기 위해 특정 뼈의 월드 트랜스폼을 계산
     */
    FTransform GetBoneWorldTransform(int32 BoneIndex);

    /**
     * @brief 애니메이션 뷰어에서 직접 포즈 배열에 접근하기 위한 getter
     */
    TArray<FTransform>& GetCurrentLocalSpacePose() { return CurrentLocalSpacePose; }

    /**
     * @brief CurrentLocalSpacePose의 변경사항을 ComponentSpace -> FinalMatrices 계산까지 모두 수행
     */
    void ForceRecomputePose();

    /**
     * @brief 현재 포즈를 BindPose로 초기화 (애니메이션 해제 시 사용)
     */
    void ResetToBindPose();

protected:

    /**
     * @brief CurrentLocalSpacePose를 기반으로 CurrentComponentSpacePose 채우기
     */
    void UpdateComponentSpaceTransforms();

    /**
     * @brief CurrentComponentSpacePose를 기반으로 TempFinalSkinningMatrices 채우기
     */
    void UpdateFinalSkinningMatrices();

protected:
    /**
     * @brief 각 뼈의 부모 기준 로컬 트랜스폼
     */
    TArray<FTransform> CurrentLocalSpacePose;

    /**
     * @brief LocalSpacePose로부터 계산된 컴포넌트 기준 트랜스폼
     */
    TArray<FTransform> CurrentComponentSpacePose;

    /**
     * @brief 부모에게 보낼 최종 스키닝 행렬 (임시 계산용)
     */
    TArray<FMatrix> TempFinalSkinningMatrices;
    /**
     * @brief CPU 스키닝에 전달할 최종 노말 스키닝 행렬
     */
    TArray<FMatrix> TempFinalSkinningNormalMatrices;


// ====================================
// Animation Playback (SingleNode Mode)
// ====================================
public:
    /**
     * @brief 애니메이션 재생 모드
     */
    enum class EAnimationMode : uint8
    {
        AnimationSingleNode,  ///< AnimInstance 없이 단일 애니메이션 재생
        AnimationBlueprint    ///< AnimInstance를 통한 복잡한 애니메이션 로직 (블렌딩, 스테이트 머신)
    };

    /**
     * @brief 애니메이션을 재생합니다 (편의 함수)
     * @param NewAnimToPlay 재생할 애니메이션 에셋
     * @param bLooping 루프 재생 여부
     */
    void PlayAnimation(class UAnimationAsset* NewAnimToPlay, bool bLooping = true);

    /**
     * @brief 현재 재생 중인 애니메이션을 정지하고 BindPose로 복원합니다
     */
    void StopAnimation();

    /**
     * @brief 애니메이션 모드를 설정합니다
     * @param InAnimationMode SingleNode 또는 AnimationBlueprint
     */
    void SetAnimationMode(EAnimationMode InAnimationMode);

    /**
     * @brief 재생할 애니메이션을 설정합니다 (시간 초기화)
     * @param NewAnimation 설정할 애니메이션 에셋
     */
    void SetAnimation(class UAnimationAsset* NewAnimation);

    /**
     * @brief 현재 설정된 애니메이션을 재생 시작합니다
     * @param bLooping 루프 재생 여부
     */
    void Play(bool bLooping = true);

private:
    /**
     * @brief 매 프레임 애니메이션 시간을 업데이트하고 본 포즈를 갱신합니다
     * @param DeltaTime 프레임 델타 타임
     */
    void UpdateAnimation(float DeltaTime);

    UAnimationAsset* CurrentAnimation = nullptr;        // 현재 재생 중인 애니메이션
    EAnimationMode AnimationMode = EAnimationMode::AnimationSingleNode;  // 현재 애니메이션 모드
    float CurrentAnimationTime = 0.0f;                  // 현재 애니메이션 재생 시간 (초)
    bool bIsPlaying = false;                            // 재생 중 여부
    bool bIsLooping = true;                             // 루프 재생 여부

// FOR TEST!!!
private:
    float TestTime = 0;
    bool bIsInitialized = false;
    FTransform TestBoneBasePose;
};

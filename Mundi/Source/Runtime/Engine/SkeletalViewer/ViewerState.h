#pragma once

class UWorld; class FViewport; class FViewportClient; class ASkeletalMeshActor; class USkeletalMesh; class UAnimationSequence;

class ViewerState
{
public:
    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;

    // Have a pointer to the currently selected mesh to render in the viewer
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    FString LoadedMeshPath;  // Track loaded mesh path for unloading
    int32 SelectedBoneIndex = -1;
    bool bShowMesh = true;
    bool bShowBones = true;
    // Bone line rebuild control
    bool bBoneLinesDirty = true;      // true면 본 라인 재구성
    int32 LastSelectedBoneIndex = -1; // 색상 갱신을 위한 이전 선택 인덱스
    // UI path buffer per-tab
    char MeshPathBuffer[260] = {0};
    std::set<int32> ExpandedBoneIndices;

    // 본 트랜스폼 편집 관련
    FVector EditBoneLocation;
    FVector EditBoneRotation;  // Euler angles in degrees
    FVector EditBoneScale;

    bool bBoneTransformChanged = false;
    bool bBoneRotationEditing = false;

    // 애니메이션 관련
    TArray<UAnimationSequence*> AvailableAnimations;  // 로드된 애니메이션 리스트
    TArray<FString> AnimationNames;                   // 애니메이션 이름 리스트
    int32 SelectedAnimIndex = -1;                     // 선택된 애니메이션 인덱스
    bool bIsAnimPlaying = false;                      // 재생 중인지 여부
    float CurrentAnimTime = 0.0f;                     // 현재 애니메이션 시간
    float AnimPlayRate = 1.0f;                        // 재생 속도 (1.0 = 정상)
    bool bAnimLoop = true;                            // 루프 재생 여부
};

#include "pch.h"
#include "SkeletalMeshActor.h"
#include "World.h"

ASkeletalMeshActor::ASkeletalMeshActor()
{
    ObjectName = "Skeletal Mesh Actor";

    // 스킨드 메시 렌더용 컴포넌트 생성 및 루트로 설정
    // - 프리뷰 장면에서 메시를 표시하는 실제 렌더링 컴포넌트
    SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
    RootComponent = SkeletalMeshComponent;
}

ASkeletalMeshActor::~ASkeletalMeshActor() = default;

void ASkeletalMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

FAABB ASkeletalMeshActor::GetBounds() const
{
    // Be robust to component replacement: query current root
    if (auto* Current = Cast<USkeletalMeshComponent>(RootComponent))
    {
        return Current->GetWorldAABB();
    }
    return FAABB();
}

void ASkeletalMeshActor::SetSkeletalMeshComponent(USkeletalMeshComponent* InComp)
{
    SkeletalMeshComponent = InComp;
}

void ASkeletalMeshActor::SetSkeletalMesh(const FString& PathFileName)
{
    if (SkeletalMeshComponent)
    {
        SkeletalMeshComponent->SetSkeletalMesh(PathFileName);
    }
}

void ASkeletalMeshActor::EnsureViewerComponents()
{
    // Only create viewer components if they don't exist and we're in a preview world
    if (BoneLineComponent && BoneAnchor)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || !World->IsPreviewWorld())
    {
        return;
    }

    // Create bone line component for skeleton visualization
    if (!BoneLineComponent)
    {
        BoneLineComponent = NewObject<ULineComponent>();
        if (BoneLineComponent && RootComponent)
        {
            BoneLineComponent->ObjectName = "BoneLines";
            BoneLineComponent->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
            BoneLineComponent->SetAlwaysOnTop(true);
            AddOwnedComponent(BoneLineComponent);
            BoneLineComponent->RegisterComponent(World);
        }
    }

    // Create bone anchor for gizmo placement
    if (!BoneAnchor)
    {
        BoneAnchor = NewObject<UBoneAnchorComponent>();
        if (BoneAnchor && RootComponent)
        {
            BoneAnchor->ObjectName = "BoneAnchor";
            BoneAnchor->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
            BoneAnchor->SetVisibility(false);
            AddOwnedComponent(BoneAnchor);
            BoneAnchor->RegisterComponent(World);
        }
    }
}

void ASkeletalMeshActor::RebuildBoneLines(int32 SelectedBoneIndex)
{
    // Ensure viewer components exist before using them
    EnsureViewerComponents();

    if (!BoneLineComponent || !SkeletalMeshComponent)
    {
        return;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }

    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneCount <= 0)
    {
        return;
    }

    // Initialize cache once per mesh
    if (!bBoneLinesInitialized || BoneLinesCache.Num() != BoneCount)
    {
        BoneLineComponent->ClearLines();
        BuildBoneLinesCache();
        bBoneLinesInitialized = true;
        CachedSelected = -1;
    }

    // Update selection highlight only when changed
    if (CachedSelected != SelectedBoneIndex)
    {
        UpdateBoneSelectionHighlight(SelectedBoneIndex);
        CachedSelected = SelectedBoneIndex;
    }

    // Update transforms only for the selected bone subtree
    if (SelectedBoneIndex >= 0 && SelectedBoneIndex < BoneCount)
    {
        UpdateBoneSubtreeTransforms(SelectedBoneIndex);
    }
    else if (BoneCount > 0)
    {
        // 본이 선택 안 되어있을 때는 루트부터 전체 업데이트 (애니메이션 재생 시)
        UpdateBoneSubtreeTransforms(0);
    }
}

void ASkeletalMeshActor::RepositionAnchorToBone(int32 BoneIndex)
{
    // Ensure viewer components exist before using them
    EnsureViewerComponents();

    if (!SkeletalMeshComponent || !BoneAnchor)
    {
        return;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }

    const auto& Bones = Data->Skeleton.Bones;
    if (BoneIndex < 0 || BoneIndex >= (int32)Bones.size())
    {
        return;
    }

    // Wire target/index first, then place anchor without writeback
    BoneAnchor->SetTarget(SkeletalMeshComponent, BoneIndex);

    BoneAnchor->SetEditability(true);
    BoneAnchor->SetVisibility(true);
}

void ASkeletalMeshActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // Find skeletal mesh component (always exists)
    for (UActorComponent* Component : OwnedComponents)
    {
        if (auto* Comp = Cast<USkeletalMeshComponent>(Component))
        {
            SkeletalMeshComponent = Comp;
            break;
        }
    }

    // Find viewer components (may not exist if not in preview world)
    for (UActorComponent* Component : OwnedComponents)
    {
        if (auto* Comp = Cast<ULineComponent>(Component))
        {
            BoneLineComponent = Comp;
        }
        else if (auto* Comp = Cast<UBoneAnchorComponent>(Component))
        {
            BoneAnchor = Comp;
        }
    }
}

void ASkeletalMeshActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RootComponent);
    }
}

void ASkeletalMeshActor::BuildBoneLinesCache()
{
    if (!SkeletalMeshComponent || !BoneLineComponent)
    {
        return;
    }
    
    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }
    
    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }

    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());

    BoneLinesCache.Empty();
    BoneLinesCache.resize(BoneCount);
    BoneChildren.Empty();
    BoneChildren.resize(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const int32 p = Bones[i].ParentIndex;
        if (p >= 0 && p < BoneCount)
        {
            BoneChildren[p].Add(i);
        }
    }

    // Initial centers from current bone transforms (object space)
    // Use GetBoneWorldTransform to properly accumulate parent transforms
    const FMatrix WorldInv = GetWorldMatrix().InverseAffine();
    TArray<FVector> JointPos;
    JointPos.resize(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const FMatrix W = SkeletalMeshComponent->GetBoneWorldTransform(i).ToMatrix();
        const FMatrix O = W * WorldInv;
        JointPos[i] = FVector(O.M[3][0], O.M[3][1], O.M[3][2]);
    }

    const int NumSegments = CachedSegments;
    
    for (int32 i = 0; i < BoneCount; ++i)
    {
        FBoneDebugLines& BL = BoneLinesCache[i];
        const FVector Center = JointPos[i];
        const int32 parent = Bones[i].ParentIndex;
        
        if (parent >= 0)
        {
            const FVector ParentPos = JointPos[parent];
            const FVector ChildPos = Center;
            const FVector BoneDir = (ChildPos - ParentPos).GetNormalized();

            // Calculate perpendicular vectors for the cone base
            // Find a vector not parallel to BoneDir
            FVector Up = FVector(0, 0, 1);
            if (std::abs(FVector::Dot(Up, BoneDir)) > 0.99f)
            {
                Up = FVector(0, 1, 0); // Use Y if bone is too aligned with Z
            }

            FVector Right = FVector::Cross(BoneDir, Up).GetNormalized();
            FVector Forward = FVector::Cross(Right, BoneDir).GetNormalized();

            // Scale cone radius based on bone length
            const float BoneLength = (ChildPos - ParentPos).Size();
            const float Radius = std::min(BoneBaseRadius, BoneLength * 0.15f);

            // Create cone geometry
            BL.ConeEdges.reserve(NumSegments);
            BL.ConeBase.reserve(NumSegments);

            for (int k = 0; k < NumSegments; ++k)
            {
                const float angle0 = (static_cast<float>(k) / NumSegments) * TWO_PI;
                const float angle1 = (static_cast<float>((k + 1) % NumSegments) / NumSegments) * TWO_PI;

                // Base circle vertices at parent
                const FVector BaseVertex0 = ParentPos + Right * (Radius * std::cos(angle0)) + Forward * (Radius * std::sin(angle0));
                const FVector BaseVertex1 = ParentPos + Right * (Radius * std::cos(angle1)) + Forward * (Radius * std::sin(angle1));

                // Cone edge from base vertex to tip (child)
                BL.ConeEdges.Add(BoneLineComponent->AddLine(BaseVertex0, ChildPos, FVector4(0, 1, 0, 1)));

                // Base circle edge
                BL.ConeBase.Add(BoneLineComponent->AddLine(BaseVertex0, BaseVertex1, FVector4(0, 1, 0, 1)));
            }
        }

        // Joint sphere visualization (3 orthogonal rings)
        BL.Rings.reserve(NumSegments*3);

        for (int k = 0; k < NumSegments; ++k)
        {
            const float a0 = (static_cast<float>(k) / NumSegments) * TWO_PI;
            const float a1 = (static_cast<float>((k + 1) % NumSegments) / NumSegments) * TWO_PI;
            BL.Rings.Add(BoneLineComponent->AddLine(
                Center + FVector(BoneJointRadius * std::cos(a0), BoneJointRadius * std::sin(a0), 0.0f),
                Center + FVector(BoneJointRadius * std::cos(a1), BoneJointRadius * std::sin(a1), 0.0f),
                FVector4(0.8f,0.8f,0.8f,1.0f)));
            BL.Rings.Add(BoneLineComponent->AddLine(
                Center + FVector(BoneJointRadius * std::cos(a0), 0.0f, BoneJointRadius * std::sin(a0)),
                Center + FVector(BoneJointRadius * std::cos(a1), 0.0f, BoneJointRadius * std::sin(a1)),
                FVector4(0.8f,0.8f,0.8f,1.0f)));
            BL.Rings.Add(BoneLineComponent->AddLine(
                Center + FVector(0.0f, BoneJointRadius * std::cos(a0), BoneJointRadius * std::sin(a0)),
                Center + FVector(0.0f, BoneJointRadius * std::cos(a1), BoneJointRadius * std::sin(a1)),
                FVector4(0.8f,0.8f,0.8f,1.0f)));
        }
    }
}

void ASkeletalMeshActor::UpdateBoneSelectionHighlight(int32 SelectedBoneIndex)
{
    if (!SkeletalMeshComponent)
    {
        return;
    }
    
    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }
    
    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }
    
    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());

    const FVector4 SelRing(1.0f, 0.85f, 0.2f, 1.0f);
    const FVector4 NormalRing(0.8f, 0.8f, 0.8f, 1.0f);
    const FVector4 SelCone(1.0f, 0.0f, 0.0f, 1.0f);      // Red for selected bone cone
    const FVector4 NormalCone(0.0f, 1.0f, 0.0f, 1.0f);   // Green for normal bone cone

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const bool bSelected = (i == SelectedBoneIndex);
        const FVector4 RingColor = bSelected ? SelRing : NormalRing;
        FBoneDebugLines& BL = BoneLinesCache[i];

        // Update joint ring colors
        for (ULine* L : BL.Rings)
        {
            if (L) L->SetColor(RingColor);
        }

        // Update cone colors
        const int32 parent = Bones[i].ParentIndex;
        const bool bConeSelected = (i == SelectedBoneIndex || parent == SelectedBoneIndex);
        const FVector4 ConeColor = bConeSelected ? SelCone : NormalCone;

        for (ULine* L : BL.ConeEdges)
        {
            if (L) L->SetColor(ConeColor);
        }

        for (ULine* L : BL.ConeBase)
        {
            if (L) L->SetColor(ConeColor);
        }
    }
}

void ASkeletalMeshActor::UpdateBoneSubtreeTransforms(int32 BoneIndex)
{
    if (!SkeletalMeshComponent)
    {
        return;
    }
    
    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return;
    }
    
    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return;
    }
    
    const auto& Bones = Data->Skeleton.Bones;
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneIndex < 0 || BoneIndex >= BoneCount)
    {
        return;
    }

    TArray<int32> Stack; Stack.Add(BoneIndex);
    TArray<int32> ToUpdate;
    while (!Stack.IsEmpty())
    {
        int32 b = Stack.Last(); Stack.Pop();
        ToUpdate.Add(b);
        for (int32 c : BoneChildren[b]) Stack.Add(c);
    }

    const FMatrix WorldInv = GetWorldMatrix().InverseAffine();
    TArray<FVector> Centers; Centers.resize(BoneCount);
    
    for (int32 b : ToUpdate)
    {
        const FMatrix W = SkeletalMeshComponent->GetBoneWorldTransform(b).ToMatrix();
        const FMatrix O = W * WorldInv;
        Centers[b] = FVector(O.M[3][0], O.M[3][1], O.M[3][2]);
    }

    const int NumSegments = CachedSegments;

    for (int32 b : ToUpdate)
    {
        FBoneDebugLines& BL = BoneLinesCache[b];
        const int32 parent = Bones[b].ParentIndex;

        // Update cone geometry
        if (parent >= 0 && !BL.ConeEdges.IsEmpty())
        {
            const FMatrix ParentW = SkeletalMeshComponent->GetBoneWorldTransform(parent).ToMatrix();
            const FMatrix ParentO = ParentW * WorldInv;
            const FVector ParentPos(ParentO.M[3][0], ParentO.M[3][1], ParentO.M[3][2]);
            const FVector ChildPos = Centers[b];

            const FVector BoneDir = (ChildPos - ParentPos).GetNormalized();

            // Calculate perpendicular vectors for the cone base
            FVector Up = FVector(0, 0, 1);
            if (std::abs(FVector::Dot(Up, BoneDir)) > 0.99f)
            {
                Up = FVector(0, 1, 0);
            }

            FVector Right = FVector::Cross(BoneDir, Up).GetNormalized();
            FVector Forward = FVector::Cross(Right, BoneDir).GetNormalized();

            // Scale cone radius based on bone length
            const float BoneLength = (ChildPos - ParentPos).Size();
            const float Radius = std::min(BoneBaseRadius, BoneLength * 0.15f);

            // Update cone lines
            for (int k = 0; k < NumSegments && k < BL.ConeEdges.Num(); ++k)
            {
                const float angle0 = (static_cast<float>(k) / NumSegments) * TWO_PI;
                const float angle1 = (static_cast<float>((k + 1) % NumSegments) / NumSegments) * TWO_PI;

                const FVector BaseVertex0 = ParentPos + Right * (Radius * std::cos(angle0)) + Forward * (Radius * std::sin(angle0));
                const FVector BaseVertex1 = ParentPos + Right * (Radius * std::cos(angle1)) + Forward * (Radius * std::sin(angle1));

                // Update cone edge
                if (BL.ConeEdges[k])
                {
                    BL.ConeEdges[k]->SetLine(BaseVertex0, ChildPos);
                }

                // Update base circle edge
                if (k < BL.ConeBase.Num() && BL.ConeBase[k])
                {
                    BL.ConeBase[k]->SetLine(BaseVertex0, BaseVertex1);
                }
            }
        }

        // Update joint rings
        const FVector Center = Centers[b];

        for (int k = 0; k < NumSegments; ++k)
        {
            const float a0 = (static_cast<float>(k) / NumSegments) * TWO_PI;
            const float a1 = (static_cast<float>((k + 1) % NumSegments) / NumSegments) * TWO_PI;
            const int base = k * 3;
            if (BL.Rings.IsEmpty() || base + 2 >= BL.Rings.Num()) break;
            BL.Rings[base+0]->SetLine(
                Center + FVector(BoneJointRadius * std::cos(a0), BoneJointRadius * std::sin(a0), 0.0f),
                Center + FVector(BoneJointRadius * std::cos(a1), BoneJointRadius * std::sin(a1), 0.0f));
            BL.Rings[base+1]->SetLine(
                Center + FVector(BoneJointRadius * std::cos(a0), 0.0f, BoneJointRadius * std::sin(a0)),
                Center + FVector(BoneJointRadius * std::cos(a1), 0.0f, BoneJointRadius * std::sin(a1)));
            BL.Rings[base+2]->SetLine(
                Center + FVector(0.0f, BoneJointRadius * std::cos(a0), BoneJointRadius * std::sin(a0)),
                Center + FVector(0.0f, BoneJointRadius * std::cos(a1), BoneJointRadius * std::sin(a1)));
        }
    }
}

int32 ASkeletalMeshActor::PickBone(const FRay& Ray, float& OutDistance) const
{
    if (!SkeletalMeshComponent)
    {
        return -1;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
    if (!SkeletalMesh)
    {
        return -1;
    }

    const FSkeletalMeshData* Data = SkeletalMesh->GetSkeletalMeshData();
    if (!Data)
    {
        return -1;
    }

    const auto& Bones = Data->Skeleton.Bones;
    if (Bones.empty())
    {
        return -1;
    }

    int32 ClosestBoneIndex = -1;
    float ClosestDistance = FLT_MAX;

    // Test each bone with a bounding sphere
    for (int32 i = 0; i < (int32)Bones.size(); ++i)
    {
        // Get bone world transform
        FTransform BoneWorldTransform = SkeletalMeshComponent->GetBoneWorldTransform(i);
        FVector BoneWorldPos = BoneWorldTransform.Translation;

        // Use BoneJointRadius as picking radius (can be adjusted)
        float PickRadius = BoneJointRadius * 2.0f; // Slightly larger for easier picking

        // Test ray-sphere intersection
        float HitDistance;
        if (IntersectRaySphere(Ray, BoneWorldPos, PickRadius, HitDistance))
        {
            if (HitDistance < ClosestDistance)
            {
                ClosestDistance = HitDistance;
                ClosestBoneIndex = i;
            }
        }
    }

    if (ClosestBoneIndex >= 0)
    {
        OutDistance = ClosestDistance;
    }

    return ClosestBoneIndex;
}

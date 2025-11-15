#pragma once
#include "SWindow.h"
#include "Source/Runtime/Engine/SkeletalViewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;

class SSkeletalMeshViewerWindow : public SWindow
{
public:
    SSkeletalMeshViewerWindow();
    virtual ~SSkeletalMeshViewerWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

    // SWindow overrides
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    void OnRenderViewport();

    // Accessors (active tab)
    FViewport* GetViewport() const { return ActiveState ? ActiveState->Viewport : nullptr; }
    FViewportClient* GetViewportClient() const { return ActiveState ? ActiveState->Client : nullptr; }

    // Load a skeletal mesh into the active tab
    void LoadSkeletalMesh(const FString& Path);

private:
    // Tabs
    void OpenNewTab(const char* Name = "Viewer");
    void CloseTab(int Index);

private:
    // Per-tab state
    ViewerState* ActiveState = nullptr;
    TArray<ViewerState*> Tabs;
    int ActiveTabIndex = -1;

    // For legacy single-state flows; removed once tabs are stable
    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;

    // Layout state
    float LeftPanelRatio = 0.25f;   // 25% of width
    float RightPanelRatio = 0.25f;  // 25% of width

    // Cached center region used for viewport sizing and input mapping
    FRect CenterRect;

    // Whether we've applied the initial ImGui window placement
    bool bInitialPlacementDone = false;

    // Request focus on first open
    bool bRequestFocus = false;

    // Window open state
    bool bIsOpen = true;

public:
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }

private:
    void UpdateBoneTransformFromSkeleton(ViewerState* State);
    void ApplyBoneTransform(ViewerState* State);

    void ExpandToSelectedBone(ViewerState* State, int32 BoneIndex);

    // Animation helpers
    void FindAnimationsForMesh(ViewerState* State, const FString& MeshPath);
    void RenderAnimationSection(ViewerState* State);
    void UpdateAnimationPlayback(ViewerState* State, float DeltaTime);
};

#pragma once

#include <framegen/IFGFeature.h>

#include <proxies/Streamline_Proxy.h>
#include "DLSSG_Status.h"

// The Streamline side of the DLSSG output, shared by the D3D12 and Vulkan ones
class DLSSG_Common : public virtual IFGFeature
{
  protected:
    sl::ViewportHandle _viewport { 0 };
    sl::FrameToken* _frameToken = nullptr;
    DLSSGStatus::Monitor _runtimeStatus;
    bool _optionsApplied = false;

    void UpdateRuntimeStatus(bool paused);

    // Max interpolation count and DMFG support, once the swapchain exists
    void QueryState();

    // DLSSG and Reflex options for the next present, Reflex optimizes with the game's markers when it has them
    void SetOptions(bool gameMarkersReachStreamline);

    // Fills _frameToken and sends the frame's constants, pauses FG when Streamline refuses them
    bool SetConstants(int fIndex, uint32_t frameId, uint32_t mvWidth, uint32_t mvHeight);

    void PauseAfterError();

  public:
    // IFGFeature
    const char* Name() override final { return Sm86Bridge::IsSelected() ? "DLSSG-SM86" : "DLSSG"; }
    DLSSGStatus::Snapshot RuntimeStatus() const { return _runtimeStatus.Read(); }
    feature_version Version() override final;
    void Activate() override final;
    void Deactivate() override final;
    bool SetInterpolatedFrameCount(UINT interpolatedFrameCount) override final { return true; }
};

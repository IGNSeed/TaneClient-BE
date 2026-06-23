#pragma once

#include <cstddef>
#include <cstdint>

namespace tane::offsets::item_hud {

inline constexpr std::uintptr_t kGetArmorItemStackRva = 0x01675670;
inline constexpr std::uintptr_t kScreenViewSetupAndRenderRva = 0x004B0D90;
inline constexpr std::uintptr_t kRenderDecoratedGuiItemRva = 0x04D94CB0;
inline constexpr std::uintptr_t kItemRendererRenderGuiItemNewRva = 0x04DC4370;
inline constexpr std::uintptr_t kBaseActorRenderContextCtorRva = 0x01A7CB20;
inline constexpr std::uintptr_t kBaseActorRenderContextDtorRva = 0x01A7CE50;
inline constexpr std::uintptr_t kBaseActorRenderContextGetItemRendererRva = 0x002C8490;
inline constexpr std::uintptr_t kItemStackIsEnchantedRva = 0x01E053B0;
inline constexpr std::uintptr_t kItemStackGetDamageValueRva = 0x01DFD8D0;
inline constexpr std::uintptr_t kItemStackGetDurabilityItemRva = 0x01E237E0;
inline constexpr std::uintptr_t kGetDurabilityComponentKeyRva = 0x03CA7320;
inline constexpr std::uintptr_t kMobEffectsRendererRenderRva = 0x0799F370;

constexpr std::size_t kMinecraftUIRenderContextClientInstanceOffset = 0x08;
constexpr std::size_t kMinecraftUIRenderContextScreenContextOffset = 0x10;
constexpr std::size_t kScreenViewVisualTreeOffset = 0x48;
constexpr std::size_t kVisualTreeRootOffset = 0x08;
constexpr std::size_t kUIControlLayerNameOffsetCandidates[] = {0x18, 0x20};
constexpr std::size_t kClientInstanceMinecraftGameOffset = 0xC8;
constexpr std::size_t kClientInstanceMinecraftGameVtableOffset = 0x280;
constexpr std::size_t kBaseActorRenderContextItemRendererOffset = 0x58;
constexpr std::size_t kClientInstanceGuiItemContextVtableOffset = 0xF8;
constexpr std::size_t kItemResolveGuiRenderModeVtableOffset = 0x3B8;
constexpr std::size_t kClientInstanceGuiDataOffset = 0x560;
constexpr std::size_t kGuiDataScreenSizeOffset = 0x30;
constexpr std::size_t kGuiDataScaledSizeOffset = 0x40;
constexpr std::size_t kClientInstanceGuiDataOffsetCandidates[] = {
    kClientInstanceGuiDataOffset,
    0x548,
    0x550,
    0x558,
    0x568,
    0x570,
    0x578,
    0x580,
    0x588,
    0x590,
    0x5B0,
    0x5B8,
    0x648,
};
constexpr std::size_t kGuiDataScaledSizeOffsetCandidates[] = {
    kGuiDataScaledSizeOffset,
    0x28,
    0x30,
    0x38,
    0x48,
    0x50,
};
constexpr std::size_t kClientInstanceLocalPlayerVtableOffset = 0xF8;
constexpr std::size_t kGetCarriedItemVtableOffset = 0x268;
constexpr std::size_t kActorLevelOffset = 0x1D8;
constexpr std::size_t kLevelItemRegistryRefOffset = 0x198;
constexpr std::size_t kItemRegistryNameToItemMapOffset = 0x88;
constexpr std::size_t kItemRegistryTileItemNameToItemMapOffset = 0x108;

constexpr std::size_t kLocalPlayerVtableCandidates[] = {
    kClientInstanceLocalPlayerVtableOffset,
};

constexpr std::size_t kPlayerInventoryCandidates[] = {
    0x5B8,
    0x7C0,
    0x7E8,
    0x7F0,
    0x7B0,
    0x788,
    0x760,
};
constexpr std::size_t kPlayerInventorySelectedSlotOffset = 0x10;
constexpr std::size_t kPlayerInventoryInventoryOffset = 0xB8;
constexpr std::size_t kInventoryGetItemVtableOffset = 7 * sizeof(void*);
constexpr std::size_t kInventorySetItemVtableOffset = 11 * sizeof(void*);
constexpr int kPlayerInventoryMainSlotCount = 36;
constexpr std::size_t kItemGetComponentVtableOffset = 0xC0;
constexpr std::size_t kItemLegacyMaxDamageVtableOffset = 0x120;
constexpr std::size_t kDurabilityComponentMaxDamageOffset = 0x10;

constexpr std::size_t kItemPointerOffset = 0x08;
constexpr std::size_t kItemAuxValueOffset = 0x20;
constexpr std::size_t kItemCountOffset = 0x22;
constexpr std::size_t kItemValidOffset = 0x23;
constexpr std::size_t kItemRenderEntrySize = 0xC0;
constexpr std::size_t kItemRenderEntryGlintFlagOffset = 0xBC;
constexpr std::size_t kItemNameOffsetCandidates[] = {0xD0, 0xD8};
constexpr std::size_t kMinecraftUiRenderContextDrawDebugTextVtableOffset = 4 * sizeof(void*);
constexpr std::size_t kMinecraftUiRenderContextFlushTextVtableOffset = 6 * sizeof(void*);
constexpr std::size_t kMinecraftUiRenderContextFillRectangleVtableOffset = 15 * sizeof(void*);
constexpr std::uint32_t kMobEffectsComponentHash = 0xE6A1B550;
constexpr std::size_t kActorEntityContextOffset = 0x10;
constexpr std::size_t kActorEntityIdOffset = 0x18;
constexpr std::size_t kMobEffectsComponentEntrySize = 0x18;
constexpr std::size_t kMobEffectsBeginOffset = 0x00;
constexpr std::size_t kMobEffectsEndOffset = 0x08;
constexpr std::size_t kMobEffectInstanceSize = 0x88;
constexpr std::size_t kMobEffectIdOffset = 0x00;
constexpr std::size_t kMobEffectDurationOffset = 0x04;
constexpr std::size_t kMobEffectAmplifierOffset = 0x20;
constexpr std::size_t kMobEffectShowIconOffset = 0x27;

constexpr float kHudX = 4.0f;
constexpr float kItemSize = 16.0f;
constexpr float kItemSpacing = 20.0f;
constexpr float kNativeCountTextSize = 0.8f;
constexpr float kDurabilityTextXOffset = kItemSize + 6.0f;
constexpr float kDurabilityTextYOffset = 3.0f;
constexpr float kDurabilityTextWidth = 96.0f;
constexpr float kNativeDurabilityTextSize = 0.65f;
constexpr int kItemSlotCount = 5;
constexpr std::uint64_t kHudCacheRefreshMs = 750;

}  // namespace tane::offsets::item_hud

namespace tane::offsets::tab {

constexpr std::size_t kClientInstanceLocalPlayerVtableOffset = 0xF8;
constexpr std::size_t kActorLevelOffset = 0x1D8;
constexpr std::size_t kLevelGetPlayerListVtableOffset = 0x9E8;
constexpr std::size_t kLevelPlayerListMapOffset = 0x4E0;
constexpr std::size_t kPlayerListEntryNameOffset = 0x18;
constexpr std::size_t kPlayerListEntrySkinOffset = 0x80;
constexpr std::size_t kPlayerSkinSkinImageOffset = 0xA0;

}  // namespace tane::offsets::tab

namespace tane::offsets::ping {

inline constexpr std::uintptr_t kRakPeerGetAveragePingRva = 0x01427EB0;
inline constexpr std::uintptr_t kRakPeerGetLastPingRva = 0x01428060;

constexpr std::size_t kClientInstancePacketSenderOffsetCandidates[] = {0x1C8, 0x1D0, 0x1B8, 0x1A8};
constexpr std::size_t kPacketSenderNetworkSystemOffset = 0x20;
constexpr std::size_t kNetworkSystemRemoteConnectorCompositeOffsetCandidates[] = {0xF0, 0x90, 0x80, 0x60};
constexpr std::size_t kRemoteConnectorCompositeNetherNetConnectorOffset = 0x68;
constexpr std::size_t kNetherNetConnectorPeersOffset = 0x1F0;
constexpr std::size_t kWebRtcNetworkPeerCurrentPingOffset = 0x54;
constexpr std::size_t kWebRtcNetworkPeerAveragePingOffset = 0x58;
constexpr std::size_t kWebRtcNetworkPeerStatusProviderOffset = 0x118;
constexpr std::size_t kNetworkStatusProviderGetStatusVtableOffset = 0x10;
constexpr std::size_t kNetworkStatusCurrentPingOffset = 0x08;
constexpr std::size_t kNetworkStatusAveragePingOffset = 0x10;

}  // namespace tane::offsets::ping

namespace tane::offsets::gui {

inline constexpr std::uintptr_t kDebugFpsValueRva = 0x0DCF37D0;

}  // namespace tane::offsets::gui

namespace tane::offsets::movement {

inline constexpr std::uintptr_t kMoveInputStateSetFlagRva = 0x04D563F0;
inline constexpr std::uintptr_t kMoveInputComponentSetFlagRva = 0x02A9A510;

constexpr std::uint32_t kMoveInputComponentHash = 0x018B1887;
constexpr std::uint32_t kRawMoveInputComponentHash = 0x3613E513;
constexpr std::size_t kClientInstanceLocalPlayerVtableOffset = 0xF8;
constexpr std::size_t kActorEntityContextOffset = 0x10;
constexpr std::size_t kActorEntityIdOffset = 0x18;
constexpr std::size_t kActorLevelOffset = 0x1D8;
constexpr std::size_t kMoveInputStateSize = 0x10;
constexpr std::size_t kMoveInputTransientOffset = 0x20;
constexpr std::size_t kMoveInputTransientSize = 0x34;
constexpr std::size_t kMoveInputComponentEntrySize = 0x64;
constexpr std::size_t kRawMoveInputComponentEntrySize = 0x18;

}  // namespace tane::offsets::movement

namespace tane::offsets::input {

inline constexpr std::uintptr_t kProcessAnalogInputStateRva = 0x00178B0;
inline constexpr std::uintptr_t kInputServiceRva = 0x0DCE9C30;
inline constexpr std::uintptr_t kButtonPressDispatchRva = 0x00298FF0;
inline constexpr std::uintptr_t kButtonTransitionDispatchRva = 0x002990A0;
inline constexpr std::uintptr_t kButtonStateDispatchRva = 0x002999A0;
inline constexpr std::uintptr_t kButtonAnalogDispatchRva = 0x00299150;
inline constexpr std::uintptr_t kAnalogVectorDispatchRva = 0x00299220;
inline constexpr std::uintptr_t kAnalogDispatchRva = 0x00299300;
inline constexpr std::uintptr_t kPointerMotionDispatchRva = 0x002AD400;
inline constexpr std::uintptr_t kPointerButtonDispatchRva = 0x002AD490;
inline constexpr std::uintptr_t kPointerCursorPositionRva = 0x0DCED10C;
inline constexpr std::uintptr_t kUiNavigationDispatchRva = 0x002AE980;

constexpr int kInputHandlerTickCallRipOffset = 1;
constexpr int kMouseInputVectorRipOffset = 3;
constexpr int kMouseDeviceRipOffset = 3;
constexpr std::size_t kMouseDeviceInputsOffset = 0x18;
constexpr std::size_t kMouseInputPacketTypeOffset = 0x08;
constexpr std::size_t kMouseInputPacketStateOffset = 0x09;
constexpr int kMouseButtonLeft = 1;
constexpr int kMouseButtonRight = 2;
constexpr int kMouseButtonPress = 1;
constexpr int kMouseButtonRelease = 0;

constexpr int kKeyAttackInputId = 0x01;
constexpr int kKeyUseInputId = 0x03;
constexpr int kButtonBuildOrInteractInputId = 0x82;
constexpr int kButtonDestroyAttackInputId = 0x83;
constexpr int kButtonBuildOrAttackInputId = 0x8C;
constexpr int kButtonDestroyInteractInputId = 0x8D;

}  // namespace tane::offsets::input

namespace tane::offsets::render {

inline constexpr std::uintptr_t kNameTagThirdPersonSelfSkipPatchRva = 0x03AE52B7;
constexpr std::size_t kNameTagShortPatchSize = 2;
inline constexpr std::uintptr_t kOptionsGetGammaRva = 0x014C6AB0;
inline constexpr std::uintptr_t kOptionsSetDevRenderBoundingBoxRva = 0x014C78A0;
inline constexpr std::uintptr_t kOptionsGetDevRenderBoundingBoxRva = 0x014C7910;
inline constexpr std::uintptr_t kBoolOptionSetValueRva = 0x008D7BA0;
inline constexpr std::uintptr_t kLevelRendererRenderLevelRva = 0x03AB2260;
inline constexpr std::uintptr_t kLevelRendererPlayerFogRenderUpdateRva = 0x03ADFA30;
inline constexpr std::uintptr_t kVolumetricFogRenderRva = 0x0181D900;
inline constexpr std::uintptr_t kTessellatorBeginRva = 0x03273FF0;
inline constexpr std::uintptr_t kTessellatorColorRva = 0x03274600;
inline constexpr std::uintptr_t kTessellatorVertexRva = 0x032748F0;
inline constexpr std::uintptr_t kMeshHelpersRenderMeshImmediatelyRva = 0x021B6EE0;
inline constexpr std::uintptr_t kHashedStringCtorRva = 0x00655EA0;
inline constexpr std::uintptr_t kRenderMaterialGroupCommonRva = 0x0DD8FD10;
constexpr float kFullbrightGamma = 25.0f;
constexpr std::size_t kClientInstanceOptionsOffset = 0xC38;
constexpr std::size_t kClientInstanceLocalPlayerVtableOffset = 0xF8;
constexpr std::size_t kOptionsGetOptionVtableOffset = 0x08;
constexpr std::size_t kBoolOptionValueOffset = 0x10;
constexpr std::size_t kLevelGetRuntimeActorListVtableOffset = 0xA18;
constexpr std::size_t kLevelRendererPlayerOffset = 0x478;
constexpr std::size_t kLevelRendererPlayerOriginOffset = 0x710;
constexpr std::size_t kScreenContextShaderColorOffset = 0x30;
constexpr std::size_t kScreenContextTessellatorOffset = 0xB8;
constexpr std::size_t kRenderMaterialGroupCreateMaterialVtableOffset = 0x08;
constexpr std::size_t kActorEntityContextOffset = 0x10;
constexpr std::size_t kActorEntityIdOffset = 0x18;
constexpr std::size_t kActorLevelOffset = 0x1D8;
constexpr std::uint32_t kPlayerComponentHash = 0xF95D258F;
constexpr std::uint32_t kAABBShapeComponentHash = 0xBAC1B3CF;
constexpr std::uint32_t kStateVectorComponentHash = 0x1B5D5238;
constexpr std::uint32_t kRenderPositionComponentHash = 0xE53C7221;
constexpr std::uint32_t kActorRotationComponentHash = 0x75DF36B7;
constexpr std::size_t kPlayerComponentEntrySize = 0x01;
constexpr std::size_t kAABBShapeComponentEntrySize = 0x20;
constexpr std::size_t kStateVectorComponentEntrySize = 0x24;
constexpr std::size_t kRenderPositionComponentEntrySize = 0x0C;
constexpr std::size_t kActorRotationComponentEntrySize = 0x10;
constexpr std::size_t kActorRotationPitchOffset = 0x00;
constexpr std::size_t kActorRotationYawOffset = 0x04;
constexpr int kDevRenderBoundingBoxOptionId = 0x129;

}  // namespace tane::offsets::render

namespace tane::offsets::camera {

inline constexpr std::uintptr_t kCameraProjectionFovOptionRva = 0x03A62410;
inline constexpr std::uintptr_t kCameraDefaultFovApplyRva = 0x07273060;
inline constexpr std::uintptr_t kCameraCustomFovApplyRva = 0x07273070;
inline constexpr std::uintptr_t kFreeLookCameraYawPatchRva = 0x0157B70A;
inline constexpr std::uintptr_t kFreeLookCameraYaw2PatchRva = 0x05AA08E7;
inline constexpr std::uintptr_t kFreeLookUpdatePlayerFromCameraRva = 0x05627580;
constexpr std::size_t kCameraOutputFovOffset = 0x50;
constexpr std::size_t kCameraComponentLookAnglesOffset = 0x30;
constexpr std::size_t kClientInstanceOptionsOffset = 0xC38;
constexpr std::size_t kClientInstanceLocalPlayerVtableOffset = 0xF8;
constexpr std::size_t kActorEntityContextOffset = 0x10;
constexpr std::size_t kActorEntityIdOffset = 0x18;
constexpr std::uint32_t kActorRotationComponentHash = 0x75DF36B7;
constexpr std::size_t kActorRotationComponentEntrySize = 0x10;
constexpr std::size_t kActorRotationPitchOffset = 0x00;
constexpr std::size_t kActorRotationYawOffset = 0x04;
constexpr std::size_t kActorRotationPrevPitchOffset = 0x08;
constexpr std::size_t kActorRotationPrevYawOffset = 0x0C;
constexpr std::size_t kOptionsSetPerspectiveVtableIndex = 123;
constexpr std::size_t kOptionsGetPerspectiveVtableIndex = 124;
constexpr float kMinZoomFovRadians = 0.02f;
constexpr float kMaxZoomFovRadians = 3.20f;

}  // namespace tane::offsets::camera

namespace tane::offsets::patch {

// v26.21: direct per-tab OreUI settings gate used by navigateToOptionsScreen.
inline constexpr std::uintptr_t kIsOreUiSettingsTabEnabledRva = 0x03A451A0;
// v26.21: populates the ScreenTechStack OreUI route registry.
inline constexpr std::uintptr_t kInitializeOreUiConfigRegistryRva = 0x0306E160;
// v26.21: Play ScreenTechStackSelector predicate. False disables OreUI.
inline constexpr std::uintptr_t kPlayScreenTechStackOreUiPredicateRva = 0x03078490;
// v26.21: enables the registered /legacy-play route when /play is unavailable.
inline constexpr std::uintptr_t kUseLegacyPlayScreenRva = 0x0367C8F0;
// v26.21: registers the /play redirect and legacy play_scr factory.
inline constexpr std::uintptr_t kRegisterLegacyPlayRouteRva = 0x0367C9C0;
// v26.21: common Scene Router navigation boundary.
inline constexpr std::uintptr_t kNavigateSceneRouteRva = 0x02442E70;
constexpr std::size_t kSceneRouterRouteProviderOffset = 0x80;
constexpr std::size_t kSceneRouterSceneStackNonOwnerOffset = 0x88;
constexpr std::size_t kRouteProviderExtractRouteVtableIndex = 2;
constexpr std::size_t kRouteProviderRoutesBeginOffset = 0x08;
constexpr std::size_t kRouteProviderRoutesEndOffset = 0x10;
constexpr std::size_t kRouteProviderRouteEntrySize = 0xD0;
constexpr std::size_t kRouteProviderRoutePathOffset = 0x20;
constexpr std::size_t kRouteProviderRouteFactoryTargetOffset = 0xC8;
constexpr std::size_t kRouteFactorySceneFactoryOffset = 0x08;
inline constexpr std::uintptr_t kRouteFactoryWrapperVtableRva = 0x0ADF2AC0;
constexpr std::size_t kClientInstanceOreUiConfigRegistryOffset = 0xA48;

}  // namespace tane::offsets::patch

#include <windows.h>
#include <shellapi.h>
#include <math.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "camera_math.h"
#include "overlay_placement.h"

#pragma warning(disable : 4191)

namespace {

constexpr const char* kModAuthor = "ERY_Asylum";
constexpr const char* kModName = "TL2_WASD_and_more";
constexpr const char* kModVersion = "v1.0.02z1";
constexpr LONG kCurrentConfigRevision = 17;

void FormatModTitle(char* destination, size_t destinationSize, bool includeAuthor) {
    if (destination == nullptr || destinationSize == 0) {
        return;
    }
    if (includeAuthor) {
        sprintf_s(
            destination,
            destinationSize,
            "%s %s - %s",
            kModName,
            kModVersion,
            kModAuthor);
    } else {
        sprintf_s(destination, destinationSize, "%s %s", kModName, kModVersion);
    }
}

HMODULE g_module = nullptr;
wchar_t g_logFile[MAX_PATH]{};
wchar_t g_iniFile[MAX_PATH]{};
volatile LONG g_threadStarted = 0;
volatile LONG g_running = 1;
volatile LONG g_messageProbeInstalled = 0;
volatile LONG g_transitionMessageCounts[6]{};
volatile LONG g_transitionDiagnosticLogLines = 0;
void* g_messageProbeGateway = nullptr;
BYTE g_messageProbeOriginal[7]{};
volatile LONG g_localMoveProbeInstalled = 0;
volatile LONG g_localMoveProbeCalls = 0;
void* g_localMoveProbeGateway = nullptr;
BYTE g_localMoveProbeOriginal[6]{};
volatile LONG g_stationaryFallbackInstalled = 0;
volatile LONG g_stationaryFallbackActivations = 0;
volatile LONG g_stationaryFallbackLogged = 0;
volatile LONG g_d3d9ViewportReadyProbeInstalled = 0;
volatile LONG g_d3d9ViewportReadyProbeState = 0;
void* g_stationaryFallbackAttackTarget = nullptr;
void* g_stationaryFallbackContinueTarget = nullptr;
BYTE g_stationaryFallbackOriginal[8]{};
constexpr bool kVerboseHookDiagnostics = false;
constexpr bool kVerboseInputLogging = false;
constexpr bool kEnableTransitionDiagnostics = true;
constexpr bool kEnableOgreLogLoadSignals = true;
constexpr bool kEnableReleaseSettle = false;
constexpr bool kEnableDllTrayConfig = false;
constexpr bool kShowDllTrayConfigAtStartup = false;
constexpr int kOverlayFallbackWidth = 900;
constexpr int kOverlayFallbackHeight = 640;
constexpr int kOverlayMonitorMargin = 16;
constexpr LONG kDefaultOverlayOpacity = 242;
constexpr LONG kDefaultOverlayFontScaleMode = 0; // 0 auto, 1 normal, 2 large, 3 extra large.
constexpr const char* kD3D9OverlayRenderVisibleProp = "TL2TrueKeyboardMove.D3D9OverlayVisible";
constexpr const char* kD3D9OverlayMirrorActiveProp = "TL2TrueKeyboardMove.D3D9OverlayMirrorActive";
constexpr UINT kOverlayWrappedMeasureFlags = DT_CENTER | DT_WORDBREAK;
constexpr UINT kOverlayWrappedDrawFlags = DT_CENTER | DT_TOP | DT_WORDBREAK;
constexpr UINT kOverlaySingleLineMeasureFlags = DT_CENTER | DT_SINGLELINE;
constexpr UINT kOverlaySingleLineDrawFlags = DT_CENTER | DT_TOP | DT_SINGLELINE;
constexpr DWORD kDefaultOverlayStartupDelayMs = 800;
constexpr DWORD kDefaultOverlayGeometryStableMs = 250;
constexpr DWORD kDefaultOverlayRepositionDebounceMs = 200;
constexpr LONG kStandardScanUp = 0x11;
constexpr LONG kStandardScanLeft = 0x1E;
constexpr LONG kStandardScanDown = 0x1F;
constexpr LONG kStandardScanRight = 0x20;
constexpr DWORD kCameraAcquireRetryMs = 75;
constexpr DWORD kCameraAcquireWindowMs = 10000;
constexpr int kCameraAcquireMaxAttempts = 120;
constexpr DWORD kDefaultCameraSyncDelayMs = 100;
constexpr DWORD kDefaultCameraSyncSampleIntervalMs = 50;
constexpr LONG kDefaultCameraSyncStableSamples = 3;
constexpr LONG kDefaultCameraSyncAngleToleranceX100 = 35;
constexpr DWORD kDefaultCameraSyncTimeoutMs = 2000;
constexpr DWORD kDefaultInitialCameraObservationTimeoutMs = 2000;
constexpr DWORD kDefaultPostFirstMoveCameraWatchMs = 2000;
constexpr DWORD kDefaultRuntimeTransitionCameraAcquireTimeoutMs = 2000;
constexpr DWORD kDefaultRuntimeTransitionSameAngleHoldMs = 500;
constexpr DWORD kDefaultPostReleaseCameraVerificationDelayMs = 10;
constexpr DWORD kDefaultPostReleaseContextPollMs = 50;
constexpr DWORD kDefaultPostReleaseContextTimeoutMs = 2000;
constexpr LONG kDefaultPostReleaseStableReads = 2;
constexpr DWORD kDefaultPostReleaseQuietMs = 300;
constexpr DWORD kPostReleaseBrakeRetryGraceMs = 120;
constexpr DWORD kDefaultPostReleaseCameraVerificationTimeoutMs = 2000;
constexpr float kProvisionalCameraForwardX = -0.7071067811865475f;
constexpr float kProvisionalCameraForwardZ = 0.7071067811865475f;
constexpr float kProvisionalCameraAngle = -45.0f;
constexpr LONG kRequiredConcordantEngineCameraObservations = 2;
constexpr DWORD kPrimaryActorTargetSetterOffset = 0x00126400;
constexpr DWORD kSecondaryActorTargetSetterOffset = 0x00126520;
constexpr DWORD kTransitionDiagnosticPollMs = 250;
constexpr DWORD kStabilityInstabilityWindowMs = 4000;
constexpr DWORD kStabilityFallbackMs = 20000;
constexpr DWORD kPassiveContextSuspendMs = 1000;
constexpr DWORD kLocalMoveExceptionQuarantineMs = 1200;
constexpr DWORD kTransitionFastResumeSettleMs = 250;
constexpr int kStabilityInstabilityThreshold = 5;
constexpr int kDirectContextStableReadsRequired = 4;
constexpr int kDirectContextBootstrapStableReadsRequired = 2;
constexpr DWORD kDirectContextBootstrapPollMs = 50;
constexpr DWORD kDirectContextStablePollMs = 1000;
constexpr DWORD kContextMonitorStablePollMs = 1500;
constexpr DWORD kMemoryWatchdogLogIntervalMs = 30000;
constexpr DWORD kTransientContextHoldMs = 220;
constexpr DWORD kSkillCarryStrictGraceMs = 1200;
constexpr DWORD kKeyboardMoveMinSendIntervalMs = 80;
constexpr DWORD kKeyboardMoveRepeatMs = 95;
constexpr float kKeyboardMoveLeadStep = 2.0f;
constexpr bool kEnableReleaseBrake = true;
constexpr LONG kDefaultReleaseBrakeMode = 1; // 0 none, 1 LocalMove current position.
constexpr DWORD kDefaultReleaseBrakeDelayMs = 85;
constexpr DWORD kLocalMoveReleaseBrakeMinIntervalMs = 300;
constexpr DWORD kLocalMoveReleaseBrakeDuplicateWindowMs = 1200;
constexpr float kLocalMoveReleaseBrakeDuplicateDistanceSq = 0.04f;
constexpr DWORD kHoldPositionVanillaBypassMs = 120;
constexpr DWORD kHoldPositionReleaseKeyboardSettleMs = 90;
constexpr DWORD kKeyboardMissingContextReacquireMs = 750;
constexpr UINT kGameThreadMoveMessage = WM_APP + 0x342;
constexpr UINT kGameThreadCaptureMessage = WM_APP + 0x343;
constexpr UINT kGameThreadPostReleaseCameraMessage = WM_APP + 0x344;
constexpr UINT kGameThreadWasdOwnershipRecoveryMessage = WM_APP + 0x345;
constexpr DWORD kDefaultEventCaptureTimeoutMs = 10000;
constexpr LONG kDefaultEventCaptureStableFrames = 3;
constexpr DWORD kDefaultQueuedCommandMaxAgeMs = 250;
constexpr DWORD kDefaultUiReadyFallbackDelayMs = 8000;
constexpr DWORD kDefaultMinimumCaptureAgeAfterSetRunningMs = 100;
constexpr DWORD kDefaultAdjacentLoadingContextMaxDistance = 262144;
constexpr DWORD kDefaultCapturePumpIntervalMs = 50;
constexpr LONG kDefaultDynamicArg1StableReads = 2;
constexpr DWORD kDefaultDynamicArg1SettleMs = 60;
constexpr DWORD kDefaultWasdOwnershipRecoveryWindowMs = 3000;
constexpr DWORD kDefaultWasdOwnershipRecoveryPollMs = 50;
constexpr LONG kDefaultWasdOwnershipRecoveryStableReads = 3;
constexpr DWORD kDefaultWasdOwnershipRecoveryRetryDelayMs = 250;
constexpr DWORD kWasdOwnershipVanillaEvidenceMaxAgeMs = 5000;
constexpr DWORD kDefaultEmergencyWasdBootstrapWindowMs = 30000;
constexpr DWORD kDefaultEmergencyWasdBootstrapRetryDelayMs = 500;
constexpr DWORD kEmergencyWasdBootstrapSetRunningMaxAgeMs = 60000;
constexpr DWORD kEmergencyWasdBootstrapVanillaEvidenceMaxAgeMs = 15000;
constexpr DWORD kCeguiSetVisibleIatStatic = 0x0211D134;
constexpr DWORD kCeguiSetTextIatStatic = 0x0211D230;
constexpr DWORD kCameraApiResolveIntervalMs = 1000;
constexpr SIZE_T kD3D9UpdateViewportPatchLength = 7;
const BYTE kExpectedD3D9UpdateViewportPrologue[kD3D9UpdateViewportPatchLength] = {
    0x80, 0xB9, 0xD8, 0x00, 0x00, 0x00, 0x00
};
void* g_d3d9UpdateViewportExport = nullptr;
void* g_d3d9UpdateViewportGateway = nullptr;
BYTE g_d3d9UpdateViewportOriginal[kD3D9UpdateViewportPatchLength]{};
bool g_launchExternalConfig = false;
bool g_enableOverlay = true;
SRWLOCK g_localMoveCaptureLock = SRWLOCK_INIT;
void* g_playerLocalMoveSelf = nullptr;
DWORD g_playerLocalMoveArg1 = 0;
void* g_lastKnownPlayerLocalMoveSelf = nullptr;
DWORD g_lastKnownPlayerLocalMoveArg1 = 0;
volatile LONG g_hasPlayerLocalMove = 0;
volatile LONG g_hasLastKnownPlayerLocalMove = 0;
volatile LONG g_internalLocalMoveCall = 0;
volatile LONG g_keyboardMoveMissingContextLogged = 0;
volatile LONG g_trayThreadStarted = 0;
volatile LONG g_keyUp = 'W';
volatile LONG g_keyLeft = 'A';
volatile LONG g_keyDown = 'S';
volatile LONG g_keyRight = 'D';
volatile LONG g_usePhysicalKeyboardLayout = 0;
volatile LONG g_scanUp = kStandardScanUp;
volatile LONG g_scanLeft = kStandardScanLeft;
volatile LONG g_scanDown = kStandardScanDown;
volatile LONG g_scanRight = kStandardScanRight;
volatile LONG g_keyboardMousePatchEnabled = 1;
volatile LONG g_disableClickMovement = 0;
volatile LONG g_holdPositionKey = VK_SHIFT;
volatile LONG g_holdPositionSuspended = 0;
volatile LONG g_holdPositionSuspensionActivations = 0;
volatile LONG g_holdPositionSuspensionLogged = 0;
volatile LONG g_holdPositionVanillaBypassActivations = 0;
volatile LONG g_holdPositionVanillaBypassLogged = 0;
volatile LONG g_holdPositionReleaseSettleActivations = 0;
volatile LONG g_holdPositionReleaseSettleLogged = 0;
volatile LONG g_releaseBrakeMode = kDefaultReleaseBrakeMode;
volatile LONG g_releaseBrakeDelayMs = kDefaultReleaseBrakeDelayMs;
volatile LONG g_directReacquireRequested = 0;
volatile LONG g_keyboardMoveMinSendIntervalMs = kKeyboardMoveMinSendIntervalMs;
volatile LONG g_keyboardMoveRepeatMs = kKeyboardMoveRepeatMs;
volatile LONG g_keyboardMoveLeadStepX100 = static_cast<LONG>(kKeyboardMoveLeadStep * 100.0f);
volatile LONG g_gameInputFocused = 1;
volatile LONG g_currentKeyboardMoveMask = 0;
volatile LONG g_focusSuspensionLogged = 0;
volatile LONG g_activeRangePatchConfigured = 0;
volatile LONG g_activeRangePatchStartupCaptured = 0;
volatile LONG g_activeRangePatchEnabledAtStartup = 0;
volatile LONG g_activeRangePatchApplied = 0;
volatile LONG g_activeRangePatchRestartRequired = 0;
volatile LONG g_enableMemoryWatchdog = 0;
volatile LONG g_enableTransitionDiagnosticsRuntime = 0;
volatile LONG g_enableTransitionMessageHook = 0;
volatile LONG g_enableMapRenderReadyProbe = 0;
volatile LONG g_enableCameraAutoCalibration = 0;
volatile LONG g_cameraSyncAfterContextLock = 1;
volatile LONG g_cameraSyncDelayMs = kDefaultCameraSyncDelayMs;
volatile LONG g_cameraSyncSampleIntervalMs = kDefaultCameraSyncSampleIntervalMs;
volatile LONG g_cameraSyncStableSamplesRequired = kDefaultCameraSyncStableSamples;
volatile LONG g_cameraSyncAngleToleranceX100 = kDefaultCameraSyncAngleToleranceX100;
volatile LONG g_cameraSyncTimeoutMs = kDefaultCameraSyncTimeoutMs;
volatile LONG g_blockKeyboardUntilCameraSync = 0;
volatile LONG g_initialCameraObservationTimeoutMs = kDefaultInitialCameraObservationTimeoutMs;
volatile LONG g_allowProvisionalFallbackAfterInitialTimeout = 1;
volatile LONG g_enablePostFirstMoveCameraWatch = 1;
volatile LONG g_postFirstMoveCameraWatchMs = kDefaultPostFirstMoveCameraWatchMs;
volatile LONG g_runtimeTransitionCameraAcquireTimeoutMs = kDefaultRuntimeTransitionCameraAcquireTimeoutMs;
volatile LONG g_runtimeTransitionSameAngleHoldMs = kDefaultRuntimeTransitionSameAngleHoldMs;
volatile LONG g_verifyCameraAfterFirstKeyboardRelease = 1;
volatile LONG g_postReleaseCameraVerificationDelayMs = kDefaultPostReleaseCameraVerificationDelayMs;
volatile LONG g_postReleaseContextPollMs = kDefaultPostReleaseContextPollMs;
volatile LONG g_postReleaseContextTimeoutMs = kDefaultPostReleaseContextTimeoutMs;
volatile LONG g_postReleaseStableReadsRequired = kDefaultPostReleaseStableReads;
volatile LONG g_postReleaseQuietMs = kDefaultPostReleaseQuietMs;
volatile LONG g_postReleaseCameraVerificationTimeoutMs = kDefaultPostReleaseCameraVerificationTimeoutMs;
volatile LONG g_blockKeyboardDuringPostReleaseCameraVerification = 0;
volatile LONG g_resetInteractionFacingOnKeyboardStart = 1;
volatile LONG g_clearSecondaryInteractionTarget = 1;
volatile LONG g_keyboardFacingResetPending = 0;
volatile LONG g_keyboardFacingResetCount = 0;
volatile LONG g_keyboardFacingResetLogged = 0;
volatile LONG g_interactionTargetApiState = 0;
volatile LONG g_enableOgreLogLoadSignals = 0;

enum class EventCaptureState : LONG {
    Disabled = 0,
    Loading = 1,
    AwaitReadySignal = 2,
    Capturing = 3,
    Locked = 4,
    Failed = 5
};

enum class PendingMovementKind : LONG {
    None = 0,
    Move = 1,
    ReleaseBrake = 2
};

volatile LONG g_enableEventDrivenCapture = 1;
volatile LONG g_requireSetRunningBeforeCapture = 1;
volatile LONG g_requirePlayerCamBeforeCapture = 0;
volatile LONG g_requireUiReadyBeforeCapture = 0;
volatile LONG g_requireStrictPlayerMarkerAtCapture = 0;
volatile LONG g_enableUiLifecycleProbe = 0;
volatile LONG g_captureOnMapTitleText = 1;
volatile LONG g_captureOnLoadingHidden = 1;
volatile LONG g_captureOnHudVisible = 0;
volatile LONG g_allowPlayerCamFallback = 0;
volatile LONG g_uiReadyFallbackDelayMs = kDefaultUiReadyFallbackDelayMs;
volatile LONG g_logUiLifecycle = 0;
volatile LONG g_logAllUiTextDuringLoad = 0;
volatile LONG g_lockPlayerIdentityOnly = 1;
volatile LONG g_resolveArg1AtDispatch = 1;
volatile LONG g_preserveEventLockAcrossRuntimeDips = 1;
volatile LONG g_capturePumpIntervalMs = kDefaultCapturePumpIntervalMs;
volatile LONG g_dynamicArg1StableReadsRequired = kDefaultDynamicArg1StableReads;
volatile LONG g_dynamicArg1SettleMs = kDefaultDynamicArg1SettleMs;
volatile LONG g_captureStableFrames = kDefaultEventCaptureStableFrames;
volatile LONG g_captureTimeoutMs = kDefaultEventCaptureTimeoutMs;
volatile LONG g_useExactSetRunningVftableOnly = 1;
volatile LONG g_minimumCaptureAgeAfterSetRunningMs = kDefaultMinimumCaptureAgeAfterSetRunningMs;
volatile LONG g_rejectAdjacentLoadingContext = 1;
volatile LONG g_adjacentLoadingContextMaxDistance = kDefaultAdjacentLoadingContextMaxDistance;
volatile LONG g_enableContinuousControllerScan = 0;
volatile LONG g_enableRuntimeReacquire = 0;
volatile LONG g_localMoveCanRefreshContext = 0;
volatile LONG g_enableWasdOwnershipRecovery = 1;
volatile LONG g_wasdOwnershipRecoveryWindowMs = kDefaultWasdOwnershipRecoveryWindowMs;
volatile LONG g_wasdOwnershipRecoveryPollMs = kDefaultWasdOwnershipRecoveryPollMs;
volatile LONG g_wasdOwnershipRecoveryStableReadsRequired = kDefaultWasdOwnershipRecoveryStableReads;
volatile LONG g_wasdOwnershipRecoveryRetryDelayMs = kDefaultWasdOwnershipRecoveryRetryDelayMs;
volatile LONG g_allowVanillaPlayerMoveOwnershipEvidence = 1;
volatile LONG g_enableEmergencyWasdBootstrap = 1;
volatile LONG g_autoEmergencyBootstrapOnOrphanSetRunning = 1;
volatile LONG g_useSetRunningCoreAsEmergencyController = 1;
volatile LONG g_useVanillaMoveAsEmergencyBootstrapEvidence = 1;
volatile LONG g_emergencyWasdBootstrapWindowMs = kDefaultEmergencyWasdBootstrapWindowMs;
volatile LONG g_emergencyWasdBootstrapRetryDelayMs = kDefaultEmergencyWasdBootstrapRetryDelayMs;
volatile LONG g_clearContextOnSetLevel = 1;
volatile LONG g_dispatchOnGameWindowThread = 1;
volatile LONG g_allowWorkerThreadLocalMove = 0;
volatile LONG g_queueLatestInputOnly = 1;
volatile LONG g_queuedCommandMaxAgeMs = kDefaultQueuedCommandMaxAgeMs;
volatile LONG g_requireGameWindowThread = 1;
volatile LONG g_requireStrictPlayerMarkerAtDispatch = 0;
volatile LONG g_validateLocalMoveDispatchList = 1;
volatile LONG g_localMoveDispatchListOffset = 0x6B0;
volatile LONG g_localMoveDispatchListMaxEntries = 128;
volatile LONG g_logThreadAffinity = 1;
volatile LONG g_logCaptureLifecycle = 1;
volatile LONG g_eventCaptureState = static_cast<LONG>(EventCaptureState::Disabled);
volatile LONG g_mapGenerationSynthetic = 0;
volatile LONG g_captureMessagePosted = 0;
volatile LONG g_moveMessagePosted = 0;
volatile LONG g_postReleaseCameraMessagePosted = 0;
volatile LONG g_wasdOwnershipRecoveryMessagePosted = 0;
volatile LONG g_wasdOwnershipRecoveryActive = 0;
volatile LONG g_emergencyWasdBootstrapActive = 0;
volatile LONG g_emergencyWasdBootstrapManualRequested = 0;
volatile LONG g_emergencyWasdBootstrapPostLoadSeen = 0;
volatile LONG g_pendingMovementAvailable = 0;
SRWLOCK g_pendingMovementLock = SRWLOCK_INIT;
SRWLOCK g_gameWindowHookLock = SRWLOCK_INIT;
SRWLOCK g_wasdOwnershipRecoveryLock = SRWLOCK_INIT;
SRWLOCK g_emergencyWasdBootstrapLock = SRWLOCK_INIT;
PendingMovementKind g_pendingMovementKind = PendingMovementKind::None;
int g_pendingMovementMask = 0;
LONG g_pendingMovementStepX100 = 0;
DWORD g_pendingMovementQueuedTick = 0;
DWORD g_pendingMovementSequence = 0;
DWORD g_lastExecutedMovementSequence = 0;
HWND g_dispatchGameWindow = nullptr;
WNDPROC g_originalGameWindowProc = nullptr;
DWORD g_gameWindowThreadId = 0;
DWORD g_workerThreadId = 0;
DWORD g_setRunningThreadId = 0;
DWORD g_playerCamThreadId = 0;
DWORD g_lastVanillaPlayerMoveThreadId = 0;
DWORD g_setRunningSeenGeneration = 0;
DWORD g_setRunningSeenTick = 0;
DWORD g_eventCaptureGeneration = 0;
DWORD g_eventCaptureStartedTick = 0;
void* g_eventCaptureCandidateSelf = nullptr;
DWORD g_eventCaptureCandidateArg1 = 0;
LONG g_eventCaptureCandidateStableFrames = 0;
DWORD g_lastEventCaptureLogTick = 0;
DWORD g_lastDispatchSkipLogTick = 0;
DWORD g_lastDynamicArg1LogTick = 0;
DWORD g_lastPlayerMoveSourceLogTick = 0;
DWORD g_wasdOwnershipRecoveryGeneration = 0;
DWORD g_wasdOwnershipRecoveryUntilTick = 0;
DWORD g_wasdOwnershipRecoveryNextPollTick = 0;
DWORD g_wasdOwnershipRecoveryRetryAfterTick = 0;
DWORD g_lastWasdOwnershipRecoveryLogTick = 0;
void* g_wasdOwnershipRecoveryCandidateController = nullptr;
void* g_wasdOwnershipRecoveryCandidateSelf = nullptr;
DWORD g_wasdOwnershipRecoveryCandidateArg1 = 0;
LONG g_wasdOwnershipRecoveryCandidateStableReads = 0;
void* g_wasdOwnershipVanillaController = nullptr;
void* g_wasdOwnershipVanillaSelf = nullptr;
DWORD g_wasdOwnershipVanillaArg1 = 0;
DWORD g_wasdOwnershipVanillaGeneration = 0;
DWORD g_wasdOwnershipVanillaEvidenceTick = 0;
LONG g_wasdOwnershipVanillaEvidenceReads = 0;
DWORD g_emergencyWasdBootstrapUntilTick = 0;
DWORD g_emergencyWasdBootstrapRetryAfterTick = 0;
DWORD g_emergencyWasdBootstrapNextAttemptTick = 0;
DWORD g_lastEmergencyWasdBootstrapLogTick = 0;
void* g_emergencyWasdSetRunningController = nullptr;
DWORD g_emergencyWasdSetRunningTick = 0;
void* g_emergencyWasdVanillaController = nullptr;
void* g_emergencyWasdVanillaSelf = nullptr;
DWORD g_emergencyWasdVanillaArg1 = 0;
DWORD g_emergencyWasdVanillaEvidenceTick = 0;
LONG g_emergencyWasdVanillaEvidenceReads = 0;
DWORD g_lastCapturePumpTick = 0;
DWORD g_lastSuppressedContextClearLogTick = 0;
DWORD g_lastLocalMoveDispatchGuardLogTick = 0;
DWORD g_dynamicArg1Candidate = 0;
DWORD g_dynamicArg1CandidateFirstTick = 0;
LONG g_dynamicArg1CandidateStableReads = 0;
DWORD g_uiReadyGeneration = 0;
DWORD g_uiReadyTick = 0;
DWORD g_lastUiLifecycleLogTick = 0;
LONG g_uiLifecycleLogLines = 0;
char g_lastUiReadyWindow[128]{};
char g_lastUiReadyText[192]{};
char g_mapTitleWindowToken[128]{};
volatile LONG g_ceguiHooksInstalled = 0;
using CeguiWindowSetTextFn = void(__thiscall*)(void*, const void*);
using CeguiWindowSetVisibleFn = void(__thiscall*)(void*, bool);
using CeguiWindowGetNameFn = const void*(__thiscall*)(void*);
using CeguiStringCStrFn = const char*(__thiscall*)(const void*);
using SetActorTargetFn = void(__thiscall*)(void*, void*);
SetActorTargetFn g_setPrimaryActorTarget = nullptr;
SetActorTargetFn g_setSecondaryActorTarget = nullptr;
CeguiWindowSetTextFn g_ceguiSetTextOriginal = nullptr;
CeguiWindowSetVisibleFn g_ceguiSetVisibleOriginal = nullptr;
CeguiWindowGetNameFn g_ceguiGetWindowName = nullptr;
CeguiStringCStrFn g_ceguiStringCStr = nullptr;
char g_keyUpName[16] = "W";
char g_keyLeftName[16] = "A";
char g_keyDownName[16] = "S";
char g_keyRightName[16] = "D";
char g_holdPositionKeyName[16] = "SHIFT";
int g_currentPresetIndex = 0;
int g_overlaySelectedPresetIndex = 0;
HWND g_overlayWindow = nullptr;
HWND g_overlayGameWindow = nullptr;
volatile LONG g_overlayVisible = 0;
volatile LONG g_overlayFallbackHiddenForFocusLoss = 0;
volatile LONG g_overlayFontScaleMode = kDefaultOverlayFontScaleMode;
volatile LONG g_overlayOpacity = kDefaultOverlayOpacity;
volatile LONG g_enableD3D9FrameOverlay = 1;
volatile LONG g_hideWindowOverlayWhenD3D9Active = 1;
volatile LONG g_integratedD3D9OverlayDeviceHookInstalled = 0;
volatile LONG g_integratedD3D9OverlayFrameDrawn = 0;
bool g_skipOverlayBackgroundFill = false;
volatile LONG g_overlayShowAtStartup = 1;
volatile LONG g_overlayStartupPending = 0;
volatile LONG g_overlayStartupDelayMs = kDefaultOverlayStartupDelayMs;
volatile LONG g_overlayGeometryStableMs = kDefaultOverlayGeometryStableMs;
volatile LONG g_overlayRepositionDebounceMs = kDefaultOverlayRepositionDebounceMs;
int g_overlayCustomStep = -1;
bool g_overlayCustomWaitingForRelease = false;
int g_overlayCustomKeys[4]{};
wchar_t g_overlayCustomNames[4][16]{};
int g_overlayCustomHoldKey = VK_SHIFT;
wchar_t g_overlayCustomHoldName[16] = L"SHIFT";
DWORD g_lastOverlayPositionTick = 0;
HWND g_lastOverlayAnchorWindow = nullptr;
int g_lastOverlayX = INT_MIN;
int g_lastOverlayY = INT_MIN;
int g_lastOverlayWidth = 0;
int g_lastOverlayHeight = 0;
RECT g_lastOverlayAnchorRect{};
bool g_hasLastOverlayAnchorRect = false;
RECT g_overlayObservedAnchorRect{};
bool g_overlayObservedAnchorRectValid = false;
DWORD g_overlayObservedAnchorStableSinceTick = 0;
DWORD g_overlayGeometryDirtyTick = 0;
DWORD g_overlayStartupNotBeforeTick = 0;
DWORD g_lastKeyboardMoveLogTick = 0;
DWORD g_lastInjectedLocalMoveTick = 0;
DWORD g_lastSuccessfulKeyboardMoveGeneration = 0;
DWORD g_lastReleaseSettleTick = 0;
DWORD g_lastLocalMoveReleaseBrakeSentTick = 0;
void* g_lastLocalMoveReleaseBrakeSelf = nullptr;
DWORD g_lastLocalMoveReleaseBrakeArg1 = 0;
float g_lastLocalMoveReleaseBrakeX = 0.0f;
float g_lastLocalMoveReleaseBrakeZ = 0.0f;
volatile LONG g_localMoveBrakePending = 0;
DWORD g_localMoveBrakePendingTick = 0;
DWORD g_lastKeyboardReacquireRequestTick = 0;
DWORD g_lastKeyboardReacquireRequestLogTick = 0;
DWORD g_localMoveExceptionBlockUntilTick = 0;
DWORD g_transitionLocalMoveBlockUntilTick = 0;
DWORD g_lastTransitionBlockLogTick = 0;
DWORD g_stabilityFallbackUntilTick = 0;
DWORD g_stabilityInstabilityWindowStartTick = 0;
DWORD g_lastStabilityFallbackLogTick = 0;
int g_stabilityInstabilityCount = 0;
DWORD g_holdPositionVanillaBypassUntilTick = 0;
DWORD g_passiveContextSuspendUntilTick = 0;
DWORD g_lastPassiveContextSuspendLogTick = 0;
DWORD g_localMoveExceptionQuarantineUntilTick = 0;
DWORD g_lastLocalMoveExceptionQuarantineLogTick = 0;
DWORD g_lastMemoryWatchdogLogTick = 0;
DWORD g_transientContextHoldUntilTick = 0;
DWORD g_lastTransientContextHoldLogTick = 0;
volatile LONG g_requiresVanillaMoveAfterLocalMoveException = 0;
volatile LONG g_localMoveExceptionCount = 0;
volatile LONG g_localMoveExceptionQuarantineEntries = 0;
volatile LONG g_disableKeyboardInjectionAfterLocalMoveException = 0;
volatile LONG g_transientContextHoldEntries = 0;
DWORD g_bootstrapWindowUntilTick = 0;
DWORD g_lastContextMonitorTick = 0;
DWORD g_lastTransitionDiagnosticPollTick = 0;
DWORD g_lastDirectContextDiagnosticPollTick = 0;
DWORD g_transitionMessageLastLogTick[6]{};
DWORD g_mapGeneration = 0;
DWORD g_mapIdentityScene = 0;
DWORD g_mapIdentityActiveLevel = 0;
DWORD g_mapRenderedPlayerCamGeneration = 0;
DWORD g_mapRenderedPlayerCamScene = 0;
DWORD g_mapRenderedPlayerCamActiveLevel = 0;
DWORD g_mapLockedGeneration = 0;
void* g_mapLockedController = nullptr;
DWORD g_mapLockedScene = 0;
DWORD g_mapLockedActiveLevel = 0;
DWORD g_lastMapLockLogTick = 0;
DWORD g_lastMapLoadSignalTick = 0;
DWORD g_lastStrictDirectContextMatchTick = 0;
DWORD g_lastSkillCarryLogTick = 0;
DWORD g_lastOgreLogPollTick = 0;
DWORD g_ogreLogReadOffset = 0;
volatile LONG g_ogreLogLoadSignals = 0;
volatile LONG g_skillCarryEntries = 0;
volatile LONG g_skillCarryLogged = 0;
DWORD g_lastDiagnosticGameRoot = 0;
DWORD g_lastDiagnosticLevelState = 0;
DWORD g_lastDiagnosticScene = 0;
DWORD g_lastDiagnosticActiveLevel = 0;
DWORD g_lastDiagnosticSceneVftable = 0;
DWORD g_lastDiagnosticWarpFlags = 0;
void* g_playerMoveController = nullptr;
void* g_mapLockedSelf = nullptr;
void* g_lastDiagnosticDirectPlayer = nullptr;
DWORD g_lastDiagnosticDirectLink = 0;
DWORD g_lastDiagnosticDirectArg1 = 0;
DWORD g_mapLockedArg1 = 0;
void* g_directContextCandidatePlayer = nullptr;
DWORD g_directContextCandidateArg1 = 0;
int g_directContextCandidateStableReads = 0;
bool g_lastDiagnosticDirectPlayerValid = false;
bool g_lastDiagnosticDirectPlayerUsable = false;
bool g_lastDiagnosticDirectArg1Valid = false;
DWORD g_sceneGeneration = 0;
DWORD g_lastIniReloadCheckTick = 0;
DWORD g_lastKeyboardLayoutPollTick = 0;
FILETIME g_lastIniWriteTime{};
HKL g_lastResolvedKeyboardLayout = nullptr;
char g_windowsKeyboardLayoutName[64] = "Unknown";
using GetRootSingletonPtrFn = void* (__cdecl*)();
using GetRenderSystemFn = void* (__thiscall*)(void*);
using GetActiveViewportFn = void* (__thiscall*)(void*);
using GetCameraFn = void* (__thiscall*)(void*);
using GetReferenceFn = const void* (__thiscall*)(void*);
using GetQuaternionRefFn = const tl2_camera_math::Quaternion* (__thiscall*)(void*);
GetRootSingletonPtrFn g_getOgreRootSingletonPtr = nullptr;
GetRenderSystemFn g_getOgreRenderSystem = nullptr;
GetActiveViewportFn g_getOgreActiveViewport = nullptr;
GetCameraFn g_getOgreViewportCamera = nullptr;
GetReferenceFn g_getOgreCameraName = nullptr;
GetQuaternionRefFn g_getOgreCameraDerivedOrientation = nullptr;
int g_ogreCameraApiState = 0;
DWORD g_lastCameraApiResolveTick = 0;
DWORD g_cameraAcquireDueTick = 0;
DWORD g_cameraAcquireUntilTick = 0;
DWORD g_cameraAcquireScene = 0;
DWORD g_cameraAcquireActiveLevel = 0;
DWORD g_cameraLatchedScene = 0;
DWORD g_cameraLatchedActiveLevel = 0;
int g_cameraAcquireAttempts = 0;
bool g_cameraAcquireRequiresLevelIdentity = false;
void* g_playerCamera = nullptr;
float g_cameraForwardX = 0.0f;
float g_cameraForwardZ = 0.0f;
bool g_cameraBasisAvailable = false;
bool g_cameraModeInitialized = false;
bool g_cameraAcquireActive = false;
bool g_cameraAcquireWaitingForContextLogged = false;
bool g_cameraAcquirePreserveBasisOnFailure = false;
bool g_cameraAcquireBlocksMovement = false;
float g_cameraCandidateForwardX = 0.0f;
float g_cameraCandidateForwardZ = 0.0f;
float g_cameraCandidateGameplayAngle = 0.0f;
LONG g_cameraCandidateStableSamples = 0;
DWORD g_lastCameraSyncWaitLogTick = 0;
volatile LONG g_cameraGameplayMode = 0;
float g_lastCameraVisualAngle = kProvisionalCameraAngle;
float g_lastCameraGameplayAngle = kProvisionalCameraAngle;

enum class CameraAcquisitionOrigin : LONG {
    None = 0,
    InitialAutomatic = 1,
    PostReleaseAutomatic = 2,
    RuntimeTransitionAutomatic = 3,
    Manual = 4
};

volatile LONG g_cameraAcquireOrigin = static_cast<LONG>(CameraAcquisitionOrigin::None);
volatile LONG g_cameraEngineConcordantObservations = 0;
float g_cameraEngineReferenceAngle = kProvisionalCameraAngle;
DWORD g_cameraEngineObservationArg1 = 0;
DWORD g_cameraEngineObservationGeneration = 0;
volatile LONG g_cameraBasisProvisional = 1;
volatile LONG g_runtimeCameraContextEstablished = 0;
volatile LONG g_postFirstMoveCameraWatchActive = 0;
DWORD g_postFirstMoveCameraWatchGeneration = 0;
DWORD g_postFirstMoveCameraWatchInitialArg1 = 0;
DWORD g_postFirstMoveCameraWatchUntilTick = 0;
DWORD g_runtimeTransitionCameraStartedTick = 0;
float g_runtimeTransitionCameraBaselineAngle = kProvisionalCameraAngle;
// Post-release verification stages:
// 0 = eligible, 1 = waiting for a stable release-brake dispatch context,
// 2 = release brake sent and waiting for the post-brake arg1 to settle,
// 3 = final delay pending, 4 = isolated camera acquisition active,
// 5 = completed or abandoned for the current map.
volatile LONG g_postReleaseCameraVerificationStage = 0;
DWORD g_postReleaseCameraVerificationGeneration = 0;
DWORD g_postReleaseCameraVerificationDueTick = 0;
DWORD g_postReleaseCameraVerificationUntilTick = 0;
DWORD g_postReleaseCameraVerificationEarliestBrakeTick = 0;
DWORD g_postReleaseBrakeSentTick = 0;
DWORD g_postReleaseObservedArg1 = 0;
LONG g_postReleaseStableReads = 0;
DWORD g_lastPostReleaseContextPollTick = 0;
float g_postReleaseCameraVerificationBaselineAngle = kProvisionalCameraAngle;
float g_lastPlayerMonitorX = 0.0f;
float g_lastPlayerMonitorZ = 0.0f;
volatile LONG g_levelBootstrapActive = 0;
volatile LONG g_contextLockedForLevel = 0;
volatile LONG g_hasLastPlayerMonitorPosition = 0;
volatile LONG g_hasTransitionDiagnosticSnapshot = 0;
volatile LONG g_hasPlayerMoveController = 0;
volatile LONG g_hasDirectContextDiagnosticSnapshot = 0;
volatile LONG g_cameraPostLoadRequest = 0;
void* g_loggedLocalMoveSelfs[16]{};
volatile LONG g_loggedLocalMoveSelfCount = 0;
void* g_loggedArg1Pointers[8]{};
volatile LONG g_loggedArg1PointerCount = 0;
HWND g_trayConfigWindow = nullptr;
HICON g_trayConfigIcon = nullptr;
NOTIFYICONDATAW g_dllTrayIcon{};

constexpr UINT kDllTrayMessage = WM_APP + 81;
constexpr UINT kDllTrayOpen = 3001;
constexpr UINT kDllTrayWasd = 3002;
constexpr UINT kDllTrayZqsd = 3003;
constexpr UINT kDllTrayArrows = 3004;
constexpr UINT kDllTrayHideIcon = 3005;
constexpr int kDllEditLeft = 3101;
constexpr int kDllEditUp = 3102;
constexpr int kDllEditRight = 3103;
constexpr int kDllEditDown = 3104;
constexpr int kDllButtonWasd = 3201;
constexpr int kDllButtonZqsd = 3202;
constexpr int kDllButtonArrows = 3203;
constexpr int kDllButtonApply = 3204;
constexpr int kDllButtonHide = 3205;
void LogLine(const char* text);

DWORD FloatToBits(float value) {
    DWORD bits = 0;
    CopyMemory(&bits, &value, sizeof(bits));
    return bits;
}

float BitsToFloat(DWORD bits) {
    float value = 0.0f;
    CopyMemory(&value, &bits, sizeof(value));
    return value;
}

bool ReadDwordSafe(const void* base, DWORD offset, DWORD* valueOut) {
    __try {
        *valueOut = *reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(base) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *valueOut = 0;
        return false;
    }
}

bool ReadByteSafe(const void* base, DWORD offset, BYTE* valueOut) {
    __try {
        *valueOut = *reinterpret_cast<const BYTE*>(static_cast<const BYTE*>(base) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *valueOut = 0;
        return false;
    }
}

bool ReadFloatSafe(const void* base, DWORD offset, float* valueOut) {
    __try {
        *valueOut = *reinterpret_cast<const float*>(static_cast<const BYTE*>(base) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *valueOut = 0.0f;
        return false;
    }
}

DWORD StaticToRuntime(DWORD staticAddress) {
    constexpr DWORD kOriginalImageBase = 0x00400000;
    const DWORD moduleBase = reinterpret_cast<DWORD>(GetModuleHandleW(nullptr));
    return moduleBase + (staticAddress - kOriginalImageBase);
}

void ApplyActiveRangePatchAtStartup() {
    const bool enabled = InterlockedCompareExchange(&g_activeRangePatchConfigured, 0, 0) != 0;
    InterlockedExchange(&g_activeRangePatchEnabledAtStartup, enabled ? 1 : 0);
    InterlockedExchange(&g_activeRangePatchStartupCaptured, 1);
    InterlockedExchange(&g_activeRangePatchRestartRequired, 0);

    if (!enabled) {
        InterlockedExchange(&g_activeRangePatchApplied, 0);
        LogLine("ActiveRangePatch: disabled in ini at startup.");
        return;
    }

    InterlockedExchange(&g_activeRangePatchApplied, 1);
    LogLine("ActiveRangePatch: enabled in ini; overlay reports applied. Engine patch is handled by MSWSOCK loader.");
}

void InitPaths(HMODULE module) {
    wchar_t modulePath[MAX_PATH]{};
    const DWORD written = GetModuleFileNameW(module, modulePath, MAX_PATH);
    if (written == 0 || written >= MAX_PATH) {
        g_logFile[0] = L'\0';
        g_iniFile[0] = L'\0';
        return;
    }

    for (DWORD i = written; i > 0; --i) {
        if (modulePath[i - 1] == L'\\' || modulePath[i - 1] == L'/') {
            modulePath[i] = L'\0';
            wcscpy_s(g_logFile, modulePath);
            wcscat_s(g_logFile, L"TL2TrueKeyboardMove.log");
            wcscpy_s(g_iniFile, modulePath);
            wcscat_s(g_iniFile, L"TL2TrueKeyboardMove.ini");
            return;
        }
    }

    wcscpy_s(g_logFile, L"TL2TrueKeyboardMove.log");
    wcscpy_s(g_iniFile, L"TL2TrueKeyboardMove.ini");
}

bool GetModuleDirectory(wchar_t* buffer, DWORD length) {
    const DWORD written = GetModuleFileNameW(g_module, buffer, length);
    if (written == 0 || written >= length) {
        return false;
    }

    for (DWORD i = written; i > 0; --i) {
        if (buffer[i - 1] == L'\\' || buffer[i - 1] == L'/') {
            buffer[i] = L'\0';
            return true;
        }
    }

    return false;
}

void LogLine(const char* text) {
    if (g_logFile[0] == L'\0') {
        return;
    }

    HANDLE file = CreateFileW(g_logFile, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}

bool BuildOgreLogPath(wchar_t* path, DWORD pathCount) {
    if (path == nullptr || pathCount == 0) {
        return false;
    }

    wchar_t profile[MAX_PATH]{};
    const DWORD written = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    if (written == 0 || written >= MAX_PATH) {
        return false;
    }

    path[0] = L'\0';
    wcscpy_s(path, pathCount, profile);
    wcscat_s(path, pathCount, L"\\Documents\\My Games\\Runic Games\\Torchlight 2\\Ogre.log");
    return true;
}

bool BufferHasEnvironmentLoadMarker(const char* text) {
    if (text == nullptr) {
        return false;
    }

    return strstr(text, "LEVEL LOAD - LAYOUT") != nullptr;
}

bool BeginMapLoadGeneration(DWORD now, DWORD scene, DWORD activeLevel, const char* reason);
bool IsGameWindowForegroundForInput();
void CaptureEventDrivenContextOnGameThread();
void ExecutePendingMovementOnGameWindowThread();
void TryAdvancePostReleaseCameraVerificationOnGameThread();
void PollWasdOwnershipRecoveryOnGameThread();
void PumpWasdOwnershipRecovery(DWORD now);
void ResetWasdOwnershipRecoveryState(bool clearVanillaEvidence);
void ResetEmergencyWasdBootstrapState(bool clearEvidence);
void ArmEmergencyWasdBootstrap(DWORD now, bool manual, const char* reason);
void PumpEmergencyWasdBootstrap(DWORD now);
void RequestManualWasdBootstrap(DWORD now);
void RequestWasdBootstrapAfterPatchReenable(DWORD now);
void ObserveEmergencySetRunningController(DWORD now, void* controller);
void ObserveConfirmedVanillaPlayerMoveForEmergencyBootstrap(
    DWORD now,
    void* sourceController,
    void* self,
    DWORD arg1,
    void* directPlayer,
    DWORD directArg1);
void RequestManualCameraRecalibration(DWORD now);
void StartPostReleaseCameraRecalibration(DWORD now);
void InstallCeguiLifecycleHooks();
void RestoreCeguiLifecycleHooks();
void MarkUiReadyForCurrentMap(const char* reason, const char* windowName, const char* text);
void QueueGameThreadMovement(PendingMovementKind kind, int mask, LONG stepX100);
void ResetPendingMovementQueue();
void BlockLocalMoveForTransition(
    const char* reason,
    DWORD now,
    DWORD blockMs,
    bool clearCurrentContext);

void PollOgreLogLoadSignals(DWORD now) {
    if (!kEnableOgreLogLoadSignals ||
        InterlockedCompareExchange(&g_enableOgreLogLoadSignals, 0, 0) == 0) {
        (void)now;
        return;
    }

    if (g_lastOgreLogPollTick != 0 &&
        now - g_lastOgreLogPollTick < 250) {
        return;
    }
    g_lastOgreLogPollTick = now;

    wchar_t path[MAX_PATH]{};
    if (!BuildOgreLogPath(path, MAX_PATH)) {
        return;
    }

    HANDLE file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    const DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return;
    }

    if (g_ogreLogReadOffset == 0 || g_ogreLogReadOffset > size) {
        g_ogreLogReadOffset = size;
        CloseHandle(file);
        return;
    }

    if (size == g_ogreLogReadOffset) {
        CloseHandle(file);
        return;
    }

    const DWORD bytesToRead = size - g_ogreLogReadOffset > 8191
        ? 8191
        : size - g_ogreLogReadOffset;
    SetFilePointer(file, g_ogreLogReadOffset, nullptr, FILE_BEGIN);
    char buffer[8192]{};
    DWORD bytesRead = 0;
    const BOOL ok = ReadFile(file, buffer, bytesToRead, &bytesRead, nullptr);
    CloseHandle(file);
    g_ogreLogReadOffset = size;
    if (!ok || bytesRead == 0) {
        return;
    }
    buffer[bytesRead < sizeof(buffer) ? bytesRead : sizeof(buffer) - 1] = '\0';

    if (!BufferHasEnvironmentLoadMarker(buffer)) {
        return;
    }

    InterlockedIncrement(&g_ogreLogLoadSignals);
    if (BeginMapLoadGeneration(now, 0, 0, "Ogre.log level layout marker")) {
        BlockLocalMoveForTransition(
            "Ogre.log level layout marker; player local move context cleared.",
            now,
            2200,
            true);
    }
}

void CopyWideKeyName(char* destination, size_t destinationSize, const wchar_t* source) {
    if (destinationSize == 0) {
        return;
    }

    char converted[32]{};
    WideCharToMultiByte(CP_ACP, 0, source, -1, converted, sizeof(converted), nullptr, nullptr);
    lstrcpynA(destination, converted, static_cast<int>(destinationSize));
}

HWND FindGameWindow();

HKL GetGameKeyboardLayout() {
    HWND gameWindow = g_overlayGameWindow;
    if (gameWindow == nullptr || !IsWindow(gameWindow)) {
        gameWindow = FindGameWindow();
    }

    if (gameWindow != nullptr) {
        const DWORD gameThread = GetWindowThreadProcessId(gameWindow, nullptr);
        if (gameThread != 0) {
            HKL layout = GetKeyboardLayout(gameThread);
            if (layout != nullptr) {
                return layout;
            }
        }
    }

    return GetKeyboardLayout(0);
}

void SetVirtualKeyDisplayName(int key, char* destination, size_t destinationSize) {
    if (destination == nullptr || destinationSize == 0) {
        return;
    }

    if (key >= 'A' && key <= 'Z') {
        destination[0] = static_cast<char>(key);
        destination[1] = '\0';
        return;
    }

    const char* name = nullptr;
    switch (key) {
        case VK_LEFT: name = "LEFT"; break;
        case VK_UP: name = "UP"; break;
        case VK_RIGHT: name = "RIGHT"; break;
        case VK_DOWN: name = "DOWN"; break;
        case VK_SHIFT: name = "SHIFT"; break;
        case VK_LSHIFT: name = "LSHIFT"; break;
        case VK_RSHIFT: name = "RSHIFT"; break;
        case VK_CONTROL: name = "CTRL"; break;
        case VK_LCONTROL: name = "LCTRL"; break;
        case VK_RCONTROL: name = "RCTRL"; break;
        case VK_MENU: name = "ALT"; break;
        case VK_LMENU: name = "LALT"; break;
        case VK_RMENU: name = "RALT"; break;
        case VK_SPACE: name = "SPACE"; break;
        case VK_F1: name = "F1"; break;
        case VK_F2: name = "F2"; break;
        case VK_F3: name = "F3"; break;
        case VK_F4: name = "F4"; break;
        case VK_F5: name = "F5"; break;
        case VK_F6: name = "F6"; break;
        case VK_F7: name = "F7"; break;
        case VK_F8: name = "F8"; break;
        case VK_F9: name = "F9"; break;
        case VK_F10: name = "F10"; break;
        case VK_F11: name = "F11"; break;
        case VK_F12: name = "F12"; break;
        default: break;
    }

    if (name != nullptr) {
        lstrcpynA(destination, name, static_cast<int>(destinationSize));
    } else {
        wsprintfA(destination, "VK%02X", key & 0xFF);
    }
}

void UpdateWindowsKeyboardLayoutName(HKL layout) {
    const LANGID languageId = LOWORD(reinterpret_cast<ULONG_PTR>(layout));
    const LCID localeId = MAKELCID(languageId, SORT_DEFAULT);
    wchar_t wideName[64]{};
    if (GetLocaleInfoW(
            localeId,
            LOCALE_SENGLISHDISPLAYNAME,
            wideName,
            static_cast<int>(sizeof(wideName) / sizeof(wideName[0]))) != 0 &&
        WideCharToMultiByte(
            CP_ACP,
            0,
            wideName,
            -1,
            g_windowsKeyboardLayoutName,
            static_cast<int>(sizeof(g_windowsKeyboardLayoutName)),
            nullptr,
            nullptr) != 0) {
        return;
    }

    {
        wsprintfA(
            g_windowsKeyboardLayoutName,
            "Windows layout 0x%04X",
            static_cast<unsigned int>(languageId));
    }
}

const char* ResolvedMovementStyle() {
    const int up = static_cast<int>(g_keyUp);
    const int left = static_cast<int>(g_keyLeft);
    const int down = static_cast<int>(g_keyDown);
    const int right = static_cast<int>(g_keyRight);
    if (up == 'W' && left == 'A' && down == 'S' && right == 'D') {
        return "WASD";
    }
    if (up == 'Z' && left == 'Q' && down == 'S' && right == 'D') {
        return "ZQSD";
    }
    if (up == VK_UP && left == VK_LEFT && down == VK_DOWN && right == VK_RIGHT) {
        return "Arrow keys";
    }
    return "physical positions";
}

bool ResolvePhysicalKeyboardBindings(bool forceLog) {
    if (InterlockedCompareExchange(&g_usePhysicalKeyboardLayout, 0, 0) == 0) {
        return false;
    }

    HKL layout = GetGameKeyboardLayout();
    if (layout == nullptr) {
        return false;
    }

    const int up = static_cast<int>(MapVirtualKeyExW(
        static_cast<UINT>(g_scanUp), MAPVK_VSC_TO_VK_EX, layout));
    const int left = static_cast<int>(MapVirtualKeyExW(
        static_cast<UINT>(g_scanLeft), MAPVK_VSC_TO_VK_EX, layout));
    const int down = static_cast<int>(MapVirtualKeyExW(
        static_cast<UINT>(g_scanDown), MAPVK_VSC_TO_VK_EX, layout));
    const int right = static_cast<int>(MapVirtualKeyExW(
        static_cast<UINT>(g_scanRight), MAPVK_VSC_TO_VK_EX, layout));
    if (up == 0 || left == 0 || down == 0 || right == 0) {
        return false;
    }

    const bool layoutChanged = layout != g_lastResolvedKeyboardLayout;
    InterlockedExchange(&g_keyUp, up);
    InterlockedExchange(&g_keyLeft, left);
    InterlockedExchange(&g_keyDown, down);
    InterlockedExchange(&g_keyRight, right);
    SetVirtualKeyDisplayName(up, g_keyUpName, sizeof(g_keyUpName));
    SetVirtualKeyDisplayName(left, g_keyLeftName, sizeof(g_keyLeftName));
    SetVirtualKeyDisplayName(down, g_keyDownName, sizeof(g_keyDownName));
    SetVirtualKeyDisplayName(right, g_keyRightName, sizeof(g_keyRightName));
    g_lastResolvedKeyboardLayout = layout;
    UpdateWindowsKeyboardLayoutName(layout);

    if (layoutChanged || forceLog) {
        char line[320]{};
        wsprintfA(
            line,
            "Windows keyboard layout resolved: %s style=%s Left=%s Up=%s Right=%s Down=%s.",
            g_windowsKeyboardLayoutName,
            ResolvedMovementStyle(),
            g_keyLeftName,
            g_keyUpName,
            g_keyRightName,
            g_keyDownName);
        LogLine(line);
        if (g_overlayWindow != nullptr && IsWindow(g_overlayWindow)) {
            InvalidateRect(g_overlayWindow, nullptr, TRUE);
        }
    }
    return true;
}

void PollWindowsKeyboardLayout(DWORD now) {
    if (now - g_lastKeyboardLayoutPollTick < 250) {
        return;
    }
    g_lastKeyboardLayoutPollTick = now;
    ResolvePhysicalKeyboardBindings(false);
}

struct KeyboardPreset {
    const wchar_t* id;
    const char* label;
    int up;
    int left;
    int down;
    int right;
    const wchar_t* upName;
    const wchar_t* leftName;
    const wchar_t* downName;
    const wchar_t* rightName;
    bool useWindowsLayout;
};

constexpr KeyboardPreset kKeyboardPresets[] = {
    {L"AUTO", "Windows Auto (recommended)", 'W', 'A', 'S', 'D', L"W", L"A", L"S", L"D", true},
    {L"ARROWS", "Arrow keys", VK_UP, VK_LEFT, VK_DOWN, VK_RIGHT, L"UP", L"LEFT", L"DOWN", L"RIGHT", false},
};

constexpr int kKeyboardPresetCount = sizeof(kKeyboardPresets) / sizeof(kKeyboardPresets[0]);

void WriteScanCodeSetting(const wchar_t* name, LONG scanCode) {
    wchar_t value[16]{};
    wsprintfW(value, L"%ld", scanCode);
    WritePrivateProfileStringW(L"Keys", name, value, g_iniFile);
}

void ApplyKeyboardPresetByIndex(int index, bool persist) {
    if (index < 0 || index >= kKeyboardPresetCount) {
        index = 0;
    }

    const KeyboardPreset& preset = kKeyboardPresets[index];
    g_currentPresetIndex = index;
    if (preset.useWindowsLayout) {
        InterlockedExchange(&g_scanUp, kStandardScanUp);
        InterlockedExchange(&g_scanLeft, kStandardScanLeft);
        InterlockedExchange(&g_scanDown, kStandardScanDown);
        InterlockedExchange(&g_scanRight, kStandardScanRight);
        InterlockedExchange(&g_usePhysicalKeyboardLayout, 1);
        ResolvePhysicalKeyboardBindings(true);
    } else {
        InterlockedExchange(&g_usePhysicalKeyboardLayout, 0);
        InterlockedExchange(&g_keyUp, preset.up);
        InterlockedExchange(&g_keyLeft, preset.left);
        InterlockedExchange(&g_keyDown, preset.down);
        InterlockedExchange(&g_keyRight, preset.right);
        CopyWideKeyName(g_keyUpName, sizeof(g_keyUpName), preset.upName);
        CopyWideKeyName(g_keyLeftName, sizeof(g_keyLeftName), preset.leftName);
        CopyWideKeyName(g_keyDownName, sizeof(g_keyDownName), preset.downName);
        CopyWideKeyName(g_keyRightName, sizeof(g_keyRightName), preset.rightName);
    }

    if (persist && g_iniFile[0] != L'\0') {
        WritePrivateProfileStringW(L"Keys", L"Preset", preset.id, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Up", preset.upName, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Left", preset.leftName, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Right", preset.rightName, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Down", preset.downName, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Forward", nullptr, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Back", nullptr, g_iniFile);
        WritePrivateProfileStringW(
            L"Keys",
            L"BindingMode",
            preset.useWindowsLayout ? L"PHYSICAL" : L"VIRTUAL",
            g_iniFile);
        if (preset.useWindowsLayout) {
            WriteScanCodeSetting(L"ScanUp", g_scanUp);
            WriteScanCodeSetting(L"ScanLeft", g_scanLeft);
            WriteScanCodeSetting(L"ScanDown", g_scanDown);
            WriteScanCodeSetting(L"ScanRight", g_scanRight);
        } else {
            WritePrivateProfileStringW(L"Keys", L"ScanUp", nullptr, g_iniFile);
            WritePrivateProfileStringW(L"Keys", L"ScanLeft", nullptr, g_iniFile);
            WritePrivateProfileStringW(L"Keys", L"ScanDown", nullptr, g_iniFile);
            WritePrivateProfileStringW(L"Keys", L"ScanRight", nullptr, g_iniFile);
        }
    }

    char line[384]{};
    wsprintfA(
        line,
        "Keyboard preset applied: %s Left=%s Up=%s Right=%s Down=%s.",
        preset.label,
        g_keyLeftName,
        g_keyUpName,
        g_keyRightName,
        g_keyDownName);
    LogLine(line);
}

int ParseKeyName(const wchar_t* text, int fallback, char* displayName, size_t displayNameSize) {
    if (text == nullptr || text[0] == L'\0') {
        return fallback;
    }

    wchar_t key[32]{};
    lstrcpynW(key, text, 32);
    CharUpperBuffW(key, lstrlenW(key));

    int parsed = 0;
    if (lstrlenW(key) == 1 && key[0] >= L'A' && key[0] <= L'Z') {
        parsed = static_cast<int>(key[0]);
    } else if (lstrcmpW(key, L"UP") == 0) {
        parsed = VK_UP;
    } else if (lstrcmpW(key, L"DOWN") == 0) {
        parsed = VK_DOWN;
    } else if (lstrcmpW(key, L"LEFT") == 0) {
        parsed = VK_LEFT;
    } else if (lstrcmpW(key, L"RIGHT") == 0) {
        parsed = VK_RIGHT;
    }

    if (parsed == 0) {
        return fallback;
    }

    CopyWideKeyName(displayName, displayNameSize, key);
    return parsed;
}

int ParseHoldPositionKeyName(const wchar_t* text, int fallback, char* displayName, size_t displayNameSize) {
    if (text == nullptr || text[0] == L'\0') {
        SetVirtualKeyDisplayName(fallback, displayName, displayNameSize);
        return fallback;
    }

    wchar_t key[32]{};
    lstrcpynW(key, text, 32);
    CharUpperBuffW(key, lstrlenW(key));

    int parsed = 0;
    if (lstrlenW(key) == 1 && key[0] >= L'A' && key[0] <= L'Z') {
        parsed = static_cast<int>(key[0]);
    } else if (lstrcmpW(key, L"SHIFT") == 0) {
        parsed = VK_SHIFT;
    } else if (lstrcmpW(key, L"LSHIFT") == 0 || lstrcmpW(key, L"LEFTSHIFT") == 0) {
        parsed = VK_LSHIFT;
    } else if (lstrcmpW(key, L"RSHIFT") == 0 || lstrcmpW(key, L"RIGHTSHIFT") == 0) {
        parsed = VK_RSHIFT;
    } else if (lstrcmpW(key, L"CTRL") == 0 || lstrcmpW(key, L"CONTROL") == 0) {
        parsed = VK_CONTROL;
    } else if (lstrcmpW(key, L"LCTRL") == 0 || lstrcmpW(key, L"LCONTROL") == 0) {
        parsed = VK_LCONTROL;
    } else if (lstrcmpW(key, L"RCTRL") == 0 || lstrcmpW(key, L"RCONTROL") == 0) {
        parsed = VK_RCONTROL;
    } else if (lstrcmpW(key, L"ALT") == 0) {
        parsed = VK_MENU;
    } else if (lstrcmpW(key, L"LALT") == 0) {
        parsed = VK_LMENU;
    } else if (lstrcmpW(key, L"RALT") == 0) {
        parsed = VK_RMENU;
    } else if (lstrcmpW(key, L"SPACE") == 0) {
        parsed = VK_SPACE;
    } else if (lstrcmpW(key, L"UP") == 0) {
        parsed = VK_UP;
    } else if (lstrcmpW(key, L"DOWN") == 0) {
        parsed = VK_DOWN;
    } else if (lstrcmpW(key, L"LEFT") == 0) {
        parsed = VK_LEFT;
    } else if (lstrcmpW(key, L"RIGHT") == 0) {
        parsed = VK_RIGHT;
    } else if (key[0] == L'F') {
        const int fNumber = _wtoi(key + 1);
        if (fNumber >= 1 && fNumber <= 12) {
            parsed = VK_F1 + fNumber - 1;
        }
    }

    if (parsed == 0) {
        SetVirtualKeyDisplayName(fallback, displayName, displayNameSize);
        return fallback;
    }

    SetVirtualKeyDisplayName(parsed, displayName, displayNameSize);
    return parsed;
}

LONG ClampLong(LONG value, LONG minimum, LONG maximum);
const char* OverlayFontScaleModeLabel(LONG mode);
bool EnsureOverlayWindow();
void SetOverlayVisible(bool visible);
void UpdateD3D9OverlayRenderVisibleProp();
bool ShouldSuppressOverlayWindowForD3D9Mirror();
void InstallIntegratedD3D9OverlayHook();

void SetKeyboardPreset(const wchar_t* preset) {
    if (preset == nullptr) {
        return;
    }

    wchar_t normalized[32]{};
    lstrcpynW(normalized, preset, 32);
    CharUpperBuffW(normalized, lstrlenW(normalized));

    for (int i = 0; i < kKeyboardPresetCount; ++i) {
        if (lstrcmpW(normalized, kKeyboardPresets[i].id) == 0) {
            ApplyKeyboardPresetByIndex(i, false);
            return;
        }
    }

    if (lstrcmpW(normalized, L"CUSTOM") == 0) {
        g_currentPresetIndex = -1;
        return;
    }

    if (lstrcmpW(normalized, L"WASD") == 0 || lstrcmpW(normalized, L"ZQSD") == 0) {
        ApplyKeyboardPresetByIndex(0, false);
        return;
    }

    ApplyKeyboardPresetByIndex(0, false);
}

void RefreshCurrentPresetFromKeys();

void LoadKeyboardConfig() {
    if (g_iniFile[0] == L'\0') {
        return;
    }

    const LONG configRevision =
        GetPrivateProfileIntW(L"General", L"ConfigRevision", 0, g_iniFile);

    InterlockedExchange(
        &g_overlayFontScaleMode,
        ClampLong(GetPrivateProfileIntW(L"Overlay", L"TextScaleMode", kDefaultOverlayFontScaleMode, g_iniFile), 0, 3));
    InterlockedExchange(
        &g_overlayOpacity,
        ClampLong(GetPrivateProfileIntW(L"Overlay", L"Opacity", kDefaultOverlayOpacity, g_iniFile), 160, 255));
    InterlockedExchange(
        &g_enableD3D9FrameOverlay,
        GetPrivateProfileIntW(L"Overlay", L"EnableD3D9FrameOverlay", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_hideWindowOverlayWhenD3D9Active,
        GetPrivateProfileIntW(L"Overlay", L"HideWindowOverlayWhenD3D9FrameOverlayActive", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_overlayShowAtStartup,
        GetPrivateProfileIntW(L"Overlay", L"ShowAtStartup", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_overlayStartupDelayMs,
        ClampLong(GetPrivateProfileIntW(L"Overlay", L"StartupDelayMs", kDefaultOverlayStartupDelayMs, g_iniFile), 0, 10000));
    InterlockedExchange(
        &g_overlayGeometryStableMs,
        ClampLong(GetPrivateProfileIntW(L"Overlay", L"GeometryStableMs", kDefaultOverlayGeometryStableMs, g_iniFile), 100, 5000));
    InterlockedExchange(
        &g_overlayRepositionDebounceMs,
        ClampLong(GetPrivateProfileIntW(L"Overlay", L"RepositionDebounceMs", kDefaultOverlayRepositionDebounceMs, g_iniFile), 100, 3000));
    if (g_overlayWindow != nullptr && IsWindow(g_overlayWindow)) {
        SetLayeredWindowAttributes(
            g_overlayWindow,
            0,
            static_cast<BYTE>(InterlockedCompareExchange(&g_overlayOpacity, 0, 0)),
            LWA_ALPHA);
        InvalidateRect(g_overlayWindow, nullptr, FALSE);
    }

    wchar_t presetValue[32]{};
    GetPrivateProfileStringW(L"Keys", L"Preset", L"AUTO", presetValue, 32, g_iniFile);
    SetKeyboardPreset(presetValue);

    wchar_t bindingMode[32]{};
    GetPrivateProfileStringW(
        L"Keys",
        L"BindingMode",
        L"VIRTUAL",
        bindingMode,
        32,
        g_iniFile);
    CharUpperBuffW(bindingMode, lstrlenW(bindingMode));
    const bool autoPreset =
        lstrcmpiW(presetValue, L"AUTO") == 0 ||
        lstrcmpiW(presetValue, L"WASD") == 0 ||
        lstrcmpiW(presetValue, L"ZQSD") == 0;
    const bool customPreset = lstrcmpiW(presetValue, L"CUSTOM") == 0;
    bool customPhysicalLoaded = false;
    if (customPreset && lstrcmpW(bindingMode, L"PHYSICAL") == 0) {
        const LONG scanUp = GetPrivateProfileIntW(L"Keys", L"ScanUp", 0, g_iniFile);
        const LONG scanLeft = GetPrivateProfileIntW(L"Keys", L"ScanLeft", 0, g_iniFile);
        const LONG scanDown = GetPrivateProfileIntW(L"Keys", L"ScanDown", 0, g_iniFile);
        const LONG scanRight = GetPrivateProfileIntW(L"Keys", L"ScanRight", 0, g_iniFile);
        if (scanUp != 0 && scanLeft != 0 && scanDown != 0 && scanRight != 0) {
            InterlockedExchange(&g_scanUp, scanUp);
            InterlockedExchange(&g_scanLeft, scanLeft);
            InterlockedExchange(&g_scanDown, scanDown);
            InterlockedExchange(&g_scanRight, scanRight);
            InterlockedExchange(&g_usePhysicalKeyboardLayout, 1);
            g_currentPresetIndex = -1;
            customPhysicalLoaded = ResolvePhysicalKeyboardBindings(true);
        }
    }

    if (!autoPreset && !customPhysicalLoaded) {
        InterlockedExchange(&g_usePhysicalKeyboardLayout, 0);
        wchar_t value[32]{};
        GetPrivateProfileStringW(L"Keys", L"Up", L"", value, 32, g_iniFile);
        if (value[0] == L'\0') {
            GetPrivateProfileStringW(L"Keys", L"Forward", L"", value, 32, g_iniFile);
        }
        g_keyUp = ParseKeyName(value, g_keyUp, g_keyUpName, sizeof(g_keyUpName));
        GetPrivateProfileStringW(L"Keys", L"Left", L"", value, 32, g_iniFile);
        g_keyLeft = ParseKeyName(value, g_keyLeft, g_keyLeftName, sizeof(g_keyLeftName));
        GetPrivateProfileStringW(L"Keys", L"Right", L"", value, 32, g_iniFile);
        g_keyRight = ParseKeyName(value, g_keyRight, g_keyRightName, sizeof(g_keyRightName));
        GetPrivateProfileStringW(L"Keys", L"Down", L"", value, 32, g_iniFile);
        if (value[0] == L'\0') {
            GetPrivateProfileStringW(L"Keys", L"Back", L"", value, 32, g_iniFile);
        }
        g_keyDown = ParseKeyName(value, g_keyDown, g_keyDownName, sizeof(g_keyDownName));
        RefreshCurrentPresetFromKeys();
    }
    wchar_t holdPositionValue[32]{};
    GetPrivateProfileStringW(
        L"Keys",
        L"HoldPosition",
        L"SHIFT",
        holdPositionValue,
        32,
        g_iniFile);
    char holdPositionName[16]{};
    const int holdPositionKey = ParseHoldPositionKeyName(
        holdPositionValue,
        static_cast<int>(g_holdPositionKey),
        holdPositionName,
        sizeof(holdPositionName));
    InterlockedExchange(&g_holdPositionKey, holdPositionKey);
    lstrcpynA(g_holdPositionKeyName, holdPositionName, sizeof(g_holdPositionKeyName));
    InterlockedExchange(
        &g_disableClickMovement,
        GetPrivateProfileIntW(L"Mouse", L"DisableClickMovement", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_keyboardMousePatchEnabled,
        GetPrivateProfileIntW(L"KeyboardMousePatch", L"EnableKeyboardMousePatch", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_releaseBrakeMode,
        ClampLong(
            GetPrivateProfileIntW(L"Developer", L"ReleaseBrakeMode", kDefaultReleaseBrakeMode, g_iniFile),
            0,
            1));
    InterlockedExchange(
        &g_releaseBrakeDelayMs,
        ClampLong(
            GetPrivateProfileIntW(L"Developer", L"ReleaseBrakeDelayMs", kDefaultReleaseBrakeDelayMs, g_iniFile),
            0,
            250));
    const LONG requestedMinSendMs =
        GetPrivateProfileIntW(L"Developer", L"KeyboardMoveMinSendIntervalMs", kKeyboardMoveMinSendIntervalMs, g_iniFile);
    const LONG clampedMinSendMs = ClampLong(requestedMinSendMs, kKeyboardMoveMinSendIntervalMs, 250);
    if (requestedMinSendMs != clampedMinSendMs) {
        char clampLine[160]{};
        wsprintfA(
            clampLine,
            "KeyboardMoveMinSendIntervalMs=%ld clamped to stable floor %ld.",
            requestedMinSendMs,
            clampedMinSendMs);
        LogLine(clampLine);
    }
    InterlockedExchange(&g_keyboardMoveMinSendIntervalMs, clampedMinSendMs);

    const LONG requestedRepeatMs =
        GetPrivateProfileIntW(L"Developer", L"KeyboardMoveRepeatMs", kKeyboardMoveRepeatMs, g_iniFile);
    const LONG clampedRepeatMs = ClampLong(requestedRepeatMs, kKeyboardMoveRepeatMs, 350);
    if (requestedRepeatMs != clampedRepeatMs) {
        char clampLine[160]{};
        wsprintfA(
            clampLine,
            "KeyboardMoveRepeatMs=%ld clamped to stable floor %ld.",
            requestedRepeatMs,
            clampedRepeatMs);
        LogLine(clampLine);
    }
    InterlockedExchange(&g_keyboardMoveRepeatMs, clampedRepeatMs);
    InterlockedExchange(
        &g_keyboardMoveLeadStepX100,
        ClampLong(
            GetPrivateProfileIntW(L"Developer", L"KeyboardMoveLeadStepX100", static_cast<int>(kKeyboardMoveLeadStep * 100.0f), g_iniFile),
            100,
            1000));
    InterlockedExchange(
        &g_enableEventDrivenCapture,
        GetPrivateProfileIntW(L"ContextCapture", L"EnableEventDrivenCapture", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_requireSetRunningBeforeCapture,
        GetPrivateProfileIntW(L"ContextCapture", L"RequireSetRunningBeforeCapture", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_requirePlayerCamBeforeCapture,
        GetPrivateProfileIntW(L"ContextCapture", L"RequirePlayerCamBeforeCapture", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_requireUiReadyBeforeCapture,
        GetPrivateProfileIntW(L"ContextCapture", L"RequireUiReadyBeforeCapture", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_requireStrictPlayerMarkerAtCapture,
        GetPrivateProfileIntW(L"ContextCapture", L"RequireStrictPlayerMarkerAtCapture", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_enableUiLifecycleProbe,
        GetPrivateProfileIntW(L"ContextCapture", L"EnableUiLifecycleProbe", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_captureOnMapTitleText,
        GetPrivateProfileIntW(L"ContextCapture", L"CaptureOnMapTitleText", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_captureOnLoadingHidden,
        GetPrivateProfileIntW(L"ContextCapture", L"CaptureOnLoadingHidden", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_captureOnHudVisible,
        GetPrivateProfileIntW(L"ContextCapture", L"CaptureOnHudVisible", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_allowPlayerCamFallback,
        GetPrivateProfileIntW(L"ContextCapture", L"AllowPlayerCamFallback", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_uiReadyFallbackDelayMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"PlayerCamFallbackDelayMs", kDefaultUiReadyFallbackDelayMs, g_iniFile), 1000, 30000));
    InterlockedExchange(
        &g_lockPlayerIdentityOnly,
        GetPrivateProfileIntW(L"ContextCapture", L"LockPlayerIdentityOnly", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_resolveArg1AtDispatch,
        GetPrivateProfileIntW(L"ContextCapture", L"ResolveArg1AtDispatch", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_preserveEventLockAcrossRuntimeDips,
        GetPrivateProfileIntW(L"ContextCapture", L"PreserveLockedPlayerAcrossRuntimeDips", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_capturePumpIntervalMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"CapturePollIntervalMs", kDefaultCapturePumpIntervalMs, g_iniFile), 16, 250));
    InterlockedExchange(
        &g_dynamicArg1StableReadsRequired,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"DynamicArg1StableReadsRequired", kDefaultDynamicArg1StableReads, g_iniFile), 1, 8));
    InterlockedExchange(
        &g_dynamicArg1SettleMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"DynamicArg1SettleMs", kDefaultDynamicArg1SettleMs, g_iniFile), 0, 1000));
    wchar_t mapTitleTokenWide[128]{};
    GetPrivateProfileStringW(
        L"ContextCapture",
        L"MapTitleWindowContains",
        L"",
        mapTitleTokenWide,
        static_cast<DWORD>(sizeof(mapTitleTokenWide) / sizeof(mapTitleTokenWide[0])),
        g_iniFile);
    WideCharToMultiByte(
        CP_UTF8,
        0,
        mapTitleTokenWide,
        -1,
        g_mapTitleWindowToken,
        static_cast<int>(sizeof(g_mapTitleWindowToken)),
        nullptr,
        nullptr);
    g_mapTitleWindowToken[sizeof(g_mapTitleWindowToken) - 1] = '\0';
    InterlockedExchange(
        &g_captureStableFrames,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"StableFramesRequired", kDefaultEventCaptureStableFrames, g_iniFile), 1, 12));
    InterlockedExchange(
        &g_captureTimeoutMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"CaptureTimeoutMs", kDefaultEventCaptureTimeoutMs, g_iniFile), 500, 15000));
    InterlockedExchange(
        &g_useExactSetRunningVftableOnly,
        GetPrivateProfileIntW(L"ContextCapture", L"UseExactSetRunningVftableOnly", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_minimumCaptureAgeAfterSetRunningMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"MinimumCaptureAgeAfterSetRunningMs", kDefaultMinimumCaptureAgeAfterSetRunningMs, g_iniFile), 0, 2000));
    InterlockedExchange(
        &g_rejectAdjacentLoadingContext,
        GetPrivateProfileIntW(L"ContextCapture", L"RejectAdjacentLoadingContext", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_adjacentLoadingContextMaxDistance,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"AdjacentLoadingContextMaxDistance", kDefaultAdjacentLoadingContextMaxDistance, g_iniFile), 65536, 2097152));
    InterlockedExchange(
        &g_enableContinuousControllerScan,
        GetPrivateProfileIntW(L"ContextCapture", L"ContinuousControllerScan", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_enableRuntimeReacquire,
        GetPrivateProfileIntW(L"ContextCapture", L"RuntimeReacquire", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_localMoveCanRefreshContext,
        GetPrivateProfileIntW(L"ContextCapture", L"LocalMoveCanRefreshContext", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_enableWasdOwnershipRecovery,
        GetPrivateProfileIntW(L"ContextCapture", L"EnableWasdOwnershipRecovery", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_wasdOwnershipRecoveryWindowMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"WasdOwnershipRecoveryWindowMs", kDefaultWasdOwnershipRecoveryWindowMs, g_iniFile), 500, 10000));
    InterlockedExchange(
        &g_wasdOwnershipRecoveryPollMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"WasdOwnershipRecoveryPollMs", kDefaultWasdOwnershipRecoveryPollMs, g_iniFile), 16, 250));
    InterlockedExchange(
        &g_wasdOwnershipRecoveryStableReadsRequired,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"WasdOwnershipRecoveryStableReads", kDefaultWasdOwnershipRecoveryStableReads, g_iniFile), 2, 8));
    InterlockedExchange(
        &g_wasdOwnershipRecoveryRetryDelayMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"WasdOwnershipRecoveryRetryDelayMs", kDefaultWasdOwnershipRecoveryRetryDelayMs, g_iniFile), 50, 2000));
    InterlockedExchange(
        &g_allowVanillaPlayerMoveOwnershipEvidence,
        GetPrivateProfileIntW(L"ContextCapture", L"UseConfirmedVanillaMoveAsRecoveryEvidence", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_enableEmergencyWasdBootstrap,
        GetPrivateProfileIntW(L"ContextCapture", L"EnableEmergencyWasdBootstrap", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_autoEmergencyBootstrapOnOrphanSetRunning,
        GetPrivateProfileIntW(L"ContextCapture", L"AutoBootstrapOnSetRunningWithoutLevel", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_useSetRunningCoreAsEmergencyController,
        GetPrivateProfileIntW(L"ContextCapture", L"UseSetRunningCoreAsEmergencyController", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_useVanillaMoveAsEmergencyBootstrapEvidence,
        GetPrivateProfileIntW(L"ContextCapture", L"UseVanillaMoveAsEmergencyBootstrapEvidence", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_emergencyWasdBootstrapWindowMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"EmergencyWasdBootstrapWindowMs", kDefaultEmergencyWasdBootstrapWindowMs, g_iniFile), 3000, 120000));
    InterlockedExchange(
        &g_emergencyWasdBootstrapRetryDelayMs,
        ClampLong(GetPrivateProfileIntW(L"ContextCapture", L"EmergencyWasdBootstrapRetryDelayMs", kDefaultEmergencyWasdBootstrapRetryDelayMs, g_iniFile), 100, 5000));
    InterlockedExchange(
        &g_clearContextOnSetLevel,
        GetPrivateProfileIntW(L"ContextCapture", L"ClearContextOnSetLevel", 1, g_iniFile) != 0 ? 1 : 0);

    InterlockedExchange(
        &g_cameraSyncAfterContextLock,
        GetPrivateProfileIntW(L"CameraSync", L"EnableAfterContextLock", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_cameraSyncDelayMs,
        ClampLong(GetPrivateProfileIntW(L"CameraSync", L"DelayAfterContextLockMs", kDefaultCameraSyncDelayMs, g_iniFile), 0, 2000));
    InterlockedExchange(
        &g_cameraSyncSampleIntervalMs,
        ClampLong(GetPrivateProfileIntW(L"CameraSync", L"SampleIntervalMs", kDefaultCameraSyncSampleIntervalMs, g_iniFile), 16, 250));
    InterlockedExchange(
        &g_cameraSyncStableSamplesRequired,
        ClampLong(GetPrivateProfileIntW(L"CameraSync", L"StableSamplesRequired", kDefaultCameraSyncStableSamples, g_iniFile), 1, 12));
    InterlockedExchange(
        &g_cameraSyncAngleToleranceX100,
        ClampLong(GetPrivateProfileIntW(L"CameraSync", L"AngleToleranceDegreesX100", kDefaultCameraSyncAngleToleranceX100, g_iniFile), 1, 500));
    InterlockedExchange(
        &g_cameraSyncTimeoutMs,
        ClampLong(GetPrivateProfileIntW(L"CameraSync", L"TimeoutMs", kDefaultCameraSyncTimeoutMs, g_iniFile), 500, 10000));
    const LONG legacyBlockKeyboardUntilReady =
        GetPrivateProfileIntW(L"CameraSync", L"BlockKeyboardUntilReady", 0, g_iniFile) != 0 ? 1 : 0;
    InterlockedExchange(
        &g_blockKeyboardUntilCameraSync,
        GetPrivateProfileIntW(
            L"CameraSync",
            L"BlockKeyboardUntilFirstEngineObservation",
            legacyBlockKeyboardUntilReady,
            g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_initialCameraObservationTimeoutMs,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"InitialObservationTimeoutMs",
                kDefaultInitialCameraObservationTimeoutMs,
                g_iniFile),
            500,
            10000));
    InterlockedExchange(
        &g_allowProvisionalFallbackAfterInitialTimeout,
        GetPrivateProfileIntW(
            L"CameraSync",
            L"AllowProvisionalFallbackAfterTimeout",
            1,
            g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_enablePostFirstMoveCameraWatch,
        GetPrivateProfileIntW(
            L"CameraSync",
            L"EnablePostFirstMoveTransitionWatch",
            1,
            g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_postFirstMoveCameraWatchMs,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"PostFirstMoveTransitionWatchMs",
                kDefaultPostFirstMoveCameraWatchMs,
                g_iniFile),
            250,
            10000));
    InterlockedExchange(
        &g_runtimeTransitionCameraAcquireTimeoutMs,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"RuntimeTransitionAcquireTimeoutMs",
                kDefaultRuntimeTransitionCameraAcquireTimeoutMs,
                g_iniFile),
            500,
            10000));
    InterlockedExchange(
        &g_runtimeTransitionSameAngleHoldMs,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"RuntimeTransitionSameAngleHoldMs",
                kDefaultRuntimeTransitionSameAngleHoldMs,
                g_iniFile),
            0,
            3000));

    const LONG legacyVerifyAfterFirstMove =
        GetPrivateProfileIntW(L"CameraSync", L"VerifyAfterFirstKeyboardMove", 1, g_iniFile);
    const LONG legacyVerificationDelay =
        GetPrivateProfileIntW(
            L"CameraSync",
            L"VerificationDelayAfterFirstMoveMs",
            kDefaultPostReleaseCameraVerificationDelayMs,
            g_iniFile);
    InterlockedExchange(
        &g_verifyCameraAfterFirstKeyboardRelease,
        GetPrivateProfileIntW(
            L"CameraSync",
            L"VerifyAfterFirstKeyboardRelease",
            legacyVerifyAfterFirstMove,
            g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_postReleaseCameraVerificationDelayMs,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"VerificationDelayAfterReleaseBrakeMs",
                legacyVerificationDelay,
                g_iniFile),
            0,
            3000));
    InterlockedExchange(
        &g_postReleaseContextPollMs,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"PostReleaseContextPollMs",
                kDefaultPostReleaseContextPollMs,
                g_iniFile),
            16,
            250));
    InterlockedExchange(
        &g_postReleaseContextTimeoutMs,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"PostReleaseContextTimeoutMs",
                kDefaultPostReleaseContextTimeoutMs,
                g_iniFile),
            250,
            5000));
    InterlockedExchange(
        &g_postReleaseStableReadsRequired,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"PostReleaseStableReadsRequired",
                kDefaultPostReleaseStableReads,
                g_iniFile),
            1,
            8));
    InterlockedExchange(
        &g_postReleaseQuietMs,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"PostReleaseQuietMs",
                kDefaultPostReleaseQuietMs,
                g_iniFile),
            0,
            2000));
    InterlockedExchange(
        &g_postReleaseCameraVerificationTimeoutMs,
        ClampLong(
            GetPrivateProfileIntW(
                L"CameraSync",
                L"VerificationTimeoutMs",
                kDefaultPostReleaseCameraVerificationTimeoutMs,
                g_iniFile),
            500,
            10000));
    InterlockedExchange(
        &g_blockKeyboardDuringPostReleaseCameraVerification,
        GetPrivateProfileIntW(L"CameraSync", L"BlockKeyboardDuringVerification", 0, g_iniFile) != 0 ? 1 : 0);

    InterlockedExchange(
        &g_resetInteractionFacingOnKeyboardStart,
        GetPrivateProfileIntW(L"InteractionFacing", L"ResetOnKeyboardMoveStart", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_clearSecondaryInteractionTarget,
        GetPrivateProfileIntW(L"InteractionFacing", L"ClearSecondaryTarget", 1, g_iniFile) != 0 ? 1 : 0);

    InterlockedExchange(
        &g_dispatchOnGameWindowThread,
        GetPrivateProfileIntW(L"MovementDispatch", L"DispatchOnGameWindowThread", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_allowWorkerThreadLocalMove,
        GetPrivateProfileIntW(L"MovementDispatch", L"AllowWorkerThreadLocalMove", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_queueLatestInputOnly,
        GetPrivateProfileIntW(L"MovementDispatch", L"QueueLatestInputOnly", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_queuedCommandMaxAgeMs,
        ClampLong(GetPrivateProfileIntW(L"MovementDispatch", L"MaxQueuedCommandAgeMs", kDefaultQueuedCommandMaxAgeMs, g_iniFile), 50, 1000));
    InterlockedExchange(
        &g_requireGameWindowThread,
        GetPrivateProfileIntW(L"MovementDispatch", L"RequireGameWindowThread", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_requireStrictPlayerMarkerAtDispatch,
        GetPrivateProfileIntW(L"MovementDispatch", L"RequireStrictPlayerMarkerAtDispatch", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_validateLocalMoveDispatchList,
        GetPrivateProfileIntW(L"MovementDispatch", L"ValidateSelfDispatchList", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_localMoveDispatchListOffset,
        ClampLong(GetPrivateProfileIntW(L"MovementDispatch", L"DispatchListOffset", 1712, g_iniFile), 0, 4096));
    InterlockedExchange(
        &g_localMoveDispatchListMaxEntries,
        ClampLong(GetPrivateProfileIntW(L"MovementDispatch", L"DispatchListMaxEntries", 128, g_iniFile), 1, 512));
    InterlockedExchange(
        &g_logThreadAffinity,
        GetPrivateProfileIntW(L"Diagnostics", L"LogThreadAffinity", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_logCaptureLifecycle,
        GetPrivateProfileIntW(L"Diagnostics", L"LogContextCapture", 1, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_logUiLifecycle,
        GetPrivateProfileIntW(L"Diagnostics", L"LogUiLifecycle", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_logAllUiTextDuringLoad,
        GetPrivateProfileIntW(L"Diagnostics", L"LogAllUiTextDuringLoad", 0, g_iniFile) != 0 ? 1 : 0);

    const LONG activeRangeConfigured =
        GetPrivateProfileIntW(L"EnginePatch", L"EnableActiveRangePatch", 0, g_iniFile) != 0 ? 1 : 0;
    InterlockedExchange(&g_activeRangePatchConfigured, activeRangeConfigured);
    InterlockedExchange(
        &g_enableMemoryWatchdog,
        GetPrivateProfileIntW(L"Diagnostics", L"EnableMemoryWatchdog", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_enableTransitionDiagnosticsRuntime,
        GetPrivateProfileIntW(L"Diagnostics", L"EnableTransitionDiagnostics", 0, g_iniFile) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_enableTransitionMessageHook,
        GetPrivateProfileIntW(L"Diagnostics", L"EnableTransitionMessageHook", 1, g_iniFile) != 0 ? 1 : 0);

    // Legacy INI files explicitly enabled the high-volume UI trace and required
    // the delayed PlayerCam fallback. A user replacing only the DLL would
    // therefore keep the freezes and eight-second delay. Treat an unversioned
    // configuration as legacy and force the safe event architecture until the
    // matching INI is installed.
    if (configRevision < 3) {
        InterlockedExchange(&g_enableEventDrivenCapture, 1);
        InterlockedExchange(&g_requireSetRunningBeforeCapture, 1);
        InterlockedExchange(&g_useExactSetRunningVftableOnly, 1);
        InterlockedExchange(&g_minimumCaptureAgeAfterSetRunningMs, kDefaultMinimumCaptureAgeAfterSetRunningMs);
        InterlockedExchange(&g_requireUiReadyBeforeCapture, 0);
        InterlockedExchange(&g_requirePlayerCamBeforeCapture, 0);
        InterlockedExchange(&g_enableUiLifecycleProbe, 0);
        InterlockedExchange(&g_allowPlayerCamFallback, 0);
        InterlockedExchange(&g_rejectAdjacentLoadingContext, 1);
        InterlockedExchange(&g_adjacentLoadingContextMaxDistance, kDefaultAdjacentLoadingContextMaxDistance);
        InterlockedExchange(&g_capturePumpIntervalMs, kDefaultCapturePumpIntervalMs);
        InterlockedExchange(&g_captureStableFrames, kDefaultEventCaptureStableFrames);
        InterlockedExchange(&g_captureTimeoutMs, kDefaultEventCaptureTimeoutMs);
        InterlockedExchange(&g_lockPlayerIdentityOnly, 1);
        InterlockedExchange(&g_resolveArg1AtDispatch, 1);
        InterlockedExchange(&g_dynamicArg1StableReadsRequired, kDefaultDynamicArg1StableReads);
        InterlockedExchange(&g_dynamicArg1SettleMs, kDefaultDynamicArg1SettleMs);
        InterlockedExchange(&g_preserveEventLockAcrossRuntimeDips, 1);
        InterlockedExchange(&g_enableContinuousControllerScan, 0);
        InterlockedExchange(&g_enableRuntimeReacquire, 0);
        InterlockedExchange(&g_localMoveCanRefreshContext, 0);
        InterlockedExchange(&g_enableWasdOwnershipRecovery, 1);
        InterlockedExchange(&g_wasdOwnershipRecoveryWindowMs, kDefaultWasdOwnershipRecoveryWindowMs);
        InterlockedExchange(&g_wasdOwnershipRecoveryPollMs, kDefaultWasdOwnershipRecoveryPollMs);
        InterlockedExchange(&g_wasdOwnershipRecoveryStableReadsRequired, kDefaultWasdOwnershipRecoveryStableReads);
        InterlockedExchange(&g_wasdOwnershipRecoveryRetryDelayMs, kDefaultWasdOwnershipRecoveryRetryDelayMs);
        InterlockedExchange(&g_allowVanillaPlayerMoveOwnershipEvidence, 1);
        InterlockedExchange(&g_dispatchOnGameWindowThread, 1);
        InterlockedExchange(&g_allowWorkerThreadLocalMove, 0);
        InterlockedExchange(&g_requireStrictPlayerMarkerAtCapture, 0);
        InterlockedExchange(&g_requireStrictPlayerMarkerAtDispatch, 0);
        InterlockedExchange(&g_validateLocalMoveDispatchList, 1);
        InterlockedExchange(&g_logThreadAffinity, 0);
        InterlockedExchange(&g_logUiLifecycle, 0);
        InterlockedExchange(&g_logAllUiTextDuringLoad, 0);
        InterlockedExchange(&g_cameraSyncAfterContextLock, 1);
        InterlockedExchange(&g_cameraSyncDelayMs, kDefaultCameraSyncDelayMs);
        InterlockedExchange(&g_cameraSyncSampleIntervalMs, kDefaultCameraSyncSampleIntervalMs);
        InterlockedExchange(&g_cameraSyncStableSamplesRequired, kDefaultCameraSyncStableSamples);
        InterlockedExchange(&g_cameraSyncAngleToleranceX100, kDefaultCameraSyncAngleToleranceX100);
        InterlockedExchange(&g_cameraSyncTimeoutMs, kDefaultCameraSyncTimeoutMs);
        InterlockedExchange(&g_blockKeyboardUntilCameraSync, 0);
        InterlockedExchange(&g_initialCameraObservationTimeoutMs, kDefaultInitialCameraObservationTimeoutMs);
        InterlockedExchange(&g_allowProvisionalFallbackAfterInitialTimeout, 1);
        InterlockedExchange(&g_enablePostFirstMoveCameraWatch, 1);
        InterlockedExchange(&g_postFirstMoveCameraWatchMs, kDefaultPostFirstMoveCameraWatchMs);
        InterlockedExchange(&g_runtimeTransitionCameraAcquireTimeoutMs, kDefaultRuntimeTransitionCameraAcquireTimeoutMs);
        InterlockedExchange(&g_runtimeTransitionSameAngleHoldMs, kDefaultRuntimeTransitionSameAngleHoldMs);
        InterlockedExchange(&g_verifyCameraAfterFirstKeyboardRelease, 1);
        InterlockedExchange(&g_postReleaseCameraVerificationDelayMs, kDefaultPostReleaseCameraVerificationDelayMs);
        InterlockedExchange(&g_postReleaseContextPollMs, kDefaultPostReleaseContextPollMs);
        InterlockedExchange(&g_postReleaseContextTimeoutMs, kDefaultPostReleaseContextTimeoutMs);
        InterlockedExchange(&g_postReleaseStableReadsRequired, kDefaultPostReleaseStableReads);
        InterlockedExchange(&g_postReleaseQuietMs, kDefaultPostReleaseQuietMs);
        InterlockedExchange(&g_postReleaseCameraVerificationTimeoutMs, kDefaultPostReleaseCameraVerificationTimeoutMs);
        InterlockedExchange(&g_blockKeyboardDuringPostReleaseCameraVerification, 0);
        InterlockedExchange(&g_resetInteractionFacingOnKeyboardStart, 1);
        InterlockedExchange(&g_clearSecondaryInteractionTarget, 1);
        char revisionLine[256]{};
        wsprintfA(
            revisionLine,
            "Legacy/unversioned INI detected: safe capture, logging, camera-sync and interaction-facing defaults forced. Replace the INI with the supplied ConfigRevision=%ld file.",
            kCurrentConfigRevision);
        LogLine(revisionLine);
    }

    const LONG configuredMapRenderProbe =
        configRevision >= 3 &&
        GetPrivateProfileIntW(L"Diagnostics", L"EnableMapRenderReadyProbe", 0, g_iniFile) != 0 ? 1 : 0;
    const LONG eventCaptureNeedsPlayerCam =
        InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0 &&
        (InterlockedCompareExchange(&g_requirePlayerCamBeforeCapture, 0, 0) != 0 ||
            InterlockedCompareExchange(&g_allowPlayerCamFallback, 0, 0) != 0);
    InterlockedExchange(
        &g_enableMapRenderReadyProbe,
        configuredMapRenderProbe != 0 || eventCaptureNeedsPlayerCam ? 1 : 0);
    const LONG legacyCameraAutoCalibration =
        GetPrivateProfileIntW(L"Diagnostics", L"EnableCameraAutoCalibration", 0, g_iniFile) != 0 ? 1 : 0;
    InterlockedExchange(
        &g_enableCameraAutoCalibration,
        legacyCameraAutoCalibration != 0 ||
            InterlockedCompareExchange(&g_cameraSyncAfterContextLock, 0, 0) != 0 ? 1 : 0);
    InterlockedExchange(
        &g_enableOgreLogLoadSignals,
        GetPrivateProfileIntW(L"Diagnostics", L"EnableOgreLogLoadSignals", 0, g_iniFile) != 0 ? 1 : 0);
    if (InterlockedCompareExchange(&g_activeRangePatchStartupCaptured, 1, 1) != 0) {
        const LONG activeRangeAtStartup =
            InterlockedCompareExchange(&g_activeRangePatchEnabledAtStartup, 0, 0);
        InterlockedExchange(
            &g_activeRangePatchRestartRequired,
            activeRangeConfigured != activeRangeAtStartup ? 1 : 0);
    }
    UpdateWindowsKeyboardLayoutName(GetGameKeyboardLayout());

    char line[2300]{};
    wsprintfA(
        line,
        "Keyboard config loaded: mode=%s layout=%s Left=%s Up=%s Right=%s Down=%s HoldPosition=%s keyboardMousePatch=%s clickMovement=%s activeRangeX2=%s%s backend=LocalMove releaseBrake=%ld brakeDelayMs=%ld minSendMs=%ld repeatMs=%ld stepX100=%ld diagnostics(memory=%ld transition=%ld transitionHook=%ld mapRender=%ld cameraAuto=%ld ogreLog=%ld ui=%ld uiAllText=%ld) context(event=%ld setRunning=%ld exactRunning=%ld minAgeMs=%ld rejectAdjacent=%ld adjacentMax=%ld uiReady=%ld captureStrict=%ld playerCam=%ld mapTitle=%ld loadingHidden=%ld hudVisible=%ld camFallback=%ld fallbackMs=%ld stableFrames=%ld identityOnly=%ld dynamicArg1=%ld preserveLock=%ld capturePollMs=%ld arg1StableReads=%ld arg1SettleMs=%ld continuousScan=%ld runtimeReacquire=%ld localMoveRefresh=%ld wasdRecovery=%ld recoveryWindowMs=%ld recoveryPollMs=%ld recoveryStableReads=%ld recoveryRetryMs=%ld vanillaEvidence=%ld) cameraSync(afterLock=%ld delayMs=%ld sampleMs=%ld stableSamples=%ld toleranceX100=%ld timeoutMs=%ld block=%ld) interactionFacing(resetOnMoveStart=%ld clearSecondary=%ld) dispatch(gameWindow=%ld workerDirect=%ld strictMarker=%ld listGuard=%ld listOffset=%ld listMax=%ld maxAgeMs=%ld).",
        InterlockedCompareExchange(&g_usePhysicalKeyboardLayout, 0, 0) != 0 ? "Windows physical" : "virtual",
        g_windowsKeyboardLayoutName,
        g_keyLeftName,
        g_keyUpName,
        g_keyRightName,
        g_keyDownName,
        g_holdPositionKeyName,
        InterlockedCompareExchange(&g_keyboardMousePatchEnabled, 0, 0) != 0 ? "enabled" : "disabled",
        InterlockedCompareExchange(&g_disableClickMovement, 0, 0) != 0 ? "disabled" : "enabled",
        activeRangeConfigured != 0 ? "enabled" : "disabled",
        InterlockedCompareExchange(&g_activeRangePatchRestartRequired, 0, 0) != 0 ? " restart-required" : "",
        InterlockedCompareExchange(&g_releaseBrakeMode, 0, 0),
        InterlockedCompareExchange(&g_releaseBrakeDelayMs, 0, 0),
        InterlockedCompareExchange(&g_keyboardMoveMinSendIntervalMs, 0, 0),
        InterlockedCompareExchange(&g_keyboardMoveRepeatMs, 0, 0),
        InterlockedCompareExchange(&g_keyboardMoveLeadStepX100, 0, 0),
        InterlockedCompareExchange(&g_enableMemoryWatchdog, 0, 0),
        InterlockedCompareExchange(&g_enableTransitionDiagnosticsRuntime, 0, 0),
        InterlockedCompareExchange(&g_enableTransitionMessageHook, 0, 0),
        InterlockedCompareExchange(&g_enableMapRenderReadyProbe, 0, 0),
        InterlockedCompareExchange(&g_enableCameraAutoCalibration, 0, 0),
        InterlockedCompareExchange(&g_enableOgreLogLoadSignals, 0, 0),
        InterlockedCompareExchange(&g_logUiLifecycle, 0, 0),
        InterlockedCompareExchange(&g_logAllUiTextDuringLoad, 0, 0),
        InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0),
        InterlockedCompareExchange(&g_requireSetRunningBeforeCapture, 0, 0),
        InterlockedCompareExchange(&g_useExactSetRunningVftableOnly, 0, 0),
        InterlockedCompareExchange(&g_minimumCaptureAgeAfterSetRunningMs, 0, 0),
        InterlockedCompareExchange(&g_rejectAdjacentLoadingContext, 0, 0),
        InterlockedCompareExchange(&g_adjacentLoadingContextMaxDistance, 0, 0),
        InterlockedCompareExchange(&g_requireUiReadyBeforeCapture, 0, 0),
        InterlockedCompareExchange(&g_requireStrictPlayerMarkerAtCapture, 0, 0),
        InterlockedCompareExchange(&g_requirePlayerCamBeforeCapture, 0, 0),
        InterlockedCompareExchange(&g_captureOnMapTitleText, 0, 0),
        InterlockedCompareExchange(&g_captureOnLoadingHidden, 0, 0),
        InterlockedCompareExchange(&g_captureOnHudVisible, 0, 0),
        InterlockedCompareExchange(&g_allowPlayerCamFallback, 0, 0),
        InterlockedCompareExchange(&g_uiReadyFallbackDelayMs, 0, 0),
        InterlockedCompareExchange(&g_captureStableFrames, 0, 0),
        InterlockedCompareExchange(&g_lockPlayerIdentityOnly, 0, 0),
        InterlockedCompareExchange(&g_resolveArg1AtDispatch, 0, 0),
        InterlockedCompareExchange(&g_preserveEventLockAcrossRuntimeDips, 0, 0),
        InterlockedCompareExchange(&g_capturePumpIntervalMs, 0, 0),
        InterlockedCompareExchange(&g_dynamicArg1StableReadsRequired, 0, 0),
        InterlockedCompareExchange(&g_dynamicArg1SettleMs, 0, 0),
        InterlockedCompareExchange(&g_enableContinuousControllerScan, 0, 0),
        InterlockedCompareExchange(&g_enableRuntimeReacquire, 0, 0),
        InterlockedCompareExchange(&g_localMoveCanRefreshContext, 0, 0),
        InterlockedCompareExchange(&g_enableWasdOwnershipRecovery, 0, 0),
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryWindowMs, 0, 0),
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryPollMs, 0, 0),
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryStableReadsRequired, 0, 0),
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryRetryDelayMs, 0, 0),
        InterlockedCompareExchange(&g_allowVanillaPlayerMoveOwnershipEvidence, 0, 0),
        InterlockedCompareExchange(&g_cameraSyncAfterContextLock, 0, 0),
        InterlockedCompareExchange(&g_cameraSyncDelayMs, 0, 0),
        InterlockedCompareExchange(&g_cameraSyncSampleIntervalMs, 0, 0),
        InterlockedCompareExchange(&g_cameraSyncStableSamplesRequired, 0, 0),
        InterlockedCompareExchange(&g_cameraSyncAngleToleranceX100, 0, 0),
        InterlockedCompareExchange(&g_cameraSyncTimeoutMs, 0, 0),
        InterlockedCompareExchange(&g_blockKeyboardUntilCameraSync, 0, 0),
        InterlockedCompareExchange(&g_resetInteractionFacingOnKeyboardStart, 0, 0),
        InterlockedCompareExchange(&g_clearSecondaryInteractionTarget, 0, 0),
        InterlockedCompareExchange(&g_dispatchOnGameWindowThread, 0, 0),
        InterlockedCompareExchange(&g_allowWorkerThreadLocalMove, 0, 0),
        InterlockedCompareExchange(&g_requireStrictPlayerMarkerAtDispatch, 0, 0),
        InterlockedCompareExchange(&g_validateLocalMoveDispatchList, 0, 0),
        InterlockedCompareExchange(&g_localMoveDispatchListOffset, 0, 0),
        InterlockedCompareExchange(&g_localMoveDispatchListMaxEntries, 0, 0),
        InterlockedCompareExchange(&g_queuedCommandMaxAgeMs, 0, 0));
    LogLine(line);
    char cameraVerifyLine[512]{};
    wsprintfA(
        cameraVerifyLine,
        "Camera confidence verification: firstEngineGate=%ld initialTimeoutMs=%ld provisionalFallback=%ld postFirstMoveWatch=%ld watchMs=%ld runtimeAcquireTimeoutMs=%ld runtimeSameAngleHoldMs=%ld afterFirstRelease=%ld delayMs=%ld contextPollMs=%ld contextTimeoutMs=%ld stableReads=%ld quietMs=%ld acquireTimeoutMs=%ld postReleaseBlock=%ld provisionalAngle=-45 initialObservationNonConfirming=1 engineConfirmationsRequired=2 noPermanentCameraProbe=1.",
        InterlockedCompareExchange(&g_blockKeyboardUntilCameraSync, 0, 0),
        InterlockedCompareExchange(&g_initialCameraObservationTimeoutMs, 0, 0),
        InterlockedCompareExchange(&g_allowProvisionalFallbackAfterInitialTimeout, 0, 0),
        InterlockedCompareExchange(&g_enablePostFirstMoveCameraWatch, 0, 0),
        InterlockedCompareExchange(&g_postFirstMoveCameraWatchMs, 0, 0),
        InterlockedCompareExchange(&g_runtimeTransitionCameraAcquireTimeoutMs, 0, 0),
        InterlockedCompareExchange(&g_runtimeTransitionSameAngleHoldMs, 0, 0),
        InterlockedCompareExchange(&g_verifyCameraAfterFirstKeyboardRelease, 0, 0),
        InterlockedCompareExchange(&g_postReleaseCameraVerificationDelayMs, 0, 0),
        InterlockedCompareExchange(&g_postReleaseContextPollMs, 0, 0),
        InterlockedCompareExchange(&g_postReleaseContextTimeoutMs, 0, 0),
        InterlockedCompareExchange(&g_postReleaseStableReadsRequired, 0, 0),
        InterlockedCompareExchange(&g_postReleaseQuietMs, 0, 0),
        InterlockedCompareExchange(&g_postReleaseCameraVerificationTimeoutMs, 0, 0),
        InterlockedCompareExchange(&g_blockKeyboardDuringPostReleaseCameraVerification, 0, 0));
    LogLine(cameraVerifyLine);
    char emergencyBootstrapLine[640]{};
    wsprintfA(
        emergencyBootstrapLine,
        "Emergency WASD bootstrap: enabled=%ld autoOrphanSetRunning=%ld setRunningCore=%ld vanillaEvidence=%ld windowMs=%ld retryMs=%ld F10Manual=1 syntheticGeneration=1 boundedOnly=1.",
        InterlockedCompareExchange(&g_enableEmergencyWasdBootstrap, 0, 0),
        InterlockedCompareExchange(&g_autoEmergencyBootstrapOnOrphanSetRunning, 0, 0),
        InterlockedCompareExchange(&g_useSetRunningCoreAsEmergencyController, 0, 0),
        InterlockedCompareExchange(&g_useVanillaMoveAsEmergencyBootstrapEvidence, 0, 0),
        InterlockedCompareExchange(&g_emergencyWasdBootstrapWindowMs, 0, 0),
        InterlockedCompareExchange(&g_emergencyWasdBootstrapRetryDelayMs, 0, 0));
    LogLine(emergencyBootstrapLine);
    char overlayConfigLine[384]{};
    wsprintfA(
        overlayConfigLine,
        "Overlay UI: fullClientDim=1 scaleMode=%s opacity=%ld showAtStartup=%ld startupDelayMs=%ld geometryStableMs=%ld repositionDebounceMs=%ld F5ScaleCycle=1 doubleBuffered=1.",
        OverlayFontScaleModeLabel(InterlockedCompareExchange(&g_overlayFontScaleMode, 0, 0)),
        InterlockedCompareExchange(&g_overlayOpacity, 0, 0),
        InterlockedCompareExchange(&g_overlayShowAtStartup, 0, 0),
        InterlockedCompareExchange(&g_overlayStartupDelayMs, 0, 0),
        InterlockedCompareExchange(&g_overlayGeometryStableMs, 0, 0),
        InterlockedCompareExchange(&g_overlayRepositionDebounceMs, 0, 0));
    LogLine(overlayConfigLine);
    if (g_mapTitleWindowToken[0] != '\0') {
        char tokenLine[256]{};
        wsprintfA(tokenLine, "UI map-title exact filter from INI: window contains \"%s\".", g_mapTitleWindowToken);
        LogLine(tokenLine);
    }
}

bool GetIniWriteTime(FILETIME* writeTimeOut) {
    if (writeTimeOut == nullptr || g_iniFile[0] == L'\0') {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(g_iniFile, GetFileExInfoStandard, &data)) {
        return false;
    }

    *writeTimeOut = data.ftLastWriteTime;
    return true;
}

void RefreshIniWriteTime() {
    FILETIME writeTime{};
    if (GetIniWriteTime(&writeTime)) {
        g_lastIniWriteTime = writeTime;
    }
}

void SetActiveRangePatchConfigured(bool enabled, bool persist) {
    InterlockedExchange(&g_activeRangePatchConfigured, enabled ? 1 : 0);
    const LONG activeRangeAtStartup =
        InterlockedCompareExchange(&g_activeRangePatchEnabledAtStartup, 0, 0);
    InterlockedExchange(
        &g_activeRangePatchRestartRequired,
        (enabled ? 1 : 0) != activeRangeAtStartup ? 1 : 0);

    if (persist && g_iniFile[0] != L'\0') {
        WritePrivateProfileStringW(
            L"EnginePatch",
            L"EnableActiveRangePatch",
            enabled ? L"1" : L"0",
            g_iniFile);
        RefreshIniWriteTime();
    }

    LogLine(
        enabled
            ? "ActiveRangePatch: config set to enabled. RESTART REQUIRED: close Torchlight II completely and launch it again to apply active range x1.5."
            : "ActiveRangePatch: config set to disabled. RESTART REQUIRED: close Torchlight II completely and launch it again to remove active range x1.5.");
}

void ToggleActiveRangePatchConfigured() {
    const bool currentlyEnabled =
        InterlockedCompareExchange(&g_activeRangePatchConfigured, 0, 0) != 0;
    SetActiveRangePatchConfigured(!currentlyEnabled, true);
}

void SetKeyboardMousePatchEnabled(bool enabled, bool persist) {
    const bool previouslyEnabled =
        InterlockedCompareExchange(&g_keyboardMousePatchEnabled, 0, 0) != 0;
    InterlockedExchange(&g_keyboardMousePatchEnabled, enabled ? 1 : 0);
    if (persist && g_iniFile[0] != L'\0') {
        WritePrivateProfileStringW(
            L"KeyboardMousePatch",
            L"EnableKeyboardMousePatch",
            enabled ? L"1" : L"0",
            g_iniFile);
        RefreshIniWriteTime();
    }
    LogLine(enabled ? "Keyboard/mouse movement patch enabled." : "Keyboard/mouse movement patch disabled.");

    // F6 is also an explicit recovery gesture. Re-enabling the patch must not
    // merely restore input polling while leaving a broken first-map ownership
    // state untouched. The same bounded emergency bootstrap used by F10 is
    // restarted immediately, without enabling any permanent controller probe.
    if (enabled && !previouslyEnabled) {
        RequestWasdBootstrapAfterPatchReenable(GetTickCount());
    }
}

void ToggleKeyboardMousePatchEnabled() {
    const bool currentlyEnabled =
        InterlockedCompareExchange(&g_keyboardMousePatchEnabled, 0, 0) != 0;
    SetKeyboardMousePatchEnabled(!currentlyEnabled, true);
}

void SetOverlayFontScaleMode(LONG mode, bool persist) {
    const LONG clamped = ClampLong(mode, 0, 3);
    InterlockedExchange(&g_overlayFontScaleMode, clamped);
    if (persist && g_iniFile[0] != L'\0') {
        wchar_t value[16]{};
        _snwprintf_s(value, _countof(value), _TRUNCATE, L"%ld", clamped);
        WritePrivateProfileStringW(L"Overlay", L"TextScaleMode", value, g_iniFile);
        RefreshIniWriteTime();
    }
    if (g_overlayWindow != nullptr && IsWindow(g_overlayWindow)) {
        InvalidateRect(g_overlayWindow, nullptr, FALSE);
    }
    char line[192]{};
    wsprintfA(line, "Overlay text scale changed: mode=%s (%ld).", OverlayFontScaleModeLabel(clamped), clamped);
    LogLine(line);
}

void CycleOverlayFontScaleMode() {
    LONG mode = InterlockedCompareExchange(&g_overlayFontScaleMode, 0, 0);
    mode = (mode + 1) % 4;
    SetOverlayFontScaleMode(mode, true);
}

void CheckKeyboardConfigReload() {
    const DWORD now = GetTickCount();
    if (now - g_lastIniReloadCheckTick < 1000) {
        return;
    }
    g_lastIniReloadCheckTick = now;

    FILETIME writeTime{};
    if (!GetIniWriteTime(&writeTime)) {
        return;
    }

    if (CompareFileTime(&writeTime, &g_lastIniWriteTime) == 0) {
        return;
    }

    g_lastIniWriteTime = writeTime;
    LoadKeyboardConfig();
    LogLine("Keyboard config reloaded after external ini change.");
}

void SetClickMovementDisabled(bool disabled, bool persist) {
    InterlockedExchange(&g_disableClickMovement, disabled ? 1 : 0);
    if (persist && g_iniFile[0] != L'\0') {
        WritePrivateProfileStringW(
            L"Mouse",
            L"DisableClickMovement",
            disabled ? L"1" : L"0",
            g_iniFile);
        RefreshIniWriteTime();
    }
    LogLine(disabled ? "Movement by left click disabled." : "Movement by left click enabled.");
}

void ToggleClickMovementDisabled() {
    const bool currentlyDisabled =
        InterlockedCompareExchange(&g_disableClickMovement, 0, 0) != 0;
    SetClickMovementDisabled(!currentlyDisabled, true);
}

LONG ClampLong(LONG value, LONG minimum, LONG maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

bool IsVirtualKeyDown(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool IsConfiguredHoldPositionKeyDown() {
    const int key = static_cast<int>(InterlockedCompareExchange(&g_holdPositionKey, 0, 0));
    switch (key) {
        case VK_SHIFT:
            return IsVirtualKeyDown(VK_SHIFT) ||
                   IsVirtualKeyDown(VK_LSHIFT) ||
                   IsVirtualKeyDown(VK_RSHIFT);
        case VK_CONTROL:
            return IsVirtualKeyDown(VK_CONTROL) ||
                   IsVirtualKeyDown(VK_LCONTROL) ||
                   IsVirtualKeyDown(VK_RCONTROL);
        case VK_MENU:
            return IsVirtualKeyDown(VK_MENU) ||
                   IsVirtualKeyDown(VK_LMENU) ||
                   IsVirtualKeyDown(VK_RMENU);
        default:
            return key != 0 && IsVirtualKeyDown(key);
    }
}

struct KeyboardInputSnapshot {
    bool gameFocusedForInput = false;
    bool holdPositionDown = false;
    bool upMoveDown = false;
    bool leftMoveDown = false;
    bool downMoveDown = false;
    bool rightMoveDown = false;
    int moveMask = 0;
};

KeyboardInputSnapshot CaptureKeyboardInputSnapshot() {
    KeyboardInputSnapshot snapshot{};
    snapshot.gameFocusedForInput = IsGameWindowForegroundForInput();
    snapshot.holdPositionDown = IsConfiguredHoldPositionKeyDown();

    const int keyUp = static_cast<int>(InterlockedCompareExchange(&g_keyUp, 0, 0));
    const int keyLeft = static_cast<int>(InterlockedCompareExchange(&g_keyLeft, 0, 0));
    const int keyDown = static_cast<int>(InterlockedCompareExchange(&g_keyDown, 0, 0));
    const int keyRight = static_cast<int>(InterlockedCompareExchange(&g_keyRight, 0, 0));
    snapshot.upMoveDown = IsVirtualKeyDown(keyUp);
    snapshot.leftMoveDown = IsVirtualKeyDown(keyLeft);
    snapshot.downMoveDown = IsVirtualKeyDown(keyDown);
    snapshot.rightMoveDown = IsVirtualKeyDown(keyRight);

    if (snapshot.upMoveDown) {
        snapshot.moveMask |= 1;
    }
    if (snapshot.leftMoveDown) {
        snapshot.moveMask |= 2;
    }
    if (snapshot.downMoveDown) {
        snapshot.moveMask |= 4;
    }
    if (snapshot.rightMoveDown) {
        snapshot.moveMask |= 8;
    }
    return snapshot;
}

bool IsHoldPositionVanillaBypassActive(DWORD now) {
    return g_holdPositionVanillaBypassUntilTick != 0 &&
        static_cast<LONG>(now - g_holdPositionVanillaBypassUntilTick) < 0;
}

void ExtendHoldPositionVanillaBypass(DWORD now) {
    const DWORD until = now + kHoldPositionVanillaBypassMs;
    if (g_holdPositionVanillaBypassUntilTick == 0 ||
        static_cast<LONG>(until - g_holdPositionVanillaBypassUntilTick) > 0) {
        g_holdPositionVanillaBypassUntilTick = until;
    }
}

bool ApplyHoldPositionSnapshotToMod(const KeyboardInputSnapshot& snapshot, DWORD now) {
    InterlockedExchange(&g_holdPositionSuspended, snapshot.holdPositionDown ? 1 : 0);
    if (snapshot.holdPositionDown) {
        ExtendHoldPositionVanillaBypass(now);
        InterlockedIncrement(&g_holdPositionSuspensionActivations);
    }
    return snapshot.holdPositionDown;
}

extern "C" LONG ShouldBypassStationaryClickFallbackForHoldPosition() {
    const DWORD now = GetTickCount();
    if (IsConfiguredHoldPositionKeyDown()) {
        InterlockedExchange(&g_holdPositionSuspended, 1);
        ExtendHoldPositionVanillaBypass(now);
        InterlockedIncrement(&g_holdPositionSuspensionActivations);
        InterlockedIncrement(&g_holdPositionVanillaBypassActivations);
        return 1;
    }
    if (IsHoldPositionVanillaBypassActive(now)) {
        InterlockedIncrement(&g_holdPositionVanillaBypassActivations);
        return 1;
    }
    return 0;
}

void RefreshCurrentPresetFromKeys() {
    if (InterlockedCompareExchange(&g_usePhysicalKeyboardLayout, 0, 0) != 0) {
        const bool standardPhysicalLayout =
            g_scanUp == kStandardScanUp &&
            g_scanLeft == kStandardScanLeft &&
            g_scanDown == kStandardScanDown &&
            g_scanRight == kStandardScanRight;
        g_currentPresetIndex = standardPhysicalLayout ? 0 : -1;
        return;
    }

    g_currentPresetIndex = -1;
    for (int i = 0; i < kKeyboardPresetCount; ++i) {
        const KeyboardPreset& preset = kKeyboardPresets[i];
        if (!preset.useWindowsLayout &&
            g_keyUp == preset.up &&
            g_keyLeft == preset.left &&
            g_keyDown == preset.down &&
            g_keyRight == preset.right) {
            g_currentPresetIndex = i;
            return;
        }
    }
}

const char* CurrentPresetLabel() {
    switch (g_currentPresetIndex) {
        case 0: return "Windows Auto";
        case 1: return "Arrow keys";
        default: break;
    }
    return InterlockedCompareExchange(&g_usePhysicalKeyboardLayout, 0, 0) != 0
        ? "Custom physical"
        : "Custom";
}

bool KeyCodeToWideName(int key, wchar_t* name, size_t nameCount) {
    if (name == nullptr || nameCount == 0) {
        return false;
    }

    if (key >= 'A' && key <= 'Z') {
        name[0] = static_cast<wchar_t>(key);
        name[1] = L'\0';
        return true;
    }

    const wchar_t* text = nullptr;
    switch (key) {
        case VK_LEFT: text = L"LEFT"; break;
        case VK_UP: text = L"UP"; break;
        case VK_RIGHT: text = L"RIGHT"; break;
        case VK_DOWN: text = L"DOWN"; break;
        case VK_SHIFT: text = L"SHIFT"; break;
        case VK_LSHIFT: text = L"LSHIFT"; break;
        case VK_RSHIFT: text = L"RSHIFT"; break;
        case VK_CONTROL: text = L"CTRL"; break;
        case VK_LCONTROL: text = L"LCTRL"; break;
        case VK_RCONTROL: text = L"RCTRL"; break;
        case VK_MENU: text = L"ALT"; break;
        case VK_LMENU: text = L"LALT"; break;
        case VK_RMENU: text = L"RALT"; break;
        case VK_SPACE: text = L"SPACE"; break;
        case VK_F1: text = L"F1"; break;
        case VK_F2: text = L"F2"; break;
        case VK_F3: text = L"F3"; break;
        case VK_F4: text = L"F4"; break;
        case VK_F5: text = L"F5"; break;
        case VK_F6: text = L"F6"; break;
        case VK_F7: text = L"F7"; break;
        case VK_F8: text = L"F8"; break;
        case VK_F9: text = L"F9"; break;
        case VK_F10: text = L"F10"; break;
        case VK_F11: text = L"F11"; break;
        case VK_F12: text = L"F12"; break;
        default: break;
    }

    if (text == nullptr) {
        return false;
    }

    lstrcpynW(name, text, static_cast<int>(nameCount));
    return true;
}

bool IsAnyCustomCandidateDown() {
    for (int key = 'A'; key <= 'Z'; ++key) {
        if ((GetAsyncKeyState(key) & 0x8000) != 0) {
            return true;
        }
    }

    const int arrows[] = {VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN};
    for (int i = 0; i < 4; ++i) {
        if ((GetAsyncKeyState(arrows[i]) & 0x8000) != 0) {
            return true;
        }
    }

    return false;
}

bool IsAnyHoldPositionCandidateDown() {
    const int candidates[] = {
        VK_SHIFT,
        VK_CONTROL,
        VK_MENU,
        VK_SPACE,
        VK_LEFT,
        VK_UP,
        VK_RIGHT,
        VK_DOWN,
        VK_F1,
        VK_F2,
        VK_F3,
        VK_F4,
        VK_F5,
        VK_F6,
        VK_F9,
        VK_F10,
        VK_F11,
        VK_F12};
    for (int i = 0; i < static_cast<int>(sizeof(candidates) / sizeof(candidates[0])); ++i) {
        if ((GetAsyncKeyState(candidates[i]) & 0x8000) != 0) {
            return true;
        }
    }

    for (int key = 'A'; key <= 'Z'; ++key) {
        if ((GetAsyncKeyState(key) & 0x8000) != 0) {
            return true;
        }
    }

    return false;
}

bool TryCaptureCustomKey(int* keyOut, wchar_t* nameOut, size_t nameOutCount) {
    for (int key = 'A'; key <= 'Z'; ++key) {
        if ((GetAsyncKeyState(key) & 0x8000) != 0 && KeyCodeToWideName(key, nameOut, nameOutCount)) {
            *keyOut = key;
            return true;
        }
    }

    const int arrows[] = {VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN};
    for (int i = 0; i < 4; ++i) {
        const int key = arrows[i];
        if ((GetAsyncKeyState(key) & 0x8000) != 0 && KeyCodeToWideName(key, nameOut, nameOutCount)) {
            *keyOut = key;
            return true;
        }
    }

    return false;
}

bool TryCaptureHoldPositionKey(int* keyOut, wchar_t* nameOut, size_t nameOutCount) {
    const int preferred[] = {
        VK_SHIFT,
        VK_CONTROL,
        VK_MENU,
        VK_SPACE,
        VK_LEFT,
        VK_UP,
        VK_RIGHT,
        VK_DOWN,
        VK_F1,
        VK_F2,
        VK_F3,
        VK_F4,
        VK_F5,
        VK_F6,
        VK_F9,
        VK_F10,
        VK_F11,
        VK_F12};
    for (int i = 0; i < static_cast<int>(sizeof(preferred) / sizeof(preferred[0])); ++i) {
        const int key = preferred[i];
        if ((GetAsyncKeyState(key) & 0x8000) != 0 && KeyCodeToWideName(key, nameOut, nameOutCount)) {
            *keyOut = key;
            return true;
        }
    }

    for (int key = 'A'; key <= 'Z'; ++key) {
        if ((GetAsyncKeyState(key) & 0x8000) != 0 && KeyCodeToWideName(key, nameOut, nameOutCount)) {
            *keyOut = key;
            return true;
        }
    }

    return false;
}

void PersistCustomKeyboardConfig() {
    if (g_iniFile[0] == L'\0') {
        return;
    }

    WritePrivateProfileStringW(L"Keys", L"Preset", L"CUSTOM", g_iniFile);
    WritePrivateProfileStringW(L"Keys", L"Left", g_overlayCustomNames[0], g_iniFile);
    WritePrivateProfileStringW(L"Keys", L"Up", g_overlayCustomNames[1], g_iniFile);
    WritePrivateProfileStringW(L"Keys", L"Right", g_overlayCustomNames[2], g_iniFile);
    WritePrivateProfileStringW(L"Keys", L"Down", g_overlayCustomNames[3], g_iniFile);
    WritePrivateProfileStringW(L"Keys", L"HoldPosition", g_overlayCustomHoldName, g_iniFile);
    WritePrivateProfileStringW(L"Keys", L"Forward", nullptr, g_iniFile);
    WritePrivateProfileStringW(L"Keys", L"Back", nullptr, g_iniFile);
    WritePrivateProfileStringW(L"Keys", L"BindingMode", L"PHYSICAL", g_iniFile);
    WriteScanCodeSetting(L"ScanLeft", g_scanLeft);
    WriteScanCodeSetting(L"ScanUp", g_scanUp);
    WriteScanCodeSetting(L"ScanRight", g_scanRight);
    WriteScanCodeSetting(L"ScanDown", g_scanDown);
}

void ApplyCustomOverlayKeys(bool persist) {
    InterlockedExchange(&g_scanLeft, g_overlayCustomKeys[0]);
    InterlockedExchange(&g_scanUp, g_overlayCustomKeys[1]);
    InterlockedExchange(&g_scanRight, g_overlayCustomKeys[2]);
    InterlockedExchange(&g_scanDown, g_overlayCustomKeys[3]);
    InterlockedExchange(&g_holdPositionKey, g_overlayCustomHoldKey);
    CopyWideKeyName(g_holdPositionKeyName, sizeof(g_holdPositionKeyName), g_overlayCustomHoldName);
    InterlockedExchange(&g_usePhysicalKeyboardLayout, 1);
    g_currentPresetIndex = -1;
    if (!ResolvePhysicalKeyboardBindings(true)) {
        InterlockedExchange(&g_usePhysicalKeyboardLayout, 0);
        LogLine("Custom physical keyboard config rejected: Windows could not resolve a scan code.");
        return;
    }

    if (persist) {
        PersistCustomKeyboardConfig();
    }

    char line[256]{};
    wsprintfA(
        line,
        "Custom keyboard config applied: Left=%s Up=%s Right=%s Down=%s HoldPosition=%s.",
        g_keyLeftName,
        g_keyUpName,
        g_keyRightName,
        g_keyDownName,
        g_holdPositionKeyName);
    LogLine(line);
}

void CopyAnsiToWideKeyName(wchar_t* destination, size_t destinationSize, const char* source) {
    if (destination == nullptr || destinationSize == 0) {
        return;
    }

    MultiByteToWideChar(CP_ACP, 0, source, -1, destination, static_cast<int>(destinationSize));
    destination[destinationSize - 1] = L'\0';
}

bool IsUsableGameWindowCandidate(HWND window, bool requireVisible) {
    if (window == nullptr ||
        !IsWindow(window) ||
        window == g_overlayWindow ||
        window == g_trayConfigWindow) {
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != GetCurrentProcessId()) {
        return false;
    }
    if (requireVisible && !IsWindowVisible(window)) {
        return false;
    }
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    return (style & WS_CHILD) == 0;
}

BOOL CALLBACK EnumGameWindowProc(HWND window, LPARAM parameter) {
    if (!IsUsableGameWindowCandidate(window, true) ||
        GetWindow(window, GW_OWNER) != nullptr) {
        return TRUE;
    }

    *reinterpret_cast<HWND*>(parameter) = window;
    return FALSE;
}

HWND FindGameWindow() {
    if (IsUsableGameWindowCandidate(g_dispatchGameWindow, true)) {
        return g_dispatchGameWindow;
    }

    HWND foreground = GetForegroundWindow();
    if (IsUsableGameWindowCandidate(foreground, true)) {
        return foreground;
    }

    HWND found = nullptr;
    EnumWindows(EnumGameWindowProc, reinterpret_cast<LPARAM>(&found));
    return found;
}

bool IsGameWindowForegroundForInput() {
    HWND foreground = GetForegroundWindow();
    if (foreground == nullptr ||
        foreground == g_overlayWindow ||
        foreground == g_trayConfigWindow) {
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    return processId == GetCurrentProcessId();
}

LRESULT CALLBACK GameWindowDispatchProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == kGameThreadCaptureMessage) {
        InterlockedExchange(&g_captureMessagePosted, 0);
        CaptureEventDrivenContextOnGameThread();
        return 0;
    }
    if (message == kGameThreadMoveMessage) {
        InterlockedExchange(&g_moveMessagePosted, 0);
        ExecutePendingMovementOnGameWindowThread();
        return 0;
    }
    if (message == kGameThreadPostReleaseCameraMessage) {
        InterlockedExchange(&g_postReleaseCameraMessagePosted, 0);
        TryAdvancePostReleaseCameraVerificationOnGameThread();
        return 0;
    }
    if (message == kGameThreadWasdOwnershipRecoveryMessage) {
        InterlockedExchange(&g_wasdOwnershipRecoveryMessagePosted, 0);
        PollWasdOwnershipRecoveryOnGameThread();
        return 0;
    }

    WNDPROC original = g_originalGameWindowProc;
    return original != nullptr
        ? CallWindowProcW(original, window, message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}

bool InstallGameWindowDispatchHook() {
    if (InterlockedCompareExchange(&g_dispatchOnGameWindowThread, 0, 0) == 0 &&
        InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0) {
        return true;
    }

    AcquireSRWLockExclusive(&g_gameWindowHookLock);
    if (g_dispatchGameWindow != nullptr &&
        IsWindow(g_dispatchGameWindow) &&
        g_originalGameWindowProc != nullptr) {
        ReleaseSRWLockExclusive(&g_gameWindowHookLock);
        return true;
    }

    HWND window = FindGameWindow();
    if (window == nullptr) {
        ReleaseSRWLockExclusive(&g_gameWindowHookLock);
        return false;
    }

    SetLastError(0);
    LONG_PTR previous = SetWindowLongPtrW(
        window,
        GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(GameWindowDispatchProc));
    if (previous == 0 && GetLastError() != 0) {
        ReleaseSRWLockExclusive(&g_gameWindowHookLock);
        return false;
    }

    // A concurrent installer must never record our own procedure as the
    // original callback. The exclusive lock above makes this assignment atomic
    // with respect to both the worker and PlayerCam/render callbacks.
    if (previous == reinterpret_cast<LONG_PTR>(GameWindowDispatchProc)) {
        ReleaseSRWLockExclusive(&g_gameWindowHookLock);
        return g_originalGameWindowProc != nullptr;
    }

    g_dispatchGameWindow = window;
    g_originalGameWindowProc = reinterpret_cast<WNDPROC>(previous);
    g_gameWindowThreadId = GetWindowThreadProcessId(window, nullptr);

    char line[320]{};
    wsprintfA(
        line,
        "Game-window dispatch installed: hwnd=0x%08X windowThread=%lu workerThread=%lu.",
        reinterpret_cast<DWORD>(window),
        g_gameWindowThreadId,
        g_workerThreadId);
    LogLine(line);
    ReleaseSRWLockExclusive(&g_gameWindowHookLock);
    return true;
}

void RestoreGameWindowDispatchHook() {
    AcquireSRWLockExclusive(&g_gameWindowHookLock);
    HWND window = g_dispatchGameWindow;
    WNDPROC original = g_originalGameWindowProc;
    if (window != nullptr && IsWindow(window) && original != nullptr) {
        const LONG_PTR current = GetWindowLongPtrW(window, GWLP_WNDPROC);
        if (current == reinterpret_cast<LONG_PTR>(GameWindowDispatchProc)) {
            SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
        }
    }
    g_dispatchGameWindow = nullptr;
    g_originalGameWindowProc = nullptr;
    g_gameWindowThreadId = 0;
    ReleaseSRWLockExclusive(&g_gameWindowHookLock);
}

void PostCaptureTickToGameWindow() {
    if (InterlockedCompareExchange(&g_captureMessagePosted, 1, 0) != 0) {
        return;
    }
    if (!InstallGameWindowDispatchHook() ||
        !PostMessageW(g_dispatchGameWindow, kGameThreadCaptureMessage, 0, 0)) {
        InterlockedExchange(&g_captureMessagePosted, 0);
    }
}

void PostWasdOwnershipRecoveryTickToGameWindow() {
    if (InterlockedCompareExchange(&g_wasdOwnershipRecoveryMessagePosted, 1, 0) != 0) {
        return;
    }
    if (!InstallGameWindowDispatchHook() ||
        !PostMessageW(g_dispatchGameWindow, kGameThreadWasdOwnershipRecoveryMessage, 0, 0)) {
        InterlockedExchange(&g_wasdOwnershipRecoveryMessagePosted, 0);
    }
}

void ResetOverlayCustomBuffer() {
    if (InterlockedCompareExchange(&g_usePhysicalKeyboardLayout, 0, 0) != 0) {
        g_overlayCustomKeys[0] = static_cast<int>(g_scanLeft);
        g_overlayCustomKeys[1] = static_cast<int>(g_scanUp);
        g_overlayCustomKeys[2] = static_cast<int>(g_scanRight);
        g_overlayCustomKeys[3] = static_cast<int>(g_scanDown);
    } else {
        HKL layout = GetGameKeyboardLayout();
        g_overlayCustomKeys[0] = static_cast<int>(MapVirtualKeyExW(
            static_cast<UINT>(g_keyLeft), MAPVK_VK_TO_VSC_EX, layout));
        g_overlayCustomKeys[1] = static_cast<int>(MapVirtualKeyExW(
            static_cast<UINT>(g_keyUp), MAPVK_VK_TO_VSC_EX, layout));
        g_overlayCustomKeys[2] = static_cast<int>(MapVirtualKeyExW(
            static_cast<UINT>(g_keyRight), MAPVK_VK_TO_VSC_EX, layout));
        g_overlayCustomKeys[3] = static_cast<int>(MapVirtualKeyExW(
            static_cast<UINT>(g_keyDown), MAPVK_VK_TO_VSC_EX, layout));
    }
    CopyAnsiToWideKeyName(g_overlayCustomNames[0], 16, g_keyLeftName);
    CopyAnsiToWideKeyName(g_overlayCustomNames[1], 16, g_keyUpName);
    CopyAnsiToWideKeyName(g_overlayCustomNames[2], 16, g_keyRightName);
    CopyAnsiToWideKeyName(g_overlayCustomNames[3], 16, g_keyDownName);
    g_overlayCustomHoldKey = static_cast<int>(InterlockedCompareExchange(&g_holdPositionKey, 0, 0));
    CopyAnsiToWideKeyName(g_overlayCustomHoldName, 16, g_holdPositionKeyName);
}

bool GetWindowClientRectOnScreen(HWND window, RECT* rectOut) {
    if (window == nullptr || rectOut == nullptr || !IsWindow(window)) {
        return false;
    }

    RECT client{};
    if (!GetClientRect(window, &client)) {
        return false;
    }
    POINT topLeft{client.left, client.top};
    POINT bottomRight{client.right, client.bottom};
    if (!ClientToScreen(window, &topLeft) || !ClientToScreen(window, &bottomRight)) {
        return false;
    }
    if (bottomRight.x <= topLeft.x || bottomRight.y <= topLeft.y) {
        return false;
    }

    rectOut->left = topLeft.x;
    rectOut->top = topLeft.y;
    rectOut->right = bottomRight.x;
    rectOut->bottom = bottomRight.y;
    return true;
}

bool RectsNearlyEqual(const RECT& left, const RECT& right, int tolerance = 2) {
    return abs(left.left - right.left) <= tolerance &&
        abs(left.top - right.top) <= tolerance &&
        abs(left.right - right.right) <= tolerance &&
        abs(left.bottom - right.bottom) <= tolerance;
}

bool GetCurrentOverlayAnchorGeometry(
    HWND* anchorWindowOut,
    RECT* anchorRectOut,
    RECT* monitorRectOut,
    RECT* workRectOut) {
    HWND anchorWindow = FindGameWindow();
    if (anchorWindow != nullptr) {
        g_overlayGameWindow = anchorWindow;
    }

    RECT anchorRect{};
    bool hasAnchorRect =
        anchorWindow != nullptr &&
        !IsIconic(anchorWindow) &&
        IsWindowVisible(anchorWindow) &&
        GetWindowClientRectOnScreen(anchorWindow, &anchorRect);
    if (!hasAnchorRect && anchorWindow != nullptr && !IsIconic(anchorWindow)) {
        hasAnchorRect = GetWindowRect(anchorWindow, &anchorRect) != FALSE;
    }

    HMONITOR monitor = MonitorFromWindow(
        anchorWindow != nullptr ? anchorWindow : g_overlayWindow,
        MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    RECT monitorRect{};
    RECT workRect{};
    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo)) {
        monitorRect = monitorInfo.rcMonitor;
        workRect = monitorInfo.rcWork;
    } else {
        monitorRect.left = 0;
        monitorRect.top = 0;
        monitorRect.right = GetSystemMetrics(SM_CXSCREEN);
        monitorRect.bottom = GetSystemMetrics(SM_CYSCREEN);
        workRect = monitorRect;
    }

    if (!hasAnchorRect) {
        anchorRect = workRect;
    }
    if (anchorWindowOut != nullptr) *anchorWindowOut = anchorWindow;
    if (anchorRectOut != nullptr) *anchorRectOut = anchorRect;
    if (monitorRectOut != nullptr) *monitorRectOut = monitorRect;
    if (workRectOut != nullptr) *workRectOut = workRect;
    return hasAnchorRect;
}

void ResetOverlayPlacementCache() {
    g_lastOverlayAnchorWindow = nullptr;
    g_lastOverlayX = INT_MIN;
    g_lastOverlayY = INT_MIN;
    g_lastOverlayWidth = 0;
    g_lastOverlayHeight = 0;
    g_hasLastOverlayAnchorRect = false;
}

bool ApplyOverlayWindowPosition(bool showWindow, const char* reason) {
    if (g_overlayWindow == nullptr || !IsWindow(g_overlayWindow)) {
        return false;
    }

    HWND anchorWindow = nullptr;
    RECT anchorRect{};
    RECT monitorRect{};
    RECT workRect{};
    const bool hasAnchorRect = GetCurrentOverlayAnchorGeometry(
        &anchorWindow,
        &anchorRect,
        &monitorRect,
        &workRect);

    tl2_overlay_placement::Placement placement{};
    if (hasAnchorRect) {
        placement = tl2_overlay_placement::ComputeCoverPlacement(
            tl2_overlay_placement::Rect{
                anchorRect.left, anchorRect.top, anchorRect.right, anchorRect.bottom},
            tl2_overlay_placement::Rect{
                monitorRect.left, monitorRect.top, monitorRect.right, monitorRect.bottom});
    } else {
        placement = tl2_overlay_placement::ComputePlacement(
            tl2_overlay_placement::Rect{
                workRect.left, workRect.top, workRect.right, workRect.bottom},
            tl2_overlay_placement::Rect{
                workRect.left, workRect.top, workRect.right, workRect.bottom},
            kOverlayFallbackWidth,
            kOverlayFallbackHeight,
            kOverlayMonitorMargin);
    }

    const bool anchorChanged =
        !g_hasLastOverlayAnchorRect || !RectsNearlyEqual(anchorRect, g_lastOverlayAnchorRect, 0);
    const bool changed =
        anchorWindow != g_lastOverlayAnchorWindow ||
        anchorChanged ||
        placement.x != g_lastOverlayX ||
        placement.y != g_lastOverlayY ||
        placement.width != g_lastOverlayWidth ||
        placement.height != g_lastOverlayHeight;
    if (!changed && !showWindow) {
        return true;
    }

    if (anchorWindow != nullptr) {
        SetWindowLongPtrW(
            g_overlayWindow,
            GWLP_HWNDPARENT,
            reinterpret_cast<LONG_PTR>(anchorWindow));
    }

    const bool suppressWindowForD3D9 = showWindow && ShouldSuppressOverlayWindowForD3D9Mirror();
    UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER;
    if (showWindow && !suppressWindowForD3D9) {
        flags |= SWP_SHOWWINDOW;
    }
    if (!SetWindowPos(
            g_overlayWindow,
            HWND_TOPMOST,
            placement.x,
            placement.y,
            placement.width,
            placement.height,
            flags)) {
        return false;
    }

    g_lastOverlayAnchorWindow = anchorWindow;
    g_lastOverlayAnchorRect = anchorRect;
    g_hasLastOverlayAnchorRect = true;
    g_lastOverlayX = placement.x;
    g_lastOverlayY = placement.y;
    g_lastOverlayWidth = placement.width;
    g_lastOverlayHeight = placement.height;

    if (suppressWindowForD3D9) {
        ShowWindow(g_overlayWindow, SW_HIDE);
    }

    if (showWindow) {
        char line[640]{};
        wsprintfA(
            line,
            "Overlay presented after stable geometry: reason=%s anchor=0x%08X client=(%ld,%ld)-(%ld,%ld) overlay=(%d,%d,%d,%d) fullClientDim=%d d3d9Mirror=%d.",
            reason != nullptr ? reason : "show",
            reinterpret_cast<DWORD>(anchorWindow),
            anchorRect.left,
            anchorRect.top,
            anchorRect.right,
            anchorRect.bottom,
            placement.x,
            placement.y,
            placement.width,
            placement.height,
            hasAnchorRect ? 1 : 0,
            suppressWindowForD3D9 ? 1 : 0);
        LogLine(line);
    }
    return true;
}

void MarkOverlayGeometryDirty() {
    g_overlayGeometryDirtyTick = GetTickCount();
    g_overlayObservedAnchorRectValid = false;
    g_overlayObservedAnchorStableSinceTick = 0;
}

void UpdateOverlayWindowPositionStable(DWORD now) {
    if (InterlockedCompareExchange(&g_overlayVisible, 0, 0) == 0 ||
        g_overlayWindow == nullptr || !IsWindow(g_overlayWindow)) {
        return;
    }

    HWND anchorWindow = nullptr;
    RECT anchorRect{};
    RECT monitorRect{};
    RECT workRect{};
    if (!GetCurrentOverlayAnchorGeometry(
            &anchorWindow, &anchorRect, &monitorRect, &workRect)) {
        return;
    }

    if (!g_overlayObservedAnchorRectValid ||
        !RectsNearlyEqual(anchorRect, g_overlayObservedAnchorRect)) {
        g_overlayObservedAnchorRect = anchorRect;
        g_overlayObservedAnchorRectValid = true;
        g_overlayObservedAnchorStableSinceTick = now;
        return;
    }

    const DWORD debounceMs = static_cast<DWORD>(
        InterlockedCompareExchange(&g_overlayRepositionDebounceMs, 0, 0));
    if (g_overlayObservedAnchorStableSinceTick == 0 ||
        now - g_overlayObservedAnchorStableSinceTick < debounceMs) {
        return;
    }
    if (g_overlayGeometryDirtyTick != 0 && now - g_overlayGeometryDirtyTick < debounceMs) {
        return;
    }

    ApplyOverlayWindowPosition(false, "stable geometry update");
    g_overlayGeometryDirtyTick = 0;
}

void UpdateOverlayWindowPosition() {
    ApplyOverlayWindowPosition(false, "compatibility position update");
}

void ShowOverlayWindowAtUnifiedPosition(const char* reason) {
    ResetOverlayPlacementCache();
    if (!ApplyOverlayWindowPosition(true, reason)) {
        if (!ShouldSuppressOverlayWindowForD3D9Mirror()) {
            ShowWindow(g_overlayWindow, SW_SHOWNOACTIVATE);
        } else {
            ShowWindow(g_overlayWindow, SW_HIDE);
        }
        SetWindowPos(
            g_overlayWindow,
            HWND_TOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        LogLine("Overlay positioning failed; the existing window position was retained.");
    }
}

bool IsForegroundWithinWindowRoot(HWND anchorWindow) {
    if (anchorWindow == nullptr || !IsWindow(anchorWindow)) {
        return false;
    }
    HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) {
        return false;
    }
    return foreground == anchorWindow ||
           GetAncestor(foreground, GA_ROOT) == GetAncestor(anchorWindow, GA_ROOT);
}

void OpenStartupOverlayAtStableGeometry(HWND anchorWindow, bool allowFallbackWindow) {
    if (!EnsureOverlayWindow()) {
        return;
    }

    g_overlayGameWindow = anchorWindow != nullptr ? anchorWindow : FindGameWindow();
    g_overlaySelectedPresetIndex =
        (g_currentPresetIndex >= 0 && g_currentPresetIndex < kKeyboardPresetCount)
            ? g_currentPresetIndex
            : 0;
    g_overlayCustomStep = -1;
    g_overlayCustomWaitingForRelease = false;
    ResetOverlayCustomBuffer();
    InterlockedExchange(&g_overlayVisible, 1);
    UpdateD3D9OverlayRenderVisibleProp();
    g_overlayObservedAnchorRectValid = false;
    g_overlayObservedAnchorStableSinceTick = GetTickCount();

    if (allowFallbackWindow) {
        ShowOverlayWindowAtUnifiedPosition("startup automatic overlay open");
    } else {
        ApplyOverlayWindowPosition(false, "startup automatic overlay position without focus");
        ShowWindow(g_overlayWindow, SW_HIDE);
        InterlockedExchange(&g_overlayFallbackHiddenForFocusLoss, 1);
        LogLine("Overlay startup presentation completed without foreground focus; D3D9 in-game mirror is active and the fallback window stays hidden until game focus is restored.");
    }

    InvalidateRect(g_overlayWindow, nullptr, TRUE);
}

void UpdateOverlayFallbackWindowFocusVisibility(bool gameFocusedForInput) {
    if (!g_enableOverlay ||
        g_overlayWindow == nullptr ||
        !IsWindow(g_overlayWindow) ||
        InterlockedCompareExchange(&g_overlayVisible, 1, 1) == 0) {
        return;
    }

    if (!gameFocusedForInput) {
        if (IsWindowVisible(g_overlayWindow)) {
            ShowWindow(g_overlayWindow, SW_HIDE);
        }
        InterlockedExchange(&g_overlayFallbackHiddenForFocusLoss, 1);
        return;
    }

    if (InterlockedExchange(&g_overlayFallbackHiddenForFocusLoss, 0) != 0 &&
        !ShouldSuppressOverlayWindowForD3D9Mirror()) {
        ShowOverlayWindowAtUnifiedPosition("game focus restored");
    }
}

int ScaleOverlayMetric(int value, int scalePercent) {
    return MulDiv(value, scalePercent, 100);
}

const char* OverlayFontScaleModeLabel(LONG mode) {
    switch (mode) {
        case 1: return "Normal";
        case 2: return "Large";
        case 3: return "Extra large";
        default: return "Auto";
    }
}

int GetOverlayEffectiveScalePercent(const RECT& client) {
    const LONG mode = InterlockedCompareExchange(&g_overlayFontScaleMode, 0, 0);
    if (mode == 1) return 100;
    if (mode == 2) return 125;
    if (mode == 3) return 150;

    const int height = client.bottom > client.top ? client.bottom - client.top : 1080;
    return tl2_overlay_placement::ComputeAutoTextScalePercent(height);
}

HFONT CreateOverlayFont(
    int basePixelHeight,
    int scalePercent,
    int weight,
    const char* face) {
    int height = ScaleOverlayMetric(basePixelHeight, scalePercent);
    if (height < 12) height = 12;
    return CreateFontA(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face);
}

int MeasureWrappedText(HDC dc, const char* text, RECT bounds, UINT flags) {
    RECT measured = bounds;
    DrawTextA(dc, text, -1, &measured, flags | DT_CALCRECT);
    return measured.bottom - measured.top;
}

void FillOverlayBackground(HDC dc, RECT client) {
    if (g_skipOverlayBackgroundFill) {
        return;
    }
    HBRUSH background = CreateSolidBrush(RGB(34, 36, 40));
    FillRect(dc, &client, background);
    DeleteObject(background);
}

void DrawHoldPositionWarning(HDC dc, RECT client) {
    const int scale = GetOverlayEffectiveScalePercent(client);
    const int clientWidth = client.right - client.left;
    const int contentWidth = min(clientWidth - ScaleOverlayMetric(80, scale), ScaleOverlayMetric(1080, scale));
    const int left = client.left + (clientWidth - contentWidth) / 2;
    HFONT font = CreateOverlayFont(16, scale, FW_SEMIBOLD, "Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 199, 88));
    const char* text =
        "Hold Position compatibility: set this key to match Torchlight II Controls > Hold Position.\n"
        "While held, TL2TrueKeyboardMove pauses and leaves immobilization/facing to the vanilla game.";
    RECT rect{left, client.bottom - ScaleOverlayMetric(150, scale), left + contentWidth, client.bottom - ScaleOverlayMetric(80, scale)};
    DrawTextA(dc, text, -1, &rect, kOverlayWrappedDrawFlags);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void DrawOverlayCommandHelp(HDC dc, RECT client) {
    const int scale = GetOverlayEffectiveScalePercent(client);
    const int clientWidth = client.right - client.left;
    const int contentWidth = min(clientWidth - ScaleOverlayMetric(80, scale), ScaleOverlayMetric(1080, scale));
    const int left = client.left + (clientWidth - contentWidth) / 2;
    HFONT font = CreateOverlayFont(15, scale, FW_NORMAL, "Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(190, 196, 204));
    const char* commands =
        "Up/Down: preset   Enter: apply   Delete: custom   F5: text size   F6-F10: tools   End/Esc: close";
    RECT rect{left, client.bottom - ScaleOverlayMetric(68, scale), left + contentWidth, client.bottom - ScaleOverlayMetric(24, scale)};
    DrawTextA(dc, commands, -1, &rect, kOverlayWrappedDrawFlags);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

const char* CameraGameplayModeLabel() {
    const LONG mode = InterlockedCompareExchange(&g_cameraGameplayMode, 0, 0);
    if (mode == 2) return "special";
    if (mode == 1) return "isometric";
    return "fallback";
}

void DrawOverlayText(HDC dc, RECT client) {
    FillOverlayBackground(dc, client);
    const int scale = GetOverlayEffectiveScalePercent(client);
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    const int horizontalMargin = ScaleOverlayMetric(44, scale);
    int contentWidth = min(
        clientWidth - horizontalMargin * 2,
        ScaleOverlayMetric(1080, scale));
    if (contentWidth < 320) contentWidth = max(1, clientWidth - 24);
    const int left = client.left + (clientWidth - contentWidth) / 2;

    HFONT titleFont = CreateOverlayFont(30, scale, FW_SEMIBOLD, "Georgia");
    HFONT bodyFont = CreateOverlayFont(18, scale, FW_NORMAL, "Segoe UI");
    HFONT warningFont = CreateOverlayFont(16, scale, FW_SEMIBOLD, "Segoe UI");
    HFONT helpFont = CreateOverlayFont(15, scale, FW_NORMAL, "Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, bodyFont));
    SetBkMode(dc, TRANSPARENT);

    const bool customMode = g_overlayCustomStep >= 0;
    char text[2300]{};
    if (customMode) {
        const char* directionLabels[5] = {"Left", "Up", "Right", "Down", "Hold Position"};
        wsprintfA(
            text,
            "Author: %s\n"
            "WARNING: Unofficial DLL mod. It may freeze or crash the game.\n"
            "Use at your own risk.\n\n"
            "Custom keys: press a key for %s.\n\n"
            "Bindings use physical key positions and follow Windows layouts.\n\n"
            "Order: Left / Up / Right / Down / Hold Position\n"
            "Movement keys: A-Z and arrow keys\n"
            "Hold Position: SHIFT, CTRL, ALT, SPACE, A-Z, arrows, F1-F12\n\n"
            "Current: Left=%s  Up=%s  Right=%s  Down=%s  Hold=%s\n\n"
            "F5: text size    Esc: cancel    End: close",
            kModAuthor,
            directionLabels[g_overlayCustomStep],
            g_keyLeftName,
            g_keyUpName,
            g_keyRightName,
            g_keyDownName,
            g_holdPositionKeyName);
    } else {
        const KeyboardPreset& selected = kKeyboardPresets[g_overlaySelectedPresetIndex];
        const char* keyboardMousePatchToggle =
            InterlockedCompareExchange(&g_keyboardMousePatchEnabled, 0, 0) != 0
                ? "[X] Keyboard/mouse movement patch"
                : "[ ] Keyboard/mouse movement patch";
        const char* clickMovementToggle =
            InterlockedCompareExchange(&g_disableClickMovement, 0, 0) != 0
                ? "[X] Disable movement by left click"
                : "[ ] Disable movement by left click";
        const bool activeRangeConfigured =
            InterlockedCompareExchange(&g_activeRangePatchConfigured, 0, 0) != 0;
        const bool activeRangeRestartRequired =
            InterlockedCompareExchange(&g_activeRangePatchRestartRequired, 0, 0) != 0;
        char activeRangeToggle[192]{};
        if (activeRangeRestartRequired) {
            wsprintfA(
                activeRangeToggle,
                "%s Active range x1.5: RESTART GAME - will be %s next launch",
                activeRangeConfigured ? "[X]" : "[ ]",
                activeRangeConfigured ? "enabled" : "disabled");
        } else if (activeRangeConfigured) {
            lstrcpynA(activeRangeToggle, "[X] Active range x1.5: enabled this session", static_cast<int>(sizeof(activeRangeToggle)));
        } else {
            lstrcpynA(activeRangeToggle, "[ ] Active range x1.5: disabled", static_cast<int>(sizeof(activeRangeToggle)));
        }
        char selectedLeft[16]{};
        char selectedUp[16]{};
        char selectedRight[16]{};
        char selectedDown[16]{};
        if (selected.useWindowsLayout) {
            HKL layout = GetGameKeyboardLayout();
            SetVirtualKeyDisplayName(static_cast<int>(MapVirtualKeyExW(kStandardScanLeft, MAPVK_VSC_TO_VK_EX, layout)), selectedLeft, sizeof(selectedLeft));
            SetVirtualKeyDisplayName(static_cast<int>(MapVirtualKeyExW(kStandardScanUp, MAPVK_VSC_TO_VK_EX, layout)), selectedUp, sizeof(selectedUp));
            SetVirtualKeyDisplayName(static_cast<int>(MapVirtualKeyExW(kStandardScanRight, MAPVK_VSC_TO_VK_EX, layout)), selectedRight, sizeof(selectedRight));
            SetVirtualKeyDisplayName(static_cast<int>(MapVirtualKeyExW(kStandardScanDown, MAPVK_VSC_TO_VK_EX, layout)), selectedDown, sizeof(selectedDown));
        } else {
            CopyWideKeyName(selectedLeft, sizeof(selectedLeft), selected.leftName);
            CopyWideKeyName(selectedUp, sizeof(selectedUp), selected.upName);
            CopyWideKeyName(selectedRight, sizeof(selectedRight), selected.rightName);
            CopyWideKeyName(selectedDown, sizeof(selectedDown), selected.downName);
        }
        char selectedKeys[128]{};
        wsprintfA(selectedKeys, "Left=%s  Up=%s  Right=%s  Down=%s", selectedLeft, selectedUp, selectedRight, selectedDown);
        char cameraState[128]{};
        sprintf_s(
            cameraState,
            sizeof(cameraState),
            "Camera basis: %s  visual=%.2f  gameplay=%.2f",
            CameraGameplayModeLabel(),
            g_lastCameraVisualAngle,
            g_lastCameraGameplayAngle);
        const LONG scaleMode = InterlockedCompareExchange(&g_overlayFontScaleMode, 0, 0);
        char scaleState[128]{};
        wsprintfA(
            scaleState,
            "Overlay text size: %s (%d%% effective)",
            OverlayFontScaleModeLabel(scaleMode),
            scale);
        wsprintfA(
            text,
            "Author: %s\n"
            "WARNING: Unofficial DLL mod. It may freeze or crash the game. Use at your own risk.\n\n"
            "Current: %s  Left=%s  Up=%s  Right=%s  Down=%s\n"
            "Hold Position key: %s pauses mod input while held\n"
            "Windows layout: %s  |  Active style: %s\n\n"
            "Selected preset: %s\n"
            "%s\n"
            "%s\n"
            "%s\n"
            "%s\n"
            "%s\n"
            "%s",
            kModAuthor,
            CurrentPresetLabel(),
            g_keyLeftName,
            g_keyUpName,
            g_keyRightName,
            g_keyDownName,
            g_holdPositionKeyName,
            g_windowsKeyboardLayoutName,
            ResolvedMovementStyle(),
            selected.label,
            selectedKeys,
            keyboardMousePatchToggle,
            clickMovementToggle,
            activeRangeToggle,
            cameraState,
            scaleState);
    }

    const char* warningText =
        "Hold Position compatibility: set this key to match Torchlight II Controls > Hold Position.\n"
        "While held, TL2TrueKeyboardMove pauses and lets the vanilla game handle immobilization and facing.";
    const char* commands =
        "Up/Down: choose preset    Enter: apply    Delete: custom keys    F5: text size\n"
        "F6: keyboard/mouse patch    F7: left-click movement    F8: active range x1.5    F9: camera\n"
        "F10: emergency WASD recovery    End: close/reopen overlay    Esc: close";

    SelectObject(dc, titleFont);
    RECT titleMeasure{0, 0, contentWidth, ScaleOverlayMetric(90, scale)};
    char title[128]{};
    FormatModTitle(title, sizeof(title), false);
    const int titleHeight = MeasureWrappedText(dc, title, titleMeasure, kOverlaySingleLineMeasureFlags);

    SelectObject(dc, bodyFont);
    RECT bodyMeasure{0, 0, contentWidth, clientHeight};
    const int bodyHeight = MeasureWrappedText(dc, text, bodyMeasure, kOverlayWrappedMeasureFlags);

    int warningHeight = 0;
    int helpHeight = 0;
    if (!customMode) {
        SelectObject(dc, warningFont);
        RECT warningMeasure{0, 0, contentWidth, clientHeight};
        warningHeight = MeasureWrappedText(dc, warningText, warningMeasure, kOverlayWrappedMeasureFlags);
        SelectObject(dc, helpFont);
        RECT helpMeasure{0, 0, contentWidth, clientHeight};
        helpHeight = MeasureWrappedText(dc, commands, helpMeasure, kOverlayWrappedMeasureFlags);
    }

    const int gapTitle = ScaleOverlayMetric(24, scale);
    const int gapSection = ScaleOverlayMetric(20, scale);
    int totalHeight = titleHeight + gapTitle + bodyHeight;
    if (!customMode) totalHeight += gapSection + warningHeight + gapSection + helpHeight;
    int y = client.top + (clientHeight - totalHeight) / 2;
    const int minimumTop = client.top + ScaleOverlayMetric(24, scale);
    if (y < minimumTop) y = minimumTop;

    SelectObject(dc, titleFont);
    SetTextColor(dc, RGB(255, 205, 96));
    RECT titleRect{left, y, left + contentWidth, y + titleHeight};
    DrawTextA(dc, title, -1, &titleRect, kOverlaySingleLineDrawFlags);
    y += titleHeight + gapTitle;

    SelectObject(dc, bodyFont);
    SetTextColor(dc, RGB(238, 240, 242));
    RECT bodyRect{left, y, left + contentWidth, y + bodyHeight};
    DrawTextA(dc, text, -1, &bodyRect, kOverlayWrappedDrawFlags);
    y += bodyHeight;

    if (!customMode) {
        y += gapSection;
        SelectObject(dc, warningFont);
        SetTextColor(dc, RGB(255, 199, 88));
        RECT warningRect{left, y, left + contentWidth, y + warningHeight};
        DrawTextA(dc, warningText, -1, &warningRect, kOverlayWrappedDrawFlags);
        y += warningHeight + gapSection;

        SelectObject(dc, helpFont);
        SetTextColor(dc, RGB(190, 196, 204));
        RECT helpRect{left, y, left + contentWidth, y + helpHeight};
        DrawTextA(dc, commands, -1, &helpRect, kOverlayWrappedDrawFlags);
    }

    SelectObject(dc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(bodyFont);
    DeleteObject(warningFont);
    DeleteObject(helpFont);
}

void UpdateD3D9OverlayRenderVisibleProp() {
    if (g_overlayWindow == nullptr || !IsWindow(g_overlayWindow)) {
        return;
    }

    const bool renderVisible =
        InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0;
    SetPropA(
        g_overlayWindow,
        kD3D9OverlayRenderVisibleProp,
        reinterpret_cast<HANDLE>(renderVisible ? 1 : 0));
}

bool ShouldSuppressOverlayWindowForD3D9Mirror() {
    if (g_overlayWindow == nullptr || !IsWindow(g_overlayWindow)) {
        return false;
    }
    if (InterlockedCompareExchange(&g_hideWindowOverlayWhenD3D9Active, 0, 0) == 0) {
        return false;
    }
    return InterlockedCompareExchange(&g_integratedD3D9OverlayFrameDrawn, 1, 1) != 0 ||
           GetPropA(g_overlayWindow, kD3D9OverlayMirrorActiveProp) != nullptr;
}

LRESULT CALLBACK OverlayWndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PRINT:
        case WM_PRINTCLIENT: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            if (dc == nullptr) {
                return 0;
            }
            RECT client{};
            GetClientRect(window, &client);
            DrawOverlayText(dc, client);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            const int width = client.right - client.left;
            const int height = client.bottom - client.top;
            HDC memoryDc = CreateCompatibleDC(dc);
            HBITMAP bitmap = width > 0 && height > 0 ? CreateCompatibleBitmap(dc, width, height) : nullptr;
            if (memoryDc != nullptr && bitmap != nullptr) {
                HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
                DrawOverlayText(memoryDc, client);
                BitBlt(dc, 0, 0, width, height, memoryDc, 0, 0, SRCCOPY);
                SelectObject(memoryDc, oldBitmap);
            } else {
                DrawOverlayText(dc, client);
            }
            if (bitmap != nullptr) DeleteObject(bitmap);
            if (memoryDc != nullptr) DeleteDC(memoryDc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
        case WM_DPICHANGED:
            ResetOverlayPlacementCache();
            MarkOverlayGeometryDirty();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_CLOSE:
            ShowWindow(window, SW_HIDE);
            InterlockedExchange(&g_overlayVisible, 0);
            g_overlayCustomStep = -1;
            UpdateD3D9OverlayRenderVisibleProp();
            return 0;
        default:
            return DefWindowProcA(window, message, wparam, lparam);
    }
}

namespace integrated_d3d9_overlay {

constexpr UINT kCreateDeviceVtableIndex = 16;
constexpr UINT kResetVtableIndex = 16;
constexpr UINT kPresentVtableIndex = 17;
constexpr UINT kGetBackBufferVtableIndex = 18;
constexpr UINT kCreateTextureVtableIndex = 23;
constexpr UINT kUpdateSurfaceVtableIndex = 30;
constexpr UINT kStretchRectVtableIndex = 34;
constexpr UINT kCreateOffscreenPlainSurfaceVtableIndex = 36;
constexpr UINT kSetRenderTargetVtableIndex = 37;
constexpr UINT kGetRenderTargetVtableIndex = 38;
constexpr UINT kEndSceneVtableIndex = 42;
constexpr UINT kSetRenderStateVtableIndex = 57;
constexpr UINT kSetTextureVtableIndex = 65;
constexpr UINT kSetTextureStageStateVtableIndex = 67;
constexpr UINT kSetSamplerStateVtableIndex = 69;
constexpr UINT kDrawPrimitiveUPVtableIndex = 83;
constexpr UINT kSetFVFVtableIndex = 89;
constexpr UINT kCreateStateBlockVtableIndex = 59;
constexpr UINT kSurfaceReleaseVtableIndex = 2;
constexpr UINT kSurfaceGetDescVtableIndex = 12;
constexpr UINT kSurfaceGetDCVtableIndex = 15;
constexpr UINT kSurfaceReleaseDCVtableIndex = 16;
constexpr UINT kTextureReleaseVtableIndex = 2;
constexpr UINT kTextureLockRectVtableIndex = 19;
constexpr UINT kTextureUnlockRectVtableIndex = 20;
constexpr UINT kStateBlockReleaseVtableIndex = 2;
constexpr UINT kStateBlockApplyVtableIndex = 5;
constexpr UINT kD3DPoolDefault = 0;
constexpr UINT kD3DPoolManaged = 1;
constexpr UINT kD3DPoolSystemMem = 2;
constexpr UINT kD3DTexfNone = 0;
constexpr UINT kD3DTexfPoint = 1;
constexpr UINT kD3DBackBufferTypeMono = 0;
constexpr UINT kD3DFormatA8R8G8B8 = 21;
constexpr UINT kD3DUsageNone = 0;
constexpr UINT kD3DPTTriangleStrip = 5;
constexpr UINT kD3DFVFXyzRhw = 0x004;
constexpr UINT kD3DFVFDiffuse = 0x040;
constexpr UINT kD3DFVFTex1 = 0x100;
constexpr UINT kD3DFVFOverlayVertex = kD3DFVFXyzRhw | kD3DFVFDiffuse | kD3DFVFTex1;
constexpr UINT kD3DRS_ZEnable = 7;
constexpr UINT kD3DRS_ZWriteEnable = 14;
constexpr UINT kD3DRS_AlphaTestEnable = 15;
constexpr UINT kD3DRS_SrcBlend = 19;
constexpr UINT kD3DRS_DestBlend = 20;
constexpr UINT kD3DRS_FogEnable = 28;
constexpr UINT kD3DRS_AlphaBlendEnable = 27;
constexpr UINT kD3DRS_Lighting = 137;
constexpr UINT kD3DBlendSrcAlpha = 5;
constexpr UINT kD3DBlendInvSrcAlpha = 6;
constexpr UINT kD3DTSSColorOp = 1;
constexpr UINT kD3DTSSColorArg1 = 2;
constexpr UINT kD3DTSSAlphaOp = 4;
constexpr UINT kD3DTSSAlphaArg1 = 5;
constexpr UINT kD3DTA_Texture = 2;
constexpr UINT kD3DTopDisable = 1;
constexpr UINT kD3DTopSelectArg1 = 2;
constexpr UINT kD3DSampMagFilter = 5;
constexpr UINT kD3DSampMinFilter = 6;
constexpr UINT kD3DSampMipFilter = 7;
constexpr UINT kD3DSBTAll = 1;
constexpr UINT kMaxOverlaySurfaceDimension = 8192;
constexpr LONG kHookRetryFailed = -1;
constexpr HRESULT kD3DErrInvalidCall = static_cast<HRESULT>(0x8876086CL);

using D3DDEVTYPE = UINT;
using D3DFORMAT = UINT;
using D3DPOOL = UINT;
using D3DTEXTUREFILTERTYPE = UINT;
using IDirect3D9 = void;
using IDirect3D9Ex = void;
using IDirect3DDevice9 = void;
using IDirect3DSurface9 = void;
using IDirect3DTexture9 = void;
using IDirect3DStateBlock9 = void;

constexpr D3DFORMAT kD3DFormatUnknown = 0;

struct D3DPRESENT_PARAMETERS;
struct D3DLOCKED_RECT {
    INT Pitch;
    void* pBits;
};

struct D3DSURFACE_DESC {
    D3DFORMAT Format;
    UINT Type;
    DWORD Usage;
    D3DPOOL Pool;
    UINT MultiSampleType;
    DWORD MultiSampleQuality;
    UINT Width;
    UINT Height;
};

struct OverlayVertex {
    float x;
    float y;
    float z;
    float rhw;
    DWORD diffuse;
    float u;
    float v;
};

using Direct3DCreate9Fn = IDirect3D9*(WINAPI*)(UINT);
using Direct3DCreate9ExFn = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
using CreateDeviceFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using ResetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using EndSceneFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using OgreGetActiveD3D9DeviceFn = IDirect3DDevice9*(__cdecl*)();

constexpr const char* kOgreGetActiveD3D9DeviceExport =
    "?getActiveD3D9Device@D3D9RenderSystem@Ogre@@SAPAUIDirect3DDevice9@@XZ";

Direct3DCreate9Fn g_originalDirect3DCreate9 = nullptr;
Direct3DCreate9ExFn g_originalDirect3DCreate9Ex = nullptr;
CreateDeviceFn g_originalCreateDevice = nullptr;
ResetFn g_originalReset = nullptr;
PresentFn g_originalPresent = nullptr;
EndSceneFn g_originalEndScene = nullptr;

volatile LONG g_iatHookState = 0;
volatile LONG g_ogreActiveDeviceExceptionLogged = 0;
volatile LONG g_frameDrawSuccessLogged = 0;
volatile LONG g_directBackbufferGdiFailureLogged = 0;
volatile LONG g_alphaTextureFailureLogged = 0;
volatile LONG g_renderTargetRestoreFailureLogged = 0;
IDirect3DSurface9* g_overlaySystemSurface = nullptr;
IDirect3DSurface9* g_overlayDefaultSurface = nullptr;
IDirect3DTexture9* g_overlayTexture = nullptr;
D3DFORMAT g_overlayFormat = kD3DFormatUnknown;
UINT g_overlaySurfaceWidth = 0;
UINT g_overlaySurfaceHeight = 0;
UINT g_overlayTextureWidth = 0;
UINT g_overlayTextureHeight = 0;
DWORD g_overlayTextureLastPaintTick = 0;
bool g_overlayTextureHasPixels = false;
bool g_loggedSurfaceFailure = false;

HRESULT STDMETHODCALLTYPE HookCreateDevice(
    IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
HRESULT STDMETHODCALLTYPE HookReset(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
HRESULT STDMETHODCALLTYPE HookEndScene(IDirect3DDevice9*);

template <typename Fn>
Fn VtableFunction(void* object, UINT slot) {
    void** vtable = *reinterpret_cast<void***>(object);
    return reinterpret_cast<Fn>(vtable[slot]);
}

template <typename Fn>
bool PatchVtableSlot(void* object, UINT slot, Fn hook, Fn* original) {
    if (!object || !hook || !original) {
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(object);
    if (!vtable) {
        return false;
    }
    if (vtable[slot] == reinterpret_cast<void*>(hook)) {
        return true;
    }
    if (*original == nullptr) {
        *original = reinterpret_cast<Fn>(vtable[slot]);
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(&vtable[slot], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    vtable[slot] = reinterpret_cast<void*>(hook);
    DWORD ignored = 0;
    VirtualProtect(&vtable[slot], sizeof(void*), oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), &vtable[slot], sizeof(void*));
    return true;
}

ULONG SurfaceRelease(IDirect3DSurface9* surface) {
    using Fn = ULONG(STDMETHODCALLTYPE*)(IDirect3DSurface9*);
    return VtableFunction<Fn>(surface, kSurfaceReleaseVtableIndex)(surface);
}

ULONG TextureRelease(IDirect3DTexture9* texture) {
    using Fn = ULONG(STDMETHODCALLTYPE*)(IDirect3DTexture9*);
    return VtableFunction<Fn>(texture, kTextureReleaseVtableIndex)(texture);
}

ULONG StateBlockRelease(IDirect3DStateBlock9* stateBlock) {
    using Fn = ULONG(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
    return VtableFunction<Fn>(stateBlock, kStateBlockReleaseVtableIndex)(stateBlock);
}

HRESULT StateBlockApply(IDirect3DStateBlock9* stateBlock) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
    return VtableFunction<Fn>(stateBlock, kStateBlockApplyVtableIndex)(stateBlock);
}

HRESULT SurfaceGetDesc(IDirect3DSurface9* surface, D3DSURFACE_DESC* desc) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DSurface9*, D3DSURFACE_DESC*);
    return VtableFunction<Fn>(surface, kSurfaceGetDescVtableIndex)(surface, desc);
}

HRESULT SurfaceGetDC(IDirect3DSurface9* surface, HDC* dc) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DSurface9*, HDC*);
    return VtableFunction<Fn>(surface, kSurfaceGetDCVtableIndex)(surface, dc);
}

HRESULT SurfaceReleaseDC(IDirect3DSurface9* surface, HDC dc) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DSurface9*, HDC);
    return VtableFunction<Fn>(surface, kSurfaceReleaseDCVtableIndex)(surface, dc);
}

HRESULT TextureLockRect(IDirect3DTexture9* texture, UINT level, D3DLOCKED_RECT* locked, const RECT* rect, DWORD flags) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9*, UINT, D3DLOCKED_RECT*, const RECT*, DWORD);
    return VtableFunction<Fn>(texture, kTextureLockRectVtableIndex)(texture, level, locked, rect, flags);
}

HRESULT TextureUnlockRect(IDirect3DTexture9* texture, UINT level) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9*, UINT);
    return VtableFunction<Fn>(texture, kTextureUnlockRectVtableIndex)(texture, level);
}

HRESULT DeviceCreateTexture(IDirect3DDevice9* device,
                            UINT width,
                            UINT height,
                            UINT levels,
                            DWORD usage,
                            D3DFORMAT format,
                            D3DPOOL pool,
                            IDirect3DTexture9** texture) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
    return VtableFunction<Fn>(device, kCreateTextureVtableIndex)(
        device, width, height, levels, usage, format, pool, texture, nullptr);
}

HRESULT DeviceCreateOffscreenPlainSurface(IDirect3DDevice9* device,
                                          UINT width,
                                          UINT height,
                                          D3DFORMAT format,
                                          D3DPOOL pool,
                                          IDirect3DSurface9** surface) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DPOOL, IDirect3DSurface9**, HANDLE*);
    return VtableFunction<Fn>(device, kCreateOffscreenPlainSurfaceVtableIndex)(
        device, width, height, format, pool, surface, nullptr);
}

HRESULT DeviceGetBackBuffer(IDirect3DDevice9* device,
                            UINT swapChain,
                            UINT backBuffer,
                            UINT type,
                            IDirect3DSurface9** target) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, UINT, IDirect3DSurface9**);
    return VtableFunction<Fn>(device, kGetBackBufferVtableIndex)(device, swapChain, backBuffer, type, target);
}

HRESULT DeviceUpdateSurface(IDirect3DDevice9* device,
                            IDirect3DSurface9* source,
                            IDirect3DSurface9* destination) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const POINT*);
    return VtableFunction<Fn>(device, kUpdateSurfaceVtableIndex)(device, source, nullptr, destination, nullptr);
}

HRESULT DeviceStretchRect(IDirect3DDevice9* device,
                          IDirect3DSurface9* source,
                          IDirect3DSurface9* destination,
                          const RECT* destinationRect) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE);
    return VtableFunction<Fn>(device, kStretchRectVtableIndex)(
        device, source, nullptr, destination, destinationRect, kD3DTexfNone);
}

HRESULT DeviceSetRenderTarget(IDirect3DDevice9* device, DWORD renderTargetIndex, IDirect3DSurface9* renderTarget) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
    return VtableFunction<Fn>(device, kSetRenderTargetVtableIndex)(device, renderTargetIndex, renderTarget);
}

HRESULT DeviceGetRenderTarget(IDirect3DDevice9* device, DWORD renderTargetIndex, IDirect3DSurface9** renderTarget) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9**);
    return VtableFunction<Fn>(device, kGetRenderTargetVtableIndex)(device, renderTargetIndex, renderTarget);
}

HRESULT DeviceCreateStateBlock(IDirect3DDevice9* device, UINT type, IDirect3DStateBlock9** stateBlock) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, IDirect3DStateBlock9**);
    return VtableFunction<Fn>(device, kCreateStateBlockVtableIndex)(device, type, stateBlock);
}

HRESULT DeviceSetRenderState(IDirect3DDevice9* device, UINT state, DWORD value) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, DWORD);
    return VtableFunction<Fn>(device, kSetRenderStateVtableIndex)(device, state, value);
}

HRESULT DeviceSetTexture(IDirect3DDevice9* device, DWORD stage, IDirect3DTexture9* texture) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DTexture9*);
    return VtableFunction<Fn>(device, kSetTextureVtableIndex)(device, stage, texture);
}

HRESULT DeviceSetTextureStageState(IDirect3DDevice9* device, DWORD stage, UINT type, DWORD value) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, UINT, DWORD);
    return VtableFunction<Fn>(device, kSetTextureStageStateVtableIndex)(device, stage, type, value);
}

HRESULT DeviceSetSamplerState(IDirect3DDevice9* device, DWORD sampler, UINT type, DWORD value) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, UINT, DWORD);
    return VtableFunction<Fn>(device, kSetSamplerStateVtableIndex)(device, sampler, type, value);
}

HRESULT DeviceSetFVF(IDirect3DDevice9* device, DWORD fvf) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD);
    return VtableFunction<Fn>(device, kSetFVFVtableIndex)(device, fvf);
}

HRESULT DeviceDrawPrimitiveUP(IDirect3DDevice9* device,
                              UINT primitiveType,
                              UINT primitiveCount,
                              const void* vertexStreamZeroData,
                              UINT vertexStreamZeroStride) {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, const void*, UINT);
    return VtableFunction<Fn>(device, kDrawPrimitiveUPVtableIndex)(
        device, primitiveType, primitiveCount, vertexStreamZeroData, vertexStreamZeroStride);
}

void ReleaseOverlaySurfaces() {
    if (g_overlaySystemSurface) {
        SurfaceRelease(g_overlaySystemSurface);
        g_overlaySystemSurface = nullptr;
    }
    if (g_overlayDefaultSurface) {
        SurfaceRelease(g_overlayDefaultSurface);
        g_overlayDefaultSurface = nullptr;
    }
    if (g_overlayTexture) {
        TextureRelease(g_overlayTexture);
        g_overlayTexture = nullptr;
    }
    g_overlayFormat = kD3DFormatUnknown;
    g_overlaySurfaceWidth = 0;
    g_overlaySurfaceHeight = 0;
    g_overlayTextureWidth = 0;
    g_overlayTextureHeight = 0;
    g_overlayTextureLastPaintTick = 0;
    g_overlayTextureHasPixels = false;
}

bool EnsureOverlaySurfaces(IDirect3DDevice9* device, D3DFORMAT format, UINT width, UINT height) {
    if (g_overlaySystemSurface &&
        g_overlayDefaultSurface &&
        g_overlayFormat == format &&
        g_overlaySurfaceWidth == width &&
        g_overlaySurfaceHeight == height) {
        return true;
    }

    ReleaseOverlaySurfaces();
    g_overlayFormat = format;
    g_overlaySurfaceWidth = width;
    g_overlaySurfaceHeight = height;

    HRESULT hr = DeviceCreateOffscreenPlainSurface(
        device, width, height, format, kD3DPoolSystemMem, &g_overlaySystemSurface);
    if (FAILED(hr)) {
        ReleaseOverlaySurfaces();
        return false;
    }
    hr = DeviceCreateOffscreenPlainSurface(
        device, width, height, format, kD3DPoolDefault, &g_overlayDefaultSurface);
    if (FAILED(hr)) {
        ReleaseOverlaySurfaces();
        return false;
    }
    return true;
}

bool EnsureOverlayTexture(IDirect3DDevice9* device, UINT width, UINT height) {
    if (g_overlayTexture &&
        g_overlayTextureWidth == width &&
        g_overlayTextureHeight == height) {
        return true;
    }

    if (g_overlayTexture) {
        TextureRelease(g_overlayTexture);
        g_overlayTexture = nullptr;
    }
    g_overlayTextureWidth = 0;
    g_overlayTextureHeight = 0;
    g_overlayTextureLastPaintTick = 0;
    g_overlayTextureHasPixels = false;

    HRESULT hr = DeviceCreateTexture(
        device,
        width,
        height,
        1,
        kD3DUsageNone,
        kD3DFormatA8R8G8B8,
        kD3DPoolManaged,
        &g_overlayTexture);
    if (FAILED(hr) || !g_overlayTexture) {
        if (g_overlayTexture) {
            TextureRelease(g_overlayTexture);
            g_overlayTexture = nullptr;
        }
        return false;
    }

    g_overlayTextureWidth = width;
    g_overlayTextureHeight = height;
    return true;
}

void PromoteDibTextPixelsToAlpha(BYTE* pixels, UINT width, UINT height) {
    if (!pixels || width == 0 || height == 0) {
        return;
    }

    const UINT pixelCount = width * height;
    for (UINT i = 0; i < pixelCount; ++i) {
        BYTE* pixel = pixels + i * 4;
        const BYTE blue = pixel[0];
        const BYTE green = pixel[1];
        const BYTE red = pixel[2];
        const BYTE alpha = max(red, max(green, blue));
        if (alpha < 48) {
            pixel[0] = 0;
            pixel[1] = 0;
            pixel[2] = 0;
            pixel[3] = 0;
            continue;
        }
        pixel[3] = alpha;
    }
}

bool RenderOverlayToTexture(IDirect3DTexture9* texture, UINT width, UINT height) {
    if (!texture || width == 0 || height == 0) {
        return false;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = static_cast<LONG>(width);
    info.bmiHeader.biHeight = -static_cast<LONG>(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        return false;
    }
    HBITMAP bitmap = CreateDIBSection(screenDc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screenDc);
    if (!bitmap || !bits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        return false;
    }

    HDC memoryDc = CreateCompatibleDC(nullptr);
    if (!memoryDc) {
        DeleteObject(bitmap);
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
    memset(bits, 0, static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    RECT client{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    g_skipOverlayBackgroundFill = true;
    DrawOverlayText(memoryDc, client);
    g_skipOverlayBackgroundFill = false;
    GdiFlush();
    PromoteDibTextPixelsToAlpha(static_cast<BYTE*>(bits), width, height);

    D3DLOCKED_RECT locked{};
    HRESULT hr = TextureLockRect(texture, 0, &locked, nullptr, 0);
    if (SUCCEEDED(hr) && locked.pBits && locked.Pitch >= static_cast<INT>(width * 4)) {
        const BYTE* source = static_cast<const BYTE*>(bits);
        BYTE* destination = static_cast<BYTE*>(locked.pBits);
        const UINT sourcePitch = width * 4;
        for (UINT y = 0; y < height; ++y) {
            memcpy(destination + y * locked.Pitch, source + y * sourcePitch, sourcePitch);
        }
        TextureUnlockRect(texture, 0);
    } else {
        if (SUCCEEDED(hr)) {
            TextureUnlockRect(texture, 0);
        }
        hr = E_FAIL;
    }

    SelectObject(memoryDc, oldBitmap);
    DeleteDC(memoryDc);
    DeleteObject(bitmap);
    return SUCCEEDED(hr);
}

bool DrawAlphaTextureOverlay(IDirect3DDevice9* device, UINT width, UINT height) {
    if (!device || !EnsureOverlayTexture(device, width, height)) {
        return false;
    }

    const DWORD now = GetTickCount();
    if (!g_overlayTextureHasPixels || now - g_overlayTextureLastPaintTick >= 100) {
        if (!RenderOverlayToTexture(g_overlayTexture, width, height)) {
            return false;
        }
        g_overlayTextureHasPixels = true;
        g_overlayTextureLastPaintTick = now;
    }

    IDirect3DSurface9* originalRenderTarget = nullptr;
    DeviceGetRenderTarget(device, 0, &originalRenderTarget);
    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(DeviceGetBackBuffer(device, 0, 0, kD3DBackBufferTypeMono, &backBuffer)) || !backBuffer) {
        if (originalRenderTarget) {
            SurfaceRelease(originalRenderTarget);
        }
        return false;
    }

    if (FAILED(DeviceSetRenderTarget(device, 0, backBuffer))) {
        if (InterlockedExchange(&g_renderTargetRestoreFailureLogged, 1) == 0) {
            LogLine("Integrated D3D9 overlay could not switch to the backbuffer render target.");
        }
        SurfaceRelease(backBuffer);
        if (originalRenderTarget) {
            SurfaceRelease(originalRenderTarget);
        }
        return false;
    }

    IDirect3DStateBlock9* stateBlock = nullptr;
    DeviceCreateStateBlock(device, kD3DSBTAll, &stateBlock);
    DeviceSetRenderState(device, kD3DRS_ZEnable, FALSE);
    DeviceSetRenderState(device, kD3DRS_ZWriteEnable, FALSE);
    DeviceSetRenderState(device, kD3DRS_AlphaTestEnable, FALSE);
    DeviceSetRenderState(device, kD3DRS_FogEnable, FALSE);
    DeviceSetRenderState(device, kD3DRS_Lighting, FALSE);
    DeviceSetRenderState(device, kD3DRS_AlphaBlendEnable, TRUE);
    DeviceSetRenderState(device, kD3DRS_SrcBlend, kD3DBlendSrcAlpha);
    DeviceSetRenderState(device, kD3DRS_DestBlend, kD3DBlendInvSrcAlpha);
    DeviceSetTextureStageState(device, 0, kD3DTSSColorOp, kD3DTopSelectArg1);
    DeviceSetTextureStageState(device, 0, kD3DTSSColorArg1, kD3DTA_Texture);
    DeviceSetTextureStageState(device, 0, kD3DTSSAlphaOp, kD3DTopSelectArg1);
    DeviceSetTextureStageState(device, 0, kD3DTSSAlphaArg1, kD3DTA_Texture);
    DeviceSetTextureStageState(device, 1, kD3DTSSColorOp, kD3DTopDisable);
    DeviceSetTextureStageState(device, 1, kD3DTSSAlphaOp, kD3DTopDisable);
    DeviceSetSamplerState(device, 0, kD3DSampMinFilter, kD3DTexfPoint);
    DeviceSetSamplerState(device, 0, kD3DSampMagFilter, kD3DTexfPoint);
    DeviceSetSamplerState(device, 0, kD3DSampMipFilter, kD3DTexfNone);
    DeviceSetFVF(device, kD3DFVFOverlayVertex);
    DeviceSetTexture(device, 0, g_overlayTexture);

    OverlayVertex vertices[4] = {
        {-0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, 0.0f},
        {static_cast<float>(width) - 0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 1.0f, 0.0f},
        {-0.5f, static_cast<float>(height) - 0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, 1.0f},
        {static_cast<float>(width) - 0.5f, static_cast<float>(height) - 0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 1.0f, 1.0f}
    };
    const HRESULT hr = DeviceDrawPrimitiveUP(device, kD3DPTTriangleStrip, 2, vertices, sizeof(OverlayVertex));
    DeviceSetTexture(device, 0, nullptr);

    if (stateBlock) {
        StateBlockApply(stateBlock);
        StateBlockRelease(stateBlock);
    }

    HRESULT restoreHr = S_OK;
    if (originalRenderTarget) {
        restoreHr = DeviceSetRenderTarget(device, 0, originalRenderTarget);
        SurfaceRelease(originalRenderTarget);
    }
    SurfaceRelease(backBuffer);
    if (FAILED(restoreHr) && InterlockedExchange(&g_renderTargetRestoreFailureLogged, 1) == 0) {
        LogLine("Integrated D3D9 overlay render target restore failed.");
    }
    return SUCCEEDED(hr);
}

bool PaintOverlaySurface() {
    if (!g_overlaySystemSurface) {
        return false;
    }

    HDC dc = nullptr;
    if (FAILED(SurfaceGetDC(g_overlaySystemSurface, &dc)) || !dc) {
        return false;
    }

    RECT client{0, 0, static_cast<LONG>(g_overlaySurfaceWidth), static_cast<LONG>(g_overlaySurfaceHeight)};
    DrawOverlayText(dc, client);
    SurfaceReleaseDC(g_overlaySystemSurface, dc);
    return true;
}

bool ShouldDrawOverlay() {
    if (InterlockedCompareExchange(&g_enableD3D9FrameOverlay, 0, 0) == 0) {
        return false;
    }
    return InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0;
}

void MarkMirrorActive() {
    InterlockedExchange(&g_integratedD3D9OverlayDeviceHookInstalled, 1);
    InterlockedExchange(&g_integratedD3D9OverlayFrameDrawn, 1);
    if (g_overlayWindow != nullptr && IsWindow(g_overlayWindow)) {
        SetPropA(g_overlayWindow, kD3D9OverlayMirrorActiveProp, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1)));
        if (InterlockedCompareExchange(&g_hideWindowOverlayWhenD3D9Active, 0, 0) != 0) {
            ShowWindow(g_overlayWindow, SW_HIDE);
        }
    }
}

void DrawOverlayUnsafe(IDirect3DDevice9* device) {
    if (!device || !ShouldDrawOverlay()) {
        return;
    }

    IDirect3DSurface9* target = nullptr;
    if (FAILED(DeviceGetBackBuffer(device, 0, 0, kD3DBackBufferTypeMono, &target)) || !target) {
        return;
    }

    D3DSURFACE_DESC desc{};
    if (FAILED(SurfaceGetDesc(target, &desc)) ||
        desc.Width == 0 ||
        desc.Height == 0 ||
        desc.Width > kMaxOverlaySurfaceDimension ||
        desc.Height > kMaxOverlaySurfaceDimension) {
        SurfaceRelease(target);
        return;
    }

    if (DrawAlphaTextureOverlay(device, desc.Width, desc.Height)) {
        MarkMirrorActive();
        if (InterlockedExchange(&g_frameDrawSuccessLogged, 1) == 0) {
            LogLine("Integrated D3D9 overlay alpha frame drawn successfully.");
        }
    } else if (InterlockedExchange(&g_alphaTextureFailureLogged, 1) == 0) {
        LogLine("Integrated D3D9 overlay alpha texture draw failed.");
    }

    SurfaceRelease(target);
}

void DrawOverlay(IDirect3DDevice9* device) {
    __try {
        DrawOverlayUnsafe(device);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_skipOverlayBackgroundFill = false;
        LogLine("Exception while drawing integrated D3D9 overlay.");
    }
}

void InstallDeviceHooks(IDirect3DDevice9* device) {
    if (!device) {
        return;
    }

    const bool resetHooked = PatchVtableSlot(device, kResetVtableIndex, &HookReset, &g_originalReset);
    const bool presentHooked = PatchVtableSlot(device, kPresentVtableIndex, &HookPresent, &g_originalPresent);
    const bool endSceneHooked = PatchVtableSlot(device, kEndSceneVtableIndex, &HookEndScene, &g_originalEndScene);
    if (resetHooked && presentHooked && endSceneHooked) {
        InterlockedExchange(&g_integratedD3D9OverlayDeviceHookInstalled, 1);
        LogLine("Integrated D3D9 overlay device hooks installed.");
    }
}

bool TryInstallFromOgreActiveDevice() {
    if (InterlockedCompareExchange(&g_integratedD3D9OverlayDeviceHookInstalled, 1, 1) != 0) {
        return true;
    }

    HMODULE renderSystem = GetModuleHandleW(L"RenderSystem_Direct3D9.dll");
    if (!renderSystem) {
        return false;
    }

    auto getActiveDevice = reinterpret_cast<OgreGetActiveD3D9DeviceFn>(
        GetProcAddress(renderSystem, kOgreGetActiveD3D9DeviceExport));
    if (!getActiveDevice) {
        return false;
    }

    IDirect3DDevice9* device = nullptr;
    __try {
        device = getActiveDevice();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (InterlockedExchange(&g_ogreActiveDeviceExceptionLogged, 1) == 0) {
            LogLine("Integrated D3D9 overlay Ogre active-device probe failed with exception.");
        }
        return false;
    }

    if (!device) {
        return false;
    }

    InstallDeviceHooks(device);
    if (InterlockedCompareExchange(&g_integratedD3D9OverlayDeviceHookInstalled, 1, 1) != 0) {
        LogLine("Integrated D3D9 overlay active device hook installed via Ogre getActiveD3D9Device.");
        return true;
    }
    return false;
}

void InstallD3D9ObjectHook(IDirect3D9* d3d) {
    if (d3d &&
        PatchVtableSlot(d3d, kCreateDeviceVtableIndex, &HookCreateDevice, &g_originalCreateDevice)) {
        LogLine("Integrated D3D9 overlay IDirect3D9::CreateDevice hook installed.");
    }
}

IDirect3D9* WINAPI HookDirect3DCreate9(UINT sdkVersion) {
    if (!g_originalDirect3DCreate9) {
        return nullptr;
    }
    IDirect3D9* d3d = g_originalDirect3DCreate9(sdkVersion);
    InstallD3D9ObjectHook(d3d);
    return d3d;
}

HRESULT WINAPI HookDirect3DCreate9Ex(UINT sdkVersion, IDirect3D9Ex** outD3D) {
    if (!g_originalDirect3DCreate9Ex) {
        return kD3DErrInvalidCall;
    }
    const HRESULT hr = g_originalDirect3DCreate9Ex(sdkVersion, outD3D);
    if (SUCCEEDED(hr) && outD3D && *outD3D) {
        InstallD3D9ObjectHook(reinterpret_cast<IDirect3D9*>(*outD3D));
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE HookCreateDevice(IDirect3D9* self,
                                           UINT adapter,
                                           D3DDEVTYPE deviceType,
                                           HWND focusWindow,
                                           DWORD behaviorFlags,
                                           D3DPRESENT_PARAMETERS* presentationParameters,
                                           IDirect3DDevice9** returnedDevice) {
    if (!g_originalCreateDevice) {
        return kD3DErrInvalidCall;
    }
    const HRESULT hr = g_originalCreateDevice(
        self, adapter, deviceType, focusWindow, behaviorFlags, presentationParameters, returnedDevice);
    if (SUCCEEDED(hr) && returnedDevice && *returnedDevice) {
        InstallDeviceHooks(*returnedDevice);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE HookReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentationParameters) {
    ReleaseOverlaySurfaces();
    if (!g_originalReset) {
        return kD3DErrInvalidCall;
    }
    return g_originalReset(device, presentationParameters);
}

HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9* device,
                                      const RECT* sourceRect,
                                      const RECT* destinationRect,
                                      HWND destinationWindowOverride,
                                      const RGNDATA* dirtyRegion) {
    (void)device;
    if (!g_originalPresent) {
        return kD3DErrInvalidCall;
    }
    return g_originalPresent(device, sourceRect, destinationRect, destinationWindowOverride, dirtyRegion);
}

HRESULT STDMETHODCALLTYPE HookEndScene(IDirect3DDevice9* device) {
    DrawOverlay(device);
    if (!g_originalEndScene) {
        return kD3DErrInvalidCall;
    }
    return g_originalEndScene(device);
}

bool PatchImportByName(HMODULE module,
                       const char* importedModuleName,
                       const char* importName,
                       void* hook,
                       void** original) {
    if (!module || !importedModuleName || !importName || !hook || !original) {
        return false;
    }

    BYTE* base = reinterpret_cast<BYTE*>(module);
    IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const IMAGE_DATA_DIRECTORY& importDirectory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDirectory.VirtualAddress == 0) {
        return false;
    }

    IMAGE_IMPORT_DESCRIPTOR* descriptor =
        reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importDirectory.VirtualAddress);
    bool patched = false;
    for (; descriptor->Name != 0; ++descriptor) {
        const char* dllName = reinterpret_cast<const char*>(base + descriptor->Name);
        if (_stricmp(dllName, importedModuleName) != 0) {
            continue;
        }

        IMAGE_THUNK_DATA* originalThunk = descriptor->OriginalFirstThunk != 0
            ? reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk)
            : reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        IMAGE_THUNK_DATA* firstThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        for (; originalThunk->u1.AddressOfData != 0 && firstThunk->u1.Function != 0; ++originalThunk, ++firstThunk) {
            if (IMAGE_SNAP_BY_ORDINAL(originalThunk->u1.Ordinal)) {
                continue;
            }

            IMAGE_IMPORT_BY_NAME* importByName =
                reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + originalThunk->u1.AddressOfData);
            if (strcmp(reinterpret_cast<const char*>(importByName->Name), importName) != 0) {
                continue;
            }

            void** slot = reinterpret_cast<void**>(&firstThunk->u1.Function);
            if (*slot == hook) {
                patched = true;
                continue;
            }
            if (*original == nullptr) {
                *original = *slot;
            }

            DWORD oldProtect = 0;
            if (VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *slot = hook;
                DWORD ignored = 0;
                VirtualProtect(slot, sizeof(void*), oldProtect, &ignored);
                FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
                patched = true;
            }
        }
    }
    return patched;
}

bool PatchKnownD3D9Imports() {
    bool patched = false;
    HMODULE modules[] = {
        GetModuleHandleW(nullptr),
        GetModuleHandleW(L"RenderSystem_Direct3D9.dll"),
        GetModuleHandleW(L"OgreMain.dll")
    };

    for (HMODULE module : modules) {
        if (!module) {
            continue;
        }
        patched |= PatchImportByName(
            module,
            "d3d9.dll",
            "Direct3DCreate9",
            reinterpret_cast<void*>(&HookDirect3DCreate9),
            reinterpret_cast<void**>(&g_originalDirect3DCreate9));
        patched |= PatchImportByName(
            module,
            "d3d9.dll",
            "Direct3DCreate9Ex",
            reinterpret_cast<void*>(&HookDirect3DCreate9Ex),
            reinterpret_cast<void**>(&g_originalDirect3DCreate9Ex));
    }

    return patched;
}

}  // namespace integrated_d3d9_overlay

void InstallIntegratedD3D9OverlayHook() {
    if (InterlockedCompareExchange(&g_enableD3D9FrameOverlay, 0, 0) == 0) {
        return;
    }

    integrated_d3d9_overlay::TryInstallFromOgreActiveDevice();

    if (InterlockedCompareExchange(&integrated_d3d9_overlay::g_iatHookState, 1, 1) == 0 &&
        integrated_d3d9_overlay::PatchKnownD3D9Imports()) {
        InterlockedExchange(&integrated_d3d9_overlay::g_iatHookState, 1);
        LogLine("Integrated D3D9 overlay import hooks installed.");
    }

    integrated_d3d9_overlay::TryInstallFromOgreActiveDevice();
}

bool EnsureOverlayWindow() {
    if (g_overlayWindow != nullptr && IsWindow(g_overlayWindow)) {
        return true;
    }

    WNDCLASSEXA windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = OverlayWndProc;
    windowClass.hInstance = g_module;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = "TL2TrueKeyboardMoveOverlay";
    RegisterClassExA(&windowClass);

    char overlayTitle[128]{};
    FormatModTitle(overlayTitle, sizeof(overlayTitle), false);
    g_overlayWindow = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        "TL2TrueKeyboardMoveOverlay",
        overlayTitle,
        WS_POPUP,
        0,
        0,
        kOverlayFallbackWidth,
        kOverlayFallbackHeight,
        nullptr,
        nullptr,
        g_module,
        nullptr);

    if (g_overlayWindow == nullptr) {
        LogLine("Overlay creation failed.");
        return false;
    }

    SetLayeredWindowAttributes(g_overlayWindow, 0, static_cast<BYTE>(InterlockedCompareExchange(&g_overlayOpacity, 0, 0)), LWA_ALPHA);
    UpdateD3D9OverlayRenderVisibleProp();
    return true;
}

void ArmDeferredStartupOverlay(DWORD now) {
    if (!g_enableOverlay ||
        InterlockedCompareExchange(&g_overlayShowAtStartup, 0, 0) == 0) {
        return;
    }
    if (!EnsureOverlayWindow()) {
        return;
    }
    ShowWindow(g_overlayWindow, SW_HIDE);
    InterlockedExchange(&g_overlayVisible, 0);
    InterlockedExchange(&g_overlayStartupPending, 1);
    g_overlayStartupNotBeforeTick = now + static_cast<DWORD>(
        InterlockedCompareExchange(&g_overlayStartupDelayMs, 0, 0));
    g_overlayObservedAnchorRectValid = false;
    g_overlayObservedAnchorStableSinceTick = 0;
    char line[192]{};
    wsprintfA(
        line,
        "Overlay startup presentation armed: delayMs=%lu geometryStableMs=%ld.",
        static_cast<DWORD>(InterlockedCompareExchange(&g_overlayStartupDelayMs, 0, 0)),
        InterlockedCompareExchange(&g_overlayGeometryStableMs, 0, 0));
    LogLine(line);
}

void PumpDeferredStartupOverlay(DWORD now) {
    if (InterlockedCompareExchange(&g_overlayStartupPending, 0, 0) == 0) {
        return;
    }

    HWND anchorWindow = nullptr;
    RECT anchorRect{};
    RECT monitorRect{};
    RECT workRect{};
    if (!GetCurrentOverlayAnchorGeometry(
            &anchorWindow, &anchorRect, &monitorRect, &workRect) ||
        anchorWindow == nullptr || IsIconic(anchorWindow)) {
        g_overlayObservedAnchorRectValid = false;
        g_overlayObservedAnchorStableSinceTick = 0;
        return;
    }

    const bool gameForeground = IsForegroundWithinWindowRoot(anchorWindow);

    if (!g_overlayObservedAnchorRectValid ||
        !RectsNearlyEqual(anchorRect, g_overlayObservedAnchorRect)) {
        g_overlayObservedAnchorRect = anchorRect;
        g_overlayObservedAnchorRectValid = true;
        g_overlayObservedAnchorStableSinceTick = now;
        return;
    }

    const DWORD stableMs = static_cast<DWORD>(
        InterlockedCompareExchange(&g_overlayGeometryStableMs, 0, 0));
    if (now < g_overlayStartupNotBeforeTick ||
        g_overlayObservedAnchorStableSinceTick == 0 ||
        now - g_overlayObservedAnchorStableSinceTick < stableMs) {
        return;
    }

    InterlockedExchange(&g_overlayStartupPending, 0);
    OpenStartupOverlayAtStableGeometry(anchorWindow, gameForeground);
    LogLine("Overlay startup presentation completed once after stable fullscreen/client geometry; no startup show/hide loop remains active.");
}

void SetOverlayVisible(bool visible) {
    if (!EnsureOverlayWindow()) {
        return;
    }

    InterlockedExchange(&g_overlayStartupPending, 0);
    if (visible) {
        g_overlayGameWindow = FindGameWindow();
        g_overlaySelectedPresetIndex = (g_currentPresetIndex >= 0 && g_currentPresetIndex < kKeyboardPresetCount) ? g_currentPresetIndex : 0;
        g_overlayCustomStep = -1;
        g_overlayCustomWaitingForRelease = false;
        ResetOverlayCustomBuffer();
        InterlockedExchange(&g_overlayVisible, 1);
        UpdateD3D9OverlayRenderVisibleProp();
        g_overlayObservedAnchorRectValid = false;
        g_overlayObservedAnchorStableSinceTick = GetTickCount();
        ShowOverlayWindowAtUnifiedPosition("configuration overlay open");
        InvalidateRect(g_overlayWindow, nullptr, TRUE);
        LogLine("Keyboard configuration overlay opened with unified client-area positioning.");
    } else {
        ShowWindow(g_overlayWindow, SW_HIDE);
        InterlockedExchange(&g_overlayVisible, 0);
        g_overlayCustomStep = -1;
        g_overlayCustomWaitingForRelease = false;
        UpdateD3D9OverlayRenderVisibleProp();
        LogLine("Keyboard configuration overlay closed.");
    }
}

void PumpOverlayMessages() {
    MSG message{};
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

void StartOverlayCustomBinding() {
    ResetOverlayCustomBuffer();
    g_overlayCustomStep = 0;
    g_overlayCustomWaitingForRelease = false;
    InvalidateRect(g_overlayWindow, nullptr, TRUE);
    LogLine("Keyboard overlay custom binding started.");
}

void HandleOverlayCustomCapture() {
    if (g_overlayCustomStep < 0 || g_overlayCustomStep >= 5) {
        return;
    }

    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
        g_overlayCustomStep = -1;
        g_overlayCustomWaitingForRelease = true;
        InvalidateRect(g_overlayWindow, nullptr, TRUE);
        LogLine("Keyboard overlay custom binding cancelled.");
        return;
    }

    if (g_overlayCustomWaitingForRelease) {
        const bool anyCandidateDown =
            g_overlayCustomStep >= 4
                ? IsAnyHoldPositionCandidateDown()
                : IsAnyCustomCandidateDown();
        if (!anyCandidateDown) {
            g_overlayCustomWaitingForRelease = false;
        }
        return;
    }

    int key = 0;
    wchar_t keyName[16]{};
    if (g_overlayCustomStep >= 4) {
        if (!TryCaptureHoldPositionKey(&key, keyName, 16)) {
            return;
        }
        g_overlayCustomHoldKey = key;
        lstrcpynW(g_overlayCustomHoldName, keyName, 16);
        ++g_overlayCustomStep;
        g_overlayCustomWaitingForRelease = true;
    } else if (!TryCaptureCustomKey(&key, keyName, 16)) {
        return;
    } else {
        HKL layout = GetGameKeyboardLayout();
        const UINT scanCode = MapVirtualKeyExW(
            static_cast<UINT>(key),
            MAPVK_VK_TO_VSC_EX,
            layout);
        if (scanCode == 0) {
            return;
        }

        g_overlayCustomKeys[g_overlayCustomStep] = static_cast<int>(scanCode);
        lstrcpynW(g_overlayCustomNames[g_overlayCustomStep], keyName, 16);
        ++g_overlayCustomStep;
        g_overlayCustomWaitingForRelease = true;
    }

    if (g_overlayCustomStep >= 5) {
        ApplyCustomOverlayKeys(true);
        RefreshIniWriteTime();
        SetOverlayVisible(false);
        return;
    }

    InvalidateRect(g_overlayWindow, nullptr, TRUE);
}

void LogProcessModuleInfo() {
    wchar_t widePath[MAX_PATH]{};
    char path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, widePath, MAX_PATH) == 0) {
        LogLine("Process module: unable to resolve executable path.");
        return;
    }

    WideCharToMultiByte(CP_ACP, 0, widePath, -1, path, MAX_PATH, nullptr, nullptr);

    char line[512]{};
    wsprintfA(
        line,
        "Process module: base=0x%08X path=%s.",
        reinterpret_cast<DWORD>(GetModuleHandleW(nullptr)),
        path);
    LogLine(line);
}

void LogBytesAt(const char* label, const BYTE* bytes, DWORD length) {
    char line[512]{};
    char* out = line;
    out += wsprintfA(out, "%s", label);
    for (DWORD i = 0; i < length; ++i) {
        out += wsprintfA(out, " %02X", bytes[i]);
    }
    LogLine(line);
}

bool LooksLikeUsableStoredPlayerSelf(void* self);
bool LooksLikeReadableLocalMoveArg1(DWORD arg1);
void ClearPlayerLocalMoveContext(const char* reason);
bool IsCurrentMapRenderReady();
bool IsCurrentMapContextLockedFor(void* self, DWORD arg1);

bool IsTransitionLocalMoveBlocked(DWORD now) {
    return g_transitionLocalMoveBlockUntilTick != 0 &&
        static_cast<LONG>(now - g_transitionLocalMoveBlockUntilTick) < 0;
}

bool IsStabilityFallbackActive(DWORD now) {
    return g_stabilityFallbackUntilTick != 0 &&
        static_cast<LONG>(now - g_stabilityFallbackUntilTick) < 0;
}

bool IsPassiveContextSuspendActive(DWORD now) {
    return g_passiveContextSuspendUntilTick != 0 &&
        static_cast<LONG>(now - g_passiveContextSuspendUntilTick) < 0;
}

bool IsLocalMoveExceptionQuarantineActive(DWORD now) {
    return g_localMoveExceptionQuarantineUntilTick != 0 &&
        static_cast<LONG>(now - g_localMoveExceptionQuarantineUntilTick) < 0;
}

bool IsTransientContextHoldActive(DWORD now) {
    return g_transientContextHoldUntilTick != 0 &&
        static_cast<LONG>(now - g_transientContextHoldUntilTick) < 0;
}

void EnterTransientContextHold(const char* reason, DWORD now) {
    const DWORD until = now + kTransientContextHoldMs;
    if (g_transientContextHoldUntilTick == 0 ||
        static_cast<LONG>(until - g_transientContextHoldUntilTick) > 0) {
        g_transientContextHoldUntilTick = until;
    }
    InterlockedIncrement(&g_transientContextHoldEntries);

    if (now - g_lastTransientContextHoldLogTick >= 500) {
        g_lastTransientContextHoldLogTick = now;
        char line[384]{};
        wsprintfA(
            line,
            "Transient context hold for %lu ms: %s. Stable LocalMove context preserved; keyboard target carry continues.",
            kTransientContextHoldMs,
            reason != nullptr ? reason : "short direct context ambiguity");
        LogLine(line);
    }
}

bool IsLocalMoveExceptionRecoveryBlocked(DWORD now) {
    if (IsLocalMoveExceptionQuarantineActive(now)) {
        return true;
    }

    if (g_localMoveExceptionQuarantineUntilTick != 0 ||
        g_localMoveExceptionBlockUntilTick != 0 ||
        InterlockedCompareExchange(&g_requiresVanillaMoveAfterLocalMoveException, 0, 0) != 0) {
        g_localMoveExceptionQuarantineUntilTick = 0;
        g_localMoveExceptionBlockUntilTick = 0;
        InterlockedExchange(&g_requiresVanillaMoveAfterLocalMoveException, 0);
        InterlockedExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 0);
        InterlockedExchange(&g_keyboardMoveMissingContextLogged, 0);
        if (now - g_lastLocalMoveExceptionQuarantineLogTick >= 500) {
            g_lastLocalMoveExceptionQuarantineLogTick = now;
            LogLine("LocalMove exception quarantine expired; keyboard injection re-enabled and direct reacquisition allowed.");
        }
    }

    return false;
}

void EnterLocalMoveExceptionQuarantine(DWORD now) {
    InterlockedIncrement(&g_localMoveExceptionQuarantineEntries);
    g_localMoveExceptionQuarantineUntilTick = now + kLocalMoveExceptionQuarantineMs;
    g_localMoveExceptionBlockUntilTick = g_localMoveExceptionQuarantineUntilTick;
    InterlockedExchange(&g_requiresVanillaMoveAfterLocalMoveException, 1);
    if (g_cameraAcquireActive) {
        g_cameraAcquireActive = false;
        g_cameraAcquirePreserveBasisOnFailure = false;
        g_cameraAcquireBlocksMovement = false;
        InterlockedExchange(
            &g_cameraAcquireOrigin,
            static_cast<LONG>(CameraAcquisitionOrigin::None));
    }

    if (now - g_lastLocalMoveExceptionQuarantineLogTick >= 500) {
        g_lastLocalMoveExceptionQuarantineLogTick = now;
        char line[320]{};
        wsprintfA(
            line,
            "LocalMove exception quarantine active for %lu ms; keyboard injection waits for a vanilla player move source.",
            kLocalMoveExceptionQuarantineMs);
        LogLine(line);
    }
}

void ReleaseLocalMoveExceptionQuarantineFromVanillaMove(DWORD now) {
    const bool activeRecovery =
        IsLocalMoveExceptionQuarantineActive(now) ||
        g_localMoveExceptionQuarantineUntilTick != 0 ||
        g_localMoveExceptionBlockUntilTick != 0 ||
        InterlockedExchange(&g_requiresVanillaMoveAfterLocalMoveException, 0) != 0;
    if (!activeRecovery) {
        return;
    }
    g_localMoveExceptionQuarantineUntilTick = 0;
    g_localMoveExceptionBlockUntilTick = 0;
    InterlockedExchange(&g_requiresVanillaMoveAfterLocalMoveException, 0);
    InterlockedExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 0);
    InterlockedExchange(&g_keyboardMoveMissingContextLogged, 0);
    char line[256]{};
    wsprintfA(
        line,
        "LocalMove exception quarantine released by vanilla player move source at t=%lu; keyboard injection re-enabled.",
        now);
    LogLine(line);
}

typedef BOOL(WINAPI* GetProcessMemoryInfoPtr)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);

GetProcessMemoryInfoPtr ResolveGetProcessMemoryInfo() {
    static GetProcessMemoryInfoPtr function = nullptr;
    static bool resolved = false;
    if (resolved) {
        return function;
    }

    resolved = true;
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32 != nullptr) {
        function = reinterpret_cast<GetProcessMemoryInfoPtr>(
            GetProcAddress(kernel32, "K32GetProcessMemoryInfo"));
        if (function != nullptr) {
            return function;
        }
    }

    HMODULE psapi = LoadLibraryW(L"psapi.dll");
    if (psapi != nullptr) {
        function = reinterpret_cast<GetProcessMemoryInfoPtr>(
            GetProcAddress(psapi, "GetProcessMemoryInfo"));
    }
    return function;
}

bool ReadProcessMemoryCounters(PROCESS_MEMORY_COUNTERS_EX* counters) {
    if (counters == nullptr) {
        return false;
    }

    ZeroMemory(counters, sizeof(*counters));
    counters->cb = sizeof(*counters);
    GetProcessMemoryInfoPtr function = ResolveGetProcessMemoryInfo();
    if (function == nullptr) {
        return false;
    }

    return function(
        GetCurrentProcess(),
        reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(counters),
        sizeof(*counters)) != FALSE;
}

ULONGLONG GetCurrentLogFileSizeBytes() {
    if (g_logFile[0] == L'\0') {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(g_logFile, GetFileExInfoStandard, &data)) {
        return 0;
    }

    ULARGE_INTEGER size{};
    size.LowPart = data.nFileSizeLow;
    size.HighPart = data.nFileSizeHigh;
    return size.QuadPart;
}

DWORD BytesToMegabytes(SIZE_T bytes) {
    return static_cast<DWORD>(bytes / (1024u * 1024u));
}

void LogMemoryWatchdogSnapshot(DWORD now) {
    if (InterlockedCompareExchange(&g_enableMemoryWatchdog, 0, 0) == 0) {
        return;
    }

    if (g_lastMemoryWatchdogLogTick != 0 &&
        now - g_lastMemoryWatchdogLogTick < kMemoryWatchdogLogIntervalMs) {
        return;
    }
    g_lastMemoryWatchdogLogTick = now;

    PROCESS_MEMORY_COUNTERS_EX counters{};
    const bool hasMemoryCounters = ReadProcessMemoryCounters(&counters);
    DWORD handleCount = 0;
    const bool hasHandleCount = GetProcessHandleCount(GetCurrentProcess(), &handleCount) != FALSE;
    const ULONGLONG logBytes = GetCurrentLogFileSizeBytes();

    void* self = nullptr;
    DWORD arg1 = 0;
    void* controller = nullptr;
    AcquireSRWLockShared(&g_localMoveCaptureLock);
    self = g_playerLocalMoveSelf;
    arg1 = g_playerLocalMoveArg1;
    controller = g_playerMoveController;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);

    char line[1024]{};
    wsprintfA(
        line,
        "Memory watchdog t=%lu privateMB=%lu workingSetMB=%lu peakWorkingSetMB=%lu pagefileMB=%lu handles=%lu handlesValid=%d logKB=%lu hasMove=%ld bootstrap=%ld passive=%d transition=%d stability=%d quarantine=%d transientHold=%d mapGen=%lu mapRender=%d mapLock=%d ogreLoadSignals=%ld cameraAcquire=%d foreground=%ld injectionDisabled=%ld self=0x%08X arg1=0x%08X controller=0x%08X localMoveExceptions=%ld quarantines=%ld transientHolds=%ld.",
        now,
        hasMemoryCounters ? BytesToMegabytes(counters.PrivateUsage) : 0,
        hasMemoryCounters ? BytesToMegabytes(counters.WorkingSetSize) : 0,
        hasMemoryCounters ? BytesToMegabytes(counters.PeakWorkingSetSize) : 0,
        hasMemoryCounters ? BytesToMegabytes(counters.PagefileUsage) : 0,
        hasHandleCount ? handleCount : 0,
        hasHandleCount ? 1 : 0,
        static_cast<DWORD>(logBytes / 1024u),
        InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1),
        InterlockedCompareExchange(&g_levelBootstrapActive, 1, 1),
        IsPassiveContextSuspendActive(now) ? 1 : 0,
        IsTransitionLocalMoveBlocked(now) ? 1 : 0,
        IsStabilityFallbackActive(now) ? 1 : 0,
        IsLocalMoveExceptionQuarantineActive(now) ? 1 : 0,
        IsTransientContextHoldActive(now) ? 1 : 0,
        g_mapGeneration,
        IsCurrentMapRenderReady() ? 1 : 0,
        IsCurrentMapContextLockedFor(self, arg1) ? 1 : 0,
        InterlockedCompareExchange(&g_ogreLogLoadSignals, 0, 0),
        g_cameraAcquireActive ? 1 : 0,
        InterlockedCompareExchange(&g_gameInputFocused, 1, 1),
        InterlockedCompareExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 1, 1),
        reinterpret_cast<DWORD>(self),
        arg1,
        reinterpret_cast<DWORD>(controller),
        InterlockedCompareExchange(&g_localMoveExceptionCount, 0, 0),
        InterlockedCompareExchange(&g_localMoveExceptionQuarantineEntries, 0, 0),
        InterlockedCompareExchange(&g_transientContextHoldEntries, 0, 0));
    LogLine(line);
}

void SoftSuspendPlayerLocalMoveContext(const char* reason, DWORD now) {
    const bool preserveEventLock =
        InterlockedCompareExchange(&g_preserveEventLockAcrossRuntimeDips, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0) ==
            static_cast<LONG>(EventCaptureState::Locked) &&
        g_mapLockedGeneration == g_mapGeneration &&
        g_mapLockedController != nullptr &&
        g_mapLockedSelf != nullptr;
    if (preserveEventLock) {
        if (now - g_lastSuppressedContextClearLogTick >= 500) {
            g_lastSuppressedContextClearLogTick = now;
            char line[384]{};
            wsprintfA(line,
                "Runtime context suspension suppressed: event-locked player identity preserved. reason=%s.",
                reason != nullptr ? reason : "transient runtime dip");
            LogLine(line);
        }
        ResetPendingMovementQueue();
        return;
    }

    AcquireSRWLockExclusive(&g_localMoveCaptureLock);
    g_playerLocalMoveSelf = nullptr;
    g_playerLocalMoveArg1 = 0;
    ReleaseSRWLockExclusive(&g_localMoveCaptureLock);

    InterlockedExchange(&g_hasPlayerLocalMove, 0);
    InterlockedExchange(&g_contextLockedForLevel, 0);
    InterlockedExchange(&g_hasLastPlayerMonitorPosition, 0);
    InterlockedExchange(&g_levelBootstrapActive, 1);
    g_mapLockedGeneration = 0;
    g_lastSuccessfulKeyboardMoveGeneration = 0;
    g_mapLockedController = nullptr;
    g_mapLockedScene = 0;
    g_mapLockedActiveLevel = 0;
    g_mapLockedSelf = nullptr;
    g_mapLockedArg1 = 0;
    g_lastStrictDirectContextMatchTick = 0;
    g_bootstrapWindowUntilTick = now + 60000;
    g_passiveContextSuspendUntilTick = now + kPassiveContextSuspendMs;

    if (now - g_lastPassiveContextSuspendLogTick >= 500) {
        g_lastPassiveContextSuspendLogTick = now;
        char line[320]{};
        wsprintfA(
            line,
            "Passive context suspend for %lu ms: %s. Movement injection paused; aggressive reacquisition deferred.",
            kPassiveContextSuspendMs,
            reason != nullptr ? reason : "transient context monitor alert");
        LogLine(line);
    }
}

void EnterStabilityFallback(const char* reason, DWORD now) {
    g_stabilityFallbackUntilTick = now + kStabilityFallbackMs;
    g_stabilityInstabilityWindowStartTick = 0;
    g_stabilityInstabilityCount = 0;

    if (g_cameraAcquireActive) {
        g_cameraAcquireActive = false;
        g_cameraAcquirePreserveBasisOnFailure = false;
        g_cameraAcquireBlocksMovement = false;
        InterlockedExchange(
            &g_cameraAcquireOrigin,
            static_cast<LONG>(CameraAcquisitionOrigin::None));
    }

    if (now - g_lastStabilityFallbackLogTick >= 1000) {
        g_lastStabilityFallbackLogTick = now;
        char line[320]{};
        wsprintfA(
            line,
            "Stability fallback active for %lu ms: %s. Existing usable movement context is kept; extra reacquisition is suspended.",
            kStabilityFallbackMs,
            reason != nullptr ? reason : "repeated transient context instability");
        LogLine(line);
    }
}

void RecordContextInstability(DWORD now, const char* reason) {
    if (InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) == 0) {
        return;
    }
    if (IsTransitionLocalMoveBlocked(now) || IsStabilityFallbackActive(now)) {
        return;
    }

    if (g_stabilityInstabilityWindowStartTick == 0 ||
        now - g_stabilityInstabilityWindowStartTick > kStabilityInstabilityWindowMs) {
        g_stabilityInstabilityWindowStartTick = now;
        g_stabilityInstabilityCount = 1;
        return;
    }

    ++g_stabilityInstabilityCount;
    if (g_stabilityInstabilityCount >= kStabilityInstabilityThreshold) {
        EnterStabilityFallback(reason, now);
    }
}

bool CanUseStoredContextDuringStabilityFallback(DWORD now, void* self, DWORD arg1) {
    return IsStabilityFallbackActive(now) &&
        LooksLikeUsableStoredPlayerSelf(self) &&
        LooksLikeReadableLocalMoveArg1(arg1);
}

void FastResumeTransitionGuardAfterStableContext(DWORD now, const char* reason) {
    if (!IsTransitionLocalMoveBlocked(now)) {
        return;
    }

    const DWORD shortenedUntil = now + kTransitionFastResumeSettleMs;
    if (static_cast<LONG>(g_transitionLocalMoveBlockUntilTick - shortenedUntil) <= 0) {
        return;
    }

    g_transitionLocalMoveBlockUntilTick = shortenedUntil;
    char line[256]{};
    wsprintfA(
        line,
        "Transition guard fast-resume: stable movement context reacquired; remaining block reduced to %lu ms. reason=%s.",
        kTransitionFastResumeSettleMs,
        reason != nullptr ? reason : "context reacquired");
    LogLine(line);
}

void BlockLocalMoveForTransition(
    const char* reason,
    DWORD now,
    DWORD blockMs,
    bool clearCurrentContext) {
    const DWORD until = now + blockMs;
    if (g_transitionLocalMoveBlockUntilTick == 0 ||
        static_cast<LONG>(until - g_transitionLocalMoveBlockUntilTick) > 0) {
        g_transitionLocalMoveBlockUntilTick = until;
    }

    if (clearCurrentContext &&
        InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) != 0) {
        ClearPlayerLocalMoveContext(reason);
        return;
    }

    if (now - g_lastTransitionBlockLogTick >= 1000) {
        g_lastTransitionBlockLogTick = now;
        char line[256]{};
        wsprintfA(
            line,
            "Keyboard move transition guard active for %lu ms: %s.",
            blockMs,
            reason != nullptr ? reason : "transition detected");
        LogLine(line);
    }
}

void StartLevelBootstrapWindow(const char* reason) {
    g_bootstrapWindowUntilTick = GetTickCount() + 60000;
    const DWORD sceneGeneration = ++g_sceneGeneration;
    InterlockedExchange(&g_levelBootstrapActive, 1);
    InterlockedExchange(&g_cameraPostLoadRequest, 1);
    if (reason != nullptr && reason[0] != '\0') {
        char line[256]{};
        wsprintfA(line, "%s scene=%lu.", reason, sceneGeneration);
        LogLine(line);
    }
}

void BeginCameraAcquisition(
    DWORD now,
    DWORD scene,
    DWORD activeLevel,
    const char* reason) {
    if (IsLocalMoveExceptionRecoveryBlocked(now)) {
        if (now - g_lastLocalMoveExceptionQuarantineLogTick >= 500) {
            g_lastLocalMoveExceptionQuarantineLogTick = now;
            LogLine("Camera acquisition skipped: LocalMove exception quarantine is active.");
        }
        return;
    }

    if (IsStabilityFallbackActive(now) &&
        InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) != 0) {
        if (now - g_lastStabilityFallbackLogTick >= 1000) {
            g_lastStabilityFallbackLogTick = now;
            LogLine("Camera acquisition skipped: stability fallback is active and movement context is already usable.");
        }
        return;
    }

    const bool hasLevelIdentity = scene != 0 && activeLevel != 0;
    const DWORD acquireScene = hasLevelIdentity
        ? scene
        : (g_mapGeneration != 0 ? g_mapGeneration : g_sceneGeneration);
    const DWORD acquireActiveLevel = hasLevelIdentity ? activeLevel : 0;
    if (acquireScene == 0) {
        return;
    }
    if (g_cameraBasisAvailable &&
        g_cameraLatchedScene == acquireScene &&
        g_cameraLatchedActiveLevel == acquireActiveLevel) {
        return;
    }
    if (!g_cameraAcquireActive &&
        g_cameraModeInitialized &&
        !g_cameraBasisAvailable &&
        g_cameraAcquireScene == acquireScene &&
        g_cameraAcquireActiveLevel == acquireActiveLevel) {
        return;
    }
    if (g_cameraAcquireActive &&
        g_cameraAcquireScene == acquireScene &&
        g_cameraAcquireActiveLevel == acquireActiveLevel) {
        return;
    }

    g_playerCamera = nullptr;
    // Keep the provisional/current movement basis available while the engine is sampled.
    // The first valid engine result is applied immediately, but remains unconfirmed.
    g_cameraModeInitialized = true;
    g_cameraAcquireActive = true;
    InterlockedExchange(
        &g_cameraAcquireOrigin,
        static_cast<LONG>(CameraAcquisitionOrigin::InitialAutomatic));
    g_cameraAcquireScene = acquireScene;
    g_cameraAcquireActiveLevel = acquireActiveLevel;
    g_cameraAcquireRequiresLevelIdentity = hasLevelIdentity;
    g_cameraAcquireAttempts = 0;
    g_cameraAcquireWaitingForContextLogged = false;
    g_cameraAcquirePreserveBasisOnFailure = false;
    g_cameraCandidateForwardX = 0.0f;
    g_cameraCandidateForwardZ = 0.0f;
    g_cameraCandidateGameplayAngle = 0.0f;
    g_cameraCandidateStableSamples = 0;
    g_lastCameraSyncWaitLogTick = 0;
    const DWORD delayMs = static_cast<DWORD>(
        InterlockedCompareExchange(&g_cameraSyncDelayMs, 0, 0));
    const bool firstEngineObservationGate =
        g_cameraAcquireBlocksMovement &&
        InterlockedCompareExchange(&g_blockKeyboardUntilCameraSync, 0, 0) != 0;
    const DWORD timeoutMs = static_cast<DWORD>(
        InterlockedCompareExchange(
            firstEngineObservationGate
                ? &g_initialCameraObservationTimeoutMs
                : &g_cameraSyncTimeoutMs,
            0,
            0));
    g_cameraAcquireDueTick = now + delayMs;
    g_cameraAcquireUntilTick = g_cameraAcquireDueTick + timeoutMs;

    char line[352]{};
    wsprintfA(
        line,
        firstEngineObservationGate
            ? "Initial PlayerCam observation gate armed: token=0x%08X activeLevel=0x%08X identity=%s delay=%lu ms timeout=%lu ms reason=%s."
            : "Camera acquisition queued after player-context lock: token=0x%08X activeLevel=0x%08X identity=%s delay=%lu ms timeout=%lu ms reason=%s.",
        acquireScene,
        acquireActiveLevel,
        hasLevelIdentity ? "level" : "event-generation",
        delayMs,
        timeoutMs,
        reason != nullptr ? reason : "player context locked");
    LogLine(line);
}

void SetProvisionalDefaultCameraBasis(const char* reason) {
    g_cameraForwardX = kProvisionalCameraForwardX;
    g_cameraForwardZ = kProvisionalCameraForwardZ;
    g_cameraBasisAvailable = true;
    g_cameraModeInitialized = true;
    g_lastCameraVisualAngle = kProvisionalCameraAngle;
    g_lastCameraGameplayAngle = kProvisionalCameraAngle;
    InterlockedExchange(&g_cameraGameplayMode, 1);
    InterlockedExchange(&g_cameraBasisProvisional, 1);
    if (reason != nullptr && reason[0] != '\0') {
        char line[320]{};
        wsprintfA(
            line,
            "Provisional camera basis anchored at -45 degrees: %s; engine confirmation remains pending.",
            reason);
        LogLine(line);
    }
}

void InvalidateCameraForTransition() {
    g_playerCamera = nullptr;
    g_cameraAcquireActive = false;
    InterlockedExchange(
        &g_cameraAcquireOrigin,
        static_cast<LONG>(CameraAcquisitionOrigin::None));
    g_cameraAcquireScene = 0;
    g_cameraAcquireActiveLevel = 0;
    g_cameraLatchedScene = 0;
    g_cameraLatchedActiveLevel = 0;
    g_cameraAcquireRequiresLevelIdentity = false;
    g_cameraAcquireAttempts = 0;
    g_cameraAcquireWaitingForContextLogged = false;
    g_cameraAcquirePreserveBasisOnFailure = false;
    g_cameraAcquireBlocksMovement = false;
    g_cameraCandidateForwardX = 0.0f;
    g_cameraCandidateForwardZ = 0.0f;
    g_cameraCandidateGameplayAngle = 0.0f;
    g_cameraCandidateStableSamples = 0;
    g_lastCameraSyncWaitLogTick = 0;
    InterlockedExchange(&g_postReleaseCameraVerificationStage, 0);
    InterlockedExchange(&g_postReleaseCameraMessagePosted, 0);
    g_postReleaseCameraVerificationGeneration = 0;
    g_postReleaseCameraVerificationDueTick = 0;
    g_postReleaseCameraVerificationUntilTick = 0;
    g_postReleaseCameraVerificationEarliestBrakeTick = 0;
    g_postReleaseBrakeSentTick = 0;
    g_postReleaseObservedArg1 = 0;
    g_postReleaseStableReads = 0;
    g_lastPostReleaseContextPollTick = 0;
    g_postReleaseCameraVerificationBaselineAngle = kProvisionalCameraAngle;
    InterlockedExchange(&g_cameraEngineConcordantObservations, 0);
    g_cameraEngineReferenceAngle = kProvisionalCameraAngle;
    g_cameraEngineObservationArg1 = 0;
    g_cameraEngineObservationGeneration = g_mapGeneration;
    InterlockedExchange(&g_runtimeCameraContextEstablished, 0);
    InterlockedExchange(&g_postFirstMoveCameraWatchActive, 0);
    g_postFirstMoveCameraWatchGeneration = 0;
    g_postFirstMoveCameraWatchInitialArg1 = 0;
    g_postFirstMoveCameraWatchUntilTick = 0;
    g_runtimeTransitionCameraStartedTick = 0;
    g_runtimeTransitionCameraBaselineAngle = kProvisionalCameraAngle;
    SetProvisionalDefaultCameraBasis("new map generation; -45-degree fallback prepared while the loading camera and first runtime movement context are still provisional");
}

void BeginCameraSyncAfterEventContextLock(DWORD now) {
    if (InterlockedCompareExchange(&g_cameraSyncAfterContextLock, 0, 0) == 0) {
        return;
    }

    // This first acquisition may still describe the loading camera. Its value is
    // applied immediately when valid, but it never contributes to the 2/2 runtime
    // confidence. By default movement is not gated: the short post-first-move watch
    // observes the already-existing dynamic arg1 checks and starts a fresh PlayerCam
    // acquisition as soon as the runtime movement context appears.
    g_cameraAcquireBlocksMovement =
        InterlockedCompareExchange(&g_blockKeyboardUntilCameraSync, 0, 0) != 0;
    BeginCameraAcquisition(
        now,
        0,
        0,
        "event-locked player context is stable; sample the loading camera provisionally before the first runtime movement");
    if (!g_cameraAcquireActive) {
        g_cameraAcquireBlocksMovement = false;
    } else if (g_cameraAcquireBlocksMovement) {
        LogLine("Keyboard movement gated by configuration until the provisional loading-camera observation completes or the bounded fallback timeout.");
    }
}

void ResetPostReleaseCameraVerificationCycle() {
    InterlockedExchange(&g_postReleaseCameraVerificationStage, 0);
    InterlockedExchange(&g_postReleaseCameraMessagePosted, 0);
    g_postReleaseCameraVerificationGeneration = 0;
    g_postReleaseCameraVerificationDueTick = 0;
    g_postReleaseCameraVerificationUntilTick = 0;
    g_postReleaseCameraVerificationEarliestBrakeTick = 0;
    g_postReleaseBrakeSentTick = 0;
    g_postReleaseObservedArg1 = 0;
    g_postReleaseStableReads = 0;
    g_lastPostReleaseContextPollTick = 0;
}

bool IsAutomaticPostReleaseCameraAcquisitionActive() {
    return g_cameraAcquireActive &&
        g_cameraAcquirePreserveBasisOnFailure &&
        InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0) == 4 &&
        g_postReleaseCameraVerificationGeneration == g_mapGeneration;
}

void CancelAutomaticPostReleaseCameraVerificationForInput(const char* reason) {
    const LONG stage = InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0);
    if (stage < 1 || stage > 4 ||
        g_postReleaseCameraVerificationGeneration != g_mapGeneration) {
        return;
    }

    if (stage == 4 && IsAutomaticPostReleaseCameraAcquisitionActive()) {
        g_cameraAcquireActive = false;
        g_cameraAcquirePreserveBasisOnFailure = false;
        g_cameraAcquireBlocksMovement = false;
        InterlockedExchange(
            &g_cameraAcquireOrigin,
            static_cast<LONG>(CameraAcquisitionOrigin::None));
        g_cameraCandidateForwardX = 0.0f;
        g_cameraCandidateForwardZ = 0.0f;
        g_cameraCandidateGameplayAngle = 0.0f;
        g_cameraCandidateStableSamples = 0;
        g_lastCameraSyncWaitLogTick = 0;
        g_playerCamera = nullptr;
    }

    ResetPostReleaseCameraVerificationCycle();
    char line[320]{};
    wsprintfA(
        line,
        "Automatic PlayerCam verification deferred: %s; the previous movement basis is kept and the next keyboard release will retry.",
        reason != nullptr ? reason : "keyboard input resumed during the isolated acquisition");
    LogLine(line);
}

void ReopenPostReleaseCameraVerificationForNextRelease() {
    CancelAutomaticPostReleaseCameraVerificationForInput(
        "movement resumed before the isolated PlayerCam verification completed");
}

void StartRuntimeTransitionCameraAcquisition(
    DWORD now,
    DWORD previousArg1,
    DWORD currentArg1,
    const char* reason) {
    const CameraAcquisitionOrigin activeOrigin = static_cast<CameraAcquisitionOrigin>(
        InterlockedCompareExchange(&g_cameraAcquireOrigin, 0, 0));
    if (g_cameraAcquireActive && activeOrigin == CameraAcquisitionOrigin::Manual) {
        LogLine("Runtime-transition PlayerCam acquisition deferred because a manual F9 recalibration is already active.");
        return;
    }

    if (g_cameraAcquireActive) {
        g_cameraAcquireActive = false;
        g_cameraAcquirePreserveBasisOnFailure = false;
        g_cameraAcquireBlocksMovement = false;
    }

    const DWORD acquireScene = ++g_sceneGeneration;
    g_playerCamera = nullptr;
    g_cameraAcquireActive = true;
    InterlockedExchange(
        &g_cameraAcquireOrigin,
        static_cast<LONG>(CameraAcquisitionOrigin::RuntimeTransitionAutomatic));
    g_cameraAcquireScene = acquireScene;
    g_cameraAcquireActiveLevel = 0;
    g_cameraAcquireRequiresLevelIdentity = false;
    g_cameraAcquireAttempts = 0;
    g_cameraAcquireWaitingForContextLogged = false;
    g_cameraAcquirePreserveBasisOnFailure = true;
    g_cameraAcquireBlocksMovement = false;
    g_cameraCandidateForwardX = 0.0f;
    g_cameraCandidateForwardZ = 0.0f;
    g_cameraCandidateGameplayAngle = 0.0f;
    g_cameraCandidateStableSamples = 0;
    g_lastCameraSyncWaitLogTick = 0;
    g_runtimeTransitionCameraStartedTick = now;
    g_runtimeTransitionCameraBaselineAngle = g_lastCameraGameplayAngle;
    g_cameraAcquireDueTick = now;
    g_cameraAcquireUntilTick = now + static_cast<DWORD>(
        InterlockedCompareExchange(&g_runtimeTransitionCameraAcquireTimeoutMs, 0, 0));

    char line[512]{};
    wsprintfA(
        line,
        "Post-first-move runtime PlayerCam acquisition queued without blocking movement: scene=0x%08X oldArg1=0x%08X newArg1=0x%08X timeout=%ld ms sameAngleHold=%ld ms reason=%s.",
        acquireScene,
        previousArg1,
        currentArg1,
        InterlockedCompareExchange(&g_runtimeTransitionCameraAcquireTimeoutMs, 0, 0),
        InterlockedCompareExchange(&g_runtimeTransitionSameAngleHoldMs, 0, 0),
        reason != nullptr ? reason : "runtime movement context became authoritative");
    LogLine(line);
}

void ArmPostFirstMoveCameraTransitionWatch(DWORD now, DWORD arg1) {
    if (InterlockedCompareExchange(&g_enablePostFirstMoveCameraWatch, 0, 0) == 0) {
        InterlockedExchange(&g_runtimeCameraContextEstablished, 1);
        return;
    }
    if (InterlockedCompareExchange(&g_runtimeCameraContextEstablished, 0, 0) != 0 ||
        g_mapGeneration == 0 ||
        g_mapLockedGeneration != g_mapGeneration ||
        arg1 == 0 ||
        g_postFirstMoveCameraWatchGeneration == g_mapGeneration) {
        return;
    }

    g_postFirstMoveCameraWatchGeneration = g_mapGeneration;
    g_postFirstMoveCameraWatchInitialArg1 = arg1;
    g_postFirstMoveCameraWatchUntilTick = now + static_cast<DWORD>(
        InterlockedCompareExchange(&g_postFirstMoveCameraWatchMs, 0, 0));
    InterlockedExchange(&g_postFirstMoveCameraWatchActive, 1);

    char line[448]{};
    wsprintfA(
        line,
        "Post-first-move camera transition watch armed: generation=%lu initialArg1=0x%08X window=%ld ms; existing per-dispatch arg1 validation is reused and no continuous controller scan is enabled.",
        g_mapGeneration,
        arg1,
        InterlockedCompareExchange(&g_postFirstMoveCameraWatchMs, 0, 0));
    LogLine(line);
}

void PumpPostFirstMoveCameraTransitionWatch(DWORD now) {
    if (InterlockedCompareExchange(&g_postFirstMoveCameraWatchActive, 0, 0) == 0) {
        return;
    }
    if (g_postFirstMoveCameraWatchGeneration != g_mapGeneration ||
        g_mapLockedGeneration != g_mapGeneration) {
        InterlockedExchange(&g_postFirstMoveCameraWatchActive, 0);
        return;
    }
    if (g_postFirstMoveCameraWatchUntilTick == 0 ||
        static_cast<LONG>(now - g_postFirstMoveCameraWatchUntilTick) < 0) {
        return;
    }

    InterlockedExchange(&g_postFirstMoveCameraWatchActive, 0);
    InterlockedExchange(&g_runtimeCameraContextEstablished, 1);
    const DWORD currentArg1 = g_mapLockedArg1;
    StartRuntimeTransitionCameraAcquisition(
        now,
        g_postFirstMoveCameraWatchInitialArg1,
        currentArg1,
        "bounded post-first-move window expired without an arg1 promotion; the current movement context is now treated as runtime-stable");
    LogLine("Post-first-move transition watch completed without a new arg1; one bounded runtime PlayerCam acquisition was started and no watch remains active.");
}

void MaybeRearmCameraVerificationAfterRuntimeArg1Change(
    DWORD previousArg1,
    DWORD currentArg1) {
    if ((InterlockedCompareExchange(&g_verifyCameraAfterFirstKeyboardRelease, 0, 0) == 0 &&
            InterlockedCompareExchange(&g_enablePostFirstMoveCameraWatch, 0, 0) == 0) ||
        previousArg1 == 0 ||
        currentArg1 == 0 ||
        previousArg1 == currentArg1) {
        return;
    }

    const LONG observations = InterlockedCompareExchange(
        &g_cameraEngineConcordantObservations, 0, 0);
    const LONG stage = InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0);
    if (stage >= 1 && stage <= 4 &&
        g_postReleaseCameraVerificationGeneration == g_mapGeneration) {
        CancelAutomaticPostReleaseCameraVerificationForInput(
            "the runtime LocalMove arg1 changed while camera confirmation was pending");
    } else {
        ResetPostReleaseCameraVerificationCycle();
    }

    InterlockedExchange(&g_cameraEngineConcordantObservations, 0);
    InterlockedExchange(&g_cameraBasisProvisional, 1);
    InterlockedExchange(&g_runtimeCameraContextEstablished, 1);
    InterlockedExchange(&g_postFirstMoveCameraWatchActive, 0);
    g_cameraEngineReferenceAngle = g_lastCameraGameplayAngle;
    g_cameraEngineObservationArg1 = currentArg1;
    g_cameraEngineObservationGeneration = g_mapGeneration;

    char line[512]{};
    if (observations > 0) {
        wsprintfA(
            line,
            "Runtime LocalMove arg1 promotion invalidated %ld camera engine observation(s): old=0x%08X new=0x%08X; an immediate non-blocking PlayerCam acquisition will apply the post-promotion axis while movement may remain held.",
            observations,
            previousArg1,
            currentArg1);
    } else {
        wsprintfA(
            line,
            "Runtime LocalMove arg1 promotion established the first authoritative movement context: old=0x%08X new=0x%08X; an immediate non-blocking PlayerCam acquisition will apply the post-promotion axis while movement may remain held.",
            previousArg1,
            currentArg1);
    }
    LogLine(line);

    StartRuntimeTransitionCameraAcquisition(
        GetTickCount(),
        previousArg1,
        currentArg1,
        "dynamic arg1 promotion detected by the existing movement-dispatch validator");
}

void ArmPostReleaseCameraVerification(DWORD now) {
    if (InterlockedCompareExchange(&g_verifyCameraAfterFirstKeyboardRelease, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_runtimeCameraContextEstablished, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_cameraEngineConcordantObservations, 0, 0) >=
            kRequiredConcordantEngineCameraObservations ||
        g_mapGeneration == 0 ||
        g_mapLockedGeneration != g_mapGeneration ||
        g_lastSuccessfulKeyboardMoveGeneration != g_mapGeneration ||
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0) !=
            static_cast<LONG>(EventCaptureState::Locked)) {
        return;
    }

    if (InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 1, 0) != 0) {
        return;
    }

    g_postReleaseCameraVerificationGeneration = g_mapGeneration;
    g_postReleaseCameraVerificationDueTick = 0;
    const LONG releaseBrakeDelayMs = InterlockedCompareExchange(&g_releaseBrakeDelayMs, 0, 0);
    g_postReleaseCameraVerificationEarliestBrakeTick =
        now + static_cast<DWORD>(releaseBrakeDelayMs > 0 ? releaseBrakeDelayMs : 0) +
        kPostReleaseBrakeRetryGraceMs;
    g_postReleaseCameraVerificationUntilTick =
        g_postReleaseCameraVerificationEarliestBrakeTick + static_cast<DWORD>(
            InterlockedCompareExchange(&g_postReleaseContextTimeoutMs, 0, 0));
    g_lastPostReleaseContextPollTick = 0;
    g_postReleaseCameraVerificationBaselineAngle = g_lastCameraGameplayAngle;

    char line[352]{};
    wsprintfA(
        line,
        "Post-release PlayerCam confirmation armed: generation=%lu currentEngineObservations=%ld/2 previousAngleX100=%ld contextTimeoutMs=%ld; waiting for a stable release-brake context.",
        g_mapGeneration,
        InterlockedCompareExchange(&g_cameraEngineConcordantObservations, 0, 0),
        static_cast<LONG>(g_postReleaseCameraVerificationBaselineAngle * 100.0f),
        InterlockedCompareExchange(&g_postReleaseContextTimeoutMs, 0, 0));
    LogLine(line);
}

void MarkPostReleaseBrakeDispatched(DWORD now, DWORD arg1) {
    if (g_postReleaseCameraVerificationGeneration != g_mapGeneration ||
        InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 2, 1) != 1) {
        return;
    }

    g_postReleaseBrakeSentTick = now;
    g_postReleaseObservedArg1 = arg1;
    g_postReleaseStableReads = 0;
    g_postReleaseCameraVerificationUntilTick = now + static_cast<DWORD>(
        InterlockedCompareExchange(&g_postReleaseContextTimeoutMs, 0, 0));
    g_lastPostReleaseContextPollTick = 0;

    char line[352]{};
    wsprintfA(
        line,
        "Release brake dispatched with stable context: generation=%lu arg1=0x%08X; waiting for post-brake arg1 stability.",
        g_mapGeneration,
        arg1);
    LogLine(line);
}

void SchedulePostReleaseCameraRecalibration(DWORD now, DWORD arg1) {
    if (g_postReleaseCameraVerificationGeneration != g_mapGeneration ||
        InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 3, 2) != 2) {
        return;
    }

    const DWORD delayMs = static_cast<DWORD>(
        InterlockedCompareExchange(&g_postReleaseCameraVerificationDelayMs, 0, 0));
    g_postReleaseCameraVerificationDueTick = now + delayMs;
    g_postReleaseCameraVerificationUntilTick =
        g_postReleaseCameraVerificationDueTick + static_cast<DWORD>(
            InterlockedCompareExchange(&g_postReleaseCameraVerificationTimeoutMs, 0, 0));

    char line[384]{};
    wsprintfA(
        line,
        "Post-release movement context settled: generation=%lu arg1=0x%08X stableReads=%ld; isolated PlayerCam recalibration due in %lu ms.",
        g_mapGeneration,
        arg1,
        g_postReleaseStableReads,
        delayMs);
    LogLine(line);
}

void PumpPostReleaseCameraVerification(DWORD now) {
    const LONG stage = InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0);
    if (stage == 0 || stage >= 5) {
        return;
    }

    if (g_postReleaseCameraVerificationGeneration != g_mapGeneration ||
        g_mapLockedGeneration != g_mapGeneration) {
        InterlockedExchange(&g_postReleaseCameraVerificationStage, 5);
        return;
    }

    if (stage == 1 || stage == 2) {
        if (stage == 1 &&
            g_postReleaseCameraVerificationEarliestBrakeTick != 0 &&
            static_cast<LONG>(now - g_postReleaseCameraVerificationEarliestBrakeTick) < 0) {
            return;
        }
        if (g_postReleaseCameraVerificationUntilTick != 0 &&
            static_cast<LONG>(now - g_postReleaseCameraVerificationUntilTick) >= 0) {
            LogLine(stage == 1
                ? "Post-release camera confirmation deferred: no stable release-brake dispatch context appeared inside the bounded window; the next release will retry."
                : "Post-release camera confirmation deferred: arg1 did not stabilize after the release brake inside the bounded window; the next release will retry.");
            ResetPostReleaseCameraVerificationCycle();
            return;
        }

        const DWORD pollMs = static_cast<DWORD>(
            InterlockedCompareExchange(&g_postReleaseContextPollMs, 0, 0));
        if (g_lastPostReleaseContextPollTick != 0 &&
            now - g_lastPostReleaseContextPollTick < pollMs) {
            return;
        }
        g_lastPostReleaseContextPollTick = now;

        if (InterlockedCompareExchange(&g_postReleaseCameraMessagePosted, 1, 0) == 0) {
            if (!InstallGameWindowDispatchHook() ||
                !PostMessageW(g_dispatchGameWindow, kGameThreadPostReleaseCameraMessage, 0, 0)) {
                InterlockedExchange(&g_postReleaseCameraMessagePosted, 0);
            }
        }
        return;
    }

    if (stage == 3 &&
        g_postReleaseCameraVerificationDueTick != 0 &&
        static_cast<LONG>(now - g_postReleaseCameraVerificationDueTick) >= 0 &&
        !g_cameraAcquireActive) {
        const KeyboardInputSnapshot input = CaptureKeyboardInputSnapshot();
        if (input.moveMask == 0 &&
            input.gameFocusedForInput &&
            InterlockedCompareExchange(&g_overlayVisible, 1, 1) == 0) {
            StartPostReleaseCameraRecalibration(now);
        }
    }
}

bool BeginMapLoadGeneration(DWORD now, DWORD scene, DWORD activeLevel, const char* reason) {
    if (g_lastMapLoadSignalTick != 0 &&
        now - g_lastMapLoadSignalTick < 1200 &&
        g_mapLockedGeneration == 0 &&
        InterlockedCompareExchange(&g_mapGenerationSynthetic, 0, 0) == 0) {
        return false;
    }
    g_lastMapLoadSignalTick = now;
    ++g_mapGeneration;
    InterlockedExchange(&g_mapGenerationSynthetic, 0);
    g_mapIdentityScene = scene;
    g_mapIdentityActiveLevel = activeLevel;
    g_mapRenderedPlayerCamGeneration = 0;
    g_mapRenderedPlayerCamScene = 0;
    g_mapRenderedPlayerCamActiveLevel = 0;
    g_mapLockedGeneration = 0;
    g_lastSuccessfulKeyboardMoveGeneration = 0;
    g_mapLockedController = nullptr;
    g_mapLockedScene = 0;
    g_mapLockedActiveLevel = 0;
    g_mapLockedSelf = nullptr;
    g_mapLockedArg1 = 0;
    g_dynamicArg1Candidate = 0;
    g_dynamicArg1CandidateFirstTick = 0;
    g_dynamicArg1CandidateStableReads = 0;
    g_lastStrictDirectContextMatchTick = 0;
    ResetWasdOwnershipRecoveryState(true);
    ResetEmergencyWasdBootstrapState(true);
    InterlockedExchange(&g_keyboardFacingResetPending, 0);
    InvalidateCameraForTransition();

    if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0) {
        g_eventCaptureGeneration = g_mapGeneration;
        g_setRunningSeenGeneration = 0;
        g_setRunningSeenTick = 0;
        g_uiReadyGeneration = 0;
        g_uiReadyTick = 0;
        g_lastUiReadyWindow[0] = '\0';
        g_lastUiReadyText[0] = '\0';
        g_uiLifecycleLogLines = 0;
        g_lastCapturePumpTick = 0;
        g_eventCaptureStartedTick = 0;
        g_eventCaptureCandidateSelf = nullptr;
        g_eventCaptureCandidateArg1 = 0;
        g_eventCaptureCandidateStableFrames = 0;
        InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::Loading));
        InterlockedExchange(&g_captureMessagePosted, 0);
        ResetPendingMovementQueue();
    }

    char line[320]{};
    wsprintfA(
        line,
        "Map load generation %lu started: scene=0x%08X activeLevel=0x%08X reason=%s.",
        g_mapGeneration,
        scene,
        activeLevel,
        reason != nullptr ? reason : "environment load signal");
    LogLine(line);
    return true;
}

void PollTransitionDiagnostics(DWORD now) {
    if (!kEnableTransitionDiagnostics ||
        InterlockedCompareExchange(&g_enableTransitionDiagnosticsRuntime, 0, 0) == 0 ||
        now - g_lastTransitionDiagnosticPollTick < kTransitionDiagnosticPollMs) {
        return;
    }
    g_lastTransitionDiagnosticPollTick = now;

    DWORD gameRoot = 0;
    DWORD levelState = 0;
    DWORD scene = 0;
    DWORD activeLevel = 0;
    DWORD activeLevelVftable = 0;
    DWORD warpFlags = 0;
    ReadDwordSafe(reinterpret_cast<void*>(StaticToRuntime(0x031542B0)), 0, &gameRoot);
    if (gameRoot != 0) {
        ReadDwordSafe(reinterpret_cast<void*>(gameRoot), 0x10, &levelState);
        ReadDwordSafe(reinterpret_cast<void*>(gameRoot), 0xA8, &scene);
    }
    if (scene != 0) {
        ReadDwordSafe(reinterpret_cast<void*>(scene), 0x4C, &activeLevel);
    }
    if (activeLevel != 0) {
        ReadDwordSafe(reinterpret_cast<void*>(activeLevel), 0, &activeLevelVftable);
    }
    ReadDwordSafe(reinterpret_cast<void*>(StaticToRuntime(0x029048A0)), 0, &warpFlags);

    const bool firstSnapshot =
        InterlockedCompareExchange(&g_hasTransitionDiagnosticSnapshot, 1, 1) == 0;
    if (!firstSnapshot &&
        gameRoot == g_lastDiagnosticGameRoot &&
        levelState == g_lastDiagnosticLevelState &&
        scene == g_lastDiagnosticScene &&
        activeLevel == g_lastDiagnosticActiveLevel &&
        activeLevelVftable == g_lastDiagnosticSceneVftable &&
        warpFlags == g_lastDiagnosticWarpFlags) {
        return;
    }

    const bool sceneIdentityChanged =
        firstSnapshot ||
        scene != g_lastDiagnosticScene ||
        activeLevel != g_lastDiagnosticActiveLevel;

    g_lastDiagnosticGameRoot = gameRoot;
    g_lastDiagnosticLevelState = levelState;
    g_lastDiagnosticScene = scene;
    g_lastDiagnosticActiveLevel = activeLevel;
    g_lastDiagnosticSceneVftable = activeLevelVftable;
    g_lastDiagnosticWarpFlags = warpFlags;
    InterlockedExchange(&g_hasTransitionDiagnosticSnapshot, 1);

    char line[384]{};
    wsprintfA(
        line,
        "Transition state t=%lu root=0x%08X levelState=0x%08X scene=0x%08X activeLevel=0x%08X activeVft=0x%08X warpFlags=0x%08X hasMove=%ld.",
        now,
        gameRoot,
        levelState,
        scene,
        activeLevel,
        activeLevelVftable,
        warpFlags,
        InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1));
    LogLine(line);

    if (sceneIdentityChanged) {
        if (!firstSnapshot || scene != 0 || activeLevel != 0) {
            BeginMapLoadGeneration(
                now,
                scene,
                activeLevel,
                firstSnapshot ? "initial scene snapshot" : "scene identity changed");
        }
        BlockLocalMoveForTransition(
            firstSnapshot
                ? "Initial scene snapshot; local move paused until level settles."
                : "Scene identity changed; player local move context cleared.",
            now,
            firstSnapshot ? 800 : 1800,
            !firstSnapshot);
        InvalidateCameraForTransition();
        if (InterlockedCompareExchange(&g_enableCameraAutoCalibration, 0, 0) != 0) {
            BeginCameraAcquisition(now, scene, activeLevel, "stable scene identity detected");
        }
    }
}

void LogTransitionDiagnosticMessage(
    int messageIndex,
    const char* messageName,
    void* core,
    void* message,
    DWORD staticVftable,
    BYTE code,
    DWORD callerStatic) {
    if (!kEnableTransitionDiagnostics ||
        InterlockedCompareExchange(&g_enableTransitionDiagnosticsRuntime, 0, 0) == 0 ||
        messageIndex < 0 ||
        messageIndex >= 6) {
        return;
    }

    const LONG occurrence = InterlockedIncrement(&g_transitionMessageCounts[messageIndex]);
    const DWORD now = GetTickCount();
    if (occurrence > 4 && now - g_transitionMessageLastLogTick[messageIndex] < 1000) {
        return;
    }
    g_transitionMessageLastLogTick[messageIndex] = now;

    const LONG logLine = InterlockedIncrement(&g_transitionDiagnosticLogLines);
    if (logLine > 512) {
        return;
    }

    DWORD words[10]{};
    for (DWORD i = 0; i < 10; ++i) {
        ReadDwordSafe(message, i * sizeof(DWORD), &words[i]);
    }

    char line[512]{};
    wsprintfA(
        line,
        "Transition message #%ld t=%lu name=%s occurrence=%ld callerStatic=0x%08X core=0x%08X staticVft=0x%08X code=0x%02X raw08=0x%08X raw0C=0x%08X raw10=0x%08X raw14=0x%08X raw18=0x%08X raw1C=0x%08X raw20=0x%08X raw24=0x%08X hasMove=%ld scene=%lu.",
        logLine,
        now,
        messageName,
        occurrence,
        callerStatic,
        reinterpret_cast<DWORD>(core),
        staticVftable,
        code,
        words[2],
        words[3],
        words[4],
        words[5],
        words[6],
        words[7],
        words[8],
        words[9],
        InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1),
        g_sceneGeneration);
    LogLine(line);
}

void LogMessageProbeCall(void* core, void* message, void* callerReturn) {
    if (message == nullptr) {
        if (kVerboseHookDiagnostics) {
            LogLine("Message transition hook called with null message.");
        }
        return;
    }

    const DWORD moduleBase = reinterpret_cast<DWORD>(GetModuleHandleW(nullptr));
    DWORD vftable = 0;
    BYTE code = 0;
    if (!ReadDwordSafe(message, 0x00, &vftable) || !ReadByteSafe(message, 0x04, &code)) {
        if (kVerboseHookDiagnostics) {
            LogLine("Message send 0x65B2D0 skipped: message header is unreadable.");
        }
        return;
    }
    constexpr DWORD kOriginalImageBase = 0x00400000;
    constexpr DWORD kSetRunningStatic = 0x0214E81C;
    constexpr DWORD kSpawnStatic = 0x0214E84C;
    constexpr DWORD kPostSpawnSettingsStatic = 0x0214E60C;
    constexpr DWORD kRequestLevelOwnershipStatic = 0x0214AC3C;
    constexpr DWORD kSendLevelOwnershipStatic = 0x0214E61C;
    constexpr DWORD kRerollLevelStatic = 0x0214AD0C;
    constexpr DWORD kSetLevelStatic = 0x0214E92C;
    const DWORD setRunningVftable = moduleBase + (kSetRunningStatic - kOriginalImageBase);
    const DWORD spawnVftable = moduleBase + (kSpawnStatic - kOriginalImageBase);
    const DWORD postSpawnSettingsVftable = moduleBase + (kPostSpawnSettingsStatic - kOriginalImageBase);
    const DWORD requestLevelOwnershipVftable = moduleBase + (kRequestLevelOwnershipStatic - kOriginalImageBase);
    const DWORD sendLevelOwnershipVftable = moduleBase + (kSendLevelOwnershipStatic - kOriginalImageBase);
    const DWORD rerollLevelVftable = moduleBase + (kRerollLevelStatic - kOriginalImageBase);
    const DWORD setLevelVftable = moduleBase + (kSetLevelStatic - kOriginalImageBase);
    const DWORD staticVftable = vftable - moduleBase + kOriginalImageBase;
    const DWORD caller = reinterpret_cast<DWORD>(callerReturn);
    const DWORD callerStatic = caller - moduleBase + kOriginalImageBase;

    int transitionMessageIndex = -1;
    const char* transitionMessageName = nullptr;
    if (vftable == spawnVftable) {
        transitionMessageIndex = 0;
        transitionMessageName = "UNetSpawnMsg";
    } else if (vftable == postSpawnSettingsVftable) {
        transitionMessageIndex = 1;
        transitionMessageName = "UNetPostSpawnSettingsMsg";
    } else if (vftable == setLevelVftable) {
        transitionMessageIndex = 2;
        transitionMessageName = "UNetSetLevelMsg";
    } else if (vftable == requestLevelOwnershipVftable) {
        transitionMessageIndex = 3;
        transitionMessageName = "UNetRequestLevelOwnershipMsg";
    } else if (vftable == sendLevelOwnershipVftable) {
        transitionMessageIndex = 4;
        transitionMessageName = "UNetSendLevelOwnershipMsg";
    } else if (vftable == rerollLevelVftable) {
        transitionMessageIndex = 5;
        transitionMessageName = "UNetRerollLevelMsg";
    }
    if (vftable == setLevelVftable && core != nullptr) {
        bool controllerChanged = false;
        AcquireSRWLockExclusive(&g_localMoveCaptureLock);
        if (g_playerMoveController != core) {
            g_playerMoveController = core;
            controllerChanged = true;
        }
        ReleaseSRWLockExclusive(&g_localMoveCaptureLock);
        InterlockedExchange(&g_hasPlayerMoveController, 1);
        if (controllerChanged) {
            InterlockedExchange(&g_hasDirectContextDiagnosticSnapshot, 0);
            char line[192]{};
            wsprintfA(
                line,
                "Player move controller captured from message core: controller=0x%08X.",
                reinterpret_cast<DWORD>(core));
            LogLine(line);
        }
    }
    if (transitionMessageName != nullptr) {
        LogTransitionDiagnosticMessage(
            transitionMessageIndex,
            transitionMessageName,
            core,
            message,
            staticVftable,
            code,
            callerStatic);

        const bool startsRealLevelLoad =
            vftable == setLevelVftable || vftable == rerollLevelVftable;
        if (startsRealLevelLoad) {
            const DWORD now = GetTickCount();
            ResetEmergencyWasdBootstrapState(true);
            char reason[160]{};
            wsprintfA(
                reason,
                "Transition message %s detected; event capture reset for the next map.",
                transitionMessageName);
            BeginMapLoadGeneration(now, 0, 0, transitionMessageName);
            const bool clearContext =
                InterlockedCompareExchange(&g_clearContextOnSetLevel, 0, 0) != 0;
            BlockLocalMoveForTransition(reason, now, 10000, clearContext);
        }
    }

    const bool exactSetRunningMessage = vftable == setRunningVftable;
    const bool legacySetRunningCodeFallback =
        InterlockedCompareExchange(&g_useExactSetRunningVftableOnly, 0, 0) == 0 && code == 0x4F;
    if (exactSetRunningMessage || legacySetRunningCodeFallback) {
        const DWORD threadId = GetCurrentThreadId();
        const DWORD setRunningNow = GetTickCount();
        g_setRunningThreadId = threadId;
        if (exactSetRunningMessage && core != nullptr) {
            ObserveEmergencySetRunningController(setRunningNow, core);
        }
        if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0 &&
            g_mapGeneration != 0) {
            const LONG currentCaptureState =
                InterlockedCompareExchange(&g_eventCaptureState, 0, 0);
            const bool alreadyLockedForGeneration =
                currentCaptureState == static_cast<LONG>(EventCaptureState::Locked) &&
                g_eventCaptureGeneration == g_mapGeneration &&
                g_mapLockedGeneration == g_mapGeneration;
            const bool alreadyFailedForGeneration =
                currentCaptureState == static_cast<LONG>(EventCaptureState::Failed) &&
                g_eventCaptureGeneration == g_mapGeneration;
            if (!alreadyLockedForGeneration && !alreadyFailedForGeneration) {
                const bool firstSetRunningForGeneration =
                    g_setRunningSeenGeneration != g_mapGeneration;
                g_setRunningSeenGeneration = g_mapGeneration;
                g_eventCaptureGeneration = g_mapGeneration;
                if (firstSetRunningForGeneration) {
                    g_setRunningSeenTick = setRunningNow;
                    g_eventCaptureStartedTick = 0;
                    g_eventCaptureCandidateSelf = nullptr;
                    g_eventCaptureCandidateArg1 = 0;
                    g_eventCaptureCandidateStableFrames = 0;
                }
                InterlockedExchange(
                    &g_eventCaptureState,
                    static_cast<LONG>(EventCaptureState::AwaitReadySignal));
                if (firstSetRunningForGeneration &&
                    InterlockedCompareExchange(&g_logCaptureLifecycle, 0, 0) != 0) {
                    char line[320]{};
                    wsprintfA(
                        line,
                        "Event capture armed by UNetSetRunningMsg: generation=%lu thread=%lu; capture pump armed; UI/PlayerCam gates are optional.",
                        g_mapGeneration,
                        threadId);
                    LogLine(line);
                }
                const bool uiReady = g_uiReadyGeneration == g_mapGeneration;
                const bool uiRequired = InterlockedCompareExchange(&g_requireUiReadyBeforeCapture, 0, 0) != 0;
                const bool camRequired = InterlockedCompareExchange(&g_requirePlayerCamBeforeCapture, 0, 0) != 0;
                const bool camReady = g_mapRenderedPlayerCamGeneration == g_mapGeneration;
                if ((!uiRequired || uiReady) && (!camRequired || camReady)) {
                    InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::Capturing));
                    PostCaptureTickToGameWindow();
                }
            }
        } else if (InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) == 0) {
            if (InterlockedExchange(&g_levelBootstrapActive, 1) == 0) {
                StartLevelBootstrapWindow("Post-load gameplay signal detected; level-ready bootstrap window opened.");
            }
        }
    }
}

bool MarkLocalMoveSelfForDetails(void* self) {
    const LONG count = InterlockedCompareExchange(&g_loggedLocalMoveSelfCount, 0, 0);
    for (LONG i = 0; i < count && i < 16; ++i) {
        if (g_loggedLocalMoveSelfs[i] == self) {
            return false;
        }
    }

    const LONG index = InterlockedIncrement(&g_loggedLocalMoveSelfCount) - 1;
    if (index < 0 || index >= 16) {
        return false;
    }

    g_loggedLocalMoveSelfs[index] = self;
    return true;
}

void LogLocalMoveSelfDetails(void* self) {
    if (!MarkLocalMoveSelfForDetails(self)) {
        return;
    }

    DWORD d000 = 0;
    DWORD d004 = 0;
    DWORD d008 = 0;
    DWORD d00C = 0;
    DWORD d010 = 0;
    DWORD d070 = 0;
    DWORD d074 = 0;
    DWORD d078 = 0;
    DWORD d1A8 = 0;
    DWORD d278 = 0;
    DWORD d27C = 0;
    DWORD d280 = 0;
    DWORD d314 = 0;
    DWORD d368 = 0;
    DWORD d36C = 0;
    DWORD d370 = 0;

    ReadDwordSafe(self, 0x000, &d000);
    ReadDwordSafe(self, 0x004, &d004);
    ReadDwordSafe(self, 0x008, &d008);
    ReadDwordSafe(self, 0x00C, &d00C);
    ReadDwordSafe(self, 0x010, &d010);
    ReadDwordSafe(self, 0x070, &d070);
    ReadDwordSafe(self, 0x074, &d074);
    ReadDwordSafe(self, 0x078, &d078);
    ReadDwordSafe(self, 0x1A8, &d1A8);
    ReadDwordSafe(self, 0x278, &d278);
    ReadDwordSafe(self, 0x27C, &d27C);
    ReadDwordSafe(self, 0x280, &d280);
    ReadDwordSafe(self, 0x314, &d314);
    ReadDwordSafe(self, 0x368, &d368);
    ReadDwordSafe(self, 0x36C, &d36C);
    ReadDwordSafe(self, 0x370, &d370);

    char line[768]{};
    wsprintfA(
        line,
        "Local move self fingerprint: self=0x%08X d000=0x%08X d004=0x%08X d008=0x%08X d00C=0x%08X d010=0x%08X pos70=0x%08X pos74=0x%08X pos78=0x%08X d1A8=0x%08X d278=0x%08X d27C=0x%08X d280=0x%08X d314=0x%08X d368=0x%08X d36C=0x%08X d370=0x%08X.",
        reinterpret_cast<DWORD>(self),
        d000,
        d004,
        d008,
        d00C,
        d010,
        d070,
        d074,
        d078,
        d1A8,
        d278,
        d27C,
        d280,
        d314,
        d368,
        d36C,
        d370);
    LogLine(line);
}

bool MarkArg1ForDetails(void* arg1) {
    if (arg1 == nullptr) {
        return false;
    }

    const LONG count = InterlockedCompareExchange(&g_loggedArg1PointerCount, 0, 0);
    for (LONG i = 0; i < count && i < 8; ++i) {
        if (g_loggedArg1Pointers[i] == arg1) {
            return false;
        }
    }

    const LONG index = InterlockedIncrement(&g_loggedArg1PointerCount) - 1;
    if (index < 0 || index >= 8) {
        return false;
    }

    g_loggedArg1Pointers[index] = arg1;
    return true;
}

void LogArg1Details(DWORD arg1) {
    void* pointer = reinterpret_cast<void*>(arg1);
    if (!MarkArg1ForDetails(pointer)) {
        return;
    }

    DWORD d000 = 0;
    DWORD d004 = 0;
    DWORD d008 = 0;
    DWORD d00C = 0;
    DWORD d010 = 0;
    DWORD d014 = 0;
    DWORD d018 = 0;
    DWORD d01C = 0;
    DWORD d020 = 0;
    DWORD d024 = 0;
    DWORD d028 = 0;
    DWORD d02C = 0;
    DWORD d030 = 0;
    DWORD d034 = 0;
    DWORD d038 = 0;
    DWORD d03C = 0;

    ReadDwordSafe(pointer, 0x000, &d000);
    ReadDwordSafe(pointer, 0x004, &d004);
    ReadDwordSafe(pointer, 0x008, &d008);
    ReadDwordSafe(pointer, 0x00C, &d00C);
    ReadDwordSafe(pointer, 0x010, &d010);
    ReadDwordSafe(pointer, 0x014, &d014);
    ReadDwordSafe(pointer, 0x018, &d018);
    ReadDwordSafe(pointer, 0x01C, &d01C);
    ReadDwordSafe(pointer, 0x020, &d020);
    ReadDwordSafe(pointer, 0x024, &d024);
    ReadDwordSafe(pointer, 0x028, &d028);
    ReadDwordSafe(pointer, 0x02C, &d02C);
    ReadDwordSafe(pointer, 0x030, &d030);
    ReadDwordSafe(pointer, 0x034, &d034);
    ReadDwordSafe(pointer, 0x038, &d038);
    ReadDwordSafe(pointer, 0x03C, &d03C);

    char line[768]{};
    wsprintfA(
        line,
        "Local move arg1 fingerprint: arg1=0x%08X d000=0x%08X d004=0x%08X d008=0x%08X d00C=0x%08X d010=0x%08X d014=0x%08X d018=0x%08X d01C=0x%08X d020=0x%08X d024=0x%08X d028=0x%08X d02C=0x%08X d030=0x%08X d034=0x%08X d038=0x%08X d03C=0x%08X.",
        arg1,
        d000,
        d004,
        d008,
        d00C,
        d010,
        d014,
        d018,
        d01C,
        d020,
        d024,
        d028,
        d02C,
        d030,
        d034,
        d038,
        d03C);
    LogLine(line);
}

bool LooksLikeReadableLocalMoveArg1(DWORD arg1) {
    if (arg1 == 0) {
        return false;
    }

    void* pointer = reinterpret_cast<void*>(arg1);
    DWORD d000 = 0;
    DWORD d004 = 0;
    DWORD d008 = 0;
    DWORD d00C = 0;
    DWORD d010 = 0;
    if (!ReadDwordSafe(pointer, 0x000, &d000) ||
        !ReadDwordSafe(pointer, 0x004, &d004) ||
        !ReadDwordSafe(pointer, 0x008, &d008) ||
        !ReadDwordSafe(pointer, 0x00C, &d00C) ||
        !ReadDwordSafe(pointer, 0x010, &d010)) {
        return false;
    }

    if (d000 == StaticToRuntime(0x0217FFCC) &&
        d008 == StaticToRuntime(0x0217FFC0) &&
        d00C == StaticToRuntime(0x0217FFB0)) {
        return true;
    }

    return d000 != 0 &&
        d008 != 0 &&
        d00C != 0 &&
        (d004 != 0 || d010 != 0);
}

bool LooksLikePlayerLocalMoveSelf(void* self) {
    DWORD d000 = 0;
    DWORD d1A8 = 0;
    DWORD d314 = 0;
    ReadDwordSafe(self, 0x000, &d000);
    ReadDwordSafe(self, 0x1A8, &d1A8);
    ReadDwordSafe(self, 0x314, &d314);

    float currentX = 0.0f;
    float currentZ = 0.0f;
    if (!ReadFloatSafe(self, 0x70, &currentX) || !ReadFloatSafe(self, 0x78, &currentZ)) {
        return false;
    }

    const bool coordinatesValid =
        currentX == currentX &&
        currentZ == currentZ &&
        !(currentX > -0.01f && currentX < 0.01f && currentZ > -0.01f && currentZ < 0.01f) &&
        currentX > -100000.0f &&
        currentX < 100000.0f &&
        currentZ > -100000.0f &&
        currentZ < 100000.0f;

    return d000 == StaticToRuntime(0x0217EA64) &&
        d314 == 0 &&
        (d1A8 & 0xFF000000) != 0 &&
        coordinatesValid;
}

bool LooksLikeUsableStoredPlayerSelf(void* self) {
    DWORD d000 = 0;
    if (!ReadDwordSafe(self, 0x000, &d000) || d000 != StaticToRuntime(0x0217EA64)) {
        return false;
    }

    float currentX = 0.0f;
    float currentZ = 0.0f;
    if (!ReadFloatSafe(self, 0x70, &currentX) || !ReadFloatSafe(self, 0x78, &currentZ)) {
        return false;
    }

    return currentX == currentX &&
        currentZ == currentZ &&
        currentX > -100000.0f &&
        currentX < 100000.0f &&
        currentZ > -100000.0f &&
        currentZ < 100000.0f;
}

bool LooksLikeUiReadyPlayerSelf(void* self) {
    // d314 and an exact (0,0) position are not reliable loading markers. Interior
    // maps, immobilising skills and forced movement can legitimately expose those
    // states. The controller chain, player vtable and readable finite coordinates
    // are the stable identity checks.
    return LooksLikeUsableStoredPlayerSelf(self);
}

bool LooksLikeAdjacentLoadingContext(void* self, DWORD arg1) {
    if (InterlockedCompareExchange(&g_rejectAdjacentLoadingContext, 0, 0) == 0 ||
        self == nullptr || arg1 == 0) {
        return false;
    }
    const DWORD selfAddress = reinterpret_cast<DWORD>(self);
    const DWORD distance = selfAddress > arg1 ? selfAddress - arg1 : arg1 - selfAddress;
    const DWORD maximum = static_cast<DWORD>(
        InterlockedCompareExchange(&g_adjacentLoadingContextMaxDistance, 0, 0));
    return distance <= maximum;
}

bool LooksLikeSafeLocalMoveDispatchSelf(void* self, char* reasonOut, DWORD reasonCapacity) {
    if (InterlockedCompareExchange(&g_validateLocalMoveDispatchList, 0, 0) == 0) return true;
    if (reasonOut != nullptr && reasonCapacity > 0) reasonOut[0] = '\0';
    if (self == nullptr) {
        if (reasonOut != nullptr && reasonCapacity > 0) lstrcpynA(reasonOut, "self is null", reasonCapacity);
        return false;
    }

    const DWORD listOffset = static_cast<DWORD>(
        InterlockedCompareExchange(&g_localMoveDispatchListOffset, 0, 0));
    const DWORD maxEntries = static_cast<DWORD>(
        InterlockedCompareExchange(&g_localMoveDispatchListMaxEntries, 0, 0));
    DWORD entriesBase = 0;
    DWORD entryCount = 0;
    DWORD inlineThreshold = 0;
    if (!ReadDwordSafe(self, listOffset + 0x00, &entriesBase) ||
        !ReadDwordSafe(self, listOffset + 0x04, &entryCount) ||
        !ReadDwordSafe(self, listOffset + 0x08, &inlineThreshold)) {
        if (reasonOut != nullptr && reasonCapacity > 0) lstrcpynA(reasonOut, "dispatch list header unreadable", reasonCapacity);
        return false;
    }
    if (entryCount == 0) return true;
    if (entriesBase == 0) {
        if (reasonOut != nullptr && reasonCapacity > 0) lstrcpynA(reasonOut, "dispatch list base is null while count is non-zero", reasonCapacity);
        return false;
    }
    if (entryCount > maxEntries) {
        if (reasonOut != nullptr && reasonCapacity > 0) wsprintfA(reasonOut, "dispatch list count %lu exceeds guard maximum %lu", entryCount, maxEntries);
        return false;
    }

    // This mirrors Torchlight2.exe+0x136150..+0x13618E exactly. In particular,
    // indices >= self+0x6B8 intentionally reuse the pointer stored at self+0x6B0.
    for (DWORD i = 0; i < entryCount; ++i) {
        const DWORD slotAddress = i < inlineThreshold
            ? entriesBase + i * sizeof(DWORD)
            : entriesBase;
        DWORD entry = 0;
        if (!ReadDwordSafe(reinterpret_cast<void*>(slotAddress), 0, &entry)) {
            if (reasonOut != nullptr && reasonCapacity > 0) wsprintfA(reasonOut, "dispatch list slot %lu unreadable at 0x%08X", i, slotAddress);
            return false;
        }
        if (entry == 0) {
            if (reasonOut != nullptr && reasonCapacity > 0) wsprintfA(reasonOut, "dispatch list slot %lu is null at 0x%08X", i, slotAddress);
            return false;
        }
        DWORD entryVtable = 0;
        if (!ReadDwordSafe(reinterpret_cast<void*>(entry), 0, &entryVtable) || entryVtable == 0) {
            if (reasonOut != nullptr && reasonCapacity > 0) wsprintfA(reasonOut, "dispatch list slot %lu points to an invalid object 0x%08X", i, entry);
            return false;
        }
        DWORD dispatchVtable = 0;
        if (!ReadDwordSafe(reinterpret_cast<void*>(entryVtable), 0, &dispatchVtable) || dispatchVtable == 0) {
            if (reasonOut != nullptr && reasonCapacity > 0) wsprintfA(reasonOut, "dispatch list slot %lu has invalid vtable chain 0x%08X", i, entryVtable);
            return false;
        }
        DWORD dispatchMethod = 0;
        if (!ReadDwordSafe(reinterpret_cast<void*>(dispatchVtable), 0x188, &dispatchMethod) || dispatchMethod == 0) {
            if (reasonOut != nullptr && reasonCapacity > 0) lstrcpynA(reasonOut, "dispatch list slot has no callable movement method", reasonCapacity);
            return false;
        }
    }
    return true;
}

bool LooksLikeCompanionLocalMoveSelf(void* self) {
    DWORD d314 = 0;
    DWORD d370 = 0;
    ReadDwordSafe(self, 0x314, &d314);
    ReadDwordSafe(self, 0x370, &d370);
    return d314 == 2 || d370 == 0xFFFFFF00;
}

void SetPlayerLocalMoveContext(void* self, DWORD arg1, const char* reason);

bool ReadDirectPlayerContextFromController(
    void* controller,
    void** playerOut,
    DWORD* linkOut,
    DWORD* arg1Out) {
    DWORD player = 0;
    DWORD link = 0;
    DWORD arg1 = 0;
    if (controller != nullptr) {
        ReadDwordSafe(controller, 0x2C, &player);
    }
    if (player != 0) {
        ReadDwordSafe(reinterpret_cast<void*>(player), 0x1C4, &link);
    }
    if (link != 0) {
        ReadDwordSafe(reinterpret_cast<void*>(link), 0x08, &arg1);
    }

    *playerOut = reinterpret_cast<void*>(player);
    *linkOut = link;
    *arg1Out = arg1;
    return player != 0 && link != 0 && arg1 != 0;
}

bool HasUsableCurrentEventLockedOwnership() {
    if (InterlockedCompareExchange(&g_eventCaptureState, 0, 0) !=
            static_cast<LONG>(EventCaptureState::Locked) ||
        g_mapGeneration == 0 ||
        g_mapLockedGeneration != g_mapGeneration ||
        g_mapLockedController == nullptr ||
        g_mapLockedSelf == nullptr ||
        g_mapLockedArg1 == 0) {
        return false;
    }

    void* player = nullptr;
    DWORD link = 0;
    DWORD arg1 = 0;
    if (!ReadDirectPlayerContextFromController(
            g_mapLockedController,
            &player,
            &link,
            &arg1)) {
        return false;
    }
    return player == g_mapLockedSelf &&
        LooksLikeUsableStoredPlayerSelf(player) &&
        LooksLikeReadableLocalMoveArg1(arg1);
}

void ResetEmergencyWasdBootstrapState(bool clearEvidence) {
    InterlockedExchange(&g_emergencyWasdBootstrapActive, 0);
    InterlockedExchange(&g_emergencyWasdBootstrapManualRequested, 0);
    g_emergencyWasdBootstrapUntilTick = 0;
    g_emergencyWasdBootstrapRetryAfterTick = 0;
    g_emergencyWasdBootstrapNextAttemptTick = 0;
    if (!clearEvidence) {
        return;
    }

    InterlockedExchange(&g_emergencyWasdBootstrapPostLoadSeen, 0);
    AcquireSRWLockExclusive(&g_emergencyWasdBootstrapLock);
    g_emergencyWasdSetRunningController = nullptr;
    g_emergencyWasdSetRunningTick = 0;
    g_emergencyWasdVanillaController = nullptr;
    g_emergencyWasdVanillaSelf = nullptr;
    g_emergencyWasdVanillaArg1 = 0;
    g_emergencyWasdVanillaEvidenceTick = 0;
    g_emergencyWasdVanillaEvidenceReads = 0;
    ReleaseSRWLockExclusive(&g_emergencyWasdBootstrapLock);
}

void ArmEmergencyWasdBootstrap(DWORD now, bool manual, const char* reason) {
    if (InterlockedCompareExchange(&g_enableEmergencyWasdBootstrap, 0, 0) == 0) {
        return;
    }
    if (!manual &&
        InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0) == 0 &&
        InterlockedCompareExchange(&g_emergencyWasdBootstrapPostLoadSeen, 0, 0) == 0) {
        return;
    }
    if (HasUsableCurrentEventLockedOwnership()) {
        ResetEmergencyWasdBootstrapState(false);
        return;
    }
    if (!manual && g_emergencyWasdBootstrapRetryAfterTick != 0 &&
        static_cast<LONG>(now - g_emergencyWasdBootstrapRetryAfterTick) < 0) {
        return;
    }

    const bool alreadyActive =
        InterlockedCompareExchange(&g_emergencyWasdBootstrapActive, 1, 1) != 0;
    if (manual) {
        InterlockedExchange(&g_emergencyWasdBootstrapManualRequested, 1);
    }
    if (alreadyActive) {
        return;
    }

    InterlockedExchange(&g_emergencyWasdBootstrapActive, 1);
    g_emergencyWasdBootstrapUntilTick = now + static_cast<DWORD>(
        InterlockedCompareExchange(&g_emergencyWasdBootstrapWindowMs, 0, 0));
    g_emergencyWasdBootstrapRetryAfterTick = 0;
    g_emergencyWasdBootstrapNextAttemptTick = now;

    char line[512]{};
    wsprintfA(
        line,
        "Emergency WASD bootstrap armed: mapGeneration=%lu eventState=%ld manual=%d window=%ld ms reason=%s.",
        g_mapGeneration,
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0),
        manual ? 1 : 0,
        InterlockedCompareExchange(&g_emergencyWasdBootstrapWindowMs, 0, 0),
        reason != nullptr ? reason : "movement ownership is missing before a normal SetLevel lock");
    LogLine(line);
}

void ObserveEmergencySetRunningController(DWORD now, void* controller) {
    if (controller == nullptr ||
        InterlockedCompareExchange(&g_enableEmergencyWasdBootstrap, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_useSetRunningCoreAsEmergencyController, 0, 0) == 0) {
        return;
    }

    AcquireSRWLockExclusive(&g_emergencyWasdBootstrapLock);
    g_emergencyWasdSetRunningController = controller;
    g_emergencyWasdSetRunningTick = now;
    ReleaseSRWLockExclusive(&g_emergencyWasdBootstrapLock);

    const LONG state = InterlockedCompareExchange(&g_eventCaptureState, 0, 0);
    const bool orphanSetRunning =
        g_mapGeneration == 0 || state == static_cast<LONG>(EventCaptureState::Failed);
    if (!orphanSetRunning) {
        return;
    }

    InterlockedExchange(&g_emergencyWasdBootstrapPostLoadSeen, 1);
    if (InterlockedCompareExchange(&g_autoEmergencyBootstrapOnOrphanSetRunning, 0, 0) != 0) {
        ArmEmergencyWasdBootstrap(
            now,
            false,
            g_mapGeneration == 0
                ? "UNetSetRunningMsg arrived without any preceding UNetSetLevelMsg"
                : "UNetSetRunningMsg arrived after the normal event capture had failed");
    }
}

void ObserveConfirmedVanillaPlayerMoveForEmergencyBootstrap(
    DWORD now,
    void* sourceController,
    void* self,
    DWORD arg1,
    void* directPlayer,
    DWORD directArg1) {
    if (InterlockedCompareExchange(&g_enableEmergencyWasdBootstrap, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_useVanillaMoveAsEmergencyBootstrapEvidence, 0, 0) == 0 ||
        sourceController == nullptr || self == nullptr || arg1 == 0 ||
        directPlayer != self || directArg1 != arg1 ||
        !LooksLikeUsableStoredPlayerSelf(self) ||
        !LooksLikeReadableLocalMoveArg1(arg1) ||
        LooksLikeAdjacentLoadingContext(self, arg1)) {
        return;
    }

    AcquireSRWLockExclusive(&g_emergencyWasdBootstrapLock);
    if (g_emergencyWasdVanillaController == sourceController &&
        g_emergencyWasdVanillaSelf == self &&
        g_emergencyWasdVanillaArg1 == arg1) {
        ++g_emergencyWasdVanillaEvidenceReads;
    } else {
        g_emergencyWasdVanillaController = sourceController;
        g_emergencyWasdVanillaSelf = self;
        g_emergencyWasdVanillaArg1 = arg1;
        g_emergencyWasdVanillaEvidenceReads = 1;
    }
    g_emergencyWasdVanillaEvidenceTick = now;
    const LONG reads = g_emergencyWasdVanillaEvidenceReads;
    ReleaseSRWLockExclusive(&g_emergencyWasdBootstrapLock);

    if (!HasUsableCurrentEventLockedOwnership()) {
        if (now - g_lastEmergencyWasdBootstrapLogTick >= 500) {
            g_lastEmergencyWasdBootstrapLogTick = now;
            char line[448]{};
            wsprintfA(
                line,
                "Confirmed vanilla player LocalMove retained as emergency bootstrap evidence: controller=0x%08X self=0x%08X arg1=0x%08X reads=%ld.",
                reinterpret_cast<DWORD>(sourceController),
                reinterpret_cast<DWORD>(self),
                arg1,
                reads);
            LogLine(line);
        }
        ArmEmergencyWasdBootstrap(
            now,
            false,
            "confirmed vanilla player LocalMove exists but no usable event-locked WASD ownership exists");
    }
}

bool TryStartEmergencyWasdEventCapture(DWORD now) {
    struct Candidate {
        void* controller;
        void* expectedPlayer;
        DWORD expectedArg1;
        bool requiresExactVanillaMatch;
        const char* source;
    };

    void* setRunningController = nullptr;
    DWORD setRunningTick = 0;
    void* vanillaController = nullptr;
    void* vanillaSelf = nullptr;
    DWORD vanillaArg1 = 0;
    DWORD vanillaTick = 0;
    AcquireSRWLockShared(&g_emergencyWasdBootstrapLock);
    setRunningController = g_emergencyWasdSetRunningController;
    setRunningTick = g_emergencyWasdSetRunningTick;
    vanillaController = g_emergencyWasdVanillaController;
    vanillaSelf = g_emergencyWasdVanillaSelf;
    vanillaArg1 = g_emergencyWasdVanillaArg1;
    vanillaTick = g_emergencyWasdVanillaEvidenceTick;
    ReleaseSRWLockShared(&g_emergencyWasdBootstrapLock);

    void* currentController = nullptr;
    AcquireSRWLockShared(&g_localMoveCaptureLock);
    currentController = g_playerMoveController;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);

    Candidate candidates[4]{};
    int candidateCount = 0;
    if (vanillaController != nullptr && vanillaTick != 0 &&
        now - vanillaTick <= kEmergencyWasdBootstrapVanillaEvidenceMaxAgeMs) {
        candidates[candidateCount++] = {
            vanillaController,
            vanillaSelf,
            vanillaArg1,
            true,
            "confirmed-vanilla-player-LocalMove"};
    }
    if (setRunningController != nullptr && setRunningTick != 0 &&
        now - setRunningTick <= kEmergencyWasdBootstrapSetRunningMaxAgeMs &&
        setRunningController != vanillaController) {
        candidates[candidateCount++] = {
            setRunningController,
            nullptr,
            0,
            false,
            "orphan-SetRunning-core"};
    }
    if (currentController != nullptr &&
        currentController != vanillaController &&
        currentController != setRunningController) {
        candidates[candidateCount++] = {
            currentController,
            nullptr,
            0,
            false,
            "last-known-controller"};
    }
    if (g_mapLockedController != nullptr &&
        g_mapLockedController != vanillaController &&
        g_mapLockedController != setRunningController &&
        g_mapLockedController != currentController) {
        candidates[candidateCount++] = {
            g_mapLockedController,
            nullptr,
            0,
            false,
            "previous-event-lock-controller"};
    }

    for (int i = 0; i < candidateCount; ++i) {
        const Candidate& candidate = candidates[i];
        void* player = nullptr;
        DWORD link = 0;
        DWORD arg1 = 0;
        if (!ReadDirectPlayerContextFromController(
                candidate.controller,
                &player,
                &link,
                &arg1)) {
            continue;
        }
        if (candidate.requiresExactVanillaMatch &&
            (player != candidate.expectedPlayer || arg1 != candidate.expectedArg1)) {
            continue;
        }
        if (!LooksLikeUsableStoredPlayerSelf(player) ||
            !LooksLikeReadableLocalMoveArg1(arg1) ||
            LooksLikeAdjacentLoadingContext(player, arg1)) {
            continue;
        }
        char dispatchGuardReason[192]{};
        if (!LooksLikeSafeLocalMoveDispatchSelf(
                player,
                dispatchGuardReason,
                sizeof(dispatchGuardReason))) {
            continue;
        }

        const bool manual =
            InterlockedCompareExchange(&g_emergencyWasdBootstrapManualRequested, 0, 0) != 0;
        bool syntheticGenerationCreated = false;
        if (g_mapGeneration == 0) {
            if (!BeginMapLoadGeneration(
                    now,
                    0,
                    0,
                    "synthetic playable generation created by emergency WASD bootstrap")) {
                continue;
            }
            syntheticGenerationCreated = true;
            InterlockedExchange(&g_mapGenerationSynthetic, 1);
            InterlockedExchange(&g_emergencyWasdBootstrapActive, 1);
            InterlockedExchange(
                &g_emergencyWasdBootstrapManualRequested,
                manual ? 1 : 0);
            g_emergencyWasdBootstrapUntilTick = now + static_cast<DWORD>(
                InterlockedCompareExchange(&g_emergencyWasdBootstrapWindowMs, 0, 0));
        }

        AcquireSRWLockExclusive(&g_localMoveCaptureLock);
        g_playerMoveController = candidate.controller;
        ReleaseSRWLockExclusive(&g_localMoveCaptureLock);
        InterlockedExchange(&g_hasPlayerMoveController, 1);
        InterlockedExchange(&g_hasDirectContextDiagnosticSnapshot, 0);

        g_eventCaptureGeneration = g_mapGeneration;
        g_setRunningSeenGeneration = g_mapGeneration;
        g_setRunningSeenTick = now;
        g_eventCaptureStartedTick = 0;
        g_eventCaptureCandidateSelf = nullptr;
        g_eventCaptureCandidateArg1 = 0;
        g_eventCaptureCandidateStableFrames = 0;
        g_lastCapturePumpTick = 0;
        InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::Capturing));
        InterlockedExchange(&g_levelBootstrapActive, 1);
        g_bootstrapWindowUntilTick = now + static_cast<DWORD>(
            InterlockedCompareExchange(&g_emergencyWasdBootstrapWindowMs, 0, 0));
        InterlockedExchange(&g_cameraPostLoadRequest, 1);
        PostCaptureTickToGameWindow();

        char line[704]{};
        wsprintfA(
            line,
            "Emergency WASD bootstrap started event capture: source=%s controller=0x%08X player=0x%08X arg1=0x%08X link=0x%08X mapGeneration=%lu syntheticGeneration=%d manual=%d; the normal timed stable-sample validator remains authoritative.",
            candidate.source,
            reinterpret_cast<DWORD>(candidate.controller),
            reinterpret_cast<DWORD>(player),
            arg1,
            link,
            g_mapGeneration,
            syntheticGenerationCreated ? 1 : 0,
            manual ? 1 : 0);
        LogLine(line);
        return true;
    }
    return false;
}

void PumpEmergencyWasdBootstrap(DWORD now) {
    if (InterlockedCompareExchange(&g_enableEmergencyWasdBootstrap, 0, 0) == 0) {
        return;
    }

    const bool active =
        InterlockedCompareExchange(&g_emergencyWasdBootstrapActive, 1, 1) != 0;
    const bool manual =
        InterlockedCompareExchange(&g_emergencyWasdBootstrapManualRequested, 0, 0) != 0;
    const bool movementRequested =
        InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0) != 0;
    const bool postLoadSeen =
        InterlockedCompareExchange(&g_emergencyWasdBootstrapPostLoadSeen, 0, 0) != 0;
    const bool structurallyLocked =
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0) ==
            static_cast<LONG>(EventCaptureState::Locked) &&
        g_mapGeneration != 0 &&
        g_mapLockedGeneration == g_mapGeneration &&
        g_mapLockedController != nullptr &&
        g_mapLockedSelf != nullptr;

    // Do not turn the emergency path into a permanent controller probe. A normal
    // structural lock is trusted until an actual dispatch failure explicitly arms
    // this path. The direct controller chain is read only during a bounded attempt.
    if (!active && !manual && !postLoadSeen &&
        (!movementRequested || structurallyLocked)) {
        return;
    }
    if ((active || manual || postLoadSeen) &&
        HasUsableCurrentEventLockedOwnership()) {
        ResetEmergencyWasdBootstrapState(true);
        return;
    }
    if (InterlockedCompareExchange(&g_emergencyWasdBootstrapActive, 1, 1) == 0) {
        if (!manual && !movementRequested && !postLoadSeen) {
            return;
        }
        ArmEmergencyWasdBootstrap(
            now,
            manual,
            manual
                ? "manual F10 request"
                : (movementRequested
                    ? "movement input requested without a usable event lock"
                    : "post-load SetRunning signal has no matching SetLevel ownership"));
    }
    if (InterlockedCompareExchange(&g_emergencyWasdBootstrapActive, 1, 1) == 0) {
        return;
    }
    const HWND bootstrapForeground = GetForegroundWindow();
    const bool overlayOwnsForeground =
        bootstrapForeground != nullptr &&
        bootstrapForeground == g_overlayWindow &&
        IsWindow(g_overlayWindow);
    if (!IsGameWindowForegroundForInput() && !overlayOwnsForeground) {
        return;
    }
    if (g_emergencyWasdBootstrapUntilTick != 0 &&
        static_cast<LONG>(now - g_emergencyWasdBootstrapUntilTick) >= 0) {
        const DWORD retryDelay = static_cast<DWORD>(
            InterlockedCompareExchange(&g_emergencyWasdBootstrapRetryDelayMs, 0, 0));
        ResetEmergencyWasdBootstrapState(false);
        InterlockedExchange(&g_emergencyWasdBootstrapPostLoadSeen, 0);
        g_emergencyWasdBootstrapRetryAfterTick = now + retryDelay;
        LogLine("Emergency WASD bootstrap window expired without a stable playable context; a held movement key or F10 may start another bounded attempt.");
        return;
    }

    const LONG captureState = InterlockedCompareExchange(&g_eventCaptureState, 0, 0);
    if ((captureState == static_cast<LONG>(EventCaptureState::Capturing) ||
            captureState == static_cast<LONG>(EventCaptureState::AwaitReadySignal)) &&
        g_eventCaptureGeneration != 0 &&
        g_eventCaptureGeneration == g_mapGeneration) {
        return;
    }
    if (captureState == static_cast<LONG>(EventCaptureState::Failed)) {
        if (g_emergencyWasdBootstrapNextAttemptTick == 0) {
            g_emergencyWasdBootstrapNextAttemptTick = now + static_cast<DWORD>(
                InterlockedCompareExchange(&g_emergencyWasdBootstrapRetryDelayMs, 0, 0));
            return;
        }
        if (static_cast<LONG>(now - g_emergencyWasdBootstrapNextAttemptTick) < 0) {
            return;
        }
    }
    if (g_emergencyWasdBootstrapNextAttemptTick != 0 &&
        static_cast<LONG>(now - g_emergencyWasdBootstrapNextAttemptTick) < 0) {
        return;
    }
    g_emergencyWasdBootstrapNextAttemptTick = now + static_cast<DWORD>(
        InterlockedCompareExchange(&g_capturePumpIntervalMs, 0, 0));
    TryStartEmergencyWasdEventCapture(now);
}

void RequestManualWasdBootstrap(DWORD now) {
    LogLine("Manual emergency WASD bootstrap requested by F10.");
    InterlockedExchange(&g_emergencyWasdBootstrapManualRequested, 1);
    InterlockedExchange(&g_emergencyWasdBootstrapPostLoadSeen, 1);
    g_emergencyWasdBootstrapRetryAfterTick = 0;
    ResetPendingMovementQueue();
    ArmEmergencyWasdBootstrap(now, true, "manual F10 request");
    PumpEmergencyWasdBootstrap(now);
}

void RequestWasdBootstrapAfterPatchReenable(DWORD now) {
    LogLine("Keyboard/mouse patch re-enabled by F6; bounded WASD ownership detection restarted.");
    InterlockedExchange(&g_emergencyWasdBootstrapManualRequested, 1);
    InterlockedExchange(&g_emergencyWasdBootstrapPostLoadSeen, 1);
    g_emergencyWasdBootstrapRetryAfterTick = 0;
    ResetWasdOwnershipRecoveryState(false);
    ResetPendingMovementQueue();
    ArmEmergencyWasdBootstrap(now, true, "F6 patch re-enable requested a fresh WASD ownership bootstrap");
    PumpEmergencyWasdBootstrap(now);
}


bool ContainsAsciiInsensitive(const char* text, const char* token) {
    if (text == nullptr || token == nullptr || *token == '\0') {
        return false;
    }
    for (const char* start = text; *start != '\0'; ++start) {
        const char* left = start;
        const char* right = token;
        while (*left != '\0' && *right != '\0') {
            char a = *left;
            char b = *right;
            if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 'a' + 'A');
            if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 'a' + 'A');
            if (a != b) break;
            ++left;
            ++right;
        }
        if (*right == '\0') return true;
    }
    return false;
}

void CopyUiStringSanitized(char* destination, size_t destinationCount, const char* source) {
    if (destination == nullptr || destinationCount == 0) return;
    destination[0] = '\0';
    if (source == nullptr) return;
    size_t written = 0;
    for (size_t i = 0; source[i] != '\0' && written + 1 < destinationCount; ++i) {
        const unsigned char c = static_cast<unsigned char>(source[i]);
        if (c == '\r' || c == '\n' || c == '\t') {
            destination[written++] = ' ';
        } else if (c >= 0x20 || c >= 0x80) {
            destination[written++] = static_cast<char>(c);
        }
    }
    destination[written] = '\0';
}

bool IsNonEmptyUiText(const char* text) {
    if (text == nullptr) return false;
    int visible = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0; ++p) {
        if (*p > 0x20) {
            ++visible;
            if (visible >= 2) return true;
        }
    }
    return false;
}

bool IsMapTitleWindowName(const char* name) {
    if (name == nullptr || *name == '\0') return false;
    if (g_mapTitleWindowToken[0] != '\0' &&
        ContainsAsciiInsensitive(name, g_mapTitleWindowToken)) {
        return true;
    }
    if (ContainsAsciiInsensitive(name, "AREA NAME") ||
        ContainsAsciiInsensitive(name, "AREANAME") ||
        ContainsAsciiInsensitive(name, "AREA_NAME") ||
        ContainsAsciiInsensitive(name, "MAP NAME") ||
        ContainsAsciiInsensitive(name, "MAPNAME") ||
        ContainsAsciiInsensitive(name, "LEVEL NAME") ||
        ContainsAsciiInsensitive(name, "LEVELNAME") ||
        ContainsAsciiInsensitive(name, "ZONE NAME") ||
        ContainsAsciiInsensitive(name, "ZONENAME") ||
        ContainsAsciiInsensitive(name, "LOCATION NAME") ||
        ContainsAsciiInsensitive(name, "LOCATIONNAME") ||
        ContainsAsciiInsensitive(name, "REGION NAME") ||
        ContainsAsciiInsensitive(name, "REGIONNAME")) {
        return true;
    }
    const bool areaLike = ContainsAsciiInsensitive(name, "AREA") ||
        ContainsAsciiInsensitive(name, "LEVEL") ||
        ContainsAsciiInsensitive(name, "ZONE") ||
        ContainsAsciiInsensitive(name, "REGION") ||
        ContainsAsciiInsensitive(name, "LOCATION");
    const bool titleLike = ContainsAsciiInsensitive(name, "TITLE") ||
        ContainsAsciiInsensitive(name, "TEXT") ||
        ContainsAsciiInsensitive(name, "LABEL") ||
        ContainsAsciiInsensitive(name, "NAME");
    return areaLike && titleLike;
}

bool IsLoadingWindowName(const char* name) {
    return name != nullptr &&
        (ContainsAsciiInsensitive(name, "LOADSCREEN") ||
            ContainsAsciiInsensitive(name, "LOADING") ||
            ContainsAsciiInsensitive(name, "LOAD SCREEN"));
}

bool IsGameplayHudWindowName(const char* name) {
    if (name == nullptr) return false;
    const bool hud = ContainsAsciiInsensitive(name, "HUD");
    const bool gameplay = ContainsAsciiInsensitive(name, "HEALTH") ||
        ContainsAsciiInsensitive(name, "MANA") ||
        ContainsAsciiInsensitive(name, "PLAYER") ||
        ContainsAsciiInsensitive(name, "INGAME") ||
        ContainsAsciiInsensitive(name, "GAMEUI") ||
        ContainsAsciiInsensitive(name, "MAINHUD");
    return hud && gameplay;
}

const char* TryGetCeguiWindowName(void* window) {
    if (window == nullptr || g_ceguiGetWindowName == nullptr || g_ceguiStringCStr == nullptr) return nullptr;
    const char* result = nullptr;
    __try {
        const void* value = g_ceguiGetWindowName(window);
        result = value != nullptr ? g_ceguiStringCStr(value) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = nullptr;
    }
    return result;
}

const char* TryGetCeguiStringText(const void* value) {
    if (value == nullptr || g_ceguiStringCStr == nullptr) return nullptr;
    const char* result = nullptr;
    __try {
        result = g_ceguiStringCStr(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = nullptr;
    }
    return result;
}

void MarkUiReadyForCurrentMap(const char* reason, const char* windowName, const char* text) {
    if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0 || g_mapGeneration == 0) return;
    if (g_uiReadyGeneration == g_mapGeneration) return;

    g_uiReadyGeneration = g_mapGeneration;
    g_uiReadyTick = GetTickCount();
    CopyUiStringSanitized(g_lastUiReadyWindow, sizeof(g_lastUiReadyWindow), windowName);
    CopyUiStringSanitized(g_lastUiReadyText, sizeof(g_lastUiReadyText), text);

    char line[640]{};
    wsprintfA(
        line,
        "UI ready signal accepted: generation=%lu reason=%s window=\"%s\" text=\"%s\".",
        g_mapGeneration,
        reason != nullptr ? reason : "CEGUI lifecycle",
        g_lastUiReadyWindow,
        g_lastUiReadyText);
    LogLine(line);

    const bool setRunningReady =
        InterlockedCompareExchange(&g_requireSetRunningBeforeCapture, 0, 0) == 0 ||
        g_setRunningSeenGeneration == g_mapGeneration;
    const bool camRequired = InterlockedCompareExchange(&g_requirePlayerCamBeforeCapture, 0, 0) != 0;
    if (setRunningReady && (!camRequired || g_mapRenderedPlayerCamGeneration == g_mapGeneration)) {
        InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::Capturing));
        PostCaptureTickToGameWindow();
    } else {
        InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::AwaitReadySignal));
    }
}

void LogUiLifecycleEvent(const char* operation, const char* windowName, const char* value) {
    if (InterlockedCompareExchange(&g_logUiLifecycle, 0, 0) == 0 || g_mapGeneration == 0) return;
    const LONG state = InterlockedCompareExchange(&g_eventCaptureState, 0, 0);
    if (state == static_cast<LONG>(EventCaptureState::Locked) ||
        state == static_cast<LONG>(EventCaptureState::Disabled)) return;
    if (g_uiLifecycleLogLines >= 120) return;
    const DWORD now = GetTickCount();
    const DWORD minimumInterval =
        InterlockedCompareExchange(&g_logAllUiTextDuringLoad, 0, 0) != 0 ? 25 : 150;
    if (now - g_lastUiLifecycleLogTick < minimumInterval) return;
    g_lastUiLifecycleLogTick = now;
    ++g_uiLifecycleLogLines;
    char cleanName[160]{};
    char cleanValue[224]{};
    CopyUiStringSanitized(cleanName, sizeof(cleanName), windowName);
    CopyUiStringSanitized(cleanValue, sizeof(cleanValue), value);
    char line[640]{};
    wsprintfA(line, "UI lifecycle: generation=%lu op=%s window=\"%s\" value=\"%s\".",
        g_mapGeneration, operation != nullptr ? operation : "?", cleanName, cleanValue);
    LogLine(line);
}

void __fastcall CeguiWindowSetTextHook(void* window, void*, const void* textValue) {
    CeguiWindowSetTextFn original = g_ceguiSetTextOriginal;
    // Let CEGUI complete the update first. The capture message is then queued on
    // the same window thread, so the controller is sampled only after the UI
    // state that announced the new map has actually been committed.
    if (original != nullptr) original(window, textValue);
    if (InterlockedCompareExchange(&g_enableUiLifecycleProbe, 0, 0) == 0) return;

    const char* windowName = TryGetCeguiWindowName(window);
    const char* text = TryGetCeguiStringText(textValue);
    LogUiLifecycleEvent("setText", windowName, text);
    if (InterlockedCompareExchange(&g_captureOnMapTitleText, 0, 0) != 0 &&
        IsMapTitleWindowName(windowName) && IsNonEmptyUiText(text)) {
        MarkUiReadyForCurrentMap("map-title text updated", windowName, text);
    }
}

void __fastcall CeguiWindowSetVisibleHook(void* window, void*, bool visible) {
    CeguiWindowSetVisibleFn original = g_ceguiSetVisibleOriginal;
    // As with setText, observe the lifecycle only after CEGUI has applied it.
    if (original != nullptr) original(window, visible);
    if (InterlockedCompareExchange(&g_enableUiLifecycleProbe, 0, 0) == 0) return;

    const char* windowName = TryGetCeguiWindowName(window);
    LogUiLifecycleEvent("setVisible", windowName, visible ? "1" : "0");
    if (!visible && InterlockedCompareExchange(&g_captureOnLoadingHidden, 0, 0) != 0 &&
        IsLoadingWindowName(windowName)) {
        MarkUiReadyForCurrentMap("loading window hidden", windowName, "hidden");
    }
    if (visible && InterlockedCompareExchange(&g_captureOnHudVisible, 0, 0) != 0 &&
        IsGameplayHudWindowName(windowName)) {
        MarkUiReadyForCurrentMap("gameplay HUD visible", windowName, "visible");
    }
}

bool WriteImportedFunctionPointer(void** slot, void* replacement) {
    if (slot == nullptr || replacement == nullptr) return false;
    DWORD oldProtect = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtect)) return false;
    *slot = replacement;
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), oldProtect, &ignored);
    return *slot == replacement;
}

void InstallCeguiLifecycleHooks() {
    if (InterlockedCompareExchange(&g_enableUiLifecycleProbe, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_ceguiHooksInstalled, 1, 1) != 0) return;
    HMODULE cegui = GetModuleHandleW(L"CEGUIBase.dll");
    if (cegui == nullptr) return;
    g_ceguiGetWindowName = reinterpret_cast<CeguiWindowGetNameFn>(
        GetProcAddress(cegui, "?getName@Window@CEGUI@@QBEABVString@2@XZ"));
    g_ceguiStringCStr = reinterpret_cast<CeguiStringCStrFn>(
        GetProcAddress(cegui, "?c_str@String@CEGUI@@QBEPBDXZ"));
    if (g_ceguiGetWindowName == nullptr || g_ceguiStringCStr == nullptr) {
        LogLine("CEGUI lifecycle probe unavailable: getName/c_str exports missing.");
        return;
    }

    void** textSlot = reinterpret_cast<void**>(StaticToRuntime(kCeguiSetTextIatStatic));
    void** visibleSlot = reinterpret_cast<void**>(StaticToRuntime(kCeguiSetVisibleIatStatic));
    if (textSlot == nullptr || visibleSlot == nullptr) {
        LogLine("CEGUI lifecycle probe install failed: executable IAT slots unavailable.");
        return;
    }

    void* currentText = *textSlot;
    void* currentVisible = *visibleSlot;
    if (currentText == reinterpret_cast<void*>(&CeguiWindowSetTextHook) &&
        currentVisible == reinterpret_cast<void*>(&CeguiWindowSetVisibleHook) &&
        g_ceguiSetTextOriginal != nullptr && g_ceguiSetVisibleOriginal != nullptr) {
        InterlockedExchange(&g_ceguiHooksInstalled, 1);
        return;
    }
    if (currentText == reinterpret_cast<void*>(&CeguiWindowSetTextHook) ||
        currentVisible == reinterpret_cast<void*>(&CeguiWindowSetVisibleHook)) {
        LogLine("CEGUI lifecycle probe install refused: partial pre-existing hook state.");
        return;
    }

    g_ceguiSetTextOriginal = reinterpret_cast<CeguiWindowSetTextFn>(currentText);
    g_ceguiSetVisibleOriginal = reinterpret_cast<CeguiWindowSetVisibleFn>(currentVisible);
    if (!WriteImportedFunctionPointer(textSlot, reinterpret_cast<void*>(&CeguiWindowSetTextHook))) {
        g_ceguiSetTextOriginal = nullptr;
        g_ceguiSetVisibleOriginal = nullptr;
        LogLine("CEGUI lifecycle probe install failed: Window::setText IAT patch refused.");
        return;
    }
    if (!WriteImportedFunctionPointer(visibleSlot, reinterpret_cast<void*>(&CeguiWindowSetVisibleHook))) {
        WriteImportedFunctionPointer(textSlot, currentText);
        g_ceguiSetTextOriginal = nullptr;
        g_ceguiSetVisibleOriginal = nullptr;
        LogLine("CEGUI lifecycle probe install failed: Window::setVisible IAT patch refused; setText rolled back.");
        return;
    }

    InterlockedExchange(&g_ceguiHooksInstalled, 1);
    LogLine("CEGUI lifecycle probe installed: Window::setText and Window::setVisible.");
}

void RestoreCeguiLifecycleHooks() {
    if (InterlockedExchange(&g_ceguiHooksInstalled, 0) == 0) return;
    void** textSlot = reinterpret_cast<void**>(StaticToRuntime(kCeguiSetTextIatStatic));
    void** visibleSlot = reinterpret_cast<void**>(StaticToRuntime(kCeguiSetVisibleIatStatic));
    DWORD oldProtect = 0;
    if (textSlot != nullptr && g_ceguiSetTextOriginal != nullptr &&
        VirtualProtect(textSlot, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        if (*textSlot == reinterpret_cast<void*>(&CeguiWindowSetTextHook)) *textSlot = reinterpret_cast<void*>(g_ceguiSetTextOriginal);
        DWORD ignored = 0; VirtualProtect(textSlot, sizeof(void*), oldProtect, &ignored);
    }
    if (visibleSlot != nullptr && g_ceguiSetVisibleOriginal != nullptr &&
        VirtualProtect(visibleSlot, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        if (*visibleSlot == reinterpret_cast<void*>(&CeguiWindowSetVisibleHook)) *visibleSlot = reinterpret_cast<void*>(g_ceguiSetVisibleOriginal);
        DWORD ignored = 0; VirtualProtect(visibleSlot, sizeof(void*), oldProtect, &ignored);
    }
    g_ceguiSetTextOriginal = nullptr;
    g_ceguiSetVisibleOriginal = nullptr;
    LogLine("CEGUI lifecycle probe restored.");
}

void CaptureEventDrivenContextOnGameThread() {
    if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0) {
        return;
    }

    const LONG state = InterlockedCompareExchange(&g_eventCaptureState, 0, 0);
    if (state != static_cast<LONG>(EventCaptureState::Capturing) &&
        state != static_cast<LONG>(EventCaptureState::AwaitReadySignal)) {
        return;
    }
    if (g_eventCaptureGeneration == 0 ||
        g_eventCaptureGeneration != g_mapGeneration) {
        return;
    }
    if (InterlockedCompareExchange(&g_requireSetRunningBeforeCapture, 0, 0) != 0 &&
        g_setRunningSeenGeneration != g_mapGeneration) {
        return;
    }
    const DWORD now = GetTickCount();
    const DWORD minimumAge = static_cast<DWORD>(
        InterlockedCompareExchange(&g_minimumCaptureAgeAfterSetRunningMs, 0, 0));
    if (g_setRunningSeenTick != 0 && now - g_setRunningSeenTick < minimumAge) {
        return;
    }
    const bool uiRequired = InterlockedCompareExchange(&g_requireUiReadyBeforeCapture, 0, 0) != 0;
    const bool uiReady = g_uiReadyGeneration == g_mapGeneration;
    const bool camReady = g_mapRenderedPlayerCamGeneration == g_mapGeneration;
    const bool camRequired = InterlockedCompareExchange(&g_requirePlayerCamBeforeCapture, 0, 0) != 0;
    bool fallbackReady = false;
    if (uiRequired && !uiReady &&
        InterlockedCompareExchange(&g_allowPlayerCamFallback, 0, 0) != 0 &&
        camReady && g_setRunningSeenTick != 0) {
        const DWORD delay = static_cast<DWORD>(InterlockedCompareExchange(&g_uiReadyFallbackDelayMs, 0, 0));
        fallbackReady = now - g_setRunningSeenTick >= delay;
        if (fallbackReady && g_uiReadyGeneration != g_mapGeneration) {
            MarkUiReadyForCurrentMap("delayed PlayerCam fallback", "PlayerCam", "fallback");
        }
    }
    if (uiRequired && !uiReady && !fallbackReady) return;
    if (camRequired && !camReady) return;

    if (g_eventCaptureStartedTick == 0) {
        g_eventCaptureStartedTick = now;
        InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::Capturing));
    }
    const DWORD timeoutMs = static_cast<DWORD>(
        InterlockedCompareExchange(&g_captureTimeoutMs, 0, 0));
    if (now - g_eventCaptureStartedTick > timeoutMs) {
        InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::Failed));
        LogLine("Event capture timed out. Normal capture waits for the next UNetSetLevelMsg; the bounded emergency WASD bootstrap may retry while movement is requested or after F10.");
        return;
    }

    void* controller = nullptr;
    AcquireSRWLockShared(&g_localMoveCaptureLock);
    controller = g_playerMoveController;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);

    void* player = nullptr;
    DWORD link = 0;
    DWORD arg1 = 0;
    ReadDirectPlayerContextFromController(controller, &player, &link, &arg1);
    const bool strictCaptureMarkerRequired =
        InterlockedCompareExchange(&g_requireStrictPlayerMarkerAtCapture, 0, 0) != 0;
    const bool playerUsable = LooksLikeUiReadyPlayerSelf(player);
    const bool arg1Readable = LooksLikeReadableLocalMoveArg1(arg1);
    const bool adjacentLoadingContext = LooksLikeAdjacentLoadingContext(player, arg1);
    const bool strictMarkerReady =
        !strictCaptureMarkerRequired || LooksLikePlayerLocalMoveSelf(player);
    const bool valid =
        playerUsable && arg1Readable && !adjacentLoadingContext && strictMarkerReady;

    if (!valid) {
        g_eventCaptureCandidateSelf = nullptr;
        g_eventCaptureCandidateArg1 = 0;
        g_eventCaptureCandidateStableFrames = 0;
        if (InterlockedCompareExchange(&g_logCaptureLifecycle, 0, 0) != 0 &&
            now - g_lastEventCaptureLogTick >= 500) {
            g_lastEventCaptureLogTick = now;
            char line[384]{};
            wsprintfA(
                line,
                "Event capture waiting: generation=%lu controller=0x%08X player=0x%08X link=0x%08X arg1=0x%08X usable=%d arg1Readable=%d adjacentLoading=%d strict=%d windowThread=%lu.",
                g_mapGeneration,
                reinterpret_cast<DWORD>(controller),
                reinterpret_cast<DWORD>(player),
                link,
                arg1,
                playerUsable ? 1 : 0,
                arg1Readable ? 1 : 0,
                adjacentLoadingContext ? 1 : 0,
                strictMarkerReady ? 1 : 0,
                GetCurrentThreadId());
            LogLine(line);
        }
        // Do not repost from the game-window callback. The worker-side bounded
        // pump will request the next sample after CapturePollIntervalMs.
        return;
    }

    if (g_eventCaptureCandidateSelf == player &&
        g_eventCaptureCandidateArg1 == arg1) {
        ++g_eventCaptureCandidateStableFrames;
    } else {
        g_eventCaptureCandidateSelf = player;
        g_eventCaptureCandidateArg1 = arg1;
        g_eventCaptureCandidateStableFrames = 1;
    }

    const LONG required = InterlockedCompareExchange(&g_captureStableFrames, 0, 0);
    if (g_eventCaptureCandidateStableFrames < required) {
        // Samples must be separated by the configured poll interval; immediate
        // reposting caused a tight main-thread message loop and visible freezes.
        return;
    }

    SetPlayerLocalMoveContext(player, arg1, "Event-driven post-load capture locked");
    g_mapLockedGeneration = g_mapGeneration;
    g_mapLockedController = controller;
    g_mapLockedScene = g_mapIdentityScene;
    g_mapLockedActiveLevel = g_mapIdentityActiveLevel;
    g_mapLockedSelf = player;
    g_mapLockedArg1 = arg1;
    g_dynamicArg1Candidate = 0;
    g_dynamicArg1CandidateFirstTick = 0;
    g_dynamicArg1CandidateStableReads = 0;
    g_transitionLocalMoveBlockUntilTick = now;
    InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::Locked));
    InterlockedExchange(&g_levelBootstrapActive, 0);
    InterlockedExchange(&g_directReacquireRequested, 0);

    char line[384]{};
    wsprintfA(
        line,
        "Event context locked once for map: generation=%lu controller=0x%08X self=0x%08X initialArg1=0x%08X stableFrames=%ld captureThread=%lu setRunningThread=%lu PlayerCamThread=%lu UI=\"%s\".",
        g_mapGeneration,
        reinterpret_cast<DWORD>(controller),
        reinterpret_cast<DWORD>(player),
        arg1,
        required,
        GetCurrentThreadId(),
        g_setRunningThreadId,
        g_playerCamThreadId,
        g_lastUiReadyText);
    LogLine(line);
    const bool emergencyBootstrapCompleted =
        InterlockedCompareExchange(&g_emergencyWasdBootstrapActive, 1, 1) != 0 ||
        InterlockedCompareExchange(&g_emergencyWasdBootstrapManualRequested, 1, 1) != 0;
    if (emergencyBootstrapCompleted) {
        char emergencyLine[512]{};
        wsprintfA(
            emergencyLine,
            "Emergency WASD bootstrap completed: generation=%lu controller=0x%08X self=0x%08X arg1=0x%08X; keyboard movement ownership is now locked without requiring a portal or an older save.",
            g_mapGeneration,
            reinterpret_cast<DWORD>(controller),
            reinterpret_cast<DWORD>(player),
            arg1);
        LogLine(emergencyLine);
        const int moveMask = InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0);
        if (moveMask != 0) {
            QueueGameThreadMovement(
                PendingMovementKind::Move,
                moveMask,
                InterlockedCompareExchange(&g_keyboardMoveLeadStepX100, 0, 0));
        }
    }
    ResetEmergencyWasdBootstrapState(true);
    BeginCameraSyncAfterEventContextLock(now);
}

bool StoredContextMatchesCurrentController(void* self, DWORD arg1) {
    if (self == nullptr || arg1 == 0) {
        return false;
    }

    void* controller = nullptr;
    AcquireSRWLockShared(&g_localMoveCaptureLock);
    controller = g_playerMoveController;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);
    if (controller == nullptr) {
        return false;
    }

    void* directPlayer = nullptr;
    DWORD directLink = 0;
    DWORD directArg1 = 0;
    if (!ReadDirectPlayerContextFromController(controller, &directPlayer, &directLink, &directArg1)) {
        return false;
    }

    return directPlayer == self &&
        directArg1 == arg1 &&
        LooksLikeUsableStoredPlayerSelf(directPlayer) &&
        LooksLikeReadableLocalMoveArg1(directArg1);
}

bool StoredContextStrictlyMatchesCurrentController(void* self, DWORD arg1) {
    if (self == nullptr || arg1 == 0) {
        return false;
    }

    void* controller = nullptr;
    AcquireSRWLockShared(&g_localMoveCaptureLock);
    controller = g_playerMoveController;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);
    if (controller == nullptr) {
        return false;
    }

    void* directPlayer = nullptr;
    DWORD directLink = 0;
    DWORD directArg1 = 0;
    if (!ReadDirectPlayerContextFromController(controller, &directPlayer, &directLink, &directArg1)) {
        return false;
    }

    const bool matches =
        directPlayer == self &&
        directArg1 == arg1 &&
        LooksLikePlayerLocalMoveSelf(directPlayer) &&
        LooksLikeReadableLocalMoveArg1(directArg1);
    if (matches) {
        g_lastStrictDirectContextMatchTick = GetTickCount();
    }
    return matches;
}

bool StoredContextMatchesLastKnownPlayer(void* self, DWORD arg1) {
    if (self == nullptr || arg1 == 0) {
        return false;
    }

    void* lastKnownSelf = nullptr;
    DWORD lastKnownArg1 = 0;
    AcquireSRWLockShared(&g_localMoveCaptureLock);
    lastKnownSelf = g_lastKnownPlayerLocalMoveSelf;
    lastKnownArg1 = g_lastKnownPlayerLocalMoveArg1;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);

    return InterlockedCompareExchange(&g_hasLastKnownPlayerLocalMove, 1, 1) != 0 &&
        self == lastKnownSelf &&
        arg1 == lastKnownArg1;
}

bool CanUseDirectCarryDuringStrictDip(DWORD now, void* self, DWORD arg1) {
    if (IsTransitionLocalMoveBlocked(now)) {
        return false;
    }
    if (!LooksLikeUsableStoredPlayerSelf(self) ||
        !LooksLikeReadableLocalMoveArg1(arg1)) {
        return false;
    }
    if (!StoredContextMatchesCurrentController(self, arg1)) {
        return false;
    }
    if (!StoredContextMatchesLastKnownPlayer(self, arg1) &&
        !IsCurrentMapContextLockedFor(self, arg1)) {
        return false;
    }

    InterlockedIncrement(&g_skillCarryEntries);
    if constexpr (kVerboseHookDiagnostics) {
        if (now - g_lastSkillCarryLogTick >= 500) {
            g_lastSkillCarryLogTick = now;
            char line[256]{};
            wsprintfA(
                line,
                "Direct carry allowed during strict player-valid dip: self=0x%08X arg1=0x%08X.",
                reinterpret_cast<DWORD>(self),
                arg1);
            LogLine(line);
        }
    }
    return true;
}

bool CurrentMapIdentityMatches(DWORD scene, DWORD activeLevel) {
    if (scene == 0 || activeLevel == 0) {
        return g_mapGeneration != 0;
    }

    return scene == g_lastDiagnosticScene &&
        activeLevel == g_lastDiagnosticActiveLevel &&
        scene == g_mapIdentityScene &&
        activeLevel == g_mapIdentityActiveLevel;
}

bool IsCurrentMapRenderReady() {
    if (!CurrentMapIdentityMatches(g_mapIdentityScene, g_mapIdentityActiveLevel)) {
        return false;
    }

    if (InterlockedCompareExchange(&g_enableMapRenderReadyProbe, 0, 0) == 0 &&
        InterlockedCompareExchange(&g_enableCameraAutoCalibration, 0, 0) == 0) {
        return g_mapGeneration != 0;
    }

    if (g_mapRenderedPlayerCamGeneration == g_mapGeneration) {
        return true;
    }

    return g_cameraBasisAvailable &&
        g_cameraLatchedScene != 0;
}

bool IsCurrentMapContextLockedFor(void* self, DWORD arg1) {
    if (g_mapLockedGeneration != g_mapGeneration ||
        g_mapLockedScene != g_mapIdentityScene ||
        g_mapLockedActiveLevel != g_mapIdentityActiveLevel ||
        g_mapLockedSelf != self ||
        !IsCurrentMapRenderReady()) {
        return false;
    }
    if (InterlockedCompareExchange(&g_lockPlayerIdentityOnly, 0, 0) != 0) {
        (void)arg1;
        return g_mapLockedController != nullptr;
    }
    return g_mapLockedArg1 == arg1;
}

bool TryLockCurrentMapContext(const char* reason, DWORD now) {
    // Event mode owns the complete capture/lock lifecycle. Legacy callers may
    // still invoke this helper, but they are not allowed to create or refresh a
    // map lock. Only CaptureEventDrivenContextOnGameThread performs that action.
    if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0) {
        (void)reason;
        (void)now;
        return InterlockedCompareExchange(&g_eventCaptureState, 0, 0) ==
                static_cast<LONG>(EventCaptureState::Locked) &&
            g_mapLockedGeneration == g_mapGeneration &&
            g_mapLockedSelf != nullptr &&
            g_mapLockedArg1 != 0;
    }

    if (!IsCurrentMapRenderReady()) {
        return false;
    }

    if (InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) == 0) {
        return false;
    }

    void* self = nullptr;
    DWORD arg1 = 0;
    AcquireSRWLockShared(&g_localMoveCaptureLock);
    self = g_playerLocalMoveSelf;
    arg1 = g_playerLocalMoveArg1;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);

    if (!LooksLikeUsableStoredPlayerSelf(self) ||
        !LooksLikeReadableLocalMoveArg1(arg1)) {
        return false;
    }

    if (IsCurrentMapContextLockedFor(self, arg1)) {
        return true;
    }

    g_mapLockedGeneration = g_mapGeneration;
    g_mapLockedScene = g_mapIdentityScene;
    g_mapLockedActiveLevel = g_mapIdentityActiveLevel;
    g_mapLockedSelf = self;
    g_mapLockedArg1 = arg1;

    if (now - g_lastMapLockLogTick >= 500) {
        g_lastMapLockLogTick = now;
        char line[384]{};
        wsprintfA(
            line,
            "Map context locked: generation=%lu scene=0x%08X activeLevel=0x%08X self=0x%08X arg1=0x%08X reason=%s.",
            g_mapLockedGeneration,
            g_mapLockedScene,
            g_mapLockedActiveLevel,
            reinterpret_cast<DWORD>(g_mapLockedSelf),
            g_mapLockedArg1,
            reason != nullptr ? reason : "stable rendered map and player LocalMove context");
        LogLine(line);
    }
    return true;
}

void MarkMapRenderReadyFromPlayerCamLatch(DWORD now, const char* reason) {
    if (g_mapGeneration == 0 ||
        g_mapRenderedPlayerCamGeneration == g_mapGeneration) {
        return;
    }

    g_mapRenderedPlayerCamGeneration = g_mapGeneration;
    g_mapRenderedPlayerCamScene = g_mapIdentityScene;
    g_mapRenderedPlayerCamActiveLevel = g_mapIdentityActiveLevel;

    char line[288]{};
    wsprintfA(
        line,
        "Map render ready: PlayerCam camera basis latched for generation=%lu scene=0x%08X activeLevel=0x%08X reason=%s.",
        g_mapGeneration,
        g_mapIdentityScene,
        g_mapIdentityActiveLevel,
        reason != nullptr ? reason : "PlayerCam basis latched");
    LogLine(line);
    TryLockCurrentMapContext(reason, now);
}

void ResetWasdOwnershipRecoveryState(bool clearVanillaEvidence) {
    if (!clearVanillaEvidence &&
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryActive, 1, 1) == 0 &&
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryMessagePosted, 1, 1) == 0 &&
        g_wasdOwnershipRecoveryGeneration == 0) {
        return;
    }
    InterlockedExchange(&g_wasdOwnershipRecoveryActive, 0);
    InterlockedExchange(&g_wasdOwnershipRecoveryMessagePosted, 0);
    g_wasdOwnershipRecoveryGeneration = 0;
    g_wasdOwnershipRecoveryUntilTick = 0;
    g_wasdOwnershipRecoveryNextPollTick = 0;
    g_wasdOwnershipRecoveryRetryAfterTick = 0;

    AcquireSRWLockExclusive(&g_wasdOwnershipRecoveryLock);
    g_wasdOwnershipRecoveryCandidateController = nullptr;
    g_wasdOwnershipRecoveryCandidateSelf = nullptr;
    g_wasdOwnershipRecoveryCandidateArg1 = 0;
    g_wasdOwnershipRecoveryCandidateStableReads = 0;
    if (clearVanillaEvidence) {
        g_wasdOwnershipVanillaController = nullptr;
        g_wasdOwnershipVanillaSelf = nullptr;
        g_wasdOwnershipVanillaArg1 = 0;
        g_wasdOwnershipVanillaGeneration = 0;
        g_wasdOwnershipVanillaEvidenceTick = 0;
        g_wasdOwnershipVanillaEvidenceReads = 0;
    }
    ReleaseSRWLockExclusive(&g_wasdOwnershipRecoveryLock);
}

void ClearWasdOwnershipRecoveryCandidate() {
    AcquireSRWLockExclusive(&g_wasdOwnershipRecoveryLock);
    g_wasdOwnershipRecoveryCandidateController = nullptr;
    g_wasdOwnershipRecoveryCandidateSelf = nullptr;
    g_wasdOwnershipRecoveryCandidateArg1 = 0;
    g_wasdOwnershipRecoveryCandidateStableReads = 0;
    ReleaseSRWLockExclusive(&g_wasdOwnershipRecoveryLock);
}

void ArmWasdOwnershipRecovery(DWORD now, const char* reason) {
    if (InterlockedCompareExchange(&g_enableWasdOwnershipRecovery, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0) !=
            static_cast<LONG>(EventCaptureState::Locked) ||
        g_mapGeneration == 0 ||
        g_mapLockedGeneration != g_mapGeneration ||
        !IsCurrentMapRenderReady()) {
        return;
    }
    if (g_wasdOwnershipRecoveryRetryAfterTick != 0 &&
        static_cast<LONG>(now - g_wasdOwnershipRecoveryRetryAfterTick) < 0) {
        return;
    }
    if (InterlockedCompareExchange(&g_wasdOwnershipRecoveryActive, 1, 1) != 0 &&
        g_wasdOwnershipRecoveryGeneration == g_mapGeneration) {
        return;
    }

    ClearWasdOwnershipRecoveryCandidate();
    g_wasdOwnershipRecoveryGeneration = g_mapGeneration;
    g_wasdOwnershipRecoveryUntilTick = now + static_cast<DWORD>(
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryWindowMs, 0, 0));
    g_wasdOwnershipRecoveryNextPollTick = now;
    g_wasdOwnershipRecoveryRetryAfterTick = 0;
    InterlockedExchange(&g_wasdOwnershipRecoveryActive, 1);

    char line[512]{};
    wsprintfA(
        line,
        "WASD ownership recovery armed: generation=%lu lockedController=0x%08X lockedSelf=0x%08X lockedArg1=0x%08X window=%ld ms poll=%ld ms stableReads=%ld reason=%s.",
        g_mapGeneration,
        reinterpret_cast<DWORD>(g_mapLockedController),
        reinterpret_cast<DWORD>(g_mapLockedSelf),
        g_mapLockedArg1,
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryWindowMs, 0, 0),
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryPollMs, 0, 0),
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryStableReadsRequired, 0, 0),
        reason != nullptr ? reason : "movement intent has no usable event-locked player ownership");
    LogLine(line);
}

bool ReadRecentVanillaWasdOwnershipEvidence(
    DWORD now,
    void** controllerOut,
    void** selfOut,
    DWORD* arg1Out) {
    if (controllerOut == nullptr || selfOut == nullptr || arg1Out == nullptr ||
        InterlockedCompareExchange(&g_allowVanillaPlayerMoveOwnershipEvidence, 0, 0) == 0) {
        return false;
    }

    void* controller = nullptr;
    void* self = nullptr;
    DWORD arg1 = 0;
    DWORD generation = 0;
    DWORD tick = 0;
    AcquireSRWLockShared(&g_wasdOwnershipRecoveryLock);
    controller = g_wasdOwnershipVanillaController;
    self = g_wasdOwnershipVanillaSelf;
    arg1 = g_wasdOwnershipVanillaArg1;
    generation = g_wasdOwnershipVanillaGeneration;
    tick = g_wasdOwnershipVanillaEvidenceTick;
    ReleaseSRWLockShared(&g_wasdOwnershipRecoveryLock);

    if (controller == nullptr || self == nullptr || arg1 == 0 ||
        generation != g_mapGeneration || tick == 0 ||
        now - tick > kWasdOwnershipVanillaEvidenceMaxAgeMs) {
        return false;
    }

    *controllerOut = controller;
    *selfOut = self;
    *arg1Out = arg1;
    return true;
}

void ObserveConfirmedVanillaPlayerMoveForWasdRecovery(
    DWORD now,
    void* sourceController,
    void* self,
    DWORD arg1,
    void* directPlayer,
    DWORD directArg1) {
    if (InterlockedCompareExchange(&g_enableWasdOwnershipRecovery, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_allowVanillaPlayerMoveOwnershipEvidence, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0) !=
            static_cast<LONG>(EventCaptureState::Locked) ||
        g_mapGeneration == 0 || g_mapLockedGeneration != g_mapGeneration ||
        sourceController == nullptr || self == nullptr || arg1 == 0 ||
        directPlayer != self || directArg1 != arg1 ||
        !LooksLikeUsableStoredPlayerSelf(self) ||
        !LooksLikeReadableLocalMoveArg1(arg1) ||
        LooksLikeAdjacentLoadingContext(self, arg1)) {
        return;
    }

    AcquireSRWLockExclusive(&g_wasdOwnershipRecoveryLock);
    if (g_wasdOwnershipVanillaController == sourceController &&
        g_wasdOwnershipVanillaSelf == self &&
        g_wasdOwnershipVanillaArg1 == arg1 &&
        g_wasdOwnershipVanillaGeneration == g_mapGeneration) {
        ++g_wasdOwnershipVanillaEvidenceReads;
    } else {
        g_wasdOwnershipVanillaController = sourceController;
        g_wasdOwnershipVanillaSelf = self;
        g_wasdOwnershipVanillaArg1 = arg1;
        g_wasdOwnershipVanillaGeneration = g_mapGeneration;
        g_wasdOwnershipVanillaEvidenceReads = 1;
    }
    g_wasdOwnershipVanillaEvidenceTick = now;
    const LONG evidenceReads = g_wasdOwnershipVanillaEvidenceReads;
    ReleaseSRWLockExclusive(&g_wasdOwnershipRecoveryLock);

    const bool differsFromLock =
        sourceController != g_mapLockedController ||
        self != g_mapLockedSelf ||
        arg1 != g_mapLockedArg1;
    if (differsFromLock && now - g_lastWasdOwnershipRecoveryLogTick >= 500) {
        g_lastWasdOwnershipRecoveryLogTick = now;
        char line[448]{};
        wsprintfA(
            line,
            "Confirmed vanilla player LocalMove recorded as WASD recovery evidence: controller=0x%08X self=0x%08X arg1=0x%08X reads=%ld currentLockedSelf=0x%08X currentLockedArg1=0x%08X.",
            reinterpret_cast<DWORD>(sourceController),
            reinterpret_cast<DWORD>(self),
            arg1,
            evidenceReads,
            reinterpret_cast<DWORD>(g_mapLockedSelf),
            g_mapLockedArg1);
        LogLine(line);
    }

    if (differsFromLock && InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0) != 0) {
        ArmWasdOwnershipRecovery(now, "confirmed vanilla player LocalMove differs from the event-locked ownership");
        PostWasdOwnershipRecoveryTickToGameWindow();
    }
}

void RearmCameraAfterWasdOwnershipRecovery(
    DWORD now,
    void* previousSelf,
    DWORD previousArg1,
    void* currentSelf,
    DWORD currentArg1) {
    const LONG observations = InterlockedCompareExchange(
        &g_cameraEngineConcordantObservations, 0, 0);
    const LONG stage = InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0);
    if (stage >= 1 && stage <= 4 &&
        g_postReleaseCameraVerificationGeneration == g_mapGeneration) {
        CancelAutomaticPostReleaseCameraVerificationForInput(
            "the authoritative WASD player ownership changed while camera confirmation was pending");
    } else {
        ResetPostReleaseCameraVerificationCycle();
    }

    InterlockedExchange(&g_cameraEngineConcordantObservations, 0);
    InterlockedExchange(&g_cameraBasisProvisional, 1);
    InterlockedExchange(&g_runtimeCameraContextEstablished, 1);
    InterlockedExchange(&g_postFirstMoveCameraWatchActive, 0);
    g_cameraEngineReferenceAngle = g_lastCameraGameplayAngle;
    g_cameraEngineObservationArg1 = currentArg1;
    g_cameraEngineObservationGeneration = g_mapGeneration;

    char line[640]{};
    wsprintfA(
        line,
        "WASD ownership rebound invalidated %ld camera engine observation(s): oldSelf=0x%08X newSelf=0x%08X oldArg1=0x%08X newArg1=0x%08X; the current camera basis is kept provisionally and one non-blocking runtime acquisition is queued.",
        observations,
        reinterpret_cast<DWORD>(previousSelf),
        reinterpret_cast<DWORD>(currentSelf),
        previousArg1,
        currentArg1);
    LogLine(line);

    if (InterlockedCompareExchange(&g_enablePostFirstMoveCameraWatch, 0, 0) != 0 ||
        InterlockedCompareExchange(&g_verifyCameraAfterFirstKeyboardRelease, 0, 0) != 0) {
        StartRuntimeTransitionCameraAcquisition(
            now,
            previousArg1,
            currentArg1,
            "bounded WASD ownership recovery established the authoritative runtime player context");
    }
}

bool CommitWasdOwnershipRecovery(
    DWORD now,
    void* controller,
    void* player,
    DWORD arg1) {
    if (InterlockedCompareExchange(&g_wasdOwnershipRecoveryActive, 1, 1) == 0 ||
        g_wasdOwnershipRecoveryGeneration != g_mapGeneration ||
        InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0) !=
            static_cast<LONG>(EventCaptureState::Locked) ||
        g_mapLockedGeneration != g_mapGeneration) {
        return false;
    }

    void* finalPlayer = nullptr;
    DWORD finalLink = 0;
    DWORD finalArg1 = 0;
    if (!ReadDirectPlayerContextFromController(controller, &finalPlayer, &finalLink, &finalArg1) ||
        finalPlayer != player || finalArg1 != arg1 ||
        !LooksLikeUsableStoredPlayerSelf(finalPlayer) ||
        !LooksLikeReadableLocalMoveArg1(finalArg1) ||
        LooksLikeAdjacentLoadingContext(finalPlayer, finalArg1)) {
        return false;
    }
    char dispatchGuardReason[192]{};
    if (!LooksLikeSafeLocalMoveDispatchSelf(finalPlayer, dispatchGuardReason, sizeof(dispatchGuardReason))) {
        return false;
    }

    void* previousController = g_mapLockedController;
    void* previousSelf = g_mapLockedSelf;
    const DWORD previousArg1 = g_mapLockedArg1;

    AcquireSRWLockExclusive(&g_localMoveCaptureLock);
    g_playerMoveController = controller;
    g_playerLocalMoveSelf = finalPlayer;
    g_playerLocalMoveArg1 = finalArg1;
    g_lastKnownPlayerLocalMoveSelf = finalPlayer;
    g_lastKnownPlayerLocalMoveArg1 = finalArg1;
    ReleaseSRWLockExclusive(&g_localMoveCaptureLock);

    g_mapLockedController = controller;
    g_mapLockedSelf = finalPlayer;
    g_mapLockedArg1 = finalArg1;
    g_dynamicArg1Candidate = 0;
    g_dynamicArg1CandidateFirstTick = 0;
    g_dynamicArg1CandidateStableReads = 0;
    g_directContextCandidatePlayer = nullptr;
    g_directContextCandidateArg1 = 0;
    g_directContextCandidateStableReads = 0;
    g_lastStrictDirectContextMatchTick = LooksLikePlayerLocalMoveSelf(finalPlayer) ? now : 0;
    InterlockedExchange(&g_hasPlayerMoveController, 1);
    InterlockedExchange(&g_hasPlayerLocalMove, 1);
    InterlockedExchange(&g_hasLastKnownPlayerLocalMove, 1);
    InterlockedExchange(&g_contextLockedForLevel, 1);
    InterlockedExchange(&g_levelBootstrapActive, 0);
    InterlockedExchange(&g_directReacquireRequested, 0);
    InterlockedExchange(&g_keyboardMoveMissingContextLogged, 0);
    g_bootstrapWindowUntilTick = 0;

    float currentX = 0.0f;
    float currentZ = 0.0f;
    if (ReadFloatSafe(finalPlayer, 0x70, &currentX) &&
        ReadFloatSafe(finalPlayer, 0x78, &currentZ) &&
        currentX == currentX && currentZ == currentZ &&
        currentX > -100000.0f && currentX < 100000.0f &&
        currentZ > -100000.0f && currentZ < 100000.0f) {
        g_lastPlayerMonitorX = currentX;
        g_lastPlayerMonitorZ = currentZ;
        InterlockedExchange(&g_hasLastPlayerMonitorPosition, 1);
    }

    ResetWasdOwnershipRecoveryState(true);

    char line[704]{};
    wsprintfA(
        line,
        "WASD ownership recovered: generation=%lu oldController=0x%08X newController=0x%08X oldSelf=0x%08X newSelf=0x%08X oldArg1=0x%08X newArg1=0x%08X link=0x%08X; queued keyboard movement will resume.",
        g_mapGeneration,
        reinterpret_cast<DWORD>(previousController),
        reinterpret_cast<DWORD>(controller),
        reinterpret_cast<DWORD>(previousSelf),
        reinterpret_cast<DWORD>(finalPlayer),
        previousArg1,
        finalArg1,
        finalLink);
    LogLine(line);

    RearmCameraAfterWasdOwnershipRecovery(
        now,
        previousSelf,
        previousArg1,
        finalPlayer,
        finalArg1);

    const int moveMask = InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0);
    if (moveMask != 0) {
        QueueGameThreadMovement(
            PendingMovementKind::Move,
            moveMask,
            InterlockedCompareExchange(&g_keyboardMoveLeadStepX100, 0, 0));
    }
    return true;
}

void PollWasdOwnershipRecoveryOnGameThread() {
    const DWORD now = GetTickCount();
    if (InterlockedCompareExchange(&g_wasdOwnershipRecoveryActive, 1, 1) == 0 ||
        g_wasdOwnershipRecoveryGeneration != g_mapGeneration ||
        InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0) == 0 ||
        !IsGameWindowForegroundForInput() ||
        InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0 ||
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0) !=
            static_cast<LONG>(EventCaptureState::Locked) ||
        g_mapLockedGeneration != g_mapGeneration) {
        return;
    }

    void* evidenceController = nullptr;
    void* evidenceSelf = nullptr;
    DWORD evidenceArg1 = 0;
    const bool hasVanillaEvidence = ReadRecentVanillaWasdOwnershipEvidence(
        now,
        &evidenceController,
        &evidenceSelf,
        &evidenceArg1);

    void* controller = hasVanillaEvidence ? evidenceController : g_mapLockedController;
    if (controller == nullptr) {
        return;
    }

    void* player = nullptr;
    DWORD link = 0;
    DWORD arg1 = 0;
    if (!ReadDirectPlayerContextFromController(controller, &player, &link, &arg1)) {
        return;
    }

    bool vanillaBacked = hasVanillaEvidence &&
        controller == evidenceController &&
        player == evidenceSelf &&
        arg1 == evidenceArg1;
    bool sameController = controller == g_mapLockedController;
    if (!sameController && !vanillaBacked && g_mapLockedController != nullptr) {
        // A stored click-to-move observation may already be stale. Do not let it
        // mask the safer same-controller recovery path for the current event lock.
        controller = g_mapLockedController;
        player = nullptr;
        link = 0;
        arg1 = 0;
        if (!ReadDirectPlayerContextFromController(controller, &player, &link, &arg1)) {
            return;
        }
        vanillaBacked = false;
        sameController = true;
    }
    if (!sameController && !vanillaBacked) {
        return;
    }

    // arg1-only changes are handled by the existing dynamic settling gate. This
    // recovery path exists specifically for new-game phases which replace the
    // player/controller ownership after the event lock was established.
    const bool ownershipChanged =
        controller != g_mapLockedController ||
        player != g_mapLockedSelf;
    if (!ownershipChanged) {
        ClearWasdOwnershipRecoveryCandidate();
        return;
    }

    if (!LooksLikeUsableStoredPlayerSelf(player) ||
        !LooksLikeReadableLocalMoveArg1(arg1) ||
        LooksLikeAdjacentLoadingContext(player, arg1)) {
        ClearWasdOwnershipRecoveryCandidate();
        return;
    }
    char dispatchGuardReason[192]{};
    if (!LooksLikeSafeLocalMoveDispatchSelf(player, dispatchGuardReason, sizeof(dispatchGuardReason))) {
        ClearWasdOwnershipRecoveryCandidate();
        return;
    }

    LONG stableReads = 0;
    bool firstCandidateRead = false;
    AcquireSRWLockExclusive(&g_wasdOwnershipRecoveryLock);
    if (g_wasdOwnershipRecoveryCandidateController == controller &&
        g_wasdOwnershipRecoveryCandidateSelf == player &&
        g_wasdOwnershipRecoveryCandidateArg1 == arg1) {
        ++g_wasdOwnershipRecoveryCandidateStableReads;
    } else {
        g_wasdOwnershipRecoveryCandidateController = controller;
        g_wasdOwnershipRecoveryCandidateSelf = player;
        g_wasdOwnershipRecoveryCandidateArg1 = arg1;
        g_wasdOwnershipRecoveryCandidateStableReads = 1;
        firstCandidateRead = true;
    }
    stableReads = g_wasdOwnershipRecoveryCandidateStableReads;
    ReleaseSRWLockExclusive(&g_wasdOwnershipRecoveryLock);

    if (firstCandidateRead) {
        char line[512]{};
        wsprintfA(
            line,
            "WASD ownership recovery candidate observed: controller=0x%08X self=0x%08X arg1=0x%08X link=0x%08X evidence=%s requiredReads=%ld.",
            reinterpret_cast<DWORD>(controller),
            reinterpret_cast<DWORD>(player),
            arg1,
            link,
            vanillaBacked ? "confirmed-vanilla+direct" : "same-controller-direct",
            InterlockedCompareExchange(&g_wasdOwnershipRecoveryStableReadsRequired, 0, 0));
        LogLine(line);
    }

    const LONG requiredReads = InterlockedCompareExchange(
        &g_wasdOwnershipRecoveryStableReadsRequired,
        0,
        0);
    if (stableReads >= requiredReads) {
        CommitWasdOwnershipRecovery(now, controller, player, arg1);
    }
}

void PumpWasdOwnershipRecovery(DWORD now) {
    if (InterlockedCompareExchange(&g_wasdOwnershipRecoveryActive, 1, 1) == 0) {
        return;
    }

    const bool mustStop =
        g_wasdOwnershipRecoveryGeneration != g_mapGeneration ||
        InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0) == 0 ||
        !IsGameWindowForegroundForInput() ||
        InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0;
    if (mustStop) {
        ResetWasdOwnershipRecoveryState(false);
        return;
    }

    if (g_wasdOwnershipRecoveryUntilTick != 0 &&
        static_cast<LONG>(now - g_wasdOwnershipRecoveryUntilTick) >= 0) {
        InterlockedExchange(&g_wasdOwnershipRecoveryActive, 0);
        InterlockedExchange(&g_wasdOwnershipRecoveryMessagePosted, 0);
        g_wasdOwnershipRecoveryRetryAfterTick = now + static_cast<DWORD>(
            InterlockedCompareExchange(&g_wasdOwnershipRecoveryRetryDelayMs, 0, 0));
        ClearWasdOwnershipRecoveryCandidate();
        char line[448]{};
        wsprintfA(
            line,
            "WASD ownership recovery window expired without a stable replacement; the event lock was preserved and another bounded window may start after %ld ms while a movement key remains held.",
            InterlockedCompareExchange(&g_wasdOwnershipRecoveryRetryDelayMs, 0, 0));
        LogLine(line);
        return;
    }

    if (g_wasdOwnershipRecoveryNextPollTick != 0 &&
        static_cast<LONG>(now - g_wasdOwnershipRecoveryNextPollTick) < 0) {
        return;
    }
    g_wasdOwnershipRecoveryNextPollTick = now + static_cast<DWORD>(
        InterlockedCompareExchange(&g_wasdOwnershipRecoveryPollMs, 0, 0));
    PostWasdOwnershipRecoveryTickToGameWindow();
}

void RequestKeyboardMovementReacquire(DWORD now, const char* reason) {
    if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_enableRuntimeReacquire, 0, 0) == 0) {
        ArmEmergencyWasdBootstrap(now, false, reason);
        ArmWasdOwnershipRecovery(now, reason);
        if (InterlockedCompareExchange(&g_enableWasdOwnershipRecovery, 0, 0) == 0 &&
            now - g_lastKeyboardReacquireRequestLogTick >= 2000) {
            g_lastKeyboardReacquireRequestLogTick = now;
            char line[256]{};
            wsprintfA(
                line,
                "Runtime reacquire suppressed by event capture mode: %s.",
                reason != nullptr ? reason : "movement context unavailable");
            LogLine(line);
        }
        return;
    }
    if (g_lastKeyboardReacquireRequestTick != 0 &&
        now - g_lastKeyboardReacquireRequestTick < kKeyboardMissingContextReacquireMs) {
        return;
    }

    g_lastKeyboardReacquireRequestTick = now;
    InterlockedExchange(&g_directReacquireRequested, 1);
    InterlockedExchange(&g_levelBootstrapActive, 1);
    if (InterlockedCompareExchange(&g_enableCameraAutoCalibration, 0, 0) != 0) {
        InterlockedExchange(&g_cameraPostLoadRequest, 1);
    }
    g_bootstrapWindowUntilTick = now + 60000;
    g_lastDirectContextDiagnosticPollTick = 0;
    g_directContextCandidatePlayer = nullptr;
    g_directContextCandidateArg1 = 0;
    g_directContextCandidateStableReads = 0;

    if (now - g_lastKeyboardReacquireRequestLogTick >= 2000) {
        g_lastKeyboardReacquireRequestLogTick = now;
        char line[256]{};
        wsprintfA(
            line,
            "Keyboard movement reacquire requested: %s.",
            reason != nullptr ? reason : "movement key held without usable context");
        LogLine(line);
    }
}

void PollDirectPlayerContextDiagnostics(DWORD now) {
    const bool hasMoveContext =
        InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) != 0;
    const bool bootstrapActive =
        InterlockedCompareExchange(&g_levelBootstrapActive, 1, 1) != 0;
    const bool directReacquireRequested =
        InterlockedCompareExchange(&g_directReacquireRequested, 1, 1) != 0;
    const bool bootstrapNeedsContext =
        bootstrapActive && (!hasMoveContext || directReacquireRequested);
    if (hasMoveContext &&
        !bootstrapActive &&
        !directReacquireRequested &&
        !IsTransitionLocalMoveBlocked(now)) {
        return;
    }

    const DWORD directPollInterval =
        bootstrapNeedsContext ? kDirectContextBootstrapPollMs : kDirectContextStablePollMs;

    if (!kEnableTransitionDiagnostics ||
        now - g_lastDirectContextDiagnosticPollTick < directPollInterval ||
        InterlockedCompareExchange(&g_hasPlayerMoveController, 1, 1) == 0) {
        return;
    }
    g_lastDirectContextDiagnosticPollTick = now;

    if (IsPassiveContextSuspendActive(now)) {
        return;
    }

    if (IsLocalMoveExceptionRecoveryBlocked(now)) {
        return;
    }

    if (IsStabilityFallbackActive(now) && hasMoveContext) {
        return;
    }

    void* controller = nullptr;
    AcquireSRWLockShared(&g_localMoveCaptureLock);
    controller = g_playerMoveController;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);

    void* player = nullptr;
    DWORD link = 0;
    DWORD arg1 = 0;
    ReadDirectPlayerContextFromController(controller, &player, &link, &arg1);
    const bool playerValid = LooksLikePlayerLocalMoveSelf(player);
    const bool playerUsable = LooksLikeUsableStoredPlayerSelf(player);
    const bool arg1Valid = LooksLikeReadableLocalMoveArg1(arg1);
    if (hasMoveContext && playerValid && playerUsable && arg1Valid) {
        void* storedSelf = nullptr;
        DWORD storedArg1 = 0;
        AcquireSRWLockShared(&g_localMoveCaptureLock);
        storedSelf = g_playerLocalMoveSelf;
        storedArg1 = g_playerLocalMoveArg1;
        ReleaseSRWLockShared(&g_localMoveCaptureLock);
        if (player == storedSelf && arg1 == storedArg1) {
            g_lastStrictDirectContextMatchTick = now;
        }
    }
    const bool firstSnapshot =
        InterlockedCompareExchange(&g_hasDirectContextDiagnosticSnapshot, 1, 1) == 0;
    const bool stateChanged =
        firstSnapshot ||
        player != g_lastDiagnosticDirectPlayer ||
        link != g_lastDiagnosticDirectLink ||
        arg1 != g_lastDiagnosticDirectArg1 ||
        playerValid != g_lastDiagnosticDirectPlayerValid ||
        playerUsable != g_lastDiagnosticDirectPlayerUsable ||
        arg1Valid != g_lastDiagnosticDirectArg1Valid;
    if (stateChanged) {
        g_lastDiagnosticDirectPlayer = player;
        g_lastDiagnosticDirectLink = link;
        g_lastDiagnosticDirectArg1 = arg1;
        g_lastDiagnosticDirectPlayerValid = playerValid;
        g_lastDiagnosticDirectPlayerUsable = playerUsable;
        g_lastDiagnosticDirectArg1Valid = arg1Valid;
        InterlockedExchange(&g_hasDirectContextDiagnosticSnapshot, 1);

        if (kVerboseHookDiagnostics || !hasMoveContext || bootstrapActive) {
            char line[384]{};
            wsprintfA(
                line,
                "Direct context state t=%lu controller=0x%08X player=0x%08X link=0x%08X arg1=0x%08X playerValid=%d playerUsable=%d arg1Valid=%d hasMove=%ld.",
                now,
                reinterpret_cast<DWORD>(controller),
                reinterpret_cast<DWORD>(player),
                link,
                arg1,
                playerValid ? 1 : 0,
                playerUsable ? 1 : 0,
                arg1Valid ? 1 : 0,
                InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1));
            LogLine(line);
        }

        void* storedSelf = nullptr;
        DWORD storedArg1 = 0;
        AcquireSRWLockShared(&g_localMoveCaptureLock);
        storedSelf = g_playerLocalMoveSelf;
        storedArg1 = g_playerLocalMoveArg1;
        ReleaseSRWLockShared(&g_localMoveCaptureLock);
        const bool mapLocked =
            hasMoveContext &&
            IsCurrentMapContextLockedFor(storedSelf, storedArg1);

        if (hasMoveContext &&
            !mapLocked &&
            !playerValid &&
            playerUsable &&
            arg1Valid) {
            RecordContextInstability(
                now,
                "Repeated direct context validity flips while movement remains usable");
            EnterTransientContextHold(
                "Direct player context is temporarily not strict-valid while stored movement remains usable",
                now);
        }
    }

    const bool needsContext = bootstrapNeedsContext || directReacquireRequested;
    void* lastKnownPlayer = nullptr;
    AcquireSRWLockShared(&g_localMoveCaptureLock);
    lastKnownPlayer = g_lastKnownPlayerLocalMoveSelf;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);
    const bool hasLastKnownPlayer =
        InterlockedCompareExchange(&g_hasLastKnownPlayerLocalMove, 1, 1) != 0;
    const bool playerIdentityTrusted =
        playerValid || (hasLastKnownPlayer && player == lastKnownPlayer);
    const bool trustedCandidate =
        needsContext &&
        playerIdentityTrusted &&
        playerUsable &&
        arg1Valid;
    if (!trustedCandidate) {
        g_directContextCandidatePlayer = nullptr;
        g_directContextCandidateArg1 = 0;
        g_directContextCandidateStableReads = 0;
        return;
    }

    if (g_directContextCandidatePlayer == player && g_directContextCandidateArg1 == arg1) {
        ++g_directContextCandidateStableReads;
    } else {
        g_directContextCandidatePlayer = player;
        g_directContextCandidateArg1 = arg1;
        g_directContextCandidateStableReads = 1;
    }
    const int stableReadsRequired =
        bootstrapNeedsContext
            ? kDirectContextBootstrapStableReadsRequired
            : kDirectContextStableReadsRequired;
    if (g_directContextCandidateStableReads >= stableReadsRequired) {
        g_directContextCandidateStableReads = 0;
        InterlockedExchange(&g_directReacquireRequested, 0);
        SetPlayerLocalMoveContext(player, arg1, "Direct controller reacquired");
    }
}

void SetPlayerLocalMoveContext(void* self, DWORD arg1, const char* reason) {
    bool changed = false;
    AcquireSRWLockExclusive(&g_localMoveCaptureLock);
    if (g_playerLocalMoveSelf != self || g_playerLocalMoveArg1 != arg1) {
        g_playerLocalMoveSelf = self;
        g_playerLocalMoveArg1 = arg1;
        changed = true;
    }
    g_lastKnownPlayerLocalMoveSelf = self;
    g_lastKnownPlayerLocalMoveArg1 = arg1;
    ReleaseSRWLockExclusive(&g_localMoveCaptureLock);
    InterlockedExchange(&g_hasLastKnownPlayerLocalMove, 1);
    InterlockedExchange(&g_contextLockedForLevel, 1);
    InterlockedExchange(&g_levelBootstrapActive, 0);
    InterlockedExchange(&g_directReacquireRequested, 0);
    g_bootstrapWindowUntilTick = 0;
    if (LooksLikePlayerLocalMoveSelf(self) && LooksLikeReadableLocalMoveArg1(arg1)) {
        g_lastStrictDirectContextMatchTick = GetTickCount();
    }
    FastResumeTransitionGuardAfterStableContext(GetTickCount(), reason);

    float currentX = 0.0f;
    float currentZ = 0.0f;
    if (ReadFloatSafe(self, 0x70, &currentX) &&
        ReadFloatSafe(self, 0x78, &currentZ) &&
        currentX == currentX &&
        currentZ == currentZ &&
        currentX > -100000.0f &&
        currentX < 100000.0f &&
        currentZ > -100000.0f &&
        currentZ < 100000.0f) {
        g_lastPlayerMonitorX = currentX;
        g_lastPlayerMonitorZ = currentZ;
        InterlockedExchange(&g_hasLastPlayerMonitorPosition, 1);
    } else {
        InterlockedExchange(&g_hasLastPlayerMonitorPosition, 0);
    }

    if (InterlockedExchange(&g_hasPlayerLocalMove, 1) == 0 || changed) {
        InterlockedExchange(&g_keyboardMoveMissingContextLogged, 0);
        char line[256]{};
        wsprintfA(
            line,
            "%s player local move context: self=0x%08X arg1=0x%08X.",
            reason,
            reinterpret_cast<DWORD>(self),
            arg1);
        LogLine(line);
    }

    TryLockCurrentMapContext(reason, GetTickCount());
}

void ClearPlayerLocalMoveContext(const char* reason) {
    const DWORD now = GetTickCount();
    const bool preserveEventLock =
        InterlockedCompareExchange(&g_preserveEventLockAcrossRuntimeDips, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0) ==
            static_cast<LONG>(EventCaptureState::Locked) &&
        g_mapLockedGeneration == g_mapGeneration &&
        g_mapLockedController != nullptr &&
        g_mapLockedSelf != nullptr;
    if (preserveEventLock) {
        ResetPendingMovementQueue();
        if (now - g_lastSuppressedContextClearLogTick >= 500) {
            g_lastSuppressedContextClearLogTick = now;
            char line[448]{};
            wsprintfA(line,
                "Runtime context clear suppressed: event-locked player identity preserved until next UNetSetLevelMsg. reason=%s.",
                reason != nullptr ? reason : "runtime validation dip");
            LogLine(line);
        }
        return;
    }

    AcquireSRWLockExclusive(&g_localMoveCaptureLock);
    g_playerLocalMoveSelf = nullptr;
    g_playerLocalMoveArg1 = 0;
    ReleaseSRWLockExclusive(&g_localMoveCaptureLock);
    InterlockedExchange(&g_hasPlayerLocalMove, 0);
    InterlockedExchange(&g_contextLockedForLevel, 0);
    InterlockedExchange(&g_hasLastPlayerMonitorPosition, 0);
    g_mapLockedGeneration = 0;
    g_lastSuccessfulKeyboardMoveGeneration = 0;
    g_mapLockedController = nullptr;
    g_mapLockedScene = 0;
    g_mapLockedActiveLevel = 0;
    g_mapLockedSelf = nullptr;
    g_mapLockedArg1 = 0;
    g_lastStrictDirectContextMatchTick = 0;
    ResetPendingMovementQueue();
    LogLine(reason);
    if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_enableRuntimeReacquire, 0, 0) != 0) {
        StartLevelBootstrapWindow("Post-load bootstrap window opened after context clear.");
    }
}

void AutoRefreshPlayerLocalMoveContext(void* self, DWORD arg1) {
    if (!LooksLikePlayerLocalMoveSelf(self)) {
        return;
    }
    if (!LooksLikeReadableLocalMoveArg1(arg1)) {
        return;
    }

    if (InterlockedCompareExchange(&g_contextLockedForLevel, 1, 1) != 0 &&
        InterlockedCompareExchange(&g_levelBootstrapActive, 1, 1) == 0) {
        void* currentSelf = nullptr;
        DWORD currentArg1 = 0;
        AcquireSRWLockShared(&g_localMoveCaptureLock);
        currentSelf = g_playerLocalMoveSelf;
        currentArg1 = g_playerLocalMoveArg1;
        ReleaseSRWLockShared(&g_localMoveCaptureLock);
        if (currentSelf == self && currentArg1 == arg1) {
            return;
        }

        if (IsCurrentMapContextLockedFor(currentSelf, currentArg1)) {
            if (currentSelf == self && kVerboseHookDiagnostics) {
                LogLine("Auto refresh ignored: map-locked context keeps original arg1 until scene/activeLevel changes.");
            }
            return;
        }

        if (currentSelf == self && currentArg1 != arg1) {
            LogLine("Level transition detected: player movement context arg1 changed.");
            InterlockedExchange(&g_contextLockedForLevel, 0);
            InterlockedExchange(&g_levelBootstrapActive, 1);
            SetPlayerLocalMoveContext(self, arg1, "Level transition refreshed");
            return;
        }

        LogLine("Auto refresh ignored: level context is already locked.");
        return;
    }

    SetPlayerLocalMoveContext(self, arg1, "Auto refreshed");
}

void LogLocalMoveProbeCall(
    void* self,
    DWORD arg1,
    DWORD arg2,
    DWORD arg3,
    DWORD arg4,
    DWORD arg5,
    void* sourceCallerReturn,
    void* sourceController) {
    const bool internalCall = InterlockedCompareExchange(&g_internalLocalMoveCall, 0, 0) > 0;
    if (internalCall) {
        if (kVerboseHookDiagnostics) {
            const LONG callIndex = InterlockedIncrement(&g_localMoveProbeCalls);
            if (callIndex <= 64) {
                char line[384]{};
                wsprintfA(
                    line,
                    "Local move 0x554AB0 internal call ignored for capture: self=0x%08X arg1=0x%08X arg2=0x%08X arg3=0x%08X.",
                    reinterpret_cast<DWORD>(self),
                    arg1,
                    arg2,
                    arg3);
                LogLine(line);
            }
        }
        return;
    }

    if (!LooksLikeReadableLocalMoveArg1(arg1)) {
        if (kVerboseHookDiagnostics) {
            char line[384]{};
            wsprintfA(
                line,
                "Local move 0x554AB0 capture skipped: unreadable arg1 self=0x%08X arg1=0x%08X.",
                reinterpret_cast<DWORD>(self),
                arg1);
            LogLine(line);
        }
        return;
    }

    if (kVerboseHookDiagnostics) {
        LogLocalMoveSelfDetails(self);
        LogArg1Details(arg1);
    }

    const DWORD moduleBase = reinterpret_cast<DWORD>(GetModuleHandleW(nullptr));
    const DWORD sourceCaller = reinterpret_cast<DWORD>(sourceCallerReturn);
    const DWORD sourceCallerStatic = sourceCaller - moduleBase + 0x00400000;

    // Character classes, transformations and companion combinations can alter
    // legacy marker fields such as self+0x1A8, self+0x314 or self+0x370. The
    // confirmed vanilla player callsite plus the controller's direct player
    // chain is a stronger identity proof and is independent of those layouts.
    void* directPlayer = nullptr;
    DWORD directLink = 0;
    DWORD directArg1 = 0;
    ReadDirectPlayerContextFromController(
        sourceController,
        &directPlayer,
        &directLink,
        &directArg1);
    const bool confirmedPlayerCallsite = sourceCallerStatic == 0x0042748E;
    const bool controllerIdentityPlayer =
        confirmedPlayerCallsite &&
        sourceController != nullptr &&
        directPlayer == self &&
        directArg1 == arg1 &&
        LooksLikeUsableStoredPlayerSelf(self) &&
        LooksLikeReadableLocalMoveArg1(arg1) &&
        !LooksLikeAdjacentLoadingContext(self, arg1);
    const bool legacyMarkerPlayer =
        confirmedPlayerCallsite && LooksLikePlayerLocalMoveSelf(self);
    const bool playerSource = controllerIdentityPlayer || legacyMarkerPlayer;
    if (playerSource) {
        const DWORD now = GetTickCount();
        const DWORD vanillaThreadId = GetCurrentThreadId();
        if (g_lastVanillaPlayerMoveThreadId != vanillaThreadId) {
            g_lastVanillaPlayerMoveThreadId = vanillaThreadId;
            if (InterlockedCompareExchange(&g_logThreadAffinity, 0, 0) != 0) {
                char threadLine[256]{};
                wsprintfA(
                    threadLine,
                    "Vanilla player LocalMove source thread=%lu gameWindowThread=%lu setRunningThread=%lu.",
                    vanillaThreadId,
                    g_gameWindowThreadId,
                    g_setRunningThreadId);
                LogLine(threadLine);
            }
        }
        ReleaseLocalMoveExceptionQuarantineFromVanillaMove(now);

        // In event-driven mode the SetLevel message core is the only controller
        // identity source. A LocalMove observation may come from a skill,
        // interaction, scripted facing or temporary gameplay object, so it is
        // diagnostic-only unless legacy refresh is explicitly re-enabled.
        const bool mayMutateContextFromLocalMove =
            InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0 ||
            InterlockedCompareExchange(&g_localMoveCanRefreshContext, 0, 0) != 0;

        bool controllerChanged = false;
        if (mayMutateContextFromLocalMove) {
            AcquireSRWLockExclusive(&g_localMoveCaptureLock);
            if (g_playerMoveController != sourceController) {
                g_playerMoveController = sourceController;
                controllerChanged = true;
            }
            ReleaseSRWLockExclusive(&g_localMoveCaptureLock);
            InterlockedExchange(&g_hasPlayerMoveController, 1);
            if (controllerChanged) {
                InterlockedExchange(&g_hasDirectContextDiagnosticSnapshot, 0);
            }
        }

        if (controllerIdentityPlayer && !legacyMarkerPlayer &&
            now - g_lastEmergencyWasdBootstrapLogTick >= 500) {
            g_lastEmergencyWasdBootstrapLogTick = now;
            DWORD d1A8 = 0;
            DWORD d314 = 0;
            DWORD d370 = 0;
            ReadDwordSafe(self, 0x1A8, &d1A8);
            ReadDwordSafe(self, 0x314, &d314);
            ReadDwordSafe(self, 0x370, &d370);
            char identityLine[512]{};
            wsprintfA(
                identityLine,
                "Vanilla player LocalMove accepted by controller identity despite legacy marker mismatch: self=0x%08X arg1=0x%08X d1A8=0x%08X d314=0x%08X d370=0x%08X; class/companion-specific markers are ignored for recovery.",
                reinterpret_cast<DWORD>(self),
                arg1,
                d1A8,
                d314,
                d370);
            LogLine(identityLine);
        }
        ObserveConfirmedVanillaPlayerMoveForEmergencyBootstrap(
            now,
            sourceController,
            self,
            arg1,
            directPlayer,
            directArg1);
        ObserveConfirmedVanillaPlayerMoveForWasdRecovery(
            now,
            sourceController,
            self,
            arg1,
            directPlayer,
            directArg1);
        const LONG hasMoveBeforeLog = InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1);
        const bool recoveryLog =
            controllerChanged ||
            hasMoveBeforeLog == 0 ||
            IsLocalMoveExceptionQuarantineActive(now) ||
            InterlockedCompareExchange(&g_requiresVanillaMoveAfterLocalMoveException, 1, 1) != 0;
        const bool rateLimitedRecoveryLog =
            recoveryLog && now - g_lastPlayerMoveSourceLogTick >= 1000;
        if (kVerboseHookDiagnostics || rateLimitedRecoveryLog) {
            g_lastPlayerMoveSourceLogTick = now;
            char line[448]{};
            wsprintfA(
                line,
                "Player move source observed: t=%lu callerStatic=0x%08X controller=0x%08X self=0x%08X arg1=0x%08X directPlayer=0x%08X directLink=0x%08X directArg1=0x%08X hasMove=%ld observationalOnly=%d.",
                now,
                sourceCallerStatic,
                reinterpret_cast<DWORD>(sourceController),
                reinterpret_cast<DWORD>(self),
                arg1,
                reinterpret_cast<DWORD>(directPlayer),
                directLink,
                directArg1,
                hasMoveBeforeLog,
                mayMutateContextFromLocalMove ? 0 : 1);
            LogLine(line);
        }
    }

    if (!playerSource && LooksLikeCompanionLocalMoveSelf(self)) {
        if (kVerboseHookDiagnostics) {
            LogLine("Local move capture skipped: companion/pet-like actor is not used for player WASD context.");
        }
        return;
    }

    if (playerSource &&
        InterlockedCompareExchange(&g_localMoveCanRefreshContext, 0, 0) != 0) {
        AutoRefreshPlayerLocalMoveContext(self, arg1);
    }

    if (!kVerboseHookDiagnostics) {
        return;
    }

    const LONG callIndex = InterlockedIncrement(&g_localMoveProbeCalls);
    if (callIndex > 64) {
        return;
    }

    char line[384]{};
    wsprintfA(
        line,
        "Local move 0x554AB0 captured: self=0x%08X arg1=0x%08X arg2=0x%08X arg3=0x%08X arg4=0x%08X arg5=0x%08X.",
        reinterpret_cast<DWORD>(self),
        arg1,
        arg2,
        arg3,
        arg4,
        arg5);
    LogLine(line);
}

void CallLocalMove(void* self, DWORD arg1, DWORD arg2, DWORD arg3, DWORD arg4, DWORD arg5) {
    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) {
        LogLine("Local move call skipped: no executable module handle.");
        return;
    }

    using LocalMoveFn = void(__thiscall*)(void*, DWORD, DWORD, DWORD, DWORD, DWORD);
    LocalMoveFn localMove = reinterpret_cast<LocalMoveFn>(reinterpret_cast<BYTE*>(exe) + 0x154AB0);

    InterlockedIncrement(&g_internalLocalMoveCall);
    __try {
        localMove(self, arg1, arg2, arg3, arg4, arg5);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        const DWORD now = GetTickCount();
        InterlockedIncrement(&g_localMoveExceptionCount);
        InterlockedExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 1);
        EnterLocalMoveExceptionQuarantine(now);
        EnterTransientContextHold(
            "Local move call trapped an exception; stable movement context preserved while send loop cools down",
            now);
    }
    InterlockedDecrement(&g_internalLocalMoveCall);
}

bool GetLockedPlayerLocalMoveContext(void** selfOut, DWORD* arg1Out) {
    const DWORD now = GetTickCount();
    if (InterlockedCompareExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 1, 1) != 0) {
        if (IsLocalMoveExceptionRecoveryBlocked(now) ||
            InterlockedCompareExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 1, 1) != 0) {
            if (InterlockedExchange(&g_keyboardMoveMissingContextLogged, 1) == 0) {
                LogLine("Keyboard move skipped: LocalMove exception cooldown active.");
            }
            return false;
        }
    }

    if (IsPassiveContextSuspendActive(now)) {
        if (InterlockedExchange(&g_keyboardMoveMissingContextLogged, 1) == 0) {
            LogLine("Keyboard move skipped: passive context suspend active.");
        }
        return false;
    }

    if (IsLocalMoveExceptionRecoveryBlocked(now)) {
        if (InterlockedExchange(&g_keyboardMoveMissingContextLogged, 1) == 0) {
            LogLine("Keyboard move skipped: LocalMove exception quarantine active.");
        }
        return false;
    }

    if (IsTransitionLocalMoveBlocked(now)) {
        if (InterlockedExchange(&g_keyboardMoveMissingContextLogged, 1) == 0) {
            LogLine("Keyboard move skipped: transition guard active.");
        }
        return false;
    }

    if (IsTransientContextHoldActive(now)) {
        if (InterlockedExchange(&g_keyboardMoveMissingContextLogged, 1) == 0) {
            LogLine("Keyboard move skipped: transient context hold active; stable context preserved.");
        }
        return false;
    }

    if (InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) == 0) {
        if (InterlockedExchange(&g_keyboardMoveMissingContextLogged, 1) == 0) {
            LogLine("Keyboard move skipped: no level context yet. Waiting for post-load bootstrap.");
        }
        return false;
    }

    AcquireSRWLockShared(&g_localMoveCaptureLock);
    *selfOut = g_playerLocalMoveSelf;
    *arg1Out = g_playerLocalMoveArg1;
    ReleaseSRWLockShared(&g_localMoveCaptureLock);

    if (*selfOut == nullptr) {
        InterlockedExchange(&g_hasPlayerLocalMove, 0);
        return false;
    }

    if (!LooksLikeUsableStoredPlayerSelf(*selfOut)) {
        ClearPlayerLocalMoveContext("Player context invalidated: stored self is no longer usable.");
        return false;
    }

    if (!LooksLikeReadableLocalMoveArg1(*arg1Out)) {
        if (g_mapLockedGeneration == g_mapGeneration &&
            g_mapLockedSelf == *selfOut &&
            g_mapLockedArg1 == *arg1Out) {
            EnterTransientContextHold(
                "Map-locked arg1 readability dipped; stable context preserved",
                now);
            return false;
        }
        ClearPlayerLocalMoveContext("Player context invalidated: stored arg1 is no longer readable.");
        return false;
    }

    if (!StoredContextMatchesCurrentController(*selfOut, *arg1Out)) {
        if (IsCurrentMapContextLockedFor(*selfOut, *arg1Out)) {
            EnterTransientContextHold(
                "Map-locked context is not confirmed by the direct controller; stable context preserved",
                now);
            return false;
        }
        if (CanUseStoredContextDuringStabilityFallback(now, *selfOut, *arg1Out)) {
            return true;
        }
        RecordContextInstability(
            now,
            "Stored movement context remains usable but direct controller validation is unstable");
        if (InterlockedExchange(&g_keyboardMoveMissingContextLogged, 1) == 0) {
            LogLine("Keyboard move skipped: direct controller validation is unstable; stored context kept.");
        }
        return false;
    }

    if (*selfOut == nullptr || *arg1Out == 0) {
        return false;
    }

    return true;
}

void SendLocalMoveTarget(void* self, DWORD arg1, float targetX, float targetZ, const char* reason) {
    const DWORD now = GetTickCount();
    if (InterlockedCompareExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 1, 1) != 0) {
        if (IsLocalMoveExceptionRecoveryBlocked(now) ||
            InterlockedCompareExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 1, 1) != 0) {
            if (kVerboseInputLogging) {
                LogLine("Local move call skipped: LocalMove exception cooldown active.");
            }
            return;
        }
    }

    if (IsPassiveContextSuspendActive(now)) {
        if (kVerboseInputLogging) {
            LogLine("Local move call skipped: passive context suspend active at send time.");
        }
        return;
    }

    if (IsLocalMoveExceptionRecoveryBlocked(now)) {
        if (kVerboseInputLogging) {
            LogLine("Local move call skipped: LocalMove exception quarantine active at send time.");
        }
        return;
    }

    if (IsTransitionLocalMoveBlocked(now)) {
        if (kVerboseInputLogging) {
            LogLine("Local move call skipped: transition guard active at send time.");
        }
        return;
    }

    if (IsTransientContextHoldActive(now)) {
        if (kVerboseInputLogging) {
            LogLine("Local move call skipped: transient context hold active at send time.");
        }
        return;
    }

    if (!IsGameWindowForegroundForInput()) {
        if (kVerboseInputLogging) {
            LogLine("Local move call skipped: game window is not foreground at send time.");
        }
        return;
    }

    if (!LooksLikeUsableStoredPlayerSelf(self)) {
        ClearPlayerLocalMoveContext("Local move call skipped: stored player self failed final send-time validation.");
        return;
    }

    if (!LooksLikeReadableLocalMoveArg1(arg1)) {
        EnterTransientContextHold(
            "Final send-time arg1 readability check failed while stable player context is preserved",
            now);
        return;
    }

    if (!StoredContextMatchesCurrentController(self, arg1)) {
        if (!CanUseStoredContextDuringStabilityFallback(now, self, arg1)) {
            RecordContextInstability(
                now,
                "Final send-time controller validation is unstable while stored context remains usable");
            EnterTransientContextHold(
                "Final send-time controller validation is unstable while stored context remains usable",
                now);
            if (kVerboseInputLogging) {
                LogLine("Local move call skipped: direct controller validation unstable at send time; stored context kept.");
            }
            return;
        }
    }

    if (!StoredContextStrictlyMatchesCurrentController(self, arg1)) {
        if (CanUseDirectCarryDuringStrictDip(now, self, arg1)) {
            goto SendLocalMove;
        }
        RecordContextInstability(
            now,
            "Final strict send gate rejected unstable player context before LocalMove call");
        EnterTransientContextHold(
            "Final strict send gate rejected unstable player context before LocalMove call",
            now);
        if (kVerboseInputLogging) {
            LogLine("Local move call skipped: strict player context validation failed at send time.");
        }
        return;
    }

SendLocalMove:
    const DWORD targetXBits = FloatToBits(targetX);
    const DWORD targetZBits = FloatToBits(targetZ);

    const bool isMicrostep = lstrcmpA(reason, "ZQSD microstep") == 0;
    const bool isReleaseBrake = lstrcmpA(reason, "ZQSD release brake") == 0;
    if ((!isMicrostep && !isReleaseBrake) ||
        (kVerboseInputLogging && now - g_lastKeyboardMoveLogTick >= 1000)) {
        if (isMicrostep || isReleaseBrake) {
            g_lastKeyboardMoveLogTick = now;
        }
        char line[256]{};
        wsprintfA(
            line,
            "%s local move target: self=0x%08X xRaw=0x%08X zRaw=0x%08X.",
            reason,
            reinterpret_cast<DWORD>(self),
            targetXBits,
            targetZBits);
        LogLine(line);
    }

    // The confirmed vanilla player callsite returning at 0x0042748E pushes
    // zero for both trailing LocalMove arguments as well.
    CallLocalMove(self, arg1, targetXBits, targetZBits, 0, 0);
}

template <typename T>
bool ResolveOgreCameraExport(HMODULE module, const char* name, T* target) {
    FARPROC address = GetProcAddress(module, name);
    *target = reinterpret_cast<T>(address);
    if (address == nullptr) {
        char line[320]{};
        wsprintfA(line, "Camera-relative movement missing OGRE export: %s.", name);
        LogLine(line);
        return false;
    }
    return true;
}

bool ResolveOgreCameraApi(DWORD now) {
    if (g_ogreCameraApiState > 0) {
        return true;
    }
    if (g_ogreCameraApiState < 0) {
        return false;
    }
    if (g_lastCameraApiResolveTick != 0 &&
        now - g_lastCameraApiResolveTick < kCameraApiResolveIntervalMs) {
        return false;
    }
    g_lastCameraApiResolveTick = now;

    HMODULE ogre = GetModuleHandleW(L"OgreMain.dll");
    if (ogre == nullptr) {
        return false;
    }

    bool resolved = true;
    resolved &= ResolveOgreCameraExport(
        ogre,
        "?getSingletonPtr@Root@Ogre@@SAPAV12@XZ",
        &g_getOgreRootSingletonPtr);
    resolved &= ResolveOgreCameraExport(
        ogre,
        "?getRenderSystem@Root@Ogre@@QAEPAVRenderSystem@2@XZ",
        &g_getOgreRenderSystem);
    resolved &= ResolveOgreCameraExport(
        ogre,
        "?_getViewport@RenderSystem@Ogre@@UAEPAVViewport@2@XZ",
        &g_getOgreActiveViewport);
    resolved &= ResolveOgreCameraExport(
        ogre,
        "?getCamera@Viewport@Ogre@@QBEPAVCamera@2@XZ",
        &g_getOgreViewportCamera);
    resolved &= ResolveOgreCameraExport(
        ogre,
        "?getName@Camera@Ogre@@UBEABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ",
        &g_getOgreCameraName);
    resolved &= ResolveOgreCameraExport(
        ogre,
        "?getDerivedOrientation@Camera@Ogre@@QBEABVQuaternion@2@XZ",
        &g_getOgreCameraDerivedOrientation);

    g_ogreCameraApiState = resolved ? 1 : -1;
    if (resolved) {
        LogLine("Camera-relative movement OGRE API resolved.");
    }
    return resolved;
}

bool DecodeVisualCpp2008CameraName(
    const void* stringObject,
    char* destination,
    size_t destinationCount) {
    if (stringObject == nullptr || destination == nullptr || destinationCount < 2) {
        return false;
    }

    BYTE raw[28]{};
    CopyMemory(raw, stringObject, sizeof(raw));
    DWORD nameSize = 0;
    DWORD nameCapacity = 0;
    CopyMemory(&nameSize, raw + 20, sizeof(nameSize));
    CopyMemory(&nameCapacity, raw + 24, sizeof(nameCapacity));
    if (nameSize >= destinationCount || nameCapacity < nameSize || nameCapacity > 0x100000) {
        return false;
    }

    const char* characters = nullptr;
    if (nameCapacity < 16) {
        characters = reinterpret_cast<const char*>(raw + 4);
    } else {
        CopyMemory(&characters, raw + 4, sizeof(characters));
    }
    if (characters == nullptr && nameSize != 0) {
        return false;
    }

    for (DWORD index = 0; index < nameSize; ++index) {
        const unsigned char value = static_cast<unsigned char>(characters[index]);
        if (value < 0x20 || value > 0x7E) {
            return false;
        }
        destination[index] = static_cast<char>(value);
    }
    destination[nameSize] = '\0';
    return true;
}

bool IsExactPlayerCamera(void* camera) {
    if (camera == nullptr) {
        return false;
    }
    char name[16]{};
    const void* nameObject = g_getOgreCameraName(camera);
    return DecodeVisualCpp2008CameraName(nameObject, name, sizeof(name)) &&
        lstrcmpA(name, "PlayerCam") == 0;
}

void MarkPlayerCamRenderedForCurrentMap(void* window, void* viewport) {
    (void)window;
    if (viewport == nullptr ||
        g_ogreCameraApiState <= 0 ||
        g_getOgreViewportCamera == nullptr ||
        g_getOgreCameraName == nullptr) {
        return;
    }

    __try {
        void* camera = g_getOgreViewportCamera(viewport);
        if (!IsExactPlayerCamera(camera)) {
            return;
        }

        const DWORD playerCamThreadId = GetCurrentThreadId();
        if (g_playerCamThreadId != playerCamThreadId) {
            g_playerCamThreadId = playerCamThreadId;
            if (InterlockedCompareExchange(&g_logThreadAffinity, 0, 0) != 0) {
                char threadLine[256]{};
                wsprintfA(
                    threadLine,
                    "PlayerCam render thread=%lu gameWindowThread=%lu setRunningThread=%lu.",
                    playerCamThreadId,
                    g_gameWindowThreadId,
                    g_setRunningThreadId);
                LogLine(threadLine);
            }
        }

        const DWORD scene = g_lastDiagnosticScene;
        const DWORD activeLevel = g_lastDiagnosticActiveLevel;
        if (g_mapGeneration == 0) {
            return;
        }
        if (g_mapIdentityScene != 0 &&
            g_mapIdentityActiveLevel != 0 &&
            (scene != g_mapIdentityScene ||
                activeLevel != g_mapIdentityActiveLevel)) {
            return;
        }

        const bool firstPlayerCamForGeneration =
            g_mapRenderedPlayerCamGeneration != g_mapGeneration;
        if (firstPlayerCamForGeneration) {
            g_mapRenderedPlayerCamGeneration = g_mapGeneration;
            g_mapRenderedPlayerCamScene = g_mapIdentityScene != 0 ? g_mapIdentityScene : scene;
            g_mapRenderedPlayerCamActiveLevel = g_mapIdentityActiveLevel != 0 ? g_mapIdentityActiveLevel : activeLevel;
        }

        const LONG captureState = InterlockedCompareExchange(&g_eventCaptureState, 0, 0);
        if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0 &&
            (captureState == static_cast<LONG>(EventCaptureState::AwaitReadySignal) ||
                captureState == static_cast<LONG>(EventCaptureState::Capturing)) &&
            g_eventCaptureGeneration == g_mapGeneration) {
            InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::Capturing));
            PostCaptureTickToGameWindow();
        }

        if (!firstPlayerCamForGeneration) {
            return;
        }

        char line[288]{};
        wsprintfA(
            line,
            "Map render ready: PlayerCam viewport rendered for generation=%lu scene=0x%08X activeLevel=0x%08X.",
            g_mapGeneration,
            scene,
            activeLevel);
        LogLine(line);
        TryLockCurrentMapContext("PlayerCam rendered in D3D9 viewport", GetTickCount());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        char line[128]{};
        wsprintfA(line, "Map render ready probe callback raised exception=0x%08X.", GetExceptionCode());
        LogLine(line);
    }
}

void __stdcall OnD3D9RenderWindowUpdateViewportForMapReady(
    void* window,
    void* viewport,
    bool updateStatistics) {
    (void)updateStatistics;
    MarkPlayerCamRenderedForCurrentMap(window, viewport);
}

__declspec(naked) void D3D9RenderWindowUpdateViewportMapReadyHook() {
    __asm {
        pushfd
        pushad
        mov eax, [esp + 44]
        push eax
        mov eax, [esp + 44]
        push eax
        mov eax, [esp + 32]
        push eax
        call OnD3D9RenderWindowUpdateViewportForMapReady
        popad
        popfd
        jmp dword ptr [g_d3d9UpdateViewportGateway]
    }
}

enum class CameraReadFailure {
    None,
    RootUnavailable,
    RenderSystemUnavailable,
    ViewportUnavailable,
    PlayerCameraUnavailable,
    OrientationUnavailable,
    GetterException
};

const char* CameraReadFailureName(CameraReadFailure failure) {
    switch (failure) {
        case CameraReadFailure::RootUnavailable: return "OGRE root unavailable";
        case CameraReadFailure::RenderSystemUnavailable: return "OGRE render system unavailable";
        case CameraReadFailure::ViewportUnavailable: return "OGRE active viewport unavailable";
        case CameraReadFailure::PlayerCameraUnavailable: return "PlayerCam unavailable";
        case CameraReadFailure::OrientationUnavailable: return "PlayerCam orientation unavailable";
        case CameraReadFailure::GetterException: return "protected OGRE camera call failed";
        default: return "camera data unavailable";
    }
}

bool TryReadPlayerCameraOrientation(
    tl2_camera_math::Quaternion* orientationOut,
    CameraReadFailure* failureOut) {
    *failureOut = CameraReadFailure::None;

    __try {
        void* root = g_getOgreRootSingletonPtr();
        if (root == nullptr) {
            *failureOut = CameraReadFailure::RootUnavailable;
            return false;
        }
        void* renderSystem = g_getOgreRenderSystem(root);
        if (renderSystem == nullptr) {
            *failureOut = CameraReadFailure::RenderSystemUnavailable;
            return false;
        }
        void* viewport = g_getOgreActiveViewport(renderSystem);
        if (viewport == nullptr) {
            *failureOut = CameraReadFailure::ViewportUnavailable;
            return false;
        }

        void* activeCamera = g_getOgreViewportCamera(viewport);
        void* selectedCamera = nullptr;
        if (IsExactPlayerCamera(activeCamera)) {
            selectedCamera = activeCamera;
        } else if (IsExactPlayerCamera(g_playerCamera)) {
            selectedCamera = g_playerCamera;
        }
        if (selectedCamera == nullptr) {
            g_playerCamera = nullptr;
            *failureOut = CameraReadFailure::PlayerCameraUnavailable;
            return false;
        }

        g_playerCamera = selectedCamera;
        const tl2_camera_math::Quaternion* orientation =
            g_getOgreCameraDerivedOrientation(selectedCamera);
        if (orientation == nullptr) {
            *failureOut = CameraReadFailure::OrientationUnavailable;
            return false;
        }
        *orientationOut = *orientation;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_playerCamera = nullptr;
        *failureOut = CameraReadFailure::GetterException;
        return false;
    }
}

void UseFixedCameraFallback(const char* reason) {
    g_cameraCandidateStableSamples = 0;
    const CameraAcquisitionOrigin origin = static_cast<CameraAcquisitionOrigin>(
        InterlockedExchange(
            &g_cameraAcquireOrigin,
            static_cast<LONG>(CameraAcquisitionOrigin::None)));
    if (origin == CameraAcquisitionOrigin::RuntimeTransitionAutomatic) {
        g_runtimeTransitionCameraStartedTick = 0;
    }

    const bool failedInitialGate =
        origin == CameraAcquisitionOrigin::InitialAutomatic &&
        g_cameraAcquireBlocksMovement &&
        InterlockedCompareExchange(&g_blockKeyboardUntilCameraSync, 0, 0) != 0;
    if (failedInitialGate &&
        InterlockedCompareExchange(&g_allowProvisionalFallbackAfterInitialTimeout, 0, 0) == 0) {
        const DWORD now = GetTickCount();
        g_playerCamera = nullptr;
        g_cameraAcquireActive = true;
        InterlockedExchange(
            &g_cameraAcquireOrigin,
            static_cast<LONG>(CameraAcquisitionOrigin::InitialAutomatic));
        g_cameraAcquireAttempts = 0;
        g_cameraAcquireWaitingForContextLogged = false;
        g_cameraAcquirePreserveBasisOnFailure = false;
        g_cameraAcquireBlocksMovement = true;
        g_cameraCandidateForwardX = 0.0f;
        g_cameraCandidateForwardZ = 0.0f;
        g_cameraCandidateGameplayAngle = 0.0f;
        g_cameraAcquireDueTick = now + kCameraAcquireRetryMs;
        g_cameraAcquireUntilTick = g_cameraAcquireDueTick + static_cast<DWORD>(
            InterlockedCompareExchange(&g_initialCameraObservationTimeoutMs, 0, 0));
        char retryLine[448]{};
        wsprintfA(
            retryLine,
            "Initial PlayerCam observation gate did not obtain a valid basis: %s; provisional fallback is disabled, so another bounded acquisition window was armed while movement remains gated.",
            reason);
        LogLine(retryLine);
        return;
    }

    g_cameraAcquireBlocksMovement = false;
    if (g_cameraAcquirePreserveBasisOnFailure) {
        g_cameraAcquirePreserveBasisOnFailure = false;
        const bool postReleaseVerification =
            origin == CameraAcquisitionOrigin::PostReleaseAutomatic &&
            InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0) == 4 &&
            g_postReleaseCameraVerificationGeneration == g_mapGeneration;
        const bool runtimeTransitionVerification =
            origin == CameraAcquisitionOrigin::RuntimeTransitionAutomatic;
        if (postReleaseVerification) {
            ResetPostReleaseCameraVerificationCycle();
        }
        char line[448]{};
        wsprintfA(
            line,
            postReleaseVerification
                ? "Post-release camera engine observation failed: %s; keeping the applied provisional basis and retrying after the next release."
                : (runtimeTransitionVerification
                    ? "Post-first-move runtime camera observation failed: %s; keeping the current basis and leaving release-triggered confirmation available."
                    : "Camera recalibration failed: %s; keeping the previous movement basis."),
            reason);
        LogLine(line);
        return;
    }

    SetProvisionalDefaultCameraBasis(reason);
    char line[448]{};
    wsprintfA(
        line,
        failedInitialGate
            ? "Initial PlayerCam observation timed out without a valid engine basis: %s; keyboard movement gate released with provisional -45-degree movement, and release-triggered observations remain active until confidence reaches 2/2."
            : "Automatic PlayerCam detection produced no valid engine basis: %s; provisional -45-degree movement remains active until two concordant engine observations are obtained.",
        reason);
    LogLine(line);
}

void StartF9EquivalentCameraRecalibration(
    DWORD now,
    bool postReleaseVerification) {
    const DWORD acquireScene = ++g_sceneGeneration;
    g_playerCamera = nullptr;
    g_cameraAcquireActive = true;
    InterlockedExchange(
        &g_cameraAcquireOrigin,
        static_cast<LONG>(postReleaseVerification
            ? CameraAcquisitionOrigin::PostReleaseAutomatic
            : CameraAcquisitionOrigin::Manual));
    g_cameraAcquireScene = acquireScene;
    g_cameraAcquireActiveLevel = 0;
    g_cameraAcquireRequiresLevelIdentity = false;
    g_cameraAcquireAttempts = 0;
    g_cameraAcquireWaitingForContextLogged = false;
    g_cameraAcquirePreserveBasisOnFailure = true;
    g_cameraAcquireBlocksMovement =
        postReleaseVerification &&
        InterlockedCompareExchange(&g_blockKeyboardDuringPostReleaseCameraVerification, 0, 0) != 0;
    g_cameraCandidateForwardX = 0.0f;
    g_cameraCandidateForwardZ = 0.0f;
    g_cameraCandidateGameplayAngle = 0.0f;
    g_cameraCandidateStableSamples = 0;
    g_lastCameraSyncWaitLogTick = 0;
    g_cameraAcquireDueTick = now;
    g_cameraAcquireUntilTick = now + (postReleaseVerification
        ? static_cast<DWORD>(InterlockedCompareExchange(
            &g_postReleaseCameraVerificationTimeoutMs,
            0,
            0))
        : kCameraAcquireWindowMs);

    char line[320]{};
    wsprintfA(
        line,
        postReleaseVerification
            ? "Post-release isolated camera recalibration queued: scene=0x%08X retry=%lu ms timeout=%lu ms."
            : "Manual camera recalibration queued from overlay: scene=0x%08X retry=%lu ms timeout=%lu ms.",
        acquireScene,
        kCameraAcquireRetryMs,
        g_cameraAcquireUntilTick - now);
    LogLine(line);
}

void RequestManualCameraRecalibration(DWORD now) {
    const LONG stage = InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0);
    if (stage > 0 && stage < 5 &&
        g_postReleaseCameraVerificationGeneration == g_mapGeneration) {
        CancelAutomaticPostReleaseCameraVerificationForInput(
            "manual F9 recalibration superseded the pending automatic observation");
    } else {
        ResetPostReleaseCameraVerificationCycle();
    }
    StartF9EquivalentCameraRecalibration(now, false);
}

void StartPostReleaseCameraRecalibration(DWORD now) {
    const KeyboardInputSnapshot input = CaptureKeyboardInputSnapshot();
    if (input.moveMask != 0 ||
        !input.gameFocusedForInput ||
        InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0) {
        return;
    }
    if (g_postReleaseCameraVerificationGeneration != g_mapGeneration ||
        InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 4, 3) != 3) {
        return;
    }

    g_postReleaseCameraVerificationBaselineAngle = g_lastCameraGameplayAngle;
    StartF9EquivalentCameraRecalibration(now, true);
}

float CameraAngleDistance(float left, float right) {
    float difference = fabsf(left - right);
    if (difference > 180.0f) {
        difference = 360.0f - difference;
    }
    return difference;
}

const char* CameraAcquisitionOriginName(CameraAcquisitionOrigin origin) {
    switch (origin) {
        case CameraAcquisitionOrigin::InitialAutomatic: return "initial-loading";
        case CameraAcquisitionOrigin::PostReleaseAutomatic: return "post-release";
        case CameraAcquisitionOrigin::RuntimeTransitionAutomatic: return "post-first-move-runtime";
        case CameraAcquisitionOrigin::Manual: return "manual-F9";
        default: return "unknown";
    }
}

LONG RecordEngineCameraObservation(
    float gameplayAngle,
    DWORD arg1,
    CameraAcquisitionOrigin origin) {
    if (origin == CameraAcquisitionOrigin::InitialAutomatic) {
        g_cameraEngineObservationArg1 = arg1;
        g_cameraEngineObservationGeneration = g_mapGeneration;
        InterlockedExchange(&g_cameraBasisProvisional, 1);
        const LONG observations = InterlockedCompareExchange(
            &g_cameraEngineConcordantObservations, 0, 0);
        char loadingLine[448]{};
        sprintf_s(
            loadingLine,
            sizeof(loadingLine),
            "Loading-context PlayerCam observation applied but not counted: angle=%.2f arg1=0x%08X confidence=%ld/2 state=awaiting-post-first-move-runtime-context.",
            gameplayAngle,
            arg1,
            observations);
        LogLine(loadingLine);
        return observations;
    }

    if (g_cameraEngineObservationGeneration != g_mapGeneration) {
        InterlockedExchange(&g_cameraEngineConcordantObservations, 0);
        g_cameraEngineReferenceAngle = gameplayAngle;
        g_cameraEngineObservationArg1 = arg1;
        g_cameraEngineObservationGeneration = g_mapGeneration;
    }

    const float toleranceDegrees = static_cast<float>(
        InterlockedCompareExchange(&g_cameraSyncAngleToleranceX100, 0, 0)) / 100.0f;
    LONG observations = InterlockedCompareExchange(
        &g_cameraEngineConcordantObservations, 0, 0);
    const float previousReference = g_cameraEngineReferenceAngle;
    const bool concordant = observations > 0 &&
        CameraAngleDistance(previousReference, gameplayAngle) <= toleranceDegrees;

    if (observations <= 0) {
        observations = 1;
        g_cameraEngineReferenceAngle = gameplayAngle;
    } else if (concordant) {
        if (observations < kRequiredConcordantEngineCameraObservations) {
            ++observations;
        }
        g_cameraEngineReferenceAngle = gameplayAngle;
    } else {
        char mismatchLine[416]{};
        sprintf_s(
            mismatchLine,
            sizeof(mismatchLine),
            "Camera engine observation changed before confirmation: previousReference=%.2f newAngle=%.2f difference=%.2f deg source=%s; new value applied immediately and confirmation restarts at 1/2.",
            previousReference,
            gameplayAngle,
            CameraAngleDistance(previousReference, gameplayAngle),
            CameraAcquisitionOriginName(origin));
        LogLine(mismatchLine);
        observations = 1;
        g_cameraEngineReferenceAngle = gameplayAngle;
    }

    g_cameraEngineObservationArg1 = arg1;
    g_cameraEngineObservationGeneration = g_mapGeneration;
    InterlockedExchange(&g_cameraEngineConcordantObservations, observations);
    InterlockedExchange(
        &g_cameraBasisProvisional,
        observations >= kRequiredConcordantEngineCameraObservations ? 0 : 1);

    char line[448]{};
    sprintf_s(
        line,
        sizeof(line),
        "Camera engine observation accepted: source=%s angle=%.2f arg1=0x%08X concordant=%ld/2 state=%s; engine basis is already active.",
        CameraAcquisitionOriginName(origin),
        gameplayAngle,
        arg1,
        observations,
        observations >= kRequiredConcordantEngineCameraObservations
            ? "confirmed"
            : "provisional-awaiting-concordance");
    LogLine(line);
    return observations;
}

bool ShouldUseContinuousGameplayBasis(float visualAngle) {
    return CameraAngleDistance(visualAngle, -20.0f) <= 2.5f;
}

void PollPlayerCameraOrientation(DWORD now) {
    if (!g_cameraAcquireActive) {
        return;
    }

    if (IsAutomaticPostReleaseCameraAcquisitionActive()) {
        const KeyboardInputSnapshot input = CaptureKeyboardInputSnapshot();
        if (input.moveMask != 0 ||
            !input.gameFocusedForInput ||
            InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0) {
            CancelAutomaticPostReleaseCameraVerificationForInput(
                input.moveMask != 0
                    ? "a movement key was pressed during camera sampling"
                    : "the isolated gameplay state ended during camera sampling");
            return;
        }
    }

    if (IsStabilityFallbackActive(now)) {
        const bool postReleaseVerification =
            g_cameraAcquirePreserveBasisOnFailure &&
            InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0) == 4 &&
            g_postReleaseCameraVerificationGeneration == g_mapGeneration;
        g_cameraAcquireActive = false;
        g_cameraAcquirePreserveBasisOnFailure = false;
        g_cameraAcquireBlocksMovement = false;
        InterlockedExchange(
            &g_cameraAcquireOrigin,
            static_cast<LONG>(CameraAcquisitionOrigin::None));
        if (postReleaseVerification) {
            ResetPostReleaseCameraVerificationCycle();
            LogLine("Post-release camera observation suspended by stability fallback; previous basis kept and the next release will retry.");
        } else {
            LogLine("Camera acquisition suspended: stability fallback is active; keeping current movement basis.");
        }
        return;
    }

    if (g_cameraAcquireRequiresLevelIdentity &&
        (g_cameraAcquireScene != g_lastDiagnosticScene ||
            g_cameraAcquireActiveLevel != g_lastDiagnosticActiveLevel)) {
        InvalidateCameraForTransition();
        return;
    }

    if (static_cast<LONG>(now - g_cameraAcquireDueTick) < 0) {
        return;
    }

    if (g_cameraAcquireAttempts >= kCameraAcquireMaxAttempts ||
        static_cast<LONG>(now - g_cameraAcquireUntilTick) >= 0) {
        g_cameraAcquireActive = false;
        UseFixedCameraFallback("post-load camera acquisition expired");
        return;
    }

    ++g_cameraAcquireAttempts;
    g_cameraAcquireDueTick = now + static_cast<DWORD>(
        InterlockedCompareExchange(&g_cameraSyncSampleIntervalMs, 0, 0));

    if (!ResolveOgreCameraApi(now)) {
        if (g_cameraAcquireAttempts >= kCameraAcquireMaxAttempts) {
            g_cameraAcquireActive = false;
            UseFixedCameraFallback("OGRE camera API unavailable after bounded acquisition");
        }
        return;
    }

    tl2_camera_math::Quaternion orientation{};
    CameraReadFailure failure = CameraReadFailure::None;
    if (!TryReadPlayerCameraOrientation(&orientation, &failure)) {
        if (failure == CameraReadFailure::GetterException ||
            g_cameraAcquireAttempts >= kCameraAcquireMaxAttempts) {
            g_cameraAcquireActive = false;
            UseFixedCameraFallback(CameraReadFailureName(failure));
        }
        return;
    }

    float visualForwardX = 0.0f;
    float visualForwardZ = 0.0f;
    if (!tl2_camera_math::TryGetForwardXZ(orientation, &visualForwardX, &visualForwardZ)) {
        g_cameraAcquireActive = false;
        UseFixedCameraFallback("PlayerCam quaternion rejected");
        return;
    }

    float gameplayUpX = 0.0f;
    float gameplayUpZ = 0.0f;
    if (!tl2_camera_math::TrySelectIsometricGameplayBasis(
            orientation,
            &gameplayUpX,
            &gameplayUpZ)) {
        g_cameraAcquireActive = false;
        UseFixedCameraFallback("PlayerCam screen axes rejected");
        return;
    }

    constexpr float kRadiansToDegrees = 57.29577951308232f;
    const float visualAngle = atan2f(visualForwardX, visualForwardZ) * kRadiansToDegrees;
    const bool useContinuousGameplayBasis = ShouldUseContinuousGameplayBasis(visualAngle);
    if (useContinuousGameplayBasis) {
        gameplayUpX = visualForwardX;
        gameplayUpZ = visualForwardZ;
    }

    const float gameplayAngle = atan2f(gameplayUpX, gameplayUpZ) * kRadiansToDegrees;
    const LONG stableSamplesRequired =
        InterlockedCompareExchange(&g_cameraSyncStableSamplesRequired, 0, 0);
    const float toleranceDegrees = static_cast<float>(
        InterlockedCompareExchange(&g_cameraSyncAngleToleranceX100, 0, 0)) / 100.0f;

    if (g_cameraCandidateStableSamples > 0 &&
        CameraAngleDistance(g_cameraCandidateGameplayAngle, gameplayAngle) <= toleranceDegrees) {
        ++g_cameraCandidateStableSamples;
    } else {
        g_cameraCandidateStableSamples = 1;
    }
    g_cameraCandidateForwardX = gameplayUpX;
    g_cameraCandidateForwardZ = gameplayUpZ;
    g_cameraCandidateGameplayAngle = gameplayAngle;

    if (g_cameraCandidateStableSamples < stableSamplesRequired) {
        if (now - g_lastCameraSyncWaitLogTick >= 250) {
            g_lastCameraSyncWaitLogTick = now;
            char waitLine[320]{};
            sprintf_s(
                waitLine,
                sizeof(waitLine),
                "Camera synchronization waiting for stable PlayerCam basis: angle=%.2f sample=%ld/%ld tolerance=%.2f deg.",
                gameplayAngle,
                g_cameraCandidateStableSamples,
                stableSamplesRequired,
                toleranceDegrees);
            LogLine(waitLine);
        }
        return;
    }

    const CameraAcquisitionOrigin pendingOrigin = static_cast<CameraAcquisitionOrigin>(
        InterlockedCompareExchange(&g_cameraAcquireOrigin, 0, 0));
    if (pendingOrigin == CameraAcquisitionOrigin::RuntimeTransitionAutomatic &&
        g_runtimeTransitionCameraStartedTick != 0) {
        const DWORD sameAngleHoldMs = static_cast<DWORD>(
            InterlockedCompareExchange(&g_runtimeTransitionSameAngleHoldMs, 0, 0));
        const DWORD runtimeAgeMs = now - g_runtimeTransitionCameraStartedTick;
        const bool stillMatchesPreTransitionBasis =
            CameraAngleDistance(
                g_runtimeTransitionCameraBaselineAngle,
                g_cameraCandidateGameplayAngle) <= toleranceDegrees;
        if (stillMatchesPreTransitionBasis && runtimeAgeMs < sameAngleHoldMs) {
            if (now - g_lastCameraSyncWaitLogTick >= 250) {
                g_lastCameraSyncWaitLogTick = now;
                char holdLine[416]{};
                sprintf_s(
                    holdLine,
                    sizeof(holdLine),
                    "Runtime-transition PlayerCam still matches the pre-transition basis: angle=%.2f baseline=%.2f age=%lu/%lu ms; acceptance is held briefly so a delayed engine camera promotion can appear.",
                    g_cameraCandidateGameplayAngle,
                    g_runtimeTransitionCameraBaselineAngle,
                    runtimeAgeMs,
                    sameAngleHoldMs);
                LogLine(holdLine);
            }
            return;
        }
    }

    gameplayUpX = g_cameraCandidateForwardX;
    gameplayUpZ = g_cameraCandidateForwardZ;

    if (IsAutomaticPostReleaseCameraAcquisitionActive()) {
        const KeyboardInputSnapshot input = CaptureKeyboardInputSnapshot();
        if (input.moveMask != 0 ||
            !input.gameFocusedForInput ||
            InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0) {
            CancelAutomaticPostReleaseCameraVerificationForInput(
                "input changed before the final stable camera sample could be committed");
            return;
        }
    }

    const bool hasStableMoveContext =
        InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) != 0;
    if (!useContinuousGameplayBasis && !hasStableMoveContext) {
        if (!g_cameraAcquireWaitingForContextLogged) {
            g_cameraAcquireWaitingForContextLogged = true;
            char waitLine[256]{};
            sprintf_s(
                waitLine,
                sizeof(waitLine),
                "PlayerCam provisional isometric basis ignored until player context is stable: visualAngle=%.2f attempts=%d.",
                visualAngle,
                g_cameraAcquireAttempts);
            LogLine(waitLine);
        }
        return;
    }

    const CameraAcquisitionOrigin acquisitionOrigin = static_cast<CameraAcquisitionOrigin>(
        InterlockedExchange(
            &g_cameraAcquireOrigin,
            static_cast<LONG>(CameraAcquisitionOrigin::None)));
    if (acquisitionOrigin == CameraAcquisitionOrigin::RuntimeTransitionAutomatic) {
        g_runtimeTransitionCameraStartedTick = 0;
    }
    const bool releasingInitialEngineGate =
        acquisitionOrigin == CameraAcquisitionOrigin::InitialAutomatic &&
        g_cameraAcquireBlocksMovement;
    const bool completedPostReleaseVerification =
        acquisitionOrigin == CameraAcquisitionOrigin::PostReleaseAutomatic &&
        InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0) == 4 &&
        g_postReleaseCameraVerificationGeneration == g_mapGeneration;
    const bool completedRuntimeTransitionVerification =
        acquisitionOrigin == CameraAcquisitionOrigin::RuntimeTransitionAutomatic;
    const float previousGameplayAngle = g_lastCameraGameplayAngle;

    // Apply the first valid engine value immediately. Confirmation only controls
    // whether future release-triggered tests stop; it never delays the correction.
    g_cameraForwardX = gameplayUpX;
    g_cameraForwardZ = gameplayUpZ;
    g_cameraBasisAvailable = true;
    g_cameraModeInitialized = true;
    g_cameraAcquireActive = false;
    g_cameraAcquirePreserveBasisOnFailure = false;
    g_cameraAcquireBlocksMovement = false;
    g_cameraCandidateStableSamples = 0;
    g_cameraLatchedScene = g_cameraAcquireScene;
    g_cameraLatchedActiveLevel = g_cameraAcquireActiveLevel;
    g_lastCameraVisualAngle = visualAngle;
    g_lastCameraGameplayAngle = gameplayAngle;
    InterlockedExchange(&g_cameraGameplayMode, useContinuousGameplayBasis ? 2 : 1);

    LONG engineObservations = InterlockedCompareExchange(
        &g_cameraEngineConcordantObservations, 0, 0);
    if (acquisitionOrigin != CameraAcquisitionOrigin::None) {
        engineObservations = RecordEngineCameraObservation(
            gameplayAngle,
            g_mapLockedArg1,
            acquisitionOrigin);
    }
    if (releasingInitialEngineGate) {
        char gateLine[384]{};
        sprintf_s(
            gateLine,
            sizeof(gateLine),
            "Provisional loading-camera observation accepted: angle=%.2f runtimeConfidence=%ld/2; optional keyboard movement gate released immediately.",
            gameplayAngle,
            engineObservations);
        LogLine(gateLine);
    }

    if (completedRuntimeTransitionVerification) {
        char runtimeLine[512]{};
        sprintf_s(
            runtimeLine,
            sizeof(runtimeLine),
            "Post-first-move runtime PlayerCam observation complete while movement remained available: previousAngle=%.2f engineAngle=%.2f correction=%.2f deg arg1=0x%08X confidence=%ld/2.",
            previousGameplayAngle,
            gameplayAngle,
            CameraAngleDistance(previousGameplayAngle, gameplayAngle),
            g_mapLockedArg1,
            engineObservations);
        LogLine(runtimeLine);
        if (engineObservations >= kRequiredConcordantEngineCameraObservations) {
            ResetPostReleaseCameraVerificationCycle();
            InterlockedExchange(&g_postReleaseCameraVerificationStage, 5);
            LogLine("Camera basis confirmed by the runtime-transition observation; no release-triggered camera test remains active for this context.");
        } else {
            const LONG pendingReleaseStage = InterlockedCompareExchange(
                &g_postReleaseCameraVerificationStage, 0, 0);
            if (pendingReleaseStage == 4) {
                ResetPostReleaseCameraVerificationCycle();
            }
            LogLine(pendingReleaseStage >= 1 && pendingReleaseStage <= 3
                ? "Runtime-transition camera axis applied immediately at confidence 1/2; the release already in progress may provide the concordant confirmation."
                : "Runtime-transition camera axis applied immediately at confidence 1/2; the next completed movement release will provide the concordant confirmation.");
        }
    } else if (completedPostReleaseVerification) {
        char verificationLine[448]{};
        sprintf_s(
            verificationLine,
            sizeof(verificationLine),
            "Post-release isolated PlayerCam observation complete: previousAngle=%.2f engineAngle=%.2f correction=%.2f deg confidence=%ld/2.",
            previousGameplayAngle,
            gameplayAngle,
            CameraAngleDistance(previousGameplayAngle, gameplayAngle),
            engineObservations);
        LogLine(verificationLine);
        if (engineObservations >= kRequiredConcordantEngineCameraObservations) {
            InterlockedExchange(&g_postReleaseCameraVerificationStage, 5);
            LogLine("Camera basis confirmed by two concordant engine observations; release-triggered camera testing stopped for this movement context.");
        } else {
            ResetPostReleaseCameraVerificationCycle();
            LogLine("Camera basis remains provisional after one valid engine observation; the next completed movement release will request another bounded observation.");
        }
    } else if (acquisitionOrigin == CameraAcquisitionOrigin::Manual &&
        engineObservations < kRequiredConcordantEngineCameraObservations) {
        ResetPostReleaseCameraVerificationCycle();
    }
    char line[512]{};
    sprintf_s(
        line,
        sizeof(line),
        "PlayerCam basis latched for level: camera=0x%08X scene=0x%08X activeLevel=0x%08X visualForwardXZ=(%.6f,%.6f) visualAngle=%.2f gameplayMode=%s gameplayUpXZ=(%.6f,%.6f) gameplayAngle=%.2f attempts=%d confidence=%ld/2 provisional=%ld.",
        reinterpret_cast<DWORD>(g_playerCamera),
        g_cameraLatchedScene,
        g_cameraLatchedActiveLevel,
        visualForwardX,
        visualForwardZ,
        visualAngle,
        useContinuousGameplayBasis ? "continuous-special" : "isometric",
        gameplayUpX,
        gameplayUpZ,
        gameplayAngle,
        g_cameraAcquireAttempts,
        InterlockedCompareExchange(&g_cameraEngineConcordantObservations, 0, 0),
        InterlockedCompareExchange(&g_cameraBasisProvisional, 0, 0));
    LogLine(line);
    MarkMapRenderReadyFromPlayerCamLatch(GetTickCount(), "PlayerCam basis latched after load signal");
    TryLockCurrentMapContext("PlayerCam basis latched for current map", GetTickCount());
}

bool KeyboardMaskToWorldVector(int mask, float* dxOut, float* dzOut) {
    int keyX = 0;
    int keyY = 0;
    if ((mask & 2) != 0) {
        --keyX;
    }
    if ((mask & 8) != 0) {
        ++keyX;
    }
    if ((mask & 1) != 0) {
        --keyY;
    }
    if ((mask & 4) != 0) {
        ++keyY;
    }

    if (keyX == 0 && keyY == 0) {
        return false;
    }

    if (g_cameraBasisAvailable) {
        const int forwardAxis = -keyY;
        const int rightAxis = keyX;
        if (tl2_camera_math::TryComposeMovementVector(
                g_cameraForwardX,
                g_cameraForwardZ,
                forwardAxis,
                rightAxis,
                dxOut,
                dzOut)) {
            return true;
        }
    }

    const int worldX = keyY - keyX;
    const int worldZ = -(keyX + keyY);

    if (worldX == 0 && worldZ == 0) {
        return false;
    }

    const float dx = static_cast<float>(worldX);
    const float dz = static_cast<float>(worldZ);
    const float length = sqrtf(dx * dx + dz * dz);
    if (length <= 0.0f) {
        return false;
    }

    *dxOut = dx / length;
    *dzOut = dz / length;
    return true;
}

bool ResolveInteractionTargetClearApi() {
    const LONG state = InterlockedCompareExchange(&g_interactionTargetApiState, 0, 0);
    if (state > 0) return true;
    if (state < 0) return false;

    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) {
        InterlockedExchange(&g_interactionTargetApiState, -1);
        LogLine("Interaction-facing reset disabled: Torchlight2.exe module handle unavailable.");
        return false;
    }

    BYTE* primary = reinterpret_cast<BYTE*>(exe) + kPrimaryActorTargetSetterOffset;
    BYTE* secondary = reinterpret_cast<BYTE*>(exe) + kSecondaryActorTargetSetterOffset;
    const BYTE expectedPrimary[] = {0x55, 0x8B, 0x6C, 0x24, 0x08, 0x56, 0x57, 0x8B, 0xF1};
    const BYTE expectedSecondary[] = {0x56, 0x57, 0x6A, 0x1C, 0x8B, 0xF1};
    if (memcmp(primary, expectedPrimary, sizeof(expectedPrimary)) != 0 ||
        memcmp(secondary, expectedSecondary, sizeof(expectedSecondary)) != 0) {
        InterlockedExchange(&g_interactionTargetApiState, -1);
        LogLine("Interaction-facing reset disabled: target-clear function bytes do not match the Steam executable.");
        return false;
    }

    g_setPrimaryActorTarget = reinterpret_cast<SetActorTargetFn>(primary);
    g_setSecondaryActorTarget = reinterpret_cast<SetActorTargetFn>(secondary);
    InterlockedExchange(&g_interactionTargetApiState, 1);
    LogLine("Interaction-facing reset API resolved: vanilla primary/secondary actor target clear functions ready.");
    return true;
}

bool ClearVanillaInteractionFacingTarget(void* self) {
    if (self == nullptr ||
        InterlockedCompareExchange(&g_resetInteractionFacingOnKeyboardStart, 0, 0) == 0) {
        return true;
    }
    if (!ResolveInteractionTargetClearApi()) {
        return false;
    }

    __try {
        // Torchlight II's normal next-click path calls these two setters with
        // nullptr before assigning a new click target. Reusing only that target
        // clearing step preserves the natural NPC-facing animation while the
        // interaction is active, then removes its persistent look-at target as
        // soon as keyboard movement actually begins.
        g_setPrimaryActorTarget(self, nullptr);
        if (InterlockedCompareExchange(&g_clearSecondaryInteractionTarget, 0, 0) != 0) {
            g_setSecondaryActorTarget(self, nullptr);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_interactionTargetApiState, -1);
        InterlockedExchange(&g_resetInteractionFacingOnKeyboardStart, 0);
        LogLine("Interaction-facing reset disabled after a trapped engine exception; keyboard movement continues unchanged.");
        return false;
    }

    InterlockedIncrement(&g_keyboardFacingResetCount);
    if (InterlockedExchange(&g_keyboardFacingResetLogged, 1) == 0) {
        LogLine("Keyboard movement cleared the previous vanilla interaction target; final idle facing now follows keyboard movement.");
    }
    return true;
}

void ResetPendingMovementQueue() {
    AcquireSRWLockExclusive(&g_pendingMovementLock);
    g_pendingMovementKind = PendingMovementKind::None;
    g_pendingMovementMask = 0;
    g_pendingMovementStepX100 = 0;
    g_pendingMovementQueuedTick = 0;
    ReleaseSRWLockExclusive(&g_pendingMovementLock);
    InterlockedExchange(&g_pendingMovementAvailable, 0);
    InterlockedExchange(&g_moveMessagePosted, 0);
}

void QueueGameThreadMovement(PendingMovementKind kind, int mask, LONG stepX100) {
    if (InterlockedCompareExchange(&g_queueLatestInputOnly, 0, 0) == 0 &&
        InterlockedCompareExchange(&g_pendingMovementAvailable, 1, 1) != 0) {
        return;
    }

    AcquireSRWLockExclusive(&g_pendingMovementLock);
    g_pendingMovementKind = kind;
    g_pendingMovementMask = mask;
    g_pendingMovementStepX100 = stepX100;
    g_pendingMovementQueuedTick = GetTickCount();
    ++g_pendingMovementSequence;
    ReleaseSRWLockExclusive(&g_pendingMovementLock);
    InterlockedExchange(&g_pendingMovementAvailable, 1);

    if (InterlockedCompareExchange(&g_moveMessagePosted, 1, 0) != 0) {
        return;
    }
    if (!InstallGameWindowDispatchHook() ||
        !PostMessageW(g_dispatchGameWindow, kGameThreadMoveMessage, 0, 0)) {
        InterlockedExchange(&g_moveMessagePosted, 0);
    }
}

bool GetEventLockedPlayerContext(void** selfOut, DWORD* arg1Out) {
    if (selfOut == nullptr || arg1Out == nullptr) return false;
    *selfOut = nullptr;
    *arg1Out = 0;
    if (InterlockedCompareExchange(&g_eventCaptureState, 0, 0) !=
        static_cast<LONG>(EventCaptureState::Locked)) return false;
    if (g_mapLockedGeneration == 0 || g_mapLockedGeneration != g_mapGeneration ||
        g_mapLockedController == nullptr || g_mapLockedSelf == nullptr) return false;

    void* player = nullptr;
    DWORD link = 0;
    DWORD currentArg1 = 0;
    if (InterlockedCompareExchange(&g_resolveArg1AtDispatch, 0, 0) != 0) {
        if (!ReadDirectPlayerContextFromController(g_mapLockedController, &player, &link, &currentArg1)) return false;
        if (player != g_mapLockedSelf) return false;
    } else {
        player = g_mapLockedSelf;
        currentArg1 = g_mapLockedArg1;
    }

    if (!LooksLikeUsableStoredPlayerSelf(player) || !LooksLikeReadableLocalMoveArg1(currentArg1)) return false;

    if (currentArg1 != g_mapLockedArg1) {
        const DWORD now = GetTickCount();
        if (g_dynamicArg1Candidate == currentArg1) {
            ++g_dynamicArg1CandidateStableReads;
        } else {
            g_dynamicArg1Candidate = currentArg1;
            g_dynamicArg1CandidateFirstTick = now;
            g_dynamicArg1CandidateStableReads = 1;
        }

        const LONG requiredReads = InterlockedCompareExchange(&g_dynamicArg1StableReadsRequired, 0, 0);
        const DWORD settleMs = static_cast<DWORD>(InterlockedCompareExchange(&g_dynamicArg1SettleMs, 0, 0));
        const bool stableEnough = g_dynamicArg1CandidateStableReads >= requiredReads;
        const bool settledLongEnough = settleMs == 0 || now - g_dynamicArg1CandidateFirstTick >= settleMs;
        if (!stableEnough || !settledLongEnough) {
            if (now - g_lastDynamicArg1LogTick >= 250) {
                g_lastDynamicArg1LogTick = now;
                char line[384]{};
                wsprintfA(line,
                    "Dynamic LocalMove arg1 transition settling: self=0x%08X accepted=0x%08X candidate=0x%08X reads=%ld/%ld ageMs=%lu/%lu; movement tick skipped.",
                    reinterpret_cast<DWORD>(player),
                    g_mapLockedArg1,
                    currentArg1,
                    g_dynamicArg1CandidateStableReads,
                    requiredReads,
                    now - g_dynamicArg1CandidateFirstTick,
                    settleMs);
                LogLine(line);
            }
            return false;
        }

        AcquireSRWLockExclusive(&g_localMoveCaptureLock);
        g_playerLocalMoveSelf = player;
        g_playerLocalMoveArg1 = currentArg1;
        g_lastKnownPlayerLocalMoveSelf = player;
        g_lastKnownPlayerLocalMoveArg1 = currentArg1;
        ReleaseSRWLockExclusive(&g_localMoveCaptureLock);
        const DWORD previousArg1 = g_mapLockedArg1;
        g_mapLockedArg1 = currentArg1;
        g_dynamicArg1Candidate = 0;
        g_dynamicArg1CandidateFirstTick = 0;
        g_dynamicArg1CandidateStableReads = 0;
        if (now - g_lastDynamicArg1LogTick >= 100) {
            g_lastDynamicArg1LogTick = now;
            char line[352]{};
            wsprintfA(line,
                "Dynamic LocalMove arg1 accepted after settling: self=0x%08X old=0x%08X new=0x%08X link=0x%08X.",
                reinterpret_cast<DWORD>(player), previousArg1, currentArg1, link);
            LogLine(line);
        }
        MaybeRearmCameraVerificationAfterRuntimeArg1Change(previousArg1, currentArg1);
    } else {
        g_dynamicArg1Candidate = 0;
        g_dynamicArg1CandidateFirstTick = 0;
        g_dynamicArg1CandidateStableReads = 0;
    }

    *selfOut = player;
    *arg1Out = currentArg1;
    return true;
}

void TryAdvancePostReleaseCameraVerificationOnGameThread() {
    const LONG stage = InterlockedCompareExchange(&g_postReleaseCameraVerificationStage, 0, 0);
    if ((stage != 1 && stage != 2) ||
        InterlockedCompareExchange(&g_currentKeyboardMoveMask, 0, 0) != 0 ||
        !IsGameWindowForegroundForInput() ||
        g_postReleaseCameraVerificationGeneration != g_mapGeneration ||
        g_mapLockedGeneration != g_mapGeneration) {
        return;
    }

    void* self = nullptr;
    DWORD arg1 = 0;
    if (!GetEventLockedPlayerContext(&self, &arg1)) {
        return;
    }

    const DWORD now = GetTickCount();
    if (stage == 1) {
        if (InterlockedCompareExchange(&g_releaseBrakeMode, 0, 0) == 1) {
            QueueGameThreadMovement(PendingMovementKind::ReleaseBrake, 0, 0);
        } else {
            MarkPostReleaseBrakeDispatched(now, arg1);
        }
        return;
    }

    if (g_postReleaseObservedArg1 == arg1) {
        ++g_postReleaseStableReads;
    } else {
        g_postReleaseObservedArg1 = arg1;
        g_postReleaseStableReads = 1;
    }

    const LONG requiredReads = InterlockedCompareExchange(
        &g_postReleaseStableReadsRequired,
        0,
        0);
    const DWORD quietMs = static_cast<DWORD>(
        InterlockedCompareExchange(&g_postReleaseQuietMs, 0, 0));
    const bool quietEnough =
        g_postReleaseBrakeSentTick != 0 &&
        now - g_postReleaseBrakeSentTick >= quietMs;
    if (g_postReleaseStableReads >= requiredReads && quietEnough) {
        SchedulePostReleaseCameraRecalibration(now, arg1);
    }
}

void ExecutePendingMovementOnGameWindowThread() {
    if (InterlockedExchange(&g_pendingMovementAvailable, 0) == 0) {
        return;
    }

    PendingMovementKind kind = PendingMovementKind::None;
    int mask = 0;
    LONG stepX100 = 0;
    DWORD queuedTick = 0;
    DWORD sequence = 0;
    AcquireSRWLockShared(&g_pendingMovementLock);
    kind = g_pendingMovementKind;
    mask = g_pendingMovementMask;
    stepX100 = g_pendingMovementStepX100;
    queuedTick = g_pendingMovementQueuedTick;
    sequence = g_pendingMovementSequence;
    ReleaseSRWLockShared(&g_pendingMovementLock);

    if (kind == PendingMovementKind::None || sequence == g_lastExecutedMovementSequence) {
        return;
    }
    g_lastExecutedMovementSequence = sequence;

    const DWORD now = GetTickCount();
    const DWORD currentThreadId = GetCurrentThreadId();
    if (InterlockedCompareExchange(&g_requireGameWindowThread, 0, 0) != 0 &&
        g_gameWindowThreadId != 0 &&
        currentThreadId != g_gameWindowThreadId) {
        if (now - g_lastDispatchSkipLogTick >= 1000) {
            g_lastDispatchSkipLogTick = now;
            LogLine("Queued movement rejected: handler is not running on the game-window thread.");
        }
        return;
    }

    const DWORD maxAge = static_cast<DWORD>(
        InterlockedCompareExchange(&g_queuedCommandMaxAgeMs, 0, 0));
    if (queuedTick == 0 || now - queuedTick > maxAge) {
        return;
    }
    if (IsTransitionLocalMoveBlocked(now) ||
        IsLocalMoveExceptionRecoveryBlocked(now) ||
        IsTransientContextHoldActive(now) ||
        !IsGameWindowForegroundForInput()) {
        return;
    }

    if (g_cameraAcquireBlocksMovement && g_cameraAcquireActive) {
        if (now - g_lastCameraSyncWaitLogTick >= 500) {
            g_lastCameraSyncWaitLogTick = now;
            LogLine("Queued movement paused briefly: waiting for the first valid PlayerCam engine observation (bounded initial gate).");
        }
        return;
    }

    if (kind == PendingMovementKind::Move &&
        InterlockedCompareExchange(&g_keyboardFacingResetPending, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_eventCaptureState, 0, 0) ==
            static_cast<LONG>(EventCaptureState::Locked) &&
        g_mapLockedGeneration == g_mapGeneration &&
        LooksLikeUsableStoredPlayerSelf(g_mapLockedSelf)) {
        // Clear the vanilla interaction target before resolving arg1 for this
        // dispatch. If target cleanup changes the movement-link object, the
        // normal dynamic-arg1 settling gate below observes the new value and
        // skips this tick rather than calling LocalMove with a stale pointer.
        ClearVanillaInteractionFacingTarget(g_mapLockedSelf);
        InterlockedExchange(&g_keyboardFacingResetPending, 0);
    }

    void* self = nullptr;
    DWORD arg1 = 0;
    if (!GetEventLockedPlayerContext(&self, &arg1)) {
        ArmEmergencyWasdBootstrap(
            now,
            false,
            "queued movement could not resolve any event-locked player ownership");
        ArmWasdOwnershipRecovery(
            now,
            "queued movement could not resolve the event-locked player ownership");
        PollWasdOwnershipRecoveryOnGameThread();
        if (now - g_lastDispatchSkipLogTick >= 1000) {
            g_lastDispatchSkipLogTick = now;
            if (InterlockedCompareExchange(&g_emergencyWasdBootstrapActive, 1, 1) != 0) {
                LogLine("Queued movement waiting: emergency WASD bootstrap is rebuilding the missing first playable context.");
            } else if (InterlockedCompareExchange(&g_wasdOwnershipRecoveryActive, 1, 1) != 0) {
                LogLine("Queued movement waiting: bounded WASD ownership recovery is active.");
            } else {
                LogLine("Queued movement waiting: no event-locked map context.");
            }
        }
        return;
    }

    // The old d1A8 "strict marker" is not a stable player-identity marker: skills,
    // NPC interactions and forced movement legitimately toggle it. It is optional now.
    // The default safety model is the UI-locked controller/player identity, a settled
    // dynamic arg1, execution on the game-window thread, and the engine dispatch-list
    // preflight that mirrors the exact loop which caused the previous access violation.
    if (InterlockedCompareExchange(&g_requireStrictPlayerMarkerAtDispatch, 0, 0) != 0 &&
        !LooksLikePlayerLocalMoveSelf(self)) {
        if (now - g_lastDispatchSkipLogTick >= 500) {
            g_lastDispatchSkipLogTick = now;
            char line[256]{};
            wsprintfA(
                line,
                "Queued movement paused: locked player strict marker is temporarily unavailable. self=0x%08X arg1=0x%08X; context kept.",
                reinterpret_cast<DWORD>(self),
                arg1);
            LogLine(line);
        }
        return;
    }

    char dispatchGuardReason[192]{};
    if (!LooksLikeSafeLocalMoveDispatchSelf(self, dispatchGuardReason, sizeof(dispatchGuardReason))) {
        if (now - g_lastLocalMoveDispatchGuardLogTick >= 500) {
            g_lastLocalMoveDispatchGuardLogTick = now;
            char line[448]{};
            wsprintfA(line,
                "Queued movement paused: self dispatch list is not safe (%s). self=0x%08X arg1=0x%08X; event lock kept.",
                dispatchGuardReason,
                reinterpret_cast<DWORD>(self),
                arg1);
            LogLine(line);
        }
        return;
    }

    float currentX = 0.0f;
    float currentZ = 0.0f;
    if (!ReadFloatSafe(self, 0x70, &currentX) ||
        !ReadFloatSafe(self, 0x78, &currentZ) ||
        currentX != currentX ||
        currentZ != currentZ ||
        currentX < -100000.0f || currentX > 100000.0f ||
        currentZ < -100000.0f || currentZ > 100000.0f) {
        return;
    }

    float targetX = currentX;
    float targetZ = currentZ;
    if (kind == PendingMovementKind::Move) {
        float dx = 0.0f;
        float dz = 0.0f;
        if (!KeyboardMaskToWorldVector(mask, &dx, &dz)) {
            return;
        }
        const float step = static_cast<float>(stepX100) / 100.0f;
        targetX += dx * step;
        targetZ += dz * step;
    }

    const DWORD targetXBits = FloatToBits(targetX);
    const DWORD targetZBits = FloatToBits(targetZ);
    g_lastInjectedLocalMoveTick = now;
    // The confirmed vanilla player callsite returning at 0x0042748E also pushes
    // zero for the two trailing LocalMove parameters. Mirroring 0,0 here is the
    // game's own normal click-movement ABI, not an unknown fallback guess.
    CallLocalMove(self, arg1, targetXBits, targetZBits, 0, 0);
    if (kind == PendingMovementKind::Move &&
        InterlockedCompareExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 0, 0) == 0) {
        const bool firstSuccessfulMove =
            g_lastSuccessfulKeyboardMoveGeneration != g_mapGeneration;
        g_lastSuccessfulKeyboardMoveGeneration = g_mapGeneration;
        if (firstSuccessfulMove) {
            ArmPostFirstMoveCameraTransitionWatch(now, arg1);
        }
    }
    if (kind == PendingMovementKind::ReleaseBrake &&
        InterlockedCompareExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 0, 0) == 0) {
        MarkPostReleaseBrakeDispatched(now, arg1);
    }
}

bool SendKeyboardMoveTarget(int mask, float step, const char* reason) {
    const DWORD now = GetTickCount();
    const DWORD minSendInterval =
        static_cast<DWORD>(InterlockedCompareExchange(&g_keyboardMoveMinSendIntervalMs, 0, 0));
    if (now - g_lastInjectedLocalMoveTick < minSendInterval) {
        return false;
    }

    void* self = nullptr;
    DWORD arg1 = 0;
    if (!GetLockedPlayerLocalMoveContext(&self, &arg1)) {
        RequestKeyboardMovementReacquire(now, "movement key held but player LocalMove context is not usable");
        return false;
    }

    float dx = 0.0f;
    float dz = 0.0f;
    if (!KeyboardMaskToWorldVector(mask, &dx, &dz)) {
        return false;
    }

    float currentX = 0.0f;
    float currentZ = 0.0f;
    if (!ReadFloatSafe(self, 0x70, &currentX) || !ReadFloatSafe(self, 0x78, &currentZ)) {
        if (IsCurrentMapContextLockedFor(self, arg1)) {
            EnterTransientContextHold(
                "Map-locked keyboard target skipped: current coordinates are temporarily unreadable",
                now);
            return false;
        }
        ClearPlayerLocalMoveContext("ZQSD move skipped: current player coordinates are unreadable.");
        return false;
    }
    if (currentX != currentX || currentZ != currentZ || currentX < -100000.0f || currentX > 100000.0f || currentZ < -100000.0f || currentZ > 100000.0f) {
        if (IsCurrentMapContextLockedFor(self, arg1)) {
            EnterTransientContextHold(
                "Map-locked keyboard target skipped: current coordinates are temporarily invalid",
                now);
            return false;
        }
        ClearPlayerLocalMoveContext("ZQSD move skipped: current player coordinates are invalid.");
        return false;
    }
    g_lastInjectedLocalMoveTick = now;
    SendLocalMoveTarget(self, arg1, currentX + dx * step, currentZ + dz * step, reason);
    if (InterlockedCompareExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 0, 0) == 0) {
        const bool firstSuccessfulMove =
            g_lastSuccessfulKeyboardMoveGeneration != g_mapGeneration;
        g_lastSuccessfulKeyboardMoveGeneration = g_mapGeneration;
        if (firstSuccessfulMove) {
            ArmPostFirstMoveCameraTransitionWatch(now, arg1);
        }
    }
    return true;
}

void SendKeyboardMove(int mask) {
    const LONG stepX100 = InterlockedCompareExchange(&g_keyboardMoveLeadStepX100, 0, 0);
    if (InterlockedCompareExchange(&g_dispatchOnGameWindowThread, 0, 0) != 0) {
        QueueGameThreadMovement(PendingMovementKind::Move, mask, stepX100);
        return;
    }
    if (InterlockedCompareExchange(&g_allowWorkerThreadLocalMove, 0, 0) == 0) {
        return;
    }
    const float step = static_cast<float>(stepX100) / 100.0f;
    SendKeyboardMoveTarget(mask, step, "ZQSD microstep");
}

void CancelPendingLocalMoveBrake() {
    InterlockedExchange(&g_localMoveBrakePending, 0);
    g_localMoveBrakePendingTick = 0;
}

void SendLocalMoveReleaseBrake() {
    if (InterlockedCompareExchange(&g_dispatchOnGameWindowThread, 0, 0) != 0) {
        QueueGameThreadMovement(PendingMovementKind::ReleaseBrake, 0, 0);
        return;
    }
    if (InterlockedCompareExchange(&g_allowWorkerThreadLocalMove, 0, 0) == 0) {
        return;
    }

    void* self = nullptr;
    DWORD arg1 = 0;
    if (!GetLockedPlayerLocalMoveContext(&self, &arg1)) {
        return;
    }

    float currentX = 0.0f;
    float currentZ = 0.0f;
    if (!ReadFloatSafe(self, 0x70, &currentX) ||
        !ReadFloatSafe(self, 0x78, &currentZ) ||
        currentX != currentX ||
        currentZ != currentZ ||
        currentX < -100000.0f ||
        currentX > 100000.0f ||
        currentZ < -100000.0f ||
        currentZ > 100000.0f) {
        return;
    }

    const DWORD now = GetTickCount();
    if (g_lastLocalMoveReleaseBrakeSentTick != 0 &&
        now - g_lastLocalMoveReleaseBrakeSentTick < kLocalMoveReleaseBrakeMinIntervalMs) {
        return;
    }

    if (g_lastLocalMoveReleaseBrakeSentTick != 0 &&
        g_lastLocalMoveReleaseBrakeSelf == self &&
        g_lastLocalMoveReleaseBrakeArg1 == arg1 &&
        now - g_lastLocalMoveReleaseBrakeSentTick < kLocalMoveReleaseBrakeDuplicateWindowMs) {
        const float dx = currentX - g_lastLocalMoveReleaseBrakeX;
        const float dz = currentZ - g_lastLocalMoveReleaseBrakeZ;
        if (dx * dx + dz * dz < kLocalMoveReleaseBrakeDuplicateDistanceSq) {
            return;
        }
    }

    g_lastLocalMoveReleaseBrakeSentTick = now;
    g_lastLocalMoveReleaseBrakeSelf = self;
    g_lastLocalMoveReleaseBrakeArg1 = arg1;
    g_lastLocalMoveReleaseBrakeX = currentX;
    g_lastLocalMoveReleaseBrakeZ = currentZ;
    g_lastInjectedLocalMoveTick = now;
    SendLocalMoveTarget(self, arg1, currentX, currentZ, "ZQSD release brake");
    if (InterlockedCompareExchange(&g_disableKeyboardInjectionAfterLocalMoveException, 0, 0) == 0) {
        MarkPostReleaseBrakeDispatched(now, arg1);
    }
}

void QueueLocalMoveReleaseBrake(DWORD now) {
    const LONG delayMs = InterlockedCompareExchange(&g_releaseBrakeDelayMs, 0, 0);
    if (delayMs <= 0) {
        SendLocalMoveReleaseBrake();
        return;
    }

    g_localMoveBrakePendingTick = now + static_cast<DWORD>(delayMs);
    InterlockedExchange(&g_localMoveBrakePending, 1);
}

void FirePendingLocalMoveBrakeIfNeeded(DWORD now) {
    if (InterlockedCompareExchange(&g_localMoveBrakePending, 1, 1) == 0) {
        return;
    }
    if (g_localMoveBrakePendingTick != 0 &&
        static_cast<LONG>(now - g_localMoveBrakePendingTick) < 0) {
        return;
    }

    InterlockedExchange(&g_localMoveBrakePending, 0);
    g_localMoveBrakePendingTick = 0;
    SendLocalMoveReleaseBrake();
}

void SendKeyboardReleaseBrake() {
    if constexpr (!kEnableReleaseBrake) {
        return;
    }

    const LONG mode = InterlockedCompareExchange(&g_releaseBrakeMode, 0, 0);
    if (mode == 1) {
        QueueLocalMoveReleaseBrake(GetTickCount());
    }
}

void SendKeyboardReleaseSettle(int mask) {
    if (!kEnableReleaseSettle) {
        (void)mask;
        return;
    }

    constexpr float kKeyboardReleaseSettleStep = 3.0f;
    const DWORD now = GetTickCount();
    if (now - g_lastReleaseSettleTick < 150) {
        return;
    }
    g_lastReleaseSettleTick = now;

    if (InterlockedCompareExchange(&g_dispatchOnGameWindowThread, 0, 0) != 0) {
        QueueGameThreadMovement(
            PendingMovementKind::Move,
            mask,
            static_cast<LONG>(kKeyboardReleaseSettleStep * 100.0f));
        return;
    }
    if (InterlockedCompareExchange(&g_allowWorkerThreadLocalMove, 0, 0) == 0) {
        return;
    }
    SendKeyboardMoveTarget(mask, kKeyboardReleaseSettleStep, "ZQSD release settle");
}

extern "C" __declspec(naked) void MessageSendProbeHook() {
    __asm {
        pushfd
        pushad
        mov eax, [esp + 18h]
        mov ebx, [esp + 28h]
        mov ecx, [esp + 24h]
        push ecx
        push ebx
        push eax
        call LogMessageProbeCall
        add esp, 0Ch
        popad
        popfd
        jmp dword ptr [g_messageProbeGateway]
    }
}

extern "C" __declspec(naked) void LocalMoveProbeHook() {
    __asm {
        pushfd
        pushad
        mov eax, [esp + 18h]
        mov esi, [esp + 0Ch]
        mov ebp, [esi + 1Ch]
        mov ebx, [esi + 08h]
        mov ecx, [esi + 0Ch]
        mov edx, [esi + 10h]
        mov edi, [esi + 14h]
        mov esi, [esi + 18h]
        push dword ptr [esp + 04h]
        push ebp
        push esi
        push edi
        push edx
        push ecx
        push ebx
        push eax
        call LogLocalMoveProbeCall
        add esp, 20h
        popad
        popfd
        jmp dword ptr [g_localMoveProbeGateway]
    }
}

extern "C" __declspec(naked) void StationaryClickFallbackHook() {
    __asm {
        cmp dword ptr [g_keyboardMousePatchEnabled], 0
        je originalDecision
        cmp dword ptr [g_disableClickMovement], 0
        je originalDecision
        cmp dword ptr [g_gameInputFocused], 0
        je originalDecision
        pushad
        call ShouldBypassStationaryClickFallbackForHoldPosition
        test eax, eax
        popad
        jne originalDecision
        lock inc dword ptr [g_stationaryFallbackActivations]
        xor bl, bl
    originalDecision:
        test bl, bl
        je stationaryAttack
        cmp byte ptr [esi + 52h], 0
        jmp dword ptr [g_stationaryFallbackContinueTarget]
    stationaryAttack:
        jmp dword ptr [g_stationaryFallbackAttackTarget]
    }
}

bool InstallRelativeJumpHook(void* target, void* hook, BYTE* original, DWORD length, void** gateway) {
    if (length < 5) {
        return false;
    }

    BYTE* targetBytes = static_cast<BYTE*>(target);
    for (DWORD i = 0; i < length; ++i) {
        original[i] = targetBytes[i];
    }

    BYTE* allocated = static_cast<BYTE*>(VirtualAlloc(nullptr, length + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (allocated == nullptr) {
        return false;
    }

    for (DWORD i = 0; i < length; ++i) {
        allocated[i] = original[i];
    }

    allocated[length] = 0xE9;
    *reinterpret_cast<DWORD*>(allocated + length + 1) =
        reinterpret_cast<DWORD>(targetBytes + length) - reinterpret_cast<DWORD>(allocated + length + 5);

    DWORD oldProtect = 0;
    if (!VirtualProtect(targetBytes, length, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    targetBytes[0] = 0xE9;
    *reinterpret_cast<DWORD*>(targetBytes + 1) =
        reinterpret_cast<DWORD>(hook) - reinterpret_cast<DWORD>(targetBytes + 5);
    for (DWORD i = 5; i < length; ++i) {
        targetBytes[i] = 0x90;
    }

    DWORD ignored = 0;
    VirtualProtect(targetBytes, length, oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), targetBytes, length);

    *gateway = allocated;
    return true;
}

void RestoreMapRenderReadyProbe() {
    if (InterlockedExchange(&g_d3d9ViewportReadyProbeInstalled, 0) == 0 ||
        g_d3d9UpdateViewportExport == nullptr) {
        return;
    }

    BYTE* target = static_cast<BYTE*>(g_d3d9UpdateViewportExport);
    DWORD oldProtect = 0;
    if (VirtualProtect(target, kD3D9UpdateViewportPatchLength, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        CopyMemory(target, g_d3d9UpdateViewportOriginal, kD3D9UpdateViewportPatchLength);
        FlushInstructionCache(GetCurrentProcess(), target, kD3D9UpdateViewportPatchLength);
        DWORD ignored = 0;
        VirtualProtect(target, kD3D9UpdateViewportPatchLength, oldProtect, &ignored);
    }
    if (g_d3d9UpdateViewportGateway != nullptr) {
        VirtualFree(g_d3d9UpdateViewportGateway, 0, MEM_RELEASE);
        g_d3d9UpdateViewportGateway = nullptr;
    }
}

void InstallMapRenderReadyProbe(DWORD now) {
    if (InterlockedCompareExchange(&g_enableMapRenderReadyProbe, 0, 0) == 0) {
        return;
    }

    if (InterlockedCompareExchange(&g_d3d9ViewportReadyProbeInstalled, 1, 1) != 0 ||
        InterlockedCompareExchange(&g_d3d9ViewportReadyProbeState, -1, -1) < 0) {
        return;
    }

    if (!ResolveOgreCameraApi(now)) {
        return;
    }

    HMODULE d3d9rs = GetModuleHandleW(L"RenderSystem_Direct3D9.dll");
    if (d3d9rs == nullptr) {
        return;
    }

    g_d3d9UpdateViewportExport = reinterpret_cast<void*>(
        GetProcAddress(d3d9rs, "?_updateViewport@D3D9RenderWindow@Ogre@@UAEXPAVViewport@2@_N@Z"));
    if (g_d3d9UpdateViewportExport == nullptr) {
        InterlockedExchange(&g_d3d9ViewportReadyProbeState, -1);
        LogLine("Map render ready probe disabled: D3D9RenderWindow::_updateViewport export missing.");
        return;
    }

    BYTE* target = static_cast<BYTE*>(g_d3d9UpdateViewportExport);
    if (memcmp(target, kExpectedD3D9UpdateViewportPrologue, kD3D9UpdateViewportPatchLength) != 0) {
        InterlockedExchange(&g_d3d9ViewportReadyProbeState, -1);
        char line[256]{};
        wsprintfA(
            line,
            "Map render ready probe disabled: D3D9RenderWindow::_updateViewport prologue mismatch %02X %02X %02X %02X %02X %02X %02X.",
            target[0],
            target[1],
            target[2],
            target[3],
            target[4],
            target[5],
            target[6]);
        LogLine(line);
        return;
    }

    if (!InstallRelativeJumpHook(
            g_d3d9UpdateViewportExport,
            reinterpret_cast<void*>(&D3D9RenderWindowUpdateViewportMapReadyHook),
            g_d3d9UpdateViewportOriginal,
            kD3D9UpdateViewportPatchLength,
            &g_d3d9UpdateViewportGateway)) {
        InterlockedExchange(&g_d3d9ViewportReadyProbeState, -1);
        LogLine("Map render ready probe disabled: D3D9 viewport hook install failed.");
        return;
    }

    InterlockedExchange(&g_d3d9ViewportReadyProbeInstalled, 1);
    InterlockedExchange(&g_d3d9ViewportReadyProbeState, 1);
    LogLine("Map render ready probe installed: D3D9RenderWindow::_updateViewport.");
}

void InstallStationaryClickFallback() {
    if (InterlockedExchange(&g_stationaryFallbackInstalled, 1) != 0) {
        return;
    }

    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) {
        LogLine("Stationary click fallback install failed: no executable module handle.");
        return;
    }

    BYTE* target = reinterpret_cast<BYTE*>(exe) + 0x2B1A9;
    const BYTE expected[] = {0x84, 0xDB, 0x74, 0xDE, 0x80, 0x7E, 0x52, 0x00};
    for (DWORD i = 0; i < sizeof(expected); ++i) {
        if (target[i] != expected[i]) {
            LogBytesAt("Stationary click fallback target bytes:", target, 12);
            LogLine("Stationary click fallback install skipped: target bytes do not match Steam executable.");
            return;
        }
    }

    for (DWORD i = 0; i < sizeof(g_stationaryFallbackOriginal); ++i) {
        g_stationaryFallbackOriginal[i] = target[i];
    }
    g_stationaryFallbackAttackTarget = reinterpret_cast<BYTE*>(exe) + 0x2B18B;
    g_stationaryFallbackContinueTarget = reinterpret_cast<BYTE*>(exe) + 0x2B1B1;

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, sizeof(g_stationaryFallbackOriginal), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LogLine("Stationary click fallback install failed: target protection could not be changed.");
        return;
    }

    target[0] = 0xE9;
    *reinterpret_cast<DWORD*>(target + 1) =
        reinterpret_cast<DWORD>(&StationaryClickFallbackHook) -
        reinterpret_cast<DWORD>(target + 5);
    for (DWORD i = 5; i < sizeof(g_stationaryFallbackOriginal); ++i) {
        target[i] = 0x90;
    }

    DWORD ignored = 0;
    VirtualProtect(target, sizeof(g_stationaryFallbackOriginal), oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(g_stationaryFallbackOriginal));
    LogLine("Stationary click fallback installed at Torchlight2.exe+0x2B1A9.");
}

void InstallMessageSendProbe() {
    if (InterlockedExchange(&g_messageProbeInstalled, 1) != 0) {
        return;
    }

    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) {
        LogLine("Transition message hook install failed: no executable module handle.");
        return;
    }

    BYTE* target = reinterpret_cast<BYTE*>(exe) + 0x25B2D0;
    const BYTE expectedPrefix[] = {0x6A, 0xFF, 0x68};
    const BYTE expectedSuffix[] = {0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x50, 0x64, 0x89};
    for (DWORD i = 0; i < sizeof(expectedPrefix); ++i) {
        if (target[i] != expectedPrefix[i]) {
            LogBytesAt("Transition message hook target bytes:", target, 16);
            LogLine("Transition message hook install skipped: target prefix does not match Steam executable.");
            return;
        }
    }
    for (DWORD i = 0; i < sizeof(expectedSuffix); ++i) {
        if (target[i + 7] != expectedSuffix[i]) {
            LogBytesAt("Transition message hook target bytes:", target, 16);
            LogLine("Transition message hook install skipped: target bytes do not match Steam executable.");
            return;
        }
    }

    if (InstallRelativeJumpHook(target, MessageSendProbeHook, g_messageProbeOriginal, 7, &g_messageProbeGateway)) {
        LogLine("Transition message hook installed at Torchlight2.exe+0x25B2D0.");
    } else {
        LogLine("Transition message hook install failed.");
    }
}

void InstallLocalMoveProbe() {
    if (InterlockedExchange(&g_localMoveProbeInstalled, 1) != 0) {
        return;
    }

    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) {
        LogLine("Local move probe install failed: no executable module handle.");
        return;
    }

    BYTE* target = reinterpret_cast<BYTE*>(exe) + 0x154AB0;
    const BYTE expected[] = {0x83, 0xEC, 0x74, 0x53, 0x55, 0x56};
    for (DWORD i = 0; i < sizeof(expected); ++i) {
        if (target[i] != expected[i]) {
            LogBytesAt("Local move hook target bytes:", target, 12);
            LogLine("Local move hook install skipped: target bytes do not match Steam executable.");
            return;
        }
    }

    if (InstallRelativeJumpHook(target, LocalMoveProbeHook, g_localMoveProbeOriginal, 6, &g_localMoveProbeGateway)) {
        LogLine("Local move hook installed at Torchlight2.exe+0x154AB0.");
    } else {
        LogLine("Local move hook install failed.");
    }
}

HICON CreateTrayConfigIcon() {
    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr) {
        return LoadIconW(nullptr, IDI_APPLICATION);
    }

    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP colorBitmap = CreateCompatibleBitmap(screenDc, 32, 32);
    HBITMAP maskBitmap = CreateBitmap(32, 32, 1, 1, nullptr);
    if (memoryDc == nullptr || colorBitmap == nullptr || maskBitmap == nullptr) {
        if (maskBitmap != nullptr) {
            DeleteObject(maskBitmap);
        }
        if (colorBitmap != nullptr) {
            DeleteObject(colorBitmap);
        }
        if (memoryDc != nullptr) {
            DeleteDC(memoryDc);
        }
        ReleaseDC(nullptr, screenDc);
        return LoadIconW(nullptr, IDI_APPLICATION);
    }

    HGDIOBJ oldBitmap = SelectObject(memoryDc, colorBitmap);
    RECT background{0, 0, 32, 32};
    HBRUSH backgroundBrush = CreateSolidBrush(RGB(16, 20, 26));
    FillRect(memoryDc, &background, backgroundBrush);
    DeleteObject(backgroundBrush);

    HBRUSH accentBrush = CreateSolidBrush(RGB(72, 190, 112));
    HPEN accentPen = CreatePen(PS_SOLID, 1, RGB(12, 108, 54));
    HGDIOBJ oldBrush = SelectObject(memoryDc, accentBrush);
    HGDIOBJ oldPen = SelectObject(memoryDc, accentPen);
    RoundRect(memoryDc, 3, 3, 29, 29, 7, 7);
    SelectObject(memoryDc, oldPen);
    SelectObject(memoryDc, oldBrush);
    DeleteObject(accentPen);
    DeleteObject(accentBrush);

    SetBkMode(memoryDc, TRANSPARENT);
    SetTextColor(memoryDc, RGB(255, 255, 255));
    HFONT font = CreateFontW(
        12,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(memoryDc, font);
    RECT textRect{1, 8, 31, 25};
    DrawTextW(memoryDc, L"TL2", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(memoryDc, oldFont);
    DeleteObject(font);
    SelectObject(memoryDc, oldBitmap);

    ICONINFO iconInfo{};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = colorBitmap;
    iconInfo.hbmMask = maskBitmap;
    HICON icon = CreateIconIndirect(&iconInfo);

    DeleteObject(maskBitmap);
    DeleteObject(colorBitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    return icon != nullptr ? icon : LoadIconW(nullptr, IDI_APPLICATION);
}

void WideFromAnsi(char* source, wchar_t* destination, size_t destinationCount) {
    if (destination == nullptr || destinationCount == 0) {
        return;
    }
    MultiByteToWideChar(CP_ACP, 0, source, -1, destination, static_cast<int>(destinationCount));
    destination[destinationCount - 1] = L'\0';
}

void SetTrayEditText(int controlId, const wchar_t* text) {
    if (g_trayConfigWindow != nullptr && IsWindow(g_trayConfigWindow)) {
        SetDlgItemTextW(g_trayConfigWindow, controlId, text);
    }
}

void RefreshTrayEditText() {
    wchar_t left[32]{};
    wchar_t up[32]{};
    wchar_t right[32]{};
    wchar_t down[32]{};
    WideFromAnsi(g_keyLeftName, left, 32);
    WideFromAnsi(g_keyUpName, up, 32);
    WideFromAnsi(g_keyRightName, right, 32);
    WideFromAnsi(g_keyDownName, down, 32);
    SetTrayEditText(kDllEditLeft, left);
    SetTrayEditText(kDllEditUp, up);
    SetTrayEditText(kDllEditRight, right);
    SetTrayEditText(kDllEditDown, down);
}

void ApplyTrayPreset(int presetIndex) {
    ApplyKeyboardPresetByIndex(presetIndex, true);
    RefreshIniWriteTime();
    RefreshTrayEditText();
}

void ApplyTrayCustomKeys() {
    if (g_trayConfigWindow == nullptr || !IsWindow(g_trayConfigWindow)) {
        return;
    }

    wchar_t left[32]{};
    wchar_t up[32]{};
    wchar_t right[32]{};
    wchar_t down[32]{};
    GetDlgItemTextW(g_trayConfigWindow, kDllEditLeft, left, 32);
    GetDlgItemTextW(g_trayConfigWindow, kDllEditUp, up, 32);
    GetDlgItemTextW(g_trayConfigWindow, kDllEditRight, right, 32);
    GetDlgItemTextW(g_trayConfigWindow, kDllEditDown, down, 32);

    char leftName[16]{};
    char upName[16]{};
    char rightName[16]{};
    char downName[16]{};
    const int parsedLeft = ParseKeyName(left, 0, leftName, sizeof(leftName));
    const int parsedUp = ParseKeyName(up, 0, upName, sizeof(upName));
    const int parsedRight = ParseKeyName(right, 0, rightName, sizeof(rightName));
    const int parsedDown = ParseKeyName(down, 0, downName, sizeof(downName));

    if (parsedLeft == 0 || parsedUp == 0 || parsedRight == 0 || parsedDown == 0) {
        MessageBoxW(g_trayConfigWindow, L"Touches acceptees: A-Z, LEFT, UP, RIGHT, DOWN.", L"TL2TrueKeyboardMove", MB_OK | MB_ICONWARNING);
        return;
    }

    g_keyLeft = parsedLeft;
    g_keyUp = parsedUp;
    g_keyRight = parsedRight;
    g_keyDown = parsedDown;
    lstrcpynA(g_keyLeftName, leftName, sizeof(g_keyLeftName));
    lstrcpynA(g_keyUpName, upName, sizeof(g_keyUpName));
    lstrcpynA(g_keyRightName, rightName, sizeof(g_keyRightName));
    lstrcpynA(g_keyDownName, downName, sizeof(g_keyDownName));
    RefreshCurrentPresetFromKeys();

    if (g_iniFile[0] != L'\0') {
        WritePrivateProfileStringW(L"Keys", L"Preset", L"CUSTOM", g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Left", left, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Up", up, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Right", right, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Down", down, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Forward", nullptr, g_iniFile);
        WritePrivateProfileStringW(L"Keys", L"Back", nullptr, g_iniFile);
        RefreshIniWriteTime();
    }

    LogLine("Tray custom keyboard config applied.");
}

void ShowTrayConfigWindow() {
    if (g_trayConfigWindow == nullptr || !IsWindow(g_trayConfigWindow)) {
        LogLine("DLL config window open requested but window is unavailable.");
        return;
    }
    RefreshTrayEditText();
    ShowWindow(g_trayConfigWindow, SW_SHOWNORMAL);
    SetWindowPos(g_trayConfigWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(g_trayConfigWindow);
    SetForegroundWindow(g_trayConfigWindow);
    LogLine("DLL config window shown.");
}

void HideTrayConfigWindow() {
    if (g_trayConfigWindow != nullptr && IsWindow(g_trayConfigWindow)) {
        ShowWindow(g_trayConfigWindow, SW_MINIMIZE);
        LogLine("DLL config window minimized.");
    }
}

void AddDllTrayIcon(HWND window) {
    ZeroMemory(&g_dllTrayIcon, sizeof(g_dllTrayIcon));
    g_dllTrayIcon.cbSize = sizeof(g_dllTrayIcon);
    g_dllTrayIcon.hWnd = window;
    g_dllTrayIcon.uID = 1;
    g_dllTrayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_dllTrayIcon.uCallbackMessage = kDllTrayMessage;
    g_dllTrayIcon.hIcon = g_trayConfigIcon != nullptr ? g_trayConfigIcon : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_dllTrayIcon.szTip, L"TL2TrueKeyboardMove");
    if (Shell_NotifyIconW(NIM_ADD, &g_dllTrayIcon)) {
        LogLine("DLL tray icon added.");
    } else {
        LogLine("DLL tray icon add failed.");
    }
}

void RemoveDllTrayIcon() {
    if (g_dllTrayIcon.cbSize != 0) {
        Shell_NotifyIconW(NIM_DELETE, &g_dllTrayIcon);
        ZeroMemory(&g_dllTrayIcon, sizeof(g_dllTrayIcon));
    }
}

void ShowDllTrayMenu(HWND window) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kDllTrayOpen, L"Open configuration");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kDllTrayWasd, L"Preset WASD");
    AppendMenuW(menu, MF_STRING, kDllTrayZqsd, L"Preset ZQSD");
    AppendMenuW(menu, MF_STRING, kDllTrayArrows, L"Preset Arrow keys");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kDllTrayHideIcon, L"Hide tray icon");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window, nullptr);
    DestroyMenu(menu);
}

void CreateTrayLabel(const wchar_t* text, int x, int y, int width, int height) {
    CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, width, height, g_trayConfigWindow, nullptr, g_module, nullptr);
}

void CreateTrayButton(const wchar_t* text, int id, int x, int y, int width, int height) {
    CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, width, height, g_trayConfigWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_module, nullptr);
}

void CreateTrayEdit(int id, int x, int y) {
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x, y, 70, 24, g_trayConfigWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_module, nullptr);
}

void CreateTrayConfigControls() {
    CreateTrayLabel(L"TL2TrueKeyboardMove - ERY_Asylum", 16, 14, 360, 22);
    CreateTrayLabel(L"Unofficial Torchlight II DLL mod created by ERY_Asylum.", 16, 38, 520, 20);
    CreateTrayLabel(L"Warning: this mod may be unstable, freeze, or crash the game.", 16, 64, 540, 20);
    CreateTrayLabel(L"Use at your own risk. Keep or minimize this window while playing.", 16, 86, 540, 20);
    CreateTrayLabel(L"Configuration changes are applied immediately.", 16, 112, 460, 20);

    CreateTrayButton(L"WASD", kDllButtonWasd, 16, 144, 92, 28);
    CreateTrayButton(L"ZQSD", kDllButtonZqsd, 116, 144, 92, 28);
    CreateTrayButton(L"Arrows", kDllButtonArrows, 216, 144, 92, 28);

    CreateTrayLabel(L"Left", 18, 192, 70, 20);
    CreateTrayLabel(L"Up", 116, 192, 70, 20);
    CreateTrayLabel(L"Right", 214, 192, 70, 20);
    CreateTrayLabel(L"Down", 312, 192, 70, 20);
    CreateTrayEdit(kDllEditLeft, 16, 214);
    CreateTrayEdit(kDllEditUp, 114, 214);
    CreateTrayEdit(kDllEditRight, 212, 214);
    CreateTrayEdit(kDllEditDown, 310, 214);

    CreateTrayButton(L"Apply custom", kDllButtonApply, 16, 258, 130, 30);
    CreateTrayButton(L"Minimize", kDllButtonHide, 156, 258, 120, 30);
}

LRESULT CALLBACK DllTrayWndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            g_trayConfigWindow = window;
            CreateTrayConfigControls();
            RefreshTrayEditText();
            return 0;
        case WM_COMMAND: {
            const UINT command = LOWORD(wparam);
            if (command == kDllTrayOpen) {
                ShowTrayConfigWindow();
            } else if (command == kDllTrayWasd || command == kDllButtonWasd) {
                ApplyTrayPreset(0);
            } else if (command == kDllTrayZqsd || command == kDllButtonZqsd) {
                ApplyTrayPreset(1);
            } else if (command == kDllTrayArrows || command == kDllButtonArrows) {
                ApplyTrayPreset(2);
            } else if (command == kDllButtonApply) {
                ApplyTrayCustomKeys();
            } else if (command == kDllButtonHide) {
                HideTrayConfigWindow();
            } else if (command == kDllTrayHideIcon) {
                HideTrayConfigWindow();
            }
            return 0;
        }
        case kDllTrayMessage:
        {
            const UINT event = LOWORD(lparam);
            if (event == WM_LBUTTONUP || event == WM_LBUTTONDBLCLK || event == NIN_SELECT || event == NIN_KEYSELECT) {
                ShowTrayConfigWindow();
            } else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
                ShowDllTrayMenu(window);
            }
            return 0;
        }
        case WM_CLOSE:
            HideTrayConfigWindow();
            return 0;
        case WM_DESTROY:
            g_trayConfigWindow = nullptr;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wparam, lparam);
    }
}

DWORD WINAPI DllTrayConfigThread(void*) {
    if constexpr (!kEnableDllTrayConfig) {
        return 0;
    }

    g_trayConfigIcon = CreateTrayConfigIcon();

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = DllTrayWndProc;
    windowClass.hInstance = g_module;
    windowClass.hIcon = g_trayConfigIcon != nullptr ? g_trayConfigIcon : LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = g_trayConfigIcon != nullptr ? g_trayConfigIcon : LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = L"TL2TrueKeyboardMoveDllTrayWindow";
    RegisterClassExW(&windowClass);

    HWND window = CreateWindowExW(
        WS_EX_TOPMOST,
        L"TL2TrueKeyboardMoveDllTrayWindow",
        L"TL2TrueKeyboardMove",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        600,
        360,
        nullptr,
        nullptr,
        g_module,
        nullptr);

    if (window == nullptr) {
        LogLine("DLL config window creation failed.");
        if (g_trayConfigIcon != nullptr) {
            DestroyIcon(g_trayConfigIcon);
            g_trayConfigIcon = nullptr;
        }
        return 1;
    }

    ShowWindow(window, SW_HIDE);
    if constexpr (kShowDllTrayConfigAtStartup) {
        ShowTrayConfigWindow();
    }

    MSG message{};
    while (g_running != 0 && GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (window != nullptr && IsWindow(window)) {
        DestroyWindow(window);
    }
    if (g_trayConfigIcon != nullptr) {
        DestroyIcon(g_trayConfigIcon);
        g_trayConfigIcon = nullptr;
    }
    return 0;
}

void StartDllTrayConfigThread() {
    if constexpr (!kEnableDllTrayConfig) {
        return;
    }
    if (InterlockedExchange(&g_trayThreadStarted, 1) != 0) {
        return;
    }

    HANDLE thread = CreateThread(nullptr, 0, DllTrayConfigThread, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    } else {
        LogLine("Failed to create DLL config window thread.");
    }
}

void LogKeyMask(int mask) {
    if (!kVerboseInputLogging) {
        return;
    }

    if (mask == 0) {
        LogLine("Keys held: none | vector: idle.");
        return;
    }

    char keys[96]{};
    bool first = true;
    const char* names[4] = {g_keyUpName, g_keyLeftName, g_keyDownName, g_keyRightName};
    for (int i = 0; i < 4; ++i) {
        if ((mask & (1 << i)) == 0) {
            continue;
        }

        if (!first) {
            lstrcatA(keys, "+");
        }
        lstrcatA(keys, names[i]);
        first = false;
    }

    char line[160]{};
    wsprintfA(line, "Keys held: %s | mask=%d.", keys, mask);
    LogLine(line);
}

void LaunchExternalConfigTool() {
    if (!g_launchExternalConfig) {
        return;
    }

    wchar_t directory[MAX_PATH]{};
    if (!GetModuleDirectory(directory, MAX_PATH)) {
        LogLine("External config launch skipped: unable to resolve module directory.");
        return;
    }

    wchar_t exePath[MAX_PATH]{};
    wcscpy_s(exePath, directory);
    wcscat_s(exePath, L"TL2TrueKeyboardMoveConfig.exe");

    const DWORD attributes = GetFileAttributesW(exePath);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        LogLine("External config launch skipped: TL2TrueKeyboardMoveConfig.exe not found.");
        return;
    }

    wchar_t commandLine[MAX_PATH + 4]{};
    wsprintfW(commandLine, L"\"%s\"", exePath);

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION processInfo{};
    if (!CreateProcessW(exePath, commandLine, nullptr, nullptr, FALSE, 0, nullptr, directory, &startupInfo, &processInfo)) {
        char line[160]{};
        wsprintfA(line, "External config launch failed: error=%lu.", GetLastError());
        LogLine(line);
        return;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    LogLine("External tray config launched.");
}

DWORD WINAPI StartupProbeThread(void*) {
    g_workerThreadId = GetCurrentThreadId();
    char startupLine[192]{};
    sprintf_s(
        startupLine,
        sizeof(startupLine),
        "%s %s started. Author: %s.",
        kModName,
        kModVersion,
        kModAuthor);
    LogLine(startupLine);
    LoadKeyboardConfig();
    RefreshIniWriteTime();
    LogProcessModuleInfo();
    ApplyActiveRangePatchAtStartup();
    InstallIntegratedD3D9OverlayHook();

    // Install the level-message hook immediately. The consolidated event-driven path does not
    // wait for CEGUI: exact SetLevel/SetRunning messages drive the bounded capture
    // pump. If the optional UI diagnostic is enabled, its IAT hook is retried only
    // during this bounded startup window and then by the normal worker loop.
    if (InterlockedCompareExchange(&g_enableTransitionMessageHook, 0, 0) != 0) {
        InstallMessageSendProbe();
    } else {
        LogLine("Transition message hook disabled by diagnostics config.");
    }
    if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0) {
        InterlockedExchange(&g_eventCaptureState, static_cast<LONG>(EventCaptureState::Loading));
    }
    for (int startupAttempt = 0; startupAttempt < 120 && g_running != 0; ++startupAttempt) {
        InstallGameWindowDispatchHook();
        const bool uiProbeEnabled =
            InterlockedCompareExchange(&g_enableUiLifecycleProbe, 0, 0) != 0;
        if (uiProbeEnabled) {
            InstallCeguiLifecycleHooks();
        }
        if (g_dispatchGameWindow != nullptr &&
            (!uiProbeEnabled ||
                InterlockedCompareExchange(&g_ceguiHooksInstalled, 1, 1) != 0)) {
            break;
        }
        Sleep(25);
    }

    InstallLocalMoveProbe();
    InstallStationaryClickFallback();
    InstallGameWindowDispatchHook();
    InstallCeguiLifecycleHooks();
    ArmDeferredStartupOverlay(GetTickCount());

    int lastMask = -1;
    int lastSentMoveMask = 0;
    int lastNonZeroMoveMask = 0;
    DWORD lastKeyboardMoveTick = 0;
    bool endWasDown = false;
    bool f7WasDown = false;
    bool overlayUpWasDown = false;
    bool overlayDownWasDown = false;
    bool overlayEnterWasDown = false;
    bool overlayDeleteWasDown = false;
    bool overlayF5WasDown = false;
    bool overlayF6WasDown = false;
    bool overlayF8WasDown = false;
    bool overlayF9WasDown = false;
    bool f10WasDown = false;
    bool overlayEscWasDown = false;
    bool holdPositionWasDown = false;
    DWORD holdPositionKeyboardResumeAfterTick = 0;

    while (g_running != 0) {
        const DWORD now = GetTickCount();
        LogMemoryWatchdogSnapshot(now);
        CheckKeyboardConfigReload();
        PollWindowsKeyboardLayout(now);
        InstallIntegratedD3D9OverlayHook();
        if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0 ||
            InterlockedCompareExchange(&g_enableContinuousControllerScan, 0, 0) != 0) {
            PollTransitionDiagnostics(now);
        }
        PollOgreLogLoadSignals(now);
        InstallGameWindowDispatchHook();
        InstallCeguiLifecycleHooks();
        // This is a bounded post-load pump, not a permanent controller scan.
        // It keeps the capture state alive long enough to observe the map-title /
        // loading-screen lifecycle and to apply the delayed PlayerCam fallback.
        if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) != 0) {
            const LONG captureState = InterlockedCompareExchange(&g_eventCaptureState, 0, 0);
            const DWORD pumpInterval = static_cast<DWORD>(
                InterlockedCompareExchange(&g_capturePumpIntervalMs, 0, 0));
            if ((captureState == static_cast<LONG>(EventCaptureState::AwaitReadySignal) ||
                    captureState == static_cast<LONG>(EventCaptureState::Capturing)) &&
                g_eventCaptureGeneration == g_mapGeneration &&
                now - g_lastCapturePumpTick >= pumpInterval) {
                g_lastCapturePumpTick = now;
                PostCaptureTickToGameWindow();
            }
        }
        InstallMapRenderReadyProbe(now);
        if (InterlockedExchange(&g_cameraPostLoadRequest, 0) != 0 &&
            InterlockedCompareExchange(&g_enableCameraAutoCalibration, 0, 0) != 0) {
            BeginCameraAcquisition(
                now,
                0,
                0,
                "post-load gameplay signal");
        }
        if (InterlockedCompareExchange(&g_enableCameraAutoCalibration, 0, 0) != 0 ||
            g_cameraAcquirePreserveBasisOnFailure) {
            PollPlayerCameraOrientation(now);
        }
        if (InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0 ||
            InterlockedCompareExchange(&g_enableContinuousControllerScan, 0, 0) != 0) {
            PollDirectPlayerContextDiagnostics(now);
            TryLockCurrentMapContext("main loop stable map/context check", now);
        }
        if (InterlockedCompareExchange(&g_stationaryFallbackActivations, 0, 0) > 0 &&
            InterlockedExchange(&g_stationaryFallbackLogged, 1) == 0) {
            LogLine("Stationary click fallback engaged; empty-world left click routed to attack.");
        }
        if (InterlockedCompareExchange(&g_holdPositionSuspensionActivations, 0, 0) > 0 &&
            InterlockedExchange(&g_holdPositionSuspensionLogged, 1) == 0) {
            LogLine("Hold Position key held; TL2TrueKeyboardMove input injection suspended.");
        }
        if (InterlockedCompareExchange(&g_holdPositionVanillaBypassActivations, 0, 0) > 0 &&
            InterlockedExchange(&g_holdPositionVanillaBypassLogged, 1) == 0) {
            LogLine("Hold Position vanilla mouse path bypass active; TL2 handles immobilization and facing.");
        }
        if (InterlockedCompareExchange(&g_holdPositionReleaseSettleActivations, 0, 0) > 0 &&
            InterlockedExchange(&g_holdPositionReleaseSettleLogged, 1) == 0) {
            LogLine("Hold Position release settle active; keyboard movement resume delayed briefly.");
        }
        if (InterlockedCompareExchange(&g_skillCarryEntries, 0, 0) > 0 &&
            InterlockedExchange(&g_skillCarryLogged, 1) == 0) {
            LogLine("Skill/secondary-action strict context dip carried; stable player LocalMove context preserved.");
        }

        if (g_enableOverlay) {
            PumpOverlayMessages();
            PumpDeferredStartupOverlay(now);
            UpdateOverlayWindowPositionStable(now);
        }

        const KeyboardInputSnapshot keyboardSnapshot = CaptureKeyboardInputSnapshot();
        UpdateOverlayFallbackWindowFocusVisibility(keyboardSnapshot.gameFocusedForInput);
        const bool overlayVisibleForFocus =
            g_enableOverlay &&
            keyboardSnapshot.gameFocusedForInput &&
            InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0;
        InterlockedExchange(&g_gameInputFocused, keyboardSnapshot.gameFocusedForInput ? 1 : 0);
        if (!keyboardSnapshot.gameFocusedForInput && !overlayVisibleForFocus) {
            if (lastMask != 0) {
                LogKeyMask(0);
            }
            lastMask = 0;
            lastSentMoveMask = 0;
            lastNonZeroMoveMask = 0;
            InterlockedExchange(&g_currentKeyboardMoveMask, 0);
            ResetWasdOwnershipRecoveryState(false);
            endWasDown = false;
            f7WasDown = false;
            overlayUpWasDown = false;
            overlayDownWasDown = false;
            overlayEnterWasDown = false;
            overlayDeleteWasDown = false;
            overlayF5WasDown = false;
            overlayF6WasDown = false;
            overlayF8WasDown = false;
            overlayF9WasDown = false;
            f10WasDown = false;
            overlayEscWasDown = false;
            holdPositionWasDown = false;
            holdPositionKeyboardResumeAfterTick = 0;
            InterlockedExchange(&g_holdPositionSuspended, 0);
            if (InterlockedExchange(&g_focusSuspensionLogged, 1) == 0) {
                LogLine("Game window is not foreground; TL2TrueKeyboardMove input injection suspended.");
            }
            Sleep(16);
            continue;
        }

        const bool f10Down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
        if (f10Down && !f10WasDown && g_overlayCustomStep < 0) {
            RequestManualWasdBootstrap(now);
            if (g_overlayWindow != nullptr && IsWindow(g_overlayWindow)) {
                InvalidateRect(g_overlayWindow, nullptr, TRUE);
            }
        }
        f10WasDown = f10Down;
        PumpEmergencyWasdBootstrap(now);

        const bool endDown = g_enableOverlay && (GetAsyncKeyState(VK_END) & 0x8000) != 0;
        if (g_enableOverlay && endDown && !endWasDown) {
            const bool overlayVisible =
                InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0;
            if (!overlayVisible) {
                SetOverlayVisible(true);
            } else {
                SetOverlayVisible(false);
            }
        }
        endWasDown = endDown;

        const bool f7Down = g_enableOverlay && (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        if (g_enableOverlay && f7Down && !f7WasDown) {
            const bool overlayVisible =
                InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0;
            if (overlayVisible && g_overlayCustomStep < 0) {
                ToggleClickMovementDisabled();
                InvalidateRect(g_overlayWindow, nullptr, TRUE);
            }
        }
        f7WasDown = f7Down;

        if (g_enableOverlay && InterlockedCompareExchange(&g_overlayVisible, 1, 1) != 0) {
            lastMask = 0;
            lastSentMoveMask = 0;
            lastNonZeroMoveMask = 0;
            InterlockedExchange(&g_currentKeyboardMoveMask, 0);
            ResetWasdOwnershipRecoveryState(false);

            const bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
            if (f5Down && !overlayF5WasDown) {
                CycleOverlayFontScaleMode();
            }
            overlayF5WasDown = f5Down;

            if (g_overlayCustomStep >= 0) {
                HandleOverlayCustomCapture();
                overlayF6WasDown = false;
                overlayF8WasDown = false;
                overlayF9WasDown = false;
                overlayEscWasDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
                Sleep(4);
                continue;
            }

            const bool escDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
            if (escDown && !overlayEscWasDown) {
                SetOverlayVisible(false);
            }
            overlayEscWasDown = escDown;

            if (InterlockedCompareExchange(&g_overlayVisible, 1, 1) == 0) {
                overlayF5WasDown = false;
                Sleep(4);
                continue;
            }

            const bool upDown = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
            const bool downDown = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
            const bool enterDown = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
            const bool deleteDown = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
            const bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
            const bool f8Down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
            const bool f9Down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;

            if (upDown && !overlayUpWasDown) {
                --g_overlaySelectedPresetIndex;
                if (g_overlaySelectedPresetIndex < 0) {
                    g_overlaySelectedPresetIndex = kKeyboardPresetCount - 1;
                }
                InvalidateRect(g_overlayWindow, nullptr, TRUE);
            }
            if (downDown && !overlayDownWasDown) {
                ++g_overlaySelectedPresetIndex;
                if (g_overlaySelectedPresetIndex >= kKeyboardPresetCount) {
                    g_overlaySelectedPresetIndex = 0;
                }
                InvalidateRect(g_overlayWindow, nullptr, TRUE);
            }
            if (enterDown && !overlayEnterWasDown) {
                ApplyKeyboardPresetByIndex(g_overlaySelectedPresetIndex, true);
                RefreshIniWriteTime();
                SetOverlayVisible(false);
            }
            if (deleteDown && !overlayDeleteWasDown) {
                StartOverlayCustomBinding();
            }
            if (f6Down && !overlayF6WasDown) {
                ToggleKeyboardMousePatchEnabled();
                InvalidateRect(g_overlayWindow, nullptr, TRUE);
            }
            if (f8Down && !overlayF8WasDown) {
                ToggleActiveRangePatchConfigured();
                InvalidateRect(g_overlayWindow, nullptr, TRUE);
            }
            if (f9Down && !overlayF9WasDown) {
                RequestManualCameraRecalibration(now);
                InvalidateRect(g_overlayWindow, nullptr, TRUE);
            }
            overlayUpWasDown = upDown;
            overlayDownWasDown = downDown;
            overlayEnterWasDown = enterDown;
            overlayDeleteWasDown = deleteDown;
            overlayF6WasDown = f6Down;
            overlayF8WasDown = f8Down;
            overlayF9WasDown = f9Down;

            Sleep(4);
            continue;
        }

        overlayUpWasDown = false;
        overlayDownWasDown = false;
        overlayEnterWasDown = false;
        overlayDeleteWasDown = false;
        overlayF6WasDown = false;
        overlayF8WasDown = false;
        overlayF9WasDown = false;
        overlayEscWasDown = false;

        if (InterlockedCompareExchange(&g_keyboardMousePatchEnabled, 0, 0) == 0) {
            if (lastMask != 0) {
                LogKeyMask(0);
            }
            lastMask = 0;
            lastSentMoveMask = 0;
            lastNonZeroMoveMask = 0;
            InterlockedExchange(&g_currentKeyboardMoveMask, 0);
            ResetWasdOwnershipRecoveryState(false);
            holdPositionWasDown = false;
            holdPositionKeyboardResumeAfterTick = 0;
            InterlockedExchange(&g_holdPositionSuspended, 0);
            Sleep(16);
            continue;
        }

        if (IsPassiveContextSuspendActive(now)) {
            if (lastMask != 0) {
                LogKeyMask(0);
            }
            lastMask = 0;
            lastSentMoveMask = 0;
            lastNonZeroMoveMask = 0;
            InterlockedExchange(&g_currentKeyboardMoveMask, 0);
            ResetWasdOwnershipRecoveryState(false);
            holdPositionWasDown = false;
            holdPositionKeyboardResumeAfterTick = 0;
            InterlockedExchange(&g_holdPositionSuspended, 0);
            Sleep(16);
            continue;
        }

        if (IsTransitionLocalMoveBlocked(now)) {
            if (lastMask != 0) {
                LogKeyMask(0);
            }
            lastMask = 0;
            lastSentMoveMask = 0;
            lastNonZeroMoveMask = 0;
            InterlockedExchange(&g_currentKeyboardMoveMask, 0);
            ResetWasdOwnershipRecoveryState(false);
            holdPositionWasDown = false;
            holdPositionKeyboardResumeAfterTick = 0;
            InterlockedExchange(&g_holdPositionSuspended, 0);
            Sleep(16);
            continue;
        }

        if (InterlockedCompareExchange(&g_levelBootstrapActive, 1, 1) != 0 &&
            InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) == 0) {
            if (g_bootstrapWindowUntilTick != 0 && static_cast<LONG>(now - g_bootstrapWindowUntilTick) >= 0) {
                InterlockedExchange(&g_levelBootstrapActive, 0);
                LogLine("Post-load bootstrap window expired without a stable level context.");
            }
        }

        if ((InterlockedCompareExchange(&g_enableEventDrivenCapture, 0, 0) == 0 ||
                InterlockedCompareExchange(&g_enableContinuousControllerScan, 0, 0) != 0) &&
            InterlockedCompareExchange(&g_hasPlayerLocalMove, 1, 1) != 0 &&
            now - g_lastContextMonitorTick >= kContextMonitorStablePollMs) {
            g_lastContextMonitorTick = now;
            void* monitorSelf = nullptr;
            DWORD monitorArg1 = 0;
            AcquireSRWLockShared(&g_localMoveCaptureLock);
            monitorSelf = g_playerLocalMoveSelf;
            monitorArg1 = g_playerLocalMoveArg1;
            ReleaseSRWLockShared(&g_localMoveCaptureLock);
            if (monitorSelf == nullptr) {
                ClearPlayerLocalMoveContext("Level context monitor invalidated current context: null player self.");
            } else if (monitorArg1 == 0) {
                EnterTransientContextHold(
                    "Level context monitor observed null arg1; stable player self preserved",
                    now);
            } else if (!LooksLikeUsableStoredPlayerSelf(monitorSelf)) {
                ClearPlayerLocalMoveContext("Level context monitor invalidated current context: player self no longer usable.");
            } else if (!LooksLikeReadableLocalMoveArg1(monitorArg1)) {
                EnterTransientContextHold(
                    "Level context monitor observed transient unreadable arg1",
                    now);
            } else if (IsCurrentMapContextLockedFor(monitorSelf, monitorArg1)) {
                TryLockCurrentMapContext("periodic map-lock monitor refresh", now);
            } else if (!StoredContextMatchesCurrentController(monitorSelf, monitorArg1)) {
                EnterTransientContextHold(
                    "Level context monitor observed controller mismatch; stable context preserved",
                    now);
            } else {
                float currentX = 0.0f;
                float currentZ = 0.0f;
                if (!ReadFloatSafe(monitorSelf, 0x70, &currentX) ||
                    !ReadFloatSafe(monitorSelf, 0x78, &currentZ) ||
                    currentX != currentX ||
                    currentZ != currentZ ||
                    currentX < -100000.0f ||
                    currentX > 100000.0f ||
                    currentZ < -100000.0f ||
                    currentZ > 100000.0f) {
                    ClearPlayerLocalMoveContext("Level context monitor invalidated current context coordinates.");
                } else if (InterlockedCompareExchange(&g_hasLastPlayerMonitorPosition, 1, 1) != 0) {
                    const float dx = currentX - g_lastPlayerMonitorX;
                    const float dz = currentZ - g_lastPlayerMonitorZ;
                    const float distanceSquared = dx * dx + dz * dz;
                    if (distanceSquared > 10000.0f) {
                        EnterTransientContextHold(
                            "Large position jump observed without confirmed portal; stable context preserved",
                            now);
                    } else {
                        g_lastPlayerMonitorX = currentX;
                        g_lastPlayerMonitorZ = currentZ;
                    }
                } else {
                    g_lastPlayerMonitorX = currentX;
                    g_lastPlayerMonitorZ = currentZ;
                    InterlockedExchange(&g_hasLastPlayerMonitorPosition, 1);
                }
            }
        }

        const bool holdPositionDown = ApplyHoldPositionSnapshotToMod(keyboardSnapshot, now);
        if (holdPositionDown) {
            if (lastMask != 0) {
                LogKeyMask(0);
            }
            lastMask = 0;
            lastSentMoveMask = 0;
            lastNonZeroMoveMask = 0;
            InterlockedExchange(&g_currentKeyboardMoveMask, 0);
            ResetWasdOwnershipRecoveryState(false);
            holdPositionWasDown = true;
            holdPositionKeyboardResumeAfterTick = 0;
            Sleep(4);
            continue;
        }
        if (holdPositionWasDown) {
            holdPositionWasDown = false;
            holdPositionKeyboardResumeAfterTick = now + kHoldPositionReleaseKeyboardSettleMs;
            InterlockedIncrement(&g_holdPositionReleaseSettleActivations);
        }
        if (holdPositionKeyboardResumeAfterTick != 0 &&
            static_cast<LONG>(now - holdPositionKeyboardResumeAfterTick) < 0) {
            if (lastMask != 0) {
                LogKeyMask(0);
            }
            lastMask = 0;
            lastSentMoveMask = 0;
            lastNonZeroMoveMask = 0;
            InterlockedExchange(&g_currentKeyboardMoveMask, 0);
            ResetWasdOwnershipRecoveryState(false);
            Sleep(4);
            continue;
        }
        holdPositionKeyboardResumeAfterTick = 0;

        const int mask = keyboardSnapshot.moveMask;
        InterlockedExchange(&g_currentKeyboardMoveMask, mask);
        PumpEmergencyWasdBootstrap(now);
        PumpWasdOwnershipRecovery(now);
        PumpPostFirstMoveCameraTransitionWatch(now);
        PumpPostReleaseCameraVerification(now);

        if (mask != 0) {
            CancelPendingLocalMoveBrake();
        } else {
            FirePendingLocalMoveBrakeIfNeeded(now);
        }

        if (mask != lastMask) {
            if (mask == 0 && lastMask > 0) {
                if (lastNonZeroMoveMask != 0 &&
                    keyboardSnapshot.gameFocusedForInput &&
                    !overlayVisibleForFocus) {
                    ArmPostReleaseCameraVerification(now);
                    SendKeyboardReleaseBrake();
                    lastKeyboardMoveTick = now;
                }
                lastSentMoveMask = 0;
            } else if (mask != 0) {
                CancelPendingLocalMoveBrake();
                ReopenPostReleaseCameraVerificationForNextRelease();
                lastNonZeroMoveMask = mask;
                if (lastMask == 0 &&
                    InterlockedCompareExchange(&g_resetInteractionFacingOnKeyboardStart, 0, 0) != 0) {
                    InterlockedExchange(&g_keyboardFacingResetPending, 1);
                }
            }
            LogKeyMask(mask);
            lastMask = mask;
        }

        if (mask != 0 &&
            (mask != lastSentMoveMask ||
                now - lastKeyboardMoveTick >= static_cast<DWORD>(
                    InterlockedCompareExchange(&g_keyboardMoveRepeatMs, 0, 0)))) {
            CancelPendingLocalMoveBrake();
            SendKeyboardMove(mask);
            lastSentMoveMask = mask;
            lastNonZeroMoveMask = mask;
            lastKeyboardMoveTick = now;
        }

        Sleep(4);
    }

    RestoreCeguiLifecycleHooks();
    RestoreGameWindowDispatchHook();
    LogLine("TL2TrueKeyboardMove keyboard movement worker exited cleanly.");
    return 0;
}

void StartStartupProbeThread() {
    if (InterlockedExchange(&g_threadStarted, 1) != 0) {
        return;
    }

    HANDLE thread = CreateThread(nullptr, 0, StartupProbeThread, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    } else {
        LogLine("Failed to create delayed worker thread.");
    }
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        InitPaths(module);
        DisableThreadLibraryCalls(module);
        LogLine("TL2TrueKeyboardMove DLL loaded. Starting delayed worker probe.");
        StartStartupProbeThread();
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = 0;
        RestoreCeguiLifecycleHooks();
        RestoreGameWindowDispatchHook();
        RestoreMapRenderReadyProbe();
        LogLine("TL2TrueKeyboardMove DLL unloading.");
    }

    return TRUE;
}

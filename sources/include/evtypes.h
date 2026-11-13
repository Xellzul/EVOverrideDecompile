/* evtypes.h -- small shared types and limits. Notes: Research/notes/headers.md */
#ifndef EVTYPES_H
#define EVTYPES_H

#include <MacTypes.h>

#define kMaxJunkTypes           128
#define kMissionDescBase        4000
#define kBarDescBase            10000
#define kMapDoneItem            1
#define kMapViewItem            3
#define kMapZoomOutItem         4
#define kMapZoomInItem          5
#define kMapClearRouteItem      8
#define kMapInfoItem            6
#define kOkItem                 1
#define kSmallBet               1000
#define kBigBet                 5000
#define kNumKeys                31
#define kMaxSystemLinks         16
#define kMaxHyperRoute          32

#define kKeyDownArrow           0x75
#define kKeyUpArrow             0x76
#define kStatusPanelWidth       144
#define kFixedOne               0x10000
#define kKeyDigit1              0x1A
#define kKeyDigit2              0x1B
#define kKeyDigit3              0x1C
#define kKeyDigit4              0x1D
#define kKeyReturn              0x2C
#define kKeyShift               0x30
#define kKeyCapsLock            0x31
#define kKeyOption              0x32
#define kKeyControl             0x33
#define kKeySpace               0x39
#define kKeyCommand             0x3F
#define kKeyF5                  0x68
#define kKeyF3                  0x6B
#define kKeyF2                  0x70
#define kKeyF1                  0x72
#define kKeyPower               0x77
#define kKeyF4                  0x7E
#define kKeyQ                   0x04
#define kKeyEquals              0x10
#define kKeyMinus               0x13
#define kKeyDigit5              0x1F
#define kKeySemicolon           0x21
#define kKeyQuote               0x2F
#define kKeyPadDivide           0x43
#define kKeyPadDecimal          0x49
#define kKeyPadEquals           0x59
#define kKeyF10                 0x65
#define kKeyF12                 0x67
#define kKeyF6                  0x69
#define kKeyF7                  0x6A
#define kKeyF8                  0x6C
#define kKeyF9                  0x6D
#define kKeyF11                 0x6F
#define kKeyF15                 0x79
#define kKeyC                   0x00
#define kKeyV                   0x01
#define kKeyB                   0x03
#define kKeyW                   0x05
#define kKeyE                   0x06
#define kKeyR                   0x07
#define kKeyA                   0x08
#define kKeyS                   0x09
#define kKeyD                   0x0A
#define kKeyF                   0x0B
#define kKeyH                   0x0C
#define kKeyZ                   0x0E
#define kKeyO                   0x17
#define kKeyY                   0x18
#define kKeyK                   0x20
#define kKeyBackslash           0x22
#define kKeyM                   0x26
#define kKeyU                   0x28
#define kKeyI                   0x2A
#define kKeyP                   0x2B
#define kKeyL                   0x2D
#define kKeyJ                   0x2E
#define kKeyTab                 0x38
#define kKeyGrave               0x3A
#define kKeyEscape              0x3D
#define kKeyPadMinus            0x46
#define kKeyPadMultiply         0x4B
#define kKeyPadPlus             0x4D
#define kKeyPageDown            0x71
#define kKeyLeftArrow           0x73
#define kKeyRightArrow          0x74
#define kKeyHelp                0x7A
#define kKeyHome                0x7B
#define kKeyPageUp              0x7C
#define kKeyForwardDelete       0x7D
#define kKeyEnd                 0x7F

/* button pictures by caption (and width, where a caption comes in several): the raised one; + 1 is the lit one */
#define kLeave78ButtonPict                  7000
#define kBuyButtonPict                      7002
#define kSellButtonPict                     7004
#define kBuyShipButtonPict                  7006
#define kLeave121ButtonPict                 7008
#define kDone121ButtonPict                  7010
#define kDone78ButtonPict                   7012
#define kLeave200ButtonPict                 7014
#define kRefuelShipButtonPict               7016
#define kCommodityExchangeButtonPict        7018
#define kOutfitShipButtonPict               7020
#define kVisitShipyardButtonPict            7022
#define kMinusButtonPict                    7024
#define kPlusButtonPict                     7026
#define kUpArrowButtonPict                  7028
#define kDownArrowButtonPict                7030
#define kMissionComputerButtonPict          7032
#define kEndCommunicationButtonPict         7034
#define kGreetingsButtonPict                7036
#define kGeneralButtonPict                  7038
#define kRequestAssistanceButtonPict        7040
#define kOfferBribeButtonPict               7042
#define kCargo121ButtonPict                 7044
#define kExtrasButtonPict                   7046
#define kBegForMercyButtonPict              7048
#define kAcceptMissionButtonPict            7050
#define kCancel78ButtonPict                 7052
#define kOkayButtonPict                     7054
#define kAcceptPaymentButtonPict            7056
#define kDemandMoreButtonPict               7058
#define kAcceptPriceButtonPict              7060
#define kLowerPriceButtonPict               7062
#define kHireEscortButtonPict               7064
#define kRequisitionEscortButtonPict        7066
#define kReleaseFromServitudeButtonPict     7068
#define kSelectButtonPict                   7072
#define kCancel121ButtonPict                7074
#define kAbortButtonPict                    7076
#define kCargo61ButtonPict                  7078
#define kCreditsButtonPict                  7080
#define kAmmoButtonPict                     7082
#define kDemandTributeButtonPict            7084
#define kFuelButtonPict                     7086
#define kCaptureShipButtonPict              7088
#define kSpaceportBarButtonPict             7090
#define kAbortMissionButtonPict             7092
#define kGambleButtonPict                   7094
#define kWatchHoloVidButtonPict             7096
#define kHireAnEscortButtonPict             7100
#define kBackRoomButtonPict                 7102
#define kBet1000ButtonPict                  7104
#define kBet5000ButtonPict                  7106
#define kYesButtonPict                      7108
#define kNoButtonPict                       7110
#define kUseAsEscortButtonPict              7112
#define kUseAsMyShipButtonPict              7114
#define kInfoButtonPict                     7116
#define kClearRouteButtonPict               7118

#pragma options align=mac68k

typedef struct HudRedraw {
	Boolean	nav;
	Boolean	shield;
	Boolean	credits;
	Boolean	ammo;
	Boolean	fuel;
	Boolean	radar;
	Boolean	target;
} HudRedraw;

typedef struct GamePrefs {
	Boolean	introMusic;
	Boolean	voicesOn;
	Boolean	highDetail;
	Boolean	noMovies;
	Boolean	useQuickdraw;
	Boolean	noStreaks;
	short		soundVolume;
} GamePrefs;

#pragma options align=reset

/* Numbers shared across files (Research/notes/REVIEW-R1.md) */
#define kFullCircle             360
#define kHalfCircle             180
#define kMidVolume              128	/* sound volume and pan, half of 255 */
#define kDeadMark               (-32000)	/* shield, life or timer set far below zero: dead */
#define kLowestValue            (-32767)
#define kNoSystemGovt           (-32000)	/* a system's govt above this: its syst resource exists (unused slots get kLowestValue) */
#define kDefaultLoopID          30001	/* the intro's looped sound pair */
#define kDialogTextSize         9	/* geneva 9, the size of every dialog and list text */
#define kShortMax               32767
#define kMaxCredits             0x5FFFFFFF	/* the cap on the player's money */
#define kHeadingFrames          36	/* rotation frames of a ship, weapon or box sprite */
#define kHeadingStep            10	/* degrees between two of them */
#define kMissionSpawnDelay      70	/* a mission ship appears after Rand(70) + 70 ticks */

/* keyMap indices: the configurable controls (default keys are set in Setup.c) */
#define kCtlNextWeapon          0
#define kCtlClearWeapon         1
#define kCtlFirePrimary         2
#define kCtlFireSecondary       3
#define kCtlHail                4
#define kCtlLand                5
#define kCtlSkipMessage         6
#define kCtlFaceTarget          7
#define kCtlNavOff              8
#define kCtlMap                 9
#define kCtlNextTarget          10
#define kCtlNearestTarget       11
#define kCtlNavHyperspace       12
#define kCtlNextLink            13
#define kCtlJump                14
#define kCtlJettison            15
#define kCtlBoard               16
#define kCtlEscortAttack        17
#define kCtlEscortRecall        18
#define kCtlEscortHold          19
#define kCtlEject               20
#define kCtlSelfDestruct        21
#define kCtlTurnLeft            22
#define kCtlTurnRight           23
#define kCtlThrust              24
#define kCtlReverse             25
#define kCtlAfterburner         27
#define kCtlNavDefault          29	/* four keys, one per navDefault slot of the system */
#define kCtlSpawnPerson         33	/* 33 to 42 are the developer's: spawn, kill, cheat */
#define kCtlDropTarget          34
#define kCtlResetPlayer         35
#define kCtlWreckTarget         36
#define kCtlKillTarget          37
#define kCtlRefillAmmo          38
#define kCtlAdjustCredits       39
#define kCtlShowMemory          40
#define kCtlNextShipType        41
#define kCtlPrevShipType        42
#define kCtlPlayerInfo          28
#define kCtlMissions            43
#define kCtlCloak               44

#endif

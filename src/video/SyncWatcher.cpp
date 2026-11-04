#include "SyncWatcher.h"
#include "../core/Globals.h"
#include "../web/SerialMirror.h"
#include "../hardware/LEDControl.h"
#include "../gbs/GBSRegister.h"
#include "../gbs/GBSPresets.h"
#include "../gbs/GBSSync.h"
#include "../gbs/GBSPhase.h"
#include "../gbs/GBSController.h"
#include "../gbs/GBSVideoProcessing.h"
#include "../video/VideoInput.h"
#include "../video/VideoTiming.h"
#include "../video/VideoScaling.h"
#include "../video/Scanlines.h"
#include "../video/Deinterlacing.h"
#include "../clock/Si5351Manager.h"
#include "../wifi/WiFiManager.h"
#include "../utils/DebugHelpers.h"
#include "../gbs/tv5725.h"
#include "framesync.h"
#include "../../lib/si5351mcu/si5351mcu.h"
#include <Arduino.h>
#include <Wire.h>

typedef TV5725<GBS_ADDR> GBS;

void fastSogAdjust()
{
    if (rto->noSyncCounter <= 5) {
        uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
        uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
        if (debug_backup != 0xa) {
            GBS::TEST_BUS_SEL::write(0xa);
        }
        if (debug_backup_SP != 0x0f) {
            GBS::TEST_BUS_SP_SEL::write(0x0f);
        }

        if ((GBS::TEST_BUS_2F::read() & 0x05) != 0x05) {
            while ((GBS::TEST_BUS_2F::read() & 0x05) != 0x05) {
                if (rto->currentLevelSOG >= 4) {
                    rto->currentLevelSOG -= 2;
                } else {
                    rto->currentLevelSOG = 13;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(40);
                    break; // abort / restart next round
                }
                setAndUpdateSogLevel(rto->currentLevelSOG);
                delay(28); // 4
            }
            delay(10);
        }

        if (debug_backup != 0xa) {
            GBS::TEST_BUS_SEL::write(debug_backup);
        }
        if (debug_backup_SP != 0x0f) {
            GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
        }
    }
}

boolean snapToIntegralFrameRate(void)
{
    // Fetch the current output frame rate
    float ofr = getOutputFrameRate();

    if (ofr < 1.0f) {
        delay(1);
        ofr = getOutputFrameRate();
    }

    float target;
    if (ofr > 56.5f && ofr < 64.5f) {
        target = 60.0f; // NTSC like
    } else if (ofr > 46.5f && ofr < 54.5f) {
        target = 50.0f; // PAL like
    } else {
        // too far out of spec for an auto adjust
        SerialM.println(F("out of bounds"));
        return false;
    }

    SerialM.print(F("Snap to "));
    SerialM.print(target, 1); // precission 1
    SerialM.println("Hz");

    // We'll be adjusting the htotal incrementally, so store current and best match.
    uint16_t currentHTotal = GBS::VDS_HSYNC_RST::read();
    uint16_t closestHTotal = currentHTotal;

    // What's the closest we've been to the frame rate?
    float closestDifference = fabs(target - ofr);

    // Repeatedly adjust htotals until we find the closest match.
    for (;;) {
        delay(0);

        // Try to move closer to the desired framerate.
        if (target > ofr) {
            if (currentHTotal > 0 && applyBestHTotal(currentHTotal - 1)) {
                --currentHTotal;
            } else {
                return false;
            }
        } else if (target < ofr) {
            if (currentHTotal < 4095 && applyBestHTotal(currentHTotal + 1)) {
                ++currentHTotal;
            } else {
                return false;
            }
        } else {
            return true;
        }

        // Are we closer?
        ofr = getOutputFrameRate();

        if (ofr < 1.0f) {
            delay(1);
            ofr = getOutputFrameRate();
        }
        if (ofr < 1.0f) {
            return false;
        }

        // If we're getting closer, continue trying, otherwise break out of the test loop.
        float newDifference = fabs(target - ofr);
        if (newDifference < closestDifference) {
            closestDifference = newDifference;
            closestHTotal = currentHTotal;
        } else {
            break;
        }
    }

    // Reapply the closest htotal if need be.
    if (closestHTotal != currentHTotal) {
        applyBestHTotal(closestHTotal);
    }

    return true;
}

boolean checkBoardPower()
{
    GBS::ADC_UNUSED_69::write(0x6a); // 0110 1010
    if (GBS::ADC_UNUSED_69::read() == 0x6a) {
        GBS::ADC_UNUSED_69::write(0);
        return 1;
    }

    GBS::ADC_UNUSED_69::write(0); // attempt to clear
    if (rto->boardHasPower == true) {
        Serial.println(F("! power / i2c lost !"));
    }

    return 0;
}

void calibrateAdcOffset()
{
    GBS::PAD_BOUT_EN::write(0);          // disable output to pin for test
    GBS::PLL648_CONTROL_01::write(0xA5); // display clock to adc = 162mhz
    GBS::ADC_INPUT_SEL::write(2);        // 10 > R2/G2/B2 as input (not connected, so to isolate ADC)
    GBS::DEC_MATRIX_BYPS::write(1);
    GBS::DEC_TEST_ENABLE::write(1);
    GBS::ADC_5_03::write(0x31);    // bottom clamps, filter max (40mhz)
    GBS::ADC_TEST_04::write(0x00); // disable bit 1
    GBS::SP_CS_CLP_ST::write(0x00);
    GBS::SP_CS_CLP_SP::write(0x00);
    GBS::SP_5_56::write(0x05); // SP_SOG_MODE needs to be 1
    GBS::SP_5_57::write(0x80);
    GBS::ADC_5_00::write(0x02);
    GBS::TEST_BUS_SEL::write(0x0b); // 0x2b
    GBS::TEST_BUS_EN::write(1);
    resetDigital();

    uint16_t hitTargetCounter = 0;
    uint16_t readout16 = 0;
    uint8_t missTargetCounter = 0;
    uint8_t readout = 0;

    GBS::ADC_RGCTRL::write(0x7F);
    GBS::ADC_GGCTRL::write(0x7F);
    GBS::ADC_BGCTRL::write(0x7F);
    GBS::ADC_ROFCTRL::write(0x7F);
    GBS::ADC_GOFCTRL::write(0x3D); // start
    GBS::ADC_BOFCTRL::write(0x7F);
    GBS::DEC_TEST_SEL::write(1); // 5_1f = 0x1c

    //unsigned long overallTimer = millis();
    unsigned long startTimer = 0;
    for (uint8_t i = 0; i < 3; i++) {
        missTargetCounter = 0;
        hitTargetCounter = 0;
        delay(20);
        startTimer = millis();

        // loop breaks either when the timer runs out, or hitTargetCounter reaches target
        while ((millis() - startTimer) < 800) {
            readout16 = GBS::TEST_BUS::read() & 0x7fff;
            //Serial.println(readout16, HEX);
            // readout16 is unsigned, always >= 0
            if (readout16 < 7) {
                hitTargetCounter++;
                missTargetCounter = 0;
            } else if (missTargetCounter++ > 2) {
                if (i == 0) {
                    GBS::ADC_GOFCTRL::write(GBS::ADC_GOFCTRL::read() + 1); // incr. offset
                    readout = GBS::ADC_GOFCTRL::read();
                    Serial.print(" G: ");
                } else if (i == 1) {
                    GBS::ADC_ROFCTRL::write(GBS::ADC_ROFCTRL::read() + 1);
                    readout = GBS::ADC_ROFCTRL::read();
                    Serial.print(" R: ");
                } else if (i == 2) {
                    GBS::ADC_BOFCTRL::write(GBS::ADC_BOFCTRL::read() + 1);
                    readout = GBS::ADC_BOFCTRL::read();
                    Serial.print(" B: ");
                }
                Serial.print(readout, HEX);

                if (readout >= 0x52) {
                    // some kind of failure
                    break;
                }

                delay(10);
                hitTargetCounter = 0;
                missTargetCounter = 0;
                startTimer = millis(); // extend timer
            }
            if (hitTargetCounter > 1500) {
                break;
            }
        }
        if (i == 0) {
            // G done, prep R
            adco->g_off = GBS::ADC_GOFCTRL::read();
            GBS::ADC_GOFCTRL::write(0x7F);
            GBS::ADC_ROFCTRL::write(0x3D);
            GBS::DEC_TEST_SEL::write(2); // 5_1f = 0x2c
        }
        if (i == 1) {
            adco->r_off = GBS::ADC_ROFCTRL::read();
            GBS::ADC_ROFCTRL::write(0x7F);
            GBS::ADC_BOFCTRL::write(0x3D);
            GBS::DEC_TEST_SEL::write(3); // 5_1f = 0x3c
        }
        if (i == 2) {
            adco->b_off = GBS::ADC_BOFCTRL::read();
        }
        Serial.println("");
    }

    if (readout >= 0x52) {
        // there was a problem; revert
        adco->r_off = adco->g_off = adco->b_off = 0x40;
    }

    GBS::ADC_GOFCTRL::write(adco->g_off);
    GBS::ADC_ROFCTRL::write(adco->r_off);
    GBS::ADC_BOFCTRL::write(adco->b_off);
}

// --- MD plausibility watchdog: line count helpers ----------------------------------
// STATUS_SYNC_PROC_VTOTAL ("vt:" in the info line) as measured by the sync processor. Interlaced
// SD modes measure one field (about 262 NTSC, about 310 PAL), progressive EDTV modes a whole
// frame (about 524 for 480p, about 624 for 576p). A measurement chain that lost its reference
// reads far below all of these (vt:97..155 observed in the stuck state).
//
// The watchdog only acts on a reading that is provably wrong for the loaded mode:
//   * it lies in the band of a *different* mode family (the bands below do not overlap), or
//   * it is far below anything a real SD/EDTV source produces (garbage measured by a sync
//     processor that lost its reference).
// A reading outside every band but above the garbage limit (24 kHz and other unusual sources)
// is never acted on, so such sources keep behaving exactly as they did without the watchdog.
static const uint16_t MD_WATCHDOG_GARBAGE_BELOW = 200;

// Stable passes in one foreign mode family before the watchdog fast path applies that
// family's preset directly (while MD names no mode, or - one attempt per episode - while MD
// keeps misconfirming the loaded preset; see the fast path block below). 8 passes at the
// ~20ms syncwatcher rate is ~160ms, far below the 25-pass strike threshold, justified
// because the reading must not only be stable but land in a *known* band on every pass.
static const uint8_t MD_FAST_FORMAT_PASSES = 8;

// Implausible (or dead) passes after which the watchdog blanks the DAC output. A stuck-MD
// episode would otherwise push unsynced garbage through its whole detection and escalation
// window: with MD latched, the HSACT based sync loss detection never fires and nothing else
// stops the DAC. Blanking at the same pass count the fast path needs hides the entire
// watchdog transition behind clean black.
// The output comes back through the paths that already exist - doPostPresetLoadSteps()
// enables the DAC at its end (every preset application), the "stable again" branch
// re-enables what the source-gone handling blanked - plus one new one: a plausible reading
// (episode repaired in place) re-enables it directly, but only when this watchdog itself
// blanked it (see mdBlackoutOwned). Only the DAC is touched; sync out keeps running, so the
// display stays locked showing black instead of dropping to standby and back. (The longer
// source-gone blank also stops sync out - that is a different situation, it wants the
// display to power down.)
// 8 passes never trips on the 1-3 transient implausible passes a healthy format switch
// produces while the SP locks onto the new source (development logs showed a correct vt
// from the first pass after a preset load).
static const uint8_t MD_BLACKOUT_PASSES = 8;

// 0 = outside every band (HD modes etc. are not covered by this watchdog)
static uint8_t spLineCountFamily(uint16_t lineCount)
{
    if (lineCount >= 220 && lineCount <= 288) {
        return 1; // NTSC 480i / 240p
    }
    if (lineCount >= 292 && lineCount <= 352) {
        return 2; // PAL 576i / 288p
    }
    if (lineCount >= 495 && lineCount <= 552) {
        return 3; // 480p / EDTV 60
    }
    if (lineCount >= 595 && lineCount <= 655) {
        return 4; // 576p / EDTV 50
    }
    return 0;
}

// lineCount 0 is the sync processor's reset state and carries no information
static boolean spLineCountImplausibleForMode(uint8_t mode, uint16_t lineCount)
{
    if (lineCount == 0) {
        return false;
    }
    if (lineCount < MD_WATCHDOG_GARBAGE_BELOW) {
        return true;
    }
    uint8_t family = spLineCountFamily(lineCount);
    return family != 0 && family != mode;
}

void runSyncWatcher()
{
    if (!rto->boardHasPower) {
        return;
    }

    static uint8_t newVideoModeCounter = 0;
    static uint16_t activeStableLineCount = 0;
    static unsigned long lastSyncDrop = millis();
    static unsigned long lastLineCountMeasure = millis();
    // counts consecutive "no signal" probes that find the source completely inactive
    static uint8_t sourceGoneProbeCount = 0;
    // set once the MD unit has named a mode for the source that is currently attached, cleared by
    // the teardown below. Tells a source that never classified apart from one that classified
    // fine and is only glitching, which read identically to Block 2 below in a single pass.
    static boolean sourceWasClassified = false;
    // counts consecutive passes with an unknown mode while sync is active, and the latch it sets.
    // At function scope so the discrete VSync retry (Block 2) can drop the latch when it
    // reclassifies the source, and so the teardown can clear it for the next one.
    static uint8_t modeUnknownCounter = 0;
    static boolean crtcTimingHold = false;
    // one shot guard for the fallback preset used on a source the MD unit has no
    // profile for at all (see the fallback preset at noSyncCounter >= 200 below)
    static boolean fallbackPresetTried = false;
    // MD plausibility watchdog state (the block further down explains what these do). At
    // function scope, not block scope: Block 1 - the CRTC timing hold, which runs earlier
    // in every pass - needs to see whether a watchdog episode has already acted before it
    // decides to hold a mode-0 source.
    static uint8_t mdImplausibleCounter = 0;
    static uint8_t mdDeadCounter = 0; // consecutive passes with a dead vertical (vt 0)
    static uint8_t mdStuckResetRounds = 0;
    static uint8_t mdStuckTeardowns = 0;
    static unsigned long mdStuckQuietStart = 0;
    static uint32_t mdStuckQuietLen = 0;
    static uint8_t mdWatchedMode = 0;
    // fast path state - which foreign family a stuck-MD reading keeps landing in, how
    // many stable passes in a row it has been that family, and when the fast path last
    // applied a preset (ping-pong guard: at most one fast switch per 3 seconds).
    static uint8_t mdForeignFamily = 0;
    static uint8_t mdForeignStreak = 0;
    static unsigned long mdFastPathLastApply = 0;
    // one-shot latch for the misconfirm variant of the fast path. When MD actively keeps
    // naming the loaded (wrong) preset, a fast switch can fight the format change branch:
    // MD still names the old mode afterwards, the format change branch reloads it, and the
    // watchdog would measure the foreign family again. One attempt per episode; if it does
    // not stick, the strike escalation takes over. Cleared only by a plausible reading, a
    // stage-2 teardown or a lost source - NOT by a loaded-mode change, because that is
    // what the ping-pong produces.
    static uint8_t mdFastPathMisconfirmUsed = 0;
    // set while the MD watchdog itself holds the DAC dark (it wrote the 0). Only then may
    // the watchdog switch the output back on by itself - a DAC that the "source gone"
    // handling blanked belongs to that handling and its "stable again" re-enable.
    static boolean mdBlackoutOwned = false;
    // consecutive dead-vertical passes while the CRTC hold is latched
    static uint8_t crtcHoldVdeadCounter = 0;

    uint16_t thisStableLineCount = 0;
    uint8_t detectedVideoMode = getVideoMode();
    boolean status16SpHsStable = getStatus16SpHsStable();

    if (rto->outModeHdBypass && status16SpHsStable) {
        if (videoStandardInputIsPalNtscSd()) {
            if (millis() - lastLineCountMeasure > 765) {
                thisStableLineCount = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                for (uint8_t i = 0; i < 3; i++) {
                    delay(2);
                    if (GBS::STATUS_SYNC_PROC_VTOTAL::read() < (thisStableLineCount - 3) ||
                        GBS::STATUS_SYNC_PROC_VTOTAL::read() > (thisStableLineCount + 3)) {
                        thisStableLineCount = 0;
                        break;
                    }
                }

                if (thisStableLineCount != 0) {
                    if (thisStableLineCount < (activeStableLineCount - 3) ||
                        thisStableLineCount > (activeStableLineCount + 3)) {
                        activeStableLineCount = thisStableLineCount;
                        if (activeStableLineCount < 230 || activeStableLineCount > 340) {
                            // only doing NTSC/PAL currently, an unusual line count probably means a format change
                            setCsVsStart(1);
                            if (getCsVsStop() == 1) {
                                setCsVsStop(2);
                            }
                            // MD likes to get stuck as usual
                            nudgeMD();
                        } else {
                            setCsVsStart(thisStableLineCount - 9);
                        }

                        Serial.printf("HDBypass CsVsSt: %d\n", getCsVsStart());
                        delay(150);
                    }
                }

                lastLineCountMeasure = millis();
            }
        }
    }

    if (rto->videoStandardInput == 13) { // using flaky graphic modes
        if (detectedVideoMode == 0) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                detectedVideoMode = 13; // then keep it
            }
        }
    }

    static unsigned long preemptiveSogWindowStart = millis();
    static const uint16_t sogWindowLen = 3000; // ms
    static uint16_t badHsActive = 0;
    static boolean lastAdjustWasInActiveWindow = 0;

    if (rto->syncTypeCsync && !rto->inputIsYpBpR && (newVideoModeCounter == 0)) {
        // look for SOG instability
        if (GBS::STATUS_INT_SOG_BAD::read() == 1 || GBS::STATUS_INT_SOG_SW::read() == 1) {
            resetInterruptSogSwitchBit();
            if ((millis() - preemptiveSogWindowStart) > sogWindowLen) {
                // start new window
                preemptiveSogWindowStart = millis();
                badHsActive = 0;
            }
            lastVsyncLock = millis(); // best reset this
        }

        if ((millis() - preemptiveSogWindowStart) < sogWindowLen) {
            for (uint8_t i = 0; i < 16; i++) {
                if (GBS::STATUS_INT_SOG_BAD::read() == 1 || GBS::STATUS_SYNC_PROC_HSACT::read() == 0) {
                    resetInterruptSogBadBit();
                    uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
                    if (rto->videoStandardInput == 0)
                        hlowStart = 777; // fix initial state no HLOW_LEN
                    for (int a = 0; a < 20; a++) {
                        if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {
                            // okay, source still active so count this one, break back to outer for loop
                            badHsActive++;
                            lastVsyncLock = millis(); // delay this
                            //Serial.print(badHsActive); Serial.print(" ");
                            break;
                        }
                    }
                }
                if ((i % 3) == 0) {
                    delay(1);
                } else {
                    delay(0);
                }
            }

            if (badHsActive >= 17) {
                if (rto->currentLevelSOG >= 2) {
                    rto->currentLevelSOG -= 1;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(30);
                    updateSpDynamic(0);
                    badHsActive = 0;
                    lastAdjustWasInActiveWindow = 1;
                } else if (badHsActive > 40) {
                    optimizeSogLevel();
                    badHsActive = 0;
                    lastAdjustWasInActiveWindow = 1;
                }
                preemptiveSogWindowStart = millis(); // restart window
            }
        } else if (lastAdjustWasInActiveWindow) {
            lastAdjustWasInActiveWindow = 0;
            if (rto->currentLevelSOG >= 8) {
                rto->currentLevelSOG -= 1;
                setAndUpdateSogLevel(rto->currentLevelSOG);
                delay(30);
                updateSpDynamic(0);
                badHsActive = 0;
                rto->phaseIsSet = 0;
            }
        }
    }

    // Once a mode is set, rely on HSACT (not IF mode bits) for sync loss detection.
    // IF mode bits transiently clear during vsync when vsync extraction from csync isn't working.
    boolean rawSyncLoss = !status16SpHsStable ||
        (detectedVideoMode == 0 && rto->videoStandardInput == 0);

    // Debounce the HSACT-based sync loss on established SD/HD modes. getStatus16SpHsStable() is a
    // single un-debounced read of the HS-active flag, and on dark / near-black content (where
    // sync-on-green is marginal) that flag flickers for a frame. A single flaky read would flip
    // genuineSyncLoss true, which resets continousStableCounter, bumps noSyncCounter, freezes, and
    // makes the recovery ramp re-run optimizePhaseSP() — the sampling-phase shift shows up as
    // pixels / artefacts jumping around. Requiring a few consecutive bad reads rides out the glitch.
    // Only debounce for SP-driven modes (1..13); during mode search (0) and RGBHV (14/15) react
    // immediately as before.
    static uint8_t syncLossDebounce = 0;
    boolean genuineSyncLoss;
    if (rto->videoStandardInput >= 1 && rto->videoStandardInput <= 13) {
        if (rawSyncLoss) {
            if (syncLossDebounce < 255)
                syncLossDebounce++;
        } else {
            syncLossDebounce = 0;
        }
        genuineSyncLoss = (syncLossDebounce >= 3);
    } else {
        syncLossDebounce = 0;
        genuineSyncLoss = rawSyncLoss;
    }

    // --- Discrete VSync safeguards (additive only) ------------------------------------
    // csync vs. discrete sync classification stays with GBS::SP_SOG_MODE (the format-change
    // branch below and GBSSync.cpp). The two blocks below only add handling for cases where
    // that register-based classification alone is not enough, before falling through to the
    // noSyncCounter escalation below (which runs on the debounced genuineSyncLoss above, so
    // brief csync-SD glitches never reach it in the first place).

    // Block 2 (checked first - see note below): a source with separate H/V sync that
    // GBS::SP_SOG_MODE nonetheless classified as csync stays horizontally locked with the
    // picture rolling and mode never resolving (HV input gated off, vertical free running).
    // probeDiscreteVSync() can tell the two apart once H has locked, so retry here
    // periodically once the timing has settled.
    //
    // Must run before Block 1 (CRTC timing hold): that block returns early for as long as
    // the mode reads unknown, and a source misjudged as CSync reads unknown forever - so
    // checked second, this retry would never get a turn.
    //
    // A source that never classified is retried quickly (~3s). One that did and then
    // dropped to mode 0 is usually just glitching - 15kHz CSync sources do that routinely -
    // and probing them is actively harmful: the probe turns SOG slicing off for a third of
    // a second, which for a source whose sync only arrives that way is itself a sync
    // dropout. So a previously-classified source has to hold mode 0 far longer (~30s)
    // before it's worth disturbing. But not excluded outright, or a source that switches
    // to a mode the MD unit has no profile for (e.g. Atari ST 71Hz mono) would be stuck
    // with a rolling picture and no recovery short of a full teardown.
    {
        static uint16_t noModeWithCsyncCounter = 0;

        if (detectedVideoMode > 0) {
            sourceWasClassified = true;
        }

        if (status16SpHsStable && rto->syncTypeCsync && detectedVideoMode == 0 &&
            rto->videoStandardInput > 0 && rto->videoStandardInput < 14) {
            // syncwatcher runs roughly every 20ms: ~3s retry for a source that never classified,
            // ~30s for one that did and is presumably just glitching.
            uint16_t retryAfter = sourceWasClassified ? 1500 : 150;
            if (++noModeWithCsyncCounter >= retryAfter) {
                noModeWithCsyncCounter = 0;
                if (probeDiscreteVSync(NULL)) {
                    SerialM.println(F("CSync assumed, but source has discrete VSync > RGB/HV"));
                    rto->syncTypeCsync = false;
                    // the source is about to be driven from its own HV pins, so whatever Block 1
                    // latched while it was free running no longer describes anything - left set
                    // it would return early on every pass through mode 15 as well.
                    crtcTimingHold = false;
                    modeUnknownCounter = 0;
                    rto->videoStandardInput = 15;
                    applyPresets(rto->videoStandardInput);
                    delay(300);
                    return;
                }
            }
        } else {
            noModeWithCsyncCounter = 0;
        }
    }

    // Block 1: Detect CRTC timing changes (e.g. non-standard horizontal total) that confuse
    // mode detection while keeping HSACT active - e.g. Amstrad/Schneider CPC PAL, whose
    // long lines overflow the chip's 12-bit HTOTAL register. MODE then reads 0 while HSync
    // stays active - left alone, the escalation below would run, apply a wrong preset, and
    // corrupt the picture for as long as the anomaly lasts.
    // Guard with htotal: during a normal transient mode=0 (vsync extraction glitch on
    // csync) the PLLAD stays locked so htotal stays near its expected value; during a real
    // CRTC timing change the 12-bit register overflows and reads garbage (e.g. 9 for CPC
    // PAL's 3x-longer lines).
    // Do NOT freeze - by the time we detect the anomaly the frame buffer is already
    // corrupted. Do NOT funnel into genuineSyncLoss - that triggers noSyncCounter
    // escalation, updateSpDynamic, and ultimately a wrong preset being applied.
    // Instead: block all recovery machinery while the anomaly lasts, then on restore
    // trigger a targeted framesync + phase reset so the GBS re-locks cleanly to the
    // reverted timing.
    // Two escape hatches keep the hold from swallowing a dying measurement chain - a
    // stuck-MD episode ends up looking exactly like a CRTC anomaly here (mode 0, HS active,
    // HTOTAL decaying below 100), and the hold's early return would keep the MD watchdog,
    // which sits further down this function, from ever running its re-detection:
    //   1. The hold only latches - and only stays latched - while a vertical is being
    //      measured: a genuine CRTC anomaly keeps counting its lines, a dead chain reads
    //      VTOTAL 0. A held anomaly whose vertical goes dead is dropped, and the source
    //      handed to the watchdog below.
    //   2. An MD watchdog episode that has already ACTED (stage-1 resets done) means the
    //      measurement chain is known-bad; such a state is never held. Counting passes
    //      without acting must not count as "acting": a fallback-preset source that can
    //      never classify (the CPC itself) measures implausibly for the 480p fallback until
    //      this hold latches at pass 5, long before the watchdog's own 25-pass threshold.
    {
        boolean mdEpisodeHot = (mdStuckResetRounds != 0); // a watchdog episode has already acted

        if (crtcTimingHold) {
            if (!genuineSyncLoss && detectedVideoMode == 0) {
                // a held anomaly that stops measuring a vertical is not an anomaly - the
                // chain died while holding. Drop the hold and hand the source to the
                // watchdog below instead of holding a dead state forever. A genuine CRTC
                // anomaly keeps its vertical, so it keeps holding.
                if (GBS::STATUS_SYNC_PROC_VTOTAL::read() == 0) {
                    if (crtcHoldVdeadCounter < 255) {
                        crtcHoldVdeadCounter++;
                    }
                    if (crtcHoldVdeadCounter >= 25) { // ~0.5s of dead vertical is not a glitch
                        crtcHoldVdeadCounter = 0;
                        crtcTimingHold = false;
                        modeUnknownCounter = 0;
                        SerialM.println(F("CRTC hold dropped: vertical gone dead, MD watchdog takes over"));
                        // deliberately no return: the pass continues into the watchdog below
                    }
                } else {
                    crtcHoldVdeadCounter = 0;
                }
                if (crtcTimingHold) {
                    return; // mode still unknown: skip all recovery machinery
                }
            }
            // Mode returned to a valid value (or genuine sync loss) — fall through to normal handling
            crtcTimingHold = false;
            modeUnknownCounter = 0;
            if (!genuineSyncLoss) {
                SerialM.println(F("CRTC timing restored"));
                rto->phaseIsSet = 0;
                FrameSync::resetWithoutRecalculation();
            }
        }

        if (!genuineSyncLoss && !crtcTimingHold && detectedVideoMode == 0 &&
            rto->videoStandardInput > 0 && rto->videoStandardInput < 14 &&
            GBS::STATUS_SYNC_PROC_HTOTAL::read() < 100) {
            if (modeUnknownCounter < 255) {
                modeUnknownCounter++;
            }
            if (modeUnknownCounter == 5) {
                if (mdEpisodeHot || GBS::STATUS_SYNC_PROC_VTOTAL::read() == 0) {
                    // not a CRTC anomaly - the watchdog has already acted on this source,
                    // or nothing is being measured at all. Say so once (the counter
                    // saturates instead of resetting) and let the pass run on into the
                    // watchdog below, whose dead handling escalates to the full
                    // re-detection.
                    SerialM.println(F("\nmode unknown while sync active, chain looks dead - no CRTC hold"));
                } else {
                    SerialM.println(F("\nmode unknown while sync active (CRTC timing change?)"));
                    crtcTimingHold = true;
                    modeUnknownCounter = 0;
                    return; // skip recovery machinery for this iteration
                }
            }
        } else if (!crtcTimingHold) {
            modeUnknownCounter = 0;
        }
    }

    if (genuineSyncLoss && rto->videoStandardInput != 15) {
        rto->noSyncCounter++;
        rto->continousStableCounter = 0;
        lastVsyncLock = millis(); // best reset this
        if (rto->noSyncCounter == 1) {
            freezeVideo();
            return; // do nothing else
        }

        rto->phaseIsSet = 0;

        if (rto->noSyncCounter <= 3 || GBS::STATUS_SYNC_PROC_HSACT::read() == 0) {
            freezeVideo();
        }

        if (newVideoModeCounter == 0) {
            LEDOFF; // LEDOFF on sync loss

            if (rto->noSyncCounter == 2) { // this usually repeats
                //printInfo(); printInfo(); SerialM.println();
                //rto->printInfos = 0;
                if ((millis() - lastSyncDrop) > 1500) { // minimum space between runs
                    if (rto->printInfos == false) {
                        SerialM.print("\n.");
                    }
                } else {
                    if (rto->printInfos == false) {
                        SerialM.print(".");
                    }
                }

                // if sog is lowest, adjust up
                if (rto->currentLevelSOG <= 1 && videoStandardInputIsPalNtscSd()) {
                    rto->currentLevelSOG += 1;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(30);
                }
                lastSyncDrop = millis(); // restart timer
            }
        }

        if (rto->noSyncCounter == 8) {
            GBS::SP_H_CST_ST::write(0x10);
            GBS::SP_H_CST_SP::write(0x100);
            //GBS::SP_H_PROTECT::write(1);  // at noSyncCounter = 32 will alternate on / off
            if (videoStandardInputIsPalNtscSd()) {
                // this can early detect mode changes (before updateSpDynamic resets all)
                GBS::SP_PRE_COAST::write(9);
                GBS::SP_POST_COAST::write(9);
                // new: test SD<>EDTV changes
                uint8_t ignore = GBS::SP_H_PULSE_IGNOR::read();
                if (ignore >= 0x33 && !rto->syncTypeCsync) {
                    GBS::SP_H_PULSE_IGNOR::write(ignore / 2);
                }
            }
            rto->coastPositionIsSet = 0;
        }

        if (rto->noSyncCounter % 27 == 0) {
            // the * check needs to be first (go before auto sog level) to support SD > HDTV detection
            SerialM.print("*");
            updateSpDynamic(1);
        }

        if (rto->noSyncCounter % 32 == 0) {
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                unfreezeVideo();
            } else {
                freezeVideo();
            }
        }

        if (rto->inputIsYpBpR && (rto->noSyncCounter == 34)) {
            GBS::SP_NO_CLAMP_REG::write(1); // unlock clamp
            rto->clampPositionIsSet = false;
        }

        if (rto->noSyncCounter == 38) {
            nudgeMD();
        }

        // HSync is solidly present but no video mode was ever recognised. Nothing has
        // applied a preset, so the PLL never locks, and that's a dead end in both
        // directions: the format-change branch needs a detected mode before it runs, and
        // probeDiscreteVSync() needs H lock before it can answer. Load the fallback preset
        // once to establish lock and let the normal machinery continue. Reached by anything
        // the MD unit has no profile for, e.g. Atari ST high res at 35.7kHz.
        // 200, not a round 300: the discrete VSync retry above is the last thing that can
        // still make the MD unit name this source, and it has had a full second by then.
        // Firing early is cheap even when it's wrong: the format-change branch replaces the
        // fallback preset as soon as a real mode does turn up.
        if (rto->noSyncCounter >= 200 && !fallbackPresetTried && rto->videoStandardInput == 0 &&
            GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
            fallbackPresetTried = true;
            SerialM.println(F("\nHSync present but no mode found, applying fallback preset"));
            rto->noSyncCounter = 0;
            applyPresets(0); // overrides to 480p60 and re-checks the sync type
            delay(300);

            // The preset has just given the PLL the H lock that probeDiscreteVSync() needs,
            // and this is the one place that knows a probe is worth taking right now. The
            // sync type applyPresets() decided a moment ago cannot be trusted - it probed
            // before this preset existed, with no lock, where the answer can only ever be
            // "none". Asking again here saves the ~3s the periodic retry above would
            // otherwise spend; a source that really is CSync answers "none" and loses only
            // the probe's own third of a second, and the periodic retry still covers the
            // case where the lock needs longer.
            if (rto->syncTypeCsync && probeDiscreteVSync(NULL)) {
                SerialM.println(F("fallback preset locked, source has discrete VSync > RGB/HV"));
                rto->syncTypeCsync = false;
                crtcTimingHold = false;
                modeUnknownCounter = 0;
                rto->videoStandardInput = 15;
                applyPresets(rto->videoStandardInput);
                delay(300);
            }
            return;
        }

        if (rto->syncTypeCsync) {
            if (rto->noSyncCounter > 47) {
                if (rto->noSyncCounter % 16 == 0) {
                    GBS::SP_H_PROTECT::write(!GBS::SP_H_PROTECT::read());
                }
            }
        }

        if (rto->noSyncCounter % 150 == 0) {
            if (rto->noSyncCounter == 150 || rto->noSyncCounter % 900 == 0) {
                SerialM.print("\nno signal\n");
                // check whether discrete VSync is present. if so, need to go to input detect
                uint8_t extSyncBackup = GBS::SP_EXT_SYNC_SEL::read();
                GBS::SP_EXT_SYNC_SEL::write(0);
                delay(240);
                printInfo();
                if (GBS::STATUS_SYNC_PROC_VSACT::read() == 1) {
                    delay(10);
                    if (GBS::STATUS_SYNC_PROC_VSACT::read() == 1) {
                        rto->noSyncCounter = 0x07fe;
                    }
                }
                GBS::SP_EXT_SYNC_SEL::write(extSyncBackup);
            }
            GBS::SP_H_COAST::write(0);   // 5_3e 2
            GBS::SP_H_PROTECT::write(0); // 5_3e 4
            GBS::SP_H_CST_ST::write(0x10);
            GBS::SP_H_CST_SP::write(0x100); // instead of disabling 5_3e 5 coast
            GBS::SP_CS_CLP_ST::write(32);   // neutral clamp values
            GBS::SP_CS_CLP_SP::write(48);   //
            // ... which the flag has to follow. Left set, it claims the clamp is positioned while
            // the registers say otherwise, and the retry in doPostPresetLoadSteps()/runSyncWatcher
            // only runs while it is clear - so a returning source gets its picture back with this
            // neutral window still in place, and keeps it until the next occasional refresh.
            rto->clampPositionIsSet = false;
            updateSpDynamic(1);
            nudgeMD(); // can fix MD not noticing a line count update
            delay(80);

            // prepare optimizeSogLevel
            // use STATUS_SYNC_PROC_HLOW_LEN changes to determine whether source is still active
            uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
            if (GBS::PLLAD_VCORST::read() == 1) {
                // exception: we're in startup and pllad isn't locked yet > HLOW_LEN always 0
                hlowStart = 777; // now it'll run optimizeSogLevel if needed
            }
            boolean sourceStillActive = false;
            for (int a = 0; a < 128; a++) {
                if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {
                    // source still there
                    sourceStillActive = true;
                    if (rto->noSyncCounter % 450 == 0) {
                        rto->currentLevelSOG = 0; // worst case, sometimes necessary, will be unstable but at least detect
                        setAndUpdateSogLevel(rto->currentLevelSOG);
                    } else {
                        optimizeSogLevel();
                    }
                    break;
                } else if (a == 127) {
                    // set sog to be able to see something
                    rto->currentLevelSOG = 5;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                }
                delay(0);
            }

            // The HLOW_LEN probe above found no source activity at all. freezeVideo() only stops
            // capture — it keeps the last (frequently corrupted) frame on the DAC output, so when
            // the source is simply switched off that garbage frame stays on screen indefinitely.
            // Require two consecutive probes to agree the source is gone (guards against a single
            // momentarily-static HLOW_LEN reading), then just blank the DAC so nothing is shown.
            // Do NOT tear down to low power / reset parameters here: that wipes videoStandardInput
            // and the tuned SOG/sync config, forcing a slow full re-detection when the source comes
            // back. Keeping the config lets the normal recovery path re-lock in place quickly; the
            // DAC is re-enabled again in the "mode stable again" branch below.
            if (sourceStillActive) {
                sourceGoneProbeCount = 0;
            } else if (sourceGoneProbeCount < 255) {
                sourceGoneProbeCount++;
            }
            if (sourceGoneProbeCount >= 2) {
                // from here on the dark output belongs to this handling, even if the MD
                // watchdog blanked it first (see mdBlackoutOwned)
                mdBlackoutOwned = false;
            }
            if (sourceGoneProbeCount >= 2 && GBS::DAC_RGBS_PWDNZ::read() == 1) {
                SerialM.println(F("source gone, blanking output"));
                GBS::DAC_RGBS_PWDNZ::write(0); // black output instead of a frozen garbage frame
                // Blanking alone leaves sync going out, so the display sits there showing black
                // rather than dropping into standby - for the ~40s until the teardown at 0x07fe
                // stops it. Nothing is being displayed anyway, so stop sync too and let the
                // display power down. Both come back together in the "stable again" branch below.
                if (!uopt->wantOutputComponent) {
                    GBS::PAD_SYNC_OUT_ENZ::write(1); // disable sync out
                }
            }

            resetSyncProcessor();
            delay(8);
            resetModeDetect();
            delay(8);
        }

        // long no signal time, check other input
        if (rto->noSyncCounter % 413 == 0) {
            if (GBS::ADC_INPUT_SEL::read() == 1) {
                GBS::ADC_INPUT_SEL::write(0);
            } else {
                GBS::ADC_INPUT_SEL::write(1);
            }
            delay(40);
            unsigned long timeout = millis();
            while (millis() - timeout <= 210) {
                if (getStatus16SpHsStable()) {
                    rto->noSyncCounter = 0x07fe; // will cause a return
                    break;
                }
                handleWiFi(0);
                delay(1);
            }

            if (millis() - timeout > 210) {
                if (GBS::ADC_INPUT_SEL::read() == 1) {
                    GBS::ADC_INPUT_SEL::write(0);
                } else {
                    GBS::ADC_INPUT_SEL::write(1);
                }
            }
        }

        newVideoModeCounter = 0;
        // sog unstable check end
    }

    // if format changed to valid, potentially new video mode
    if (((detectedVideoMode != 0 && detectedVideoMode != rto->videoStandardInput) ||
         (detectedVideoMode != 0 && rto->videoStandardInput == 0)) &&
        rto->videoStandardInput != 15) {
        // before thoroughly checking for a mode change, watch format via newVideoModeCounter
        if (newVideoModeCounter < 255) {
            newVideoModeCounter++;
            rto->continousStableCounter = 0; // usually already 0, but occasionally not
            if (newVideoModeCounter > 1) {   // help debug a few commits worth
                if (newVideoModeCounter == 2) {
                    SerialM.println();
                }
                SerialM.print(newVideoModeCounter);
            }
            if (newVideoModeCounter == 3) {
                freezeVideo();
                GBS::SP_H_CST_ST::write(0x10);
                GBS::SP_H_CST_SP::write(0x100);
                rto->coastPositionIsSet = 0;
                delay(10);
                if (getVideoMode() == 0) {
                    updateSpDynamic(1); // check ntsc to 480p and back
                    delay(40);
                }
            }
        }

        if (newVideoModeCounter >= 8) {
            uint8_t vidModeReadout = 0;
            SerialM.print(F("\nFormat change:"));
            for (int a = 0; a < 30; a++) {
                vidModeReadout = getVideoMode();
                if (vidModeReadout == 13) {
                    newVideoModeCounter = 5;
                } // treat ps2 quasi rgb as stable
                if (vidModeReadout != detectedVideoMode) {
                    newVideoModeCounter = 0;
                }
            }
            if (newVideoModeCounter != 0) {
                // apply new mode
                SerialM.print(" ");
                SerialM.print(vidModeReadout);
                SerialM.println(F(" <stable>"));
                //Serial.print("Old: "); Serial.print(rto->videoStandardInput);
                //Serial.print(" New: "); Serial.println(detectedVideoMode);
                rto->videoIsFrozen = false;

                if (GBS::SP_SOG_MODE::read() == 1) {
                    rto->syncTypeCsync = true;
                } else {
                    rto->syncTypeCsync = false;
                }
                boolean wantPassThroughMode = uopt->presetPreference == 10;

                if (((rto->videoStandardInput == 1 || rto->videoStandardInput == 3) && (detectedVideoMode == 2 || detectedVideoMode == 4)) ||
                    rto->videoStandardInput == 0 ||
                    ((rto->videoStandardInput == 2 || rto->videoStandardInput == 4) && (detectedVideoMode == 1 || detectedVideoMode == 3))) {
                    rto->useHdmiSyncFix = 1;
                    //SerialM.println("hdmi sync fix: yes");
                } else {
                    rto->useHdmiSyncFix = 0;
                    //SerialM.println("hdmi sync fix: no");
                }

                if (!wantPassThroughMode) {
                    // needs to know the sync type for early updateclamp (set above)
                    applyPresets(detectedVideoMode);
                } else {
                    rto->videoStandardInput = detectedVideoMode;
                    setOutModeHdBypass(false);
                }
                rto->videoStandardInput = detectedVideoMode;
                rto->noSyncCounter = 0;
                rto->continousStableCounter = 0; // also in postloadsteps
                newVideoModeCounter = 0;
                activeStableLineCount = 0;
                delay(20); // post delay
                badHsActive = 0;
                preemptiveSogWindowStart = millis();
            } else {
                unfreezeVideo(); // (whops)
                SerialM.print(" ");
                SerialM.print(vidModeReadout);
                SerialM.println(F(" <not stable>"));
                printInfo();
                newVideoModeCounter = 0;
                if (rto->videoStandardInput == 0) {
                    // if we got here from standby mode, return there soon
                    // but occasionally, this is a regular new mode that needs a SP parameter change to work
                    // ie: 1080p needs longer post coast, which the syncwatcher loop applies at some point
                    rto->noSyncCounter = 0x05ff; // give some time in normal loop
                }
            }
        }
    // The second term is the same exception updateClampPosition() makes: the MD unit's mode bits
    // stay 0 for some csync SD sources however long they run, so requiring a named mode here means
    // continousStableCounter never leaves 0 for them. Everything that waits on that counter then
    // never happens - the clamp retry at 4, autoBestHtotal at 10, frame time lock at 20, the post
    // preset stage that syncs the external clock generator at 35, auto gain at 90 - while the
    // picture itself is perfectly stable. Sync loss still resets the counter through the branches
    // above, and HSACT going away takes getStatus16SpHsStable() with it, so an absent source
    // cannot accumulate here.
    // Gated on genuineSyncLoss rather than noSyncCounter: the counter is only cleared inside this
    // branch, so testing it here would let the first sync blip lock a mode-less source out of the
    // branch permanently - it could never get back in to clear what keeps it out.
    } else if (getStatus16SpHsStable() && rto->videoStandardInput != 15 &&
               ((detectedVideoMode != 0 && rto->videoStandardInput == detectedVideoMode) ||
                (rto->syncTypeCsync && videoStandardInputIsPalNtscSd() && !genuineSyncLoss))) {
        // last used mode reappeared / stable again
        if (rto->continousStableCounter < 255) {
            rto->continousStableCounter++;
        }

        static boolean doFullRestore = 0;
        if (rto->noSyncCounter >= 150) {
            // source was gone for longer // clamp will be updated at continousStableCounter 50
            rto->coastPositionIsSet = false;
            rto->phaseIsSet = false;
            FrameSync::reset(uopt->frameTimeLockMethod);
            doFullRestore = 1;
            SerialM.println();
        }

        rto->noSyncCounter = 0;
        sourceGoneProbeCount = 0; // source is back, forget any pending "source gone" probes
        fallbackPresetTried = false; // re-arm for the next time a source turns up unclassifiable
        newVideoModeCounter = 0;

        if (rto->continousStableCounter == 1 && !doFullRestore) {
            rto->videoIsFrozen = true; // ensures unfreeze
            unfreezeVideo();
        }

        if (rto->continousStableCounter == 2) {
            updateSpDynamic(0);
            if (doFullRestore) {
                delay(20);
                optimizeSogLevel();
                doFullRestore = 0;
            }
            rto->videoIsFrozen = true; // ensures unfreeze
            unfreezeVideo();           // called 2nd time here to make sure
            if (GBS::DAC_RGBS_PWDNZ::read() == 0) {
                // re-enable whatever the "source gone" handling above blanked, now that the
                // source is confirmed back and stable - but never while an MD watchdog
                // episode holds the output dark. In the misconfirm case MD keeps naming the
                // loaded mode, so this very branch counts its stability passes straight
                // through such an episode, and a counter reset in between (sync blip,
                // format change counting) would let the == 2 case flash the garbage back
                // on behind the watchdog's back. The episode re-enables the output itself:
                // on a plausible reading, or through the preset application every
                // escalation path ends in.
                if (mdImplausibleCounter < MD_BLACKOUT_PASSES && mdDeadCounter < MD_BLACKOUT_PASSES &&
                    mdStuckResetRounds == 0) {
                    GBS::DAC_RGBS_PWDNZ::write(1); // re-enable output if it was blanked on source loss
                    if (!uopt->wantOutputComponent) {
                        GBS::PAD_SYNC_OUT_ENZ::write(0); // and sync with it, if that was stopped too
                    }
                }
            }
        }

        if (rto->continousStableCounter == 4) {
            LEDON;
        }

        if (!rto->phaseIsSet) {
            if (rto->continousStableCounter >= 10 && rto->continousStableCounter < 61) {
                // added < 61 to make a window, else sources with little pll lock hammer this
                if ((rto->continousStableCounter % 10) == 0) {
                    rto->phaseIsSet = optimizePhaseSP();
                }
            }
        }

        // 5_3e 2 SP_H_COAST test
        //if (rto->continousStableCounter == 11) {
        //  if (rto->coastPositionIsSet) {
        //    GBS::SP_H_COAST::write(1);
        //  }
        //}

        if (rto->continousStableCounter == 160) {
            resetInterruptSogBadBit();
        }

        if (rto->continousStableCounter == 45) {
            GBS::ADC_UNUSED_67::write(0); // clear sync fix temp registers (67/68)
            //rto->coastPositionIsSet = 0; // leads to a flicker
            rto->clampPositionIsSet = 0; // run updateClampPosition occasionally
        }

        if (rto->continousStableCounter % 31 == 0) {
            // new: 8 regular interval checks up until 255
            updateSpDynamic(0);
        }

        if (rto->continousStableCounter >= 3) {
            if ((rto->videoStandardInput == 1 || rto->videoStandardInput == 2) &&
                !rto->outModeHdBypass && rto->noSyncCounter == 0) {
                // deinterlacer and scanline code
                static uint8_t timingAdjustDelay = 0;
                static uint8_t oddEvenWhenArmed = 0;
                boolean preventScanlines = 0;

                if (rto->deinterlaceAutoEnabled) {
                    uint16_t VPERIOD_IF = GBS::VPERIOD_IF::read();
                    static uint8_t filteredLineCountMotionAdaptiveOn = 0, filteredLineCountMotionAdaptiveOff = 0;
                    static uint16_t VPERIOD_IF_OLD = VPERIOD_IF; // for glitch filter

                    if (VPERIOD_IF_OLD != VPERIOD_IF) {
                        //freezeVideo(); // glitch filter
                        preventScanlines = 1;
                        filteredLineCountMotionAdaptiveOn = 0;
                        filteredLineCountMotionAdaptiveOff = 0;
                        if (uopt->enableFrameTimeLock || rto->extClockGenDetected) {
                            if (uopt->deintMode == 1) { // using bob
                                timingAdjustDelay = 11; // arm timer (always)
                                oddEvenWhenArmed = VPERIOD_IF % 2;
                            }
                        }
                    }

                    // csync SD sources have unreliable VPERIOD_IF (no discrete vsync, VSACT=0)
                    // — require more stable readings before toggling deinterlacer, and skip
                    // timingAdjustDelay (clock recalibration based on noisy data causes jumps)
                    const uint8_t deintStableThreshold = rto->syncTypeCsync ? 8 : 2;

                    if (VPERIOD_IF == 522 || VPERIOD_IF == 524 || VPERIOD_IF == 526 ||
                        VPERIOD_IF == 622 || VPERIOD_IF == 624 || VPERIOD_IF == 626) { // ie v:524, even counts > enable
                        filteredLineCountMotionAdaptiveOn++;
                        filteredLineCountMotionAdaptiveOff = 0;
                        if (filteredLineCountMotionAdaptiveOn >= deintStableThreshold)
                        {
                            if (uopt->deintMode == 0 && !rto->motionAdaptiveDeinterlaceActive) {
                                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) { // don't rely on rto->scanlinesEnabled
                                    disableScanlines();
                                }
                                enableMotionAdaptDeinterlace();
                                if (!rto->syncTypeCsync) {
                                    if (timingAdjustDelay == 0) {
                                        timingAdjustDelay = 11; // arm timer only if it's not already armed
                                        oddEvenWhenArmed = VPERIOD_IF % 2;
                                    } else {
                                        timingAdjustDelay = 0; // cancel timer
                                    }
                                }
                                preventScanlines = 1;
                            }
                            filteredLineCountMotionAdaptiveOn = 0;
                        }
                    } else if (VPERIOD_IF == 521 || VPERIOD_IF == 523 || VPERIOD_IF == 525 ||
                               VPERIOD_IF == 623 || VPERIOD_IF == 625 || VPERIOD_IF == 627) { // ie v:523, uneven counts > disable
                        filteredLineCountMotionAdaptiveOff++;
                        filteredLineCountMotionAdaptiveOn = 0;
                        if (filteredLineCountMotionAdaptiveOff >= deintStableThreshold)
                        {
                            if (uopt->deintMode == 0 && rto->motionAdaptiveDeinterlaceActive) {
                                disableMotionAdaptDeinterlace();
                                if (!rto->syncTypeCsync) {
                                    if (timingAdjustDelay == 0) {
                                        timingAdjustDelay = 11; // arm timer only if it's not already armed
                                        oddEvenWhenArmed = VPERIOD_IF % 2;
                                    } else {
                                        timingAdjustDelay = 0; // cancel timer
                                    }
                                }
                            }
                            filteredLineCountMotionAdaptiveOff = 0;
                        }
                    } else {
                        filteredLineCountMotionAdaptiveOn = filteredLineCountMotionAdaptiveOff = 0;
                    }

                    VPERIOD_IF_OLD = VPERIOD_IF; // part of glitch filter

                    if (uopt->deintMode == 1) { // using bob
                        if (rto->motionAdaptiveDeinterlaceActive) {
                            disableMotionAdaptDeinterlace();
                            FrameSync::reset(uopt->frameTimeLockMethod);
                            GBS::GBS_RUNTIME_FTL_ADJUSTED::write(1);
                            lastVsyncLock = millis();
                        }
                        if (uopt->wantScanlines && !rto->scanlinesEnabled) {
                            enableScanlines();
                        } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                            disableScanlines();
                        }
                    }

                    // timing adjust after a few stable cycles
                    // should arrive here with either odd or even VPERIOD_IF
                    /*if (timingAdjustDelay != 0) {
            Serial.print(timingAdjustDelay); Serial.print(" ");
          }*/
                    if (timingAdjustDelay != 0) {
                        if ((VPERIOD_IF % 2) == oddEvenWhenArmed) {
                            timingAdjustDelay--;
                            if (timingAdjustDelay == 0) {
                                if (uopt->enableFrameTimeLock) {
                                    FrameSync::reset(uopt->frameTimeLockMethod);
                                    GBS::GBS_RUNTIME_FTL_ADJUSTED::write(1);
                                    delay(10);
                                    lastVsyncLock = millis();
                                }
                                externalClockGenSyncInOutRate();
                            }
                        }
                        /*else {
              Serial.println("!!!");
            }*/
                    }
                }

                // scanlines
                if (uopt->wantScanlines) {
                    // switching scanlines off is done where wantScanlines is cleared
                    // (serial command '7', OSD scanlines handler) and in the bob path above
                    if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive && !preventScanlines) {
                        enableScanlines();
                    }
                }
            }
        }
    }

    // --- MD plausibility watchdog for modes 1..4 ---------------------------------------
    // Failure mode (observed on hardware, Wii): after an SD/EDTV format switch the mode
    // detect unit (MD) can get stuck. HSync stays active, so the HSACT based sync loss
    // detection above never fires, while MD names either the old mode or no mode at all and
    // the sync processor measures garbage (vt:97 instead of 310, h:214 instead of 431). The
    // format change branch needs a differently named mode and the recovery machinery needs
    // a sync loss, so nothing intervenes. The stuck state is not even stable: over a second
    // or two the measurement chain decays completely (st00 VT_OK 1->0, VT_BAD set, all MD
    // classification bits 0, VTOTAL finally reading 0, HTOTAL falling below 100) - and the
    // CRTC timing hold further up would happily latch that dead state. Without this
    // watchdog, only power cycling restored the picture.
    //
    // Idea: while sync is stably active with no genuine sync loss, a *stable* line count
    // that is provably wrong for the loaded mode (see spLineCountImplausibleForMode) means
    // the measurement chain does not match the source. Repair it, escalating in two stages:
    //   stage 1 (twice): resetSyncProcessor + resetModeDetect with settle delays - the same
    //                    sequence the sync loss recovery above uses - plus detection
    //                    friendly SP parameters and a double pulse of the MD VS flip
    //                    (nudgeMD, so the flip ends up as it was). A legitimate source is
    //                    simply re-classified as the same mode.
    //   stage 2:         full re-detection with setResetParameters + prepareSyncProcessor +
    //                    resetSyncProcessor + resetModeDetect, i.e. the teardown the RGB/HV
    //                    branch uses at limitNoSync. inputAndSyncDetect() then brings the
    //                    source up from scratch, which is what power cycling does.
    // Safeguards (stability of working sources is the priority):
    //   * only while HS is stably active and no genuine sync loss is pending
    //   * only when the MD confirms the loaded mode or names no mode at all - a differently
    //     named mode belongs to the format change branch above
    //   * the line count must be provably wrong and stable over 4 reads (+/-3); a jumping
    //     reading is skipped (neither counts nor clears), a zero reading counts as a dead
    //     chain once it persists for 25 passes while HS stays stable
    //   * in normal operation the check is one register read per pass; the confirmation
    //     reads (with their delays) only happen for a reading that already looks wrong
    //   * 25 stable implausible passes before every action (runSyncWatcher() runs at most
    //     every 20 ms, so that takes at least half a second)
    //   * any plausible reading clears the whole episode; a change of the loaded mode (new
    //     source, preset selection) restarts the watchdog from scratch
    //   * after three stage-2 teardowns without improvement it stays quiet for 10 minutes
    //     instead of fighting a source it cannot fix
    //
    // Fast path: with MD naming no mode (or, one attempt per episode, misconfirming the
    // loaded preset), a reading that is stable AND keeps landing in one other mode family's
    // band is the SP truthfully describing the new source. After MD_FAST_FORMAT_PASSES
    // stable passes, apply that family's preset directly - the same sequence the format
    // change branch runs - instead of striking, resetting and re-detecting from scratch.
    // At most one fast switch per 3 seconds; a wrong guess simply falls through to the
    // normal escalation below afterwards.
    //
    // Blackout: from MD_BLACKOUT_PASSES stable implausible (or dead) passes on, the DAC is
    // blanked, so the strike count, the reset rounds and the re-detection all run behind
    // black instead of unsynced garbage. Re-enabled by the existing output-on paths (the
    // end of doPostPresetLoadSteps() behind every preset application, and the "stable
    // again" branch - which never lifts a blackout while an episode is active, see its
    // exception there) plus the plausible-reading re-enable, which returns the picture as
    // soon as a stage-1 repair measures the loaded mode again, but only when this watchdog
    // blanked it (mdBlackoutOwned). The quiet periods (10s breather, 10min give-up) leave
    // the output as the last action produced it: a working re-detection ends in a preset
    // application and shows the new picture, a dead one stays dark.
    {
        // (the watchdog state variables live at function scope so that Block 1 above can
        // see whether an episode has already acted - declared next to fallbackPresetTried)

        if (!genuineSyncLoss && rto->videoStandardInput >= 1 && rto->videoStandardInput <= 4) {
            if (mdWatchedMode != rto->videoStandardInput) {
                // new loaded mode: judge it fresh
                mdWatchedMode = rto->videoStandardInput;
                mdImplausibleCounter = 0;
                mdDeadCounter = 0;
                mdStuckResetRounds = 0;
                mdStuckTeardowns = 0;
                mdStuckQuietLen = 0;
                mdForeignFamily = 0;
                mdForeignStreak = 0;
            }

            // the blind spot: MD confirms the loaded mode, or names no mode at all
            boolean mdLatched =
                (detectedVideoMode == rto->videoStandardInput) || (detectedVideoMode == 0);

            if (!status16SpHsStable || !mdLatched ||
                ((millis() - mdStuckQuietStart) < mdStuckQuietLen)) {
                mdImplausibleCounter = 0;
                mdDeadCounter = 0;
                mdForeignFamily = 0;
                mdForeignStreak = 0;
            } else {
                uint16_t lineCount = GBS::STATUS_SYNC_PROC_VTOTAL::read();

                boolean strike = false;

                if (lineCount == 0) {
                    // a dead vertical is not a format hint: it breaks any fast path streak
                    mdForeignFamily = 0;
                    mdForeignStreak = 0;
                    // a single zero reading is still "no information" (the SP sits in
                    // reset for a moment right after a preset load). But persistent zeros
                    // with a stable HS - VTOTAL dead while HS stays active - are evidence
                    // of a dead chain, so count them like an implausible reading; 25 of
                    // them strike, and the shared escalation does the rest.
                    if (mdDeadCounter < 255) {
                        mdDeadCounter++;
                    }
                    // a dead chain holds the last captured frame on the DAC - frozen
                    // garbage. Once the dead state has outlasted any SP reset transient,
                    // blank the output like the implausible case below does. The write and
                    // its log happen once; the read() gate stays closed until one of the
                    // re-enable paths fires.
                    if (mdDeadCounter >= MD_BLACKOUT_PASSES &&
                        GBS::DAC_RGBS_PWDNZ::read() == 1) {
                        SerialM.println(F("MD watchdog: dead chain - blanking output"));
                        GBS::DAC_RGBS_PWDNZ::write(0);
                        mdBlackoutOwned = true;
                    }
                    if (mdDeadCounter >= 25) {
                        mdDeadCounter = 0;
                        strike = true;
                        SerialM.print(F("\nMD plausibility watchdog: no vertical measured while mode "));
                        SerialM.print(rto->videoStandardInput);
                        SerialM.println(F(" is loaded (dead chain)"));
                    }
                } else if (!spLineCountImplausibleForMode(rto->videoStandardInput, lineCount)) {
                    // plausible (or at least not provably wrong): forget the whole episode,
                    // including the misconfirm latch - the loaded mode measures correctly
                    // now, so a future misconfirm deserves a fresh attempt.
                    // A plausible reading is also the earliest point at which a repair paid
                    // off - stage 1 reset the SP and MD around a correctly loaded source and
                    // the chain now measures it again. Bring the picture back right here
                    // instead of waiting for the "stable again" passes - but only when THIS
                    // watchdog blanked the output (mdBlackoutOwned): a DAC that the
                    // source-gone handling blanked must stay dark until its "stable again"
                    // branch has finished the restore (SOG optimisation, unfreeze) -
                    // switching it on earlier would show the frozen last frame of the
                    // vanished source. The flag keeps the normal-operation cost at one
                    // register read per pass. A flag left over from a blackout that a
                    // preset application ended is harmless: the read finds the DAC on,
                    // and the flag is dropped.
                    if (mdBlackoutOwned) {
                        mdBlackoutOwned = false;
                        if (GBS::DAC_RGBS_PWDNZ::read() == 0) {
                            SerialM.println(F("MD watchdog: measurement plausible again - output back on"));
                            GBS::DAC_RGBS_PWDNZ::write(1);
                            if (!uopt->wantOutputComponent) {
                                GBS::PAD_SYNC_OUT_ENZ::write(0);
                            }
                        }
                    }
                    mdImplausibleCounter = 0;
                    mdDeadCounter = 0;
                    mdStuckResetRounds = 0;
                    mdStuckTeardowns = 0;
                    mdStuckQuietLen = 0;
                    mdForeignFamily = 0;
                    mdForeignStreak = 0;
                    mdFastPathMisconfirmUsed = 0;
                } else {
                    // looks wrong: only a stable reading counts as evidence
                    boolean stableReading = true;
                    for (uint8_t i = 0; stableReading && i < 3; i++) {
                        delay(2);
                        int16_t again = (int16_t)GBS::STATUS_SYNC_PROC_VTOTAL::read();
                        if (again < ((int16_t)lineCount - 3) || again > ((int16_t)lineCount + 3)) {
                            stableReading = false; // jumps around: not evidence
                        }
                    }

                    // A reading that jumps around is skipped: it neither counts nor clears. The
                    // garbage of the stuck state wanders from pass to pass (vt:97..132 in the
                    // log), so requiring an unbroken run of clean passes would never complete.
                    if (stableReading) {
                        if (mdImplausibleCounter < 255) {
                            mdImplausibleCounter++;
                        }

                        // the episode now has solid evidence that the loaded preset does
                        // not fit the source. Blank the output for the rest of the
                        // transition: the 25-pass strike count, the stage-1 reset rounds
                        // and - when MD names a foreign family - the fast path switch
                        // below all run behind black instead of unsynced garbage. For the
                        // fast path this is the same instant it fires (both count the same
                        // passes); for the strike paths it pulls the dark forward from the
                        // strike to ~160ms after the source changed.
                        if (mdImplausibleCounter >= MD_BLACKOUT_PASSES &&
                            GBS::DAC_RGBS_PWDNZ::read() == 1) {
                            SerialM.println(F("MD watchdog: measurement off - blanking output"));
                            GBS::DAC_RGBS_PWDNZ::write(0);
                            mdBlackoutOwned = true;
                        }

                        // fast path bookkeeping. While MD names no mode, a reading that
                        // keeps landing in the band of one particular other mode family is
                        // the SP truthfully describing the new source (measured: vt:270/271
                        // stable for a full second across resets while MD said 0). The same
                        // evidence counts when MD keeps *misconfirming* the loaded preset -
                        // same blind spot, same remedy, but at most one attempt per episode
                        // (ping-pong guard, see the mdFastPathMisconfirmUsed latch).
                        uint8_t readingFamily = spLineCountFamily(lineCount);
                        boolean fastPathEligible =
                            (detectedVideoMode == 0 ||
                             (detectedVideoMode == rto->videoStandardInput &&
                              mdFastPathMisconfirmUsed == 0));
                        if (fastPathEligible && readingFamily != 0 &&
                            readingFamily != rto->videoStandardInput) {
                            if (readingFamily == mdForeignFamily) {
                                if (mdForeignStreak < 255) {
                                    mdForeignStreak++;
                                }
                            } else {
                                mdForeignFamily = readingFamily;
                                mdForeignStreak = 1;
                            }
                        } else {
                            mdForeignFamily = 0;
                            mdForeignStreak = 0;
                        }

                        if (mdForeignStreak >= MD_FAST_FORMAT_PASSES &&
                            (millis() - mdFastPathLastApply) > 3000) {
                            // skip the strike escalation entirely - the source has changed
                            // format, MD is stuck (or misconfirming), and the SP has named
                            // the new format on MD_FAST_FORMAT_PASSES stable passes in a
                            // row. Apply that family's preset directly, mirroring the
                            // format change branch (same SOG mode / HDMI sync fix / preset
                            // / counter reset sequence), so the result matches a normal
                            // switch. In the misconfirm case the latch makes it a one-shot,
                            // so a wrong guess merely returns the episode to the normal
                            // escalation below.
                            boolean viaMisconfirm = (detectedVideoMode != 0);
                            if (viaMisconfirm) {
                                mdFastPathMisconfirmUsed = 1;
                            }
                            mdFastPathLastApply = millis();
                            if (viaMisconfirm) {
                                SerialM.print(
                                    F("\nMD watchdog fast path (misconfirm): stable vt:"));
                                SerialM.print(lineCount);
                                SerialM.print(F(" maps to mode "));
                                SerialM.print(readingFamily);
                                SerialM.print(F(" while MD keeps naming mode "));
                                SerialM.print(rto->videoStandardInput);
                                SerialM.println(F(" - switching preset"));
                            } else {
                                SerialM.print(F("\nMD watchdog fast path: stable vt:"));
                                SerialM.print(lineCount);
                                SerialM.print(F(" maps to mode "));
                                SerialM.print(readingFamily);
                                SerialM.println(F(" while MD is stuck - switching preset"));
                            }
                            // the documented stuck-MD lever: with MD freshly reset, the
                            // getVideoMode() wait inside doPostPresetLoadSteps() finds a
                            // classification instead of running into its 1.5s timeout
                            resetModeDetect();
                            delay(8);

                            if (GBS::SP_SOG_MODE::read() == 1) {
                                rto->syncTypeCsync = true;
                            } else {
                                rto->syncTypeCsync = false;
                            }
                            if (((rto->videoStandardInput == 1 || rto->videoStandardInput == 3) &&
                                 (readingFamily == 2 || readingFamily == 4)) ||
                                ((rto->videoStandardInput == 2 || rto->videoStandardInput == 4) &&
                                 (readingFamily == 1 || readingFamily == 3))) {
                                rto->useHdmiSyncFix = 1;
                            } else {
                                rto->useHdmiSyncFix = 0;
                            }
                            rto->videoIsFrozen = false;
                            if (uopt->presetPreference != 10) {
                                applyPresets(readingFamily);
                            } else {
                                rto->videoStandardInput = readingFamily;
                                setOutModeHdBypass(false);
                            }
                            rto->videoStandardInput = readingFamily;
                            rto->noSyncCounter = 0;
                            rto->continousStableCounter = 0;
                            newVideoModeCounter = 0;
                            activeStableLineCount = 0;
                            badHsActive = 0;
                            preemptiveSogWindowStart = millis();
                            delay(20);
                            return;
                        }

                        if (mdImplausibleCounter >= 25) {
                            mdImplausibleCounter = 0;
                            strike = true;
                            SerialM.print(F("\nMD plausibility watchdog: vt:"));
                            SerialM.print(lineCount);
                            SerialM.print(F(" can't belong to mode "));
                            SerialM.println(rto->videoStandardInput);
                        }
                    }
                }

                if (strike) {
                    // stage 1 only pays off when MD is actively naming the loaded mode
                    // (measurement chain confused around a correctly loaded source). With
                    // MD stuck on "no mode" the SP+MD resets never produced a
                    // classification and only added ~1s per round; those cases go straight
                    // to the stage 2 re-detection.
                    if (mdStuckResetRounds < 2 && detectedVideoMode != 0) {
                        // stage 1: repair the measurement chain
                        mdStuckResetRounds++;
                        SerialM.println(F("MD stuck: resetting SP + mode detect"));
                        resetSyncProcessor();
                        delay(8);
                        resetModeDetect(); // SFTRST_MODE_RSTZ: the actual stuck-MD lever
                        delay(8);
                        updateSpDynamic(1); // detection friendly SP parameters
                        // The lever this file already uses elsewhere for "MD not noticing
                        // a change" (see the noSyncCounter == 38 handler). Pulsed here
                        // instead of toggled once, so MD_VS_FLIP ends up as it was. The
                        // nudge never rescues the MD on its own, but it keeps the chain
                        // from decaying into the fully dead state - kept as insurance
                        // alongside the dead handling.
                        nudgeMD();
                        delay(20);
                        nudgeMD();
                    } else {
                        // stage 2: stuck through repeated resets - re-detect from scratch.
                        // A full teardown starts a fresh detection round, so the misconfirm
                        // latch re-arms - it only has to outlive one escalation.
                        mdStuckResetRounds = 0;
                        mdFastPathMisconfirmUsed = 0;
                        SerialM.println(F("MD stuck through resets: full re-detection"));
                        setResetParameters();
                        prepareSyncProcessor();
                        resetSyncProcessor();
                        resetModeDetect();
                        // the source is right there (HS was stably active a moment ago) -
                        // don't sit out loop()'s 500ms source-check cadence, start the
                        // re-detection immediately. It returns as soon as MD names the
                        // mode, and handles a vanished source the same way the loop would.
                        inputAndSyncDetect();
                        rto->noSyncCounter = 0;
                        mdStuckTeardowns++;
                        if (mdStuckTeardowns >= 3) {
                            mdStuckTeardowns = 0;
                            mdStuckQuietLen = 600000; // 10 minutes
                            SerialM.println(F("MD watchdog: giving up for 10 minutes"));
                        } else {
                            mdStuckQuietLen = 10000; // 10s breather
                        }
                        mdStuckQuietStart = millis();
                        return; // start the next pass from a clean slate
                    }
                }
            }
        }
    }

    if (rto->videoStandardInput >= 14) { // RGBHV checks
        static uint16_t RGBHVNoSyncCounter = 0;

        if (uopt->preferScalingRgbhv && rto->continousStableCounter >= 2) {
            boolean needPostAdjust = 0;
            static uint16_t activePresetLineCount = 0;
            // is the source in range for scaling RGBHV and is it currently in mode 15?
            uint16_t sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read(); // if sourceLines = 0, might be in some reset state
            if ((sourceLines <= 535 && sourceLines != 0) && rto->videoStandardInput == 15) {
                uint16_t firstDetectedSourceLines = sourceLines;
                boolean moveOn = 1;
                for (int i = 0; i < 30; i++) { // not the best check, but we don't want to try if this is not stable (usually is though)
                    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                    // range needed for interlace
                    if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                        moveOn = 0;
                        break;
                    }
                    delay(10);
                }
                if (moveOn) {
                    SerialM.println(F(" RGB/HV upscale mode"));
                    rto->isValidForScalingRGBHV = true;
                    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                    rto->autoBestHtotalEnabled = 1;

                    if (rto->syncTypeCsync == false) {
                        GBS::SP_SOG_MODE::write(0);
                        GBS::SP_NO_COAST_REG::write(1);
                        GBS::ADC_5_00::write(0x10); // 5_00 might be required
                        GBS::PLL_IS::write(0);      // 0_40 2: this provides a clock for IF and test bus readings
                        GBS::PLL_VCORST::write(1);  // 0_43 5: also required for clock
                        delay(320);                 // min 250
                    } else {
                        GBS::SP_SOG_MODE::write(1);
                        GBS::SP_H_CST_ST::write(0x10); // 5_4d  // set some default values
                        GBS::SP_H_CST_SP::write(0x80); // will be updated later
                        GBS::SP_H_PROTECT::write(1);   // some modes require this (or invert SOG)
                    }
                    delay(4);

                    float sourceRate = getSourceFieldRate(1);
                    Serial.println(sourceRate);

                    // todo: this hack is hard to understand when looking at applypreset and mode is suddenly 1,2 or 3
                    if (uopt->presetPreference == 2) {
                        // custom preset defined, try to load (set mode = 14 here early)
                        rto->videoStandardInput = 14;
                    } else {
                        if (sourceLines < 280) {
                            // this is "NTSC like?" check, seen 277 lines in "512x512 interlaced (emucrt)"
                            rto->videoStandardInput = 1;
                        } else if (sourceLines < 380) {
                            // this is "PAL like?" check, seen vt:369 (MDA mode)
                            rto->videoStandardInput = 2;
                        } else if (sourceRate > 44.0f && sourceRate < 53.8f) {
                            // not low res but PAL = "EDTV"
                            rto->videoStandardInput = 4;
                            needPostAdjust = 1;
                        } else { // sourceRate > 53.8f
                            // "60Hz EDTV"
                            rto->videoStandardInput = 3;
                            needPostAdjust = 1;
                        }
                    }

                    if (uopt->presetPreference == 10) {
                        uopt->presetPreference = Output960P; // fix presetPreference which can be "bypass"
                    }

                    activePresetLineCount = sourceLines;
                    applyPresets(rto->videoStandardInput);

                    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                    GBS::IF_INI_ST::write(16);   // fixes pal(at least) interlace
                    GBS::SP_SOG_P_ATO::write(1); // 5_20 1 auto SOG polarity (now "hpw" should never be close to "ht")

                    GBS::SP_SDCS_VSST_REG_L::write(2); // 5_3f
                    GBS::SP_SDCS_VSSP_REG_L::write(0); // 5_40

                    rto->coastPositionIsSet = rto->clampPositionIsSet = 0;
                    rto->videoStandardInput = 14;

                    if (GBS::PLLAD_ICP::read() >= 6) {
                        GBS::PLLAD_ICP::write(5); // reduce charge pump current for more general use
                        latchPLLAD();
                        delay(40);
                    }

                    updateSpDynamic(1);
                    if (rto->syncTypeCsync == false) {
                        GBS::SP_SOG_MODE::write(0);
                        GBS::SP_CLAMP_MANUAL::write(1);
                        GBS::SP_NO_COAST_REG::write(1);
                    } else {
                        GBS::SP_SOG_MODE::write(1);
                        GBS::SP_H_CST_ST::write(0x10); // 5_4d  // set some default values
                        GBS::SP_H_CST_SP::write(0x80); // will be updated later
                        GBS::SP_H_PROTECT::write(1);   // some modes require this (or invert SOG)
                    }
                    delay(300);

                        if (rto->extClockGenDetected && !uopt->disableExternalClockGenerator) {
                            // switch to ext clock
                            if (!rto->outModeHdBypass) {
                                if (GBS::PLL648_CONTROL_01::read() != 0x75) {
                                    // Store current clock if it's not our target or internal default
                                    if (GBS::PLL648_CONTROL_01::read() != 0x35) {
                                        GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());
                                    }
                                    
                                    // Switch to external clock (0x75)
                                    Si.enable(0);
                                    delayMicroseconds(800);
                                    GBS::PLL648_CONTROL_01::write(0x75);
                                    GBS::PAD_CKIN_ENZ::write(0); // Ensure CKIN is enabled
                                }
                            }
                            // sync clocks now
                            externalClockGenSyncInOutRate();
                        }

                    // note: this is all duplicated below. unify!
                    if (needPostAdjust) {
                        // base preset was "3" / no line doubling
                        // info: actually the position needs to be adjusted based on hor. freq or "h:" value (todo!)
                        GBS::IF_HB_ST2::write(0x08);  // patches
                        GBS::IF_HB_SP2::write(0x68);  // image
                        GBS::IF_HBIN_SP::write(0x50); // position
                        if (rto->presetID == 0x05) {
                            GBS::IF_HB_ST2::write(0x480);
                            GBS::IF_HB_SP2::write(0x8E);
                        }

                        float sfr = getSourceFieldRate(0);
                        if (sfr >= 69.0) {
                            SerialM.println("source >= 70Hz");
                            // increase vscale; vscale -= 57 seems to hit magic factor often
                            // 512 + 57 = 569 + 57 = 626 + 57 = 683
                            // wrap guard: 10 bit register, skip on pathological low values
                            if (GBS::VDS_VSCALE::read() >= 57) {
                                GBS::VDS_VSCALE::write(GBS::VDS_VSCALE::read() - 57);
                            }
                        } else {
                            // 50/60Hz, presumably
                            // adjust vposition
                            GBS::IF_VB_SP::write(8);
                            GBS::IF_VB_ST::write(6);
                        }
                    }
                }
            }
            // if currently in scaling RGB/HV, check for "SD" < > "EDTV" style source changes
            else if ((sourceLines <= 535 && sourceLines != 0) && rto->videoStandardInput == 14) {
                // Sort both the source and the preset that is loaded into the same three
                // bands and act when they stop agreeing. Comparing the two against
                // thresholds pairwise leaves a hole: a PAL-like source with an NTSC-like
                // preset in place (312 lines against 262) matches none of the cases, so
                // nothing switches back. That is the transition an Atari makes on reset -
                // it comes up at 60Hz and switches itself to 50Hz a moment later, and the
                // 60Hz preset stayed behind, leaving the frame time lock chasing an output
                // rate it can never reach.
                uint8_t sourceBand = (sourceLines < 280) ? 1 : ((sourceLines < 380) ? 2 : 3);
                // 0 = no preset measured yet, which counts as disagreeing with anything
                uint8_t activeBand = (activePresetLineCount == 0)
                    ? 0
                    : ((activePresetLineCount < 280) ? 1 : ((activePresetLineCount < 380) ? 2 : 3));

                if (sourceBand != activeBand) {
                    // check thoroughly first
                    uint16_t firstDetectedSourceLines = sourceLines;
                    boolean moveOn = 1;
                    for (int i = 0; i < 30; i++) {
                        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                        if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                            moveOn = 0;
                            break;
                        }
                        delay(10);
                    }

                    if (moveOn) {
                        // A steady line count is not the same as a line count that comes from the
                        // source. With the discrete H/V input gated off - which is what a disturbed
                        // source can leave behind, since a CSync verdict gates it and the verdict
                        // survives the disturbance - the sync processor free runs its vertical, and
                        // that shows up right here as a rock steady "format change" to something
                        // PAL-like that the source never sent.
                        // So ask the H/V pins directly before believing it. If they disagree, the
                        // sync configuration changed, not the source, and it has to be rebuilt from
                        // bypass rather than followed.
                        uint16_t probedLines = 0;
                        boolean probeFoundVSync = probeDiscreteVSync(&probedLines);
                        int lineDelta = (int)probedLines - (int)sourceLines;
                        if (lineDelta < 0) {
                            lineDelta = -lineDelta;
                        }

                        SerialM.print(F("RGB/HV format change, vt:"));
                        SerialM.print(sourceLines);
                        SerialM.print(F(" was:"));
                        SerialM.println(activePresetLineCount);

                        if (probeFoundVSync && lineDelta > 8) {
                            SerialM.print(F("vertical disagrees, discrete vt:"));
                            SerialM.println(probedLines);
                            rto->isValidForScalingRGBHV = false;
                            rto->videoStandardInput = 15;
                            applyPresets(15); // re-probes the sync type on the way
                            delay(300);
                            return;
                        }

                        // Hand the change to the classification branch above rather than swapping
                        // the preset in place here. Reloading a preset is only part of the job: the
                        // classification prepares the ADC and the PLLs first, then waits out the
                        // settling time, and applies anything only after that. A preset loaded onto
                        // a front end still set up for the format that just went away leaves the
                        // capture side without a working clock, which is what puts those fading
                        // patterns on screen - a decaying frame buffer instead of a picture. Going
                        // through mode 15 also re-measures the field rate, so a 50Hz source gets the
                        // 50Hz base preset instead of the 60Hz one this path assumed. It is the same
                        // handover the web UI performs when a preset is picked.
                        rto->videoStandardInput = 15;
                        return;
                    }
                }
            }
            // check whether to revert back to full bypass
            else if ((sourceLines > 535) && rto->videoStandardInput == 14) {
                uint16_t firstDetectedSourceLines = sourceLines;
                boolean moveOn = 1;
                for (int i = 0; i < 30; i++) {
                    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                    // range needed for interlace
                    if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                        moveOn = 0;
                        break;
                    }
                    delay(10);
                }
                if (moveOn) {
                    SerialM.println(F(" RGB/HV upscale mode disabled"));
                    rto->videoStandardInput = 15;
                    rto->isValidForScalingRGBHV = false;

                    activePresetLineCount = 0;
                    applyPresets(rto->videoStandardInput); // exception: apply preset here, not later in syncwatcher

                    delay(300);
                }
            }
        } // done preferScalingRgbhv

        if (!uopt->preferScalingRgbhv && rto->videoStandardInput == 14) {
            // user toggled the web ui button / revert scaling rgbhv
            rto->videoStandardInput = 15;
            rto->isValidForScalingRGBHV = false;
            applyPresets(rto->videoStandardInput);
            delay(300);
        }

        // stability check, for CSync and HV separately
        uint16_t limitNoSync = 0;
        uint8_t VSHSStatus = 0;
        boolean stable = 0;
        if (rto->syncTypeCsync == true) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
                // STATUS_INT_SOG_BAD = 0x0f bit 0, interrupt reg
                resetModeDetect();
                stable = 0;
                SerialM.print("`");
                delay(10);
                resetInterruptSogBadBit();
            } else {
                stable = 1;
                VSHSStatus = GBS::STATUS_00::read();
                // this status can get stuck (regularly does)
                stable = ((VSHSStatus & 0x04) == 0x04); // RGBS > check h+v from 0_00
            }
            limitNoSync = 200; // 100
        } else {
            VSHSStatus = GBS::STATUS_16::read();
            // this status usually updates when a source goes off
            if (rto->vsyncFlagUnreliable) {
                // this source never raises the VS active bit, so requiring it here would
                // count every pass as unstable and reset a perfectly locked picture
                stable = ((VSHSStatus & 0x02) == 0x02);
            } else {
                stable = ((VSHSStatus & 0x0a) == 0x0a); // RGBHV > check h+v from 0_16
            }
            limitNoSync = 300;
        }

        // set while the source is away, acted on once it is back and holding still again
        static boolean rgbhvSourceWasInterrupted = false;

        if (!stable) {
            LEDOFF;
            RGBHVNoSyncCounter++;
            rto->continousStableCounter = 0;
            if (RGBHVNoSyncCounter >= 20) {
                // longer than a stray glitch: the source really went away for a moment
                rgbhvSourceWasInterrupted = true;
            }
            if (RGBHVNoSyncCounter % 20 == 0) {
                SerialM.print("`");
            }
        } else {
            RGBHVNoSyncCounter = 0;
            LEDON;
            if (rto->continousStableCounter < 255) {
                rto->continousStableCounter++;
                if (rto->continousStableCounter == 6) {
                    updateSpDynamic(1);
                }
            }
        }

        if (RGBHVNoSyncCounter > limitNoSync) {
            RGBHVNoSyncCounter = 0;
            setResetParameters();
            prepareSyncProcessor();
            resetSyncProcessor(); // todo: fix MD being stuck in last mode when sync disappears
            //resetModeDetect();
            rto->noSyncCounter = 0;
            //Serial.println("RGBHV limit no sync");
        }

        // Catches an RGB/HV upscale mode that survives a source interruption (e.g. the
        // source being reset) with stale measurements, which is what leaves a black
        // screen, wrong colours, or a rejected output timing after the source comes back.
        // Re-enters via mode 15 to classify the source from scratch rather than resuming
        // with whatever base preset/sync setup was measured before the interruption.

        // remembers what the source measured once the upscale mode had settled on it
        static uint16_t settledLineCount = 0;
        // a reading already re-detected once, kept so a source that keeps producing it can't loop
        static uint16_t rejectedLineCount = 0;

        if (rto->videoStandardInput != 14) {
            // mode 15 gets fully re-measured by the upscale branch above anyway
            rgbhvSourceWasInterrupted = false;
            settledLineCount = 0;
        }

        // The stability check above watches the horizontal only for sources whose VS active flag the
        // SP never raises, so a disturbance that breaks just the vertical - a source restarting its
        // frame counter, which is exactly what a reset button does - passes unnoticed, and the frame
        // sync stays locked to a vertical that no longer exists. The measured line count is the one
        // reading that always follows the source, so watch that instead. Calibrated from the source
        // itself rather than from the classification measurement, so a line count that simply reads
        // differently in mode 14 than it did in mode 15 cannot start a loop, and the deviation has to
        // persist: a source jittering around its own value keeps resetting the count.
        if (rto->videoStandardInput == 14 && stable) {
            static uint8_t lineCountOffCounter = 0;
            uint16_t lines = GBS::STATUS_SYNC_PROC_VTOTAL::read();

            if (rto->continousStableCounter < 50) {
                // not settled (or a preset was just applied): the old reference no longer describes
                // what is being measured, take a fresh one once it holds still again
                settledLineCount = 0;
                lineCountOffCounter = 0;
            } else if (settledLineCount == 0) {
                if (lines >= 100) {
                    // Check once per preset, now that it has settled, that this vertical is the
                    // source's and not the sync processor's own free running one. A free run reads
                    // just as steadily as a real source, so nothing above can tell them apart, and
                    // the preset that got built on it stays in place looking broken - the picture
                    // repeating, sampled at a dot clock for a line rate the source doesn't send.
                    // Only meaningful for sources believed to have discrete H/V; for CSync the
                    // probe has nothing authoritative to say. Rate limited because a source that
                    // keeps unsettling would otherwise arrive here over and over, and the probe
                    // costs half a second of reconfigured sync processor each time.
                    static unsigned long lastVerticalVerify = 0;
                    boolean worthVerifying = !rto->syncTypeCsync && lines != rejectedLineCount &&
                        (millis() - lastVerticalVerify) > 10000;

                    if (worthVerifying) {
                        uint16_t probedLines = 0;
                        lastVerticalVerify = millis();

                        int lineDelta = 0;
                        if (probeDiscreteVSync(&probedLines)) {
                            lineDelta = (int)probedLines - (int)lines;
                            if (lineDelta < 0) {
                                lineDelta = -lineDelta;
                            }
                        }

                        if (lineDelta > 8) {
                            // once per distinct reading: if re-detecting lands on the same value
                            // again, it is the best this source is going to give, so let it stand
                            rejectedLineCount = lines;
                            SerialM.print(F("\nRGB/HV locked to a vertical the source doesn't send, vt:"));
                            SerialM.print(lines);
                            SerialM.print(F(" discrete vt:"));
                            SerialM.print(probedLines);
                            SerialM.println(F(" > re-detecting"));
                            settledLineCount = 0;
                            rgbhvSourceWasInterrupted = false;
                            rto->videoStandardInput = 15;
                            rto->isValidForScalingRGBHV = false;
                            applyPresets(15);
                            delay(300);
                            return;
                        }
                    }
                    settledLineCount = lines;
                }
                lineCountOffCounter = 0;
            } else if (lines < (settledLineCount - 8) || lines > (settledLineCount + 8)) {
                if (++lineCountOffCounter >= 25) { // ~0.5s
                    lineCountOffCounter = 0;
                    settledLineCount = 0;
                    rgbhvSourceWasInterrupted = true;
                }
            } else {
                lineCountOffCounter = 0;
            }
        }

        // The upscale mode is built entirely out of measurements taken when it was entered: base
        // preset, scale factors, sync type and the whole SP setup describe the source as it looked
        // then. After an interruption (someone hitting reset on the source, a machine changing
        // resolution while restarting) those measurements no longer have to hold, and resuming with
        // them is what leaves a black screen, a wrongly coloured picture, or an output timing the
        // display refuses. The sync loss machinery further up cannot repair that either: it is built
        // around CSync sources and only ever restores parts of the setup.
        // So do what re-selecting a preset in the web UI does: drop back to plain bypass and let the
        // upscale branch classify the source again from scratch. applyPresets(15) re-probes the sync
        // type on the way, which also undoes a CSync verdict picked up during the disturbance.
        // Waiting for the source to come back stable is only safe while the preset still fits it.
        // Once it doesn't - the source changed format, or the sync setup got scrambled - the preset
        // is itself what keeps the source from ever reading stable, and the wait would never end:
        // the teardown at limitNoSync is 6 seconds of no picture away and restarts from input
        // detection. Plain bypass locks to whatever timing arrives, so go there instead and let the
        // upscale branch classify the source from there.
        // The HSACT condition keeps a source that was simply switched off out of this: that one has
        // nothing to re-detect and is handled by the teardown further down, which blanks the output
        // rather than leaving the last frame on screen.
        if (rto->videoStandardInput == 14 &&
            ((RGBHVNoSyncCounter >= 40 && GBS::STATUS_SYNC_PROC_HSACT::read() == 1) ||
             (rgbhvSourceWasInterrupted && rto->continousStableCounter >= 20))) {
            rgbhvSourceWasInterrupted = false;
            settledLineCount = 0;
            rejectedLineCount = 0; // the source really went away, so judge it fresh when it returns
            RGBHVNoSyncCounter = 0;
            SerialM.println(F("\nRGB/HV source interrupted, re-detecting"));
            rto->videoStandardInput = 15;
            rto->isValidForScalingRGBHV = false;
            applyPresets(15);
            delay(300);
            return;
        }

        // The CSync verdict is self confirming: syncTypeCsync gates the discrete H/V input off, and
        // with it gated off nothing can ever show that the source has a vertical of its own. A source
        // misjudged during a disturbance therefore stays misjudged - horizontally locked at best,
        // vertical free running, and no way out but applying a preset by hand. probeDiscreteVSync()
        // looks past the gate, so retry it while the vertical measurement stays empty. This mirrors
        // the retry that modes 1..13 get further up, which deliberately excludes RGB/HV.
        {
            static uint16_t rgbhvNoVerticalCounter = 0;

            if (rto->syncTypeCsync && GBS::STATUS_SYNC_PROC_HSACT::read() == 1 &&
                GBS::STATUS_SYNC_PROC_VTOTAL::read() == 0) {
                if (++rgbhvNoVerticalCounter >= 150) { // syncwatcher runs every 20ms, so ~3s
                    rgbhvNoVerticalCounter = 0;
                    if (probeDiscreteVSync(NULL)) {
                        SerialM.println(F("CSync assumed, but source has discrete VSync > RGB/HV"));
                        rto->syncTypeCsync = false;
                        rto->videoStandardInput = 15;
                        rto->isValidForScalingRGBHV = false;
                        applyPresets(15);
                        delay(300);
                        return;
                    }
                }
            } else {
                rgbhvNoVerticalCounter = 0;
            }
        }

        static unsigned long lastTimeSogAndPllRateCheck = millis();
        if ((millis() - lastTimeSogAndPllRateCheck) > 900) {
            if (rto->videoStandardInput == 15) {
                // start out by adjusting sync polarity, may reset sog unstable interrupt flag
                updateHVSyncEdge();
                delay(100);
            }

            static uint8_t runsWithSogBadStatus = 0;
            static uint8_t oldHPLLState = 0;
            if (rto->syncTypeCsync == false) {
                if (GBS::STATUS_INT_SOG_BAD::read()) { // SOG source unstable indicator
                    runsWithSogBadStatus++;
                    //SerialM.print("test: "); SerialM.println(runsWithSogBadStatus);
                    if (runsWithSogBadStatus >= 4) {
                        // check before switching: the SOG bad flag stays up for as long as
                        // a source is disturbed, and a machine being reset holds it up well
                        // past four passes without having changed its sync type at all. The
                        // switch is a one way door: syncTypeCsync gates the discrete H/V
                        // input off, so afterwards nothing can show that the source still
                        // has a vertical of its own.
                        if (probeDiscreteVSync(NULL)) {
                            // still RGB/HV, it was only disturbed
                            runsWithSogBadStatus = 0;
                        } else {
                            SerialM.println(F("RGB/HV < > SOG"));
                            rto->syncTypeCsync = true;
                            rto->HPLLState = runsWithSogBadStatus = RGBHVNoSyncCounter = 0;
                            rto->noSyncCounter = 0x07fe; // will cause a return
                        }
                    }
                } else {
                    runsWithSogBadStatus = 0;
                }
            }

            uint32_t currentPllRate = 0;
            static uint32_t oldPllRate = 10;

            // how fast is the PLL running? needed to set charge pump and gain
            // typical: currentPllRate: 1560, currentPllRate: 3999 max seen the pll reach: 5008 for 1280x1024@75
            if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                currentPllRate = getPllRate();
                //Serial.println(currentPllRate);
                if (currentPllRate > 100 && currentPllRate < 7500) {
                    if ((currentPllRate < (oldPllRate - 3)) || (currentPllRate > (oldPllRate + 3))) {
                        delay(40);
                        if (GBS::STATUS_INT_SOG_BAD::read() == 1)
                            delay(100);
                        currentPllRate = getPllRate(); // test again, guards against random spurs
                        // but don't force currentPllRate to = 0 if these inner checks fail,
                        // prevents csync <> hvsync changes
                        if ((currentPllRate < (oldPllRate - 3)) || (currentPllRate > (oldPllRate + 3))) {
                            oldPllRate = currentPllRate; // okay, it changed
                        }
                    }
                } else {
                    currentPllRate = 0;
                }
            }

            resetInterruptSogBadBit();

            //short activeChargePumpLevel = GBS::PLLAD_ICP::read();
            //short activeGainBoost = GBS::PLLAD_FS::read();
            //SerialM.print(" rto->HPLLState: "); SerialM.println(rto->HPLLState);
            //SerialM.print(" currentPllRate: "); SerialM.println(currentPllRate);
            //SerialM.print(" CPL: "); SerialM.print(activeChargePumpLevel);
            //SerialM.print(" Gain: "); SerialM.print(activeGainBoost);
            //SerialM.print(" KS: "); SerialM.print(GBS::PLLAD_KS::read());

            oldHPLLState = rto->HPLLState; // do this first, else it can miss events
            if (currentPllRate != 0) {
                if (currentPllRate < 1030) {
                    rto->HPLLState = 1;
                } else if (currentPllRate < 2300) {
                    rto->HPLLState = 2;
                } else if (currentPllRate < 3200) {
                    rto->HPLLState = 3;
                } else if (currentPllRate < 3800) {
                    rto->HPLLState = 4;
                } else {
                    rto->HPLLState = 5;
                }
            }

            if (rto->videoStandardInput == 15) {
                if (oldHPLLState != rto->HPLLState) {
                    if (rto->HPLLState == 1) {
                        GBS::PLLAD_KS::write(2); // KS = 2 okay
                        GBS::PLLAD_FS::write(0);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 2) {
                        GBS::PLLAD_KS::write(1);
                        GBS::PLLAD_FS::write(0);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 3) { // KS = 1 okay
                        GBS::PLLAD_KS::write(1);
                        GBS::PLLAD_FS::write(1);
                        GBS::PLLAD_ICP::write(6); // would need 7 but this is risky
                    } else if (rto->HPLLState == 4) {
                        GBS::PLLAD_KS::write(0); // KS = 0 from here on
                        GBS::PLLAD_FS::write(0);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 5) {
                        GBS::PLLAD_KS::write(0); // KS = 0
                        GBS::PLLAD_FS::write(1);
                        GBS::PLLAD_ICP::write(6);
                    }

                    latchPLLAD();
                    delay(2);
                    setOverSampleRatio(4, false); // false = do apply // will auto decrease to max possible factor
                    SerialM.print(F("(H-PLL) rate: "));
                    SerialM.print(currentPllRate);
                    SerialM.print(F(" state: "));
                    SerialM.println(rto->HPLLState);
                    delay(100);
                }
            } else if (rto->videoStandardInput == 14) {
                if (oldHPLLState != rto->HPLLState) {
                    SerialM.print(F("(H-PLL) rate: "));
                    SerialM.print(currentPllRate);
                    SerialM.print(F(" state (no change): "));
                    SerialM.println(rto->HPLLState);
                    // need to manage HPLL state change somehow

                    //FrameSync::reset(uopt->frameTimeLockMethod);
                }
            }

            if (rto->videoStandardInput == 14) {
                // scanlines
                if (uopt->wantScanlines) {
                    if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive) {
                        if (GBS::IF_LD_RAM_BYPS::read() == 0) { // line doubler on?
                            enableScanlines();
                        }
                    }
                }
            }

            rto->clampPositionIsSet = false; // RGBHV should regularly check clamp position
            lastTimeSogAndPllRateCheck = millis();
        }
    }

    if (rto->noSyncCounter >= 0x07fe) {
        // couldn't recover, source is lost
        // restore initial conditions and move to input detect
        GBS::DAC_RGBS_PWDNZ::write(0); // 0 = disable DAC
        rto->noSyncCounter = 0;
        sourceGoneProbeCount = 0; // whatever turns up next has to prove itself again
        sourceWasClassified = false;
        fallbackPresetTried = false; // full teardown, so the fallback is worth a fresh attempt
        crtcTimingHold = false;      // nothing left to hold, and it must not greet the next source
        modeUnknownCounter = 0;
        crtcHoldVdeadCounter = 0;   // and the hold's dead-vertical counter starts fresh
        mdFastPathMisconfirmUsed = 0; // whatever turns up next starts without the latch
        SerialM.println();
        goLowPowerWithInputDetection(); // does not further nest, so it can be called here // sets reset parameters
    }

    // Monitor Si5351 lock status if generator is detected and enabled
    if (rto->extClockGenDetected && !uopt->disableExternalClockGenerator && !rto->sourceDisconnected) {
        static uint32_t lastSiLockCheck = 0;
        if (millis() - lastSiLockCheck > 2000) {
            lastSiLockCheck = millis();
            
            int st = -999;
            Wire.beginTransmission(SIADDR);
            Wire.write(0x00);
            if (Wire.endTransmission() == 0) {
                size_t n = Wire.requestFrom((uint8_t)SIADDR, (size_t)1, false);
                if (n == 1) st = Wire.read();
            }
            
            // Treat LOLA as lock indicator; ignore LOLB (PLLB). bit5=LOLA, bit6=LOLB, bit4=LOS, bit7=SYS_INIT
            // Relaxed check: ignore SYS_INIT (bit7) for monitoring as it can be transient/sticky while PLLA is locked.
            bool siLocked = (st >= 0) && ((st & 0x20) == 0);
            if (!siLocked) {
                static uint32_t lastSiLockFailLog = 0;
                if (millis() - lastSiLockFailLog > 1000) {
                    SerialM.printf("Si5351 lock lost (st:0x%02X). Re-syncing...\n", st);
                    lastSiLockFailLog = millis();
                }
                // Try to restore sync if we have a stable source
                if (rto->continousStableCounter > 20 && rto->noSyncCounter == 0) {
                    externalClockGenSyncInOutRate();
                }
            }
        }
    }
}


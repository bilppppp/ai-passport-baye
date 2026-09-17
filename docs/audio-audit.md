# AI Passport Baye: Audio Archeology & Subsystem Audit

**Document Version:** 2.0  
**Date:** 2026-09-17  
**Target Hardware:** FoloToy AI Passport (ESP32-C3 revision v1.1, 8MB Flash)  
**Audio Hardware:** Everest Semiconductor ES8311 I2S Codec + NS4150 Class-D PA  
**Status:** Archeological Findings Corrected; Full Music Integration Baseline

---

## 1. Upstream Audio Archeology & Historical Fact Separation

To establish technical rigor, we strictly distinguish between **platform system capabilities** and the **actual behavior of the Baye game core**.

### 1.1 BBK Platform Capabilities
The BBK (步步高) electronic dictionary operating system and SDK (`dictsys.h`) exposed formal audio, melody, and volume management APIs:

```c
FAR void SysPlayMelody(U8 melodyNum);
FAR void SysStopMelody(void);
FAR void SysSetVolume(U8 volume);
FAR U8   SysGetVolume(void);
FAR void SysSetKeySound(U8 keySoundFlag);
FAR U8   SysGetKeySound(void);
```

The BBK hardware and system firmware layer possessed sound, melody, volume control, and speech synthesis facilities. The platform itself was fully audio-capable.

### 1.2 Baye Engine Actual Behavior
In the native C source code of *Sango / Baye* (三国霸业) available to us:
- **No Melody Calls:** Zero calls to `SysPlayMelody()` or `SysStopMelody()`.
- **No Volume Calls:** Zero calls to `SysSetVolume()` or `SysGetVolume()`.
- **No Sound Driver / BGM Engine:** Zero internal soundtrack sequencers, audio drivers, or music lookup tables exist in the game logic.
- **Explicit Key Sound Suppression:** In `components/baye/core/comIn.c`:
  ```c
  /* GamConInit(): backup and disable system key clicks upon game entry */
  g_GamKeySound = SysGetKeySound();
  SysSetKeySound(false);

  /* GamConRst(): restore system key clicks upon game exit */
  SysSetKeySound(g_GamKeySound);
  ```
  The game explicitly turns off the system key clicks while running and only restores the setting upon termination.

### 1.3 iBaye Port Counter-Evidence
The reference cross-platform port `iBaye` (running on iOS/macOS) successfully reproduces the complete gameplay experience of *Sango / Baye*. In its platform adaptation layer, `SysPlayMelody()` and `SysStopMelody()` are either unmapped or empty stubs. The normal gameplay loop executes completely without requiring or triggering these melody routines.

### 1.4 Resource Inspection (`dat.lib`)
Current structural and resource audits of `dat.lib` (196,890 bytes) have identified bitmaps, maps, city data, officer records, event strings, and logic matrices. No identifiable PCM samples, waveforms, complete BGM sequences, or music assets referenced by Baye execution paths have been found. We state this empirical finding without asserting mathematical impossibility of unrecognized binary structures.

### 1.5 Definitive Formulation

We strictly distinguish three architectural scopes:

1. **BBK Platform (步步高系统平台):**  
   Possesses complete hardware and firmware audio facilities, including tone generation, melody playback (`SysPlayMelody`), volume control (`SysSetVolume`), and speech synthesis. The platform itself is fully audio-capable.

2. **Baye Game Core (原生三国内核):**  
   In the native C engine source code currently available to us, there is zero call chain to `SysPlayMelody`, `SysStopMelody`, or `SysSetVolume`, and no BGM driver exists. Furthermore, the game explicitly suppresses the platform's key click sound on entry (`g_GamKeySound = SysGetKeySound(); SysSetKeySound(false);` in `components/baye/core/comIn.c`) and restores it upon exit (`SysSetKeySound(g_GamKeySound);`).

3. **Passport Enhanced (本次原声音乐增强):**  
   Features a newly engineered, 5-track scene-aware retro Original Soundtrack (OST) crafted specifically for FoloToy AI Passport (ESP32-C3 + ES8311 I2S Codec), with seamless looping, soft transient Q15 fades, volume HUD, and NVS persistence. It is **not** a recreation or restoration of any historical game soundtrack.

---

## 2. Evidence Classification Hierarchy

| Level | Finding / Claim | Evidentiary Basis |
| :--- | :--- | :--- |
| **CONFIRMED** | BBK SDK exposes `SysPlayMelody`, `SysStopMelody`, `SysSetVolume` | Verified in `components/baye/core/inc/dictsys.h` |
| **CONFIRMED** | Baye core disables system key sound on startup and restores on exit | Verified in `components/baye/core/comIn.c` (`GamConInit`, `GamConRst`) |
| **CONFIRMED** | Baye core makes 0 calls to known BBK Melody and Volume APIs | Full symbol grep of native C engine codebase |
| **CONFIRMED** | iBaye platform executes full game without melody implementations | Verified in iBaye platform abstraction codebase |
| **STRONGLY SUPPORTED** | The original Baye gameplay experience featured no background music | Lack of call sites, asset references, or driver hooks in game core |
| **NOT YET PROVEN** | Whether any historical Baye binary on any BBK hardware variant invoked audio | Requires binary syscall tracing on physical or emulated devices |
| **NOT YET PROVEN** | `dat.lib` contains zero unknown encoded musical data | Empirical absence of known patterns; unparsed structures may exist |

---

## 3. Future Archeology Proposal (Non-Blocking)

If an original BBK A-series ROM environment (including original `.gam` binary, matching `E.BIN`, and `8.BIN`) is obtained in the future:
1. Execute the original 6502 binary under a cycle-accurate BBK emulator with syscall tracing.
2. Trap any hardware I/O writes or OS interrupt jumps targeting melody, tone, sound, or volume vectors.
3. Determine whether any specific hardware revision invoked undocumented audio routines.

*Note: This prospective binary research does not block the development or integration of the Baye Passport Enhanced Soundtrack.*

---

## 4. Platform Layer Implementation (`passport_sys.c`)

In `components/baye/platform/passport_sys.c`:
```c
FAR U8 SysGetKeySound(void) {
    return 0;
}

FAR void SysSetKeySound(U8 keySoundFlag) {
    (void)keySoundFlag;
}
```
These stubs fulfill the dictionary OS contract cleanly while allowing the dedicated `passport_audio` manager to drive enhanced soundtrack playback without engine contention.

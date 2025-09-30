
# NXP Systems Software Engineer Candidate Challenge

**Deadline:** **October 24th 2025**  
**Goal:** Build a small system that simulates a hardware sensor in the Linux kernel and exposes it to user space, with a user app to configure/read it. A lightweight GUI is **optional**.

**Note:** It does not need to be perfect or cover every point, just do your best and demonstrate your skills. Obviously try to follow the guidelines and deliver what is requested as much as possible. Dont get block on any request is not understood, get something working then go back and analyze.

---

## 1) Summary & Goals

You will implement **“Virtual Sensor + Alert Path”**:

- **Kernel module (C)**: out‑of‑tree platform driver producing periodic “temperature” samples.
- **User↔Kernel communication**: character device with `read()` + `poll/epoll`; configuration via `sysfs` and/or `ioctl`.
- **Device Tree (DT)**: DTS snippet + proper binding and property parsing at kernel driver.
- **User space app (Python **or **C++**): CLI required; optional GUI to visualize readings/alerts.
- **Shell scripts**: build, run demo, (optional) lint.
- **Git hygiene**: clear commit history/branches/tags.
- **Design quality**: modularity, locking choices, API contract, problem‑solving write‑ups.

---

## 2) What You’ll Build

### 2.1 Kernel Module: `nxp_simtemp`

**Responsibilities**
- Register a **platform driver**; bind using DT `compatible = "nxp,simtemp"`.
- Produce simulated temperature samples every *N* ms (configurable) using a timer/hrtimer/workqueue.
- Expose a **character device** `/dev/simtemp` (via `miscdevice` or `cdev`).
- Support **blocking reads** returning a binary record with timestamp and temperature.
- Support **`poll()`/`epoll()`**:
  - Wake on **new sample** availability.
  - Wake when **threshold is crossed** (separate event bit).
- Provide **sysfs** controls under `/sys/class/.../simtemp/`:
  - `sampling_ms` (RW): update period.
  - `threshold_mC` (RW): alert threshold in milli‑°C.
  - `mode` (RW): e.g., `normal|noisy|ramp`.
  - `stats` (RO): counters (updates, alerts, last error).
- Optional: **`ioctl`** for atomic/batch config.

**Binary record (example)**
```c
#include <linux/types.h>

struct simtemp_sample {
    __u64 timestamp_ns;   // monotonic timestamp
    __s32 temp_mC;        // milli-degree Celsius (e.g., 44123 = 44.123 °C)
    __u32 flags;          // bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED (extend as needed)
} __attribute__((packed));
```

**Device Tree snippet (example)**
```dts
simtemp0: simtemp@0 {
    compatible = "nxp,simtemp";
    sampling-ms = <100>;
    threshold-mC = <45000>;
    status = "okay";
};
```

**Concurrency & Safety**
- Use appropriate locking around shared state (e.g., ring buffer indices, threshold flags).
- Validate `copy_to_user`/`copy_from_user`.
- Correctly handle partial/short reads and `O_NONBLOCK`.
- Clean teardown: cancel timers/work, free resources, remove sysfs, unregister device without leaks or OOPS.

> **Note:** You may create a local `platform_device` for testing, but still include DT + `of_match_table` for completeness and document how it would bind on an i.MX/QEMU system.

---

### 2.2 User Space: CLI (Required) & GUI (Optional)

**CLI app (Python or C++)**
- Configure sampling period & threshold (via **sysfs** and/or **ioctl**).
- Read from `/dev/simtemp` using `select/poll/epoll` and print lines like:
  ```
  2025-09-22T20:15:04.123Z temp=44.1C alert=0
  ```
- Include a **test mode**: set a low threshold and verify an alert event occurs within 2 periods (exit non‑zero on failure).

**GUI (optional)**
- Python (Tkinter/PyQt) or C++ (Qt/ImGui) or any other you are comfortable with.
- Live plot/gauge of temperature; visual alert on threshold.
- Controls to change sampling/threshold at runtime.

---

### 2.3 Shell Scripting (Required)

- `scripts/build.sh`: builds the kernel module and user app; detects kernel headers; fails fast with helpful hints.
- `scripts/run_demo.sh`: `insmod` → configure → run CLI test → `rmmod`; return non‑zero on failure.
- `scripts/lint.sh` (optional): run `checkpatch.pl`, `clang-format`, etc., if available.

---

## 3) Acceptance Criteria (Core)

1. **Build & Load**
   - `build.sh` succeeds on a stock Ubuntu LTS dev VM with installed kernel headers.
   - `insmod nxp_simtemp.ko` creates `/dev/simtemp` and the sysfs attributes.

2. **Data Path**
   - Reading `/dev/simtemp` returns the documented binary record.
   - `poll/epoll` wakes on **new samples** and on **threshold crossing** (distinct flag).

3. **Config Path**
   - `echo 50 > /sys/.../sampling_ms` visibly increases sample frequency.
   - `echo 42000 > /sys/.../threshold_mC` affects alert behavior.
   - `cat stats` shows sane counters.

4. **Robustness**
   - Clean unload: no warnings, no lingering nodes, no use‑after‑free.

5. **User App**
   - CLI test mode configures device, waits for event, reports PASS/FAIL reliably.

6. **Docs & Git**
   - `README.md`: exact build/run steps. **It MUST include the links to the video and git repo**
   - `DESIGN.md`: architecture, API contract, threading/locking model, DT mapping.
   - Git history: small, purposeful commits; a tag for `v1.0`.

**Stretch (bonus)**
- GUI dashboard.
- QEMU ARM `virt` demo using the DT overlay (or run on a board).
- Unit tests for user‑space record parsing/event logic.

---

## 4) Recommended Repository Layout

```text
simtemp/
├─ kernel/
│  ├─ Kbuild
│  ├─ Makefile
│  ├─ nxp_simtemp.c
│  ├─ nxp_simtemp.h
│  ├─ nxp_simtemp_ioctl.h
│  └─ dts/
│     └─ nxp-simtemp.dtsi
├─ user/
│  ├─ cli/
│  │  ├─ main.py            # or main.cpp
│  │  └─ requirements.txt   # if Python
│  └─ gui/                  # optional
│     └─ app.py             # or qt_app.cpp
├─ scripts/
│  ├─ build.sh
│  ├─ run_demo.sh
│  └─ lint.sh               # optional
├─ docs/
│  ├─ README.md
│  ├─ DESIGN.md             # block diagram will be nice
│  ├─ TESTPLAN.md
│  └─ AI_NOTES.md           # prompts used and validation notes
└─ .gitignore
```

---

## 5) Suggested Test Plan (High Level)

- **T1 — Load/Unload:** build → `insmod` → verify `/dev/simtemp` & sysfs → `rmmod` (no warnings).
- **T2 — Periodic Read:** set `sampling_ms=100`; verify ~10±1 samples/sec using timestamps.
- **T3 — Threshold Event:** lower threshold slightly below mean; ensure `poll` unblocks within 2–3 periods and flag is set.
- **T4 — Error Paths:** invalid sysfs writes → `-EINVAL`; very fast sampling (e.g., `1ms`) doesn’t wedge; `stats` still increments.
- **T5 — Concurrency:** run reader + config writer concurrently; no deadlocks; safe unload.
- **T6 — API Contract:** struct size/endianness documented; user app handles partial reads.

---

## 6) Architecture & Modularity (desirable) Expectations

- **Driver internals**
  - Producer (timer/work) fills a **bounded ring buffer**.
  - Wake a **wait queue** on new data and threshold events.
  - Proper **lifetimes/refcounts**; cancel work before free/unregister.

- **User‑kernel API**
  - Control via **sysfs** (and/or `ioctl`), data via **char device**, events via **poll**.
  - Binary record layout documented with versioning notes.

- **Modularity**
  - Split code: core, sysfs/ioctl, DT parsing, buffer/queue helpers.
  - Keep headers minimal and focused.

- **Security & safety**
  - Validate user input; avoid unbounded copies; least‑privilege sysfs perms.

---

## 7) Problem‑Solving Write‑Up (place in `DESIGN.md`)

- Describe in this document below info:
  - Block diagram of how the different modules (kernel and userspace) interact
  - Describe in words how they interact (event, signals, etc)
- If apply:
  - **Locking choices:** Where do you use spinlocks vs mutexes and why? Point to code paths.
  - **API trade‑offs:** Why use `ioctl` vs `sysfs` for control/eventing here.
  - **Device Tree mapping:** How does `compatible` and properties map to `probe()`? Defaults if DT is missing.
  - **Scaling:** What breaks first at **10 kHz** sampling? Strategies to mitigate.

---

## 8) Tooling & Environment

- **OS:** Ubuntu with matching **kernel headers** installed or any other Linux distribution (Yocto, Debian)
- **Build:** out‑of‑tree module via `KDIR=/lib/modules/$(uname -r)/build`.
- **Languages:** Kernel (**C**); user app (**Python or C++**).
- **AI usage allowed:** Include `docs/AI_NOTES.md` with prompts used and how you validated outputs.

> Extra credit: Run on QEMU ARM64 `virt` with DT overlay or any i.MX or Linux embedded platform.

---

## 9) Submission

- Provide a Git repo (public link).
- Include a short **2–3 min demo video** showing at least:
  - load → configure → live readings → threshold alert → unload.
  - build process and other is a plus
- Note what you would do next with more time.
- Use **git-send email** to send just the patch (text/diff) to the README.md file  that includes ONLY the links to the video and links to the git repo.
  - Use "R-1005999X Software Engineer Position at NXP Guadalajara" as subject on the email
  - Your name and email should be on the author and sign off of the patch.
- Don't attach the video or any other file to the email

---


## 10) Candidate Instructions (TL;DR)

1. Implement **`nxp_simtemp`**: DT binding, char dev, pollable events, sysfs controls.
2. Write a **CLI** (Python/C++) to configure and read the device.
3. Provide **build & demo scripts**.
4. Document design, API, DT mapping, and **AI usage**.
5. Optional: GUI, QEMU/DT, run on NXP i.MX or any Linux embedded platform.
6. Submit links to git repo and **2–3 min video** demo using git-send email

---
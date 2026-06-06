---
name: feedback-build-process
description: User builds the project themselves and sends error output — do not attempt to run cmake or the compiler via PowerShell
metadata:
  type: feedback
---

User handles building manually and pastes error output into the conversation.

**Why:** PowerShell cannot capture compiler stderr output from msys2/ucrt64 toolchain reliably. cmake in mingw64 crashes (exit 57). CLion cmake works but output capture via PowerShell Tee-Object fails for this project.

**How to apply:** After making code changes, do NOT attempt to run cmake or any compiler commands. Instead, tell the user "빌드해주세요" and wait for them to paste the error output. Then fix errors based on what they send.

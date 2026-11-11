# Tools for this attempt

- `kernel_gsp_demand_logger.patch` — the instrumentation patch applied to
  `src/nvidia/src/kernel/gpu/gsp/kernel_gsp.c` in a local checkout of
  NVIDIA's open-gpu-kernel-modules (version 595.99.02), right after the
  real `osPackageRegistry()` call. Dumps every DWORD registry key GSP-RM's
  host side actually packages and sends to firmware, with its value.
  Built with the tree's own top-level `make modules -j32` and loaded with
  `insmod` directly (not `modprobe`, to point at the freshly-built `.ko`).
  This is what produced the `GU_DEMAND_LOG` lines quoted in this attempt's
  `README.md` — a real build, not a mockup, but reconstructed here from the
  diff shown in the session's own write-up (the running `/tmp` build itself
  does not survive a live-boot reboot).
- `registry_keys_all.txt` — the full, authoritative list of all 667 string
  literals GSP-RM's firmware's own registry-read helper
  (`FUN_ram_011cfb80` in the project's Ghidra decompile) is ever called
  with. Extracted by searching the complete ~22,267-function decompiled
  firmware corpus for every call site of that helper, not by guessing
  plausible names. This is the candidate pool for every registry-key test
  in this and the sibling `11-registry-key-sweep-acr-cluster` attempt.

Applies to an `open-gpu-kernel-modules` checkout at the exact driver
version actually running on the test machine (595.99.02) — see
<https://github.com/NVIDIA/open-gpu-kernel-modules>.

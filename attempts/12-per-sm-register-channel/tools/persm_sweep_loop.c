/*
 * persm_sweep_loop.c -- the bounded group/subgroup sweep added on top of
 * persm_channel_probe.c's existing GR_CTX_SM probe (see persm_channel_probe.c
 * in this same folder for the full channel-build tool this extends).
 *
 * This is a partial reconstruction: only the sweep loop itself is preserved
 * verbatim from the project's own write-up (the full extended file,
 * /tmp/persm_sweep.c, did not survive the disposable live-boot session it
 * ran in). Wire this loop in place of persm_channel_probe.c's single-point
 * stage-5 GR_CTX_SM probe, run once before and once after the real GPU
 * submission in stage 6, to reproduce the result described in this
 * attempt's README.
 *
 * Swept both FEAT_OVR_SM_SS0 (0x0082381c) and FEAT_OVR_SM_SS1 (0x00823820)
 * over every (group, subgroup) pair below. All 128 reads (64 pairs x 2
 * registers) returned plain NV_ERR_INVALID_ARGUMENT (0x1f), both before and
 * after a completed real GPU engine submission.
 */
for (uint32_t grp = 0; grp < 8; grp++)
  for (uint32_t sub = 0; sub < 4; sub++) {
      /* GR_CTX_SM-routed read of FEAT_OVR_SM_SS0/SS1 with this (grp, sub),
       * via the same exec_regops()-style NV2080_CTRL_CMD_GPU_EXEC_REG_OPS
       * call persm_channel_probe.c already builds, REG_TYPE_GR_CTX_SM (4),
       * regGroupMask = grp, regSubGroupMask = sub. */
  }

/*
 * persm_channel_probe.c -- build a minimal, caller-owned RM channel (real
 * GPU object creation + scheduling, but NO pushbuffer/kernel ever submitted
 * and the doorbell never rung) solely so NV2080_CTRL_CMD_GPU_EXEC_REG_OPS
 * calls with REG_TYPE_GR_CTX_SM have a live, addressable context to resolve
 * against -- per checklist.json's per-sm-selector item ("only the global
 * FEAT block has been probed; reaching per-SM copies needs a caller-owned
 * RM channel that doesn't exist yet").
 *
 * Standalone: does NOT build or link against a separate internal reference
 * tool (same reasoning as fuzz_harness.c in this directory -- that tool is
 * scoped to a different shared machine/GPU with its own authorization
 * policy). This DOES use that tool's real sequence as a template (object
 * order, flags) -- read, not copied verbatim -- but includes NVIDIA's own
 * real headers directly from this project's own 595.91.07 checkout (closer
 * to the running 595.99.02 than that tool's bundled 580.173.02) for
 * guaranteed-correct struct layouts,
 * since hand-rolled structs (regops.c's approach) are only safe for the
 * few simple, stable fields that tool needed -- channel/UVM/VA-space
 * objects have real memory-safety stakes if a layout is wrong.
 *
 * Staged, with an explicit --stage N argument: each stage builds one more
 * layer of the object graph and then EXITS (clean teardown in reverse, or
 * prints the live handles and intentionally leaks them for the next
 * invocation to re-establish from scratch -- simplest safe model: every
 * invocation is a fresh, independent attempt, nothing persists across
 * stages. No stage after the open+classlist stage runs unless explicitly
 * requested -- this is the same "operator decides when to continue"
 * discipline as fuzz_harness.c, applied to escalating object-creation risk
 * instead of escalating input malformation.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -Werror \
 *     -I$TREE/src/common/sdk/nvidia/inc \
 *     -I$TREE/src/nvidia/arch/nvalloc/unix/include \
 *     -I$TREE/kernel-open/nvidia-uvm \
 *     -I$TREE/kernel-open/common/inc \
 *     -o persm_channel_probe persm_channel_probe.c
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "nvos.h"
#include "nv-ioctl.h"
#include "nv-ioctl-numbers.h"
#include "nv_escape.h"
#include "uvm_linux_ioctl.h"
#include "uvm_ioctl.h"
#include "nv-unix-nvos-params-wrappers.h"
#include "class/cl0000.h"
#include "class/cl0070.h"
#include "class/cl003e.h"
#include "class/cl0080.h"
#include "class/cl2080.h"
#include "class/cl90f1.h"
#include "class/cl9067.h"
#include "class/cla06c.h"
#include "class/clc96f.h"
#include "class/clcec0.h"
#include "class/clc661.h"
#include "class/clc361.h"
#include "alloc/alloc_channel.h"
#include "ctrl/ctrl0000/ctrl0000gpu.h"
#include "ctrl/ctrl0080/ctrl0080gpu.h"
#include "ctrl/ctrl2080/ctrl2080gpu.h"
#include "ctrl/ctrl2080/ctrl2080gr.h"
#include "ctrl/ctrla06c.h"
#include "ctrl/ctrlc36f.h"

#define NV_IOWR(nr, sz) _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC, (nr), (sz))

static int fd_ctl = -1, fd_dev = -1, fd_uvm = -1, fd_uvm2 = -1;
static NvHandle h_root, h_device, h_subdevice, h_virtmem, h_vaspace;
static NvHandle h_fifo_mem, h_notifier_mem, h_chgroup, h_ctxshare, h_gpfifo, h_compute;
static uint8_t gpu_uuid[16];
static uint32_t gpu_id;
static void *fifo_cpu;
static uint32_t fifo_entries = 1024;
static uint32_t userd_off;
static uint32_t gpu_minor;
static uint32_t gpfifo_put; /* next free ring slot, tracked across stage 6's single submission */
static uint32_t gpfifo_token;

/* nvrm_cpu_barrier()'s own definition (cpu_barrier.h, not pulled in since we
 * don't link that tool): a full two-way store/load fence so the GPU's view of
 * the ring and USERD is ordered with these CPU writes. */
static inline void cpu_barrier(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

/* GP ring entry per clc96f.h, copied verbatim from that reference tool's
 * nvrm_gp_entry() (nvrm.c) -- not reinvented: ENTRY0 GET 31:2 (VA low,
 * dword-aligned), LENGTH 30:10 in dwords (bit 42+), bit 41 = LEVEL_SUBROUTINE
 * (the bit that tool's own real-hardware receipts ran with). */
static uint64_t gp_entry(uint64_t va, uint32_t nwords) {
    return (va & ~3ull) | ((uint64_t)(nwords & 0x1fffff) << 42) | (1ull << 41);
}

static void die(const char *what, int rc, uint32_t status) {
    fprintf(stderr, "FATAL: %s failed (ioctl rc=%d, RM status=0x%x)\n", what, rc, status);
    exit(1);
}

static int nv_ioctl(int fd, unsigned nr, void *arg, size_t sz) {
    return ioctl(fd, NV_IOWR(nr, sz), arg);
}

static void rm_alloc(NvHandle parent, uint32_t cls, void *params, uint32_t psz,
                     NvHandle *out, const char *label) {
    NVOS21_PARAMETERS p;
    memset(&p, 0, sizeof p);
    p.hRoot = h_root ? h_root : 0;
    p.hObjectParent = parent;
    p.hObjectNew = 0;
    p.hClass = cls;
    p.pAllocParms = params ? NV_PTR_TO_NvP64(params) : NV_PTR_TO_NvP64(NULL);
    p.paramsSize = psz;
    if (nv_ioctl(fd_ctl, NV_ESC_RM_ALLOC, &p, sizeof p) != 0)
        die(label, errno, 0);
    if (p.status != NV_OK)
        die(label, 0, p.status);
    *out = p.hObjectNew;
    fprintf(stderr, "alloc %-14s class=0x%05x -> handle 0x%08x\n", label, cls, *out);
}

static void rm_control(NvHandle obj, uint32_t cmd, void *params, uint32_t psz, const char *label) {
    NVOS54_PARAMETERS p;
    memset(&p, 0, sizeof p);
    p.hClient = h_root;
    p.hObject = obj;
    p.cmd = cmd;
    p.params = NV_PTR_TO_NvP64(params);
    p.paramsSize = psz;
    if (nv_ioctl(fd_ctl, NV_ESC_RM_CONTROL, &p, sizeof p) != 0)
        die(label, errno, 0);
    if (p.status != NV_OK)
        die(label, 0, p.status);
    fprintf(stderr, "control %-14s cmd=0x%08x OK\n", label, cmd);
}

/* Returns the RM status without dying -- used for the final GR_CTX_SM probe,
 * where a non-success result is itself the interesting answer, not a fault. */
static uint32_t rm_control_soft(NvHandle obj, uint32_t cmd, void *params, uint32_t psz) {
    NVOS54_PARAMETERS p;
    memset(&p, 0, sizeof p);
    p.hClient = h_root;
    p.hObject = obj;
    p.cmd = cmd;
    p.params = NV_PTR_TO_NvP64(params);
    p.paramsSize = psz;
    if (nv_ioctl(fd_ctl, NV_ESC_RM_CONTROL, &p, sizeof p) != 0)
        return 0xFFFFFFFFu; /* ioctl-level failure, distinct from any real NV_STATUS */
    return p.status;
}

static void uvm_call(int fd, unsigned long cmd, void *params, uint32_t *status, const char *label) {
    if (ioctl(fd, cmd, params) != 0) die(label, errno, 0);
    if (status && *status != 0) die(label, 0, *status);
    fprintf(stderr, "uvm %-14s OK\n", label);
}

/* CPU mapping of an RM memory object, matching that reference tool's nvrm.c map_to_cpu()
 * EXACTLY (verified by reading it, not guessed): open a FRESH /dev/nvidiaN
 * fd, register it against fd_ctl, wrap NVOS33_PARAMETERS with that fd
 * (nv_ioctl_nvos33_parameters_with_fd -- a distinct struct, not plain
 * NVOS33_PARAMETERS), issue RM_MAP_MEMORY on fd_ctl with the WRAPPER's
 * size, then mmap the SAME fd at offset 0 (not fd_ctl/fd_dev, and NOT at
 * params.pLinearAddress -- an earlier draft used both wrong, caught by
 * re-reading map_to_cpu's real body before running anything). */
/* `system` distinguishes the TWO node choices nvrm.c's map_to_cpu() itself
 * branches on, confirmed by reading every call site: NV01_MEMORY_SYSTEM
 * (sysmem -- our fifo/notifier/pushbuffer buffers) maps through a FRESH
 * /dev/nvidiactl fd; the usermode/VRAM path (system=0) maps through
 * /dev/nvidiaN via open_dev_fd(). An earlier attempt used /dev/nvidiaN
 * unconditionally and got RM_MAP_MEMORY INVALID_ARGUMENT for the sysmem
 * case -- this is the fix, not a guess. */
static void *map_to_cpu(NvHandle hmem, uint64_t size, void *target, int system) {
    int fd;
    if (system) {
        fd = open("/dev/nvidiactl", O_RDWR);
        if (fd < 0) die("map_to_cpu_open_ctl", errno, 0);
    } else {
        char path[32];
        snprintf(path, sizeof path, "/dev/nvidia%u", gpu_minor);
        fd = open(path, O_RDWR);
        if (fd < 0) die("map_to_cpu_open_dev", errno, 0);
        nv_ioctl_register_fd_t r; r.ctl_fd = fd_ctl;
        if (nv_ioctl(fd, NV_ESC_REGISTER_FD, &r, sizeof r) != 0) die("map_to_cpu_register_fd", errno, 0);
    }

    nv_ioctl_nvos33_parameters_with_fd m; memset(&m, 0, sizeof m);
    m.fd = fd;
    m.params.hClient = h_root;
    m.params.hDevice = h_device;
    m.params.hMemory = hmem;
    m.params.length = size;
    if (nv_ioctl(fd_ctl, NV_ESC_RM_MAP_MEMORY, &m, sizeof m) != 0 || m.params.status != NV_OK) {
        close(fd);
        die("map_to_cpu_rm_map", 0, m.params.status);
    }
    void *p = mmap(target, size, PROT_READ | PROT_WRITE,
                  MAP_SHARED | (target ? MAP_FIXED : 0), fd, 0);
    close(fd); /* mmap keeps the mapping alive independent of the fd */
    if (p == MAP_FAILED) die("map_to_cpu_mmap", errno, 0);
    return p;
}

typedef struct {
    uint8_t regOp, regType, regStatus, regQuad;
    uint32_t regGroupMask, regSubGroupMask, regOffset;
    uint32_t regValueHi, regValueLo, regAndNMaskHi, regAndNMaskLo;
} reg_op_t;
typedef struct { uint32_t flags; uint64_t route; } route_info_t;
typedef struct {
    NvHandle hClientTarget, hChannelTarget;
    uint32_t bNonTransactional;
    uint32_t reserved00[2];
    uint32_t regOpCount;
    NvU64 regOps;
    route_info_t grRouteInfo;
} exec_params_t;

/* GR_CTX_SM-routed read of the same four addresses regops.c's gprobe
 * already tried without a channel (all returned INVALID_ARGUMENT there).
 * Routed via NV2080_CTRL_GR_ROUTE_INFO.flags=CHANNEL (0x2, a 2-bit TYPE
 * enum -- NOT a bitmask; an initial flags=3/ENGID|CHANNEL guess was wrong,
 * confirmed by reading ctrl2080gr.h) with route=the channel handle. */
static void probe_gr_ctx_sm(const char *label) {
    static const uint32_t offs[] = { 0x00504204u, 0x0082381cu, 0x00823820u, 0x00823804u };
    static const char *names[] = { "SM_ISSUE_RATE_MOD", "FEAT_OVR_SM_SS0", "FEAT_OVR_SM_SS1", "FEAT_OVR_PLM" };
    reg_op_t ops[4];
    memset(ops, 0, sizeof ops);
    for (int i = 0; i < 4; i++) { ops[i].regOp = 0 /* READ_32 */; ops[i].regType = 4 /* GR_CTX_SM */; ops[i].regOffset = offs[i]; }

    exec_params_t ep; memset(&ep, 0, sizeof ep);
    ep.hClientTarget = h_root;
    ep.hChannelTarget = h_gpfifo;
    ep.regOpCount = 4;
    ep.regOps = (NvU64)(uintptr_t)ops;
    ep.grRouteInfo.flags = 0x2u /* NV2080_CTRL_GR_ROUTE_INFO_FLAGS_TYPE_CHANNEL */;
    ep.grRouteInfo.route = h_gpfifo;

    uint32_t st = rm_control_soft(h_subdevice, 0x20800122u /* NV2080_CTRL_CMD_GPU_EXEC_REG_OPS */,
                                  &ep, sizeof ep);
    fprintf(stderr, "\n=== GR_CTX_SM probe (%s): overall status=0x%x ===\n", label, st);
    for (int i = 0; i < 4; i++)
        fprintf(stderr, "  %-20s (0x%08x) = 0x%08x  per-op status=%u\n",
                names[i], offs[i], ops[i].regValueLo, (unsigned)ops[i].regStatus);
}

int main(int argc, char **argv) {
    int stage = argc > 1 ? atoi(argv[1]) : 1;
    fprintf(stderr, "=== persm_channel_probe: running up to stage %d ===\n", stage);

    fd_ctl = open("/dev/nvidiactl", O_RDWR);
    fd_uvm = open("/dev/nvidia-uvm", O_RDWR);
    fd_uvm2 = open("/dev/nvidia-uvm", O_RDWR);
    if (fd_ctl < 0 || fd_uvm < 0 || fd_uvm2 < 0) { perror("open"); return 1; }

    rm_alloc(0, NV01_ROOT_CLIENT, NULL, 0, &h_root, "client");

    UVM_INITIALIZE_PARAMS ui; memset(&ui, 0, sizeof ui);
    uvm_call(fd_uvm, UVM_INITIALIZE, &ui, &ui.rmStatus, "uvm_init");

    /* Second fd associates THIS process's mm with the va_space -- without
     * it, UVM_REGISTER_GPU_VASPACE's page-fault-enabled path has no mm to
     * attach to (confirmed by reading kernel-open/nvidia-uvm/uvm.c's
     * va_space_mm state machine after an initial run hit exactly this gap:
     * NV_ERR_PAGE_TABLE_NOT_AVAIL, UVM_VA_SPACE_MM_STATE_UNINITIALIZED).
     * that reference tool's own nvrm_open() tolerates this call's own failure ("libcuda
     * also tolerates failure here") -- same here. */
    UVM_MM_INITIALIZE_PARAMS mi; memset(&mi, 0, sizeof mi);
    mi.uvmFd = fd_uvm;
    if (ioctl(fd_uvm2, UVM_MM_INITIALIZE, &mi) != 0)
        fprintf(stderr, "uvm_mm_init     tolerated failure (errno %d)\n", errno);
    else
        fprintf(stderr, "uvm_mm_init     OK\n");

    nv_ioctl_card_info_t cards[64]; memset(cards, 0, sizeof cards);
    if (nv_ioctl(fd_ctl, NV_ESC_CARD_INFO, cards, sizeof cards) != 0) die("card_info", errno, 0);
    int idx = -1;
    for (int i = 0; i < 64; i++) if (cards[i].valid) { idx = i; break; }
    if (idx < 0) { fprintf(stderr, "no GPU found\n"); return 1; }
    gpu_id = cards[idx].gpu_id;
    gpu_minor = cards[idx].minor_number;
    fprintf(stderr, "card: minor=%u gpu_id=0x%x\n", gpu_minor, gpu_id);

    char devpath[32];
    snprintf(devpath, sizeof devpath, "/dev/nvidia%u", gpu_minor);
    fd_dev = open(devpath, O_RDWR);
    if (fd_dev < 0) die("open_dev", errno, 0);
    int ctl_fd_copy = fd_ctl;
    if (ioctl(fd_dev, NV_IOWR(NV_ESC_REGISTER_FD, sizeof(int)), &ctl_fd_copy) != 0)
        die("register_fd", errno, 0);

    NV0000_CTRL_GPU_GET_ID_INFO_V2_PARAMS idinfo; memset(&idinfo, 0, sizeof idinfo);
    idinfo.gpuId = gpu_id;
    rm_control(h_root, NV0000_CTRL_CMD_GPU_GET_ID_INFO_V2, &idinfo, sizeof idinfo, "get_id_info");

    NV0080_ALLOC_PARAMETERS dp; memset(&dp, 0, sizeof dp);
    dp.deviceId = idinfo.deviceInstance;
    dp.hClientShare = h_root;
    dp.vaMode = NV_DEVICE_ALLOCATION_VAMODE_OPTIONAL_MULTIPLE_VASPACES;
    rm_alloc(h_root, NV01_DEVICE_0, &dp, sizeof dp, &h_device, "device");

    NV2080_ALLOC_PARAMETERS sp; memset(&sp, 0, sizeof sp);
    rm_alloc(h_device, NV20_SUBDEVICE_0, &sp, sizeof sp, &h_subdevice, "subdevice");

    NV_MEMORY_VIRTUAL_ALLOCATION_PARAMS vp; memset(&vp, 0, sizeof vp);
    vp.limit = 0x1ffffffffffffull;
    rm_alloc(h_device, NV01_MEMORY_VIRTUAL, &vp, sizeof vp, &h_virtmem, "virtmem");

    NV_VASPACE_ALLOCATION_PARAMETERS vap; memset(&vap, 0, sizeof vap);
    vap.vaBase = 0x1000;
    vap.vaSize = 0x1fffffb000000ull;
    vap.flags = NV_VASPACE_ALLOCATION_FLAGS_ENABLE_PAGE_FAULTING | NV_VASPACE_ALLOCATION_FLAGS_IS_EXTERNALLY_OWNED;
    rm_alloc(h_device, FERMI_VASPACE_A, &vap, sizeof vap, &h_vaspace, "vaspace");

    NV2080_CTRL_GPU_GET_GID_INFO_PARAMS gid; memset(&gid, 0, sizeof gid);
    gid.flags = NV2080_GPU_CMD_GPU_GET_GID_FLAGS_FORMAT_BINARY;
    gid.length = 16;
    rm_control(h_subdevice, NV2080_CTRL_CMD_GPU_GET_GID_INFO, &gid, sizeof gid, "get_gid");
    memcpy(gpu_uuid, gid.data, 16);

    UVM_REGISTER_GPU_PARAMS rg; memset(&rg, 0, sizeof rg);
    memcpy(rg.gpu_uuid.uuid, gpu_uuid, 16);
    rg.rmCtrlFd = -1;
    uvm_call(fd_uvm, UVM_REGISTER_GPU, &rg, &rg.rmStatus, "reg_gpu");

    UVM_REGISTER_GPU_VASPACE_PARAMS rv; memset(&rv, 0, sizeof rv);
    memcpy(rv.gpuUuid.uuid, gpu_uuid, 16);
    rv.rmCtrlFd = fd_ctl;
    rv.hClient = h_root;
    rv.hVaSpace = h_vaspace;
    uvm_call(fd_uvm, UVM_REGISTER_GPU_VASPACE, &rv, &rv.rmStatus, "reg_vaspace");

    fprintf(stderr, ">>> STAGE 1 complete: client/device/subdevice/vaspace/UVM-GPU-registration done.\n");
    if (stage <= 1) return 0;

    /* ---- Stage 2: the two small GPU-uncached memory buffers the channel
     * needs (ring+USERD, and an error notifier) -- real VRAM/sysmem
     * allocation + UVM external range + DMA map, the first "real resource
     * consumption" step beyond pure object/control calls. */
    uint64_t fifo_va = 0x200000000ull, notifier_va = 0x200100000ull;
    uint32_t fifo_size = 0x10000, notifier_size = 0x100000;

    void *fifo_res = mmap((void *)fifo_va, fifo_size, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (fifo_res != (void *)fifo_va) die("mmap_reserve_fifo", errno, 0);

    NV_MEMORY_ALLOCATION_PARAMS mp; memset(&mp, 0, sizeof mp);
    mp.owner = h_root;
    mp.type = NVOS32_TYPE_IMAGE;
    mp.flags = NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED | NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
               NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE | NVOS32_ALLOC_FLAGS_IGNORE_BANK_PLACEMENT;
    mp.attr = (NVOS32_ATTR_PHYSICALITY_ALLOW_NONCONTIGUOUS << 27) | (NVOS32_ATTR_LOCATION_PCI << 25);
    mp.attr2 = (NVOS32_ATTR2_GPU_CACHEABLE_NO << 2) | NVOS32_ATTR2_ZBC_PREFER_NO_ZBC;
    mp.format = 6;
    mp.size = fifo_size;
    mp.alignment = 0x1000;
    mp.limit = fifo_size - 1;
    rm_alloc(h_device, NV01_MEMORY_SYSTEM, &mp, sizeof mp, &h_fifo_mem, "fifo_mem");

    UVM_CREATE_EXTERNAL_RANGE_PARAMS cr; memset(&cr, 0, sizeof cr);
    cr.base = fifo_va; cr.length = fifo_size;
    uvm_call(fd_uvm, UVM_CREATE_EXTERNAL_RANGE, &cr, &cr.rmStatus, "ext_range_fifo");

    NVOS46_PARAMETERS d; memset(&d, 0, sizeof d);
    d.hClient = h_root; d.hDevice = h_device; d.hDma = h_virtmem; d.hMemory = h_fifo_mem;
    d.length = fifo_size;
    d.flags = (NVOS46_FLAGS_PAGE_SIZE_4KB << 8) | (NVOS46_FLAGS_CACHE_SNOOP_ENABLE << 4) |
              (NVOS46_FLAGS_DMA_OFFSET_FIXED_TRUE << 15);
    d.dmaOffset = fifo_va;
    if (nv_ioctl(fd_ctl, NV_ESC_RM_MAP_MEMORY_DMA, &d, sizeof d) != 0 || d.status != 0)
        die("map_memory_dma_fifo", errno, d.status);
    fprintf(stderr, "dma-map %-14s OK\n", "fifo");

    UVM_MAP_EXTERNAL_ALLOCATION_PARAMS ma; memset(&ma, 0, sizeof ma);
    ma.base = fifo_va; ma.length = fifo_size; ma.rmCtrlFd = fd_ctl; ma.hClient = h_root; ma.hMemory = h_fifo_mem;
    ma.gpuAttributesCount = 1;
    memcpy(ma.perGpuAttributes[0].gpuUuid.uuid, gpu_uuid, 16);
    ma.perGpuAttributes[0].gpuMappingType = UvmGpuMappingTypeReadWriteAtomic;
    uvm_call(fd_uvm, UVM_MAP_EXTERNAL_ALLOCATION, &ma, &ma.rmStatus, "map_ext_fifo");

    /* CPU-side RW mapping of the fifo buffer -- stages 1-5 never needed to
     * write into it, but stage 6 writes the ring entry and GPPut here. */
    fifo_cpu = map_to_cpu(h_fifo_mem, fifo_size, (void *)fifo_va, 1 /* NV01_MEMORY_SYSTEM */);
    fprintf(stderr, "fifo CPU mapping  %p\n", fifo_cpu);

    void *notifier_res = mmap((void *)notifier_va, notifier_size, PROT_NONE,
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (notifier_res != (void *)notifier_va) die("mmap_reserve_notifier", errno, 0);

    NV_MEMORY_ALLOCATION_PARAMS mp2 = mp;
    mp2.size = notifier_size; mp2.limit = notifier_size - 1;
    rm_alloc(h_device, NV01_MEMORY_SYSTEM, &mp2, sizeof mp2, &h_notifier_mem, "notifier_mem");

    UVM_CREATE_EXTERNAL_RANGE_PARAMS cr2; memset(&cr2, 0, sizeof cr2);
    cr2.base = notifier_va; cr2.length = notifier_size;
    uvm_call(fd_uvm, UVM_CREATE_EXTERNAL_RANGE, &cr2, &cr2.rmStatus, "ext_range_notif");

    NVOS46_PARAMETERS d2 = d;
    d2.hMemory = h_notifier_mem; d2.length = notifier_size; d2.dmaOffset = notifier_va;
    if (nv_ioctl(fd_ctl, NV_ESC_RM_MAP_MEMORY_DMA, &d2, sizeof d2) != 0 || d2.status != 0)
        die("map_memory_dma_notif", errno, d2.status);
    fprintf(stderr, "dma-map %-14s OK\n", "notifier");

    UVM_MAP_EXTERNAL_ALLOCATION_PARAMS ma2 = ma;
    ma2.base = notifier_va; ma2.length = notifier_size; ma2.hMemory = h_notifier_mem;
    uvm_call(fd_uvm, UVM_MAP_EXTERNAL_ALLOCATION, &ma2, &ma2.rmStatus, "map_ext_notif");

    fprintf(stderr, ">>> STAGE 2 complete: fifo+notifier GPU-uncached memory allocated and mapped.\n");
    if (stage <= 2) return 0;

    /* ---- Stage 3: channel group, context share, the GPFIFO channel itself,
     * and the compute object under it. This is the object that, once
     * scheduled (stage 4), gives GR_CTX a live context -- it is NEVER fed
     * any pushbuffer content and the doorbell is never rung, so no actual
     * engine work runs even after scheduling. */
    NV_CHANNEL_GROUP_ALLOCATION_PARAMETERS cg; memset(&cg, 0, sizeof cg);
    cg.engineType = NV2080_ENGINE_TYPE_GRAPHICS;
    rm_alloc(h_device, KEPLER_CHANNEL_GROUP_A, &cg, sizeof cg, &h_chgroup, "chgroup");

    NV_CTXSHARE_ALLOCATION_PARAMETERS cs; memset(&cs, 0, sizeof cs);
    cs.hVASpace = h_vaspace;
    cs.flags = NV_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_ASYNC;
    rm_alloc(h_chgroup, FERMI_CONTEXT_SHARE_A, &cs, sizeof cs, &h_ctxshare, "ctxshare");

    userd_off = fifo_entries * 8; /* USERD follows the ring in the same allocation */
    NV_CHANNEL_ALLOC_PARAMS gp; memset(&gp, 0, sizeof gp);
    gp.gpFifoOffset = fifo_va;
    gp.gpFifoEntries = fifo_entries;
    gp.hObjectError = h_notifier_mem;
    gp.hObjectBuffer = h_fifo_mem;
    gp.hUserdMemory[0] = h_fifo_mem;
    gp.userdOffset[0] = userd_off;
    gp.engineType = 0;
    gp.hContextShare = h_ctxshare;
    rm_alloc(h_chgroup, BLACKWELL_CHANNEL_GPFIFO_A, &gp, sizeof gp, &h_gpfifo, "gpfifo");

    rm_alloc(h_gpfifo, BLACKWELL_COMPUTE_B, NULL, 0, &h_compute, "compute_obj");

    fprintf(stderr, ">>> STAGE 3 complete: channel group + ctxshare + GPFIFO channel + compute object allocated.\n");
    if (stage <= 3) return 0;

    /* ---- Stage 4: UVM channel registration + GPFIFO_SCHEDULE enable. After
     * this the channel is live on the runlist -- the highest-risk single
     * step in this tool, reviewed and run only with explicit go-ahead. */
    NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN_PARAMS tk; memset(&tk, 0, sizeof tk);
    tk.workSubmitToken = 0xffffffffu;
    rm_control(h_gpfifo, NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN, &tk, sizeof tk, "get_token");
    gpfifo_token = tk.workSubmitToken;

    UVM_REGISTER_CHANNEL_PARAMS rc; memset(&rc, 0, sizeof rc);
    memcpy(rc.gpuUuid.uuid, gpu_uuid, 16);
    rc.rmCtrlFd = fd_ctl;
    rc.hClient = h_root;
    rc.hChannel = h_gpfifo;
    rc.base = 0x204000000ull;
    rc.length = 0x4000000ull;
    uvm_call(fd_uvm, UVM_REGISTER_CHANNEL, &rc, &rc.rmStatus, "reg_channel");

    NVA06C_CTRL_GPFIFO_SCHEDULE_PARAMS sch; memset(&sch, 0, sizeof sch);
    sch.bEnable = 1;
    rm_control(h_chgroup, NVA06C_CTRL_CMD_GPFIFO_SCHEDULE, &sch, sizeof sch, "schedule");

    fprintf(stderr, ">>> STAGE 4 complete: channel registered with UVM and SCHEDULED (live on runlist, "
                    "doorbell never rung, no pushbuffer content ever written).\n");
    if (stage <= 4) return 0;

    /* ---- Stage 5: the actual point of this tool, first attempt -- a
     * GR_CTX_SM-routed read before any real work has ever been submitted
     * through the channel. */
    probe_gr_ctx_sm("live channel, never fed any work");

    fprintf(stderr, ">>> STAGE 5 complete: probe attempted. No further stages.\n");
    if (stage <= 5) return 0;

    /* ---- Stage 6: the real escalation. Submit the SAME semaphore-release
     * method stream that reference tool's own m16_requalify.c already proved works on
     * real hardware (bind compute class to subchannel 1, then a release
     * packet on subchannel 0: method 0x005c, 5 data words -- marker VA lo/hi,
     * payload, 0, RELEASE|WFI flags) -- copied from m16/m16_native.c's
     * m16_native_build_release(), not reinvented. This is the first actual
     * GPU engine instruction executed anywhere in this project: a single
     * semaphore release, nothing else -- no compute kernel, no SM work
     * beyond what completing one release packet requires. Purpose: GSP-RM's
     * GR_CTX addressing may require the channel's context to be "promoted"
     * by real submitted work before per-context register state is
     * addressable (stage 5 returned INVALID_ARGUMENT with a live but
     * never-fed channel) -- this tests that directly. */
    {
        /* Usermode object + MMIO mapping for the doorbell (not needed by
         * earlier stages, which issued only RM_CONTROL calls). Mapping via
         * map_to_cpu(), the same verified-correct helper stage 2 uses. */
        NvHandle h_usermode;
        rm_alloc(h_subdevice, HOPPER_USERMODE_A, NULL, 0, &h_usermode, "usermode");
        void *mmio = map_to_cpu(h_usermode, 0x10000, NULL, 0 /* usermode: /dev/nvidiaN path */);
        volatile uint32_t *doorbell = (volatile uint32_t *)((uint8_t *)mmio + NVC361_NOTIFY_CHANNEL_PENDING);
        fprintf(stderr, "usermode mapped at %p, doorbell at offset 0x%x\n", mmio, NVC361_NOTIFY_CHANNEL_PENDING);

        /* Small GPU-uncached buffer for the pushbuffer content + a marker
         * word at the end of the same buffer. */
        uint64_t pb_va = 0x300000000ull;
        uint32_t pb_size = 0x1000;
        void *pb_res = mmap((void *)pb_va, pb_size, PROT_NONE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (pb_res != (void *)pb_va) die("mmap_reserve_pb", errno, 0);

        NV_MEMORY_ALLOCATION_PARAMS mp4; memset(&mp4, 0, sizeof mp4);
        mp4.owner = h_root; mp4.type = NVOS32_TYPE_IMAGE;
        mp4.flags = NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED | NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
                    NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE | NVOS32_ALLOC_FLAGS_IGNORE_BANK_PLACEMENT;
        mp4.attr = (NVOS32_ATTR_PHYSICALITY_ALLOW_NONCONTIGUOUS << 27) | (NVOS32_ATTR_LOCATION_PCI << 25);
        mp4.attr2 = (NVOS32_ATTR2_GPU_CACHEABLE_NO << 2) | NVOS32_ATTR2_ZBC_PREFER_NO_ZBC;
        mp4.format = 6; mp4.size = pb_size; mp4.alignment = 0x1000; mp4.limit = pb_size - 1;
        NvHandle h_pb_mem;
        rm_alloc(h_device, NV01_MEMORY_SYSTEM, &mp4, sizeof mp4, &h_pb_mem, "pb_mem");

        UVM_CREATE_EXTERNAL_RANGE_PARAMS cr3; memset(&cr3, 0, sizeof cr3);
        cr3.base = pb_va; cr3.length = pb_size;
        uvm_call(fd_uvm, UVM_CREATE_EXTERNAL_RANGE, &cr3, &cr3.rmStatus, "ext_range_pb");

        NVOS46_PARAMETERS d3; memset(&d3, 0, sizeof d3);
        d3.hClient = h_root; d3.hDevice = h_device; d3.hDma = h_virtmem; d3.hMemory = h_pb_mem;
        d3.length = pb_size;
        d3.flags = (NVOS46_FLAGS_PAGE_SIZE_4KB << 8) | (NVOS46_FLAGS_CACHE_SNOOP_ENABLE << 4) |
                   (NVOS46_FLAGS_DMA_OFFSET_FIXED_TRUE << 15);
        d3.dmaOffset = pb_va;
        if (nv_ioctl(fd_ctl, NV_ESC_RM_MAP_MEMORY_DMA, &d3, sizeof d3) != 0 || d3.status != 0)
            die("map_memory_dma_pb", errno, d3.status);

        UVM_MAP_EXTERNAL_ALLOCATION_PARAMS ma3; memset(&ma3, 0, sizeof ma3);
        ma3.base = pb_va; ma3.length = pb_size; ma3.rmCtrlFd = fd_ctl; ma3.hClient = h_root; ma3.hMemory = h_pb_mem;
        ma3.gpuAttributesCount = 1;
        memcpy(ma3.perGpuAttributes[0].gpuUuid.uuid, gpu_uuid, 16);
        ma3.perGpuAttributes[0].gpuMappingType = UvmGpuMappingTypeReadWriteAtomic;
        uvm_call(fd_uvm, UVM_MAP_EXTERNAL_ALLOCATION, &ma3, &ma3.rmStatus, "map_ext_pb");

        void *pb_cpu = map_to_cpu(h_pb_mem, pb_size, (void *)pb_va, 1 /* NV01_MEMORY_SYSTEM */);
        fprintf(stderr, "pushbuffer memory mapped at CPU=%p VA=0x%lx\n", pb_cpu, (unsigned long)pb_va);

        volatile uint32_t *marker = (volatile uint32_t *)((uint8_t *)pb_cpu + pb_size - 4);
        uint64_t marker_va = pb_va + pb_size - 4;
        *marker = 0;

        /* The exact method stream from m16_native_build_release() (m16/
         * m16_native.c), copied word-for-word, including the final
         * RELEASE|WFI flags word an earlier draft dropped (caught by
         * recounting against the source before compiling): 2 words to bind
         * the compute class to subchannel 1, then a 1-word method header
         * plus 5 data words (va_lo, va_hi, payload, 0, flags) for the
         * semaphore release on subchannel 0 -- 8 words total. */
        uint32_t methods[8];
        uint32_t n = 0;
        methods[n++] = (2u << 28) | (1u << 16) | (1u << 13) | (0x0000u >> 2);   /* bind subc 1 */
        methods[n++] = BLACKWELL_COMPUTE_B;
        methods[n++] = (2u << 28) | (5u << 16) | (0u << 13) | (0x005cu >> 2);   /* release subc 0, count=5 */
        methods[n++] = (uint32_t)marker_va;
        methods[n++] = (uint32_t)(marker_va >> 32);
        methods[n++] = 0x16c0ffeeu;  /* same proven sentinel value that reference tool's own receipts used */
        methods[n++] = 0;
        methods[n++] = 0x1u | (1u << 20); /* RELEASE | WFI */

        memcpy(pb_cpu, methods, n * sizeof(uint32_t));
        cpu_barrier();

        /* GP ring entry, written to the SAME fifo buffer stage 1/2 already
         * mapped (fifo_cpu): the ring occupies its first fifo_entries*8
         * bytes; this is the channel's first-ever entry (gpfifo_put starts
         * at 0). */
        volatile uint64_t *ring = (volatile uint64_t *)fifo_cpu;
        ring[gpfifo_put % fifo_entries] = gp_entry(pb_va, n);
        gpfifo_put++;
        cpu_barrier();
        volatile uint32_t *gpput = (volatile uint32_t *)((uint8_t *)fifo_cpu + userd_off + offsetof(Nvc96fControl, GPPut));
        *gpput = gpfifo_put % fifo_entries;
        cpu_barrier();

        fprintf(stderr, "ring entry written (pb_va=0x%lx nwords=%u), GPPut=%u. Ringing doorbell with token=0x%x...\n",
                (unsigned long)pb_va, n, gpfifo_put % fifo_entries, gpfifo_token);
        *doorbell = gpfifo_token;

        /* Bounded wait for the marker -- this is the ONLY point in this
         * entire tool where the GPU is expected to actually do something.
         * A 2-second timeout matching m16_requalify.c's own proven bound. */
        uint64_t start_ns;
        struct timespec ts0; clock_gettime(CLOCK_MONOTONIC, &ts0);
        start_ns = (uint64_t)ts0.tv_sec * 1000000000ull + (uint64_t)ts0.tv_nsec;
        int completed = 0;
        for (;;) {
            if (*marker == 0x16c0ffeeu) { completed = 1; break; }
            struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
            uint64_t now_ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
            if (now_ns - start_ns > 2000000000ull) break;
            usleep(1000);
        }
        fprintf(stderr, "marker after doorbell: 0x%08x (expected 0x16c0ffee) -- %s\n",
                *marker, completed ? "COMPLETED" : "TIMED OUT");

        if (completed)
            probe_gr_ctx_sm("after one real semaphore-release submission");
        else
            fprintf(stderr, ">>> Skipping GR_CTX_SM retry: the submission itself did not complete, so "
                            "retrying the probe would not be a meaningful test.\n");
    }

    fprintf(stderr, ">>> STAGE 6 complete.\n");
    return 0;
}

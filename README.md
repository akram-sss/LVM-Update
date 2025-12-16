# 🚀 LVM Auto-Extender - Complete Guide

**Professional automatic LVM management system for Red Hat Enterprise Linux**

Automatically extends hungry logical volumes by intelligently redistributing space from over-provisioned volumes. Features beautiful color-coded terminal output, HTTP dashboard, and safe test mode.

---

## 📋 Table of Contents

1. [What This Does](#what-this-does)
2. [Prerequisites](#prerequisites)
3. [Initial Setup](#initial-setup)
4. [Build & Run](#build--run)
5. [Testing & Usage](#testing--usage)
6. [Configuration](#configuration)
7. [Monitoring](#monitoring)
8. [Cleaning & Resetting](#cleaning--resetting)
9. [Troubleshooting](#troubleshooting)

---

## 🎯 What This Does

This program automatically manages LVM disk space:

- **Monitors** your filesystems every 8 seconds
- **Detects** when a volume reaches 80% usage (HUNGRY)
- **Finds** over-provisioned volumes with excess free space (<40% usage)
- **Shrinks** donor volumes safely (ext4 only, not XFS)
- **Extends** hungry volumes automatically
- **Adds** fallback storage if needed

**Safety:** Only monitors specific paths you configure. Your system disks (`/`, `/boot`, etc.) are never touched!

---

## ✅ Prerequisites

### Required Software

```bash
# Install on RHEL/CentOS/Fedora
sudo dnf install -y lvm2 gcc make curl

# Verify installation
lvs --version
gcc --version
make --version
```

### Required Hardware

- **Main disk:** `/dev/sdb` (20GB recommended) - for LVM volume group
- **Fallback disk:** `/dev/sdc` (20GB optional) - for extra space when needed

**Check your available disks:**
```bash
lsblk
```

**⚠️ WARNING:** Make sure `/dev/sdb` and `/dev/sdc` don't contain important data - they will be wiped!

---

## 🔧 Initial Setup

### Step 1: Create Physical Volume (PV)

```bash
# Wipe and initialize /dev/sdb
sudo pvcreate /dev/sdb

# Verify
sudo pvs
```

**Expected output:**
```
PV         VG     Fmt  Attr PSize  PFree
/dev/sdb          lvm2 ---  20.00g 20.00g
```

### Step 2: Create Volume Group (VG)

```bash
# Create volume group named 'vgdata'
sudo vgcreate vgdata /dev/sdb

# Verify
sudo vgs
```

**Expected output:**
```
VG     #PV #LV #SN Attr   VSize  VFree
vgdata   1   0   0 wz--n- 20.00g 20.00g
```

### Step 3: Create Logical Volumes (LVs)

```bash
# Create three logical volumes with different sizes
sudo lvcreate -L 5G -n lv_home vgdata
sudo lvcreate -L 3G -n lv_data1 vgdata
sudo lvcreate -L 3G -n lv_data2 vgdata

# Verify
sudo lvs
```

**Expected output:**
```
LV       VG     Attr       LSize
lv_data1 vgdata -wi-a----- 3.00g
lv_data2 vgdata -wi-a----- 3.00g
lv_home  vgdata -wi-a----- 5.00g
```

### Step 4: Create Filesystems

**For ext4 (can be shrunk):**
```bash
sudo mkfs.ext4 /dev/vgdata/lv_home
sudo mkfs.ext4 /dev/vgdata/lv_data1
```

**For XFS (RHEL default, cannot be shrunk):**
```bash
sudo mkfs.xfs /dev/vgdata/lv_data2
```

**Note:** XFS can only grow, never shrink. Only ext4 volumes can be used as donors!

### Step 5: Create Mount Points

```bash
# Create directories
sudo mkdir -p /mnt/lv_home /mnt/lv_data1 /mnt/lv_data2

# Mount volumes
sudo mount /dev/mapper/vgdata-lv_home /mnt/lv_home
sudo mount /dev/mapper/vgdata-lv_data1 /mnt/lv_data1
sudo mount /dev/mapper/vgdata-lv_data2 /mnt/lv_data2

# Verify mounts
df -h /mnt/lv_*
```

**Expected output:**
```
Filesystem                    Size  Used Avail Use% Mounted on
/dev/mapper/vgdata-lv_home    5.0G   68M  4.9G   2% /mnt/lv_home
/dev/mapper/vgdata-lv_data1   3.0G   68M  2.9G   3% /mnt/lv_data1
/dev/mapper/vgdata-lv_data2   3.0G   68M  2.9G   3% /mnt/lv_data2
```

### Step 6: Make Mounts Persistent (Optional)

To survive reboots, add to `/etc/fstab`:

```bash
sudo nano /etc/fstab

# Add these lines:
/dev/mapper/vgdata-lv_home   /mnt/lv_home   ext4   defaults   0 0
/dev/mapper/vgdata-lv_data1  /mnt/lv_data1  ext4   defaults   0 0
/dev/mapper/vgdata-lv_data2  /mnt/lv_data2  xfs    defaults   0 0
```

---

## 🏗️ Build & Run

### Step 1: Navigate to Project

```bash
cd /path/to/LVM/
```

### Step 2: Build the Program

```bash
make
```

**Expected output:**
```
════════════════════════════════════════════════════════════════
✓ Build complete: lvm_manager
════════════════════════════════════════════════════════════════
```

### Step 3: Run in Safe Test Mode

**Default mode is DRY_RUN = 1 (no real changes):**

```bash
sudo ./lvm_manager
```

**You should see:**
```
╔══════════════════════════════════════════════════════════════╗
║           LVM AUTO-EXTENDER FOR RED HAT LINUX               ║
╠══════════════════════════════════════════════════════════════╣
║  Automatic Logical Volume Management & Optimization         ║
╚══════════════════════════════════════════════════════════════╝

┌─ CONFIGURATION ────────────────────────────────────────────┐
│ Mode:              ⚠  DRY-RUN (Simulation)                  │
│ Check Interval:    8 seconds                                │
│ Hungry Threshold:  >= 80%                                   │
└────────────────────────────────────────────────────────────┘

[OK   ] Supervisor   │ Thread started - monitoring filesystems
[OK   ] HTTP         │ Dashboard listening on http://0.0.0.0:8080
[INFO ] Main         │ ✓ System operational - Press Ctrl+C to stop
```

### Step 4: Stop the Program

Press **Ctrl+C** for graceful shutdown:

```
[WARN ] Main │ Received signal SIGINT (Ctrl+C) - shutting down...
[OK   ] Main │ All threads stopped
[OK   ] Main │ Shutdown complete
```

---

## 🧪 Testing & Usage

### Test 1: Verify Monitoring

While running, the program logs detected volumes:

```
[DEBUG] DFParser    │ Found: /dev/mapper/vgdata-lv_home at /mnt/lv_home (5%)
[DEBUG] Supervisor  │ ✓ OK: /mnt/lv_home (5%)
```

**Note:** It will also log other filesystems but only MONITORS `/mnt/lv_*` paths!

### Test 2: Trigger Auto-Extension (DRY-RUN)

**Terminal 1:** Keep `lvm_manager` running

**Terminal 2:** Fill up a volume
```bash
# Check current usage
df -h /mnt/lv_home

# Fill it to 85% (adjust count as needed)
sudo dd if=/dev/zero of=/mnt/lv_home/testfile.bin bs=1M count=4000

# Check again
df -h /mnt/lv_home
```

**Watch Terminal 1 output:**
```
[WARN ] Supervisor │ 🔥 HUNGRY LV: /dev/mapper/vgdata-lv_home at /mnt/lv_home (85%)
[INFO ] Extender   │ 🔧 Processing extension for: /dev/mapper/vgdata-lv_home
[INFO ] Extender   │ [DRY-RUN] Would shrink /dev/vgdata/lv_data1 by 1GB
[INFO ] Extender   │ [DRY-RUN] Would extend /dev/vgdata/lv_home by 1GB
```

### Test 3: Enable Real Operations

**⚠️ WARNING:** This will perform actual LVM operations!

**Step 1:** Stop the program (Ctrl+C)

**Step 2:** Edit configuration
```bash
nano lvm_config.h

# Find this line (around line 12):
#define DRY_RUN                 1

# Change to:
#define DRY_RUN                 0

# Save and exit (Ctrl+X, Y, Enter)
```

**Step 3:** Rebuild
```bash
make clean
make
```

**Step 4:** Run with real operations
```bash
sudo ./lvm_manager
```

**Step 5:** Trigger extension again
```bash
# Fill up lv_home to >80%
sudo dd if=/dev/zero of=/mnt/lv_home/bigfile2.bin bs=1M count=4000
```

**Watch actual operations happen:**
```
[WARN ] Supervisor │ 🔥 HUNGRY LV detected (85%)
[INFO ] Extender   │ Found donor: vgdata/lv_data1 (will shrink by 1.00 GB)
[OK   ] Extender   │ Successfully shrunk vgdata/lv_data1
[OK   ] Extender   │ ✓ Extension completed successfully
```

**Verify changes:**
```bash
df -h /mnt/lv_*
sudo lvs
```

---

## ⚙️ Configuration

All settings are in **`lvm_config.h`**:

### Key Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `DRY_RUN` | 1 | `1` = test mode, `0` = real operations |
| `THRESHOLD_PCT` | 80 | Extend when volume reaches this % |
| `LOW_PCT` | 40 | Volume is over-provisioned below this % |
| `EXTEND_SIZE_GB` | 1 | GB to add/remove per operation |
| `CHECK_INTERVAL` | 8 | Seconds between filesystem checks |
| `FALLBACK_DEV` | "/dev/sdc" | Backup disk to add when needed |
| `MONITORED_MOUNTS` | (see below) | Paths to monitor |
| `DASHBOARD_PORT` | 8080 | HTTP dashboard port |

### Monitored Paths

**Line 33:**
```c
#define MONITORED_MOUNTS "/mnt/lv_home","/mnt/lv_data1","/mnt/lv_data2"
```

**To monitor different paths:**
1. Edit this line
2. Run `make clean && make`
3. Restart program

**After any config change:**
```bash
make clean
make
sudo ./lvm_manager
```

---

## 📊 Monitoring

### HTTP Dashboard

**Check status while running:**
```bash
curl http://localhost:8080
```

**Pretty print with jq:**
```bash
curl -s http://localhost:8080 | jq .
```

**Response example:**
```json
{
  "status": "running",
  "dry_run": true,
  "stats": {
    "checks": 45,
    "extensions_ok": 2,
    "extensions_fail": 0,
    "shrinks": 3,
    "fallback_pvs": 0
  },
  "volumes": [
    {
      "device": "/dev/mapper/vgdata-lv_home",
      "mount": "/mnt/lv_home",
      "use": 85,
      "msg": "extension succeeded"
    }
  ]
}
```

### Live Statistics

The program automatically displays statistics every 60 seconds:

```
╔═ SYSTEM STATISTICS ═══════════════════════════════════════╗
║ Uptime:              00:15:32                              ║
║ Checks Performed:    116                                   ║
║ Extensions Success:  3                                     ║
║ Shrinks Performed:   5                                     ║
╚═══════════════════════════════════════════════════════════╝
```

### Check LVM Status

```bash
# Volume usage
df -h /mnt/lv_*

# Logical volumes
sudo lvs -a -o +devices

# Volume group space
sudo vgs -o +vg_free

# Physical volumes
sudo pvs -o +pv_free
```

---

## 🧹 Cleaning & Resetting

### Quick Clean (Keep LVM Setup)

```bash
# Stop program
sudo pkill -INT lvm_manager

# Remove test files
sudo rm -rf /mnt/lv_home/writer*
sudo rm -f /mnt/lv_home/*.bin
sudo rm -f /mnt/lv_data1/*.bin
sudo rm -f /mnt/lv_data2/*.bin

# Check space freed
df -h /mnt/lv_*
```

### Full Reset (Start from Scratch)

**⚠️ WARNING:** This destroys all data on the LVs!

```bash
# 1. Stop program
sudo pkill -INT lvm_manager
ps aux | grep lvm_manager  # Verify stopped

# 2. Unmount filesystems
sudo umount /mnt/lv_home /mnt/lv_data1 /mnt/lv_data2 2>/dev/null

# 3. Remove logical volumes
sudo lvremove -f /dev/vgdata/lv_home
sudo lvremove -f /dev/vgdata/lv_data1
sudo lvremove -f /dev/vgdata/lv_data2

# 4. Remove volume group
sudo vgremove -f vgdata

# 5. Remove physical volumes
sudo pvremove -f /dev/sdb
sudo pvremove -f /dev/sdc 2>/dev/null

# 6. Verify clean state
sudo pvs
sudo vgs
sudo lvs
lsblk
```

**Then follow [Initial Setup](#initial-setup) to recreate everything.**

### Clean Build Artifacts

```bash
# Remove compiled files
make clean

# Rebuild
make
```

---

## 🐛 Troubleshooting

### Issue: "df: no file systems processed"

**Cause:** LVs are not mounted

**Solution:**
```bash
# Mount the volumes
sudo mount /dev/mapper/vgdata-lv_home /mnt/lv_home
sudo mount /dev/mapper/vgdata-lv_data1 /mnt/lv_data1
sudo mount /dev/mapper/vgdata-lv_data2 /mnt/lv_data2

# Verify
df -h /mnt/lv_*
```

### Issue: "Permission denied"

**Solution:** Always use `sudo`:
```bash
sudo ./lvm_manager
```

### Issue: "Port 8080 already in use"

**Solution:** Change the port in `lvm_config.h`:
```c
#define DASHBOARD_PORT          8090  // Changed from 8080
```
Then rebuild: `make clean && make`

### Issue: XFS volumes not being shrunk

**This is expected!** XFS can only grow, never shrink. Only ext4 volumes can be donors.

**Solution:** Create donor volumes with ext4:
```bash
sudo mkfs.ext4 /dev/vgdata/lv_data1
```

### Issue: Program monitoring system disks

**Impossible!** The program ONLY monitors paths in `MONITORED_MOUNTS`. 

Check the config:
```bash
grep MONITORED_MOUNTS lvm_config.h
```

You'll see it only watches `/mnt/lv_*` - your system disks are safe!

### Issue: No colors in output

**Your terminal doesn't support ANSI colors. The program still works!**

Or disable colors:
```c
// In lvm_config.h
#define LOG_USE_COLORS          0
```

### Check Logs for Errors

The color-coded output tells you exactly what's happening:
- 🟢 Green = Success
- 🟡 Yellow = Warning
- 🔴 Red = Error
- 🔵 Blue = Info

---

## 📚 Additional Resources

- **[SIMPLE_START.md](SIMPLE_START.md)** - Beginner's quick start guide
- **[README_NEW.md](README_NEW.md)** - Detailed feature documentation
- **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** - Command cheat sheet
- **[PROJECT_SUMMARY.md](PROJECT_SUMMARY.md)** - Architecture overview

---

## 🎯 Quick Command Reference

```bash
# Build
make

# Run in test mode
sudo ./lvm_manager

# Stop gracefully
Ctrl+C

# Check dashboard
curl http://localhost:8080

# Clean test files
sudo rm -rf /mnt/lv_home/writer* /mnt/lv_home/*.bin

# Check volume status
df -h /mnt/lv_*
sudo lvs

# Full reset
sudo umount /mnt/lv_*
sudo lvremove -f /dev/vgdata/*
sudo vgremove -f vgdata
sudo pvremove -f /dev/sdb /dev/sdc
```

---

## ✅ Safety Checklist

Before enabling real operations (`DRY_RUN = 0`):

- [ ] Tested in DRY_RUN mode thoroughly
- [ ] Understand which paths are monitored
- [ ] Know that XFS cannot be shrunk
- [ ] Have backups of important data
- [ ] Verified fallback device exists
- [ ] Watched several test cycles in DRY_RUN

---

**🎉 You're all set! Start with DRY_RUN mode and watch the beautiful color-coded output!**

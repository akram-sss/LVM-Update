# LVM Auto-Extender for Red Hat Linux

**Professional-grade automatic LVM management system with intelligent space redistribution and beautiful terminal output.**

Optimized for **Red Hat Enterprise Linux**, **CentOS**, **Fedora**, and **AlmaLinux**.

---

## ✨ Features

### 🎯 Core Functionality
- **Intelligent Monitoring**: Tracks filesystem usage every 8 seconds
- **Auto-Extension**: Automatically extends hungry logical volumes (≥80% full)
- **Smart Shrinking**: Safely shrinks over-provisioned LVs to free space
- **Fallback Storage**: Automatically adds fallback PVs when needed
- **XFS & ext4 Support**: Handles both RHEL default filesystems

### 🎨 Enhanced User Experience
- **Color-Coded Output**: Beautiful ANSI terminal colors for easy reading
- **Structured Logging**: Severity levels (DEBUG, INFO, SUCCESS, WARN, ERROR)
- **Real-Time Stats**: Live statistics display every 60 seconds
- **HTTP Dashboard**: JSON API on port 8080 for monitoring
- **Graceful Shutdown**: Signal handling for clean exit (Ctrl+C)

### 🏗️ Professional Architecture
- **Modular Design**: Separated into logical components (.c/.h files)
- **Thread-Safe**: Proper mutex locking for concurrent operations
- **Production-Ready**: DRY_RUN mode for safe testing
- **Easy Configuration**: All settings in `lvm_config.h`

---

## 📁 Project Structure

```
LVM/
├── lvm_main.c          # Main program & signal handling
├── lvm_logger.c/h      # Color-coded logging system
├── lvm_utils.c/h       # Volume management & df parsing
├── lvm_extender.c/h    # LVM extension/shrinking logic
├── lvm_threads.c/h     # Worker threads (supervisor, extender, writer, HTTP)
├── lvm_config.h        # Configuration constants
├── lvm_types.h         # Type definitions & structures
├── Makefile            # Build system
└── README_NEW.md       # This file
```

---

## 🚀 Quick Start

### 1. Prerequisites

```bash
# Install required packages (RHEL/CentOS/Fedora)
sudo dnf install -y lvm2 gcc make curl

# Verify LVM tools
lvs --version
vgs --version
```

### 2. Setup Test Environment

```bash
# Create physical volumes
sudo pvcreate /dev/sdb

# Create volume group
sudo vgcreate vgdata /dev/sdb

# Create logical volumes
sudo lvcreate -L 5G -n lv_home vgdata
sudo lvcreate -L 3G -n lv_data1 vgdata
sudo lvcreate -L 3G -n lv_data2 vgdata

# Create filesystems (ext4 or xfs)
sudo mkfs.ext4 /dev/vgdata/lv_home
sudo mkfs.ext4 /dev/vgdata/lv_data1
sudo mkfs.xfs /dev/vgdata/lv_data2  # XFS example

# Mount filesystems
sudo mkdir -p /mnt/lv_home /mnt/lv_data1 /mnt/lv_data2
sudo mount /dev/vgdata/lv_home /mnt/lv_home
sudo mount /dev/vgdata/lv_data1 /mnt/lv_data1
sudo mount /dev/vgdata/lv_data2 /mnt/lv_data2

# Verify
df -h /mnt/lv_*
```

### 3. Build the Program

```bash
# Clone or download the project
cd LVM/

# Build
make

# Run tests
make test
```

### 4. Configure

Edit `lvm_config.h` to customize:

```c
#define DRY_RUN                 1       // 1 = safe test mode, 0 = real operations
#define THRESHOLD_PCT           80      // % to trigger extension
#define LOW_PCT                 40      // % for over-provisioned detection
#define FALLBACK_DEV            "/dev/sdc"
```

### 5. Run in DRY-RUN Mode (Safe)

```bash
# Start the manager (no real changes)
sudo ./lvm_manager
```

**You'll see beautiful output like:**
```
╔══════════════════════════════════════════════════════════════╗
║           LVM AUTO-EXTENDER FOR RED HAT LINUX               ║
╠══════════════════════════════════════════════════════════════╣
║  Automatic Logical Volume Management & Optimization         ║
║  Intelligent space redistribution & load balancing          ║
╚══════════════════════════════════════════════════════════════╝

┌─ CONFIGURATION ────────────────────────────────────────────┐
│ Mode:              ⚠  DRY-RUN (Simulation)                  │
│ Check Interval:    8 seconds                                │
│ Hungry Threshold:  >= 80%                                   │
│ Low Threshold:     < 40%                                    │
│ Extension Size:    1 GB per operation                       │
│ Fallback Device:   /dev/sdc                                 │
│ Dashboard Port:    8080                                     │
└────────────────────────────────────────────────────────────┘

[OK   ] 12:34:56 Supervisor   │ Thread started - monitoring filesystems
[OK   ] 12:34:56 Extender     │ Thread started - ready to process extension requests
[OK   ] 12:34:56 HTTP         │ Dashboard listening on http://0.0.0.0:8080
[OK   ] 12:34:56 Main         │ All threads started successfully
```

### 6. Check the Dashboard

```bash
# View JSON status
curl http://localhost:8080

# Or open in browser
firefox http://localhost:8080
```

### 7. Enable Production Mode

When ready for real operations:

1. Edit `lvm_config.h`: set `DRY_RUN` to `0`
2. Rebuild: `make clean && make`
3. Run: `sudo ./lvm_manager`

---

## 🎮 Usage Examples

### Trigger Auto-Extension

```bash
# Fill up lv_home to >80%
sudo dd if=/dev/zero of=/mnt/lv_home/bigfile.bin bs=1M count=4000

# Watch the manager logs - you'll see:
# [WARN ] Supervisor   │ 🔥 HUNGRY LV: /dev/mapper/vgdata-lv_home at /mnt/lv_home (85%)
# [INFO ] Extender     │ 🔧 Processing extension for: /dev/mapper/vgdata-lv_home
# [INFO ] Extender     │ Found donor LV: vgdata/lv_data1 (will shrink by 1.00 GB)
# [OK   ] Extender     │ ✓ Extension completed successfully
```

### Monitor Statistics

The program automatically displays statistics every 60 seconds:

```
╔═ SYSTEM STATISTICS ═══════════════════════════════════════╗
║ Uptime:              00:15:32                              ║
║ Checks Performed:    116                                   ║
║ Extensions Success:  3                                     ║
║ Extensions Failed:   0                                     ║
║ Shrinks Performed:   5                                     ║
║ Fallback PVs Added:  0                                     ║
╚═══════════════════════════════════════════════════════════╝
```

### Graceful Shutdown

```bash
# Press Ctrl+C to trigger graceful shutdown
# You'll see:
# [WARN ] Main         │ Received signal SIGINT (Ctrl+C) - initiating graceful shutdown...
# [INFO ] Supervisor   │ Thread shutting down
# [INFO ] Extender     │ Thread shutting down
# [OK   ] Main         │ All threads stopped
# [OK   ] Main         │ Shutdown complete
```

---

## ⚙️ Configuration Reference

### Key Settings in `lvm_config.h`

| Setting | Default | Description |
|---------|---------|-------------|
| `DRY_RUN` | 1 | Safe test mode (no real LVM ops) |
| `CHECK_INTERVAL` | 8 | Seconds between filesystem checks |
| `THRESHOLD_PCT` | 80 | Usage % to trigger extension |
| `LOW_PCT` | 40 | Usage % for over-provisioned |
| `EXTEND_SIZE_GB` | 1 | GB to extend/shrink per operation |
| `FALLBACK_DEV` | "/dev/sdc" | Fallback physical volume |
| `DASHBOARD_PORT` | 8080 | HTTP API port |
| `WRITER_ENABLED` | 1 | Enable load generators |

---

## 🔧 Makefile Targets

```bash
make              # Build the program
make clean        # Remove build artifacts
make install      # Install to /usr/local/bin
make uninstall    # Remove from system
make test         # Run validation tests
make debug        # Build with debug symbols
make production   # Optimized production build
make help         # Show help
```

---

## 📊 HTTP Dashboard API

### Endpoint: `GET /`

Returns JSON with system status:

```json
{
  "status": "running",
  "dry_run": true,
  "stats": {
    "checks": 116,
    "extensions_ok": 3,
    "extensions_fail": 0,
    "shrinks": 5,
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

---

## 🐛 Troubleshooting

### Permission Denied

```bash
# LVM commands require root
sudo ./lvm_manager
```

### Port 8080 Already in Use

Edit `lvm_config.h`:
```c
#define DASHBOARD_PORT          8090  // Use different port
```

### XFS Cannot Be Shrunk

XFS filesystems can only be grown, not shrunk. The program will automatically skip XFS donors and only shrink ext4 LVs.

### SELinux Issues (RHEL)

```bash
# Temporarily disable (testing only)
sudo setenforce 0

# Or add proper context
sudo chcon -t bin_t ./lvm_manager
```

---

## 🔒 Security Considerations

- **Requires root**: LVM operations need elevated privileges
- **DRY_RUN first**: Always test with `DRY_RUN=1` before production
- **Backup data**: Ensure backups before enabling real operations
- **Monitor logs**: Watch output carefully during first runs
- **Firewall**: Dashboard port (8080) should be firewalled in production

---

## 🎨 Color Output

The program uses ANSI colors for better readability:

- 🔵 **Blue** - INFO messages
- 🟢 **Green** - SUCCESS operations
- 🟡 **Yellow** - WARNINGS
- 🔴 **Red** - ERRORS
- ⚫ **Cyan/Dim** - DEBUG info

Colors auto-disable when output is piped or redirected.

---

## 📝 Logging Levels

- **DEBUG**: Detailed technical info (compile with `-DDEBUG`)
- **INFO**: Normal operational messages
- **SUCCESS**: Successful operations
- **WARNING**: Important alerts that don't stop operation
- **ERROR**: Failed operations
- **CRITICAL**: Fatal errors causing shutdown

---

## 🧪 Testing Workflow

1. **Build with DRY_RUN=1**
2. **Run and observe logs**
3. **Manually fill filesystems to test triggers**
4. **Verify correct LV classification (HUNGRY, OK, OVERPROVISIONED)**
5. **Check dashboard API works**
6. **Test graceful shutdown (Ctrl+C)**
7. **Set DRY_RUN=0 and rebuild**
8. **Run in production**

---

## 🤝 Contributing

Improvements welcome! Focus areas:
- Additional filesystem support (btrfs, etc.)
- Web-based dashboard UI
- Email/Slack alerting
- Systemd service unit
- Configuration file support (YAML/JSON)

---

## 📄 License

Open source - use and modify as needed for your environment.

---

## 🙏 Credits

Enhanced version with modular architecture, beautiful output, and Red Hat optimizations.

Original concept: LVM auto-extender with donor shrinking

---

**Enjoy professional LVM management with beautiful terminal output! 🚀**

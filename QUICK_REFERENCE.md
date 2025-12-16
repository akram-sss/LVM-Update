# LVM Auto-Extender - Quick Reference

## 🚀 Quick Commands

### Build & Run
```bash
make                    # Build
sudo ./lvm_manager      # Run
curl localhost:8080     # Check status
```

### Common Tasks
```bash
# Start in test mode (safe)
sudo ./lvm_manager

# View in real-time with color
sudo ./lvm_manager | less -R

# Run in background
sudo ./lvm_manager > lvm.log 2>&1 &

# Stop gracefully
sudo pkill -INT lvm_manager
```

## 📊 Log Message Types

| Symbol | Level | Color | Meaning |
|--------|-------|-------|---------|
| DEBUG | Debug | Cyan | Technical details |
| INFO  | Info | Blue | Normal operation |
| OK    | Success | Green | Operation succeeded |
| WARN  | Warning | Yellow | Attention needed |
| ERROR | Error | Red | Operation failed |
| CRIT  | Critical | Red BG | Fatal error |

## 🔧 Configuration Quick Edit

```bash
# Edit config
nano lvm_config.h

# Key settings:
DRY_RUN = 1          # Test mode (no changes)
DRY_RUN = 0          # Production mode
THRESHOLD_PCT = 80   # Trigger at 80% full
LOW_PCT = 40         # Over-provisioned below 40%
```

## 📈 Volume States

| State | Symbol | Meaning | Action |
|-------|--------|---------|--------|
| OK | ✓ | 40-80% usage | None - monitoring |
| HUNGRY | 🔥 | ≥80% usage | Extend LV |
| OVERPROVISIONED | 💤 | <40% usage | Donor candidate |

## 🎯 Workflow

1. **Monitoring** - Supervisor checks every 8s
2. **Detection** - LV reaches 80% (HUNGRY)
3. **Queue** - Device queued for extension
4. **Shrink** - Find donors with >1GB free (ext4 only)
5. **Extend** - Add freed space to hungry LV
6. **Fallback** - Add `/dev/sdc` if still needed
7. **Complete** - LV extended, usage drops

## 🔍 Debugging

```bash
# Enable debug logs (rebuild required)
# Add to lvm_config.h:
#define DEBUG 1

# Check LVM status
sudo lvs -a -o +devices
sudo vgs -o +vg_free
sudo pvs -o +pv_free

# Monitor filesystem
watch -n 1 'df -h /mnt/lv_*'

# Check HTTP dashboard
curl -s localhost:8080 | jq .
```

## 🚨 Emergency Stop

```bash
# Graceful (recommended)
sudo pkill -INT lvm_manager

# Immediate
sudo pkill -9 lvm_manager

# Verify stopped
ps aux | grep lvm_manager
```

## 📂 File Reference

| File | Purpose |
|------|---------|
| `lvm_main.c` | Entry point, signal handling |
| `lvm_logger.c` | Color output, formatting |
| `lvm_utils.c` | Volume tracking, df parsing |
| `lvm_extender.c` | LVM shrink/extend logic |
| `lvm_threads.c` | Supervisor, extender, HTTP |
| `lvm_config.h` | **All configuration here** |
| `lvm_types.h` | Data structures |

## 🎨 Terminal Colors

If colors don't show:
```bash
# Force colors
export TERM=xterm-256color

# Or disable in code
# Edit lvm_config.h:
#define LOG_USE_COLORS 0
```

## 🔄 Reset Lab

```bash
# Stop program
sudo pkill lvm_manager

# Clean test files
sudo rm -rf /mnt/lv_home/writer*
sudo rm -f /mnt/lv_home/*.bin

# Reset LVs (DESTRUCTIVE!)
sudo umount /mnt/lv_*
sudo lvremove -f /dev/vgdata/*
sudo vgremove -f vgdata
sudo pvremove -f /dev/sdb /dev/sdc

# Recreate (see README_NEW.md)
```

## 📊 Dashboard JSON

```bash
# Pretty print
curl -s localhost:8080 | jq .

# Get specific stat
curl -s localhost:8080 | jq '.stats.extensions_ok'

# Monitor continuously
watch -n 2 'curl -s localhost:8080 | jq .'
```

## ⚡ Performance Tuning

```bash
# Faster checks (edit lvm_config.h)
#define CHECK_INTERVAL 5     # Check every 5s

# More aggressive extension
#define THRESHOLD_PCT 70     # Extend at 70%

# Larger extensions
#define EXTEND_SIZE_GB 2     # Extend by 2GB
```

## 🛡️ Safety Checklist

- [ ] Test with `DRY_RUN = 1` first
- [ ] Verify only monitored mounts affected
- [ ] Backup important data
- [ ] Check ext4 vs XFS (XFS can't shrink)
- [ ] Ensure fallback device exists
- [ ] Monitor first runs carefully
- [ ] Test graceful shutdown

## 📞 Support

**Check logs first!** Most issues are clearly indicated in the color-coded output.

**Common issues:**
- Permission denied → Use `sudo`
- Port in use → Change `DASHBOARD_PORT`
- XFS not shrinking → Expected (XFS limitation)
- No donors found → Add more LVs or fallback PV

---

**Remember:** Always test with `DRY_RUN=1` before production! 🚀

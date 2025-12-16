# 🚀 SIMPLE START GUIDE - LVM Auto-Extender

**Complete beginner tutorial - follow these exact steps!**

---

## ⚡ STEP 1: Build the Program

Open your terminal on your Red Hat/CentOS machine and:

```bash
# Go to the project folder
cd /path/to/your/LVM/folder

# Build it (creates the executable)
make
```

**What you should see:**
```
Compiling lvm_main.c...
Compiling lvm_logger.c...
...
✓ Build complete: lvm_manager
```

**Result:** You now have an executable file called `lvm_manager`

---

## ⚡ STEP 2: Run It (Test Mode - Safe!)

The program starts in **TEST MODE** by default - it won't make any real changes.

```bash
# Run the program (needs sudo for LVM commands)
sudo ./lvm_manager
```

**What you should see:**
- A fancy colored banner
- Configuration showing "DRY-RUN (Simulation)"
- Log messages like:
  ```
  [OK   ] Supervisor   │ Thread started
  [INFO ] HTTP         │ Dashboard listening on http://0.0.0.0:8080
  ```

**The program is now running!** It will:
- Check your filesystems every 8 seconds
- Show you what it WOULD do (but won't actually do it)
- Keep running until you press **Ctrl+C**

---

## ⚡ STEP 3: Check the Dashboard (Optional)

While the program is running, open a NEW terminal window:

```bash
# Check status via HTTP
curl http://localhost:8080
```

**You'll see JSON data like:**
```json
{
  "status": "running",
  "dry_run": true,
  "stats": {...},
  "volumes": [...]
}
```

---

## ⚡ STEP 4: Stop the Program

Go back to the terminal where it's running and press:

```
Ctrl+C
```

**You'll see:**
```
[WARN ] Main │ Received signal SIGINT (Ctrl+C) - shutting down...
[OK   ] Main │ All threads stopped
[OK   ] Main │ Shutdown complete
```

---

## 🎯 WHAT IT DOES (Simple Explanation)

Think of it like an automatic disk space manager:

1. **Monitors** your disk space every 8 seconds
2. **Detects** when a disk is getting full (over 80%)
3. **Finds** other disks that have too much free space
4. **Takes** space from over-provisioned disks
5. **Gives** that space to the full disk

**Example scenario:**
- `/mnt/lv_home` is 85% full (HUNGRY!) 🔥
- `/mnt/lv_data1` is only 30% full (has extra space) 💤
- Program shrinks `lv_data1` by 1GB
- Program extends `lv_home` by 1GB
- `/mnt/lv_home` is now 75% full (happy!) ✓

---

## 📝 WHICH DISKS DOES IT WATCH?

By default, it only monitors these:
- `/mnt/lv_home`
- `/mnt/lv_data1`
- `/mnt/lv_data2`

**It ignores your system disks** (like `/`, `/boot`, `/home`) - so it's safe!

---

## 🔧 CHANGE SETTINGS (If You Want)

Open the config file:

```bash
nano lvm_config.h
```

**Important settings you can change:**

| Line | Setting | What it means |
|------|---------|---------------|
| `DRY_RUN = 1` | Test mode ON | Change to `0` for real operations |
| `THRESHOLD_PCT = 80` | Extend at 80% | When to add space |
| `LOW_PCT = 40` | Below 40% = donor | When disk has too much space |
| `DASHBOARD_PORT = 8080` | Web port | HTTP dashboard port |

**After changing settings:**
```bash
# Rebuild
make clean
make

# Run again
sudo ./lvm_manager
```

---

## 🎮 TRY IT FOR REAL (When Ready)

### ⚠️ WARNING: Only do this when you're ready for real changes!

**Step 1: Enable real mode**
```bash
# Edit config
nano lvm_config.h

# Find this line:
#define DRY_RUN                 1

# Change to:
#define DRY_RUN                 0

# Save and exit (Ctrl+X, then Y, then Enter)
```

**Step 2: Rebuild**
```bash
make clean
make
```

**Step 3: Run with real operations**
```bash
sudo ./lvm_manager
```

**Now it will actually shrink/extend volumes when needed!**

---

## 📊 WATCH IT WORK

### Test it by filling up a disk:

**Terminal 1:** (Run the manager)
```bash
sudo ./lvm_manager
```

**Terminal 2:** (Fill up a disk to trigger action)
```bash
# Check current usage
df -h /mnt/lv_home

# Fill it up (creates a 3GB file)
sudo dd if=/dev/zero of=/mnt/lv_home/testfile.bin bs=1M count=3000

# Check again
df -h /mnt/lv_home
```

**Watch Terminal 1** - you'll see:
```
[WARN ] Supervisor │ 🔥 HUNGRY LV: /mnt/lv_home (85%)
[INFO ] Extender   │ Processing extension...
[INFO ] Extender   │ Found donor: lv_data1
[OK   ] Extender   │ ✓ Extension complete
```

---

## 🛑 STOP IT

**Always stop gracefully:**
```
Press Ctrl+C
```

**Don't use** `kill -9` - let it shut down cleanly!

---

## ❓ COMMON QUESTIONS

### Q: Is it safe to test?
**A:** YES! When `DRY_RUN = 1` (default), it only logs what it would do. No real changes.

### Q: What if I want to monitor different disks?
**A:** Edit `lvm_config.h`, find the line:
```c
#define MONITORED_MOUNTS "/mnt/lv_home","/mnt/lv_data1","/mnt/lv_data2"
```
Change the paths to your disks, then rebuild with `make clean && make`.

### Q: Can it shrink XFS filesystems?
**A:** NO. XFS can only grow, never shrink. The program knows this and will skip XFS disks as donors. Only ext4 disks can be shrunk safely.

### Q: What if there's no space to give?
**A:** It will try to add a "fallback" disk (`/dev/sdc` by default) to the volume group.

### Q: How do I see what's happening?
**A:** Just watch the colored terminal output! It tells you everything.

### Q: Does it run automatically on boot?
**A:** Not yet. After testing, you can install it as a system service (advanced).

---

## 🆘 TROUBLESHOOTING

### Problem: "Permission denied"
**Solution:** Use `sudo`:
```bash
sudo ./lvm_manager
```

### Problem: "make: command not found"
**Solution:** Install build tools:
```bash
sudo dnf install -y gcc make
```

### Problem: "lvs: command not found"
**Solution:** Install LVM tools:
```bash
sudo dnf install -y lvm2
```

### Problem: No colors showing
**Solution:** Your terminal doesn't support colors. That's OK - it still works!

### Problem: Port 8080 already in use
**Solution:** Change the port:
1. Edit `lvm_config.h`
2. Find `#define DASHBOARD_PORT 8080`
3. Change `8080` to something else like `8090`
4. Rebuild: `make clean && make`

---

## ✅ QUICK CHECKLIST

- [ ] Built the program (`make`)
- [ ] Ran in test mode (`sudo ./lvm_manager`)
- [ ] Saw colored output with "DRY-RUN"
- [ ] Checked dashboard (`curl localhost:8080`)
- [ ] Stopped cleanly (Ctrl+C)
- [ ] Understand it only watches `/mnt/lv_*` disks
- [ ] Know how to enable real mode (set `DRY_RUN = 0`)

---

## 🎓 NEXT LEVEL (Optional)

Once you're comfortable:

1. **Install system-wide:**
   ```bash
   sudo make install
   # Now you can run: sudo lvm_manager (from anywhere)
   ```

2. **Run as a service:**
   ```bash
   # Use the deployment script
   chmod +x build_and_deploy.sh
   ./build_and_deploy.sh
   ```

3. **Monitor with systemd:**
   ```bash
   sudo systemctl start lvm-auto-extender
   sudo systemctl status lvm-auto-extender
   sudo journalctl -u lvm-auto-extender -f
   ```

---

## 🎉 THAT'S IT!

You now know how to:
- ✅ Build it
- ✅ Run it in safe test mode
- ✅ Check the dashboard
- ✅ Stop it
- ✅ Enable real operations when ready

**Start with test mode (`DRY_RUN = 1`) and just watch what it does. When you're comfortable, enable real mode!**

Need help? Check the logs - they tell you everything in color! 🌈

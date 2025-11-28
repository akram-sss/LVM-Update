# LVM Auto-Extender Lab
This lab simulates a system that automatically extends an LVM logical volume when it gets close to full, by shrinking other LVs in the same volume group and/or using extra disks. It also exposes a small HTTP dashboard.

## 1. Lab prerequisites
### Linux VM with:

- LVM tools (lvm2)

- gcc, make, curl

### Two extra block devices:

- /dev/sdb for the main VG

- /dev/sdc as a fallback PV (optional)

**Adjust device names in the code/config if your disks are different.**




## 2. Create PV, VG and LVs
### Wipe and initialize /dev/sdb as a PV:

```bash
sudo pvcreate /dev/sdb
```

### Create volume group:

```bash
sudo vgcreate vgdata /dev/sdb
```

### Create three logical volumes (example sizes):

```bash
sudo lvcreate -L 5G -n lv_home vgdata

sudo lvcreate -L 3G -n lv_data1 vgdata

sudo lvcreate -L 3G -n lv_data2 vgdata
```

### Create filesystems:

```bash
sudo mkfs.ext4 /dev/vgdata/lv_home

sudo mkfs.ext4 /dev/vgdata/lv_data1

sudo mkfs.ext4 /dev/vgdata/lv_data2
```

### Create mount points and mount:

```bash
sudo mkdir -p /mnt/lv_home /mnt/lv_data1 /mnt/lv_data2

sudo mount /dev/vgdata/lv_home /mnt/lv_home

sudo mount /dev/vgdata/lv_data1 /mnt/lv_data1

sudo mount /dev/vgdata/lv_data2 /mnt/lv_data2
```
### Check:
```bash
df -h /mnt/lv_home /mnt/lv_data1 /mnt/lv_data2
```




## 3. Configure the LVM Manager
### Open lvm_manager.c in nano:
```bash
makdir lvm_lab
cd ~/lvm_lab

nano lvm_manager.c
```

### Key settings at the top:

**DRY_RUN:**
```c
static int DRY_RUN = 1; // dry-run mode (no real LVM changes)

static int DRY_RUN = 0; // real changes (shrink/extend)
```
**Thresholds:**
```c
static const int THRESHOLD_PCT = 80; // LV becomes “HUNGRY” at or above this %

static const int LOW_PCT = 40; // considered over-provisioned if below this for a while
```
**Fallback disk:**
```c
static const char *FALLBACK_DEV = "/dev/sdc";
```

**Writer (load generator):**
```c
static const char *WRITER_BASE_PATH = "/mnt/lv_home";

static const int WRITER_FILE_SIZE_MB = 1;

static const int WRITER_SLEEP_USEC = 20000;
```
- Save and exit.





## 4. Build the program
### From ~/lvm_lab:
```bash
gcc -O2 -Wall lvm_manager.c -o lvm_manager -pthread
```
This should create the lvm_manager binary.




## 5. Run in DRY-RUN mode (safe test)
Ensure DRY_RUN = 1 in the code, rebuild, then run:
```bash
sudo ./lvm_manager
```
**You should see:**

LVM Manager starting (DRY_RUN=1) …

HTTP dashboard listening on 0.0.0.0:8080

**Check it’s running:**
```bash

ps aux | grep lvm_manager
```
**Watch log output:**

Writers simulate creating files under /mnt/lv_home/writer1 and writer2.

**Supervisor prints:**

OVER-PROVISIONED for LVs that stay below LOW_PCT for a while.

HUNGRY when an LV crosses THRESHOLD_PCT (but in DRY_RUN it only logs, no real changes).

**Test HTTP dashboard:**
```bash
curl http://127.0.0.1:8080
```
You get a JSON listing of devices, mounts, usage, and last message.

Stop the program with Ctrl+C.





## 6. Enable real shrinking/extending
- Set DRY_RUN = 0, keep THRESHOLD_PCT = 80 (or 95 if you prefer later action):

- static int DRY_RUN = 0;

**Rebuild:**
```bash
gcc -O2 -Wall lvm_manager.c -o lvm_manager -pthread
```
**Start again:**
```bash
sudo ./lvm_manager
```
**You should see:**

- LVM Manager starting (DRY_RUN=0). Threshold=80%

- HTTP dashboard listening on 0.0.0.0:8080





## 7. Trigger auto-extension of lv_home
1. Fill /mnt/lv_home until usage is above the threshold:
```bash
df -h /mnt/lv_home

sudo dd if=/dev/zero of=/mnt/lv_home/bigfile1.bin bs=1M count=5000

df -h /mnt/lv_home # repeat dd if needed until Use% >= threshold
```
2. When /mnt/lv_home crosses THRESHOLD_PCT:

- Supervisor logs:

Supervisor: HUNGRY LV /dev/mapper/vgdata-lv_home at /mnt/lv_home (xx%)

- Extender logs:

Extender: processing /dev/mapper/vgdata-lv_home

Extender: VG=vgdata LV=lv_home for device=/dev/mapper/vgdata-lv_home

3. Extender behavior:

- It loops over all LVs in vgdata:

If a sibling LV is ext4 and has at least 1 GiB free, it shrinks it by 1 GiB:
```bash
sudo lvreduce -r -L -1G /dev/vgdata/lv_data1 -y
```
- After donors, it checks VG free space.

If VG free space >= 1 GiB:
```bash
sudo lvextend -r -L +1G /dev/vgdata/lv_home
```
resize2fs runs online to grow the ext4 filesystem.

4. Check results:
```bash
df -h /mnt/lv_home /mnt/lv_data1 /mnt/lv_data2
```
- lv_home size increased.

- Donor LVs (e.g. lv_data1) may be smaller.
```bash
sudo lvs -a -o +devices | grep vgdata
```
- Shows updated LV sizes.

5. Writers keep filling /mnt/lv_home, so the cycle can repeat:

- Each time lv_home becomes HUNGRY again, it is extended further while there is space.


## 8. Reset the lab to a clean state
If you want to reset everything back to the “fresh” state:

Stop the program:
```bash
sudo pkill lvm_manager

ps aux | grep lvm_manager # confirm only grep remains
```
Clean test files:
```bash
sudo rm -rf /mnt/lv_home/writer*

sudo rm -f /mnt/lv_home/bigfile*.bin

df -h /mnt/lv_home
```
Reset LVs and VG (optional full reset):
```bash
sudo umount /mnt/lv_home /mnt/lv_data1 /mnt/lv_data2 2>/dev/null

sudo lvremove -f /dev/vgdata/lv_home /dev/vgdata/lv_data1 /dev/vgdata/lv_data2

sudo vgremove -f vgdata

sudo pvremove -f /dev/sdb /dev/sdc 2>/dev/null
```
Then repeat section 2 to recreate PV/VG/LVs and filesystems.
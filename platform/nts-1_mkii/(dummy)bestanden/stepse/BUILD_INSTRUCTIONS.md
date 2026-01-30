# 🎹 STEPSEQ - BUILD INSTRUCTIONS

## 📋 **REQUIREMENTS**

### **Software:**
- Korg logue SDK (latest version)
- ARM GCC toolchain (gcc-arm-none-eabi-10.3-2021.10)
- GNU Make
- Git (for SDK)

### **Hardware:**
- Korg NTS-1 digital kit mkII
- USB-C cable
- KORG KONTROL Editor software

---

## 🚀 **QUICK START (RECOMMENDED)**

### **Option 1: Use Pre-built Unit** (Easiest!)

If someone has already compiled this, you just need:

1. Download `stepseq.nts1mkiiunit` file
2. Open KORG KONTROL Editor
3. Connect your NTS-1 mkII via USB
4. Drag & drop the `.nts1mkiiunit` file into a MOD FX slot
5. Click "Send" to upload to NTS-1 mkII
6. Done! Select STEPSEQ as modulation effect

---

## 🛠️ **OPTION 2: BUILD FROM SOURCE**

### **Step 1: Install Korg logue SDK**

```bash
# Clone SDK
git clone https://github.com/korginc/logue-sdk.git
cd logue-sdk
git submodule update --init

# Install toolchain (see SDK docs for your OS)
# macOS/Linux: Follow instructions in tools/gcc/
# Windows: Use MSYS2 or WSL
```

### **Step 2: Copy STEPSEQ Files**

```bash
# Copy this entire directory to SDK
cp -r /path/to/stepseq_build/ logue-sdk/platform/nts-1_mkii/stepseq/

# Verify files
cd logue-sdk/platform/nts-1_mkii/stepseq/
ls -l
# Should see: header.c, unit.cc, manifest.json, config.mk, Makefile
```

### **Step 3: Build**

```bash
cd logue-sdk/platform/nts-1_mkii/stepseq/
make clean
make

# If successful, you'll see:
# - stepseq.nts1mkiiunit (the final file!)
```

### **Step 4: Upload to NTS-1 mkII**

1. Open KORG KONTROL Editor
2. Connect NTS-1 mkII via USB
3. Drag `stepseq.nts1mkiiunit` into MOD FX slot
4. Click "Send"
5. Done!

---

## 🐛 **TROUBLESHOOTING**

### **"make: *** No rule to make target 'platform.mk'"**

**Cause:** Files not in correct SDK directory structure

**Fix:**
```bash
# Make sure you're in the right place:
cd logue-sdk/platform/nts-1_mkii/stepseq/

# Check parent directory has common/ and ld/
ls ../../common/
ls ../../ld/
```

---

### **"arm-none-eabi-gcc: command not found"**

**Cause:** ARM toolchain not installed or not in PATH

**Fix:**

**macOS:**
```bash
brew install --cask gcc-arm-embedded
```

**Linux:**
```bash
# Download from ARM website or:
sudo apt-get install gcc-arm-none-eabi
```

**Windows (MSYS2):**
```bash
pacman -S mingw-w64-arm-none-eabi-gcc
```

---

### **"undefined reference to `fx_sinf`"**

**Cause:** Missing SDK includes

**Fix:** Make sure you're building from within SDK directory structure

---

### **Build succeeds but unit doesn't work on NTS-1**

**Check:**
1. Firmware version >= 1.0.0
2. Used correct module type (modfx)
3. File is actually `.nts1mkiiunit` extension
4. KORG KONTROL Editor shows successful upload

---

## 📂 **FILE CHECKLIST**

Before building, verify you have all files:

```
stepseq/
├── header.c          ✅ (Parameter definitions)
├── unit.cc           ✅ (DSP implementation)
├── manifest.json     ✅ (Metadata)
├── config.mk         ✅ (Build configuration)
├── Makefile          ✅ (Build script)
└── BUILD_INSTRUCTIONS.md (this file)
```

---

## 🔧 **ADVANCED: MODIFY THE CODE**

### **Change Parameters:**

Edit `header.c`:
```c
// Example: Change default pattern to P3 instead of P1
{0, 7, 0, 2, k_unit_param_type_enum, 0, 0, 0, {"PATTERN"}},
//          ^ change this number (0-7)
```

### **Change Default Patterns:**

Edit `unit.cc`, around line 267:
```cpp
// Pattern 0: Chromatic scale up
for (int s = 0; s < NUM_STEPS; s++) {
    s_patterns[0].steps[s].pitch_offset = s - 7;  // Modify this!
}
```

### **Add More Patterns:**

1. Change `NUM_PATTERNS` in `unit.cc` (line 25)
2. Update header.c parameter max value
3. Add pattern initialization in `unit_init()`

---

## 📊 **BUILD OUTPUT DETAILS**

Successful build produces:

**File:** `stepseq.nts1mkiiunit`
- **Type:** ELF 32-bit LSB shared object
- **Architecture:** ARM EABI5
- **Size:** ~16KB (modfx has 16KB limit)
- **Platform:** NTS-1 mkII only (not compatible with mk1!)

---

## 🎓 **DEVELOPMENT TIPS**

### **Testing Changes:**

```bash
# After modifying code:
make clean    # Always clean first!
make          # Build
# Upload to NTS-1 mkII
# Test on hardware
```

### **Debug Output:**

Add debug prints (limited in embedded environment):
```cpp
// Note: Printf won't work on NTS-1!
// Use parameter values to debug:
// - Set a parameter to a calculated value
// - Read it on hardware to see the value
```

### **Memory Constraints:**

- **Code size:** Max 16KB
- **SDRAM:** 256KB available
- **Stack:** Limited (avoid deep recursion)

---

## 📚 **SDK RESOURCES**

### **Official Documentation:**
- https://github.com/korginc/logue-sdk
- https://korginc.github.io/logue-sdk/

### **Community:**
- Korg forums
- logue SDK GitHub issues
- NTS-1 user groups

### **Example Units:**
```bash
# Check SDK examples:
cd logue-sdk/platform/nts-1_mkii/
ls -l
# dummy-modfx/  <- Simple example
# waves/        <- More complex example
```

---

## ✅ **VERIFICATION CHECKLIST**

Before considering build complete:

- [ ] `make clean` completes without errors
- [ ] `make` completes without errors  
- [ ] `.nts1mkiiunit` file created
- [ ] File size < 16KB (check with `ls -lh`)
- [ ] KORG KONTROL Editor accepts file
- [ ] Upload to NTS-1 succeeds
- [ ] Unit appears in MOD FX menu
- [ ] Unit produces sound when selected
- [ ] All parameters accessible
- [ ] No crashes or glitches

---

## 🎉 **SUCCESS!**

If you got here and everything works:

**CONGRATULATIONS!** 🎊

You now have a **fully programmable 16-step sequencer** on your NTS-1 mkII!

Read `STEPSEQ_MANUAL.md` for usage instructions.

---

## 🆘 **STILL HAVING ISSUES?**

### **Check these common mistakes:**

1. **Wrong directory structure**
   - Must be in `logue-sdk/platform/nts-1_mkii/stepseq/`

2. **Missing SDK files**
   - Need complete SDK, not just these files

3. **Wrong toolchain version**
   - Use gcc-arm-none-eabi-10.3-2021.10

4. **Old firmware**
   - NTS-1 mkII firmware must be >= 1.0.0

5. **Wrong module type**
   - This is MODFX, not OSC/DELFX/REVFX

---

## 🔗 **USEFUL LINKS**

- **SDK:** https://github.com/korginc/logue-sdk
- **Toolchain:** https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain
- **KORG:** https://www.korg.com/us/products/synthesizers/nts_1_mk2/

---

**Made with ❤️ for the NTS-1 mkII community**
**Based on Korg logue SDK**

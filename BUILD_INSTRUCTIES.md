# 🔧 BUILD INSTRUCTIES - NTS-1 mkII Units

## 📋 VOORWAARDEN
- Windows met WSL (Windows Subsystem for Linux) geïnstalleerd
- Docker Desktop draait
- logue-sdk repository in: `C:\Users\sande_ej\logue-sdk`

---

## 🚀 STAPPENPLAN - Unit Builden

### **STAP 1: Verwijder oude build directory (PowerShell)**

```powershell
# Voor één unit:
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\UNIT_NAAM\build" -Recurse -Force -ErrorAction SilentlyContinue

# Voorbeeld (chicago_bells):
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\chicago_bells\build" -Recurse -Force -ErrorAction SilentlyContinue
```

**OF via WSL:**
```bash
wsl bash -c "rm -rf /mnt/c/Users/sande_ej/logue-sdk/platform/nts-1_mkii/UNIT_NAAM/build"
```

---

### **STAP 2: Build de unit (WSL)**

**Optie A: Via build script (aanbevolen)**
```bash
cd C:\Users\sande_ej\logue-sdk
wsl bash build_UNIT_NAAM.sh
```

**Voorbeelden:**
```bash
# Chicago Bells
cd C:\Users\sande_ej\logue-sdk
wsl bash build_chicago_bells.sh

# Cathedral Rev
cd C:\Users\sande_ej\logue-sdk
wsl bash build_cathedral_rev.sh

# Cathedral Smooth
cd C:\Users\sande_ej\logue-sdk
wsl bash build_cathedral_smooth.sh

# Melancholic Circuit
cd C:\Users\sande_ej\logue-sdk
wsl bash build_melancholic_circuit.sh
```

**Optie B: Direct via Docker**
```bash
cd C:\Users\sande_ej\logue-sdk
wsl bash -c "docker run --rm -v /mnt/c/Users/sande_ej/logue-sdk:/workspace -w /workspace -h logue-sdk xiashj/logue-sdk:latest /app/cmd_entry build platform/nts-1_mkii/UNIT_NAAM"
```

---

### **STAP 3: Verifieer het bestand (PowerShell)**

```powershell
Get-ChildItem -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\UNIT_NAAM\*.nts1mkiiunit" | Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize
```

**Voorbeeld:**
```powershell
Get-ChildItem -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\chicago_bells\*.nts1mkiiunit" | Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize
```

**Verwachte output:**
```
Name                       Length LastWriteTime      
----                       ------ -------------
chicago_bells.nts1mkiiunit   8672 27-12-2025 19:43:54
```

---

## 📝 COMPLETE VOORBEELDEN

### **Voorbeeld 1: Chicago Bells rebuilden**

**PowerShell:**
```powershell
# Stap 1: Verwijder build directory
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\chicago_bells\build" -Recurse -Force -ErrorAction SilentlyContinue

# Stap 2: Build
cd C:\Users\sande_ej\logue-sdk
wsl bash build_chicago_bells.sh

# Stap 3: Verifieer
Get-ChildItem -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\chicago_bells\*.nts1mkiiunit" | Select-Object Name, Length, LastWriteTime
```

---

### **Voorbeeld 2: Cathedral Rev rebuilden**

**PowerShell:**
```powershell
# Stap 1: Verwijder build directory
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\cathedral_rev\build" -Recurse -Force -ErrorAction SilentlyContinue

# Stap 2: Build
cd C:\Users\sande_ej\logue-sdk
wsl bash build_cathedral_rev.sh

# Stap 3: Verifieer
Get-ChildItem -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\cathedral_rev\*.nts1mkiiunit" | Select-Object Name, Length, LastWriteTime
```

---

### **Voorbeeld 3: Cathedral Smooth rebuilden**

**PowerShell:**
```powershell
# Stap 1: Verwijder build directory
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\cathedral_smooth\build" -Recurse -Force -ErrorAction SilentlyContinue

# Stap 2: Build
cd C:\Users\sande_ej\logue-sdk
wsl bash build_cathedral_smooth.sh

# Stap 3: Verifieer
Get-ChildItem -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\cathedral_smooth\*.nts1mkiiunit" | Select-Object Name, Length, LastWriteTime
```

---

### **Voorbeeld 4: Melancholic Circuit rebuilden**

**PowerShell:**
```powershell
# Stap 1: Verwijder build directory
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\melancholic_circuit\build" -Recurse -Force -ErrorAction SilentlyContinue

# Stap 2: Build
cd C:\Users\sande_ej\logue-sdk
wsl bash build_melancholic_circuit.sh

# Stap 3: Verifieer
Get-ChildItem -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\melancholic_circuit\*.nts1mkiiunit" | Select-Object Name, Length, LastWriteTime
```

---

## 🔍 TROUBLESHOOTING

### **Probleem: Permission denied errors**

**Oplossing:**
```powershell
# Verwijder build directory vanuit PowerShell (niet vanuit WSL)
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\UNIT_NAAM\build" -Recurse -Force
```

---

### **Probleem: Build script bestaat niet**

**Oplossing:**
Maak een build script aan in `C:\Users\sande_ej\logue-sdk\build_UNIT_NAAM.sh`:

```bash
#!/bin/bash
# Build UNIT_NAAM unit

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
UNIT="UNIT_NAAM"

echo "Building $UNIT..."

UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT"

docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    -w /workspace \
    -h logue-sdk \
    xiashj/logue-sdk:latest \
    /app/cmd_entry build "platform/nts-1_mkii/$UNIT" > /tmp/build_${UNIT}.log 2>&1

BUILD_EXIT=$?

if [ -f "$UNIT_PATH/${UNIT}.nts1mkiiunit" ]; then
    echo "  ✓ SUCCESS - File created: ${UNIT}.nts1mkiiunit"
    ls -lh "$UNIT_PATH/${UNIT}.nts1mkiiunit"
    SIZE=$(stat -f%z "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null || stat -c%s "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null || echo "unknown")
    echo "  File size: $SIZE bytes (~$((SIZE/1024))KB)"
    if [ "$SIZE" != "unknown" ] && [ "$SIZE" -gt 49152 ]; then
        echo "  ⚠ WARNING: File size exceeds 48KB limit!"
    fi
    exit 0
else
    echo "  ✗ FAILED - File not created (exit code: $BUILD_EXIT)"
    echo "  Last 30 lines of build log:"
    tail -30 /tmp/build_${UNIT}.log 2>/dev/null || echo "  (log not available)"
    exit 1
fi
```

---

### **Probleem: Bestand wordt niet geüpdatet**

**Oplossing:**
1. Verwijder eerst de build directory
2. Verwijder het oude .nts1mkiiunit bestand (optioneel)
3. Build opnieuw

```powershell
# Verwijder alles
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\UNIT_NAAM\build" -Recurse -Force
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\UNIT_NAAM\*.nts1mkiiunit" -Force

# Build opnieuw
cd C:\Users\sande_ej\logue-sdk
wsl bash build_UNIT_NAAM.sh
```

---

## ✅ CHECKLIST

Voor elke rebuild:
- [ ] Build directory verwijderd
- [ ] Build script uitgevoerd
- [ ] .nts1mkiiunit bestand gecontroleerd (timestamp en grootte)
- [ ] Geen errors in build output

---

## 📌 BELANGRIJKE NOTITIES

1. **ALTIJD eerst de build directory verwijderen** voordat je rebuild
2. **Controleer altijd de timestamp** van het .nts1mkiiunit bestand na build
3. **Gebruik PowerShell** voor file operations (betere permissions)
4. **Gebruik WSL** voor Docker builds
5. **Docker Desktop moet draaien** voordat je build

---

## 🎯 SNEL REFERENTIE

**Eén commando voor alles (PowerShell):**
```powershell
$unit = "chicago_bells"  # Verander naar gewenste unit
Remove-Item -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\$unit\build" -Recurse -Force -ErrorAction SilentlyContinue
cd C:\Users\sande_ej\logue-sdk
wsl bash "build_$unit.sh"
Get-ChildItem -Path "C:\Users\sande_ej\logue-sdk\platform\nts-1_mkii\$unit\*.nts1mkiiunit" | Select-Object Name, Length, LastWriteTime
```


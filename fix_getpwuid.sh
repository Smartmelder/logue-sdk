#!/bin/bash
# WSL ROOT-ONLY Repair Script - Voor getpwuid(1000) failed 17 als ROOT
# Run: chmod +x fix_getpwuid.sh && ./fix_getpwuid.sh
# Reset: ./fix_getpwuid.sh --reset

BACKUP_DIR="/root/wsl_backup_$(date +%Y%m%d_%H%M%S)"
RESET=false

if [[ "$1" == "--reset" ]]; then
    RESET=true
    echo "=== ROOT RESET MODE ==="
fi

echo "=== WSL getpwuid(1000) ROOT FIX ==="
echo "Backup: $BACKUP_DIR"
mkdir -p "$BACKUP_DIR"

# Backup FIRST
cp /etc/passwd* "$BACKUP_DIR/" 2>/dev/null || true
cp /etc/shadow* "$BACKUP_DIR/" 2>/dev/null || true
cp /etc/group* "$BACKUP_DIR/" 2>/dev/null || true

if [ "$RESET" = true ]; then
    echo "=== RESET backups ==="
    cp "$BACKUP_DIR/passwd"* /etc/ 2>/dev/null || true
    cp "$BACKUP_DIR/shadow"* /etc/ 2>/dev/null || true
    cp "$BACKUP_DIR/group"* /etc/ 2>/dev/null || true
    echo "Reset OK. wsl --shutdown && wsl -u root"
    exit 0
fi

echo "=== 1. FORCE PWCK REPAIR ==="
pwck -q || echo "pwck warning (expected)"
pwconv || echo "pwconv warning"
grpconv || echo "grpconv warning"

echo "=== 2. ROOT-ONLY UID 1000 FORCE CLEAN ==="
# Verwijder ALLE traces van UID 1000
sed -i '/:1000:/d' /etc/passwd /etc/shadow /etc/group 2>/dev/null || true

echo "=== 3. NIEUWE SANDE_EJ (UID 1000) ==="
# Verwijder oude user eerst (als bestaat)
userdel -f sande_ej 2>/dev/null || true
# Maak nieuwe user met UID 1000
useradd -m -u 1000 -G sudo,adm -s /bin/bash -c "Sande EJ" sande_ej
echo "sande_ej:sande_ej" | chpasswd
usermod -aG sudo sande_ej

echo "=== 4. HOME PERMISSIONS FORCE ==="
chown -R 1000:1000 /home/sande_ej 2>/dev/null || true
chmod 755 /home/sande_ej 2>/dev/null || true
chmod 700 /home/sande_ej/.??* 2>/dev/null || true

echo "=== 5. DNS (voor later) ==="
cat > /etc/wsl.conf << 'EOF'
[network]
generateResolvConf = false
EOF

echo "=== 6. VALIDATE ==="
echo "UID check:"
id sande_ej || echo "id command failed"
getent passwd 1000 || echo "getent failed"
pwck -q || echo "pwck warning"

echo "=== 7. ROOT TEST LOGIN ==="
su - sande_ej -c "echo 'LOGIN OK'; whoami; id" || echo "su test failed (may be OK)"

echo "=== KLAAR! ==="
echo "1. SAVED: $BACKUP_DIR"
echo "2. Wachtwoord sande_ej: 'sande_ej' (wijzig met passwd)"
echo "3. UITVOEREN: exit && PowerShell: wsl"
echo "4. Reset: $0 --reset"
echo "5. Test: ping google.com"


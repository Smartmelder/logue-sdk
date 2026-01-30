#!/bin/bash
# Fix apt repository issues (VPN routing problems)

echo "=== Fixing apt repositories ==="

# Backup
cp /etc/apt/sources.list /etc/apt/sources.list.backup.$(date +%Y%m%d)

# Use main archive only (more reliable)
cat > /etc/apt/sources.list << 'EOF'
deb http://archive.ubuntu.com/ubuntu/ noble main restricted universe multiverse
deb http://archive.ubuntu.com/ubuntu/ noble-updates main restricted universe multiverse
deb http://archive.ubuntu.com/ubuntu/ noble-security main restricted universe multiverse
EOF

echo "=== Updated sources.list ==="
echo "=== Running apt update ==="

apt update

echo "=== Done ==="
echo "If still errors, try: sudo apt update --fix-missing"


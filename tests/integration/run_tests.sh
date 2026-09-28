#!/usr/bin/env bash
set -euo pipefail

# =============================================================================
# prelogin-statusd Integration Tests
# Runs without systemd by launching a temporary D-Bus bus.
# Includes Tailscale mock server for "connected" state testing.
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
DAEMON_SRC="${PROJECT_ROOT}/prelogin-statusd"
DAEMON_BUILD="${DAEMON_SRC}/build"
DAEMON_BIN="${DAEMON_BUILD}/prelogin-statusd"

TEST_DIR="/tmp/prelogin-statusd-test-$$"
DBUS_SOCK="${TEST_DIR}/dbus.sock"
TAILSCALE_SOCK="${TEST_DIR}/tailscaled.sock"
TEST_CONFIG="${TEST_DIR}/test-config.ini"
DBUS_PID=""
DAEMON_PID=""
MOCK_PID=""

PASSED=0
FAILED=0
TOTAL=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

info() {
    echo -e "${YELLOW}[INFO]${NC} $*"
}

pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASSED++)) || true
    ((TOTAL++)) || true
}

fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((FAILED++)) || true
    ((TOTAL++)) || true
}

cleanup() {
    info "Cleaning up..."
    if [[ -n "${MOCK_PID:-}" ]] && kill -0 "${MOCK_PID}" 2>/dev/null; then
        kill "${MOCK_PID}" 2>/dev/null || true
        wait "${MOCK_PID}" 2>/dev/null || true
    fi
    if [[ -n "${DAEMON_PID:-}" ]] && kill -0 "${DAEMON_PID}" 2>/dev/null; then
        kill "${DAEMON_PID}" 2>/dev/null || true
        wait "${DAEMON_PID}" 2>/dev/null || true
    fi
    if [[ -n "${DBUS_PID:-}" ]] && kill -0 "${DBUS_PID}" 2>/dev/null; then
        kill "${DBUS_PID}" 2>/dev/null || true
        wait "${DBUS_PID}" 2>/dev/null || true
    fi
    rm -rf "${TEST_DIR}"
}

trap cleanup EXIT

# ---------------------------------------------------------------------------
# 1. Compile daemon if needed
# ---------------------------------------------------------------------------

if [[ ! -x "${DAEMON_BIN}" ]]; then
    info "Daemon not built. Compiling..."
    mkdir -p "${DAEMON_BUILD}"
    cd "${DAEMON_BUILD}"
    cmake .. >/dev/null 2>&1 || { echo "CMake failed"; exit 2; }
    make -j"$(nproc)" >/dev/null 2>&1 || { echo "Build failed"; exit 2; }
    cd "${SCRIPT_DIR}"
fi

# ---------------------------------------------------------------------------
# 2. Prepare test environment
# ---------------------------------------------------------------------------

mkdir -p "${TEST_DIR}"

# Write dynamic test config with correct socket path
cat > "${TEST_CONFIG}" <<EOF
[General]
UpdateIntervalSeconds=60
ExposeErrors=false

[Tailscale]
Socket=${TAILSCALE_SOCK}

[Directory]
Type=none
Endpoint=
TimeoutMilliseconds=2000
EOF

cat > "${TEST_DIR}/dbus-system.conf" <<EOF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <listen>unix:path=${DBUS_SOCK}</listen>
  <policy context="default">
    <allow own="*"/>
    <allow send_destination="*"/>
    <allow receive_sender="*"/>
    <allow send_type="method_call"/>
  </policy>
</busconfig>
EOF

# ---------------------------------------------------------------------------
# 3. Start temporary D-Bus system bus
# ---------------------------------------------------------------------------

info "Starting temporary D-Bus system bus on ${DBUS_SOCK}..."
dbus-daemon --config-file="${TEST_DIR}/dbus-system.conf" --fork --print-address 2>/dev/null > "${TEST_DIR}/dbus.address"
sleep 0.5

if [[ ! -S "${DBUS_SOCK}" ]]; then
    echo "Failed to start D-Bus daemon"
    exit 2
fi

export DBUS_SYSTEM_BUS_ADDRESS="unix:path=${DBUS_SOCK}"
info "D-Bus address: ${DBUS_SYSTEM_BUS_ADDRESS}"

# ---------------------------------------------------------------------------
# 4. Start daemon WITHOUT mock socket (tailscale unavailable)
# ---------------------------------------------------------------------------

info "Starting prelogin-statusd (no Tailscale socket)..."
"${DAEMON_BIN}" --config "${TEST_CONFIG}" > "${TEST_DIR}/daemon.log" 2>&1 &
DAEMON_PID=$!

# Wait for registration
for _ in {1..50}; do
    if dbus-send --system --dest=org.prelogin.Status1 --type=method_call \
        --print-reply /org/prelogin/Status1 org.freedesktop.DBus.Properties.Get \
        string:org.prelogin.Status1 string:Version >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

if ! kill -0 "${DAEMON_PID}" 2>/dev/null; then
    echo "Daemon exited unexpectedly:"
    cat "${TEST_DIR}/daemon.log"
    exit 2
fi

info "Daemon registered. Running phase 1 tests (Tailscale unavailable)..."
echo ""

# ---------------------------------------------------------------------------
# 5. Test helpers
# ---------------------------------------------------------------------------

query_prop() {
    dbus-send --system --dest=org.prelogin.Status1 --type=method_call \
        --print-reply /org/prelogin/Status1 org.freedesktop.DBus.Properties.Get \
        string:org.prelogin.Status1 "string:$1" 2>/dev/null
}

call_method() {
    dbus-send --system --dest=org.prelogin.Status1 --type=method_call \
        --print-reply /org/prelogin/Status1 org.prelogin.Status1."$1" 2>/dev/null
}

# ---------------------------------------------------------------------------
# 6. Phase 1: Tailscale UNAVAILABLE tests
# ---------------------------------------------------------------------------

# TEST 1: D-Bus Registration
if query_prop Version | grep -q 'string "1.0.0"'; then
    pass "D-Bus registration and property Version"
else
    fail "D-Bus registration and property Version"
fi

# TEST 2: TailscaleState (should be unavailable because socket does not exist)
if query_prop TailscaleState | grep -qE 'string "(unavailable|unknown)"'; then
    pass "Property TailscaleState is unavailable/unknown when socket missing"
else
    fail "Property TailscaleState is unavailable/unknown when socket missing"
fi

# TEST 3: DirectoryServiceState (type=none => unknown)
if query_prop DirectoryServiceState | grep -q 'string "unknown"'; then
    pass "Property DirectoryServiceState is unknown when type=none"
else
    fail "Property DirectoryServiceState is unknown when type=none"
fi

# TEST 4: DirectoryServiceType
if query_prop DirectoryServiceType | grep -q 'string "none"'; then
    pass "Property DirectoryServiceType is none when unconfigured"
else
    fail "Property DirectoryServiceType is none when unconfigured"
fi

# TEST 5: TailscalePeerCount should be 0
if query_prop TailscalePeerCount | grep -q 'uint32 0'; then
    pass "Property TailscalePeerCount is 0 when unavailable"
else
    fail "Property TailscalePeerCount is 0 when unavailable"
fi

# TEST 6: GetCapabilities
if call_method GetCapabilities | grep -q 'string "Tailscale"'; then
    pass "Method GetCapabilities returns Tailscale"
else
    fail "Method GetCapabilities returns Tailscale"
fi

# TEST 7: Refresh method (should not crash / return void)
if call_method Refresh >/dev/null 2>&1; then
    pass "Method Refresh executes without error"
else
    fail "Method Refresh executes without error"
fi

# TEST 8: Config parsing — verify UpdateIntervalSeconds=60 was read
if grep -q "Normal updates every 60 seconds" "${TEST_DIR}/daemon.log"; then
    pass "Config parsing: UpdateIntervalSeconds=60 correctly loaded"
else
    fail "Config parsing: UpdateIntervalSeconds=60 correctly loaded"
fi

# ---------------------------------------------------------------------------
# 7. Phase 2: Start Tailscale MOCK and test CONNECTED state
# ---------------------------------------------------------------------------

echo ""
info "Starting Tailscale mock server on ${TAILSCALE_SOCK}..."
python3 "${SCRIPT_DIR}/fixtures/mock_tailscale.py" "${TAILSCALE_SOCK}" > "${TEST_DIR}/mock.log" 2>&1 &
MOCK_PID=$!

# Wait for socket creation
for _ in {1..30}; do
    if [[ -S "${TAILSCALE_SOCK}" ]]; then
        break
    fi
    sleep 0.1
done

if ! [[ -S "${TAILSCALE_SOCK}" ]]; then
    echo "Mock server did not create socket"
    exit 2
fi

info "Mock running. Triggering daemon Refresh..."
call_method Refresh >/dev/null 2>&1

# Give daemon time to process (synchronous refresh, but small delay for D-Bus signal)
sleep 0.5

info "Running phase 2 tests (Tailscale connected via mock)..."
echo ""

# TEST 9: TailscaleState should now be "connected"
if query_prop TailscaleState | grep -q 'string "connected"'; then
    pass "Property TailscaleState changes to connected when mock socket available"
else
    fail "Property TailscaleState changes to connected when mock socket available"
fi

# TEST 10: TailscalePeerCount should be 2 (mock has 2 peers)
if query_prop TailscalePeerCount | grep -q 'uint32 2'; then
    pass "Property TailscalePeerCount reflects 2 mock peers"
else
    fail "Property TailscalePeerCount reflects 2 mock peers"
fi

# TEST 11: TailscaleExitNodeActive should be true (one peer is exit node)
if query_prop TailscaleExitNodeActive | grep -q 'boolean true'; then
    pass "Property TailscaleExitNodeActive is true (mock peer is exit node)"
else
    fail "Property TailscaleExitNodeActive is true (mock peer is exit node)"
fi

# TEST 12: Retry mode should have stopped (all services OK now)
if grep -q "Stopping retry mode" "${TEST_DIR}/daemon.log"; then
    pass "Retry mode stops when all services become reachable"
else
    fail "Retry mode stops when all services become reachable"
fi

# ---------------------------------------------------------------------------
# 8. Results
# ---------------------------------------------------------------------------

echo ""
echo "============================================"
printf "Results: %d/%d passed (%d%%)\n" "${PASSED}" "${TOTAL}" "$(( PASSED * 100 / TOTAL ))"
echo "============================================"

if [[ ${FAILED} -gt 0 ]]; then
    echo ""
    info "Daemon log excerpt:"
    tail -n 20 "${TEST_DIR}/daemon.log"
    exit 1
fi

exit 0

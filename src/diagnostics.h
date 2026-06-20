#pragma once

// Boot-time self-check. Reads every peripheral once and prints a PASS / WARN /
// FAIL report to Serial so whoever powers the assembled unit gets immediate
// feedback on wiring before the tracker starts moving. Runs in both the real
// firmware (esp32dev) and the native simulation (simulate).
//
// Returns true if no FAIL-level problems were found.
bool runSelfCheck();

#ifndef ALTIVEC_DETECT_H
#define ALTIVEC_DETECT_H

// Detects whether the current CPU supports AltiVec / VMX vector unit.
// Safe on all PowerPC systems (X1000, X5000, Sam440, Sam460, Pegasos II, etc.).
bool cpu_has_altivec();

// Logs CPU vector capabilities to startup log and standard log
void log_cpu_features();

#endif // ALTIVEC_DETECT_H

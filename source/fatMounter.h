#include <stdbool.h>

bool FATMount();
void FATUnmount();
void FATSelectDefault();
const char* GetActiveDeviceName();

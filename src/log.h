#pragma once

#define HTL_LOGE(...) printf("[ERROR]   In File '%s' at Line %d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n")
#define HTL_LOGW(...) printf("[WARNING] In File '%s' at Line %d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n")
#define HTL_LOGD(...) printf("[DEBUG]   In File '%s' at Line %d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n")

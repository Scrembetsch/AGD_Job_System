#pragma once

#define HTL_LOGE(...) printf("[ERROR]   "); printf("In File '%s' at Line %d: " __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n")
#define HTL_LOGW(...) printf("[WARNING] "); printf("In File '%s' at Line %d: " __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n")
#define HTL_LOGD(...) printf("[DEBUG]   "); printf("In File '%s' at Line %d: " __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n")

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "curl/curl.h"

void download_init();
void download_cleanup();

void download_updates(const char* product);
bool download_file(char* url, char* filepath);

#endif  // DOWNLOAD_H
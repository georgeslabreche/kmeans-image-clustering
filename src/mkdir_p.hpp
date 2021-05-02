/* recursive mkdir based on
http://nion.modprobe.de/blog/archives/357-Recursive-directory-creation.html
*/

#ifndef MKDIR_P_H
#define MKDIR_P_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#define PATH_MAX_STRING_SIZE 256

int mkdir_p(const char *dir, const mode_t mode);

#endif
#ifndef _RELVIEW_VERSION_H
#  define _RELVIEW_VERSION_H

#define RELVIEW_MAJOR_VERSION 8
#define RELVIEW_MINOR_VERSION 2
#define RELVIEW_MICRO_VERSION 0

#define RELVIEW_CHECK_VERSION(major,minor,mirco)                                \
(((major) < RELVIEW_MAJOR_VERSION)                                              \
 || ((major) == RELVIEW_MAJOR_VERSION && (minor) < RELVIEW_MINOR_VERSION)       \
 || ((major) == RELVIEW_MAJOR_VERSION && (minor) == RELVIEW_MINOR_VERSION       \
     && (micro) >= RELVIEW_MICRO_VERSION))

int relview_check_version (int major, int minor, int micro);

#endif /* _RELVIEW_VERSION_H? */

#include "version.h"

/*! Check whether a given RelView version number is not less than the
 * version number of the current RelView system.
 *
 * \return Returns 1 (true), if the current version is newer than the
 *         systems version number given, 0 (false) otherwise.
 */
int relview_check_version (int major, int minor, int micro)
{
  return RELVIEW_CHECK_VERSION (major, minor, micro);
}

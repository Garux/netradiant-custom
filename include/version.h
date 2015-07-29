#ifndef INCLUDED_RADIANT_VERSION_H
#define INCLUDED_RADIANT_VERSION_H

#include <string>
namespace radiant {
	std::string version();
	int version_major();
	int version_minor();
	std::string about_msg();
} // namespace radiant

namespace q3map {
std::string stream_version();
} // namespace q3map

#endif // INCLUDED_RADIANT_VERSION_H

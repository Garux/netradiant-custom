#ifndef EXPORT_H
#define EXPORT_H
#include <set>
#include <string>

enum collapsemode
{
	COLLAPSE_ALL,
	COLLAPSE_BY_MATERIAL,
	COLLAPSE_NONE
};

typedef std::set<std::string, bool (*)( const std::string&, const std::string& )> StringSetWithLambda;

bool ExportSelection( const StringSetWithLambda& ignorelist, collapsemode m, bool exmat, const std::string& path, bool limitMatNames, bool objects, bool weld );

#endif

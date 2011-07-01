#if !defined(INCLUDED_FILEMATCH_H)
#define INCLUDED_FILEMATCH_H

int matchpattern(const char *in, const char *pattern, int caseinsensitive);
int matchpattern_with_separator(const char *in, const char *pattern, int caseinsensitive, const char *separators, int wildcard_least_one);

#endif

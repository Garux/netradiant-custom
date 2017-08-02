#if !defined( INCLUDED_FILTERBAR_H )
#define INCLUDED_FILTERBAR_H





//#include "string/string.h"
#include "string/stringfwd.h"

typedef struct _GtkToolbar GtkToolbar;

GtkToolbar* create_filter_toolbar();

CopiedString GetCommonShader( const char* name );

const char* GetCaulkShader();

#endif

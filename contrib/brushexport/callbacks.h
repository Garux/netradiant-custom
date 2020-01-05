typedef struct _GtkWidget GtkWidget;
typedef struct _GtkButton GtkButton;

namespace callbacks {

void OnExportClicked( GtkButton *, gpointer );
void OnAddMaterial( GtkButton *, gpointer );
void OnRemoveMaterial( GtkButton *, gpointer );
gboolean OnRemoveMaterialKb( GtkWidget *, GdkEventKey *, gpointer );

} // callbacks

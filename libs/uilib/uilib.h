#ifndef INCLUDED_UILIB_H
#define INCLUDED_UILIB_H

#include <string>

using ui_accelgroup = struct _GtkAccelGroup;
using ui_adjustment = struct _GtkAdjustment;
using ui_alignment = struct _GtkAlignment;
using ui_box = struct _GtkBox;
using ui_button = struct _GtkButton;
using ui_checkbutton = struct _GtkCheckButton;
using ui_combobox = struct _GtkComboBox;
using ui_comboboxtext = struct _GtkComboBoxText;
using ui_cellrenderer = struct _GtkCellRenderer;
using ui_cellrenderertext = struct _GtkCellRendererText;
using ui_entry = struct _GtkEntry;
using ui_evkey = struct _GdkEventKey;
using ui_frame = struct _GtkFrame;
using ui_hbox = struct _GtkHBox;
using ui_hscale = struct _GtkHScale;
using ui_hpaned = struct _GtkHPaned;
using ui_image = struct _GtkImage;
using ui_label = struct _GtkLabel;
using ui_menu = struct _GtkMenu;
using ui_menuitem = struct _GtkMenuItem;
using ui_modal = struct ModalDialog;
using ui_object = struct _GtkObject;
using ui_paned = struct _GtkPaned;
using ui_scrolledwindow = struct _GtkScrolledWindow;
using ui_spinbutton = struct _GtkSpinButton;
using ui_table = struct _GtkTable;
using ui_textview = struct _GtkTextView;
using ui_treemodel = struct _GtkTreeModel;
using ui_treepath = struct _GtkTreePath;
using ui_treeview = struct _GtkTreeView;
using ui_treeviewcolumn = struct _GtkTreeViewColumn;
using ui_typeinst = struct _GTypeInstance;
using ui_vbox = struct _GtkVBox;
using ui_vpaned = struct _GtkVPaned;
using ui_widget = struct _GtkWidget;
using ui_window = struct _GtkWindow;

namespace ui {

    void init(int argc, char *argv[]);

    void main();

    enum class alert_type {
        OK,
        OKCANCEL,
        YESNO,
        YESNOCANCEL,
        NOYES,
    };

    enum class alert_icon {
        Default,
        Error,
        Warning,
        Question,
        Asterisk,
    };

    enum class alert_response {
        OK,
        CANCEL,
        YES,
        NO,
    };

    enum class window_type {
        TOP,
        POPUP
    };

    template<class Self, class T, bool implicit = true>
    struct Convertible;

    template<class Self, class T>
    struct Convertible<Self, T, true> {
        operator T *() const
        { return reinterpret_cast<T *>(static_cast<const Self *>(this)->_handle); }
    };

    template<class Self, class T>
    struct Convertible<Self, T, false> {
        explicit operator T *() const
        { return reinterpret_cast<T *>(static_cast<const Self *>(this)->_handle); }
    };

    class Object : public Convertible<Object, ui_object, false> {
    public:
        using native = ui_object;
        void *_handle;

        Object(void *h) : _handle(h)
        { }

        explicit operator bool() const
        { return _handle != nullptr; }

        explicit operator ui_typeinst *() const
        { return (ui_typeinst *) _handle; }

        explicit operator void *() const
        { return _handle; }
    };

    static_assert(sizeof(Object) == sizeof(ui_widget *), "object slicing");

    class Widget : public Object, public Convertible<Widget, ui_widget> {
    public:
        using native = ui_widget;
        explicit Widget(ui_widget *h = nullptr) : Object((void *) h)
        { }

        alert_response alert(std::string text, std::string title = "NetRadiant",
                             alert_type type = alert_type::OK, alert_icon icon = alert_icon::Default);

        const char *file_dialog(bool open, const char *title, const char *path = nullptr,
                                const char *pattern = nullptr, bool want_load = false, bool want_import = false,
                                bool want_save = false);
    };

    static_assert(sizeof(Widget) == sizeof(Object), "object slicing");

    extern Widget root;

#define WRAP(name, super, impl, methods) \
    class name : public super, public Convertible<name, impl> { \
        public: \
            using native = impl; \
            explicit name(impl *h) : super(reinterpret_cast<super::native *>(h)) {} \
        methods \
    }; \
    static_assert(sizeof(name) == sizeof(super), "object slicing")

    WRAP(AccelGroup, Object, ui_accelgroup,
         AccelGroup();
    );

    WRAP(Adjustment, Widget, ui_adjustment,
         Adjustment(double value,
                    double lower, double upper,
                    double step_increment, double page_increment,
                    double page_size);
    );

    WRAP(Alignment, Widget, ui_alignment,
         Alignment(float xalign, float yalign, float xscale, float yscale);
    );

    WRAP(Box, Widget, ui_box,);

    WRAP(Button, Widget, ui_button,
         Button();
         Button(const char *label);
    );

    WRAP(CellRenderer, Object, ui_cellrenderer,);

    WRAP(CellRendererText, CellRenderer, ui_cellrenderertext,
         CellRendererText();
    );

    WRAP(CheckButton, Widget, ui_checkbutton,
         CheckButton(const char *label);
    );

    WRAP(ComboBox, Widget, ui_combobox,);

    WRAP(ComboBoxText, ComboBox, ui_comboboxtext,
         ComboBoxText();
    );

    WRAP(Entry, Widget, ui_entry,
         Entry();
         Entry(std::size_t max_length);
    );

    WRAP(Frame, Widget, ui_frame,
         Frame(const char *label = nullptr);
    );

    WRAP(HBox, Box, ui_hbox,
         HBox(bool homogenous, int spacing);
    );

    WRAP(HScale, Widget, ui_hscale,
         HScale(Adjustment adjustment);
         HScale(double min, double max, double step);
    );

    WRAP(Image, Widget, ui_image,
         Image();
    );

    WRAP(Label, Widget, ui_label,
         Label(const char *label);
    );

    WRAP(Menu, Widget, ui_menu,
         Menu();
    );

    WRAP(MenuItem, Widget, ui_menuitem,
         MenuItem(const char *label, bool mnemonic = false);
    );

    WRAP(Paned, Widget, ui_paned,);

        WRAP(HPaned, Paned, ui_hpaned,
             HPaned();
        );

        WRAP(VPaned, Paned, ui_vpaned,
             VPaned();
        );

    WRAP(ScrolledWindow, Widget, ui_scrolledwindow,
         ScrolledWindow();
    );

    WRAP(SpinButton, Widget, ui_spinbutton,
         SpinButton(Adjustment adjustment, double climb_rate, std::size_t digits);
    );

    WRAP(Table, Widget, ui_table,
         Table(std::size_t rows, std::size_t columns, bool homogenous);
    );

    WRAP(TextView, Widget, ui_textview,
         TextView();
    );

    WRAP(TreeModel, Widget, ui_treemodel,);

    WRAP(TreePath, Object, ui_treepath,
         TreePath();
         TreePath(const char *path);
    );

    WRAP(TreeView, Widget, ui_treeview,
         TreeView();
         TreeView(TreeModel model);
    );

    struct TreeViewColumnAttribute {
        const char *attribute;
        int column;
    };
    WRAP(TreeViewColumn, Widget, ui_treeviewcolumn,
         TreeViewColumn(const char *title, CellRenderer renderer, std::initializer_list<TreeViewColumnAttribute> attributes);
    );

    WRAP(VBox, Box, ui_vbox,
         VBox(bool homogenous, int spacing);
    );

    WRAP(Window, Widget, ui_window,
         Window() : Window(nullptr) {};
         Window(window_type type);

         Window create_dialog_window(const char *title, void func(), void *data, int default_w = -1,
                                     int default_h = -1);

         Window create_modal_dialog_window(const char *title, ui_modal &dialog, int default_w = -1,
                                           int default_h = -1);

         Window create_floating_window(const char *title);

         std::uint64_t on_key_press(bool (*f)(Widget widget, ui_evkey *event, void *extra),
                                    void *extra = nullptr);
    );

#undef WRAP

}

#endif

#ifndef INCLUDED_UILIB_H
#define INCLUDED_UILIB_H

#include <string>

using ui_typeinst = struct _GTypeInstance;
using ui_widget = struct _GtkWidget;
using ui_window = struct _GtkWindow;
using ui_button = struct _GtkButton;
using ui_menuitem = struct _GtkMenuItem;
using ui_label = struct _GtkLabel;

using ui_evkey = struct _GdkEventKey;

using ui_modal = struct ModalDialog;

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
        DEFAULT,
        ERROR,
        WARNING,
        QUESTION,
        ASTERISK,
    };

    enum class alert_response {
        OK,
        CANCEL,
        YES,
        NO,
    };

    template<class Self, class T>
    class Convertible {
    public:
        T *handle() const
        { return (T *) static_cast<const Self *>(this)->_handle; }

        operator T *() const
        { return handle(); }
    };

    class Base {
    public:
        void *_handle;

        Base(void *h) : _handle(h)
        { }

        explicit operator bool() const
        { return _handle != nullptr; }

        explicit operator ui_typeinst *() const
        { return (ui_typeinst *) _handle; }

        explicit operator void *() const
        { return _handle; }
    };

    static_assert(sizeof(Base) == sizeof(ui_widget *), "object slicing");

    class Widget : public Base, public Convertible<Widget, ui_widget> {
    public:
        explicit Widget(ui_widget *h = nullptr) : Base((void *) h)
        { }

        alert_response alert(std::string text, std::string title = "NetRadiant",
                             alert_type type = alert_type::OK, alert_icon icon = alert_icon::DEFAULT);

        const char *file_dialog(bool open, const char *title, const char *path = nullptr,
                                const char *pattern = nullptr, bool want_load = false, bool want_import = false,
                                bool want_save = false);
    };

    static_assert(sizeof(Widget) == sizeof(Base), "object slicing");

    extern Widget root;

#define WIDGET(name, impl, methods) \
    class name : public Widget, public Convertible<name, impl> { \
        public: \
            explicit name(impl *h = nullptr) : Widget(reinterpret_cast<ui_widget *>(h)) {} \
        methods \
    }; \
    static_assert(sizeof(name) == sizeof(Widget), "object slicing")

    WIDGET(Window, ui_window,
           Window create_dialog_window(const char *title, void func(), void *data, int default_w = -1,
                                       int default_h = -1);

                   Window create_modal_dialog_window(const char *title, ui_modal &dialog, int default_w = -1,
                                                     int default_h = -1);

                   Window create_floating_window(const char *title);

                   std::uint64_t on_key_press(bool (*f)(Widget widget, ui_evkey *event, void *extra),
                                              void *extra = nullptr);
    );

    WIDGET(Button, ui_button,
           Button(const char *label);
    );

    WIDGET(CheckButton, ui_widget,);

    WIDGET(SpinButton, ui_widget,);

    WIDGET(MenuItem, ui_menuitem,);

    WIDGET(Label, ui_label,
           Label(const char *label);
    );

}

#endif

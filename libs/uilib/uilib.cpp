#include "uilib.h"

#include <tuple>

#include <gtk/gtk.h>

#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/window.h"

namespace ui {

    void init(int argc, char *argv[])
    {
        gtk_disable_setlocale();
        gtk_init(&argc, &argv);
    }

    void main()
    {
        gtk_main();
    }

    Widget root;

    alert_response Widget::alert(std::string text, std::string title, alert_type type, alert_icon icon)
    {
        auto ret = gtk_MessageBox(*this, text.c_str(),
                                  title.c_str(),
                                  type == alert_type::OK ? eMB_OK :
                                  type == alert_type::OKCANCEL ? eMB_OKCANCEL :
                                  type == alert_type::YESNO ? eMB_YESNO :
                                  type == alert_type::YESNOCANCEL ? eMB_YESNOCANCEL :
                                  type == alert_type::NOYES ? eMB_NOYES :
                                  eMB_OK,
                                  icon == alert_icon::DEFAULT ? eMB_ICONDEFAULT :
                                  icon == alert_icon::ERROR ? eMB_ICONERROR :
                                  icon == alert_icon::WARNING ? eMB_ICONWARNING :
                                  icon == alert_icon::QUESTION ? eMB_ICONQUESTION :
                                  icon == alert_icon::ASTERISK ? eMB_ICONASTERISK :
                                  eMB_ICONDEFAULT
        );
        return
                ret == eIDOK ? alert_response::OK :
                ret == eIDCANCEL ? alert_response::CANCEL :
                ret == eIDYES ? alert_response::YES :
                ret == eIDNO ? alert_response::NO :
                alert_response::OK;
    }

    const char *Widget::file_dialog(bool open, const char *title, const char *path,
                                    const char *pattern, bool want_load, bool want_import,
                                    bool want_save)
    {
        return ::file_dialog(*this, open, title, path, pattern, want_load, want_import, want_save);
    }

    Window Window::create_dialog_window(const char *title, void func(), void *data, int default_w, int default_h)
    {
        return Window(::create_dialog_window(*this, title, func, data, default_w, default_h));
    }

    Window Window::create_modal_dialog_window(const char *title, ui_modal &dialog, int default_w, int default_h)
    {
        return Window(::create_modal_dialog_window(*this, title, dialog, default_w, default_h));
    }

    Window Window::create_floating_window(const char *title)
    {
        return Window(::create_floating_window(title, *this));
    }

    std::uint64_t Window::on_key_press(bool (*f)(Widget widget, ui_evkey *event, void *extra), void *extra)
    {
        auto pass = std::make_tuple(f, extra);
        auto func = [](ui_widget *widget, GdkEventKey *event, void *pass_) -> bool {
            using pass_t = decltype(pass);
            auto &args = *(pass_t *) pass_;
            auto func = std::get<0>(args);
            auto pass = std::get<1>(args);
            return func(Widget(widget), event, pass);
        };
        return g_signal_connect(G_OBJECT(*this), "key-press-event", (GCallback) +func, &pass);
    }

    AccelGroup::AccelGroup() : AccelGroup(GTK_ACCEL_GROUP(gtk_accel_group_new()))
    { }

    Adjustment::Adjustment(double value,
                           double lower, double upper,
                           double step_increment, double page_increment,
                           double page_size)
            : Adjustment(
            GTK_ADJUSTMENT(gtk_adjustment_new(value, lower, upper, step_increment, page_increment, page_size)))
    { }

    Alignment::Alignment(float xalign, float yalign, float xscale, float yscale)
            : Alignment(GTK_ALIGNMENT(gtk_alignment_new(xalign, yalign, xscale, yscale)))
    { }

    Button::Button() : Button(GTK_BUTTON(gtk_button_new()))
    { }

    Button::Button(const char *label) : Button(GTK_BUTTON(gtk_button_new_with_label(label)))
    { }

    CellRendererText::CellRendererText() : CellRendererText(GTK_CELL_RENDERER_TEXT(gtk_cell_renderer_text_new()))
    { }

    ComboBox ComboBoxText()
    { return ComboBox(GTK_COMBO_BOX(gtk_combo_box_new_text())); }

    CheckButton::CheckButton(const char *label) : CheckButton(GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label)))
    { }

    Entry::Entry() : Entry(GTK_ENTRY(gtk_entry_new()))
    { }

    Entry::Entry(std::size_t max_length) : Entry(GTK_ENTRY(gtk_entry_new_with_max_length(max_length)))
    { }

    Frame::Frame(const char *label) : Frame(GTK_FRAME(gtk_frame_new(label)))
    { }

    HBox::HBox(bool homogenous, int spacing) : HBox(GTK_HBOX(gtk_hbox_new(homogenous, spacing)))
    { }

    Label::Label(const char *label) : Label(GTK_LABEL(gtk_label_new(label)))
    { }

    Menu::Menu() : Menu(GTK_MENU(gtk_menu_new()))
    { }

    MenuItem::MenuItem(const char *label, bool mnemonic) : MenuItem(
            GTK_MENU_ITEM((mnemonic ? gtk_menu_item_new_with_mnemonic : gtk_menu_item_new_with_label)(label)))
    { }

    HPaned::HPaned() : HPaned(GTK_HPANED(gtk_hpaned_new()))
    { }

    VPaned::VPaned() : VPaned(GTK_VPANED(gtk_vpaned_new()))
    { }

    ScrolledWindow::ScrolledWindow() : ScrolledWindow(GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr)))
    { }

    SpinButton::SpinButton(Adjustment adjustment, double climb_rate, std::size_t digits) : SpinButton(
            GTK_SPIN_BUTTON(gtk_spin_button_new(adjustment, climb_rate, digits)))
    { }

    Table::Table(std::size_t rows, std::size_t columns, bool homogenous) : Table(
            GTK_TABLE(gtk_table_new(rows, columns, homogenous)))
    { }

    TreePath::TreePath() : TreePath(gtk_tree_path_new())
    { }

    TreePath::TreePath(const char *path) : TreePath(gtk_tree_path_new_from_string(path))
    { }

    TreeView::TreeView(TreeModel model) : TreeView(GTK_TREE_VIEW(gtk_tree_view_new_with_model(model)))
    { }

    TreeViewColumn::TreeViewColumn(const char *title, CellRenderer renderer,
                                   std::initializer_list<TreeViewColumnAttribute> attributes)
            : TreeViewColumn(gtk_tree_view_column_new_with_attributes(title, renderer, nullptr))
    {
        for (auto &it : attributes) {
            gtk_tree_view_column_add_attribute(*this, renderer, it.attribute, it.column);
        }
    };

    VBox::VBox(bool homogenous, int spacing) : VBox(GTK_VBOX(gtk_vbox_new(homogenous, spacing)))
    { }

}

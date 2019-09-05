#include "pch.h"
#include "util.h"
#include "PlatformContext.h"

namespace Raekor {

std::string PlatformContext::open_file_dialog(const std::vector<Ffilter>& filters) {
    //init gtk
    m_assert(gtk_init_check(NULL, NULL), "failed to init gtk");
    // allocate a new dialog window
    auto dialog = gtk_file_chooser_dialog_new(
        "Open File",
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        NULL);

    for (auto& filter : filters) {
        auto gtk_filter = gtk_file_filter_new();
        gtk_file_filter_set_name(gtk_filter, filter.name.c_str());

        std::vector<std::string> patterns;
        std::string extensions = filter.extensions;
        size_t pos = 0;
        while(pos != std::string::npos) {
            pos = extensions.find(";");
            patterns.push_back(extensions.substr(0, pos));
            extensions.erase(0, pos + 1);
        }
        for(auto& pattern : patterns) {
            gtk_file_filter_add_pattern(gtk_filter, pattern.c_str());
        }
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), gtk_filter);
    }

    char* path = NULL;
    // if the ok button is pressed (gtk response type ACCEPT) we get the selected filepath
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }
    // destroy our dialog window
    gtk_widget_destroy(dialog);
    // if our filepath is not empty we make it the return value
    std::string file;
    if (path) {
        file = std::string(path, strlen(path));
    }
    // main event loop for our window, this took way too long to fix 
    // (newer GTK's produce segfaults, something to do with SDL)
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
    return file;
}

} // Namespace Raekor
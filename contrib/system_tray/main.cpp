// reading file
#include <iostream>
#include <fstream>
// trimming string
#include <algorithm>
#include <cctype>
#include <locale>
#include <stdlib.h>
// regex
#include <regex>
// gtk
#include <gtk/gtk.h>

static std::string logfilepath = "";

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

// с++ read file to string
std::string readfile(std::string filepath) {
  std::fstream f(filepath, std::fstream::in );
  std::string s;
  getline( f, s, '\0');
  f.close();
  return s;
}

// c++ regex example
std::string whereIsLogFile() {
	std::string xl2tpd=readfile("/etc/xl2tpd/xl2tpd.conf");

	if (xl2tpd!="") {
		xl2tpd = std::regex_replace (xl2tpd, std::regex("(^|\n);[^\n]*(?=\n|$)"), "");
		std::string options = "";
		std::smatch match;
		if (std::regex_search(xl2tpd, match, std::regex("(^|\n)pppoptfile\\s*=\\s*([^\n]+)(\n|$)")) && match.size() > 1) {
			options=readfile(match.str(2));
			if (options!="") {
				options = std::regex_replace (options, std::regex("(^|\n)#[^\n]*(?=\n|$)"), "");
				if (std::regex_search(options, match, std::regex("(^|\n)logfile\\s*([^\n]+)(\n|$)")) && match.size() > 1) {
					return match.str(2);
				}
			}
		}
	}
	return "/home/user/beeline.xl2tpd.log";
}

// c++ search string in file
int get_status_from_file() {

  std::string s = readfile(logfilepath);
  // split file by string
  std::string delimiter = "Modem hangup";
   // select last element of array
   size_t pos = 0;
   std::string token;
   while ((pos = s.find(delimiter)) != std::string::npos) {
      token = s.substr(0, pos);
      s.erase(0, pos + delimiter.length());
   }
   trim(s);
   if (s=="") {
      return 0;
   }
   else {
      delimiter = "status = 0x0";
      token = "1";
      while ((pos = s.find(delimiter)) != std::string::npos) {
         token = "3";
         s.erase(0, pos + delimiter.length());
      }
      trim(s);
      if (s=="") {
         return 2;
      }
      if (token == "1") {
         return 1;
      }
      else {
         return 3;
      }
   }
}

// c++ convert gchar std::string
const gchar* convertstring2gchar(std::string s) {
   const gchar* x;
   const char* cv = s.c_str();
   x = (const gchar*) cv;
   return x;
}

// с++ system tray icon
static gboolean updateIcon(gpointer data) {
   GtkStatusIcon *icon = (GtkStatusIcon*)data;
   int m = get_status_from_file();
   std::string icon_name;
   std::string icontext;
   std::string icons_path = "/usr/share/xl2tpd-tray-icon/";
   std::string ext = ".png";
    switch (m) {
    case 0: icon_name="offline"; icontext = "Internet offline"; break;
    case 1: icon_name="connect"; icontext = "Trying to connect to internet"; break;
    case 2: icon_name="online"; icontext = "Internet works"; break;
    case 3: icon_name="disconnect"; icontext = "Disconnecting internet"; break;
    }
   gtk_status_icon_set_from_file (icon, convertstring2gchar(icons_path+icon_name + ext));
   gtk_status_icon_set_tooltip (icon, convertstring2gchar(icontext));
   return true;
}

static void trayIconPopup(GtkStatusIcon *status_icon, guint button, guint32 activate_time, gpointer popUpMenu) {
    gtk_menu_popup(GTK_MENU(popUpMenu), NULL, NULL, gtk_status_icon_position_menu, status_icon, button, activate_time);
}

static void trayExit(GtkMenuItem *item, gpointer user_data) {
    exit(0);
}

int main(int argc, char **argv) {
   gtk_init(&argc,&argv);

   GtkStatusIcon *icon = gtk_status_icon_new_from_file ("connect.png");
   gtk_status_icon_set_visible(icon, 1);
   gtk_status_icon_set_tooltip(icon, "Icon");

   GtkWidget *menu, *menuItemView, *menuItemExit;
   menu = gtk_menu_new();
   menuItemExit = gtk_menu_item_new_with_label ("Exit");
   gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemExit);
   gtk_widget_show_all (menu);
   g_signal_connect(GTK_STATUS_ICON (icon), "popup-menu", GTK_SIGNAL_FUNC (trayIconPopup), menu);
   g_signal_connect (G_OBJECT (menuItemExit), "activate", G_CALLBACK (trayExit), NULL);


	logfilepath = whereIsLogFile();
	// redraw status icon every 2 seconds
	g_timeout_add_seconds(2, updateIcon, icon);

	gtk_main();
	return 0;
}

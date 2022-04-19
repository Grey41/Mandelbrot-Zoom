#INCLUDE = gtk-3.0 glib-2.0 pango-1.0 harfbuzz cairo gdk-pixbuf-2.0 atk-1.0
#FOLDERS = $(addprefix -I/usr/include/, $(INCLUDE)) -I/usr/lib/x86_64-linux-gnu/glib-2.0/include

zoom: main.c
	gcc main.c -o zoom -Wall -Wextra `pkg-config --cflags --libs gtk+-3.0` -lepoxy -lgmp -lm -lpng

# -I/usr/include/gtk-3.0 -I/usr/include/glib-2.0 \
# -I/usr/include/pango-1.0 -I/usr/include/harfbuzz -I/usr/include/cairo \
# -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/atk-1.0 \
# -I/usr/lib/x86_64-linux-gnu/glib-2.0/include 
#-Werror
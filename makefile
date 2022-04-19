zoom: main.c
	gcc main.c -o zoom -Wall -Wextra `pkg-config --cflags --libs gtk+-3.0` -lepoxy -lgmp -lm -lpng

#include <gtk/gtk.h>
#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <gmp.h>
#include <math.h>
#include <png.h>

#define UNUSED __attribute__((unused))
#define COLORED 1
#define QUEUED 2
#define SCALE 2

typedef struct {
    unsigned short x;
    unsigned short y;
} Vec2;

static GLuint program, vertexArray, texture;
static mpz_t posX, posY, zoom;
static Vec2 size = {640, 480};
static GtkWidget *window;

static unsigned char *image;
static unsigned char *overlay;
static unsigned int *queue;
static unsigned int length = 0;
static unsigned char resolution = 100;

static Vec2 getDimensions() {
    const Vec2 dimentions = {
        (double) size.x / 100 * resolution,
        (double) size.y / 100 * resolution
    };

    return dimentions;
}

static double superLog(mpz_t integer) {
    signed long int exponent;
    const double decimal = mpz_get_d_2exp(&exponent, integer);

    return log(decimal) + log(2) * exponent;
}

static unsigned char getColor(const unsigned int pixel) {
    if (overlay[pixel] & COLORED)
        return image[pixel];

    mpz_t originalX, originalY, copyX, copyY, powerX, powerY;
    const Vec2 res = getDimensions();
    unsigned long loop = 0;

    mpz_init_set_si(originalX, (signed int) pixel % res.x - res.x / 2);
    mpz_init_set_si(originalY, (signed int) pixel / res.x - res.y / 2);
    mpz_sub(originalX, originalX, posX);
    mpz_sub(originalY, originalY, posY);
    mpz_inits(copyX, copyY, powerX, powerY, NULL);

    for (unsigned long i = 0; i < superLog(zoom) * 50; i ++) {
        mpz_t left, right, template;
        mpz_inits(left, right, NULL);

        mpz_mul(powerX, copyX, copyX);
        mpz_mul(powerY, copyY, copyY);
        mpz_div(powerX, powerX, zoom);
        mpz_div(powerY, powerY, zoom);

        mpz_add(left, copyX, copyY);
        mpz_mul_ui(right, zoom, 5);

        if (mpz_cmp(left, right) > 0) {
            mpz_clears(left, right, NULL);
            loop = i;
            break;
        }

        mpz_init(template);
        mpz_sub(template, powerX, powerY);
        mpz_add(template, template, originalX);

        mpz_mul(copyY, copyX, copyY);
        mpz_mul_ui(copyY, copyY, 2);
        mpz_div(copyY, copyY, zoom);
        mpz_add(copyY, copyY, originalY);

        mpz_set(copyX, template);
        mpz_clears(left, right, template, NULL);
    }

    mpz_clears(originalX, originalY, copyX, copyY, powerX, powerY, NULL);
    image[pixel] = loop % 256;
    overlay[pixel] |= COLORED;

    return image[pixel];
}

static void checkPixel(const int pixel, const unsigned char color) {
    if (!(overlay[pixel] & QUEUED) && color != getColor(pixel)) {
        overlay[pixel] |= QUEUED;
        queue[length] = pixel;
        length ++;
    }
}

static void reset() {
    const Vec2 res = getDimensions();
    length = 0;

    for (unsigned int i = 0; i < res.x * res.y; i ++)
        overlay[i] = 0;

    for (unsigned short x = 0; x < res.x; x ++)
        for (unsigned short y = 0; y < res.y; y ++)
            if (!x || x == res.x - 1 || !y || y == res.y - 1) {
                queue[length] = x + y * res.x;
                length ++;
            }
}

static void changeSize(const Vec2 old) {
    const Vec2 res = getDimensions();
    overlay = realloc(overlay, res.x * res.y * sizeof overlay);
    queue = realloc(queue, res.x * res.y * sizeof queue);

    unsigned char *pending = malloc(res.x * res.y * sizeof pending);
    const short offsetX = (old.x - res.x) / 2;
    const short offsetY = (old.y - res.y) / 2;

    for (unsigned short x = 0; x < res.x; x ++)
        for (unsigned short y = 0; y < res.y; y ++)
            pending[x + y * res.x] = image[abs((x + offsetX) + (y + offsetY) * old.x)];

    reset();
    free(image);
    image = pending;
}

static void realize(GtkGLArea *area) {
    gtk_gl_area_make_current(area);
    
    const Vec2 res = getDimensions();    
    image = malloc(res.x * res.y * sizeof image);
    overlay = malloc(res.x * res.y * sizeof overlay);
    queue = malloc(res.x * res.y * sizeof queue);

    mpz_init_set_ui(posX, 0);
    mpz_init_set_ui(posY, 0);
    mpz_init_set_ui(zoom, 200);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    GLchar mesh[] = {-1, 1, 0, 0, 1, 1, 1, 0, -1, -1, 0, 1, 1, -1, 1, 1};
    GLuint vertexBuffer;

    const GLchar *vertexSource =
        "#version 300 es\n"

        "in vec2 vertex;"
        "in vec2 coordinate;"
        "out vec2 position;"

        "void main() {"
        "    gl_Position = vec4(vertex, 0, 1);"
        "    position = coordinate;"
        "}";

    const GLchar *fragmentSource =
        "#version 300 es\n"

        "precision lowp float;"
        "uniform sampler2D sampler;"

        "in vec2 position;"
        "out vec4 fragment;"

        "void main() {"
        "    float pixel = texture(sampler, position).x;"

        "    fragment = vec4("
        "        sin(pixel * 3.0),"
        "        sin(pixel * 4.0),"
        "        sin(pixel * 5.0), 1);"
        "}";

    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);

    glAttachShader(program = glCreateProgram(), vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glUseProgram(program);
    glUniform1i(0, 0);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof mesh, mesh, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_BYTE, GL_FALSE, sizeof *mesh * 4, 0);
    glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, sizeof *mesh * 4, (void *) (sizeof *mesh * 2));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glDeleteBuffers(1, &vertexBuffer);
    glBindVertexArray(vertexArray);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

static void unrealize(GtkGLArea *area) {
    gtk_gl_area_make_current(area);
    mpz_clears(posX, posY, zoom, NULL);
    
    free(image);
    free(overlay);
    free(queue);

    glDeleteProgram(program);
    glDeleteVertexArrays(1, &vertexArray);
    glDeleteTextures(1, &texture);
}

static void resize(UNUSED GtkGLArea *area, gint width, gint height) {
    const Vec2 res = getDimensions();

    size.x = width;
    size.y = height;
    changeSize(res);
}

static gboolean render(GtkGLArea *area) {
    gtk_widget_queue_draw(GTK_WIDGET(area));

    const Vec2 res = getDimensions();
    const clock_t timer = clock();

    while (length && (double) (clock() - timer) / CLOCKS_PER_SEC < 1.0 / 60) {
        const unsigned int pixel = queue[0];
        const unsigned char color = getColor(pixel);
        const unsigned short x = pixel % res.x;
        const unsigned short y = pixel / res.x;
        
        for (unsigned int i = 0; i < length; i ++)
            queue[i] = queue[i + 1];

        length --;

        if (y > 0) {
            checkPixel(pixel - res.x, color);
            if (x > 0) checkPixel(pixel - res.x - 1, color);
            if (x < res.x - 1) checkPixel(pixel - res.x + 1, color);
        }

        if (y < res.y - 1) {
            checkPixel(pixel + res.x, color);
            if (x > 0) checkPixel(pixel + res.x - 1, color);
            if (x < res.x - 1) checkPixel(pixel + res.x + 1, color);
        }

        if (x > 0) checkPixel(pixel - 1, color);
        if (x < res.x - 1) checkPixel(pixel + 1, color);

        if (!length) {
            for (unsigned int pixel = 0; pixel < res.x * res.y; pixel ++)
                if (!(overlay[pixel] & COLORED))
                    image[pixel] = image[pixel - 1];
        }
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, res.x, res.y, 0, GL_RED, GL_UNSIGNED_BYTE, image);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    return true;
}

static void valueChanged(GtkRange *range) {
    const Vec2 res = getDimensions();

    resolution = gtk_range_get_value(range);
    changeSize(res);
}

static gboolean buttonPressEvent(UNUSED GtkWidget *window, GdkEventButton *event) {
    const Vec2 res = getDimensions();
    mpz_t dataX, dataY;

    mpz_mul_ui(zoom, zoom, SCALE);
    mpz_mul_ui(posX, posX, SCALE);
    mpz_mul_ui(posY, posY, SCALE);

    const unsigned short mouseX = event -> x * (double) res.x / size.x;
    const unsigned short mouseY = event -> y * (double) res.y / size.y;

    mpz_init_set_si(dataX, (mouseX - res.x / 2) * (SCALE - 1));
    mpz_init_set_si(dataY, (mouseY - res.y / 2) * (SCALE - 1));
    mpz_sub(posX, posX, dataX);
    mpz_sub(posY, posY, dataY);

    mpz_clears(dataX, dataY, NULL);
    unsigned char *pending = malloc(res.x * res.y * sizeof(unsigned char));

    for (unsigned short x = 0; x < res.x; x ++)
        for (unsigned short y = 0; y < res.y; y ++)
            pending[x + y * res.x] = image[(x + mouseX) / SCALE + (y + mouseY) / SCALE * res.x];

    reset();
    free(image);
    image = pending;

    return true;
}

static void response(GtkDialog *dialog, int response) {
    if (response == GTK_RESPONSE_ACCEPT) {
        const gchar *name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        const Vec2 res = getDimensions();

        FILE *file = fopen(name, "wb");

        if (!file) {
            printf("Failed to create file\n");
            return;
        }

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png) {
            printf("Failed to allocate write struct\n");
            fclose(file);
            return;
        }

        png_infop info = png_create_info_struct(png);

        if (!info) {
            printf("Failed to allocate info struct\n");
            png_destroy_write_struct(&png, NULL);
            fclose(file);
            return;
        }

        if (setjmp(png_jmpbuf(png))) {
            printf("Failed to create png\n");
            png_destroy_write_struct(&png, &info);
            fclose(file);
            return;
        }

        png_init_io(png, file);
        png_set_IHDR(
            png, info, res.x, res.y, 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        png_write_info(png, info);
        png_bytep row = malloc(res.x * sizeof image);

        for (unsigned short y = 0; y < res.y; y ++) {
            for (unsigned short x = 0; x < res.x; x ++)
                row[x] = image[x + y * res.x];

            png_write_row(png, row);
        }

        png_write_end(png, NULL);
        png_free_data(png, info, PNG_FREE_ALL, -1);
        fclose(file);
        free(row);
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void clicked(UNUSED GtkButton *button, UNUSED gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Save Image", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE,
        "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);

    g_signal_connect(dialog, "response", G_CALLBACK(response), NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "Image");
    gtk_widget_show(dialog);
}

static void activate(GtkApplication *app) {
    const gchar css[] =
        ".controls {"
        "    background-color: @bg_color;"
        "    border-bottom-left-radius: 0.3em;"
        "    padding: 1em"
        "}";

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, sizeof css, NULL);

    GdkScreen *screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(
        screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *area = gtk_gl_area_new();
    g_signal_connect(area, "realize", G_CALLBACK(realize), NULL);
    g_signal_connect(area, "unrealize", G_CALLBACK(unrealize), NULL);
    g_signal_connect(area, "resize", G_CALLBACK(resize), NULL);
    g_signal_connect(area, "render", G_CALLBACK(render), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(box, 200, -1);
    gtk_widget_set_halign(box, GTK_ALIGN_END);
    gtk_widget_set_valign(box, GTK_ALIGN_START);

    GtkStyleContext *style = gtk_widget_get_style_context(box);
    gtk_style_context_add_class(style, "controls");

    GtkWidget *button = gtk_button_new_with_label("Save image");
    gtk_container_add(GTK_CONTAINER(box), button);
    g_signal_connect(button, "clicked", G_CALLBACK(clicked), NULL);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 100, 1);
    gtk_range_set_value(GTK_RANGE(scale), resolution);
    gtk_container_add(GTK_CONTAINER(box), scale);
    g_signal_connect(scale, "value-changed", G_CALLBACK(valueChanged), NULL);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), area);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), box);
    
    window = gtk_application_window_new(app);
    g_signal_connect(window, "button-press-event", G_CALLBACK(buttonPressEvent), NULL);
    gtk_container_add(GTK_CONTAINER(window), overlay);
    gtk_window_set_title(GTK_WINDOW(window), "Mandelbrot Zoom");
    gtk_window_set_default_size(GTK_WINDOW(window), size.x, size.y);
    gtk_widget_show_all(window);
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.mandelbrot", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
}
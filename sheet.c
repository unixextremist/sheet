#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX 4096
#define VERSION "0.2"
static char list[MAX][4096];
static int n,sel,top_idx;
static char current_dir[4096];
static char wallsetter[256] = "feh";
static int first_time = 1;

static int compare(const void *a, const void *b) {
    const char *file_a = strrchr(*(char(*)[4096])a, '/') + 1;
    const char *file_b = strrchr(*(char(*)[4096])b, '/') + 1;
    return strcmp(file_a, file_b);
}

static void expand_path(char *path) {
    if (path[0] == '~') {
        char *home = getenv("HOME");
        char expanded[4096];
        snprintf(expanded, sizeof(expanded), "%s%s", home, path + 1);
        strcpy(path, expanded);
    }
}

static void save_config() {
    char config_path[4096];
    snprintf(config_path, sizeof(config_path), "%s/.sheet_config", getenv("HOME"));
    FILE *f = fopen(config_path, "w");
    if (f) {
        fprintf(f, "DIR=%s\n", current_dir);
        fprintf(f, "SETTER=%s\n", wallsetter);
        fclose(f);
    }
}

static void save_last_wallpaper(const char *wallpaper) {
    char last_path[4096];
    snprintf(last_path, sizeof(last_path), "%s/.sheet_last_wallpaper", getenv("HOME"));
    FILE *f = fopen(last_path, "w");
    if (f) {
        fprintf(f, "%s\n", wallpaper);
        fclose(f);
    }
}

static char* load_last_wallpaper() {
    static char last_wallpaper[4096];
    char last_path[4096];
    snprintf(last_path, sizeof(last_path), "%s/.sheet_last_wallpaper", getenv("HOME"));
    FILE *f = fopen(last_path, "r");
    if (f) {
        if (fgets(last_wallpaper, sizeof(last_wallpaper), f)) {
            last_wallpaper[strcspn(last_wallpaper, "\n")] = 0;
            fclose(f);
            return last_wallpaper;
        }
        fclose(f);
    }
    return NULL;
}

static void load_config() {
    char config_path[4096];
    snprintf(config_path, sizeof(config_path), "%s/.sheet_config", getenv("HOME"));
    FILE *f = fopen(config_path, "r");
    if (f) {
        char line[4096];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "DIR=", 4) == 0) {
                strcpy(current_dir, line + 4);
                current_dir[strcspn(current_dir, "\n")] = 0;
                expand_path(current_dir);
            } else if (strncmp(line, "SETTER=", 7) == 0) {
                strcpy(wallsetter, line + 7);
                wallsetter[strcspn(wallsetter, "\n")] = 0;
            }
        }
        fclose(f);
        first_time = 0;
    }
}

static int scan(const char *p) {
    n = 0;
    DIR *d = opendir(p);
    if (!d) {
        return 0;
    }
    struct dirent *e;
    while ((e = readdir(d)) && n < MAX) {
        char *x = strrchr(e->d_name, '.');
        if (x && (!strcasecmp(x, ".jpg") || !strcasecmp(x, ".jpeg") || !strcasecmp(x, ".png") ||
                  !strcasecmp(x, ".gif") || !strcasecmp(x, ".bmp") || !strcasecmp(x, ".webp")))
            snprintf(list[n++], 4096, "%s/%s", p, e->d_name);
    }
    closedir(d);
    qsort(list, n, 4096, compare);
    return n;
}

static void draw() {
    clear();
    mvprintw(0, 0, "Directory: %s | Setter: %s", current_dir, wallsetter);
    mvprintw(1, 0, "Press F1 to change | Enter to set wallpaper | v to view | q to quit");

    if (n == 0) {
        mvprintw(3, 0, "No images found in directory");
        mvprintw(4, 0, "Press F1 to change directory");
    } else {
        int max_display = LINES - 3;
        for (int i = top_idx; i < n && i < top_idx + max_display; i++) {
            char *filename = strrchr(list[i], '/') + 1;
            mvprintw(i - top_idx + 2, 0, "%s %s", i == sel ? ">" : " ", filename);
        }
    }
    refresh();
}

static void show() {
    if (n == 0) return;

    def_prog_mode();
    endwin();

    pid_t pid = fork();
    if (pid == 0) {
        char width_str[16], height_str[16];
        snprintf(width_str, sizeof(width_str), "%d", COLS / 2);
        snprintf(height_str, sizeof(height_str), "%d", LINES - 2);

        char *args[] = {"viu", "-t", "-w", width_str, "-h", height_str, list[sel], NULL};
        execvp("viu", args);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }

    printf("\nPress any key to continue...");
    fflush(stdout);
    getchar();

    reset_prog_mode();
    refresh();
}

static void kill_wallpaper_processes() {
    FILE *fp = popen("ps -o pid= -o comm= | grep feh", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            int pid;
            if (sscanf(line, "%d", &pid) == 1) {
                kill(pid, SIGTERM);
            }
        }
        pclose(fp);
    }

    fp = popen("ps -o pid= -o comm= | grep swaybg", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            int pid;
            if (sscanf(line, "%d", &pid) == 1) {
                kill(pid, SIGTERM);
            }
        }
        pclose(fp);
    }

    fp = popen("ps -o pid= -o comm= | grep xwallpaper", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            int pid;
            if (sscanf(line, "%d", &pid) == 1) {
                kill(pid, SIGTERM);
            }
        }
        pclose(fp);
    }
}

static void set_wallpaper_from_file(const char *file) {
    kill_wallpaper_processes();

    pid_t pid = fork();
    if (pid == 0) {
        setsid();

        pid_t pid2 = fork();
        if (pid2 == 0) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }

            if (strcmp(wallsetter, "feh") == 0) {
                char *args[] = {"feh", "--bg-scale", (char*)file, NULL};
                execvp("feh", args);
            } else if (strcmp(wallsetter, "swaybg") == 0) {
                char *args[] = {"swaybg", "-m", "fill", "-i", (char*)file, NULL};
                execvp("swaybg", args);
            } else if (strcmp(wallsetter, "xwallpaper") == 0) {
                char *args[] = {"xwallpaper", "--zoom", (char*)file, NULL};
                execvp("xwallpaper", args);
            }
            perror("execvp");
            exit(1);
        } else {
            exit(0);
        }
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
        save_last_wallpaper(file);
        if (isatty(STDOUT_FILENO)) {
            mvprintw(LINES-1, 0, "Wallpaper set! (daemonized)");
            clrtoeol();
            refresh();
        } else {
            printf("Wallpaper set: %s\n", file);
        }
    }
}

static void set_wallpaper() {
    if (n == 0) return;
    set_wallpaper_from_file(list[sel]);
}

static void restore_last_wallpaper() {
    char *last_wallpaper = load_last_wallpaper();
    if (last_wallpaper) {
        struct stat st;
        if (stat(last_wallpaper, &st) == 0) {
            set_wallpaper_from_file(last_wallpaper);
        } else {
            fprintf(stderr, "Last wallpaper file not found: %s\n", last_wallpaper);
        }
    } else {
        fprintf(stderr, "No last wallpaper saved.\n");
    }
}

static void change_config() {
    def_prog_mode();
    endwin();

    printf("\nCurrent directory: %s\n", current_dir);
    printf("Enter new directory: ");
    fflush(stdout);
    char new_dir[4096];
    if (fgets(new_dir, sizeof(new_dir), stdin)) {
        new_dir[strcspn(new_dir, "\n")] = 0;
        if (strlen(new_dir) > 0) {
            expand_path(new_dir);
            strcpy(current_dir, new_dir);
        }
    }

    printf("\nCurrent wallpaper setter: %s\n", wallsetter);
    printf("Enter new setter (feh, swaybg, or xwallpaper): ");
    fflush(stdout);
    char new_setter[256];
    if (fgets(new_setter, sizeof(new_setter), stdin)) {
        new_setter[strcspn(new_setter, "\n")] = 0;
        if (strlen(new_setter) > 0 && (strcmp(new_setter, "feh") == 0 || strcmp(new_setter, "swaybg") == 0 || strcmp(new_setter, "xwallpaper") == 0))
            strcpy(wallsetter, new_setter);
    }

    save_config();
    n = scan(current_dir);
    sel = 0;
    top_idx = 0;

    printf("\nPress any key to continue...");
    fflush(stdout);
    getchar();

    reset_prog_mode();
    refresh();
}

static void first_time_setup() {
    def_prog_mode();
    endwin();

    char default_dir[4096];
    snprintf(default_dir, sizeof(default_dir), "%s/Pictures", getenv("HOME"));

    printf("Welcome to sheet!\n\n");
    printf("Use default directory (%s)? (y/n): ", default_dir);
    fflush(stdout);

    char choice[10];
    fgets(choice, sizeof(choice), stdin);

    if (choice[0] == 'y' || choice[0] == 'Y' || choice[0] == '\n') {
        strcpy(current_dir, default_dir);
    } else {
        printf("\nEnter directory with images: ");
        fflush(stdout);
        fgets(current_dir, sizeof(current_dir), stdin);
        current_dir[strcspn(current_dir, "\n")] = 0;
        expand_path(current_dir);
    }

    printf("\nChoose wallpaper setter (1 for feh, 2 for swaybg, 3 for xwallpaper): ");
    fflush(stdout);
    fgets(choice, sizeof(choice), stdin);
    if (choice[0] == '2')
        strcpy(wallsetter, "swaybg");
    else if (choice[0] == '3')
        strcpy(wallsetter, "xwallpaper");
    else
        strcpy(wallsetter, "feh");

    save_config();
    first_time = 0;

    printf("\nPress any key to launch...");
    fflush(stdout);
    getchar();

    reset_prog_mode();
    refresh();
}

static void print_help() {
    printf("sheet - Terminal wallpaper selector\n");
    printf("Version: %s\n\n", VERSION);
    printf("Usage: sheet [OPTION] [DIRECTORY]\n\n");
    printf("Options:\n");
    printf("  -h, --help     Display this help message\n");
    printf("  -v, --version  Display version information\n");
    printf("  -r, --restore  Restore last set wallpaper\n");
    printf("\nIf DIRECTORY is provided, it will be set as the image directory.\n");
    printf("Otherwise, the program starts with the saved or default directory.\n");
}

static void print_version() {
    printf("sheet version %s\n", VERSION);
}

int main(int argc, char **argv) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    load_config();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "--restore") == 0 || strcmp(argv[i], "-r") == 0) {
            restore_last_wallpaper();
            return 0;
        } else if (argv[i][0] != '-') {
            strcpy(current_dir, argv[i]);
            expand_path(current_dir);
            save_config();
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help();
            return 1;
        }
    }

    if (first_time && argc < 2) {
        first_time_setup();
    }

    n = scan(current_dir);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    sel = 0;
    top_idx = 0;
    draw();

    int ch;
    while ((ch = getch()) != 'q') {
        if (n > 0) {
            if (ch == KEY_DOWN && sel + 1 < n) {
                sel++;
                if (sel >= top_idx + LINES - 3) top_idx++;
                draw();
            }
            if (ch == KEY_UP && sel > 0) {
                sel--;
                if (sel < top_idx) top_idx--;
                draw();
            }
            if (ch == '\n') {
                set_wallpaper();
            }
            if (ch == 'v') {
                show();
                draw();
            }
            if (ch == 'k') {
                kill_wallpaper_processes();
                mvprintw(LINES-1, 0, "Wallpaper killed");
                clrtoeol();
                refresh();
            }
        }
        if (ch == KEY_F(1)) {
            change_config();
            draw();
        }
    }

    endwin();
    return 0;
}

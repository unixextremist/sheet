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
static char list[MAX][4096];
static int n,sel,top;
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
        for (int i = top; i < n && i < top + max_display; i++) {
            char *filename = strrchr(list[i], '/') + 1;
            mvprintw(i - top + 2, 0, "%s %s", i == sel ? ">" : " ", filename);
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
}

static void set_wallpaper() {
    if (n == 0) return;

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
                char *args[] = {"feh", "--bg-scale", list[sel], NULL};
                execvp("feh", args);
            } else {
                char *args[] = {"swaybg", "-m", "fill", "-i", list[sel], NULL};
                execvp("swaybg", args);
            }
            perror("execvp");
            exit(1);
        } else {
            exit(0);
        }
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
        mvprintw(LINES-1, 0, "Wallpaper set! (daemonized)");
        clrtoeol();
        refresh();
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
    printf("Enter new setter (feh or swaybg): ");
    fflush(stdout);
    char new_setter[256];
    if (fgets(new_setter, sizeof(new_setter), stdin)) {
        new_setter[strcspn(new_setter, "\n")] = 0;
        if (strlen(new_setter) > 0 && (strcmp(new_setter, "feh") == 0 || strcmp(new_setter, "swaybg") == 0))
            strcpy(wallsetter, new_setter);
    }

    save_config();
    n = scan(current_dir);
    sel = 0;
    top = 0;

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

    printf("\nChoose wallpaper setter (1 for feh, 2 for swaybg): ");
    fflush(stdout);
    fgets(choice, sizeof(choice), stdin);
    if (choice[0] == '2')
        strcpy(wallsetter, "swaybg");
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

int main(int argc, char **argv) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    load_config();

    if (argc > 1) {
        strcpy(current_dir, argv[1]);
        expand_path(current_dir);
        save_config();
    } else if (first_time) {
        first_time_setup();
    }

    n = scan(current_dir);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    sel = 0;
    top = 0;
    draw();

    int ch;
    while ((ch = getch()) != 'q') {
        if (n > 0) {
            if (ch == KEY_DOWN && sel + 1 < n) {
                sel++;
                if (sel >= top + LINES - 3) top++;
                draw();
            }
            if (ch == KEY_UP && sel > 0) {
                sel--;
                if (sel < top) top--;
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

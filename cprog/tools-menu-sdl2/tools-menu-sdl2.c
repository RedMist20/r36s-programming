#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define MAX_OUTPUT 65536
#define MAX_INPUT 256
#define FONT_SIZE 15
#define LINE_H 18

typedef struct {
    const char *name;
    char command[512];
    bool editable;
} Action;

enum {
    ACTION_WIFI_SCAN = 0,
    ACTION_TOGGLE_MONITOR,
    ACTION_SET_MANAGED,
    ACTION_SET_MONITOR,
    ACTION_WLAN_DOWN,
    ACTION_WLAN_UP,
    ACTION_SHOW_IW_DEV,
    ACTION_SET_CHANNEL,
    ACTION_SHOW_TXPOWER,
    ACTION_SET_TXPOWER,
    ACTION_ADAPTER_DRIVER_INFO,
    ACTION_RESTART_NETWORK,
    ACTION_DMESG_LAST50,
    ACTION_CHECK_RTL8812AU,
    ACTION_MDK4_INSTALLED,
    ACTION_MDK4_HELP,
    ACTION_EXIT
};

static const char *font_candidates[] = {
    "/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
    NULL
};

static char output_buf[MAX_OUTPUT];
static char terminal_input[MAX_INPUT];
static int selected = 0;
static int channel_value = 6;
static int txpower_value = 1000;
static bool input_mode = false;

static Action actions[] = {
    {"WiFi Scan (SSID list)", "ip link set wlan0 up 2>/dev/null; iw dev wlan0 scan 2>/dev/null | awk '/SSID: /{ssid=substr($0,7)} /freq: /{freq=$2} /signal: /{sig=$2\" \"$3} /last seen: /{if(ssid==\"\") ssid=\"(hidden)\"; printf \"SSID: %-30s Freq: %-6s Signal: %s\\n\", ssid, freq, sig; ssid=\"\"; freq=\"\"; sig=\"\"}'", false},
    {"Toggle Monitor Mode (wlan0)", "CUR=$(iw dev wlan0 info 2>/dev/null | awk '/type/ {print $2; exit}'); ip link set wlan0 down 2>/dev/null; if [ \"$CUR\" = \"monitor\" ]; then iw dev wlan0 set type managed 2>/dev/null || iw dev wlan0 set type station 2>/dev/null; else iw dev wlan0 set monitor none 2>/dev/null || iw dev wlan0 set type monitor 2>/dev/null; fi; ip link set wlan0 up 2>/dev/null; iw dev wlan0 info 2>/dev/null", false},
    {"Set wlan0 MANAGED", "ip link set wlan0 down 2>/dev/null; iw dev wlan0 set type managed 2>/dev/null || iw dev wlan0 set type station 2>/dev/null; ip link set wlan0 up 2>/dev/null; iw dev wlan0 info 2>/dev/null", false},
    {"Set wlan0 MONITOR", "ip link set wlan0 down 2>/dev/null; iw dev wlan0 set monitor none 2>/dev/null || iw dev wlan0 set type monitor 2>/dev/null; ip link set wlan0 up 2>/dev/null; iw dev wlan0 info 2>/dev/null", false},
    {"wlan0 DOWN", "ip link set wlan0 down 2>/dev/null; ip -br link wlan0 2>/dev/null", false},
    {"wlan0 UP", "ip link set wlan0 up 2>/dev/null; ip -br link wlan0 2>/dev/null", false},
    {"Show: iw dev", "iw dev 2>/dev/null", false},
    {"Set wlan0 channel", "", true},
    {"Show wlan0 txpower", "iwlist wlan0 txpower 2>/dev/null", false},
    {"Set wlan0 txpower fixed", "", true},
    {"Show Adapter + Driver Info", "echo Kernel: $(uname -r); echo; lsusb 2>/dev/null; echo; ip -br link 2>/dev/null; echo; ip -br addr 2>/dev/null; echo; lsmod 2>/dev/null | egrep -i '8812|88xx|rtl|cfg80211|mac80211'; echo; dmesg 2>/dev/null | egrep -i '8812|rtl|cfg80211|mac80211' | tail -n 50", false},
    {"Restart Network", "(systemctl restart NetworkManager 2>/dev/null || systemctl restart networking 2>/dev/null || service network-manager restart 2>/dev/null || service networking restart 2>/dev/null || true); ip link set wlan0 down 2>/dev/null; sleep 1; ip link set wlan0 up 2>/dev/null; ip -br addr 2>/dev/null", false},
    {"Log dmesg last 50", "dmesg 2>/dev/null | tail -n 50", false},
    {"Check rtl8812au module", "lsmod 2>/dev/null | grep -i rtl8812au || echo '(not loaded)'; echo; modinfo rtl8812au 2>/dev/null || echo 'module not found'; echo; find /lib/modules/$(uname -r) -type f \\( -name '*8812au*.ko' -o -name '*8812*.ko' \\) 2>/dev/null | head -n 50", false},
    {"MDK4 status + version", "if command -v mdk4 >/dev/null 2>&1; then echo 'mdk4 detected:'; command -v mdk4; echo; mdk4 --version 2>&1 || true; else echo 'mdk4 is not installed.'; fi", false},
    {"MDK4 help (safe read-only)", "if command -v mdk4 >/dev/null 2>&1; then echo 'Read-only mdk4 help output:'; echo; mdk4 --help 2>&1; else echo 'mdk4 is not installed.'; fi", false},
    {"Exit program", "", false}
};

static const int action_count = sizeof(actions) / sizeof(actions[0]);

static void append_text(const char *text)
{
    strncat(output_buf, text, MAX_OUTPUT - strlen(output_buf) - 1);
}

static void set_output_header(const char *title)
{
    output_buf[0] = '\0';
    append_text("=== ");
    append_text(title);
    append_text(" ===\n");
}

static void execute_shell_command(const char *title, const char *shell_cmd)
{
    set_output_header(title);
    FILE *fp = popen(shell_cmd, "r");
    if (!fp) {
        append_text("Failed to launch command.\n");
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        append_text(line);
    }

    int rc = pclose(fp);
    int exit_code = rc;
    if (WIFEXITED(rc)) {
        exit_code = WEXITSTATUS(rc);
    }

    char rc_line[64];
    snprintf(rc_line, sizeof(rc_line), "\n[exit code: %d]\n", exit_code);
    append_text(rc_line);
}

static void run_terminal_input(void)
{
    if (terminal_input[0] == '\0') {
        set_output_header("Terminal Input");
        append_text("No command entered.\n");
        return;
    }

    char shell_cmd[1024];
    snprintf(shell_cmd, sizeof(shell_cmd), "sh -c '%s'", terminal_input);

    char title[320];
    snprintf(title, sizeof(title), "Terminal Input: %s", terminal_input);
    execute_shell_command(title, shell_cmd);
    terminal_input[0] = '\0';
}

static void run_action(int idx)
{
    if (idx == ACTION_EXIT) {
        return;
    }

    char shell_cmd[1024];
    if (idx == ACTION_SET_CHANNEL) {
        snprintf(shell_cmd, sizeof(shell_cmd), "sh -c 'echo Requested channel: %d; iw dev wlan0 set channel %d 2>/dev/null; ip link set wlan0 down 2>/dev/null; iw dev wlan0 set channel %d 2>/dev/null; ip link set wlan0 up 2>/dev/null; iw dev wlan0 info 2>/dev/null'", channel_value, channel_value, channel_value);
    } else if (idx == ACTION_SET_TXPOWER) {
        snprintf(shell_cmd, sizeof(shell_cmd), "sh -c 'echo Requested txpower: %d mBm; ip link set wlan0 down 2>/dev/null; iw dev wlan0 set txpower fixed %dmBm 2>/dev/null; ip link set wlan0 up 2>/dev/null; iwlist wlan0 txpower 2>/dev/null'", txpower_value, txpower_value);
    } else {
        snprintf(shell_cmd, sizeof(shell_cmd), "sh -c '%s'", actions[idx].command);
    }

    execute_shell_command(actions[idx].name, shell_cmd);
}

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, int x, int y, SDL_Color color, const char *text)
{
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void render_ui(SDL_Renderer *renderer, TTF_Font *font)
{
    SDL_SetRenderDrawColor(renderer, 22, 26, 30, 255);
    SDL_RenderClear(renderer);

    SDL_Rect menu_pane = {10, 10, 260, 460};
    SDL_Rect out_pane = {280, 10, 350, 460};

    SDL_SetRenderDrawColor(renderer, 36, 43, 51, 255);
    SDL_RenderFillRect(renderer, &menu_pane);
    SDL_SetRenderDrawColor(renderer, 30, 35, 41, 255);
    SDL_RenderFillRect(renderer, &out_pane);

    SDL_Color white = {230, 230, 230, 255};
    SDL_Color dim = {170, 170, 170, 255};
    SDL_Color sel = {30, 160, 255, 255};
    SDL_Color input_color = {255, 205, 92, 255};

    draw_text(renderer, font, 18, 18, white, "R36S Tools (SDL2)");
    draw_text(renderer, font, 18, 38, dim, "A/B run | X clear | Y input mode");

    int y = 66;
    for (int i = 0; i < action_count; i++) {
        if (!input_mode && i == selected) {
            SDL_Rect hi = {14, y - 2, 252, LINE_H};
            SDL_SetRenderDrawColor(renderer, sel.r, sel.g, sel.b, 120);
            SDL_RenderFillRect(renderer, &hi);
        }

        char label[180];
        if (i == ACTION_SET_CHANNEL) {
            snprintf(label, sizeof(label), "%d. %s [%d]", i + 1, actions[i].name, channel_value);
        } else if (i == ACTION_SET_TXPOWER) {
            snprintf(label, sizeof(label), "%d. %s [%d mBm]", i + 1, actions[i].name, txpower_value);
        } else {
            snprintf(label, sizeof(label), "%d. %s", i + 1, actions[i].name);
        }
        draw_text(renderer, font, 18, y, white, label);
        y += LINE_H;
    }

    int ox = 288;
    int oy = 18;
    draw_text(renderer, font, ox, oy, white, "Terminal Output");
    oy += 24;

    char *copy = strdup(output_buf[0] ? output_buf : "Run a command to view output.");
    if (copy) {
        char *line = strtok(copy, "\n");
        while (line && oy < 420) {
            line[56] = '\0';
            draw_text(renderer, font, ox, oy, dim, line);
            oy += LINE_H;
            line = strtok(NULL, "\n");
        }
        free(copy);
    }

    SDL_Rect input_bar = {286, 430, 338, 34};
    SDL_SetRenderDrawColor(renderer, 20, 22, 26, 255);
    SDL_RenderFillRect(renderer, &input_bar);

    char prompt[320];
    snprintf(prompt, sizeof(prompt), "> %s%s", terminal_input, input_mode ? "_" : "");
    draw_text(renderer, font, 292, 438, input_mode ? input_color : white, prompt);

    SDL_RenderPresent(renderer);
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("R36S Tools SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!window || !renderer) {
        fprintf(stderr, "Window/renderer failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font *font = NULL;
    for (int i = 0; font_candidates[i]; i++) {
        font = TTF_OpenFont(font_candidates[i], FONT_SIZE);
        if (font) break;
    }
    if (!font) {
        fprintf(stderr, "No font found for SDL_ttf.\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

    set_output_header("Ready");
    append_text("Press Y to toggle input mode. Type command and press Enter/A/B.\n");
    SDL_StartTextInput();

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_TEXTINPUT && input_mode) {
                if (strlen(terminal_input) + strlen(e.text.text) < (size_t)(MAX_INPUT - 1)) {
                    strcat(terminal_input, e.text.text);
                }
            }

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE || k == SDLK_q) running = false;

                if (k == SDLK_y) {
                    input_mode = !input_mode;
                    continue;
                }

                if (k == SDLK_x) {
                    output_buf[0] = '\0';
                    continue;
                }

                if (input_mode) {
                    if (k == SDLK_BACKSPACE) {
                        size_t len = strlen(terminal_input);
                        if (len > 0) terminal_input[len - 1] = '\0';
                    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                        run_terminal_input();
                    }
                    continue;
                }

                if (k == SDLK_UP) selected = (selected - 1 + action_count) % action_count;
                if (k == SDLK_DOWN) selected = (selected + 1) % action_count;
                if (k == SDLK_RETURN || k == SDLK_SPACE) {
                    if (selected == ACTION_EXIT) running = false;
                    else run_action(selected);
                }
                if (k == SDLK_LEFT) {
                    if (selected == ACTION_SET_CHANNEL && channel_value > 1) channel_value--;
                    if (selected == ACTION_SET_TXPOWER && txpower_value > 100) txpower_value -= 100;
                }
                if (k == SDLK_RIGHT) {
                    if (selected == ACTION_SET_CHANNEL && channel_value < 165) channel_value++;
                    if (selected == ACTION_SET_TXPOWER && txpower_value < 3000) txpower_value += 100;
                }
            }

            if (e.type == SDL_JOYBUTTONDOWN) {
                int b = e.jbutton.button;

                if (b == 11) {
                    running = false;
                    continue;
                }

                if (b == 3) {
                    input_mode = !input_mode;
                    continue;
                }

                if (b == 2) {
                    output_buf[0] = '\0';
                    continue;
                }

                if (input_mode) {
                    if (b == 0 || b == 1) run_terminal_input();
                    continue;
                }

                if (b == 8) selected = (selected - 1 + action_count) % action_count;
                if (b == 9) selected = (selected + 1) % action_count;
                if (b == 0 || b == 1) {
                    if (selected == ACTION_EXIT) running = false;
                    else run_action(selected);
                }
                if (b == 10) {
                    if (selected == ACTION_SET_CHANNEL && channel_value > 1) channel_value--;
                    if (selected == ACTION_SET_TXPOWER && txpower_value > 100) txpower_value -= 100;
                }
                if (b == 14) {
                    if (selected == ACTION_SET_CHANNEL && channel_value < 165) channel_value++;
                    if (selected == ACTION_SET_TXPOWER && txpower_value < 3000) txpower_value += 100;
                }
            }

            if (e.type == SDL_JOYAXISMOTION && !input_mode) {
                if (e.jaxis.axis == 1) {
                    if (e.jaxis.value < -20000) selected = (selected - 1 + action_count) % action_count;
                    if (e.jaxis.value > 20000) selected = (selected + 1) % action_count;
                }
                if (e.jaxis.axis == 0 && selected == ACTION_SET_CHANNEL) {
                    if (e.jaxis.value < -22000 && channel_value > 1) channel_value--;
                    if (e.jaxis.value > 22000 && channel_value < 165) channel_value++;
                }
                if (e.jaxis.axis == 2 && selected == ACTION_SET_TXPOWER) {
                    if (e.jaxis.value < -22000 && txpower_value > 100) txpower_value -= 100;
                    if (e.jaxis.value > 22000 && txpower_value < 3000) txpower_value += 100;
                }
            }
        }

        render_ui(renderer, font);
        SDL_Delay(16);
    }

    SDL_StopTextInput();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

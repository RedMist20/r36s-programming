#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define MENU_WIDTH 245
#define MAX_OUTPUT 65536
#define MAX_LINES 2000
#define MAX_INPUT 32
#define FONT_SIZE 16

#define BTN_B 0
#define BTN_A 1
#define BTN_X 2
#define BTN_Y 3
#define BTN_UP 8
#define BTN_DOWN 9
#define BTN_LEFT 10
#define BTN_RIGHT 11

typedef struct {
    const char *title;
    bool needs_numeric_input;
    const char *input_label;
    const char *input_default;
} MenuItem;

static const MenuItem MENU_ITEMS[] = {
    {"WiFi Scan (SSID list)", false, NULL, NULL},
    {"Toggle Monitor Mode", false, NULL, NULL},
    {"Set wlan0 MANAGED", false, NULL, NULL},
    {"Set wlan0 MONITOR", false, NULL, NULL},
    {"wlan0 DOWN", false, NULL, NULL},
    {"wlan0 UP", false, NULL, NULL},
    {"Show: iw dev", false, NULL, NULL},
    {"Set wlan0 channel", true, "Channel", "6"},
    {"Show wlan0 txpower", false, NULL, NULL},
    {"Set wlan0 txpower", true, "mBm", "1000"},
    {"Show Adapter + Driver Info", false, NULL, NULL},
    {"Restart Network", false, NULL, NULL},
    {"Log dmesg last 50", false, NULL, NULL},
    {"Check rtl8812au module", false, NULL, NULL},
    {"Exit program", false, NULL, NULL},
};

static const char *FONT_CANDIDATES[] = {
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    NULL
};

static void append_output(char *output, const char *text) {
    size_t have = strlen(output);
    size_t add = strlen(text);
    if (have + add + 1 >= MAX_OUTPUT) {
        size_t cut = have + add + 1 - MAX_OUTPUT;
        if (cut >= have) {
            output[0] = '\0';
            have = 0;
        } else {
            memmove(output, output + cut, have - cut + 1);
            have = strlen(output);
        }
    }
    strncat(output, text, MAX_OUTPUT - have - 1);
}

static void run_script_to_output(char *output, const char *script) {
    const char *script_path = "/tmp/r36s-tools-menu-sdl2.sh";
    FILE *fpw = fopen(script_path, "w");
    if (!fpw) {
        snprintf(output, MAX_OUTPUT, "Failed to create temporary script file.\n");
        return;
    }

    fputs("#!/bin/sh\n", fpw);
    fputs(script, fpw);
    fclose(fpw);

    char command[256];
    snprintf(command, sizeof(command), "sh %s 2>&1", script_path);

    output[0] = '\0';
    append_output(output, "$ ");
    append_output(output, command);
    append_output(output, "\n\n");

    FILE *fpr = popen(command, "r");
    if (!fpr) {
        append_output(output, "Failed to run command.\n");
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fpr)) {
        append_output(output, line);
    }

    int rc = pclose(fpr);
    char status[64];
    snprintf(status, sizeof(status), "\n[command exit status: %d]\n", rc);
    append_output(output, status);
}

static void run_action(int idx, const char *input_value, char *output) {
    char script[4096];
    const char *value = input_value ? input_value : "";

    switch (idx) {
    case 0:
        snprintf(script, sizeof(script),
                 "echo '=== WiFi Scan (wlan0) ==='\n"
                 "date\n"
                 "echo\n"
                 "ip link show wlan0 >/dev/null 2>&1 || { echo 'No wlan0 found.'; ip -br link 2>/dev/null; exit 0; }\n"
                 "command -v iw >/dev/null 2>&1 || { echo 'iw not installed.'; exit 0; }\n"
                 "ip link set wlan0 up 2>/dev/null || true\n"
                 "iw dev wlan0 scan 2>/dev/null | awk '/SSID: /{ssid=substr($0,7)} /freq: /{freq=$2} /signal: /{sig=$2 \" \" $3} /last seen: /{if(ssid==\"\")ssid=\"(hidden)\"; printf \"SSID: %-30s  Freq: %-6s  Signal: %s\\n\", ssid, freq, sig; ssid=\"\"; freq=\"\"; sig=\"\";}'\n");
        break;
    case 1:
        snprintf(script, sizeof(script),
                 "echo '=== Toggle Monitor Mode ==='\n"
                 "date\n"
                 "echo\n"
                 "ip link show wlan0 >/dev/null 2>&1 || { echo 'No wlan0 found.'; exit 0; }\n"
                 "command -v iw >/dev/null 2>&1 || { echo 'iw not installed.'; exit 0; }\n"
                 "CUR=$(iw dev wlan0 info 2>/dev/null | awk '/type/ {print $2; exit}')\n"
                 "echo \"Current: ${CUR:-unknown}\"\n"
                 "ip link set wlan0 down 2>/dev/null || true\n"
                 "if [ \"$CUR\" = \"monitor\" ]; then iw dev wlan0 set type managed 2>/dev/null || iw dev wlan0 set type station 2>/dev/null || true; else iw dev wlan0 set monitor none 2>/dev/null || iw dev wlan0 set type monitor 2>/dev/null || true; fi\n"
                 "ip link set wlan0 up 2>/dev/null || true\n"
                 "NEW=$(iw dev wlan0 info 2>/dev/null | awk '/type/ {print $2; exit}')\n"
                 "echo \"New: ${NEW:-unknown}\"\n");
        break;
    case 2:
        snprintf(script, sizeof(script), "echo '=== Set MANAGED ==='\n"
                 "date\n"
                 "ip link show wlan0 >/dev/null 2>&1 || { echo 'No wlan0 found.'; exit 0; }\n"
                 "ip link set wlan0 down 2>/dev/null || true\n"
                 "iw dev wlan0 set type managed 2>/dev/null || iw dev wlan0 set type station 2>/dev/null || true\n"
                 "ip link set wlan0 up 2>/dev/null || true\n"
                 "iw dev wlan0 info 2>/dev/null || true\n");
        break;
    case 3:
        snprintf(script, sizeof(script), "echo '=== Set MONITOR ==='\n"
                 "date\n"
                 "ip link show wlan0 >/dev/null 2>&1 || { echo 'No wlan0 found.'; exit 0; }\n"
                 "ip link set wlan0 down 2>/dev/null || true\n"
                 "iw dev wlan0 set monitor none 2>/dev/null || iw dev wlan0 set type monitor 2>/dev/null || true\n"
                 "ip link set wlan0 up 2>/dev/null || true\n"
                 "iw dev wlan0 info 2>/dev/null || true\n");
        break;
    case 4:
        snprintf(script, sizeof(script), "echo '=== wlan0 DOWN ==='\n"
                 "ip link show wlan0 >/dev/null 2>&1 || { echo 'No wlan0 found.'; exit 0; }\n"
                 "ip link set wlan0 down 2>/dev/null || true\n"
                 "ip -br link wlan0 2>/dev/null || true\n");
        break;
    case 5:
        snprintf(script, sizeof(script), "echo '=== wlan0 UP ==='\n"
                 "ip link show wlan0 >/dev/null 2>&1 || { echo 'No wlan0 found.'; exit 0; }\n"
                 "ip link set wlan0 up 2>/dev/null || true\n"
                 "ip -br link wlan0 2>/dev/null || true\n");
        break;
    case 6:
        snprintf(script, sizeof(script), "echo '=== iw dev ==='\n"
                 "command -v iw >/dev/null 2>&1 && iw dev 2>/dev/null || echo 'iw not installed.'\n");
        break;
    case 7:
        snprintf(script, sizeof(script), "echo '=== Set channel to %s ==='\n"
                 "ip link show wlan0 >/dev/null 2>&1 || { echo 'No wlan0 found.'; exit 0; }\n"
                 "command -v iw >/dev/null 2>&1 || { echo 'iw not installed.'; exit 0; }\n"
                 "iw dev wlan0 set channel %s 2>/dev/null || true\n"
                 "ip link set wlan0 down 2>/dev/null || true\n"
                 "iw dev wlan0 set channel %s 2>/dev/null || true\n"
                 "ip link set wlan0 up 2>/dev/null || true\n"
                 "iw dev wlan0 info 2>/dev/null || true\n", value, value, value);
        break;
    case 8:
        snprintf(script, sizeof(script), "echo '=== txpower read ==='\n"
                 "ip link show wlan0 >/dev/null 2>&1 || { echo 'No wlan0 found.'; exit 0; }\n"
                 "command -v iwlist >/dev/null 2>&1 && iwlist wlan0 txpower 2>/dev/null || echo 'iwlist not installed.'\n");
        break;
    case 9:
        snprintf(script, sizeof(script), "echo '=== Set txpower to %smBm ==='\n"
                 "ip link show wlan0 >/dev/null 2>&1 || { echo 'No wlan0 found.'; exit 0; }\n"
                 "command -v iw >/dev/null 2>&1 || { echo 'iw not installed.'; exit 0; }\n"
                 "ip link set wlan0 down 2>/dev/null || true\n"
                 "iw dev wlan0 set txpower fixed %smBm 2>/dev/null || true\n"
                 "ip link set wlan0 up 2>/dev/null || true\n"
                 "command -v iwlist >/dev/null 2>&1 && iwlist wlan0 txpower 2>/dev/null || true\n", value, value);
        break;
    case 10:
        snprintf(script, sizeof(script), "echo '=== Adapter + Driver Info ==='\n"
                 "date\n"
                 "echo \"Kernel: $(uname -r)\"\n"
                 "echo\n"
                 "command -v lsusb >/dev/null 2>&1 && lsusb || echo 'lsusb not installed.'\n"
                 "echo\n"
                 "ip -br link 2>/dev/null || true\n"
                 "echo\n"
                 "ip -br addr 2>/dev/null || true\n"
                 "echo\n"
                 "lsmod 2>/dev/null | egrep -i '8812|88xx|rtl|cfg80211|mac80211' || echo '(none found)'\n"
                 "echo\n"
                 "dmesg 2>/dev/null | egrep -i '8812|rtl|cfg80211|mac80211' | tail -n 50 || true\n");
        break;
    case 11:
        snprintf(script, sizeof(script), "echo '=== Restart Network ==='\n"
                 "DID=0\n"
                 "if command -v systemctl >/dev/null 2>&1; then for svc in NetworkManager networking network; do systemctl restart \"$svc\" 2>/dev/null && echo \"Restarted: $svc\" && DID=1; done; fi\n"
                 "if [ \"$DID\" -eq 0 ] && command -v service >/dev/null 2>&1; then for svc in network-manager networking network; do service \"$svc\" restart 2>/dev/null && echo \"Restarted: $svc\" && DID=1; done; fi\n"
                 "ip link show wlan0 >/dev/null 2>&1 && { ip link set wlan0 down 2>/dev/null || true; sleep 1; ip link set wlan0 up 2>/dev/null || true; }\n"
                 "echo\n"
                 "ip -br link 2>/dev/null || true\n"
                 "echo\n"
                 "ip -br addr 2>/dev/null || true\n");
        break;
    case 12:
        snprintf(script, sizeof(script), "echo '=== dmesg last 50 ==='\n"
                 "dmesg 2>/dev/null | tail -n 50\n");
        break;
    case 13:
        snprintf(script, sizeof(script), "echo '=== rtl8812au module check ==='\n"
                 "lsmod 2>/dev/null | grep -i rtl8812au || echo '(not loaded)'\n"
                 "echo\n"
                 "modinfo rtl8812au 2>/dev/null || echo 'modinfo: module not found'\n"
                 "echo\n"
                 "uname_r=$(uname -r)\n"
                 "find \"/lib/modules/$uname_r\" -type f \( -name '*8812au*.ko' -o -name '*8812*.ko' \) 2>/dev/null | head -n 50\n");
        break;
    default:
        snprintf(script, sizeof(script), "echo 'No action.'\n");
        break;
    }

    run_script_to_output(output, script);
}

static TTF_Font *load_font(void) {
    for (int i = 0; FONT_CANDIDATES[i]; ++i) {
        TTF_Font *f = TTF_OpenFont(FONT_CANDIDATES[i], FONT_SIZE);
        if (f) return f;
    }
    return NULL;
}

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, int x, int y, SDL_Color color, const char *text) {
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, color);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_RenderCopy(renderer, t, NULL, &dst);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

static int split_lines(const char *text, const char **lines, int max_lines) {
    int n = 0;
    lines[n++] = text;
    for (const char *p = text; *p && n < max_lines; ++p) {
        if (*p == '\n' && *(p + 1)) lines[n++] = p + 1;
    }
    return n;
}

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0 || TTF_Init() < 0) return 1;

    SDL_Window *window = SDL_CreateWindow("R36S Tools Menu (SDL2)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    TTF_Font *font = load_font();
    if (!window || !renderer || !font) return 1;
    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

    char output[MAX_OUTPUT] = "Welcome. Use D-Pad Up/Down to pick command, A to run, B to exit.\n";
    int selected = 0, scroll = 0;
    bool running = true, input_mode = false;
    int pending_idx = -1, input_cursor = 0;
    char input_value[MAX_INPUT] = {0};

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (!input_mode) {
                    if (k == SDLK_UP) selected = (selected + 14) % 15;
                    if (k == SDLK_DOWN) selected = (selected + 1) % 15;
                    if (k == SDLK_RETURN) {
                        if (selected == 14) running = false;
                        else if (MENU_ITEMS[selected].needs_numeric_input) {
                            input_mode = true;
                            pending_idx = selected;
                            snprintf(input_value, sizeof(input_value), "%s", MENU_ITEMS[selected].input_default);
                            input_cursor = (int)strlen(input_value) - 1;
                            if (input_cursor < 0) input_cursor = 0;
                        } else {
                            run_action(selected, NULL, output);
                            scroll = 0;
                        }
                    }
                    if (k == SDLK_ESCAPE) running = false;
                }
            }
            if (e.type == SDL_JOYBUTTONDOWN) {
                int b = e.jbutton.button;
                if (!input_mode) {
                    if (b == BTN_UP) selected = (selected + 14) % 15;
                    if (b == BTN_DOWN) selected = (selected + 1) % 15;
                    if (b == BTN_X) scroll++;
                    if (b == BTN_Y && scroll > 0) scroll--;
                    if (b == BTN_B) running = false;
                    if (b == BTN_A) {
                        if (selected == 14) running = false;
                        else if (MENU_ITEMS[selected].needs_numeric_input) {
                            input_mode = true;
                            pending_idx = selected;
                            snprintf(input_value, sizeof(input_value), "%s", MENU_ITEMS[selected].input_default);
                            input_cursor = (int)strlen(input_value) - 1;
                            if (input_cursor < 0) input_cursor = 0;
                        } else {
                            run_action(selected, NULL, output);
                            scroll = 0;
                        }
                    }
                } else {
                    int len = (int)strlen(input_value);
                    if (b == BTN_LEFT && input_cursor > 0) input_cursor--;
                    if (b == BTN_RIGHT && input_cursor < len - 1) input_cursor++;
                    if (b == BTN_UP || b == BTN_DOWN) {
                        if (input_value[input_cursor] < '0' || input_value[input_cursor] > '9') input_value[input_cursor] = '0';
                        input_value[input_cursor] += (b == BTN_UP) ? 1 : -1;
                        if (input_value[input_cursor] > '9') input_value[input_cursor] = '0';
                        if (input_value[input_cursor] < '0') input_value[input_cursor] = '9';
                    }
                    if (b == BTN_A) {
                        run_action(pending_idx, input_value, output);
                        input_mode = false;
                        pending_idx = -1;
                        scroll = 0;
                    }
                    if (b == BTN_B) {
                        input_mode = false;
                        pending_idx = -1;
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 14, 20, 30, 255);
        SDL_RenderClear(renderer);

        SDL_Rect left = {10, 10, MENU_WIDTH, SCREEN_HEIGHT - 20};
        SDL_SetRenderDrawColor(renderer, 30, 38, 50, 255);
        SDL_RenderFillRect(renderer, &left);

        SDL_Color fg = {230, 230, 230, 255};
        draw_text(renderer, font, 18, 18, fg, "Tools Menu");
        for (int i = 0; i < 15; ++i) {
            int y = 45 + i * 24;
            if (i == selected) {
                SDL_Rect hi = {14, y - 1, MENU_WIDTH - 8, 22};
                SDL_SetRenderDrawColor(renderer, 50, 120, 210, 255);
                SDL_RenderFillRect(renderer, &hi);
            }
            draw_text(renderer, font, 18, y, fg, MENU_ITEMS[i].title);
        }

        SDL_Rect right = {MENU_WIDTH + 20, 10, SCREEN_WIDTH - MENU_WIDTH - 30, SCREEN_HEIGHT - 40};
        SDL_SetRenderDrawColor(renderer, 6, 12, 18, 255);
        SDL_RenderFillRect(renderer, &right);
        draw_text(renderer, font, MENU_WIDTH + 28, 18, fg, "Terminal Output");

        const char *lines[MAX_LINES];
        int total = split_lines(output, lines, MAX_LINES);
        int visible = (right.h - 38) / 18;
        int start = total - visible - scroll;
        if (start < 0) start = 0;
        for (int i = 0; i < visible && (start + i) < total; ++i) {
            const char *p = lines[start + i];
            char row[256];
            int c = 0;
            while (*p && *p != '\n' && c < 255) row[c++] = *p++;
            row[c] = '\0';
            draw_text(renderer, font, MENU_WIDTH + 28, 42 + i * 18, fg, row);
        }

        if (input_mode && pending_idx >= 0) {
            SDL_Rect box = {150, 170, 340, 130};
            SDL_SetRenderDrawColor(renderer, 24, 30, 44, 245);
            SDL_RenderFillRect(renderer, &box);
            draw_text(renderer, font, 165, 192, fg, "Numeric Input");
            draw_text(renderer, font, 165, 216, fg, MENU_ITEMS[pending_idx].input_label);
            draw_text(renderer, font, 165, 240, fg, input_value);
            int cx = 165 + input_cursor * 10;
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, cx, 260, cx + 9, 260);
        }

        draw_text(renderer, font, 14, SCREEN_HEIGHT - 22, fg,
                  input_mode ? "Input: Left/Right cursor, Up/Down digit, A run, B cancel" : "Controls: DPad Up/Down select | A run | B exit | X/Y scroll");

        SDL_RenderPresent(renderer);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

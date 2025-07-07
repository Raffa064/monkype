#define NCURSES_WIDECHAR 1
#define _POSIX_C_SOURCE 200809L

#include <locale.h>
#include <ncursesw/ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#define MAX_WORDS 1000000
#define MAX_WORD_LENGTH 40

#define KEY_1 L'1'
#define KEY_2 L'2'
#define KEY_3 L'3'
#define KEY_4 L'4'
#define KEY_SPACE L' '
#define KEY_ESC 27

#define IS_BACKSPACE(key) (key == KEY_BACKSPACE || key == 127 || key == 8)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define colored(color, code)                                                   \
  {                                                                            \
    attron(COLOR_PAIR(color));                                                 \
    code;                                                                      \
    attroff(COLOR_PAIR(color));                                                \
  }

#define c_mvprintw(color, y, x, pattern, ...)                                  \
  colored(color, mvprintw(y, x, pattern, __VA_ARGS__))

#define c_printw(color, pattern, ...)                                          \
  colored(color, printw(pattern, __VA_ARGS__))

#define more_relative(x, y, dx, dy)                                            \
  getyx(stdscr, y, x);                                                         \
  move(y + dy, x + dx);

#define reverse(code)                                                          \
  {                                                                            \
    attron(A_REVERSE);                                                         \
    code;                                                                      \
    attroff(A_REVERSE);                                                        \
  }

typedef struct timespec timespec;

wchar_t DATASET[MAX_WORDS][MAX_WORD_LENGTH];

enum {
  HEADER = 1,
  HEADER_PAUSED,
  GHOST,
  CORRECT,
  INCORRECT,
  MISSED,
  EXTRA,
  CURSOR
};

typedef struct {
  int word_count;
  wchar_t **wordset;
  wchar_t **u_wordset;
  int u_word;
  int u_idx;
} session_data;

typedef struct {
  int correct;
  int incorrect;
  int missed;
  int extra;
  double time;
} stats;

typedef struct {
  int seed;
  int word_count;
  int target_fps;
  char *dataset_path;
  char *export_csv;
  int print_only;
} options;

double get_wpm_stat(stats stats) {
  if (stats.time == 0)
    return 0;

  return (stats.correct * 60.) / (stats.time * 5.);
}

double get_accuracy_stat(stats stats) {
  double stat_sum =
      stats.correct + stats.incorrect + stats.missed + stats.extra;

  if (stat_sum == 0)
    return 0;

  return (stats.correct * 1.) / stat_sum * 100.;
}

double difftime_sec(timespec start, timespec end) {
  time_t sec = (end.tv_sec - start.tv_sec);
  long nsec = (end.tv_nsec - start.tv_nsec);

  return (double)sec + (double)nsec / 1e9;
}

void setup_ncurses() {
  initscr();
  noecho();
  cbreak();
  keypad(stdscr, TRUE);
  curs_set(0);
  nodelay(stdscr, TRUE);
  setlocale(LC_ALL, "");

  start_color();

  init_pair(HEADER, COLOR_BLACK, COLOR_BLUE);
  init_pair(HEADER_PAUSED, COLOR_BLACK, COLOR_WHITE);
  init_pair(GHOST, COLOR_WHITE, COLOR_BLACK);
  init_pair(CORRECT, COLOR_GREEN, COLOR_BLACK);
  init_pair(INCORRECT, COLOR_RED, COLOR_BLACK);
  init_pair(MISSED, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(EXTRA, COLOR_YELLOW, COLOR_BLACK);
  init_pair(CURSOR, COLOR_BLACK, COLOR_WHITE);
}

int load_dataset(char *path) {
  FILE *file = fopen(path, "r");

  if (!file) {
    perror("Failed to open dataset");
    exit(1);
  }

  int i = 0;
  wchar_t word[MAX_WORD_LENGTH];
  while (fgetws(word, sizeof(word) / sizeof(wchar_t), file)) {
    int len = wcscspn(word, L"\n"); // strcspn
    word[len] = L'\0';

    if (len < MAX_WORD_LENGTH)
      wcscpy(DATASET[i++], word);
    else
      wprintf(L"Skipping large word: '%ls'\n", word);

    if (i >= MAX_WORDS) {
      wprintf(L"Exceed word limit, dataset trucated at %d words\n", i);
      break;
    }
  }

  return i;
}

void draw_center_text(int x, int y, int w, wchar_t *text) {
  int len = wcslen(text);
  int ox = (w - len) / 2;

  mvprintw(y, x + ox, "%ls", text);
}

void draw_header(int x, int y, int w, int paused) {
  int color = paused ? HEADER_PAUSED : HEADER;
  wchar_t *title = L"MONKYPE v1.0";

  colored(color, {
    for (int i = 0; i < w; i++)
      mvaddch(y, x + i, L' ');

    draw_center_text(x, y, w, title);
  })
}

// Returns the line which the user is in
int draw_session(const session_data session, int x, int y, int w) {
  // Offsets
  int ox = 0;
  int oy = 0;
  int user_offset = 0;

  for (int i = 0; i < session.word_count; i++) {
    wchar_t *word = session.wordset[i];
    wchar_t *u_word = session.u_wordset[i];

    int len = wcslen(word);
    int u_len = wcslen(u_word);
    int max_len = MAX(len, u_len);

    if (i == session.u_word)
      user_offset = oy;

    // word wrap
    if (ox + max_len >= w) {
      ox = 0;
      oy++;
    }

    for (int j = 0; j <= max_len; j++) {
      wchar_t ch = word[j];
      int color = GHOST;

      int changed_by_used =
          session.u_word > i || (session.u_word == i && session.u_idx > j);

      if (changed_by_used) {
        ch = u_word[j];
        color = u_word[j] == word[j] ? CORRECT : INCORRECT;

        if (u_word[j] == L'\0') {
          ch = word[j];
          color = MISSED; // early spaced
        }
      }

      if (j >= len)
        color = EXTRA; // More chars than expected

      // Check if current char is under cursor
      if (i == session.u_word) {
        int word_cursor = j == session.u_idx;
        int end_cursor = j == len && session.u_idx > len && j >= session.u_idx;

        if (word_cursor ^ end_cursor)
          color = CURSOR;

        if (ch == L'\0')
          ch = ' ';
      }

      c_mvprintw(color, y + oy, x + ox + j, "%lc", ch);
    }

    ox += max_len + 1;
  }

  return user_offset;
}

void draw_info(int x, int y, int show_word, wchar_t *word, int show_fps,
               double dt) {
  move(y, x);

  if (show_word)
    printw("%ls ", word);

  if (show_fps)
    printw("%.2f FPS", 1. / dt);
}

int lg10(int n) {
  int i = 0;

  while (n > 0) {
    n /= 10;
    i++;
  }

  return i;
}

void draw_stats(stats stats, int x, int y, int w) {
  double wpm = get_wpm_stat(stats);
  double accuracy = get_accuracy_stat(stats);

  c_mvprintw(HEADER, y, x, " %s ", "WORDED");

  reverse({
    c_printw(HEADER, " WPM: %.1f ACC: %.1f%% %.1fs ", wpm, accuracy,
             stats.time);

    int n = 3; // spacing
    int len = MAX(3, lg10((double)stats.correct)) +
              MAX(3, lg10((double)stats.incorrect)) +
              MAX(3, lg10((double)stats.missed)) +
              MAX(3, lg10((double)stats.extra)) + n * 4;

    move(y, w - len);

    c_printw(CORRECT, " %.3d ", stats.correct);
    more_relative(x, y, 1, 0);
    c_printw(INCORRECT, " %.3d ", stats.incorrect);
    more_relative(x, y, 1, 0);
    c_printw(MISSED, " %.3d ", stats.missed);
    more_relative(x, y, 1, 0);
    c_printw(EXTRA, " %.3d ", stats.extra);
  })
}

stats run_session(options opt) {
  session_data session;
  session.word_count = opt.word_count;
  session.wordset = malloc(sizeof(wchar_t *) * opt.word_count);
  session.u_wordset = malloc(sizeof(wchar_t *) * opt.word_count);
  session.u_word = 0;
  session.u_idx = 0;

  srand(opt.seed);
  int dataset_length = load_dataset(opt.dataset_path);
  for (int i = 0; i < opt.word_count; i++) {
    session.wordset[i] = DATASET[rand() % dataset_length];
    session.u_wordset[i] = calloc(MAX_WORD_LENGTH, sizeof(wchar_t));
  }

  setup_ncurses();

  stats stats;
  stats.correct = 0;
  stats.incorrect = 0;
  stats.missed = 0;
  stats.extra = 0;
  stats.time = 0;

  timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  // napms(1000/fps)
  double frame_delay = 1000. / opt.target_fps;

  int running = 1;
  int paused = 1;
  int show_word = 0;
  int show_fps = 0;
  int scroll_y = 0;
  while (running) {
    const wchar_t key;
    int ktype = get_wch(&key);
    int is_special_key = ktype == KEY_CODE_YES || key == L'\n';

    wchar_t *target_word = session.wordset[session.u_word];
    wchar_t *u_word = session.u_wordset[session.u_word];

    if (ktype != ERR) {
      int target_len = wcslen(target_word);

      if (key == KEY_ESC)
        paused = 1;
      else if (key == KEY_1)
        running = 0;
      else if (key == KEY_2)
        show_word = !show_word;
      else if (key == KEY_3)
        show_fps = !show_fps;
      else if (key == KEY_4) {
        session.u_word = 0;
        session.u_idx = 0;

        for (int i = 0; i < opt.word_count; i++)
          memset(session.u_wordset[i], 0, MAX_WORD_LENGTH * sizeof(wchar_t));

        stats.correct = 0;
        stats.incorrect = 0;
        stats.missed = 0;
        stats.extra = 0;
        stats.time = 0;

        paused = 1;
      } else {
        if (paused && !is_special_key) {
          paused = 0;
          clock_gettime(CLOCK_MONOTONIC, &start); // reset timer
        }

        if (key == KEY_SPACE) {
          // Count extra/missed chars
          int len_diff = session.u_idx - target_len;
          if (len_diff < 0)
            stats.missed -= len_diff;
          else {
            stats.extra += len_diff;
          }

          // Move to next word
          if (session.u_idx > 0 && session.u_word < opt.word_count - 1) {
            session.u_word++;
            session.u_idx = 0;
          }

        } else if (IS_BACKSPACE(key)) {
          if (session.u_idx >= 0) {
            u_word[session.u_idx] = '\0';
            session.u_idx = MAX(0, session.u_idx - 1);
          }
        } else if (!is_special_key && session.u_idx < MAX_WORD_LENGTH - 1) {
          if (target_word[session.u_idx] == key)
            stats.correct++;
          else
            stats.incorrect++;

          u_word[session.u_idx] = key;
          u_word[++session.u_idx] = L'\0';

          // Check if session was finished
          int is_last = session.u_word == opt.word_count - 1;
          int done = session.u_idx >= target_len;
          int finish = (is_last && done);

          if (finish)
            running = 0;
        }
      }
    }

    // Timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    double dt = difftime_sec(start, end);
    start = end;

    erase();

    int w, h;
    getmaxyx(stdscr, h, w);

    scroll_y = draw_session(session, 1, 2 - scroll_y, w - 2);

    draw_header(0, 0, w, paused);
    draw_info(1, h - 2, show_word, target_word, show_fps, dt);
    draw_stats(stats, 1, h - 1, w);

    if (paused) {
      colored(HEADER, { draw_center_text(0, h - 3, w, L" Paused "); })
    } else {
      stats.time += dt;
    }

    refresh();

    napms(frame_delay);
  }

  for (int i = 0; i < opt.word_count; i++)
    free(session.u_wordset[i]);

  free(session.u_wordset);
  free(session.wordset);

  endwin();

  return stats;
}

options get_cmdline_options(int argc, char **argv) {
  options opt;
  opt.seed = time(NULL);
  opt.word_count = 10;
  opt.target_fps = 60;
  opt.dataset_path = "./words.txt";
  opt.export_csv = NULL;
  opt.print_only = 0;

  for (int i = 1; i < argc; i++) {
    char *cmd = argv[i];

    if (strcmp(cmd, "-s") == 0)
      opt.seed = strtol(argv[++i], NULL, 10);
    else if (strcmp(cmd, "-w") == 0)
      opt.word_count = strtol(argv[++i], NULL, 10);
    else if (strcmp(cmd, "-f") == 0)
      opt.target_fps = strtol(argv[++i], NULL, 10);
    else if (strcmp(cmd, "-d") == 0)
      opt.dataset_path = argv[++i];
    else if (strcmp(cmd, "--csv") == 0)
      opt.export_csv = argv[++i];
    else if (strcmp(cmd, "-p") == 0)
      opt.print_only = 1;
    else {
      if (strcmp(cmd, "-h") != 0)
        printf("Invalid option: '%s'", cmd);

      printf("Monkype v1.0\n\n"
             "usage monkype [ -h] [ -s <seed>] [ -w <word_count> ] [ -f <fps>] "
             "[ -d <dataset_path> ] [ --csv <file> ] [ -p ]\n"
             "\n"
             "\t -h  Show this information dialog\n"
             "\t -s  Set the random generator seed\n"
             "\t -w  Amount of words\n"
             "\t -f  Set target fps\n"
             "\t -d  Location for the dataset df: ./words.txt\n"
             "\t --csv print stats as csv\n"
             "\t -p  Print words only\n");

      exit(0);
    }
  }

  return opt;
}

void print_words(options opt) {
  srand(opt.seed);
  int dataset_length = load_dataset(opt.dataset_path);
  for (int i = 0; i < opt.word_count; i++)
    wprintf(L"%ls ", DATASET[rand() % dataset_length]);

  wprintf(L"\n");
}

void print_results(options opt, stats stats) {
  double wpm = get_wpm_stat(stats);
  double accuracy = get_accuracy_stat(stats);

  if (opt.export_csv) {
    FILE *file = fopen(opt.export_csv, "a");

    if (ftell(file) == 0) {
      printf("Wrinting CSV header into '%s'...\n", opt.export_csv);
      fprintf(file, "timestamp,seed,word_count,wpm,accuracy,time,correct,"
                    "incorrect,missed,extra\n");
    }

    fprintf(file, "%ld,%d,%d,%f,%f,%f,%d,%d,%d,%d\n", time(NULL), opt.seed,
            opt.word_count, wpm, accuracy, stats.time, stats.correct,
            stats.incorrect, stats.missed, stats.extra);

    if (file == stdout)
      fclose(file);
  }

  printf("Monkeype v1.0\n");
  printf("  Seed: %d\n", opt.seed);
  printf("  Words: %d\n", opt.word_count);
  printf("  WPM: %.1f\n", wpm);
  printf("  Accuracy: %.1f%%\n", accuracy);
  printf("  Time: %.1fs\n", stats.time);
  printf("  C/I/M/E: %d/%d/%d/%d\n", stats.correct, stats.incorrect,
         stats.missed, stats.extra);
}

int main(int argc, char **argv) {
  options opt = get_cmdline_options(argc, argv);

  if (opt.print_only) {
    print_words(opt);
    return 0;
  }

  stats stats = run_session(opt);
  print_results(opt, stats);
}

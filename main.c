#include <locale.h>
#include <ncursesw/ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#define MAX_PATH_LENGTH 4096

#define DATA_DIR_PERMISSIONS 0775

#define MAX_WORDS 1000000
#define MAX_WORD_LENGTH 40

#define KEY_1 L'1'
#define KEY_2 L'2'
#define KEY_3 L'3'
#define KEY_4 L'4'
#define KEY_SPACE L' '
#define KEY_ESC 27

#define NULL_BYTE L'\0'

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
  bool forced_quit;
} stats;

typedef struct {
  bool loop;
  int seed;
  bool custom_seed;
  int word_count;
  int target_fps;
  char dataset_name[256];
  char *custom_dataset_path;
  char *output_csv_path;
  bool print_only;
} options;

// This function returns the ~/local/share/monkype directory
// WARN: It also ensures that the directory exists
// WARN: It's heap allocated
char *get_data_dir() {
  char *home = getenv("HOME");

  if (!home) {
    perror("Couldn't get $HOME");
    exit(1);
  }

  char *path = malloc(MAX_PATH_LENGTH);
  strcat(strcpy(path, home), "/.local/share/monkype");

  if (access(path, F_OK) != 0) {
    mkdir(path, DATA_DIR_PERMISSIONS);
  }

  return path;
}

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

void close_ncurses() { endwin(); }

void setup_ncurses() {
  initscr();
  noecho();
  cbreak();
  keypad(stdscr, true);
  curs_set(0);
  nodelay(stdscr, true);
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

  signal(SIGINT, close_ncurses);
  signal(SIGTERM, close_ncurses);
}

int load_dataset(options opt) {
  char *path = opt.custom_dataset_path;

  bool path_should_be_freed = false;
  if (!opt.custom_dataset_path) {
    path = get_data_dir();
    strcat(path, "/datasets/");
    strcat(path, opt.dataset_name);
    strcat(path, ".txt");

    path_should_be_freed = true;
  }

  FILE *file = fopen(path, "r");

  if (!file) {
    printf("Failed to open dataset: %s\n", path);
    exit(1);
  }

  int i = 0;
  wchar_t word[MAX_WORD_LENGTH];
  while (fgetws(word, sizeof(word) / sizeof(wchar_t), file)) {
    int len = wcscspn(word, L"\n"); // strcspn
    word[len] = NULL_BYTE;

    if (len < MAX_WORD_LENGTH)
      wcscpy(DATASET[i++], word);
    else
      wprintf(L"Skipping large word: '%ls'\n", word);

    if (i >= MAX_WORDS) {
      wprintf(L"Exceed word limit, dataset trucated at %d words\n", i);
      break;
    }
  }

  fclose(file);

  if (path_should_be_freed)
    free(path);

  return i;
}

void draw_center_text(int x, int y, int w, wchar_t *text) {
  int len = wcslen(text);
  int ox = (w - len) / 2;

  mvprintw(y, x + ox, "%ls", text);
}

void draw_header(int x, int y, int w, bool paused) {
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

      bool changed_by_used =
          session.u_word > i || (session.u_word == i && session.u_idx > j);

      if (changed_by_used) {
        ch = u_word[j];
        color = u_word[j] == word[j] ? CORRECT : INCORRECT;

        if (u_word[j] == NULL_BYTE) {
          ch = word[j];
          color = MISSED; // early spaced
        }
      }

      if (j >= len)
        color = EXTRA; // More chars than expected

      // Check if current char is under cursor
      if (i == session.u_word) {
        bool word_cursor = j == session.u_idx;
        bool end_cursor = j == len && session.u_idx > len && j >= session.u_idx;

        if (word_cursor ^ end_cursor)
          color = CURSOR;

        if (ch == NULL_BYTE)
          ch = KEY_SPACE;
      }

      c_mvprintw(color, y + oy, x + ox + j, "%lc", ch);
    }

    ox += max_len + 1;
  }

  return user_offset;
}

void draw_info(int x, int y, int show_word, wchar_t *word, bool show_fps,
               double dt) {
  move(y, x);

  if (show_word)
    printw("%ls ", word);

  if (show_fps)
    printw("%.2f FPS", 1. / dt);
}

// Print-size of a number
int p_size(int n) {
  int i = 0;

  while (n > 0) {
    n /= 10;
    i++;
  }

  return i;
}

void draw_stats(int x, int y, int w, stats stats) {
  double wpm = get_wpm_stat(stats);
  double accuracy = get_accuracy_stat(stats);

  c_mvprintw(HEADER, y, x + 1, " %s ", "WORDED"); // mode name

  int _stats[4][2] = {
      {CORRECT, stats.correct},     //
      {INCORRECT, stats.incorrect}, //
      {MISSED, stats.missed},       //
      {EXTRA, stats.extra},
  };

  reverse({
    c_printw(HEADER, " WPM: %.1f ACC: %.1f%% %.1fs ", wpm, accuracy,
             stats.time);

    int min_sb_size = 12; // ".000.." 3 digits per stat
    int spacing = 12;     // ".000.." 3 spaces per stat

    int target_sb_size = p_size(stats.correct) +   //
                         p_size(stats.incorrect) + //
                         p_size(stats.missed) +    //
                         p_size(stats.extra);

    int sb_size = MAX(min_sb_size, target_sb_size) + spacing;

    // Place cursor at the right position
    move(y, w - sb_size);

    for (int i = 0; i < 4; i++) {
      c_printw(_stats[i][0], " %.3d ", _stats[i][1]);
      more_relative(x, y, 1, 0); // gap between stats
    }
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
  int dataset_length = load_dataset(opt);
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
  stats.forced_quit = false;

  timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  // napms(1000/fps)
  double frame_delay = 1000. / opt.target_fps;

  bool running = true;
  bool paused = true;
  bool show_word = false;
  bool show_fps = false;
  int scroll_y = 0;
  while (running) {
    wint_t key;
    int ktype = get_wch(&key);
    bool is_special_key = ktype == KEY_CODE_YES || key == L'\n';

    wchar_t *target_word = session.wordset[session.u_word];
    wchar_t *u_word = session.u_wordset[session.u_word];

    if (ktype != ERR) {
      int target_len = wcslen(target_word);

      if (key == KEY_ESC)
        paused = true;
      else if (key == KEY_1) {
        running = false;
        stats.forced_quit = true;
      } else if (key == KEY_2)
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

        paused = true;
      } else {
        if (paused && !is_special_key) {
          paused = false;
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
            u_word[session.u_idx] = NULL_BYTE;
            session.u_idx = MAX(0, session.u_idx - 1);
          }
        } else if (!is_special_key && session.u_idx < MAX_WORD_LENGTH - 1) {
          if (target_word[session.u_idx] == key)
            stats.correct++;
          else
            stats.incorrect++;

          u_word[session.u_idx] = key;
          u_word[++session.u_idx] = NULL_BYTE;

          // Check if session was finished
          bool is_last = session.u_word == opt.word_count - 1;
          bool done = session.u_idx >= target_len;
          bool finish = (is_last && done);

          if (finish)
            running = false;
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

    int left = 0;
    int top = 0;
    int bottom = h - 1;

    scroll_y = draw_session(session, 1, 2 - scroll_y, w - 2);

    draw_header(left, top, w, paused);
    draw_info(left, bottom - 1, show_word, target_word, show_fps, dt);
    draw_stats(left, bottom, w, stats);

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

  close_ncurses();

  return stats;
}

options get_cmdline_options(int argc, char **argv) {
  options opt;
  opt.loop = false;
  opt.seed = 0;
  opt.custom_seed = false;
  opt.word_count = 10;
  opt.target_fps = 60;
  opt.custom_dataset_path = NULL;
  opt.output_csv_path = strcat(get_data_dir(), "/stats.txt");
  opt.print_only = false;

  char *dot_file_path = strcat(get_data_dir(), "/.dataset");
  char dataset_name[256] = {0};
  if (access(dot_file_path, F_OK) == 0) {
    FILE *dot_file = fopen(dot_file_path, "r");

    fread(dataset_name, 1, sizeof(dataset_name), dot_file);
    dataset_name[strcspn(dataset_name, "\n")] = NULL_BYTE; // remove line breaks
    strcpy(opt.dataset_name, dataset_name);

    fclose(dot_file);
  } else {
    strcpy(dataset_name, "<None>");
  }

  free(dot_file_path);

  for (int i = 1; i < argc; i++) {
    char *cmd = argv[i];

    if (strcmp(cmd, "-l") == 0)
      opt.loop = true;
    else if (strcmp(cmd, "-s") == 0) {
      opt.seed = strtol(argv[++i], NULL, 10);
      opt.custom_seed = true;
    } else if (strcmp(cmd, "-w") == 0)
      opt.word_count = strtol(argv[++i], NULL, 10);
    else if (strcmp(cmd, "-f") == 0)
      opt.target_fps = strtol(argv[++i], NULL, 10);
    else if (strcmp(cmd, "-d") == 0)
      opt.custom_dataset_path = argv[++i];
    else if (strcmp(cmd, "-D") == 0)
      strcpy(opt.dataset_name, argv[++i]);
    else if (strcmp(cmd, "--csv") == 0)
      strcpy(opt.output_csv_path, argv[++i]);
    else if (strcmp(cmd, "-p") == 0)
      opt.print_only = true;
    else {
      if (strcmp(cmd, "-h") != 0)
        printf("Invalid option: '%s'", cmd);

      printf("Monkype v1.0\n"
             "\n"
             "Usage:\n"
             "  monkype [options]\n"
             "\n"
             "Options:\n"
             "  -h              Show this help message and exit\n"
             "  -l              Loop mode (restart after finish)\n"
             "  -s <seed>       Set RNG seed\n"
             "  -w <count>      Number of words in session\n"
             "  -f <fps>        Target frames per second\n"
             "  -d <file>       Use custom dataset from path\n"
             "  -D <dataset>    Use installed dataset (default: %s)\n"
             "  --csv <file>    Export session stats to a CSV file (default: "
             "~/.local/share/monkype/stats.csv)\n"
             "  -p              Print session word list only\n",
             dataset_name);

      exit(0);
    }
  }

  return opt;
}

void print_words(options opt) {
  srand(opt.seed);
  int dataset_length = load_dataset(opt);
  for (int i = 0; i < opt.word_count; i++)
    wprintf(L"%ls ", DATASET[rand() % dataset_length]);

  wprintf(L"\n");
}

void print_results(options opt, stats stats) {
  if (stats.forced_quit) {
    printf("Force quit\n");
    return;
  }

  double wpm = get_wpm_stat(stats);
  double accuracy = get_accuracy_stat(stats);

  if (opt.output_csv_path) {
    FILE *file = fopen(opt.output_csv_path, "a");

    if (ftell(file) == 0) {
      printf("Wrinting CSV header into '%s'...\n", opt.output_csv_path);
      fprintf(file,
              "timestamp,dataset,seed,word_count,wpm,accuracy,time,correct,"
              "incorrect,missed,extra\n");
    }

    char *dataset_name = opt.dataset_name;
    if (opt.custom_dataset_path)
      dataset_name = "custom";

    fprintf(file, "%ld,%s,%d,%d,%f,%f,%f,%d,%d,%d,%d\n", time(NULL),
            dataset_name, opt.seed, opt.word_count, wpm, accuracy, stats.time,
            stats.correct, stats.incorrect, stats.missed, stats.extra);

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

  while (true) {
    if (!opt.custom_seed)
      opt.seed = time(NULL);

    if (opt.print_only) {
      print_words(opt);
      break;
    }

    stats stats = run_session(opt);
    print_results(opt, stats);

    if (!opt.loop || stats.forced_quit)
      break;
  }

  free(opt.output_csv_path);
}

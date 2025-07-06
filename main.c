#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#define MAX_WORDS 451435
#define MAX_WORD_LENGTH 50

#define KEY_1 L'1'
#define KEY_2 L'2'
#define KEY_SPACE L' '
#define KEY_ESC 27

#define MIN(a, b) a < b ? a : b
#define MAX(a, b) a > b ? a : b

typedef struct timespec timespec;

char DATASET[MAX_WORDS][MAX_WORD_LENGTH];

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
  char **wordset;
  char **u_wordset;
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

  int i = 0;
  char word[MAX_WORD_LENGTH];
  while (fgets(word, sizeof(word), file)) {
    int len = strcspn(word, "\n");
    word[len] = '\0';

    if (len < MAX_WORD_LENGTH)
      strcpy(DATASET[i++], word);
    else
      printf("Skipping large word: '%s'\n", word);

    if (i >= MAX_WORDS) {
      printf("Exceed word limit, dataset trucated at %d words\n", i);
      break;
    }
  }

  return i;
}

void draw_center_text(int x, int y, int w, char *text) {
  int len = strlen(text);
  int ox = (w - len) / 2;

  mvprintw(y, x + ox, "%s", text);
}

void draw_header(int x, int y, int w, int paused) {
  int color = paused ? HEADER_PAUSED : HEADER;
  char *title = paused ? "~ Paused ~" : "MONKYPE v1.0";

  attron(COLOR_PAIR(color));

  for (int i = 0; i < w - 1; i++)
    mvaddch(y, x + i, ' ');

  draw_center_text(x, y, w, title);

  attroff(COLOR_PAIR(color));
}

int draw_session(const session_data session, int x, int y, int w) {
  // Offsets
  int ox = 0;
  int oy = 0;
  for (int i = 0; i < session.word_count; i++) {
    char *word = session.wordset[i];
    char *u_word = session.u_wordset[i];

    int len = strlen(word);
    int u_len = strlen(u_word);
    int max_len = MAX(len, u_len);

    // word wrap
    if (ox + max_len >= w) {
      ox = 0;
      oy++;
    }

    for (int j = 0; j < max_len; j++) {
      char ch = word[j];
      int color = GHOST;

      int changed_by_used =
          session.u_word > i || (session.u_word == i && session.u_idx > j);

      if (changed_by_used) {
        ch = u_word[j];
        color = u_word[j] == word[j] ? CORRECT : INCORRECT;

        if (u_word[j] == '\0') {
          ch = word[j];
          color = MISSED; // early spaced
        }
      }

      if (j > len)
        color = EXTRA;

      if (i == session.u_word && j == session.u_idx)
        color = CURSOR;

      attron(COLOR_PAIR(color));
      mvaddch(x + oy, y + ox + j, ch);
      attroff(COLOR_PAIR(color));
    }

    ox += max_len + 1;
  }

  return oy;
}

#define __colored(color, macro)                                                \
  attron(COLOR_PAIR(color));                                                   \
  macro;                                                                       \
  attroff(COLOR_PAIR(color));

#define cmvprintw(color, y, x, pattern, ...)                                   \
  __colored(color, mvprintw(y, x, pattern, __VA_ARGS__));

#define cprintw(color, pattern, ...)                                           \
  __colored(color, printw(pattern, __VA_ARGS__));

#define mvrel(x, y, dx, dy)                                                    \
  getyx(stdscr, y, x);                                                         \
  move(y + dy, x + dx);

void draw_stats(stats stats, int x, int y) {
  double wpm = get_wpm_stat(stats);
  double accuracy = get_accuracy_stat(stats);

  attron(A_REVERSE);
  cmvprintw(HEADER, y, x, "WPM: %.1f ACC: %.1f%% %.1fs ", wpm, accuracy,
            stats.time);
  cprintw(CORRECT, " %.3d ", stats.correct);
  mvrel(x, y, 1, 0);
  cprintw(INCORRECT, " %.3d ", stats.incorrect);
  mvrel(x, y, 1, 0);
  cprintw(MISSED, " %.3d ", stats.missed);
  mvrel(x, y, 1, 0);
  cprintw(EXTRA, " %.3d ", stats.extra);
  attroff(A_REVERSE);
}

stats run_session(char *dataset_path, int seed, int word_count,
                  int target_fps) {
  session_data session;
  session.word_count = word_count;
  session.wordset = malloc(sizeof(char *) * word_count);
  session.u_wordset = malloc(sizeof(char *) * word_count);
  session.u_word = 0;
  session.u_idx = 0;

  srand(seed);
  int dataset_length = load_dataset(dataset_path);
  for (int i = 0; i < word_count; i++) {
    session.wordset[i] = DATASET[rand() % dataset_length];
    session.u_wordset[i] = calloc(MAX_WORD_LENGTH, 1);
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

  wchar_t input, last_input;

  int running = 1;
  int paused = 1;
  int debug = 0;
  while (running) {
    input = getch();
    int just_press = input != last_input;

    if (input != ERR) {
      char *target_word = session.wordset[session.u_word];

      if (input == KEY_1 && just_press)
        running = 0;
      if (input == KEY_2 && just_press)
        debug = !debug;
      else if (input == KEY_ESC && just_press) {
        paused = 1;
      } else {
        if (paused) {
          paused = 0;
          clock_gettime(CLOCK_MONOTONIC, &start); // reset timer
        }

        if (input == KEY_SPACE && just_press) {
          int len_diff = session.u_idx - strlen(target_word);

          if (len_diff >= 0)
            stats.extra += len_diff;
          else
            stats.missed -= len_diff;

          session.u_word++;
          session.u_idx = 0;
        } else if (input == KEY_BACKSPACE || input == 127 || input == 8) {
          if (session.u_idx >= 0) {
            session.u_wordset[session.u_word][session.u_idx] = '\0';
            session.u_idx = MAX(0, session.u_idx - 1);
          }
        } else {
          if (session.u_idx >= strlen(target_word)) {
            stats.incorrect++;
          } else {
            if (target_word[session.u_idx] == input)
              stats.correct++;
            else
              stats.incorrect++;
          }

          if (session.u_idx < MAX_WORD_LENGTH) {
            session.u_wordset[session.u_word][session.u_idx] = input;
            session.u_idx++;
          }
        }

        int is_session_complete = session.u_word == word_count - 1 &&
                                  session.u_idx == strlen(target_word);
        if (session.u_word >= word_count || is_session_complete)
          running = 0;
      }
    }

    erase();

    int w, h;
    getmaxyx(stdscr, h, w);

    draw_header(0, 0, w, paused);
    int rows = draw_session(session, 2, 1, w - 2);
    draw_stats(stats, 1, h - 1);

    // Timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    double dt = difftime_sec(start, end);
    start = end;

    if (debug)
      mvprintw(h - 3, 2, "%.2f FPS", 1 / dt);

    if (paused) {
      int y = MIN(rows + 4, h - 3);
      draw_center_text(0, y, w, " Paused, click any key to return ");
    } else {
      stats.time += dt;
    }

    refresh();

    napms(1000. / target_fps);
  }

  for (int i = 0; i < word_count; i++)
    free(session.u_wordset[i]);

  free(session.u_wordset);
  free(session.wordset);

  endwin();

  return stats;
}

void finish() {}

int main() {
  int seed = time(NULL);
  stats stats = run_session("./words.txt", seed, 10, 60);
  double wpm = get_wpm_stat(stats);
  double accuracy = get_accuracy_stat(stats);

  printf("Results:\n");
  printf("  WPM: %.1f\n", wpm);
  printf("  Accuracy: %.1f%%\n", accuracy);
  printf("  Time: %.1fs\n", stats.time);
  printf("  Corrects: %d\n", stats.correct);
  printf("  Incorrects: %d\n", stats.incorrect);
  printf("  Missed: %d\n", stats.missed);
  printf("  Extra: %d\n", stats.extra);
}

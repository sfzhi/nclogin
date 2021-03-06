/* form.c */
/******************************************************************************/
/* Copyright 2015 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
#include "main.h"
#include "form.h"
#include "auth.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <ncurses.h>
#include <term.h>
#include <form.h>
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <errno.h>
#include <wchar.h> // mbrlen(), wcswidth()
#include <wctype.h> // iswprint()
#include <stdlib.h> // mbstowcs()
#include <unistd.h> // STDOUT_FILENO
/*============================================================================*/
#define MAX_USERNAME_LEN 24
#define MAX_PASSWORD_LEN 32
/*----------------------------------------------------------------------------*/
#define SIZE_H 11
#define SIZE_W 32
/*============================================================================*/
enum {
  fi_LL,
  fi_LI,
  fi_PL,
  fi_PI,
  fi_SB,
  fi_RB,
};
/*----------------------------------------------------------------------------*/
typedef struct {
  WINDOW  *win;
  FORM   *form;
  FIELD *field;
  int curindex;
  bool insmode;
  bool started;
  bool changed;
  struct {
    int screen;
    int window;
    int popout;
    int editor;
    int efocus;
    int button;
    int bfocus;
    int errmsg;
  } attrs;
} dialog_data_t;
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef struct {
  unsigned char chars[MAX_PASSWORD_LEN];
  mbstate_t mbstate;
  unsigned int cpos;
  unsigned int bpos;
  char bytes[256];
} pwedit_data_t;
/*============================================================================*/
static int visible_width(const char *text)
{
  size_t len = strlen(text);
  wchar_t wide[len];
  int width = (ssize_t)mbstowcs(wide, text, len);
  if (width > 0)
    width = wcswidth(wide, width);
  return width;
}
/*============================================================================*/
void nclogin_form_init(void)
{
  if (nclogin_config.wipescreen)
    setupterm(NULL, STDOUT_FILENO, NULL);
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static void clrscr(void)
{
  if (nclogin_config.wipescreen)
  {
    putp(clear_screen);
    fflush(stdout);
  }
}
/*============================================================================*/
static void pwedit_reset(pwedit_data_t *data)
{
  mbrlen(NULL, 0, &data->mbstate);
  data->bpos -= data->chars[data->cpos];
  data->chars[data->cpos] = 0;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static void pwedit_clear(pwedit_data_t *data)
{
  mbrlen(NULL, 0, &data->mbstate);
  data->bpos = data->cpos = 0;
  data->chars[data->cpos] = 0;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static void pwedit_erase(pwedit_data_t *data)
{
  if (data->cpos > 0)
    data->bpos -= data->chars[data->cpos--];
  pwedit_reset(data);
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool pwedit_store(pwedit_data_t *data, char ch)
{
  if (data->bpos < sizeof(data->bytes))
  {
    data->bytes[data->bpos++] = ch;
    data->chars[data->cpos]++;
    return true;
  }
  return false;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool pwedit_input(pwedit_data_t *data, int code)
{
  if ((code >= 0) && (code <= 0xFF) && (data->cpos < MAX_PASSWORD_LEN))
  {
    wchar_t wc;
    char ch = (unsigned char)code;
    switch ((ssize_t)mbrtowc(&wc, &ch, 1, &data->mbstate))
    {
    case -2:
      if (pwedit_store(data, ch))
        break;
      //no break;
    case -1:
      pwedit_reset(data);
      break;
    case 0:
      break;
    case 1:
      if (iswprint(wc) && pwedit_store(data, ch))
      {
        data->chars[++data->cpos] = 0;
        return true;
      }
      break;
    default:;
    }
  }
  return false;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static const char *pwedit_value(pwedit_data_t *data)
{
  pwedit_reset(data);
  if (data->bpos < sizeof(data->bytes))
  {
    data->bytes[data->bpos] = '\0';
    return data->bytes;
  }
  else
    return NULL;
}
/*============================================================================*/
static void resize_form(dialog_data_t *data)
{
  erase();
  wresize(data->win, SIZE_H, SIZE_W);
  mvwin(data->win, (LINES - SIZE_H) / 2, (COLS - SIZE_W) / 2);
  refresh();
}
/*----------------------------------------------------------------------------*/
static void resize_both(dialog_data_t *data)
{
  resize_form(data);
  nodelay(data->win, TRUE);
  int ch = wgetch(data->win);
  if (ch != ERR) ungetch(ch);
  nodelay(data->win, FALSE);
}
/*============================================================================*/
static bool modal_popup(dialog_data_t *data, const char *msg, bool query)
{
  WINDOW *win;
  bool result = false;
  int cstate = curs_set(0);
  size_t len = strlen(msg);
repeat:
  win = newwin(3, SIZE_W - 4, (LINES - 3) / 2, (COLS - SIZE_W + 4) / 2);
  wbkgd(win, data->attrs.errmsg);
  leaveok(win, TRUE);
  keypad(win, TRUE);
  box(win, 0, 0);
  mvwaddstr(win, 1, (SIZE_W - 4 - len) / 2, msg);
  wrefresh(win);

  bkgd(data->attrs.screen);

  int ch;
  bool loop = true;
  do {
    errno = 0;
    switch (ch = wgetch(win))
    {
    case ERR:
      if ((errno != 0) && (errno != EINTR))
        loop = false;
      break;
    case KEY_RESIZE:
      delwin(win);
      resize_both(data);
      goto repeat;
    case KEY_ENTER:
    case '\n':
      if (query)
        break;
      //no break;
    case ' ':
      loop = false;
      break;
    case 'y':
    case 'n':
    case 'Y':
    case 'N':
      if (query)
      {
        result = (ch == 'y') || (ch == 'Y');
        loop = false;
      }
      break;
    default:;
    }
  } while (loop);
  delwin(win);
  if (cstate != ERR)
    curs_set(cstate);
  wredrawln(data->win, 4, 3);
  return result;
}
/*============================================================================*/
static void field_enter(FORM *form)
{
  dialog_data_t *data = form_userptr(form);
  data->field = current_field(form);
  data->curindex = field_index(data->field);
  switch (data->curindex)
  {
  case fi_LI:
  case fi_PI:
    set_field_back(data->field, data->attrs.efocus);
    curs_set(1);
    break;
  case fi_SB:
    set_field_back(data->field, data->attrs.bfocus);
    data->form->curcol = 2;
    break;
  case fi_RB:
    set_field_back(data->field, data->attrs.bfocus);
    data->form->curcol = 3;
    break;
  default:;
  }
  if (data->started)
    data->changed = true;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static void field_leave(FORM *form)
{
  dialog_data_t *data = form_userptr(form);
  switch (data->curindex)
  {
  case fi_LI:
  case fi_PI:
    set_field_back(data->field, data->attrs.editor);
    curs_set(0);
    break;
  case fi_SB:
  case fi_RB:
    set_field_back(data->field, data->attrs.button);
    break;
  default:;
  }
  data->field = NULL;
  data->curindex = -1;
  data->changed = false;
}
/*----------------------------------------------------------------------------*/
static size_t field_value(FIELD *field, char *out, size_t max)
{
  size_t len = 0;
  char *str = field_buffer(field, 0);
  if (str != NULL)
  {
    len = strlen(str);
    while ((len > 0) && (str[len - 1] == ' ')) len--;
    if (len >= max)
      len = max - 1;
    if (len > 0)
      memcpy(out, str, len);
  }
  out[len] = '\0';
  return len;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool check_creds(login_info_t *info, dialog_data_t *data,
  pwedit_data_t *pwedit)
{
  int result = 0;
  char login[256];
  FIELD **fields = form_fields(data->form);
  if (field_value(fields[fi_LI], login, sizeof(login)) > 0)
  {
    const char *passw = pwedit_value(pwedit);
    if (passw != NULL)
      result = nclogin_auth_user(info, login, passw);
  }
  if (result <= 0)
    modal_popup(data, "Authentication failed!", false);
  return result > 0;
}
/*----------------------------------------------------------------------------*/
static void command(dialog_data_t *data, int *result)
{
  if (modal_popup(data, (data->curindex == fi_SB)?
      "Really shutdown? (Y/N)": "Really reboot? (Y/N)", true))
    *result = (data->curindex == fi_SB)? fres_SHUTDOWN: fres_REBOOT;
}
/*----------------------------------------------------------------------------*/
static inline bool input_error(void)
{
  failure("Error while processing form input events: %m\n");
  return true;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static int backspace(dialog_data_t *data)
{
  if ((data->field->dcols == data->field->maxgrow) &&
      (data->form->curcol == data->field->dcols - 1))
  {
    form_driver(data->form, REQ_VALIDATION);
    char *str = field_buffer(data->field, 0);
    if ((str != NULL) && (*str != '\0') && (str[strlen(str) - 1] != ' '))
      return REQ_DEL_CHAR;
  }
  return REQ_DEL_PREV;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static int input_loop(dialog_data_t *data, login_info_t *info)
{
  int result = -1;
  pwedit_data_t pwedit;
  memset(&pwedit, 0, sizeof(pwedit));
  mbrlen(NULL, 0, &pwedit.mbstate);
  data->started = true;
  do {
    errno = 0;
    int ch = wgetch(data->win);
    int code = ERR;
    switch (ch)
    {
    case ERR:
      if ((errno != 0) && (errno != EINTR) && input_error())
        result = fres_FAILURE;
      break;
    case '\x0C': // ^L
      erase();
      redrawwin(data->win);
      pos_form_cursor(data->form);
      refresh();
      break;
    case KEY_RESIZE:
      resize_form(data);
      break;
    case KEY_UP:
      code = REQ_UP_FIELD;
      break;
    case KEY_DOWN:
      code = (data->curindex == fi_PI)? REQ_NEXT_FIELD: REQ_DOWN_FIELD;
      break;
    case '\t':
      code = REQ_NEXT_FIELD;
      break;
    case KEY_BTAB:
      code = REQ_PREV_FIELD;
      break;
    case KEY_BEG:
      code = REQ_FIRST_FIELD;
      break;
    case KEY_ENTER:
    case '\n':
      switch (data->curindex)
      {
      case fi_LI:
        code = REQ_NEXT_FIELD;
        break;
      case fi_PI:
        if (check_creds(info, data, &pwedit))
          result = fres_SUCCESS;
        else
          code = REQ_CLR_FIELD;
        pwedit_clear(&pwedit);
        break;
      case fi_SB:
      case fi_RB:
        command(data, &result);
        break;
      default:;
      }
      break;
    case KEY_EXIT:
    case '\x1B': // Esc
      if (nclogin_config.enablequit)
        result = fres_ABANDON;
      break;
    default:
      switch (data->curindex)
      {
      case fi_LI:
        switch (ch)
        {
        case KEY_BACKSPACE:
        case '\x7F': // Del
        case '\b':
          code = backspace(data);
          break;
        case KEY_DC:
          code = REQ_DEL_CHAR;
          break;
        case KEY_IC:
          data->insmode = !data->insmode;
          code = data->insmode? REQ_INS_MODE: REQ_OVL_MODE;
          break;
        case KEY_LEFT:
          code = REQ_PREV_CHAR;
          break;
        case KEY_RIGHT:
          code = REQ_NEXT_CHAR;
          break;
        case KEY_HOME:
          code = REQ_BEG_LINE;
          break;
        case KEY_END:
          code = REQ_END_LINE;
          break;
        default:
          code = ch;
        }
        break;
      case fi_PI:
        switch (ch)
        {
        case KEY_BACKSPACE:
        case '\x7F': // Del
        case '\b':
          pwedit_erase(&pwedit);
          code = backspace(data);
          break;
        case KEY_DC:
          pwedit_clear(&pwedit);
          code = REQ_CLR_FIELD;
          break;
        case KEY_END:
          code = REQ_END_LINE;
          break;
        case KEY_IC:
        case KEY_HOME:
        case KEY_LEFT:
        case KEY_RIGHT:
          break;
        default:
          code = pwedit_input(&pwedit, ch)? '*': ERR;
        }
        break;
      case fi_SB:
      case fi_RB:
        switch (ch)
        {
        case KEY_LEFT:
          code = REQ_LEFT_FIELD;
          break;
        case KEY_RIGHT:
          code = REQ_RIGHT_FIELD;
          break;
        case ' ':
          command(data, &result);
          break;
        default:;
        }
        break;
      default:;
      }
    }
    if (code != ERR)
      form_driver(data->form, code);
    if (data->changed)
    {
      switch (data->curindex)
      {
      case fi_PI:
        pwedit_reset(&pwedit);
        //no break;
      case fi_LI:
        form_driver(data->form, REQ_END_FIELD);
        break;
      case fi_SB:
      case fi_RB:
        form_driver(data->form, REQ_NEXT_WORD);
        break;
      default:;
      }
      data->changed = false;
    }
  } while (result < 0);
  return result;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
int nclogin_form_main(login_info_t *info)
{
  clrscr(); initscr(); raw(); noecho();

  dialog_data_t data = {
    .win = NULL,
    .form = NULL,
    .field = NULL,
    .curindex = -1,
    .insmode = true,
  };

  if (!nclogin_config.monochrome && has_colors())
  {
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_WHITE, COLOR_BLUE);
    init_pair(3, COLOR_GREEN, COLOR_BLUE);
    init_pair(4, COLOR_BLUE, COLOR_CYAN);
    init_pair(5, COLOR_BLACK, COLOR_CYAN);
    init_pair(6, COLOR_WHITE, COLOR_RED);
    data.attrs.screen = COLOR_PAIR(1)|A_DIM;
    data.attrs.window = COLOR_PAIR(2);
    data.attrs.popout = COLOR_PAIR(3)|A_BOLD;
    data.attrs.editor = COLOR_PAIR(4);
    data.attrs.efocus = COLOR_PAIR(5);
    data.attrs.button = COLOR_PAIR(2)|A_BOLD;
    data.attrs.bfocus = COLOR_PAIR(5);
    data.attrs.errmsg = COLOR_PAIR(6)|A_BOLD;
    if (nclogin_config.restricted)
      data.attrs.button &= ~A_BOLD;
  }
  else
  {
    data.attrs.screen = A_DIM;
    data.attrs.window = A_NORMAL;
    data.attrs.popout = A_NORMAL;
    data.attrs.editor = A_REVERSE;
    data.attrs.efocus = A_REVERSE;
    data.attrs.button = A_NORMAL;
    data.attrs.bfocus = A_REVERSE;
    data.attrs.errmsg = A_REVERSE;
  }
  if (nclogin_config.checkboard)
    data.attrs.screen |= ACS_BOARD;
  if (nclogin_config.background)
    bkgd(data.attrs.screen);
  else
    use_default_colors();
  curs_set(0);

  data.win = newwin(SIZE_H, SIZE_W, (LINES - SIZE_H) / 2, (COLS - SIZE_W) / 2);
  keypad(data.win, TRUE);
  wbkgd(data.win, data.attrs.window);
  box(data.win, 0, 0);
  mvwaddch(data.win, 2, 0, ACS_LTEE);
  whline(data.win, ACS_HLINE, SIZE_W - 2);
  mvwaddch(data.win, 2, SIZE_W - 1, ACS_RTEE);
  int nwidth = visible_width(nclogin_config.name);
  if ((nwidth > 0) && (nwidth <= SIZE_W - 13))
  {
    mvwaddstr(data.win, 1, (SIZE_W - 11 - nwidth) / 2, "Welcome to");
    wattron(data.win, data.attrs.popout);
    mvwaddstr(data.win, 1, (SIZE_W + 11 - nwidth) / 2, nclogin_config.name);
  }
  else
    mvwaddstr(data.win, 1, 12, "Welcome!");

  WINDOW *sub = derwin(data.win, SIZE_H - 4, SIZE_W - 2, 3, 1);

  FIELD *fields[] = {
    [fi_LL] = new_field(1, 6, 1, 2, 0, 0),
    [fi_LI] = new_field(1, 16, 1, 12, 0, 0),
    [fi_PL] = new_field(1, 9, 3, 2, 0, 0),
    [fi_PI] = new_field(1, 16, 3, 12, 0, 0),
    [fi_SB] = new_field(1, 12, 5, 2, 0, 0),
    [fi_RB] = new_field(1, 12, 5, 16, 0, 0),
    NULL
  };

  Field_Options button_field_opts = nclogin_config.restricted?
    O_VISIBLE|O_PUBLIC|O_STATIC: O_VISIBLE|O_ACTIVE|O_PUBLIC|O_STATIC;

  set_field_back(fields[fi_LL], data.attrs.window);
  set_field_opts(fields[fi_LL], O_VISIBLE|O_PUBLIC|O_STATIC);
  set_field_buffer(fields[fi_LL], 0, "Login:");

  set_field_back(fields[fi_LI], data.attrs.editor);
  set_field_opts(fields[fi_LI], O_VISIBLE|O_ACTIVE|O_PUBLIC|O_EDIT|O_BLANK);
  set_max_field(fields[fi_LI], MAX_USERNAME_LEN);

  set_field_back(fields[fi_PL], data.attrs.window);
  set_field_opts(fields[fi_PL], O_VISIBLE|O_PUBLIC|O_STATIC);
  set_field_buffer(fields[fi_PL], 0, "Password:");

  set_field_back(fields[fi_PI], data.attrs.editor);
  set_field_opts(fields[fi_PI], O_VISIBLE|O_ACTIVE|O_PUBLIC|O_EDIT|O_BLANK);
  set_max_field(fields[fi_PI], MAX_PASSWORD_LEN);

  set_field_back(fields[fi_SB], data.attrs.button);
  set_field_opts(fields[fi_SB], button_field_opts);
  set_field_buffer(fields[fi_SB], 0, "[ Shutdown ]");

  set_field_back(fields[fi_RB], data.attrs.button);
  set_field_opts(fields[fi_RB], button_field_opts);
  set_field_buffer(fields[fi_RB], 0, "[  Reboot  ]");

  if (nclogin_config.init != NULL)
    set_field_buffer(fields[fi_LI], 0, nclogin_config.init);

  data.form = new_form(fields);
  set_form_userptr(data.form, &data);
  set_field_init(data.form, field_enter);
  set_field_term(data.form, field_leave);
  set_form_win(data.form, data.win);
  set_form_sub(data.form, sub);
  set_form_opts(data.form, 0);
  post_form(data.form);
  refresh();

  int result = input_loop(&data, info);

  unpost_form(data.form);
  free_form(data.form);
  for (unsigned int i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
    free_field(fields[i]);
  delwin(sub);
  delwin(data.win);
  endwin();
  clrscr();
  return result;
}
/*============================================================================*/

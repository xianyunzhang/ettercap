/*
    WDG -- window widget

    Copyright (C) ALoR

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

    $Id: wdg_panel.c,v 1.2 2003/10/26 09:42:04 alor Exp $
*/

#include <wdg.h>

#include <ncurses.h>
#include <panel.h>
#include <stdarg.h>

#define W(x) panel_window(x)

/* GLOBALS */

struct wdg_window {
   PANEL *win;
   PANEL *sub;
   char *title;
   char align;
};

/* PROTOS */

void wdg_create_panel(struct wdg_object *wo);

static int wdg_panel_destroy(struct wdg_object *wo);
static int wdg_panel_resize(struct wdg_object *wo);
static int wdg_panel_redraw(struct wdg_object *wo);
static int wdg_panel_get_focus(struct wdg_object *wo);
static int wdg_panel_lost_focus(struct wdg_object *wo);
static int wdg_panel_get_msg(struct wdg_object *wo, int key, struct wdg_mouse_event *mouse);

void wdg_panel_set_title(struct wdg_object *wo, char *title, size_t align);
static void wdg_panel_border(struct wdg_object *wo);

void wdg_panel_print(wdg_t *wo, size_t x, size_t y, char *fmt, ...);

/*******************************************/

/* 
 * called to create a window
 */
void wdg_create_panel(struct wdg_object *wo)
{
   /* set the callbacks */
   wo->destroy = wdg_panel_destroy;
   wo->resize = wdg_panel_resize;
   wo->redraw = wdg_panel_redraw;
   wo->get_focus = wdg_panel_get_focus;
   wo->lost_focus = wdg_panel_lost_focus;
   wo->get_msg = wdg_panel_get_msg;

   WDG_SAFE_CALLOC(wo->extend, 1, sizeof(struct wdg_window));
}

/* 
 * called to destroy a window
 */
static int wdg_panel_destroy(struct wdg_object *wo)
{
   WDG_WO_EXT(struct wdg_window, ww);

   /* erase the window */
   werase(W(ww->sub));
   werase(W(ww->win));
   
   /* dealloc the structures */
   delwin(W(ww->sub));
   delwin(W(ww->win));
   del_panel(ww->win);
   del_panel(ww->sub);

   /* update the screen */
   update_panels();
   doupdate();

   WDG_SAFE_FREE(ww->title);

   WDG_SAFE_FREE(wo->extend);

   return WDG_ESUCCESS;
}

/* 
 * called to resize a window
 */
static int wdg_panel_resize(struct wdg_object *wo)
{
   wdg_panel_redraw(wo);

   return WDG_ESUCCESS;
}

/* 
 * called to redraw a window
 */
static int wdg_panel_redraw(struct wdg_object *wo)
{
   WDG_WO_EXT(struct wdg_window, ww);
   size_t c = wdg_get_ncols(wo);
   size_t l = wdg_get_nlines(wo);
   size_t x = wdg_get_begin_x(wo);
   size_t y = wdg_get_begin_y(wo);
 
   /* the window already exist */
   if (ww->win) {
      /* erase the border */
      werase(W(ww->win));
    
      /* XXX - try to keep the window on screen */
      if (c <= 2) c = 3;
      if (l <= 2) l = 3;
      if (x == 0) x = 1;
      if (y == 0) y = 1;

      /* resize the window and draw the new border */
      WDG_MOVE_PANEL(ww->win, y, x);
      WDG_WRESIZE(W(ww->win), l, c);
      replace_panel(ww->win, W(ww->win));
      wdg_panel_border(wo);
      
      /* resize the actual window and touch it */
      WDG_MOVE_PANEL(ww->sub, y + 1, x + 1);
      WDG_WRESIZE(W(ww->sub), l - 2, c - 2);
      replace_panel(ww->sub, W(ww->sub));
      /* set the window color */
      wbkgd(W(ww->sub), COLOR_PAIR(wo->window_color));
      touchwin(W(ww->sub));

   /* the first time we have to allocate the window */
   } else {

      /* create the outher window */
      if ((ww->win = new_panel(newwin(l, c, y, x))) == NULL)
         return -WDG_EFATAL;

      /* draw the borders */
      wdg_panel_border(wo);

      /* create the inner (actual) window */
      if ((ww->sub = new_panel(newwin(l - 2, c - 2, y + 1, x + 1))) == NULL)
         return -WDG_EFATAL;
      
      /* set the window color */
      wbkgd(W(ww->sub), COLOR_PAIR(wo->window_color));
      
      /* initialize the pointer */
      wmove(W(ww->sub), 0, 0);

      scrollok(W(ww->sub), TRUE);

      /* put them ontop of the panel stack */
      top_panel(ww->win);
      top_panel(ww->sub);
   }
   
   /* refresh the screen */   
   update_panels();
   doupdate();
   
   wo->flags |= WDG_OBJ_VISIBLE;

   return WDG_ESUCCESS;
}

/* 
 * called when the window gets the focus
 */
static int wdg_panel_get_focus(struct wdg_object *wo)
{
   /* set the flag */
   wo->flags |= WDG_OBJ_FOCUSED;

   /* redraw the window */
   wdg_panel_redraw(wo);
   
   return WDG_ESUCCESS;
}

/* 
 * called when the window looses the focus
 */
static int wdg_panel_lost_focus(struct wdg_object *wo)
{
   /* set the flag */
   wo->flags &= ~WDG_OBJ_FOCUSED;
   
   /* redraw the window */
   wdg_panel_redraw(wo);
   
   return WDG_ESUCCESS;
}

/* 
 * called by the messages dispatcher when the window is focused
 */
static int wdg_panel_get_msg(struct wdg_object *wo, int key, struct wdg_mouse_event *mouse)
{
   WDG_WO_EXT(struct wdg_window, ww);
   wprintw(W(ww->sub), "WDG WIN: char %d\n", key);
   /* update the screen */
   update_panels();
   doupdate();

   /* is the mouse event witin out edges ? */
   if (WDG_MOUSE_ENCLOSE(W(ww->win), key, mouse)) {
      wdg_set_focus(wo);
      return WDG_ESUCCESS;
   }

   if (key == 'q') {
      wdg_destroy_object(&wo);
      return WDG_ESUCCESS;
   }
   
   return -WDG_ENOTHANDLED;
}

/*
 * set the window's title 
 */
void wdg_panel_set_title(struct wdg_object *wo, char *title, size_t align)
{
   WDG_WO_EXT(struct wdg_window, ww);

   ww->align = align;
   ww->title = strdup(title);
}

static void wdg_panel_border(struct wdg_object *wo)
{
   WDG_WO_EXT(struct wdg_window, ww);
   size_t c = wdg_get_ncols(wo);
      
   /* the object was focused */
   if (wo->flags & WDG_OBJ_FOCUSED) {
      wattron(W(ww->win), A_BOLD);
      wbkgdset(W(ww->win), COLOR_PAIR(wo->focus_color));
      top_panel(ww->win);
      top_panel(ww->sub);
   } else 
      wbkgdset(W(ww->win), COLOR_PAIR(wo->border_color));
   
   /* draw the borders */
   box(W(ww->win), 0, 0);
   
   /* set the border color */
   wbkgdset(W(ww->win), COLOR_PAIR(wo->title_color));

   /* there is a title: print it */
   if (ww->title) {
      switch (ww->align) {
         case WDG_ALIGN_LEFT:
            wmove(W(ww->win), 0, 3);
            break;
         case WDG_ALIGN_CENTER:
            wmove(W(ww->win), 0, (c - strlen(ww->title)) / 2);
            break;
         case WDG_ALIGN_RIGHT:
            wmove(W(ww->win), 0, c - strlen(ww->title) - 3);
            break;
      }
      wprintw(W(ww->win), ww->title);
   }
   
   /* restore the attribute */
   if (wo->flags & WDG_OBJ_FOCUSED)
      wattroff(W(ww->win), A_BOLD);
}

/*
 * print a string in the window
 */
void wdg_panel_print(wdg_t *wo, size_t x, size_t y, char *fmt, ...)
{
   WDG_WO_EXT(struct wdg_window, ww);
   va_list ap;

   /* move the pointer */
   wmove(W(ww->sub), y, x);

   /* print the message */
   va_start(ap, fmt);
   vw_printw(W(ww->sub), fmt, ap);
   va_end(ap);

   update_panels();
   doupdate();
}

/* EOF */

// vim:ts=3:expandtab


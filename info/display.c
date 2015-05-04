/* display.c -- How to display Info windows.
   $Id: display.c 5986 2014-12-25 16:37:09Z gavin $

   Copyright 1993, 1997, 2003, 2004, 2006, 2007, 2008, 2012, 2013, 2014
   Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Originally written by Brian Fox. */

#include "info.h"
#include "display.h"
#include "session.h"
#include "tag.h"
#include "signals.h"

static void free_display (DISPLAY_LINE **display);
static DISPLAY_LINE **make_display (int width, int height);

/* An array of display lines which tell us what is currently visible on
   the display.  */
DISPLAY_LINE **the_display = NULL;

/* Non-zero means do no output. */
int display_inhibited = 0;

/* Initialize THE_DISPLAY to WIDTH and HEIGHT, with nothing in it. */
void
display_initialize_display (int width, int height)
{
  free_display (the_display);
  the_display = make_display (width, height);
  display_clear_display (the_display);
}

/* Clear all of the lines in DISPLAY making the screen blank. */
void
display_clear_display (DISPLAY_LINE **display)
{
  register int i;

  signal_block_winch ();
  for (i = 0; display[i]; i++)
    {
      display[i]->text[0] = '\0';
      display[i]->textlen = 0;
      display[i]->inverse = 0;
    }
  signal_unblock_winch ();
}

/* Non-zero if we didn't completely redisplay a window. */
int display_was_interrupted_p = 0;

/* Update the windows on the display. */
void
display_update_display (void)
{
  register WINDOW *win;

  /* Block window resize signals (SIGWINCH) while accessing the the_display
     object, because the signal handler may reallocate it out from under our
     feet. */
  signal_block_winch ();
  display_was_interrupted_p = 0;

  /* For every window in the list, check contents against the display. */
  for (win = windows; win; win = win->next)
    {
      /* Only re-display visible windows which need updating. */
      if (((win->flags & W_WindowVisible) == 0) ||
          ((win->flags & W_UpdateWindow) == 0) ||
          (win->height == 0))
        continue;

      display_update_one_window (win);
      if (display_was_interrupted_p)
        break;
    }

  /* Always update the echo area. */
  display_update_one_window (the_echo_area);
  signal_unblock_winch ();
}

/* Return the screen column of where to write to screen to update line to
   match A, given that B contains the current state of the line.  *PPOS gets
   the offset into the string A to write from. */
static int
find_diff (const char *a, size_t alen, const char *b, size_t blen, int *ppos)
{
  mbi_iterator_t itra, itrb;
  int i;
  int pos = 0;
  int first_escape = -1;
  int escape_pos = -1;
  
  for (i = 0, mbi_init (itra, a, alen), mbi_init (itrb, b, blen);
       mbi_avail (itra) && mbi_avail (itrb);
       i += wcwidth (itra.cur.wc), mbi_advance (itra), mbi_advance (itrb))
    {
      if (mb_cmp (mbi_cur (itra), mbi_cur (itrb)))
        break;

      if (first_escape == -1 && *mbi_cur_ptr (itra) == '\033')
        {
          first_escape = i;
          escape_pos = pos;
        }
      pos += mb_len (mbi_cur (itra));
    }

  if (mbi_avail (itra) || mbi_avail (itrb))
    {
      if (first_escape != -1)
        {
          *ppos = escape_pos;
          return first_escape;
        }
      else
        {
          /* If there was a difference in the line, and there was an escape
             character, return the position of the escape character, as it could
             start a terminal escape sequence. */
          *ppos = pos;
          return i;
        }
    }

  /* Otherwise, no redrawing is required. */
  return -1;
}

/* Update line PL_NUM of the screen to be PRINTED_LINE, which is PL_BYTES long
   and takes up PL_CHARS columns. */
int
display_node_text (long pl_num, char *printed_line,
                   long pl_bytes, long pl_chars)
{
  DISPLAY_LINE **display = the_display;
  DISPLAY_LINE *entry;

 entry = display[pl_num];

  /* We have the exact line as it should appear on the screen.
     Check to see if this line matches the one already appearing
     on the screen. */

  /* If the window is very small, entry might be NULL. */
  if (entry)
    {
      int i, off;
	      
      /* If the screen line is inversed, or if the entry is marked as
         invalid, then clear the line from the screen first. */
      if (entry->inverse)
	{
	  terminal_goto_xy (0, pl_num);
	  terminal_clear_to_eol ();
	  entry->inverse = 0;
	  entry->text[0] = '\0';
	  entry->textlen = 0;
	}

      i = find_diff (printed_line, pl_bytes,
		     entry->text, strlen (entry->text), &off);

      /* If the lines differed at all, we must do some redrawing. */
      if (i != -1)
	{
	  /* Move to the proper point on the terminal. */
	  terminal_goto_xy (i, pl_num);

	  /* If there is any text to print, print it. */
          terminal_put_text (printed_line + off);
	  
	  /* If the printed text didn't extend all the way to the edge
	     of the screen, and text was appearing between here and the
	     edge of the screen, clear from here to the end of the
	     line. */
	  if ((pl_chars < screenwidth && pl_chars < entry->textlen)
	      || entry->inverse)
	    terminal_clear_to_eol ();
	  
	  fflush (stdout);
	  
	  /* Update the display text buffer. */
	  if (strlen (printed_line) > (unsigned int) screenwidth)
	    /* printed_line[] can include more than screenwidth
	       characters, e.g. if multibyte encoding is used or
	       if we are under -R and there are escape sequences
	       in it.  However, entry->text was allocated (in
	       display_initialize_display) for screenwidth
	       bytes only.  */
	    entry->text = xrealloc (entry->text, strlen (printed_line) + 1);
	  strcpy (entry->text + off, printed_line + off);
	  entry->textlen = pl_chars;
	  
	  /* Lines showing node text are not in inverse.  Only modelines
	     have that distinction. */
	  entry->inverse = 0;
	}
    }

  /* A line has been displayed, and the screen reflects that state.
     If there is typeahead pending, then let that typeahead be read
     now, instead of continuing with the display. */
  if (info_any_buffered_input_p ())
    {
      display_was_interrupted_p = 1;
      return 1;
    }
  return 0;
}


int highlight_searches_p = 0;

/* Given an array MATCHES with regions, and an offset *MATCH_INDEX, decide
   if we are inside a region at offset OFF.  The matches are assumed not
   to overlap and to be in order. */
static void
decide_if_in_match (long off, int *in_match, regmatch_t *matches,
                    size_t match_count, size_t *match_index)
{
  size_t i = *match_index;
  int m = *in_match;

  for (; i < match_count; i++)
    {
      if (matches[i].rm_so > off)
        break;

      m = 1;

      if (matches[i].rm_eo > off)
        break;

      m = 0;
    }

  *match_index = i;
  *in_match = m;
}

/* Print each line in the window into our local buffer, and then
   check the contents of that buffer against the display.  If they
   differ, update the display.
   Return value: number of lines processed.  */
long
display_update_window_1 (WINDOW *win)
{
  char *start = win->node->contents + win->line_starts[win->pagetop];

  struct text_buffer tb_printed_line;     /* Buffer for a printed line. */
  long pl_chars = 0;     /* Number of characters written to printed_line */
  long pl_num = 0;       /* Number of physical lines done so far. */
  mbi_iterator_t iter;

  /* If there are no highlighted regions in a line, we output the line with
     display_node_text, which does some optimization of the redisplay.
     Otherwise, the entire line is output in this function. */
  int match_seen_in_line = 0;

  regmatch_t *matches = 0;
  size_t match_index = 0;
  int in_match = 0; /* If we have highlighting on for a match. */

  if (highlight_searches_p)
    matches = win->matches;

  /* Find first search match after the start of the page, and check whether
     we start inside a match. */
  if (matches)
    {
      match_index = 0;
      decide_if_in_match (win->line_starts[win->pagetop], &in_match,
                          matches, win->match_count, &match_index);
    }

  text_buffer_init (&tb_printed_line);

  if (in_match)
    {
      terminal_begin_standout ();
      match_seen_in_line = 1;
      terminal_goto_xy (0, win->first_row);
    }

  for (mbi_init (iter, start, 
                 win->node->contents + win->node->nodelen - start);
       mbi_avail (iter);
       mbi_advance (iter))
    {
      const char *cur_ptr;
      char *rep;

      size_t pchars = 0; /* Printed chars */
      size_t pbytes = 0; /* Bytes to output. */
      int delim = 0;
      int finish = 0;

      /* Check if we have processed all the lines in the window. */
      if (pl_num == win->height)
        break;

      /* Check if this line of the window is off the screen.  This might happen
         if the screen was resized very small. */
      if (win->first_row + pl_num >= screenheight)
        break;

      rep = printed_representation (&iter, &delim, pl_chars, &pchars, &pbytes);

      cur_ptr = mbi_cur_ptr (iter);

      if (matches && match_index != win->match_count)
        {
          int was_in_match = in_match;
          decide_if_in_match (cur_ptr - win->node->contents,
                              &in_match, matches, win->match_count,
                              &match_index);

          if (was_in_match && !in_match)
            {
              terminal_end_standout ();
            }
          else if (!was_in_match && in_match)
            {
              if (!match_seen_in_line)
                {
                  match_seen_in_line = 1;

                  /* Output the line so far. */
                  terminal_goto_xy (0, win->first_row + pl_num);
                  terminal_write_chars (text_buffer_base (&tb_printed_line),
                                      text_buffer_off (&tb_printed_line));
                }
              terminal_begin_standout ();
            }
        }

      if (delim || pl_chars + pchars >= win->width)
        {
          /* If this character cannot be printed in this line, we have
             found the end of this line as it would appear on the screen. */

          text_buffer_add_char (&tb_printed_line, '\0');

          if (!match_seen_in_line)
            {
              finish = display_node_text (win->first_row + pl_num,
                          text_buffer_base (&tb_printed_line),
                          text_buffer_off (&tb_printed_line) - 1,
                          pl_chars);
            }
          else
            {
              terminal_clear_to_eol ();
              /* Let display_node_text know to clear this entire line. */
              the_display[win->first_row + pl_num]->inverse = 1;
            }

          /* Check if a line continuation character should be displayed.
             Don't print one if printing the last character in this window 
             could possibly cause the screen to scroll. */
          if (!delim && 1 + pl_num + win->first_row < the_screen->height)
            {
              terminal_goto_xy (win->width - 1, win->first_row + pl_num);

              if (!(win->flags & W_NoWrap))
                terminal_put_text ("\\");
              else
                {
                  terminal_put_text ("$");
                  rep = 0; /* Don't display this character. */

                  /* If this window has chosen not to wrap lines, skip to the
                     end of the logical line in the buffer, and start a new
                     line here. */
                  for (; mbi_avail (iter); mbi_advance (iter))
                    if (mb_len (mbi_cur (iter)) == 1
                        && *mbi_cur_ptr (iter) == '\n')
                      break;

                  if (matches)
                    {
                      /* Check if the next line starts in a match. */
                      decide_if_in_match (mbi_cur_ptr (iter) - win->node->contents,
                                          &in_match, matches, win->match_count,
                                          &match_index);
                      if (!in_match)
                        terminal_end_standout ();
                    }
                }
              fflush (stdout);
            }

          /* Set for next line. */
          match_seen_in_line = in_match ? 1 : 0;
          ++pl_num;

          pl_chars = 0;
          text_buffer_reset (&tb_printed_line);

          if (finish)
            break;

          /* Go to the start of the next line if we are outputting in this
             function. */
          if (match_seen_in_line)
            terminal_goto_xy (0, win->first_row + pl_num);
        }

      if (*cur_ptr != '\n' && rep) 
        {
          if (!match_seen_in_line)
            text_buffer_add_string (&tb_printed_line, rep, pbytes);
          else
            terminal_write_chars (rep, pbytes);

          pl_chars += pchars;
          continue;
        }
    }

  /* This would be the very last line of the node. */
  if (pl_chars && !match_seen_in_line)
    {
      text_buffer_add_char (&tb_printed_line, '\0');
      display_node_text (win->first_row + pl_num,
                         text_buffer_base (&tb_printed_line),
                         text_buffer_off (&tb_printed_line),
                         pl_chars);
      pl_num++;
    }

  if (in_match)
    terminal_end_standout ();

  text_buffer_free (&tb_printed_line);
  return pl_num;
}

/* Update one window on the screen. */
void
display_update_one_window (WINDOW *win)
{
  size_t line_index = 0;
  DISPLAY_LINE **display = the_display;

  signal_block_winch ();

  /* If display is inhibited, that counts as an interrupted display. */
  if (display_inhibited)
    {
      display_was_interrupted_p = 1;
      goto funexit;
    }

  /* If the window has no height, quit now.  Strictly speaking, it
     should only be necessary to test if the values are equal to zero, since
     window_new_screen_size should ensure that the window height/width never
     becomes negative, but since historically this has often been the culprit
     for crashes, do our best to be doubly safe.  */
  if (win->height <= 0 || win->width <= 0)
    goto funexit;

  /* If the window's first row doesn't appear in the_screen, then it
     cannot be displayed.  This can happen when the_echo_area is the
     window to be displayed, and the screen has shrunk to less than one
     line. */
  if ((win->first_row < 0) || (win->first_row > the_screen->height))
    goto funexit;

  if (win->node && win->line_starts)
    {
      line_index = display_update_window_1 (win);

      if (display_was_interrupted_p)
	goto funexit;
    }

  /* We have reached the end of the node or the end of the window.  If it
     is the end of the node, then clear the lines of the window from here
     to the end of the window. */
  for (; line_index < win->height; line_index++)
    {
      DISPLAY_LINE *entry = display[win->first_row + line_index];

      /* If this line has text on it, or if we don't know what is on the line,
         clear this line. */
      if (entry && entry->textlen || entry->inverse)
        {
          entry->textlen = 0;
          entry->text[0] = '\0';

          terminal_goto_xy (0, win->first_row + line_index);
          terminal_clear_to_eol ();
          fflush (stdout);

          if (info_any_buffered_input_p ())
            {
              display_was_interrupted_p = 1;
              goto funexit;
            }
        }
    }

  /* Finally, if this window has a modeline it might need to be redisplayed.
     Check the window's modeline against the one in the display, and update
     if necessary. */
  if (!(win->flags & W_InhibitMode))
    {
      window_make_modeline (win);
      line_index = win->first_row + win->height;

      /* This display line must both be in inverse, and have the same
         contents. */
      if ((!display[line_index]->inverse
           || (strcmp (display[line_index]->text, win->modeline) != 0))
          /* Check screen isn't very small. */
          && line_index < the_screen->height)
        {
          terminal_goto_xy (0, line_index);
          terminal_begin_inverse ();
          terminal_put_text (win->modeline);
          terminal_end_inverse ();
          strcpy (display[line_index]->text, win->modeline);
          display[line_index]->inverse = 1;
          display[line_index]->textlen = strlen (win->modeline);
        }
    }

  fflush (stdout);

  /* Okay, this window doesn't need updating anymore. */
  win->flags &= ~W_UpdateWindow;
funexit:
  signal_unblock_winch ();
}

/* Scroll screen lines from START inclusive to END exclusive down
   by AMOUNT lines.  Negative AMOUNT means move them up. */
static void
display_scroll_region (int start, int end, int amount)
{
  int i;
  DISPLAY_LINE *temp;

  /* Do it on the screen. */
  terminal_scroll_region (start, end, amount);

  /* Now do it in the display buffer so our contents match the screen. */
  if (amount > 0)
    {
      for (i = end - 1; i >= start + amount; i--)
        {
          /* Swap rows i and (i - amount). */
          temp = the_display[i];
          the_display[i] = the_display[i - amount];
          the_display[i - amount] = temp;
        }

      /* Clear vacated lines */
      for (i = start; i < start + amount && i < end; i++)
        {
          the_display[i]->text[0] = '\0';
          the_display[i]->textlen = 0;
          the_display[i]->inverse = 0;
        }
    }
  else
    {
      amount *= -1;
      for (i = start; i <= end - 1 - amount; i++)
        {
          /* Swap rows i and (i + amount). */
          temp = the_display[i];
          the_display[i] = the_display[i + amount];
          the_display[i + amount] = temp;
        }

      /* Clear vacated lines */
      for (i = end - 1; i >= end - amount && i >= start; i--)
        {
          the_display[i]->text[0] = '\0';
          the_display[i]->textlen = 0;
          the_display[i]->inverse = 0;
        }
    }
}

/* Scroll the region of the_display starting at START, ending at END, and
   moving the lines AMOUNT lines.  If AMOUNT is less than zero, the lines
   are moved up in the screen, otherwise down.  Actually, it is possible
   for no scrolling to take place in the case that the terminal doesn't
   support it.  This doesn't matter to us. */
void
display_scroll_display (int start, int end, int amount)
{
  register int i, last;
  DISPLAY_LINE *temp;

  /* If this terminal cannot do scrolling, give up now. */
  if (!terminal_can_scroll && !terminal_can_scroll_region)
    return;

  /* If there isn't anything displayed on the screen because it is too
     small, quit now. */
  if (!the_display[0])
    return;

  /* If there is typeahead pending, then don't actually do any scrolling. */
  if (info_any_buffered_input_p ())
    return;

  /* Use scrolling region if we can because it doesn't affect the area
     below the area we want to scroll. */
  if (terminal_can_scroll_region)
    {
      display_scroll_region (start, end, amount);
      return;
    }

  /* Otherwise scroll by deleting and inserting lines. */

  if (amount < 0)
    start -= amount;
  else
    end -= amount;

  /* Do it on the screen. */
  terminal_scroll_terminal (start, end, amount);

  /* Now do it in the display buffer so our contents match the screen. */
  if (amount > 0)
    {
      last = end + amount;

      /* Shift the lines to scroll right into place. */
      for (i = 1; i <= (end - start); i++)
        {
          temp = the_display[last - i];
          the_display[last - i] = the_display[end - i];
          the_display[end - i] = temp;
        }

      /* The lines have been shifted down in the buffer.  Clear all of the
         lines that were vacated. */
      for (i = start; i != (start + amount); i++)
        {
          the_display[i]->text[0] = '\0';
          the_display[i]->textlen = 0;
          the_display[i]->inverse = 0;
        }
    }
  else
    {
      last = start + amount;
      for (i = 0; i < (end - start); i++)
        {
          temp = the_display[last + i];
          the_display[last + i] = the_display[start + i];
          the_display[start + i] = temp;
        }

      /* The lines have been shifted up in the buffer.  Clear all of the
         lines that are left over. */
      for (i = end + amount; i != end; i++)
        {
          the_display[i]->text[0] = '\0';
          the_display[i]->textlen = 0;
          the_display[i]->inverse = 0;
        }
    }
}

/* Try to scroll lines in WINDOW.  OLD_PAGETOP is the pagetop of WINDOW before
   having had its line starts recalculated.  OLD_STARTS is the list of line
   starts that used to appear in this window.  OLD_COUNT is the number of lines
   that appear in the OLD_STARTS array. */
void
display_scroll_line_starts (WINDOW *window, int old_pagetop,
    long *old_starts, int old_count)
{
  register int i, old, new;     /* Indices into the line starts arrays. */
  int last_new, last_old;       /* Index of the last visible line. */
  int old_first, new_first;     /* Index of the first changed line. */
  int unchanged_at_top = 0;
  int already_scrolled = 0;

  /* Locate the first line which was displayed on the old window. */
  old_first = old_pagetop;
  new_first = window->pagetop;

  /* Find the last line currently visible in this window. */
  last_new = window->pagetop + (window->height - 1);
  if (last_new > window->line_count)
    last_new = window->line_count - 1;

  /* Find the last line which used to be currently visible in this window. */
  last_old = old_pagetop + (window->height - 1);
  if (last_old > old_count)
    last_old = old_count - 1;

  for (old = old_first, new = new_first;
       old < last_old && new < last_new;
       old++, new++)
    if (old_starts[old] != window->line_starts[new])
      break;
    else
      unchanged_at_top++;

  /* Loop through the old lines looking for a match in the new lines. */
  for (old = old_first + unchanged_at_top; old < last_old; old++)
    {
      for (new = new_first; new < last_new; new++)
        if (old_starts[old] == window->line_starts[new])
          {
            /* Find the extent of the matching lines. */
            for (i = 0; (old + i) < last_old; i++)
              if (old_starts[old + i] != window->line_starts[new + i])
                break;

            /* Scroll these lines if there are enough of them. */
            {
              int start, end, amount;

              start = (window->first_row
                       + ((old + already_scrolled) - old_pagetop));
              amount = new - (old + already_scrolled);
              end = window->first_row + window->height;

              /* If we are shifting the block of lines down, then the last
                 AMOUNT lines will become invisible.  Thus, don't bother
                 scrolling them. */
              if (amount > 0)
                end -= amount;

              if ((end - start) > 0)
                {
                  display_scroll_display (start, end, amount);

                  /* Some lines have been scrolled.  Simulate the scrolling
                     by offsetting the value of the old index. */
                  old += i;
                  already_scrolled += amount;
                }
            }
          }
    }
}

/* Move the screen cursor to directly over the current character in WINDOW. */
void
display_cursor_at_point (WINDOW *window)
{
  int vpos, hpos;

  vpos = window_line_of_point (window) - window->pagetop + window->first_row;
  hpos = window_get_cursor_column (window);
  terminal_goto_xy (hpos, vpos);
  fflush (stdout);
}

/* **************************************************************** */
/*                                                                  */
/*                   Functions Static to this File                  */
/*                                                                  */
/* **************************************************************** */

/* Make a DISPLAY_LINE ** with width and height. */
static DISPLAY_LINE **
make_display (int width, int height)
{
  register int i;
  DISPLAY_LINE **display;

  display = xmalloc ((1 + height) * sizeof (DISPLAY_LINE *));

  for (i = 0; i < height; i++)
    {
      display[i] = xmalloc (sizeof (DISPLAY_LINE));
      display[i]->text = xmalloc (1 + width);
      display[i]->textlen = 0;
      display[i]->inverse = 0;
    }
  display[i] = NULL;
  return display;
}

/* Free the storage allocated to DISPLAY. */
static void
free_display (DISPLAY_LINE **display)
{
  register int i;
  register DISPLAY_LINE *display_line;

  if (!display)
    return;

  for (i = 0; (display_line = display[i]); i++)
    {
      free (display_line->text);
      free (display_line);
    }
  free (display);
}



/****************************************************************************/
/* MESSAGEBOX                                                               */
/* ------------------------------------------------------------------------ */
/*  PURPOSE : General purpose modal message boxes with a flexible number of */
/*            buttons.                                                      */
/*  CREATED :        1995 Peter Schneider (PS)                              */
/* MODIFIED : 03-JUL-2000 Werner Lehmann (WL)                               */
/*                        - GTK+ port                                       */
/*                        - moved all relevant parts from xv_x.c into this  */
/*                          module)                                         */
/*            29-AUG-2000 WL:                                               */
/*                        - simplified code                                 */
/*                        - beautified appearance                           */
/*            21-SEP-2000 WL                                                */
/*                        - added delete_event handling code (delete_event  */
/*                          occurs on window decoration close button click) */
/****************************************************************************/

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "prefs.h"
#include "utilities.h"
#include "utilities.h"
#include "msg_boxes.h"


static int result = NOTICE_INVALID;
static GtkWidget * messagebox = NULL;
static GMainLoop * loop = NULL;

/****************************************************************************/
/*       NAME : buttonMB_Clicked                                            */
/*    PURPOSE : Common "clicked" handler for all messagebox buttons.        */
/*    CREATED : 29-AUG-2000 WL                                              */
/****************************************************************************/
void buttonMB_Clicked (GtkWidget * widget, gpointer data)
{
  result = GPOINTER_TO_INT (data);
  g_main_quit (loop);
}

/****************************************************************************/
/*       NAME : free_resources                                              */
/*    PURPOSE : Destroy messagebox.                                         */
/*    CREATED : 29-AUG-2000 WL                                              */
/****************************************************************************/
static void free_resources ()
{
  if (messagebox) {
    gtk_object_destroy (GTK_OBJECT (messagebox));
    messagebox = NULL;
  }
  if (loop) {
    g_main_destroy (loop);
    loop = NULL;
  }
}

/****************************************************************************/
/*       NAME : show_messagebox                                             */
/*    PURPOSE : Displays the prepared messagebox and returns the value of   */
/*              the clicked button.                                         */
/*    CREATED : 29-AUG-2000 WL                                              */
/*              19-DEC-2001 UMI: Usage of preferences                       */
/****************************************************************************/
static int show_messagebox ()
{
  g_assert (messagebox);

  while (gtk_events_pending ())
    gtk_main_iteration ();

  if (prefs_get_int ("settings", "ring_bell", FALSE))
    gdk_beep ();
  loop = g_main_new (FALSE);
  gtk_widget_show (messagebox);
  g_main_run (loop);
  free_resources ();

  return result;
}

/****************************************************************************/
/*       NAME : overwrite_and_wait                                          */
/*    PURPOSE : shows confirmation dialog with 4 buttons                    */
/* PARAMETERS : error message (char *)                                      */
/*    RETURNS : NOTICE_YES, NOTICE_NO, NOTICE_OVERWRITE, NOTICE_CANCEL      */
/*    CREATED : 02-MAR-1995 PS                                              */
/*   MODIFIED : 03-JUL-2000 WL: GTK+ port                                   */
/*              19-DEC-2001 UMI: Usage of preferences                       */
/****************************************************************************/
int overwrite_and_wait (const gchar * message)
{
#if 1
  if (!prefs_get_int ("settings", "ask_for_confirmation", FALSE))
    return (NOTICE_YES);
  else {
    
    gint result;
    GtkWidget * dialog
      = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                message);
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            "Yes", NOTICE_YES,
                            "No", NOTICE_NO,
                            "All", NOTICE_OVERWRITE_ALL,
                            "Cancel", NOTICE_CANCEL,
                            NULL);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    result = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
  
    return (result == GTK_RESPONSE_DELETE_EVENT) ? NOTICE_CANCEL
      : result;
  }
#else
  char * buttons [] = {
    "Yes",
    "No",
    "All",
    "Cancel"
  };
  int returns [] = {
    NOTICE_YES,
    NOTICE_NO,
    NOTICE_OVERWRITE_ALL,
    NOTICE_CANCEL
  };
  int defbtn = 0;
  int cnlbtn = 3;

  if (!prefs_get_int ("settings", "ask_for_confirmation", FALSE))
    return (NOTICE_YES);

  /* construct message box */
  free_resources ();
  messagebox = create_messagebox ("Overwrite?", message, 4, buttons, returns,
                                  defbtn, cnlbtn);

  /* show modal dialog */
  return show_messagebox ();
#endif
}

/****************************************************************************/
/*       NAME : error_and_wait                                              */
/*    PURPOSE : shows error message, waits for confirmation,                */
/*              returns NOTICE_YES or NOTICE_NO                             */
/* PARAMETERS : error message (char *)                                      */
/*    CREATED : 02-MAR-1995 PS                                              */
/*   MODIFIED : 23-MAR-2000 WL: GTK+ port                                   */
/*              19-DEC-2001 UMI: Usage of preferences                       */
/****************************************************************************/
int error_and_wait (const gchar * message)
{
#if 1
  if (!prefs_get_int ("settings", "ask_for_confirmation", FALSE))
    return (NOTICE_YES);
  else {

    gint result;
    GtkWidget * dialog
      = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                message);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    result = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
  
    return (result == GTK_RESPONSE_YES) ? NOTICE_YES : NOTICE_NO;
  }
  
#else
  char * buttons [] = {
    "Yes",
    "No"
  };
  int returns [] = {
    NOTICE_YES,
    NOTICE_NO
  };
  int defbtn = 0;
  int cnlbtn = 1;

  if (!prefs_get_int ("settings", "ask_for_confirmation", FALSE))
    return (NOTICE_YES);

  /* construct message box */
  free_resources ();
  messagebox = create_messagebox ("Confirmation", message, 2, buttons, returns,
                                  defbtn, cnlbtn);

  /* show modal dialog */
  return show_messagebox ();
#endif
}

/****************************************************************************/
/*       NAME : error_notifier                                              */
/*    PURPOSE : shows error message, waits for confirmation,                */
/*              returns NOTICE_YES                                          */
/* PARAMETERS : error message (char *)                                      */
/*    CREATED : 02-MAR-1995 PS                                              */
/*   MODIFIED : 23-MAR-2000 WL: GTK+ port                                   */
/****************************************************************************/
int error_notifier (const gchar * message)
{
#if 1
  gint result;
  GtkWidget * dialog
    = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                              GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                              message);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  
  return (result == GTK_RESPONSE_OK) ? NOTICE_YES : NOTICE_NO;
#else
  char * buttons [] = {
    "Ok"
  };
  int returns [] = {
    NOTICE_YES
  };
  int defbtn = 0;
  int cnlbtn = 0;

  /* construct message box */
  free_resources ();
  messagebox = create_messagebox ("Information", message, 1, buttons, returns,
                                  defbtn, cnlbtn);

  /* show modal dialog */
  return show_messagebox ();
#endif
}

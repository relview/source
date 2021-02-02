/*
 * RelationViewport.c
 *
 *  Copyright (C) 2008,2009,2010,2011 Stefan Bolus, University of Kiel, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "Relview.h"
#include "RelationViewport.h"
#include "Relation.h"
#include "Graph.h"
#include "utilities.h"
#include "label.h"
#include "msg_boxes.h"
#include "input_string_gtk.h"
#include "DirWindow.h"
#include "prefs.h"
#include "LabelChooserDialog.h"
#include "GraphWindow.h"
#include "RelationWindow.h"
#include "RelationProxyAdapter.h"

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct _RelationViewport
{
	Relview * rv;

	RelviewObserver rvObserver;
  RelationObserver relObserver;

  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *drawingarea;
  GtkAdjustment *horzAdjust, *vertAdjust;
  GtkWidget *horzScroll, *vertScroll;

  Rel * rel;

  GdkPixmap * backpixmap;
  cairo_t * crOffset;

  int labelmx, labelmy; /* label margins */
  int marginx; /* right & left margin */
  int marginy; /* top & bottom margin */

  int canvasWidth, canvasHeight; /* width and height of the virtual drawing canvas. */

  RelationViewportLineMode lineMode;

  int zoom;

  char * fontFamily;
  gboolean zoomFont;

  /* Statusbar */
  GtkWidget * sbar_labelPosition;
  GtkWidget * sbar_sbZoom; /*!< spin button to change the zoom */
  char sbar_labels [100];
  char * sbar_position;
  char * sbar_entries;

  GtkWidget * popup; /* we are not the owner. */
  RelationViewportPreparePopupFunc popupPrepareFunc;
  gpointer popupUserData;

  /* Labels */
  Label * rowLabel, *colLabel; //!< may be null.
  LabelObserver labelObserver;

  /* Pixmap and corresponding mask to draw a filled cell. */
  GdkPixmap * cellPixmap;
  GdkBitmap * cellMask;

  /* Number of cells, that are set. */
  mpz_t relEntryCount;

  /* Whether to show the caption (i.e. relation's name and dimension) in the
   * \ref RelationViewport itself. */
  gboolean drawCaption;

  /* TRUE, if the relation dimension exceeds integer limits. */
  gboolean tooBig;

  /* TRUE, if the window must be drawn before it's drawn to
   * the screen. */
  gboolean needRedraw;
};

#define MINMARGIN 20  /* minimum border on all sides [pixels] */
#define MINDELTA   4  /* minimum grid cell size [pixels] */
#define MINSIZE  200

/* Margin for the caption at the top of the relation viewport drawing area. */
#define CAPTION_MARGIN_TOP 20 /* px */

#define GRIDCELL_FILLMETHOD 0

/* -------------------------------------------------- Function Prototypes --- */

static
gboolean _relation_viewport_on_mouse_motion(GtkWidget * widget,
                                            GdkEventMotion * event,
                                            RelationViewport * self);

static
gboolean _relation_viewport_on_button_press(GtkWidget * widget,
                                            GdkEventButton * event,
                                            RelationViewport * self);
static
gboolean _relation_viewport_on_configure(GtkWidget * widget,
                                         GdkEventConfigure * event,
                                         RelationViewport * self);

static
gboolean _relation_viewport_on_expose(GtkWidget * widget,
                                      GdkEventExpose * event,
                                      RelationViewport * self);

static
void _relation_viewport_on_scroll(GtkWidget * range, RelationViewport * self);

static
void _relation_viewport_on_zoom_change(GtkWidget * widget,
                                       RelationViewport * self);

static
void _relation_viewport_sbar_update_entries(RelationViewport * self);

static
void _relation_viewport_sbar_update_labels(RelationViewport * self);

static
void _relation_viewport_sbar_update_position(RelationViewport * self, int x,
                                             int y);

static
void _relation_viewport_sbar_update(RelationViewport * self);

int
    pixelToGrid(RelationViewport * self, int pix_x, int pix_y, int * x, int * y);
void
    gridToPixel(RelationViewport * self, int x, int y, int * pix_x, int * pix_y);
void drawGridCell(RelationViewport * self, int x, int y, int isSet,
                  int invalidate);

void adjustAdjustments(RelationViewport * self, GtkRange * range, gfloat upper,
                       gfloat page_size);

void put_rel_line_label(RelationViewport * self, const char * label, int line);
void put_rel_col_label(RelationViewport * self, const char * label, int column);

static void _relation_viewport_relation_changed(RelationViewport * self,
                                                Rel * rel);
static void _relation_viewport_relation_on_delete(RelationViewport * self,
                                                  Rel * rel);
static void _relation_viewport_relation_renamed(RelationViewport * self,
                                                Rel * rel);

void relation_viewport_configure(RelationViewport * self);
static void
    _relation_viewport_draw_cannot_be_displayed(RelationViewport * self);
static void draw_matrix(RelationViewport * self, GdkRectangle cliprect);
static void _relation_viewport_redraw_real(RelationViewport * self);
static void _relation_viewport_set_labels (RelationViewport * self,
                                   Label * rowLabel, Label * colLabel);




#define RELATION_VIEWPORT_OBJECT_ATTACH_KEY "RelationViewportObj"

void _relation_viewport_attach(RelationViewport * self, GtkWidget * widget)
{
  g_object_set_data(G_OBJECT(widget), RELATION_VIEWPORT_OBJECT_ATTACH_KEY,
                     (gpointer) self);
}

RelationViewport * relation_viewport_get(GtkWidget * widget)
{
  while (widget) {
    if (GTK_IS_MENU (widget)) {
      widget = gtk_menu_get_attach_widget(GTK_MENU(widget));
      if (!widget) {
        fprintf(stderr,
        "The clicked menu doesn't have an attached widget.\n");
        assert (widget != NULL);
      }
    } else {
      gpointer * pdata = g_object_get_data(G_OBJECT(widget),
      RELATION_VIEWPORT_OBJECT_ATTACH_KEY);

      /* printf ("widget: %p\n", widget );*/

      if (pdata) {
        return (RelationViewport*) pdata;
      }

      widget = gtk_widget_get_parent(widget);
    }
  }

  assert (FALSE);
  return NULL;
}

/* draw a centered message to the relation viewport. */
static void _relation_viewport_draw_message(RelationViewport * self,
                                            gchar * text)
{
  GdkRectangle visrect;

  cairo_t * cr = self->crOffset;
  cairo_text_extents_t te;
  int tx, ty;

  visrect = getVisibleRect(self->drawingarea, self->horzScroll,
                           self->vertScroll);
  visrect.x = visrect.y = 0;

  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, 0);
  cairo_set_font_size(cr, 12);

  cairo_text_extents(cr, text, &te);
  tx = visrect.width / 2 - te.width / 2;
  ty = visrect.height / 2 - te.height / 2;

  cairo_move_to(cr, tx, ty);
  cairo_show_text(cr, text);

  cairo_identity_matrix(cr);
  cairo_new_path(cr);
}

static void _relation_viewport_draw_cannot_be_displayed(RelationViewport * self)
{
  GdkRectangle visrect;

  visrect = getVisibleRect(self->drawingarea, self->horzScroll,
                           self->vertScroll);
  visrect.x = visrect.y = 0;

  /* Clear the backpixmap */
  gdk_draw_rectangle(self->backpixmap, self->drawingarea->style->white_gc,
  TRUE /* filled */, 0, 0, /* x,y */
  visrect.width, visrect.height /* width,height */);

   int relationLimit = prefs_get_int("settings", "relation_display_limit", 2 << 26);
   char errMessage[100];
   sprintf(errMessage,"This relation can not be displayed, because the limit of %i was reached.", relationLimit);

  _relation_viewport_draw_message(self, errMessage);
}

static void _relation_viewport_draw_no_rel_avail(RelationViewport * self)
{
  GdkRectangle visrect;

  visrect = getVisibleRect(self->drawingarea, self->horzScroll,
                           self->vertScroll);
  visrect.x = visrect.y = 0;

  /* Clear the backpixmap */
  gdk_draw_rectangle(self->backpixmap, self->drawingarea->style->white_gc,
  TRUE /* filled */, 0, 0, /* x,y */
  visrect.width, visrect.height /* width,height */);

  _relation_viewport_draw_message(self, "No current relation.");
}

Relview * relation_viewport_get_relview (RelationViewport * self) { return self->rv; }

/* don't forget to invalidate the window after redrawing it.
 * Don't call this method. Use relation_viewport_draw for a
 * deferred redraw. This avoids multiple redraw calls at once. */
void _relation_viewport_redraw_real(RelationViewport * self)
{
  if (self->backpixmap != NULL) {
    if (self->rel != NULL) {
      if (self->tooBig) {
        _relation_viewport_draw_cannot_be_displayed(self);
      } else {
        GdkRectangle visrect;

        visrect = getVisibleRect(self->drawingarea, self->horzScroll,
                                 self->vertScroll);
        visrect.x = visrect.y = 0;

        draw_matrix(self, visrect);
      }
    } else {
      _relation_viewport_draw_no_rel_avail(self);
    }
  } else { ; }
}

/* Forces a redraw of the viewports contents before it's drawn to
 * the screen on the next exposure event. */
void relation_viewport_redraw (RelationViewport * self)
{
  self->needRedraw = TRUE;
}

/* redraw and invalidate the viewport */
void relation_viewport_refresh(RelationViewport * self)
{
  relation_viewport_redraw(self);
  relation_viewport_invalidate_rect(self, NULL);
}

void relation_viewport_set_line_mode(RelationViewport * self,
                                     RelationViewportLineMode lineMode)
{
  self->lineMode = lineMode;
}

static void _foreach_in_line(Rel * rel,
		RelationViewportLineMode lineMode, int x, int y,
		gboolean yesno)
{
	KureRel * impl = rel_get_impl(rel);
	if ( !kure_rel_fits_si(impl)) {
		printf ("RelationViewport::_foreach_in_line: Relation too big.\n");
		return;
	}
	else {

		int i;
		int breite = kure_rel_get_cols_si(impl);
		int hoehe = kure_rel_get_rows_si(impl);

		switch ((int) lineMode) {
		case MODE_DOWN_UP:
			i = 0;
			while ((y - i) >= 0) {
				kure_set_bit_si (impl, yesno, y - i, x);
				i ++;
			}

			i = 0;
			while ((y + i) < hoehe) {
				kure_set_bit_si (impl, yesno, y + i, x);
				i ++;
			}
			break;

		case MODE_LEFT_RIGHT:
			i = 0;
			while ((x - i) >= 0) {
				kure_set_bit_si (impl, yesno, y, x-i);
				i ++;
			}

			i = 0;
			while ((x + i) < breite) {
				kure_set_bit_si (impl, yesno, y, x+i);
				i ++;
			}
			break;

		case MODE_DOWN_RIGHT:
			/* determine start cell */
			if (x < y) {
				y -= x;
				x = 0;
			} else {
				x -= y;
				y = 0;
			}
			/* loop through line */
			for (; (x < breite) && (y < hoehe); x ++, y ++)
				kure_set_bit_si (impl, yesno, y,x);
			break;

		case MODE_DOWN_LEFT:
			/* determine start cell */
			if (breite - x - 1 < y) {
				y -= (breite - x - 1);
				x = breite - 1;
			} else {
				x += y;
				y = 0;
			}
			/* loop through line */
			for (; (x >= 0) && (y < hoehe); x --, y ++)
				kure_set_bit_si (impl, yesno, y,x);
			break;
		}
	}
}

static
gboolean _relation_viewport_on_left_button_press(GtkWidget * widget,
                                                 GdkEventButton * event,
                                                 RelationViewport * self)
{
  int x, y;
  Rel * rel = self->rel;
  int pix_x, pix_y;
  KureRel * impl = rel_get_impl(rel);

  if (rel_allow_display(rel)) {
    if (pixelToGrid(self, event->x, event->y, &x, &y)) {

      gboolean isSet = kure_get_bit_si (impl, y, x, NULL);
      kure_set_bit_si(impl, !isSet, y, x);

      rel_changed(rel);

      gridToPixel(self, x, y, &pix_x, &pix_y);
      drawGridCell(self, pix_x, pix_y, !isSet, TRUE);
      _relation_viewport_sbar_update_entries(self);
    }
  }

  return FALSE;
}

static
gboolean _relation_viewport_on_middle_button_press(GtkWidget * widget,
                                                   GdkEventButton * event,
                                                   RelationViewport * self)
{
  int x, y;
  Rel * rel = self->rel;

  if (rel_allow_display(rel)) {

    if (pixelToGrid(self, event->x, event->y, &x, &y)) {
    	Kure_success success;
    	gboolean complete
			= kure_is_row_complete_si (rel_get_impl(rel), x,y, self->lineMode, &success);

    	if (!success) {
    		KureContext * context = kure_rel_get_context(rel_get_impl(rel));
    		KureError * err = kure_context_get_error(context);
    		g_warning ("_relation_viewport_on_middle_button_press: %s\n",
    				(err && err->message) ? err->message : "Unknown");
    	}
    	else {
		  _foreach_in_line(rel, self->lineMode, x, y, !complete);

		  rel_changed(rel);

		  relation_viewport_redraw(self);
		  relation_viewport_invalidate_rect(self, NULL);
    	}
    }
  }

  return FALSE;
}

static
gboolean _relation_viewport_on_right_button_press(GtkWidget * widget,
                                                  GdkEventButton * event,
                                                  RelationViewport * self)
{
  // Get the user flag, if menu should be displayed
  int windowDisabled = prefs_get_int("settings", "disable_windows_on_limit", 2 << 26);

  // RelationLimit reached, do not display the Menu
 Rel * rel = self->rel;
 if (!rel_allow_display(rel) && windowDisabled)
  return;

  /* Show the popup menu, if there is one. */
  if (self->popup) {
    if (self->popupPrepareFunc) {
      gboolean showPopup = self->popupPrepareFunc(self, self->popup,
                                                  self->popupUserData);

      if (!showPopup)
        return FALSE;
    }

    gtk_menu_popup(GTK_MENU (self->popup), NULL, NULL, NULL, NULL,
    event->button, event->time);
  }

  return FALSE;
}


static
gboolean _relation_viewport_on_button_press(GtkWidget * widget,
                                            GdkEventButton * event,
                                            RelationViewport * self)
{
  /* If there is no current relation in the viewport, ignore mouse events. */
  if (!relation_viewport_has_relation(self))
    return FALSE;

  if (1 == event->button)
    return _relation_viewport_on_left_button_press(widget, event, self);
  else if (2 == event->button)
    _relation_viewport_on_middle_button_press(widget, event, self);
  else if (3 == event->button)
    _relation_viewport_on_right_button_press(widget, event, self);

  return FALSE;
}


static
gboolean _relation_viewport_on_mouse_motion(GtkWidget * widget,
                                            GdkEventMotion * event,
                                            RelationViewport * self)
{
  _relation_viewport_sbar_update_position(self, event->x, event->y);
  return FALSE;
}

void relation_viewport_set_popup(RelationViewport * self, GtkWidget * popup,
                                 RelationViewportPreparePopupFunc f,
                                 gpointer user_data)
{
  if (self->popup != popup) {
    if (popup) {
      self->popupUserData = user_data;
      self->popupPrepareFunc = f;
      self->popup = popup;

      gtk_menu_attach_to_widget(GTK_MENU(self->popup),
      relation_viewport_get_widget (self), NULL);

      gtk_widget_ref(self->popup);
    } else
      self->popup = NULL;
  }
}

#define _DUMP(x,fmt) (#x),(fmt),(x)


void relation_viewport_configure(RelationViewport * self)
{
  GtkAllocation vis_rect;
  int page_width;
  int page_height;
  int breite, colCount;
  int hoehe, rowCount;
  Rel * rel = self->rel;
  int delta = _relation_viewport_get_delta(self);
  gboolean useColLabels, useRowLabels;

  /* vis_rect: The widget's available dimension.
   * canvasWidth and canvasHeight: The dimension of the (complete) relation
   *    with labels and margins.
   * marginx and marginy: The left and top margin. If the canvas is not
   *    bigger than the visible rect., the margin is set to MINMARGIN.
   *    The margin is only used, if the corresponding label is present.
   * labelmx and labelmy: The left and top space ('m' for margin) for
   *    the column and row labels.
   * delta: The cell width (without the border line(s)?).
   * page_width and page_height: Number of rows/columns fit in the visible
   *    rect. */

  /* Calling getVisibleRect requires the scrollbars and yields an
   * orgin we do not care about. */
  vis_rect = GTK_WIDGET(self->drawingarea)->allocation;

  if (!rel) {
#if 0
    adjustAdjustments (self, GTK_RANGE (self->horzScroll),
        breite + (self->marginx / delta), page_width);
    adjustAdjustments (self, GTK_RANGE (self->vertScroll),
        hoehe + (self->marginy / delta), page_height);

    /* unblock configure event handler */
    gtk_signal_handler_unblock_by_func
    (GTK_OBJECT (self->drawingarea),
        GTK_SIGNAL_FUNC (_relation_viewport_on_configure), self);

#endif

    /* Discard backing pixmap and create new one */
    if (self->backpixmap) {
      cairo_destroy(self->crOffset);
      g_object_unref(G_OBJECT(self->backpixmap));
    }

    /* may be NULL, if the window was never visible */
    if (self->drawingarea->window) {
      assert (self->drawingarea->window);
      self->backpixmap
          = gdk_pixmap_new(self->drawingarea->window, vis_rect.width,
                           vis_rect.height, -1 /* depth */);

      self->crOffset = gdk_cairo_create(self->backpixmap);
      cairo_set_source_rgb(self->crOffset, .0, .0, .0); /* black */
      cairo_set_antialias(self->crOffset, CAIRO_ANTIALIAS_NONE);
      cairo_set_line_width(self->crOffset, 1);

      relation_viewport_redraw(self);
      relation_viewport_invalidate_rect(self, NULL);
    }

    return;
  }

  /* Configure for an empty viewport, if the relation is too big. */
  if (self->tooBig) {
    /* Do nothing. */
  } else {
	  KureRel * impl = rel_get_impl (rel);

    /* Discard back (buffer) pixmap and create a new one. We have to create
     * it before any other stuff, because the determination of the label
     * margins depends on an existing cairo handle to request text extents. */
    if (self->backpixmap) {
      cairo_destroy(self->crOffset);
      g_object_unref(G_OBJECT(self->backpixmap));
    }

    /* May be NULL, if the window was never visible */
    if (!self->drawingarea->window)
      return;

    self->backpixmap
        = gdk_pixmap_new(self->drawingarea->window, vis_rect.width,
                         vis_rect.height, -1 /* depth */);
    assert (self->backpixmap != NULL);

    self->crOffset = gdk_cairo_create(self->backpixmap);
    assert (self->crOffset != NULL);
    cairo_set_source_rgb(self->crOffset, .0, .0, .0); /* black */
    cairo_set_antialias(self->crOffset, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_width(self->crOffset, 1);

    colCount = breite = kure_rel_get_cols_si(impl);
    rowCount = hoehe = kure_rel_get_rows_si(impl);

    assert (breite> 0 && hoehe> 0);

    /* Calculate the label margins. */
    self->labelmy = self->labelmx = 0;

    if (self->colLabel || self->rowLabel) {
    	cairo_text_extents_t exts;
    	cairo_text_extents(self->crOffset, "A", &exts);

        /* This is not correct, if column labels are drawn rotated. But it's
         * an upper bound. */

    	if (self->colLabel)
    		self->labelmy = (label_get_max_len (self->colLabel, 1, breite) + 2) * exts.width;
    	if (self->rowLabel)
    		self->labelmx = (label_get_max_len (self->rowLabel, 1, hoehe) + 2) * exts.width;
    }

    useColLabels = (self->labelmy > 0);
    useRowLabels = (self->labelmx > 0);

    /* Calculate the size of the canvas and the margins. Delta (=grid cell width
     and height) is given by the zoom factor.
     The margins (horz+vert) are MINMARGIN by default and can be increased if
     the whole relation fits into the visible rect. This is done to have the
     matrix centered horz/vert. */

    self->marginx = self->marginy = MINMARGIN;

    /* How much space is required to display the complete relation (with
     * labels, when present)? */
    self->canvasWidth = delta * colCount;
    if (useRowLabels)
      self->canvasWidth += self->marginx + self->labelmx;
    self->canvasHeight = delta * rowCount;
    if (useColLabels)
      self->canvasHeight += self->marginx + self->labelmy;

    /* If the complete drawing fits into the visible rect, center it. */
    if (self->canvasWidth < vis_rect.width) {
      self->canvasWidth = vis_rect.width;
      self->marginx = (self->canvasWidth - self->labelmx - (delta * breite))
          / 2;
    }
    if (self->canvasHeight < vis_rect.height) {
      self->canvasHeight = vis_rect.height;
      self->marginy = (self->canvasHeight - self->labelmy - (delta * hoehe))
          / 2;
    }

    /* The adjustment change may emit another configure event => block signal
     handler. */
    gtk_signal_handler_block_by_func
    (GTK_OBJECT (self->drawingarea),
        GTK_SIGNAL_FUNC (_relation_viewport_on_configure), self);

    /* How much rows and columns can be displayed?  page_height and page_width
     * must be calculated precisely with the border lines. Otherwise we get in
     * trouble with the scrollbars. */
    page_width = vis_rect.width;
    if (useRowLabels)
      page_width -= (self->marginx + self->labelmx);
    page_width = (page_width - 1/*left border*/) / (delta + 1);

    page_height = vis_rect.height;
    if (useColLabels)
      page_height -= (self->marginy + self->labelmy);
    page_height = (page_height - 1/*top border*/) / (delta + 1);

    adjustAdjustments(self, GTK_RANGE (self->horzScroll), colCount, page_width);
    adjustAdjustments(self, GTK_RANGE (self->vertScroll), rowCount, page_height);

    /* unblock configure event handler */
    gtk_signal_handler_unblock_by_func
    (GTK_OBJECT (self->drawingarea),
    GTK_SIGNAL_FUNC (_relation_viewport_on_configure), self);

    relation_viewport_refresh (self);

  } /* not too big. */

  _relation_viewport_sbar_update_entries(self);
}

static
gboolean _relation_viewport_on_configure(GtkWidget * widget,
                                         GdkEventConfigure * event,
                                         RelationViewport * self)
{
  relation_viewport_configure(self);

  return TRUE;
}

void relation_viewport_invalidate_rect(RelationViewport * self,
                                       GdkRectangle * rc)
{
  /* emit an expose event to draw the back buffer to the screen. */
  if (rc)
    gtk_widget_queue_draw_area(self->drawingarea, rc->x, rc->y, rc->width,
                               rc->height);
  else
    gtk_widget_queue_draw(self->drawingarea);
}

static
gboolean _relation_viewport_on_expose(GtkWidget * widget,
                                      GdkEventExpose * event,
                                      RelationViewport * self)
{
  if (self->needRedraw) {
    _relation_viewport_redraw_real (self);
    self->needRedraw = FALSE;
  }

  {
    GdkRectangle area = event->area;
    GtkWidget * da = self->drawingarea;

    gdk_draw_pixmap (da->window, da->style->fg_gc [GTK_WIDGET_STATE (da)],
                     self->backpixmap, area.x, area.y,
                     area.x, area.y,
                     area.width, area.height);
  }

  return FALSE;
}


static void _on_label_assocs_changed (gpointer object, Relview * rv)
{
	RelationViewport * self = (RelationViewport*) object;
	Rel * rel = relation_viewport_get_relation(self);

	verbose_printf (VERBOSE_TRACE, __FILE__": _on_label_assocs_changed\n");

	if (rel) {
		LabelAssoc assoc = rv_label_assoc_get (rv, rel, self);
		_relation_viewport_set_labels (self, assoc.labels[0], assoc.labels[1]);
	}
	else { /* ignore */ }
}


/*!
 * Called if one of the current labels is being deleted. Callback for
 * LabelObserver.onDelete(...).
 */
static void _relation_viewport_on_delete_label (gpointer user_data, Label * label)
{
	RelationViewport * self = (RelationViewport*) user_data;
	if (self->colLabel == label) self->colLabel = NULL;
	if (self->rowLabel == label) self->rowLabel = NULL;

	relation_viewport_redraw (self);
	relation_viewport_invalidate_rect (self, NULL);
}


RelationViewport * relation_viewport_new()
{
  RelationViewport * self =
      (RelationViewport*) calloc(1, sizeof(RelationViewport));

  self->rv = rv_get_instance ();

  /* Prepare the Relview observer */
  self->rvObserver.object = self;
  self->rvObserver.labelAssocsChanged = _on_label_assocs_changed;
  rv_register_observer (self->rv, &self->rvObserver);

  /* Prepare the relation observer. */
  self->relObserver.object = (gpointer) self;
  self->relObserver.changed = RELATION_OBSERVER_CHANGED_FUNC (_relation_viewport_relation_changed);
  self->relObserver.renamed = RELATION_OBSERVER_RENAMED_FUNC (_relation_viewport_relation_renamed);
  self->relObserver.onDelete = RELATION_OBSERVER_ON_DELETE_FUNC (_relation_viewport_relation_on_delete);

  self->vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_ref(self->vbox); /* otherwise removeing the widget from a container
   * could drop its reference count to 0. */

  /* create the viewport's gtk widgets */
  self->table = gtk_table_new(2, 2, FALSE);
  gtk_box_pack_start(GTK_BOX (self->vbox), self->table, TRUE, TRUE, 0);

  self->horzAdjust = GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 1, 0, 0));
  self->vertAdjust = GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 1, 0, 0));
  self->horzScroll = gtk_hscrollbar_new(self->horzAdjust);
  self->vertScroll = gtk_vscrollbar_new(self->vertAdjust);

  gtk_table_attach(GTK_TABLE (self->table), self->horzScroll, 0, 1, 1, 2,
  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
  (GtkAttachOptions) (0), 0, 0);

  gtk_table_attach(GTK_TABLE (self->table), self->vertScroll, 1, 2, 0, 1,
  (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
  (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
  0, 0);

  gtk_range_set_update_policy(GTK_RANGE (self->horzScroll),
  GTK_UPDATE_CONTINUOUS);
  gtk_range_set_update_policy(GTK_RANGE (self->vertScroll),
  GTK_UPDATE_CONTINUOUS);

  self->drawingarea = gtk_drawing_area_new();
  gtk_table_attach(GTK_TABLE (self->table), self->drawingarea, 0, 1, 0, 1,
  (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
  (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
  1, 1);

  /* ---- Statusbar ----- */
  {
    GtkWidget * outerhbox = gtk_hbox_new(FALSE, 0), *innerhbox =
        gtk_hbox_new(FALSE, 0);
    GtkWidget * label = gtk_label_new("Zoom");
    int zoomdefault = prefs_get_int("settings", "relationwindow_zoom", 3);
    GtkAdjustment * adjust;

    gtk_box_pack_start(GTK_BOX (self->vbox), outerhbox, FALSE, TRUE, 0);

    self->sbar_labelPosition = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX (outerhbox), self->sbar_labelPosition, TRUE, TRUE, 0);
    gtk_label_set_justify(GTK_LABEL (self->sbar_labelPosition), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC (self->sbar_labelPosition), 0.00, 0.5);

    gtk_box_pack_start(GTK_BOX (outerhbox), innerhbox, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX (innerhbox), label, TRUE, TRUE, 5);
    gtk_label_set_justify(GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
    gtk_misc_set_alignment(GTK_MISC (label), 0.94, 0.5);

    adjust = GTK_ADJUSTMENT (gtk_adjustment_new (zoomdefault, 1, 10, 1, 1, 0));
    self->sbar_sbZoom = gtk_spin_button_new(GTK_ADJUSTMENT (adjust), 1, 0);
    gtk_box_pack_start(GTK_BOX (innerhbox), self->sbar_sbZoom, FALSE, TRUE, 0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (self->sbar_sbZoom), TRUE);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON (self->sbar_sbZoom), TRUE);
  }

  /* ---- Connect the Signal Handlers ---- */
  gtk_signal_connect (GTK_OBJECT (self->drawingarea), "button_press_event",
      GTK_SIGNAL_FUNC (_relation_viewport_on_button_press),
      (gpointer) self);
  gtk_signal_connect (GTK_OBJECT (self->drawingarea), "motion_notify_event",
      GTK_SIGNAL_FUNC (_relation_viewport_on_mouse_motion),
      (gpointer) self);
  gtk_signal_connect_after (GTK_OBJECT (self->drawingarea), "configure_event",
      GTK_SIGNAL_FUNC (_relation_viewport_on_configure),
      (gpointer) self);
  gtk_signal_connect_after (GTK_OBJECT (self->drawingarea), "expose_event",
      GTK_SIGNAL_FUNC (_relation_viewport_on_expose),
      (gpointer) self);

  /* Scrollbars */
  gtk_signal_connect (GTK_OBJECT(self->horzScroll), "value-changed",
      GTK_SIGNAL_FUNC (_relation_viewport_on_scroll),
      (gpointer) self);
  gtk_signal_connect (GTK_OBJECT(self->vertScroll), "value-changed",
      GTK_SIGNAL_FUNC (_relation_viewport_on_scroll),
      (gpointer) self);

  /* Zoom */
  gtk_signal_connect (GTK_OBJECT(self->sbar_sbZoom), "value-changed",
      GTK_SIGNAL_FUNC (_relation_viewport_on_zoom_change),
      (gpointer) self);

  gtk_widget_set_events(self->drawingarea, GDK_EXPOSURE_MASK
      | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);

  /* ---- Attach the RelationViewport to topmost Widget ---- */
  _relation_viewport_attach(self, relation_viewport_get_widget(self));

  //gtk_widget_show_all (self->vbox);

  /* moved from src/main.c */
  self->lineMode = MODE_DOWN_RIGHT; /* default draw_mode for rel. */

  /* create relation window */
  self->zoom = prefs_get_int("settings", "relationwindow_zoom", 3);
  self->zoomFont = TRUE;
  self->fontFamily = strdup("Sans");

  self->sbar_position = (char*) calloc (1,1); // \0
  self->sbar_entries = (char*) calloc (1,1); // \0
  strcpy(self->sbar_labels, "");

  /* Set labels. */
  self->rowLabel = NULL;
  self->colLabel = NULL;
  self->labelObserver.object = self;
  self->labelObserver.onDelete = _relation_viewport_on_delete_label;

  /* There is no default relation. */
  self->rel = NULL;

  /* Show the caption by default. */
  self->drawCaption = TRUE;

  /* Not (default) relation is not too big to be shown. */
  self->tooBig = FALSE;

  /* At the beginning we need a redraw for sure. Actually the value
   * will be set to TRUE by relation_viewport_configure anyway. */
  self->needRedraw = TRUE;

  mpz_init(self->relEntryCount);

  /* At least the default labels need a (additional) configuration. */
  relation_viewport_configure(self);

  return self;
}

void relation_viewport_destroy(RelationViewport * self)
{
  /* the popup owner is responsible to destroy the menu. */
  if (self->popup) {
    gtk_menu_detach(GTK_MENU (self->popup));
    gtk_widget_unref(self->popup);
  }

  _relation_viewport_set_labels(self, NULL, NULL);

  gtk_widget_destroy(self->vbox);

  if (self->backpixmap) {
    gdk_pixmap_unref (self->backpixmap);
    cairo_destroy(self->crOffset);
  }

  mpz_clear(self->relEntryCount);

  free(self->fontFamily);
  free(self);
}


gboolean relation_viewport_has_relation(RelationViewport * self)
{
  return self->rel != NULL;
}


static void _relation_viewport_relation_on_delete(RelationViewport * self,
                                                  Rel * rel)
{
  /* it's possible, that another observer take care on this event. We only
   * unset the relation, if it has not changed already. */

  Rel * _rel = relation_viewport_get_relation(self);

  if (rel == _rel) {
    relation_viewport_set_relation(self, NULL);

    relation_viewport_configure(self);
    relation_viewport_redraw(self);
    relation_viewport_invalidate_rect(self, NULL);
  }
}


/* called, when the relation has changed. Doesn't redraw the viewport.
 * This would be possible and elegant, too, but since the viewport is
 * more or less enclosed, this would cause many redundant redraws. */
static void _relation_viewport_relation_changed(RelationViewport * self,
                                                Rel * rel)
{
  Rel * _rel = relation_viewport_get_relation(self);

  if (rel != _rel) { ; }
  else {
    /* The current relation has been changed. */
	    relation_viewport_configure(self);
	    relation_viewport_refresh(self);
  }

  /* Recount the number of entries in the relation. This is a very
   * exhaustive operation, so take a look at the number of callback
   * invocations. */
  kure_get_entries(rel_get_impl(rel), self->relEntryCount);
}


/* invoked when the current relation changes its name */
static void _relation_viewport_relation_renamed(RelationViewport * self,
                                                Rel * rel)
{
  Rel * _rel = relation_viewport_get_relation(self);

  assert (rel == _rel);
  relation_viewport_redraw(self);
  relation_viewport_invalidate_rect(self, NULL);
}


void relation_viewport_set_relation(RelationViewport * self, Rel * rel)
{
  if (self->rel == rel)
    return;
  else {
    if (self->rel) {
      relation_unregister_observer(self->rel, &self->relObserver);

      // TODO
      //Kure_mp_mfree(self->relEntryCount);
      //self->relEntryCount = NULL;
    }

    self->rel = rel;

    if (self->rel) {
      relation_register_observer(self->rel, &self->relObserver);

      /* Update the number of bits set (minterms) and reset the cursor
       * position/labels/entries displayed in the status bar. */
#if 0
      fprintf (stderr, "relation_viewport_set_relation:\n"
    		  "   self: %p\n"
    		  "   self->rel: %p\n"
    		  "   impl: %p\n", self, self->rel, rel_get_impl(self->rel));
#endif
      kure_get_entries(rel_get_impl(self->rel), self->relEntryCount);

      if (self->sbar_position) free (self->sbar_position);
      self->sbar_position = (char*)calloc(1,1);

      if (self->sbar_entries) free (self->sbar_entries);
      self->sbar_entries = (char*)calloc(1,1);

      strcpy (self->sbar_labels, "");

      /* If the new relation is too big, then we won't show the relation, due
       * to internal conversion problems to (unsigned) int. */
      self->tooBig = !kure_rel_fits_si(rel_get_impl(rel));

      /* Set labels. */
      {
    	  LabelAssoc assoc = rv_label_assoc_get (self->rv, self->rel, self);
    	  _relation_viewport_set_labels (self, assoc.labels[0], assoc.labels[1]);
      }
    } /* self->rel != NULL */

    relation_viewport_configure(self);
    relation_viewport_refresh(self);
  }
}

Rel * relation_viewport_get_relation(RelationViewport * self)
{
  return self->rel;
}

RelationViewportLineMode relation_viewport_get_line_mode(RelationViewport * self)
{
  return self->lineMode;
}

/* delta is the size of the displayed rects for the relation bits. */
int _relation_viewport_get_delta(RelationViewport * self)
{
  /* function needed: interval 1..10 [zoom] -> 4..22 [pixels/cell] */
  return MAX(((self->zoom - 1) * 2) + 4, MINDELTA);
}

GtkWidget * relation_viewport_get_drawable(RelationViewport * self)
{
  return self->drawingarea;
}

GtkWidget * relation_viewport_get_widget(RelationViewport * self)
{
  return self->vbox;
}

/*****************************************************************************/
/*       NAME : getCellRect                                                  */
/*    PURPOSE : returns the canvas pixel coordinates of the given relation   */
/*              cell                                                         */
/* PARAMETERS : (x, y) == relation cell                                      */
/*    CREATED : Werner Lehmann                                               */
/*       DATE : 02-MAY-2000 Werner Lehmann                                   */
/*****************************************************************************/
GdkRectangle getCellRect(RelationViewport * self, int x, int y)
{
  GdkRectangle cell_rect;
  int delta = _relation_viewport_get_delta(self);

  cell_rect.x = self->marginx + self->labelmx + (x * delta);
  cell_rect.y = self->marginy + self->labelmy + (y * delta);
  cell_rect.width = cell_rect.height = delta;

  return cell_rect;
}

GdkPoint viewToCanvasPoint(RelationViewport * self, int x, int y)
{
  GdkPoint canvas_point;

  canvas_point = getCanvasOrigin(self->horzScroll, self->vertScroll);
  canvas_point.x += x;
  canvas_point.y += y;

  return canvas_point;
}

GdkPoint canvasToViewPoint(RelationViewport * self, int x, int y)
{
  GdkPoint view_point;

  view_point = getCanvasOrigin(self->horzScroll, self->vertScroll);
  view_point.x = (x - view_point.x);
  view_point.y = (y - view_point.y);

  return view_point;
}

/*****************************************************************************/
/*       NAME : drawGridCell                                                 */
/*    PURPOSE : draw a white or pattern/black grid cell according to isSet   */
/* PARAMETERS : actual drawingarea coordinates (int x, int y),               */
/*              cell state (isSet), whether pixmap should be copied to screen*/
/*              immediately (invalidate) or not                              */
/*    CREATED : 25-MAR-2000 Werner Lehmann                                   */
/*   MODIFIED : 11-MAY-2000 WL added parameter invalidate                    */
/*****************************************************************************/
void drawGridCell(RelationViewport * self, int x, int y, int isSet,
                  int invalidate)
{
  GdkRectangle cliprect;

  cairo_t * cr = self->crOffset;
  int dstX = x+1, dstY = y+1;
  int delta = _relation_viewport_get_delta(self);

  /* Draw the surrounding border */
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_rectangle(cr, x, y, delta, delta);
  cairo_stroke(cr);

  if (isSet) {
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
  } else {
    cairo_set_source_rgb(cr, 1, 1, 1);
  }

  cairo_rectangle(cr, dstX, dstY, delta-1, delta-1);
  cairo_fill(cr);

  if (invalidate) {
    cliprect.x = x;
    cliprect.y = y;
    cliprect.width = cliprect.height = delta;
    gtk_widget_draw(self->drawingarea, &cliprect);
  }
}

/*****************************************************************************/
/*       NAME : pixelToGrid                                                  */
/*    PURPOSE : calculate grid coordinates from pixel coordinates            */
/*              result FALSE if pixels coordinates are not within grid       */
/* PARAMETERS : relation (Rel * rel)                                         */
/*              pixel co. (int x, int y),                                    */
/*              grid co. (int * grid_x, int * grid_y)                        */
/*    CREATED : 25-MAR-2000 WL                                               */
/*   MODIFIED : 14-JUN-2000 WL: Bugfix. A point slighty above or to the left */
/*                              of the grid had been reported as if it was   */
/*                              in  the first row or column.                 */
/*              26-JUN-2000 WL: Label support.                               */
/*****************************************************************************/
int pixelToGrid(RelationViewport * self, int pix_x, int pix_y, int * x, int * y)
{
  GdkPoint scroll_pos;
  int pix_x_start;
  int pix_y_start;
  int delta = _relation_viewport_get_delta(self);
  KureRel * impl = rel_get_impl(self->rel);

  scroll_pos = getCanvasOrigin(self->horzScroll, self->vertScroll);

  pix_x_start = (self->labelmx > 0) ? (self->marginx + self->labelmx)
                                    : MAX(self->marginx - scroll_pos.x * delta,
                                           0);
  pix_y_start = CAPTION_MARGIN_TOP + ((self->labelmy> 0) ?
      (self->marginy + self->labelmy)
      : MAX (self->marginy - scroll_pos.y * delta, 0));

  if ((pix_x < pix_x_start) || (pix_y < pix_y_start))
    return FALSE;

  *x = scroll_pos.x + (pix_x - pix_x_start) / delta;
  *y = scroll_pos.y + (pix_y - pix_y_start) / delta;

  return *x < kure_rel_get_cols_si(impl) &&
		  *y < kure_rel_get_rows_si(impl);
}

void gridToPixel(RelationViewport * self, int x, int y, int * pix_x, int * pix_y)
{
  GdkPoint scroll_pos;
  int pix_x_start;
  int pix_y_start;
  int delta = _relation_viewport_get_delta(self);

  scroll_pos = getCanvasOrigin(self->horzScroll, self->vertScroll);

  pix_x_start = (self->labelmx > 0) ? (self->marginx + self->labelmx)
                                    : MAX(self->marginx - scroll_pos.x * delta,
                                           0);
  pix_y_start = CAPTION_MARGIN_TOP + ((self->labelmy> 0) ?
      (self->marginy + self->labelmy)
      : MAX (self->marginy - scroll_pos.y * delta, 0));

  *pix_x = (x - scroll_pos.x) * delta + pix_x_start;
  *pix_y = (y - scroll_pos.y) * delta + pix_y_start;
}

/*****************************************************************************/
/*       NAME : adjustAdjustments                                            */
/*    PURPOSE : change adjustment values according to the parameters         */
/* PARAMETERS : the scrollbar (range), the maximum size (upper) and page size*/
/*    CREATED : 23-MAR-2000 WL                                               */
/*****************************************************************************/
void adjustAdjustments(RelationViewport * self, GtkRange * range, gfloat upper,
                       gfloat page_size)
{
  gfloat value_correction;
  GtkAdjustment * adjust;

  adjust = gtk_range_get_adjustment(range);
  value_correction = (adjust->value + (adjust->page_size / 2)) / adjust->upper;

  adjust->upper = upper;
  adjust->page_size = (rel_allow_display(self->rel)) ? page_size : upper;
  adjust->step_increment = 1; /* (adjust->page_size / 10); */
  adjust->page_increment = adjust->page_size;
  gtk_adjustment_changed(adjust);

  adjust->value = (value_correction * adjust->upper) - (adjust->page_size / 2);
  if ((adjust->page_size >= adjust->upper) || (adjust->value < 0)
      || adjust->value > adjust->upper)
    adjust->value = 0;

  /* Show page_size bits if possible. */
  adjust->value = MAX(.0f, MIN(adjust->value, adjust->upper - adjust->page_size));

  gtk_adjustment_value_changed (adjust);
}

static int _relation_viewport_get_label_font_size(RelationViewport * self)
{
  return self->zoomFont ? (_relation_viewport_get_delta(self) - 4) : 9;
}

/*****************************************************************************/
/*       NAME : put_rel_line_label                                           */
/*    PURPOSE : draws a label at position "line"                             */
/* PARAMETERS : the label to draw (label), the line                          */
/*    CREATED : 21-MAY-1996 PS                                               */
/*   MODIFIED : 22-MAY-1996 PS (?)                                           */
/*              23-JUN-2000 WL : GTK+ port, reduced parameter count          */
/*              10-MAR-2008 STB: Positioning corrections.                    */
/*****************************************************************************/
void put_rel_line_label(RelationViewport * self, const char * label, int line)
{
  cairo_t * cr = self->crOffset;
  cairo_text_extents_t te;
  cairo_font_extents_t fe;
  int tx, ty;
  int delta = _relation_viewport_get_delta(self);
  const char * text = label;

  cairo_select_font_face(cr, self->fontFamily, CAIRO_FONT_SLANT_NORMAL, 0);
  cairo_set_font_size(cr, _relation_viewport_get_label_font_size(self));

  cairo_font_extents(cr, &fe);

  /* compute the individual text position from the rectangle needed by the
   * the node name. */
  cairo_text_extents(cr, text, &te);
  tx = self->marginx + self->labelmx - te.width - 10 /*margin*/;
  ty = CAPTION_MARGIN_TOP + self->marginy + self->labelmy
  + ((line - 1) * delta) + fe.height + (delta - fe.height) / 2 - 1;

  cairo_move_to(cr, tx, ty);
  cairo_show_text(cr, text);

  cairo_identity_matrix(cr);
  cairo_new_path(cr);
}

/*****************************************************************************/
/*       NAME : put_rel_col_label                                            */
/*    PURPOSE : draws a label at position "column"                           */
/* PARAMETERS : the label to draw (label), the column                        */
/*    CREATED : 21-MAY-1996 PS                                               */
/*   MODIFIED : 22-MAY-1996 PS (?)                                           */
/*              23-JUN-2000 WL : GTK+ port, reduced parameter count          */
/*****************************************************************************/
void put_rel_col_label(RelationViewport * self, const char * label, int column)
{
  cairo_t * cr = self->crOffset;
  cairo_text_extents_t te;
  cairo_font_extents_t fe;
  int tx, ty;
  int delta = _relation_viewport_get_delta(self);
  const char * text = label;

  cairo_select_font_face(cr, self->fontFamily, CAIRO_FONT_SLANT_NORMAL, 0);
  cairo_set_font_size(cr, _relation_viewport_get_label_font_size(self));

  cairo_font_extents(cr, &fe);

  /* compute the individual text position from the rectangle needed by the
   * the node name. */
  cairo_text_extents(cr, text, &te);
  tx = self->marginx + self->labelmx + (column - 1) * delta + fe.height
      + (delta - fe.height) / 2 - (delta / 10.0);
  ty = CAPTION_MARGIN_TOP + self->marginy + self->labelmy - 5 /*margin*/;

  cairo_move_to(cr, tx, ty);
  /* Rotate the the label text by 45 degree ccw. */
  cairo_rotate(cr, -M_PI/4);
  cairo_show_text(cr, text);

  cairo_identity_matrix(cr);
  cairo_new_path(cr);
}

void relation_viewport_show_caption(RelationViewport * self, gboolean yesno)
{
  self->drawCaption = yesno;
}

gint _relation_viewport_draw_caption(RelationViewport * self)
{
  Rel * rel = relation_viewport_get_relation(self);
  char * caption= NULL;

  if (rel) {
	  mpz_t rows, cols;
	  mpz_init (rows); mpz_init (cols);

	  rel_get_rows(rel, rows);
	  rel_get_cols(rel, cols);

	  gmp_asprintf (&caption, "NAME: %s  DIM: %Zd X %Zd", rel_get_name(rel), rows, cols);

	  mpz_clear(rows); mpz_clear(cols);
  } else {
    caption = strdup("No Relation");
  }

  {
    cairo_t * cr = self->crOffset;
    cairo_text_extents_t te;
    int tx, ty;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, 0);
    cairo_set_font_size(cr, 12);

    cairo_text_extents(cr, caption, &te);
    tx = 5;
    ty = te.height;

    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, caption);

    cairo_identity_matrix(cr);
    cairo_new_path(cr);
  }

  free(caption);

  return 15;
}

/*****************************************************************************/
/*       NAME : draw_matrix                                                  */
/*    PURPOSE : draw relation grid                                           */
/* PARAMETERS : relation to draw (rel)                                       */
/*    CREATED : 23-MAR-2000 WL                                               */
/*   MODIFIED : 23-JUN-2000 WL: label support                                */
/*              22-NOV-2000 WL: minor fixes & enhancements, support for non  */
/*                              displayable relations                        */
/*              09-MAR-2008 STB: Labeling enhancements                       */
/*****************************************************************************/
void draw_matrix(RelationViewport * self, GdkRectangle cliprect)
{
  GdkPoint scroll_pos; /* [cells] */
  GdkRectangle vis_rect; /* x,y in [cells], width,height in [px] */
  GdkPoint draw_pos; /* [cells] */
  Rel * rel = self->rel;
  KureRel * impl = rel_get_impl (rel);
  int delta = _relation_viewport_get_delta(self);

  vis_rect = getVisibleRect(self->drawingarea, self->horzScroll,
                            self->vertScroll);
  scroll_pos = draw_pos = getCanvasOrigin(self->horzScroll, self->vertScroll);

  /* clear cliprect in backpixmap */
  gdk_draw_rectangle(self->backpixmap, self->drawingarea->style->white_gc,
  TRUE /* filled */, 0, 0, /* x,y */
  cliprect.width, cliprect.height /* width,height */);

  if (self->drawCaption)
    _relation_viewport_draw_caption(self);

  if (rel_allow_display(rel)) {
    int breite, hoehe;
    int pix_x;
    int pix_x_start, pix_y_start;
    int vis_width, vis_height;
    gint rows, cols;

    /* display relation */
    breite = kure_rel_get_cols_si(rel_get_impl(rel));
    hoehe  = kure_rel_get_rows_si(rel_get_impl(rel));

    /* ignore the cliprect for the time being; simply redraw the whole
     * backpixmap depending on labelm[xy], margin[xy] and the adjustments */

    /* pixel positions to start drawing the grid cells.  */
    pix_x = pix_x_start = (self->labelmx > 0) ? (self->marginx + self->labelmx)
                                              : MAX(self->marginx
                                                 - scroll_pos.x * delta, 0);
    pix_y_start = CAPTION_MARGIN_TOP + ((self->labelmy> 0)
        ? (self->marginy + self->labelmy)
        : MAX (self->marginy - scroll_pos.y * delta, 0));

    cols = MAX(1, MIN (breite-(signed)scroll_pos.x, (vis_rect.width - pix_x) / delta));
    rows = MAX(1, MIN (hoehe-(signed)scroll_pos.y, (vis_rect.height - pix_y_start) / delta));

    /* draw the grid cells column-wise */
    {
      gint firstCol = draw_pos.x, firstRow = draw_pos.y;
      gint i, j;
      int vars_cols = kure_rel_get_vars_cols(impl), vars_rows = kure_rel_get_vars_rows(impl);

      {
        cairo_t * cr = self->crOffset;

        /* Draw the complete grid at once. */
        cairo_set_source_rgb(cr, 0, 0, 0);
        for (j = 0; j <= cols; j ++) /* vertical grid lines */{
          cairo_move_to(cr, pix_x + j*delta, pix_y_start);
          cairo_rel_line_to(cr, 0, rows * delta);
        }
        for (i = 0; i <= rows; i ++) /* horizontal grid lines */{
          cairo_move_to(cr, pix_x, pix_y_start + i*delta);
          cairo_rel_line_to(cr, cols * delta, 0);
        }
        cairo_stroke(cr);

        /* Draw the bits set */
        cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
        for (j = 0; j < cols; j ++) {
          for (i = 0; i < rows; i ++) {
            if (kure_get_bit_fast_si (impl,i+firstRow,j+firstCol,vars_rows,vars_cols)) {
              int x = pix_x + j*delta, y = pix_y_start + i*delta;

              cairo_rectangle(cr, x, y, delta-1, delta-1);
            }
          }
        }
        cairo_fill(cr);

        /* Reset the color to black */
        cairo_set_source_rgb(cr, 0, 0, 0);
      }

    }

    /* draw the labels */
    vis_width = self->drawingarea->allocation.width - self->marginx
        - self->labelmx;
    vis_height = self->drawingarea->allocation.height - self->marginy
        - self->labelmy;

    /* NOTE: There is a major problem with the drawing routine on the sun's
     *       at the university, if cairo is used and the fon't becomes very
     *       small (e.g. zoom=1). Nothing will be drawn to the screen. Neither
     *       the matrix itself, nor the labels. But the matrix is drawn, if the
     *       following code (col and row drawing) is commented out. The work-
     *       around is to do not (try to) draw labels, when the font size becomes
     *       too small.  */
    if (relation_viewport_get_zoom(self) > 1) {
    	GString * name = g_string_new ("");

    	if (self->rowLabel) {
    		int first = scroll_pos.y + 1;
    		LabelIter * iter = label_iterator (self->rowLabel);
    		label_iter_seek (iter, first);

    		while (label_iter_is_valid(iter) && label_iter_number(iter) < first + rows) {
    			int k = label_iter_number (iter);
    			label_iter_name(iter, name);

    			if ((0 == self->labelmy) && scroll_pos.y) /*?*/
    				put_rel_line_label (self, name->str, k-first);
    			else
    				put_rel_line_label (self, name->str, k-first+1);
    			label_iter_next (iter);
    		}

    		label_iter_destroy (iter);
    	}

    	if (self->colLabel) {
    		int first = scroll_pos.x + 1;
    		LabelIter * iter = label_iterator (self->colLabel);
    		label_iter_seek (iter, first);

    		while (label_iter_is_valid(iter) && label_iter_number(iter) < first + cols) {
    			int k = label_iter_number (iter);
    			label_iter_name(iter, name);

    			if ((0 == self->labelmx) && scroll_pos.x) /*?*/
    				put_rel_col_label (self, name->str, k-first);
    			else
    				put_rel_col_label (self, name->str, k-first+1);
    			label_iter_next (iter);
    		}

    		label_iter_destroy (iter);
    	}

    	g_string_free (name, TRUE);
    }

  } else /* ! rel_allow_display (rel) */{
    int relationLimit = prefs_get_int("settings", "relation_display_limit", 2 << 26);
    char errMessage[100];
    sprintf(errMessage,"This relation can not be displayed, because the limit of %i was reached.", relationLimit);

    _relation_viewport_draw_message(self, errMessage);
  }

  _relation_viewport_sbar_update_labels(self);
  _relation_viewport_sbar_update_entries(self);
}

static
void _relation_viewport_on_scroll(GtkWidget * range, RelationViewport * self)
{
  relation_viewport_refresh(self);
}

void relation_viewport_set_zoom(RelationViewport * self, int zoom)
{
  self->zoom = zoom;

  relation_viewport_configure(self);
  relation_viewport_refresh(self);

  /* save the zoom to the configuration file. */
  prefs_set_int("settings", "relationwindow_zoom", self->zoom);
}

int relation_viewport_get_zoom(RelationViewport * self)
{
  return self->zoom;
}

static
void _relation_viewport_on_zoom_change(GtkWidget * widget,
                                       RelationViewport * self)
{
  relation_viewport_set_zoom(self,
                              (int) gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
}


/*****************************************************************************/
/*       NAME : redraw_rel                                                   */
/*    PURPOSE : redraws the relation window with respect to the scrollbars   */
/*              (right, bottom) and initializes delta and the margins.       */
/* PARAMETERS : the appropriate relation (rel)                               */
/*    CREATED : 02-AUG-1995 PS                                               */
/*   MODIFIED : 16-MAR-1995 PS: ?                                            */
/*              13-JUN-2000 WL: GTK+ port                                    */
/*              14-JUN-2000 WL: the backpixmap size and the scrollbars had   */
/*                              not been updated (important after            */
/*                              Graph->Relation)                             */
/*****************************************************************************/
void redraw_rel(RelationWindow * window)
{
  if (relation_window_is_visible(window)) {
    RelationViewport * vp = relation_window_get_viewport(window);

    relation_viewport_redraw(vp);
    relation_viewport_invalidate_rect(vp, NULL);
  }
}


/*!
 * Sets or unsets the labels of the \ref RelationViewport.
 *
 * \param rowLabel May be NULL.
 * \param colLabel May be NULL.
 */
void _relation_viewport_set_labels (RelationViewport * self, Label * rowLabel,
                                  Label * colLabel)
{
	if (self->rowLabel)
		label_unregister_observer (self->rowLabel, &self->labelObserver);
	if (self->colLabel)
		label_unregister_observer (self->colLabel, &self->labelObserver);

	self->rowLabel = rowLabel;
	self->colLabel = colLabel;

	if (self->rowLabel) {
		label_register_observer (self->rowLabel, &self->labelObserver);
	}
	if (self->colLabel) {
		label_register_observer (self->colLabel, &self->labelObserver);
	}

    relation_viewport_configure(self);
    relation_viewport_redraw(self);
    relation_viewport_invalidate_rect(self, NULL);
}

/*****************************************************************************/
/*       NAME : update_sbar_entries (put_count_high_rel_into_footer)         */
/*    PURPOSE : Writes the number of fulfilling entries into the footer      */
/*    CREATED : 15-SEP-2000 Ulf Milanese                                     */
/*   MODIFIED : 21-NOV-2000 WL: changed to fit into new statusbar function   */
/*                              organization                                 */
/*              13-MAR-2002 UMI: Printing of really big numbers is possible  */
/*****************************************************************************/
static
void _relation_viewport_sbar_update_entries(RelationViewport * self)
{
	GString * str = g_string_new ("");

	gchar * ents = rv_user_format_large_int (self->relEntryCount);

	if (self->sbar_entries != NULL)
		free (self->sbar_entries);

	g_string_append (str, "[");
	g_string_append (str, ents);
	g_string_append (str, " entries]");

	g_free (ents);

	self->sbar_entries = strdup (str->str);

	g_string_free (str, TRUE);

	_relation_viewport_sbar_update(self);
}


/*****************************************************************************/
/*       NAME : update_sbar_labels                                           */
/*    PURPOSE : writes current label descriptions into sbar_labels           */
/*    CREATED : 21-NOV-2000 WL                                               */
/*****************************************************************************/
static void _relation_viewport_sbar_update_labels(RelationViewport * self)
{
	self->sbar_labels [0] = '\0';

	if (self->rowLabel || self->colLabel) {
		sprintf(self->sbar_labels, "(%s / %s)",
				self->rowLabel ? label_get_name(self->rowLabel) : "",
						self->colLabel ? label_get_name(self->colLabel) : "");
	}

	_relation_viewport_sbar_update(self);
}


/*****************************************************************************/
/*       NAME : update_sbar_position                                         */
/*    PURPOSE : writes the current mouse/grid position into sbar_position    */
/*    CREATED : 21-NOV-2000 WL                                               */
/*****************************************************************************/
static
void _relation_viewport_sbar_update_position (RelationViewport * self, int x, int y)
{
	/* It is perfectly sufficient to use integer values here until very large
	 * relation are not displayed. */

	if (self->rel && rel_allow_display(self->rel)) {
		int grid_x, grid_y;

		if ( self->sbar_position)
			free (self->sbar_position);

		if (pixelToGrid(self, x, y, &grid_x, &grid_y))
			gmp_asprintf (&self->sbar_position, "(c: %d / r: %d)", 1+grid_x, 1+grid_y);
		else
			self->sbar_position = strdup ("(Unknown)");

		_relation_viewport_sbar_update(self);
	}
}


/*****************************************************************************/
/*       NAME : update_statusbar                                             */
/*    PURPOSE : appends all sbar_* strings and updates labelPosition         */
/*    CREATED : 21-NOV-2000 WL                                               */
/*****************************************************************************/
static
void _relation_viewport_sbar_update(RelationViewport * self)
{
	GString * sbar = g_string_new (self->sbar_labels);
	if (sbar->len > 0 && !g_str_equal("", self->sbar_entries))
		g_string_append (sbar, " -- ");
	g_string_append (sbar, self->sbar_entries);
	if (sbar->len > 0 && !g_str_equal("", self->sbar_position))
		g_string_append (sbar, " -- ");
	g_string_append (sbar, self->sbar_position);

	gtk_label_set_text(GTK_LABEL (self->sbar_labelPosition), sbar->str);
	g_string_free (sbar, TRUE);

	/* queue_draw: if the text did not change it is not updated. can result in
     * an empty statusbar. */
	gtk_widget_queue_draw(GTK_WIDGET (self->sbar_labelPosition));
}


/* set all bits of the relation to 0. */
void relation_viewport_clear_relation(RelationViewport * self)
{
  if ( self->rel) {
    kure_O(rel_get_impl(self->rel));
    rel_changed(self->rel);
  }
}

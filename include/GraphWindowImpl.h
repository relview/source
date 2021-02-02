/*
 * GraphWindowImpl.h
 *
 *  Copyright (C) 2010 Stefan Bolus, University of Kiel, Germany
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

#ifndef GRAPHWINDOWIMPL_H_
#define GRAPHWINDOWIMPL_H_

#include "GraphWindow.h"
#include "Menu.h" // MenuDomain
#include "label.h"

#define GRAPH_WINDOW_MENU_PREFIX "/graph-window"

#define DEFAULT_SPANNING_WIDTH    1800
#define DEFAULT_SPANNING_HEIGHT   1800
#define DEFAULT_MIN_RANK           30
#define DEFAULT_MIN_ORDER          50

extern GtkWidget* create_windowGraph (void);
extern GtkWidget * labelTooltipGW;
extern GtkWidget *windowGraph;
extern GtkWidget *labelPositionGW;
extern GtkWidget *cbZoomLevelGW;
extern GtkWidget *hscrollbarGW;
extern GtkWidget *vscrollbarGW;
extern GtkWidget *drawingareaGW;

#define RADIUS  	40       /* Radius der Kanten vom Knoten auf sich selbst */

#define PM        	0     /* nur in Pixmap malen */
#define GW        	1     /* nur auf Graph-Window malen */

#define GRAPH_SELECTION_TYPE_NODES 1
#define GRAPH_SELECTION_TYPE_EDGES 2

extern GtkWidget* create_popupmenuGraph (void);
extern GtkWidget *popupmenuGraph;


extern gboolean drawingarea_ButtonPress (GtkWidget *widget, GdkEventButton *event,
                                         gpointer user_data);
extern gboolean drawingarea_ButtonRelease (GtkWidget *widget, GdkEventButton *event,
                                         gpointer user_data);
extern gboolean drawingarea_MotionNotify (GtkWidget *widget, GdkEventMotion *event,
                                          gpointer user_data);
extern gint drawingarea_Configure (GtkWidget *widget, GdkEventConfigure *event);
extern gint drawingarea_Expose (GtkWidget *widget, GdkEventExpose *event);
extern gint graphwindow_DeleteEvent (GtkWidget *widget, GdkEvent *event,
                                     gpointer data);
extern void adjustmentValueChangedGW (GtkAdjustment *adjustment, gpointer user_data);


struct _GraphWindow
{
/* private: */
	Relview * rv;
	RelviewObserver rvObserver;

  GtkWidget * window;

  GtkWidget * drawingarea;
  GtkWidget * hscroll, * vscroll;
  GtkWidget * labelPosition;
  GtkWidget * labelTooltip;
  GtkWidget * cbZoomLevel;

  /* The backpixmap holds only black edges. Also the dashed and bold ones.
   * Edges of other types, maybe colored and the nodes and labels are
   * stored in a seperate pixmap called layer. The layer pixmap will be
   * redrawn every time a graph is (de-)selected or an scroll event occurs,
   * while the pixmap for the edges is rather constant, will complex to
   * draw. */
  GdkPixmap * backpixmap;
  GdkPixmap * layer;
  cairo_t * crOffset;      /* cairo context to use for drawing */
  cairo_t * cr;

  XGraph * graph;

  gint delta_grid;
  gboolean useGrid;

  gboolean editable;

  SelectionManager * selManager;
  XGraphNode * selectedNode;          /*!< node selected for edge creation. */
  gint selectedNodeState;

  GtkWidget * popupmenu;
  GtkWidget * popupmenuEmpty; 		/*!< Popup, if the graph is empty. */

  double zoom; /* (0,1] */

  GQueue/*<gchar*>*/ history; /*! Saves the graph names recently shown. Most
							   * recent first. */

  gboolean block_observer;	/*!< If TRUE, notifications from the displayed
							 * graph are ignored. */

  XGraphObserver graphObserver;

  MenuDomain * menu_domain;

  Label * label;
  LabelObserver labelObserver;
};



#endif /* GRAPHWINDOWIMPL_H_ */

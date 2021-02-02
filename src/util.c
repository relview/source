/*
 * util.c
 *
 *  Copyright (C) 2000,2008,2009,2010,2011 The Relview Maintainer,
 *  University of Kiel, Germany
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

#include <gtk/gtk.h>
#include "utilities.h"


/****************************************************************************/
/*       NAME : getCanvasOrigin                                             */
/*    PURPOSE : Returns the top left visible canvas pixel coordinates.      */
/*              Used in graphwindow_gtk.c and relationwindow_gtk.c          */
/* PARAMETERS : The horizontal and vertical scrollbar widgets.              */
/*    CREATED : 2000.05.02 WL                                               */
/*   MODIFIED : 2000.06.05 WL: introduced all parameters so that the        */
/*                             function can be used both from graph- and    */
/*                             relationwindow.                              */
/****************************************************************************/
GdkPoint getCanvasOrigin (GtkWidget * horz, GtkWidget * vert)
{
  GdkPoint origin;
  GtkAdjustment * adjust;

  adjust = gtk_range_get_adjustment (GTK_RANGE (horz));
  origin.x = (int) ceil(adjust->value);

  adjust = gtk_range_get_adjustment (GTK_RANGE (vert));
  origin.y = (int) ceil(adjust->value);

  return origin;
}


/****************************************************************************/
/*       NAME : getVisibleRect                                              */
/*    PURPOSE : returns a GdkRectangle of the drawingarea area              */
/* PARAMETERS : the drawingarea and the horizontal and vertical scrollbar   */
/*              widgets.                                                    */
/*    CREATED : 2000.05.12 WL                                               */
/*   MODIFIED : 2000.06.05 WL: introduced all parameters so that the        */
/*                             function can be used both from graph- and    */
/*                             relationwindow.                              */
/****************************************************************************/
GdkRectangle getVisibleRect (GtkWidget * drawingarea,
                             GtkWidget * horz, GtkWidget * vert)
{
  GdkRectangle vis_rect;
  GdkPoint origin;

  origin = getCanvasOrigin (horz, vert);
  vis_rect.x = origin.x;
  vis_rect.y = origin.y;
  vis_rect.width = GTK_WIDGET (drawingarea)->allocation.width;
  vis_rect.height = GTK_WIDGET (drawingarea)->allocation.height;

  return vis_rect;
}


char *error_msg [100] = {
  "Relation already exists [overwrite?]",
  "Graph already exists [overwrite?]",
  "Relation does not exist [press OK to continue]",
  "Do you really want to delete?",
  "Do you really want to delete ALL RELATIONS?",
  "Do you really want to delete ALL FUNCTIONS?",
  "Do you really want to delete ALL PROGRAMS?",
  "Do you really want to delete ALL DOMAINS?",
  "Do you really want to delete ALL LABELS?",
  "Relation-Selection is out of range!",
  "Error in Rel-Term",
  "Rel is not quadratic! [press OK to continue]",
  "No Rel-Inclusion! [press OK to continue]",
  "Rel and Graph are out of sync [overwrite rel?]",
  "Function already exists [overwrite?]",
  "Program already exists [overwrite?]",
  "Domain already exists [overwrite?]",
  "Label already exists [overwrite?]",
  "Do you really want to quit?",
  "Wrong input",
  "Name not allowed",
  "Operation not allowed with Relation $ [press OK to continue]",
  "Wrong Domain-definition (+ | X ) [press OK to continue]",
  "Error in basic-operation [press OK to continue]",
  "Syntax error [press OK to continue]",
  "Function not found [press OK to continue]",
  "Domain not found [press OK to continue]",
  "Function-Eval-Error [press OK to continue]",
  "Token-Mismatch [press OK to continue]",
  "Error: cannot create relation [press OK to continue]",
  "Error: cannot create function [press OK to continue]",
  "Error in relation-dimension [press OK to continue]",
  "File already exists! Overwrite existing file?",
  "Save-error! File not saved [press OK to continue]",
  "Save-error! Relation-Dimension too big! File not saved [press OK to continue]",
  "Load-error! Unable to load file [press OK to continue]",
  "Double-definition in prog-file [press OK to continue]",
  "Name-mismatch of relation and graph [press OK to continue]",
  "Graph and rel have not the same dimension [press OK to continue]",
  "Next component of graph? [press OK to continue]",
  "No vector or wrong dimension for marking nodes! [press OK to continue]",
  "Operation not allowed on correspondence graph [press OK to continue]",
  "Label-number bigger than Relation-Dimensions [press OK to continue]",
  "Relation already labeled [overwrite?]",
  "No such label for columns [press OK to continue]",
  "No such label for rows [press OK to continue]",
  "Graphalgorithm failed  [press OK to continue]"
};




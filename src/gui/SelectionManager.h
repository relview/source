#ifndef SELECTIONMANAGER_H
#  define SELECTIONMANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>

typedef gpointer Selectable;

typedef struct _SelectionClass SelectionClass;

typedef struct _SelectionManager SelectionManager;

/*interface*/ struct _SelectionClass
{
  gboolean (*isOver) (SelectionManager*,Selectable, gint x, gint y);
  void (*highlight) (SelectionManager*,Selectable);
  void (*unhighlight) (SelectionManager*,Selectable);
  void (*select) (SelectionManager*,Selectable);
  void (*unselect) (SelectionManager*,Selectable);

  gpointer (*begin) (SelectionManager*,SelectionClass*);
  void (*next) (gpointer*);
  gboolean (*hasNext) (gpointer);
  Selectable (*object) (gpointer);

  int priority; /* je hoeher die Prioritaet, desto eher wird geprueft, ob
                    * die Maus ueber dem Objekt ist. */
  gboolean multiple;
  
  gpointer data; /* user data */
};

typedef SelectionClass * SelectionClassId;

struct _SelectionManager
{
  SelectionClassId      (*registerClass) (SelectionManager*,
                                          const SelectionClass*);
  gboolean              (*mouseMoved) (SelectionManager*, GdkEventMotion*);
  gboolean              (*mouseLeftButtonPressed) (SelectionManager*,
                                                   GdkEventButton*);
  GList*                (*getSelection) (SelectionManager*,
                                         const SelectionClass**);
  void (*unref)         (SelectionManager*, Selectable);
  gboolean              (*hasSelection) (SelectionManager*);
  const SelectionClass * (*getSelectionClass) (SelectionManager*);
  gpointer              (*getData) (SelectionManager*);
  void                  (*setData) (SelectionManager*, gpointer);
  gboolean              (*isSelected) (SelectionManager*,Selectable);
  
  /* doesn't invoke the unselect callback of the selected objects. */
  void                  (*clearSelection) (SelectionManager*);

/* private: */
  GList * classes; /* sorted by priority */

  SelectionClass * selectedClass;
  GList * selected;
  
  Selectable * highlighted;
  SelectionClass * highlightedClass;

  gpointer data; /* user data */
};


SelectionManager * selection_manager_new ();
void selection_manager_destroy (SelectionManager * m);

#endif /* SelectionManager.h */

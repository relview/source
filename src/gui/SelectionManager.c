#include <gui/SelectionManager.h>

#include <string.h> /* memcpy */

typedef SelectionClassId ClassId;
typedef SelectionClass Class;
typedef SelectionManager Manager;

#warning TODO: Es muss hier geschaut werden, ob die Logik der Iteratoren so passt.

#define SM SelectionManager

#define _F(x) selection_manager_##x

ClassId _F(register_class) (Manager * m, const Class * class);
gboolean _F(mouse_moved) (Manager * m, GdkEventMotion * event);
gboolean _F(mouse_left_button_pressed) (Manager * m, GdkEventButton * event);


void _selection_class_free (gpointer class, gpointer foo)
{ free (class); }

void _F(destroy) (SelectionManager * m)
{
  g_list_foreach (m->selected, _selection_class_free, (gpointer) NULL);
  g_list_free (m->selected);

  g_list_foreach (m->classes, _selection_class_free, (gpointer) NULL);
  g_list_free (m->classes);
}

static
gint _class_prior_compare (gconstpointer a, gconstpointer b)
{
  return ((Class*)a)->priority < ((Class*)b)->priority;
}

static
Class * _selection_class_dup (const Class * class)
{
  Class * copy = (Class*) malloc (sizeof (Class));
  memcpy (copy, class, sizeof (Class));
  
  return copy;
}

/* we create a copy of the class. */
ClassId _F(register_class) (Manager * m, const Class * class)
{
  Class * copy = _selection_class_dup (class);

  m->classes = g_list_insert_sorted
    (m->classes, copy, _class_prior_compare);

  return copy;
}


static void _highlight (Manager * m, Selectable obj, Class * class);
static void _unhighlight (Manager * m);
static void _unselect_all (Manager * m);
static void _unselect (Manager * m, Selectable obj);
static void _select (Manager * m, Selectable obj, Class * class);
static void _select_too (Manager * m, Selectable obj);

static Selectable _get_object (Manager * m, int x, int y,
                               Class /*out*/ ** pclass)
{
  /* find the object the mouse is over, if there is one. */
  GList * classIter = g_list_first (m->classes);

  for ( ; classIter != NULL ; classIter = g_list_next (classIter)) {
    Class * class = (Class*) classIter->data;
    gpointer objIter = class->begin (m, class);

    *pclass = class;
    
    for ( ; class->hasNext (objIter) ; class->next (&objIter)) {
      Selectable obj = class->object (objIter);

      if (class->isOver (m, obj, x,y))
        return obj;
    }
  }

  return NULL;
}

gboolean _F(mouse_moved) (Manager * m, GdkEventMotion * event)
{
  Class * class;
  Selectable obj = _get_object (m, event->x, event->y, &class);

  if (obj) {
    if (m->highlighted == obj)
      return FALSE;
    else {
      _unhighlight (m);
      _highlight (m, obj, class);
      return TRUE;
    }
  }
  else {
    if (m->highlighted) {
      _unhighlight (m);
      return TRUE;
    }
    else
      return FALSE;
  }
}

gboolean _F(mouse_left_button_pressed) (Manager * m,
                                        GdkEventButton * event)
{
  Class * class;
  Selectable obj = _get_object (m, event->x, event->y, &class);

  if (obj) {
    /* if the object is already selected, unselect it */
    if (NULL != g_list_find (m->selected, obj)) {
      _unselect (m, obj);
    }
    else {
      if (class->multiple 
          && event->state & GDK_CONTROL_MASK
          && m->selected != NULL) {
        if (m->selectedClass != class) {
          return FALSE;
        }
        else
          _select_too (m, obj);
      }
      else {
        _unselect_all (m);
        _select (m, obj, class);
      }
    }

    return TRUE;
  }
  else {
    if (m->selected != NULL) {
      _unselect_all (m);
      return TRUE;
    }
    else
      return FALSE;
  }
}


/* returns a list, which must be freed after use with g_list_free(...) */
GList * _F(get_selection) (SM * m, const SelectionClass ** pSelClass)
{
  if (pSelClass)
    *pSelClass = m->selectedClass;
  return g_list_copy (m->selected);
}


static
void _highlight (Manager * m, Selectable obj, Class * class)
{
  m->highlightedClass = class;
  m->highlighted = obj;
  m->highlightedClass->highlight (m, m->highlighted);
}

static
void _unhighlight (Manager * m)
{
  if (m->highlighted) {
    m->highlightedClass->unhighlight (m, m->highlighted);
    m->highlighted = NULL;
  }
}

static
void _unselect_all (Manager * m)
{
  GList * iter = m->selected;

  for ( ; iter != NULL ; iter = g_list_next (iter))
    m->selectedClass->unselect (m, (Selectable) iter->data);

  g_list_free (m->selected);
  m->selected = NULL;
}

static void _unselect (Manager * m, Selectable obj)
{
  m->selectedClass->unselect (m, obj);
  m->selected = g_list_remove_link (m->selected,
                               g_list_find (m->selected, obj));
}

static
void _select (Manager * m, Selectable obj, Class * class)
{
  m->selectedClass = class;
  m->selected = g_list_prepend (m->selected, obj);
  class->select (m, obj);
}

static
void _select_too (Manager * m, Selectable obj)
{
  m->selected = g_list_prepend (m->selected, obj); 
  m->selectedClass->select (m, obj);
}

/* ensure, that the object is neither highlightes, nor selected. */
void _F(unref) (Manager * m, Selectable obj)
{
  if (m->highlighted == obj)
    _unhighlight (m);
  if (NULL != g_list_find (m->selected, obj)) {
    _unselect (m, obj);
  }
}

gboolean _F(hasSelection) (SelectionManager * m)
{
  return NULL != m->selected;
}

const SelectionClass * _F(getSelectionClass) (SelectionManager * m)
{
  if (NULL == m->selected)
    return NULL;
  else
    return m->selectedClass;
}

gpointer _F(getData) (SelectionManager * sm)
{
  return sm->data;
}

void _F(setData) (SelectionManager * sm, gpointer data)
{
  sm->data = data;
}

gboolean _F(isSelected) (SelectionManager * m, Selectable sel)
{
  return NULL != g_list_find (m->selected, (gpointer)sel);
}

void _F(clearSelection) (SelectionManager * m)
{
  g_list_free (m->selected);
  m->selected = NULL;
}

SelectionManager * _F(new) ()
{
  Manager * m = (Manager*) malloc (sizeof (SelectionManager));
  
  m->classes = NULL;
  
  m->selected = NULL;
  m->selectedClass = NULL;
    
  m->highlighted = NULL;
  m->highlightedClass = NULL;

  m->registerClass = _F(register_class);
  m->mouseMoved = _F(mouse_moved);
  m->mouseLeftButtonPressed = _F(mouse_left_button_pressed);
  m->getSelection = _F(get_selection);
  m->unref = _F(unref);
  m->hasSelection = _F(hasSelection);
  m->getSelectionClass = _F(getSelectionClass);
  m->setData = _F(setData);
  m->getData = _F(getData); 
  m->isSelected = _F(isSelected);
  m->clearSelection = _F(clearSelection);

  return m;
}


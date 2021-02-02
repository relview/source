/*
 * label.h
 *
 *  Copyright (C) 2010,2011 Stefan Bolus, University of Kiel, Germany
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

#ifndef _LABEL_H
#define _LABEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <glib.h>

typedef struct _Label Label;
typedef struct _LabelIter LabelIter;
typedef struct _LabelList LabelList;
typedef struct _LabelFunc LabelFunc;
typedef struct _LabelObserver LabelObserver;

struct _LabelObserver
{
	void (*onDelete) (gpointer, Label*);

	gpointer object; /*!< user-data*/
};

/*! Function-prototype for labeling functions. The first parameter is
 * the number stb. want a label for. The second and third parameter are
 * the buffer and it's size to store the resulting text into. Neither
 * buf may be NULL, nor may buf_size be 0, or larger than buf's
 * actual size! Prototypes MUST return buf. The function must be
 * totally defined on 1..INT_MAX. */
typedef const gchar * (*LabelRawFunc)(int /* number */,
                           GString * /* buf */,
                           gpointer user_data);


/*!
 * Returns the maximal length (in characters) of all labels in range
 * [first,last].
 */
typedef int (*LabelMaxLenFunc) (int first, int last, gpointer user_data);


/*!
 * If the label is persistent, a call to \ref label_destroy has no effect.
 *
 * \see label_set_persistent
 * \see label_is_persistent
 */
void 			label_destroy (Label * self);

void			label_set_persistent (Label * self, gboolean yesno);
gboolean		label_is_persistent (Label * self);
const gchar * 	label_get_nth (Label * self, int n, GString * s);
int				label_get_max_len (Label * self, int first, int last /*incl.*/);
const gchar * 	label_get_name (Label * self);
LabelIter *		label_iterator (Label * self);
void			label_register_observer (Label * self, LabelObserver * o);
void 			label_unregister_observer (Label * self, LabelObserver * o);

/*!
 * Creates a new empty label based on a list of assignment.
 *
 * \see label_list_add_assignment
 */
LabelList * 	label_list_new (const gchar * name);

/*!
 * Add an assignment to the list of assignments. Returns true if the element
 * was inserted and false it not. E.g. in case there is an assignment with the
 * given number already or number is negative.
 */
gboolean		label_list_add (LabelList * self, int number, const gchar * name);

LabelFunc * 	label_func_new (const gchar * name, LabelRawFunc func,
								LabelMaxLenFunc maxlen_func,
							    gpointer user_data, GFreeFunc user_dtor);

gboolean		label_iter_is_valid (LabelIter * self);
void			label_iter_next 	(LabelIter * self);
const gchar * 	label_iter_name 	(LabelIter * self, GString * s);
int				label_iter_number 	(LabelIter * self);
void			label_iter_seek 	(LabelIter * self, int n);
void			label_iter_destroy 	(LabelIter * self);



#ifdef __cplusplus
}
#endif


#if 0
/*abstract*/ struct _Label
{
	Label * parent; /* !<Must be the first element. */

	gchar * name;
	GSList/*<LabelObserver*>*/ * observers;

	const gchar 	(*getName)();
	const gchar * 	(*getNth)(int n, GString*);
	int 			(*getMaxLen)(int first, int last);
	LabelIter *		(*iterator)();
	GSList/*<GFreeFunc>*/* destroy; /* first: derived, last: base */

	void 			(registerObserver) (Label*,LabelObserver*);
	void 			(unregisterObserver) (Label*,LabelObserver*);
};

struct _LabelIter
{
	LabelIter * parent; /* !<Must be the first element. */

	int n;
	int last; /*!< Incl. */
	Label * label;

	gboolean 		(*end) 		(LabelIter*);
	void 			(*next) 	(LabelIter*);
	const gchar * 	(name) 		(LabelIter*,GString*);
	int 			(*number)	(LabelIter*); /*!< Returns n. */
};


struct _LabelAssignment
{
	int 	number;
	gchar * name;
};

struct _LabelList
{
	Label * parent; /* !<Must be the first element. */

	GList * assignments;
};


struct _LabelListIter
{
	LabelIter * parent; /* !<Must be the first element. */

	GList * iter;
};



struct _LabelFunc
{
	Label * parent; /*!< Must be the first element. */

	LabelRawFunc func;
	gpointer user_data;
	GFreeFunc user_dtor;
};

struct _LabelFuncIter
{
	LabelIter * parent; /* !<Must be the first element. */
};

/* Simple inheritance with only one level. Once you have casted down, there is
 * no way back. */
#define LABEL(derived) (((Label*)derived)->parent)


void 			label_destroy (Label * self);
const gchar * 	label_get_nth (Label * self, int n, GString * s);
int				label_get_max_len (Label * self, int first, int last /*incl.*/);
const gchar * 	label_get_name (Label * self);
LabelIter *		label_iterator (Label * self);


/*!
 * Creates a new empty label based on a list of assignment.
 *
 * \see label_list_add_assignment
 */
LabelList * 	label_list_new (const gchar * name);

/*!
 * Add an assignment to the list of assignments. Returns true if the element
 * was inserted and false it not. E.g. in case there is an assignment with the
 * given number already or number is negative.
 */
gboolean		label_list_add (LabelList * self, int number, const gchar * name);

LabelFunc * 	label_func_new (const gchar * name, LabelRawFunc func,
							    gpointer user_data, GFreeFunc user_dtor);
#endif

#if 0
//////////////////////////////////////////////////////////////

#include "Relation.h"
#include "global.h"

#include <assert.h>
#include <glib.h>

/*!
 * The concept of "labels" used here is quiet ambigous. A label is an
 * assignment of text to a col/row number and list of such pairs either.
 */

/*! Assigns a label to a col/row number.
 */
typedef struct {
  char name [MAXNAMELENGTH + 1]; /*!< the label name */
  int number; /*!< the col/row number */
} Label;

typedef struct l_list * l_listptr;

/*! Primitive singly-linked list of assignments of text
 * to a row/col number.
 *
 * This should be replaced by GLibs 'GSList' Data-Type.
 */
typedef struct l_list {
  Label label; /*!< the pair (or "label") */
  l_listptr next; /*!< pointer to the next pair (or "label") */
} l_listelem;


/*! Determines the type a label. If a label has type MANUALLY
 * the labels are given as a list of text/number pairs. The list's
 * root is located in the union in labellist struct, called label_root.
 * Otherwise a labeling-function is given, which assigns text to numbers
 * dynamically without upper bounds.
 */
typedef enum _label_type {
  LABEL_MANUALLY,
  LABEL_FUNCTION
} LabelType;

/*! Asserts that labelType is valid. */
#define LABEL_TYPE_UNKNOWN(labelType)                   \
{ 	 assert ((labelType) == LABEL_MANUALLY ||       \
		   (labelType) == LABEL_FUNCTION);      \
}

/* NOTE: If you change LabelFunc, you also have to change RvLabelFunc in
 *       the plugin interface. */



typedef struct labellist * labellistptr;

/*! A primitive singly-linked list of labels. Labels are either
 * lists of assignments of text to col or row numbers, or functions
 * for auto-labeling purposes.
 *
 * This should be replaced by GLibs 'GSList' Data-Type.
 */
typedef struct labellist {
  char name [MAXNAMELENGTH + 1]; /*!< the label's name/identifier */
  LabelType type; /*!< the type of the label */
  union {
    l_listptr label_root; /*!< the list of text assignments */
    LabelFunc label_func; /*!< the labeling-function */
  } label;
  void *user_data; /*!< user data for the labeling-function if it's one */
  void (*user_dtor)(void*); /*!< destructor for the user-data. No destr.
                                  * will get called, if NULL is given. */

  labellistptr next; /*!< pointer to the next label */
} labelelem;


/*! An iterator to traverse through a labels assignments one by one.
 */
typedef struct _LabelAssignmentIterator {
  int n;
  int last;
  labellistptr label;
  l_listptr labelIter;
} LabelAssignmentIterator;

LabelAssignmentIterator label_assignment_iterator_begin  (labellistptr,int);
gboolean                label_assignment_iterator_end    (const LabelAssignmentIterator*);
void                    label_assignment_iterator_next   (LabelAssignmentIterator*);
int                     label_assignment_iterator_number (const LabelAssignmentIterator*);
const char *            label_assignment_iterator_name   (const LabelAssignmentIterator*);
//const char *                label_assignment_iterator_name_threadsafe (const LabelAssignmentIterator &);

typedef struct rel_labellist * rel_labellistptr;

/*! A primitive singly-linked list of relation/label(col+row)
 * assignments
 *
 * This should be replaced by GLibs 'GSList' Data-Type.
 */
typedef struct rel_labellist {
  char name [MAXNAMELENGTH + 1]; /*!< relation name/identifier */
  char line_label [MAXNAMELENGTH + 1]; /*!< row label */
  char col_label [MAXNAMELENGTH + 1]; /*!< col label */
  rel_labellistptr next;
} rel_labelelem;


/* bekanntgabe des globalen labellistptr label_root fuer alle,  die Labels */
/* brauchen */

extern labellistptr label_root;
extern rel_labellistptr rel_label_root;

/* externe  Funktions-Prototypen */

extern labellistptr create_label_list (const gchar *);

/* Funktions-Prototypen */

const char * label_get_nth_name (labellistptr label, int n);
char * label_get_nth_name_threadsafe (labellistptr label, int n,
                                      char * buf, int bufsize);
const char * label_get_name (labellistptr label);
void label_get_rel_labels (const char * relName,
                           labellistptr * pRowLabel,
                           labellistptr * pColLabel);


labellistptr mk_label           (char *);
labellistptr appr_label         (labellistptr, labellistptr);
labellistptr get_label_list     (labellistptr, const char *);
labellistptr default_label_list ();
labellistptr del_label_list     (labellistptr);
labellistptr del_label          (labellistptr, char *);
labellistptr concat_label_lists (labellistptr, labellistptr);
labellistptr merge_label_lists  (labellistptr, labellistptr);
int label_exist                 (labellistptr, char *);

int del_label_item              (labellistptr, int);
Label * get_label               (l_listptr, int);
int add_label                   (labellistptr, int, char *);
int insert_label                (labellistptr, int, char *);
int label_item_exist            (l_listptr, int);
int get_number_of_labeldeflist_items (l_listptr);
int get_max_label_nr            (labellistptr);
//int get_max_label_len           (labellistptr);
int get_max_label_len2          (labellistptr ll, int lastIndex);

/* Verwaltung Rel -> Labels */

rel_labellistptr mk_rel_label   (char *, char *, char *);
rel_labellistptr appr_rel_label (rel_labellistptr, rel_labellistptr);
char * get_rel_line_label       (rel_labellistptr, char *);
char * get_rel_col_label        (rel_labellistptr, char *);
rel_labellistptr del_rel_label  (rel_labellistptr, char *);

/* bekanntgabe des globalen  label_root und rel_label_root */
/* fuer alle, die Labels brauchen */

extern labellistptr label_root;
extern rel_labellistptr rel_label_root;
#endif

#endif

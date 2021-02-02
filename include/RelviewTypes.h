/*
 * RelviewTypes.h
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

#ifndef RELVIEWTYPES_H_
#define RELVIEWTYPES_H_

#include <glib.h>
#include <gmp.h>


/* the given numbers are for backward-compatibility reasons.
 * Do only use the enumeration in newly written code.
 *
 * The GUI specs. (GtkBuilder) depend on the numeric values.
 */
typedef enum _DomainType
{
  DIRECT_PRODUCT = 1,
  DIRECT_SUM = 2
} DomainType;


typedef enum _RvObjectType
{
	RV_OBJECT_TYPE_RELATION,
	RV_OBJECT_TYPE_GRAPH,
	RV_OBJECT_TYPE_DOMAIN,
	RV_OBJECT_TYPE_FUNCTION,
	RV_OBJECT_TYPE_PROGRAM,
	RV_OBJECT_TYPE_LABEL,
	RV_OBJECT_TYPE_OTHER
} RvObjectType;


/*!
 * Object that stores global information on the running instance of
 * the Relview system. */
typedef struct _Relview Relview;


typedef void (*RelviewObserver_IOHandlersChangedFunc) (gpointer,Relview*);

typedef struct _RelviewObserver
{
	RelviewObserver_IOHandlersChangedFunc IOHandlersChanged;
	void (*labelAssocsChanged) (gpointer,Relview*);

	gpointer object;
} RelviewObserver;


/*!
 * Possible actions ro replace a single object. Use \ref RvReplacePolicy to
 * handle multiple objects at once.
 */
typedef enum _RvReplaceAction
{
	RV_REPLACE_ACTION_REPLACE,
	RV_REPLACE_ACTION_KEEP,
	RV_REPLACE_ACTION_ASK_USER
} RvReplaceAction;

/*!
 * Possible actions to replace at least one object. Use \ref RvReplaceAction to
 * handle a single object. Elements are compatible with \ref RvReplaceAction.
 */
typedef enum _RvReplacePolicy
{
	RV_REPLACE_POLICY_REPLACE  = RV_REPLACE_ACTION_REPLACE,
	RV_REPLACE_POLICY_KEEP     = RV_REPLACE_ACTION_KEEP,
	RV_REPLACE_POLICY_ASK_USER = RV_REPLACE_ACTION_ASK_USER,
	RV_REPLACE_POLICY_KEEP_ALL,
	RV_REPLACE_POLICY_REPLACE_ALL
} RvReplacePolicy;

#define RV_REPLACE_POLICY_TO_ACTION(policy) \
	(((policy) == RV_REPLACE_POLICY_KEEP_ALL) ? RV_REPLACE_ACTION_KEEP : \
	(((policy) == RV_REPLACE_POLICY_REPLACE_ALL) ? RV_REPLACE_ACTION_REPLACE : policy))


/*******************************************************************************
 *                                  RvObject                                   *
 *                                                                             *
 *                              Mon, 11 Oct 2010                               *
 ******************************************************************************/

typedef /*abstract*/ struct _RvObject RvObject;
typedef struct _RvObjectClass RvObjectClass;

#define RV_OBJECT(o) ((RvObject*)(o))

struct _RvObjectClass
{
	const gchar * (*getName) (RvObject*); /*!< Must return the non-NULL name. */
	void (*destroy) (RvObject*); /*!< Must not be NULL. */

	gchar * type_name; /*!< Human readable type name, e.g. "Relation", "Domain". */
};


/*abstract*/ struct _RvObject
{
	RvObjectClass * c; /*!< Must be the first member. */
};

#define rv_object_type_check(self, type) \
	((((RvObject*)(self))->c!=(type))?g_warning(__FILE__":%d: Invalid type " \
	"conversion to RvObjectClass \"%s\".", __LINE__, rv_object_class_get_type_name(type)):0, \
	((RvObject*)(self))->c==(type))


void 			rv_object_destroy (RvObject * self);
const gchar * 	rv_object_get_name (RvObject * self);
RvObjectClass *	rv_object_get_class (RvObject * self);

const gchar * 	rv_object_class_get_type_name (RvObjectClass * self);

#endif /* RELVIEWTYPES_H_ */

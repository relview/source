/*
 * Observer.h
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

#ifndef OBSERVER_H_
#define OBSERVER_H_

#if defined _0 || defined _1 || defined _2 || defined _3
#  warning Macros _1, _2 and/or _3 already defined.
#  undef _1
#  undef _2
#  undef _3
#endif
#define _0()
#define _1(a)   ,(a)
#define _2(a,b) ,(a),(b)
#define _3(a,b) ,(a),(b),(c)

/*!
 * \see OBSERVER_NOTIFY
 */
#define OBSERVER_NOTIFY_GLOBAL(l,LTYPE,TYPE,func,...) { \
		LTYPE * l_copy = g_slist_copy(l), *iter = NULL; \
        for ( iter = l_copy; iter ; iter = iter->next) { \
                TYPE * o = (TYPE*) iter->data; \
                if (o->func) { \
                        o->func (o->object __VA_ARGS__); \
                }} \
		g_slist_free (l_copy); \
	}


/*!
 * Skeleton for a notify routine which notifies all observers in the list
 * caller->l one in line with observer->func. Unfortunately, this is not as
 * trivial as it looks like because the list of observer can change during the
 * run due to arbitrary unregister calls by the current observer which are not
 * limited to this one.
 *
 * Observers which have registered during a notify message are ignored. Those
 * which unregister during a notification are still called, so be careful!
 *
 * \note Only \ref GSList is currently allowed for LTYPE.
 */
#define OBSERVER_NOTIFY(l,LTYPE,TYPE,caller,func,...) { \
        LTYPE * l_copy = g_slist_copy((caller)->l), *iter = NULL; \
        for ( iter = l_copy; iter ; iter = iter->next) { \
            TYPE * o = (TYPE*) iter->data; \
            if (o->func) { \
                o->func (o->object, (caller) __VA_ARGS__); \
            }} \
		g_slist_free (l_copy); \
	}

#endif /* OBSERVER_H_ */

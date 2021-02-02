#include "Relation.h"
#include "RelationProxyAdapter.h"
#include "Graph.h"
#include "prefs.h" // for rel_allow_display

#include <string.h>
#include <assert.h>

#include "Relview.h"
#include "Observer.h"


struct _RelManager
{
    KureContext * context;

    GHashTable/*<gchar*,Rel*>*/ * objs;

    GSList/*<RelManagerObserver*>*/ * observers;
    gint block_notify;

    /* is_local_ns indicates if we have to destroy the namespace. */
    Namespace * ns;
};

struct _RelIterator
{
    GHashTableIter iter;
    gboolean is_valid;
    Rel * cur;
};

struct _Relation
{
// private:
    RvObjectClass * c; /*!< Must be the first member. */

    RelManager * manager;
    gchar * name;
    KureRel * impl;

    gboolean is_hidden;

    GSList/*<RelationObserver>*/ * observers;
};

static RvObjectClass * _rel_class ();



void _rel_destroy (RvObject * self)
{
    if (rv_object_type_check (self, _rel_class())) {
        rel_destroy ((Rel*)self);
    }
}

static const gchar * _rel_get_name (RvObject * self)
{
    if (rv_object_type_check (self, _rel_class()))
        return rel_get_name((Rel*)self);
    else return NULL;
}

RvObjectClass * _rel_class ()
{
    static RvObjectClass * c = NULL;
    if (!c) {
        c = g_new0 (RvObjectClass,1);

        c->getName = _rel_get_name;
        c->destroy = _rel_destroy;
        c->type_name = g_strdup ("RelationClass");
    }

    return c;
}

/*!
 * NamedProfile adapters are used for \ref Namespace objects to provide a
 * unified interface to the managed objects.
 */
static NamedProfile _rel_named_profile = { (NamedProfile_getNameFunc)rel_get_name };

#define REL_MANAGER_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,RelManagerObserver,obj,func, __VA_ARGS__)


/*!
 * Not used as a callback for the observer. Instead, it is called directly by
 * the relation. Emits 'changed'. See rel_renamed.
 */
static void _rel_manager_on_renamed (void * object, Rel * rel, const gchar * old_name)
{
    RelManager * self = (RelManager*) object;
    const gchar * new_name = rel_get_name(rel);
    gpointer orig_key = NULL, value = NULL;

    /* We have to update our entry in the hash table to the new name. We
     * assume that the key doesn't already exist in the hash table. */
    if ( !g_hash_table_lookup_extended (self->objs, old_name, &orig_key, &value)) {
        g_warning ("_rel_manager_on_renamed: Missing rel \"%s\" "
                "in manager.\n", old_name);
    }
    else if (namespace_is_in (self->ns, new_name)
            || !namespace_is_valid_name(self->ns, new_name)) {
        g_warning ("_rel_manager_on_renamed: Name '%s' already in namespace. "
                "Relation removed.", new_name);
        rel_destroy(rel);
    }
    else {
        /* We may not remove the entry. This would cause the hash table to
         * destroy the rel. Thus, we have to steal and to insert it again. */
        g_assert ((gpointer)rel == value);

        g_hash_table_steal (self->objs, old_name);
        g_free (orig_key);

        g_assert (NULL == g_hash_table_lookup(self->objs, new_name));

        g_hash_table_insert (self->objs, g_strdup(new_name), rel);
        g_assert (rel == g_hash_table_lookup(self->objs, new_name));

        namespace_name_changed(rel_manager_get_namespace(self), rel);

        rel_manager_changed(self);
    }
}

void _rel_manager_dump (RelManager * self)
{
    GHashTableIter iter = {0};
    const gchar * key;
    Rel * rel = NULL;

    printf ("RelManager_______\n");
    g_hash_table_iter_init (&iter, self->objs);
    while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&rel)) {
        KureRel * impl = rel_get_impl (rel);
        printf ("\tRel '%s' => %p (%d x %d, '%s'), in namespace: %s\n", key, rel,
                kure_rel_get_rows_si(impl), kure_rel_get_cols_si(impl),
                rel_get_name(rel),
                namespace_is_in (rel_manager_get_namespace(self), key) ? "yes" : "no");
    }
}


/*!
 * This is the GDestroyFunc for the manager's hash table. See
 * \ref rel_manager_new_with_namespace. Must not be called if
 * the relation was removed from the hash table already,
 */
static void _rel_manager_destroy_rel (Rel * rel)
{
    RelManager * self = rel_get_manager(rel);
    //printf ("_rel_manager_destroy_rel: '%s'\n", rel_get_name(rel));
    //rel_manager_steal(self, rel);
    namespace_remove_by_name(rel_manager_get_namespace(self), rel_get_name(rel));
    rel->manager = NULL;
    rel_destroy (rel);
}

RelManager * rel_manager_new_with_namespace (KureContext * context, Namespace * ns)
{
    RelManager * self = g_new0 (RelManager,1);
    self->objs = g_hash_table_new_full (g_str_hash, g_str_equal,
            (GDestroyNotify)g_free, (GDestroyNotify) _rel_manager_destroy_rel);
    self->observers = NULL;
    self->block_notify = 0;
    self->context = context;
    kure_context_ref(context);

    if (NULL == ns) {
        /* Use local namespace for this manager. */
        self->ns = namespace_new ();
    }
    else self->ns = ns;

    namespace_ref (self->ns);

    return self;
}

RelManager * rel_manager_new (KureContext * context)
{
    return rel_manager_new_with_namespace (context, NULL);
}

void rel_manager_destroy (RelManager * self)
{
    REL_MANAGER_OBSERVER_NOTIFY(self,onDelete,_0());

    rel_manager_block_notify (self);
    g_hash_table_destroy (self->objs);
    g_slist_free (self->observers);
    rel_manager_unblock_notify (self);

    rel_manager_changed (self);
    kure_context_deref(self->context);

    namespace_unref (self->ns);

    g_free (self);
}

Rel * rel_manager_create_rel (RelManager * self, const char * name, mpz_t rows, mpz_t cols)
{
    KureRel * impl = kure_rel_new_with_size (self->context, rows, cols);
    if (impl) {
        Rel * rel = rel_new_from_impl(name, impl);
        if (rel) {
            if ( !rel_manager_insert (self, rel))
                rel_destroy(rel);
            else return rel;
        }
        else kure_rel_destroy(impl);
    }

    return NULL;
}

Rel * rel_manager_create_rel_si (RelManager * self, const char * name, int rows, int cols)
{
    Rel * ret;
    mpz_t _rows, _cols;
    mpz_init_set_si (_rows, rows);
    mpz_init_set_si (_cols, cols);
    ret = rel_manager_create_rel(self, name, _rows, _cols);
    mpz_clear (_rows);
    mpz_clear (_cols);
    return ret;
}

KureContext * rel_manager_get_context (RelManager * self) { return self->context; }

gboolean rel_manager_insert (RelManager * self, Rel * rel)
{
    const gchar * name = rel_get_name(rel);

    if (rel_get_manager (rel)) {
        g_warning ("rel_manager_insert: Relation \"%s\" already belongs "
                "to a manager. Not inserted!", rel_get_name(rel));
        return FALSE;
    }
    else if (kure_rel_get_context(rel->impl) != self->context) {
        g_warning ("rel_manager_insert: Relation \"%s\" has a different "
                "context than the destination manager. Not inserted!",
                rel_get_name(rel));
        return FALSE;
    }
    else if (namespace_insert (self->ns, (gpointer)rel, &_rel_named_profile)) {
        g_hash_table_insert (self->objs, g_strdup(name), rel);
        rel->manager = self;
        rel_manager_changed (self);

        return TRUE;
    }
    else return FALSE;
}

Rel * rel_manager_get_by_name (RelManager * self, const gchar * name) {
    return g_hash_table_lookup (self->objs, name);
}

gboolean rel_manager_delete_by_name (RelManager * self, const gchar * name)
{
    gboolean ret = g_hash_table_remove (self->objs, name);
    if (ret)
        rel_manager_changed (self);
    return ret;
}

GList/*<const gchar*>*/ * rel_manager_get_names (RelManager * self) {
    return g_hash_table_get_keys (self->objs); }

void rel_manager_steal (RelManager * self, Rel * d)
{
    if (self != rel_get_manager(d)) return;
    else if (g_hash_table_steal (self->objs, rel_get_name(d))) {
        namespace_remove_by_name(rel_manager_get_namespace(self), rel_get_name(d));
        d->manager = NULL;
        rel_manager_changed (self);
    }
}

gint rel_manager_steal_all (RelManager * self, RelManager * victim)
{
    if (self->context != victim->context) {
        g_warning ("rel_manager_steal_all: Managers have different "
                "KureContexts. Not allowed!");
        return 0;
    }
    else {
        GList/*<const gchar*>*/ * names = rel_manager_get_names (victim), *iter;
        gint stolenCount = 0;

        rel_manager_block_notify(self);
        rel_manager_block_notify(victim);

        for (iter = names ; iter ; iter = iter->next) {
            const gchar * name = (const gchar*) iter->data;
            if (! rel_manager_exists (self, name)) {
                Rel * rel = rel_manager_get_by_name (victim, name);
                rel_manager_steal (victim, rel);

                /* If we cannot insert the relation into the new \ref RelManager,
                 * push it back to the victim. */
                if ( !rel_manager_insert (self, rel))
                    g_assert(rel_manager_insert (victim, rel));
                else
                    stolenCount ++;
            }
        }

        rel_manager_unblock_notify(victim);
        rel_manager_unblock_notify(self);

        rel_manager_changed(victim);
        rel_manager_changed(self);

        g_list_free(names);
        return stolenCount;
    }
}

gboolean rel_manager_exists (RelManager * self, const gchar * name)
{ return g_hash_table_lookup (self->objs, name) != NULL; }

void rel_manager_register_observer (RelManager * self, RelManagerObserver * o)
{
    if (NULL == g_slist_find (self->observers, o)) {
        self->observers = g_slist_prepend (self->observers, o);
    }
}

void rel_manager_unregister_observer (RelManager * self, RelManagerObserver * o)
{ self->observers = g_slist_remove (self->observers, o); }


void rel_manager_changed (RelManager * self)
{ REL_MANAGER_OBSERVER_NOTIFY(self,changed,_0()); }

void rel_manager_block_notify (RelManager * self)
{ self->block_notify ++; }

void rel_manager_unblock_notify (RelManager * self)
{ self->block_notify = MAX(0, self->block_notify-1); }

gboolean rel_manager_is_empty (RelManager * self) { return g_hash_table_size(self->objs) == 0; }
gint rel_manager_size (RelManager * self) { return (gint) g_hash_table_size(self->objs); }
gint rel_manager_delete_with_filter (RelManager * self,
        gboolean (*filter) (Rel*,gpointer), gpointer user_data)
{
    GHashTableIter iter;
    gint deleteCount = 0;
    Rel * cur;

    rel_manager_block_notify(self);
    g_hash_table_iter_init(&iter, self->objs);
    while (g_hash_table_iter_next(&iter, NULL, (gpointer*)&cur)) {
        if (filter(cur,user_data)) {
            g_hash_table_iter_remove (&iter);
            deleteCount ++;
        }
    }
    rel_manager_unblock_notify(self);

    if (deleteCount > 0)
        rel_manager_changed(self);

    return deleteCount;
}

gboolean rel_manager_is_valid_name (RelManager * self, const gchar * name)
{
    return namespace_is_valid_name(rel_manager_get_namespace(self), name);
}

Namespace * rel_manager_get_namespace (RelManager * self) { return self->ns; }

gboolean rel_manager_is_insertable_name (RelManager * self, const gchar * name)
{
    Namespace * ns = rel_manager_get_namespace(self);
    return namespace_is_valid_name(ns, name) && !namespace_is_in(ns, name);
}

RelIterator * rel_manager_iterator (RelManager * self)
{
    RelIterator * iter = g_new0 (RelIterator,1);
    g_hash_table_iter_init (&iter->iter, self->objs);
    iter->is_valid = g_hash_table_iter_next (&iter->iter, NULL, (gpointer*) &iter->cur);
    return iter;
}
void rel_iterator_next (RelIterator * self) {
    self->is_valid = g_hash_table_iter_next
            (&self->iter, NULL, (gpointer*)&self->cur);
}
gboolean rel_iterator_is_valid (RelIterator * self) { return self->is_valid; }
Rel * rel_iterator_get (RelIterator * self) { return self->cur; }
void rel_iterator_destroy (RelIterator * self) { g_free (self); }


/*******************************************************************************
 *                                  Relation                                   *
 *                                                                             *
 *                               Fri, 9 Apr 2010                               *
 ******************************************************************************/

#define REL_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,RelObserver,obj,func, __VA_ARGS__)


void _rel_dtor (Rel * self)
{
    REL_OBSERVER_NOTIFY(self, onDelete);

    if (self->manager)
        rel_manager_steal (self->manager, self);

    g_free (self->name);
    kure_rel_destroy(self->impl);

    g_slist_free (self->observers);
}

void rel_destroy (Rel * self)
{
    _rel_dtor (self);
    g_free (self);
}

KureRel * rel_steal_impl (Rel * self)
{
    KureRel * impl = self->impl;
    self->impl = kure_rel_new(kure_rel_get_context(impl));
    rel_changed(self);
    return impl;
}

gboolean rel_allow_display (Rel * rel)
{
    KureRel * impl = rel_get_impl(rel);

    if ( !kure_rel_fits_si(impl)) return FALSE;
    else {
        int n = prefs_get_int("settings", "relation_display_limit", 2 << 26);

        return (kure_rel_get_rows_si(impl) <= n)
            && (kure_rel_get_cols_si(impl) <= n);
    }
}

gboolean rel_same_dim (const Rel * a, const Rel * b)
{
    return kure_rel_same_dim(a->impl, b->impl);
}

gboolean rel_rename (Rel * self, const gchar * new_name)
{
    if ( !self || !new_name) return FALSE;
    else if (rel_has_name(self, new_name)) return TRUE;
    else {
        RelManager * manager = rel_get_manager(self);

        if (manager && ! rel_manager_is_insertable_name(manager, new_name)) {
            printf ("rel '%s' (new_name: '%s') is not insertable into namespace.\n",
                    rel_get_name(self), new_name);
            return FALSE;
        }
        else {
            gchar * old_name = self->name;

            self->name = g_strdup (new_name);

            /* At this point, it is a bad idea to update the relation manager
             * indirectly using the observers. Other observers could be called
             * first, so they would get outdated information from the manager.
             * It's better to keep the relation between the relation and their
             * managers tight. */
            if (manager)
                _rel_manager_on_renamed (manager, self, old_name);

            REL_OBSERVER_NOTIFY(self, renamed, _1(old_name));
            g_free (old_name);
            return TRUE;
        }
    }
}

void rel_get_rows (const Rel * self, mpz_t rows) {
    kure_rel_get_rows(self->impl, rows);
}
void rel_get_cols (const Rel * self, mpz_t cols) {
    kure_rel_get_cols(self->impl, cols);
}

RelManager * rel_get_manager (Rel * self) { return self->manager; }


gboolean rel_is_valid_name (const gchar * name, RelManager * manager)
{
    return rel_manager_is_valid_name(manager, name);
}


/*!
 * Adds a observer to the relation, if it is not already registered.
 */
void rel_register_observer (Rel * obj, RelationObserver * observer)
{
    if (NULL == g_slist_find (obj->observers, observer)) {
        obj->observers = g_slist_prepend (obj->observers, observer);
    }
}

void rel_unregister_observer (Rel * obj, RelationObserver * observer)
{
    obj->observers = g_slist_remove (obj->observers, observer);
}


Rel * rel_new_from_impl (const gchar * name, KureRel * impl)
{
    Rel * self = g_new0 (Rel, 1);
    self->c = _rel_class();
    self->name = g_strdup (name);
    self->impl = impl;
    self->is_hidden = FALSE;

    return self;
}

KureRel * rel_get_impl (Rel * self) { return self->impl; }
const gchar * rel_get_name (const Rel * self) { return self->name; }
gboolean rel_has_name (const Rel * self, const gchar * name) {
    return g_str_equal(self->name, name);
}

void rel_changed (Rel * self) { REL_OBSERVER_NOTIFY(self,changed,_0()); }
gboolean rel_is_hidden (const Rel * self) { return self->is_hidden; }
void rel_set_hidden (Rel * self, gboolean yesno) { self->is_hidden = yesno; }


Rel * rel_assign (Rel * self, const Rel * src)
{
    if (!self || !src) return self;
    else {
        KureRel * impl_copy = kure_rel_new_copy (src->impl);
        if (! impl_copy) {
            g_warning ("rel_assign: Unable to copy source KureRel object.");
        }
        else {
            kure_rel_destroy(self->impl);
            self->impl = impl_copy;
        }

        return self;
    }
}
/*!
 * Constructor for a relation from a graph.
 *
 * \author stb
 * \data 09.04.2010
 *
 * \param context The context which shall be used to create the relation.
 * \param gr The graph to create a relation for.
 * \return Returns a newly created relation which corresponds to the given
 *         graph.
 */

Rel * rel_new_from_xgraph (KureContext * context, XGraph * gr)
{
    int n = xgraph_get_node_count (gr);
    KureRel * impl = kure_rel_new_with_size_si(context, n,n);

    if ( !impl) return NULL;
    else {
        Rel * rel = rel_new_from_impl(xgraph_get_name(gr), impl);
        if ( !rel) {
            kure_rel_destroy(impl);
            return NULL;
        }
        else {
            RelationProxy * rp = relation_proxy_adapter_new (rel);

            XGRAPH_FOREACH_EDGE(gr,edge,iter,{
                    gint from = atoi (xgraph_node_get_name(xgraph_edge_get_from_node(edge)));
                    gint to   = atoi (xgraph_node_get_name(xgraph_edge_get_to_node(edge)));

                    /* Remark: FROM is the row, while TO is the column; both
                     *         are 1-indexed. */
                    rp->setBit (rp, to - 1, from - 1);
            });

            return rel;
        }
    }
}
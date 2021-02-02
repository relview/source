#ifndef RELATIONPROXY_H
#  define RELATIONPROXY_H

#include <gtk/gtk.h>
#include "Relation.h"

typedef struct _RelationProxy RelationProxy;

/* The user must give the original RelationProxy to the functions.
 * Not the RelationProxy.object structure element. The virtual
 * functions will get access through the proxy to their data. */

typedef gboolean (*relation_proxy_get_bit_func_t)
  (RelationProxy*, int, int);
typedef void (*relation_proxy_set_bit_func_t)
  (RelationProxy*, int, int);
typedef void (*relation_proxy_clear_bit_func_t)
  (RelationProxy*, int, int);
typedef void (*relation_proxy_get_bits_rect_func_t)
  (RelationProxy*, int, int, int, int, gboolean*);

typedef gboolean (*relation_proxy_mp_get_bit_func_t)
  (RelationProxy*, mpz_t, mpz_t);
typedef void (*relation_proxy_mp_set_bit_func_t)
  (RelationProxy*, mpz_t, mpz_t);
typedef void (*relation_proxy_mp_clear_bit_func_t)
  (RelationProxy*, mpz_t,mpz_t);
typedef void (*relation_proxy_mp_get_bits_rect_func_t)
  (RelationProxy*, mpz_t, mpz_t, int, int, gboolean*);

typedef void (*relation_proxy_destroy_func_t) (RelationProxy*);

#if 0
#define RELATION_PROXY_GET_BIT_FUNC(f) \
        ((relation_proxy_get_bit_func_t)(f))
#define RELATION_PROXY_SET_BIT_FUNC(f) \
        ((relation_proxy_set_bit_func_t)(f))
#define RELATION_PROXY_CLEAR_BIT_FUNC(f) \
        ((relation_proxy_clear_bit_func_t)(f))
#define RELATION_PROXY_GET_BITS_RECT_FUNC(f) \
        ((relation_proxy_get_bits_rect_func_t)(f))
#define RELATION_PROXY_DESTROY_FUNC(f) \
        ((relation_proxy_destroy_func_t)(f))
#endif

/*interface*/ struct _RelationProxy
{
  /* integer interface */
  relation_proxy_get_bit_func_t            getBit;
  relation_proxy_set_bit_func_t            setBit;
  relation_proxy_clear_bit_func_t          clearBit;
  relation_proxy_get_bits_rect_func_t      getBitsRect;

  /* Kure_MINT interface */
  relation_proxy_mp_get_bit_func_t         mp_getBit;
  relation_proxy_mp_set_bit_func_t         mp_setBit;
  relation_proxy_mp_clear_bit_func_t       mp_clearBit;
  relation_proxy_mp_get_bits_rect_func_t   mp_getBitsRect;

  relation_proxy_destroy_func_t            destroy;

  gpointer object;
};

#endif /* RelationProxy.h */

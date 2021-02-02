#include "RelationProxyAdapter.h"

#define _PUBLIC(name) relation_proxy_adapter_##name
#define _PRIV(name) _relation_proxy_adapter_##name

/* -------------------------------------------- Integer Interface Adapter --- */

static gboolean _PRIV(get_bit) (RelationProxy * proxy,
                                 int col, int row)
{
  RelationProxyAdapter * self = (RelationProxyAdapter*)proxy->object;
  return kure_get_bit_fast_si(rel_get_impl(self->rel), row, col,
		  self->varsRow, self->varsCol);
}

static void _PRIV(set_bit) (RelationProxy * proxy,
                             int col, int row)
{
  RelationProxyAdapter * self = (RelationProxyAdapter*)proxy->object;
  kure_set_bit_si(rel_get_impl(self->rel), TRUE, row, col);
}


static void _PRIV(clear_bit) (RelationProxy * proxy,
                               int col, int row)
{
  RelationProxyAdapter * self = (RelationProxyAdapter*)proxy->object;
  kure_set_bit_si(rel_get_impl(self->rel), FALSE, row, col);
}


static void _PRIV(get_bits_rect) (RelationProxy * proxy,
                                   int col, int row,
                                   int width, int height,
                                   gboolean * /*out*/ data)
{
  RelationProxyAdapter * self = (RelationProxyAdapter*)proxy->object;
  Rel * rel = ((RelationProxyAdapter*)proxy->object)->rel;
  int i,j;
  KureRel * impl = rel_get_impl(rel);


  for (i = 0 ; i < height ; i ++)
    for (j = 0 ; j < width ; j ++) {
      data[i*width+j]
           = kure_get_bit_fast_si(impl, i+row, j+col, self->varsRow, self->varsCol);
    }
}


/* ------------------------------------- GMP (mp) Interface Adapter --- */

static gboolean _PRIV(mp_get_bit) (RelationProxy * proxy,
                                    mpz_t col, mpz_t row)
{
  Rel * rel = ((RelationProxyAdapter*)proxy->object)->rel;
  RelationProxyAdapter * self = (RelationProxyAdapter*)proxy->object;

  return kure_get_bit_fast(rel_get_impl(rel), row, col,
		  self->varsRow, self->varsCol);
}

static void _PRIV(mp_set_bit) (RelationProxy * proxy,
		mpz_t col, mpz_t row)
{
	Rel * rel = ((RelationProxyAdapter*)proxy->object)->rel;
	kure_set_bit(rel_get_impl(rel), TRUE, row, col);
}


static void _PRIV(mp_clear_bit) (RelationProxy * proxy,
		mpz_t col, mpz_t row)
{
	Rel * rel = ((RelationProxyAdapter*)proxy->object)->rel;
	kure_set_bit(rel_get_impl(rel), FALSE, row, col);
}


static void _PRIV(mp_get_bits_rect) (RelationProxy * proxy,
									  mpz_t col, mpz_t row,
                                      int width, int height,
                                      gboolean * /*out*/ data)
{
	printf ("mp_get_bits_rect: NOT IMPLEMENTED\n");
#if 0
  Rel * rel = ((RelationProxyAdapter*)proxy->object)->rel;
  int vars_zeile = Kure_mp_number_of_vars (rel->hoehe),
    vars_spalte = Kure_mp_number_of_vars (rel->breite);
  Kure_MINT * x = Kure_mp_itom (0),
    * y = Kure_mp_itom (0);
  Kure_MINT * nil = Kure_mp_itom (0);
  int i,j;

  Kure_mp_copy (col, x);
  Kure_mp_copy (row, y);

  for (i = 0 ; i < height ; i ++) {
    Kure_mp_copy (nil, col);

    for (j = 0 ; j < width ; j ++) {
      data[i*width+j] = mp_get_rel_bit (rel, x, y, vars_zeile, vars_spalte);
      Kure_mp_incr (x);
    }

    Kure_mp_incr (y);
  }

  Kure_mp_mfree (nil);
  Kure_mp_mfree (x);
  Kure_mp_mfree (y);
#endif
}


static void _PRIV(rel_changed) (RelationProxyAdapter * self, Rel * rel)
{
	if (rel == self->rel && ! self->deleted) {
		KureRel * impl = rel_get_impl(rel);
		self->varsRow      = kure_rel_get_vars_rows(impl);
		self->varsCol      = kure_rel_get_vars_cols(impl);
	}
}

static void _PRIV(on_rel_delete) (  RelationProxyAdapter * self, Rel * rel)
{
	if (rel == self->rel) self->deleted = TRUE;
}

/* ---------------------------------------------- Ctor and Dtor Functions --- */

/*! Destroy the given relation proxy adapter given in `proxy`.
 *
 * \author stb
 * \param proxy The proxy, created with `relation_proxy_adapter_new`.
 */
void _PUBLIC(destroy) (RelationProxy * proxy)
{
	RelationProxyAdapter * self = (RelationProxyAdapter*) proxy->object;
	if (! self->deleted)
		relation_unregister_observer(self->rel, &self->observer);

	free (proxy->object);
	free (proxy);
}


/*! Create a new relation proxy adapter for the given relation `rel`.
 *
 * \warning The relation must NOT change it's size while it is referenced
 *          by a proxy adapter.
 *
 * \author stb
 * \param rel The relation to adapt.
 * \return Returns the newly allocated adapter object.
 */
RelationProxy * _PUBLIC(new) (Rel * rel)
{
  RelationProxy * proxy = (RelationProxy*) calloc (1, sizeof (RelationProxy));
  RelationProxyAdapter * adapter
    = (RelationProxyAdapter*) calloc (1, sizeof (RelationProxyAdapter));
  KureRel * impl = rel_get_impl(rel);

  proxy->object         = (gpointer*) adapter;

  proxy->getBit         = _PRIV(get_bit);
  proxy->setBit         = _PRIV(set_bit);
  proxy->clearBit       = _PRIV(clear_bit);
  proxy->getBitsRect    = _PRIV(get_bits_rect);

  proxy->mp_getBit      = _PRIV(mp_get_bit);
  proxy->mp_setBit      = _PRIV(mp_set_bit);
  proxy->mp_clearBit    = _PRIV(mp_clear_bit);
  proxy->mp_getBitsRect = _PRIV(mp_get_bits_rect);

  proxy->destroy        = _PUBLIC(destroy);

  adapter->rel          = rel;
  adapter->varsRow      = kure_rel_get_vars_rows(impl);
  adapter->varsCol      = kure_rel_get_vars_cols(impl);

  //adapter->relWidth     = Kure_mp_mint2int (rel->breite);
  //adapter->relHeight    = Kure_mp_mint2int (rel->hoehe);

  /* The adapter depends massively on the size of the associated
   * relation. Hence, if the dimension of the relation changes,
   * we have to adjust its size here. */
  adapter->observer.renamed  = NULL;
  adapter->observer.changed  = RELATION_OBSERVER_CHANGED_FUNC(_PRIV(rel_changed));
  adapter->observer.onDelete = RELATION_OBSERVER_ON_DELETE_FUNC(_PRIV(on_rel_delete));
  adapter->observer.object   = (gpointer) adapter;
  relation_register_observer(rel, &adapter->observer);
  adapter->deleted		= FALSE;

  return proxy;
}

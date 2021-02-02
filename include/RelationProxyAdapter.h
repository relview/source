#ifndef RELATIONPROXYADAPTER_H
#  define RELATIONPROXYADAPTER_H

#include "RelationProxy.h"

/*class*/ typedef struct _RelationProxyAdapter
{
  Rel * rel;

  int varsRow, varsCol;
  //int relWidth, relHeight;
  
  RelationObserver observer;
  gboolean deleted; /*! TRUE, if the associated relation was deleted.*/
} RelationProxyAdapter;

RelationProxy *         relation_proxy_adapter_new      (Rel*);
void                    relation_proxy_adapter_destroy  (RelationProxy*);

#endif /* RelationProxyAdapter.h */

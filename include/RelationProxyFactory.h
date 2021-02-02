#ifndef RELATIONVIEWPORTFACTORY_H
#  define RELATIONVIEWPORTFACTORY_H

#include <gtk/gtk.h> /* gpointer */
#include "RelationProxy.h"

typedef struct _RelationProxyFactory RelationProxyFactory;

typedef RelationProxy* (*RelationProxyFactoryFunc)
  (RelationProxyFactory*, Rel*);

typedef void (*RelationProxyFactoryDestroyFunc) (RelationProxyFactory*);

struct _RelationProxyFactory
{
  RelationProxyFactoryFunc          createProxy;
  RelationProxyFactoryDestroyFunc   destroy;

  gpointer object;
};

#endif /* RelationProxyFactory.h */

/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001, 2002 Sun Microsystems Inc.,
 * Copyright 2001, 2002 Ximian, Inc., Ximian Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* desktop.c: implements SpiDesktop.idl */

#include <config.h>
#include <stdio.h>
#include <libbonobo.h>
#include "desktop.h"

/* SpiDesktop signals */
enum {
  APPLICATION_ADDED,
  APPLICATION_REMOVED,  
LAST_SIGNAL
};
static guint spi_desktop_signals[LAST_SIGNAL];


/* Our parent Gtk object type */
#define PARENT_TYPE SPI_ACCESSIBLE_TYPE

typedef struct {
	SpiDesktop *desktop;
	Accessibility_Application ref;
} Application;

/* A pointer to our parent object class */
static SpiAccessibleClass *parent_class;

static void
spi_desktop_init (SpiDesktop *desktop)
{
  spi_base_construct (SPI_BASE (desktop), g_object_new (ATK_TYPE_OBJECT, NULL));

  desktop->applications = NULL;
  bonobo_object_set_immortal (BONOBO_OBJECT (desktop), TRUE);

  atk_object_set_name (ATK_OBJECT (SPI_BASE (desktop)->gobj), "main");
}

static void
spi_desktop_dispose (GObject *object)
{
  SpiDesktop *desktop = (SpiDesktop *) object;

  while (desktop->applications)
    {
      Application *app = desktop->applications->data;
      g_assert (app->ref != CORBA_OBJECT_NIL);
      spi_desktop_remove_application (desktop, app->ref);
    }

  G_OBJECT_CLASS (parent_class)->dispose (object); 
}

static CORBA_long
impl_desktop_get_child_count (PortableServer_Servant servant,
                              CORBA_Environment     *ev)
{
  SpiDesktop *desktop = SPI_DESKTOP (bonobo_object_from_servant (servant));

  if (desktop->applications)
    {
      return g_list_length (desktop->applications);
    }
  else
    {
      return 0;
    }
}

static Accessibility_Accessible
impl_desktop_get_child_at_index (PortableServer_Servant servant,
                                 const CORBA_long       index,
                                 CORBA_Environment     *ev)
{
  SpiDesktop  *desktop = SPI_DESKTOP (bonobo_object_from_servant (servant));
  CORBA_Object retval;
  Application *app;

  app = g_list_nth_data (desktop->applications, index);

  if (app)
    {
      retval = bonobo_object_dup_ref (app->ref, ev);
      if (BONOBO_EX (ev))
        {
	  retval = CORBA_OBJECT_NIL;
	}
    }
  else
    {
      retval = CORBA_OBJECT_NIL;
    }

  return (Accessibility_Accessible) retval;
}

static void
spi_desktop_class_init (SpiDesktopClass *klass)
{
  GObjectClass * object_class = (GObjectClass *) klass;
  SpiAccessibleClass * spi_accessible_class = (SpiAccessibleClass *) klass;
  POA_Accessibility_Accessible__epv *epv = &spi_accessible_class->epv;

  object_class->dispose = spi_desktop_dispose;
  
  parent_class = g_type_class_ref (SPI_ACCESSIBLE_TYPE);

  spi_desktop_signals[APPLICATION_ADDED] =
    g_signal_new ("application_added",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (SpiDesktopClass, application_added),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__UINT,
		  G_TYPE_NONE,
		  1, G_TYPE_UINT);
  spi_desktop_signals[APPLICATION_REMOVED] =
    g_signal_new ("application_removed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (SpiDesktopClass, application_removed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__UINT,
		  G_TYPE_NONE,
		  1, G_TYPE_UINT);
  epv->_get_childCount = impl_desktop_get_child_count;
  epv->getChildAtIndex = impl_desktop_get_child_at_index;
}

BONOBO_TYPE_FUNC_FULL (SpiDesktop,
		       Accessibility_Desktop,
		       PARENT_TYPE,
		       spi_desktop);

SpiDesktop *
spi_desktop_new (void)
{
  SpiDesktop *retval = g_object_new (SPI_DESKTOP_TYPE, NULL);

  return retval;
}

static void
abnormal_application_termination (gpointer object, Application *app)
{
  g_return_if_fail (SPI_IS_DESKTOP (app->desktop));

  spi_desktop_remove_application (app->desktop, app->ref);
}

void
spi_desktop_add_application (SpiDesktop *desktop,
			     const Accessibility_Application application)
{
  CORBA_Environment ev;
  Application       *app;
  Accessibility_Application ref;

  g_return_if_fail (SPI_IS_DESKTOP (desktop));

  spi_desktop_remove_application (desktop, application);

  CORBA_exception_init (&ev);

  ref = bonobo_object_dup_ref (application, &ev);

  if (!BONOBO_EX (&ev))
    {
      app = g_new (Application, 1);
      app->desktop = desktop;
      app->ref = ref;

      desktop->applications = g_list_append (desktop->applications, app);

      ORBit_small_listen_for_broken (
	      app->ref, G_CALLBACK (abnormal_application_termination), app);

      g_signal_emit (G_OBJECT (desktop),
		     spi_desktop_signals[APPLICATION_ADDED], 0,
		     g_list_index (desktop->applications, app));
    }

  CORBA_exception_free (&ev);
}

void
spi_desktop_remove_application (SpiDesktop *desktop,
				const Accessibility_Application app_ref)
{
  guint idx;
  GList *l;
  CORBA_Environment ev;

  g_return_if_fail (app_ref != CORBA_OBJECT_NIL);
  g_return_if_fail (SPI_IS_DESKTOP (desktop));

  CORBA_exception_init (&ev);

  idx = 0;
  for (l = desktop->applications; l; l = l->next)
    {
      Application *app = (Application *) l->data;

      if (CORBA_Object_is_equivalent (app->ref, app_ref, &ev))
        {
	  break;
	}
      idx++;
    }

  CORBA_exception_free (&ev);

  if (l)
    {
      Application *app = (Application *) l->data;

      desktop->applications = g_list_delete_link (desktop->applications, l);

      ORBit_small_unlisten_for_broken (app->ref, G_CALLBACK (abnormal_application_termination));
      bonobo_object_release_unref (app->ref, NULL);
      g_free (app);
      
      g_signal_emit (G_OBJECT (desktop), spi_desktop_signals[APPLICATION_REMOVED], 0, idx);
    }
}

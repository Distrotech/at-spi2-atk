/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001 Sun Microsystems Inc.
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

/*
 *
 * Basic SPI initialization and event loop function prototypes
 *
 */
#include <string.h>
#include <stdlib.h>
#include <cspi/spi-private.h>

#undef DEBUG_OBJECTS

static CORBA_Environment ev = { 0 };
static Accessibility_Registry registry = CORBA_OBJECT_NIL;
static GHashTable *live_refs = NULL;

static guint
cspi_object_hash (gconstpointer key)
{
  CORBA_Object object = (CORBA_Object) key;

  return CORBA_Object_hash (object, 0, &ev);
}

static gboolean
cspi_object_equal (gconstpointer a, gconstpointer b)
{
  CORBA_Object objecta = (CORBA_Object) a;
  CORBA_Object objectb = (CORBA_Object) b;

  return CORBA_Object_is_equivalent (objecta, objectb, &ev);
}

static void
cspi_object_release (gpointer value)
{
  Accessible *a = (Accessible *) value;

#ifdef DEBUG_OBJECTS
  g_print ("releasing %p => %p\n", a, a->objref);
#endif

  if (!a->on_loan)
    {
      cspi_release_unref (a->objref);
    }

  memset (a, 0xaa, sizeof (Accessible));
  a->ref_count = -1;

#ifndef DEBUG_OBJECTS
  free (a);
#endif
}

SPIBoolean
cspi_accessible_is_a (Accessible *accessible,
		      const char *interface_name)
{
  SPIBoolean        retval;
  Bonobo_Unknown unknown;

  if (accessible == NULL)
    {
      return FALSE;
    }

  unknown = Bonobo_Unknown_queryInterface (CSPI_OBJREF (accessible),
					   interface_name, cspi_ev ());

  if (ev._major != CORBA_NO_EXCEPTION)
    {
      g_error ("Exception '%s' checking if is '%s'",
	       cspi_exception_get_text (),
	       interface_name);
    }

  if (unknown != CORBA_OBJECT_NIL)
    {
      retval = TRUE;
      cspi_release_unref (unknown);
    }
  else
    {
      retval = FALSE;
    }

  return retval;
}

static GHashTable *
cspi_get_live_refs (void)
{
  if (!live_refs) 
    {
      live_refs = g_hash_table_new_full (cspi_object_hash,
					 cspi_object_equal,
					 NULL,
					 cspi_object_release);
    }
  return live_refs;
}

CORBA_Environment *
cspi_ev (void)
{
  return &ev;
}

Accessibility_Registry
cspi_registry (void)
{
  if (!cspi_ping (registry))
    {
      registry = cspi_init ();
    }
  return registry;
}

SPIBoolean
cspi_exception (void)
{
  SPIBoolean retval;

  if (ev._major != CORBA_NO_EXCEPTION)
    {
      CORBA_exception_free (&ev);
      retval = TRUE;
    }
  else
    {
      retval = FALSE;
    }

  return retval;
}

/*
 *   This method swallows the corba_object BonoboUnknown
 * reference, and returns an Accessible associated with it.
 * If the reference is loaned, it means it is only valid
 * between a borrow / return pair.
 */
static Accessible *
cspi_object_get_ref (CORBA_Object corba_object, gboolean on_loan)
{
  Accessible *ref;

  if (corba_object == CORBA_OBJECT_NIL)
    {
      ref = NULL;
    }
  else if (!cspi_check_ev ("pre method check: add"))
    {
      ref = NULL;
    }
  else
    {
      if ((ref = g_hash_table_lookup (cspi_get_live_refs (), corba_object)))
        {
          g_assert (ref->ref_count > 0);
	  ref->ref_count++;
	  if (!on_loan)
	    {
	      if (ref->on_loan) /* Convert to a permanant ref */
		{
                  ref->on_loan = FALSE;
		}
	      else
	        {
		  cspi_release_unref (corba_object);
		}
	    }
#ifdef DEBUG_OBJECTS
          g_print ("returning cached %p => %p\n", ref, ref->objref);
#endif
	}
      else
        {
	  ref = malloc (sizeof (Accessible));
	  ref->objref = corba_object;
	  ref->ref_count = 1;
	  ref->on_loan = on_loan;
#ifdef DEBUG_OBJECTS
          g_print ("allocated %p => %p\n", ref, corba_object);
#endif
          g_hash_table_insert (cspi_get_live_refs (), ref->objref, ref);
	}
    }

  return ref;
}

Accessible *
cspi_object_add (CORBA_Object corba_object)
{
  return cspi_object_get_ref (corba_object, FALSE);
}

Accessible *
cspi_object_borrow (CORBA_Object corba_object)
{
  return cspi_object_get_ref (corba_object, TRUE);
}

void
cspi_object_return (Accessible *accessible)
{
  g_return_if_fail (accessible != NULL);

  if (!accessible->on_loan ||
      accessible->ref_count == 1)
    {
      cspi_object_unref (accessible);
    }
  else /* Convert to a permanant ref */
    {
      accessible->on_loan = FALSE;
      accessible->objref = cspi_dup_ref (accessible->objref);
      accessible->ref_count--;
    }
}

void
cspi_object_ref (Accessible *accessible)
{
  g_return_if_fail (accessible != NULL);

  accessible->ref_count++;
}

void
cspi_object_unref (Accessible *accessible)
{
  if (accessible == NULL)
    {
      return;
    }

  if (--accessible->ref_count == 0)
    {
      g_hash_table_remove (cspi_get_live_refs (), accessible->objref);
    }
}

static void
cspi_cleanup (void)
{
  GHashTable *refs;

  refs = live_refs;
  live_refs = NULL;
  if (refs)
    {
      g_hash_table_destroy (refs);
    }

  if (registry != CORBA_OBJECT_NIL)
    {
      cspi_release_unref (registry);
      registry = CORBA_OBJECT_NIL;
    }
}

static gboolean SPI_inited = FALSE;

/**
 * SPI_init:
 *
 * Connects to the accessibility registry and initializes the SPI.
 *
 * Returns: 0 on success, otherwise an integer error code.
 **/
int
SPI_init (void)
{
  if (SPI_inited)
    {
      return 1;
    }

  SPI_inited = TRUE;

  CORBA_exception_init (&ev);

  registry = cspi_init ();

  g_atexit (cspi_cleanup);
  
  return 0;
}

/**
 * SPI_event_main:
 *
 * Starts/enters the main event loop for the SPI services.
 *
 * (NOTE: This method does not return control, it is exited via a call to
 *  SPI_event_quit () from within an event handler).
 *
 **/
void
SPI_event_main (void)
{
  cspi_main ();
}

/**
 * SPI_event_quit:
 *
 * Quits the last main event loop for the SPI services,
 * see SPI_event_main
 **/
void
SPI_event_quit (void)
{
  cspi_main_quit ();
}

/**
 * SPI_eventIsReady:
 *
 * Checks to see if an SPI event is waiting in the event queue.
 * Used by clients that don't wish to use SPI_event_main().
 *
 * Not Yet Implemented.
 *
 * Returns: #TRUE if an event is waiting, otherwise #FALSE.
 *
 **/
SPIBoolean
SPI_eventIsReady ()
{
  return FALSE;
}

/**
 * SPI_nextEvent:
 * @waitForEvent: a #SPIBoolean indicating whether to block or not.
 *
 * Gets the next event in the SPI event queue; blocks if no event
 * is pending and @waitForEvent is #TRUE.
 * Used by clients that don't wish to use SPI_event_main().
 *
 * Not Yet Implemented.
 *
 * Returns: the next #AccessibleEvent in the SPI event queue.
 **/
AccessibleEvent *
SPI_nextEvent (SPIBoolean waitForEvent)
{
  return NULL;
}

static void
report_leaked_ref (gpointer key, gpointer val, gpointer user_data)
{
  char *name, *role;
  Accessible *a = (Accessible *) val;
  
  name = Accessible_getName (a);
  if (cspi_exception ())
    {
      name = NULL;
    }

  role = Accessible_getRoleName (a);
  if (cspi_exception ())
    {
      role = NULL;
    }

  fprintf (stderr, "leaked %d references to object %s, role %s\n",
	   a->ref_count, name ? name : "<?>", role ? role : "<?>");

  SPI_freeString (name);
}


/**
 * SPI_exit:
 *
 * Disconnects from the Accessibility Registry and releases 
 * any floating resources. Call only once at exit.
 *
 * Returns: 0 if there were no leaks, otherwise non zero.
 **/
int
SPI_exit (void)
{
  int leaked;

  if (!SPI_inited)
    {
      return 0;
    }

  SPI_inited = FALSE;

  if (live_refs)
    {
      leaked = g_hash_table_size (live_refs);
#define PRINT_LEAKS
#ifdef PRINT_LEAKS
      g_hash_table_foreach (live_refs, report_leaked_ref, NULL);
#endif
    }
  else
    {
      leaked = 0;
    }

#ifdef DEBUG_OBJECTS
  if (leaked)
    {
      fprintf (stderr, "Leaked %d SPI handles\n", leaked);
    }
#endif

  cspi_cleanup ();

  fprintf (stderr, "bye-bye!\n");

  return leaked;
}

/**
 * SPI_freeString:
 * @s: a character string returned from another at-spi call.
 *
 * Free a character string returned from an at-spi call.  Clients of
 * at-spi should use this function instead of free () or g_free().
 * A NULL string @s will be silently ignored.
 * This API should not be used to free strings
 * from other libraries or allocated by the client.
 **/
void
SPI_freeString (char *s)
{
  if (s)
    {
      CORBA_free (s);
    }
}

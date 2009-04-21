/* AT-SPI - Assistive Technology Service Provider Interface
 *
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001, 2003 Sun Microsystems Inc.,
 * Copyright 2001, 2002 Ximian, Inc.
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

/* deviceeventcontroller.c: implement the DeviceEventController interface */

#include <config.h>

#undef  SPI_XKB_DEBUG
#undef  SPI_DEBUG
#undef  SPI_KEYEVENT_DEBUG

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/XKBlib.h>
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>

#ifdef HAVE_XEVIE
#include <X11/Xproto.h>
#include <X11/X.h>
#include <X11/extensions/Xevie.h>
#endif /* HAVE_XEVIE */

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h> /* TODO: hide dependency (wrap in single porting file) */
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkwindow.h>

#include <spi-common/keymasks.h>
#include <spi-common/spi-dbus.h>
#include <spi-common/spi-types.h>

#include <droute/droute.h>

#include "deviceeventcontroller.h"
#include "reentrant-list.h"

KeySym ucs2keysym (long ucs);
long keysym2ucs(KeySym keysym); 

#define CHECK_RELEASE_DELAY 20
#define BIT(c, x)       (c[x/8]&(1<<(x%8)))
static guint check_release_handler = 0;
static Accessibility_DeviceEvent pressed_event;
static SpiDEController *saved_controller; 
static void wait_for_release_event (XEvent *event, SpiDEController *controller);

/* Our parent Gtk object type */
#define PARENT_TYPE G_TYPE_OBJECT

/* A pointer to our parent object class */
static int spi_error_code = 0;
static GdkPoint last_mouse_pos_static = {0, 0}; 
static GdkPoint *last_mouse_pos = &last_mouse_pos_static;
static unsigned int mouse_mask_state = 0;
static unsigned int mouse_button_mask =
  Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask;
static unsigned int key_modifier_mask =
  Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask | ShiftMask | LockMask | ControlMask | SPI_KEYMASK_NUMLOCK;
static unsigned int _numlock_physical_mask = Mod2Mask; /* a guess, will be reset */

static GQuark spi_dec_private_quark = 0;
static XModifierKeymap* xmkeymap = NULL;

static int (*x_default_error_handler) (Display *display, XErrorEvent *error_event);

typedef enum {
  SPI_DEVICE_TYPE_KBD,
  SPI_DEVICE_TYPE_MOUSE,
  SPI_DEVICE_TYPE_LAST_DEFINED
} SpiDeviceTypeCategory;

typedef struct {
  guint                             ref_count : 30;
  guint                             pending_add : 1;
  guint                             pending_remove : 1;

  Accessibility_ControllerEventMask mod_mask;
  dbus_uint32_t               key_val;  /* KeyCode */
} DEControllerGrabMask;

typedef struct {
  char *bus_name;
  char *path;
  SpiDeviceTypeCategory type;
  gulong types;
} DEControllerListener;

typedef struct {
  DEControllerListener listener;

 GSList *keys;
  Accessibility_ControllerEventMask mask;
  Accessibility_EventListenerMode  *mode;	
} DEControllerKeyListener;

typedef struct {
  unsigned int last_press_keycode;
  unsigned int last_release_keycode;
  struct timeval last_press_time;
  struct timeval last_release_time;
  int have_xkb;
  int xkb_major_extension_opcode;
  int xkb_base_event_code;
  int xkb_base_error_code;
  unsigned int xkb_latch_mask;
  unsigned int pending_xkb_mod_relatch_mask;
  XkbDescPtr xkb_desc;
  KeyCode reserved_keycode;
  KeySym reserved_keysym;
  guint  reserved_reset_timeout;
} DEControllerPrivateData;

static void     spi_controller_register_with_devices          (SpiDEController           *controller);
static gboolean spi_controller_update_key_grabs               (SpiDEController           *controller,
							       Accessibility_DeviceEvent *recv);
static gboolean spi_controller_register_device_listener       (SpiDEController           *controller,
							       DEControllerListener      *l);
static gboolean spi_device_event_controller_forward_key_event (SpiDEController           *controller,
							       const XEvent              *event);
static void     spi_controller_deregister_device_listener (SpiDEController            *controller,
					                   DEControllerListener *listener);
static void     spi_deregister_controller_key_listener (SpiDEController         *controller,
							DEControllerKeyListener *key_listener);
static gboolean spi_controller_notify_mouselisteners (SpiDEController                 *controller,
						      const Accessibility_DeviceEvent *event);

static gboolean spi_eventtype_seq_contains_event (dbus_uint32_t types,
						  const Accessibility_DeviceEvent *event);
static gboolean spi_clear_error_state (void);
static gboolean spi_dec_poll_mouse_moved (gpointer data);
static gboolean spi_dec_poll_mouse_moving (gpointer data);
static gboolean spi_dec_poll_mouse_idle (gpointer data);

#define spi_get_display() GDK_DISPLAY()

G_DEFINE_TYPE(SpiDEController, spi_device_event_controller, G_TYPE_OBJECT)

/* Private methods */
static dbus_bool_t
spi_dbus_add_disconnect_match (DBusConnection *bus, const char *name)
{
  char *match = g_strdup_printf ("interface=%s,member=NameOwnerChanged,arg0=%s", DBUS_INTERFACE_DBUS, name);
  if (match)
  {
    DBusError error;
    dbus_error_init (&error);
    dbus_bus_add_match (bus, match, &error);
    g_free (match);
    return !dbus_error_is_set (&error);
  }
  else return FALSE;
}

static dbus_bool_t
spi_dbus_remove_disconnect_match (DBusConnection *bus, const char *name)
{
  char *match = g_strdup_printf ("interface=%s,member=NameOwnerChanged,arg0=%s", DBUS_INTERFACE_DBUS, name);
  if (match)
  {
    DBusError error;
    dbus_error_init (&error);
    dbus_bus_remove_match (bus, match, &error);
    g_free (match);
    return !dbus_error_is_set (&error);
  }
  else return FALSE;
}

static unsigned int
keysym_mod_mask (KeySym keysym, KeyCode keycode)
{
	/* we really should use XKB and look directly at the keymap */
	/* this is very inelegant */
	Display *display = spi_get_display ();
	unsigned int mods_rtn = 0;
	unsigned int retval = 0;
	KeySym sym_rtn;

	if (XkbLookupKeySym (display, keycode, 0, &mods_rtn, &sym_rtn) &&
	    (sym_rtn == keysym)) {
		retval = 0;
	}
	else if (XkbLookupKeySym (display, keycode, ShiftMask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = ShiftMask;
	}
	else if (XkbLookupKeySym (display, keycode, Mod2Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = Mod2Mask;
	}
	else if (XkbLookupKeySym (display, keycode, Mod3Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = Mod3Mask;
	}
	else if (XkbLookupKeySym (display, keycode, 
				  ShiftMask | Mod2Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = (Mod2Mask | ShiftMask);
	}
	else if (XkbLookupKeySym (display, keycode, 
				  ShiftMask | Mod3Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = (Mod3Mask | ShiftMask);
	}
	else if (XkbLookupKeySym (display, keycode, 
				  ShiftMask | Mod4Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = (Mod4Mask | ShiftMask);
	}
	else
		retval = 0xFFFF;
	return retval;
}

static gboolean
spi_dec_replace_map_keysym (DEControllerPrivateData *priv, KeyCode keycode, KeySym keysym)
{
#ifdef HAVE_XKB
  Display *dpy = spi_get_display ();
  XkbDescPtr desc;
  if (!(desc = XkbGetMap (dpy, XkbAllMapComponentsMask, XkbUseCoreKbd)))
    {
      fprintf (stderr, "ERROR getting map\n");
    }
  XFlush (dpy);
  XSync (dpy, False);
  if (desc && desc->map)
    {
      gint offset = desc->map->key_sym_map[keycode].offset;
      desc->map->syms[offset] = keysym; 
    }
  else
    {
      fprintf (stderr, "Error changing key map: empty server structure\n");
    }		
  XkbSetMap (dpy, XkbAllMapComponentsMask, desc);
  /**
   *  FIXME: the use of XkbChangeMap, and the reuse of the priv->xkb_desc structure, 
   * would be far preferable.
   * HOWEVER it does not seem to work using XFree 4.3. 
   **/
  /*	    XkbChangeMap (dpy, priv->xkb_desc, priv->changes); */
  XFlush (dpy);
  XSync (dpy, False);
  XkbFreeKeyboard (desc, 0, TRUE);

  return TRUE;
#else
  return FALSE;
#endif
}

static gboolean
spi_dec_reset_reserved (gpointer data)
{
  DEControllerPrivateData *priv = data;
  spi_dec_replace_map_keysym (priv, priv->reserved_keycode, priv->reserved_keysym);
  priv->reserved_reset_timeout = 0;
  return FALSE;
}

static KeyCode
keycode_for_keysym (SpiDEController *controller, long keysym, unsigned int *modmask)
{
	KeyCode keycode = 0;
	keycode = XKeysymToKeycode (spi_get_display (), (KeySym) keysym);
	if (!keycode) 
	{
		DEControllerPrivateData *priv = (DEControllerPrivateData *)
			g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
		/* if there's no keycode available, fix it */
		if (spi_dec_replace_map_keysym (priv, priv->reserved_keycode, keysym))
		{
			keycode = priv->reserved_keycode;
			/* 
			 * queue a timer to restore the old keycode.  Ugly, but required 
			 * due to races / asynchronous X delivery.   
			 * Long-term fix is to extend the X keymap here instead of replace entries.
			 */
			priv->reserved_reset_timeout = g_timeout_add (500, spi_dec_reset_reserved, priv);
		}		
		*modmask = 0;
		return keycode;
	}
	if (modmask) 
		*modmask = keysym_mod_mask (keysym, keycode);
	return keycode;
}

static DEControllerGrabMask *
spi_grab_mask_clone (DEControllerGrabMask *grab_mask)
{
  DEControllerGrabMask *clone = g_new (DEControllerGrabMask, 1);

  memcpy (clone, grab_mask, sizeof (DEControllerGrabMask));

  clone->ref_count = 1;
  clone->pending_add = TRUE;
  clone->pending_remove = FALSE;

  return clone;
}

static void
spi_grab_mask_free (DEControllerGrabMask *grab_mask)
{
  g_free (grab_mask);
}

static gint
spi_grab_mask_compare_values (gconstpointer p1, gconstpointer p2)
{
  DEControllerGrabMask *l1 = (DEControllerGrabMask *) p1;
  DEControllerGrabMask *l2 = (DEControllerGrabMask *) p2;

  if (p1 == p2)
    {
      return 0;
    }
  else
    { 
      return ((l1->mod_mask != l2->mod_mask) || (l1->key_val != l2->key_val));
    }
}

static void
spi_dec_set_unlatch_pending (SpiDEController *controller, unsigned mask)
{
  DEControllerPrivateData *priv = 
    g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
#ifdef SPI_XKB_DEBUG
  if (priv->xkb_latch_mask) fprintf (stderr, "unlatch pending! %x\n", 
				     priv->xkb_latch_mask);
#endif
  priv->pending_xkb_mod_relatch_mask |= priv->xkb_latch_mask; 
}
 
static void
spi_dec_clear_unlatch_pending (SpiDEController *controller)
{
  DEControllerPrivateData *priv = 
    g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
  priv->xkb_latch_mask = 0;
}

static void emit(SpiDEController *controller, const char *interface, const char *name, int a1, int a2)
{
  DBusMessage *signal = NULL;
  DBusMessageIter iter, iter_variant;
  int nil = 0;
  const char *minor = "";
  const char *path = SPI_DBUS_PATH_DEC;

  signal = dbus_message_new_signal (path, interface, name);

  dbus_message_iter_init_append (signal, &iter);

  dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &minor);
  dbus_message_iter_append_basic (&iter, DBUS_TYPE_UINT32, &a1);
  dbus_message_iter_append_basic (&iter, DBUS_TYPE_UINT32, &a2);
  dbus_message_iter_open_container (&iter, DBUS_TYPE_VARIANT, "i", &iter_variant);
      dbus_message_iter_append_basic (&iter_variant, DBUS_TYPE_INT32, &nil);
  dbus_message_iter_close_container (&iter, &iter_variant);

  dbus_connection_send (controller->bus, signal, NULL);
}

static gboolean
spi_dec_button_update_and_emit (SpiDEController *controller,
				guint mask_return)
{
  Accessibility_DeviceEvent mouse_e;
  gchar event_detail[24];
  gboolean is_consumed = FALSE;

  if ((mask_return & mouse_button_mask) !=
      (mouse_mask_state & mouse_button_mask)) 
    {
      int button_number = 0;
      gboolean is_down = False;
      
      if (!(mask_return & Button1Mask) &&
	  (mouse_mask_state & Button1Mask)) 
	{
	  mouse_mask_state &= ~Button1Mask;
	  button_number = 1;
	} 
      else if ((mask_return & Button1Mask) &&
	       !(mouse_mask_state & Button1Mask)) 
	{
	  mouse_mask_state |= Button1Mask;
	  button_number = 1;
	  is_down = True;
	} 
      else if (!(mask_return & Button2Mask) &&
	       (mouse_mask_state & Button2Mask)) 
	{
	  mouse_mask_state &= ~Button2Mask;
	  button_number = 2;
	} 
      else if ((mask_return & Button2Mask) &&
	       !(mouse_mask_state & Button2Mask)) 
	{
	  mouse_mask_state |= Button2Mask;
	  button_number = 2;
	  is_down = True;
	} 
      else if (!(mask_return & Button3Mask) &&
	       (mouse_mask_state & Button3Mask)) 
	{
	  mouse_mask_state &= ~Button3Mask;
	  button_number = 3;
	} 
      else if ((mask_return & Button3Mask) &&
	       !(mouse_mask_state & Button3Mask)) 
	{
	  mouse_mask_state |= Button3Mask;
	  button_number = 3;
	  is_down = True;
	} 
      else if (!(mask_return & Button4Mask) &&
	       (mouse_mask_state & Button4Mask)) 
	{
	  mouse_mask_state &= ~Button4Mask;
	  button_number = 4;
	} 
      else if ((mask_return & Button4Mask) &&
	       !(mouse_mask_state & Button4Mask)) 
	{
	  mouse_mask_state |= Button4Mask;
	  button_number = 4;
	  is_down = True;
	} 
      else if (!(mask_return & Button5Mask) &&
	       (mouse_mask_state & Button5Mask)) 
	{
	  mouse_mask_state &= ~Button5Mask;
	  button_number = 5;
	}
      else if ((mask_return & Button5Mask) &&
	       !(mouse_mask_state & Button5Mask)) 
	{
	  mouse_mask_state |= Button5Mask;
	  button_number = 5;
	  is_down = True;
	}
      if (button_number) {
#ifdef SPI_DEBUG		  
	fprintf (stderr, "Button %d %s\n",
		 button_number, (is_down) ? "Pressed" : "Released");
#endif
	snprintf (event_detail, 22, "%d%c", button_number,
		  (is_down) ? 'p' : 'r');
	/* TODO: FIXME distinguish between physical and 
	 * logical buttons 
	 */
	mouse_e.type      = (is_down) ? 
	  Accessibility_BUTTON_PRESSED_EVENT :
	  Accessibility_BUTTON_RELEASED_EVENT;
	mouse_e.id        = button_number;
	mouse_e.hw_code   = button_number;
	mouse_e.modifiers = (dbus_uint16_t) mouse_mask_state; 
	mouse_e.timestamp = 0;
	mouse_e.event_string = "";
	mouse_e.is_text   = FALSE;
	is_consumed = 
	  spi_controller_notify_mouselisteners (controller, 
						&mouse_e);
	if (!is_consumed)
	  {
	    dbus_uint32_t x = last_mouse_pos->x, y = last_mouse_pos->y;
	    emit(controller, SPI_DBUS_INTERFACE_EVENT_MOUSE, "button", x, y);
	  }
	else
	  spi_dec_set_unlatch_pending (controller, mask_return);
      }
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}


static guint
spi_dec_mouse_check (SpiDEController *controller, 
		     int *x, int *y, gboolean *moved)
{
  int win_x_return,win_y_return;
  unsigned int mask_return;
  Window root_return, child_return;
  Display *display = spi_get_display ();

  if (display != NULL)
    XQueryPointer(display, DefaultRootWindow (display),
		  &root_return, &child_return,
		  x, y,
		  &win_x_return, &win_y_return, &mask_return);
  /* 
   * Since many clients grab the pointer, and X goes an automatic
   * pointer grab on mouse-down, we often must detect mouse button events
   * by polling rather than via a button grab. 
   * The while loop (rather than if) is used since it's possible that 
   * multiple buttons have changed state since we last checked.
   */
  if (mask_return != mouse_mask_state) 
    {
      while (spi_dec_button_update_and_emit (controller, mask_return));
    }

  if (*x != last_mouse_pos->x || *y != last_mouse_pos->y) 
    {
      // TODO: combine these two signals?
      dbus_uint32_t ix = *x, iy = *y;
      emit(controller, SPI_DBUS_INTERFACE_EVENT_MOUSE, "abs", ix, iy);
      ix -= last_mouse_pos->x;
      iy -= last_mouse_pos->y;
      emit(controller, SPI_DBUS_INTERFACE_EVENT_MOUSE, "rel", ix, iy);
      last_mouse_pos->x = *x;
      last_mouse_pos->y = *y;
      *moved = True;
    }
  else
    {
      *moved = False;
    }

  return mask_return;
}

static void
spi_dec_emit_modifier_event (SpiDEController *controller, guint prev_mask, 
			     guint current_mask)
{
  dbus_uint32_t d1, d2;

#ifdef SPI_XKB_DEBUG
  fprintf (stderr, "MODIFIER CHANGE EVENT! %x to %x\n", 
	   prev_mask, current_mask);
#endif

  /* set bits for the virtual modifiers like NUMLOCK */
  if (prev_mask & _numlock_physical_mask) 
    prev_mask |= SPI_KEYMASK_NUMLOCK;
  if (current_mask & _numlock_physical_mask) 
    current_mask |= SPI_KEYMASK_NUMLOCK;

  d1 = prev_mask & key_modifier_mask;
  d2 = current_mask & key_modifier_mask;
      emit(controller, SPI_DBUS_INTERFACE_EVENT_KEYBOARD, "modifiers", d1, d2);
}

static gboolean
spi_dec_poll_mouse_moved (gpointer data)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER(data);
  int x, y;
  gboolean moved;
  guint mask_return;

  mask_return = spi_dec_mouse_check (controller, &x, &y, &moved);

  if ((mask_return & key_modifier_mask) !=
      (mouse_mask_state & key_modifier_mask)) 
    {
      spi_dec_emit_modifier_event (controller, mouse_mask_state, mask_return);
      mouse_mask_state = mask_return;
    }

  return moved;
}

static gboolean
spi_dec_poll_mouse_idle (gpointer data)
{
  if (! spi_dec_poll_mouse_moved (data))
    return TRUE;
  else
    {
      g_timeout_add (20, spi_dec_poll_mouse_moving, data);	    
      return FALSE;	    
    }
}

static gboolean
spi_dec_poll_mouse_moving (gpointer data)
{
  if (spi_dec_poll_mouse_moved (data))
    return TRUE;
  else
    {
      g_timeout_add (100, spi_dec_poll_mouse_idle, data);	    
      return FALSE;
    }
}

#ifdef WE_NEED_UGRAB_MOUSE
static int
spi_dec_ungrab_mouse (gpointer data)
{
	Display *display = spi_get_display ();
	if (display)
	  {
	    XUngrabButton (spi_get_display (), AnyButton, AnyModifier,
			   XDefaultRootWindow (spi_get_display ()));
	  }
	return FALSE;
}
#endif

static void
spi_dec_init_mouse_listener (SpiDEController *dec)
{
#ifdef GRAB_BUTTON
  Display *display = spi_get_display ();
#endif
  g_timeout_add (100, spi_dec_poll_mouse_idle, dec);

#ifdef GRAB_BUTTON
  if (display)
    {
      if (XGrabButton (display, AnyButton, AnyModifier,
		       gdk_x11_get_default_root_xwindow (),
		       True, ButtonPressMask | ButtonReleaseMask,
		       GrabModeSync, GrabModeAsync, None, None) != Success) {
#ifdef SPI_DEBUG
	fprintf (stderr, "WARNING: could not grab mouse buttons!\n");
#endif
	;
      }
      XSync (display, False);
#ifdef SPI_DEBUG
      fprintf (stderr, "mouse buttons grabbed\n");
#endif
    }
#endif
}

/**
 * Eventually we can use this to make the marshalling of mask types
 * more sane, but for now we just use this to detect 
 * the use of 'virtual' masks such as numlock and convert them to
 * system-specific mask values (i.e. ModMask).
 * 
 **/
static Accessibility_ControllerEventMask
spi_dec_translate_mask (Accessibility_ControllerEventMask mask)
{
  Accessibility_ControllerEventMask tmp_mask;
  gboolean has_numlock;

  has_numlock = (mask & SPI_KEYMASK_NUMLOCK);
  tmp_mask = mask;
  if (has_numlock)
    {
      tmp_mask = mask ^ SPI_KEYMASK_NUMLOCK;
      tmp_mask |= _numlock_physical_mask;
    }
 
  return tmp_mask;
}

static DEControllerKeyListener *
spi_dec_key_listener_new (const char *bus_name,
			  const char *path,
			  GSList *keys,
			  const Accessibility_ControllerEventMask mask,
			  const dbus_uint32_t types,
			  const Accessibility_EventListenerMode  *mode)
{
  DEControllerKeyListener *key_listener = g_new0 (DEControllerKeyListener, 1);
  key_listener->listener.bus_name = g_strdup(bus_name);
  key_listener->listener.path = g_strdup(path);
  key_listener->listener.type = SPI_DEVICE_TYPE_KBD;
  key_listener->keys = keys;
  key_listener->mask = spi_dec_translate_mask (mask);
  key_listener->listener.types = types;
  if (mode)
  {
    key_listener->mode = (Accessibility_EventListenerMode *) g_malloc(sizeof(Accessibility_EventListenerMode));
    memcpy(key_listener->mode, mode, sizeof(*mode));
  }
  else
    key_listener->mode = NULL;

#ifdef SPI_DEBUG
  g_print ("new listener, with mask %x, is_global %d, keys %p (%d)\n",
	   (unsigned int) key_listener->mask,
           (int) (mode ? mode->global : 0),
	   (void *) key_listener->keys,
	   (int) (key_listener->keys ? g_slist_length(key_listener->keys) : 0));
#endif

  return key_listener;	
}

static DEControllerListener *
spi_dec_listener_new (const char *bus_name,
		      const char *path,
		      dbus_uint32_t types)
{
  DEControllerListener *listener = g_new0 (DEControllerListener, 1);
  listener->bus_name = g_strdup(bus_name);
  listener->path = g_strdup(path);
  listener->type = SPI_DEVICE_TYPE_MOUSE;
  listener->types = types;
  return listener;	
}

static DEControllerListener *
spi_listener_clone (DEControllerListener *listener)
{
  DEControllerListener *clone = g_new0 (DEControllerListener, 1);
  clone->bus_name = g_strdup (listener->bus_name);
  clone->path = g_strdup (listener->path);
  clone->type = listener->type;
  clone->types = listener->types;
  return clone;
}

static GSList *keylist_clone (GSList *s)
{
  GSList *d = NULL;
  GSList *l;

  for (l = s; l; l = g_slist_next(l))
  {
    Accessibility_KeyDefinition *kd = (Accessibility_KeyDefinition *)g_malloc(sizeof(Accessibility_KeyDefinition));
    if (kd)
    {
      Accessibility_KeyDefinition *kds = (Accessibility_KeyDefinition *)l->data;
      kd->keycode = kds->keycode;
      kd->keysym = kds->keysym;
      kd->keystring = g_strdup(kds->keystring);
      d = g_slist_append(d, kd);
    }
  }
  return d;
}

static DEControllerKeyListener *
spi_key_listener_clone (DEControllerKeyListener *key_listener)
{
  DEControllerKeyListener *clone = g_new0 (DEControllerKeyListener, 1);
  clone->listener.bus_name = g_strdup (key_listener->listener.bus_name);
  clone->listener.path = g_strdup (key_listener->listener.path);
  clone->listener.type = SPI_DEVICE_TYPE_KBD;
  clone->keys = keylist_clone (key_listener->keys);
  clone->mask = key_listener->mask;
  clone->listener.types = key_listener->listener.types;
  if (key_listener->mode)
  {
    clone->mode = (Accessibility_EventListenerMode *)g_malloc(sizeof(Accessibility_EventListenerMode));
    if (clone->mode) memcpy(clone->mode, key_listener->mode, sizeof(Accessibility_EventListenerMode));
  }
  else
    clone->mode = NULL;
  return clone;
}

static void keylist_free(GSList *keys)
{
  GSList *l;

  for (l = keys; l; l = g_slist_next(l))
  {
    Accessibility_KeyDefinition *kd = (Accessibility_KeyDefinition *)l->data;
    g_free(kd->keystring);
    g_free(kd);
  }
  g_slist_free (keys);
}

static void
spi_key_listener_data_free (DEControllerKeyListener *key_listener)
{
  keylist_free(key_listener->keys);
  if (key_listener->mode) g_free(key_listener->mode);
  g_free (key_listener);
}

static void
spi_key_listener_clone_free (DEControllerKeyListener *clone)
{
  spi_key_listener_data_free (clone);
}

static void
spi_listener_clone_free (DEControllerListener *clone)
{
  g_free (clone->path);
  g_free (clone->bus_name);
  g_free (clone);
}

static void
spi_dec_listener_free (DEControllerListener    *listener)
{
  g_free (listener->bus_name);
  g_free (listener->path);
  if (listener->type == SPI_DEVICE_TYPE_KBD) 
    spi_key_listener_data_free ((DEControllerKeyListener *) listener);
}

static void
_register_keygrab (SpiDEController      *controller,
		   DEControllerGrabMask *grab_mask)
{
  GList *l;

  l = g_list_find_custom (controller->keygrabs_list, grab_mask,
			  spi_grab_mask_compare_values);
  if (l)
    {
      DEControllerGrabMask *cur_mask = l->data;

      cur_mask->ref_count++;
      if (cur_mask->pending_remove)
        {
          cur_mask->pending_remove = FALSE;
	}
    }
  else
    {
      controller->keygrabs_list =
        g_list_prepend (controller->keygrabs_list,
			spi_grab_mask_clone (grab_mask));
    }
}

static void
_deregister_keygrab (SpiDEController      *controller,
		     DEControllerGrabMask *grab_mask)
{
  GList *l;

  l = g_list_find_custom (controller->keygrabs_list, grab_mask,
			  spi_grab_mask_compare_values);

  if (l)
    {
      DEControllerGrabMask *cur_mask = l->data;

      cur_mask->ref_count--;
      if (cur_mask->ref_count <= 0)
        {
          cur_mask->pending_remove = TRUE;
	}
    }
}

static void
handle_keygrab (SpiDEController         *controller,
		DEControllerKeyListener *key_listener,
		void                   (*process_cb) (SpiDEController *controller,
						      DEControllerGrabMask *grab_mask))
{
  DEControllerGrabMask grab_mask = { 0 };

  grab_mask.mod_mask = key_listener->mask;
  if (g_slist_length (key_listener->keys) == 0) /* special case means AnyKey/AllKeys */
    {
      grab_mask.key_val = AnyKey;
#ifdef SPI_DEBUG
      fprintf (stderr, "AnyKey grab!");
#endif
      process_cb (controller, &grab_mask);
    }
  else
    {
      GSList *l;

      for (l = key_listener->keys; l; l = g_slist_next(l))
        {
	  Accessibility_KeyDefinition *keydef = l->data;
	  long int key_val = keydef->keysym;
	  /* X Grabs require keycodes, not keysyms */
	  if (keydef->keystring && keydef->keystring[0])
	    {
	      key_val = XStringToKeysym(keydef->keystring);		    
	    }
	  if (key_val > 0)
	    {
	      key_val = XKeysymToKeycode (spi_get_display (), (KeySym) key_val);
	    }
	  else
	    {
	      key_val = keydef->keycode;
	    }
	  grab_mask.key_val = key_val;
	  process_cb (controller, &grab_mask);
	}
    }
}

static gboolean
spi_controller_register_global_keygrabs (SpiDEController         *controller,
					 DEControllerKeyListener *key_listener)
{
  handle_keygrab (controller, key_listener, _register_keygrab);
  if (controller->xevie_display == NULL)
    return spi_controller_update_key_grabs (controller, NULL);
  else
    return TRUE;
}

static void
spi_controller_deregister_global_keygrabs (SpiDEController         *controller,
					   DEControllerKeyListener *key_listener)
{
  handle_keygrab (controller, key_listener, _deregister_keygrab);
  if (controller->xevie_display == NULL)
    spi_controller_update_key_grabs (controller, NULL);
}

static gboolean
spi_controller_register_device_listener (SpiDEController      *controller,
					 DEControllerListener *listener)
{
  DEControllerKeyListener *key_listener;
  
  switch (listener->type) {
  case SPI_DEVICE_TYPE_KBD:
      key_listener = (DEControllerKeyListener *) listener;

      controller->key_listeners = g_list_prepend (controller->key_listeners,
						  key_listener);
      spi_dbus_add_disconnect_match (controller->bus, key_listener->listener.bus_name);
      if (key_listener->mode->global)
        {
	  return spi_controller_register_global_keygrabs (controller, key_listener);	
	}
      else
	      return TRUE;
      break;
  case SPI_DEVICE_TYPE_MOUSE:
      controller->mouse_listeners = g_list_prepend (controller->mouse_listeners, listener);
      spi_dbus_add_disconnect_match (controller->bus, listener->bus_name);
      break;
  default:
      break;
  }
  return FALSE;
}

static gboolean
Accessibility_DeviceEventListener_notifyEvent(SpiDEController *controller,
                                              SpiRegistry *registry,
                                              DEControllerListener *listener,
                                              const Accessibility_DeviceEvent *key_event)
{
  DBusMessage *message = dbus_message_new_method_call(listener->bus_name,
                                                      listener->path,
                                                      SPI_DBUS_INTERFACE_DEVICE_EVENT_LISTENER,
                                                      "notifyEvent");
  DBusError error;
  dbus_bool_t consumed = FALSE;

  dbus_error_init(&error);
  if (spi_dbus_marshal_deviceEvent(message, key_event))
  {
    // TODO: Evaluate performance: perhaps rework this whole architecture
    // to avoid blocking calls
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(controller->bus, message, 1000, &error);
    if (reply)
    {
      DBusError error;
      dbus_error_init(&error);
      dbus_message_get_args(reply, &error, DBUS_TYPE_BOOLEAN, &consumed, DBUS_TYPE_INVALID);
      dbus_message_unref(reply);
    }
  }
  dbus_message_unref(message);
  return consumed;
}

static gboolean
spi_controller_notify_mouselisteners (SpiDEController                 *controller,
				      const Accessibility_DeviceEvent *event)
{
  GList   *l;
  GSList  *notify = NULL, *l2;
  GList  **listeners = &controller->mouse_listeners;
  gboolean is_consumed;
#ifdef SPI_KEYEVENT_DEBUG
  gboolean found = FALSE;
#endif
  if (!listeners)
    {
      return FALSE;
    }

  for (l = *listeners; l; l = l->next)
    {
       DEControllerListener *listener = l->data;

       if (spi_eventtype_seq_contains_event (listener->types, event))
         {
	   /* we clone (don't dup) the listener, to avoid refcount inc. */
	   notify = g_slist_prepend (notify,
				     spi_listener_clone (listener));
#ifdef SPI_KEYEVENT_DEBUG
           found = TRUE;
#endif
         }
    }

#ifdef SPI_KEYEVENT_DEBUG
  if (!found)
    {
      g_print ("no match for event\n");
    }
#endif

  is_consumed = FALSE;
  for (l2 = notify; l2 && !is_consumed; l2 = l2->next)
    {
      DEControllerListener *listener = l2->data;

      is_consumed = Accessibility_DeviceEventListener_notifyEvent (controller, controller->registry, listener, event);

      spi_listener_clone_free ((DEControllerListener *) l2->data);
    }

  for (; l2; l2 = l2->next)
    {
      DEControllerListener *listener = l2->data;
      spi_listener_clone_free (listener);
      /* clone doesn't have its own ref, so don't use spi_device_listener_free */
    }

  g_slist_free (notify);

#ifdef SPI_DEBUG
  if (is_consumed) g_message ("consumed\n");
#endif
  return is_consumed;
}

static void
spi_device_event_controller_forward_mouse_event (SpiDEController *controller,
						 XEvent *xevent)
{
  Accessibility_DeviceEvent mouse_e;
  gchar event_detail[24];
  gboolean is_consumed = FALSE;
  gboolean xkb_mod_unlatch_occurred;
  XButtonEvent *xbutton_event = (XButtonEvent *) xevent;
  dbus_uint32_t ix, iy;

  int button = xbutton_event->button;
  
  unsigned int mouse_button_state = xbutton_event->state;

  switch (button)
    {
    case 1:
	    mouse_button_state |= Button1Mask;
	    break;
    case 2:
	    mouse_button_state |= Button2Mask;
	    break;
    case 3:
	    mouse_button_state |= Button3Mask;
	    break;
    case 4:
	    mouse_button_state |= Button4Mask;
	    break;
    case 5:
	    mouse_button_state |= Button5Mask;
	    break;
    }

  last_mouse_pos->x = ((XButtonEvent *) xevent)->x_root;
  last_mouse_pos->y = ((XButtonEvent *) xevent)->y_root;

#ifdef SPI_DEBUG  
  fprintf (stderr, "mouse button %d %s (%x)\n",
	   xbutton_event->button, 
	   (xevent->type == ButtonPress) ? "Press" : "Release",
	   mouse_button_state);
#endif
  snprintf (event_detail, 22, "%d%c", button,
	    (xevent->type == ButtonPress) ? 'p' : 'r');

  /* TODO: FIXME distinguish between physical and logical buttons */
  mouse_e.type      = (xevent->type == ButtonPress) ? 
                      Accessibility_BUTTON_PRESSED_EVENT :
                      Accessibility_BUTTON_RELEASED_EVENT;
  mouse_e.id        = button;
  mouse_e.hw_code   = button;
  mouse_e.modifiers = (dbus_uint16_t) xbutton_event->state;
  mouse_e.timestamp = (dbus_uint32_t) xbutton_event->time;
  mouse_e.event_string = "";
  mouse_e.is_text   = FALSE;
  if ((mouse_button_state & mouse_button_mask) != 
       (mouse_mask_state & mouse_button_mask))
    { 
      if ((mouse_mask_state & key_modifier_mask) !=
	  (mouse_button_state & key_modifier_mask))
	spi_dec_emit_modifier_event (controller, 
				     mouse_mask_state, mouse_button_state);
      mouse_mask_state = mouse_button_state;
      is_consumed = 
	spi_controller_notify_mouselisteners (controller, &mouse_e);
      ix = last_mouse_pos->x;
      iy = last_mouse_pos->y;
      /* TODO - Work out which part of the spec this emit is fulfilling */
      //emit(controller, SPI_DBUS_INTERFACE_EVENT_MOUSE, "button", event_detail, ix, iy);
    }

  xkb_mod_unlatch_occurred = (xevent->type == ButtonPress ||
			      xevent->type == ButtonRelease);
  
  /* if client wants to consume this event, and XKB latch state was
   *   unset by this button event, we reset it
   */
  if (is_consumed && xkb_mod_unlatch_occurred)
    spi_dec_set_unlatch_pending (controller, mouse_mask_state);
  
  XAllowEvents (spi_get_display (),
		(is_consumed) ? SyncPointer : ReplayPointer,
		CurrentTime);
}

static GdkFilterReturn
global_filter_fn (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
  XEvent *xevent = gdk_xevent;
  SpiDEController *controller;
  DEControllerPrivateData *priv;
  Display *display = spi_get_display ();
  controller = SPI_DEVICE_EVENT_CONTROLLER (data);
  priv = (DEControllerPrivateData *)
	  g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);  

  if (xevent->type == MappingNotify)
    xmkeymap = NULL;

  if (xevent->type == KeyPress || xevent->type == KeyRelease)
    {
      if (controller->xevie_display == NULL)
        {
          gboolean is_consumed;

          is_consumed =
            spi_device_event_controller_forward_key_event (controller, xevent);

          if (is_consumed)
            {
              int n_events;
              int i;
              XEvent next_event;
              n_events = XPending (display);

#ifdef SPI_KEYEVENT_DEBUG
              g_print ("Number of events pending: %d\n", n_events);
#endif
              for (i = 0; i < n_events; i++)
                {
                  XNextEvent (display, &next_event);
		  if (next_event.type != KeyPress &&
		      next_event.type != KeyRelease)
			g_warning ("Unexpected event type %d in queue", next_event.type);
                 }

              XAllowEvents (display, AsyncKeyboard, CurrentTime);
              if (n_events)
                XUngrabKeyboard (display, CurrentTime);
            }
          else
            {
              if (xevent->type == KeyPress)
                wait_for_release_event (xevent, controller);
              XAllowEvents (display, ReplayKeyboard, CurrentTime);
            }
        }

      return GDK_FILTER_CONTINUE;
    }
  if (xevent->type == ButtonPress || xevent->type == ButtonRelease)
    {
      spi_device_event_controller_forward_mouse_event (controller, xevent);
    }
  if (xevent->type == priv->xkb_base_event_code)
    {
      XkbAnyEvent * xkb_ev = (XkbAnyEvent *) xevent;
      /* ugly but probably necessary...*/
      XSynchronize (display, TRUE);

      if (xkb_ev->xkb_type == XkbStateNotify)
        {
	  XkbStateNotifyEvent *xkb_snev =
		  (XkbStateNotifyEvent *) xkb_ev;
	  /* check the mouse, to catch mouse events grabbed by
	   * another client; in case we should revert this XKB delatch 
	   */
	  if (!priv->pending_xkb_mod_relatch_mask)
	    {
	      int x, y;
	      gboolean moved;
	      spi_dec_mouse_check (controller, &x, &y, &moved);
	    }
	  /* we check again, since the previous call may have 
	     changed this flag */
	  if (priv->pending_xkb_mod_relatch_mask)
	    {
	      unsigned int feedback_mask;
#ifdef SPI_XKB_DEBUG
	      fprintf (stderr, "relatching %x\n",
		       priv->pending_xkb_mod_relatch_mask);
#endif
	      /* temporarily turn off the latch bell, if it's on */
	      XkbGetControls (display,
			      XkbAccessXFeedbackMask,
			      priv->xkb_desc);
	      feedback_mask = priv->xkb_desc->ctrls->ax_options;
	      if (feedback_mask & XkbAX_StickyKeysFBMask)
	      {
	        XkbControlsChangesRec changes = {XkbAccessXFeedbackMask,
						 0, False};      
	        priv->xkb_desc->ctrls->ax_options
			      &= ~(XkbAX_StickyKeysFBMask);
	        XkbChangeControls (display, priv->xkb_desc, &changes);
	      }
	      /* TODO: account for lock as well as latch */
	      XkbLatchModifiers (display,
				 XkbUseCoreKbd,
				 priv->pending_xkb_mod_relatch_mask,
				 priv->pending_xkb_mod_relatch_mask);
	      if (feedback_mask & XkbAX_StickyKeysFBMask)
	      {	
	        XkbControlsChangesRec changes = {XkbAccessXFeedbackMask,
						 0, False};      
		priv->xkb_desc->ctrls->ax_options = feedback_mask;
		XkbChangeControls (display, priv->xkb_desc, &changes);
	      }
#ifdef SPI_XKB_DEBUG
	      fprintf (stderr, "relatched %x\n",
		       priv->pending_xkb_mod_relatch_mask);
#endif
	      priv->pending_xkb_mod_relatch_mask = 0;
	    }
	  else
	    {
	      priv->xkb_latch_mask = xkb_snev->latched_mods;
	    }
	}
      XSynchronize (display, FALSE);
    }
  
  return GDK_FILTER_CONTINUE;
}

static int
_spi_controller_device_error_handler (Display *display, XErrorEvent *error)
{
  if (error->error_code == BadAccess) 
    {  
      g_message ("Could not complete key grab: grab already in use.\n");
      spi_error_code = BadAccess;
      return 0;
    }
  else 
    {
      return (*x_default_error_handler) (display, error);
    }
}

static void
spi_controller_register_with_devices (SpiDEController *controller)
{
  DEControllerPrivateData *priv;
  int event_base, error_base, major_version, minor_version;

  priv = (DEControllerPrivateData *) g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
  if (XTestQueryExtension (spi_get_display(), &event_base, &error_base, &major_version, &minor_version))
    {
      XTestGrabControl (spi_get_display (), True);
    }

  /* calls to device-specific implementations and routines go here */
  /* register with: keyboard hardware code handler */
  /* register with: (translated) keystroke handler */

  priv->have_xkb = XkbQueryExtension (spi_get_display (),
				      &priv->xkb_major_extension_opcode,
				      &priv->xkb_base_event_code,
				      &priv->xkb_base_error_code, NULL, NULL);
  if (priv->have_xkb)
    {
      gint i;
      guint64 reserved = 0;
      priv->xkb_desc = XkbGetMap (spi_get_display (), XkbKeySymsMask, XkbUseCoreKbd);
      XkbSelectEvents (spi_get_display (),
		       XkbUseCoreKbd,
		       XkbStateNotifyMask, XkbStateNotifyMask);	    
      _numlock_physical_mask = XkbKeysymToModifiers (spi_get_display (), 
						     XK_Num_Lock);
      for (i = priv->xkb_desc->max_key_code; i >= priv->xkb_desc->min_key_code; --i)
      {
	  if (priv->xkb_desc->map->key_sym_map[i].kt_index[0] == XkbOneLevelIndex)
	  { 
	      if (XKeycodeToKeysym (spi_get_display (), i, 0) != 0)
	      {
		  /* don't use this one if there's a grab client! */
		  gdk_error_trap_push ();
		  XGrabKey (spi_get_display (), i, 0, 
			    gdk_x11_get_default_root_xwindow (),
			    TRUE,
			    GrabModeSync, GrabModeSync);
		  XSync (spi_get_display (), TRUE);
		  XUngrabKey (spi_get_display (), i, 0, 
			      gdk_x11_get_default_root_xwindow ());
		  if (!gdk_error_trap_pop ())
		  {
		      reserved = i;
		      break;
		  }
	      }
	  }
      }
      if (reserved) 
      {
	  priv->reserved_keycode = reserved;
	  priv->reserved_keysym = XKeycodeToKeysym (spi_get_display (), reserved, 0);
      }
      else
      { 
	  priv->reserved_keycode = XKeysymToKeycode (spi_get_display (), XK_numbersign);
	  priv->reserved_keysym = XK_numbersign;
      }
#ifdef SPI_RESERVED_DEBUG
      unsigned sym = 0;
      sym = XKeycodeToKeysym (spi_get_display (), reserved, 0);
      fprintf (stderr, "%x\n", sym);
      fprintf (stderr, "setting the reserved keycode to %d (%s)\n", 
	       reserved, 
	       XKeysymToString (XKeycodeToKeysym (spi_get_display (),
                                                            reserved, 0)));
#endif
    }	

  gdk_window_add_filter (NULL, global_filter_fn, controller);

  gdk_window_set_events (gdk_get_default_root_window (),
			 GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

  x_default_error_handler = XSetErrorHandler (_spi_controller_device_error_handler);
}

static gboolean
spi_key_set_contains_key (GSList                          *key_set,
			  const Accessibility_DeviceEvent *key_event)
{
  gint i;
  gint len;
  GSList *l;

  if (!key_set)
    {
      g_print ("null key set!");
      return TRUE;
    }

  len = g_slist_length (key_set);
  
  if (len == 0) /* special case, means "all keys/any key" */
    {
#ifdef SPI_DEBUG
      g_print ("anykey\n");	    
#endif
      return TRUE;
    }

  for (l = key_set,i = 0; l; l = g_slist_next(l),i++)
    {
      Accessibility_KeyDefinition *kd = l->data;
#ifdef SPI_KEYEVENT_DEBUG	    
      g_print ("key_set[%d] event = %d, code = %d; key_event %d, code %d, string %s\n",
                i,
                (int) kd->keysym,
                (int) kd->keycode,
                (int) key_event->id,
                (int) key_event->hw_code,
                key_event->event_string); 
#endif
      if (kd->keysym == (dbus_uint32_t) key_event->id)
        {
          return TRUE;
	}
      if (kd->keycode == (dbus_uint32_t) key_event->hw_code)
        {
          return TRUE;
	}
      if (key_event->event_string && key_event->event_string[0] &&
	  !strcmp (kd->keystring, key_event->event_string))
        {
          return TRUE;
	}
    }

  return FALSE;
}

static gboolean
spi_eventtype_seq_contains_event (dbus_uint32_t types,
				  const Accessibility_DeviceEvent *event)
{
  if (types == 0) /* special case, means "all events/any event" */
    {
      return TRUE;
    }

  return (types & (1 << event->type));
}

static gboolean
spi_key_event_matches_listener (const Accessibility_DeviceEvent *key_event,
				DEControllerKeyListener         *listener,
				dbus_bool_t                    is_system_global)
{
  if (((key_event->modifiers & 0xFF) == (dbus_uint16_t) (listener->mask & 0xFF)) &&
       spi_key_set_contains_key (listener->keys, key_event) &&
       spi_eventtype_seq_contains_event (listener->listener.types, key_event) && 
      (is_system_global == listener->mode->global))
    {
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
spi_controller_notify_keylisteners (SpiDEController                 *controller,
				    Accessibility_DeviceEvent       *key_event,
				    dbus_bool_t                    is_system_global)
{
  GList   *l;
  GSList  *notify = NULL, *l2;
  GList  **key_listeners = &controller->key_listeners;
  gboolean is_consumed;

  if (!key_listeners)
    {
      return FALSE;
    }

  /* set the NUMLOCK event mask bit if appropriate: see bug #143702 */
  if (key_event->modifiers & _numlock_physical_mask)
      key_event->modifiers |= SPI_KEYMASK_NUMLOCK;

  for (l = *key_listeners; l; l = l->next)
    {
       DEControllerKeyListener *key_listener = l->data;

       if (spi_key_event_matches_listener (key_event, key_listener, is_system_global))
         {
	   /* we clone (don't dup) the listener, to avoid refcount inc. */
	   notify = g_slist_prepend (notify,
				     spi_key_listener_clone (key_listener));
         }
    }

#ifdef SPI_KEYEVENT_DEBUG
  if (!notify)
    {
      g_print ("no match for event\n");
    }
#endif

  is_consumed = FALSE;
  for (l2 = notify; l2 && !is_consumed; l2 = l2->next)
    {
      DEControllerKeyListener *key_listener = l2->data;	    

      is_consumed = Accessibility_DeviceEventListener_notifyEvent (controller, controller->registry, &key_listener->listener, key_event) &&
	            key_listener->mode->preemptive;

      spi_key_listener_clone_free (key_listener);
    }

  for (; l2; l2 = l2->next)
    {
      DEControllerKeyListener *key_listener = l2->data;	    
      spi_key_listener_clone_free (key_listener);
      /* clone doesn't have its own ref, so don't use spi_dec_listener_free */
    }

  g_slist_free (notify);

#ifdef SPI_DEBUG
  if (is_consumed) g_message ("consumed\n");
#endif
  return is_consumed;
}

static gboolean
spi_clear_error_state (void)
{
	gboolean retval = spi_error_code != 0;
	spi_error_code = 0;
	return retval;
}

static Accessibility_DeviceEvent
spi_keystroke_from_x_key_event (XKeyEvent *x_key_event)
{
  Accessibility_DeviceEvent key_event;
  KeySym keysym;
  const int cbuf_bytes = 20;
  char cbuf [21];
  int nbytes;

  nbytes = XLookupString (x_key_event, cbuf, cbuf_bytes, &keysym, NULL);  
  key_event.id = (dbus_int32_t)(keysym);
  key_event.hw_code = (dbus_int16_t) x_key_event->keycode;
  if (((XEvent *) x_key_event)->type == KeyPress)
    {
      key_event.type = Accessibility_KEY_PRESSED_EVENT;
    }
  else
    {
      key_event.type = Accessibility_KEY_RELEASED_EVENT;
    } 
  key_event.modifiers = (dbus_uint16_t)(x_key_event->state);
  key_event.is_text = FALSE;
  switch (keysym)
    {
      case ' ':
        key_event.event_string = g_strdup ("space");
        break;
      case XK_Tab:
        key_event.event_string = g_strdup ("Tab");
	break;
      case XK_BackSpace:
        key_event.event_string = g_strdup ("Backspace");
	break;
      case XK_Return:
        key_event.event_string = g_strdup ("Return");
	break;
      case XK_Home:
        key_event.event_string = g_strdup ("Home");
	break;
      case XK_Page_Down:
        key_event.event_string = g_strdup ("Page_Down");
	break;
      case XK_Page_Up:
        key_event.event_string = g_strdup ("Page_Up");
	break;
      case XK_F1:
        key_event.event_string = g_strdup ("F1");
	break;
      case XK_F2:
        key_event.event_string = g_strdup ("F2");
	break;
      case XK_F3:
        key_event.event_string = g_strdup ("F3");
	break;
      case XK_F4:
        key_event.event_string = g_strdup ("F4");
	break;
      case XK_F5:
        key_event.event_string = g_strdup ("F5");
	break;
      case XK_F6:
        key_event.event_string = g_strdup ("F6");
	break;
      case XK_F7:
        key_event.event_string = g_strdup ("F7");
	break;
      case XK_F8:
        key_event.event_string = g_strdup ("F8");
	break;
      case XK_F9:
        key_event.event_string = g_strdup ("F9");
	break;
      case XK_F10:
        key_event.event_string = g_strdup ("F10");
	break;
      case XK_F11:
        key_event.event_string = g_strdup ("F11");
	break;
      case XK_F12:
        key_event.event_string = g_strdup ("F12");
	break;
      case XK_End:
        key_event.event_string = g_strdup ("End");
	break;
      case XK_Escape:
        key_event.event_string = g_strdup ("Escape");
	break;
      case XK_Up:
        key_event.event_string = g_strdup ("Up");
	break;
      case XK_Down:
        key_event.event_string = g_strdup ("Down");
	break;
      case XK_Left:
        key_event.event_string = g_strdup ("Left");
	break;
      case XK_Right:
        key_event.event_string = g_strdup ("Right");
	break;
      default:
        if (nbytes > 0)
          {
	    gunichar c;
	    cbuf[nbytes] = '\0'; /* OK since length is cbuf_bytes+1 */
            key_event.event_string = g_strdup (cbuf);
	    c = keysym2ucs (keysym);
	    if (c > 0 && !g_unichar_iscntrl (c))
	      {
	        key_event.is_text = TRUE; 
		/* incorrect for some composed chars? */
	      }
          }
        else
          {
            key_event.event_string = g_strdup ("");
          }
    }

  key_event.timestamp = (dbus_uint32_t) x_key_event->time;
#ifdef SPI_KEYEVENT_DEBUG
  {
    char *pressed_str  = "pressed";
    char *released_str = "released";
    char *state_ptr;

    if (key_event.type == Accessibility_KEY_PRESSED_EVENT)
      state_ptr = pressed_str;
    else
      state_ptr = released_str;
 
    fprintf (stderr,
	     "Key %lu %s (%c), modifiers %d; string=%s [%x] %s\n",
	     (unsigned long) keysym,
	     state_ptr,
	     keysym ? (int) keysym : '*',
	     (int) x_key_event->state,
	     key_event.event_string,
	     key_event.event_string[0],
	     (key_event.is_text == TRUE) ? "(text)" : "(not text)");
  }
#endif
#ifdef SPI_DEBUG
  fprintf (stderr, "%s%c\n",
     (x_key_event->state & Mod1Mask)?"Alt-":"",
     ((x_key_event->state & ShiftMask)^(x_key_event->state & LockMask))?
     g_ascii_toupper (keysym) : g_ascii_tolower (keysym));
  fprintf (stderr, "serial: %x Time: %x\n", x_key_event->serial, x_key_event->time);
#endif /* SPI_DEBUG */
  return key_event;	
}

static gboolean
spi_controller_update_key_grabs (SpiDEController           *controller,
				 Accessibility_DeviceEvent *recv)
{
  GList *l, *next;
  gboolean   update_failed = FALSE;
  KeyCode keycode = 0;
  
  g_return_val_if_fail (controller != NULL, FALSE);

  /*
   * masks known to work with default RH 7.1+:
   * 0 (no mods), LockMask, Mod1Mask, Mod2Mask, ShiftMask,
   * ShiftMask|LockMask, Mod1Mask|LockMask, Mod2Mask|LockMask,
   * ShiftMask|Mod1Mask, ShiftMask|Mod2Mask, Mod1Mask|Mod2Mask,
   * ShiftMask|LockMask|Mod1Mask, ShiftMask|LockMask|Mod2Mask,
   *
   * ControlMask grabs are broken, must be in use already
   */
  if (recv)
    keycode = keycode_for_keysym (controller, recv->id, NULL);
  for (l = controller->keygrabs_list; l; l = next)
    {
      gboolean do_remove;
      gboolean re_issue_grab;
      DEControllerGrabMask *grab_mask = l->data;

      next = l->next;

      re_issue_grab = recv &&
	      (recv->modifiers & grab_mask->mod_mask) &&
	      (grab_mask->key_val == keycode);

#ifdef SPI_DEBUG
      fprintf (stderr, "mask=%lx %lx (%c%c) %s\n",
	       (long int) grab_mask->key_val,
	       (long int) grab_mask->mod_mask,
	       grab_mask->pending_add ? '+' : '.',
	       grab_mask->pending_remove ? '-' : '.',
	       re_issue_grab ? "re-issue": "");
#endif

      do_remove = FALSE;

      if (grab_mask->pending_add && grab_mask->pending_remove)
        {
          do_remove = TRUE;
	}
      else if (grab_mask->pending_remove)
        {
#ifdef SPI_DEBUG
      fprintf (stderr, "ungrabbing, mask=%x\n", grab_mask->mod_mask);
#endif
	  XUngrabKey (spi_get_display (),
		      grab_mask->key_val,
		      grab_mask->mod_mask,
		      gdk_x11_get_default_root_xwindow ());

          do_remove = TRUE;
	}
      else if (grab_mask->pending_add || re_issue_grab)
        {

#ifdef SPI_DEBUG
	  fprintf (stderr, "grab %d with mask %x\n", grab_mask->key_val, grab_mask->mod_mask);
#endif
          XGrabKey (spi_get_display (),
		    grab_mask->key_val,
		    grab_mask->mod_mask,
		    gdk_x11_get_default_root_xwindow (),
		    True,
		    GrabModeSync,
		    GrabModeSync);
	  XSync (spi_get_display (), False);
	  update_failed = spi_clear_error_state ();
	  if (update_failed) {
		  while (grab_mask->ref_count > 0) --grab_mask->ref_count;
		  do_remove = TRUE;
	  }
	}

      grab_mask->pending_add = FALSE;
      grab_mask->pending_remove = FALSE;

      if (do_remove)
        {
          g_assert (grab_mask->ref_count <= 0);

	  controller->keygrabs_list = g_list_delete_link (
	    controller->keygrabs_list, l);

	  spi_grab_mask_free (grab_mask);
	}

    } 

  return ! update_failed;
}

/*
 * Implemented GObject::finalize
 */
static void
spi_device_event_controller_object_finalize (GObject *object)
{
  SpiDEController *controller;
  DEControllerPrivateData *private;
  controller = SPI_DEVICE_EVENT_CONTROLLER (object);
  GObjectClass *parent_class = G_OBJECT_CLASS(spi_device_event_controller_parent_class);
#ifdef SPI_DEBUG
  fprintf(stderr, "spi_device_event_controller_object_finalize called\n");
#endif
  /* disconnect any special listeners, get rid of outstanding keygrabs */
  XUngrabKey (spi_get_display (), AnyKey, AnyModifier, DefaultRootWindow (spi_get_display ()));

#ifdef HAVE_XEVIE
  if (controller->xevie_display != NULL)
    {
      XevieEnd(controller->xevie_display);
#ifdef SPI_KEYEVENT_DEBUG
      printf("XevieEnd(dpy) finished \n");
#endif
    }
#endif

  private = g_object_get_data (G_OBJECT (controller), "spi-dec-private");
  if (private->xkb_desc)
	  XkbFreeKeyboard (private->xkb_desc, 0, True);
  g_free (private);
  parent_class->finalize (object);
}

/*
 * DBus Accessibility::DEController::registerKeystrokeListener
 *     method implementation
 */
static DBusMessage *
impl_register_keystroke_listener (DBusConnection *bus,
				  DBusMessage *message,
				  void *user_data)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER(user_data);
  DEControllerKeyListener *dec_listener;
  DBusMessageIter iter, iter_array;
  const char *path;
  GSList *keys = NULL;
  dbus_int32_t mask, type;
  Accessibility_EventListenerMode *mode;
  dbus_bool_t ret;
  DBusMessage *reply = NULL;
  char *keystring;

  dbus_message_iter_init(message, &iter);
  // TODO: verify type signature
  dbus_message_iter_get_basic(&iter, &path);
  dbus_message_iter_next(&iter);
  dbus_message_iter_recurse(&iter, &iter_array);
  while (dbus_message_iter_get_arg_type(&iter_array) != DBUS_TYPE_INVALID)
  {
    Accessibility_KeyDefinition *kd = (Accessibility_KeyDefinition *)g_malloc(sizeof(Accessibility_KeyDefinition));
    if (!spi_dbus_message_iter_get_struct(&iter_array, DBUS_TYPE_INT32, &kd->keycode, DBUS_TYPE_INT32, &kd->keysym, DBUS_TYPE_STRING, &keystring, DBUS_TYPE_INVALID))
    {
      break;
    }
    kd->keystring = g_strdup (keystring);
    keys = g_slist_append(keys, kd);
  }
  dbus_message_iter_next(&iter);
  dbus_message_iter_get_basic(&iter, &mask);
  dbus_message_iter_next(&iter);
  if (!strcmp (dbus_message_iter_get_signature (&iter), "u"))
    dbus_message_iter_get_basic(&iter, &type);
  else
  {
    dbus_message_iter_recurse(&iter, &iter_array);
    while (dbus_message_iter_get_arg_type(&iter_array) != DBUS_TYPE_INVALID)
    {
      dbus_uint32_t t;
      dbus_message_iter_get_basic (&iter_array, &t);
      type |= (1 << t);
      dbus_message_iter_next (&iter_array);
    }
    dbus_message_iter_next (&iter_array);
  }
  dbus_message_iter_next(&iter);
  mode = (Accessibility_EventListenerMode *)g_malloc(sizeof(Accessibility_EventListenerMode));
  if (mode)
  {
    spi_dbus_message_iter_get_struct(&iter, DBUS_TYPE_BOOLEAN, &mode->synchronous, DBUS_TYPE_BOOLEAN, &mode->preemptive, DBUS_TYPE_BOOLEAN, &mode->global, DBUS_TYPE_INVALID);
  }
#ifdef SPI_DEBUG
  fprintf (stderr, "registering keystroke listener %s:%s with maskVal %lu\n",
	   dbus_message_get_sender(message), path, (unsigned long) mask);
#endif
  dec_listener = spi_dec_key_listener_new (dbus_message_get_sender(message), path, keys, mask, type, mode);
  ret = spi_controller_register_device_listener (
	  controller, (DEControllerListener *) dec_listener);
  reply = dbus_message_new_method_return (message);
  if (reply)
  {
    dbus_message_append_args (reply, DBUS_TYPE_BOOLEAN, &ret, DBUS_TYPE_INVALID);
  }
  return reply;
}

/*
 * DBus Accessibility::DEController::registerDeviceEventListener
 *     method implementation
 */
static DBusMessage *
impl_register_device_event_listener (DBusConnection *bus,
				  DBusMessage *message,
				  void *user_data)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER(user_data);
  DEControllerListener *dec_listener;
  DBusError error;
  const char *path;
  dbus_int32_t event_types;
  dbus_bool_t ret;
  DBusMessage *reply = NULL;

  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_UINT32, &event_types, DBUS_TYPE_INVALID))
  {
    return droute_invalid_arguments_error (message);
  }
  dec_listener = spi_dec_listener_new (dbus_message_get_sender(message), path, event_types);
  ret =  spi_controller_register_device_listener (
	  controller, (DEControllerListener *) dec_listener);
  reply = dbus_message_new_method_return (message);
  if (reply)
  {
    dbus_message_append_args (reply, DBUS_TYPE_BOOLEAN, &ret, DBUS_TYPE_INVALID);
  }
  return reply;
}

typedef struct {
	DBusConnection *bus;
	DEControllerListener    *listener;
} RemoveListenerClosure;

static SpiReEntrantContinue
remove_listener_cb (GList * const *list,
		    gpointer       user_data)
{
  DEControllerListener  *listener = (*list)->data;
  RemoveListenerClosure *ctx = user_data;

  if (!strcmp(ctx->listener->bus_name, listener->bus_name) &&
      !strcmp(ctx->listener->path, listener->path))
    {
      spi_re_entrant_list_delete_link (list);
      spi_dbus_remove_disconnect_match (ctx->bus, listener->bus_name);
      spi_dec_listener_free (listener);
    }

  return SPI_RE_ENTRANT_CONTINUE;
}

static SpiReEntrantContinue
copy_key_listener_cb (GList * const *list,
		      gpointer       user_data)
{
  DEControllerKeyListener  *key_listener = (*list)->data;
  RemoveListenerClosure    *ctx = user_data;

  if (!strcmp(ctx->listener->bus_name, key_listener->listener.bus_name) &&
      !strcmp(ctx->listener->path, key_listener->listener.path))
    {
      /* TODO: FIXME aggregate keys in case the listener is registered twice */
      DEControllerKeyListener *ctx_key_listener = 
	(DEControllerKeyListener *) ctx->listener; 
      keylist_free (ctx_key_listener->keys);	    
      ctx_key_listener->keys = keylist_clone(key_listener->keys);
    }

  return SPI_RE_ENTRANT_CONTINUE;
}

static void
spi_controller_deregister_device_listener (SpiDEController            *controller,
					   DEControllerListener *listener)
{
  RemoveListenerClosure  ctx;

  ctx.bus = controller->bus;
  ctx.listener = listener;

  spi_re_entrant_list_foreach (&controller->mouse_listeners,
			       remove_listener_cb, &ctx);
}

static void
spi_deregister_controller_key_listener (SpiDEController            *controller,
					DEControllerKeyListener    *key_listener)
{
  RemoveListenerClosure  ctx;

  ctx.bus = controller->bus;
  ctx.listener = (DEControllerListener *) key_listener;

  /* special case, copy keyset from existing controller list entry */
  if (g_slist_length(key_listener->keys) == 0)
    {
      spi_re_entrant_list_foreach (&controller->key_listeners,
				  copy_key_listener_cb, &ctx);
    }

  spi_controller_deregister_global_keygrabs (controller, key_listener);

  spi_re_entrant_list_foreach (&controller->key_listeners,
				remove_listener_cb, &ctx);

}

void
spi_remove_device_listeners (SpiDEController *controller, const char *bus_name)
{
  GList *l, *tmp;

  for (l = controller->mouse_listeners; l; l = tmp)
  {
    DEControllerListener *listener = l->data;
    tmp = l->next;
    if (!strcmp (listener->bus_name, bus_name))
    {
      spi_controller_deregister_device_listener (controller, listener);
    }
  }
  for (l = controller->key_listeners; l; l = tmp)
  {
    DEControllerKeyListener *key_listener = l->data;
    tmp = l->next;
    if (!strcmp (key_listener->listener.bus_name, bus_name))
    {
      spi_deregister_controller_key_listener (controller, key_listener);
    }
  }
}

/*
 * DBus Accessibility::DEController::deregisterKeystrokeListener
 *     method implementation
 */
static DBusMessage *
impl_deregister_keystroke_listener (DBusConnection *bus,
				  DBusMessage *message,
				  void *user_data)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER(user_data);
  DEControllerKeyListener *key_listener;
  DBusMessageIter iter, iter_array;
  const char *path;
  GSList *keys = NULL;
  dbus_int32_t mask, type;
  DBusMessage *reply = NULL;

  dbus_message_iter_init(message, &iter);
  // TODO: verify type signature
  dbus_message_iter_get_basic(&iter, &path);
  dbus_message_iter_next(&iter);
  dbus_message_iter_recurse(&iter, &iter_array);
  while (dbus_message_iter_get_arg_type(&iter_array) != DBUS_TYPE_INVALID)
  {
    Accessibility_KeyDefinition *kd = (Accessibility_KeyDefinition *)g_malloc(sizeof(Accessibility_KeyDefinition));
  char *keystring;

    if (!spi_dbus_message_iter_get_struct(&iter_array, DBUS_TYPE_INT32, &kd->keycode, DBUS_TYPE_INT32, &kd->keysym, DBUS_TYPE_STRING, &keystring, DBUS_TYPE_INVALID))
    {
      break;
    }
    kd->keystring = g_strdup (keystring);
    keys = g_slist_append(keys, kd);
  }
  dbus_message_iter_next(&iter);
  dbus_message_iter_get_basic(&iter, &mask);
  dbus_message_iter_next(&iter);
  dbus_message_iter_get_basic(&iter, &type);
  dbus_message_iter_next(&iter);
  key_listener = spi_dec_key_listener_new (dbus_message_get_sender(message), path, keys, mask, type, NULL);
#ifdef SPI_DEREGISTER_DEBUG
  fprintf (stderr, "deregistering keystroke listener %p with maskVal %lu\n",
	   (void *) l, (unsigned long) mask->value);
#endif

  spi_deregister_controller_key_listener (controller, key_listener);

  spi_dec_listener_free ((DEControllerListener *) key_listener);
  reply = dbus_message_new_method_return (message);
  return reply;
}

/*
 * DBus Accessibility::DEController::deregisterDeviceEventListener
 *     method implementation
 */
static DBusMessage *
impl_deregister_device_event_listener (DBusConnection *bus,
				  DBusMessage *message,
				  void *user_data)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER(user_data);
  DEControllerListener *listener;
  DBusError error;
  const char *path;
  dbus_int32_t event_types;
  DBusMessage *reply = NULL;

  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_UINT32, &event_types, DBUS_TYPE_INVALID))
  {
    return droute_invalid_arguments_error (message);
  }
  listener = spi_dec_listener_new (dbus_message_get_sender(message), path, event_types);
  spi_controller_deregister_device_listener (
	  controller, listener);
  reply = dbus_message_new_method_return (message);
  return reply;
}

static unsigned int dec_xkb_get_slowkeys_delay (SpiDEController *controller)
{
  unsigned int retval = 0;
  DEControllerPrivateData *priv = (DEControllerPrivateData *)
	  g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
#ifdef HAVE_XKB
#ifdef XKB_HAS_GET_SLOW_KEYS_DELAY	
  retval = XkbGetSlowKeysDelay (spi_get_display (),
				XkbUseCoreKbd, &bounce_delay);
#else
  if (!(priv->xkb_desc == (XkbDescPtr) BadAlloc || priv->xkb_desc == NULL))
    {
      Status s = XkbGetControls (spi_get_display (),
				 XkbAllControlsMask, priv->xkb_desc);
      if (s == Success)
        {
	 if (priv->xkb_desc->ctrls->enabled_ctrls & XkbSlowKeysMask)
		 retval = priv->xkb_desc->ctrls->slow_keys_delay;
	}
    }
#endif
#endif
#ifdef SPI_XKB_DEBUG
	fprintf (stderr, "SlowKeys delay: %d\n", (int) retval);
#endif
        return retval;
}

static unsigned int dec_xkb_get_bouncekeys_delay (SpiDEController *controller)
{
  unsigned int retval = 0;
  DEControllerPrivateData *priv = (DEControllerPrivateData *)
	  g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
#ifdef HAVE_XKB
#ifdef XKB_HAS_GET_BOUNCE_KEYS_DELAY	
  retval = XkbGetBounceKeysDelay (spi_get_display (),
				  XkbUseCoreKbd, &bounce_delay);
#else
  if (!(priv->xkb_desc == (XkbDescPtr) BadAlloc || priv->xkb_desc == NULL))
    {
      Status s = XkbGetControls (spi_get_display (),
				 XkbAllControlsMask, priv->xkb_desc);
      if (s == Success)
        {
	  if (priv->xkb_desc->ctrls->enabled_ctrls & XkbBounceKeysMask)
		  retval = priv->xkb_desc->ctrls->debounce_delay;
	}
    }
#endif
#endif
#ifdef SPI_XKB_DEBUG
  fprintf (stderr, "BounceKeys delay: %d\n", (int) retval);
#endif
  return retval;
}

static gboolean
dec_synth_keycode_press (SpiDEController *controller,
			 unsigned int keycode)
{
	unsigned int time = CurrentTime;
	unsigned int bounce_delay;
	unsigned int elapsed_msec;
	struct timeval tv;
	DEControllerPrivateData *priv =
		(DEControllerPrivateData *) g_object_get_qdata (G_OBJECT (controller),
								spi_dec_private_quark);
	if (keycode == priv->last_release_keycode)
	{
		bounce_delay = dec_xkb_get_bouncekeys_delay (controller); 
                if (bounce_delay)
		{
			gettimeofday (&tv, NULL);
			elapsed_msec =
				(tv.tv_sec - priv->last_release_time.tv_sec) * 1000
				+ (tv.tv_usec - priv->last_release_time.tv_usec) / 1000;
#ifdef SPI_XKB_DEBUG			
			fprintf (stderr, "%d ms elapsed (%ld usec)\n", elapsed_msec,
				 (long) (tv.tv_usec - priv->last_release_time.tv_usec));
#endif
#ifdef THIS_IS_BROKEN
			if (elapsed_msec < bounce_delay)
				time = bounce_delay - elapsed_msec + 1;
#else
			time = bounce_delay + 10;
			/* fudge for broken XTest */
#endif
#ifdef SPI_XKB_DEBUG			
			fprintf (stderr, "waiting %d ms\n", time);
#endif
		}
	}
        XTestFakeKeyEvent (spi_get_display (), keycode, True, time);
	priv->last_press_keycode = keycode;
	XFlush (spi_get_display ());
	XSync (spi_get_display (), False);
	gettimeofday (&priv->last_press_time, NULL);
	return TRUE;
}

static gboolean
dec_synth_keycode_release (SpiDEController *controller,
			   unsigned int keycode)
{
	unsigned int time = CurrentTime;
	unsigned int slow_delay;
	unsigned int elapsed_msec;
	struct timeval tv;
	DEControllerPrivateData *priv =
		(DEControllerPrivateData *) g_object_get_qdata (G_OBJECT (controller),
								spi_dec_private_quark);
	if (keycode == priv->last_press_keycode)
	{
		slow_delay = dec_xkb_get_slowkeys_delay (controller);
		if (slow_delay)
		{
			gettimeofday (&tv, NULL);
			elapsed_msec =
				(tv.tv_sec - priv->last_press_time.tv_sec) * 1000
				+ (tv.tv_usec - priv->last_press_time.tv_usec) / 1000;
#ifdef SPI_XKB_DEBUG			
			fprintf (stderr, "%d ms elapsed (%ld usec)\n", elapsed_msec,
				 (long) (tv.tv_usec - priv->last_press_time.tv_usec));
#endif
#ifdef THIS_IS_BROKEN_DUNNO_WHY
			if (elapsed_msec < slow_delay)
				time = slow_delay - elapsed_msec + 1;
#else
			time = slow_delay + 10;
			/* our XTest seems broken, we have to add slop as above */
#endif
#ifdef SPI_XKB_DEBUG			
			fprintf (stderr, "waiting %d ms\n", time);
#endif
		}
	}
        XTestFakeKeyEvent (spi_get_display (), keycode, False, time);
	priv->last_release_keycode = keycode;
	XSync (spi_get_display (), False);
	gettimeofday (&priv->last_release_time, NULL);
	return TRUE;
}

static unsigned
dec_get_modifier_state (SpiDEController *controller)
{
	return mouse_mask_state;
}

static gboolean
dec_lock_modifiers (SpiDEController *controller, unsigned modifiers)
{
    DEControllerPrivateData *priv = (DEControllerPrivateData *) 
    g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);	 
    
    if (priv->have_xkb) {
        return XkbLockModifiers (spi_get_display (), XkbUseCoreKbd, 
                                  modifiers, modifiers);
    } else {
	int mod_index;
	for (mod_index=0;mod_index<8;mod_index++)
	    if (modifiers & (1<<mod_index))
	        dec_synth_keycode_press(controller, xmkeymap->modifiermap[mod_index]);
	return TRUE;
    }
}

static gboolean
dec_unlock_modifiers (SpiDEController *controller, unsigned modifiers)
{
    DEControllerPrivateData *priv = (DEControllerPrivateData *) 
    g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);	 
    
    if (priv->have_xkb) {
        return XkbLockModifiers (spi_get_display (), XkbUseCoreKbd, 
                                  modifiers, 0);
    } else {
	int mod_index;
	for (mod_index=0;mod_index<8;mod_index++)
	    if (modifiers & (1<<mod_index))
	        dec_synth_keycode_release(controller, xmkeymap->modifiermap[mod_index]);
	return TRUE;
    }
}

static KeySym
dec_keysym_for_unichar (SpiDEController *controller, gunichar unichar)
{
	return ucs2keysym ((long) unichar);
}

static gboolean
dec_synth_keysym (SpiDEController *controller, KeySym keysym)
{
	KeyCode key_synth_code;
	unsigned int modifiers, synth_mods, lock_mods;

	key_synth_code = keycode_for_keysym (controller, keysym, &synth_mods);

	if ((key_synth_code == 0) || (synth_mods == 0xFF)) return FALSE;

	/* TODO: set the modifiers accordingly! */
	modifiers = dec_get_modifier_state (controller);
	/* side-effect; we may unset mousebutton modifiers here! */

	lock_mods = 0;
	if (synth_mods != modifiers) {
		lock_mods = synth_mods & ~modifiers;
		dec_lock_modifiers (controller, lock_mods);
	}
	dec_synth_keycode_press (controller, key_synth_code);
	dec_synth_keycode_release (controller, key_synth_code);

	if (synth_mods != modifiers) 
		dec_unlock_modifiers (controller, lock_mods);
	return TRUE;
}


static gboolean
dec_synth_keystring (SpiDEController *controller, const char *keystring)
{
	/* probably we need to create and inject an XIM handler eventually. */
	/* for now, try to match the string to existing 
	 * keycode+modifier states. 
         */
	KeySym *keysyms;
	gint maxlen = 0;
	gunichar unichar = 0;
	gint i = 0;
	gboolean retval = TRUE;
	const gchar *c;

	maxlen = strlen (keystring) + 1;
	keysyms = g_new0 (KeySym, maxlen);
	if (!(keystring && *keystring && g_utf8_validate (keystring, -1, &c))) { 
		retval = FALSE;
	} 
	else {
#ifdef SPI_DEBUG
		fprintf (stderr, "[keystring synthesis attempted on %s]\n", keystring);
#endif
		while (keystring && (unichar = g_utf8_get_char (keystring))) {
			KeySym keysym;
			char bytes[6];
			gint mbytes;
			
			mbytes = g_unichar_to_utf8 (unichar, bytes);
			bytes[mbytes] = '\0';
#ifdef SPI_DEBUG
		        fprintf (stderr, "[unichar %s]", bytes);
#endif
			keysym = dec_keysym_for_unichar (controller, unichar);
			if (keysym == NoSymbol) {
#ifdef SPI_DEBUG
				fprintf (stderr, "no keysym for %s", bytes);
#endif
				retval = FALSE;
				break;
			}
			keysyms[i++] = keysym;
			keystring = g_utf8_next_char (keystring); 
		}
		keysyms[i++] = 0;
		XSynchronize (spi_get_display (), TRUE);
		for (i = 0; keysyms[i]; ++i) {
			if (!dec_synth_keysym (controller, keysyms[i])) {
#ifdef SPI_DEBUG
				fprintf (stderr, "could not synthesize %c\n",
					 (int) keysyms[i]);
#endif
				retval = FALSE;
				break;
			}
		}
		XSynchronize (spi_get_display (), FALSE);
	}
	g_free (keysyms);

	return retval;
}


/*
 * DBus Accessibility::DEController::registerKeystrokeListener
 *     method implementation
 */
static DBusMessage * impl_generate_keyboard_event (DBusConnection *bus, DBusMessage *message, void *user_data)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER(user_data);
  DBusError error;
  dbus_int32_t keycode;
  char *keystring;
  dbus_uint32_t synth_type;
  gint err;
  KeySym keysym;
  DEControllerPrivateData *priv;
  DBusMessage *reply = NULL;

  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error, DBUS_TYPE_INT32, &keycode, DBUS_TYPE_STRING, &keystring, DBUS_TYPE_UINT32, &synth_type, DBUS_TYPE_INVALID))
  {
    return droute_invalid_arguments_error (message);
  }

#ifdef SPI_DEBUG
	fprintf (stderr, "synthesizing keystroke %ld, type %d\n",
		 (long) keycode, (int) synth_type);
#endif
  /* TODO: hide/wrap/remove X dependency */

  /*
   * TODO: when initializing, query for XTest extension before using,
   * and fall back to XSendEvent() if XTest is not available.
   */
  
  gdk_error_trap_push ();

  priv = (DEControllerPrivateData *) 
      g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);

  if (!priv->have_xkb && xmkeymap==NULL) {
    xmkeymap = XGetModifierMapping(spi_get_display ());
  }

  switch (synth_type)
    {
      case Accessibility_KEY_PRESS:
	      dec_synth_keycode_press (controller, keycode);
	      break;
      case Accessibility_KEY_PRESSRELEASE:
	      dec_synth_keycode_press (controller, keycode);
      case Accessibility_KEY_RELEASE:
	      dec_synth_keycode_release (controller, keycode);
	      break;
      case Accessibility_KEY_SYM:
#ifdef SPI_XKB_DEBUG	      
	      fprintf (stderr, "KeySym synthesis\n");
#endif
	      /* 
	       * note: we are using long for 'keycode'
	       * in our arg list; it can contain either
	       * a keycode or a keysym.
	       */
	      dec_synth_keysym (controller, (KeySym) keycode);
	      break;
      case Accessibility_KEY_STRING:
	      if (!dec_synth_keystring (controller, keystring))
		      fprintf (stderr, "Keystring synthesis failure, string=%s\n",
			       keystring);
	      break;
    }
  if (synth_type == Accessibility_KEY_SYM) {
    keysym = keycode;
  }
  else {
    keysym = XkbKeycodeToKeysym (spi_get_display (), keycode, 0, 0);
  }
  if (XkbKeysymToModifiers (spi_get_display (), keysym) == 0) 
    {
      spi_dec_clear_unlatch_pending (controller);
    }
  reply = dbus_message_new_method_return (message);
  return reply;
}

/* Accessibility::DEController::generateMouseEvent */
static DBusMessage * impl_generate_mouse_event (DBusConnection *bus, DBusMessage *message, void *user_data)
{
  DBusError error;
  dbus_int32_t       x;
  dbus_int32_t       y;
  char *eventName;
  DBusMessage *reply = NULL;
  int button = 0;
  gboolean err = FALSE;
  Display *display = spi_get_display ();

  if (!dbus_message_get_args(message, &error, DBUS_TYPE_INT32, &x, DBUS_TYPE_INT32, &y, DBUS_TYPE_STRING, &eventName, DBUS_TYPE_INVALID))
  {
    return droute_invalid_arguments_error (message);
  }

#ifdef SPI_DEBUG
  fprintf (stderr, "generating mouse %s event at %ld, %ld\n",
	   eventName, (long int) x, (long int) y);
#endif
  switch (eventName[0])
    {
      case 'b':
        switch (eventName[1])
	  {
	  /* TODO: check number of buttons before parsing */
	  case '1':
		    button = 1;
		    break;
	  case '2':
		  button = 2;
		  break;
	  case '3':
	          button = 3;
	          break;
	  case '4':
	          button = 4;
	          break;
	  case '5':
	          button = 5;
	          break;
	  default:
		  err = TRUE;
	  }
	if (!err)
	  {
	    if (x != -1 && y != -1)
	      {
	        XTestFakeMotionEvent (display, DefaultScreen (display),
				      x, y, 0);
	      }
	    XTestFakeButtonEvent (display, button, !(eventName[2] == 'r'), 0);
	    if (eventName[2] == 'c')
	      XTestFakeButtonEvent (display, button, FALSE, 1);
	    else if (eventName[2] == 'd')
	      {
	      XTestFakeButtonEvent (display, button, FALSE, 1);
	      XTestFakeButtonEvent (display, button, TRUE, 2);
	      XTestFakeButtonEvent (display, button, FALSE, 3);
	      }
	  }
	break;
      case 'r': /* relative motion */ 
	XTestFakeRelativeMotionEvent (display, x, y, 0);
        break;
      case 'a': /* absolute motion */
	XTestFakeMotionEvent (display, DefaultScreen (display),
			      x, y, 0);
        break;
    }
  reply = dbus_message_new_method_return (message);
  return reply;
}

/* Accessibility::DEController::notifyListenersSync */
static DBusMessage *
impl_notify_listeners_sync (DBusConnection *bus, DBusMessage *message, void *user_data)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER(user_data);
  Accessibility_DeviceEvent event;
  dbus_bool_t ret;
  DBusMessage *reply = NULL;

  if (!spi_dbus_demarshal_deviceEvent(message, &event))
  {
    return droute_invalid_arguments_error (message);
  }
#ifdef SPI_DEBUG
  g_print ("notifylistening listeners synchronously: controller %p, event id %d\n",
	   controller, (int) event.id);
#endif
  ret = spi_controller_notify_keylisteners (controller,
					     (Accessibility_DeviceEvent *) 
					     &event, FALSE) ?
	  TRUE : FALSE; 
  reply = dbus_message_new_method_return (message);
  if (reply)
  {
    dbus_message_append_args (reply, DBUS_TYPE_BOOLEAN, &ret, DBUS_TYPE_INVALID);
  }
  return reply;
}

static DBusMessage *
impl_notify_listeners_async (DBusConnection *bus, DBusMessage *message, void *user_data)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER(user_data);
  Accessibility_DeviceEvent event;
  DBusMessage *reply = NULL;

  if (!spi_dbus_demarshal_deviceEvent(message, &event))
  {
    return droute_invalid_arguments_error (message);
  }
#ifdef SPI_DEBUG
  g_print ("notifylistening listeners asynchronously: controller %p, event id %d\n",
	   controller, (int) event.id);
#endif
  spi_controller_notify_keylisteners (controller, (Accessibility_DeviceEvent *)
				      &event, FALSE); 
  reply = dbus_message_new_method_return (message);
  return reply;
}

static void
spi_device_event_controller_class_init (SpiDEControllerClass *klass)
{
  GObjectClass * object_class = (GObjectClass *) klass;

  spi_device_event_controller_parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = spi_device_event_controller_object_finalize;

  if (!spi_dec_private_quark)
	  spi_dec_private_quark = g_quark_from_static_string ("spi-dec-private");
}

#ifdef HAVE_XEVIE
static Bool isEvent(Display *dpy, XEvent *event, char *arg)
{
   return TRUE;
}

static gboolean
handle_io (GIOChannel *source,
           GIOCondition condition,
           gpointer data) 
{
  SpiDEController *controller = (SpiDEController *) data;
  gboolean is_consumed = FALSE;
  XEvent ev;

  while (XCheckIfEvent(controller->xevie_display, &ev, isEvent, NULL))
    {
      if (ev.type == KeyPress || ev.type == KeyRelease)
        is_consumed = spi_device_event_controller_forward_key_event (controller, &ev);

      if (! is_consumed)
        XevieSendEvent(controller->xevie_display, &ev, XEVIE_UNMODIFIED);
    }

  return TRUE;
}
#endif /* HAVE_XEVIE */

static void
spi_device_event_controller_init (SpiDEController *device_event_controller)
{
#ifdef HAVE_XEVIE
  GIOChannel *ioc;
  int fd;
#endif /* HAVE_XEVIE */

  DEControllerPrivateData *private;	
  device_event_controller->key_listeners   = NULL;
  device_event_controller->mouse_listeners = NULL;
  device_event_controller->keygrabs_list   = NULL;
  device_event_controller->xevie_display   = NULL;

#ifdef HAVE_XEVIE
  device_event_controller->xevie_display = XOpenDisplay(NULL);

  if (XevieStart(device_event_controller->xevie_display) == TRUE)
    {
#ifdef SPI_KEYEVENT_DEBUG
      fprintf (stderr, "XevieStart() success \n");
#endif
      XevieSelectInput(device_event_controller->xevie_display, KeyPressMask | KeyReleaseMask);

      fd = ConnectionNumber(device_event_controller->xevie_display);
      ioc = g_io_channel_unix_new (fd);
      g_io_add_watch (ioc, G_IO_IN | G_IO_HUP, handle_io, device_event_controller);
      g_io_channel_unref (ioc);
    }
  else
    {
      device_event_controller->xevie_display = NULL;
#ifdef SPI_KEYEVENT_DEBUG
      fprintf (stderr, "XevieStart() failed, only one client is allowed to do event int exception\n");
#endif
    }
#endif /* HAVE_XEVIE */

  private = g_new0 (DEControllerPrivateData, 1);
  gettimeofday (&private->last_press_time, NULL);
  gettimeofday (&private->last_release_time, NULL);
  g_object_set_qdata (G_OBJECT (device_event_controller),
		      spi_dec_private_quark,
		      private);
  spi_controller_register_with_devices (device_event_controller);
}

static gboolean
spi_device_event_controller_forward_key_event (SpiDEController *controller,
					       const XEvent    *event)
{
  Accessibility_DeviceEvent key_event;
  gboolean ret;

  g_assert (event->type == KeyPress || event->type == KeyRelease);

  key_event = spi_keystroke_from_x_key_event ((XKeyEvent *) event);

  if (controller->xevie_display == NULL)
    spi_controller_update_key_grabs (controller, &key_event);

  /* relay to listeners, and decide whether to consume it or not */
  ret = spi_controller_notify_keylisteners (controller, &key_event, TRUE);
  g_free(key_event.event_string);
  return ret;
}


static gboolean
is_key_released (KeyCode code)
{
  char keys[32];
  int down;

  XQueryKeymap (spi_get_display (), keys);
  down = BIT (keys, code);
  return (down == 0);
}

static gboolean
check_release (gpointer data)
{
  gboolean released;
  Accessibility_DeviceEvent *event = (Accessibility_DeviceEvent *)data;
  KeyCode code = event->hw_code;

  released = is_key_released (code);

  if (released)
    {
      check_release_handler = 0;
      event->type = Accessibility_KEY_RELEASED_EVENT;
      spi_controller_notify_keylisteners (saved_controller, event, TRUE);
    }
  return (released == 0);
}

static void wait_for_release_event (XEvent          *event,
                                    SpiDEController *controller)
{
  pressed_event = spi_keystroke_from_x_key_event ((XKeyEvent *) event);
  saved_controller = controller;
  check_release_handler = g_timeout_add (CHECK_RELEASE_DELAY, check_release, &pressed_event);
}

static DRouteMethod dev_methods[] =
{
  { impl_register_keystroke_listener, "registerKeystrokeListener" },
  { impl_register_device_event_listener, "registerDeviceEventListener" },
  { impl_deregister_keystroke_listener, "deregisterKeystrokeListener" },
  { impl_deregister_device_event_listener, "deregisterDeviceEventListener" },
  { impl_generate_keyboard_event, "generateKeyboardEvent" },
  { impl_generate_mouse_event, "generateMouseEvent" },
  { impl_notify_listeners_sync, "notifyListenersSync" },
  { impl_notify_listeners_async, "notifyListenersAsync" },
  { NULL, NULL }
};

SpiDEController *
spi_registry_dec_new (SpiRegistry *reg, DBusConnection *bus, DRouteContext *droute)
{
  SpiDEController *dec = g_object_new (SPI_DEVICE_EVENT_CONTROLLER_TYPE, NULL);
  DRoutePath *path;

  dec->registry = g_object_ref (reg);
  dec->bus = bus;

  path = droute_add_one (droute,
                         "/org/freedesktop/atspi/registry/deviceeventcontroller",
                         dec);

  droute_path_add_interface (path,
                             SPI_DBUS_INTERFACE_DEC,
                             dev_methods,
                             NULL);

  spi_dec_init_mouse_listener (dec);
  /* TODO: kill mouse listener on finalize */

  return dec;
}
